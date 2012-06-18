/*
 * Required functions exported by the port-specific (os-dependent) driver
 * to common (os-independent) driver code.
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 * $Id: et_export.h,v 1.14 2008-07-25 02:41:40 Exp $
 */

#ifndef _et_export_h_
#define _et_export_h_

/* misc callbacks */
extern void et_init(void *et, uint options);
extern void et_reset(void *et);
extern void et_link_up(void *et);
extern void et_link_down(void *et);
extern int et_up(void *et);
extern int et_down(void *et, int reset);
extern void et_dump(void *et, struct bcmstrbuf *b);
extern void et_intrson(void *et);

/* for BCM5222 dual-phy shared mdio contortion */
extern void *et_phyfind(void *et, uint coreunit);
extern uint16 et_phyrd(void *et, uint phyaddr, uint reg);
extern void et_phywr(void *et, uint reg, uint phyaddr, uint16 val);

#endif	/* _et_export_h_ */
