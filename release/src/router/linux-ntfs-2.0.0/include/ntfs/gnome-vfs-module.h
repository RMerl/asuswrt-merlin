/*
 * gnome-vfs-module.h - Exports for Gnome-VFS init/shutdown implementation of
 *			interface to libntfs. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2003 Jan Kratochvil <project-captive@jankratochvil.net>
 * Copyright (c) 2000-2004 Anton Altaparmakov
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
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _NTFS_GNOME_VFS_MODULE_H
#define _NTFS_GNOME_VFS_MODULE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

G_BEGIN_DECLS

G_LOCK_EXTERN(libntfs);

#define libntfs_newn(objp, n)	((objp) = (typeof(objp))g_new(typeof(*(objp)), (n)))
#define libntfs_new(objp)	(libntfs_newn((objp), 1))
#define LIBNTFS_MEMZERO(objp)	(memset((objp), 0, sizeof(*(objp))))

G_END_DECLS

#endif /* _NTFS_GNOME_VFS_MODULE_H */

