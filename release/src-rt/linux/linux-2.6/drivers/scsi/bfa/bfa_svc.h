/*
 * Copyright (c) 2005-2010 Brocade Communications Systems, Inc.
 * All rights reserved
 * www.brocade.com
 *
 * Linux driver for Brocade Fibre Channel Host Bus Adapter.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License (GPL) Version 2 as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef __BFA_SVC_H__
#define __BFA_SVC_H__

#include "bfa_cs.h"
#include "bfi_ms.h"


/*
 * Scatter-gather DMA related defines
 */
#define BFA_SGPG_MIN	(16)

/*
 * Alignment macro for SG page allocation
 */
#define BFA_SGPG_ROUNDUP(_l) (((_l) + (sizeof(struct bfi_sgpg_s) - 1))	\
			      & ~(sizeof(struct bfi_sgpg_s) - 1))

struct bfa_sgpg_wqe_s {
	struct list_head qe;	/*  queue sg page element	*/
	int	nsgpg;		/*  pages to be allocated	*/
	int	nsgpg_total;	/*  total pages required	*/
	void	(*cbfn) (void *cbarg);	/*  callback function	*/
	void	*cbarg;		/*  callback arg		*/
	struct list_head sgpg_q;	/*  queue of alloced sgpgs	*/
};

struct bfa_sgpg_s {
	struct list_head  qe;	/*  queue sg page element	*/
	struct bfi_sgpg_s *sgpg;	/*  va of SG page		*/
	union bfi_addr_u sgpg_pa;	/*  pa of SG page		*/
};

/*
 * Given number of SG elements, BFA_SGPG_NPAGE() returns the number of
 * SG pages required.
 */
#define BFA_SGPG_NPAGE(_nsges)  (((_nsges) / BFI_SGPG_DATA_SGES) + 1)

struct bfa_sgpg_mod_s {
	struct bfa_s *bfa;
	int		num_sgpgs;	/*  number of SG pages		*/
	int		free_sgpgs;	/*  number of free SG pages	*/
	struct bfa_sgpg_s	*hsgpg_arr;	/*  BFA SG page array	*/
	struct bfi_sgpg_s *sgpg_arr;	/*  actual SG page array	*/
	u64	sgpg_arr_pa;	/*  SG page array DMA addr	*/
	struct list_head	sgpg_q;		/*  queue of free SG pages */
	struct list_head	sgpg_wait_q;	/*  wait queue for SG pages */
};
#define BFA_SGPG_MOD(__bfa)	(&(__bfa)->modules.sgpg_mod)

bfa_status_t bfa_sgpg_malloc(struct bfa_s *bfa, struct list_head *sgpg_q,
			     int nsgpgs);
void bfa_sgpg_mfree(struct bfa_s *bfa, struct list_head *sgpg_q, int nsgpgs);
void bfa_sgpg_winit(struct bfa_sgpg_wqe_s *wqe,
		    void (*cbfn) (void *cbarg), void *cbarg);
void bfa_sgpg_wait(struct bfa_s *bfa, struct bfa_sgpg_wqe_s *wqe, int nsgpgs);
void bfa_sgpg_wcancel(struct bfa_s *bfa, struct bfa_sgpg_wqe_s *wqe);


/*
 * FCXP related defines
 */
#define BFA_FCXP_MIN		(1)
#define BFA_FCXP_MAX_IBUF_SZ	(2 * 1024 + 256)
#define BFA_FCXP_MAX_LBUF_SZ	(4 * 1024 + 256)

struct bfa_fcxp_mod_s {
	struct bfa_s      *bfa;		/* backpointer to BFA */
	struct bfa_fcxp_s *fcxp_list;	/* array of FCXPs */
	u16	num_fcxps;	/* max num FCXP requests */
	struct list_head  fcxp_free_q;	/* free FCXPs */
	struct list_head  fcxp_active_q;	/* active FCXPs */
	void		*req_pld_list_kva;	/* list of FCXP req pld */
	u64	req_pld_list_pa;	/* list of FCXP req pld */
	void		*rsp_pld_list_kva;	/* list of FCXP resp pld */
	u64	rsp_pld_list_pa;	/* list of FCXP resp pld */
	struct list_head  wait_q;		/* wait queue for free fcxp */
	u32	req_pld_sz;
	u32	rsp_pld_sz;
};

