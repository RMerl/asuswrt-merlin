/*
 * Broadcom SiliconBackplane FA (Flow accelerator) definitions
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: $
 */

#ifndef	_FA_CORE_H_
#define	_FA_CORE_H_

#define FA_BASE_OFFSET		0xc00

/* FA counter stats offsets */
#define FASTAT_HIT		0	/* NAPT lookup hits count */
#define FASTAT_MISS		1	/* NAPT lookup miss count */
#define FASTAT_SNAP_FAIL	2	/* SNAP failures */
#define FASTAT_TYPE_FAIL	3	/* Ethernet type failures */
#define FASTAT_VER_FAIL		4	/* Version failures */
#define FASTAT_FRAG_FAIL	5	/* Fragmentation Failures */
#define FASTAT_PROT_FAIL	6	/* UDP/TCP protocol failures */
#define FASTAT_V4CS_FAIL	7	/* IPV4 checksum failures */
#define FASTAT_V4OP_FAIL	8	/* IPV4 option failures */
#define FASTAT_V4HL_FAIL	9	/* IPV4 hdr len failures */
#define FASTAT_MAX		10

/* Capture address of ECC error */
#define FAECC_NPTFL_STAT	0
#define FAECC_NHOP_STAT		1
#define FAECC_HWQ_STAT		2
#define FAECC_LAB_STAT		3
#define FAECC_HB_STAT		4
#define FAECC_STAT_MAX		5

#define FA_MAXDATA		8	/* 0:255 bits */

/* Flow-Accelerator Config registers */
typedef volatile struct _faregs {
	uint32	control;		/* 0x0 */
	uint32	mem_acc_ctl;		/* 0x4 */
	uint32	bcm_hdr_ctl;		/* 0x8 */
	uint32	l2_skip_ctl;		/* 0xc */
	uint32	l2_tag;			/* 0x10 */
	uint32	l2_llc_max_len;		/* 0x14 */
	uint32	l2_snap_typelo;		/* 0x18 */
	uint32	l2_snap_typehi;		/* 0x1c */
	uint32	l2_ethtype;		/* 0x20 */
	uint32	l3_ipv6_type;		/* 0x24 */
	uint32	l3_ipv4_type;		/* 0x28 */
	uint32	l3_napt_ctl;		/* 0x2c */
	uint32	status;			/* 0x30 */
	uint32	status_mask;		/* 0x34 */
	uint32	rcv_status_en;		/* 0x38 */
	uint32	stats[FASTAT_MAX];	/* 0x3c ... 0x60 */
	uint32	error;			/* 0x64 */
	uint32	error_mask;		/* 0x68 */
	uint32	dbg_ctl;		/* 0x6c */
	uint32	dbg_status;		/* 0x70 */
	uint32	mem_dbg;		/* 0x74 */
	uint32	ecc_dbg;		/* 0x78 */
	uint32	ecc_error;		/* 0x7c */
	uint32	ecc_error_mask;		/* 0x80 */
	uint32	eccst[FAECC_STAT_MAX];	/* 0x84 ... 0x94 */
	uint32	hwq_max_depth;		/* 0x98 */
	uint32	lab_max_depth;		/* 0x9c */
	uint32	m_accdata[FA_MAXDATA];	/* 0xa0 ... 0xa7 */
} faregs_t;

/* FA CTF Control Register(0x0) */
#define CTF_CTL_SW_ACC_MODE			(1 << 0)
#define CTF_CTL_BYPASS_CTF			(1 << 1)
#define CTF_CTL_CRC_FWD				(1 << 2)
#define CTF_CTL_CRC_OWRT			(1 << 3)
#define CTF_CTL_HWQ_THRESHLD_MASK		(0x1FF << 4)
#define CTF_CTL_NAPT_FLOW_INIT			(1 << 13)
#define CTF_CTL_NEXT_HOP_INIT			(1 << 14)
#define CTF_CTL_HWQ_INIT			(1 << 15)
#define CTF_CTL_LAB_INIT			(1 << 16)
#define CTF_CTL_HB_INIT				(1 << 17)
#define CTF_CTL_DSBL_MAC_DA_CHECK		(1 << 18)

