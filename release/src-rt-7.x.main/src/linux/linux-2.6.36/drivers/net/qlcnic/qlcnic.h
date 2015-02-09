/*
 * Copyright (C) 2009 - QLogic Corporation.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA  02111-1307, USA.
 *
 * The full GNU General Public License is included in this distribution
 * in the file called "COPYING".
 *
 */

#ifndef _QLCNIC_H_
#define _QLCNIC_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/firmware.h>

#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/timer.h>

#include <linux/vmalloc.h>

#include <linux/io.h>
#include <asm/byteorder.h>

#include "qlcnic_hdr.h"

#define _QLCNIC_LINUX_MAJOR 5
#define _QLCNIC_LINUX_MINOR 0
#define _QLCNIC_LINUX_SUBVERSION 7
#define QLCNIC_LINUX_VERSIONID  "5.0.7"
#define QLCNIC_DRV_IDC_VER  0x01

#define QLCNIC_VERSION_CODE(a, b, c)	(((a) << 24) + ((b) << 16) + (c))
#define _major(v)	(((v) >> 24) & 0xff)
#define _minor(v)	(((v) >> 16) & 0xff)
#define _build(v)	((v) & 0xffff)

/* version in image has weird encoding:
 *  7:0  - major
 * 15:8  - minor
 * 31:16 - build (little endian)
 */
#define QLCNIC_DECODE_VERSION(v) \
	QLCNIC_VERSION_CODE(((v) & 0xff), (((v) >> 8) & 0xff), ((v) >> 16))

#define QLCNIC_MIN_FW_VERSION     QLCNIC_VERSION_CODE(4, 4, 2)
#define QLCNIC_NUM_FLASH_SECTORS (64)
#define QLCNIC_FLASH_SECTOR_SIZE (64 * 1024)
#define QLCNIC_FLASH_TOTAL_SIZE  (QLCNIC_NUM_FLASH_SECTORS \
					* QLCNIC_FLASH_SECTOR_SIZE)

#define RCV_DESC_RINGSIZE(rds_ring)	\
	(sizeof(struct rcv_desc) * (rds_ring)->num_desc)
#define RCV_BUFF_RINGSIZE(rds_ring)	\
	(sizeof(struct qlcnic_rx_buffer) * rds_ring->num_desc)
#define STATUS_DESC_RINGSIZE(sds_ring)	\
	(sizeof(struct status_desc) * (sds_ring)->num_desc)
#define TX_BUFF_RINGSIZE(tx_ring)	\
	(sizeof(struct qlcnic_cmd_buffer) * tx_ring->num_desc)
#define TX_DESC_RINGSIZE(tx_ring)	\
	(sizeof(struct cmd_desc_type0) * tx_ring->num_desc)

#define QLCNIC_P3P_A0		0x50

#define QLCNIC_IS_REVISION_P3P(REVISION)     (REVISION >= QLCNIC_P3P_A0)

#define FIRST_PAGE_GROUP_START	0
#define FIRST_PAGE_GROUP_END	0x100000

#define P3_MAX_MTU                     (9600)
#define QLCNIC_MAX_ETHERHDR                32 /* This contains some padding */

#define QLCNIC_P3_RX_BUF_MAX_LEN         (QLCNIC_MAX_ETHERHDR + ETH_DATA_LEN)
#define QLCNIC_P3_RX_JUMBO_BUF_MAX_LEN   (QLCNIC_MAX_ETHERHDR + P3_MAX_MTU)
#define QLCNIC_CT_DEFAULT_RX_BUF_LEN	2048
#define QLCNIC_LRO_BUFFER_EXTRA		2048

/* Opcodes to be used with the commands */
#define TX_ETHER_PKT	0x01
#define TX_TCP_PKT	0x02
#define TX_UDP_PKT	0x03
#define TX_IP_PKT	0x04
#define TX_TCP_LSO	0x05
#define TX_TCP_LSO6	0x06
#define TX_IPSEC	0x07
#define TX_IPSEC_CMD	0x0a
#define TX_TCPV6_PKT	0x0b
#define TX_UDPV6_PKT	0x0c

/* Tx defines */
#define MAX_TSO_HEADER_DESC	2
#define MGMT_CMD_DESC_RESV	4
#define TX_STOP_THRESH		((MAX_SKB_FRAGS >> 2) + MAX_TSO_HEADER_DESC \
							+ MGMT_CMD_DESC_RESV)
#define QLCNIC_MAX_TX_TIMEOUTS	2

/*
 * Following are the states of the Phantom. Phantom will set them and
 * Host will read to check if the fields are correct.
 */
#define PHAN_INITIALIZE_FAILED		0xffff
#define PHAN_INITIALIZE_COMPLETE	0xff01

/* Host writes the following to notify that it has done the init-handshake */
#define PHAN_INITIALIZE_ACK		0xf00f
#define PHAN_PEG_RCV_INITIALIZED	0xff01

#define NUM_RCV_DESC_RINGS	3
#define NUM_STS_DESC_RINGS	4

#define RCV_RING_NORMAL 0
#define RCV_RING_JUMBO	1

#define MIN_CMD_DESCRIPTORS		64
#define MIN_RCV_DESCRIPTORS		64
#define MIN_JUMBO_DESCRIPTORS		32

#define MAX_CMD_DESCRIPTORS		1024
#define MAX_RCV_DESCRIPTORS_1G		4096
#define MAX_RCV_DESCRIPTORS_10G 	8192
#define MAX_JUMBO_RCV_DESCRIPTORS_1G	512
#define MAX_JUMBO_RCV_DESCRIPTORS_10G	1024

#define DEFAULT_RCV_DESCRIPTORS_1G	2048
#define DEFAULT_RCV_DESCRIPTORS_10G	4096

#define get_next_index(index, length)	\
	(((index) + 1) & ((length) - 1))

/*
 * Following data structures describe the descriptors that will be used.
 * Added fileds of tcpHdrSize and ipHdrSize, The driver needs to do it only when
 * we are doing LSO (above the 1500 size packet) only.
 */

#define FLAGS_VLAN_TAGGED	0x10
#define FLAGS_VLAN_OOB		0x40

#define qlcnic_set_tx_vlan_tci(cmd_desc, v)	\
	(cmd_desc)->vlan_TCI = cpu_to_le16(v);
#define qlcnic_set_cmd_desc_port(cmd_desc, var)	\
	((cmd_desc)->port_ctxid |= ((var) & 0x0F))
#define qlcnic_set_cmd_desc_ctxid(cmd_desc, var)	\
	((cmd_desc)->port_ctxid |= ((var) << 4 & 0xF0))

#define qlcnic_set_tx_port(_desc, _port) \
	((_desc)->port_ctxid = ((_port) & 0xf) | (((_port) << 4) & 0xf0))

#define qlcnic_set_tx_flags_opcode(_desc, _flags, _opcode) \
	((_desc)->flags_opcode = \
	cpu_to_le16(((_flags) & 0x7f) | (((_opcode) & 0x3f) << 7)))

#define qlcnic_set_tx_frags_len(_desc, _frags, _len) \
	((_desc)->nfrags__length = \
	cpu_to_le32(((_frags) & 0xff) | (((_len) & 0xffffff) << 8)))