#define BFA_FCXP_MOD(__bfa)		(&(__bfa)->modules.fcxp_mod)
#define BFA_FCXP_FROM_TAG(__mod, __tag)	(&(__mod)->fcxp_list[__tag])

typedef void    (*fcxp_send_cb_t) (struct bfa_s *ioc, struct bfa_fcxp_s *fcxp,
				   void *cb_arg, bfa_status_t req_status,
				   u32 rsp_len, u32 resid_len,
				   struct fchs_s *rsp_fchs);

typedef u64 (*bfa_fcxp_get_sgaddr_t) (void *bfad_fcxp, int sgeid);
typedef u32 (*bfa_fcxp_get_sglen_t) (void *bfad_fcxp, int sgeid);
typedef void (*bfa_cb_fcxp_send_t) (void *bfad_fcxp, struct bfa_fcxp_s *fcxp,
				    void *cbarg, enum bfa_status req_status,
				    u32 rsp_len, u32 resid_len,
				    struct fchs_s *rsp_fchs);
typedef void (*bfa_fcxp_alloc_cbfn_t) (void *cbarg, struct bfa_fcxp_s *fcxp);



/*
 * Information needed for a FCXP request
 */
struct bfa_fcxp_req_info_s {
	struct bfa_rport_s *bfa_rport;
					/* Pointer to the bfa rport that was
					 * returned from bfa_rport_create().
					 * This could be left NULL for WKA or
					 * for FCXP interactions before the
					 * rport nexus is established
					 */
	struct fchs_s	fchs;	/*  request FC header structure */
	u8		cts;	/*  continuous sequence */
	u8		class;	/*  FC class for the request/response */
	u16	max_frmsz;	/*  max send frame size */
	u16	vf_id;	/*  vsan tag if applicable */
	u8		lp_tag;	/*  lport tag */
	u32	req_tot_len;	/*  request payload total length */
};

struct bfa_fcxp_rsp_info_s {
	struct fchs_s	rsp_fchs;
				/* Response frame's FC header will
				 * be sent back in this field */
	u8		rsp_timeout;
				/* timeout in seconds, 0-no response */
	u8		rsvd2[3];
	u32	rsp_maxlen;	/*  max response length expected */
};

struct bfa_fcxp_s {
	struct list_head	qe;		/*  fcxp queue element */
	bfa_sm_t	sm;		/*  state machine */
	void		*caller;	/*  driver or fcs */
	struct bfa_fcxp_mod_s *fcxp_mod;
	/*  back pointer to fcxp mod */
	u16	fcxp_tag;	/*  internal tag */
	struct bfa_fcxp_req_info_s req_info;
	/*  request info */
	struct bfa_fcxp_rsp_info_s rsp_info;
	/*  response info */
	u8	use_ireqbuf;	/*  use internal req buf */
	u8		use_irspbuf;	/*  use internal rsp buf */
	u32	nreq_sgles;	/*  num request SGLEs */
	u32	nrsp_sgles;	/*  num response SGLEs */
	struct list_head req_sgpg_q;	/*  SG pages for request buf */
	struct list_head req_sgpg_wqe;	/*  wait queue for req SG page */
	struct list_head rsp_sgpg_q;	/*  SG pages for response buf */
	struct list_head rsp_sgpg_wqe;	/*  wait queue for rsp SG page */

	bfa_fcxp_get_sgaddr_t req_sga_cbfn;
	/*  SG elem addr user function */
	bfa_fcxp_get_sglen_t req_sglen_cbfn;
	/*  SG elem len user function */
	bfa_fcxp_get_sgaddr_t rsp_sga_cbfn;
	/*  SG elem addr user function */
	bfa_fcxp_get_sglen_t rsp_sglen_cbfn;
	/*  SG elem len user function */
	bfa_cb_fcxp_send_t send_cbfn;   /*  send completion callback */
	void		*send_cbarg;	/*  callback arg */
	struct bfa_sge_s   req_sge[BFA_FCXP_MAX_SGES];
	/*  req SG elems */
	struct bfa_sge_s   rsp_sge[BFA_FCXP_MAX_SGES];
	/*  rsp SG elems */
	u8		rsp_status;	/*  comp: rsp status */
	u32	rsp_len;	/*  comp: actual response len */
	u32	residue_len;	/*  comp: residual rsp length */
	struct fchs_s	rsp_fchs;	/*  comp: response fchs */
	struct bfa_cb_qe_s    hcb_qe;	/*  comp: callback qelem */
	struct bfa_reqq_wait_s	reqq_wqe;
	bfa_boolean_t	reqq_waiting;
};

