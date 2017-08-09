/*
   Unix SMB/CIFS implementation.
   Authentication utility functions
   Copyright (C) Volker Lendecke 2010

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

#ifndef _RPC_CLIENT_UTIL_NETLOGON_H_
#define _RPC_CLIENT_UTIL_NETLOGON_H_

/* The following definitions come from rpc_client/util_netlogon.c  */

NTSTATUS copy_netr_SamBaseInfo(TALLOC_CTX *mem_ctx,
			       const struct netr_SamBaseInfo *in,
			       struct netr_SamBaseInfo *out);

#endif /* _RPC_CLIENT_UTIL_NETLOGON_H_ */