#define CTF_CTL_HWQ_DEF_THRESHLD		0x140

/* FA CTF Memory Access Control Register(0x4) */
#define CTF_MEMACC_TAB_INDEX_MASK		(0x3FF << 0)
#define CTF_MEMACC_TAB_SEL_MASK			(0x3 << 10)
#define CTF_MEMACC_NAPT_FLOW_TAB		(~(3 << 10))
#define CTF_MEMACC_NAPT_POOL_TAB		(1 << 10)
#define CTF_MEMACC_NEXT_HOP_TAB			(2 << 10)
#define CTF_MEMACC_RD_WR_N			(1 << 12)
#define CTF_MEMACC_CUR_TBL_INDEX_MASK		(0x3FF << 13)

#define CTF_MEMACC_TBL_NF			0
#define CTF_MEMACC_TBL_NP			1
#define CTF_MEMACC_TBL_NH			2
#define CTF_MEMACC_RD_TABLE(t, i)		(CTF_MEMACC_RD_WR_N | (t << 10) | \
						 (i & CTF_MEMACC_TAB_INDEX_MASK))
#define CTF_MEMACC_WR_TABLE(t, i)		((t << 10) | (i & CTF_MEMACC_TAB_INDEX_MASK))

/* FA CTF BRCM Header Control Register(0x8) */
#define CTF_BRCM_HDR_HW_EN			(1 << 0)
#define CTF_BRCM_HDR_SW_RX_EN			(1 << 1)
#define CTF_BRCM_HDR_SW_TX_EN			(1 << 2)
#define CTF_BRCM_HDR_PARSE_IGN_EN		(1 << 3)
#define CTF_BRCM_HDR_TE				(0x3 << 4)
#define CTF_BRCM_HDR_TC				(0x7 << 6)

/* FA CTF L2 Skip Control Register(0xc) */
#define CTF_L2SKIP_ET_SKIP_TYPE_MASK		(0xFFFF << 0)
#define CTF_L2SKIP_ET_SKIP_BYTES_MASK		(0x7 << 16)
#define CTF_L2SKIP_ET_SKIP_ENABLE		(1 << 19)
#define CTF_L2SKIP_ET_TO_SNAP_CONV		(1 << 20)

/* FA CTF L2 Tag Type Register(0x10) */
#define CTF_L2TAG_ET_TAG_TYPE0_MASK		(0xFFFF << 0)
#define CTF_L2TAG_ET_TAG_TYPE1_MASK		(0xFFFF << 16)

/* FA CTF L2 LLC Max Length Register(0x14) */
#define CTF_L2LLC_MAX_LENGTH_MASK		(0xFFFF << 0)
#define CTF_LLC_MAX_LENGTH_DEF			0x5DC

/* FA CTF L2 Ether Type Register(0x20) */
#define CTF_L2ET_IPV6				(0xFFFF << 16)
#define CTF_L2ET_IPV4				(0xFFFF << 0)

/* FA CTF L3 IPv6 Type Register(0x24) */
#define CTF_L3_IPV6_NEXT_HDR_TCP		(0xFF << 0)
#define CTF_L3_IPV6_HDR_DEF_TCP			0x6
#define CTF_L3_IPV6_NEXT_HDR_UDP		(0xFF << 8)
#define CTF_L3_IPV6_HDR_DEF_UDP 		0x11

/* FA CTF L3 IPv4 Type Register(0x28) */
#define CTF_L3_IPV4_NEXT_HDR_TCP		(0xFF << 0)
#define CTF_L3_IPV4_HDR_DEF_TCP			0x6
#define CTF_L3_IPV4_NEXT_HDR_UDP		(0xFF << 8)
#define CTF_L3_IPV4_HDR_DEF_UDP			0x11
#define CTF_L3_IPV4_CKSUM_EN			(1 << 16)