struct bfa_fcxp_wqe_s {
	struct list_head		qe;
	bfa_fcxp_alloc_cbfn_t	alloc_cbfn;
	void		*alloc_cbarg;
	void		*caller;
	struct bfa_s	*bfa;
	int		nreq_sgles;
	int		nrsp_sgles;
	bfa_fcxp_get_sgaddr_t	req_sga_cbfn;
	bfa_fcxp_get_sglen_t	req_sglen_cbfn;
	bfa_fcxp_get_sgaddr_t	rsp_sga_cbfn;
	bfa_fcxp_get_sglen_t	rsp_sglen_cbfn;
};

#define BFA_FCXP_REQ_PLD(_fcxp)		(bfa_fcxp_get_reqbuf(_fcxp))
#define BFA_FCXP_RSP_FCHS(_fcxp)	(&((_fcxp)->rsp_info.fchs))
#define BFA_FCXP_RSP_PLD(_fcxp)		(bfa_fcxp_get_rspbuf(_fcxp))

#define BFA_FCXP_REQ_PLD_PA(_fcxp)				\
	((_fcxp)->fcxp_mod->req_pld_list_pa +			\
	 ((_fcxp)->fcxp_mod->req_pld_sz  * (_fcxp)->fcxp_tag))

#define BFA_FCXP_RSP_PLD_PA(_fcxp)				\
	((_fcxp)->fcxp_mod->rsp_pld_list_pa +			\
	 ((_fcxp)->fcxp_mod->rsp_pld_sz * (_fcxp)->fcxp_tag))

void	bfa_fcxp_isr(struct bfa_s *bfa, struct bfi_msg_s *msg);


/*
 * RPORT related defines
 */
enum bfa_rport_event {
	BFA_RPORT_SM_CREATE	= 1,	/*  rport create event          */
	BFA_RPORT_SM_DELETE	= 2,	/*  deleting an existing rport  */
	BFA_RPORT_SM_ONLINE	= 3,	/*  rport is online             */
	BFA_RPORT_SM_OFFLINE	= 4,	/*  rport is offline            */
	BFA_RPORT_SM_FWRSP	= 5,	/*  firmware response           */
	BFA_RPORT_SM_HWFAIL	= 6,	/*  IOC h/w failure             */
	BFA_RPORT_SM_QOS_SCN	= 7,	/*  QoS SCN from firmware       */
	BFA_RPORT_SM_SET_SPEED	= 8,	/*  Set Rport Speed             */
	BFA_RPORT_SM_QRESUME	= 9,	/*  space in requeue queue      */
};

#define BFA_RPORT_MIN	4

struct bfa_rport_mod_s {
	struct bfa_rport_s *rps_list;	/*  list of rports	*/
	struct list_head	rp_free_q;	/*  free bfa_rports	*/
	struct list_head	rp_active_q;	/*  free bfa_rports	*/
	u16	num_rports;	/*  number of rports	*/
};

#define BFA_RPORT_MOD(__bfa)	(&(__bfa)->modules.rport_mod)

/*
 * Convert rport tag to RPORT
 */
#define BFA_RPORT_FROM_TAG(__bfa, _tag)				\
	(BFA_RPORT_MOD(__bfa)->rps_list +			\
	 ((_tag) & (BFA_RPORT_MOD(__bfa)->num_rports - 1)))

/*
 * protected functions
 */
void	bfa_rport_isr(struct bfa_s *bfa, struct bfi_msg_s *msg);

/*
 *	BFA rport information.
 */
struct bfa_rport_info_s {
	u16	max_frmsz;	/*  max rcv pdu size		    */
	u32	pid:24,	/*  remote port ID		    */
		lp_tag:8;	/*  tag			    */
	u32	local_pid:24,	/*  local port ID		    */
		cisc:8;	/*  CIRO supported		    */
	u8	fc_class;	/*  supported FC classes. enum fc_cos */
	u8	vf_en;		/*  virtual fabric enable	    */
	u16	vf_id;		/*  virtual fabric ID		    */
	enum bfa_port_speed speed;	/*  Rport's current speed	    */
};

/*
 * BFA rport data structure
 */