struct cmd_desc_type0 {
	u8 tcp_hdr_offset;	/* For LSO only */
	u8 ip_hdr_offset;	/* For LSO only */
	__le16 flags_opcode;	/* 15:13 unused, 12:7 opcode, 6:0 flags */
	__le32 nfrags__length;	/* 31:8 total len, 7:0 frag count */

	__le64 addr_buffer2;

	__le16 reference_handle;
	__le16 mss;
	u8 port_ctxid;		/* 7:4 ctxid 3:0 port */
	u8 total_hdr_length;	/* LSO only : MAC+IP+TCP Hdr size */
	__le16 conn_id;		/* IPSec offoad only */

	__le64 addr_buffer3;
	__le64 addr_buffer1;

	__le16 buffer_length[4];

	__le64 addr_buffer4;

	u8 eth_addr[ETH_ALEN];
	__le16 vlan_TCI;

} __attribute__ ((aligned(64)));

/* Note: sizeof(rcv_desc) should always be a mutliple of 2 */
struct rcv_desc {
	__le16 reference_handle;
	__le16 reserved;
	__le32 buffer_length;	/* allocated buffer length (usually 2K) */
	__le64 addr_buffer;
};

/* opcode field in status_desc */
#define QLCNIC_SYN_OFFLOAD	0x03
#define QLCNIC_RXPKT_DESC  	0x04
#define QLCNIC_OLD_RXPKT_DESC	0x3f
#define QLCNIC_RESPONSE_DESC	0x05
#define QLCNIC_LRO_DESC  	0x12

/* for status field in status_desc */
#define STATUS_CKSUM_OK		(2)

/* owner bits of status_desc */
#define STATUS_OWNER_HOST	(0x1ULL << 56)
#define STATUS_OWNER_PHANTOM	(0x2ULL << 56)

/* Status descriptor:
   0-3 port, 4-7 status, 8-11 type, 12-27 total_length
   28-43 reference_handle, 44-47 protocol, 48-52 pkt_offset
   53-55 desc_cnt, 56-57 owner, 58-63 opcode
 */
#define qlcnic_get_sts_port(sts_data)	\
	((sts_data) & 0x0F)
#define qlcnic_get_sts_status(sts_data)	\
	(((sts_data) >> 4) & 0x0F)
#define qlcnic_get_sts_type(sts_data)	\
	(((sts_data) >> 8) & 0x0F)
#define qlcnic_get_sts_totallength(sts_data)	\
	(((sts_data) >> 12) & 0xFFFF)
#define qlcnic_get_sts_refhandle(sts_data)	\
	(((sts_data) >> 28) & 0xFFFF)
#define qlcnic_get_sts_prot(sts_data)	\
	(((sts_data) >> 44) & 0x0F)
#define qlcnic_get_sts_pkt_offset(sts_data)	\
	(((sts_data) >> 48) & 0x1F)
#define qlcnic_get_sts_desc_cnt(sts_data)	\
	(((sts_data) >> 53) & 0x7)
#define qlcnic_get_sts_opcode(sts_data)	\
	(((sts_data) >> 58) & 0x03F)

#define qlcnic_get_lro_sts_refhandle(sts_data) 	\
	((sts_data) & 0x0FFFF)
#define qlcnic_get_lro_sts_length(sts_data)	\
	(((sts_data) >> 16) & 0x0FFFF)
#define qlcnic_get_lro_sts_l2_hdr_offset(sts_data)	\
	(((sts_data) >> 32) & 0x0FF)
#define qlcnic_get_lro_sts_l4_hdr_offset(sts_data)	\
	(((sts_data) >> 40) & 0x0FF)
#define qlcnic_get_lro_sts_timestamp(sts_data)	\
	(((sts_data) >> 48) & 0x1)
#define qlcnic_get_lro_sts_type(sts_data)	\
	(((sts_data) >> 49) & 0x7)
#define qlcnic_get_lro_sts_push_flag(sts_data)		\
	(((sts_data) >> 52) & 0x1)
#define qlcnic_get_lro_sts_seq_number(sts_data)		\
	((sts_data) & 0x0FFFFFFFF)


struct status_desc {
	__le64 status_desc_data[2];
} __attribute__ ((aligned(16)));

/* UNIFIED ROMIMAGE */
#define QLCNIC_UNI_FW_MIN_SIZE		0xc8000
#define QLCNIC_UNI_DIR_SECT_PRODUCT_TBL	0x0
#define QLCNIC_UNI_DIR_SECT_BOOTLD	0x6
#define QLCNIC_UNI_DIR_SECT_FW		0x7

/*Offsets */
#define QLCNIC_UNI_CHIP_REV_OFF		10
#define QLCNIC_UNI_FLAGS_OFF		11
#define QLCNIC_UNI_BIOS_VERSION_OFF 	12
#define QLCNIC_UNI_BOOTLD_IDX_OFF	27
#define QLCNIC_UNI_FIRMWARE_IDX_OFF 	29

struct uni_table_desc{
	u32	findex;
	u32	num_entries;
	u32	entry_size;
	u32	reserved[5];
};

struct uni_data_desc{
	u32	findex;
	u32	size;
	u32	reserved[5];
};

/* Magic number to let user know flash is programmed */
#define	QLCNIC_BDINFO_MAGIC 0x12345678

#define QLCNIC_BRDTYPE_P3_REF_QG	0x0021
#define QLCNIC_BRDTYPE_P3_HMEZ		0x0022
#define QLCNIC_BRDTYPE_P3_10G_CX4_LP	0x0023
#define QLCNIC_BRDTYPE_P3_4_GB		0x0024
#define QLCNIC_BRDTYPE_P3_IMEZ		0x0025
#define QLCNIC_BRDTYPE_P3_10G_SFP_PLUS	0x0026
#define QLCNIC_BRDTYPE_P3_10000_BASE_T	0x0027
#define QLCNIC_BRDTYPE_P3_XG_LOM	0x0028
#define QLCNIC_BRDTYPE_P3_4_GB_MM	0x0029
#define QLCNIC_BRDTYPE_P3_10G_SFP_CT	0x002a
#define QLCNIC_BRDTYPE_P3_10G_SFP_QT	0x002b
#define QLCNIC_BRDTYPE_P3_10G_CX4	0x0031
#define QLCNIC_BRDTYPE_P3_10G_XFP	0x0032
#define QLCNIC_BRDTYPE_P3_10G_TP	0x0080

#define QLCNIC_MSIX_TABLE_OFFSET	0x44

/* Flash memory map */
#define QLCNIC_BRDCFG_START	0x4000		/* board config */
#define QLCNIC_BOOTLD_START	0x10000		/* bootld */
#define QLCNIC_IMAGE_START	0x43000		/* compressed image */
#define QLCNIC_USER_START	0x3E8000	/* Firmare info */

#define QLCNIC_FW_VERSION_OFFSET	(QLCNIC_USER_START+0x408)
#define QLCNIC_FW_SIZE_OFFSET		(QLCNIC_USER_START+0x40c)
#define QLCNIC_FW_SERIAL_NUM_OFFSET	(QLCNIC_USER_START+0x81c)
#define QLCNIC_BIOS_VERSION_OFFSET	(QLCNIC_USER_START+0x83c)

