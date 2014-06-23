/*
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: emf_linux.h 344424 2012-07-12 18:33:22Z $
 */

#ifndef _EMF_LINUX_H_
#define _EMF_LINUX_H_

#define EMF_MAX_INST          8

#ifdef EMFDBG
#define EMF_DUMP_PKT(data) \
{ \
	int32 i; \
	for (i = 0; i < 64; i++) \
		printk("%02x ", (data)[i]); \
	printk("\n"); \
}
#else /* EMFDBG */
#define EMF_DUMP_PKT(data)
#endif /* EMFDBG */

#define EMF_BRPORT_STATE(if)  (((br_port_t *)((if)->br_port))->state)

typedef struct emf_iflist
{
	struct emf_iflist  *next;        /* Next pointer */
	struct net_device  *if_ptr;      /* Interface pointer */
} emf_iflist_t;

typedef struct emf_info
{
	struct emf_info    *next;        /* Next pointer */
	int8               inst_id[16];  /* EMF Instance identifier */
	osl_t              *osh;         /* OS layer handle */
	struct net_device  *br_dev;      /* Bridge device pointer */
	struct emfc_info   *emfci;       /* EMFC Global data handle */
	uint32             hooks_reg;    /* EMF Hooks registration */
	emf_iflist_t       *iflist_head; /* EMF interfaces list */
} emf_info_t;

typedef struct emf_struct
{
	struct sock *nl_sk;              /* Netlink socket */
	emf_info_t  *list_head;          /* EMF instance list */
	osl_lock_t  lock;                /* EMF locking */
	int32       hooks_reg;           /* EMF hooks registration ref count */
	int32       inst_count;          /* EMF instance count */
} emf_struct_t;

typedef struct br_port
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36))
	/* Not used for this version of Linux */
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
	struct net_bridge	*br;
	struct net_device	*dev;
	struct list_head	list;
	/* STP */
	u8			priority;
	u8			state;
#else /* LINUX_VERSION_CODE >= 2.6.0 */
	struct br_port     *next;
	struct net_bridge  *br;
	struct net_device  *dev;
	int32              port_no;
	uint16             port_id;
	int32              state;
#endif /* LINUX_VERSION_CODE >= 2.6.0 */
} br_port_t;

#endif /* _EMF_LINUX_H_ */
