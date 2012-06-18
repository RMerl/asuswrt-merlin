/*
 * drivers/net/big_sur_ge.h - Driver for PMC-Sierra Big Sur
 * ethernet ports
 *
 * Copyright (C) 2003 PMC-Sierra Inc.
 * Author : Manish Lachwani (lachwani@pmc-sierra.com)
 * Copyright (C) 2005 Ralf Baechle (ralf@linux-mips.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef	__BIG_SUR_GE_H__
#define	__BIG_SUR_GE_H__

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#define	BIG_SUR_DEVICE_NAME	"big sur"
#define	BIG_SUR_DEVICE_DESC	"Big Sur Ethernet 10/100 MAC"

#define BIG_SUR_GE_BASE			0xbb000000

#define	BIG_SUR_GE_WRITE(ofs,data)	*(volatile u32 *)(BIG_SUR_GE_BASE+(ofs)) = data

#define	BIG_SUR_GE_READ(ofs)		*(volatile u32 *)(BIG_SUR_GE_BASE+(ofs))

/* Manish : Need to fix these defines later */
#define	BIG_SUR_GE_EMAC_0_HIGHADDR
#define	BIG_SUR_GE_EMAC_0_BASEADDR
#define	BIG_SUR_GE_INTC_0_EMAC_0_VEC_ID		1
#define	BIG_SUR_GE_INTC_1_EMAC_1_VEC_ID		2
#define	BIG_SUR_GE_INTC_2_EMAC_2_VEC_ID		3
#define	BIG_SUR_GE_EMAC_0_ERR_COUNT_EXIST
#define	BIG_SUR_GE_EMAC_0_DMA_PRESENT
#define	BIG_SUR_GE_EMAC_0_MII_EXIST
#define	BIG_SUR_GE_OPB_ETHERNET_0_BASEADDR
#define	BIG_SUR_GE_EMAC_0_DEVICE_ID
#define	BIG_SUR_GE_OPB_ETHERNET_0_ERR_COUNT_EXIST
#define	BIG_SUR_GE_OPB_ETHERNET_0_DMA_PRESENT
#define	BIG_SUR_GE_OPB_ETHERNET_0_MII_EXIST
#define	BIG_SUR_GE_OPB_ETHERNET_0_DEVICE_ID

#define	BIG_SUR_GE_FIFO_WIDTH_BYTE_COUNT	4UL
/* IPIF specific defines */
#define XIIF_V123B_DISR_OFFSET     0UL  /* device interrupt status register */
#define XIIF_V123B_DIPR_OFFSET     4UL  /* device interrupt pending register */
#define XIIF_V123B_DIER_OFFSET     8UL  /* device interrupt enable register */
#define XIIF_V123B_DIIR_OFFSET     24UL /* device interrupt ID register */
#define XIIF_V123B_DGIER_OFFSET    28UL /* device global interrupt enable reg */
#define XIIF_V123B_IISR_OFFSET     32UL /* IP interrupt status register */
#define XIIF_V123B_IIER_OFFSET     40UL /* IP interrupt enable register */
#define XIIF_V123B_RESETR_OFFSET   64UL /* reset register */
#define XIIF_V123B_RESET_MASK             0xAUL
#define	XIIF_V123B_ERROR_MASK		0x1UL

/* defines */
#define BIG_SUR_GE_UNICAST_OPTION        	0x00000001
#define BIG_SUR_GE_BROADCAST_OPTION      	0x00000002
#define BIG_SUR_GE_PROMISC_OPTION        	0x00000004
#define BIG_SUR_GE_FDUPLEX_OPTION        	0x00000008
#define BIG_SUR_GE_POLLED_OPTION         	0x00000010
#define BIG_SUR_GE_LOOPBACK_OPTION       	0x00000020
#define BIG_SUR_GE_FLOW_CONTROL_OPTION   	0x00000080
#define BIG_SUR_GE_INSERT_PAD_OPTION     	0x00000100
#define BIG_SUR_GE_INSERT_FCS_OPTION     	0x00000200
#define BIG_SUR_GE_INSERT_ADDR_OPTION    	0x00000400
#define BIG_SUR_GE_OVWRT_ADDR_OPTION     	0x00000800
#define BIG_SUR_GE_STRIP_PAD_FCS_OPTION  	0x00002000

/* Not Supported */
#define BIG_SUR_GE_MULTICAST_OPTION      	0x00000040
#define BIG_SUR_GE_FLOW_CONTROL_OPTION   	0x00000080
#define BIG_SUR_GE_INSERT_PAD_OPTION     	0x00000100
#define BIG_SUR_GE_INSERT_FCS_OPTION     	0x00000200
#define BIG_SUR_GE_INSERT_ADDR_OPTION    	0x00000400
#define BIG_SUR_GE_OVWRT_ADDR_OPTION     	0x00000800
#define BIG_SUR_GE_STRIP_PAD_OPTION      	0x00001000
#define BIG_SUR_GE_STRIP_FCS_OPTION     	0x00002000


/* Defaults for Interrupt Coalescing in the SG DMA Engine */
#define BIG_SUR_GE_SGDMA_DFT_THRESHOLD     1   /* Default pkt threshold */
#define BIG_SUR_GE_SGDMA_MAX_THRESHOLD     255 /* Maximum pkt theshold */
#define BIG_SUR_GE_SGDMA_DFT_WAITBOUND     5   /* Default pkt wait bound (msec) */
#define BIG_SUR_GE_SGDMA_MAX_WAITBOUND     1023        /* Maximum pkt wait bound (msec) */

/* Direction */
#define BIG_SUR_GE_SEND    1
#define BIG_SUR_GE_RECV    2

/* SG DMA */
#define BIG_SUR_GE_SGDMA_NODELAY     0 /* start SG DMA immediately */
#define BIG_SUR_GE_SGDMA_DELAY       1 /* do not start SG DMA */