#define QLCNIC_BRDTYPE_OFFSET		(QLCNIC_BRDCFG_START+0x8)
#define QLCNIC_FW_MAGIC_OFFSET		(QLCNIC_BRDCFG_START+0x128)

#define QLCNIC_FW_MIN_SIZE		(0x3fffff)
#define QLCNIC_UNIFIED_ROMIMAGE  	0
#define QLCNIC_FLASH_ROMIMAGE		1
#define QLCNIC_UNKNOWN_ROMIMAGE		0xff

#define QLCNIC_UNIFIED_ROMIMAGE_NAME	"phanfw.bin"
#define QLCNIC_FLASH_ROMIMAGE_NAME	"flash"

extern char qlcnic_driver_name[];

/* Number of status descriptors to handle per interrupt */
#define MAX_STATUS_HANDLE	(64)

/*
 * qlcnic_skb_frag{} is to contain mapping info for each SG list. This
 * has to be freed when DMA is complete. This is part of qlcnic_tx_buffer{}.
 */
struct qlcnic_skb_frag {
	u64 dma;
	u64 length;
};

struct qlcnic_recv_crb {
	u32 crb_rcv_producer[NUM_RCV_DESC_RINGS];
	u32 crb_sts_consumer[NUM_STS_DESC_RINGS];
	u32 sw_int_mask[NUM_STS_DESC_RINGS];
};

/*    Following defines are for the state of the buffers    */
#define	QLCNIC_BUFFER_FREE	0
#define	QLCNIC_BUFFER_BUSY	1

/*
 * There will be one qlcnic_buffer per skb packet.    These will be
 * used to save the dma info for pci_unmap_page()
 */
struct qlcnic_cmd_buffer {
	struct sk_buff *skb;
	struct qlcnic_skb_frag frag_array[MAX_SKB_FRAGS + 1];
	u32 frag_count;
};

/* In rx_buffer, we do not need multiple fragments as is a single buffer */
struct qlcnic_rx_buffer {
	struct list_head list;
	struct sk_buff *skb;
	u64 dma;
	u16 ref_handle;
};

/* Board types */
#define	QLCNIC_GBE	0x01
#define	QLCNIC_XGBE	0x02

/*
 * One hardware_context{} per adapter
 * contains interrupt info as well shared hardware info.
 */
struct qlcnic_hardware_context {
	void __iomem *pci_base0;
	void __iomem *ocm_win_crb;

	unsigned long pci_len0;

	rwlock_t crb_lock;
	struct mutex mem_lock;

	u8 revision_id;
	u8 pci_func;
	u8 linkup;
	u16 port_type;
	u16 board_type;
};

struct qlcnic_adapter_stats {
	u64  xmitcalled;
	u64  xmitfinished;
	u64  rxdropped;
	u64  txdropped;
	u64  csummed;
	u64  rx_pkts;
	u64  lro_pkts;
	u64  rxbytes;
	u64  txbytes;
	u64  lrobytes;
	u64  lso_frames;
	u64  xmit_on;
	u64  xmit_off;
	u64  skb_alloc_failure;
	u64  null_rxbuf;
	u64  rx_dma_map_error;
	u64  tx_dma_map_error;
};

/*
 * Rcv Descriptor Context. One such per Rcv Descriptor. There may
 * be one Rcv Descriptor for normal packets, one for jumbo and may be others.
 */
struct qlcnic_host_rds_ring {
	u32 producer;
	u32 num_desc;
	u32 dma_size;
	u32 skb_size;
	u32 flags;
	void __iomem *crb_rcv_producer;
	struct rcv_desc *desc_head;
	struct qlcnic_rx_buffer *rx_buf_arr;
	struct list_head free_list;
	spinlock_t lock;
	dma_addr_t phys_addr;
};

struct qlcnic_host_sds_ring {
	u32 consumer;
	u32 num_desc;
	void __iomem *crb_sts_consumer;
	void __iomem *crb_intr_mask;

	struct status_desc *desc_head;
	struct qlcnic_adapter *adapter;
	struct napi_struct napi;
	struct list_head free_list[NUM_RCV_DESC_RINGS];

	int irq;

	dma_addr_t phys_addr;
	char name[IFNAMSIZ+4];
};

struct qlcnic_host_tx_ring {
	u32 producer;
	__le32 *hw_consumer;
	u32 sw_consumer;
	void __iomem *crb_cmd_producer;
	u32 num_desc;

	struct netdev_queue *txq;

	struct qlcnic_cmd_buffer *cmd_buf_arr;
	struct cmd_desc_type0 *desc_head;
	dma_addr_t phys_addr;
	dma_addr_t hw_cons_phys_addr;
};

/*
 * Receive context. There is one such structure per instance of the
 * receive processing. Any state information that is relevant to
 * the receive, and is must be in this structure. The global data may be
 * present elsewhere.
 */
struct qlcnic_recv_context {
	u32 state;
	u16 context_id;
	u16 virt_port;

	struct qlcnic_host_rds_ring *rds_rings;
	struct qlcnic_host_sds_ring *sds_rings;
};

/* HW context creation */

#define QLCNIC_OS_CRB_RETRY_COUNT	4000
#define QLCNIC_CDRP_SIGNATURE_MAKE(pcifn, version) \
	(((pcifn) & 0xff) | (((version) & 0xff) << 8) | (0xcafe << 16))

#define QLCNIC_CDRP_CMD_BIT		0x80000000

/*
 * All responses must have the QLCNIC_CDRP_CMD_BIT cleared
 * in the crb QLCNIC_CDRP_CRB_OFFSET.
 */
#define QLCNIC_CDRP_FORM_RSP(rsp)	(rsp)
#define QLCNIC_CDRP_IS_RSP(rsp)	(((rsp) & QLCNIC_CDRP_CMD_BIT) == 0)

#define QLCNIC_CDRP_RSP_OK		0x00000001
#define QLCNIC_CDRP_RSP_FAIL		0x00000002
#define QLCNIC_CDRP_RSP_TIMEOUT 	0x00000003

/*
 * All commands must have the QLCNIC_CDRP_CMD_BIT set in
 * the crb QLCNIC_CDRP_CRB_OFFSET.
 */
#define QLCNIC_CDRP_FORM_CMD(cmd)	(QLCNIC_CDRP_CMD_BIT | (cmd))
#define QLCNIC_CDRP_IS_CMD(cmd)	(((cmd) & QLCNIC_CDRP_CMD_BIT) != 0)

