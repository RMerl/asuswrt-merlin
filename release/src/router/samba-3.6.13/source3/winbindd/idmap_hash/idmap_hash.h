/*
 *  lwopen.h
 *
 *  Copyright (C) Gerald Carter  <jerry@samba.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _LWOPEN_H
#define _LWOPEN_H

#define BAIL_ON_NTSTATUS_ERROR(x)	   \
	do {				   \
		if (!NT_STATUS_IS_OK(x)) { \
			DEBUG(10,("Failed! (%s)\n", nt_errstr(x)));	\
			goto done;	   \
		}			   \
	}				   \
	while (0);			   \

#define BAIL_ON_PTR_NT_ERROR(p, x)			\
	do {						\
		if ((p) == NULL ) {			\
			DEBUG(10,("NULL pointer!\n"));	\
			x = NT_STATUS_NO_MEMORY;	\
			goto done;			\
		} else {				\
			x = NT_STATUS_OK;		\
		}					\
	} while (0);

#define PRINT_NTSTATUS_ERROR(x, hdr, level)				\
	do {								\
		if (!NT_STATUS_IS_OK(x)) {				\
			DEBUG(level,("Likewise Open ("hdr"): %s\n", nt_errstr(x))); \
		}							\
	} while(0);


NTSTATUS mapfile_lookup_key(TALLOC_CTX *ctx,
			    const char *value,
			    char **key);

NTSTATUS mapfile_lookup_value(TALLOC_CTX *ctx,
			      const char *key,
			      char **value);

#endif /* _LWOPEN_H */
