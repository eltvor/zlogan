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
#define _DEFAULT_SOURCE
#define _BSD_SOURCE

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
#include <err.h>
#include <stdbool.h>
//------------------------------------------------------------------------------

unsigned WORD_SIZE = 0; // detected at runtime

// NOTE: the DMA core has apparently some address alignment limitation
#define DMA_MAX_LEN_W     (((1U << DMA_LENGTH_BITS)/WORD_SIZE - 1) & ~0x0)
#define DMA_MAX_LEN_B     (DMA_MAX_LEN_W*WORD_SIZE)
/*
#define DMA_MAX_LEN_W     ((1U << (DMA_LENGTH_BITS-1))/WORD_SIZE)
#define DMA_MAX_LEN_B     (DMA_MAX_LEN_W*WORD_SIZE)
*/
//------------------------------------------------------------------------------

int set_rt_prio_self();
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
//------------------------------------------------------------------------------

typedef struct dma_setup_t {
    size_t transfer_size_w;
    FILE *out;
} dma_setup_t;

volatile sig_atomic_t g_quit = 0;
//------------------------------------------------------------------------------

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
    log_wr(L_DEBUG, "rx_dma_trigger(%u)", dma_len);
    mm_ctrl[ZLOGAN_REG_LEN] = dma_len; // fill length
    __mb();
    mm_ctrl[ZLOGAN_REG_CR] |= ZLOGAN_CR_DMAFSM_TRIG;
    __mb();
}

static inline void rx_dma_untrigger() {
    log_wr(L_DEBUG, "rx_dma_untrigger()");
    mm_ctrl[ZLOGAN_REG_CR] &= ~ZLOGAN_CR_DMAFSM_TRIG;
    __mb();
}
//------------------------------------------------------------------------------

static inline void zlogan_enable() {
    mm_ctrl[ZLOGAN_REG_CR] |= ZLOGAN_CR_EN;
    __mb();
}
//------------------------------------------------------------------------------

static inline void zlogan_disable() {
    mm_ctrl[ZLOGAN_REG_CR] &= ~ZLOGAN_CR_EN;
    __mb();
}
//------------------------------------------------------------------------------

static inline uint32_t zlogan_read_reg(uint32_t reg) {
    return mm_ctrl[reg];
}
//------------------------------------------------------------------------------

static void dump_regs()
{
    char buf[512];
    char *p = buf;
    static const char * const names[] = {
        "CR", "SR", "LEN", "INP", "ID", "FIFO_DATA_COUNT",
        "FIFO_RD_DATA_COUNT", "FIFO_WR_DATA_COUNT", "SHADOW_LEN"
    };

    for (unsigned i=0; i<ARRAY_SIZE(names); ++i) {
        __mb();
        unsigned r = mm_ctrl[i];
        p += sprintf(p, "%s%s = 0x%08x (%u)", (i==0?"":", "), names[i], r, r);
    }
    log_wr(L_DEBUG, "%s", buf);
}
//------------------------------------------------------------------------------

void save_header(FILE *f, size_t burst_size_w, size_t transfer_size_w)
{
    zlo_header_t hdr;
    uint32_t id = mm_ctrl[ZLOGAN_REG_ID];

    memset(&hdr, 0, sizeof(hdr));

    hdr.ip_version = ZLOGAN_ID_GET_VER(id);
    hdr.nsignals = ZLOGAN_ID_GET_NSIG(id);
    hdr.word_size = WORD_SIZE;
    hdr.hdr_length = sizeof(zlo_header_t);
    hdr.burst_size_w = htole32(burst_size_w);
    hdr.transfer_size_w = htole32(transfer_size_w);
    fwrite(&hdr, 1, sizeof(zlo_header_t), f);
}
//------------------------------------------------------------------------------

void save_data(FILE *f, const void *data, size_t size)
{
    fwrite(data, 1, size, f);
}
//------------------------------------------------------------------------------

