#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>

#include "hw.h"

uint32_t *mm_data, *mm_ctrl, *mm_dma_ctl, *mm_dma_data, *mm_dma_code;

static char *memdev = "/dev/mem";
static int mem_fd = -1;

static unsigned pps_phase = 0;

int mem_open() {
  mem_fd = open(memdev, O_RDWR|O_SYNC);
  if (mem_fd < 0) {
    perror("open memory device");
    return -1;
  }
  return 0;
}

void *mem_map(unsigned long mem_start, unsigned long mem_length) {
  unsigned long pagesize, mem_window_size;
  void *mm, *mem;

  //pagesize = getpagesize();
  pagesize = sysconf(_SC_PAGESIZE);

  mem_window_size = ((mem_start & (pagesize-1)) + mem_length + pagesize-1) & ~(pagesize-1);

  mm = mmap(NULL, mem_window_size, PROT_WRITE|PROT_READ,
            MAP_SHARED, mem_fd, mem_start & ~(pagesize-1));
  mem = mm + (mem_start & (pagesize-1));

  if (mm == MAP_FAILED) {
    perror("mmap");
    return NULL;
  }

  fprintf(stderr, "mmap 0x%lx -> %p\n",mem_start,mem);
  return mem;
}

/* * */

int hw_init(unsigned f_s) {
  mem_open();
  mm_data = mem_map(DATA_ADDR, DATA_SIZE);
  mm_ctrl = mem_map(CTRL_ADDR, CTRL_SIZE);
  mm_dma_ctl = mem_map(DMA_CTL_ADDR, DMA_CTL_SIZE);
  mm_dma_data = mem_map(DMA_ADDR, DMA_SIZE);

  /* init regs */
  mm_ctrl[1] = 0;
  mm_ctrl[2] = 0;
  mm_ctrl[4] = f_s-1; //187500000-1;
  mm_ctrl[5] = 0;
  //mm_ctrl[6] = rx.f_s/50000; //187500000/50000; /* 20us pps pulse */
  __mb();
  mm_ctrl[0] = 0xffff;
  __mb();

  return 0;
}