#define BIG_SUR_GE_CFG_NO_IPIF             0   /* Not supported by the driver */
#define BIG_SUR_GE_CFG_NO_DMA              1   /* No DMA */
#define BIG_SUR_GE_CFG_SIMPLE_DMA          2   /* Simple DMA */
#define BIG_SUR_GE_CFG_DMA_SG              3   /* DMA scatter gather */

#define BIG_SUR_GE_MAC_ADDR_SIZE   6   /* six-byte MAC address */
#define BIG_SUR_GE_MTU             1500        /* max size of Ethernet frame */
#define BIG_SUR_GE_HDR_SIZE        14  /* size of Ethernet header */
#define BIG_SUR_GE_HDR_VLAN_SIZE   18  /* size of Ethernet header with VLAN */
#define BIG_SUR_GE_TRL_SIZE        4   /* size of Ethernet trailer (FCS) */
#define BIG_SUR_GE_MAX_FRAME_SIZE  \
		(BIG_SUR_GE_MTU + BIG_SUR_GE_HDR_SIZE + BIG_SUR_GE_TRL_SIZE)

#define BIG_SUR_GE_MAX_VLAN_FRAME_SIZE  \
		(BIG_SUR_GE_MTU + BIG_SUR_GE_HDR_VLAN_SIZE + BIG_SUR_GE_TRL_SIZE)

/* Send and Receive buffers */
#define BIG_SUR_GE_MIN_RECV_BUFS   32  /* minimum # of recv buffers */
#define BIG_SUR_GE_DFT_RECV_BUFS   64  /* default # of recv buffers */

#define BIG_SUR_GE_MIN_SEND_BUFS   16  /* minimum # of send buffers */
#define BIG_SUR_GE_DFT_SEND_BUFS   32  /* default # of send buffers */

#define BIG_SUR_GE_MIN_BUFFERS     (BIG_SUR_GE_MIN_RECV_BUFS + BIG_SUR_GE_MIN_SEND_BUFS)
#define BIG_SUR_GE_DFT_BUFFERS     (BIG_SUR_GE_DFT_RECV_BUFS + BIG_SUR_GE_DFT_SEND_BUFS)

/* Send and Receive Descriptors */
#define BIG_SUR_GE_MIN_RECV_DESC   16  /* minimum # of recv descriptors */
#define BIG_SUR_GE_DFT_RECV_DESC   32  /* default # of recv descriptors */

#define BIG_SUR_GE_MIN_SEND_DESC   8   /* minimum # of send descriptors */
#define BIG_SUR_GE_DFT_SEND_DESC   16  /* default # of send descriptors */

/* FIFO Specific Defines */
#define BIG_SUR_GE_READ_FIFO_TYPE      0       /* a read FIFO */
#define BIG_SUR_GE_WRITE_FIFO_TYPE     1       /* a write FIFO */
#define BIG_SUR_GE_RESET_REG_OFFSET            0UL
#define BIG_SUR_GE_MODULE_INFO_REG_OFFSET      0UL
#define BIG_SUR_GE_COUNT_STATUS_REG_OFFSET     4UL
#define BIG_SUR_GE_RESET_FIFO_MASK             0x0000000A
#define BIG_SUR_GE_COUNT_MASK                  0x0000FFFF
#define BIG_SUR_GE_DEADLOCK_MASK               0x20000000
#define BIG_SUR_GE_ALMOST_EMPTY_FULL_MASK      0x40000000
#define BIG_SUR_GE_EMPTY_FULL_MASK             0x80000000

#define BIG_SUR_GE_FIFO_RESET(fifo)	\
	BIG_SUR_GE_WRITE((fifo)->reg_base_addr + BIG_SUR_GE_RESET_REG_OFFSET, BIG_SUR_GE_RESET_FIFO_MASK)

#define	BIG_SUR_GE_GET_COUNT(fifo)	\
	(BIG_SUR_GE_READ((fifo)->reg_base_addr + BIG_SUR_GE_COUNT_STATUS_REG_OFFSET) & 	\
							BIG_SUR_GE_COUNT_MASK)

#define	BIG_SUR_GE_IS_ALMOST_EMPTY(fifo)	\
		(BIG_SUR_GE_READ(fifo->reg_base_addr + BIG_SUR_GE_COUNT_STATUS_REG_OFFSET) &	\
							BIG_SUR_GE_ALMOST_EMPTY_FULL_MASK)

#define	BIG_SUR_GE_IS_ALMOST_FULL(fifo)  \
		(BIG_SUR_GE_READ(fifo->reg_base_addr + BIG_SUR_GE_COUNT_STATUS_REG_OFFSET) &   \
							BIG_SUR_GE_ALMOST_EMPTY_FULL_MASK)

#define BIG_SUR_GE_IS_EMPTY(fifo)  \
		(BIG_SUR_GE_READ(fifo->reg_base_addr + BIG_SUR_GE_COUNT_STATUS_REG_OFFSET) &   \
							BIG_SUR_GE_EMPTY_FULL_MASK)

#define BIG_SUR_GE_IS_FULL(fifo)  \
	(BIG_SUR_GE_READ(fifo->reg_base_addr + BIG_SUR_GE_COUNT_STATUS_REG_OFFSET) &   \
							BIG_SUR_GE_EMPTY_FULL_MASK)

#define	BIG_SUR_GE_IS_DEADLOCKED(fifo)	\
	(BIG_SUR_GE_READ((fifo)->reg_base_addr + BIG_SUR_GE_COUNT_STATUS_REG_OFFSET) &   \
							BIG_SUR_GE_DEADLOCK_MASK)

/* Device Config */
typedef struct _big_sur_ge_config {
	u16		device_id;
	u32		base_address;
	u32		has_counters;
	u32		has_sg_dma;
	u8		dma_config;
	u32		has_mii;
} big_sur_ge_config;

#define BIG_SUR_GE_SIZE_IN_WORDS           10
typedef unsigned long xbuf_descriptor[BIG_SUR_GE_SIZE_IN_WORDS];

/* Callback Functions */
typedef void (*big_sur_sg_handler) (void *callback, xbuf_descriptor *desc, u32 num_desc);
typedef	void (*big_sur_fifo_handler) (void *callback);
typedef void (*big_sur_irq_handler) (void *instance);

