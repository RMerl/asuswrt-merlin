/*
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: igsu_linux.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _IGSU_LINUX_H_
#define _IGSU_LINUX_H_

#define NETLINK_IGSC 18

struct igs_cfg_request;
extern int igs_cfg_request_send(struct igs_cfg_request *buffer, int length);

#endif /* _IGSU_LINUX_H_ */
