/* 
   Unix SMB/CIFS implementation.

   endpoint server for the spoolss pipe

   Copyright (C) Tim Potter 2004
   Copyright (C) Stefan Metzmacher 2005
   
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
#include "rpc_server/dcerpc_server.h"
#include "librpc/gen_ndr/ndr_spoolss.h"
#include "ntptr/ntptr.h"
#include "lib/tsocket/tsocket.h"
#include "librpc/gen_ndr/ndr_spoolss_c.h"
#include "auth/credentials/credentials.h"
#include "param/param.h"

enum spoolss_handle {
	SPOOLSS_NOTIFY
};

#define SPOOLSS_BUFFER_UNION(fn,info,level) \
	((info)?ndr_size_##fn(info, level, 0):0)

#define SPOOLSS_BUFFER_UNION_ARRAY(fn,info,level,count) \
	((info)?ndr_size_##fn##_info(dce_call, level, count, info):0)

#define SPOOLSS_BUFFER_OK(val_true,val_false) ((r->in.offered >= *r->out.needed)?val_true:val_false)

static WERROR dcesrv_spoolss_parse_printer_name(TALLOC_CTX *mem_ctx, const char *name,
					 const char **_server_name,
					 const char **_object_name,
					 enum ntptr_HandleType *_object_type)
{
	char *p;
	char *server = NULL;
	char *server_unc = NULL;
	const char *object = name;

	/* no printername is there it's like open server */
	if (!name) {
		*_server_name = NULL;
		*_object_name = NULL;
		*_object_type = NTPTR_HANDLE_SERVER;
		return WERR_OK;
	}

	/* just "\\" is invalid */
	if (strequal("\\\\", name)) {
		return WERR_INVALID_PRINTER_NAME;
	}

	if (strncmp("\\\\", name, 2) == 0) {
		server_unc = talloc_strdup(mem_ctx, name);
		W_ERROR_HAVE_NO_MEMORY(server_unc);
		server = server_unc + 2;

		/* here we know we have "\\" in front not followed
		 * by '\0', now see if we have another "\" in the string
		 */
		p = strchr_m(server, '\\');
		if (!p) {
			/* there's no other "\", so it's ("\\%s",server)
			 */
			*_server_name = server_unc;
			*_object_name = NULL;
			*_object_type = NTPTR_HANDLE_SERVER;
			return WERR_OK;
		}
		/* here we know that we have ("\\%s\",server),
		 * if we have '\0' as next then it's an invalid name
		 * otherwise the printer_name
		 */
		p[0] = '\0';
		/* everything that follows is the printer name */
		p++;
		object = p;

		/* just "" as server is invalid */
		if (strequal(server, "")) {
			return WERR_INVALID_PRINTER_NAME;
		}
	}

	/* just "" is invalid */
	if (strequal(object, "")) {
		return WERR_INVALID_PRINTER_NAME;
	}

#define XCV_PORT ",XcvPort "
#define XCV_MONITOR ",XcvMonitor "
	if (strncmp(object, XCV_PORT, strlen(XCV_PORT)) == 0) {
		object += strlen(XCV_PORT);

		/* just "" is invalid */
		if (strequal(object, "")) {
			return WERR_INVALID_PRINTER_NAME;
		}

		*_server_name = server_unc;
		*_object_name = object;
		*_object_type = NTPTR_HANDLE_PORT;
		return WERR_OK;
	} else if (strncmp(object, XCV_MONITOR, strlen(XCV_MONITOR)) == 0) {
		object += strlen(XCV_MONITOR);

		/* just "" is invalid */
		if (strequal(object, "")) {
			return WERR_INVALID_PRINTER_NAME;
		}

		*_server_name = server_unc;
		*_object_name = object;
		*_object_type = NTPTR_HANDLE_MONITOR;
		return WERR_OK;
	}

	*_server_name = server_unc;
	*_object_name = object;
	*_object_type = NTPTR_HANDLE_PRINTER;
	return WERR_OK;
}

/*
 * Check server_name is:
 * -  "" , functions that don't allow "",
 *         should check that on their own, before calling this function
 * -  our name (only netbios yet, TODO: need to test dns name!)
 * -  our ip address of the current use socket
 * otherwise return WERR_INVALID_PRINTER_NAME
 */
