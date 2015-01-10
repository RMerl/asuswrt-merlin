/*
 * Broadcom wireless network adapter utility functions
 *
 * Copyright 2005, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: wlutils.h,v 1.1.1.10 2005/03/07 07:31:20 kanki Exp $
 */

#ifndef _wlutils_h_
#define _wlutils_h_

#include <shared.h>

#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
#include <typedefs.h>
#include <proto/ethernet.h>
#include <wlioctl.h>

/*
 * Pass a wlioctl request to the specified interface.
 * @param	name	interface name
 * @param	cmd	WLC_GET_MAGIC <= cmd < WLC_LAST
 * @param	buf	buffer for passing in and/or receiving data
 * @param	len	length of buf
 * @return	>= 0 if successful or < 0 otherwise
 */
extern int wl_ioctl(char *name, int cmd, void *buf, int len);

/*
 * Get device type.
 * @param	name	interface name
 * @param	buf	buffer for passing in and/or receiving data
 * @param	len	length of buf
 * @return	>= 0 if successful or < 0 otherwise
 */
#define DEV_TYPE_LEN 3
extern int wl_get_dev_type(char *name, void *buf, int len);

/*
 * Get the MAC (hardware) address of the specified interface.
 * @param	name	interface name
 * @param	hwaddr	6-byte buffer for receiving address
 * @return	>= 0 if successful or < 0 otherwise
 */
extern int wl_hwaddr(char *name, unsigned char *hwaddr);

/*
 * Probe the specified interface.
 * @param	name	interface name
 * @return	>= 0 if a Broadcom wireless device or < 0 otherwise
 */
extern int wl_probe(char *name);

/*
 * Set/Get named variable.
 * @param	name	interface name
 * @param	var	variable name
 * @param	val	variable value/buffer
 * @param	len	variable value/buffer length
 * @return	success == 0, failure != 0
 */
extern int wl_set_val(char *name, char *var, void *val, int len);
extern int wl_get_val(char *name, char *var, void *val, int len);
extern int wl_set_int(char *name, char *var, int val);
extern int wl_get_int(char *name, char *var, int *val);

static inline int wl_phytype_n(int phytype)
{
	return (phytype == WLC_PHY_TYPE_N)
#ifdef WLC_PHY_TYPE_SSN
		|| (phytype == WLC_PHY_TYPE_SSN)
#endif
#ifdef WLC_PHY_TYPE_LCN
		|| (phytype == WLC_PHY_TYPE_LCN)
#endif
	;
}

#if 1
/*
 * Set/Get named variable.
 * @param	ifname		interface name
 * @param	iovar		variable name
 * @param	param		input param value/buffer
 * @param	paramlen	input param value/buffer length
 * @param	bufptr		io buffer
 * @param	buflen		io buffer length
 * @param	val		val or val pointer for int routines
 * @return	success == 0, failure != 0
 */
extern int wl_iovar_setbuf(char *ifname, char *iovar, void *param, int paramlen, void *bufptr, int buflen);
extern int wl_iovar_getbuf(char *ifname, char *iovar, void *param, int paramlen, void *bufptr, int buflen);
extern int wl_iovar_set(char *ifname, char *iovar, void *param, int paramlen);
extern int wl_iovar_get(char *ifname, char *iovar, void *bufptr, int buflen);
extern int wl_iovar_setint(char *ifname, char *iovar, int val);
extern int wl_iovar_getint(char *ifname, char *iovar, int *val);

/*
 * Set/Get named variable indexed by BSS Configuration
 * @param	ifname		interface name
 * @param	iovar		variable name
 * @param	bssidx		bsscfg index
 * @param	param		input param value/buffer
 * @param	paramlen	input param value/buffer length
 * @param	bufptr		io buffer
 * @param	buflen		io buffer length
 * @param	val		val or val pointer for int routines
 * @return	success == 0, failure != 0
 */
extern int wl_bssiovar_setbuf(char *ifname, char *iovar, int bssidx, void *param, int paramlen, void *bufptr, int buflen);
extern int wl_bssiovar_getbuf(char *ifname, char *iovar, int bssidx, void *param, int paramlen, void *bufptr, int buflen);
extern int wl_bssiovar_get(char *ifname, char *iovar, int bssidx, void *outbuf, int len);
extern int wl_bssiovar_set(char *ifname, char *iovar, int bssidx, void *param, int paramlen);
extern int wl_bssiovar_setint(char *ifname, char *iovar, int bssidx, int val);
#endif

extern char * wl_ether_etoa(const struct ether_addr *n);

#endif /* CONFIG_BCMWL5 */
#endif /* _wlutils_h_ */
