/*
 * parse_opt.h -- mount option string parsing helpers
 *
 * Copyright (C) 2007 Oracle.  All rights reserved.
 * Copyright (C) 2007 Chuck Lever <chuck.lever@oracle.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 *
 */

#ifndef _NFS_UTILS_PARSE_OPT_H
#define _NFS_UTILS_PARSE_OPT_H

typedef enum {
	PO_FAILED = 0,
	PO_SUCCEEDED = 1,
} po_return_t;

typedef enum {
	PO_NOT_FOUND = 0,
	PO_FOUND = 1,
	PO_BAD_VALUE = 2,
} po_found_t;

struct mount_options;

struct mount_options *	po_split(char *);
void			po_replace(struct mount_options *,
				   struct mount_options *);
po_return_t		po_join(struct mount_options *, char **);

po_return_t		po_append(struct mount_options *, char *);
po_found_t		po_contains(struct mount_options *, char *);
char *			po_get(struct mount_options *, char *);
po_found_t		po_get_numeric(struct mount_options *,
					char *, long *);
int			po_rightmost(struct mount_options *,
					const char *keys[]);
po_found_t		po_remove_all(struct mount_options *, char *);
void			po_destroy(struct mount_options *);

#endif	/* _NFS_UTILS_PARSE_OPT_H */