typedef struct _xdma_channel_tag {
	u32			reg_base_address;
        u32                     base_address;
        u32                     ready;
        xbuf_descriptor         *put_ptr;
        xbuf_descriptor         *get_ptr;
        xbuf_descriptor         *commit_ptr;
        xbuf_descriptor         *last_ptr;

        u32                     total_desc_count;
        u32                     active_desc_count;
} xdma_channel;

typedef struct _packet_fifo {
        u32             reg_base_addr;
        u32             ready_status;
        u32             data_base_address;
} packet_fifo;


/* Big Sur GE driver structure */
typedef struct _big_sur_ge {
	u32		base_address;
	u32		started;
	u32		ready;
	u32		polled;
	u32		dma_sg;

	u8		dma_config;
	u32		has_mii;
	u32		has_mcast_hash_table;

	/* For the FIFO and simple DMA case only */
	packet_fifo	recv_fifo;
	packet_fifo	send_fifo;

	big_sur_fifo_handler	big_sur_ge_fifo_recv_handler;
	big_sur_fifo_handler	big_sur_ge_fifo_send_handler;

	void	*fifo_send_ref;
	void	*fifo_recv_ref;	

	/* For SG DMA only */
	xdma_channel	recv_channel;
	xdma_channel	send_channel;
} big_sur_ge;

/* Offset of the MAC registers from the IPIF base address */
#define BIG_SUR_GE_REG_OFFSET     0x1100UL

/*
 * Register offsets for the Ethernet MAC. Each register is 32 bits.
 */
#define BIG_SUR_GE_EMIR_OFFSET   (BIG_SUR_GE_REG_OFFSET + 0x0)        /* EMAC Module ID */
#define BIG_SUR_GE_ECR_OFFSET    (BIG_SUR_GE_REG_OFFSET + 0x4)        /* MAC Control */
#define BIG_SUR_GE_IFGP_OFFSET   (BIG_SUR_GE_REG_OFFSET + 0x8)        /* Interframe Gap */
#define BIG_SUR_GE_SAH_OFFSET    (BIG_SUR_GE_REG_OFFSET + 0xC)        /* Station addr, high */
#define BIG_SUR_GE_SAL_OFFSET    (BIG_SUR_GE_REG_OFFSET + 0x10)       /* Station addr, low */
#define BIG_SUR_GE_MGTCR_OFFSET  (BIG_SUR_GE_REG_OFFSET + 0x14)       /* MII mgmt control */
#define BIG_SUR_GE_MGTDR_OFFSET  (BIG_SUR_GE_REG_OFFSET + 0x18)       /* MII mgmt data */
#define BIG_SUR_GE_RPLR_OFFSET   (BIG_SUR_GE_REG_OFFSET + 0x1C)       /* Rx packet length */
#define BIG_SUR_GE_TPLR_OFFSET   (BIG_SUR_GE_REG_OFFSET + 0x20)       /* Tx packet length */
#define BIG_SUR_GE_TSR_OFFSET    (BIG_SUR_GE_REG_OFFSET + 0x24)       /* Tx status */
#define BIG_SUR_GE_RMFC_OFFSET   (BIG_SUR_GE_REG_OFFSET + 0x28)       /* Rx missed frames */
#define BIG_SUR_GE_RCC_OFFSET    (BIG_SUR_GE_REG_OFFSET + 0x2C)       /* Rx collisions */
#define BIG_SUR_GE_RFCSEC_OFFSET (BIG_SUR_GE_REG_OFFSET + 0x30)       /* Rx FCS errors */
#define BIG_SUR_GE_RAEC_OFFSET   (BIG_SUR_GE_REG_OFFSET + 0x34)       /* Rx alignment errors */
#define BIG_SUR_GE_TEDC_OFFSET   (BIG_SUR_GE_REG_OFFSET + 0x38)       /* Transmit excess
                                                         * deferral cnt */
/*
 * Register offsets for the IPIF components
 */
#define BIG_SUR_GE_ISR_OFFSET           0x20UL /* Interrupt status */

#define BIG_SUR_GE_DMA_OFFSET           0x2300UL
#define BIG_SUR_GE_DMA_SEND_OFFSET      (BIG_SUR_GE_DMA_OFFSET + 0x0) /* DMA send channel */
#define BIG_SUR_GE_DMA_RECV_OFFSET      (BIG_SUR_GE_DMA_OFFSET + 0x40)        /* DMA recv channel */

#define BIG_SUR_GE_PFIFO_OFFSET         0x2000UL
#define BIG_SUR_GE_PFIFO_TXREG_OFFSET   (BIG_SUR_GE_PFIFO_OFFSET + 0x0)       /* Tx registers */
#define BIG_SUR_GE_PFIFO_RXREG_OFFSET   (BIG_SUR_GE_PFIFO_OFFSET + 0x10)      /* Rx registers */
#define BIG_SUR_GE_PFIFO_TXDATA_OFFSET  (BIG_SUR_GE_PFIFO_OFFSET + 0x100)     /* Tx keyhole */
#define BIG_SUR_GE_PFIFO_RXDATA_OFFSET  (BIG_SUR_GE_PFIFO_OFFSET + 0x200)     /* Rx keyhole */

/*
 * EMAC Module Identification Register (EMIR)
 */
#define BIG_SUR_GE_EMIR_VERSION_MASK    0xFFFF0000UL   /* Device version */
#define BIG_SUR_GE_EMIR_TYPE_MASK       0x0000FF00UL   /* Device type */

/*
 * EMAC Control Register (ECR)
 */
