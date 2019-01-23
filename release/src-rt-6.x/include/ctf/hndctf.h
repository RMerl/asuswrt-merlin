/*
 * Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
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
 * $Id: hndctf.h 371260 2012-11-27 19:42:11Z $
 */

#ifndef _HNDCTF_H_
#define _HNDCTF_H_

#include <bcmutils.h>
#include <proto/bcmip.h>
#include <proto/ethernet.h>
#include <proto/vlan.h>

/*
 * Define to enable couting VLAN tx and rx packets and bytes. This could be
 * disabled if the functionality has impact on performance.
 */
#define CTFVLSTATS

#define CTF_ENAB(ci)		(((ci) != NULL) && (ci)->_ctf)
#define CTFQOS_ULDL_DIFFIF(ci)		(((ci) != NULL) && ((ci)->_ctf == (1 << 1)))

#define CTF_ACTION_TAG		(1 << 0)
#define CTF_ACTION_UNTAG	(1 << 1)
#define CTF_ACTION_SNAT		(1 << 2)
#define CTF_ACTION_DNAT		(1 << 3)
#define CTF_ACTION_SUSPEND	(1 << 4)
#define CTF_ACTION_TOS		(1 << 5)
#define CTF_ACTION_MARK		(1 << 6)
#define CTF_ACTION_BYTECNT	(1 << 7)
#define CTF_ACTION_PPPOE_ADD	(1 << 8)
#define CTF_ACTION_PPPOE_DEL	(1 << 9)
#define CTF_ACTION_PPTP_ADD	(1 << 10)
#define CTF_ACTION_PPTP_DEL	(1 << 11)
#define CTF_ACTION_L2TP_ADD	(1 << 12)
#define CTF_ACTION_L2TP_DEL	(1 << 13)

#define	ctf_attach(osh, n, m, c, a) \
	(ctf_attach_fn ? ctf_attach_fn(osh, n, m, c, a) : NULL)
#define ctf_forward(ci, p, d)	(ci)->fn.forward(ci, p, d)
#define ctf_isenabled(ci, d)	(CTF_ENAB(ci) ? (ci)->fn.isenabled(ci, d) : FALSE)
#define ctf_isbridge(ci, d)	(CTF_ENAB(ci) ? (ci)->fn.isbridge(ci, d) : FALSE)
#define ctf_enable(ci, d, e, b)	(CTF_ENAB(ci) ? (ci)->fn.enable(ci, d, e, b) : BCME_OK)
#define ctf_brc_add(ci, b)	(CTF_ENAB(ci) ? (ci)->fn.brc_add(ci, b) : BCME_OK)
#define ctf_brc_delete(ci, e)	(CTF_ENAB(ci) ? (ci)->fn.brc_delete(ci, e) : BCME_OK)
#define ctf_brc_update(ci, b)	(CTF_ENAB(ci) ? (ci)->fn.brc_update(ci, b) : BCME_OK)
#define ctf_brc_lkup(ci, e)	(CTF_ENAB(ci) ? (ci)->fn.brc_lkup(ci, e) : NULL)
#define ctf_ipc_add(ci, i, v6)	(CTF_ENAB(ci) ? (ci)->fn.ipc_add(ci, i, v6) : BCME_OK)
#define ctf_ipc_delete(ci, i, v6)	\
	(CTF_ENAB(ci) ? (ci)->fn.ipc_delete(ci, i, v6) : BCME_OK)
#define ctf_ipc_count_get(ci, i) \
	(CTF_ENAB(ci) ? (ci)->fn.ipc_count_get(ci, i) : BCME_OK)
#define ctf_ipc_delete_multi(ci, i, im, v6)	\
	(CTF_ENAB(ci) ? (ci)->fn.ipc_delete_multi(ci, i, im, v6) : BCME_OK)
#define ctf_ipc_delete_range(ci, s, e, v6)	\
	(CTF_ENAB(ci) ? (ci)->fn.ipc_delete_range(ci, s, e, v6) : BCME_OK)
#define ctf_ipc_action(ci, s, e, am, v6) \
	(CTF_ENAB(ci) ? (ci)->fn.ipc_action(ci, s, e, am, v6) : BCME_OK)
#define ctf_ipc_lkup(ci, i, v6)	\
	(CTF_ENAB(ci) ? (ci)->fn.ipc_lkup(ci, i, v6) : NULL)
#ifdef CTF_IPV6
#define ctf_ipc_lkup_l4proto(ci, iph, l4p)	(CTF_ENAB(ci) && (ci)->fn.ipc_lkup_l4proto? \
	(ci)->fn.ipc_lkup_l4proto((uint8 *)iph, l4p) : NULL)