struct bfa_rport_s {
	struct list_head	qe;	/*  queue element		    */
	bfa_sm_t	sm;		/*  state machine		    */
	struct bfa_s	*bfa;		/*  backpointer to BFA		    */
	void		*rport_drv;	/*  fcs/driver rport object	    */
	u16	fw_handle;	/*  firmware rport handle	    */
	u16	rport_tag;	/*  BFA rport tag		    */
	struct bfa_rport_info_s rport_info; /*  rport info from fcs/driver */
	struct bfa_reqq_wait_s reqq_wait; /*  to wait for room in reqq     */
	struct bfa_cb_qe_s hcb_qe;	/*  BFA callback qelem		    */
	struct bfa_rport_hal_stats_s stats; /*  BFA rport statistics	    */
	struct bfa_rport_qos_attr_s qos_attr;
	union a {
		bfa_status_t	status;	/*  f/w status */
		void		*fw_msg; /*  QoS scn event		    */
	} event_arg;
};
#define BFA_RPORT_FC_COS(_rport)	((_rport)->rport_info.fc_class)


/*
 * UF - unsolicited receive related defines
 */

#define BFA_UF_MIN	(4)


struct bfa_uf_s {
	struct list_head	qe;	/*  queue element		*/
	struct bfa_s		*bfa;	/*  bfa instance		*/
	u16	uf_tag;		/*  identifying tag fw msgs	*/
	u16	vf_id;
	u16	src_rport_handle;
	u16	rsvd;
	u8		*data_ptr;
	u16	data_len;	/*  actual receive length	*/
	u16	pb_len;		/*  posted buffer length	*/
	void		*buf_kva;	/*  buffer virtual address	*/
	u64	buf_pa;		/*  buffer physical address	*/
	struct bfa_cb_qe_s hcb_qe;	/*  comp: BFA comp qelem	*/
	struct bfa_sge_s sges[BFI_SGE_INLINE_MAX];
};

/*
 *      Callback prototype for unsolicited frame receive handler.
 *
 * @param[in]           cbarg           callback arg for receive handler
 * @param[in]           uf              unsolicited frame descriptor
 *
 * @return None
 */
typedef void (*bfa_cb_uf_recv_t) (void *cbarg, struct bfa_uf_s *uf);

struct bfa_uf_mod_s {
	struct bfa_s *bfa;		/*  back pointer to BFA */
	struct bfa_uf_s *uf_list;	/*  array of UFs */
	u16	num_ufs;	/*  num unsolicited rx frames */
	struct list_head	uf_free_q;	/*  free UFs */
	struct list_head	uf_posted_q;	/*  UFs posted to IOC */
	struct bfa_uf_buf_s *uf_pbs_kva;	/*  list UF bufs request pld */
	u64	uf_pbs_pa;	/*  phy addr for UF bufs */
	struct bfi_uf_buf_post_s *uf_buf_posts;
	/*  pre-built UF post msgs */
	bfa_cb_uf_recv_t ufrecv;	/*  uf recv handler function */
	void		*cbarg;		/*  uf receive handler arg */
};

#define BFA_UF_MOD(__bfa)	(&(__bfa)->modules.uf_mod)

#define ufm_pbs_pa(_ufmod, _uftag)					\
	((_ufmod)->uf_pbs_pa + sizeof(struct bfa_uf_buf_s) * (_uftag))

void	bfa_uf_isr(struct bfa_s *bfa, struct bfi_msg_s *msg);

#define BFA_UF_BUFSZ	(2 * 1024 + 256)

/*
 * @todo private
 */
struct bfa_uf_buf_s {
	u8		d[BFA_UF_BUFSZ];
};


/*
 * LPS - bfa lport login/logout service interface
 */