static WERROR dcesrv_spoolss_check_server_name(struct dcesrv_call_state *dce_call, 
					TALLOC_CTX *mem_ctx,
					const char *server_name)
{
	bool ret;
	const struct tsocket_address *local_address;
	char *myaddr;
	const char **aliases;
	const char *dnsdomain;
	unsigned int i;

	/* NULL is ok */
	if (!server_name) return WERR_OK;

	/* "" is ok */
	ret = strequal("",server_name);
	if (ret) return WERR_OK;

	/* just "\\" is invalid */
	if (strequal("\\\\", server_name)) {
		return WERR_INVALID_PRINTER_NAME;
	}

	/* then we need "\\" */
	if (strncmp("\\\\", server_name, 2) != 0) {
		return WERR_INVALID_PRINTER_NAME;
	}

	server_name += 2;

	/* NETBIOS NAME is ok */
	ret = strequal(lpcfg_netbios_name(dce_call->conn->dce_ctx->lp_ctx), server_name);
	if (ret) return WERR_OK;

	aliases = lpcfg_netbios_aliases(dce_call->conn->dce_ctx->lp_ctx);

	for (i=0; aliases && aliases[i]; i++) {
		if (strequal(aliases[i], server_name)) {
			return WERR_OK;
		}
	}

	/* DNS NAME is ok
	 * TODO: we need to check if aliases are also ok
	 */
	dnsdomain = lpcfg_dnsdomain(dce_call->conn->dce_ctx->lp_ctx);
	if (dnsdomain != NULL) {
		char *str;

		str = talloc_asprintf(mem_ctx, "%s.%s",
						lpcfg_netbios_name(dce_call->conn->dce_ctx->lp_ctx),
						dnsdomain);
		W_ERROR_HAVE_NO_MEMORY(str);

		ret = strequal(str, server_name);
		talloc_free(str);
		if (ret) return WERR_OK;
	}

	local_address = dcesrv_connection_get_local_address(dce_call->conn);

	myaddr = tsocket_address_inet_addr_string(local_address, mem_ctx);
	W_ERROR_HAVE_NO_MEMORY(myaddr);

	ret = strequal(myaddr, server_name);
	talloc_free(myaddr);
	if (ret) return WERR_OK;

	return WERR_INVALID_PRINTER_NAME;
}

static NTSTATUS dcerpc_spoolss_bind(struct dcesrv_call_state *dce_call, const struct dcesrv_interface *iface)
{
	NTSTATUS status;
	struct ntptr_context *ntptr;

	status = ntptr_init_context(dce_call->context, dce_call->conn->event_ctx, dce_call->conn->dce_ctx->lp_ctx,
				    lpcfg_ntptr_providor(dce_call->conn->dce_ctx->lp_ctx), &ntptr);
	NT_STATUS_NOT_OK_RETURN(status);

	dce_call->context->private_data = ntptr;

	return NT_STATUS_OK;
}

#define DCESRV_INTERFACE_SPOOLSS_BIND dcerpc_spoolss_bind

/* 
  spoolss_EnumPrinters 
*/
static WERROR dcesrv_spoolss_EnumPrinters(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_EnumPrinters *r)
{
	struct ntptr_context *ntptr = talloc_get_type(dce_call->context->private_data, struct ntptr_context);
	WERROR status;

	status = dcesrv_spoolss_check_server_name(dce_call, mem_ctx, r->in.server);
	W_ERROR_NOT_OK_RETURN(status);

	status = ntptr_EnumPrinters(ntptr, mem_ctx, r);
	W_ERROR_NOT_OK_RETURN(status);

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(spoolss_EnumPrinters, *r->out.info, r->in.level, *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);
	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

static WERROR dcesrv_spoolss_OpenPrinterEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_OpenPrinterEx *r);
/* 
  spoolss_OpenPrinter 
*/
static WERROR dcesrv_spoolss_OpenPrinter(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_OpenPrinter *r)
{
	WERROR status;
	struct spoolss_OpenPrinterEx *r2;

	r2 = talloc(mem_ctx, struct spoolss_OpenPrinterEx);
	W_ERROR_HAVE_NO_MEMORY(r2);

	r2->in.printername	= r->in.printername;
	r2->in.datatype		= r->in.datatype;
	r2->in.devmode_ctr	= r->in.devmode_ctr;
	r2->in.access_mask	= r->in.access_mask;
	r2->in.level		= 1;
	r2->in.userlevel.level1	= NULL;

	r2->out.handle		= r->out.handle;

	/* TODO: we should take care about async replies here,
	         if spoolss_OpenPrinterEx() would be async!
	 */
	status = dcesrv_spoolss_OpenPrinterEx(dce_call, mem_ctx, r2);

	r->out.handle		= r2->out.handle;

	return status;
}


