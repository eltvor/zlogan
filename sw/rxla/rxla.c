/**
 * Dump data from Zlogan.
 * Requires udmabuf kernel driver (https://github.com/ikwzm/udmabuf),
 * expects a configured /dev/udmabuf0.
 *
 * Currently uses hard-coded peripheral addresses -> TODO: pull them from
 *   device tree and remake this as a kernel module.
 * Also check that DMA_LENGTH_BITS here is the same as set in DMA peripheral.
 * Zlogan version, number of signals and DMA word size are read from Zlogan IP
 * and are included in the dump header.
 *
 * Example:
 *   ./rxla -n $((16*1024*1024)) -o out.bin
 */
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <math.h>

#include "log.h"
#include "hw.h"
#include "../zlo.h"
#include <endian.h>

static unsigned f_s = 250000000;

// TODO: detect the actual length
#define DMA_LENGTH_BITS    24

unsigned WORD_SIZE = 0; // detected at runtime

#define DMA_MAX_LEN_W     ((1U << DMA_LENGTH_BITS)/WORD_SIZE - 1)
#define DMA_MAX_LEN_B     (DMA_MAX_LEN_W*WORD_SIZE)
/*
#define DMA_MAX_LEN_W     ((1U << (DMA_LENGTH_BITS-1))/WORD_SIZE)
#define DMA_MAX_LEN_B     (DMA_MAX_LEN_W*WORD_SIZE)
*/
static struct {
  long n;
  int do_fwrite;
  FILE *out;
} dma_setup = {
  .n = 0,
  .do_fwrite = 1,
  .out = NULL,
};

static inline void dma_reg_wr(unsigned reg, uint32_t val) {
    volatile uint32_t *p = &((volatile uint32_t*)mm_dma_ctl)[reg/4];
    log_wr(L_DEBUG, "dma_reg_wr(0x%02x): *0x%08x = 0x%08x", reg, (unsigned)p, val);
    *p = val;
    __mb();
}

static inline uint32_t dma_reg_rd(unsigned reg) {
    volatile uint32_t *p = &((volatile uint32_t*)mm_dma_ctl)[reg/4];
    __mb();
    uint32_t val = *p;
    //log_wr(L_DEBUG, "dma_reg_rd(0x%02x): *0x%08x == 0x%08x", reg, (unsigned)p, val);
    return val;
}

static inline void dma_s2mm_reg_wr(unsigned reg, uint32_t val) {
    dma_reg_wr(XILINX_DMA_S2MM_CTRL_OFFSET+reg, val);
}

static inline uint32_t dma_s2mm_reg_rd(unsigned reg) {
    return dma_reg_rd(XILINX_DMA_S2MM_CTRL_OFFSET+reg);
}

static inline void rx_dma_trigger(uint32_t dma_len) {
  mm_ctrl[ZLOGAN_REG_LEN] = dma_len; // fill length & test flag
  mm_ctrl[ZLOGAN_REG_CR] |= ZLOGAN_CR_DMAFSM_TRIG; // trigger DMA
  __mb();
  mm_ctrl[ZLOGAN_REG_CR] &= ~ZLOGAN_CR_DMAFSM_TRIG;

  // TODO: clear DMA Trig?
  __mb();
}

int set_rt_prio_self();
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
static void dump_regs() {
    char buf[512];
    char *p = buf;
    const char *names[] = {"CR", "LEN", "SR", "FIFO_DATA_COUNT", "FIFO_RD_DATA_COUNT", "FIFO_WR_DATA_COUNT", "INP", "SHADOW_LEN"};
    for (int i=0; i<ARRAY_SIZE(names); ++i) {
        __mb();
        unsigned r = mm_ctrl[i];
        p += sprintf(p, "%s%s = 0x%08x (%u)", (i==0?"":", "), names[i], r, r);
    }
    log_wr(L_DEBUG, "%s", buf);
}

