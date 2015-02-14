/* ipt_geoip.h header file for libipt_geoip.c and ipt_geoip.c
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (c) 2004 Cookinglinux
 */
#ifndef _IPT_GEOIP_H
#define _IPT_GEOIP_H

#include <linux/netfilter/xt_geoip.h>

#define IPT_GEOIP_SRC	XT_GEOIP_SRC
#define IPT_GEOIP_DST	XT_GEOIP_DST
#define IPT_GEOIP_INV	XT_GEOIP_INV
#define IPT_GEOIP_MAX	XT_GEOIP_MAX

#define ipt_geoip_info	xt_geoip_match_info

#endif

/* End of ipt_geoip.h */
