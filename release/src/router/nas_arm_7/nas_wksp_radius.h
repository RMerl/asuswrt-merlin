/*
 * Radius support for NAS workspace
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 * $Id: nas_wksp_radius.h 241182 2011-02-17 21:50:03Z $:
 */

#ifndef _NAS_WKSP_RADIUS_H_
#define _NAS_WKSP_RADIUS_H_

#ifdef NAS_RADIUS
/* open connection to receive radius messages */
extern int nas_wksp_open_radius(nas_wksp_t *nwksp);
extern void nas_wksp_close_radius(nas_wksp_t *nwksp);

extern int nas_radius_open(nas_wksp_t *nwksp, nas_wpa_cb_t *nwcb);
extern void nas_radius_close(nas_wksp_t *nwksp, nas_wpa_cb_t *nwcb);

extern int nas_radius_send_packet(nas_t *nas, radius_header_t *radius, int length);

#define NAS_WKSP_OPEN_RADIUS(wksp)  nas_wksp_open_radius(wksp)
#define NAS_WKSP_CLOSE_RADIUS(wksp) nas_wksp_close_radius(wksp)

#define NAS_RADIUS_OPEN(nwksp, nwcb) nas_radius_open(nwksp, nwcb)
#define NAS_RADIUS_CLOSE(nwksp, nwcb) nas_radius_close(nwksp, nwcb)

#define NAS_RADIUS_SEND_PACKET(nas, radius, length)	nas_radius_send_packet(nas, radius, length)
#else
#define NAS_WKSP_OPEN_RADIUS(wksp) (-1)
#define NAS_WKSP_CLOSE_RADIUS(wksp)

#define NAS_RADIUS_OPEN(nwksp, nwcb) (-1)
#define NAS_RADIUS_CLOSE(nwksp, nwcb)

#define NAS_RADIUS_SEND_PACKET(nas, radius, length)	(-1)
#endif /* NAS_RADIUS */

#endif /* !defined(_NAS_WKSP_RADIUS_H_) */
