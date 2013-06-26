/*
   Unix SMB/CIFS implementation.

   test suite for netlogon rpc operations

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2003-2004
   Copyright (C) Tim Potter      2003

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
#include "../lib/util/dlinklist.h"
#include "../lib/crypto/crypto.h"
#include "system/time.h"
#include "torture/rpc/torture_rpc.h"
#include "auth/gensec/gensec.h"
#include "libcli/auth/libcli_auth.h"
#include "libcli/samsync/samsync.h"
#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "librpc/gen_ndr/ndr_netlogon_c.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "param/param.h"

#define TEST_MACHINE_NAME "samsynctest"
#define TEST_WKSTA_MACHINE_NAME "samsynctest2"
#define TEST_USER_NAME "samsynctestuser"

/*
  try a netlogon SamLogon
*/
static NTSTATUS test_SamLogon(struct torture_context *tctx,
			      struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			      struct netlogon_creds_CredentialState *creds,
			      const char *domain, const char *account_name,
			      const char *workstation,
			      struct samr_Password *lm_hash,
			      struct samr_Password *nt_hash,
			      struct netr_SamInfo3 **info3)
{
	NTSTATUS status;
	struct netr_LogonSamLogon r;
	struct netr_Authenticator auth, auth2;
	struct netr_NetworkInfo ninfo;
	union netr_LogonLevel logon;
	union netr_Validation validation;
	uint8_t authoritative;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ninfo.identity_info.domain_name.string = domain;
	ninfo.identity_info.parameter_control = 0;
	ninfo.identity_info.logon_id_low = 0;
	ninfo.identity_info.logon_id_high = 0;
	ninfo.identity_info.account_name.string = account_name;
	ninfo.identity_info.workstation.string = workstation;
	generate_random_buffer(ninfo.challenge,
			       sizeof(ninfo.challenge));
	if (nt_hash) {
		ninfo.nt.length = 24;
		ninfo.nt.data = talloc_array(mem_ctx, uint8_t, 24);
		SMBOWFencrypt(nt_hash->hash, ninfo.challenge, ninfo.nt.data);
	} else {
		ninfo.nt.length = 0;
		ninfo.nt.data = NULL;
	}

	if (lm_hash) {
		ninfo.lm.length = 24;
		ninfo.lm.data = talloc_array(mem_ctx, uint8_t, 24);
		SMBOWFencrypt(lm_hash->hash, ninfo.challenge, ninfo.lm.data);
	} else {
		ninfo.lm.length = 0;
		ninfo.lm.data = NULL;
	}

	logon.network = &ninfo;

	r.in.server_name = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computer_name = workstation;
	r.in.credential = &auth;
	r.in.return_authenticator = &auth2;
	r.in.logon_level = 2;
	r.in.logon = &logon;
	r.out.validation = &validation;
	r.out.authoritative = &authoritative;

	ZERO_STRUCT(auth2);
	netlogon_creds_client_authenticator(creds, &auth);

	r.in.validation_level = 3;

	status = dcerpc_netr_LogonSamLogon_r(b, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!netlogon_creds_client_check(creds, &r.out.return_authenticator->cred)) {
		torture_comment(tctx, "Credential chaining failed\n");
	}

	if (info3) {
		*info3 = validation.sam3;
	}

	return r.out.result;
}

struct samsync_state {
/* we remember the sequence numbers so we can easily do a DatabaseDelta */
	uint64_t seq_num[3];
	const char *domain_name[2];
	struct samsync_secret *secrets;
	struct samsync_trusted_domain *trusted_domains;
	struct netlogon_creds_CredentialState *creds;
	struct netlogon_creds_CredentialState *creds_netlogon_wksta;
	struct policy_handle *connect_handle;
	struct policy_handle *domain_handle[2];
	struct dom_sid *sid[2];
	struct dcerpc_pipe *p;
	struct dcerpc_binding_handle *b;
	struct dcerpc_pipe *p_netlogon_wksta;
	struct dcerpc_pipe *p_samr;
	struct dcerpc_binding_handle *b_samr;
	struct dcerpc_pipe *p_lsa;
	struct dcerpc_binding_handle *b_lsa;
	struct policy_handle *lsa_handle;
};

struct samsync_secret {
	struct samsync_secret *prev, *next;
	DATA_BLOB secret;
	const char *name;
	NTTIME mtime;
};

struct samsync_trusted_domain {
	struct samsync_trusted_domain *prev, *next;
        struct dom_sid *sid;
	const char *name;
};

static struct policy_handle *samsync_open_domain(struct torture_context *tctx,
						 TALLOC_CTX *mem_ctx,
						 struct samsync_state *samsync_state,
						 const char *domain,
						 struct dom_sid **sid_p)
{
	struct lsa_String name;
	struct samr_OpenDomain o;
	struct samr_LookupDomain l;
	struct dom_sid2 *sid = NULL;
	struct policy_handle *domain_handle = talloc(mem_ctx, struct policy_handle);
	NTSTATUS nt_status;

	name.string = domain;
	l.in.connect_handle = samsync_state->connect_handle;
	l.in.domain_name = &name;
	l.out.sid = &sid;

	nt_status = dcerpc_samr_LookupDomain_r(samsync_state->b_samr, mem_ctx, &l);
	if (!NT_STATUS_IS_OK(nt_status)) {
		torture_comment(tctx, "LookupDomain failed - %s\n", nt_errstr(nt_status));
		return NULL;
	}
	if (!NT_STATUS_IS_OK(l.out.result)) {
		torture_comment(tctx, "LookupDomain failed - %s\n", nt_errstr(l.out.result));
		return NULL;
	}

	o.in.connect_handle = samsync_state->connect_handle;
	o.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	o.in.sid = *l.out.sid;
	o.out.domain_handle = domain_handle;

	if (sid_p) {
		*sid_p = *l.out.sid;
	}

	nt_status = dcerpc_samr_OpenDomain_r(samsync_state->b_samr, mem_ctx, &o);
	if (!NT_STATUS_IS_OK(nt_status)) {
		torture_comment(tctx, "OpenDomain failed - %s\n", nt_errstr(nt_status));
		return NULL;
	}
	if (!NT_STATUS_IS_OK(o.out.result)) {
		torture_comment(tctx, "OpenDomain failed - %s\n", nt_errstr(o.out.result));
		return NULL;
	}

	return domain_handle;
}

static struct sec_desc_buf *samsync_query_samr_sec_desc(struct torture_context *tctx,
							TALLOC_CTX *mem_ctx,
							struct samsync_state *samsync_state,
							struct policy_handle *handle)
{
	struct samr_QuerySecurity r;
	struct sec_desc_buf *sdbuf = NULL;
	NTSTATUS status;

	r.in.handle = handle;
	r.in.sec_info = 0x7;
	r.out.sdbuf = &sdbuf;

	status = dcerpc_samr_QuerySecurity_r(samsync_state->b_samr, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "SAMR QuerySecurity failed - %s\n", nt_errstr(status));
		return NULL;
	}
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_comment(tctx, "SAMR QuerySecurity failed - %s\n", nt_errstr(r.out.result));
		return NULL;
	}

	return sdbuf;
}

static struct sec_desc_buf *samsync_query_lsa_sec_desc(struct torture_context *tctx,
						       TALLOC_CTX *mem_ctx,
						       struct samsync_state *samsync_state,
						       struct policy_handle *handle)
{
	struct lsa_QuerySecurity r;
	struct sec_desc_buf *sdbuf = NULL;
	NTSTATUS status;

	r.in.handle = handle;
	r.in.sec_info = 0x7;
	r.out.sdbuf = &sdbuf;

	status = dcerpc_lsa_QuerySecurity_r(samsync_state->b_lsa, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "LSA QuerySecurity failed - %s\n", nt_errstr(status));
		return NULL;
	}
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_comment(tctx, "LSA QuerySecurity failed - %s\n", nt_errstr(r.out.result));
		return NULL;
	}

	return sdbuf;
}

#define TEST_UINT64_EQUAL(i1, i2) do {\
	if (i1 != i2) {\
              torture_comment(tctx, "%s: uint64 mismatch: " #i1 ": 0x%016llx (%lld) != " #i2 ": 0x%016llx (%lld)\n", \
		     __location__, \
		     (long long)i1, (long long)i1, \
		     (long long)i2, (long long)i2);\
	      ret = false;\
	} \
} while (0)
#define TEST_INT_EQUAL(i1, i2) do {\
	if (i1 != i2) {\
	      torture_comment(tctx, "%s: integer mismatch: " #i1 ": 0x%08x (%d) != " #i2 ": 0x%08x (%d)\n", \
		     __location__, i1, i1, i2, i2);			\
	      ret = false;\
	} \
} while (0)
#define TEST_TIME_EQUAL(t1, t2) do {\
	if (t1 != t2) {\
	      torture_comment(tctx, "%s: NTTIME mismatch: " #t1 ":%s != " #t2 ": %s\n", \
		     __location__, nt_time_string(mem_ctx, t1),  nt_time_string(mem_ctx, t2));\
	      ret = false;\
	} \
} while (0)

