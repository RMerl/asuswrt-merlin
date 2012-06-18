/*
 * Copyright (C) 2011, Broadcom Corporation. All Rights Reserved.
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
 * $Id: hndctf.h 322208 2012-03-20 01:53:23Z $
 */

#ifndef _HNDCTF_H_
#define _HNDCTF_H_

#include <bcmutils.h>
#include <proto/ethernet.h>

/*
 * Define to enable couting VLAN tx and rx packets and bytes. This could be
 * disabled if the functionality has impact on performance.
 */
#define CTFVLSTATS

#define CTF_ENAB(ci)		(((ci) != NULL) && (ci)->_ctf)

#define CTF_ACTION_TAG		(1 << 0)
#define CTF_ACTION_UNTAG	(1 << 1)
#define CTF_ACTION_SNAT		(1 << 2)
#define CTF_ACTION_DNAT		(1 << 3)

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
#define ctf_ipc_add(ci, i)	(CTF_ENAB(ci) ? (ci)->fn.ipc_add(ci, i) : BCME_OK)
#define ctf_ipc_delete(ci, sip, dip, p, sp, dp)	\
	(CTF_ENAB(ci) ? (ci)->fn.ipc_delete(ci, sip, dip, p, sp, dp) : BCME_OK)
#define ctf_ipc_count_get(ci, i) \
	(CTF_ENAB(ci) ? (ci)->fn.ipc_count_get(ci, i) : BCME_OK)
#define ctf_ipc_delete_multi(ci, i, im)	\
	(CTF_ENAB(ci) ? (ci)->fn.ipc_delete_multi(ci, i, im) : BCME_OK)
#define ctf_ipc_delete_range(ci, i, im)	\
	(CTF_ENAB(ci) ? (ci)->fn.ipc_delete_range(ci, i, im) : BCME_OK)
#define ctf_ipc_lkup(ci, sip, dip, p, sp, dp)	\
	(CTF_ENAB(ci) ? (ci)->fn.ipc_lkup(ci, sip, dip, p, sp, dp) : NULL)
#define ctf_dev_register(ci, d, b)	\
	(CTF_ENAB(ci) ? (ci)->fn.dev_register(ci, d, b) : BCME_OK)
#define ctf_dev_vlan_add(ci, d, vid, vd)	\
	(CTF_ENAB(ci) ? (ci)->fn.dev_vlan_add(ci, d, vid, vd) : BCME_OK)
#define ctf_dev_vlan_delete(ci, d, vid)	\
	(CTF_ENAB(ci) ? (ci)->fn.dev_vlan_delete(ci, d, vid) : BCME_OK)
#define ctf_detach(ci)			if (CTF_ENAB(ci)) (ci)->fn.detach(ci)
#define ctf_dump(ci, b)			if (CTF_ENAB(ci)) (ci)->fn.dump(ci, b)
#define ctf_dev_unregister(ci, d)	if (CTF_ENAB(ci)) (ci)->fn.dev_unregister(ci, d)

#ifdef BCMDBG
#define CTFCNTINCR(s) ((s)++)
#else /* BCMDBG */
#define CTFCNTINCR(s)
#endif /* BCMDBG */

/* Copy an ethernet address in reverse order */
#define	ether_rcopy(s, d) \
do { \
	((uint16 *)(d))[2] = ((uint16 *)(s))[2]; \
	((uint16 *)(d))[1] = ((uint16 *)(s))[1]; \
	((uint16 *)(d))[0] = ((uint16 *)(s))[0]; \
} while (0)

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
typedef int32 (*ctf_ipc_add_t)(ctf_t *ci, ctf_ipc_t *ipc);
typedef int32 (*ctf_ipc_delete_t)(ctf_t *ci, uint32 sip, uint32 dip, uint8 proto,
                                  uint16 sp, uint16 dp);
typedef int32 (*ctf_ipc_count_get_t)(ctf_t *ci);
typedef int32 (*ctf_ipc_delete_multi_t)(ctf_t *ci, ctf_ipc_t *ipc, ctf_ipc_t *ipcm);
typedef int32 (*ctf_ipc_delete_range_t)(ctf_t *ci, ctf_ipc_t *start, ctf_ipc_t *end);
typedef ctf_ipc_t * (*ctf_ipc_lkup_t)(ctf_t *ci, uint32 sip, uint32 dip, uint8 proto,
                                    uint16 sp, uint16 dp);
typedef int32 (*ctf_enable_t)(ctf_t *ci, void *dev, bool enable, ctf_brc_hot_t **brc_hot);
typedef int32 (*ctf_dev_register_t)(ctf_t *ci, void *dev, bool br);
typedef void (*ctf_dev_unregister_t)(ctf_t *ci, void *dev);
typedef int32 (*ctf_dev_vlan_add_t)(ctf_t *ci, void *dev, uint16 vid, void *vldev);
typedef int32 (*ctf_dev_vlan_delete_t)(ctf_t *ci, void *dev, uint16 vid);
typedef void (*ctf_dump_t)(ctf_t *ci, struct bcmstrbuf *b);

struct ctf_brc_hot {
	struct ether_addr	ea;		/* Dest mac addr */
	uint16			PAD;		/* To round size to 8 bytes */
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
	ctf_ipc_lkup_t		ipc_lkup;
	ctf_enable_t		enable;
	ctf_dev_register_t	dev_register;
	ctf_dev_unregister_t	dev_unregister;
	ctf_detach_cb_t		detach_cb;
	void			*detach_cb_arg;
	ctf_dev_vlan_add_t	dev_vlan_add;
	ctf_dev_vlan_delete_t	dev_vlan_delete;
	ctf_dump_t		dump;
} ctf_fn_t;

struct ctf_pub {
	bool			_ctf;		/* Global CTF enable/disable */
	ctf_fn_t		fn;		/* Exported functions */
};

struct ctf_brc {
	struct	ctf_brc		*next;		/* Pointer to brc entry */
	struct	ether_addr	dhost;		/* MAC addr of host */
	uint16			vid;		/* VLAN id to use on txif */
	void			*txifp;		/* Interface connected to host */
	uint32			action;		/* Tag or untag the frames */
	uint32			live;		/* Counter used to expire the entry */
	uint32			hits;		/* Num frames matching brc entry */
};

struct ctf_conn_tuple {
	uint32	sip, dip;
	uint16	sp, dp;
	uint8	proto;
};

typedef struct ctf_nat {
	uint32	ip;
	uint16	port;
} ctf_nat_t;

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
	uint32			hits;		/* Num frames matching ipc entry */
};

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
#define CTF_HOTBRC_CMP(brc, da) \
({ \
	ctf_brc_hot_t *bh = brc + CTF_BRC_HOT_HASH(da); \
	(eacmp((bh)->ea.octet, (da)) == 0); \
})

#endif /* _HNDCTF_H_ */
