/**
 * version.c - Info about the NTFS library.  Part of the Linux-NTFS project.
 *
 * Copyright (c) 2005 Anton Altaparmakov
 * Copyright (c) 2005 Richard Russon
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "version.h"

#ifdef LTVERSION_LIBNTFS
#define LIBNTFS_VERSION_STRING LTVERSION_LIBNTFS
#else
#define LIBNTFS_VERSION_STRING "unknown"
#endif

static const char *libntfs_version_string = LIBNTFS_VERSION_STRING;

/**
 * ntfs_libntfs_version - query version number of the ntfs library libntfs
 *
 * Returns pointer to a text string representing the version of libntfs.
 */
const char *ntfs_libntfs_version(void)
{
	return libntfs_version_string;
}