static void *dma_thread(void *arg)
{
    const dma_setup_t *dma_setup = (const dma_setup_t *) arg;

    uint32_t mem_addrp = DMA_ADDR;
    size_t block_len_w = DMA_MAX_LEN_W;
    size_t total_len_w = dma_setup->transfer_size_w;
    bool first = true;

    set_rt_prio_self();

    log_wr(L_INFO, "total_len=%" PRIu32 ", block_len=%" PRIu32 "\n",
           total_len_w*WORD_SIZE, block_len_w*WORD_SIZE);

    save_header(dma_setup->out, block_len_w, total_len_w);

    /* DMA reset */
    uint32_t tmp;
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

    /*
     * The core will repeat transfers with the same length.
     * The length is loaded into a shadow register on the beginning of
     * the transfer, so we must setup the length of the last transfer
     * immediately after starting the one before that.
     */
    while (total_len_w && !g_quit) {
        bool last = total_len_w <= block_len_w;
        bool next_to_last = total_len_w <= 2*block_len_w && !last;

        if (last) {
            block_len_w = total_len_w;
            if (first)
                rx_dma_trigger(block_len_w-1);
            rx_dma_untrigger();
        }
        if (first && !last) {
            rx_dma_trigger(block_len_w-1);
        }
        if (next_to_last) {
            rx_dma_trigger(total_len_w - block_len_w - 1);
        }
        first = false;

        log_wr(L_INFO, "DMA read loop: %" PRIu32 " bytes", block_len_w*WORD_SIZE);
        tmp = mm_ctrl[ZLOGAN_REG_SR];
        if (tmp & ZLOGAN_SR_XRUN)
            log_wr(L_WARN, "FIFO Overrun. Some data will be missing.");

        /* prepare DMA engine */
        dma_s2mm_reg_wr(XILINX_DMA_REG_DMACR, XILINX_DMA_DMACR_RUNSTOP);  /* S2MM_DMACR.RS = 1 */
        dma_s2mm_reg_wr(XILINX_DMA_REG_ADDR, mem_addrp); /* S2MM_DA = addr. */
        dma_s2mm_reg_wr(XILINX_DMA_REG_ADDR_MSB, 0); /* S2MM_DA_MSB = 0. */
        dma_s2mm_reg_wr(XILINX_DMA_REG_LENGTH, block_len_w * WORD_SIZE); /* S2MM_LENGTH = len [B] */
        //log_wr(L_DEBUG, "DMA: CR=0x%08x, ADDR=%");

        total_len_w -= block_len_w;

        log_wr(L_INFO, "waiting for DMA ready");
        while (!g_quit) {
            uint32_t sr = dma_s2mm_reg_rd(XILINX_DMA_REG_DMASR);
            if (sr & XILINX_DMA_DMASR_IDLE)
                break;
            else if (sr & XILINX_DMA_DMASR_HALTED) {
                log_wr(L_ERR, "DMA Halted - DMA error? (SR=0x%08x)", sr);
                dump_regs();
                return NULL;
            }
            usleep(10);
        }
        if (g_quit) {
            /*
             * - disable zlogan
             * - zlogan will flush its output
             * - DMA transfer will fail (because the configured transfer size
             *   does not match with received data burst length)
             * - either read written data length from zlogan or detect it by
             *   inspecting the transferred bytes
             * - save only the transferred data
             */
            uint32_t len, shadow_len;
            bool ok = false;
            log_wr(L_INFO, "interrupted mid-burst, tearing down ...");
            zlogan_disable();
            for (int i=0; i<1000; ++i) {
                uint32_t sr = dma_s2mm_reg_rd(XILINX_DMA_REG_DMASR);
                if (sr & (XILINX_DMA_DMASR_IDLE | XILINX_DMA_DMASR_HALTED)) {
                    ok = true;
                    break;
                }
                usleep(100);
            }
            if (!ok)
                log_wr(L_WARN, "DMA did not finish in time, giving up");
            len = zlogan_read_reg(ZLOGAN_REG_LEN);
            shadow_len = zlogan_read_reg(ZLOGAN_REG_SHADOW_LEN);

            block_len_w = len - shadow_len;
        }
        log_wr(L_INFO, "writing %u bytes ...", block_len_w*WORD_SIZE);
        save_data(dma_setup->out, (const void *) mm_dma_data, block_len_w*WORD_SIZE);
    }
    return NULL;
}
//------------------------------------------------------------------------------

