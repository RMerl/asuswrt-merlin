/*
   Unix SMB/CIFS implementation.
   RPC pipe client
   Copyright (C) Tim Potter                        2000-2001,
   Copyright (C) Andrew Tridgell              1992-1997,2000,
   Copyright (C) Rafal Szczesniak                       2002
   Copyright (C) Jeremy Allison				2005.
   Copyright (C) Michael Adam				2007.
   Copyright (C) Guenther Deschner			2008.

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
#include "rpc_client/rpc_client.h"
#include "../librpc/gen_ndr/ndr_lsa_c.h"
#include "rpc_client/cli_lsarpc.h"
#include "rpc_client/init_lsa.h"
#include "../libcli/security/security.h"

/** @defgroup lsa LSA - Local Security Architecture
 *  @ingroup rpc_client
 *
 * @{
 **/

/**
 * @file cli_lsarpc.c
 *
 * RPC client routines for the LSA RPC pipe.  LSA means "local
 * security authority", which is half of a password database.
 **/

NTSTATUS dcerpc_lsa_open_policy(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				bool sec_qos,
				uint32_t des_access,
				struct policy_handle *pol,
				NTSTATUS *result)
{
	struct lsa_ObjectAttribute attr;
	struct lsa_QosInfo qos;
	uint16_t system_name = '\\';

	ZERO_STRUCT(attr);

	attr.len	= 0x18;

	if (sec_qos) {
		qos.len			= 0xc;
		qos.impersonation_level	= 2;
		qos.context_mode	= 1;
		qos.effective_only	= 0;

		attr.sec_qos		= &qos;
	}

	return dcerpc_lsa_OpenPolicy(h,
				     mem_ctx,
				     &system_name,
				     &attr,
				     des_access,
				     pol,
				     result);
}

/** Open a LSA policy handle
 *
 * @param cli Handle on an initialised SMB connection */