#define TEST_STRING_EQUAL(s1, s2) do {\
	if (!((!s1.string || s1.string[0]=='\0') && (!s2.string || s2.string[0]=='\0')) \
	    && strcmp_safe(s1.string, s2.string) != 0) {\
	      torture_comment(tctx, "%s: string mismatch: " #s1 ":%s != " #s2 ": %s\n", \
		     __location__, s1.string, s2.string);\
	      ret = false;\
	} \
} while (0)

#define TEST_BINARY_STRING_EQUAL(s1, s2) do {\
	if (!((!s1.array || s1.array[0]=='\0') && (!s2.array || s2.array[0]=='\0')) \
	    && memcmp(s1.array, s2.array, s1.length * 2) != 0) {\
	      torture_comment(tctx, "%s: string mismatch: " #s1 ":%s != " #s2 ": %s\n", \
		     __location__, (const char *)s1.array, (const char *)s2.array);\
	      ret = false;\
	} \
} while (0)

#define TEST_SID_EQUAL(s1, s2) do {\
	if (!dom_sid_equal(s1, s2)) {\
	      torture_comment(tctx, "%s: dom_sid mismatch: " #s1 ":%s != " #s2 ": %s\n", \
		     __location__, dom_sid_string(mem_ctx, s1), dom_sid_string(mem_ctx, s2));\
	      ret = false;\
	} \
} while (0)

/* The ~SEC_DESC_SACL_PRESENT is because we don't, as administrator,
 * get back the SACL part of the SD when we ask over SAMR */

#define TEST_SEC_DESC_EQUAL(sd1, pipe, handle) do {\
        struct sec_desc_buf *sdbuf = samsync_query_ ##pipe## _sec_desc(tctx, mem_ctx, samsync_state, \
						            handle); \
	if (!sdbuf || !sdbuf->sd) { \
                torture_comment(tctx, "Could not obtain security descriptor to match " #sd1 "\n");\
	        ret = false; \
        } else {\
		if (!security_descriptor_mask_equal(sd1.sd, sdbuf->sd, \
 			    ~SEC_DESC_SACL_PRESENT)) {\
			torture_comment(tctx, "Security Descriptor Mismatch for %s:\n", #sd1);\
		        ndr_print_debug((ndr_print_fn_t)ndr_print_security_descriptor, "SamSync", sd1.sd);\
		        ndr_print_debug((ndr_print_fn_t)ndr_print_security_descriptor, "SamR", sdbuf->sd);\
			ret = false;\
		}\
	}\
} while (0)

static bool samsync_handle_domain(struct torture_context *tctx, TALLOC_CTX *mem_ctx, struct samsync_state *samsync_state,
			   int database_id, struct netr_DELTA_ENUM *delta)
{
	struct netr_DELTA_DOMAIN *domain = delta->delta_union.domain;
	struct dom_sid *dom_sid;
	struct samr_QueryDomainInfo q[14]; /* q[0] will be unused simple for clarity */
	union samr_DomainInfo *info[14];
	uint16_t levels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13};
	int i;
	bool ret = true;

	samsync_state->seq_num[database_id] =
		domain->sequence_num;
	switch (database_id) {
	case SAM_DATABASE_DOMAIN:
		break;
	case SAM_DATABASE_BUILTIN:
		if (strcasecmp_m("BUILTIN", domain->domain_name.string) != 0) {
			torture_comment(tctx, "BUILTIN domain has different name: %s\n", domain->domain_name.string);
		}
		break;
	case SAM_DATABASE_PRIVS:
		torture_comment(tctx, "DOMAIN entry on privs DB!\n");
		return false;
		break;
	}

	if (!samsync_state->domain_name[database_id]) {
		samsync_state->domain_name[database_id] =
			talloc_reference(samsync_state, domain->domain_name.string);
	} else {
		if (strcasecmp_m(samsync_state->domain_name[database_id], domain->domain_name.string) != 0) {
			torture_comment(tctx, "Domain has name varies!: %s != %s\n", samsync_state->domain_name[database_id],
			       domain->domain_name.string);
			return false;
		}
	}

	if (!samsync_state->domain_handle[database_id]) {
		samsync_state->domain_handle[database_id]
			= talloc_reference(samsync_state,
					   samsync_open_domain(tctx,
							       mem_ctx, samsync_state, samsync_state->domain_name[database_id],
							       &dom_sid));
	}
	if (samsync_state->domain_handle[database_id]) {
		samsync_state->sid[database_id] = talloc_reference(samsync_state, dom_sid);
	}

	torture_comment(tctx, "\tsequence_nums[%d/%s]=%llu\n",
	       database_id, domain->domain_name.string,
	       (long long)samsync_state->seq_num[database_id]);

	for (i=0;i<ARRAY_SIZE(levels);i++) {

		q[levels[i]].in.domain_handle = samsync_state->domain_handle[database_id];
		q[levels[i]].in.level = levels[i];
		q[levels[i]].out.info = &info[levels[i]];

		torture_assert_ntstatus_ok(tctx,
			dcerpc_samr_QueryDomainInfo_r(samsync_state->b_samr, mem_ctx, &q[levels[i]]),
			talloc_asprintf(tctx, "QueryDomainInfo level %u failed", q[levels[i]].in.level));
		torture_assert_ntstatus_ok(tctx, q[levels[i]].out.result,
			talloc_asprintf(tctx, "QueryDomainInfo level %u failed", q[levels[i]].in.level));
	}

	TEST_STRING_EQUAL(info[5]->info5.domain_name, domain->domain_name);

	TEST_STRING_EQUAL(info[2]->general.oem_information, domain->oem_information);
	TEST_STRING_EQUAL(info[4]->oem.oem_information, domain->oem_information);
	TEST_TIME_EQUAL(info[2]->general.force_logoff_time, domain->force_logoff_time);
	TEST_TIME_EQUAL(info[3]->info3.force_logoff_time, domain->force_logoff_time);

	TEST_TIME_EQUAL(info[1]->info1.min_password_length, domain->min_password_length);
	TEST_TIME_EQUAL(info[1]->info1.password_history_length, domain->password_history_length);
	TEST_TIME_EQUAL(info[1]->info1.max_password_age, domain->max_password_age);
	TEST_TIME_EQUAL(info[1]->info1.min_password_age, domain->min_password_age);

	TEST_UINT64_EQUAL(info[8]->info8.sequence_num,
			domain->sequence_num);
	TEST_TIME_EQUAL(info[8]->info8.domain_create_time,
			domain->domain_create_time);
	TEST_TIME_EQUAL(info[13]->info13.domain_create_time,
			domain->domain_create_time);

	TEST_SEC_DESC_EQUAL(domain->sdbuf, samr, samsync_state->domain_handle[database_id]);

	return ret;
}

static bool samsync_handle_policy(struct torture_context *tctx,
				  TALLOC_CTX *mem_ctx, struct samsync_state *samsync_state,
			   int database_id, struct netr_DELTA_ENUM *delta)
{
	struct netr_DELTA_POLICY *policy = delta->delta_union.policy;

	samsync_state->seq_num[database_id] =
		policy->sequence_num;

	if (!samsync_state->domain_name[SAM_DATABASE_DOMAIN]) {
		samsync_state->domain_name[SAM_DATABASE_DOMAIN] =
			talloc_reference(samsync_state, policy->primary_domain_name.string);
	} else {
		if (strcasecmp_m(samsync_state->domain_name[SAM_DATABASE_DOMAIN], policy->primary_domain_name.string) != 0) {
			torture_comment(tctx, "PRIMARY domain has name varies between DOMAIN and POLICY!: %s != %s\n", samsync_state->domain_name[SAM_DATABASE_DOMAIN],
			       policy->primary_domain_name.string);
			return false;
		}
	}

	if (!dom_sid_equal(samsync_state->sid[SAM_DATABASE_DOMAIN], policy->sid)) {
		torture_comment(tctx, "Domain SID from POLICY (%s) does not match domain sid from SAMR (%s)\n",
		       dom_sid_string(mem_ctx, policy->sid), dom_sid_string(mem_ctx, samsync_state->sid[SAM_DATABASE_DOMAIN]));
		return false;
	}

	torture_comment(tctx, "\tsequence_nums[%d/PRIVS]=%llu\n",
	       database_id,
	       (long long)samsync_state->seq_num[database_id]);
	return true;
}

