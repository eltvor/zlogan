#ifndef _ZLOGAN_HW_H_
#define _ZLOGAN_HW_H_

#include "hw_config.h"
extern uint32_t DMA_ADDR, DMA_SIZE;

//#define SIG_BASE 0x10

/*
 * Zlogan register map:
 * 0: 0x00: CR (rw)
 *   [0]: Zlogan Reset (active 1)
 *   [1]: FIFO Reset (active 1)
 *   [2]: DMA FSM Reset (active 1)
 *   [7:3]: Reserved
 *   [8]: Zlogan ENable (active 1)
 *   [9]: DMA FSM Trigger (active 1, does not reset automatically)
 *   [31:10]: Reserved
 * 1: 0x04: SR (ro)
 *   [0]: DMA Xrun fag
 *   [15:1]: Reserved
 *   [18:16]: DMA FSM state monitor (1: ST_WAIT_TRIG, 2: ST_STAMP, 4: ST_DATA)
 *   [31:19]: Reserved
 * 2: 0x08: LEN (rw)
 *   [29:0]: DMA length (in DMA words); copied into shadow register
 *           when DMA FSM Trigger is set and FSM is in ST_WAIT_TRIG
 *   [31:30]: Reserved
 * 3: 0x0c: INP (ro)
 *   [la_inp_n-1:0]: input word
 *   [31:la_inp_n]: 0
 * 4: 0x10: ID (ro)
 *   [ 7: 0]: Zlogan version (0x01)
 *   [15: 8]: DMA word size
 *   [23:16]: nsignals
 *   [31:24]: Reserved
 * 5: 0x14: FIFO_DATA_COUNT (debug, ro)
 * 6: 0x18: FIFO_RD_DATA_COUNT (debug, ro)
 * 7: 0x1c: FIFO_WR_DATA_COUNT (debug, ro)
 * 8: 0x20: SHADOW_LEN (debug, ro)
 *   [29:0]: shadow of LEN
 *   [31:30]: Reserved
 */

#define ZLOGAN_REG_CR                 0
#define ZLOGAN_REG_SR                 1
#define ZLOGAN_REG_LEN                2
#define ZLOGAN_REG_INP                3
#define ZLOGAN_REG_ID                 4
#define ZLOGAN_REG_FIFO_DATA_COUNT    5
#define ZLOGAN_REG_FIFO_RD_DATA_COUNT 6
#define ZLOGAN_REG_FIFO_WR_DATA_COUNT 7
#define ZLOGAN_REG_SHADOW_LEN         8

#define ZLOGAN_CR_LA_RST       0x0001
#define ZLOGAN_CR_FIFO_RST     0x0002
#define ZLOGAN_CR_DMAFSM_RST   0x0004
#define ZLOGAN_CR_EN           0x0100
#define ZLOGAN_CR_DMAFSM_TRIG  0x0200

#define ZLOGAN_SR_XRUN         0x01
#define ZLOGAN_SR_GET_FSM_STATE(r) (((r) >> 16) & 0x7)
#define ZLOGAN_ID_GET_VER(r)   (((r) >> 0) & 0xFF)
#define ZLOGAN_ID_GET_WS(r)    (((r) >> 8) & 0xFF)
#define ZLOGAN_ID_GET_NSIG(r)  (((r) >> 16) & 0xFF)

extern volatile uint32_t *mm_ctrl, *mm_dma_ctl, *mm_dma_data;

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

int hw_init();
void *mem_map(unsigned long mem_start, unsigned long mem_length);

/* From linux/drivers/dma/xilinx/xilinx_dma.c */

/* Register/Descriptor Offsets */
#define XILINX_DMA_MM2S_CTRL_OFFSET		0x0000
#define XILINX_DMA_S2MM_CTRL_OFFSET		0x0030

#define BIT(n) (1U << n)

/* Control Registers */
#define XILINX_DMA_REG_DMACR			0x0000
#define XILINX_DMA_DMACR_DELAY_MAX		0xff
#define XILINX_DMA_DMACR_DELAY_SHIFT		24
#define XILINX_DMA_DMACR_FRAME_COUNT_MAX	0xff
#define XILINX_DMA_DMACR_FRAME_COUNT_SHIFT	16
#define XILINX_DMA_DMACR_ERR_IRQ		BIT(14)
#define XILINX_DMA_DMACR_DLY_CNT_IRQ		BIT(13)
#define XILINX_DMA_DMACR_FRM_CNT_IRQ		BIT(12)
#define XILINX_DMA_DMACR_MASTER_SHIFT		8
#define XILINX_DMA_DMACR_FSYNCSRC_SHIFT	5
#define XILINX_DMA_DMACR_FRAMECNT_EN		BIT(4)
#define XILINX_DMA_DMACR_GENLOCK_EN		BIT(3)
#define XILINX_DMA_DMACR_RESET			BIT(2)
#define XILINX_DMA_DMACR_CIRC_EN		BIT(1)
#define XILINX_DMA_DMACR_RUNSTOP		BIT(0)
#define XILINX_DMA_DMACR_FSYNCSRC_MASK		GENMASK(6, 5)

#define XILINX_DMA_REG_DMASR			0x0004
#define XILINX_DMA_DMASR_EOL_LATE_ERR		BIT(15)
#define XILINX_DMA_DMASR_ERR_IRQ		BIT(14)
#define XILINX_DMA_DMASR_DLY_CNT_IRQ		BIT(13)
#define XILINX_DMA_DMASR_FRM_CNT_IRQ		BIT(12)
#define XILINX_DMA_DMASR_SOF_LATE_ERR		BIT(11)
#define XILINX_DMA_DMASR_SG_DEC_ERR		BIT(10)
#define XILINX_DMA_DMASR_SG_SLV_ERR		BIT(9)
#define XILINX_DMA_DMASR_EOF_EARLY_ERR		BIT(8)
#define XILINX_DMA_DMASR_SOF_EARLY_ERR		BIT(7)
#define XILINX_DMA_DMASR_DMA_DEC_ERR		BIT(6)
#define XILINX_DMA_DMASR_DMA_SLAVE_ERR		BIT(5)
#define XILINX_DMA_DMASR_DMA_INT_ERR		BIT(4)
#define XILINX_DMA_DMASR_IDLE			BIT(1)
#define XILINX_DMA_DMASR_HALTED		BIT(0)
#define XILINX_DMA_DMASR_DELAY_MASK		GENMASK(31, 24)
#define XILINX_DMA_DMASR_FRAME_COUNT_MASK	GENMASK(23, 16)

#define XILINX_DMA_REG_ADDR		0x0018
#define XILINX_DMA_REG_ADDR_MSB		0x001C
#define XILINX_DMA_REG_LENGTH		0x0028

#endif /* _ZLOGAN_HW_H_ */
