/*
 *  Unix SMB/CIFS implementation.
 *
 *  WINREG client routines
 *
 *  Copyright (c) 2011      Andreas Schneider <asn@samba.org>
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

#ifndef CLI_WINREG_H
#define CLI_WINREG_H

/**
 * @brief Query a key for the specified dword value.
 *
 * Get the data that is associated with the named value of a specified registry
 * open key. This function ensures that the key is a dword and converts it
 * corretly.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  h        The binding handle for the rpc connection.
 *
 * @param[in]  key_handle A handle to a key that MUST have been opened
 *                        previously.
 *
 * @param[in]  value    The name of the value to query.
 *
 * @param[out] data     A pointer to store the data of the value.
 *
 * @param[out] pwerr    A pointer to a WERROR to store result of the query.
 *
 * @return              NT_STATUS_OK on success or a corresponding error if
 *                      there was a problem on the connection.
 */
NTSTATUS dcerpc_winreg_query_dword(TALLOC_CTX *mem_ctx,
				   struct dcerpc_binding_handle *h,
				   struct policy_handle *key_handle,
				   const char *value,
				   uint32_t *data,
				   WERROR *pwerr);

/**
 * @brief Query a key for the specified binary value.
 *
 * Get the data that is associated with the named value of a specified registry
 * open key. This function ensures that the key is a binary value.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  h        The binding handle for the rpc connection.
 *
 * @param[in]  key_handle A handle to a key that MUST have been opened
 *                        previously.
 *
 * @param[in]  value    The name of the value to query.
 *
 * @param[out] data     A pointer to store the data of the value.
 *
 * @param[out] pwerr    A pointer to a WERROR to store result of the query.
 *
 * @return              NT_STATUS_OK on success or a corresponding error if
 *                      there was a problem on the connection.
 */
NTSTATUS dcerpc_winreg_query_binary(TALLOC_CTX *mem_ctx,
				    struct dcerpc_binding_handle *h,
				    struct policy_handle *key_handle,
				    const char *value,
				    DATA_BLOB *data,
				    WERROR *pwerr);

/**
 * @brief Query a key for the specified multi sz value.
 *
 * Get the data that is associated with the named value of a specified registry
 * open key. This function ensures that the key is a multi sz value.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  h        The binding handle for the rpc connection.
 *
 * @param[in]  key_handle A handle to a key that MUST have been opened
 *                        previously.
 *
 * @param[in]  value    The name of the value to query.
 *
 * @param[out] data     A pointer to store the data of the value.
 *
 * @param[out] pwerr    A pointer to a WERROR to store result of the query.
 *
 * @return              NT_STATUS_OK on success or a corresponding error if
 *                      there was a problem on the connection.
 */
NTSTATUS dcerpc_winreg_query_multi_sz(TALLOC_CTX *mem_ctx,
				      struct dcerpc_binding_handle *h,
				      struct policy_handle *key_handle,
				      const char *value,
				      const char ***data,
				      WERROR *pwerr);

/**
 * @brief Query a key for the specified sz value.
 *
 * Get the data that is associated with the named value of a specified registry
 * open key. This function ensures that the key is a multi sz value.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  h        The binding handle for the rpc connection.
 *
 * @param[in]  key_handle A handle to a key that MUST have been opened
 *                        previously.
 *
 * @param[in]  value    The name of the value to query.
 *
 * @param[out] data     A pointer to store the data of the value.
 *
 * @param[out] pwerr    A pointer to a WERROR to store result of the query.
 *
 * @return              NT_STATUS_OK on success or a corresponding error if
 *                      there was a problem on the connection.
 */
NTSTATUS dcerpc_winreg_query_sz(TALLOC_CTX *mem_ctx,
				struct dcerpc_binding_handle *h,
				struct policy_handle *key_handle,
				const char *value,
				const char **data,
				WERROR *pwerr);

/**
 * @brief Query a key for the specified security descriptor.
 *
 * Get the data that is associated with the named value of a specified registry
 * open key. This function ensures that the key is a security descriptor.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  h        The binding handle for the rpc connection.
 *
 * @param[in]  key_handle A handle to a key that MUST have been opened
 *                        previously.
 *
 * @param[in]  value    The name of the value to query.
 *
 * @param[out] data     A pointer to store the data of the value.
 *
 * @param[out] pwerr    A pointer to a WERROR to store result of the query.
 *
 * @return              NT_STATUS_OK on success or a corresponding error if
 *                      there was a problem on the connection.
 */
NTSTATUS dcerpc_winreg_query_sd(TALLOC_CTX *mem_ctx,
				struct dcerpc_binding_handle *h,
				struct policy_handle *key_handle,
				const char *value,
				struct security_descriptor **data,
				WERROR *pwerr);

/**
 * @brief Set a value with the specified dword data.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  h        The binding handle for the rpc connection.
 *
 * @param[in]  key_handle A handle to a key that MUST have been opened
 *                        previously.
 *
 * @param[in]  value    The name of the value to set.
 *
 * @param[in]  data     The data to store in the value.
 *
 * @param[out] pwerr    A pointer to a WERROR to store result of the query.
 *
 * @return              NT_STATUS_OK on success or a corresponding error if
 *                      there was a problem on the connection.
 */
NTSTATUS dcerpc_winreg_set_dword(TALLOC_CTX *mem_ctx,
				 struct dcerpc_binding_handle *h,
				 struct policy_handle *key_handle,
				 const char *value,
				 uint32_t data,
				 WERROR *pwerr);