#define QLCNIC_CDRP_CMD_SUBMIT_CAPABILITIES     0x00000001
#define QLCNIC_CDRP_CMD_READ_MAX_RDS_PER_CTX    0x00000002
#define QLCNIC_CDRP_CMD_READ_MAX_SDS_PER_CTX    0x00000003
#define QLCNIC_CDRP_CMD_READ_MAX_RULES_PER_CTX  0x00000004
#define QLCNIC_CDRP_CMD_READ_MAX_RX_CTX         0x00000005
#define QLCNIC_CDRP_CMD_READ_MAX_TX_CTX         0x00000006
#define QLCNIC_CDRP_CMD_CREATE_RX_CTX           0x00000007
#define QLCNIC_CDRP_CMD_DESTROY_RX_CTX          0x00000008
#define QLCNIC_CDRP_CMD_CREATE_TX_CTX           0x00000009
#define QLCNIC_CDRP_CMD_DESTROY_TX_CTX          0x0000000a
#define QLCNIC_CDRP_CMD_SETUP_STATISTICS        0x0000000e
#define QLCNIC_CDRP_CMD_GET_STATISTICS          0x0000000f
#define QLCNIC_CDRP_CMD_DELETE_STATISTICS       0x00000010
#define QLCNIC_CDRP_CMD_SET_MTU                 0x00000012
#define QLCNIC_CDRP_CMD_READ_PHY		0x00000013
#define QLCNIC_CDRP_CMD_WRITE_PHY		0x00000014
#define QLCNIC_CDRP_CMD_READ_HW_REG		0x00000015
#define QLCNIC_CDRP_CMD_GET_FLOW_CTL		0x00000016
#define QLCNIC_CDRP_CMD_SET_FLOW_CTL		0x00000017
#define QLCNIC_CDRP_CMD_READ_MAX_MTU		0x00000018
#define QLCNIC_CDRP_CMD_READ_MAX_LRO		0x00000019
#define QLCNIC_CDRP_CMD_CONFIGURE_TOE		0x0000001a
#define QLCNIC_CDRP_CMD_FUNC_ATTRIB		0x0000001b
#define QLCNIC_CDRP_CMD_READ_PEXQ_PARAMETERS	0x0000001c
#define QLCNIC_CDRP_CMD_GET_LIC_CAPABILITIES	0x0000001d
#define QLCNIC_CDRP_CMD_READ_MAX_LRO_PER_BOARD	0x0000001e
#define QLCNIC_CDRP_CMD_MAC_ADDRESS		0x0000001f

#define QLCNIC_CDRP_CMD_GET_PCI_INFO		0x00000020
#define QLCNIC_CDRP_CMD_GET_NIC_INFO		0x00000021
#define QLCNIC_CDRP_CMD_SET_NIC_INFO		0x00000022
#define QLCNIC_CDRP_CMD_RESET_NPAR		0x00000023
#define QLCNIC_CDRP_CMD_GET_ESWITCH_CAPABILITY	0x00000024
#define QLCNIC_CDRP_CMD_TOGGLE_ESWITCH		0x00000025
#define QLCNIC_CDRP_CMD_GET_ESWITCH_STATUS	0x00000026
#define QLCNIC_CDRP_CMD_SET_PORTMIRRORING	0x00000027
#define QLCNIC_CDRP_CMD_CONFIGURE_ESWITCH	0x00000028

#define QLCNIC_RCODE_SUCCESS		0
#define QLCNIC_RCODE_TIMEOUT		17
#define QLCNIC_DESTROY_CTX_RESET	0

/*
 * Capabilities Announced
 */
#define QLCNIC_CAP0_LEGACY_CONTEXT	(1)
#define QLCNIC_CAP0_LEGACY_MN		(1 << 2)
#define QLCNIC_CAP0_LSO 		(1 << 6)
#define QLCNIC_CAP0_JUMBO_CONTIGUOUS	(1 << 7)
#define QLCNIC_CAP0_LRO_CONTIGUOUS	(1 << 8)
#define QLCNIC_CAP0_VALIDOFF		(1 << 11)

/*
 * Context state
 */
#define QLCNIC_HOST_CTX_STATE_FREED	0
#define QLCNIC_HOST_CTX_STATE_ACTIVE	2

/*
 * Rx context
 */

struct qlcnic_hostrq_sds_ring {
	__le64 host_phys_addr;	/* Ring base addr */
	__le32 ring_size;		/* Ring entries */
	__le16 msi_index;
	__le16 rsvd;		/* Padding */
};

struct qlcnic_hostrq_rds_ring {
	__le64 host_phys_addr;	/* Ring base addr */
	__le64 buff_size;		/* Packet buffer size */
	__le32 ring_size;		/* Ring entries */
	__le32 ring_kind;		/* Class of ring */
};

struct qlcnic_hostrq_rx_ctx {
	__le64 host_rsp_dma_addr;	/* Response dma'd here */
	__le32 capabilities[4];	/* Flag bit vector */
	__le32 host_int_crb_mode;	/* Interrupt crb usage */
	__le32 host_rds_crb_mode;	/* RDS crb usage */
	/* These ring offsets are relative to data[0] below */
	__le32 rds_ring_offset;	/* Offset to RDS config */
	__le32 sds_ring_offset;	/* Offset to SDS config */
	__le16 num_rds_rings;	/* Count of RDS rings */
	__le16 num_sds_rings;	/* Count of SDS rings */
	__le16 valid_field_offset;
	u8  txrx_sds_binding;
	u8  msix_handler;
	u8  reserved[128];      /* reserve space for future expansion*/
	/* MUST BE 64-bit aligned.
	   The following is packed:
	   - N hostrq_rds_rings
	   - N hostrq_sds_rings */
	char data[0];
};

struct qlcnic_cardrsp_rds_ring{
	__le32 host_producer_crb;	/* Crb to use */
	__le32 rsvd1;		/* Padding */
};

struct qlcnic_cardrsp_sds_ring {
	__le32 host_consumer_crb;	/* Crb to use */
	__le32 interrupt_crb;	/* Crb to use */
};

struct qlcnic_cardrsp_rx_ctx {
	/* These ring offsets are relative to data[0] below */
	__le32 rds_ring_offset;	/* Offset to RDS config */
	__le32 sds_ring_offset;	/* Offset to SDS config */
	__le32 host_ctx_state;	/* Starting State */
	__le32 num_fn_per_port;	/* How many PCI fn share the port */
	__le16 num_rds_rings;	/* Count of RDS rings */
	__le16 num_sds_rings;	/* Count of SDS rings */
	__le16 context_id;		/* Handle for context */
	u8  phys_port;		/* Physical id of port */
	u8  virt_port;		/* Virtual/Logical id of port */
	u8  reserved[128];	/* save space for future expansion */
	/*  MUST BE 64-bit aligned.
	   The following is packed:
	   - N cardrsp_rds_rings
	   - N cardrs_sds_rings */
	char data[0];
};

#define SIZEOF_HOSTRQ_RX(HOSTRQ_RX, rds_rings, sds_rings)	\
	(sizeof(HOSTRQ_RX) + 					\
	(rds_rings)*(sizeof(struct qlcnic_hostrq_rds_ring)) +		\
	(sds_rings)*(sizeof(struct qlcnic_hostrq_sds_ring)))

#define SIZEOF_CARDRSP_RX(CARDRSP_RX, rds_rings, sds_rings) 	\
	(sizeof(CARDRSP_RX) + 					\
	(rds_rings)*(sizeof(struct qlcnic_cardrsp_rds_ring)) + 		\
	(sds_rings)*(sizeof(struct qlcnic_cardrsp_sds_ring)))

/*
 * Tx context
 */

struct qlcnic_hostrq_cds_ring {
	__le64 host_phys_addr;	/* Ring base addr */
	__le32 ring_size;		/* Ring entries */
	__le32 rsvd;		/* Padding */
};

