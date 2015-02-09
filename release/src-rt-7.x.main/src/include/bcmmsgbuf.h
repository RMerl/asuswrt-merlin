/*
 * MSGBUF network driver ioctl/indication encoding
 * Broadcom 802.11abg Networking Device Driver
 *
 * Definitions subject to change without notice.
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
 * $Id: bcmmsgbuf.h  $
 */
#ifndef _bcmmsgbuf_h_
#define	_bcmmsgbuf_h_
#include <proto/ethernet.h>
#include <wlioctl.h>
#include <bcmpcie.h>

#define MSGBUF_MAX_MSG_SIZE   ETHER_MAX_LEN

#define H2DRING_TXPOST_ITEMSIZE		48
#define H2DRING_RXPOST_ITEMSIZE		32
#define H2DRING_CTRL_SUB_ITEMSIZE	40
#define D2HRING_TXCMPLT_ITEMSIZE	16
#define D2HRING_RXCMPLT_ITEMSIZE	32
#define D2HRING_CTRL_CMPLT_ITEMSIZE	24

#define H2DRING_TXPOST_MAX_ITEM			256
#define H2DRING_RXPOST_MAX_ITEM			256
#define H2DRING_CTRL_SUB_MAX_ITEM		20
#define D2HRING_TXCMPLT_MAX_ITEM		256
#define D2HRING_RXCMPLT_MAX_ITEM		256
#define D2HRING_CTRL_CMPLT_MAX_ITEM		20
enum {
	DNGL_TO_HOST_MSGBUF,
	HOST_TO_DNGL_MSGBUF
};

enum {
	HOST_TO_DNGL_TXP_DATA,
	HOST_TO_DNGL_RXP_DATA,
	HOST_TO_DNGL_CTRL,
	DNGL_TO_HOST_DATA,
	DNGL_TO_HOST_CTRL
};

#define MESSAGE_PAYLOAD(a) (a & MSG_TYPE_INTERNAL_USE_START) ? TRUE : FALSE

#ifdef PCIE_API_REV1

#define BCMMSGBUF_DUMMY_REF(a, b)	do {BCM_REFERENCE((a));BCM_REFERENCE((b));}  while (0)

#define BCMMSGBUF_API_IFIDX(a)		0
#define BCMMSGBUF_API_SEQNUM(a)		0
#define BCMMSGBUF_IOCTL_XTID(a)		0
#define BCMMSGBUF_IOCTL_PKTID(a)	((a)->cmd_id)

#define BCMMSGBUF_SET_API_IFIDX(a, b)	BCMMSGBUF_DUMMY_REF(a, b)
#define BCMMSGBUF_SET_API_SEQNUM(a, b)	BCMMSGBUF_DUMMY_REF(a, b)
#define BCMMSGBUF_IOCTL_SET_PKTID(a, b)	(BCMMSGBUF_IOCTL_PKTID(a) = (b))
#define BCMMSGBUF_IOCTL_SET_XTID(a, b)	BCMMSGBUF_DUMMY_REF(a, b)

#else /* PCIE_API_REV1 */

#define BCMMSGBUF_API_IFIDX(a)		((a)->if_id)
#define BCMMSGBUF_IOCTL_PKTID(a)	((a)->pkt_id)
#define BCMMSGBUF_API_SEQNUM(a)		((a)->u.seq.seq_no)
#define BCMMSGBUF_IOCTL_XTID(a)		((a)->xt_id)

#define BCMMSGBUF_SET_API_IFIDX(a, b)	(BCMMSGBUF_API_IFIDX((a)) = (b))
#define BCMMSGBUF_SET_API_SEQNUM(a, b)	(BCMMSGBUF_API_SEQNUM((a)) = (b))
#define BCMMSGBUF_IOCTL_SET_PKTID(a, b)	(BCMMSGBUF_IOCTL_PKTID((a)) = (b))
#define BCMMSGBUF_IOCTL_SET_XTID(a, b)	(BCMMSGBUF_IOCTL_XTID((a)) = (b))

#endif /* PCIE_API_REV1 */

