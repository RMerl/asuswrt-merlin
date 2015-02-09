/*
 * Broadcom PCIE
 * Software-specific definitions shared between device and host side
 * Explains the shared area between host and dongle
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
 * $Id: bcmpcie.h  $
 */

#ifndef	_bcmpcie_h_
#define	_bcmpcie_h_

#include <circularbuf.h>

#define ADDR_64(x)			(x.addr)
#define HIGH_ADDR_32(x)     ((uint32) (((sh_addr_t) x).high_addr))
#define LOW_ADDR_32(x)      ((uint32) (((sh_addr_t) x).low_addr))

typedef struct {
	uint32 low_addr;
	uint32 high_addr;
} sh_addr_t;



#ifdef BCMPCIE_SUPPORT_TX_PUSH_RING
#define BCMPCIE_PUSH_TX_RING	1
#else
#define BCMPCIE_PUSH_TX_RING	0
#endif /* BCMPCIE_SUPPORT_TX_PUSH_RING */

#define BCMPCIE_MAX_TX_FLOWS	40

#define PCIE_SHARED_VERSION		0x0005
#define PCIE_SHARED_VERSION_MASK  0x00FF
#define PCIE_SHARED_ASSERT_BUILT  0x0100
#define PCIE_SHARED_ASSERT        0x0200
#define PCIE_SHARED_TRAP          0x0400
#define PCIE_SHARED_IN_BRPT       0x0800
#define PCIE_SHARED_SET_BRPT      0x1000
#define PCIE_SHARED_PENDING_BRPT  0x2000
#define PCIE_SHARED_TXPUSH_SPRT		0x4000


#define BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT		0
#define BCMPCIE_H2D_MSGRING_RXPOST_SUBMIT		1
#define BCMPCIE_D2H_MSGRING_CONTROL_COMPLETE		2
#define BCMPCIE_D2H_MSGRING_TX_COMPLETE			3
#define BCMPCIE_D2H_MSGRING_RX_COMPLETE			4
#define BCMPCIE_COMMON_MSGRING_MAX_ID			4

/* Added only for single tx ring */
#define BCMPCIE_H2D_TXFLOWRINGID			5

#define BCMPCIE_H2D_COMMON_MSGRINGS			2
#define BCMPCIE_D2H_COMMON_MSGRINGS			3
#define BCMPCIE_COMMON_MSGRINGS				5

enum h2dring_idx {
	BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT_IDX = 0,
	BCMPCIE_H2D_MSGRING_RXPOST_SUBMIT_IDX =	1,
	BCMPCIE_H2D_MSGRING_TXFLOW_IDX_START = 2
};

enum d2hring_idx {
	BCMPCIE_D2H_MSGRING_CONTROL_COMPLETE_IDX = 0,
	BCMPCIE_D2H_MSGRING_TX_COMPLETE_IDX = 1,
	BCMPCIE_D2H_MSGRING_RX_COMPLETE_IDX = 2
};

typedef struct ring_mem {
	uint16		idx;
	uint8		type;
	uint8 rsvd;
	uint16		max_item;
	uint16		len_items;
	sh_addr_t base_addr;
} ring_mem_t;

#define RINGSTATE_INITED	1

typedef struct ring_state {
	uint8 idx;
	uint8 state;
	uint16 r_offset;
	uint16 w_offset;
	uint16 e_offset;
} ring_state_t;


typedef struct ring_info {
	/* locations in the TCM where the ringmem is and ringstate are defined */
	uint32		ringmem_ptr; 	/* ring mem location in TCM */
	uint32		h2d_w_idx_ptr;

	uint32		h2d_r_idx_ptr;
	uint32		d2h_w_idx_ptr;

	uint32		d2h_r_idx_ptr;
	/* host locations where the DMA of read/write indices are */
	sh_addr_t	d2h_w_idx_hostaddr;
	sh_addr_t	d2h_r_idx_hostaddr;
	sh_addr_t	h2d_w_idx_hostaddr;
	sh_addr_t	h2d_r_idx_hostaddr;
	uint16		max_sub_queues;
	uint16		rsvd;
} ring_info_t;