#else
#define ctf_ipc_lkup_l4proto(ci, iph, l4p)	(NULL)
#endif /* CTF_IPV6 */
#define ctf_dev_register(ci, d, b)	\
	(CTF_ENAB(ci) ? (ci)->fn.dev_register(ci, d, b) : BCME_OK)
#define ctf_dev_vlan_add(ci, d, vid, vd)	\
	(CTF_ENAB(ci) ? (ci)->fn.dev_vlan_add(ci, d, vid, vd) : BCME_OK)
#define ctf_dev_vlan_delete(ci, d, vid)	\
	(CTF_ENAB(ci) ? (ci)->fn.dev_vlan_delete(ci, d, vid) : BCME_OK)
#define ctf_detach(ci)			if (CTF_ENAB(ci)) (ci)->fn.detach(ci)
#define ctf_dump(ci, b)			if (CTF_ENAB(ci)) (ci)->fn.dump(ci, b)
#define ctf_dev_unregister(ci, d)	if (CTF_ENAB(ci)) (ci)->fn.dev_unregister(ci, d)
#ifdef CTF_L2TP
#define ctf_l2tp_lookup(__ci__, __p__)	\
	(CTF_ENAB((__ci__)) ? ((__ci__))->fn.l2tp_lookup((__ci__), (__p__)) : BCME_OK)
#endif /* CTF_L2TP */	
#define CTFCNTINCR(s) ((s)++)
#define CTFCNTADD(s, c) ((s) += (c))

#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[3]

#ifdef CTF_IPV6
#define FRAG_IPV6_UDP_PROTO	0xF6
#endif

#define PPPOE_ETYPE_OFFSET	12
#define PPPOE_VER_OFFSET	14
#define PPPOE_SESID_OFFSET	16
#define PPPOE_LEN_OFFSET	18

#define PPPOE_HLEN		20
#define PPPOE_PPP_HLEN		8 //PPPOE + PPP HEADER LEN

#define PPPOE_PROT_PPP_IP		0x0021
#define PPPOE_PROT_PPP_IP6		0x0057


typedef struct ctf_pub	ctf_t;
typedef struct ctf_brc	ctf_brc_t;
typedef struct ctf_ipc	ctf_ipc_t;
typedef struct ctf_conn_tuple	ctf_conn_tuple_t;
typedef struct ctf_brc_hot ctf_brc_hot_t;

typedef void (*ctf_detach_cb_t)(ctf_t *ci, void *arg);
typedef ctf_t * (*ctf_attach_t)(osl_t *osh, uint8 *name, uint32 *msg_level,
                                ctf_detach_cb_t cb, void *arg);
typedef void (*ctf_detach_t)(ctf_t *ci);
typedef int32 (*ctf_forward_t)(ctf_t *ci, void *p, void *rxifp);
typedef bool (*ctf_isenabled_t)(ctf_t *ci, void *dev);
typedef bool (*ctf_isbridge_t)(ctf_t *ci, void *dev);
typedef int32 (*ctf_brc_add_t)(ctf_t *ci, ctf_brc_t *brc);
typedef int32 (*ctf_brc_delete_t)(ctf_t *ci, uint8 *ea);
typedef int32 (*ctf_brc_update_t)(ctf_t *ci, ctf_brc_t *brc);
typedef ctf_brc_t * (*ctf_brc_lkup_t)(ctf_t *ci, uint8 *da);
typedef int32 (*ctf_ipc_add_t)(ctf_t *ci, ctf_ipc_t *ipc, bool v6);
typedef int32 (*ctf_ipc_delete_t)(ctf_t *ci, ctf_ipc_t *ipc, bool v6);
typedef int32 (*ctf_ipc_count_get_t)(ctf_t *ci);
typedef int32 (*ctf_ipc_delete_multi_t)(ctf_t *ci, ctf_ipc_t *ipc,
                                        ctf_ipc_t *ipcm, bool v6);
typedef int32 (*ctf_ipc_delete_range_t)(ctf_t *ci, ctf_ipc_t *start,
                                        ctf_ipc_t *end, bool v6);
typedef int32 (*ctf_ipc_action_t)(ctf_t *ci, ctf_ipc_t *start,
                                  ctf_ipc_t *end, uint32 action_mask, bool v6);