#define BIG_SUR_GE_ECR_FULL_DUPLEX_MASK         0x80000000   /* Full duplex mode */
#define BIG_SUR_GE_ECR_XMIT_RESET_MASK          0x40000000   /* Reset transmitter */
#define BIG_SUR_GE_ECR_XMIT_ENABLE_MASK         0x20000000   /* Enable transmitter */
#define BIG_SUR_GE_ECR_RECV_RESET_MASK          0x10000000   /* Reset receiver */
#define BIG_SUR_GE_ECR_RECV_ENABLE_MASK         0x08000000   /* Enable receiver */
#define BIG_SUR_GE_ECR_PHY_ENABLE_MASK          0x04000000   /* Enable PHY */
#define BIG_SUR_GE_ECR_XMIT_PAD_ENABLE_MASK     0x02000000   /* Enable xmit pad insert */
#define BIG_SUR_GE_ECR_XMIT_FCS_ENABLE_MASK     0x01000000   /* Enable xmit FCS insert */
#define BIG_SUR_GE_ECR_XMIT_ADDR_INSERT_MASK    0x00800000   /* Enable xmit source addr insertion */
#define BIG_SUR_GE_ECR_XMIT_ERROR_INSERT_MASK   0x00400000   /* Insert xmit error */
#define BIG_SUR_GE_ECR_XMIT_ADDR_OVWRT_MASK     0x00200000   /* Enable xmit source addr overwrite */
#define BIG_SUR_GE_ECR_LOOPBACK_MASK            0x00100000   /* Enable internal loopback */
#define BIG_SUR_GE_ECR_RECV_PAD_ENABLE_MASK     0x00080000   /* Enable recv pad strip */
#define BIG_SUR_GE_ECR_RECV_FCS_ENABLE_MASK     0x00040000   /* Enable recv FCS strip */
#define BIG_SUR_GE_ECR_RECV_STRIP_ENABLE_MASK   0x00080000   /* Enable recv pad/fcs strip */
#define BIG_SUR_GE_ECR_UNICAST_ENABLE_MASK      0x00020000   /* Enable unicast addr */
#define BIG_SUR_GE_ECR_MULTI_ENABLE_MASK        0x00010000   /* Enable multicast addr */
#define BIG_SUR_GE_ECR_BROAD_ENABLE_MASK        0x00008000   /* Enable broadcast addr */
#define BIG_SUR_GE_ECR_PROMISC_ENABLE_MASK      0x00004000   /* Enable promiscuous mode */
#define BIG_SUR_GE_ECR_RECV_ALL_MASK            0x00002000   /* Receive all frames */
#define BIG_SUR_GE_ECR_RESERVED2_MASK           0x00001000   /* Reserved */
#define BIG_SUR_GE_ECR_MULTI_HASH_ENABLE_MASK   0x00000800   /* Enable multicast hash */
#define BIG_SUR_GE_ECR_PAUSE_FRAME_MASK         0x00000400   /* Interpret pause frames */
#define BIG_SUR_GE_ECR_CLEAR_HASH_MASK          0x00000200   /* Clear hash table */
#define BIG_SUR_GE_ECR_ADD_HASH_ADDR_MASK       0x00000100  /* Add hash table address */

/*
 * Interframe Gap Register (IFGR)
 */
#define BIG_SUR_GE_IFGP_PART1_MASK         0xF8000000        /* Interframe Gap Part1 */
#define BIG_SUR_GE_IFGP_PART1_SHIFT        27
#define BIG_SUR_GE_IFGP_PART2_MASK         0x07C00000        /* Interframe Gap Part2 */
#define BIG_SUR_GE_IFGP_PART2_SHIFT        22

/*
 * Station Address High Register (SAH)
 */
#define BIG_SUR_GE_SAH_ADDR_MASK           0x0000FFFF        /* Station address high bytes */

/*
 * Station Address Low Register (SAL)
 */
#define BIG_SUR_GE_SAL_ADDR_MASK           0xFFFFFFFF        /* Station address low bytes */

/*
 * MII Management Control Register (MGTCR)
 */
#define BIG_SUR_GE_MGTCR_START_MASK        0x80000000        /* Start/Busy */
#define BIG_SUR_GE_MGTCR_RW_NOT_MASK       0x40000000        /* Read/Write Not (direction) */
#define BIG_SUR_GE_MGTCR_PHY_ADDR_MASK     0x3E000000        /* PHY address */
#define BIG_SUR_GE_MGTCR_PHY_ADDR_SHIFT    25  /* PHY address shift */
#define BIG_SUR_GE_MGTCR_REG_ADDR_MASK     0x01F00000        /* Register address */
#define BIG_SUR_GE_MGTCR_REG_ADDR_SHIFT    20  /* Register addr shift */
#define BIG_SUR_GE_MGTCR_MII_ENABLE_MASK   0x00080000        /* Enable MII from EMAC */
#define BIG_SUR_GE_MGTCR_RD_ERROR_MASK     0x00040000        /* MII mgmt read error */

/*
 * MII Management Data Register (MGTDR)
 */
#define BIG_SUR_GE_MGTDR_DATA_MASK         0x0000FFFF        /* MII data */

/*
 * Receive Packet Length Register (RPLR)
 */
#define BIG_SUR_GE_RPLR_LENGTH_MASK        0x0000FFFF        /* Receive packet length */

/*
 * Transmit Packet Length Register (TPLR)
 */
#define BIG_SUR_GE_TPLR_LENGTH_MASK        0x0000FFFF       /* Transmit packet length */

/*
 * Transmit Status Register (TSR)
 */
#define BIG_SUR_GE_TSR_EXCESS_DEFERRAL_MASK 0x80000000       /* Transmit excess deferral */
#define BIG_SUR_GE_TSR_FIFO_UNDERRUN_MASK   0x40000000       /* Packet FIFO underrun */
#define BIG_SUR_GE_TSR_ATTEMPTS_MASK        0x3E000000      /* Transmission attempts */
#define BIG_SUR_GE_TSR_LATE_COLLISION_MASK  0x01000000      /* Transmit late collision */

/*
 * Receive Missed Frame Count (RMFC)
 */
#define BIG_SUR_GE_RMFC_DATA_MASK          0x0000FFFF

/*
 * Receive Collision Count (RCC)
 */
#define BIG_SUR_GE_RCC_DATA_MASK           0x0000FFFF
/*
 * Receive FCS Error Count (RFCSEC)
 */
#define BIG_SUR_GE_RFCSEC_DATA_MASK        0x0000FFFF

