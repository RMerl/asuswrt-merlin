/* Router advertisement
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

/* Router advertisement prefix. */
struct rtadv_prefix
{
  /* Prefix to be advertised. */
  struct prefix prefix;
  
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
};

void rtadv_config_write (struct vty *, struct interface *);

#endif /* _ZEBRA_RTADV_H */