/**
 * @brief Set a value with the specified sz data.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  h        The binding handle for the rpc connection.
 *
 * @param[in]  key_handle A handle to a key that MUST have been opened
 *                        previously.
 *
 * @param[in]  value    The name of the value to set.
 *
 * @param[in]  data     The data to store in the value.
 *
 * @param[out] pwerr    A pointer to a WERROR to store result of the query.
 *
 * @return              NT_STATUS_OK on success or a corresponding error if
 *                      there was a problem on the connection.
 */
NTSTATUS dcerpc_winreg_set_sz(TALLOC_CTX *mem_ctx,
			      struct dcerpc_binding_handle *h,
			      struct policy_handle *key_handle,
			      const char *value,
			      const char *data,
			      WERROR *pwerr);

/**
 * @brief Set a value with the specified expand sz data.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  h        The binding handle for the rpc connection.
 *
 * @param[in]  key_handle A handle to a key that MUST have been opened
 *                        previously.
 *
 * @param[in]  value    The name of the value to set.
 *
 * @param[in]  data     The data to store in the value.
 *
 * @param[out] pwerr    A pointer to a WERROR to store result of the query.
 *
 * @return              NT_STATUS_OK on success or a corresponding error if
 *                      there was a problem on the connection.
 */
NTSTATUS dcerpc_winreg_set_expand_sz(TALLOC_CTX *mem_ctx,
				     struct dcerpc_binding_handle *h,
				     struct policy_handle *key_handle,
				     const char *value,
				     const char *data,
				     WERROR *pwerr);

/**
 * @brief Set a value with the specified multi sz data.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  h        The binding handle for the rpc connection.
 *
 * @param[in]  key_handle A handle to a key that MUST have been opened
 *                        previously.
 *
 * @param[in]  value    The name of the value to set.
 *
 * @param[in]  data     The data to store in the value.
 *
 * @param[out] pwerr    A pointer to a WERROR to store result of the query.
 *
 * @return              NT_STATUS_OK on success or a corresponding error if
 *                      there was a problem on the connection.
 */
NTSTATUS dcerpc_winreg_set_multi_sz(TALLOC_CTX *mem_ctx,
				    struct dcerpc_binding_handle *h,
				    struct policy_handle *key_handle,
				    const char *value,
				    const char **data,
				    WERROR *pwerr);

/**
 * @brief Set a value with the specified binary data.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  h        The binding handle for the rpc connection.
 *
 * @param[in]  key_handle A handle to a key that MUST have been opened
 *                        previously.
 *
 * @param[in]  value    The name of the value to set.
 *
 * @param[in]  data     The data to store in the value.
 *
 * @param[out] pwerr    A pointer to a WERROR to store result of the query.
 *
 * @return              NT_STATUS_OK on success or a corresponding error if
 *                      there was a problem on the connection.
 */
NTSTATUS dcerpc_winreg_set_binary(TALLOC_CTX *mem_ctx,
				  struct dcerpc_binding_handle *h,
				  struct policy_handle *key_handle,
				  const char *value,
				  DATA_BLOB *data,
				  WERROR *pwerr);

/**
 * @brief Set a value with the specified security descriptor.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  h        The binding handle for the rpc connection.
 *
 * @param[in]  key_handle A handle to a key that MUST have been opened
 *                        previously.
 *
 * @param[in]  value    The name of the value to set.
 *
 * @param[in]  data     The security descriptor to store in the value.
 *
 * @param[out] pwerr    A pointer to a WERROR to store result of the query.
 *
 * @return              NT_STATUS_OK on success or a corresponding error if
 *                      there was a problem on the connection.
 */
NTSTATUS dcerpc_winreg_set_sd(TALLOC_CTX *mem_ctx,
			      struct dcerpc_binding_handle *h,
			      struct policy_handle *key_handle,
			      const char *value,
			      const struct security_descriptor *data,
			      WERROR *pwerr);

/**
 * @brief Add a value to the multi sz data.
 *
 * This reads the multi sz data from the given value and adds the data to the
 * multi sz. Then it saves it to the regsitry.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  h        The binding handle for the rpc connection.
 *
 * @param[in]  key_handle A handle to a key that MUST have been opened
 *                        previously.
 *
 * @param[in]  value    The name of the value to set.
 *
 * @param[in]  data     The data to add.
 *
 * @param[out] pwerr    A pointer to a WERROR to store result of the query.
 *
 * @return              NT_STATUS_OK on success or a corresponding error if
 *                      there was a problem on the connection.
 */
NTSTATUS dcerpc_winreg_add_multi_sz(TALLOC_CTX *mem_ctx,
				    struct dcerpc_binding_handle *h,
				    struct policy_handle *key_handle,
				    const char *value,
				    const char *data,
				    WERROR *pwerr);

/**
 * @brief Enumerate on the given keyhandle to get the subkey names.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  h        The binding handle for the rpc connection.
 *
 * @param[in]  key_handle A handle to a key that MUST have been opened
 *                        previously.
 *
 * @param[out] pnum_subkeys A pointer to store the number of subkeys.
 *
 * @param[out] psubkeys A pointer to store the names of the subkeys.
 *
 * @param[out] pwerr    A pointer to a WERROR to store result of the query.
 *
 * @return              NT_STATUS_OK on success or a corresponding error if
 *                      there was a problem on the connection.
 */
NTSTATUS dcerpc_winreg_enum_keys(TALLOC_CTX *mem_ctx,
				 struct dcerpc_binding_handle *h,
				 struct policy_handle *key_hnd,
				 uint32_t *pnum_subkeys,
				 const char ***psubkeys,
				 WERROR *pwerr);

#endif /* CLI_WINREG_H */

/* vim: set ts=8 sw=8 noet cindent syntax=c.doxygen: */