struct bfa_lps_s {
	struct list_head	qe;	/*  queue element		*/
	struct bfa_s	*bfa;		/*  parent bfa instance	*/
	bfa_sm_t	sm;		/*  finite state machine	*/
	u8		lp_tag;		/*  lport tag			*/
	u8		reqq;		/*  lport request queue	*/
	u8		alpa;		/*  ALPA for loop topologies	*/
	u32	lp_pid;		/*  lport port ID		*/
	bfa_boolean_t	fdisc;		/*  snd FDISC instead of FLOGI	*/
	bfa_boolean_t	auth_en;	/*  enable authentication	*/
	bfa_boolean_t	auth_req;	/*  authentication required	*/
	bfa_boolean_t	npiv_en;	/*  NPIV is allowed by peer	*/
	bfa_boolean_t	fport;		/*  attached peer is F_PORT	*/
	bfa_boolean_t	brcd_switch;	/*  attached peer is brcd sw	*/
	bfa_status_t	status;		/*  login status		*/
	u16		pdusz;		/*  max receive PDU size	*/
	u16		pr_bbcred;	/*  BB_CREDIT from peer		*/
	u8		lsrjt_rsn;	/*  LSRJT reason		*/
	u8		lsrjt_expl;	/*  LSRJT explanation		*/
	wwn_t		pwwn;		/*  port wwn of lport		*/
	wwn_t		nwwn;		/*  node wwn of lport		*/
	wwn_t		pr_pwwn;	/*  port wwn of lport peer	*/
	wwn_t		pr_nwwn;	/*  node wwn of lport peer	*/
	mac_t		lp_mac;		/*  fpma/spma MAC for lport	*/
	mac_t		fcf_mac;	/*  FCF MAC of lport		*/
	struct bfa_reqq_wait_s	wqe;	/*  request wait queue element	*/
	void		*uarg;		/*  user callback arg		*/
	struct bfa_cb_qe_s hcb_qe;	/*  comp: callback qelem	*/
	struct bfi_lps_login_rsp_s *loginrsp;
	bfa_eproto_status_t ext_status;
};

struct bfa_lps_mod_s {
	struct list_head		lps_free_q;
	struct list_head		lps_active_q;
	struct bfa_lps_s	*lps_arr;
	int			num_lps;
};

#define BFA_LPS_MOD(__bfa)		(&(__bfa)->modules.lps_mod)
#define BFA_LPS_FROM_TAG(__mod, __tag)	(&(__mod)->lps_arr[__tag])

/*
 * external functions
 */
void	bfa_lps_isr(struct bfa_s *bfa, struct bfi_msg_s *msg);


/*
 * FCPORT related defines
 */

#define BFA_FCPORT(_bfa)	(&((_bfa)->modules.port))
typedef void (*bfa_cb_port_t) (void *cbarg, enum bfa_status status);

/*
 * Link notification data structure
 */
struct bfa_fcport_ln_s {
	struct bfa_fcport_s	*fcport;
	bfa_sm_t		sm;
	struct bfa_cb_qe_s	ln_qe;	/*  BFA callback queue elem for ln */
	enum bfa_port_linkstate ln_event; /*  ln event for callback */
};

struct bfa_fcport_trunk_s {
	struct bfa_trunk_attr_s	attr;
};

/*
 * BFA FC port data structure
 */
struct bfa_fcport_s {
	struct bfa_s		*bfa;	/*  parent BFA instance */
	bfa_sm_t		sm;	/*  port state machine */
	wwn_t			nwwn;	/*  node wwn of physical port */
	wwn_t			pwwn;	/*  port wwn of physical oprt */
	enum bfa_port_speed speed_sup;
	/*  supported speeds */
	enum bfa_port_speed speed;	/*  current speed */
	enum bfa_port_topology topology;	/*  current topology */
	u8			myalpa;	/*  my ALPA in LOOP topology */
	u8			rsvd[3];
	struct bfa_port_cfg_s	cfg;	/*  current port configuration */
	bfa_boolean_t		use_flash_cfg; /* get port cfg from flash */
	struct bfa_qos_attr_s  qos_attr;   /* QoS Attributes */
	struct bfa_qos_vc_attr_s qos_vc_attr;  /*  VC info from ELP */
	struct bfa_reqq_wait_s	reqq_wait;
	/*  to wait for room in reqq */
	struct bfa_reqq_wait_s	svcreq_wait;
	/*  to wait for room in reqq */
	struct bfa_reqq_wait_s	stats_reqq_wait;
	/*  to wait for room in reqq (stats) */
	void			*event_cbarg;
	void			(*event_cbfn) (void *cbarg,
					       enum bfa_port_linkstate event);
	union {
		union bfi_fcport_i2h_msg_u i2hmsg;
	} event_arg;
	void			*bfad;	/*  BFA driver handle */
	struct bfa_fcport_ln_s	ln; /*  Link Notification */
	struct bfa_cb_qe_s	hcb_qe;	/*  BFA callback queue elem */
	struct bfa_timer_s	timer;	/*  timer */
	u32		msgtag;	/*  fimrware msg tag for reply */
	u8			*stats_kva;
	u64		stats_pa;
	union bfa_fcport_stats_u *stats;
	union bfa_fcport_stats_u *stats_ret; /*  driver stats location */
	bfa_status_t		stats_status; /*  stats/statsclr status */
	bfa_boolean_t		stats_busy; /*  outstanding stats/statsclr */
	bfa_boolean_t		stats_qfull;
	u32		stats_reset_time; /*  stats reset time stamp */
	bfa_cb_port_t		stats_cbfn; /*  driver callback function */
	void			*stats_cbarg; /* *!< user callback arg */
	bfa_boolean_t		diag_busy; /*  diag busy status */
	bfa_boolean_t		beacon; /*  port beacon status */
	bfa_boolean_t		link_e2e_beacon; /*  link beacon status */
	struct bfa_fcport_trunk_s trunk;
	u16		fcoe_vlan;
};

