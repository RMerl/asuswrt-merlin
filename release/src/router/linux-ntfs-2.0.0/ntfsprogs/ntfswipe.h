/*
 * ntfswipe - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2002 Richard Russon
 *
 * This utility will overwrite unused space on an NTFS volume.
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

#ifndef _NTFSWIPE_H_
#define _NTFSWIPE_H_

#include "types.h"

enum action {
	act_info,
	act_test,
	act_wipe,
};

struct options {
	char	*device;	/* Device/File to work with */
	int	 info;		/* Show volume info */
	int	 force;		/* Override common sense */
	int	 quiet;		/* Less output */
	int	 verbose;	/* Extra output */
	int	 noaction;	/* Do not write to disk */
	int	 count;		/* Number of iterations */
	int	*bytes;		/* List of overwrite characters */
	int	 directory;	/* Wipe directory indexes */
	int	 logfile;	/* Wipe the logfile (journal) */
	int	 mft;		/* Wipe mft slack space */
	int	 pagefile;	/* Wipe pagefile (swap space) */
	int	 tails;		/* Wipe file tails */
	int	 unused;	/* Wipe unused clusters */
};

#endif /* _NTFSWIPE_H_ */