/*
 * Receive Alignment Error Count (RALN)
 */
#define BIG_SUR_GE_RAEC_DATA_MASK          0x0000FFFF

/*
 * Transmit Excess Deferral Count (TEDC)
 */
#define BIG_SUR_GE_TEDC_DATA_MASK          0x0000FFFF

/*
 * EMAC Interrupt Registers (Status and Enable) masks. These registers are
 * part of the IPIF IP Interrupt registers
 */
#define BIG_SUR_GE_EIR_XMIT_DONE_MASK         0x00000001     /* Xmit complete */
#define BIG_SUR_GE_EIR_RECV_DONE_MASK         0x00000002     /* Recv complete */
#define BIG_SUR_GE_EIR_XMIT_ERROR_MASK        0x00000004     /* Xmit error */
#define BIG_SUR_GE_EIR_RECV_ERROR_MASK        0x00000008     /* Recv error */
#define BIG_SUR_GE_EIR_XMIT_SFIFO_EMPTY_MASK  0x00000010     /* Xmit status fifo empty */
#define BIG_SUR_GE_EIR_RECV_LFIFO_EMPTY_MASK  0x00000020     /* Recv length fifo empty */
#define BIG_SUR_GE_EIR_XMIT_LFIFO_FULL_MASK   0x00000040     /* Xmit length fifo full */
#define BIG_SUR_GE_EIR_RECV_LFIFO_OVER_MASK   0x00000080     /* Recv length fifo overrun */
#define BIG_SUR_GE_EIR_RECV_LFIFO_UNDER_MASK  0x00000100     /* Recv length fifo underrun */
#define BIG_SUR_GE_EIR_XMIT_SFIFO_OVER_MASK   0x00000200     /* Xmit status fifo overrun */
#define BIG_SUR_GE_EIR_XMIT_SFIFO_UNDER_MASK  0x00000400     /* Transmit status fifo underrun */
#define BIG_SUR_GE_EIR_XMIT_LFIFO_OVER_MASK   0x00000800     /* Transmit length fifo overrun */
#define BIG_SUR_GE_EIR_XMIT_LFIFO_UNDER_MASK  0x00001000     /* Transmit length fifo underrun */
#define BIG_SUR_GE_EIR_XMIT_PAUSE_MASK        0x00002000     /* Transmit pause pkt received */
#define BIG_SUR_GE_EIR_RECV_DFIFO_OVER_MASK   0x00004000     /* Receive data fifo overrun */
#define BIG_SUR_GE_EIR_RECV_MISSED_FRAME_MASK 0x00008000     /* Receive missed frame error */
#define BIG_SUR_GE_EIR_RECV_COLLISION_MASK    0x00010000     /* Receive collision error */
#define BIG_SUR_GE_EIR_RECV_FCS_ERROR_MASK    0x00020000     /* Receive FCS error */
#define BIG_SUR_GE_EIR_RECV_LEN_ERROR_MASK    0x00040000     /* Receive length field error */
#define BIG_SUR_GE_EIR_RECV_SHORT_ERROR_MASK  0x00080000     /* Receive short frame error */
#define BIG_SUR_GE_EIR_RECV_LONG_ERROR_MASK   0x00100000     /* Receive long frame error */
#define BIG_SUR_GE_EIR_RECV_ALIGN_ERROR_MASK  0x00200000     /* Receive alignment error */

#define	BIG_SUR_GE_READ_REG(base_addr, reg_offset)	\
		BIG_SUR_GE_READ(base_addr + reg_offset)

#define	BIG_SUR_GE_WRITE_REG(base_addr, reg_offset, data)	\
		 BIG_SUR_GE_WRITE(base_addr + reg_offset, data)

#define BIG_SUR_GE_CONTROL_REG(base_addr, mask)		\
		BIG_SUR_GE_WRITE(base_addr + BIG_SUR_GE_ECR_OFFSET, mask)

