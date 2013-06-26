/*
 *  Unix SMB/CIFS implementation.
 *
 *  SPOOLSS RPC Pipe server / winreg client routines
 *
 *  Copyright (c) 2010      Andreas Schneider <asn@samba.org>
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
#include "rpc_server/rpc_ncacn_np.h"
#include "../librpc/gen_ndr/ndr_spoolss.h"
#include "../librpc/gen_ndr/ndr_winreg.h"
#include "srv_spoolss_util.h"
#include "rpc_client/cli_winreg_spoolss.h"

WERROR winreg_printer_binding_handle(TALLOC_CTX *mem_ctx,
				     const struct auth_serversupplied_info *session_info,
				     struct messaging_context *msg_ctx,
				     struct dcerpc_binding_handle **winreg_binding_handle)
{
	static struct client_address client_id;
	NTSTATUS status;

	strlcpy(client_id.addr, "127.0.0.1", sizeof(client_id.addr));
	client_id.name = "127.0.0.1";

	status = rpcint_binding_handle(mem_ctx,
				       &ndr_table_winreg,
				       &client_id,
				       session_info,
				       msg_ctx,
				       winreg_binding_handle);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("winreg_printer_binding_handle: Could not connect to winreg pipe: %s\n",
			  nt_errstr(status)));
		return ntstatus_to_werror(status);
	}

	return WERR_OK;
}

WERROR winreg_delete_printer_key_internal(TALLOC_CTX *mem_ctx,
					  const struct auth_serversupplied_info *session_info,
					  struct messaging_context *msg_ctx,
					  const char *printer,
					  const char *key)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_delete_printer_key(mem_ctx, b,
					 printer,
					 key);
}

WERROR winreg_printer_update_changeid_internal(TALLOC_CTX *mem_ctx,
					       const struct auth_serversupplied_info *session_info,
					       struct messaging_context *msg_ctx,
					       const char *printer)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_printer_update_changeid(mem_ctx, b,
					      printer);
}

WERROR winreg_printer_get_changeid_internal(TALLOC_CTX *mem_ctx,
					    const struct auth_serversupplied_info *session_info,
					    struct messaging_context *msg_ctx,
					    const char *printer,
					    uint32_t *pchangeid)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_printer_get_changeid(mem_ctx, b,
					   printer,
					   pchangeid);
}

WERROR winreg_get_printer_internal(TALLOC_CTX *mem_ctx,
				   const struct auth_serversupplied_info *session_info,
				   struct messaging_context *msg_ctx,
				   const char *printer,
				   struct spoolss_PrinterInfo2 **pinfo2)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_get_printer(mem_ctx, b,
				  printer,
				  pinfo2);

}

WERROR winreg_create_printer_internal(TALLOC_CTX *mem_ctx,
				      const struct auth_serversupplied_info *session_info,
				      struct messaging_context *msg_ctx,
				      const char *sharename)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_create_printer(mem_ctx, b,
				     sharename);
}

WERROR winreg_update_printer_internal(TALLOC_CTX *mem_ctx,
				      const struct auth_serversupplied_info *session_info,
				      struct messaging_context *msg_ctx,
				      const char *sharename,
				      uint32_t info2_mask,
				      struct spoolss_SetPrinterInfo2 *info2,
				      struct spoolss_DeviceMode *devmode,
				      struct security_descriptor *secdesc)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_update_printer(mem_ctx, b,
				     sharename,
				     info2_mask,
				     info2,
				     devmode,
				     secdesc);
}

WERROR winreg_set_printer_dataex_internal(TALLOC_CTX *mem_ctx,
					  const struct auth_serversupplied_info *session_info,
					  struct messaging_context *msg_ctx,
					  const char *printer,
					  const char *key,
					  const char *value,
					  enum winreg_Type type,
					  uint8_t *data,
					  uint32_t data_size)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_set_printer_dataex(mem_ctx, b,
					 printer,
					 key,
					 value,
					 type,
					 data,
					 data_size);
}

WERROR winreg_enum_printer_dataex_internal(TALLOC_CTX *mem_ctx,
					   const struct auth_serversupplied_info *session_info,
					   struct messaging_context *msg_ctx,
					   const char *printer,
					   const char *key,
					   uint32_t *pnum_values,
					   struct spoolss_PrinterEnumValues **penum_values)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_enum_printer_dataex(mem_ctx, b,
					  printer,
					  key,
					  pnum_values,
					  penum_values);
}

WERROR winreg_get_printer_dataex_internal(TALLOC_CTX *mem_ctx,
					  const struct auth_serversupplied_info *session_info,
					  struct messaging_context *msg_ctx,
					  const char *printer,
					  const char *key,
					  const char *value,
					  enum winreg_Type *type,
					  uint8_t **data,
					  uint32_t *data_size)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_get_printer_dataex(mem_ctx, b,
					 printer,
					 key,
					 value,
					 type,
					 data,
					 data_size);
}

WERROR winreg_delete_printer_dataex_internal(TALLOC_CTX *mem_ctx,
					     const struct auth_serversupplied_info *session_info,
					     struct messaging_context *msg_ctx,
					     const char *printer,
					     const char *key,
					     const char *value)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_delete_printer_dataex(mem_ctx, b,
					    printer,
					    key,
					    value);
}

WERROR winreg_get_driver_internal(TALLOC_CTX *mem_ctx,
				  const struct auth_serversupplied_info *session_info,
				  struct messaging_context *msg_ctx,
				  const char *architecture,
				  const char *driver_name,
				  uint32_t driver_version,
				  struct spoolss_DriverInfo8 **_info8)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_get_driver(mem_ctx, b,
				 architecture,
				 driver_name,
				 driver_version,
				 _info8);
}

WERROR winreg_get_driver_list_internal(TALLOC_CTX *mem_ctx,
				       const struct auth_serversupplied_info *session_info,
				       struct messaging_context *msg_ctx,
				       const char *architecture,
				       uint32_t version,
				       uint32_t *num_drivers,
				       const char ***drivers_p)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_get_driver_list(mem_ctx, b,
				      architecture,
				      version,
				      num_drivers,
				      drivers_p);
}

WERROR winreg_del_driver_internal(TALLOC_CTX *mem_ctx,
				  const struct auth_serversupplied_info *session_info,
				  struct messaging_context *msg_ctx,
				  struct spoolss_DriverInfo8 *info8,
				  uint32_t version)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_del_driver(mem_ctx, b,
				 info8,
				 version);
}

WERROR winreg_add_driver_internal(TALLOC_CTX *mem_ctx,
				  const struct auth_serversupplied_info *session_info,
				  struct messaging_context *msg_ctx,
				  struct spoolss_AddDriverInfoCtr *r,
				  const char **driver_name,
				  uint32_t *driver_version)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_add_driver(mem_ctx, b,
				 r,
				 driver_name,
				 driver_version);
}

WERROR winreg_get_printer_secdesc_internal(TALLOC_CTX *mem_ctx,
					   const struct auth_serversupplied_info *session_info,
					   struct messaging_context *msg_ctx,
					   const char *sharename,
					   struct spoolss_security_descriptor **psecdesc)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_get_printer_secdesc(mem_ctx, b,
					  sharename,
					  psecdesc);
}

WERROR winreg_set_printer_secdesc_internal(TALLOC_CTX *mem_ctx,
					   const struct auth_serversupplied_info *session_info,
					   struct messaging_context *msg_ctx,
					   const char *sharename,
					   const struct spoolss_security_descriptor *secdesc)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_set_printer_secdesc(mem_ctx, b,
					  sharename,
					  secdesc);
}

WERROR winreg_printer_enumforms1_internal(TALLOC_CTX *mem_ctx,
					  const struct auth_serversupplied_info *session_info,
					  struct messaging_context *msg_ctx,
					  uint32_t *pnum_info,
					  union spoolss_FormInfo **pinfo)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_printer_enumforms1(mem_ctx, b,
					 pnum_info,
					 pinfo);
}

WERROR winreg_printer_getform1_internal(TALLOC_CTX *mem_ctx,
					const struct auth_serversupplied_info *session_info,
					struct messaging_context *msg_ctx,
					const char *form_name,
					struct spoolss_FormInfo1 *r)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_printer_getform1(mem_ctx, b,
				       form_name,
				       r);
}

WERROR winreg_printer_addform1_internal(TALLOC_CTX *mem_ctx,
					const struct auth_serversupplied_info *session_info,
					struct messaging_context *msg_ctx,
					struct spoolss_AddFormInfo1 *form)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_printer_addform1(mem_ctx, b,
				       form);
}

WERROR winreg_printer_setform1_internal(TALLOC_CTX *mem_ctx,
					const struct auth_serversupplied_info *session_info,
					struct messaging_context *msg_ctx,
					const char *form_name,
					struct spoolss_AddFormInfo1 *form)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_printer_setform1(mem_ctx, b,
				       form_name,
				       form);
}

WERROR winreg_printer_deleteform1_internal(TALLOC_CTX *mem_ctx,
					   const struct auth_serversupplied_info *session_info,
					   struct messaging_context *msg_ctx,
					   const char *form_name)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_printer_deleteform1(mem_ctx, b,
					  form_name);
}

WERROR winreg_enum_printer_key_internal(TALLOC_CTX *mem_ctx,
					const struct auth_serversupplied_info *session_info,
					struct messaging_context *msg_ctx,
					const char *printer,
					const char *key,
					uint32_t *pnum_subkeys,
					const char ***psubkeys)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx, session_info, msg_ctx, &b);
	W_ERROR_NOT_OK_RETURN(result);

	return winreg_enum_printer_key(mem_ctx, b,
				       printer,
				       key,
				       pnum_subkeys,
				       psubkeys);
}
