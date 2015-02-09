/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005, 2006 Cisco Systems.  All rights reserved.
 * Copyright (c) 2005 PathScale, Inc.  All rights reserved.
 * Copyright (c) 2006 Mellanox Technologies.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef IB_USER_VERBS_H
#define IB_USER_VERBS_H

#include <linux/types.h>

/*
 * Increment this value if any changes that break userspace ABI
 * compatibility are made.
 */
#define IB_USER_VERBS_ABI_VERSION	6

enum {
	IB_USER_VERBS_CMD_GET_CONTEXT,
	IB_USER_VERBS_CMD_QUERY_DEVICE,
	IB_USER_VERBS_CMD_QUERY_PORT,
	IB_USER_VERBS_CMD_ALLOC_PD,
	IB_USER_VERBS_CMD_DEALLOC_PD,
	IB_USER_VERBS_CMD_CREATE_AH,
	IB_USER_VERBS_CMD_MODIFY_AH,
	IB_USER_VERBS_CMD_QUERY_AH,
	IB_USER_VERBS_CMD_DESTROY_AH,
	IB_USER_VERBS_CMD_REG_MR,
	IB_USER_VERBS_CMD_REG_SMR,
	IB_USER_VERBS_CMD_REREG_MR,
	IB_USER_VERBS_CMD_QUERY_MR,
	IB_USER_VERBS_CMD_DEREG_MR,
	IB_USER_VERBS_CMD_ALLOC_MW,
	IB_USER_VERBS_CMD_BIND_MW,
	IB_USER_VERBS_CMD_DEALLOC_MW,
	IB_USER_VERBS_CMD_CREATE_COMP_CHANNEL,
	IB_USER_VERBS_CMD_CREATE_CQ,
	IB_USER_VERBS_CMD_RESIZE_CQ,
	IB_USER_VERBS_CMD_DESTROY_CQ,
	IB_USER_VERBS_CMD_POLL_CQ,
	IB_USER_VERBS_CMD_PEEK_CQ,
	IB_USER_VERBS_CMD_REQ_NOTIFY_CQ,
	IB_USER_VERBS_CMD_CREATE_QP,
	IB_USER_VERBS_CMD_QUERY_QP,
	IB_USER_VERBS_CMD_MODIFY_QP,
	IB_USER_VERBS_CMD_DESTROY_QP,
	IB_USER_VERBS_CMD_POST_SEND,
	IB_USER_VERBS_CMD_POST_RECV,
	IB_USER_VERBS_CMD_ATTACH_MCAST,
	IB_USER_VERBS_CMD_DETACH_MCAST,
	IB_USER_VERBS_CMD_CREATE_SRQ,
	IB_USER_VERBS_CMD_MODIFY_SRQ,
	IB_USER_VERBS_CMD_QUERY_SRQ,
	IB_USER_VERBS_CMD_DESTROY_SRQ,
	IB_USER_VERBS_CMD_POST_SRQ_RECV
};

/*
 * Make sure that all structs defined in this file remain laid out so
 * that they pack the same way on 32-bit and 64-bit architectures (to
 * avoid incompatibility between 32-bit userspace and 64-bit kernels).
 * Specifically:
 *  - Do not use pointer types -- pass pointers in __u64 instead.
 *  - Make sure that any structure larger than 4 bytes is padded to a
 *    multiple of 8 bytes.  Otherwise the structure size will be
 *    different between 32-bit and 64-bit architectures.
 */

struct ib_uverbs_async_event_desc {
	__u64 element;
	__u32 event_type;	/* enum ib_event_type */
	__u32 reserved;
};

struct ib_uverbs_comp_event_desc {
	__u64 cq_handle;
};

/*
 * All commands from userspace should start with a __u32 command field
 * followed by __u16 in_words and out_words fields (which give the
 * length of the command block and response buffer if any in 32-bit
 * words).  The kernel driver will read these fields first and read
 * the rest of the command struct based on these value.
 */

struct ib_uverbs_cmd_hdr {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
};

struct ib_uverbs_get_context {
	__u64 response;
	__u64 driver_data[0];
};

struct ib_uverbs_get_context_resp {
	__u32 async_fd;
	__u32 num_comp_vectors;
};

struct ib_uverbs_query_device {
	__u64 response;
	__u64 driver_data[0];
};