/* Set the MAC Address */
#define	big_sur_ge_set_mac(base_addr, address)					\
{										\
	u32	mac_addr;							\
										\
	mac_addr = ((address[0] << 8) | (address[1]);				\
	BIG_SUR_GE_WRITE(base_address + BIG_SUR_GE_SAH_OFFSET, mac_address);		\
										\
	mac_addr = ((address[2] << 24) | (address[3] << 16) |			\
			(address[4] << 8) | address[5]);			\
										\
	BIG_SUR_GE_WRITE(base_address + BIG_SUR_GE_SAL_OFFSET, mac_address);		\
										\
}										

/* Enable the MAC unit */
#define	big_sur_ge_mac_enable(base_address)					\
{										\
	u32	control;							\
	control = BIG_SUR_GE_READ(base_address + BIG_SUR_GE_ECR_OFFSET);		\
	control &= ~(BIG_SUR_GE_ECR_XMIT_RESET_MASK | BIG_SUR_GE_ECR_RECV_RESET_MASK);	\
	control |= (BIG_SUR_GE_ECR_XMIT_ENABLE_MASK | BIG_SUR_GE_ECR_RECV_ENABLE_MASK);	\
	BIG_SUR_GE_WRITE(base_address + BIG_SUR_GE_ECR_OFFSET, control);		\
}

/* Disable the MAC unit */
#define	big_sur_ge_mac_disable(base_address)					\
{										\
	u32	control;							\
	control = BIG_SUR_GE_READ(base_address + BIG_SUR_GE_ECR_OFFSET);		\
	control &= ~(BIG_SUR_GE_ECR_XMIT_ENABLE_MASK | BIG_SUR_GE_ECR_RECV_ENABLE_MASK);	\
	BIG_SUR_GE_WRITE(base_address + BIG_SUR_GE_ECR_OFFSET, control);		\
}

/* Check if the Tx is done */
#define	big_sur_ge_tx_done(base_address)						\
	(BIG_SUR_GE_READ(base_address + BIG_SUR_GE_ISR_OFFSET) & BIG_SUR_GE_EIR_XMIT_DONE_MASK)


/* Check if Rx FIFO is empty */
#define	big_sur_ge_rx_empty(base_address)						\
	(!(BIG_SUR_GE_READ(base_address + BIG_SUR_GE_ISR_OFFSET) & BIG_SUR_GE_EIR_RECV_DONE_MASK))

/* Reset the MAC PHY */
#define	big_sur_ge_reset_phy(base_address)						\
{											\
	u32	control;								\
	control = BIG_SUR_GE_READ(base_address + BIG_SUR_GE_ECR_OFFSET);			\
	control &= ~(BIG_SUR_GE_ECR_PHY_ENABLE_MASK);						\
	BIG_SUR_GE_WRITE(base_address + BIG_SUR_GE_ECR_OFFSET, control);			\
	control |= BIG_SUR_GE_ECR_PHY_ENABLE_MASK;						\
	BIG_SUR_GE_WRITE(base_address + BIG_SUR_GE_ECR_OFFSET, control);			\
}

/* DMA SG defines */
#define BIG_SUR_GE_CONTROL_LAST_BD_MASK        0x02000000
#define BIG_SUR_GE_STATUS_LAST_BD_MASK         0x10000000
#define BIG_SUR_GE_RST_REG_OFFSET      0       /* reset register */
#define BIG_SUR_GE_MI_REG_OFFSET       0       /* module information register */
#define BIG_SUR_GE_DMAC_REG_OFFSET     4       /* DMA control register */
#define BIG_SUR_GE_SA_REG_OFFSET       8       /* source address register */
#define BIG_SUR_GE_DA_REG_OFFSET       12      /* destination address register */
#define BIG_SUR_GE_LEN_REG_OFFSET      16      /* length register */
#define BIG_SUR_GE_DMAS_REG_OFFSET     20      /* DMA status register */
#define BIG_SUR_GE_BDA_REG_OFFSET      24      /* buffer descriptor address register */
#define BIG_SUR_GE_SWCR_REG_OFFSET 28  /* software control register */
#define BIG_SUR_GE_UPC_REG_OFFSET      32      /* unserviced packet count register */
#define BIG_SUR_GE_PCT_REG_OFFSET      36      /* packet count threshold register */
#define BIG_SUR_GE_PWB_REG_OFFSET      40      /* packet wait bound register */
#define BIG_SUR_GE_IS_REG_OFFSET       44      /* interrupt status register */
#define BIG_SUR_GE_IE_REG_OFFSET       48      /* interrupt enable register */

#define BIG_SUR_GE_RESET_MASK                          0x0000000A

/* Buffer Descriptor Control */

#define BIG_SUR_GE_DEVICE_STATUS_OFFSET    0
#define BIG_SUR_GE_CONTROL_OFFSET          1
#define BIG_SUR_GE_SOURCE_OFFSET           2
#define BIG_SUR_GE_DESTINATION_OFFSET      3
#define BIG_SUR_GE_LENGTH_OFFSET           4
#define BIG_SUR_GE_STATUS_OFFSET           5
#define BIG_SUR_GE_NEXT_PTR_OFFSET         6
#define BIG_SUR_GE_ID_OFFSET               7
#define BIG_SUR_GE_FLAGS_OFFSET            8
#define BIG_SUR_GE_RQSTED_LENGTH_OFFSET    9

#define BIG_SUR_GE_FLAGS_LOCKED_MASK       1

#define	xbuf_descriptor_init(base)				\
{								\
	(*((u32 *)base + BIG_SUR_GE_CONTROL_OFFSET) = 0);		\
	(*((u32 *)base + BIG_SUR_GE_SOURCE_OFFSET) = 0);        \
    	(*((u32 *)base + BIG_SUR_GE_DESTINATION_OFFSET) = 0);   \
    	(*((u32 *)base + BIG_SUR_GE_LENGTH_OFFSET) = 0);        \
    	(*((u32 *)base + BIG_SUR_GE_STATUS_OFFSET) = 0);        \
    	(*((u32 *)base + BIG_SUR_GE_DEVICE_STATUS_OFFSET) = 0); \
    	(*((u32 *)base + BIG_SUR_GE_NEXT_PTR_OFFSET) = 0);      \
    	(*((u32 *)base + BIG_SUR_GE_ID_OFFSET) = 0);            \
    	(*((u32 *)base + BIG_SUR_GE_FLAGS_OFFSET) = 0);         \
    	(*((u32 *)base + BIG_SUR_GE_RQSTED_LENGTH_OFFSET) = 0); \
}

#define xbuf_descriptor_GetControl(base)   \
    (u32)(*((u32 *)base + BIG_SUR_GE_CONTROL_OFFSET))

#define xbuf_descriptor_SetControl(base, Control)  \
    (*((u32 *)base + BIG_SUR_GE_CONTROL_OFFSET) = (u32)Control)

#define xbuf_descriptor_IsLastControl(base) \
    (u32)(*((u32 *)base + BIG_SUR_GE_CONTROL_OFFSET) & \
               BIG_SUR_GE_CONTROL_LAST_BD_MASK)

#define xbuf_descriptor_SetLast(base) \
    (*((u32 *)base + BIG_SUR_GE_CONTROL_OFFSET) |= BIG_SUR_GECONTROL_LAST_BD_MASK)

#define xbuf_descriptor_GetSrcAddress(base) \
    ((u32 *)(*((u32 *)base + BIG_SUR_GE_SOURCE_OFFSET)))

#define xbuf_descriptor_SetSrcAddress(base, Source) \
    (*((u32 *)base + BIG_SUR_GE_SOURCE_OFFSET) = (u32)Source)

#define xbuf_descriptor_GetDestAddress(base) \
    ((u32 *)(*((u32 *)base + BIG_SUR_GE_DESTINATION_OFFSET)))

#define xbuf_descriptor_SetDestAddress(base, Destination) \
    (*((u32 *)base + BIG_SUR_GE_DESTINATION_OFFSET) = (u32)Destination)

#define xbuf_descriptor_GetLength(base)                           \
    (u32)(*((u32 *)base + BIG_SUR_GE_RQSTED_LENGTH_OFFSET) -    \
              *((u32 *)base + BIG_SUR_GE_LENGTH_OFFSET))

#define xbuf_descriptor_SetLength(base, Length)                       \
{                                                                           \
    (*((u32 *)base + BIG_SUR_GE_LENGTH_OFFSET) = (u32)(Length));    \
    (*((u32 *)base + BIG_SUR_GE_RQSTED_LENGTH_OFFSET) = (u32)(Length));\
}

#define xbuf_descriptor_GetStatus(base)    \
    (u32)(*((u32 *)base + BIG_SUR_GE_STATUS_OFFSET))

#define xbuf_descriptor_SetStatus(base, Status)    \
    (*((u32 *)base + BIG_SUR_GE_STATUS_OFFSET) = (u32)Status)

#define xbuf_descriptor_IsLastStatus(base) \
    (u32)(*((u32 *)base + BIG_SUR_GE_STATUS_OFFSET) & \
               BIG_SUR_GE_STATUS_LAST_BD_MASK)

#define xbuf_descriptor_GetDeviceStatus(base) \
    ((u32)(*((u32 *)base + BIG_SUR_GE_DEVICE_STATUS_OFFSET)))

#define xbuf_descriptor_SetDeviceStatus(base, Status) \
    (*((u32 *)base + BIG_SUR_GE_DEVICE_STATUS_OFFSET) = (u32)Status)

#define xbuf_descriptor_GetNextPtr(base) \
    (xbuf_descriptor *)(*((u32 *)base + BIG_SUR_GE_NEXT_PTR_OFFSET))

#define xbuf_descriptor_SetNextPtr(base, NextPtr) \
    (*((u32 *)base + BIG_SUR_GE_NEXT_PTR_OFFSET) = (u32)NextPtr)

#define xbuf_descriptor_GetId(base) \
    (u32)(*((u32 *)base + BIG_SUR_GE_ID_OFFSET))

#define xbuf_descriptor_SetId(base, Id) \
    (*((u32 *)base + BIG_SUR_GE_ID_OFFSET) = (u32)Id)

#define xbuf_descriptor_GetFlags(base) \
    (u32)(*((u32 *)base + BIG_SUR_GE_FLAGS_OFFSET))

#define xbuf_descriptor_SetFlags(base, Flags) \
    (*((u32 *)base + BIG_SUR_GE_FLAGS_OFFSET) = (u32)Flags)

#define xbuf_descriptor_Lock(base) \
    (*((u32 *)base + BIG_SUR_GE_FLAGS_OFFSET) |= BIG_SUR_GE_FLAGS_LOCKED_MASK)

#define xbuf_descriptor_Unlock(base) \
    (*((u32 *)base + BIG_SUR_GE_FLAGS_OFFSET) &= ~BIG_SUR_GE_FLAGS_LOCKED_MASK)

#define xbuf_descriptor_IsLocked(base) \
	(*((u32 *)base + BIG_SUR_GE_FLAGS_OFFSET) & BIG_SUR_GE_FLAGS_LOCKED_MASK)

#define BIG_SUR_GE_DMACR_SOURCE_INCR_MASK      0x80000000UL    /* increment source address */
#define BIG_SUR_GE_DMACR_DEST_INCR_MASK        0x40000000UL    /* increment dest address */
#define BIG_SUR_GE_DMACR_SOURCE_LOCAL_MASK 0x20000000UL        /* local source address */
#define BIG_SUR_GE_DMACR_DEST_LOCAL_MASK       0x10000000UL    /* local dest address */
#define BIG_SUR_GE_DMACR_SG_DISABLE_MASK       0x08000000UL    /* scatter gather disable */
#define BIG_SUR_GE_DMACR_GEN_BD_INTR_MASK      0x04000000UL    /* descriptor interrupt */
#define BIG_SUR_GE_DMACR_LAST_BD_MASK          BIG_SUR_GE_CONTROL_LAST_BD_MASK        /* last buffer */
#define BIG_SUR_GE_DMASR_BUSY_MASK                     0x80000000UL    /* channel is busy */
#define BIG_SUR_GE_DMASR_BUS_ERROR_MASK        0x40000000UL    /* bus error occurred */
#define BIG_SUR_GE_DMASR_BUS_TIMEOUT_MASK      0x20000000UL    /* bus timeout occurred */
#define BIG_SUR_GE_DMASR_LAST_BD_MASK          BIG_SUR_GE_STATUS_LAST_BD_MASK /* last buffer */
#define BIG_SUR_GE_DMASR_SG_BUSY_MASK          0x08000000UL    /* scatter gather is busy */
#define BIG_SUR_GE_IXR_DMA_DONE_MASK           0x1UL   /* dma operation done */
#define BIG_SUR_GE_IXR_DMA_ERROR_MASK      0x2UL       /* dma operation error */
#define BIG_SUR_GE_IXR_PKT_DONE_MASK       0x4UL       /* packet done */
#define BIG_SUR_GE_IXR_PKT_THRESHOLD_MASK      0x8UL   /* packet count threshold */
#define BIG_SUR_GE_IXR_PKT_WAIT_BOUND_MASK 0x10UL      /* packet wait bound reached */
#define BIG_SUR_GE_IXR_SG_DISABLE_ACK_MASK 0x20UL      /* scatter gather disable
                                                   acknowledge occurred */
#define BIG_SUR_GEIXR_SG_END_MASK                     0x40UL  /* last buffer descriptor
                                                           disabled scatter gather */
#define BIG_SUR_GEIXR_BD_MASK                         0x80UL  /* buffer descriptor done */

/* BD control */
#define BIG_SUR_GE_DFT_SEND_BD_MASK    (BIG_SUR_GEDMACR_SOURCE_INCR_MASK | \
                                 BIG_SUR_GEDMACR_DEST_LOCAL_MASK)