/* utility data structures */
union addr64 {
	struct {
		uint32 low;
		uint32 high;
	};
	struct {
		uint32 low_addr;
		uint32 high_addr;
	};
	uint64 u64;
} DECLSPEC_ALIGN(8);

typedef union addr64 addr64_t;

/* IOCTL req Hdr */
/* cmn Msg Hdr */
typedef struct cmn_msg_hdr {
	/* message type */
	uint8 msg_type;
	/* interface index this is valid for */
	uint8 if_id;
	/* flags */
	uint8 flags;
	/* alignment */
	uint8 reserved;
	/* packet Identifier for the associated host buffer */
	uint32 request_id;
} cmn_msg_hdr_t;

/* message type */
typedef enum bcmpcie_msgtype {
	MSG_TYPE_GEN_STATUS 		= 0x1,
	MSG_TYPE_RING_STATUS		= 0x2,
	MSG_TYPE_FLOW_RING_CREATE	= 0x3,
	MSG_TYPE_FLOW_RING_CREATE_CMPLT	= 0x4,
	MSG_TYPE_FLOW_RING_DELETE	= 0x5,
	MSG_TYPE_FLOW_RING_DELETE_CMPLT	= 0x6,
	MSG_TYPE_FLOW_RING_FLUSH	= 0x7,
	MSG_TYPE_FLOW_RING_FLUSH_CMPLT	= 0x8,
	MSG_TYPE_IOCTLPTR_REQ		= 0x9,
	MSG_TYPE_IOCTLPTR_REQ_ACK	= 0xA,
	MSG_TYPE_IOCTLRESP_BUF_POST	= 0xB,
	MSG_TYPE_IOCTL_CMPLT		= 0xC,
	MSG_TYPE_EVENT_BUF_POST		= 0xD,
	MSG_TYPE_WL_EVENT		= 0xE,
	MSG_TYPE_TX_POST		= 0xF,
	MSG_TYPE_TX_STATUS		= 0x10,
	MSG_TYPE_RXBUF_POST		= 0x11,
	MSG_TYPE_RX_CMPLT		= 0x12,
	MSG_TYPE_LPBK_DMAXFER 		= 0x13,
	MSG_TYPE_LPBK_DMAXFER_CMPLT	= 0x14,
	MSG_TYPE_API_MAX_RSVD		= 0x3F
} bcmpcie_msg_type_t;

typedef enum bcmpcie_msgtype_int {
	MSG_TYPE_INTERNAL_USE_START	= 0x40,
	MSG_TYPE_EVENT_PYLD		= 0x41,
	MSG_TYPE_IOCT_PYLD		= 0x42,
	MSG_TYPE_RX_PYLD		= 0x43,
	MSG_TYPE_HOST_FETCH		= 0x44,
	MSG_TYPE_LPBK_DMAXFER_PYLD	= 0x45,
	MSG_TYPE_TXMETADATA_PYLD	= 0x46,
	MSG_TYPE_HOSTDMA_PTRS		= 0x47
} bcmpcie_msgtype_int_t;

typedef enum bcmpcie_msgtype_u {
	MSG_TYPE_TX_BATCH_POST		= 0x80,
	MSG_TYPE_IOCTL_REQ		= 0x81,
	MSG_TYPE_HOST_EVNT		= 0x82,
	MSG_TYPE_LOOPBACK		= 0x83
} bcmpcie_msgtype_u_t;


/* if_id */
#define BCMPCIE_CMNHDR_IFIDX_PHYINTF_SHFT	5
#define BCMPCIE_CMNHDR_IFIDX_PHYINTF_MAX	0x7
#define BCMPCIE_CMNHDR_IFIDX_PHYINTF_MASK	\
	(BCMPCIE_CMNHDR_IFIDX_PHYINTF_MAX << BCMPCIE_CMNHDR_IFIDX_PHYINTF_SHFT)
#define BCMPCIE_CMNHDR_IFIDX_VIRTINTF_SHFT	0
#define BCMPCIE_CMNHDR_IFIDX_VIRTINTF_MAX	0x1F
#define BCMPCIE_CMNHDR_IFIDX_VIRTINTF_MASK	\
	(BCMPCIE_CMNHDR_IFIDX_PHYINTF_MAX << BCMPCIE_CMNHDR_IFIDX_PHYINTF_SHFT)