struct ib_uverbs_query_device_resp {
	__u64 fw_ver;
	__be64 node_guid;
	__be64 sys_image_guid;
	__u64 max_mr_size;
	__u64 page_size_cap;
	__u32 vendor_id;
	__u32 vendor_part_id;
	__u32 hw_ver;
	__u32 max_qp;
	__u32 max_qp_wr;
	__u32 device_cap_flags;
	__u32 max_sge;
	__u32 max_sge_rd;
	__u32 max_cq;
	__u32 max_cqe;
	__u32 max_mr;
	__u32 max_pd;
	__u32 max_qp_rd_atom;
	__u32 max_ee_rd_atom;
	__u32 max_res_rd_atom;
	__u32 max_qp_init_rd_atom;
	__u32 max_ee_init_rd_atom;
	__u32 atomic_cap;
	__u32 max_ee;
	__u32 max_rdd;
	__u32 max_mw;
	__u32 max_raw_ipv6_qp;
	__u32 max_raw_ethy_qp;
	__u32 max_mcast_grp;
	__u32 max_mcast_qp_attach;
	__u32 max_total_mcast_qp_attach;
	__u32 max_ah;
	__u32 max_fmr;
	__u32 max_map_per_fmr;
	__u32 max_srq;
	__u32 max_srq_wr;
	__u32 max_srq_sge;
	__u16 max_pkeys;
	__u8  local_ca_ack_delay;
	__u8  phys_port_cnt;
	__u8  reserved[4];
};

struct ib_uverbs_query_port {
	__u64 response;
	__u8  port_num;
	__u8  reserved[7];
	__u64 driver_data[0];
};

struct ib_uverbs_query_port_resp {
	__u32 port_cap_flags;
	__u32 max_msg_sz;
	__u32 bad_pkey_cntr;
	__u32 qkey_viol_cntr;
	__u32 gid_tbl_len;
	__u16 pkey_tbl_len;
	__u16 lid;
	__u16 sm_lid;
	__u8  state;
	__u8  max_mtu;
	__u8  active_mtu;
	__u8  lmc;
	__u8  max_vl_num;
	__u8  sm_sl;
	__u8  subnet_timeout;
	__u8  init_type_reply;
	__u8  active_width;
	__u8  active_speed;
	__u8  phys_state;
	__u8  reserved[3];
};

struct ib_uverbs_alloc_pd {
	__u64 response;
	__u64 driver_data[0];
};

struct ib_uverbs_alloc_pd_resp {
	__u32 pd_handle;
};

struct ib_uverbs_dealloc_pd {
	__u32 pd_handle;
};

struct ib_uverbs_reg_mr {
	__u64 response;
	__u64 start;
	__u64 length;
	__u64 hca_va;
	__u32 pd_handle;
	__u32 access_flags;
	__u64 driver_data[0];
};

struct ib_uverbs_reg_mr_resp {
	__u32 mr_handle;
	__u32 lkey;
	__u32 rkey;
};

struct ib_uverbs_dereg_mr {
	__u32 mr_handle;
};

struct ib_uverbs_create_comp_channel {
	__u64 response;
};

struct ib_uverbs_create_comp_channel_resp {
	__u32 fd;
};

struct ib_uverbs_create_cq {
	__u64 response;
	__u64 user_handle;
	__u32 cqe;
	__u32 comp_vector;
	__s32 comp_channel;
	__u32 reserved;
	__u64 driver_data[0];
};

struct ib_uverbs_create_cq_resp {
	__u32 cq_handle;
	__u32 cqe;
};

struct ib_uverbs_resize_cq {
	__u64 response;
	__u32 cq_handle;
	__u32 cqe;
	__u64 driver_data[0];
};

struct ib_uverbs_resize_cq_resp {
	__u32 cqe;
	__u32 reserved;
	__u64 driver_data[0];
};

struct ib_uverbs_poll_cq {
	__u64 response;
	__u32 cq_handle;
	__u32 ne;
};

struct ib_uverbs_wc {
	__u64 wr_id;
	__u32 status;
	__u32 opcode;
	__u32 vendor_err;
	__u32 byte_len;
	union {
		__u32 imm_data;
		__u32 invalidate_rkey;
	} ex;
	__u32 qp_num;
	__u32 src_qp;
	__u32 wc_flags;
	__u16 pkey_index;
	__u16 slid;
	__u8 sl;
	__u8 dlid_path_bits;
	__u8 port_num;
	__u8 reserved;
};

struct ib_uverbs_poll_cq_resp {
	__u32 count;
	__u32 reserved;
	struct ib_uverbs_wc wc[0];
};

struct ib_uverbs_req_notify_cq {
	__u32 cq_handle;
	__u32 solicited_only;
};

struct ib_uverbs_destroy_cq {
	__u64 response;
	__u32 cq_handle;
	__u32 reserved;
};

struct ib_uverbs_destroy_cq_resp {
	__u32 comp_events_reported;
	__u32 async_events_reported;
};

struct ib_uverbs_global_route {
	__u8  dgid[16];
	__u32 flow_label;
	__u8  sgid_index;
	__u8  hop_limit;
	__u8  traffic_class;
	__u8  reserved;
};