struct qlcnic_hostrq_tx_ctx {
	__le64 host_rsp_dma_addr;	/* Response dma'd here */
	__le64 cmd_cons_dma_addr;	/*  */
	__le64 dummy_dma_addr;	/*  */
	__le32 capabilities[4];	/* Flag bit vector */
	__le32 host_int_crb_mode;	/* Interrupt crb usage */
	__le32 rsvd1;		/* Padding */
	__le16 rsvd2;		/* Padding */
	__le16 interrupt_ctl;
	__le16 msi_index;
	__le16 rsvd3;		/* Padding */
	struct qlcnic_hostrq_cds_ring cds_ring;	/* Desc of cds ring */
	u8  reserved[128];	/* future expansion */
};

struct qlcnic_cardrsp_cds_ring {
	__le32 host_producer_crb;	/* Crb to use */
	__le32 interrupt_crb;	/* Crb to use */
};

struct qlcnic_cardrsp_tx_ctx {
	__le32 host_ctx_state;	/* Starting state */
	__le16 context_id;		/* Handle for context */
	u8  phys_port;		/* Physical id of port */
	u8  virt_port;		/* Virtual/Logical id of port */
	struct qlcnic_cardrsp_cds_ring cds_ring;	/* Card cds settings */
	u8  reserved[128];	/* future expansion */
};

#define SIZEOF_HOSTRQ_TX(HOSTRQ_TX)	(sizeof(HOSTRQ_TX))
#define SIZEOF_CARDRSP_TX(CARDRSP_TX)	(sizeof(CARDRSP_TX))

/* CRB */

#define QLCNIC_HOST_RDS_CRB_MODE_UNIQUE	0
#define QLCNIC_HOST_RDS_CRB_MODE_SHARED	1
#define QLCNIC_HOST_RDS_CRB_MODE_CUSTOM	2
#define QLCNIC_HOST_RDS_CRB_MODE_MAX	3

#define QLCNIC_HOST_INT_CRB_MODE_UNIQUE	0
#define QLCNIC_HOST_INT_CRB_MODE_SHARED	1
#define QLCNIC_HOST_INT_CRB_MODE_NORX	2
#define QLCNIC_HOST_INT_CRB_MODE_NOTX	3
#define QLCNIC_HOST_INT_CRB_MODE_NORXTX	4


/* MAC */

#define MC_COUNT_P3	38

#define QLCNIC_MAC_NOOP	0
#define QLCNIC_MAC_ADD	1
#define QLCNIC_MAC_DEL	2

struct qlcnic_mac_list_s {
	struct list_head list;
	uint8_t mac_addr[ETH_ALEN+2];
};

/*
 * Interrupt coalescing defaults. The defaults are for 1500 MTU. It is
 * adjusted based on configured MTU.
 */
#define QLCNIC_DEFAULT_INTR_COALESCE_RX_TIME_US	3
#define QLCNIC_DEFAULT_INTR_COALESCE_RX_PACKETS	256
#define QLCNIC_DEFAULT_INTR_COALESCE_TX_PACKETS	64
#define QLCNIC_DEFAULT_INTR_COALESCE_TX_TIME_US	4

#define QLCNIC_INTR_DEFAULT			0x04

union qlcnic_nic_intr_coalesce_data {
	struct {
		u16	rx_packets;
		u16	rx_time_us;
		u16	tx_packets;
		u16	tx_time_us;
	} data;
	u64		word;
};

struct qlcnic_nic_intr_coalesce {
	u16		stats_time_us;
	u16		rate_sample_time;
	u16		flags;
	u16		rsvd_1;
	u32		low_threshold;
	u32		high_threshold;
	union qlcnic_nic_intr_coalesce_data	normal;
	union qlcnic_nic_intr_coalesce_data	low;
	union qlcnic_nic_intr_coalesce_data	high;
	union qlcnic_nic_intr_coalesce_data	irq;
};

#define QLCNIC_HOST_REQUEST	0x13
#define QLCNIC_REQUEST		0x14

#define QLCNIC_MAC_EVENT	0x1

#define QLCNIC_IP_UP		2
#define QLCNIC_IP_DOWN		3

/*
 * Driver --> Firmware
 */
#define QLCNIC_H2C_OPCODE_START 			0
#define QLCNIC_H2C_OPCODE_CONFIG_RSS			1
#define QLCNIC_H2C_OPCODE_CONFIG_RSS_TBL		2
#define QLCNIC_H2C_OPCODE_CONFIG_INTR_COALESCE		3
#define QLCNIC_H2C_OPCODE_CONFIG_LED			4
#define QLCNIC_H2C_OPCODE_CONFIG_PROMISCUOUS		5
#define QLCNIC_H2C_OPCODE_CONFIG_L2_MAC 		6
#define QLCNIC_H2C_OPCODE_LRO_REQUEST			7
#define QLCNIC_H2C_OPCODE_GET_SNMP_STATS		8
#define QLCNIC_H2C_OPCODE_PROXY_START_REQUEST		9
#define QLCNIC_H2C_OPCODE_PROXY_STOP_REQUEST		10
#define QLCNIC_H2C_OPCODE_PROXY_SET_MTU 		11
#define QLCNIC_H2C_OPCODE_PROXY_SET_VPORT_MISS_MODE	12
#define QLCNIC_H2C_OPCODE_GET_FINGER_PRINT_REQUEST	13
#define QLCNIC_H2C_OPCODE_INSTALL_LICENSE_REQUEST	14
#define QLCNIC_H2C_OPCODE_GET_LICENSE_CAPABILITY_REQUEST	15
#define QLCNIC_H2C_OPCODE_GET_NET_STATS 		16
#define QLCNIC_H2C_OPCODE_PROXY_UPDATE_P2V		17
#define QLCNIC_H2C_OPCODE_CONFIG_IPADDR 		18
#define QLCNIC_H2C_OPCODE_CONFIG_LOOPBACK		19
#define QLCNIC_H2C_OPCODE_PROXY_STOP_DONE		20
#define QLCNIC_H2C_OPCODE_GET_LINKEVENT 		21
#define QLCNIC_C2C_OPCODE				22
#define QLCNIC_H2C_OPCODE_CONFIG_BRIDGING		23
#define QLCNIC_H2C_OPCODE_CONFIG_HW_LRO 		24
#define QLCNIC_H2C_OPCODE_LAST				25
/*
 * Firmware --> Driver
 */

#define QLCNIC_C2H_OPCODE_START 			128
#define QLCNIC_C2H_OPCODE_CONFIG_RSS_RESPONSE		129
#define QLCNIC_C2H_OPCODE_CONFIG_RSS_TBL_RESPONSE	130
#define QLCNIC_C2H_OPCODE_CONFIG_MAC_RESPONSE		131
#define QLCNIC_C2H_OPCODE_CONFIG_PROMISCUOUS_RESPONSE	132
#define QLCNIC_C2H_OPCODE_CONFIG_L2_MAC_RESPONSE	133
#define QLCNIC_C2H_OPCODE_LRO_DELETE_RESPONSE		134
#define QLCNIC_C2H_OPCODE_LRO_ADD_FAILURE_RESPONSE	135
#define QLCNIC_C2H_OPCODE_GET_SNMP_STATS		136
#define QLCNIC_C2H_OPCODE_GET_FINGER_PRINT_REPLY	137
#define QLCNIC_C2H_OPCODE_INSTALL_LICENSE_REPLY 	138
#define QLCNIC_C2H_OPCODE_GET_LICENSE_CAPABILITIES_REPLY 139
#define QLCNIC_C2H_OPCODE_GET_NET_STATS_RESPONSE	140
#define QLCNIC_C2H_OPCODE_GET_LINKEVENT_RESPONSE	141
#define QLCNIC_C2H_OPCODE_LAST				142