/* 
  spoolss_SetJob 
*/
static WERROR dcesrv_spoolss_SetJob(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_SetJob *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_GetJob 
*/
static WERROR dcesrv_spoolss_GetJob(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_GetJob *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_EnumJobs 
*/
static WERROR dcesrv_spoolss_EnumJobs(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_EnumJobs *r)
{
	return WERR_OK;
}


/* 
  spoolss_AddPrinter 
*/
static WERROR dcesrv_spoolss_AddPrinter(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_AddPrinter *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_DeletePrinter 
*/
static WERROR dcesrv_spoolss_DeletePrinter(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_DeletePrinter *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_SetPrinter 
*/
static WERROR dcesrv_spoolss_SetPrinter(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_SetPrinter *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_GetPrinter 
*/
static WERROR dcesrv_spoolss_GetPrinter(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_GetPrinter *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_AddPrinterDriver 
*/
static WERROR dcesrv_spoolss_AddPrinterDriver(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_AddPrinterDriver *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_EnumPrinterDrivers 
*/
static WERROR dcesrv_spoolss_EnumPrinterDrivers(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_EnumPrinterDrivers *r)
{
	struct ntptr_context *ntptr = talloc_get_type(dce_call->context->private_data, struct ntptr_context);
	WERROR status;

	status = dcesrv_spoolss_check_server_name(dce_call, mem_ctx, r->in.server);
	W_ERROR_NOT_OK_RETURN(status);

	status = ntptr_EnumPrinterDrivers(ntptr, mem_ctx, r);
	W_ERROR_NOT_OK_RETURN(status);

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(spoolss_EnumPrinterDrivers, *r->out.info, r->in.level, *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);
	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}


/* 
  spoolss_GetPrinterDriver 
*/
static WERROR dcesrv_spoolss_GetPrinterDriver(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_GetPrinterDriver *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_GetPrinterDriverDirectory 
*/
static WERROR dcesrv_spoolss_GetPrinterDriverDirectory(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_GetPrinterDriverDirectory *r)
{
	struct ntptr_context *ntptr = talloc_get_type(dce_call->context->private_data, struct ntptr_context);
	WERROR status;

	status = dcesrv_spoolss_check_server_name(dce_call, mem_ctx, r->in.server);
	W_ERROR_NOT_OK_RETURN(status);

	status = ntptr_GetPrinterDriverDirectory(ntptr, mem_ctx, r);
	W_ERROR_NOT_OK_RETURN(status);

	*r->out.needed	= SPOOLSS_BUFFER_UNION(spoolss_DriverDirectoryInfo, r->out.info, r->in.level);
	r->out.info	= SPOOLSS_BUFFER_OK(r->out.info, NULL);
	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}


/* 
  spoolss_DeletePrinterDriver 
*/
static WERROR dcesrv_spoolss_DeletePrinterDriver(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_DeletePrinterDriver *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_AddPrintProcessor 
*/
static WERROR dcesrv_spoolss_AddPrintProcessor(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_AddPrintProcessor *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_EnumPrintProcessors 
*/
static WERROR dcesrv_spoolss_EnumPrintProcessors(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_EnumPrintProcessors *r)
{
	return WERR_OK;
}


/* 
  spoolss_GetPrintProcessorDirectory 
*/
static WERROR dcesrv_spoolss_GetPrintProcessorDirectory(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_GetPrintProcessorDirectory *r)
{
	struct ntptr_context *ntptr = talloc_get_type(dce_call->context->private_data, struct ntptr_context);
	WERROR status;

	status = dcesrv_spoolss_check_server_name(dce_call, mem_ctx, r->in.server);
	W_ERROR_NOT_OK_RETURN(status);

	status = ntptr_GetPrintProcessorDirectory(ntptr, mem_ctx, r);
	W_ERROR_NOT_OK_RETURN(status);

	*r->out.needed	= SPOOLSS_BUFFER_UNION(spoolss_PrintProcessorDirectoryInfo, r->out.info, r->in.level);
	r->out.info	= SPOOLSS_BUFFER_OK(r->out.info, NULL);
	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}


/* 
  spoolss_StartDocPrinter 
*/
static WERROR dcesrv_spoolss_StartDocPrinter(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_StartDocPrinter *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_StartPagePrinter 
*/
static WERROR dcesrv_spoolss_StartPagePrinter(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_StartPagePrinter *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_WritePrinter 
*/
static WERROR dcesrv_spoolss_WritePrinter(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_WritePrinter *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_EndPagePrinter 
*/
static WERROR dcesrv_spoolss_EndPagePrinter(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_EndPagePrinter *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_AbortPrinter 
*/
static WERROR dcesrv_spoolss_AbortPrinter(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_AbortPrinter *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_ReadPrinter 
*/
static WERROR dcesrv_spoolss_ReadPrinter(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_ReadPrinter *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_EndDocPrinter 
*/
static WERROR dcesrv_spoolss_EndDocPrinter(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_EndDocPrinter *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_AddJob 
*/
static WERROR dcesrv_spoolss_AddJob(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_AddJob *r)
{
	if (r->in.level != 1) {
		return WERR_UNKNOWN_LEVEL;
	}

	return WERR_INVALID_PARAM;
}


/* 
  spoolss_ScheduleJob 
*/
static WERROR dcesrv_spoolss_ScheduleJob(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_ScheduleJob *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_GetPrinterData 
*/
static WERROR dcesrv_spoolss_GetPrinterData(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_GetPrinterData *r)
{
	struct ntptr_GenericHandle *handle;
	struct dcesrv_handle *h;
	WERROR status;

	r->out.type = talloc_zero(mem_ctx, enum winreg_Type);
	W_ERROR_HAVE_NO_MEMORY(r->out.type);

	r->out.needed = talloc_zero(mem_ctx, uint32_t);
	W_ERROR_HAVE_NO_MEMORY(r->out.needed);

	r->out.data = talloc_zero_array(mem_ctx, uint8_t, r->in.offered);
	W_ERROR_HAVE_NO_MEMORY(r->out.data);

	DCESRV_PULL_HANDLE_WERR(h, r->in.handle, DCESRV_HANDLE_ANY);
	handle = talloc_get_type(h->data, struct ntptr_GenericHandle);
	if (!handle)
		return WERR_BADFID;

	switch (handle->type) {
		case NTPTR_HANDLE_SERVER:
			status = ntptr_GetPrintServerData(handle, mem_ctx, r);
			break;
		default:
			status = WERR_FOOBAR;
			break;
	}

	W_ERROR_NOT_OK_RETURN(status);

	*r->out.type	= SPOOLSS_BUFFER_OK(*r->out.type, REG_NONE);
	r->out.data	= SPOOLSS_BUFFER_OK(r->out.data, r->out.data);
	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_MORE_DATA);
}


/* 
  spoolss_SetPrinterData 
*/
static WERROR dcesrv_spoolss_SetPrinterData(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_SetPrinterData *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_WaitForPrinterChange 
*/
static WERROR dcesrv_spoolss_WaitForPrinterChange(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_WaitForPrinterChange *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_ClosePrinter 
*/
static WERROR dcesrv_spoolss_ClosePrinter(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_ClosePrinter *r)
{
	struct dcesrv_handle *h;

	*r->out.handle = *r->in.handle;

	DCESRV_PULL_HANDLE_WERR(h, r->in.handle, DCESRV_HANDLE_ANY);

	talloc_free(h);

	ZERO_STRUCTP(r->out.handle);

	return WERR_OK;
}


/* 
  spoolss_AddForm 
*/
static WERROR dcesrv_spoolss_AddForm(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_AddForm *r)
{
	struct ntptr_GenericHandle *handle;
	struct dcesrv_handle *h;
	WERROR status;

	DCESRV_PULL_HANDLE_WERR(h, r->in.handle, DCESRV_HANDLE_ANY);
	handle = talloc_get_type(h->data, struct ntptr_GenericHandle);
	if (!handle)
		return WERR_BADFID;

	switch (handle->type) {
		case NTPTR_HANDLE_SERVER:
			status = ntptr_AddPrintServerForm(handle, mem_ctx, r);
			W_ERROR_NOT_OK_RETURN(status);
			break;
		case NTPTR_HANDLE_PRINTER:
			status = ntptr_AddPrinterForm(handle, mem_ctx, r);
			W_ERROR_NOT_OK_RETURN(status);
			break;
		default:
			return WERR_FOOBAR;
	}

	return WERR_OK;
}


/* 
  spoolss_DeleteForm 
*/
static WERROR dcesrv_spoolss_DeleteForm(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_DeleteForm *r)
{
	struct ntptr_GenericHandle *handle;
	struct dcesrv_handle *h;
	WERROR status;

	DCESRV_PULL_HANDLE_WERR(h, r->in.handle, DCESRV_HANDLE_ANY);
	handle = talloc_get_type(h->data, struct ntptr_GenericHandle);
	if (!handle)
		return WERR_BADFID;

	switch (handle->type) {
		case NTPTR_HANDLE_SERVER:
			status = ntptr_DeletePrintServerForm(handle, mem_ctx, r);
			W_ERROR_NOT_OK_RETURN(status);
			break;
		case NTPTR_HANDLE_PRINTER:
			status = ntptr_DeletePrinterForm(handle, mem_ctx, r);
			W_ERROR_NOT_OK_RETURN(status);
			break;
		default:
			return WERR_FOOBAR;
	}

	return WERR_OK;
}


/* 
  spoolss_GetForm 
*/
static WERROR dcesrv_spoolss_GetForm(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_GetForm *r)
{
	struct ntptr_GenericHandle *handle;
	struct dcesrv_handle *h;
	WERROR status;

	DCESRV_PULL_HANDLE_WERR(h, r->in.handle, DCESRV_HANDLE_ANY);
	handle = talloc_get_type(h->data, struct ntptr_GenericHandle);
	if (!handle)
		return WERR_BADFID;

	switch (handle->type) {
		case NTPTR_HANDLE_SERVER:
			/*
			 * stupid, but w2k3 returns WERR_BADFID here?
			 */
			return WERR_BADFID;
		case NTPTR_HANDLE_PRINTER:
			status = ntptr_GetPrinterForm(handle, mem_ctx, r);
			W_ERROR_NOT_OK_RETURN(status);
			break;
		default:
			return WERR_FOOBAR;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION(spoolss_FormInfo, r->out.info, r->in.level);
	r->out.info	= SPOOLSS_BUFFER_OK(r->out.info, NULL);
	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}


/* 
  spoolss_SetForm 
*/
static WERROR dcesrv_spoolss_SetForm(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_SetForm *r)
{
	struct ntptr_GenericHandle *handle;
	struct dcesrv_handle *h;
	WERROR status;

	DCESRV_PULL_HANDLE_WERR(h, r->in.handle, DCESRV_HANDLE_ANY);
	handle = talloc_get_type(h->data, struct ntptr_GenericHandle);
	if (!handle)
		return WERR_BADFID;

	switch (handle->type) {
		case NTPTR_HANDLE_SERVER:
			status = ntptr_SetPrintServerForm(handle, mem_ctx, r);
			W_ERROR_NOT_OK_RETURN(status);
			break;
		case NTPTR_HANDLE_PRINTER:
			status = ntptr_SetPrinterForm(handle, mem_ctx, r);
			W_ERROR_NOT_OK_RETURN(status);
			break;
		default:
			return WERR_FOOBAR;
	}

	return WERR_OK;
}


/* 
  spoolss_EnumForms 
*/
static WERROR dcesrv_spoolss_EnumForms(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_EnumForms *r)
{
	struct ntptr_GenericHandle *handle;
	struct dcesrv_handle *h;
	WERROR status;

	DCESRV_PULL_HANDLE_WERR(h, r->in.handle, DCESRV_HANDLE_ANY);
	handle = talloc_get_type(h->data, struct ntptr_GenericHandle);
	if (!handle)
		return WERR_BADFID;

	switch (handle->type) {
		case NTPTR_HANDLE_SERVER:
			status = ntptr_EnumPrintServerForms(handle, mem_ctx, r);
			W_ERROR_NOT_OK_RETURN(status);
			break;
		case NTPTR_HANDLE_PRINTER:
			status = ntptr_EnumPrinterForms(handle, mem_ctx, r);
			W_ERROR_NOT_OK_RETURN(status);
			break;
		default:
			return WERR_FOOBAR;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(spoolss_EnumForms, *r->out.info, r->in.level, *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);
	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}


/* 
  spoolss_EnumPorts 
*/
static WERROR dcesrv_spoolss_EnumPorts(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_EnumPorts *r)
{
	struct ntptr_context *ntptr = talloc_get_type(dce_call->context->private_data, struct ntptr_context);
	WERROR status;

	status = dcesrv_spoolss_check_server_name(dce_call, mem_ctx, r->in.servername);
	W_ERROR_NOT_OK_RETURN(status);

	status = ntptr_EnumPorts(ntptr, mem_ctx, r);
	W_ERROR_NOT_OK_RETURN(status);

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(spoolss_EnumPorts, *r->out.info, r->in.level, *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);
	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}


/* 
  spoolss_EnumMonitors 
*/
static WERROR dcesrv_spoolss_EnumMonitors(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_EnumMonitors *r)
{
	struct ntptr_context *ntptr = talloc_get_type(dce_call->context->private_data, struct ntptr_context);
	WERROR status;

	status = dcesrv_spoolss_check_server_name(dce_call, mem_ctx, r->in.servername);
	W_ERROR_NOT_OK_RETURN(status);

	status = ntptr_EnumMonitors(ntptr, mem_ctx, r);
	W_ERROR_NOT_OK_RETURN(status);

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(spoolss_EnumMonitors, *r->out.info, r->in.level, *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);
	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}


/* 
  spoolss_AddPort 
*/
static WERROR dcesrv_spoolss_AddPort(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_AddPort *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  spoolss_ConfigurePort 
*/
static WERROR dcesrv_spoolss_ConfigurePort(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_ConfigurePort *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_DeletePort 
*/
static WERROR dcesrv_spoolss_DeletePort(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_DeletePort *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_CreatePrinterIC 
*/
static WERROR dcesrv_spoolss_CreatePrinterIC(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_CreatePrinterIC *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_PlayGDIScriptOnPrinterIC 
*/
static WERROR dcesrv_spoolss_PlayGDIScriptOnPrinterIC(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_PlayGDIScriptOnPrinterIC *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_DeletePrinterIC 
*/
static WERROR dcesrv_spoolss_DeletePrinterIC(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_DeletePrinterIC *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_AddPrinterConnection 
*/
static WERROR dcesrv_spoolss_AddPrinterConnection(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_AddPrinterConnection *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_DeletePrinterConnection 
*/
static WERROR dcesrv_spoolss_DeletePrinterConnection(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_DeletePrinterConnection *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_PrinterMessageBox 
*/
static WERROR dcesrv_spoolss_PrinterMessageBox(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_PrinterMessageBox *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_AddMonitor 
*/
static WERROR dcesrv_spoolss_AddMonitor(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_AddMonitor *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_DeleteMonitor 
*/
static WERROR dcesrv_spoolss_DeleteMonitor(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_DeleteMonitor *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_DeletePrintProcessor 
*/
static WERROR dcesrv_spoolss_DeletePrintProcessor(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_DeletePrintProcessor *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_AddPrintProvidor 
*/
static WERROR dcesrv_spoolss_AddPrintProvidor(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_AddPrintProvidor *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_DeletePrintProvidor 
*/
static WERROR dcesrv_spoolss_DeletePrintProvidor(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_DeletePrintProvidor *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_EnumPrintProcDataTypes 
*/
static WERROR dcesrv_spoolss_EnumPrintProcDataTypes(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_EnumPrintProcDataTypes *r)
{
	return WERR_OK;
}


/* 
  spoolss_ResetPrinter 
*/
static WERROR dcesrv_spoolss_ResetPrinter(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_ResetPrinter *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_GetPrinterDriver2 
*/
static WERROR dcesrv_spoolss_GetPrinterDriver2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_GetPrinterDriver2 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_FindFirstPrinterChangeNotification 
*/
static WERROR dcesrv_spoolss_FindFirstPrinterChangeNotification(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_FindFirstPrinterChangeNotification *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_FindNextPrinterChangeNotification 
*/
static WERROR dcesrv_spoolss_FindNextPrinterChangeNotification(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_FindNextPrinterChangeNotification *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_FindClosePrinterNotify 
*/
static WERROR dcesrv_spoolss_FindClosePrinterNotify(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_FindClosePrinterNotify *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_RouterFindFirstPrinterChangeNotificationOld 
*/
static WERROR dcesrv_spoolss_RouterFindFirstPrinterChangeNotificationOld(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_RouterFindFirstPrinterChangeNotificationOld *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_ReplyOpenPrinter 
*/
static WERROR dcesrv_spoolss_ReplyOpenPrinter(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_ReplyOpenPrinter *r)
{
	struct dcesrv_handle *handle;

	handle = dcesrv_handle_new(dce_call->context, SPOOLSS_NOTIFY);
	W_ERROR_HAVE_NO_MEMORY(handle);

	/* For now, just return a handle */

	*r->out.handle = handle->wire_handle;

	return WERR_OK;
}


/* 
  spoolss_RouterReplyPrinter 
*/
static WERROR dcesrv_spoolss_RouterReplyPrinter(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_RouterReplyPrinter *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_ReplyClosePrinter 
*/
static WERROR dcesrv_spoolss_ReplyClosePrinter(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_ReplyClosePrinter *r)
{
	struct dcesrv_handle *handle;
	
	DCESRV_PULL_HANDLE_WERR(handle, r->in.handle, SPOOLSS_NOTIFY);

	talloc_free(handle);

	ZERO_STRUCTP(r->out.handle);

	return WERR_OK;
}

/* 
  spoolss_AddPortEx 
*/
static WERROR dcesrv_spoolss_AddPortEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_AddPortEx *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_RouterFindFirstPrinterChangeNotification 
*/
static WERROR dcesrv_spoolss_RouterFindFirstPrinterChangeNotification(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_RouterFindFirstPrinterChangeNotification *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_SpoolerInit 
*/
static WERROR dcesrv_spoolss_SpoolerInit(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_SpoolerInit *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_ResetPrinterEx 
*/
static WERROR dcesrv_spoolss_ResetPrinterEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_ResetPrinterEx *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_RemoteFindFirstPrinterChangeNotifyEx 
*/
static WERROR dcesrv_spoolss_RemoteFindFirstPrinterChangeNotifyEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_RemoteFindFirstPrinterChangeNotifyEx *r)
{
	struct dcerpc_pipe *p;
	struct dcerpc_binding *binding;
	NTSTATUS status;
	struct spoolss_ReplyOpenPrinter rop;
	struct cli_credentials *creds;
	struct policy_handle notify_handle;

	DEBUG(2, ("Received RFFPCNex from %s\n", r->in.local_machine));

	/*
	 * TODO: for now just open a connection to the client and drop it again
	 *       to keep the w2k3 PrintServer 
	 *       happy to allow to open the Add Printer GUI
	 *       and the torture suite passing
	 */

	binding = talloc_zero(mem_ctx, struct dcerpc_binding);

	binding->transport = NCACN_NP; 
	if (strncmp(r->in.local_machine, "\\\\", 2))
		return WERR_INVALID_COMPUTERNAME;
	binding->host = r->in.local_machine+2;

	creds = cli_credentials_init_anon(mem_ctx); /* FIXME: Use machine credentials instead ? */

	status = dcerpc_pipe_connect_b(mem_ctx, &p, binding, &ndr_table_spoolss, 
				       creds, dce_call->event_ctx,
				       dce_call->conn->dce_ctx->lp_ctx);

	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(0, ("unable to call back to %s\n", r->in.local_machine));
		return WERR_SERVER_UNAVAILABLE;
	}

	ZERO_STRUCT(rop);
	rop.in.server_name = lpcfg_netbios_name(dce_call->conn->dce_ctx->lp_ctx);
	W_ERROR_HAVE_NO_MEMORY(rop.in.server_name);
	rop.in.printer_local = 0;
	rop.in.type = REG_NONE;
	rop.in.bufsize = 0;
	rop.in.buffer = NULL;
	rop.out.handle = &notify_handle;

	status = dcerpc_spoolss_ReplyOpenPrinter_r(p->binding_handle, mem_ctx, &rop);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(0, ("unable to open remote printer %s\n",
			r->in.local_machine));
		return WERR_SERVER_UNAVAILABLE;
	}

	talloc_free(p);

	return WERR_OK;
}


/* 
  spoolss_RouterReplyPrinterEx
*/
static WERROR dcesrv_spoolss_RouterReplyPrinterEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_RouterReplyPrinterEx *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_RouterRefreshPrinterChangeNotify
*/
static WERROR dcesrv_spoolss_RouterRefreshPrinterChangeNotify(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_RouterRefreshPrinterChangeNotify *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_44 
*/
static WERROR dcesrv_spoolss_44(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_44 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
  spoolss_OpenPrinterEx 
*/
static WERROR dcesrv_spoolss_OpenPrinterEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_OpenPrinterEx *r)
{
	struct ntptr_context *ntptr = talloc_get_type(dce_call->context->private_data, struct ntptr_context);
	struct ntptr_GenericHandle *handle;
	struct dcesrv_handle *h;
	const char *server;
	const char *object;
	enum ntptr_HandleType type;
	WERROR status;

	ZERO_STRUCTP(r->out.handle);

	status = dcesrv_spoolss_parse_printer_name(mem_ctx, r->in.printername, &server, &object, &type);
	W_ERROR_NOT_OK_RETURN(status);

	status = dcesrv_spoolss_check_server_name(dce_call, mem_ctx, server);
	W_ERROR_NOT_OK_RETURN(status);

	switch (type) {
		case NTPTR_HANDLE_SERVER:
			status = ntptr_OpenPrintServer(ntptr, mem_ctx, r, server, &handle);
			W_ERROR_NOT_OK_RETURN(status);
			break;
		case NTPTR_HANDLE_PORT:
			status = ntptr_OpenPort(ntptr, mem_ctx, r, object, &handle);
			W_ERROR_NOT_OK_RETURN(status);
			break;
		case NTPTR_HANDLE_MONITOR:
			status = ntptr_OpenMonitor(ntptr, mem_ctx, r, object, &handle);
			W_ERROR_NOT_OK_RETURN(status);
			break;
		case NTPTR_HANDLE_PRINTER:
			status = ntptr_OpenPrinter(ntptr, mem_ctx, r, object, &handle);
			W_ERROR_NOT_OK_RETURN(status);
			break;
		default:
			return WERR_FOOBAR;
	}

	h = dcesrv_handle_new(dce_call->context, handle->type);
	W_ERROR_HAVE_NO_MEMORY(h);

	h->data = talloc_steal(h, handle);

	*r->out.handle	= h->wire_handle;

	return WERR_OK;
}

/* 
  spoolss_AddPrinterEx 
*/
static WERROR dcesrv_spoolss_AddPrinterEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_AddPrinterEx *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_SetPort
*/
static WERROR dcesrv_spoolss_SetPort(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_SetPort *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_EnumPrinterData 
*/
static WERROR dcesrv_spoolss_EnumPrinterData(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_EnumPrinterData *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_DeletePrinterData 
*/
static WERROR dcesrv_spoolss_DeletePrinterData(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_DeletePrinterData *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_4a 
*/
static WERROR dcesrv_spoolss_4a(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_4a *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_4b 
*/
static WERROR dcesrv_spoolss_4b(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_4b *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_4c 
*/
static WERROR dcesrv_spoolss_4c(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_4c *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_SetPrinterDataEx 
*/
static WERROR dcesrv_spoolss_SetPrinterDataEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_SetPrinterDataEx *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_GetPrinterDataEx 
*/
static WERROR dcesrv_spoolss_GetPrinterDataEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_GetPrinterDataEx *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_EnumPrinterDataEx 
*/
static WERROR dcesrv_spoolss_EnumPrinterDataEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_EnumPrinterDataEx *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_EnumPrinterKey 
*/
static WERROR dcesrv_spoolss_EnumPrinterKey(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_EnumPrinterKey *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_DeletePrinterDataEx 
*/
static WERROR dcesrv_spoolss_DeletePrinterDataEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_DeletePrinterDataEx *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_DeletePrinterKey 
*/
static WERROR dcesrv_spoolss_DeletePrinterKey(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_DeletePrinterKey *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_53 
*/
static WERROR dcesrv_spoolss_53(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_53 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_DeletePrinterDriverEx 
*/
static WERROR dcesrv_spoolss_DeletePrinterDriverEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_DeletePrinterDriverEx *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_AddPerMachineConnection
*/
static WERROR dcesrv_spoolss_AddPerMachineConnection(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_AddPerMachineConnection *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_DeletePerMachineConnection
*/
static WERROR dcesrv_spoolss_DeletePerMachineConnection(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_DeletePerMachineConnection *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_EnumPerMachineConnections
*/
static WERROR dcesrv_spoolss_EnumPerMachineConnections(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_EnumPerMachineConnections *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_XcvData
*/
static WERROR dcesrv_spoolss_XcvData(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_XcvData *r)
{
	struct ntptr_GenericHandle *handle;
	struct dcesrv_handle *h;
	WERROR status;

	DCESRV_PULL_HANDLE_WERR(h, r->in.handle, DCESRV_HANDLE_ANY);
	handle = talloc_get_type(h->data, struct ntptr_GenericHandle);

	switch (handle->type) {
		case NTPTR_HANDLE_SERVER:
			status = ntptr_XcvDataPrintServer(handle, mem_ctx, r);
			W_ERROR_NOT_OK_RETURN(status);
			break;
		case NTPTR_HANDLE_PRINTER:
			status = ntptr_XcvDataPrinter(handle, mem_ctx, r);
			W_ERROR_NOT_OK_RETURN(status);
			break;
		case NTPTR_HANDLE_PORT:
			status = ntptr_XcvDataPort(handle, mem_ctx, r);
			W_ERROR_NOT_OK_RETURN(status);
			break;
		case NTPTR_HANDLE_MONITOR:
			status = ntptr_XcvDataMonitor(handle, mem_ctx, r);
			W_ERROR_NOT_OK_RETURN(status);
			break;
		default:
			return WERR_FOOBAR;
	}

	/* TODO: handle the buffer sizes here! */
	return WERR_OK;
}


/* 
  spoolss_AddPrinterDriverEx 
*/
static WERROR dcesrv_spoolss_AddPrinterDriverEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_AddPrinterDriverEx *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_5a 
*/
static WERROR dcesrv_spoolss_5a(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_5a *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_5b 
*/
static WERROR dcesrv_spoolss_5b(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_5b *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_5c 
*/
static WERROR dcesrv_spoolss_5c(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_5c *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_5d 
*/
static WERROR dcesrv_spoolss_5d(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_5d *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_5e 
*/
static WERROR dcesrv_spoolss_5e(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_5e *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  spoolss_5f 
*/
static WERROR dcesrv_spoolss_5f(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_5f *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/*
  spoolss_60
*/
static WERROR dcesrv_spoolss_60(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_60 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  spoolss_61
*/
static WERROR dcesrv_spoolss_61(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_61 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  spoolss_62
*/
static WERROR dcesrv_spoolss_62(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_62 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  spoolss_63
*/
static WERROR dcesrv_spoolss_63(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_63 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  spoolss_64
*/
static WERROR dcesrv_spoolss_64(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_64 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  spoolss_65
*/
static WERROR dcesrv_spoolss_65(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_65 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  spoolss_GetCorePrinterDrivers
*/
static WERROR dcesrv_spoolss_GetCorePrinterDrivers(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_GetCorePrinterDrivers *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  spoolss_67
*/
static WERROR dcesrv_spoolss_67(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_67 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  spoolss_GetPrinterDriverPackagePath
*/
static WERROR dcesrv_spoolss_GetPrinterDriverPackagePath(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_GetPrinterDriverPackagePath *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  spoolss_69
*/
static WERROR dcesrv_spoolss_69(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_69 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  spoolss_6a
*/
static WERROR dcesrv_spoolss_6a(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_6a *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  spoolss_6b
*/
static WERROR dcesrv_spoolss_6b(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_6b *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  spoolss_6c
*/
static WERROR dcesrv_spoolss_6c(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_6c *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  spoolss_6d
*/
static WERROR dcesrv_spoolss_6d(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct spoolss_6d *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}



/* include the generated boilerplate */
#include "librpc/gen_ndr/ndr_spoolss_s.c"
