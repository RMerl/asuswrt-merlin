/*
   Unix SMB/CIFS implementation.

   helper routines for FRSRPC marshalling

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

#ifndef _LIBRPC_NDR_NDR_FRSRPC_H
#define _LIBRPC_NDR_NDR_FRSRPC_H

enum ndr_err_code ndr_push_frsrpc_CommPktChunkCtr(struct ndr_push *ndr,
					int ndr_flags,
					const struct frsrpc_CommPktChunkCtr *r);
enum ndr_err_code ndr_pull_frsrpc_CommPktChunkCtr(struct ndr_pull *ndr,
					int ndr_flags,
					struct frsrpc_CommPktChunkCtr *r);
size_t ndr_size_frsrpc_CommPktChunkCtr(const struct frsrpc_CommPktChunkCtr *r,
				       int flags);

#endif /* _LIBRPC_NDR_NDR_FRSRPC_H */
