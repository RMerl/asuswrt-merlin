/*
 * interface functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * Copyright 2011, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */

#ifndef __RC_INTERFACE_H__
#define __RC_INTERFACE_H__

/* Phy port logical indexes */
enum {
	SWPORT_WAN = 0,
	SWPORT_LAN1,
	SWPORT_LAN2,
	SWPORT_LAN3,
	SWPORT_LAN4,
	SWPORT_CPU,
	SWPORT_COUNT,
};

/* Default switch config indexes */
enum {
	SWCFG_DEFAULT = 0,	/* 0 Default, ALWAYS first */
	SWCFG_STB1,		/* 1 STB on LAN1 */
	SWCFG_STB2,		/* 2 STB on LAN2 */
	SWCFG_STB3,		/* 3 STB on LAN3 */
	SWCFG_STB4,		/* 4 STB on LAN4 */
	SWCFG_STB12,		/* 5 STB on LAN1&2 */
	SWCFG_STB34,		/* 6 STB on LAN3&4 */
	SWCFG_BRIDGE,		/* 7 Bridge */
	SWCFG_PSTA,		/* 8 PSta */
	WAN1PORT1,
	WAN1PORT2,
	WAN1PORT3,
	WAN1PORT4,
	SWCFG_COUNT
};

/* Phy port logical mask */
enum {
	SW_WAN = (1U << SWPORT_WAN),
	SW_L1  = (1U << SWPORT_LAN1),
	SW_L2  = (1U << SWPORT_LAN2),
	SW_L3  = (1U << SWPORT_LAN3),
	SW_L4  = (1U << SWPORT_LAN4),
	SW_CPU = (1U << SWPORT_CPU),
};

#define SWCFG_BUFSIZE sizeof("0u ")*SWPORT_COUNT /* max size of "0t 1t... CPUt" */
#define SWCFG_INIT(cfg, lan, wan) [cfg] = { .lanmask = lan, .wanmask = wan }

/* Generates switch ports config string
 * char *buf	- pointer to buffer[SWCFG_BUFSIZE] for result string
 * int *ports	- array of phy port numbers in order of SWPORT_ enum, eg. {W,L1,L2,L3,L4,C}
 * int swmask	- mask of logical ports to return
 * char *cputag	- NULL: return config string excluding CPU port
 *		  PSTR: return config string including CPU port, tagged with PSTR, eg. 8|8t|8*
 */
extern void _switch_gen_config(char *buf, const int ports[SWPORT_COUNT], int swmask, char *cputag, int wan);

/* Generates switch ports config string
 * char *buf	- pointer to buffer[SWCFG_BUFSIZE] for result string
 * int *ports	- array of phy port numbers in order of SWPORT_ enum, eg. {W,L1,L2,L3,L4,C}
 * int index	- config index in default sw_config array
 * int wan	- 0: return config string for lan ports
 *		  1: return config string for wan ports
 * char *cputag	- NULL: return config string excluding CPU port
 *		  PSTR: return config string including CPU port, tagged with PSTR, eg. 8|8t|8*
 */
extern void switch_gen_config(char *buf, const int ports[SWPORT_COUNT], int index, int wan, char *cputag);

extern void gen_lan_ports(char *buf, const int sample[SWPORT_COUNT], int index, int index1, char *cputag);

#endif