NTSTATUS rpccli_lsa_open_policy(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				bool sec_qos, uint32 des_access,
				struct policy_handle *pol)
{
	NTSTATUS status;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	status = dcerpc_lsa_open_policy(cli->binding_handle,
					mem_ctx,
					sec_qos,
					des_access,
					pol,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return result;
}

NTSTATUS dcerpc_lsa_open_policy2(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char *srv_name_slash,
				 bool sec_qos,
				 uint32_t des_access,
				 struct policy_handle *pol,
				 NTSTATUS *result)
{
	struct lsa_ObjectAttribute attr;
	struct lsa_QosInfo qos;

	ZERO_STRUCT(attr);

	attr.len	= 0x18;

	if (sec_qos) {
		qos.len			= 0xc;
		qos.impersonation_level	= 2;
		qos.context_mode	= 1;
		qos.effective_only	= 0;

		attr.sec_qos		= &qos;
	}

	return dcerpc_lsa_OpenPolicy2(h,
				      mem_ctx,
				      srv_name_slash,
				      &attr,
				      des_access,
				      pol,
				      result);
}

/** Open a LSA policy handle
  *
  * @param cli Handle on an initialised SMB connection
  */

NTSTATUS rpccli_lsa_open_policy2(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx, bool sec_qos,
				 uint32 des_access, struct policy_handle *pol)
{
	NTSTATUS status;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	status = dcerpc_lsa_open_policy2(cli->binding_handle,
					 mem_ctx,
					 cli->srv_name_slash,
					 sec_qos,
					 des_access,
					 pol,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return result;
}

/* Lookup a list of sids
 *
 * internal version withOUT memory allocation of the target arrays.
 * this assumes sufficiently sized arrays to store domains, names and types. */

static NTSTATUS dcerpc_lsa_lookup_sids_noalloc(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx,
					       TALLOC_CTX *domains_ctx,
					       TALLOC_CTX *names_ctx,
					       struct policy_handle *pol,
					       int num_sids,
					       const struct dom_sid *sids,
					       char **domains,
					       char **names,
					       enum lsa_SidType *types,
					       bool use_lookupsids3,
					       NTSTATUS *presult)
{
	NTSTATUS status = NT_STATUS_OK;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	struct lsa_SidArray sid_array;
	struct lsa_RefDomainList *ref_domains = NULL;
	struct lsa_TransNameArray lsa_names;
	enum lsa_LookupNamesLevel level = LSA_LOOKUP_NAMES_ALL;
	uint32_t count = 0;
	int i;

	ZERO_STRUCT(lsa_names);

	sid_array.num_sids = num_sids;
	sid_array.sids = TALLOC_ARRAY(mem_ctx, struct lsa_SidPtr, num_sids);
	if (sid_array.sids == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i = 0; i<num_sids; i++) {
		sid_array.sids[i].sid = dom_sid_dup(mem_ctx, &sids[i]);
		if (!sid_array.sids[i].sid) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	if (use_lookupsids3) {
		struct lsa_TransNameArray2 lsa_names2;
		uint32_t n;

		ZERO_STRUCT(lsa_names2);

		status = dcerpc_lsa_LookupSids3(h,
						mem_ctx,
						&sid_array,
						&ref_domains,
						&lsa_names2,
						level,
						&count,
						LSA_LOOKUP_OPTION_SEARCH_ISOLATED_NAMES,
						LSA_CLIENT_REVISION_2,
						&result);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		if(!NT_STATUS_IS_ERR(result)) {
			lsa_names.count = lsa_names2.count;
			lsa_names.names = talloc_array(mem_ctx,
						       struct lsa_TranslatedName,
						       lsa_names.count);
			if (lsa_names.names == NULL) {
				return NT_STATUS_NO_MEMORY;
			}
			for (n=0; n < lsa_names.count; n++) {
				lsa_names.names[n].sid_type	= lsa_names2.names[n].sid_type;
				lsa_names.names[n].name		= lsa_names2.names[n].name;
				lsa_names.names[n].sid_index	= lsa_names2.names[n].sid_index;
			}
		}

	} else {
		status = dcerpc_lsa_LookupSids(h,
					       mem_ctx,
					       pol,
					       &sid_array,
					       &ref_domains,
					       &lsa_names,
					       level,
					       &count,
					       &result);
	}

	DEBUG(10, ("LSA_LOOKUPSIDS returned status: '%s', result: '%s', "
		   "mapped count = %d'\n",
		   nt_errstr(status), nt_errstr(result), count));

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!NT_STATUS_IS_OK(result) &&
	    !NT_STATUS_EQUAL(result, NT_STATUS_NONE_MAPPED) &&
	    !NT_STATUS_EQUAL(result, STATUS_SOME_UNMAPPED))
	{
		*presult = result;
		return status;
	}

	/* Return output parameters */
	if (NT_STATUS_EQUAL(result, NT_STATUS_NONE_MAPPED) ||
	    (count == 0))
	{
		for (i = 0; i < num_sids; i++) {
			(names)[i] = NULL;
			(domains)[i] = NULL;
			(types)[i] = SID_NAME_UNKNOWN;
		}
		*presult = NT_STATUS_NONE_MAPPED;
		return status;
	}

	for (i = 0; i < num_sids; i++) {
		const char *name, *dom_name;
		uint32_t dom_idx;

		if (i >= lsa_names.count) {
			*presult = NT_STATUS_INVALID_NETWORK_RESPONSE;
			return status;
		}

		dom_idx = lsa_names.names[i].sid_index;

		/* Translate optimised name through domain index array */

		if (dom_idx != 0xffffffff) {
			if (ref_domains == NULL) {
				*presult = NT_STATUS_INVALID_NETWORK_RESPONSE;
				return status;
			}
			if (dom_idx >= ref_domains->count) {
				*presult = NT_STATUS_INVALID_NETWORK_RESPONSE;
				return status;
			}

			dom_name = ref_domains->domains[dom_idx].name.string;
			name = lsa_names.names[i].name.string;

			if (name) {
				(names)[i] = talloc_strdup(names_ctx, name);
				if ((names)[i] == NULL) {
					DEBUG(0, ("cli_lsa_lookup_sids_noalloc(): out of memory\n"));
					*presult = NT_STATUS_UNSUCCESSFUL;
					return status;
				}
			} else {
				(names)[i] = NULL;
			}
			domains[i] = talloc_strdup(domains_ctx,
						   dom_name ? dom_name : "");
			(types)[i] = lsa_names.names[i].sid_type;
			if (((domains)[i] == NULL)) {
				DEBUG(0, ("cli_lsa_lookup_sids_noalloc(): out of memory\n"));
				*presult = NT_STATUS_UNSUCCESSFUL;
				return status;
			}

		} else {
			(names)[i] = NULL;
			(domains)[i] = NULL;
			(types)[i] = SID_NAME_UNKNOWN;
		}
	}

	*presult = NT_STATUS_OK;
	return status;
}

/* Lookup a list of sids
 *
 * do it the right way: there is a limit (of 20480 for w2k3) entries
 * returned by this call. when the sids list contains more entries,
 * empty lists are returned. This version of lsa_lookup_sids passes
 * the list of sids in hunks of LOOKUP_SIDS_HUNK_SIZE to the lsa call. */

/* This constant defines the limit of how many sids to look up
 * in one call (maximum). the limit from the server side is
 * at 20480 for win2k3, but we keep it at a save 1000 for now. */
#define LOOKUP_SIDS_HUNK_SIZE 1000

NTSTATUS dcerpc_lsa_lookup_sids_generic(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *pol,
					int num_sids,
					const struct dom_sid *sids,
					char ***pdomains,
					char ***pnames,
					enum lsa_SidType **ptypes,
					bool use_lookupsids3,
					NTSTATUS *presult)
{
	NTSTATUS status = NT_STATUS_OK;
	NTSTATUS result = NT_STATUS_OK;
	int sids_left = 0;
	int sids_processed = 0;
	const struct dom_sid *hunk_sids = sids;
	char **hunk_domains;
	char **hunk_names;
	enum lsa_SidType *hunk_types;
	char **domains = NULL;
	char **names = NULL;
	enum lsa_SidType *types = NULL;
	bool have_mapped = false;
	bool have_unmapped = false;

	if (num_sids) {
		if (!(domains = TALLOC_ARRAY(mem_ctx, char *, num_sids))) {
			DEBUG(0, ("rpccli_lsa_lookup_sids(): out of memory\n"));
			status = NT_STATUS_NO_MEMORY;
			goto fail;
		}

		if (!(names = TALLOC_ARRAY(mem_ctx, char *, num_sids))) {
			DEBUG(0, ("rpccli_lsa_lookup_sids(): out of memory\n"));
			status = NT_STATUS_NO_MEMORY;
			goto fail;
		}

		if (!(types = TALLOC_ARRAY(mem_ctx, enum lsa_SidType, num_sids))) {
			DEBUG(0, ("rpccli_lsa_lookup_sids(): out of memory\n"));
			status = NT_STATUS_NO_MEMORY;
			goto fail;
		}
	}

	sids_left = num_sids;
	hunk_domains = domains;
	hunk_names = names;
	hunk_types = types;

	while (sids_left > 0) {
		int hunk_num_sids;
		NTSTATUS hunk_result = NT_STATUS_UNSUCCESSFUL;

		hunk_num_sids = ((sids_left > LOOKUP_SIDS_HUNK_SIZE)
				? LOOKUP_SIDS_HUNK_SIZE
				: sids_left);

		DEBUG(10, ("rpccli_lsa_lookup_sids: processing items "
			   "%d -- %d of %d.\n",
			   sids_processed,
			   sids_processed + hunk_num_sids - 1,
			   num_sids));

		status = dcerpc_lsa_lookup_sids_noalloc(h,
							mem_ctx,
							(TALLOC_CTX *)domains,
							(TALLOC_CTX *)names,
							pol,
							hunk_num_sids,
							hunk_sids,
							hunk_domains,
							hunk_names,
							hunk_types,
							use_lookupsids3,
							&hunk_result);
		if (!NT_STATUS_IS_OK(status)) {
			goto fail;
		}

		if (!NT_STATUS_IS_OK(hunk_result) &&
		    !NT_STATUS_EQUAL(hunk_result, STATUS_SOME_UNMAPPED) &&
		    !NT_STATUS_EQUAL(hunk_result, NT_STATUS_NONE_MAPPED))
		{
			/* An actual error occured */
			*presult = hunk_result;
			goto fail;
		}

		if (NT_STATUS_IS_OK(hunk_result)) {
			have_mapped = true;
		}
		if (NT_STATUS_EQUAL(hunk_result, NT_STATUS_NONE_MAPPED)) {
			have_unmapped = true;
		}
		if (NT_STATUS_EQUAL(hunk_result, STATUS_SOME_UNMAPPED)) {
			int i;
			for (i=0; i<hunk_num_sids; i++) {
				if (hunk_types[i] == SID_NAME_UNKNOWN) {
					have_unmapped = true;
				} else {
					have_mapped = true;
				}
			}
		}

		sids_left -= hunk_num_sids;
		sids_processed += hunk_num_sids;
		hunk_sids += hunk_num_sids;
		hunk_domains += hunk_num_sids;
		hunk_names += hunk_num_sids;
		hunk_types += hunk_num_sids;
	}

	*pdomains = domains;
	*pnames = names;
	*ptypes = types;

	if (!have_mapped) {
		result = NT_STATUS_NONE_MAPPED;
	}
	if (have_unmapped) {
		result = STATUS_SOME_UNMAPPED;
	}
	*presult = result;

	return status;

fail:
	TALLOC_FREE(domains);
	TALLOC_FREE(names);
	TALLOC_FREE(types);

	return status;
}

NTSTATUS dcerpc_lsa_lookup_sids(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *pol,
				int num_sids,
				const struct dom_sid *sids,
				char ***pdomains,
				char ***pnames,
				enum lsa_SidType **ptypes,
				NTSTATUS *result)
{
	return dcerpc_lsa_lookup_sids_generic(h,
					      mem_ctx,
					      pol,
					      num_sids,
					      sids,
					      pdomains,
					      pnames,
					      ptypes,
					      false,
					      result);
}

NTSTATUS rpccli_lsa_lookup_sids(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *pol,
				int num_sids,
				const struct dom_sid *sids,
				char ***pdomains,
				char ***pnames,
				enum lsa_SidType **ptypes)
{
	NTSTATUS status;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	status = dcerpc_lsa_lookup_sids_generic(cli->binding_handle,
						mem_ctx,
						pol,
						num_sids,
						sids,
						pdomains,
						pnames,
						ptypes,
						false,
						&result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return result;
}

NTSTATUS dcerpc_lsa_lookup_sids3(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *pol,
				 int num_sids,
				 const struct dom_sid *sids,
				 char ***pdomains,
				 char ***pnames,
				 enum lsa_SidType **ptypes,
				 NTSTATUS *result)
{
	return dcerpc_lsa_lookup_sids_generic(h,
					      mem_ctx,
					      pol,
					      num_sids,
					      sids,
					      pdomains,
					      pnames,
					      ptypes,
					      true,
					      result);
}

/** Lookup a list of names */

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
					 NTSTATUS *presult)
{
	NTSTATUS status;
	struct lsa_String *lsa_names = NULL;
	struct lsa_RefDomainList *domains = NULL;
	struct lsa_TransSidArray sid_array;
	struct lsa_TransSidArray3 sid_array3;
	uint32_t count = 0;
	uint32_t i;

	ZERO_STRUCT(sid_array);
	ZERO_STRUCT(sid_array3);

	lsa_names = TALLOC_ARRAY(mem_ctx, struct lsa_String, num_names);
	if (lsa_names == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i = 0; i < num_names; i++) {
		init_lsa_String(&lsa_names[i], names[i]);
	}

	if (use_lookupnames4) {
		status = dcerpc_lsa_LookupNames4(h,
						 mem_ctx,
						 num_names,
						 lsa_names,
						 &domains,
						 &sid_array3,
						 level,
						 &count,
						 LSA_LOOKUP_OPTION_SEARCH_ISOLATED_NAMES,
						 LSA_CLIENT_REVISION_2,
						 presult);
	} else {
		status = dcerpc_lsa_LookupNames(h,
						mem_ctx,
						pol,
						num_names,
						lsa_names,
						&domains,
						&sid_array,
						level,
						&count,
						presult);
	}
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if (!NT_STATUS_IS_OK(*presult) &&
	    !NT_STATUS_EQUAL(*presult, STATUS_SOME_UNMAPPED)) {
		/* An actual error occured */
		goto done;
	}

	/* Return output parameters */
	if (count == 0) {
		*presult = NT_STATUS_NONE_MAPPED;
		goto done;
	}

	if (num_names) {
		if (!((*sids = TALLOC_ARRAY(mem_ctx, struct dom_sid, num_names)))) {
			DEBUG(0, ("cli_lsa_lookup_sids(): out of memory\n"));
			*presult = NT_STATUS_NO_MEMORY;
			goto done;
		}

		if (!((*types = TALLOC_ARRAY(mem_ctx, enum lsa_SidType, num_names)))) {
			DEBUG(0, ("cli_lsa_lookup_sids(): out of memory\n"));
			*presult = NT_STATUS_NO_MEMORY;
			goto done;
		}

		if (dom_names != NULL) {
			*dom_names = TALLOC_ARRAY(mem_ctx, const char *, num_names);
			if (*dom_names == NULL) {
				DEBUG(0, ("cli_lsa_lookup_sids(): out of memory\n"));
				*presult = NT_STATUS_NO_MEMORY;
				goto done;
			}
		}
	} else {
		*sids = NULL;
		*types = NULL;
		if (dom_names != NULL) {
			*dom_names = NULL;
		}
	}

	for (i = 0; i < num_names; i++) {
		uint32_t dom_idx;
		struct dom_sid *sid = &(*sids)[i];

		if (use_lookupnames4) {
			if (i >= sid_array3.count) {
				*presult = NT_STATUS_INVALID_NETWORK_RESPONSE;
				goto done;
			}

			dom_idx		= sid_array3.sids[i].sid_index;
			(*types)[i]	= sid_array3.sids[i].sid_type;
		} else {
			if (i >= sid_array.count) {
				*presult = NT_STATUS_INVALID_NETWORK_RESPONSE;
				goto done;
			}

			dom_idx		= sid_array.sids[i].sid_index;
			(*types)[i]	= sid_array.sids[i].sid_type;
		}

		/* Translate optimised sid through domain index array */

		if (dom_idx == 0xffffffff) {
			/* Nothing to do, this is unknown */
			ZERO_STRUCTP(sid);
			(*types)[i] = SID_NAME_UNKNOWN;
			continue;
		}
		if (domains == NULL) {
			*presult = NT_STATUS_INVALID_NETWORK_RESPONSE;
			goto done;
		}
		if (dom_idx >= domains->count) {
			*presult = NT_STATUS_INVALID_NETWORK_RESPONSE;
			goto done;
		}

		if (use_lookupnames4) {
			sid_copy(sid, sid_array3.sids[i].sid);
		} else {
			sid_copy(sid, domains->domains[dom_idx].sid);

			if (sid_array.sids[i].rid != 0xffffffff) {
				sid_append_rid(sid, sid_array.sids[i].rid);
			}
		}

		if (dom_names == NULL) {
			continue;
		}

		(*dom_names)[i] = domains->domains[dom_idx].name.string;
	}

 done:
	return status;
}

NTSTATUS dcerpc_lsa_lookup_names(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *pol,
				 uint32_t num_names,
				 const char **names,
				 const char ***dom_names,
				 enum lsa_LookupNamesLevel level,
				 struct dom_sid **sids,
				 enum lsa_SidType **types,
				 NTSTATUS *result)
{
	return dcerpc_lsa_lookup_names_generic(h,
					       mem_ctx,
					       pol,
					       num_names,
					       names,
					       dom_names,
					       level,
					       sids,
					       types,
					       false,
					       result);
}

NTSTATUS rpccli_lsa_lookup_names(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *pol,
				 int num_names,
				 const char **names,
				 const char ***dom_names,
				 int level,
				 struct dom_sid **sids,
				 enum lsa_SidType **types)
{
	NTSTATUS status;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	status = dcerpc_lsa_lookup_names(cli->binding_handle,
					 mem_ctx,
					 pol,
					 num_names,
					 names,
					 dom_names,
					 level,
					 sids,
					 types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return result;
}

NTSTATUS dcerpc_lsa_lookup_names4(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *pol,
				  uint32_t num_names,
				  const char **names,
				  const char ***dom_names,
				  enum lsa_LookupNamesLevel level,
				  struct dom_sid **sids,
				  enum lsa_SidType **types,
				  NTSTATUS *result)
{
	return dcerpc_lsa_lookup_names_generic(h,
					       mem_ctx,
					       pol,
					       num_names,
					       names,
					       dom_names,
					       level,
					       sids,
					       types,
					       true,
					       result);
}
