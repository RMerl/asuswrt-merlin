/*
 * gnome-vfs-method.h - Export for Gnome-VFS init/shutdown implementation of
 *			interface to libntfs. Par of the Linux-NTFS project.
 *
 * Copyright (c) 2002-2003 Jan Kratochvil <project-captive@jankratochvil.net>
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

#ifndef _NTFS_GNOME_VFS_METHOD_H
#define _NTFS_GNOME_VFS_METHOD_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libgnomevfs/gnome-vfs-method.h>

G_BEGIN_DECLS

GnomeVFSMethod *libntfs_gnomevfs_method_init(const gchar *method_name,
		const gchar *args);

void libntfs_gnomevfs_method_shutdown(void);

G_END_DECLS

#endif /* _NTFS_GNOME_VFS_METHOD_H */

