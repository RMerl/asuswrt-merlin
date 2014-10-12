/*
   Unix SMB/CIFS mplementation.

   DSDB replication service - DsReplica{Add,Del,Mod} handling

   Copyright (C) Andrew Tridgell 2010

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

#include "includes.h"
#include "ldb_module.h"
#include "dsdb/samdb/samdb.h"
#include "smbd/service.h"
#include "dsdb/repl/drepl_service.h"
#include "param/param.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"

/*
  implement DsReplicaAdd (forwarded from DRS server)
 */
NTSTATUS drepl_replica_add(struct dreplsrv_service *service,
			   struct drsuapi_DsReplicaAdd *r)
{
	NDR_PRINT_FUNCTION_DEBUG(drsuapi_DsReplicaAdd, NDR_IN, r);
	return NT_STATUS_NOT_IMPLEMENTED;
}

/*
  implement DsReplicaDel (forwarded from DRS server)
 */
NTSTATUS drepl_replica_del(struct dreplsrv_service *service,
			   struct drsuapi_DsReplicaDel *r)
{
	NDR_PRINT_FUNCTION_DEBUG(drsuapi_DsReplicaDel, NDR_IN, r);
	return NT_STATUS_NOT_IMPLEMENTED;
}

/*
  implement DsReplicaMod (forwarded from DRS server)
 */
NTSTATUS drepl_replica_mod(struct dreplsrv_service *service,
			   struct drsuapi_DsReplicaMod *r)
{
	NDR_PRINT_FUNCTION_DEBUG(drsuapi_DsReplicaMod, NDR_IN, r);
	return NT_STATUS_NOT_IMPLEMENTED;
}