typedef ctf_ipc_t * (*ctf_ipc_lkup_t)(ctf_t *ci, ctf_ipc_t *ipc, bool v6);
typedef	uint8 * (*ctf_ipc_lkup_l4proto_t)(uint8 *iph, uint8 *proto_num);
typedef int32 (*ctf_enable_t)(ctf_t *ci, void *dev, bool enable, ctf_brc_hot_t **brc_hot);
typedef int32 (*ctf_dev_register_t)(ctf_t *ci, void *dev, bool br);
typedef void (*ctf_dev_unregister_t)(ctf_t *ci, void *dev);
typedef int32 (*ctf_dev_vlan_add_t)(ctf_t *ci, void *dev, uint16 vid, void *vldev);
typedef int32 (*ctf_dev_vlan_delete_t)(ctf_t *ci, void *dev, uint16 vid);
#ifdef CTF_L2TP	
typedef int32 (*ctf_l2tp_lookup_t)(ctf_t *ci, void *p);
#endif /* CTF_L2TP */
typedef void (*ctf_dump_t)(ctf_t *ci, struct bcmstrbuf *b);

struct ctf_brc_hot {
	struct ether_addr	ea;	/* Dest address */
	ctf_brc_t		*brcp;	/* BRC entry corresp to dest mac */
};

typedef struct ctf_fn {
	ctf_detach_t		detach;
	ctf_forward_t		forward;
	ctf_isenabled_t		isenabled;
	ctf_isbridge_t		isbridge;
	ctf_brc_add_t		brc_add;
	ctf_brc_delete_t	brc_delete;
	ctf_brc_update_t	brc_update;
	ctf_brc_lkup_t		brc_lkup;
	ctf_ipc_add_t		ipc_add;
	ctf_ipc_delete_t	ipc_delete;
	ctf_ipc_count_get_t	ipc_count_get;
	ctf_ipc_delete_multi_t	ipc_delete_multi;
	ctf_ipc_delete_range_t	ipc_delete_range;
	ctf_ipc_action_t	ipc_action;
	ctf_ipc_lkup_t		ipc_lkup;
	ctf_ipc_lkup_l4proto_t ipc_lkup_l4proto;
	ctf_enable_t		enable;
	ctf_dev_register_t	dev_register;
	ctf_dev_unregister_t	dev_unregister;
	ctf_detach_cb_t		detach_cb;
	void			*detach_cb_arg;
	ctf_dev_vlan_add_t	dev_vlan_add;
	ctf_dev_vlan_delete_t	dev_vlan_delete;
#ifdef CTF_L2TP		
	ctf_l2tp_lookup_t l2tp_lookup;
#endif /* CTF_L2TP */
	ctf_dump_t		dump;
} ctf_fn_t;

struct ctf_pub {
	uint8			_ctf;		/* Global CTF enable/disable */
	ctf_fn_t		fn;		/* Exported functions */
};

struct ctf_mark;	/* Connection Mark */

struct ctf_brc {
	struct	ctf_brc		*next;		/* Pointer to brc entry */
	struct	ether_addr	dhost;		/* MAC addr of host */
	uint16			vid;		/* VLAN id to use on txif */
	void			*txifp;		/* Interface connected to host */
	void			*txvifp;		/* vlan Interface connected to host*/	
	uint32			action;		/* Tag or untag the frames */
	uint32			live;		/* Counter used to expire the entry */
	uint32			hits;		/* Num frames matching brc entry */
	uint64			*bytecnt_ptr;	/* Pointer to the byte counter */
};

#ifdef CTF_IPV6
#define IPADDR_U32_SZ		(IPV6_ADDR_LEN / sizeof(uint32))
#else
#define IPADDR_U32_SZ		1
#endif

struct ctf_conn_tuple {
	uint32	sip[IPADDR_U32_SZ], dip[IPADDR_U32_SZ];
	uint16	sp, dp;
	uint8	proto;
	uint16	sid;
};

typedef struct ctf_nat {
	uint32	ip;
	uint16	port;
} ctf_nat_t;

typedef struct ctf_pptp {
	struct pptp_opt 	*pptpopt;		/*pointer to pppoe socket*/
	uint8 	sk_pmtudisc;//iph->frag_off 
	uint32	rt_dst_mtrc_lock_fgoff;//iph->frag_off 
	uint32	rt_dst_mtrc_hoplmt;//iph->ttl 
}ctf_pptp_t;

