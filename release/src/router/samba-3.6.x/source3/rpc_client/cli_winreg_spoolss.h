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

#ifndef _RPC_CLIENT_CLI_WINREG_SPOOLSS_H_
#define _RPC_CLIENT_CLI_WINREG_SPOOLSS_H_

struct dcerpc_binding_handle;

enum spoolss_PrinterInfo2Mask {
	SPOOLSS_PRINTER_INFO_ATTRIBUTES      = (int)(0x00000001),
	SPOOLSS_PRINTER_INFO_AVERAGEPPM      = (int)(0x00000002),
	SPOOLSS_PRINTER_INFO_CJOBS           = (int)(0x00000004),
	SPOOLSS_PRINTER_INFO_COMMENT         = (int)(0x00000008),
	SPOOLSS_PRINTER_INFO_DATATYPE        = (int)(0x00000010),
	SPOOLSS_PRINTER_INFO_DEFAULTPRIORITY = (int)(0x00000020),
	SPOOLSS_PRINTER_INFO_DEVMODE         = (int)(0x00000040),
	SPOOLSS_PRINTER_INFO_DRIVERNAME      = (int)(0x00000080),
	SPOOLSS_PRINTER_INFO_LOCATION        = (int)(0x00000100),
	SPOOLSS_PRINTER_INFO_NAME            = (int)(0x00000200),
	SPOOLSS_PRINTER_INFO_PARAMETERS      = (int)(0x00000400),
	SPOOLSS_PRINTER_INFO_PORTNAME        = (int)(0x00000800),
	SPOOLSS_PRINTER_INFO_PRINTERNAME     = (int)(0x00001000),
	SPOOLSS_PRINTER_INFO_PRINTPROCESSOR  = (int)(0x00002000),
	SPOOLSS_PRINTER_INFO_PRIORITY        = (int)(0x00004000),
	SPOOLSS_PRINTER_INFO_SECDESC         = (int)(0x00008000),
	SPOOLSS_PRINTER_INFO_SEPFILE         = (int)(0x00010000),
	SPOOLSS_PRINTER_INFO_SERVERNAME      = (int)(0x00020000),
	SPOOLSS_PRINTER_INFO_SHARENAME       = (int)(0x00040000),
	SPOOLSS_PRINTER_INFO_STARTTIME       = (int)(0x00080000),
	SPOOLSS_PRINTER_INFO_STATUS          = (int)(0x00100000),
	SPOOLSS_PRINTER_INFO_UNTILTIME       = (int)(0x00200000)
};

#define SPOOLSS_PRINTER_INFO_ALL SPOOLSS_PRINTER_INFO_ATTRIBUTES      | \
                                 SPOOLSS_PRINTER_INFO_AVERAGEPPM      | \
                                 SPOOLSS_PRINTER_INFO_CJOBS           | \
                                 SPOOLSS_PRINTER_INFO_COMMENT         | \
                                 SPOOLSS_PRINTER_INFO_DATATYPE        | \
                                 SPOOLSS_PRINTER_INFO_DEFAULTPRIORITY | \
                                 SPOOLSS_PRINTER_INFO_DEVMODE         | \
                                 SPOOLSS_PRINTER_INFO_DRIVERNAME      | \
                                 SPOOLSS_PRINTER_INFO_LOCATION        | \
                                 SPOOLSS_PRINTER_INFO_NAME            | \
                                 SPOOLSS_PRINTER_INFO_PARAMETERS      | \
                                 SPOOLSS_PRINTER_INFO_PORTNAME        | \
                                 SPOOLSS_PRINTER_INFO_PRINTERNAME     | \
                                 SPOOLSS_PRINTER_INFO_PRINTPROCESSOR  | \
                                 SPOOLSS_PRINTER_INFO_PRIORITY        | \
                                 SPOOLSS_PRINTER_INFO_SECDESC         | \
                                 SPOOLSS_PRINTER_INFO_SEPFILE         | \
                                 SPOOLSS_PRINTER_INFO_SERVERNAME      | \
                                 SPOOLSS_PRINTER_INFO_SHARENAME       | \
                                 SPOOLSS_PRINTER_INFO_STARTTIME       | \
                                 SPOOLSS_PRINTER_INFO_STATUS          | \
                                 SPOOLSS_PRINTER_INFO_UNTILTIME