int set_prio(pthread_t th, int policy, int prio)
{
    struct sched_param rtp;
    memset(&rtp, 0, sizeof(rtp));
    rtp.sched_priority = prio;
    int rc = pthread_setschedparam(th, policy, &rtp);
    if (rc) {
        fprintf(stderr, "pthread_setschedparam() failed\n");
        return -1;
    } else {
        fprintf(stderr, "pthread_setschedparam() ok!\n");
        return 0;
    }
}
//------------------------------------------------------------------------------

int set_rt_prio_self()
{
    /* set "realtime" priority to calling thread */
    return set_prio(pthread_self(), SCHED_FIFO,
                    sched_get_priority_max(SCHED_FIFO));
}
//------------------------------------------------------------------------------

void handle_exit(int sig)
{
    (void) sig;
    g_quit = 1;
}
//------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    int ret = 0;
    int opt;
    dma_setup_t dma_setup = {
        .transfer_size_w = 0,
        .out = stdout,
    };

    log_stream_add(stderr);
    log_level(L_DEBUG);

    optind = opterr = 0;
    while ((opt = getopt(argc, argv, "n:o:hq")) != -1) {
        switch (opt) {
        case 'n':
            dma_setup.transfer_size_w = strtoul(optarg, NULL, 0);
            break;
        case 'o':
            dma_setup.out = fopen(optarg, "w");
            break;
        case 'q':
            log_level(L_WARN);
            break;
        case 'h':
            fprintf(stderr,
"Zlogan data receiver\n"
"\n"
"Usage: %s [OPTIONS]\n"
"Options:\n"
"    -n transfer_length    Total length of bytes to transfer. Will be rounded\n"
"                          up to Zlogan word size and minimal DMA transfer size.\n"
"    -o filename           File to write the data into. Defaults to stdout.\n"
"    -q                    Be less verbose (output level set to L_WARNING).\n"
                   , argv[0]);
            return 0;
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

    hw_init();
    mm_ctrl[ZLOGAN_REG_CR] |= ZLOGAN_CR_LA_RST; __mb(); // zlogan reset
    mm_ctrl[ZLOGAN_REG_CR] &= ~ZLOGAN_CR_LA_RST; __mb();

    uint32_t id = mm_ctrl[ZLOGAN_REG_ID];
    WORD_SIZE = ZLOGAN_ID_GET_WS(id);
    if (ZLOGAN_ID_GET_VER(id) != 1) {
        log_wr(L_ERR, "unsupported Zlogan IP version (%u)", ZLOGAN_ID_GET_VER(id));
        ret = 1;
        goto cleanup;
    }
    if (dma_setup.transfer_size_w == 0)
        dma_setup.transfer_size_w = DMA_MAX_LEN_B;
    dma_setup.transfer_size_w /= WORD_SIZE;

    if (DMA_SIZE < WORD_SIZE*8)
        err(1, "DMA buffer too small: at least %u bytes required, got %u", WORD_SIZE*8, DMA_SIZE);

    struct sigaction sa = {0};
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = &handle_exit;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL); // prevent crashes on SIGPIPE

    mlockall(MCL_CURRENT | MCL_FUTURE);

    zlogan_enable();

    dma_thread(&dma_setup);
cleanup:
    fclose(dma_setup.out);
    return ret;
}
