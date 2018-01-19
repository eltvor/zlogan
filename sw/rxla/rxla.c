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

#define DMA_LEN 160000

static unsigned f_s = 250000000;

static struct {
  long n;
  int do_fwrite;
  FILE *out;
} dma_setup = {
  .n = DMA_LEN/4,
  .do_fwrite = 1,
  .out = NULL,
};

static inline void dma_reg_wr(unsigned reg, uint32_t val) {
  ((uint32_t*)mm_dma_ctl)[reg] = val;
  __mb();
}

static inline uint32_t dma_reg_rd(unsigned reg) {
  __mb();
  return ((uint32_t*)mm_dma_ctl)[reg];
}

static inline void rx_dma_trigger(uint32_t dma_ctl) {
  mm_ctrl[8] = dma_ctl; // fill length & test flag
  mm_ctrl[0] = 1<<9; // trigger DMA
  __mb();
}

int set_rt_prio_self();

void *dma_thread(void *arg) {
  uint32_t *p;
  unsigned long len;
  uint32_t mem_addrp = DMA_ADDR;
  p = mm_dma_data;

  set_rt_prio_self();
  
  len = dma_setup.n;
  fprintf(stderr, "block_len=%lu\n", len*4);
  
  unsigned len_k, len1 = len & ((1<<20)-1);
  if (len1 == 0)
    len1 = (1<<20);
  len_k = len1;
  
  /* DMA reset */
  mm_ctrl[8] = (1<<31)|(1<<30); // reset rxdma core + fifo
  __mb();
  usleep(1);
  mm_ctrl[8] = 0;
  __mb();
  dma_reg_wr(0x30/4, 1<<2);
  while ((dma_reg_rd(0x30/4) & (1<<2)));
  usleep(1);

  /* trigger xfer */
  rx_dma_trigger(len-1); // fill length & test flag
  while (len) {
    /* prepare DMA engine */
    dma_reg_wr(0x30/4, 1);  /* S2MM_DMACR.RS = 1 */
    dma_reg_wr(0x48/4, mem_addrp); /* S2MM_DA = addr. */
    dma_reg_wr(0x58/4, len_k * 4); /* S2MM_LENGTH = len [B] */

    len -= len_k;
    len_k = (1<<20);
    mem_addrp += (1<<22);
    
    while (!(dma_reg_rd(0x34/4) & 0x2));
  }
  printf("start=0x%08x, end=0x%08x\n", DMA_ADDR, mem_addrp);
  if (dma_setup.do_fwrite) {
    fwrite(p, 4, (mem_addrp-DMA_ADDR)/4, dma_setup.out);
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
  int opt;
  log_stream_add(stderr);
  log_level(L_DEBUG);

  dma_setup.out = stdout;
  
  optind = opterr = 0;
  while ((opt = getopt(argc, argv, "r:f:s:n:o:")) != -1)
    switch (opt) {
    case 'f':
      f_s = strtoul(optarg, NULL, 0);
      break;
    case 'n':
      dma_setup.n = strtoul(optarg, NULL, 0)/4;
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

  hw_init(f_s);
  mm_ctrl[4] = 0x1; __mb(); // zlogan reset
  mm_ctrl[4] = 0x0; __mb(); 
  mm_ctrl[4] = 0x2; __mb(); // zlogan enable
  
  mlockall(MCL_CURRENT | MCL_FUTURE);

  /*
   * prevent crashes on SIGPIPE
   */
  signal(SIGPIPE, SIG_IGN);
  dma_thread(NULL);
  fclose(dma_setup.out);
  
  return 0;
}
