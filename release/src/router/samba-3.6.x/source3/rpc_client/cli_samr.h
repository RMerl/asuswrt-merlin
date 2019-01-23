/*
 *  Unix SMB/CIFS implementation.
 *
 *  SAMR client routines
 *
 *  Copyright (c) 2000-2001 Tim Potter
 *  Copyright (c) 1992-2000 Andrew Tridgell
 *  Copyright (c) 2002      Rafal Szczesniak
 *  Copyright (c) 2005      Jeremy Allison
 *  Copyright (c) 2007      Michael Adam
 *  Copyright (c) 2008      Guenther Deschner
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

#ifndef _CLI_SAMR_H
#define _CLI_SAMR_H

/* The following definitions come from rpc_client/cli_samr.c  */

/**
 * @brief Change the password of a user.
 *
 * @param[in]  h        The dcerpc binding hanlde to use.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  user_handle The password of the user to chang the handle
 *
 * @param[in]  newpassword The new password to set.
 *
 * @param[in]  oldpassword The old password for verification
 *
 * @param[out] presult  A pointer for the NDR NTSTATUS error code.
 *
 * @return              A corresponding NTSTATUS error code for the connection.
 */
NTSTATUS dcerpc_samr_chgpasswd_user(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *user_handle,
				    const char *newpassword,
				    const char *oldpassword,
				    NTSTATUS *presult);

NTSTATUS rpccli_samr_chgpasswd_user(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *user_handle,
				    const char *newpassword,
				    const char *oldpassword);

/**
 * @brief Change the password of a user based on username.
 *
 * @param[in]  h        The dcerpc binding hanlde to use.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  srv_name_slash The server name with leading slashes.
 *
 * @param[in]  username The name of ther user.
 *
 * @param[in]  newpassword The new password to set.
 *
 * @param[in]  oldpassword The old password for verification
 *
 * @param[out] presult  A pointer for the NDR NTSTATUS error code.
 *
 * @return              A corresponding NTSTATUS error code for the connection.
 */
NTSTATUS dcerpc_samr_chgpasswd_user2(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     const char *srv_name_slash,
				     const char *username,
				     const char *newpassword,
				     const char *oldpassword,
				     NTSTATUS *presult);

NTSTATUS rpccli_samr_chgpasswd_user2(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     const char *username,
				     const char *newpassword,
				     const char *oldpassword);

/**
 * @brief Change the password of a user based on the user name given and using
 * blobs.
 *
 * @param[in]  h        The dcerpc binding hanlde to use.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  srv_name_slash The server name with leading slashes.
 *
 * @param[in]  username The name of ther user.
 *
 * @param[in]  new_nt_password_blob The new password as a crypted blob.
 *
 * @param[in]  old_nt_hash_enc_blob The old password as a hash encoded blob.
 *
 * @param[in]  new_lm_password_blob The new password as a lanman encoded blob.
 *
 * @param[in]  old_lm_hash_enc_blob The old password as a lanman encoded blob.
 *
 * @param[out] presult  A pointer for the NDR NTSTATUS error code.
 *
 * @return              A corresponding NTSTATUS error code for the connection.
 */
NTSTATUS dcerpc_samr_chng_pswd_auth_crap(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 const char *srv_name_slash,
					 const char *username,
					 DATA_BLOB new_nt_password_blob,
					 DATA_BLOB old_nt_hash_enc_blob,
					 DATA_BLOB new_lm_password_blob,
					 DATA_BLOB old_lm_hash_enc_blob,
					 NTSTATUS *presult);

NTSTATUS rpccli_samr_chng_pswd_auth_crap(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 const char *username,
					 DATA_BLOB new_nt_password_blob,
					 DATA_BLOB old_nt_hash_enc_blob,
					 DATA_BLOB new_lm_password_blob,
					 DATA_BLOB old_lm_hash_enc_blob);

/**
 * @brief
 *
 * @param[in]  h        The dcerpc binding hanlde to use.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  srv_name_slash The server name with leading slashes.
 *
 * @param[in]  username The name of ther user.
 *
 * @param[in]  newpassword The new password to set.
 *
 * @param[in]  oldpassword The old password to set.
 *
 * @param[in]  dominfo1 A pointer to hold the domain information.
 *
 * @param[in]  reject   A pointer to store the result of a possible reject.
 *
 * @param[out] presult  A pointer for the NDR NTSTATUS error code.
 *
 * @return              A corresponding NTSTATUS error code for the connection.
 */
NTSTATUS dcerpc_samr_chgpasswd_user3(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     const char *srv_name_slash,
				     const char *username,
				     const char *newpassword,
				     const char *oldpassword,
				     struct samr_DomInfo1 **dominfo1,
				     struct userPwdChangeFailureInformation **reject,
				     NTSTATUS *presult);

NTSTATUS rpccli_samr_chgpasswd_user3(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     const char *username,
				     const char *newpassword,
				     const char *oldpassword,
				     struct samr_DomInfo1 **dominfo1,
				     struct userPwdChangeFailureInformation **reject);

/**
 * @brief Create a set of max_entries, max_size for QueryDisplayInfo.
 *
 * This function returns a set of (max_entries, max_size) required
 * for the QueryDisplayInfo RPC to actually work against a domain controller
 * with large (10k and higher) numbers of users.  These values were
 * obtained by inspection using wireshark and NT4 running User Manager.
 *
 * @param[in]  loop_count The loop count.
 *
 * @param[out] max_entries A pointer to store maximum entries value.
 *
 * @param[out] max_size A poiter to store the maximum size value.
 */
void dcerpc_get_query_dispinfo_params(int loop_count,
				      uint32_t *max_entries,
				      uint32_t *max_size);

/**
 * @brief Try if we can connnect to samr.
 *
 * @param[in]  h        The dcerpc binding hanlde to use.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  srv_name_slash The server name with leading slashes.
 *
 * @param[in]  access_mask The access mask to use to open the connection.
 *
 * @param[in]  connect_pol A pointer to store the policy handle for the
 *                         connection.
 *
 * @param[out] presult  A pointer for the NDR NTSTATUS error code.
 *
 * @return              A corresponding NTSTATUS error code for the connection.
 */
NTSTATUS dcerpc_try_samr_connects(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  const char *srv_name_slash,
				  uint32_t access_mask,
				  struct policy_handle *connect_pol,
				  NTSTATUS *presult);

NTSTATUS rpccli_try_samr_connects(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  uint32_t access_mask,
				  struct policy_handle *connect_pol);

#endif /* _CLI_SAMR_H */
