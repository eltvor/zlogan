#ifndef _RXLA_HW_CONFIG_H_
#define _RXLA_HW_CONFIG_H_

#error Configure correct values for your system and comment out this line.

/** Zlogan registers address. */
#define CTRL_ADDR    0x43c00000
#define CTRL_SIZE          0x20

/** DMA registers address. */
#define DMA_CTL_ADDR 0x40400000
#define DMA_CTL_SIZE       0x62

/** Memory for DMA transfers is obtained from /dev/udmabufN. */
#define UDMABUF_NUM           0

/*
 * This has to be set to the length of DMA transfer length register, which is
 * set in Vivado in IP parametrization. It defaults to 14 bits.
 * NOTE: If the dump ends with a bunch of zeros (or some garbage), you got it wrong.
 * TODO: detect the actual length
 */
#define DMA_LENGTH_BITS    23

#endif // _RXLA_HW_CONFIG_H_
