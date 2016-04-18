/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *
 *  Copyright (C) Gerald (Jerry) Carter             2005.
 *  Copyright (C) Guenther Deschner                 2008,2009.
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
 */

#include "includes.h"
#include "ntdomain.h"
#include "../librpc/gen_ndr/srv_ntsvcs.h"
#include "services/svc_winreg_glue.h"
#include "../libcli/registry/util_reg.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

/********************************************************************
********************************************************************/

static char* get_device_path(TALLOC_CTX *mem_ctx, const char *device )
{
	return talloc_asprintf(mem_ctx, "ROOT\\Legacy_%s\\0000", device);
}

/********************************************************************
********************************************************************/

WERROR _PNP_GetVersion(struct pipes_struct *p,
		       struct PNP_GetVersion *r)
{
	*r->out.version = 0x0400;      /* no idea what this means */

	return WERR_OK;
}

/********************************************************************
********************************************************************/

WERROR _PNP_GetDeviceListSize(struct pipes_struct *p,
			      struct PNP_GetDeviceListSize *r)
{
	char *devicepath;

	if ((r->in.flags & CM_GETIDLIST_FILTER_SERVICE) &&
	    (!r->in.devicename)) {
		return WERR_CM_INVALID_POINTER;
	}

	if (!(devicepath = get_device_path(p->mem_ctx, r->in.devicename))) {
		return WERR_NOMEM;
	}

	*r->out.size = strlen(devicepath) + 2;

	TALLOC_FREE(devicepath);

	return WERR_OK;
}

/****************************************************************
 _PNP_GetDeviceList
****************************************************************/

WERROR _PNP_GetDeviceList(struct pipes_struct *p,
			  struct PNP_GetDeviceList *r)
{
	char *devicepath;
	uint32_t size = 0;
	const char **multi_sz = NULL;
	DATA_BLOB blob;

	if ((r->in.flags & CM_GETIDLIST_FILTER_SERVICE) &&
	    (!r->in.filter)) {
		return WERR_CM_INVALID_POINTER;
	}

	if (!(devicepath = get_device_path(p->mem_ctx, r->in.filter))) {
		return WERR_NOMEM;
	}

	size = strlen(devicepath) + 2;

	if (*r->in.length < size) {
		return WERR_CM_BUFFER_SMALL;
	}

	multi_sz = talloc_zero_array(p->mem_ctx, const char *, 2);
	if (!multi_sz) {
		return WERR_NOMEM;
	}

	multi_sz[0] = devicepath;

	if (!push_reg_multi_sz(multi_sz, &blob, multi_sz)) {
		return WERR_NOMEM;
	}

	if (*r->in.length < blob.length/2) {
		return WERR_CM_BUFFER_SMALL;
	}

	memcpy(r->out.buffer, blob.data, blob.length);

	return WERR_OK;
}

/********************************************************************
_PNP_GetDeviceRegProp
********************************************************************/

WERROR _PNP_GetDeviceRegProp(struct pipes_struct *p,
			     struct PNP_GetDeviceRegProp *r)
{
	char *ptr;
	const char *result;
	DATA_BLOB blob;
	TALLOC_CTX *mem_ctx = NULL;