/* flags */
#define BCMPCIE_CMNHDR_FLAGS_DMA_R_IDX		0x1
#define BCMPCIE_CMNHDR_FLAGS_DMA_R_IDX_INTR	0x2
#define BCMPCIE_CMNHDR_FLAGS_PHASE_BIT		0x80


/* IOCTL request message */
typedef struct ioctl_req_msg {
	/* common message header */
	cmn_msg_hdr_t 	cmn_hdr;

	/* ioctl command type */
	uint32		cmd;
	/* ioctl transaction ID, to pair with a ioctl response */
	uint16		trans_id;
	/* input arguments buffer len */
	uint16		input_buf_len;
	/* expected output len */
	uint16		output_buf_len;
	/* to aling the host address on 8 byte boundary */
	uint16		rsvd[3];
	/* always aling on 8 byte boundary */
	addr64_t	host_input_buf_addr;
	/* rsvd */
	uint32		rsvd1[2];
} ioctl_req_msg_t;

/* buffer post messages for device to use to return IOCTL responses, Events */
typedef struct ioctl_resp_evt_buf_post_msg {
	/* common message header */
	cmn_msg_hdr_t	cmn_hdr;
	/* length of the host buffer supplied */
	uint16		host_buf_len;
	/* to aling the host address on 8 byte boundary */
	uint16		reserved[3];
	/* always aling on 8 byte boundary */
	addr64_t	host_buf_addr;
	uint32		rsvd[4];
} ioctl_resp_evt_buf_post_msg_t;


typedef struct pcie_dma_xfer_params {
	/* common message header */
	cmn_msg_hdr_t	cmn_hdr;

	/* always aling on 8 byte boundary */
	addr64_t	host_input_buf_addr;

	/* always aling on 8 byte boundary */
	addr64_t	host_ouput_buf_addr;

	/* length of transfer */
	uint32		xfer_len;
	/* delay before doing the src txfer */
	uint32		srcdelay;
	/* delay before doing the dest txfer */
	uint32		destdelay;
	uint32		rsvd;
} pcie_dma_xfer_params_t;

/* Complete msgbuf hdr for flow ring update from host to dongle */
typedef struct tx_flowring_create_request {
	cmn_msg_hdr_t   msg;
	uint8	da[ETHER_ADDR_LEN];
	uint8	sa[ETHER_ADDR_LEN];
	uint8	tid;
	uint8 	if_flags;
	uint16	flow_ring_id;
	uint8 	tc;
	uint8	priority;
	uint16 	int_vector;
	uint16	max_items;
	uint16	len_item;
	addr64_t flow_ring_ptr;
} tx_flowring_create_request_t;

typedef struct tx_flowring_delete_request {
	cmn_msg_hdr_t   msg;
	uint16	flow_ring_id;
	uint16 	reason;
	uint32	rsvd[7];
} tx_flowring_delete_request_t;

typedef struct tx_flowring_flush_request {
	cmn_msg_hdr_t   msg;
	uint16	flow_ring_id;
	uint16 	reason;
	uint32	rsvd[7];
} tx_flowring_flush_request_t;

typedef union ctrl_submit_item {
	ioctl_req_msg_t			ioctl_req;
	ioctl_resp_evt_buf_post_msg_t	resp_buf_post;
	pcie_dma_xfer_params_t		dma_xfer;
	tx_flowring_create_request_t	flow_create;
	tx_flowring_delete_request_t	flow_delete;
	tx_flowring_flush_request_t	flow_flush;
	unsigned char			check[H2DRING_CTRL_SUB_ITEMSIZE];
} ctrl_submit_item_t;

/* Control Completion messages (20 bytes) */
typedef struct compl_msg_hdr {
	/* status for the completion */
	int16	status;
	/* submisison flow ring id which generated this status */
	uint16	flow_ring_id;
} compl_msg_hdr_t;