WERROR winreg_create_printer(TALLOC_CTX *mem_ctx,
			     struct dcerpc_binding_handle *b,
			     const char *sharename);

/**
 * @internal
 *
 * @brief Update the information of a printer in the registry.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  sharename  The share name.
 *
 * @param[in]  info2_mask A bitmask which defines which values should be set.
 *
 * @param[in]  info2    A SetPrinterInfo2 structure with the data to set.
 *
 * @param[in]  devmode  A device mode structure with the data to set.
 *
 * @param[in]  secdesc  A security descriptor structure with the data to set.
 *
 * @return              On success WERR_OK, a corresponding DOS error is
 *                      something went wrong.
 */
WERROR winreg_update_printer(TALLOC_CTX *mem_ctx,
			     struct dcerpc_binding_handle *b,
			     const char *sharename,
			     uint32_t info2_mask,
			     struct spoolss_SetPrinterInfo2 *info2,
			     struct spoolss_DeviceMode *devmode,
			     struct security_descriptor *secdesc);


/**
 * @brief Get the inforamtion of a printer stored in the registry.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  printer  The name of the printer to get.
 *
 * @param[out] pinfo2   A pointer to store a PRINTER_INFO_2 structure.
 *
 * @return              On success WERR_OK, a corresponding DOS error is
 *                      something went wrong.
 */
WERROR winreg_get_printer(TALLOC_CTX *mem_ctx,
			  struct dcerpc_binding_handle *b,
			  const char *printer,
			  struct spoolss_PrinterInfo2 **pinfo2);

/**
 * @brief Get the security descriptor for a printer.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  sharename  The share name.
 *
 * @param[out] psecdesc   A pointer to store the security descriptor.
 *
 * @return              On success WERR_OK, a corresponding DOS error is
 *                      something went wrong.
 */
WERROR winreg_get_printer_secdesc(TALLOC_CTX *mem_ctx,
				  struct dcerpc_binding_handle *b,
				  const char *sharename,
				  struct spoolss_security_descriptor **psecdesc);

/**
 * @brief Set the security descriptor for a printer.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  sharename  The share name.
 *
 * @param[in]  secdesc  The security descriptor to save.
 *
 * @return              On success WERR_OK, a corresponding DOS error is
 *                      something went wrong.
 */
WERROR winreg_set_printer_secdesc(TALLOC_CTX *mem_ctx,
				  struct dcerpc_binding_handle *b,
				  const char *sharename,
				  const struct spoolss_security_descriptor *secdesc);

/**
 * @internal
 *
 * @brief Set printer data over the winreg pipe.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  printer  The printer name.
 *
 * @param[in]  key      The key of the printer data to store the value.
 *
 * @param[in]  value    The value name to save.
 *
 * @param[in]  type     The type of the value to use.
 *
 * @param[in]  data     The data which sould be saved under the given value.
 *
 * @param[in]  data_size The size of the data.
 *
 * @return              On success WERR_OK, a corresponding DOS error is
 *                      something went wrong.
 */
WERROR winreg_set_printer_dataex(TALLOC_CTX *mem_ctx,
				 struct dcerpc_binding_handle *b,
				 const char *printer,
				 const char *key,
				 const char *value,
				 enum winreg_Type type,
				 uint8_t *data,
				 uint32_t data_size);

/**
 * @internal
 *
 * @brief Get printer data over a winreg pipe.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  printer  The printer name.
 *
 * @param[in]  key      The key of the printer data to get the value.
 *
 * @param[in]  value    The name of the value to query.
 *
 * @param[in]  type     The type of the value to query.
 *
 * @param[out] data     A pointer to store the data.
 *
 * @param[out] data_size A pointer to store the size of the data.
 *
 * @return              On success WERR_OK, a corresponding DOS error is
 *                      something went wrong.
 */
WERROR winreg_get_printer_dataex(TALLOC_CTX *mem_ctx,
				 struct dcerpc_binding_handle *b,
				 const char *printer,
				 const char *key,
				 const char *value,
				 enum winreg_Type *type,
				 uint8_t **data,
				 uint32_t *data_size);

/**
 * @internal
 *
 * @brief Enumerate on the values of a given key and provide the data.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  printer  The printer name.
 *
 * @param[in]  key      The key of the printer data to get the value.
 *
 * @param[out] pnum_values A pointer to store the number of values we found.
 *
 * @param[out] penum_values A pointer to store the values and its data.
 *
 * @return                   WERR_OK on success, the corresponding DOS error
 *                           code if something gone wrong.
 */