	switch( r->in.property ) {
	case DEV_REGPROP_DESC:

		/* just parse the service name from the device path and then
		   lookup the display name */
		if ( !(ptr = strrchr_m( r->in.devicepath, '\\' )) )
			return WERR_GENERAL_FAILURE;
		*ptr = '\0';

		if ( !(ptr = strrchr_m( r->in.devicepath, '_' )) )
			return WERR_GENERAL_FAILURE;
		ptr++;

		mem_ctx = talloc_stackframe();

		result = svcctl_lookup_dispname(mem_ctx,
						p->msg_ctx,
						p->session_info,
						ptr);
		if (result == NULL) {
			return WERR_GENERAL_FAILURE;
		}

		if (!push_reg_sz(mem_ctx, &blob, result)) {
			talloc_free(mem_ctx);
			return WERR_GENERAL_FAILURE;
		}

		if (*r->in.buffer_size < blob.length) {
			*r->out.needed = blob.length;
			*r->out.buffer_size = 0;
			talloc_free(mem_ctx);
			return WERR_CM_BUFFER_SMALL;
		}

		r->out.buffer = (uint8_t *)talloc_memdup(p->mem_ctx, blob.data, blob.length);
		talloc_free(mem_ctx);
		if (!r->out.buffer) {
			return WERR_NOMEM;
		}

		*r->out.reg_data_type = REG_SZ;	/* always 1...tested using a remove device manager connection */
		*r->out.buffer_size = blob.length;
		*r->out.needed = blob.length;

		break;

	default:
		*r->out.reg_data_type = 0x00437c98; /* ??? */
		return WERR_CM_NO_SUCH_VALUE;
	}

	return WERR_OK;
}

/********************************************************************
********************************************************************/

WERROR _PNP_ValidateDeviceInstance(struct pipes_struct *p,
				   struct PNP_ValidateDeviceInstance *r)
{
	/* whatever dude */
	return WERR_OK;
}

/********************************************************************
********************************************************************/

WERROR _PNP_GetHwProfInfo(struct pipes_struct *p,
			  struct PNP_GetHwProfInfo *r)
{
	/* steal the incoming buffer */

	r->out.info = r->in.info;

	/* Take the 5th Ammentment */

	return WERR_CM_NO_MORE_HW_PROFILES;
}

/********************************************************************
********************************************************************/

WERROR _PNP_HwProfFlags(struct pipes_struct *p,
			struct PNP_HwProfFlags *r)
{
	/* just nod your head */

	return WERR_OK;
}

/****************************************************************
****************************************************************/