struct ib_uverbs_ah_attr {
	struct ib_uverbs_global_route grh;
	__u16 dlid;
	__u8  sl;
	__u8  src_path_bits;
	__u8  static_rate;
	__u8  is_global;
	__u8  port_num;
	__u8  reserved;
};

struct ib_uverbs_qp_attr {
	__u32	qp_attr_mask;
	__u32	qp_state;
	__u32	cur_qp_state;
	__u32	path_mtu;
	__u32	path_mig_state;
	__u32	qkey;
	__u32	rq_psn;
	__u32	sq_psn;
	__u32	dest_qp_num;
	__u32	qp_access_flags;

	struct ib_uverbs_ah_attr ah_attr;
	struct ib_uverbs_ah_attr alt_ah_attr;

	/* ib_qp_cap */
	__u32	max_send_wr;
	__u32	max_recv_wr;
	__u32	max_send_sge;
	__u32	max_recv_sge;
	__u32	max_inline_data;

	__u16	pkey_index;
	__u16	alt_pkey_index;
	__u8	en_sqd_async_notify;
	__u8	sq_draining;
	__u8	max_rd_atomic;
	__u8	max_dest_rd_atomic;
	__u8	min_rnr_timer;
	__u8	port_num;
	__u8	timeout;
	__u8	retry_cnt;
	__u8	rnr_retry;
	__u8	alt_port_num;
	__u8	alt_timeout;
	__u8	reserved[5];
};

struct ib_uverbs_create_qp {
	__u64 response;
	__u64 user_handle;
	__u32 pd_handle;
	__u32 send_cq_handle;
	__u32 recv_cq_handle;
	__u32 srq_handle;
	__u32 max_send_wr;
	__u32 max_recv_wr;
	__u32 max_send_sge;
	__u32 max_recv_sge;
	__u32 max_inline_data;
	__u8  sq_sig_all;
	__u8  qp_type;
	__u8  is_srq;
	__u8  reserved;
	__u64 driver_data[0];
};

struct ib_uverbs_create_qp_resp {
	__u32 qp_handle;
	__u32 qpn;
	__u32 max_send_wr;
	__u32 max_recv_wr;
	__u32 max_send_sge;
	__u32 max_recv_sge;
	__u32 max_inline_data;
	__u32 reserved;
};

/*
 * This struct needs to remain a multiple of 8 bytes to keep the
 * alignment of the modify QP parameters.
 */
struct ib_uverbs_qp_dest {
	__u8  dgid[16];
	__u32 flow_label;
	__u16 dlid;
	__u16 reserved;
	__u8  sgid_index;
	__u8  hop_limit;
	__u8  traffic_class;
	__u8  sl;
	__u8  src_path_bits;
	__u8  static_rate;
	__u8  is_global;
	__u8  port_num;
};

struct ib_uverbs_query_qp {
	__u64 response;
	__u32 qp_handle;
	__u32 attr_mask;
	__u64 driver_data[0];
};

struct ib_uverbs_query_qp_resp {
	struct ib_uverbs_qp_dest dest;
	struct ib_uverbs_qp_dest alt_dest;
	__u32 max_send_wr;
	__u32 max_recv_wr;
	__u32 max_send_sge;
	__u32 max_recv_sge;
	__u32 max_inline_data;
	__u32 qkey;
	__u32 rq_psn;
	__u32 sq_psn;
	__u32 dest_qp_num;
	__u32 qp_access_flags;
	__u16 pkey_index;
	__u16 alt_pkey_index;
	__u8  qp_state;
	__u8  cur_qp_state;
	__u8  path_mtu;
	__u8  path_mig_state;
	__u8  sq_draining;
	__u8  max_rd_atomic;
	__u8  max_dest_rd_atomic;
	__u8  min_rnr_timer;
	__u8  port_num;
	__u8  timeout;
	__u8  retry_cnt;
	__u8  rnr_retry;
	__u8  alt_port_num;
	__u8  alt_timeout;
	__u8  sq_sig_all;
	__u8  reserved[5];
	__u64 driver_data[0];
};

struct ib_uverbs_modify_qp {
	struct ib_uverbs_qp_dest dest;
	struct ib_uverbs_qp_dest alt_dest;
	__u32 qp_handle;
	__u32 attr_mask;
	__u32 qkey;
	__u32 rq_psn;
	__u32 sq_psn;
	__u32 dest_qp_num;
	__u32 qp_access_flags;
	__u16 pkey_index;
	__u16 alt_pkey_index;
	__u8  qp_state;
	__u8  cur_qp_state;
	__u8  path_mtu;
	__u8  path_mig_state;
	__u8  en_sqd_async_notify;
	__u8  max_rd_atomic;
	__u8  max_dest_rd_atomic;
	__u8  min_rnr_timer;
	__u8  port_num;
	__u8  timeout;
	__u8  retry_cnt;
	__u8  rnr_retry;
	__u8  alt_port_num;
	__u8  alt_timeout;
	__u8  reserved[2];
	__u64 driver_data[0];
};

