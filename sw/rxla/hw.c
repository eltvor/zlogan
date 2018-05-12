#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>

#include "log.h"
#include "hw.h"
//------------------------------------------------------------------------------

volatile uint32_t *mm_ctrl, *mm_dma_ctl, *mm_dma_data;
uint32_t DMA_ADDR, DMA_SIZE;

static const char *memdev = "/dev/mem";
static int mem_fd = -1;
//------------------------------------------------------------------------------

static int mem_open()
{
    mem_fd = open(memdev, O_RDWR|O_SYNC);
    if (mem_fd < 0) {
        perror("open memory device");
        return -1;
    }
    return 0;
}
//------------------------------------------------------------------------------

void *mem_map(unsigned long mem_start, unsigned long mem_length)
{
    unsigned long pagesize, mem_window_size;
    void *mm, *mem;

    //pagesize = getpagesize();
    pagesize = sysconf(_SC_PAGESIZE);

    mem_window_size = ((mem_start & (pagesize-1)) + mem_length + pagesize-1) & ~(pagesize-1);

    mm = mmap(NULL, mem_window_size, PROT_WRITE|PROT_READ,
              MAP_SHARED, mem_fd, mem_start & ~(pagesize-1));
    mem = (char*)mm + (mem_start & (pagesize-1));

    if (mm == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    fprintf(stderr, "mmap 0x%lx -> %p\n",mem_start,mem);
    return mem;
}
//------------------------------------------------------------------------------

static uint32_t read_uint(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror(path);
        exit(1);
    }
    char buf[32];
    int len = read(fd, buf, sizeof(buf));
    close(fd);
    if (len < 0) {
        perror(path);
        exit(1);
    }
    char *ep = buf+len;
    uint32_t res = strtoul(buf, &ep, 0);
    log_wr(L_DEBUG, "read 0x%08x from %s\n", res, path);
    return res;
}
//------------------------------------------------------------------------------

static void* alloc_dma_buf(uint32_t *phys_addr, uint32_t *size)
{
    int fd = open("/dev/udmabuf0", O_RDONLY);
    if (fd < 0) {
        perror("/dev/udmabuf0");
        return NULL;
    }
    *phys_addr = read_uint("/sys/class/udmabuf/udmabuf0/phys_addr");
    *size = read_uint("/sys/class/udmabuf/udmabuf0/size");
    void *map = mmap(NULL, *size, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        *phys_addr = 0xFFFFFFFF;
        *size = 0;
        return NULL;
    }
    return map;
}
//------------------------------------------------------------------------------

int hw_init()
{
    mem_open();
    mm_ctrl = (volatile uint32_t *) mem_map(CTRL_ADDR, CTRL_SIZE);
    mm_dma_ctl = (volatile uint32_t *) mem_map(DMA_CTL_ADDR, DMA_CTL_SIZE);
    mm_dma_data = (volatile uint32_t *) alloc_dma_buf(&DMA_ADDR, &DMA_SIZE);

    /* init regs */
    mm_ctrl[ZLOGAN_REG_CR] = ZLOGAN_CR_LA_RST | ZLOGAN_CR_DMAFSM_RST | ZLOGAN_CR_FIFO_RST;
    mm_ctrl[ZLOGAN_REG_LEN] = 0;
    __mb();
    mm_ctrl[ZLOGAN_REG_CR] = 0x0; // disable
    __mb();

    return 0;
}
