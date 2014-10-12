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

#ifndef _SRV_SPOOLSS_UITL_H
#define _SRV_SPOOLSS_UITL_H

struct auth_serversupplied_info;
struct dcerpc_binding_handle;

WERROR winreg_printer_binding_handle(TALLOC_CTX *mem_ctx,
				     const struct auth_serversupplied_info *session_info,
				     struct messaging_context *msg_ctx,
				     struct dcerpc_binding_handle **winreg_binding_handle);

WERROR winreg_delete_printer_key_internal(TALLOC_CTX *mem_ctx,
					  const struct auth_serversupplied_info *session_info,
					  struct messaging_context *msg_ctx,
					  const char *printer,
					  const char *key);
WERROR winreg_printer_update_changeid_internal(TALLOC_CTX *mem_ctx,
					       const struct auth_serversupplied_info *session_info,
					       struct messaging_context *msg_ctx,
					       const char *printer);
WERROR winreg_printer_get_changeid_internal(TALLOC_CTX *mem_ctx,
					    const struct auth_serversupplied_info *session_info,
					    struct messaging_context *msg_ctx,
					    const char *printer,
					    uint32_t *pchangeid);
WERROR winreg_get_printer_internal(TALLOC_CTX *mem_ctx,
				   const struct auth_serversupplied_info *session_info,
				   struct messaging_context *msg_ctx,
				   const char *printer,
				   struct spoolss_PrinterInfo2 **pinfo2);
WERROR winreg_create_printer_internal(TALLOC_CTX *mem_ctx,
				      const struct auth_serversupplied_info *session_info,
				      struct messaging_context *msg_ctx,
				      const char *sharename);
WERROR winreg_update_printer_internal(TALLOC_CTX *mem_ctx,
				      const struct auth_serversupplied_info *session_info,
				      struct messaging_context *msg_ctx,
				      const char *sharename,
				      uint32_t info2_mask,
				      struct spoolss_SetPrinterInfo2 *info2,
				      struct spoolss_DeviceMode *devmode,
				      struct security_descriptor *secdesc);
WERROR winreg_set_printer_dataex_internal(TALLOC_CTX *mem_ctx,
					  const struct auth_serversupplied_info *session_info,
					  struct messaging_context *msg_ctx,
					  const char *printer,
					  const char *key,
					  const char *value,
					  enum winreg_Type type,
					  uint8_t *data,
					  uint32_t data_size);
WERROR winreg_enum_printer_dataex_internal(TALLOC_CTX *mem_ctx,
					   const struct auth_serversupplied_info *session_info,
					   struct messaging_context *msg_ctx,
					   const char *printer,
					   const char *key,
					   uint32_t *pnum_values,
					   struct spoolss_PrinterEnumValues **penum_values);
WERROR winreg_get_printer_dataex_internal(TALLOC_CTX *mem_ctx,
					  const struct auth_serversupplied_info *session_info,
					  struct messaging_context *msg_ctx,
					  const char *printer,
					  const char *key,
					  const char *value,
					  enum winreg_Type *type,
					  uint8_t **data,
					  uint32_t *data_size);
WERROR winreg_delete_printer_dataex_internal(TALLOC_CTX *mem_ctx,
					     const struct auth_serversupplied_info *session_info,
					     struct messaging_context *msg_ctx,
					     const char *printer,
					     const char *key,
					     const char *value);
WERROR winreg_get_driver_internal(TALLOC_CTX *mem_ctx,
				  const struct auth_serversupplied_info *session_info,
				  struct messaging_context *msg_ctx,
				  const char *architecture,
				  const char *driver_name,
				  uint32_t driver_version,
				  struct spoolss_DriverInfo8 **_info8);
WERROR winreg_get_driver_list_internal(TALLOC_CTX *mem_ctx,
				       const struct auth_serversupplied_info *session_info,
				       struct messaging_context *msg_ctx,
				       const char *architecture,
				       uint32_t version,
				       uint32_t *num_drivers,
				       const char ***drivers_p);
WERROR winreg_del_driver_internal(TALLOC_CTX *mem_ctx,
				  const struct auth_serversupplied_info *session_info,
				  struct messaging_context *msg_ctx,
				  struct spoolss_DriverInfo8 *info8,
				  uint32_t version);
WERROR winreg_add_driver_internal(TALLOC_CTX *mem_ctx,
				  const struct auth_serversupplied_info *session_info,
				  struct messaging_context *msg_ctx,
				  struct spoolss_AddDriverInfoCtr *r,
				  const char **driver_name,
				  uint32_t *driver_version);
WERROR winreg_get_printer_secdesc_internal(TALLOC_CTX *mem_ctx,
					   const struct auth_serversupplied_info *session_info,
					   struct messaging_context *msg_ctx,
					   const char *sharename,
					   struct spoolss_security_descriptor **psecdesc);
WERROR winreg_set_printer_secdesc_internal(TALLOC_CTX *mem_ctx,
					   const struct auth_serversupplied_info *session_info,
					   struct messaging_context *msg_ctx,
					   const char *sharename,
					   const struct spoolss_security_descriptor *secdesc);
WERROR winreg_printer_enumforms1_internal(TALLOC_CTX *mem_ctx,
					  const struct auth_serversupplied_info *session_info,
					  struct messaging_context *msg_ctx,
					  uint32_t *pnum_info,
					  union spoolss_FormInfo **pinfo);
WERROR winreg_printer_getform1_internal(TALLOC_CTX *mem_ctx,
					const struct auth_serversupplied_info *session_info,
					struct messaging_context *msg_ctx,
					const char *form_name,
					struct spoolss_FormInfo1 *r);
WERROR winreg_printer_addform1_internal(TALLOC_CTX *mem_ctx,
					const struct auth_serversupplied_info *session_info,
					struct messaging_context *msg_ctx,
					struct spoolss_AddFormInfo1 *form);
WERROR winreg_printer_setform1_internal(TALLOC_CTX *mem_ctx,
					const struct auth_serversupplied_info *session_info,
					struct messaging_context *msg_ctx,
					const char *form_name,
					struct spoolss_AddFormInfo1 *form);
WERROR winreg_printer_deleteform1_internal(TALLOC_CTX *mem_ctx,
					   const struct auth_serversupplied_info *session_info,
					   struct messaging_context *msg_ctx,
					   const char *form_name);
WERROR winreg_enum_printer_key_internal(TALLOC_CTX *mem_ctx,
					const struct auth_serversupplied_info *session_info,
					struct messaging_context *msg_ctx,
					const char *printer,
					const char *key,
					uint32_t *pnum_subkeys,
					const char ***psubkeys);
#endif /* _SRV_SPOOLSS_UITL_H */