#define BIG_SUR_GE_DFT_RECV_BD_MASK    (BIG_SUR_GEDMACR_DEST_INCR_MASK |  \
                                 BIG_SUR_GEDMACR_SOURCE_LOCAL_MASK)

/* Interrupts */
#define BIG_SUR_GE_IPIF_EMAC_MASK      0x00000004UL    /* MAC interrupt */
#define BIG_SUR_GE_IPIF_SEND_DMA_MASK  0x00000008UL    /* Send DMA interrupt */
#define BIG_SUR_GE_IPIF_RECV_DMA_MASK  0x00000010UL    /* Receive DMA interrupt */
#define BIG_SUR_GE_IPIF_RECV_FIFO_MASK 0x00000020UL    /* Receive FIFO interrupt */
#define BIG_SUR_GE_IPIF_SEND_FIFO_MASK 0x00000040UL    /* Send FIFO interrupt */

#define BIG_SUR_GE_IPIF_DMA_DFT_MASK   (BIG_SUR_GE_IPIF_SEND_DMA_MASK |   \
                                 BIG_SUR_GE_IPIF_RECV_DMA_MASK |   \
                                 BIG_SUR_GE_IPIF_EMAC_MASK |       \
                                 BIG_SUR_GE_IPIF_SEND_FIFO_MASK |  \
                                 BIG_SUR_GE_IPIF_RECV_FIFO_MASK)