static bool samsync_handle_user(struct torture_context *tctx, TALLOC_CTX *mem_ctx, struct samsync_state *samsync_state,
				int database_id, struct netr_DELTA_ENUM *delta)
{
	uint32_t rid = delta->delta_id_union.rid;
	struct netr_DELTA_USER *user = delta->delta_union.user;
	struct netr_SamInfo3 *info3;
	struct samr_Password lm_hash;
	struct samr_Password nt_hash;
	struct samr_Password *lm_hash_p = NULL;
	struct samr_Password *nt_hash_p = NULL;
	const char *domain = samsync_state->domain_name[database_id];
	const char *username = user->account_name.string;
	NTSTATUS nt_status;
	bool ret = true;

	struct samr_OpenUser r;
	struct samr_QueryUserInfo q;
	union samr_UserInfo *info;
	struct policy_handle user_handle;

	struct samr_GetGroupsForUser getgroups;
	struct samr_RidWithAttributeArray *rids;

	if (!samsync_state->domain_name || !samsync_state->domain_handle[database_id]) {
		torture_comment(tctx, "SamSync needs domain information before the users\n");
		return false;
	}

	r.in.domain_handle = samsync_state->domain_handle[database_id];
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.rid = rid;
	r.out.user_handle = &user_handle;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_samr_OpenUser_r(samsync_state->b_samr, mem_ctx, &r),
		talloc_asprintf(tctx, "OpenUser(%u) failed", rid));
	torture_assert_ntstatus_ok(tctx, r.out.result,
		talloc_asprintf(tctx, "OpenUser(%u) failed", rid));

	q.in.user_handle = &user_handle;
	q.in.level = 21;
	q.out.info = &info;

	TEST_SEC_DESC_EQUAL(user->sdbuf, samr, &user_handle);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_samr_QueryUserInfo_r(samsync_state->b_samr, mem_ctx, &q),
		talloc_asprintf(tctx, "OpenUserInfo level %u failed", q.in.level));
	torture_assert_ntstatus_ok(tctx, q.out.result,
		talloc_asprintf(tctx, "OpenUserInfo level %u failed", q.in.level));

	getgroups.in.user_handle = &user_handle;
	getgroups.out.rids = &rids;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_samr_GetGroupsForUser_r(samsync_state->b_samr, mem_ctx, &getgroups),
		"GetGroupsForUser failed");
	torture_assert_ntstatus_ok(tctx, getgroups.out.result,
		"GetGroupsForUser failed");

	if (!test_samr_handle_Close(samsync_state->b_samr, tctx, &user_handle)) {
		torture_comment(tctx, "samr_handle_Close failed\n");
		ret = false;
	}
	if (!ret) {
		return false;
	}

	TEST_STRING_EQUAL(info->info21.account_name, user->account_name);
	TEST_STRING_EQUAL(info->info21.full_name, user->full_name);
	TEST_INT_EQUAL(info->info21.rid, user->rid);
	TEST_INT_EQUAL(info->info21.primary_gid, user->primary_gid);
	TEST_STRING_EQUAL(info->info21.home_directory, user->home_directory);
	TEST_STRING_EQUAL(info->info21.home_drive, user->home_drive);
	TEST_STRING_EQUAL(info->info21.logon_script, user->logon_script);
	TEST_STRING_EQUAL(info->info21.description, user->description);
	TEST_STRING_EQUAL(info->info21.workstations, user->workstations);

	TEST_TIME_EQUAL(info->info21.last_logon, user->last_logon);
	TEST_TIME_EQUAL(info->info21.last_logoff, user->last_logoff);


	TEST_INT_EQUAL(info->info21.logon_hours.units_per_week,
		       user->logon_hours.units_per_week);
	if (ret) {
		if (memcmp(info->info21.logon_hours.bits, user->logon_hours.bits,
			   info->info21.logon_hours.units_per_week/8) != 0) {
			torture_comment(tctx, "Logon hours mismatch\n");
			ret = false;
		}
	}

	TEST_INT_EQUAL(info->info21.bad_password_count,
		       user->bad_password_count);
	TEST_INT_EQUAL(info->info21.logon_count,
		       user->logon_count);

	TEST_TIME_EQUAL(info->info21.last_password_change,
		       user->last_password_change);
	TEST_TIME_EQUAL(info->info21.acct_expiry,
		       user->acct_expiry);

	TEST_INT_EQUAL((info->info21.acct_flags & ~ACB_PW_EXPIRED), user->acct_flags);
	if (user->acct_flags & ACB_PWNOEXP) {
		if (info->info21.acct_flags & ACB_PW_EXPIRED) {
			torture_comment(tctx, "ACB flags mismatch: both expired and no expiry!\n");
			ret = false;
		}
		if (info->info21.force_password_change != (NTTIME)0x7FFFFFFFFFFFFFFFULL) {
			torture_comment(tctx, "ACB flags mismatch: no password expiry, but force password change 0x%016llx (%lld) != 0x%016llx (%lld)\n",
			       (unsigned long long)info->info21.force_password_change,
			       (unsigned long long)info->info21.force_password_change,
			       (unsigned long long)0x7FFFFFFFFFFFFFFFULL, (unsigned long long)0x7FFFFFFFFFFFFFFFULL
				);
			ret = false;
		}
	}

	TEST_INT_EQUAL(info->info21.nt_password_set, user->nt_password_present);
	TEST_INT_EQUAL(info->info21.lm_password_set, user->lm_password_present);
	TEST_INT_EQUAL(info->info21.password_expired, user->password_expired);

	TEST_STRING_EQUAL(info->info21.comment, user->comment);
	TEST_BINARY_STRING_EQUAL(info->info21.parameters, user->parameters);

	TEST_INT_EQUAL(info->info21.country_code, user->country_code);
	TEST_INT_EQUAL(info->info21.code_page, user->code_page);

	TEST_STRING_EQUAL(info->info21.profile_path, user->profile_path);

	if (user->lm_password_present) {
		lm_hash_p = &lm_hash;
	}
	if (user->nt_password_present) {
		nt_hash_p = &nt_hash;
	}

	if (user->user_private_info.SensitiveData) {
		DATA_BLOB data;
		struct netr_USER_KEYS keys;
		enum ndr_err_code ndr_err;
		data.data = user->user_private_info.SensitiveData;
		data.length = user->user_private_info.DataLength;
		ndr_err = ndr_pull_struct_blob(&data, mem_ctx, &keys, (ndr_pull_flags_fn_t)ndr_pull_netr_USER_KEYS);
		if (NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			if (keys.keys.keys2.lmpassword.length == 16) {
				lm_hash_p = &lm_hash;
			}
			if (keys.keys.keys2.ntpassword.length == 16) {
				nt_hash_p = &nt_hash;
			}
		} else {
			torture_comment(tctx, "Failed to parse Sensitive Data for %s:\n", username);
#if 0
			dump_data(0, data.data, data.length);
#endif
			return false;
		}
	}

	if (nt_hash_p) {
		DATA_BLOB nt_hash_blob = data_blob_const(nt_hash_p, 16);
		DEBUG(100,("ACCOUNT [%s\\%-25s] NTHASH %s\n", samsync_state->domain_name[0], username, data_blob_hex_string_upper(mem_ctx, &nt_hash_blob)));
	}
	if (lm_hash_p) {
		DATA_BLOB lm_hash_blob = data_blob_const(lm_hash_p, 16);
		DEBUG(100,("ACCOUNT [%s\\%-25s] LMHASH %s\n", samsync_state->domain_name[0], username, data_blob_hex_string_upper(mem_ctx, &lm_hash_blob)));
	}

	nt_status = test_SamLogon(tctx,
				  samsync_state->p_netlogon_wksta, mem_ctx, samsync_state->creds_netlogon_wksta,
				  domain,
				  username,
				  TEST_WKSTA_MACHINE_NAME,
				  lm_hash_p,
				  nt_hash_p,
				  &info3);

	if (NT_STATUS_EQUAL(nt_status, NT_STATUS_ACCOUNT_DISABLED)) {
		if (user->acct_flags & ACB_DISABLED) {
			return true;
		}
	} else if (NT_STATUS_EQUAL(nt_status, NT_STATUS_NOLOGON_WORKSTATION_TRUST_ACCOUNT)) {
		if (user->acct_flags & ACB_WSTRUST) {
			return true;
		}
	} else if (NT_STATUS_EQUAL(nt_status, NT_STATUS_NOLOGON_SERVER_TRUST_ACCOUNT)) {
		if (user->acct_flags & ACB_SVRTRUST) {
			return true;
		}
	} else if (NT_STATUS_EQUAL(nt_status, NT_STATUS_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT)) {
		if (user->acct_flags & ACB_DOMTRUST) {
			return true;
		}
	} else if (NT_STATUS_EQUAL(nt_status, NT_STATUS_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT)) {
		if (user->acct_flags & ACB_DOMTRUST) {
			return true;
		}
	} else if (NT_STATUS_EQUAL(nt_status, NT_STATUS_ACCOUNT_LOCKED_OUT)) {
		if (user->acct_flags & ACB_AUTOLOCK) {
			return true;
		}
	} else if (NT_STATUS_EQUAL(nt_status, NT_STATUS_PASSWORD_EXPIRED)) {
		if (info->info21.acct_flags & ACB_PW_EXPIRED) {
			return true;
		}
	} else if (NT_STATUS_EQUAL(nt_status, NT_STATUS_WRONG_PASSWORD)) {
		if (!lm_hash_p && !nt_hash_p) {
			return true;
		}
	} else if (NT_STATUS_EQUAL(nt_status, NT_STATUS_PASSWORD_MUST_CHANGE)) {
		/* We would need to know the server's current time to test this properly */
		return true;
	} else if (NT_STATUS_IS_OK(nt_status)) {
		TEST_INT_EQUAL(user->rid, info3->base.rid);
		TEST_INT_EQUAL(user->primary_gid, info3->base.primary_gid);
		/* this is 0x0 from NT4 sp6 */
		if (info3->base.acct_flags) {
			TEST_INT_EQUAL(user->acct_flags, info3->base.acct_flags);
		}
		/* this is NULL from NT4 sp6 */
		if (info3->base.account_name.string) {
			TEST_STRING_EQUAL(user->account_name, info3->base.account_name);
		}
		/* this is NULL from Win2k3 */
		if (info3->base.full_name.string) {
			TEST_STRING_EQUAL(user->full_name, info3->base.full_name);
		}
		TEST_STRING_EQUAL(user->logon_script, info3->base.logon_script);
		TEST_STRING_EQUAL(user->profile_path, info3->base.profile_path);
		TEST_STRING_EQUAL(user->home_directory, info3->base.home_directory);
		TEST_STRING_EQUAL(user->home_drive, info3->base.home_drive);
		TEST_STRING_EQUAL(user->logon_script, info3->base.logon_script);


		TEST_TIME_EQUAL(user->last_logon, info3->base.last_logon);
		TEST_TIME_EQUAL(user->acct_expiry, info3->base.acct_expiry);
		TEST_TIME_EQUAL(user->last_password_change, info3->base.last_password_change);
		TEST_TIME_EQUAL(info->info21.force_password_change, info3->base.force_password_change);

		/* Does the concept of a logoff time ever really
		 * exist? (not in any sensible way, according to the
		 * doco I read -- abartlet) */

		/* This copes with the two different versions of 0 I see */
		/* with NT4 sp6 we have the || case */
		if (!((user->last_logoff == 0)
		      || (info3->base.last_logoff == 0x7fffffffffffffffLL))) {
			TEST_TIME_EQUAL(user->last_logoff, info3->base.last_logoff);
		}

		TEST_INT_EQUAL(rids->count, info3->base.groups.count);
		if (rids->count == info3->base.groups.count) {
			int i, j;
			int count = rids->count;
			bool *matched = talloc_zero_array(mem_ctx, bool, rids->count);

			for (i = 0; i < count; i++) {
				for (j = 0; j < count; j++) {
					if ((rids->rids[i].rid ==
					     info3->base.groups.rids[j].rid)
					    && (rids->rids[i].attributes ==
						info3->base.groups.rids[j].attributes)) {
							matched[i] = true;
						}
				}
			}

			for (i = 0; i < rids->count; i++) {
				if (matched[i] == false) {
					ret = false;
					torture_comment(tctx, "Could not find group RID %u found in getgroups in NETLOGON reply\n",
					       rids->rids[i].rid);
				}
			}
		}
		return ret;
	} else {
		torture_comment(tctx, "Could not validate password for user %s\\%s: %s\n",
		       domain, username, nt_errstr(nt_status));
		return false;
	}
	return false;
}