/* completion header status codes */
#define	BCMPCIE_SUCCESS			0
#define BCMPCIE_NOTFOUND		1
#define BCMPCIE_NOMEM			2
#define BCMPCIE_BADOPTION		3
#define BCMPCIE_RING_IN_USE		4
#define BCMPCIE_RING_ID_INVALID		5
#define BCMPCIE_PKT_FLUSH		6
#define BCMPCIE_NO_EVENT_BUF		7
#define BCMPCIE_NO_RX_BUF		8
#define BCMPCIE_NO_IOCTLRESP_BUF	9
#define BCMPCIE_MAX_IOCTLRESP_BUF	10
#define BCMPCIE_MAX_EVENT_BUF		11

/* IOCTL completion response */
typedef struct ioctl_compl_resp_msg {
	/* common message header */
	cmn_msg_hdr_t		cmn_hdr;
	/* completeion message header */
	compl_msg_hdr_t		compl_hdr;
	/* response buffer len where a host buffer is involved */
	uint16			resp_len;
	/* transaction id to pair with a request */
	uint16			trans_id;
	/* cmd id */
	uint32			cmd;
	uint32			resvd;
} ioctl_comp_resp_msg_t;

/* IOCTL request acknowledgement */
typedef struct ioctl_req_ack_msg {
	/* common message header */
	cmn_msg_hdr_t		cmn_hdr;
	/* completeion message header */
	compl_msg_hdr_t 	compl_hdr;
	/* cmd id */
	uint32			cmd;
	uint32			rsvd[2];
} ioctl_req_ack_msg_t;

/* WL event message: send from device to host */
typedef struct wlevent_req_msg {
	/* common message header */
	cmn_msg_hdr_t		cmn_hdr;
	/* completeion message header */
	compl_msg_hdr_t		compl_hdr;
	/* event data len valid with the event buffer */
	uint16			event_data_len;
	/* rsvd	*/
	uint16			rsvd[5];
} wlevent_req_msg_t;

/* dma xfer complete message */
typedef struct pcie_dmaxfer_cmplt {
	/* common message header */
	cmn_msg_hdr_t		cmn_hdr;
	/* completeion message header */
	compl_msg_hdr_t		compl_hdr;
	uint32			rsvd[3];
} pcie_dmaxfer_cmplt_t;

/* general status message */
typedef struct pcie_gen_status {
	/* common message header */
	cmn_msg_hdr_t		cmn_hdr;
	/* completeion message header */
	compl_msg_hdr_t		compl_hdr;
	uint32			rsvd[3];
} pcie_gen_status_t;

/* ring status message */
typedef struct pcie_ring_status {
	/* common message header */
	cmn_msg_hdr_t		cmn_hdr;
	/* completeion message header */
	compl_msg_hdr_t		compl_hdr;
	/* message which firmware couldn't decode */
	uint16			write_idx;
	uint16			rsvd[5];
} pcie_ring_status_t;

typedef struct tx_flowring_create_response {
	cmn_msg_hdr_t		msg;
	compl_msg_hdr_t 	cmplt;
	uint32			rsvd[3];
} tx_flowring_create_response_t;
typedef struct tx_flowring_delete_response {
	cmn_msg_hdr_t		msg;
	compl_msg_hdr_t 	cmplt;
	uint32			rsvd[3];
} tx_flowring_delete_response_t;

typedef struct tx_flowring_flush_response {
	cmn_msg_hdr_t		msg;
	compl_msg_hdr_t 	cmplt;
	uint32			rsvd[3];
} tx_flowring_flush_response_t;

typedef union ctrl_completion_item {
	ioctl_comp_resp_msg_t		ioctl_resp;
	wlevent_req_msg_t		event;
	ioctl_req_ack_msg_t		ioct_ack;
	pcie_dmaxfer_cmplt_t		pcie_xfer_cmplt;
	pcie_gen_status_t		pcie_gen_status;
	pcie_ring_status_t		pcie_ring_status;
	tx_flowring_create_response_t	txfl_create_resp;
	tx_flowring_delete_response_t	txfl_delete_resp;
	tx_flowring_flush_response_t	txfl_flush_resp;
	unsigned char		check[D2HRING_CTRL_CMPLT_ITEMSIZE];
} ctrl_completion_item_t;

