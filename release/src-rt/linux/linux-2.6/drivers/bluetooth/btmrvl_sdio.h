/**
 * Marvell BT-over-SDIO driver: SDIO interface related definitions
 *
 * Copyright (C) 2009, Marvell International Ltd.
 *
 * This software file (the "File") is distributed by Marvell International
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991
 * (the "License").  You may use, redistribute and/or modify this File in
 * accordance with the terms and conditions of the License, a copy of which
 * is available by writing to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
 * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
 *
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 * this warranty disclaimer.
 *
 **/

#define SDIO_HEADER_LEN			4

/* SD block size can not bigger than 64 due to buf size limit in firmware */
/* define SD block size for data Tx/Rx */
#define SDIO_BLOCK_SIZE			64

/* Number of blocks for firmware transfer */
#define FIRMWARE_TRANSFER_NBLOCK	2

/* This is for firmware specific length */
#define FW_EXTRA_LEN			36

#define MRVDRV_SIZE_OF_CMD_BUFFER       (2 * 1024)

#define MRVDRV_BT_RX_PACKET_BUFFER_SIZE \
	(HCI_MAX_FRAME_SIZE + FW_EXTRA_LEN)

#define ALLOC_BUF_SIZE	(((max_t (int, MRVDRV_BT_RX_PACKET_BUFFER_SIZE, \
			MRVDRV_SIZE_OF_CMD_BUFFER) + SDIO_HEADER_LEN \
			+ SDIO_BLOCK_SIZE - 1) / SDIO_BLOCK_SIZE) \
			* SDIO_BLOCK_SIZE)

/* The number of times to try when polling for status */
#define MAX_POLL_TRIES			100

/* Max retry number of CMD53 write */
#define MAX_WRITE_IOMEM_RETRY		2

/* Host Control Registers */
#define IO_PORT_0_REG			0x00
#define IO_PORT_1_REG			0x01
#define IO_PORT_2_REG			0x02

#define CONFIG_REG			0x03
#define HOST_POWER_UP			BIT(1)
#define HOST_CMD53_FIN			BIT(2)

#define HOST_INT_MASK_REG		0x04
#define HIM_DISABLE			0xff
#define HIM_ENABLE			(BIT(0) | BIT(1))

#define HOST_INTSTATUS_REG		0x05
#define UP_LD_HOST_INT_STATUS		BIT(0)
#define DN_LD_HOST_INT_STATUS		BIT(1)

/* Card Control Registers */
#define SQ_READ_BASE_ADDRESS_A0_REG  	0x10
#define SQ_READ_BASE_ADDRESS_A1_REG  	0x11

#define CARD_STATUS_REG              	0x20
#define DN_LD_CARD_RDY               	BIT(0)
#define CARD_IO_READY              	BIT(3)

#define CARD_FW_STATUS0_REG		0x40
#define CARD_FW_STATUS1_REG		0x41
#define FIRMWARE_READY			0xfedc

#define CARD_RX_LEN_REG			0x42
#define CARD_RX_UNIT_REG		0x43


struct btmrvl_sdio_card {
	struct sdio_func *func;
	u32 ioport;
	const char *helper;
	const char *firmware;
	u8 rx_unit;
	struct btmrvl_private *priv;
};

struct btmrvl_sdio_device {
	const char *helper;
	const char *firmware;
};


/* Platform specific DMA alignment */
#define BTSDIO_DMA_ALIGN		8

/* Macros for Data Alignment : size */
#define ALIGN_SZ(p, a)	\
	(((p) + ((a) - 1)) & ~((a) - 1))

/* Macros for Data Alignment : address */
#define ALIGN_ADDR(p, a)	\
	((((unsigned long)(p)) + (((unsigned long)(a)) - 1)) & \
					~(((unsigned long)(a)) - 1))