static bool samsync_handle_alias(struct torture_context *tctx,
				 TALLOC_CTX *mem_ctx, struct samsync_state *samsync_state,
				 int database_id, struct netr_DELTA_ENUM *delta)
{
	uint32_t rid = delta->delta_id_union.rid;
	struct netr_DELTA_ALIAS *alias = delta->delta_union.alias;
	bool ret = true;

	struct samr_OpenAlias r;
	struct samr_QueryAliasInfo q;
	union samr_AliasInfo *info;
	struct policy_handle alias_handle;

	if (!samsync_state->domain_name || !samsync_state->domain_handle[database_id]) {
		torture_comment(tctx, "SamSync needs domain information before the users\n");
		return false;
	}

	r.in.domain_handle = samsync_state->domain_handle[database_id];
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.rid = rid;
	r.out.alias_handle = &alias_handle;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_samr_OpenAlias_r(samsync_state->b_samr, mem_ctx, &r),
		talloc_asprintf(tctx, "OpenUser(%u) failed", rid));
	torture_assert_ntstatus_ok(tctx, r.out.result,
		talloc_asprintf(tctx, "OpenUser(%u) failed", rid));

	q.in.alias_handle = &alias_handle;
	q.in.level = 1;
	q.out.info = &info;

	TEST_SEC_DESC_EQUAL(alias->sdbuf, samr, &alias_handle);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_samr_QueryAliasInfo_r(samsync_state->b_samr, mem_ctx, &q),
		"QueryAliasInfo failed");
	if (!test_samr_handle_Close(samsync_state->b_samr, tctx, &alias_handle)) {
		return false;
	}

	if (!NT_STATUS_IS_OK(q.out.result)) {
		torture_comment(tctx, "QueryAliasInfo level %u failed - %s\n",
		       q.in.level, nt_errstr(q.out.result));
		return false;
	}

	TEST_STRING_EQUAL(info->all.name, alias->alias_name);
	TEST_STRING_EQUAL(info->all.description, alias->description);
	return ret;
}

static bool samsync_handle_group(struct torture_context *tctx,
				 TALLOC_CTX *mem_ctx, struct samsync_state *samsync_state,
				 int database_id, struct netr_DELTA_ENUM *delta)
{
	uint32_t rid = delta->delta_id_union.rid;
	struct netr_DELTA_GROUP *group = delta->delta_union.group;
	bool ret = true;

	struct samr_OpenGroup r;
	struct samr_QueryGroupInfo q;
	union samr_GroupInfo *info;
	struct policy_handle group_handle;

	if (!samsync_state->domain_name || !samsync_state->domain_handle[database_id]) {
		torture_comment(tctx, "SamSync needs domain information before the users\n");
		return false;
	}

	r.in.domain_handle = samsync_state->domain_handle[database_id];
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.rid = rid;
	r.out.group_handle = &group_handle;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_samr_OpenGroup_r(samsync_state->b_samr, mem_ctx, &r),
		talloc_asprintf(tctx, "OpenUser(%u) failed", rid));
	torture_assert_ntstatus_ok(tctx, r.out.result,
		talloc_asprintf(tctx, "OpenUser(%u) failed", rid));

	q.in.group_handle = &group_handle;
	q.in.level = 1;
	q.out.info = &info;

	TEST_SEC_DESC_EQUAL(group->sdbuf, samr, &group_handle);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_samr_QueryGroupInfo_r(samsync_state->b_samr, mem_ctx, &q),
		"QueryGroupInfo failed");
	if (!test_samr_handle_Close(samsync_state->b_samr, tctx, &group_handle)) {
		return false;
	}

	if (!NT_STATUS_IS_OK(q.out.result)) {
		torture_comment(tctx, "QueryGroupInfo level %u failed - %s\n",
		       q.in.level, nt_errstr(q.out.result));
		return false;
	}

	TEST_STRING_EQUAL(info->all.name, group->group_name);
	TEST_INT_EQUAL(info->all.attributes, group->attributes);
	TEST_STRING_EQUAL(info->all.description, group->description);
	return ret;
}

