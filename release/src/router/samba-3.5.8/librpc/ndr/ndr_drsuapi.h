/*
   Unix SMB/CIFS implementation.

   routines for printing some linked list structs in DRSUAPI

   Copyright (C) Stefan (metze) Metzmacher 2005-2006

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

#ifndef _LIBRPC_NDR_NDR_DRSUAPI_H
#define _LIBRPC_NDR_NDR_DRSUAPI_H

void ndr_print_drsuapi_DsReplicaObjectListItem(struct ndr_print *ndr, const char *name,
					       const struct drsuapi_DsReplicaObjectListItem *r);

void ndr_print_drsuapi_DsReplicaObjectListItemEx(struct ndr_print *ndr, const char *name,
						 const struct drsuapi_DsReplicaObjectListItemEx *r);

enum ndr_err_code ndr_push_drsuapi_DsReplicaOID(struct ndr_push *ndr, int ndr_flags, const struct drsuapi_DsReplicaOID *r);
enum ndr_err_code ndr_pull_drsuapi_DsReplicaOID(struct ndr_pull *ndr, int ndr_flags, struct drsuapi_DsReplicaOID *r);
size_t ndr_size_drsuapi_DsReplicaOID_oid(const char *oid, int flags);

size_t ndr_size_drsuapi_DsReplicaObjectIdentifier3Binary_without_Binary(const struct drsuapi_DsReplicaObjectIdentifier3Binary *r, struct smb_iconv_convenience *ic, int flags);

#endif /* _LIBRPC_NDR_NDR_DRSUAPI_H */