#define VPORT_MISS_MODE_DROP		0 /* drop all unmatched */
#define VPORT_MISS_MODE_ACCEPT_ALL	1 /* accept all packets */
#define VPORT_MISS_MODE_ACCEPT_MULTI	2 /* accept unmatched multicast */

#define QLCNIC_LRO_REQUEST_CLEANUP	4

/* Capabilites received */
#define QLCNIC_FW_CAPABILITY_TSO		BIT_1
#define QLCNIC_FW_CAPABILITY_BDG		BIT_8
#define QLCNIC_FW_CAPABILITY_FVLANTX		BIT_9
#define QLCNIC_FW_CAPABILITY_HW_LRO		BIT_10

/* module types */
#define LINKEVENT_MODULE_NOT_PRESENT			1
#define LINKEVENT_MODULE_OPTICAL_UNKNOWN		2
#define LINKEVENT_MODULE_OPTICAL_SRLR			3
#define LINKEVENT_MODULE_OPTICAL_LRM			4
#define LINKEVENT_MODULE_OPTICAL_SFP_1G 		5
#define LINKEVENT_MODULE_TWINAX_UNSUPPORTED_CABLE	6
#define LINKEVENT_MODULE_TWINAX_UNSUPPORTED_CABLELEN	7
#define LINKEVENT_MODULE_TWINAX 			8

#define LINKSPEED_10GBPS	10000
#define LINKSPEED_1GBPS 	1000
#define LINKSPEED_100MBPS	100
#define LINKSPEED_10MBPS	10

#define LINKSPEED_ENCODED_10MBPS	0
#define LINKSPEED_ENCODED_100MBPS	1
#define LINKSPEED_ENCODED_1GBPS 	2

#define LINKEVENT_AUTONEG_DISABLED	0
#define LINKEVENT_AUTONEG_ENABLED	1

#define LINKEVENT_HALF_DUPLEX		0
#define LINKEVENT_FULL_DUPLEX		1

#define LINKEVENT_LINKSPEED_MBPS	0
#define LINKEVENT_LINKSPEED_ENCODED	1

#define AUTO_FW_RESET_ENABLED	0x01
/* firmware response header:
 *	63:58 - message type
 *	57:56 - owner
 *	55:53 - desc count
 *	52:48 - reserved
 *	47:40 - completion id
 *	39:32 - opcode
 *	31:16 - error code
 *	15:00 - reserved
 */
#define qlcnic_get_nic_msg_opcode(msg_hdr)	\
	((msg_hdr >> 32) & 0xFF)

struct qlcnic_fw_msg {
	union {
		struct {
			u64 hdr;
			u64 body[7];
		};
		u64 words[8];
	};
};

struct qlcnic_nic_req {
	__le64 qhdr;
	__le64 req_hdr;
	__le64 words[6];
};

struct qlcnic_mac_req {
	u8 op;
	u8 tag;
	u8 mac_addr[6];
};

#define QLCNIC_MSI_ENABLED		0x02
#define QLCNIC_MSIX_ENABLED		0x04
#define QLCNIC_LRO_ENABLED		0x08
#define QLCNIC_BRIDGE_ENABLED       	0X10
#define QLCNIC_DIAG_ENABLED		0x20
#define QLCNIC_ESWITCH_ENABLED		0x40
#define QLCNIC_IS_MSI_FAMILY(adapter) \
	((adapter)->flags & (QLCNIC_MSI_ENABLED | QLCNIC_MSIX_ENABLED))

#define MSIX_ENTRIES_PER_ADAPTER	NUM_STS_DESC_RINGS
#define QLCNIC_MSIX_TBL_SPACE		8192
#define QLCNIC_PCI_REG_MSIX_TBL 	0x44
#define QLCNIC_MSIX_TBL_PGSIZE		4096

#define QLCNIC_NETDEV_WEIGHT	128
#define QLCNIC_ADAPTER_UP_MAGIC 777

#define __QLCNIC_FW_ATTACHED		0
#define __QLCNIC_DEV_UP 		1
#define __QLCNIC_RESETTING		2
#define __QLCNIC_START_FW 		4
#define __QLCNIC_AER			5

#define QLCNIC_INTERRUPT_TEST		1
#define QLCNIC_LOOPBACK_TEST		2

struct qlcnic_adapter {
	struct qlcnic_hardware_context ahw;

	struct net_device *netdev;
	struct pci_dev *pdev;
	struct list_head mac_list;

	spinlock_t tx_clean_lock;

	u16 num_txd;
	u16 num_rxd;
	u16 num_jumbo_rxd;

	u8 max_rds_rings;
	u8 max_sds_rings;
	u8 driver_mismatch;
	u8 msix_supported;
	u8 rx_csum;
	u8 portnum;
	u8 physical_port;
	u8 reset_context;

	u8 mc_enabled;
	u8 max_mc_count;
	u8 rss_supported;
	u8 fw_wait_cnt;
	u8 fw_fail_cnt;
	u8 tx_timeo_cnt;
	u8 need_fw_reset;

	u8 has_link_events;
	u8 fw_type;
	u16 tx_context_id;
	u16 is_up;

	u16 link_speed;
	u16 link_duplex;
	u16 link_autoneg;
	u16 module_type;

	u16 op_mode;
	u16 switch_mode;
	u16 max_tx_ques;
	u16 max_rx_ques;
	u16 max_mtu;

	u32 fw_hal_version;
	u32 capabilities;
	u32 flags;
	u32 irq;
	u32 temp;

	u32 int_vec_bit;
	u32 heartbit;

	u8 max_mac_filters;
	u8 dev_state;
	u8 diag_test;
	u8 diag_cnt;
	u8 reset_ack_timeo;
	u8 dev_init_timeo;
	u16 msg_enable;

	u8 mac_addr[ETH_ALEN];

	u64 dev_rst_time;

	struct qlcnic_npar_info *npars;
	struct qlcnic_eswitch *eswitch;
	struct qlcnic_nic_template *nic_ops;

	struct qlcnic_adapter_stats stats;

	struct qlcnic_recv_context recv_ctx;
	struct qlcnic_host_tx_ring *tx_ring;

	void __iomem	*tgt_mask_reg;
	void __iomem	*tgt_status_reg;
	void __iomem	*crb_int_state_reg;
	void __iomem	*isr_int_vec;

	struct msix_entry msix_entries[MSIX_ENTRIES_PER_ADAPTER];

	struct delayed_work fw_work;

	struct qlcnic_nic_intr_coalesce coal;

	unsigned long state;
	__le32 file_prd_off;	/*File fw product offset*/
	u32 fw_version;
	const struct firmware *fw;
};

struct qlcnic_info {
	__le16	pci_func;
	__le16	op_mode; /* 1 = Priv, 2 = NP, 3 = NP passthru */
	__le16	phys_port;
	__le16	switch_mode; /* 0 = disabled, 1 = int, 2 = ext */