typedef struct {
	/* shared area version captured at flags 7:0 */
	uint32	flags;

	uint32  trap_addr;
	uint32  assert_exp_addr;
	uint32  assert_file_addr;
	uint32  assert_line;
	uint32	console_addr;		/* Address of hndrte_cons_t */

	uint32  msgtrace_addr;

	uint32  fwid;

	/* Used for debug/flow control */
	uint16  total_lfrag_pkt_cnt;
	uint16  max_host_rxbufs; /* rsvd in spec */

	uint32 dma_rxoffset; /* rsvd in spec */

	/* these will be used for sleep request/ack, d3 req/ack */
	uint32  h2d_mb_data_ptr;
	uint32  d2h_mb_data_ptr;

	/* information pertinent to host IPC/msgbuf channels */
	/* location in the TCM memory which has the ring_info */
	uint32	rings_info_ptr;

	/* block of host memory for the scratch buffer */
	uint32		host_dma_scratch_buffer_len;
	sh_addr_t	host_dma_scratch_buffer;

	/* block of host memory for the dongle to push the status into */
	uint32		device_rings_stsblk_len;
	sh_addr_t	device_rings_stsblk;
} pciedev_shared_t;


/* H2D mail box Data */
#define H2D_HOST_D3_INFORM	0x00000001
#define H2D_HOST_DS_ACK		0x00000002
#define H2D_HOST_CONS_INT	0x80000000	/* h2d int for console cmds  */

/* D2H mail box Data */
#define D2H_DEV_D3_ACK		0x00000001
#define D2H_DEV_DS_ENTER_REQ	0x00000002
#define D2H_DEV_DS_EXIT_NOTE	0x00000004
#define D2H_DEV_FWHALT		0x10000000


extern pciedev_shared_t pciedev_shared;
#define NEXTTXP(i, d)           ((((i)+1) >= (d)) ? 0 : ((i)+1))
#define NTXPACTIVE(r, w, d)     (((r) <= (w)) ? ((w)-(r)) : ((d)-(r)+(w)))
#define NTXPAVAIL(r, w, d)      (((d) - NTXPACTIVE((r), (w), (d))) > 1)

/* Function can be used to notify host of FW halt */
extern void pciedev_fwhalt(void);
#define READ_AVAIL_SPACE(w, r, d)		\
			((w >= r) ? (w - r) : (d - r))

#define WRT_PEND(x)	(x)->wr_pending
#define DNGL_RING_WPTR(msgbuf)		*((msgbuf)->tcm_rs_w_ptr)
#define BCMMSGBUF_RING_SET_W_PTR(msgbuf, a)	(DNGL_RING_WPTR(msgbuf) = (a))

#define DNGL_RING_RPTR(msgbuf)		*((msgbuf)->tcm_rs_r_ptr)
#define BCMMSGBUF_RING_SET_R_PTR(msgbuf, a)	(DNGL_RING_RPTR(msgbuf) = (a))

#define  RING_READ_PTR(x)       ((x)->ringstate->r_offset)
#define  RING_WRITE_PTR(x)      ((x)->ringstate->w_offset)
#define  RING_START_PTR(x)	((x)->ringmem->base_addr.low_addr)
#define  RING_MAX_ITEM(x)	((x)->ringmem->max_item)
#define  RING_LEN_ITEMS(x)	((x)->ringmem->len_items)
#define	 HOST_RING_BASE(x)	((x)->ring_base)


#define WRITE_SPACE_AVAIL_CONTINUOUS(r, w, d)		((w >= r) ? (d - w) : (r - w))
#define WRITE_SPACE_AVAIL(r, w, d)	(d - (NTXPACTIVE(r, w, d)) - 1)
#define CHECK_WRITE_SPACE(r, w, d)	WRITE_SPACE_AVAIL(r, w, d) ? \
	WRITE_SPACE_AVAIL_CONTINUOUS(r, w, d) : 0

#endif	/* _bcmpcie_h_ */