/* H2D Rxpost ring work items */
typedef struct host_rxbuf_post {
	/* common message header */
	cmn_msg_hdr_t   cmn_hdr;
	/* provided meta data buffer len */
	uint16		metadata_buf_len;
	/* provided data buffer len to receive data */
	uint16		data_buf_len;
	/* alignment to make the host buffers start on 8 byte boundary */
	uint32		rsvd;
	/* provided meta data buffer */
	addr64_t	metadata_buf_addr;
	/* provided data buffer to receive data */
	addr64_t	data_buf_addr;
} host_rxbuf_post_t;

typedef union rxbuf_submit_item {
	host_rxbuf_post_t	rxpost;
	unsigned char		check[H2DRING_RXPOST_ITEMSIZE];
} rxbuf_submit_item_t;


/* D2H Rxcompletion ring work items */
typedef struct host_rxbuf_cmpl {
	/* common message header */
	cmn_msg_hdr_t	cmn_hdr;
	/* completeion message header */
	compl_msg_hdr_t	compl_hdr;
	/*  filled up meta data len */
	uint16		metadata_len;
	/* filled up buffer len to receive data */
	uint16		data_len;
	/* offset in the host rx buffer where the data starts */
	uint16		data_offset;
	/* offset in the host rx buffer where the data starts */
	uint16		flags;
	/* rx status */
	uint32		rx_status_0;
	uint32		rx_status_1;
	uint32		rsvd;
} host_rxbuf_cmpl_t;

typedef union rxbuf_complete_item {
	host_rxbuf_cmpl_t	rxcmpl;
	unsigned char		check[D2HRING_RXCMPLT_ITEMSIZE];
} rxbuf_complete_item_t;


typedef struct host_txbuf_post {
	/* common message header */
	cmn_msg_hdr_t   cmn_hdr;
	/* eth header */
	uint8		txhdr[ETHER_HDR_LEN];
	/* flags */
	uint8		flags;
	/* number of segments */
	uint8		seg_cnt;

	/* provided meta data buffer for txstatus */
	addr64_t	metadata_buf_addr;
	/* provided data buffer to receive data */
	addr64_t	data_buf_addr;
	/* provided meta data buffer len */
	uint16		metadata_buf_len;
	/* provided data buffer len to receive data */
	uint16		data_len;
	uint32		rsvd;
} host_txbuf_post_t;

#define BCMPCIE_TXPOST_FLAGS_FRAME_802_3	0x01
#define BCMPCIE_TXPOST_FLAGS_FRAME_802_11	0x02
#define BCMPCIE_TXPOST_FLAGS_PRIO_SHIFT		5
#define BCMPCIE_TXPOST_FLAGS_PRIO_MASK		(7 << BCMPCIE_TXPOST_FLAGS_PRIO_SHIFT)

/* H2D Txpost ring work items */
typedef union txbuf_submit_item {
	host_txbuf_post_t	txpost;
	unsigned char		check[H2DRING_TXPOST_ITEMSIZE];
} txbuf_submit_item_t;

/* D2H Txcompletion ring work items */
typedef struct host_txbuf_cmpl {
	/* common message header */
	cmn_msg_hdr_t	cmn_hdr;
	/* completeion message header */
	compl_msg_hdr_t	compl_hdr;
	/* provided meta data len */
	uint16	metadata_len;
	/* WLAN side txstatus */
	uint16	tx_status;
} host_txbuf_cmpl_t;

typedef union txbuf_complete_item {
	host_txbuf_cmpl_t	txcmpl;
	unsigned char		check[D2HRING_TXCMPLT_ITEMSIZE];
} txbuf_complete_item_t;


#define BCMPCIE_D2H_METADATA_HDRLEN	4



/* ret buf struct */
typedef struct ret_buf_ptr {
	uint32 low_addr;
	uint32 high_addr;
} ret_buf_t;

