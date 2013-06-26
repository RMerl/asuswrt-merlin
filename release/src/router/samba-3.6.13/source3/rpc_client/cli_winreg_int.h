/*
 *  Unix SMB/CIFS implementation.
 *
 *  WINREG internal client routines
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

#ifndef CLI_WINREG_INT_H
#define CLI_WINREG_INT_H

struct dcerpc_binding_handle;
struct auth_serversupplied_info;
struct dcerpc_binding_handle;

/**
 * @brief Connect to the interal winreg server and open the given key.
 *
 * The function will create the needed subkeys if they don't exist.
 *
 * @param[in]  mem_ctx       The memory context to use.
 *
 * @param[in]  session_info   The supplied server info.
 *
 * @param[in]  key           The key to open. This needs to start with the name
 *                           of the hive like HKLM.
 *
 * @param[in]  create_key    Set to true if the key should be created if it
 *                           doesn't exist.
 *
 * @param[in]  access_mask   The access mask to open the key.
 *
 * @param[out] binding_handle A pointer for the winreg dcerpc binding handle.
 *
 * @param[out] hive_handle   A policy handle for the opened hive.
 *
 * @param[out] key_handle    A policy handle for the opened key.
 *
 * @return                   WERR_OK on success, the corresponding DOS error
 *                           code if something gone wrong.
 */
NTSTATUS dcerpc_winreg_int_openkey(TALLOC_CTX *mem_ctx,
				   const struct auth_serversupplied_info *server_info,
				   struct messaging_context *msg_ctx,
				   struct dcerpc_binding_handle **h,
				   const char *key,
				   bool create_key,
				   uint32_t access_mask,
				   struct policy_handle *hive_handle,
				   struct policy_handle *key_handle,
				   WERROR *pwerr);

/**
 * @brief Connect to the interal winreg server and open the given key.
 *
 * The function will create the needed subkeys if they don't exist.
 *
 * @param[in]  mem_ctx       The memory context to use.
 *
 * @param[in]  server_info   The supplied server info.
 *
 * @param[in]  key           The key to open.
 *
 * @param[in]  create_key    Set to true if the key should be created if it
 *                           doesn't exist.
 *
 * @param[in]  access_mask   The access mask to open the key.
 *
 * @param[out] binding_handle A pointer for the winreg dcerpc binding handle.
 *
 * @param[out] hive_handle   A policy handle for the opened hive.
 *
 * @param[out] key_handle    A policy handle for the opened key.
 *
 * @return                   WERR_OK on success, the corresponding DOS error
 *                           code if something gone wrong.
 */
NTSTATUS dcerpc_winreg_int_hklm_openkey(TALLOC_CTX *mem_ctx,
					const struct auth_serversupplied_info *session_info,
					struct messaging_context *msg_ctx,
					struct dcerpc_binding_handle **h,
					const char *key,
					bool create_key,
					uint32_t access_mask,
					struct policy_handle *hive_handle,
					struct policy_handle *key_handle,
					WERROR *pwerr);

#endif /* CLI_WINREG_INT_H */

/* vim: set ts=8 sw=8 noet cindent syntax=c.doxygen: */
