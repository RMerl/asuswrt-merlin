/*
 * $Id: $
 *
 * Header functions for UPnP discovery
 *
 * Copyright (C) 2007 Ron Pedde (ron@pedde.com)
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

/**
 * This could probably be as easily (or better?) done via some
 * UPnP library, but I don't see any that are cross-platform to
 * the variety of platforms I want to port to, not to mention that
 * it's just some simple UDP broadcasts.
 */


#ifndef _UPNP_H_
#define _UPNP_H_

#define UPNP_AD_DEVICE   1
#define UPNP_AD_SERVICE  2
#define UPNP_AD_ROOT     3
#define UPNP_AD_UUID     4

extern int upnp_init(void);
extern int upnp_deinit(void);
extern void upnp_add_packet(char *group_id, int type, char *location,
                            char *namespace, char *name, int version,
                            char *body);
extern char *upnp_uuid();

#endif /* _UPNP_H_ */