#ifdef PCIE_API_REV1
/* ioctl specific hdr */
typedef struct ioctl_hdr {
	uint16 		cmd;
	uint16		retbuf_len;
	uint32		cmd_id;
} ioctl_hdr_t;
typedef struct ioctlptr_hdr {
	uint16 		cmd;
	uint16		retbuf_len;
	uint16 		buflen;
	uint16		rsvd;
	uint32		cmd_id;
} ioctlptr_hdr_t;
#else /* PCIE_API_REV1 */
typedef struct ioctl_req_hdr {
	uint32		pkt_id; /* Packet ID */
	uint32 		cmd; /* IOCTL ID */
	uint16		retbuf_len;
	uint16 		buflen;
	uint16		xt_id; /* transaction ID */
	uint16		rsvd[1];
} ioctl_req_hdr_t;
#endif /* PCIE_API_REV1 */


/* Complete msgbuf hdr for ioctl from host to dongle */
typedef struct ioct_reqst_hdr {
	cmn_msg_hdr_t msg;
#ifdef PCIE_API_REV1
	ioctl_hdr_t ioct_hdr;
#else
	ioctl_req_hdr_t ioct_hdr;
#endif
	ret_buf_t ret_buf;
} ioct_reqst_hdr_t;
typedef struct ioctptr_reqst_hdr {
	cmn_msg_hdr_t msg;
#ifdef PCIE_API_REV1
	ioctlptr_hdr_t ioct_hdr;
#else
	ioctl_req_hdr_t ioct_hdr;
#endif
	ret_buf_t ret_buf;
	ret_buf_t ioct_buf;
} ioctptr_reqst_hdr_t;

/* ioctl response header */
typedef struct ioct_resp_hdr {
	cmn_msg_hdr_t   msg;
#ifdef PCIE_API_REV1
	uint32	cmd_id;
#else
	uint32	pkt_id;
#endif
	uint32	status;
	uint32	ret_len;
	uint32  inline_data;
#ifdef PCIE_API_REV1
#else
	uint16	xt_id;	/* transaction ID */
	uint16	rsvd[1];
#endif
} ioct_resp_hdr_t;

/* ioct resp header used in dongle */
/* ret buf hdr will be stripped off inside dongle itself */
typedef struct msgbuf_ioctl_resp {
	ioct_resp_hdr_t	ioct_hdr;
	ret_buf_t	ret_buf;	/* ret buf pointers */
} msgbuf_ioct_resp_t;

/* WL evet hdr info */
typedef struct wl_event_hdr {
	cmn_msg_hdr_t   msg;
	uint16 event;
	uint8 flags;
	uint8 rsvd;
	uint16 retbuf_len;
	uint16 rsvd1;
	uint32 rxbufid;
} wl_event_hdr_t;

#define TXDESCR_FLOWID_PCIELPBK_1	0xFF
#define TXDESCR_FLOWID_PCIELPBK_2	0xFE

typedef struct txbatch_lenptr_tup {
	uint32 pktid;
	uint16 pktlen;
	uint16 rsvd;
	ret_buf_t	ret_buf;	/* ret buf pointers */
} txbatch_lenptr_tup_t;

typedef struct txbatch_cmn_msghdr {
	cmn_msg_hdr_t   msg;
	uint8 priority;
	uint8 hdrlen;
	uint8 pktcnt;
	uint8 flowid;
	uint8 txhdr[ETHER_HDR_LEN];
	uint16 rsvd;
} txbatch_cmn_msghdr_t;

typedef struct txbatch_msghdr {
	txbatch_cmn_msghdr_t txcmn;
	txbatch_lenptr_tup_t tx_tup[0]; /* Based on packet count */
} txbatch_msghdr_t;

/* TX desc posting header */
typedef struct tx_lenptr_tup {
	uint16 pktlen;
	uint16 rsvd;
	ret_buf_t	ret_buf;	/* ret buf pointers */
} tx_lenptr_tup_t;

typedef struct txdescr_cmn_msghdr {
	cmn_msg_hdr_t   msg;
	uint8 priority;
	uint8 hdrlen;
	uint8 descrcnt;
	uint8 flowid;
	uint32 pktid;
} txdescr_cmn_msghdr_t;

