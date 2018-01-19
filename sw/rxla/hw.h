#define DATA_ADDR    0x40000000
#define DATA_SIZE        0x2000
#define CTRL_ADDR    0x43c00000
#define CTRL_SIZE          0x20
#define DMA_CTL_ADDR 0x40400000
#define DMA_CTL_SIZE       0x62
/* mem=420M */
//#define DMA_ADDR     0x1a400000
//#define DMA_SIZE      0x5c00000 /* >96000000 */
#define DMA_ADDR      0x20000000
#define DMA_SIZE      0x20000000 /* 512M @512M */

#define DATA_ADDR1    0x80000000
#define CTRL_ADDR1    0x83c00000
#define DMA_CTL_ADDR1 0x80400000
#define DMA_ADDR1     DMA_ADDR

#define SIG_BASE 0x10

extern uint32_t *mm_data, *mm_ctrl, *mm_dma_ctl, *mm_dma_data;
extern uint32_t *mm_dma_code;

#define __mb() \
 __asm__ __volatile__("dmb": : : "memory")

#define __mb_smp() \
 __asm__ __volatile__("dmb ish": : : "memory")

#define HW2PN

#ifdef HW2PN
#define ACC_N_WORDS 16
#else
#define ACC_N_WORDS 8
#endif

/*int mem_open();*/
int hw_init(unsigned f_s);
void *mem_map(unsigned long mem_start, unsigned long mem_length);