WERROR _PNP_Disconnect(struct pipes_struct *p,
		       struct PNP_Disconnect *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_Connect(struct pipes_struct *p,
		    struct PNP_Connect *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetGlobalState(struct pipes_struct *p,
			   struct PNP_GetGlobalState *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_InitDetection(struct pipes_struct *p,
			  struct PNP_InitDetection *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_ReportLogOn(struct pipes_struct *p,
			struct PNP_ReportLogOn *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetRootDeviceInstance(struct pipes_struct *p,
				  struct PNP_GetRootDeviceInstance *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetRelatedDeviceInstance(struct pipes_struct *p,
				     struct PNP_GetRelatedDeviceInstance *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_EnumerateSubKeys(struct pipes_struct *p,
			     struct PNP_EnumerateSubKeys *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetDepth(struct pipes_struct *p,
		     struct PNP_GetDepth *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_SetDeviceRegProp(struct pipes_struct *p,
			     struct PNP_SetDeviceRegProp *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetClassInstance(struct pipes_struct *p,
			     struct PNP_GetClassInstance *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_CreateKey(struct pipes_struct *p,
		      struct PNP_CreateKey *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_DeleteRegistryKey(struct pipes_struct *p,
			      struct PNP_DeleteRegistryKey *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetClassCount(struct pipes_struct *p,
			  struct PNP_GetClassCount *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetClassName(struct pipes_struct *p,
			 struct PNP_GetClassName *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_DeleteClassKey(struct pipes_struct *p,
			   struct PNP_DeleteClassKey *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetInterfaceDeviceAlias(struct pipes_struct *p,
				    struct PNP_GetInterfaceDeviceAlias *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetInterfaceDeviceList(struct pipes_struct *p,
				   struct PNP_GetInterfaceDeviceList *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetInterfaceDeviceListSize(struct pipes_struct *p,
				       struct PNP_GetInterfaceDeviceListSize *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_RegisterDeviceClassAssociation(struct pipes_struct *p,
					   struct PNP_RegisterDeviceClassAssociation *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_UnregisterDeviceClassAssociation(struct pipes_struct *p,
					     struct PNP_UnregisterDeviceClassAssociation *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetClassRegProp(struct pipes_struct *p,
			    struct PNP_GetClassRegProp *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_SetClassRegProp(struct pipes_struct *p,
			    struct PNP_SetClassRegProp *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_CreateDevInst(struct pipes_struct *p,
			  struct PNP_CreateDevInst *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_DeviceInstanceAction(struct pipes_struct *p,
				 struct PNP_DeviceInstanceAction *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetDeviceStatus(struct pipes_struct *p,
			    struct PNP_GetDeviceStatus *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_SetDeviceProblem(struct pipes_struct *p,
			     struct PNP_SetDeviceProblem *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_DisableDevInst(struct pipes_struct *p,
			   struct PNP_DisableDevInst *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_UninstallDevInst(struct pipes_struct *p,
			     struct PNP_UninstallDevInst *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_AddID(struct pipes_struct *p,
		  struct PNP_AddID *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_RegisterDriver(struct pipes_struct *p,
			   struct PNP_RegisterDriver *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_QueryRemove(struct pipes_struct *p,
			struct PNP_QueryRemove *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_RequestDeviceEject(struct pipes_struct *p,
			       struct PNP_RequestDeviceEject *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_IsDockStationPresent(struct pipes_struct *p,
				 struct PNP_IsDockStationPresent *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_RequestEjectPC(struct pipes_struct *p,
			   struct PNP_RequestEjectPC *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_AddEmptyLogConf(struct pipes_struct *p,
			    struct PNP_AddEmptyLogConf *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_FreeLogConf(struct pipes_struct *p,
			struct PNP_FreeLogConf *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetFirstLogConf(struct pipes_struct *p,
			    struct PNP_GetFirstLogConf *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetNextLogConf(struct pipes_struct *p,
			   struct PNP_GetNextLogConf *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetLogConfPriority(struct pipes_struct *p,
			       struct PNP_GetLogConfPriority *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_AddResDes(struct pipes_struct *p,
		      struct PNP_AddResDes *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_FreeResDes(struct pipes_struct *p,
		       struct PNP_FreeResDes *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetNextResDes(struct pipes_struct *p,
			  struct PNP_GetNextResDes *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetResDesData(struct pipes_struct *p,
			  struct PNP_GetResDesData *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetResDesDataSize(struct pipes_struct *p,
			      struct PNP_GetResDesDataSize *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_ModifyResDes(struct pipes_struct *p,
			 struct PNP_ModifyResDes *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_DetectResourceLimit(struct pipes_struct *p,
				struct PNP_DetectResourceLimit *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_QueryResConfList(struct pipes_struct *p,
			     struct PNP_QueryResConfList *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_SetHwProf(struct pipes_struct *p,
		      struct PNP_SetHwProf *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_QueryArbitratorFreeData(struct pipes_struct *p,
				    struct PNP_QueryArbitratorFreeData *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_QueryArbitratorFreeSize(struct pipes_struct *p,
				    struct PNP_QueryArbitratorFreeSize *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_RunDetection(struct pipes_struct *p,
			 struct PNP_RunDetection *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_RegisterNotification(struct pipes_struct *p,
				 struct PNP_RegisterNotification *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_UnregisterNotification(struct pipes_struct *p,
				   struct PNP_UnregisterNotification *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetCustomDevProp(struct pipes_struct *p,
			     struct PNP_GetCustomDevProp *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetVersionInternal(struct pipes_struct *p,
			       struct PNP_GetVersionInternal *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetBlockedDriverInfo(struct pipes_struct *p,
				 struct PNP_GetBlockedDriverInfo *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _PNP_GetServerSideDeviceInstallFlags(struct pipes_struct *p,
					    struct PNP_GetServerSideDeviceInstallFlags *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}