struct ctf_ipc {
	struct	ctf_ipc		*next;		/* Pointer to ipc entry */
	ctf_conn_tuple_t	tuple;		/* Tuple to uniquely id the flow */
	uint16			vid;		/* VLAN id to use on txif */
	struct	ether_addr	dhost;		/* Destination MAC address */
	struct	ether_addr	shost;		/* Source MAC address */
	void			*txif;		/* Target interface to send */
	uint32			action;		/* NAT and/or VLAN actions */
	ctf_brc_t		*brcp;		/* BRC entry corresp to source mac */
	uint32			live;		/* Counter used to expire the entry */
	struct	ctf_nat		nat;		/* Manip data for SNAT, DNAT */
	struct	ether_addr	sa;		/* MAC address of sender */
	uint8			tos;		/* IPv4 tos or IPv6 traffic class field with ECN cleared */
	uint16			pppoe_sid;	/*PPTP peer call id if wan type is pptp, PPPOE session ID if wan type is PPPOE*/
#ifdef CTF_PPTP	
	ctf_pptp_t		pptp;
#endif
#ifdef CTF_L2TP
	void			*pppl2tp_tun;
#endif
	void			*ppp_ifp;	/* PPP interface handle */
	uint32			hits;		/* Num frames matching ipc entry */
	uint64			*bytecnt_ptr;	/* Pointer to the byte counter */
	struct	ctf_mark	mark;		/* Mark value to use for the connection */
};




struct ctf_ppp_sk {
	struct pppox_sock 		*po;		/*pointer to pppoe socket*/
	unsigned char			pppox_protocol;	/*0:pppoe/1:pptp*/
	struct	ether_addr		dhost;		/*Remote MAC addr of host the pppox socket is bound to*/
};

typedef struct ctf_ppp {
	struct ctf_ppp_sk		psk;
	unsigned short			pppox_id;	/*PPTP peer call id if wan type is pptp, PPPOE session ID if wan type is PPPOE*/
} ctf_ppp_t;


#ifdef CTF_L2TP
/* L2TP */
struct ctf_pppol2tp_inet {
	uint32 saddr;		/* src IP address of tunnel */
	uint32 daddr;		/* src IP address of tunnel */
	uint16 sport;		/* src port                 */
	uint16 dport;		/* dst port                 */
	uint8  protocol;    /* IP protocol              */
	uint16 prio;		/* IP header priotity       */
	uint8  tos;         /* IP tos                   */
};

struct ctf_pppol2tp_session
{
	uint16 optver;				/* option/ver field       */
	uint16 tid;					/* Tunnel ID              */
	uint16 sid;					/* Session ID             */
	uint32 hdrlen;				/* l2tp pkt header length */
	uint8  ppph[16];	        /* ppp header             */
	uint32 ppph_len;			/* ppp header length      */
};

typedef struct ctf_pppol2tp_tunnel {

	struct net_device *dev;
	struct ctf_pppol2tp_inet inet;
	struct ctf_pppol2tp_session session;
} ctf_pppol2tp_tunnel_t;

#endif /* CTF_L2TP */


extern ctf_t *ctf_kattach(osl_t *osh, uint8 *name);
extern void ctf_kdetach(ctf_t *kci);
extern ctf_attach_t ctf_attach_fn;
extern ctf_t *_ctf_attach(osl_t *osh, uint8 *name, uint32 *msg_level,
                          ctf_detach_cb_t cb, void *arg);
extern ctf_t *kcih;

/* Hot bridge cache lkup */
#define MAXBRCHOT		4
#define MAXBRCHOTIF		4
#define CTF_BRC_HOT_HASH(da) 	((((uint8 *)da)[4] ^ ((uint8 *)da)[5]) & (MAXBRCHOT - 1))
#define CTF_HOTBRC_CMP(hbrc, da, rxifp) \
({ \
	ctf_brc_hot_t *bh = (hbrc) + CTF_BRC_HOT_HASH(da); \
	((eacmp((bh)->ea.octet, (da)) == 0) && (bh->brcp->txifp != (rxifp))); \
})

/* Header prep for packets matching hot bridge cache entry */
#define CTF_HOTBRC_L2HDR_PREP(osh, hbrc, prio, data, p) \
do { \
	uint8 *l2h; \
	ctf_brc_hot_t *bh = (hbrc) + CTF_BRC_HOT_HASH(data); \
	ASSERT(*(uint16 *)((data) + VLAN_TPID_OFFSET) == HTON16(ETHER_TYPE_8021Q)); \
	if (bh->brcp->action & CTF_ACTION_UNTAG) { \
		/* Remove vlan header */ \
		l2h = PKTPULL((osh), (p), VLAN_TAG_LEN); \
		ether_rcopy(l2h - VLAN_TAG_LEN + ETHER_ADDR_LEN, \
		            l2h + ETHER_ADDR_LEN); \
		ether_rcopy(l2h - VLAN_TAG_LEN, l2h); \
	} else { \
		/* Update vlan header */ \
		l2h = (data); \
		*(uint16 *)(l2h + VLAN_TCI_OFFSET) = \
		            HTON16((prio) << VLAN_PRI_SHIFT | bh->brcp->vid); \
	} \
} while (0)


#endif /* _HNDCTF_H_ */
