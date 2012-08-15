/*
 * ntfsmove - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2003 Richard Russon
 *
 * This utility will move files on an NTFS volume.
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
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _NTFSMOVE_H_
#define _NTFSMOVE_H_

#include "types.h"
					/* Move files to */
#define NTFS_MOVE_LOC_START	-1000	/*   the first available space */
#define NTFS_MOVE_LOC_BEST	-1001	/*   place big enough for entire file */
#define NTFS_MOVE_LOC_END	-1002	/*   the last available space */

struct options {
	char		*device;	/* Device/File to work with */
	char		*file;		/* File to display */
	s64		 location;	/* Where to place the file */
	int		 force;		/* Override common sense */
	int		 quiet;		/* Less output */
	int		 verbose;	/* Extra output */
	int		 noaction;	/* Do not write to disk */
	int		 nodirty;	/* Do not mark volume dirty */
};

#endif /* _NTFSMOVE_H_ */