WERROR winreg_enum_printer_dataex(TALLOC_CTX *mem_ctx,
				  struct dcerpc_binding_handle *b,
				  const char *printer,
				  const char *key,
				  uint32_t *pnum_values,
				  struct spoolss_PrinterEnumValues **penum_values);

/**
 * @internal
 *
 * @brief Delete printer data over a winreg pipe.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  printer  The printer name.
 *
 * @param[in]  key      The key of the printer data to delete.
 *
 * @param[in]  value    The name of the value to delete.
 *
 * @return              On success WERR_OK, a corresponding DOS error is
 *                      something went wrong.
 */
WERROR winreg_delete_printer_dataex(TALLOC_CTX *mem_ctx,
				    struct dcerpc_binding_handle *b,
				    const char *printer,
				    const char *key,
				    const char *value);

/**
 * @internal
 *
 * @brief Enumerate on the subkeys of a given key and provide the data.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  printer  The printer name.
 *
 * @param[in]  key      The key of the printer data to get the value.
 *
 * @param[out] pnum_subkeys A pointer to store the number of subkeys found.
 *
 * @param[in]  psubkeys A pointer to an array to store the names of the subkeys
 *                      found.
 *
 * @return              WERR_OK on success, the corresponding DOS error
 *                      code if something gone wrong.
 */
WERROR winreg_enum_printer_key(TALLOC_CTX *mem_ctx,
			       struct dcerpc_binding_handle *b,
			       const char *printer,
			       const char *key,
			       uint32_t *pnum_subkeys,
			       const char ***psubkeys);

/**
 * @internal
 *
 * @brief Delete a key with subkeys of a given printer.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  printer  The printer name.
 *
 * @param[in]  key      The key of the printer to delete.
 *
 * @return              On success WERR_OK, a corresponding DOS error is
 *                      something went wrong.
 */
WERROR winreg_delete_printer_key(TALLOC_CTX *mem_ctx,
				 struct dcerpc_binding_handle *b,
				 const char *printer,
				 const char *key);

/**
 * @brief Update the ChangeID of a printer.
 *
 * The ChangeID **must** be increasing over the lifetime of client's spoolss
 * service in order for the client's cache to show updates.
 *
 * If a form is updated of a printer, the we need to update the ChangeID of the
 * pritner.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  printer  The printer name.
 *
 * @return              On success WERR_OK, a corresponding DOS error is
 *                      something went wrong.
 */
WERROR winreg_printer_update_changeid(TALLOC_CTX *mem_ctx,
				      struct dcerpc_binding_handle *b,
				      const char *printer);

/**
 * @brief Get the ChangeID of the given printer.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  printer  The printer name.
 *
 * @param[in]  changeid A pointer to store the changeid.
 *
 * @return              On success WERR_OK, a corresponding DOS error is
 *                      something went wrong.
 */
WERROR winreg_printer_get_changeid(TALLOC_CTX *mem_ctx,
				   struct dcerpc_binding_handle *b,
				   const char *printer,
				   uint32_t *pchangeid);

/**
 * @internal
 *
 * @brief This function adds a form to the list of available forms that can be
 * selected for the specified printer.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  form     The form to add.
 *
 * @return              WERR_OK on success.
 *                      WERR_ALREADY_EXISTS if the form already exists or is a
 *                                          builtin form.
 *                      A corresponding DOS error is something went wrong.
 */
WERROR winreg_printer_addform1(TALLOC_CTX *mem_ctx,
			       struct dcerpc_binding_handle *b,
			       struct spoolss_AddFormInfo1 *form);

/*
 * @brief This function enumerates the forms supported by the specified printer.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[out] pnum_info A pointer to store the FormInfo count.
 *
 * @param[out] pinfo     A pointer to store an array with FormInfo.
 *
 * @return              On success WERR_OK, a corresponding DOS error is
 *                      something went wrong.
 */
WERROR winreg_printer_enumforms1(TALLOC_CTX *mem_ctx,
				 struct dcerpc_binding_handle *b,
				 uint32_t *pnum_info,
				 union spoolss_FormInfo **pinfo);

