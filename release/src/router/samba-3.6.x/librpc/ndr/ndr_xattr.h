/*
   Unix SMB/CIFS implementation.

   helper routines for XATTR marshalling

   Copyright (C) Stefan (metze) Metzmacher 2009

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _LIBRPC_NDR_NDR_XATTR_H
#define _LIBRPC_NDR_NDR_XATTR_H

_PUBLIC_ enum ndr_err_code ndr_push_xattr_DOSATTRIB(struct ndr_push *ndr,
						int ndr_flags,
						const struct xattr_DOSATTRIB *r);

_PUBLIC_ enum ndr_err_code ndr_pull_xattr_DOSATTRIB(struct ndr_pull *ndr,
						int ndr_flags,
						struct xattr_DOSATTRIB *r);

_PUBLIC_ void ndr_print_xattr_DOSATTRIB(struct ndr_print *ndr,
					const char *name,
					const struct xattr_DOSATTRIB *r);

#endif /* _LIBRPC_NDR_NDR_XATTR_H */