struct ib_uverbs_modify_qp_resp {
};

struct ib_uverbs_destroy_qp {
	__u64 response;
	__u32 qp_handle;
	__u32 reserved;
};

struct ib_uverbs_destroy_qp_resp {
	__u32 events_reported;
};

/*
 * The ib_uverbs_sge structure isn't used anywhere, since we assume
 * the ib_sge structure is packed the same way on 32-bit and 64-bit
 * architectures in both kernel and user space.  It's just here to
 * document the ABI.
 */
struct ib_uverbs_sge {
	__u64 addr;
	__u32 length;
	__u32 lkey;
};

struct ib_uverbs_send_wr {
	__u64 wr_id;
	__u32 num_sge;
	__u32 opcode;
	__u32 send_flags;
	union {
		__u32 imm_data;
		__u32 invalidate_rkey;
	} ex;
	union {
		struct {
			__u64 remote_addr;
			__u32 rkey;
			__u32 reserved;
		} rdma;
		struct {
			__u64 remote_addr;
			__u64 compare_add;
			__u64 swap;
			__u32 rkey;
			__u32 reserved;
		} atomic;
		struct {
			__u32 ah;
			__u32 remote_qpn;
			__u32 remote_qkey;
			__u32 reserved;
		} ud;
	} wr;
};

struct ib_uverbs_post_send {
	__u64 response;
	__u32 qp_handle;
	__u32 wr_count;
	__u32 sge_count;
	__u32 wqe_size;
	struct ib_uverbs_send_wr send_wr[0];
};

struct ib_uverbs_post_send_resp {
	__u32 bad_wr;
};

struct ib_uverbs_recv_wr {
	__u64 wr_id;
	__u32 num_sge;
	__u32 reserved;
};

struct ib_uverbs_post_recv {
	__u64 response;
	__u32 qp_handle;
	__u32 wr_count;
	__u32 sge_count;
	__u32 wqe_size;
	struct ib_uverbs_recv_wr recv_wr[0];
};

struct ib_uverbs_post_recv_resp {
	__u32 bad_wr;
};

struct ib_uverbs_post_srq_recv {
	__u64 response;
	__u32 srq_handle;
	__u32 wr_count;
	__u32 sge_count;
	__u32 wqe_size;
	struct ib_uverbs_recv_wr recv[0];
};

struct ib_uverbs_post_srq_recv_resp {
	__u32 bad_wr;
};

struct ib_uverbs_create_ah {
	__u64 response;
	__u64 user_handle;
	__u32 pd_handle;
	__u32 reserved;
	struct ib_uverbs_ah_attr attr;
};

struct ib_uverbs_create_ah_resp {
	__u32 ah_handle;
};

struct ib_uverbs_destroy_ah {
	__u32 ah_handle;
};

struct ib_uverbs_attach_mcast {
	__u8  gid[16];
	__u32 qp_handle;
	__u16 mlid;
	__u16 reserved;
	__u64 driver_data[0];
};

struct ib_uverbs_detach_mcast {
	__u8  gid[16];
	__u32 qp_handle;
	__u16 mlid;
	__u16 reserved;
	__u64 driver_data[0];
};

struct ib_uverbs_create_srq {
	__u64 response;
	__u64 user_handle;
	__u32 pd_handle;
	__u32 max_wr;
	__u32 max_sge;
	__u32 srq_limit;
	__u64 driver_data[0];
};

struct ib_uverbs_create_srq_resp {
	__u32 srq_handle;
	__u32 max_wr;
	__u32 max_sge;
	__u32 reserved;
};

struct ib_uverbs_modify_srq {
	__u32 srq_handle;
	__u32 attr_mask;
	__u32 max_wr;
	__u32 srq_limit;
	__u64 driver_data[0];
};

struct ib_uverbs_query_srq {
	__u64 response;
	__u32 srq_handle;
	__u32 reserved;
	__u64 driver_data[0];
};

struct ib_uverbs_query_srq_resp {
	__u32 max_wr;
	__u32 max_sge;
	__u32 srq_limit;
	__u32 reserved;
};

struct ib_uverbs_destroy_srq {
	__u64 response;
	__u32 srq_handle;
	__u32 reserved;
};

struct ib_uverbs_destroy_srq_resp {
	__u32 events_reported;
};

#endif /* IB_USER_VERBS_H */