/**
 * @brief This function removes a form name from the list of supported forms.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  form_name The name of the form to delete.
 *
 * @return              WERR_OK on success.
 *                      WERR_INVALID_PARAM if the form is a builtin form.
 *                      WERR_INVALID_FORM_NAME if the form or key doesn't exist.
 *                      A corresponding DOS error is something went wrong.
 */
WERROR winreg_printer_deleteform1(TALLOC_CTX *mem_ctx,
				  struct dcerpc_binding_handle *b,
				  const char *form_name);

/**
 * @brief This function sets the form information for the specified printer.
 *
 * If one provides both the name in the API call and inside the FormInfo
 * structure, then the form gets renamed.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  form_name The name of the form to set or rename.
 *
 * @param[in]  form     The FormInfo structure to save.
 *
 * @return              WERR_OK on success.
 *                      WERR_INVALID_PARAM if the form is a builtin form.
 *                      A corresponding DOS error is something went wrong.
 */
WERROR winreg_printer_setform1(TALLOC_CTX *mem_ctx,
			       struct dcerpc_binding_handle *b,
			       const char *form_name,
			       struct spoolss_AddFormInfo1 *form);

/**
 * @brief This function retrieves information about a specified form.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  form_name The name of the form to query.
 *
 * @param[out] form     A pointer to a form structure to fill out.
 *
 * @return              On success WERR_OK, a corresponding DOS error is
 *                      something went wrong.
 */
WERROR winreg_printer_getform1(TALLOC_CTX *mem_ctx,
			       struct dcerpc_binding_handle *b,
			       const char *form_name,
			       struct spoolss_FormInfo1 *form);

/**
 * @brief This function adds a new spool driver
 *
 * @param[in]  mem_ctx	       A talloc memory context.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  r	       The structure containing the new driver data.
 *
 * @param[out] driver_name     Returns the driver name.
 *
 * @param[out] driver_version  Returns the driver version.
 *
 * @return              On success WERR_OK, a corresponding DOS error is
 *                      something went wrong.
 */
WERROR winreg_add_driver(TALLOC_CTX *mem_ctx,
			 struct dcerpc_binding_handle *b,
			 struct spoolss_AddDriverInfoCtr *r,
			 const char **driver_name,
			 uint32_t *driver_version);

/**
 * @brief This function gets printer driver information
 *
 * @param[in]  mem_ctx	       A talloc memory context.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  architecture    The architecture type.
 *
 * @param[in]  driver_name     The driver name.
 *
 * @param[in]  driver_version  The driver version.
 *
 * @param[out] _info8   The structure that holds the full driver information.
 *
 * @return              On success WERR_OK, a corresponding DOS error is
 *                      something went wrong.
 */

WERROR winreg_get_driver(TALLOC_CTX *mem_ctx,
			 struct dcerpc_binding_handle *b,
			 const char *architecture,
			 const char *driver_name,
			 uint32_t driver_version,
			 struct spoolss_DriverInfo8 **_info8);

/**
 * @brief This function deletes a printer driver information
 *
 * @param[in]  mem_ctx	       A talloc memory context.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[out] info8    The structure that holds the full driver information.
 *
 * @param[in]  version  The driver type version.
 *
 * @return              On success WERR_OK, a corresponding DOS error is
 *                      something went wrong.
 */

WERROR winreg_del_driver(TALLOC_CTX *mem_ctx,
			 struct dcerpc_binding_handle *b,
			 struct spoolss_DriverInfo8 *info8,
			 uint32_t version);

/**
 * @brief This function gets printer drivers list for the specified
 *        architecture and type version
 *
 * @param[in]  mem_ctx	       A talloc memory context.
 *
 * @param[in]  b The dcerpc binding handle
 *
 * @param[in]  architecture    The architecture type.
 *
 * @param[in]  version         The driver version.
 *
 * @param[out] num_drivers     The number of drivers.
 *
 * @param[out] version         The drivers names.
 *
 * @return              On success WERR_OK, a corresponding DOS error is
 *                      something went wrong.
 */

WERROR winreg_get_driver_list(TALLOC_CTX *mem_ctx,
			      struct dcerpc_binding_handle *b,
			      const char *architecture,
			      uint32_t version,
			      uint32_t *num_drivers,
			      const char ***drivers);

#endif /* _RPC_CLIENT_CLI_WINREG_SPOOLSS_H_ */