static bool samsync_handle_secret(struct torture_context *tctx,
				  TALLOC_CTX *mem_ctx, struct samsync_state *samsync_state,
				  int database_id, struct netr_DELTA_ENUM *delta)
{
	struct netr_DELTA_SECRET *secret = delta->delta_union.secret;
	const char *name = delta->delta_id_union.name;
	struct samsync_secret *nsec = talloc(samsync_state, struct samsync_secret);
	struct samsync_secret *old = talloc(mem_ctx, struct samsync_secret);
	struct lsa_QuerySecret q;
	struct lsa_OpenSecret o;
	struct policy_handle sec_handle;
	struct lsa_DATA_BUF_PTR bufp1;
	struct lsa_DATA_BUF_PTR bufp2;
	NTTIME nsec_mtime;
	NTTIME old_mtime;
	bool ret = true;
	DATA_BLOB lsa_blob1, lsa_blob_out, session_key;
	NTSTATUS status;

	nsec->name = talloc_reference(nsec, name);
	nsec->secret = data_blob_talloc(nsec, secret->current_cipher.cipher_data, secret->current_cipher.maxlen);
	nsec->mtime = secret->current_cipher_set_time;

	nsec = talloc_reference(samsync_state, nsec);
	DLIST_ADD(samsync_state->secrets, nsec);

	old->name = talloc_reference(old, name);
	old->secret = data_blob_const(secret->old_cipher.cipher_data, secret->old_cipher.maxlen);
	old->mtime = secret->old_cipher_set_time;

	o.in.handle = samsync_state->lsa_handle;
	o.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	o.in.name.string = name;
	o.out.sec_handle = &sec_handle;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_lsa_OpenSecret_r(samsync_state->b_lsa, mem_ctx, &o),
		"OpenSecret failed");
	torture_assert_ntstatus_ok(tctx, o.out.result,
		"OpenSecret failed");

/*
  We would like to do this, but it is NOT_SUPPORTED on win2k3
  TEST_SEC_DESC_EQUAL(secret->sdbuf, lsa, &sec_handle);
*/
	status = dcerpc_fetch_session_key(samsync_state->p_lsa, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "dcerpc_fetch_session_key failed - %s\n", nt_errstr(status));
		return false;
	}


	ZERO_STRUCT(nsec_mtime);
	ZERO_STRUCT(old_mtime);

	/* fetch the secret back again */
	q.in.sec_handle = &sec_handle;
	q.in.new_val = &bufp1;
	q.in.new_mtime = &nsec_mtime;
	q.in.old_val = &bufp2;
	q.in.old_mtime = &old_mtime;

	bufp1.buf = NULL;
	bufp2.buf = NULL;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_lsa_QuerySecret_r(samsync_state->b_lsa, mem_ctx, &q),
		"QuerySecret failed");
	if (NT_STATUS_EQUAL(NT_STATUS_ACCESS_DENIED, q.out.result)) {
		/* some things are just off limits */
		return true;
	} else if (!NT_STATUS_IS_OK(q.out.result)) {
		torture_comment(tctx, "QuerySecret failed - %s\n", nt_errstr(q.out.result));
		return false;
	}

	if (q.out.old_val->buf == NULL) {
		/* probably just not available due to ACLs */
	} else {
		lsa_blob1.data = q.out.old_val->buf->data;
		lsa_blob1.length = q.out.old_val->buf->length;

		status = sess_decrypt_blob(mem_ctx, &lsa_blob1, &session_key, &lsa_blob_out);
		if (!NT_STATUS_IS_OK(status)) {
			torture_comment(tctx, "Failed to decrypt secrets OLD blob: %s\n", nt_errstr(status));
			return false;
		}

		if (!q.out.old_mtime) {
			torture_comment(tctx, "OLD mtime not available on LSA for secret %s\n", old->name);
			ret = false;
		}
		if (old->mtime != *q.out.old_mtime) {
			torture_comment(tctx, "OLD mtime on secret %s does not match between SAMSYNC (%s) and LSA (%s)\n",
			       old->name, nt_time_string(mem_ctx, old->mtime),
			       nt_time_string(mem_ctx, *q.out.old_mtime));
			ret = false;
		}

		if (old->secret.length != lsa_blob_out.length) {
			torture_comment(tctx, "Returned secret %s doesn't match: %d != %d\n",
			       old->name, (int)old->secret.length, (int)lsa_blob_out.length);
			ret = false;
		} else if (memcmp(lsa_blob_out.data,
			   old->secret.data, old->secret.length) != 0) {
			torture_comment(tctx, "Returned secret %s doesn't match: \n",
			       old->name);
			DEBUG(1, ("SamSync Secret:\n"));
			dump_data(1, old->secret.data, old->secret.length);
			DEBUG(1, ("LSA Secret:\n"));
			dump_data(1, lsa_blob_out.data, lsa_blob_out.length);
			ret = false;
		}

	}

	if (q.out.new_val->buf == NULL) {
		/* probably just not available due to ACLs */
	} else {
		lsa_blob1.data = q.out.new_val->buf->data;
		lsa_blob1.length = q.out.new_val->buf->length;

		status = sess_decrypt_blob(mem_ctx, &lsa_blob1, &session_key, &lsa_blob_out);
		if (!NT_STATUS_IS_OK(status)) {
			torture_comment(tctx, "Failed to decrypt secrets OLD blob\n");
			return false;
		}

		if (!q.out.new_mtime) {
			torture_comment(tctx, "NEW mtime not available on LSA for secret %s\n", nsec->name);
			ret = false;
		}
		if (nsec->mtime != *q.out.new_mtime) {
			torture_comment(tctx, "NEW mtime on secret %s does not match between SAMSYNC (%s) and LSA (%s)\n",
			       nsec->name, nt_time_string(mem_ctx, nsec->mtime),
			       nt_time_string(mem_ctx, *q.out.new_mtime));
			ret = false;
		}

		if (nsec->secret.length != lsa_blob_out.length) {
			torture_comment(tctx, "Returned secret %s doesn't match: %d != %d\n",
			       nsec->name, (int)nsec->secret.length, (int)lsa_blob_out.length);
			ret = false;
		} else if (memcmp(lsa_blob_out.data,
			   nsec->secret.data, nsec->secret.length) != 0) {
			torture_comment(tctx, "Returned secret %s doesn't match: \n",
			       nsec->name);
			DEBUG(1, ("SamSync Secret:\n"));
			dump_data(1, nsec->secret.data, nsec->secret.length);
			DEBUG(1, ("LSA Secret:\n"));
			dump_data(1, lsa_blob_out.data, lsa_blob_out.length);
			ret = false;
		}
	}

	return ret;
}

static bool samsync_handle_trusted_domain(struct torture_context *tctx,
					  TALLOC_CTX *mem_ctx, struct samsync_state *samsync_state,
					  int database_id, struct netr_DELTA_ENUM *delta)
{
	bool ret = true;
	struct netr_DELTA_TRUSTED_DOMAIN *trusted_domain = delta->delta_union.trusted_domain;
	struct dom_sid *dom_sid = delta->delta_id_union.sid;

	struct samsync_trusted_domain *ndom = talloc(samsync_state, struct samsync_trusted_domain);
	struct lsa_OpenTrustedDomain t;
	struct policy_handle trustdom_handle;
	struct lsa_QueryTrustedDomainInfo q;
	union lsa_TrustedDomainInfo *info[9];
	union lsa_TrustedDomainInfo *_info = NULL;
	int levels [] = {1, 3, 8};
	int i;

	ndom->name = talloc_reference(ndom, trusted_domain->domain_name.string);
	ndom->sid = talloc_reference(ndom, dom_sid);

	t.in.handle = samsync_state->lsa_handle;
	t.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	t.in.sid = dom_sid;
	t.out.trustdom_handle = &trustdom_handle;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_lsa_OpenTrustedDomain_r(samsync_state->b_lsa, mem_ctx, &t),
		"OpenTrustedDomain failed");
	torture_assert_ntstatus_ok(tctx, t.out.result,
		"OpenTrustedDomain failed");

	for (i=0; i< ARRAY_SIZE(levels); i++) {
		q.in.trustdom_handle = &trustdom_handle;
		q.in.level = levels[i];
		q.out.info = &_info;
		torture_assert_ntstatus_ok(tctx,
			dcerpc_lsa_QueryTrustedDomainInfo_r(samsync_state->b_lsa, mem_ctx, &q),
			"QueryTrustedDomainInfo failed");
		if (!NT_STATUS_IS_OK(q.out.result)) {
			if (q.in.level == 8 && NT_STATUS_EQUAL(q.out.result, NT_STATUS_INVALID_PARAMETER)) {
				info[levels[i]] = NULL;
				continue;
			}
			torture_comment(tctx, "QueryInfoTrustedDomain level %d failed - %s\n",
			       levels[i], nt_errstr(q.out.result));
			return false;
		}
		info[levels[i]]  = _info;
	}

	if (info[8]) {
		TEST_SID_EQUAL(info[8]->full_info.info_ex.sid, dom_sid);
		TEST_STRING_EQUAL(info[8]->full_info.info_ex.netbios_name, trusted_domain->domain_name);
	}
	TEST_STRING_EQUAL(info[1]->name.netbios_name, trusted_domain->domain_name);
	TEST_INT_EQUAL(info[3]->posix_offset.posix_offset, trusted_domain->posix_offset);