void *dma_thread(void *arg) {
  uint32_t *p;
  unsigned long len;
  uint32_t mem_addrp = DMA_ADDR;
  p = mm_dma_data;

  set_rt_prio_self();
  len = dma_setup.n;
  fprintf(stderr, "block_len=%lu\n", len*WORD_SIZE);
  
  unsigned len_k, len1 = len & DMA_MAX_LEN_W;
  if (len1 == 0)
    len1 = DMA_MAX_LEN_W;
  len_k = len1;
  
  /* DMA reset */
  unsigned tmp;
  tmp = mm_ctrl[ZLOGAN_REG_CR];
  tmp |= ZLOGAN_CR_FIFO_RST | ZLOGAN_CR_DMAFSM_RST;
  // reset rxdma core + fifo
  mm_ctrl[ZLOGAN_REG_CR] = tmp;
  __mb();
  usleep(1);
  tmp &= ~(ZLOGAN_CR_FIFO_RST | ZLOGAN_CR_DMAFSM_RST);
  mm_ctrl[ZLOGAN_REG_CR] = tmp;
  __mb();

  log_wr(L_INFO, "resetting DMA ...");
  dma_s2mm_reg_wr(XILINX_DMA_REG_DMACR, XILINX_DMA_DMACR_RESET);
  log_wr(L_INFO, "waiting for reset clear ...");
  while ((dma_s2mm_reg_rd(XILINX_DMA_REG_DMACR) & XILINX_DMA_DMACR_RESET))
    usleep(1);
  log_wr(L_INFO, "DMA reset clear");

  // TODO: trigger after setting the DMA, not before ... ?
  // TODO: multi-step reading does not work ...
  int first = 1;
  while (len >= len_k) {
      log_wr(L_INFO, "read loop");
      if (first) {
          rx_dma_trigger(len_k-1); // fill length & test flag
          first = 0;
      } else {
          rx_dma_trigger(len_k-1); // fill length & test flag
      }
    /* prepare DMA engine */
    dma_s2mm_reg_wr(XILINX_DMA_REG_DMACR, XILINX_DMA_DMACR_RUNSTOP);  /* S2MM_DMACR.RS = 1 */
    dma_s2mm_reg_wr(XILINX_DMA_REG_ADDR, mem_addrp); /* S2MM_DA = addr. */
    dma_s2mm_reg_wr(XILINX_DMA_REG_ADDR_MSB, 0); /* S2MM_DA_MSB = 0. */
    dma_s2mm_reg_wr(XILINX_DMA_REG_LENGTH, len_k * WORD_SIZE); /* S2MM_LENGTH = len [B] */
    //log_wr(L_DEBUG, "DMA: CR=0x%08x, ADDR=%");

    len -= len_k;
    len_k = DMA_MAX_LEN_W;
    mem_addrp += DMA_MAX_LEN_B;

    log_wr(L_INFO, "waiting for DMA ready");
    while (1) {
        unsigned sr = dma_s2mm_reg_rd(XILINX_DMA_REG_DMASR);
        if (sr & XILINX_DMA_DMASR_IDLE)
            break;
        else if (sr & XILINX_DMA_DMASR_HALTED) {
            log_wr(L_ERR, "DMA Halted - DMA error? (SR=0x%08x)", sr);
            dump_regs(); // DBG
            return NULL;
        }
        usleep(1); // DBG
    }
  }
  log_wr(L_INFO, "start=0x%08x, end=0x%08x\n", DMA_ADDR, mem_addrp);
  if (dma_setup.do_fwrite) {
      zlo_header_t hdr;
      uint32_t id = mm_ctrl[ZLOGAN_REG_ID];
      hdr.ip_version = ZLOGAN_ID_GET_VER(id);
      hdr.nsignals = ZLOGAN_ID_GET_NSIG(id);
      hdr.word_size = WORD_SIZE;
      hdr.burst_size_w = htole32(dma_setup.n);
      fwrite(&hdr, 1, sizeof(zlo_header_t), dma_setup.out);
      fwrite(p, WORD_SIZE, (mem_addrp-DMA_ADDR)/WORD_SIZE, dma_setup.out);
  }
  return NULL;
}

int set_prio(pthread_t th, int policy, int prio) {
  struct sched_param rtp;
  memset(&rtp, 0, sizeof(rtp));
  rtp.sched_priority = prio;
  int rc = pthread_setschedparam(th, policy, &rtp);
  if (rc) {
    fprintf(stderr, "pthread_setschedparam() failed\n");
    return -1;
  }
  else {
    fprintf(stderr, "pthread_setschedparam() ok!\n");
    return 0;
  }
}

int set_rt_prio_self() {
  /* set "realtime" priority to calling thread */
  return
    set_prio(pthread_self(), SCHED_FIFO, sched_get_priority_max(SCHED_FIFO));
}

int main(int argc, char *argv[]) {
  int ret = 0;
  int opt;
  log_stream_add(stderr);
  log_level(L_DEBUG);

  dma_setup.out = stdout;

  optind = opterr = 0;
  while ((opt = getopt(argc, argv, "f:n:o:")) != -1) {
    switch (opt) {
    case 'f':
      f_s = strtoul(optarg, NULL, 0);
      break;
    case 'n':
      dma_setup.n = strtoul(optarg, NULL, 0);
      break;
    case 'o':
      dma_setup.out = fopen(optarg, "w");
      break;
    case ':':
      fprintf(stderr, "command line option -%c requires an argument\n",
              optopt);
      return -1;
      break;
    case '?':
      fprintf(stderr, "unrecognized command line option -%c\n",
              optopt);
      return -1;
      break;
    default:
      break;
    }
  }

  hw_init(f_s);
  mm_ctrl[ZLOGAN_REG_CR] |= ZLOGAN_CR_LA_RST; __mb(); // zlogan reset
  mm_ctrl[ZLOGAN_REG_CR] &= ~ZLOGAN_CR_LA_RST; __mb();
  mm_ctrl[ZLOGAN_REG_CR] |= ZLOGAN_CR_EN; __mb(); // zlogan enable

  mlockall(MCL_CURRENT | MCL_FUTURE);

  uint32_t id = mm_ctrl[ZLOGAN_REG_ID];
  WORD_SIZE = ZLOGAN_ID_GET_WS(id);
  if (ZLOGAN_ID_GET_VER(id) != 1) {
    log_wr(L_ERR, "unsupported Zlogan IP version (%u)", ZLOGAN_ID_GET_VER(id));
    ret = 1;
    goto cleanup;
  }
  if (dma_setup.n == 0)
      dma_setup.n = DMA_MAX_LEN_B;
  dma_setup.n /= WORD_SIZE;

  /*
   * prevent crashes on SIGPIPE
   */
  signal(SIGPIPE, SIG_IGN);
  dma_thread(NULL);

cleanup:
  fclose(dma_setup.out);
  return ret;
}
