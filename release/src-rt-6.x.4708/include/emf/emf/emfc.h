/*
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: emfc.h 340526 2012-06-22 14:54:20Z $
 */

#ifndef _EMFC_H_
#define _EMFC_H_

#define MFDB_HASHT_SIZE         8
#define MFDB_MGRP_HASH(m)       ((((m) >> 24) + ((m) >> 16) + \
				  ((m) >> 8) + ((m) & 0xff)) & 7)

#define IP_ISMULTI(a)           (((a) & 0xf0000000) == 0xe0000000)
#define MCAST_ADDR_LINKLOCAL(a) (((a) & 0xffffff00) == 0xe0000000)
#define MCAST_ADDR_UPNP_SSDP(a) ((a) == 0xeffffffa)

#define EMFC_STATS_INCR(emfc, member) (((emfc)->stats.member)++)

#define EMFC_PROT_STATS_INCR(emfc, proto, mem1, mem2) (((proto) == IP_PROT_IGMP) ? \
	                                               (((emfc)->stats.mem1)++) : \
	                                               (((emfc)->stats.mem2)++))

/*
 * Multicast Group entry of MFDB
 */
typedef struct emfc_mgrp
{
	clist_head_t     mgrp_hlist;    /* Multicast Groups hash list */
	uint32           mgrp_ip;       /* Multicast Group IP Address */
	clist_head_t     mi_head;       /* List head of interfaces */
} emfc_mgrp_t;

/*
 * Multicast Interface entry of MFDB
 */
typedef struct emfc_mhif
{
	struct emfc_mhif *next;         /* Multicast host i/f prev and next */
	void             *mhif_ifp;     /* Interface pointer */
	uint32           mhif_data_fwd; /* Number of MCASTs sent on the i/f */
} emfc_mhif_t;

typedef struct emfc_mi
{
	clist_head_t     mi_list;       /* Multicast i/f list prev and next */
	emfc_mhif_t      *mi_mhif;      /* MH interface data */
	uint32           mi_ref;        /* Ref count of updates */
	uint32           mi_data_fwd;   /* Number of MCASTs of group fwd */
} emfc_mi_t;

/*
 * Interface list managed by EMF instance. Instead of having separate lists
 * for router ports and unregistered frames forwarding ports (uffp) we use
 * one interface list. Each interface list entry has flags (infact ref count)
 * indicating the entry type. An interface can be of type rtport, uffp or
 * both. As a general rule IGMP frames are forwarded on rtport type interfaces.
 * Unregistered data frames are forwareded on to rtport and uffp interfaces.
 */
typedef struct emfc_iflist
{
	struct emfc_iflist *next;       /* Interface list next */
	void               *ifp;        /* Interface pointer */
	uint32             uffp_ref;    /* Ref count for UFFP type */
	uint32             rtport_ref;  /* Ref count for rtport type */
} emfc_iflist_t;

/*
 * Multicast Forwarding Layer Information
 */
typedef struct emfc_info
{
	clist_head_t     emfc_list;     /* EMFC instance list prev and next */
	int8             inst_id[16];   /* EMFC instance identifier */
	                                /* Multicast forwarding database */
	clist_head_t     mgrp_fdb[MFDB_HASHT_SIZE];
	emfc_mhif_t      *mhif_head;    /* Multicast Host interface list */
	uint32           mgrp_cache_ip; /* Multicast Group IP Addr in cache */
	emfc_mgrp_t      *mgrp_cache;   /* Multicast Group cached entry */
	osl_lock_t       fdb_lock;      /* Lock for FDB access */
	void             *osh;          /* OS Layer handle */
	void             *emfi;         /* OS Specific MFL data */
	emf_stats_t      stats;         /* Multicast frames statistics */
	emfc_snooper_t	 *snooper;	/* IGMP Snooper data */
	emfc_wrapper_t   wrapper;       /* EMFC wrapper info  */
	bool		 emf_enable;	/* Enable/Disable EMF */
	bool		 mc_data_ind;	/* Indicate mcast data frames */
	osl_lock_t       iflist_lock;   /* Lock for UFFP list access */
	emfc_iflist_t    *iflist_head;  /* UFFP list head */
} emfc_info_t;

static emfc_mgrp_t *emfc_mfdb_group_find(emfc_info_t *emfc, uint32 mgrp_ip);

#endif /* _EMFC_H_ */