/*
  We would like to do this, but it is NOT_SUPPORTED on win2k3
	TEST_SEC_DESC_EQUAL(trusted_domain->sdbuf, lsa, &trustdom_handle);
*/
	ndom = talloc_reference(samsync_state, ndom);
	DLIST_ADD(samsync_state->trusted_domains, ndom);

	return ret;
}

static bool samsync_handle_account(struct torture_context *tctx,
				   TALLOC_CTX *mem_ctx, struct samsync_state *samsync_state,
					  int database_id, struct netr_DELTA_ENUM *delta)
{
	bool ret = true;
	struct netr_DELTA_ACCOUNT *account = delta->delta_union.account;
	struct dom_sid *dom_sid = delta->delta_id_union.sid;

	struct lsa_OpenAccount a;
	struct policy_handle acct_handle;
	struct lsa_EnumPrivsAccount e;
	struct lsa_PrivilegeSet *privs = NULL;
	struct lsa_LookupPrivName r;

	int i, j;

	bool *found_priv_in_lsa;

	a.in.handle = samsync_state->lsa_handle;
	a.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	a.in.sid = dom_sid;
	a.out.acct_handle = &acct_handle;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_lsa_OpenAccount_r(samsync_state->b_lsa, mem_ctx, &a),
		"OpenAccount failed");
	torture_assert_ntstatus_ok(tctx, a.out.result,
		"OpenAccount failed");

	TEST_SEC_DESC_EQUAL(account->sdbuf, lsa, &acct_handle);

	found_priv_in_lsa = talloc_zero_array(mem_ctx, bool, account->privilege_entries);

	e.in.handle = &acct_handle;
	e.out.privs = &privs;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_lsa_EnumPrivsAccount_r(samsync_state->b_lsa, mem_ctx, &e),
		"EnumPrivsAccount failed");
	torture_assert_ntstatus_ok(tctx, e.out.result,
		"EnumPrivsAccount failed");

	if ((account->privilege_entries && !privs)) {
		torture_comment(tctx, "Account %s has privileges in SamSync, but not LSA\n",
		       dom_sid_string(mem_ctx, dom_sid));
		return false;
	}

	if (!account->privilege_entries && privs && privs->count) {
		torture_comment(tctx, "Account %s has privileges in LSA, but not SamSync\n",
		       dom_sid_string(mem_ctx, dom_sid));
		return false;
	}

	TEST_INT_EQUAL(account->privilege_entries, privs->count);

	for (i=0;i< privs->count; i++) {

		struct lsa_StringLarge *name = NULL;

		r.in.handle = samsync_state->lsa_handle;
		r.in.luid = &privs->set[i].luid;
		r.out.name = &name;

		torture_assert_ntstatus_ok(tctx,
			dcerpc_lsa_LookupPrivName_r(samsync_state->b_lsa, mem_ctx, &r),
			"\nLookupPrivName failed");
		torture_assert_ntstatus_ok(tctx, r.out.result,
			"\nLookupPrivName failed");

		if (!r.out.name) {
			torture_comment(tctx, "\nLookupPrivName failed to return a name\n");
			return false;
		}
		for (j=0;j<account->privilege_entries; j++) {
			if (strcmp(name->string, account->privilege_name[j].string) == 0) {
				found_priv_in_lsa[j] = true;
				break;
			}
		}
	}
	for (j=0;j<account->privilege_entries; j++) {
		if (!found_priv_in_lsa[j]) {
			torture_comment(tctx, "Privilege %s on account %s not found in LSA\n", account->privilege_name[j].string,
			       dom_sid_string(mem_ctx, dom_sid));
			ret = false;
		}
	}
	return ret;
}

/*
  try a netlogon DatabaseSync
*/
static bool test_DatabaseSync(struct torture_context *tctx,
							  struct samsync_state *samsync_state,
							  TALLOC_CTX *mem_ctx)
{
	TALLOC_CTX *loop_ctx, *delta_ctx, *trustdom_ctx;
	struct netr_DatabaseSync r;
	const enum netr_SamDatabaseID database_ids[] = {SAM_DATABASE_DOMAIN, SAM_DATABASE_BUILTIN, SAM_DATABASE_PRIVS};
	int i, d;
	bool ret = true;
	struct samsync_trusted_domain *t;
	struct samsync_secret *s;
	struct netr_Authenticator return_authenticator, credential;
	struct netr_DELTA_ENUM_ARRAY *delta_enum_array = NULL;

	const char *domain, *username;

	ZERO_STRUCT(return_authenticator);

	r.in.logon_server = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(samsync_state->p));
	r.in.computername = TEST_MACHINE_NAME;
	r.in.preferredmaximumlength = (uint32_t)-1;
	r.in.return_authenticator = &return_authenticator;
	r.out.return_authenticator = &return_authenticator;
	r.out.delta_enum_array = &delta_enum_array;

	for (i=0;i<ARRAY_SIZE(database_ids);i++) {

		uint32_t sync_context = 0;

		r.in.database_id = database_ids[i];
		r.in.sync_context = &sync_context;
		r.out.sync_context = &sync_context;

		torture_comment(tctx, "Testing DatabaseSync of id %d\n", r.in.database_id);

		do {
			loop_ctx = talloc_named(mem_ctx, 0, "DatabaseSync loop context");
			netlogon_creds_client_authenticator(samsync_state->creds, &credential);

			r.in.credential = &credential;

			torture_assert_ntstatus_ok(tctx,
				dcerpc_netr_DatabaseSync_r(samsync_state->b, loop_ctx, &r),
				"DatabaseSync failed");
			if (!NT_STATUS_IS_OK(r.out.result) &&
			    !NT_STATUS_EQUAL(r.out.result, STATUS_MORE_ENTRIES)) {
				torture_comment(tctx, "DatabaseSync - %s\n", nt_errstr(r.out.result));
				ret = false;
				break;
			}

			if (!netlogon_creds_client_check(samsync_state->creds, &r.out.return_authenticator->cred)) {
				torture_comment(tctx, "Credential chaining failed\n");
			}

			r.in.sync_context = r.out.sync_context;

			for (d=0; d < delta_enum_array->num_deltas; d++) {
				delta_ctx = talloc_named(loop_ctx, 0, "DatabaseSync delta context");

				if (!NT_STATUS_IS_OK(samsync_fix_delta(delta_ctx, samsync_state->creds,
								       r.in.database_id,
								       &delta_enum_array->delta_enum[d]))) {
					torture_comment(tctx, "Failed to decrypt delta\n");
					ret = false;
				}

				switch (delta_enum_array->delta_enum[d].delta_type) {
				case NETR_DELTA_DOMAIN:
					if (!samsync_handle_domain(tctx, delta_ctx, samsync_state,
								   r.in.database_id, &delta_enum_array->delta_enum[d])) {
						torture_comment(tctx, "Failed to handle DELTA_DOMAIN\n");
						ret = false;
					}
					break;
				case NETR_DELTA_GROUP:
					if (!samsync_handle_group(tctx, delta_ctx, samsync_state,
								  r.in.database_id, &delta_enum_array->delta_enum[d])) {
						torture_comment(tctx, "Failed to handle DELTA_USER\n");
						ret = false;
					}
					break;
				case NETR_DELTA_USER:
					if (!samsync_handle_user(tctx, delta_ctx, samsync_state,
								 r.in.database_id, &delta_enum_array->delta_enum[d])) {
						torture_comment(tctx, "Failed to handle DELTA_USER\n");
						ret = false;
					}
					break;
				case NETR_DELTA_ALIAS:
					if (!samsync_handle_alias(tctx, delta_ctx, samsync_state,
								  r.in.database_id, &delta_enum_array->delta_enum[d])) {
						torture_comment(tctx, "Failed to handle DELTA_ALIAS\n");
						ret = false;
					}
					break;
				case NETR_DELTA_POLICY:
					if (!samsync_handle_policy(tctx, delta_ctx, samsync_state,
								   r.in.database_id, &delta_enum_array->delta_enum[d])) {
						torture_comment(tctx, "Failed to handle DELTA_POLICY\n");
						ret = false;
					}
					break;
				case NETR_DELTA_TRUSTED_DOMAIN:
					if (!samsync_handle_trusted_domain(tctx, delta_ctx, samsync_state,
									   r.in.database_id, &delta_enum_array->delta_enum[d])) {
						torture_comment(tctx, "Failed to handle DELTA_TRUSTED_DOMAIN\n");
						ret = false;
					}
					break;
				case NETR_DELTA_ACCOUNT:
					if (!samsync_handle_account(tctx, delta_ctx, samsync_state,
								    r.in.database_id, &delta_enum_array->delta_enum[d])) {
						torture_comment(tctx, "Failed to handle DELTA_ACCOUNT\n");
						ret = false;
					}
					break;
				case NETR_DELTA_SECRET:
					if (!samsync_handle_secret(tctx, delta_ctx, samsync_state,
								   r.in.database_id, &delta_enum_array->delta_enum[d])) {
						torture_comment(tctx, "Failed to handle DELTA_SECRET\n");
						ret = false;
					}
					break;
				case NETR_DELTA_GROUP_MEMBER:
				case NETR_DELTA_ALIAS_MEMBER:
					/* These are harder to cross-check, and we expect them */
					break;
				case NETR_DELTA_DELETE_GROUP:
				case NETR_DELTA_RENAME_GROUP:
				case NETR_DELTA_DELETE_USER:
				case NETR_DELTA_RENAME_USER:
				case NETR_DELTA_DELETE_ALIAS:
				case NETR_DELTA_RENAME_ALIAS:
				case NETR_DELTA_DELETE_TRUST:
				case NETR_DELTA_DELETE_ACCOUNT:
				case NETR_DELTA_DELETE_SECRET:
				case NETR_DELTA_DELETE_GROUP2:
				case NETR_DELTA_DELETE_USER2:
				case NETR_DELTA_MODIFY_COUNT:
				default:
					torture_comment(tctx, "Uxpected delta type %d\n", delta_enum_array->delta_enum[d].delta_type);
					ret = false;
					break;
				}
				talloc_free(delta_ctx);
			}
			talloc_free(loop_ctx);
		} while (NT_STATUS_EQUAL(r.out.result, STATUS_MORE_ENTRIES));

	}

	domain = samsync_state->domain_name[SAM_DATABASE_DOMAIN];
	if (!domain) {
		torture_comment(tctx, "Never got a DOMAIN object in samsync!\n");
		return false;
	}

	trustdom_ctx = talloc_named(mem_ctx, 0, "test_DatabaseSync Trusted domains context");

	username = talloc_asprintf(trustdom_ctx, "%s$", domain);
	for (t=samsync_state->trusted_domains; t; t=t->next) {
		char *secret_name = talloc_asprintf(trustdom_ctx, "G$$%s", t->name);
		for (s=samsync_state->secrets; s; s=s->next) {
			if (strcasecmp_m(s->name, secret_name) == 0) {
				NTSTATUS nt_status;
				struct samr_Password nt_hash;
				mdfour(nt_hash.hash, s->secret.data, s->secret.length);

				torture_comment(tctx, "Checking password for %s\\%s\n", t->name, username);
				nt_status = test_SamLogon(tctx,
							  samsync_state->p_netlogon_wksta, trustdom_ctx, samsync_state->creds_netlogon_wksta,
							  t->name,
							  username,
							  TEST_WKSTA_MACHINE_NAME,
							  NULL,
							  &nt_hash,
							  NULL);
				if (NT_STATUS_EQUAL(nt_status, NT_STATUS_NO_LOGON_SERVERS)) {
					torture_comment(tctx, "Verifiction of trust password to %s failed: %s (the trusted domain is not available)\n",
					       t->name, nt_errstr(nt_status));

					break;
				}
				if (!NT_STATUS_EQUAL(nt_status, NT_STATUS_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT)) {
					torture_comment(tctx, "Verifiction of trust password to %s: should have failed (nologon interdomain trust account), instead: %s\n",
					       t->name, nt_errstr(nt_status));
					ret = false;
				}

				/* break it */
				nt_hash.hash[0]++;
				nt_status = test_SamLogon(tctx,
							  samsync_state->p_netlogon_wksta, trustdom_ctx, samsync_state->creds_netlogon_wksta,
							  t->name,
							  username,
							  TEST_WKSTA_MACHINE_NAME,
							  NULL,
							  &nt_hash,
							  NULL);

				if (!NT_STATUS_EQUAL(nt_status, NT_STATUS_WRONG_PASSWORD)) {
					torture_comment(tctx, "Verifiction of trust password to %s: should have failed (wrong password), instead: %s\n",
					       t->name, nt_errstr(nt_status));
					ret = false;
				}

				break;
			}
		}
	}
	talloc_free(trustdom_ctx);
	return ret;
}


