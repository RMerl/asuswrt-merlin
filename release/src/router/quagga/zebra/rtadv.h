/* Router advertisement
 * Copyright (C) 2005 6WIND <jean-mickael.guerin@6wind.com>
 * Copyright (C) 1999 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#ifndef _ZEBRA_RTADV_H
#define _ZEBRA_RTADV_H

#include "vty.h"
#include "zebra/interface.h"

/* NB: RTADV is defined in zebra/interface.h above */
#ifdef RTADV

/* Router advertisement prefix. */
struct rtadv_prefix
{
  /* Prefix to be advertised. */
  struct prefix_ipv6 prefix;
  
  /* The value to be placed in the Valid Lifetime in the Prefix */
  u_int32_t AdvValidLifetime;
#define RTADV_VALID_LIFETIME 2592000

  /* The value to be placed in the on-link flag */
  int AdvOnLinkFlag;

  /* The value to be placed in the Preferred Lifetime in the Prefix
     Information option, in seconds.*/
  u_int32_t AdvPreferredLifetime;
#define RTADV_PREFERRED_LIFETIME 604800

  /* The value to be placed in the Autonomous Flag. */
  int AdvAutonomousFlag;

  /* The value to be placed in the Router Address Flag [RFC6275 7.2]. */
  int AdvRouterAddressFlag;
#ifndef ND_OPT_PI_FLAG_RADDR
#define ND_OPT_PI_FLAG_RADDR         0x20
#endif

};

extern void rtadv_config_write (struct vty *, struct interface *);
extern void rtadv_init (void);

/* RFC4584 Extension to Sockets API for Mobile IPv6 */

#ifndef ND_OPT_ADV_INTERVAL
#define ND_OPT_ADV_INTERVAL	7   /* Adv Interval Option */
#endif
#ifndef ND_OPT_HA_INFORMATION
#define ND_OPT_HA_INFORMATION	8   /* HA Information Option */
#endif

#ifndef HAVE_STRUCT_ND_OPT_ADV_INTERVAL
struct nd_opt_adv_interval {   /* Advertisement interval option */
        uint8_t        nd_opt_ai_type;
        uint8_t        nd_opt_ai_len;
        uint16_t       nd_opt_ai_reserved;
        uint32_t       nd_opt_ai_interval;
} __attribute__((__packed__));
#else
#ifndef HAVE_STRUCT_ND_OPT_ADV_INTERVAL_ND_OPT_AI_TYPE
/* fields may have to be renamed */
#define nd_opt_ai_type		nd_opt_adv_interval_type
#define nd_opt_ai_len		nd_opt_adv_interval_len
#define nd_opt_ai_reserved	nd_opt_adv_interval_reserved
#define nd_opt_ai_interval	nd_opt_adv_interval_ival
#endif
#endif

#ifndef HAVE_STRUCT_ND_OPT_HOMEAGENT_INFO
struct nd_opt_homeagent_info {  /* Home Agent info */
        u_int8_t        nd_opt_hai_type;
        u_int8_t        nd_opt_hai_len;
        u_int16_t       nd_opt_hai_reserved;
        u_int16_t       nd_opt_hai_preference;
        u_int16_t       nd_opt_hai_lifetime;
} __attribute__((__packed__));
#endif

extern const char *rtadv_pref_strs[];

#endif /* RTADV */

#endif /* _ZEBRA_RTADV_H */