typedef struct txdescr_msghdr {
	txdescr_cmn_msghdr_t txcmn;
	uint8 txhdr[ETHER_HDR_LEN];
	uint16 rsvd;
	tx_lenptr_tup_t tx_tup[0]; /* Based on descriptor count */
} txdescr_msghdr_t;

/* Tx status header info */
typedef struct txstatus_hdr {
	cmn_msg_hdr_t   msg;
	uint32 pktid;
} txstatus_hdr_t;
/* RX bufid-len-ptr tuple */
typedef struct rx_lenptr_tup {
	uint32 rxbufid;
	uint16 len;
	uint16 rsvd2;
	ret_buf_t	ret_buf;	/* ret buf pointers */
} rx_lenptr_tup_t;
/* Rx descr Post hdr info */
typedef struct rxdesc_msghdr {
	cmn_msg_hdr_t   msg;
	uint16 rsvd0;
	uint8 rsvd1;
	uint8 descnt;
	rx_lenptr_tup_t rx_tup[0];
} rxdesc_msghdr_t;

/* RX complete tuples */
typedef struct rxcmplt_tup {
	uint16 retbuf_len;
	uint16 data_offset;
	uint32 rxstatus0;
	uint32 rxstatus1;
	uint32 rxbufid;
} rxcmplt_tup_t;
/* RX complete messge hdr */
typedef struct rxcmplt_hdr {
	cmn_msg_hdr_t   msg;
	uint16 rsvd0;
	uint16 rxcmpltcnt;
	rxcmplt_tup_t rx_tup[0];
} rxcmplt_hdr_t;
typedef struct hostevent_hdr {
	cmn_msg_hdr_t   msg;
	uint32 evnt_pyld;
} hostevent_hdr_t;

typedef struct dma_xfer_params {
	uint32 src_physaddr_hi;
	uint32 src_physaddr_lo;
	uint32 dest_physaddr_hi;
	uint32 dest_physaddr_lo;
	uint32 len;
	uint32 srcdelay;
	uint32 destdelay;
} dma_xfer_params_t;

enum {
	HOST_EVENT_CONS_CMD = 1
};

/* defines for flags */
#define MSGBUF_IOC_ACTION_MASK 0x1

#define GET_MSG_LEN_FRM_TYPE(msgtype, msglen) switch ((msgtype)) {	\
	case MSG_TYPE_TX_POST:	\
		msglen = H2DRING_TXPOST_ITEMSIZE;	\
		break;	\
	case MSG_TYPE_RXBUF_POST:	\
	case MSG_TYPE_EVENT_BUF_POST:	\
	case MSG_TYPE_IOCTLRESP_BUF_POST:	\
		msglen = H2DRING_RXPOST_ITEMSIZE;	\
		break;	\
	case MSG_TYPE_IOCTL_REQ:	\
		msglen = H2DRING_CTRL_SUB_ITEMSIZE;	\
		break;	\
	case MSG_TYPE_TX_STATUS:	\
		msglen = D2HRING_TXCMPLT_ITEMSIZE;	\
		break;	\
	case MSG_TYPE_RX_CMPLT:	\
		msglen = D2HRING_RXCMPLT_ITEMSIZE;	\
		break;	\
	case MSG_TYPE_IOCTL_CMPLT:\
	case MSG_TYPE_WL_EVENT:	\
	case MSG_TYPE_IOCTLPTR_REQ_ACK:	\
	case MSG_TYPE_FLOW_RING_CREATE_CMPLT: \
	case MSG_TYPE_FLOW_RING_DELETE_CMPLT: \
	case MSG_TYPE_FLOW_RING_FLUSH_CMPLT: \
	case MSG_TYPE_GEN_STATUS:	\
	case MSG_TYPE_RING_STATUS:	\
	case MSG_TYPE_LPBK_DMAXFER_CMPLT:	\
		msglen = D2HRING_CTRL_CMPLT_ITEMSIZE;	\
		break;	\
	default : \
		printf("ERROR:%s:%d: Invalid msg type %d\n", __FUNCTION__, __LINE__, msgtype); \
		msglen = 0;	\
		break;	\
}
#endif /* _bcmmsgbuf_h_ */