/*
  try a netlogon DatabaseDeltas
*/
static bool test_DatabaseDeltas(struct torture_context *tctx,
				struct samsync_state *samsync_state, TALLOC_CTX *mem_ctx)
{
	TALLOC_CTX *loop_ctx;
	struct netr_DatabaseDeltas r;
	struct netr_Authenticator credential;
	struct netr_Authenticator return_authenticator;
	struct netr_DELTA_ENUM_ARRAY *delta_enum_array = NULL;
	const uint32_t database_ids[] = {0, 1, 2};
	int i;
	bool ret = true;

	ZERO_STRUCT(return_authenticator);

	r.in.logon_server = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(samsync_state->p));
	r.in.computername = TEST_MACHINE_NAME;
	r.in.credential = &credential;
	r.in.preferredmaximumlength = (uint32_t)-1;
	r.in.return_authenticator = &return_authenticator;
	r.out.return_authenticator = &return_authenticator;
	r.out.delta_enum_array = &delta_enum_array;

	for (i=0;i<ARRAY_SIZE(database_ids);i++) {

		uint64_t seq_num = samsync_state->seq_num[i];

		r.in.database_id = database_ids[i];
		r.in.sequence_num = &seq_num;
		r.out.sequence_num = &seq_num;

		if (seq_num == 0) continue;

		/* this shows that the bdc doesn't need to do a single call for
		 * each seqnumber, and the pdc doesn't need to know about old values
		 * -- metze
		 */
		seq_num -= 10;

		torture_comment(tctx, "Testing DatabaseDeltas of id %d at %llu\n",
		       r.in.database_id, (long long)seq_num);

		do {
			loop_ctx = talloc_named(mem_ctx, 0, "test_DatabaseDeltas loop context");
			netlogon_creds_client_authenticator(samsync_state->creds, &credential);

			torture_assert_ntstatus_ok(tctx,
				dcerpc_netr_DatabaseDeltas_r(samsync_state->b, loop_ctx, &r),
				"DatabaseDeltas failed");
			if (!NT_STATUS_IS_OK(r.out.result) &&
			    !NT_STATUS_EQUAL(r.out.result, STATUS_MORE_ENTRIES) &&
			    !NT_STATUS_EQUAL(r.out.result, NT_STATUS_SYNCHRONIZATION_REQUIRED)) {
				torture_comment(tctx, "DatabaseDeltas - %s\n", nt_errstr(r.out.result));
				ret = false;
			}

			if (!netlogon_creds_client_check(samsync_state->creds, &return_authenticator.cred)) {
				torture_comment(tctx, "Credential chaining failed\n");
			}

			seq_num++;
			talloc_free(loop_ctx);
		} while (NT_STATUS_EQUAL(r.out.result, STATUS_MORE_ENTRIES));
	}

	return ret;
}


/*
  try a netlogon DatabaseSync2
*/
static bool test_DatabaseSync2(struct torture_context *tctx,
			       struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			       struct netlogon_creds_CredentialState *creds)
{
	TALLOC_CTX *loop_ctx;
	struct netr_DatabaseSync2 r;
	const uint32_t database_ids[] = {0, 1, 2};
	int i;
	bool ret = true;
	struct netr_Authenticator return_authenticator, credential;
	struct netr_DELTA_ENUM_ARRAY *delta_enum_array = NULL;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(return_authenticator);

	r.in.logon_server = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computername = TEST_MACHINE_NAME;
	r.in.preferredmaximumlength = (uint32_t)-1;
	r.in.return_authenticator = &return_authenticator;
	r.out.return_authenticator = &return_authenticator;
	r.out.delta_enum_array = &delta_enum_array;

	for (i=0;i<ARRAY_SIZE(database_ids);i++) {

		uint32_t sync_context = 0;

		r.in.database_id = database_ids[i];
		r.in.sync_context = &sync_context;
		r.out.sync_context = &sync_context;
		r.in.restart_state = 0;

		torture_comment(tctx, "Testing DatabaseSync2 of id %d\n", r.in.database_id);

		do {
			loop_ctx = talloc_named(mem_ctx, 0, "test_DatabaseSync2 loop context");
			netlogon_creds_client_authenticator(creds, &credential);

			r.in.credential = &credential;

			torture_assert_ntstatus_ok(tctx,
				dcerpc_netr_DatabaseSync2_r(b, loop_ctx, &r),
				"DatabaseSync2 failed");
			if (!NT_STATUS_IS_OK(r.out.result) &&
			    !NT_STATUS_EQUAL(r.out.result, STATUS_MORE_ENTRIES)) {
				torture_comment(tctx, "DatabaseSync2 - %s\n", nt_errstr(r.out.result));
				ret = false;
			}

			if (!netlogon_creds_client_check(creds, &r.out.return_authenticator->cred)) {
				torture_comment(tctx, "Credential chaining failed\n");
			}

			talloc_free(loop_ctx);
		} while (NT_STATUS_EQUAL(r.out.result, STATUS_MORE_ENTRIES));
	}

	return ret;
}



