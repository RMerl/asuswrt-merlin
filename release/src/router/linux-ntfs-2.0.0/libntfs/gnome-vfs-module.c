/*
 * gnome-vfs-module.c - Gnome-VFS init/shutdown implementation of interface to
 *			libntfs. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2003 Jan Kratochvil <project-captive@jankratochvil.net>
 * Copyright (c) 2003 Anton Altaparmakov
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

#include "config.h"

#include "gnome-vfs-method.h"
#include <libgnomevfs/gnome-vfs-module.h>
#include <glib/gmessages.h>
#include <glib/gutils.h>	/* for g_atexit() */

static void vfs_module_shutdown_atexit(void);

/**
 * vfs_module_init:
 * @method_name: FIXME
 * @args: FIXME
 *
 * FIXME
 *
 * Returns: FIXME
 */
GnomeVFSMethod *vfs_module_init(const char *method_name, const char *args)
{
	GnomeVFSMethod *libntfs_gnomevfs_method_ptr;

	g_return_val_if_fail(method_name != NULL, NULL);
	/* 'args' may be NULL if not supplied. */

	libntfs_gnomevfs_method_ptr = libntfs_gnomevfs_method_init(method_name,
			args);

	g_atexit(vfs_module_shutdown_atexit);

	return libntfs_gnomevfs_method_ptr;
}

/**
 * vfs_module_shutdown:
 */
void vfs_module_shutdown(GnomeVFSMethod *method __attribute__((unused)))
{
	/*
	 * 'method' may be NULL if we are called from
	 *  vfs_module_shutdown_atexit().
	 */

	libntfs_gnomevfs_method_shutdown();
}

static void vfs_module_shutdown_atexit(void)
{
	vfs_module_shutdown(NULL);
}