#define BFA_FCPORT_MOD(__bfa)	(&(__bfa)->modules.fcport)

/*
 * protected functions
 */
void bfa_fcport_init(struct bfa_s *bfa);
void bfa_fcport_isr(struct bfa_s *bfa, struct bfi_msg_s *msg);

/*
 * bfa fcport API functions
 */
bfa_status_t bfa_fcport_enable(struct bfa_s *bfa);
bfa_status_t bfa_fcport_disable(struct bfa_s *bfa);
bfa_status_t bfa_fcport_cfg_speed(struct bfa_s *bfa,
				  enum bfa_port_speed speed);
enum bfa_port_speed bfa_fcport_get_speed(struct bfa_s *bfa);
bfa_status_t bfa_fcport_cfg_topology(struct bfa_s *bfa,
				     enum bfa_port_topology topo);
enum bfa_port_topology bfa_fcport_get_topology(struct bfa_s *bfa);
bfa_status_t bfa_fcport_cfg_hardalpa(struct bfa_s *bfa, u8 alpa);
bfa_boolean_t bfa_fcport_get_hardalpa(struct bfa_s *bfa, u8 *alpa);
u8 bfa_fcport_get_myalpa(struct bfa_s *bfa);
bfa_status_t bfa_fcport_clr_hardalpa(struct bfa_s *bfa);
bfa_status_t bfa_fcport_cfg_maxfrsize(struct bfa_s *bfa, u16 maxsize);
u16 bfa_fcport_get_maxfrsize(struct bfa_s *bfa);
u8 bfa_fcport_get_rx_bbcredit(struct bfa_s *bfa);
void bfa_fcport_get_attr(struct bfa_s *bfa, struct bfa_port_attr_s *attr);
wwn_t bfa_fcport_get_wwn(struct bfa_s *bfa, bfa_boolean_t node);
void bfa_fcport_event_register(struct bfa_s *bfa,
			void (*event_cbfn) (void *cbarg,
			enum bfa_port_linkstate event), void *event_cbarg);
bfa_boolean_t bfa_fcport_is_disabled(struct bfa_s *bfa);
enum bfa_port_speed bfa_fcport_get_ratelim_speed(struct bfa_s *bfa);

void bfa_fcport_set_tx_bbcredit(struct bfa_s *bfa, u16 tx_bbcredit);
bfa_boolean_t     bfa_fcport_is_ratelim(struct bfa_s *bfa);
bfa_boolean_t	bfa_fcport_is_linkup(struct bfa_s *bfa);
bfa_status_t bfa_fcport_get_stats(struct bfa_s *bfa,
				  union bfa_fcport_stats_u *stats,
				  bfa_cb_port_t cbfn, void *cbarg);
bfa_status_t bfa_fcport_clear_stats(struct bfa_s *bfa, bfa_cb_port_t cbfn,
				    void *cbarg);
bfa_boolean_t bfa_fcport_is_qos_enabled(struct bfa_s *bfa);

/*
 * bfa rport API functions
 */
struct bfa_rport_s *bfa_rport_create(struct bfa_s *bfa, void *rport_drv);
void bfa_rport_online(struct bfa_rport_s *rport,
		      struct bfa_rport_info_s *rport_info);
void bfa_rport_speed(struct bfa_rport_s *rport, enum bfa_port_speed speed);
void bfa_cb_rport_online(void *rport);
void bfa_cb_rport_offline(void *rport);
void bfa_cb_rport_qos_scn_flowid(void *rport,
				 struct bfa_rport_qos_attr_s old_qos_attr,
				 struct bfa_rport_qos_attr_s new_qos_attr);
