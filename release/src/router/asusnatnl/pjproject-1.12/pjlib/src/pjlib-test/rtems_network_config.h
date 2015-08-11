/* $Id: rtems_network_config.h 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

/*
 * Thanks Zetron, Inc and Phil Torre <ptorre@zetron.com> for donating PJLIB
 * port to RTEMS.
 */

/*
 * Network configuration
 * 
 ************************************************************
 * EDIT THIS FILE TO REFLECT YOUR NETWORK CONFIGURATION     *
 * BEFORE RUNNING ANY RTEMS PROGRAMS WHICH USE THE NETWORK! * 
 ************************************************************
 *
 */

#ifndef _RTEMS_NETWORKCONFIG_H_
#define _RTEMS_NETWORKCONFIG_H_


#define DEFAULT_IP_ADDRESS_STRING "192.168.0.2"
#define DEFAULT_NETMASK_STRING    "255.255.255.0"
#define DEFAULT_GATEWAY_STRING    "192.168.0.1"




#ifndef RTEMS_BSP_NETWORK_DRIVER_NAME
#warning "RTEMS_BSP_NETWORK_DRIVER_NAME is not defined"
#define RTEMS_BSP_NETWORK_DRIVER_NAME "no_network1"
#endif

#ifndef RTEMS_BSP_NETWORK_DRIVER_ATTACH
#warning "RTEMS_BSP_NETWORK_DRIVER_ATTACH is not defined"
#define RTEMS_BSP_NETWORK_DRIVER_ATTACH 0
#endif

#define NETWORK_STACK_PRIORITY 128
/* #define RTEMS_USE_BOOTP */

/* #define RTEMS_USE_LOOPBACK */

#include <bsp.h>

/*
 * Define RTEMS_SET_ETHERNET_ADDRESS if you want to specify the
 * Ethernet address here.  If RTEMS_SET_ETHERNET_ADDRESS is not
 * defined the driver will choose an address.
 */
// NOTE:  The address below is a dummy address that should only ever
// be used for testing on a private network.  DO NOT LET A PRODUCT
// CONTAINING THIS ETHERNET ADDRESS OUT INTO THE FIELD!
//#define RTEMS_SET_ETHERNET_ADDRESS
#if (defined (RTEMS_SET_ETHERNET_ADDRESS))
static char ethernet_address[6] = { 0x00, 0x80, 0x7F, 0x22, 0x61, 0x77 };
#endif

#define RTEMS_USE_LOOPBACK 
#ifdef RTEMS_USE_LOOPBACK 
/*
 * Loopback interface
 */
extern int rtems_bsdnet_loopattach(struct rtems_bsdnet_ifconfig* dummy, int unused);
static struct rtems_bsdnet_ifconfig loopback_config = {
	"lo0",				/* name */
	rtems_bsdnet_loopattach,	/* attach function */
	NULL,				/* link to next interface */
	"127.0.0.1",			/* IP address */
	"255.0.0.0",			/* IP net mask */
};
#endif

/*
 * Default network interface
 */
static struct rtems_bsdnet_ifconfig netdriver_config = {
	RTEMS_BSP_NETWORK_DRIVER_NAME,		/* name */
	RTEMS_BSP_NETWORK_DRIVER_ATTACH,	/* attach function */

#ifdef RTEMS_USE_LOOPBACK 
	&loopback_config,		/* link to next interface */
#else
	NULL,				/* No more interfaces */
#endif

#if (defined (RTEMS_USE_BOOTP))
	NULL,				/* BOOTP supplies IP address */
	NULL,				/* BOOTP supplies IP net mask */
#else
	"192.168.0.33",			/* IP address */
	"255.255.255.0",		/* IP net mask */
#endif /* !RTEMS_USE_BOOTP */

#if (defined (RTEMS_SET_ETHERNET_ADDRESS))
	ethernet_address,               /* Ethernet hardware address */
#else
	NULL,                           /* Driver supplies hardware address */
#endif
	0				/* Use default driver parameters */
};

/*
 * Network configuration
 */
struct rtems_bsdnet_config rtems_bsdnet_config = {
	&netdriver_config,

#if (defined (RTEMS_USE_BOOTP))
	rtems_bsdnet_do_bootp,
#else
	NULL,
#endif

	NETWORK_STACK_PRIORITY,		/* Default network task priority */
	1048576,			/* Default mbuf capacity */
	1048576,			/* Default mbuf cluster capacity */

#if (!defined (RTEMS_USE_BOOTP))
	"testnode",		/* Host name */
	"example.org",		/* Domain name */
	"192.168.6.9",		/* Gateway */
	"192.168.7.41",		/* Log host */
	{"198.137.231.1" },	/* Name server(s) */
	{"207.202.190.162" },	/* NTP server(s) */
#endif /* !RTEMS_USE_BOOTP */

};

#endif	/* _RTEMS_NETWORKCONFIG_H_ */

