/*
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
 */
/* libbb_udhcp.h - busybox compatability wrapper */

#ifndef _LIBBB_UDHCP_H
#define _LIBBB_UDHCP_H

#ifdef BB_VER
#include "libbb.h"

#ifdef CONFIG_FEATURE_UDHCP_SYSLOG
#define SYSLOG
#endif

#ifdef CONFIG_FEATURE_UDHCP_DEBUG
#define DEBUG
#endif

#define COMBINED_BINARY
#define VERSION "0.9.8-asus"

#else /* ! BB_VER */

#define TRUE			1
#define FALSE			0

#define xmalloc malloc

#endif /* BB_VER */

#endif /* _LIBBB_UDHCP_H */