/* FA CTF L3 NAPT Control Register(0x2c) */
#define CTFCTL_L3NAPT_HDR_DEC_TTL		(1 << 21)
#define CTFCTL_L3NAPT_HASH_SEL			(1 << 20)
#define CTFCTL_L3NAPT_HITS_CLR_ON_RD_EN		(1 << 19)
#define CTFCTL_L3NAPT_TS_SHIFT			(16)
#define CTFCTL_L3NAPT_TIMESTAMP			(0x7 << CTFCTL_L3NAPT_TS_SHIFT)
#define CTFCTL_L3NAPT_HASH_SEED			(0xFFFF << 0)

#define CTFCTL_L3NAPT_TS_NBITS	3
#define CTFCTL_MAX_TIMESTAMP_VAL		((1 << CTFCTL_L3NAPT_TS_NBITS) - 1)
#define CTFCTL_TIMESTAMP_MASK			((1 << CTFCTL_L3NAPT_TS_NBITS) - 1)
#define CTFCTL_TIMESTAMP_NUM_STATES		(1 << 3)

/* FA CTF Interrupt Status Register(0x30) */
#define CTF_INTSTAT_HB_INIT_DONE		(1 << 9)
#define CTF_INTSTAT_LAB_INIT_DONE		(1 << 8)
#define CTF_INTSTAT_HWQ_INIT_DONE		(1 << 7)
#define CTF_INTSTAT_NXT_HOP_INIT_DONE		(1 << 6)
#define CTF_INTSTAT_NAPT_FLOW_INIT_DONE		(1 << 5)
#define CTF_INTSTAT_BRCM_HDR_INIT_DONE		(1 << 4)
#define CTF_INTSTAT_IPV4_CKSUM_ERR		(1 << 3)
#define CTF_INTSTAT_L3_PARSE_INCOMP		(1 << 2)
#define CTF_INTSTAT_L2_PARSE_INCOMP		(1 << 1)
#define CTF_INTSTAT_BRCM_HDR_PARSE_INCOMP	(1 << 0)
#define CTF_INTSTAT_INIT_DONE			(CTF_INTSTAT_HB_INIT_DONE | \
						 CTF_INTSTAT_LAB_INIT_DONE | \
						 CTF_INTSTAT_HWQ_INIT_DONE | \
						 CTF_INTSTAT_NXT_HOP_INIT_DONE | \
						 CTF_INTSTAT_NAPT_FLOW_INIT_DONE)

/* FA CTF Interrupt Status Mask Register(0x34) */
#define CTF_INTMASK_HB_INIT_DONE		~(1 << 9)
#define CTF_INTMASK_LAB_INIT_DONE		~(1 << 8)
#define CTF_INTMASK_HWQ_INIT_DONE		~(1 << 7)
#define CTF_INTMASK_NXT_HOP_INIT_DONE		~(1 << 6)
#define CTF_INTMASK_NAPT_FLOW_INIT_DONE		~(1 << 5)
#define CTF_INTMASK_BRCM_HDR_INIT_DONE		~(1 << 4)
#define CTF_INTMASK_IPV4_CKSUM_ERR		~(1 << 3)
#define CTF_INTMASK_L3_PARSE_INCOMP		~(1 << 2)
#define CTF_INTMASK_L2_PARSE_INCOMP		~(1 << 1)
#define CTF_INTMASK_BRCM_HDR_PARSE_INCOMP	~(1 << 0)

