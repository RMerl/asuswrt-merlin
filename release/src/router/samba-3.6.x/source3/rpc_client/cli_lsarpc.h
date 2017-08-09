/*
 *  Unix SMB/CIFS implementation.
 *
 *  LSARPC client routines
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

#ifndef _CLI_LSARPC_H
#define _CLI_LSARPC_H

/* The following definitions come from rpc_client/cli_lsarpc.c  */

/**
 * @brief Open a LSA policy.
 *
 * @param[in]  h        The dcerpc binding hanlde to use.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  sec_qos  Enable security quality of services.
 *
 * @param[in]  des_access The disired access rights to be granted.
 *
 * @param[out]  pol     A pointer to a rpc policy handle.
 *
 * @param[out]  result  A pointer for the NDR NTSTATUS error code.
 *
 * @return              A corresponding NTSTATUS error code for the connection.
 */
NTSTATUS dcerpc_lsa_open_policy(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				bool sec_qos,
				uint32_t des_access,
				struct policy_handle *pol,
				NTSTATUS *result);
NTSTATUS rpccli_lsa_open_policy(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				bool sec_qos, uint32 des_access,
				struct policy_handle *pol);

/**
 * @brief Open a LSA policy.
 *
 * @param[in]  h        The dcerpc binding hanlde to use.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  sec_qos  Enable security quality of services.
 *
 * @param[in]  des_access The disired access rights to be granted.
 *
 * @param[out]  pol     A pointer to a rpc policy handle.
 *
 * @param[out]  result  A pointer for the NDR NTSTATUS error code.
 *
 * @return              A corresponding NTSTATUS error code for the connection.
 */
NTSTATUS dcerpc_lsa_open_policy2(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char *srv_name_slash,
				 bool sec_qos,
				 uint32_t des_access,
				 struct policy_handle *pol,
				 NTSTATUS *result);
NTSTATUS rpccli_lsa_open_policy2(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx, bool sec_qos,
				 uint32 des_access, struct policy_handle *pol);

/**
 * @brief Look up the names that correspond to an array of sids.
 *
 * @param[in]  h        The initialized binding handle for a dcerpc connection.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  pol      The opened domain policy handle.
 *
 * @param[in]  num_sids The number of sids in the sids array to look up.
 *
 * @param[in]  sids     The array of sids to look up.
 *
 * @param[out]  pdomains A pointer to store the refercenced domains.
 *
 * @param[out]  pnames  A pointer to an array for the translated names.
 *
 * @param[out]  ptypes  A pointer to an array for the types of the names.
 *
 * @param[out]  result  A pointer for the conversion result.
 *
 * @return              A corresponding NTSTATUS error code.
 */
NTSTATUS dcerpc_lsa_lookup_sids(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *pol,
				int num_sids,
				const struct dom_sid *sids,
				char ***pdomains,
				char ***pnames,
				enum lsa_SidType **ptypes,
				NTSTATUS *result);
NTSTATUS rpccli_lsa_lookup_sids(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *pol,
				int num_sids,
				const struct dom_sid *sids,
				char ***pdomains,
				char ***pnames,
				enum lsa_SidType **ptypes);
NTSTATUS dcerpc_lsa_lookup_sids_generic(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *pol,
					int num_sids,
					const struct dom_sid *sids,
					char ***pdomains,
					char ***pnames,
					enum lsa_SidType **ptypes,
					bool use_lookupsids3,
					NTSTATUS *presult);
/**
 * @brief Look up the names that correspond to an array of sids.
 *
 * @param[in]  h        The initialized binding handle for a dcerpc connection.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  pol      The opened domain policy handle.
 *
 * @param[in]  num_sids The number of sids in the sids array to look up.
 *
 * @param[in]  sids     The array of sids to look up.
 *
 * @param[out]  pdomains A pointer to store the refercenced domains.
 *
 * @param[out]  pnames  A pointer to an array for the translated names.
 *
 * @param[out]  ptypes  A pointer to an array for the types of the names.
 *
 * @param[out]  result  A pointer for the conversion result.
 *
 * @return              A corresponding NTSTATUS error code.
 */
NTSTATUS dcerpc_lsa_lookup_sids3(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *pol,
				 int num_sids,
				 const struct dom_sid *sids,
				 char ***pdomains,
				 char ***pnames,
				 enum lsa_SidType **ptypes,
				 NTSTATUS *result);
NTSTATUS dcerpc_lsa_lookup_names(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *pol,
				 uint32_t num_names,
				 const char **names,
				 const char ***dom_names,
				 enum lsa_LookupNamesLevel level,
				 struct dom_sid **sids,
				 enum lsa_SidType **types,
				 NTSTATUS *result);
NTSTATUS rpccli_lsa_lookup_names(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *pol, int num_names,
				 const char **names,
				 const char ***dom_names,
				 int level,
				 struct dom_sid **sids,
				 enum lsa_SidType **types);

NTSTATUS dcerpc_lsa_lookup_names4(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *pol,
				  uint32_t num_names,
				  const char **names,
				  const char ***dom_names,
				  enum lsa_LookupNamesLevel level,
				  struct dom_sid **sids,
				  enum lsa_SidType **types,
				  NTSTATUS *result);
NTSTATUS dcerpc_lsa_lookup_names_generic(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *pol,
					 uint32_t num_names,
					 const char **names,
					 const char ***dom_names,
					 enum lsa_LookupNamesLevel level,
					 struct dom_sid **sids,
					 enum lsa_SidType **types,
					 bool use_lookupnames4,
					 NTSTATUS *presult);

bool fetch_domain_sid( char *domain, char *remote_machine, struct dom_sid *psid);

#endif /* _CLI_LSARPC_H */
