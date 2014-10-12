/*
   Unix SMB/CIFS implementation.

   Extract the user/system database from a remote SamSync server

   Copyright (C) Guenther Deschner <gd@samba.org> 2008

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

#ifndef __SAMSYNC_SAMSYNC_H__ 
#define __SAMSYNC_SAMSYNC_H__ 

struct netlogon_creds_CredentialState;

/**
 * Fix up the delta, dealing with encryption issues so that the final
 * callback need only do the printing or application logic
 */
NTSTATUS samsync_fix_delta(TALLOC_CTX *mem_ctx,
			   struct netlogon_creds_CredentialState *creds,
			   enum netr_SamDatabaseID database_id,
			   struct netr_DELTA_ENUM *delta);

#endif /* __SAMSYNC_SAMSYNC_H__ */
