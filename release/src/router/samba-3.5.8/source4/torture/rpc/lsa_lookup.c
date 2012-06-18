/* 
   Unix SMB/CIFS implementation.
   test suite for lsa rpc lookup operations

   Copyright (C) Volker Lendecke 2006
   
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
#include "torture/torture.h"
#include "lib/events/events.h"
#include "libnet/libnet_join.h"
#include "torture/rpc/rpc.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "libcli/security/security.h"

static bool open_policy(TALLOC_CTX *mem_ctx, struct dcerpc_pipe *p,
			struct policy_handle **handle)
{
	struct lsa_ObjectAttribute attr;
	struct lsa_QosInfo qos;
	struct lsa_OpenPolicy2 r;
	NTSTATUS status;

	*handle = talloc(mem_ctx, struct policy_handle);
	if (!*handle) {
		return false;
	}

	qos.len = 0;
	qos.impersonation_level = 2;
	qos.context_mode = 1;
	qos.effective_only = 0;

	attr.len = 0;
	attr.root_dir = NULL;
	attr.object_name = NULL;
	attr.attributes = 0;
	attr.sec_desc = NULL;
	attr.sec_qos = &qos;

	r.in.system_name = "\\";
	r.in.attr = &attr;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle = *handle;

	status = dcerpc_lsa_OpenPolicy2(p, mem_ctx, &r);

	return NT_STATUS_IS_OK(status);
}

static bool get_domainsid(TALLOC_CTX *mem_ctx, struct dcerpc_pipe *p,
			  struct policy_handle *handle,
			  struct dom_sid **sid)
{
	struct lsa_QueryInfoPolicy r;
	union lsa_PolicyInformation *info = NULL;
	NTSTATUS status;

	r.in.level = LSA_POLICY_INFO_DOMAIN;
	r.in.handle = handle;
	r.out.info = &info;

	status = dcerpc_lsa_QueryInfoPolicy(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) return false;

	*sid = info->domain.sid;
	return true;
}

static NTSTATUS lookup_sids(TALLOC_CTX *mem_ctx, uint16_t level,
			    struct dcerpc_pipe *p,
			    struct policy_handle *handle,
			    struct dom_sid **sids, uint32_t num_sids,
			    struct lsa_TransNameArray *names)
{
	struct lsa_LookupSids r;
	struct lsa_SidArray sidarray;
	struct lsa_RefDomainList *domains;
	uint32_t count = 0;
	uint32_t i;

	names->count = 0;
	names->names = NULL;

	sidarray.num_sids = num_sids;
	sidarray.sids = talloc_array(mem_ctx, struct lsa_SidPtr, num_sids);

	for (i=0; i<num_sids; i++) {
		sidarray.sids[i].sid = sids[i];
	}

	r.in.handle = handle;
	r.in.sids = &sidarray;
	r.in.names = names;
	r.in.level = level;
	r.in.count = &count;
	r.out.names = names;
	r.out.count = &count;
	r.out.domains = &domains;

	return dcerpc_lsa_LookupSids(p, mem_ctx, &r);
}

static const char *sid_type_lookup(enum lsa_SidType r)
{
	switch (r) {
		case SID_NAME_USE_NONE: return "SID_NAME_USE_NONE"; break;
		case SID_NAME_USER: return "SID_NAME_USER"; break;
		case SID_NAME_DOM_GRP: return "SID_NAME_DOM_GRP"; break;
		case SID_NAME_DOMAIN: return "SID_NAME_DOMAIN"; break;
		case SID_NAME_ALIAS: return "SID_NAME_ALIAS"; break;
		case SID_NAME_WKN_GRP: return "SID_NAME_WKN_GRP"; break;
		case SID_NAME_DELETED: return "SID_NAME_DELETED"; break;
		case SID_NAME_INVALID: return "SID_NAME_INVALID"; break;
		case SID_NAME_UNKNOWN: return "SID_NAME_UNKNOWN"; break;
		case SID_NAME_COMPUTER: return "SID_NAME_COMPUTER"; break;
	}
	return "Invalid sid type\n";
}

static bool test_lookupsids(TALLOC_CTX *mem_ctx, struct dcerpc_pipe *p,
			    struct policy_handle *handle,
			    struct dom_sid **sids, uint32_t num_sids,
			    int level, NTSTATUS expected_result, 
			    enum lsa_SidType *types)
{
	struct lsa_TransNameArray names;
	NTSTATUS status;
	uint32_t i;
	bool ret = true;

	status = lookup_sids(mem_ctx, level, p, handle, sids, num_sids,
			     &names);
	if (!NT_STATUS_EQUAL(status, expected_result)) {
		printf("For level %d expected %s, got %s\n",
		       level, nt_errstr(expected_result),
		       nt_errstr(status));
		return false;
	}

	if (!NT_STATUS_EQUAL(status, NT_STATUS_OK) &&
	    !NT_STATUS_EQUAL(status, STATUS_SOME_UNMAPPED)) {
		return true;
	}

	for (i=0; i<num_sids; i++) {
		if (names.names[i].sid_type != types[i]) {
			printf("In level %d, for sid %s expected %s, "
			       "got %s\n", level,
			       dom_sid_string(mem_ctx, sids[i]),
			       sid_type_lookup(types[i]),
			       sid_type_lookup(names.names[i].sid_type));
			ret = false;
		}
	}
	return ret;
}

static bool get_downleveltrust(struct torture_context *tctx, struct dcerpc_pipe *p,
			       struct policy_handle *handle,
			       struct dom_sid **sid)
{
	struct lsa_EnumTrustDom r;
	uint32_t resume_handle = 0;
	struct lsa_DomainList domains;
	NTSTATUS status;
	int i;

	r.in.handle = handle;
	r.in.resume_handle = &resume_handle;
	r.in.max_size = 1000;
	r.out.domains = &domains;
	r.out.resume_handle = &resume_handle;

	status = dcerpc_lsa_EnumTrustDom(p, tctx, &r);

	if (NT_STATUS_EQUAL(status, NT_STATUS_NO_MORE_ENTRIES))
		torture_fail(tctx, "no trusts");

	if (domains.count == 0) {
		torture_fail(tctx, "no trusts");
	}

	for (i=0; i<domains.count; i++) {
		struct lsa_QueryTrustedDomainInfoBySid q;
		union lsa_TrustedDomainInfo *info = NULL;

		if (domains.domains[i].sid == NULL)
			continue;

		q.in.handle = handle;
		q.in.dom_sid = domains.domains[i].sid;
		q.in.level = 6;
		q.out.info = &info;

		status = dcerpc_lsa_QueryTrustedDomainInfoBySid(p, tctx, &q);
		if (!NT_STATUS_IS_OK(status)) continue;

		if ((info->info_ex.trust_direction & 2) &&
		    (info->info_ex.trust_type == 1)) {
			*sid = domains.domains[i].sid;
			return true;
		}
	}

	torture_fail(tctx, "I need a AD DC with an outgoing trust to NT4");
}

#define NUM_SIDS 8

bool torture_rpc_lsa_lookup(struct torture_context *torture)
{
        NTSTATUS status;
        struct dcerpc_pipe *p;
	bool ret = true;
	struct policy_handle *handle;
	struct dom_sid *dom_sid;
	struct dom_sid *trusted_sid;
	struct dom_sid *sids[NUM_SIDS];

	status = torture_rpc_connection(torture, &p, &ndr_table_lsarpc);
	if (!NT_STATUS_IS_OK(status)) {
		torture_fail(torture, "unable to connect to table");
	}

	ret &= open_policy(torture, p, &handle);
	if (!ret) return false;

	ret &= get_domainsid(torture, p, handle, &dom_sid);
	if (!ret) return false;

	ret &= get_downleveltrust(torture, p, handle, &trusted_sid);
	if (!ret) return false;

	torture_comment(torture, "domain sid: %s\n", 
					dom_sid_string(torture, dom_sid));

	sids[0] = dom_sid_parse_talloc(torture, "S-1-1-0");
	sids[1] = dom_sid_parse_talloc(torture, "S-1-5-4");
	sids[2] = dom_sid_parse_talloc(torture, "S-1-5-32");
	sids[3] = dom_sid_parse_talloc(torture, "S-1-5-32-545");
	sids[4] = dom_sid_dup(torture, dom_sid);
	sids[5] = dom_sid_add_rid(torture, dom_sid, 512);
	sids[6] = dom_sid_dup(torture, trusted_sid);
	sids[7] = dom_sid_add_rid(torture, trusted_sid, 512);

	ret &= test_lookupsids(torture, p, handle, sids, NUM_SIDS, 0,
			       NT_STATUS_INVALID_PARAMETER, NULL);

	{
		enum lsa_SidType types[NUM_SIDS] =
			{ SID_NAME_WKN_GRP, SID_NAME_WKN_GRP, SID_NAME_DOMAIN,
			  SID_NAME_ALIAS, SID_NAME_DOMAIN, SID_NAME_DOM_GRP,
			  SID_NAME_DOMAIN, SID_NAME_DOM_GRP };

		ret &= test_lookupsids(torture, p, handle, sids, NUM_SIDS, 1,
				       NT_STATUS_OK, types);
	}

	{
		enum lsa_SidType types[NUM_SIDS] =
			{ SID_NAME_UNKNOWN, SID_NAME_UNKNOWN,
			  SID_NAME_UNKNOWN, SID_NAME_UNKNOWN,
			  SID_NAME_DOMAIN, SID_NAME_DOM_GRP,
			  SID_NAME_DOMAIN, SID_NAME_DOM_GRP };
		ret &= test_lookupsids(torture, p, handle, sids, NUM_SIDS, 2,
				       STATUS_SOME_UNMAPPED, types);
	}

	{
		enum lsa_SidType types[NUM_SIDS] =
			{ SID_NAME_UNKNOWN, SID_NAME_UNKNOWN,
			  SID_NAME_UNKNOWN, SID_NAME_UNKNOWN,
			  SID_NAME_DOMAIN, SID_NAME_DOM_GRP,
			  SID_NAME_UNKNOWN, SID_NAME_UNKNOWN };
		ret &= test_lookupsids(torture, p, handle, sids, NUM_SIDS, 3,
				       STATUS_SOME_UNMAPPED, types);
	}

	{
		enum lsa_SidType types[NUM_SIDS] =
			{ SID_NAME_UNKNOWN, SID_NAME_UNKNOWN,
			  SID_NAME_UNKNOWN, SID_NAME_UNKNOWN,
			  SID_NAME_DOMAIN, SID_NAME_DOM_GRP,
			  SID_NAME_UNKNOWN, SID_NAME_UNKNOWN };
		ret &= test_lookupsids(torture, p, handle, sids, NUM_SIDS, 4,
				       STATUS_SOME_UNMAPPED, types);
	}

	ret &= test_lookupsids(torture, p, handle, sids, NUM_SIDS, 5,
			       NT_STATUS_NONE_MAPPED, NULL);

	{
		enum lsa_SidType types[NUM_SIDS] =
			{ SID_NAME_UNKNOWN, SID_NAME_UNKNOWN,
			  SID_NAME_UNKNOWN, SID_NAME_UNKNOWN,
			  SID_NAME_DOMAIN, SID_NAME_DOM_GRP,
			  SID_NAME_UNKNOWN, SID_NAME_UNKNOWN };
		ret &= test_lookupsids(torture, p, handle, sids, NUM_SIDS, 6,
				       STATUS_SOME_UNMAPPED, types);
	}

	ret &= test_lookupsids(torture, p, handle, sids, NUM_SIDS, 7,
			       NT_STATUS_INVALID_PARAMETER, NULL);
	ret &= test_lookupsids(torture, p, handle, sids, NUM_SIDS, 8,
			       NT_STATUS_INVALID_PARAMETER, NULL);
	ret &= test_lookupsids(torture, p, handle, sids, NUM_SIDS, 9,
			       NT_STATUS_INVALID_PARAMETER, NULL);
	ret &= test_lookupsids(torture, p, handle, sids, NUM_SIDS, 10,
			       NT_STATUS_INVALID_PARAMETER, NULL);

	return ret;
}

static bool test_LookupSidsReply(struct torture_context *tctx,
				 struct dcerpc_pipe *p)
{
	struct policy_handle *handle;

	struct dom_sid **sids;
	uint32_t num_sids = 1;

	struct lsa_LookupSids r;
	struct lsa_SidArray sidarray;
	struct lsa_RefDomainList *domains = NULL;
	struct lsa_TransNameArray names;
	uint32_t count = 0;

	uint32_t i;
	NTSTATUS status;
	const char *dom_sid = "S-1-5-21-1111111111-2222222222-3333333333";
	const char *dom_admin_sid;

	if (!open_policy(tctx, p, &handle)) {
		return false;
	}

	dom_admin_sid = talloc_asprintf(tctx, "%s-%d", dom_sid, 512);

	sids = talloc_array(tctx, struct dom_sid *, num_sids);

	sids[0] = dom_sid_parse_talloc(tctx, dom_admin_sid);

	names.count = 0;
	names.names = NULL;

	sidarray.num_sids = num_sids;
	sidarray.sids = talloc_array(tctx, struct lsa_SidPtr, num_sids);

	for (i=0; i<num_sids; i++) {
		sidarray.sids[i].sid = sids[i];
	}

	r.in.handle	= handle;
	r.in.sids	= &sidarray;
	r.in.names	= &names;
	r.in.level	= LSA_LOOKUP_NAMES_ALL;
	r.in.count	= &count;
	r.out.names	= &names;
	r.out.count	= &count;
	r.out.domains	= &domains;

	status = dcerpc_lsa_LookupSids(p, tctx, &r);

	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_NONE_MAPPED,
		"unexpected error code");

	torture_assert_int_equal(tctx, names.count, num_sids,
		"unexpected names count");
	torture_assert(tctx, names.names,
		"unexpected names pointer");
	torture_assert_str_equal(tctx, names.names[0].name.string, dom_admin_sid,
		"unexpected names[0].string");

#if 0
	/* vista sp1 passes, w2k3 sp2 fails */
	torture_assert_int_equal(tctx, domains->count, num_sids,
		"unexpected domains count");
	torture_assert(tctx, domains->domains,
		"unexpected domains pointer");
	torture_assert_str_equal(tctx, dom_sid_string(tctx, domains->domains[0].sid), dom_sid,
		"unexpected domain sid");
#endif

	return true;
}

/* check for lookup sids results */
struct torture_suite *torture_rpc_lsa_lookup_sids(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite;
	struct torture_rpc_tcase *tcase;

	suite = torture_suite_create(mem_ctx, "LSA-LOOKUPSIDS");
	tcase = torture_suite_add_rpc_iface_tcase(suite, "lsa",
						  &ndr_table_lsarpc);

	torture_rpc_tcase_add_test(tcase, "LookupSidsReply", test_LookupSidsReply);

	return suite;
}