void bfa_cb_rport_qos_scn_prio(void *rport,
			       struct bfa_rport_qos_attr_s old_qos_attr,
			       struct bfa_rport_qos_attr_s new_qos_attr);

/*
 * bfa fcxp API functions
 */
struct bfa_fcxp_s *bfa_fcxp_alloc(void *bfad_fcxp, struct bfa_s *bfa,
				  int nreq_sgles, int nrsp_sgles,
				  bfa_fcxp_get_sgaddr_t get_req_sga,
				  bfa_fcxp_get_sglen_t get_req_sglen,
				  bfa_fcxp_get_sgaddr_t get_rsp_sga,
				  bfa_fcxp_get_sglen_t get_rsp_sglen);
void bfa_fcxp_alloc_wait(struct bfa_s *bfa, struct bfa_fcxp_wqe_s *wqe,
				bfa_fcxp_alloc_cbfn_t alloc_cbfn,
				void *cbarg, void *bfad_fcxp,
				int nreq_sgles, int nrsp_sgles,
				bfa_fcxp_get_sgaddr_t get_req_sga,
				bfa_fcxp_get_sglen_t get_req_sglen,
				bfa_fcxp_get_sgaddr_t get_rsp_sga,
				bfa_fcxp_get_sglen_t get_rsp_sglen);
void bfa_fcxp_walloc_cancel(struct bfa_s *bfa,
			    struct bfa_fcxp_wqe_s *wqe);
void bfa_fcxp_discard(struct bfa_fcxp_s *fcxp);

void *bfa_fcxp_get_reqbuf(struct bfa_fcxp_s *fcxp);
void *bfa_fcxp_get_rspbuf(struct bfa_fcxp_s *fcxp);

void bfa_fcxp_free(struct bfa_fcxp_s *fcxp);

void bfa_fcxp_send(struct bfa_fcxp_s *fcxp, struct bfa_rport_s *rport,
		   u16 vf_id, u8 lp_tag,
		   bfa_boolean_t cts, enum fc_cos cos,
		   u32 reqlen, struct fchs_s *fchs,
		   bfa_cb_fcxp_send_t cbfn,
		   void *cbarg,
		   u32 rsp_maxlen, u8 rsp_timeout);
bfa_status_t bfa_fcxp_abort(struct bfa_fcxp_s *fcxp);
u32 bfa_fcxp_get_reqbufsz(struct bfa_fcxp_s *fcxp);
u32 bfa_fcxp_get_maxrsp(struct bfa_s *bfa);

static inline void *
bfa_uf_get_frmbuf(struct bfa_uf_s *uf)
{
	return uf->data_ptr;
}

static inline   u16
bfa_uf_get_frmlen(struct bfa_uf_s *uf)
{
	return uf->data_len;
}

/*
 * bfa uf API functions
 */
void bfa_uf_recv_register(struct bfa_s *bfa, bfa_cb_uf_recv_t ufrecv,
			  void *cbarg);
void bfa_uf_free(struct bfa_uf_s *uf);

/*
 * bfa lport service api
 */

u32 bfa_lps_get_max_vport(struct bfa_s *bfa);
struct bfa_lps_s *bfa_lps_alloc(struct bfa_s *bfa);
void bfa_lps_delete(struct bfa_lps_s *lps);
void bfa_lps_flogi(struct bfa_lps_s *lps, void *uarg, u8 alpa,
		   u16 pdusz, wwn_t pwwn, wwn_t nwwn,
		   bfa_boolean_t auth_en);
void bfa_lps_fdisc(struct bfa_lps_s *lps, void *uarg, u16 pdusz,
		   wwn_t pwwn, wwn_t nwwn);
void bfa_lps_fdisclogo(struct bfa_lps_s *lps);
void bfa_lps_set_n2n_pid(struct bfa_lps_s *lps, u32 n2n_pid);
u32 bfa_lps_get_base_pid(struct bfa_s *bfa);
u8 bfa_lps_get_tag_from_pid(struct bfa_s *bfa, u32 pid);
void bfa_cb_lps_flogi_comp(void *bfad, void *uarg, bfa_status_t status);
void bfa_cb_lps_fdisc_comp(void *bfad, void *uarg, bfa_status_t status);
void bfa_cb_lps_fdisclogo_comp(void *bfad, void *uarg);
void bfa_cb_lps_cvl_event(void *bfad, void *uarg);

#endif /* __BFA_SVC_H__ */
