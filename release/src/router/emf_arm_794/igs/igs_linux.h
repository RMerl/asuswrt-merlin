/*
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: igs_linux.h 344202 2012-07-11 20:09:47Z $
 */

#ifndef _IGS_LINUX_H_
#define _IGS_LINUX_H_

#define IGS_MAX_INST  16

typedef struct igs_info
{
	struct igs_info    *next;          /* Next pointer */
	int8               inst_id[IFNAMSIZ];/* IGS instance identifier */
	osl_t              *osh;           /* OS layer handle */
	void               *igsc_info;     /* IGSC Global data handle */
	struct net_device  *br_dev;        /* Bridge device for the instance */
} igs_info_t;

typedef struct igs_struct
{
	struct sock        *nl_sk;         /* Netlink socket */
	igs_info_t         *list_head;     /* IGS instance list head */
	osl_lock_t         lock;           /* IGS locking */
	int32              inst_count;     /* IGS instance count */
} igs_struct_t;

#endif /* _IGS_LINUX_H_ */