	__le32	capabilities;
	u8	max_mac_filters;
	u8	reserved1;
	__le16	max_mtu;

	__le16	max_tx_ques;
	__le16	max_rx_ques;
	__le16	min_tx_bw;
	__le16	max_tx_bw;
	u8	reserved2[104];
};

struct qlcnic_pci_info {
	__le16	id; /* pci function id */
	__le16	active; /* 1 = Enabled */
	__le16	type; /* 1 = NIC, 2 = FCoE, 3 = iSCSI */
	__le16	default_port; /* default port number */

	__le16	tx_min_bw; /* Multiple of 100mbpc */
	__le16	tx_max_bw;
	__le16	reserved1[2];

	u8	mac[ETH_ALEN];
	u8	reserved2[106];
};

struct qlcnic_npar_info {
	u16	vlan_id;
	u16	min_bw;
	u16	max_bw;
	u8	phy_port;
	u8	type;
	u8	active;
	u8	enable_pm;
	u8	dest_npar;
	u8	host_vlan_tag;
	u8	promisc_mode;
	u8	discard_tagged;
	u8	mac_learning;
};
struct qlcnic_eswitch {
	u8	port;
	u8	active_vports;
	u8	active_vlans;
	u8	active_ucast_filters;
	u8	max_ucast_filters;
	u8	max_active_vlans;

	u32	flags;
#define QLCNIC_SWITCH_ENABLE		BIT_1
#define QLCNIC_SWITCH_VLAN_FILTERING	BIT_2
#define QLCNIC_SWITCH_PROMISC_MODE	BIT_3
#define QLCNIC_SWITCH_PORT_MIRRORING	BIT_4
};


/* Return codes for Error handling */
#define QL_STATUS_INVALID_PARAM	-1

#define MAX_BW			100
#define MIN_BW			1
#define MAX_VLAN_ID		4095
#define MIN_VLAN_ID		2
#define MAX_TX_QUEUES		1
#define MAX_RX_QUEUES		4
#define DEFAULT_MAC_LEARN	1

#define IS_VALID_VLAN(vlan)	(vlan >= MIN_VLAN_ID && vlan <= MAX_VLAN_ID)
#define IS_VALID_BW(bw)		(bw >= MIN_BW && bw <= MAX_BW)
#define IS_VALID_TX_QUEUES(que)	(que > 0 && que <= MAX_TX_QUEUES)
#define IS_VALID_RX_QUEUES(que)	(que > 0 && que <= MAX_RX_QUEUES)
#define IS_VALID_MODE(mode)	(mode == 0 || mode == 1)

struct qlcnic_pci_func_cfg {
	u16	func_type;
	u16	min_bw;
	u16	max_bw;
	u16	port_num;
	u8	pci_func;
	u8	func_state;
	u8	def_mac_addr[6];
};

struct qlcnic_npar_func_cfg {
	u32	fw_capab;
	u16	port_num;
	u16	min_bw;
	u16	max_bw;
	u16	max_tx_queues;
	u16	max_rx_queues;
	u8	pci_func;
	u8	op_mode;
};

struct qlcnic_pm_func_cfg {
	u8	pci_func;
	u8	action;
	u8	dest_npar;
	u8	reserved[5];
};

struct qlcnic_esw_func_cfg {
	u16	vlan_id;
	u8	pci_func;
	u8	host_vlan_tag;
	u8	promisc_mode;
	u8	discard_tagged;
	u8	mac_learning;
	u8	reserved;
};

int qlcnic_fw_cmd_query_phy(struct qlcnic_adapter *adapter, u32 reg, u32 *val);
int qlcnic_fw_cmd_set_phy(struct qlcnic_adapter *adapter, u32 reg, u32 val);

u32 qlcnic_hw_read_wx_2M(struct qlcnic_adapter *adapter, ulong off);
int qlcnic_hw_write_wx_2M(struct qlcnic_adapter *, ulong off, u32 data);
int qlcnic_pci_mem_write_2M(struct qlcnic_adapter *, u64 off, u64 data);
int qlcnic_pci_mem_read_2M(struct qlcnic_adapter *, u64 off, u64 *data);
void qlcnic_pci_camqm_read_2M(struct qlcnic_adapter *, u64, u64 *);
void qlcnic_pci_camqm_write_2M(struct qlcnic_adapter *, u64, u64);

#define ADDR_IN_RANGE(addr, low, high)	\
	(((addr) < (high)) && ((addr) >= (low)))

#define QLCRD32(adapter, off) \
	(qlcnic_hw_read_wx_2M(adapter, off))
#define QLCWR32(adapter, off, val) \
	(qlcnic_hw_write_wx_2M(adapter, off, val))

int qlcnic_pcie_sem_lock(struct qlcnic_adapter *, int, u32);
void qlcnic_pcie_sem_unlock(struct qlcnic_adapter *, int);

#define qlcnic_rom_lock(a)	\
	qlcnic_pcie_sem_lock((a), 2, QLCNIC_ROM_LOCK_ID)
#define qlcnic_rom_unlock(a)	\
	qlcnic_pcie_sem_unlock((a), 2)
#define qlcnic_phy_lock(a)	\
	qlcnic_pcie_sem_lock((a), 3, QLCNIC_PHY_LOCK_ID)
#define qlcnic_phy_unlock(a)	\
	qlcnic_pcie_sem_unlock((a), 3)
#define qlcnic_api_lock(a)	\
	qlcnic_pcie_sem_lock((a), 5, 0)
#define qlcnic_api_unlock(a)	\
	qlcnic_pcie_sem_unlock((a), 5)
#define qlcnic_sw_lock(a)	\
	qlcnic_pcie_sem_lock((a), 6, 0)
#define qlcnic_sw_unlock(a)	\
	qlcnic_pcie_sem_unlock((a), 6)
#define crb_win_lock(a)	\
	qlcnic_pcie_sem_lock((a), 7, QLCNIC_CRB_WIN_LOCK_ID)
#define crb_win_unlock(a)	\
	qlcnic_pcie_sem_unlock((a), 7)

int qlcnic_get_board_info(struct qlcnic_adapter *adapter);
int qlcnic_wol_supported(struct qlcnic_adapter *adapter);
int qlcnic_config_led(struct qlcnic_adapter *adapter, u32 state, u32 rate);

/* Functions from qlcnic_init.c */
int qlcnic_load_firmware(struct qlcnic_adapter *adapter);
int qlcnic_need_fw_reset(struct qlcnic_adapter *adapter);
void qlcnic_request_firmware(struct qlcnic_adapter *adapter);
void qlcnic_release_firmware(struct qlcnic_adapter *adapter);
int qlcnic_pinit_from_rom(struct qlcnic_adapter *adapter);
int qlcnic_setup_idc_param(struct qlcnic_adapter *adapter);
int qlcnic_check_flash_fw_ver(struct qlcnic_adapter *adapter);

int qlcnic_rom_fast_read(struct qlcnic_adapter *adapter, int addr, int *valp);
int qlcnic_rom_fast_read_words(struct qlcnic_adapter *adapter, int addr,
				u8 *bytes, size_t size);
int qlcnic_alloc_sw_resources(struct qlcnic_adapter *adapter);
void qlcnic_free_sw_resources(struct qlcnic_adapter *adapter);