/* FA CTF Receive Status Mask Register(0x38) */
#define CTF_RXMASK_L3PROTO_EXT_FAIL		~(1 << 7)
#define CTF_RXMASK_L3PROTO_IPV4_HDR_LEN_FAIL	~(1 << 6)
#define CTF_RXMASK_L3PROTO_IPV4_OPT_FAIL	~(1 << 5)
#define CTF_RXMASK_L3PROTO_IPV4_CKSUM_FAIL	~(1 << 4)
#define CTF_RXMASK_L3PROTO_FRAG_FAIL		~(1 << 3)
#define CTF_RXMASK_L3PROTO_VER_FAIL		~(1 << 2)
#define CTF_RXMASK_L3PROTO_L2ETYPE_FAIL		~(1 << 1)
#define CTF_RXMASK_L3PROTO_L2SNAP_FAIL		~(1 << 0)

/* FA CTF Error Status Register(0x64) */
#define CTF_ERR_HWQ_OVFLOW			(1 << 8)
#define CTF_ERR_HB_OVFLOW			(1 << 7)
#define CTF_ERR_RXQ_OVFLOW			(1 << 6)
#define CTF_ERR_SOP_EOP				(1 << 5)
#define CTF_ERR_SPB_OVFLOW			(1 << 4)
#define CTF_ERR_LAB_OVFLOW			(1 << 3)
#define CTF_ERR_INT_MERGE			(1 << 2)
#define CTF_ERR_TXQ_OVFLOW			(1 << 1)
#define CTF_ERR_RB_OVFLOW			(1 << 0)

/* FA CTF Error Status Mask Register(0x68) */
#define CTF_ERR_HWQ_OVFLOW_MASK			~(1 << 8)
#define CTF_ERR_HB_OVFLOW_MASK			~(1 << 7)
#define CTF_ERR_RXQ_OVFLOW_MASK			~(1 << 6)
#define CTF_ERR_SOP_EOP_MASK			~(1 << 5)
#define CTF_ERR_SPB_OVFLOW_MASK			~(1 << 4)
#define CTF_ERR_LAB_OVFLOW_MASK			~(1 << 3)
#define CTF_ERR_INT_MERGE_MASK			~(1 << 2)
#define CTF_ERR_TXQ_OVFLOW_MASK			~(1 << 1)
#define CTF_ERR_RB_OVFLOW_MASK			~(1 << 0)

/* FA CTF Debug Control Register(0x6c) */
#define CTF_DBG_OK_TO_SEND			(0xF << 6)
#define CTF_DBG_FORCE_ALL_HIT			(1 << 2)
#define CTF_DBG_FORCE_ALL_MISS			(1 << 1)
#define CTF_DBG_REG				(1 << 0)

/* FA CTF Debug Control Register(0x70) */
#define CTF_DBG_MEM_ACC_BUSY			(1 << 0)

#define CTF_DATA_SIZE				8
#define CTF_MAX_POOL_TABLE_INDEX		4
#define CTF_MAX_NEXTHOP_TABLE_INDEX		128
#define CTF_MAX_BUCKET_INDEX			4
#define CTF_MAX_FLOW_TABLE			1024

/* Next hop defines */
#define CTF_NH_L2FR_TYPE_ET			0
#define CTF_NH_L2FR_TYPE_SNAP			1
#define CTF_NH_OP_STAG				0
#define CTF_NH_OP_CTAG				1
#define CTF_NH_OP_NOTAG				2

/* Pool entry defines */
#define CTF_NP_INTERNAL				0
#define CTF_NP_EXTERNAL				1

/* NAPT entry definitions */
#define CTF_NAPT_TSTAMP_MASK			0x7
#define CTF_NAPT_OVRW_IP			1
#define CTF_NAPT_OVRW_PORT			2
#define CTF_NAPT_ACTION_MASK			0x3
#define CTF_NAPT_DMA_UNIMAC			0
#define CTF_NAPT_DMA_IHOST			1

#define CTF_FA_MACC_DATA0_TS(d)			(d[0] & CTF_NAPT_TSTAMP_MASK)

#endif	/* _FA_CORE_H_ */
