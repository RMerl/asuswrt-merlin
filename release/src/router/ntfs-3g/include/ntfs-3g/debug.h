/*
 * debug.h - Debugging output functions. Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2002-2004 Anton Altaparmakov
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _NTFS_DEBUG_H
#define _NTFS_DEBUG_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "logging.h"

struct _runlist_element;

#ifdef DEBUG
extern void ntfs_debug_runlist_dump(const struct _runlist_element *rl);
#else
static __inline__ void ntfs_debug_runlist_dump(const struct _runlist_element *rl __attribute__((unused))) {}
#endif

#define NTFS_BUG(msg)							\
{									\
	int ___i;							\
	ntfs_log_critical("Bug in %s(): %s\n", __FUNCTION__, msg);	\
	ntfs_log_debug("Forcing segmentation fault!");			\
	___i = ((int*)NULL)[1];						\
}

#endif /* defined _NTFS_DEBUG_H */