void __iomem *qlcnic_get_ioaddr(struct qlcnic_adapter *, u32);

int qlcnic_alloc_hw_resources(struct qlcnic_adapter *adapter);
void qlcnic_free_hw_resources(struct qlcnic_adapter *adapter);

int qlcnic_fw_create_ctx(struct qlcnic_adapter *adapter);
void qlcnic_fw_destroy_ctx(struct qlcnic_adapter *adapter);

void qlcnic_reset_rx_buffers_list(struct qlcnic_adapter *adapter);
void qlcnic_release_rx_buffers(struct qlcnic_adapter *adapter);
void qlcnic_release_tx_buffers(struct qlcnic_adapter *adapter);

int qlcnic_init_firmware(struct qlcnic_adapter *adapter);
void qlcnic_watchdog_task(struct work_struct *work);
void qlcnic_post_rx_buffers(struct qlcnic_adapter *adapter, u32 ringid,
		struct qlcnic_host_rds_ring *rds_ring);
int qlcnic_process_rcv_ring(struct qlcnic_host_sds_ring *sds_ring, int max);
void qlcnic_set_multi(struct net_device *netdev);
void qlcnic_free_mac_list(struct qlcnic_adapter *adapter);
int qlcnic_nic_set_promisc(struct qlcnic_adapter *adapter, u32);
int qlcnic_config_intr_coalesce(struct qlcnic_adapter *adapter);
int qlcnic_config_rss(struct qlcnic_adapter *adapter, int enable);
int qlcnic_config_ipaddr(struct qlcnic_adapter *adapter, u32 ip, int cmd);
int qlcnic_linkevent_request(struct qlcnic_adapter *adapter, int enable);
void qlcnic_advert_link_change(struct qlcnic_adapter *adapter, int linkup);

int qlcnic_fw_cmd_set_mtu(struct qlcnic_adapter *adapter, int mtu);
int qlcnic_change_mtu(struct net_device *netdev, int new_mtu);
int qlcnic_config_hw_lro(struct qlcnic_adapter *adapter, int enable);
int qlcnic_config_bridged_mode(struct qlcnic_adapter *adapter, u32 enable);
int qlcnic_send_lro_cleanup(struct qlcnic_adapter *adapter);
void qlcnic_update_cmd_producer(struct qlcnic_adapter *adapter,
		struct qlcnic_host_tx_ring *tx_ring);
int qlcnic_get_mac_addr(struct qlcnic_adapter *adapter, u8 *mac);
void qlcnic_clear_ilb_mode(struct qlcnic_adapter *adapter);
int qlcnic_set_ilb_mode(struct qlcnic_adapter *adapter);
void qlcnic_fetch_mac(struct qlcnic_adapter *, u32, u32, u8, u8 *);

/* Functions from qlcnic_main.c */
int qlcnic_reset_context(struct qlcnic_adapter *);
u32 qlcnic_issue_cmd(struct qlcnic_adapter *adapter,
	u32 pci_fn, u32 version, u32 arg1, u32 arg2, u32 arg3, u32 cmd);
void qlcnic_diag_free_res(struct net_device *netdev, int max_sds_rings);
int qlcnic_diag_alloc_res(struct net_device *netdev, int test);
int qlcnic_check_loopback_buff(unsigned char *data);
netdev_tx_t qlcnic_xmit_frame(struct sk_buff *skb, struct net_device *netdev);
void qlcnic_process_rcv_ring_diag(struct qlcnic_host_sds_ring *sds_ring);

/* Management functions */
int qlcnic_set_mac_address(struct qlcnic_adapter *, u8*);
int qlcnic_get_mac_address(struct qlcnic_adapter *, u8*);
int qlcnic_get_nic_info(struct qlcnic_adapter *, struct qlcnic_info *, u8);
int qlcnic_set_nic_info(struct qlcnic_adapter *, struct qlcnic_info *);
int qlcnic_get_pci_info(struct qlcnic_adapter *, struct qlcnic_pci_info*);
int qlcnic_reset_partition(struct qlcnic_adapter *, u8);

/*  eSwitch management functions */
int qlcnic_get_eswitch_capabilities(struct qlcnic_adapter *, u8,
				struct qlcnic_eswitch *);
int qlcnic_get_eswitch_status(struct qlcnic_adapter *, u8,
				struct qlcnic_eswitch *);
int qlcnic_toggle_eswitch(struct qlcnic_adapter *, u8, u8);
int qlcnic_config_switch_port(struct qlcnic_adapter *, u8, int, u8, u8,
			u8, u8, u16);
int qlcnic_config_port_mirroring(struct qlcnic_adapter *, u8, u8, u8);
extern int qlcnic_config_tso;

/*
 * QLOGIC Board information
 */

#define QLCNIC_MAX_BOARD_NAME_LEN 100
struct qlcnic_brdinfo {
	unsigned short  vendor;
	unsigned short  device;
	unsigned short  sub_vendor;
	unsigned short  sub_device;
	char short_name[QLCNIC_MAX_BOARD_NAME_LEN];
};

static const struct qlcnic_brdinfo qlcnic_boards[] = {
	{0x1077, 0x8020, 0x1077, 0x203,
		"8200 Series Single Port 10GbE Converged Network Adapter "
		"(TCP/IP Networking)"},
	{0x1077, 0x8020, 0x1077, 0x207,
		"8200 Series Dual Port 10GbE Converged Network Adapter "
		"(TCP/IP Networking)"},
	{0x1077, 0x8020, 0x1077, 0x20b,
		"3200 Series Dual Port 10Gb Intelligent Ethernet Adapter"},
	{0x1077, 0x8020, 0x1077, 0x20c,
		"3200 Series Quad Port 1Gb Intelligent Ethernet Adapter"},
	{0x1077, 0x8020, 0x1077, 0x20f,
		"3200 Series Single Port 10Gb Intelligent Ethernet Adapter"},
	{0x1077, 0x8020, 0x0, 0x0, "cLOM8214 1/10GbE Controller"},
};

#define NUM_SUPPORTED_BOARDS ARRAY_SIZE(qlcnic_boards)

static inline u32 qlcnic_tx_avail(struct qlcnic_host_tx_ring *tx_ring)
{
	smp_mb();
	if (tx_ring->producer < tx_ring->sw_consumer)
		return tx_ring->sw_consumer - tx_ring->producer;
	else
		return tx_ring->sw_consumer + tx_ring->num_desc -
				tx_ring->producer;
}

extern const struct ethtool_ops qlcnic_ethtool_ops;

struct qlcnic_nic_template {
	int (*get_mac_addr) (struct qlcnic_adapter *, u8*);
	int (*config_bridged_mode) (struct qlcnic_adapter *, u32);
	int (*config_led) (struct qlcnic_adapter *, u32, u32);
	int (*start_firmware) (struct qlcnic_adapter *);
};

#define QLCDB(adapter, lvl, _fmt, _args...) do {	\
	if (NETIF_MSG_##lvl & adapter->msg_enable)	\
		printk(KERN_INFO "%s: %s: " _fmt,	\
			 dev_name(&adapter->pdev->dev),	\
			__func__, ##_args);		\
	} while (0)

#endif				/* __QLCNIC_H_ */