#define BIG_SUR_GE_IPIF_FIFO_DFT_MASK  (BIG_SUR_GE_IPIF_EMAC_MASK |       \
                                 BIG_SUR_GE_IPIF_SEND_FIFO_MASK |  \
                                 BIG_SUR_GE_IPIF_RECV_FIFO_MASK)

#define BIG_SUR_GE_IPIF_DMA_DEV_INTR_COUNT   7 /* Number of interrupt sources */
#define BIG_SUR_GE_IPIF_FIFO_DEV_INTR_COUNT  5 /* Number of interrupt sources */
#define BIG_SUR_GE_IPIF_DEVICE_INTR_COUNT  7   /* Number of interrupt sources */
#define BIG_SUR_GE_IPIF_IP_INTR_COUNT      22  /* Number of MAC interrupts */

/* a mask for all transmit interrupts, used in polled mode */
#define BIG_SUR_GE_EIR_XMIT_ALL_MASK   (BIG_SUR_GE_EIR_XMIT_DONE_MASK |           \
                                 BIG_SUR_GE_EIR_XMIT_ERROR_MASK |          \
                                 BIG_SUR_GE_EIR_XMIT_SFIFO_EMPTY_MASK |    \
                                 BIG_SUR_GE_EIR_XMIT_LFIFO_FULL_MASK)

/* a mask for all receive interrupts, used in polled mode */
#define BIG_SUR_GE_EIR_RECV_ALL_MASK   (BIG_SUR_GE_EIR_RECV_DONE_MASK |           \
                                 BIG_SUR_GE_EIR_RECV_ERROR_MASK |          \
                                 BIG_SUR_GE_EIR_RECV_LFIFO_EMPTY_MASK |    \
                                 BIG_SUR_GE_EIR_RECV_LFIFO_OVER_MASK |     \
                                 BIG_SUR_GE_EIR_RECV_LFIFO_UNDER_MASK |    \
                                 BIG_SUR_GE_EIR_RECV_DFIFO_OVER_MASK |     \
                                 BIG_SUR_GE_EIR_RECV_MISSED_FRAME_MASK |   \
                                 BIG_SUR_GE_EIR_RECV_COLLISION_MASK |      \
                                 BIG_SUR_GE_EIR_RECV_FCS_ERROR_MASK |      \
                                 BIG_SUR_GE_EIR_RECV_LEN_ERROR_MASK |      \
                                 BIG_SUR_GE_EIR_RECV_SHORT_ERROR_MASK |    \
                                 BIG_SUR_GE_EIR_RECV_LONG_ERROR_MASK |     \
                                 BIG_SUR_GE_EIR_RECV_ALIGN_ERROR_MASK)

/* a default interrupt mask for scatter-gather DMA operation */
#define BIG_SUR_GE_EIR_DFT_SG_MASK    (BIG_SUR_GE_EIR_RECV_ERROR_MASK |           \
                                BIG_SUR_GE_EIR_RECV_LFIFO_OVER_MASK |      \
                                BIG_SUR_GE_EIR_RECV_LFIFO_UNDER_MASK |     \
                                BIG_SUR_GE_EIR_XMIT_SFIFO_OVER_MASK |      \
                                BIG_SUR_GE_EIR_XMIT_SFIFO_UNDER_MASK |     \
                                BIG_SUR_GE_EIR_XMIT_LFIFO_OVER_MASK |      \
                                BIG_SUR_GE_EIR_XMIT_LFIFO_UNDER_MASK |     \
                                BIG_SUR_GE_EIR_RECV_DFIFO_OVER_MASK |      \
                                BIG_SUR_GE_EIR_RECV_MISSED_FRAME_MASK |    \
                                BIG_SUR_GE_EIR_RECV_COLLISION_MASK |       \
                                BIG_SUR_GE_EIR_RECV_FCS_ERROR_MASK |       \
                                BIG_SUR_GE_EIR_RECV_LEN_ERROR_MASK |       \
                                BIG_SUR_GE_EIR_RECV_SHORT_ERROR_MASK |     \
                                BIG_SUR_GE_EIR_RECV_LONG_ERROR_MASK |      \
                                BIG_SUR_GE_EIR_RECV_ALIGN_ERROR_MASK)

/* a default interrupt mask for non-DMA operation (direct FIFOs) */
#define BIG_SUR_GE_EIR_DFT_FIFO_MASK  (BIG_SUR_GE_EIR_XMIT_DONE_MASK |            \
                                BIG_SUR_GE_EIR_RECV_DONE_MASK |            \
                                BIG_SUR_GE_EIR_DFT_SG_MASK)

#define BIG_SUR_GE_DMA_SG_INTR_MASK    (BIG_SUR_GEIXR_DMA_ERROR_MASK  |      \
                                 BIG_SUR_GEIXR_PKT_THRESHOLD_MASK |   \
                                 BIG_SUR_GEIXR_PKT_WAIT_BOUND_MASK |  \
                                 BIG_SUR_GEIXR_SG_END_MASK)

#endif
