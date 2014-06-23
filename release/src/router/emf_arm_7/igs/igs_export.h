/*
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: igs_export.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _IGS_EXPORT_H_
#define _IGS_EXPORT_H_

struct igs_info;

/*
 * Description: This function is called by IGS Common code when it wants
 *              to send a multicast packet on to all the LAN ports. It
 *              allocates the native OS packet buffer, adds mac header and
 *              forwards a copy of frame on to LAN ports.
 *
 * Input:       igs_info - IGS instance information.
 *              ip       - Pointer to the buffer containing the frame to
 *                         send. Frame starts with IP header.
 *              length   - Length of the buffer.
 *              mgrp_ip  - Multicast destination address.
 *
 * Return:      SUCCESS or FAILURE
 */
extern int32 igs_broadcast(struct igs_info *igs_info, uint8 *ip, uint32 length, uint32 mgrp_ip);

#endif /* _IGS_EXPORT_H_ */