bool torture_rpc_samsync(struct torture_context *torture)
{
        NTSTATUS status;
	TALLOC_CTX *mem_ctx;
	bool ret = true;
	struct test_join *join_ctx;
	struct test_join *join_ctx2;
	struct test_join *user_ctx;
	const char *machine_password;
	const char *wksta_machine_password;
	struct dcerpc_binding *b;
	struct dcerpc_binding *b_netlogon_wksta;
	struct samr_Connect c;
	struct samr_SetDomainInfo s;
	struct policy_handle *domain_policy;

	struct lsa_ObjectAttribute attr;
	struct lsa_QosInfo qos;
	struct lsa_OpenPolicy2 r;
	struct cli_credentials *credentials;
	struct cli_credentials *credentials_wksta;

	struct samsync_state *samsync_state;

	char *test_machine_account;

	char *test_wksta_machine_account;

	mem_ctx = talloc_init("torture_rpc_netlogon");

	test_machine_account = talloc_asprintf(mem_ctx, "%s$", TEST_MACHINE_NAME);
	join_ctx = torture_create_testuser(torture, test_machine_account,
					   lpcfg_workgroup(torture->lp_ctx), ACB_SVRTRUST,
					   &machine_password);
	if (!join_ctx) {
		talloc_free(mem_ctx);
		torture_comment(torture, "Failed to join as BDC\n");
		return false;
	}

	test_wksta_machine_account = talloc_asprintf(mem_ctx, "%s$", TEST_WKSTA_MACHINE_NAME);
	join_ctx2 = torture_create_testuser(torture, test_wksta_machine_account, lpcfg_workgroup(torture->lp_ctx), ACB_WSTRUST, &wksta_machine_password);
	if (!join_ctx2) {
		talloc_free(mem_ctx);
		torture_comment(torture, "Failed to join as member\n");
		return false;
	}

	user_ctx = torture_create_testuser(torture, TEST_USER_NAME,
					   lpcfg_workgroup(torture->lp_ctx),
					   ACB_NORMAL, NULL);
	if (!user_ctx) {
		talloc_free(mem_ctx);
		torture_comment(torture, "Failed to create test account\n");
		return false;
	}

	samsync_state = talloc_zero(mem_ctx, struct samsync_state);

	samsync_state->p_samr = torture_join_samr_pipe(join_ctx);
	samsync_state->b_samr = samsync_state->p_samr->binding_handle;
	samsync_state->connect_handle = talloc_zero(samsync_state, struct policy_handle);
	samsync_state->lsa_handle = talloc_zero(samsync_state, struct policy_handle);
	c.in.system_name = NULL;
	c.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	c.out.connect_handle = samsync_state->connect_handle;

	torture_assert_ntstatus_ok_goto(torture,
		dcerpc_samr_Connect_r(samsync_state->b_samr, mem_ctx, &c),
		ret, failed,
		"samr_Connect failed");
	torture_assert_ntstatus_ok_goto(torture, c.out.result,
		ret, failed,
		"samr_Connect failed");

	domain_policy = samsync_open_domain(torture, mem_ctx, samsync_state, lpcfg_workgroup(torture->lp_ctx), NULL);
	if (!domain_policy) {
		torture_comment(torture, "samrsync_open_domain failed\n");
		ret = false;
		goto failed;
	}

	s.in.domain_handle = domain_policy;
	s.in.level = 4;
	s.in.info = talloc(mem_ctx, union samr_DomainInfo);

	s.in.info->oem.oem_information.string
		= talloc_asprintf(mem_ctx,
				  "Tortured by Samba4: %s",
				  timestring(mem_ctx, time(NULL)));
	torture_assert_ntstatus_ok_goto(torture,
		dcerpc_samr_SetDomainInfo_r(samsync_state->b_samr, mem_ctx, &s),
		ret, failed,
		"SetDomainInfo failed");

	if (!test_samr_handle_Close(samsync_state->b_samr, torture, domain_policy)) {
		ret = false;
		goto failed;
	}

	torture_assert_ntstatus_ok_goto(torture, s.out.result,
		ret, failed,
		talloc_asprintf(torture, "SetDomainInfo level %u failed", s.in.level));

	status = torture_rpc_connection(torture,
					&samsync_state->p_lsa,
					&ndr_table_lsarpc);

	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
		goto failed;
	}
	samsync_state->b_lsa = samsync_state->p_lsa->binding_handle;

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
	r.out.handle = samsync_state->lsa_handle;

	torture_assert_ntstatus_ok_goto(torture,
		dcerpc_lsa_OpenPolicy2_r(samsync_state->b_lsa, mem_ctx, &r),
		ret, failed,
		"OpenPolicy2 failed");
	torture_assert_ntstatus_ok_goto(torture, r.out.result,
		ret, failed,
		"OpenPolicy2 failed");

	status = torture_rpc_binding(torture, &b);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
		goto failed;
	}

	b->flags &= ~DCERPC_AUTH_OPTIONS;
	b->flags |= DCERPC_SCHANNEL | DCERPC_SIGN;

	credentials = cli_credentials_init(mem_ctx);

	cli_credentials_set_workstation(credentials, TEST_MACHINE_NAME, CRED_SPECIFIED);
	cli_credentials_set_domain(credentials, lpcfg_workgroup(torture->lp_ctx), CRED_SPECIFIED);
	cli_credentials_set_username(credentials, test_machine_account, CRED_SPECIFIED);
	cli_credentials_set_password(credentials, machine_password, CRED_SPECIFIED);
	cli_credentials_set_secure_channel_type(credentials,
						SEC_CHAN_BDC);

	status = dcerpc_pipe_connect_b(samsync_state,
				       &samsync_state->p, b,
					   &ndr_table_netlogon,
				       credentials, torture->ev, torture->lp_ctx);

	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "Failed to connect to server as a BDC: %s\n", nt_errstr(status));
		ret = false;
		goto failed;
	}
	samsync_state->b = samsync_state->p->binding_handle;

	status = dcerpc_schannel_creds(samsync_state->p->conn->security_state.generic_state,
				       samsync_state, &samsync_state->creds);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
	}



	status = torture_rpc_binding(torture, &b_netlogon_wksta);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
		goto failed;
	}

	b_netlogon_wksta->flags &= ~DCERPC_AUTH_OPTIONS;
	b_netlogon_wksta->flags |= DCERPC_SCHANNEL | DCERPC_SIGN;

	credentials_wksta = cli_credentials_init(mem_ctx);

	cli_credentials_set_workstation(credentials_wksta, TEST_WKSTA_MACHINE_NAME, CRED_SPECIFIED);
	cli_credentials_set_domain(credentials_wksta, lpcfg_workgroup(torture->lp_ctx), CRED_SPECIFIED);
	cli_credentials_set_username(credentials_wksta, test_wksta_machine_account, CRED_SPECIFIED);
	cli_credentials_set_password(credentials_wksta, wksta_machine_password, CRED_SPECIFIED);
	cli_credentials_set_secure_channel_type(credentials_wksta,
						SEC_CHAN_WKSTA);

	status = dcerpc_pipe_connect_b(samsync_state,
				       &samsync_state->p_netlogon_wksta,
				       b_netlogon_wksta,
					   &ndr_table_netlogon,
				       credentials_wksta, torture->ev, torture->lp_ctx);

	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "Failed to connect to server as a Workstation: %s\n", nt_errstr(status));
		ret = false;
		goto failed;
	}

	status = dcerpc_schannel_creds(samsync_state->p_netlogon_wksta->conn->security_state.generic_state,
				       samsync_state, &samsync_state->creds_netlogon_wksta);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "Failed to obtail schanel creds!\n");
		ret = false;
	}

	if (!test_DatabaseSync(torture, samsync_state, mem_ctx)) {
		torture_comment(torture, "DatabaseSync failed\n");
		ret = false;
	}

	if (!test_DatabaseDeltas(torture, samsync_state, mem_ctx)) {
		torture_comment(torture, "DatabaseDeltas failed\n");
		ret = false;
	}

	if (!test_DatabaseSync2(torture, samsync_state->p, mem_ctx, samsync_state->creds)) {
		torture_comment(torture, "DatabaseSync2 failed\n");
		ret = false;
	}
failed:

	torture_leave_domain(torture, join_ctx);
	torture_leave_domain(torture, join_ctx2);
	torture_leave_domain(torture, user_ctx);

	talloc_free(mem_ctx);

	return ret;
}
