/*
   Unix SMB/CIFS implementation.
   test suite for forest trust

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2005
   Copyright (C) Sumit Bose <sbose@redhat.com> 2010

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
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "librpc/gen_ndr/ndr_netlogon_c.h"
#include "libcli/security/security.h"
#include "libcli/auth/credentials.h"
#include "libcli/auth/libcli_auth.h"
#include "torture/rpc/torture_rpc.h"
#include "param/param.h"
#include "../lib/crypto/crypto.h"

#define TEST_DOM "torturedom"
#define TEST_DOM_DNS "torturedom.samba.example.com"
#define TEST_DOM_SID "S-1-5-21-97398-379795-10000"
#define TEST_MACHINE_NAME "lsatestmach"
#define TPASS "1234567890"


static bool test_get_policy_handle(struct torture_context *tctx,
				   struct dcerpc_pipe *p,
				   uint32_t access_mask,
				   struct policy_handle **handle  )
{
	struct policy_handle *h;
	struct lsa_OpenPolicy2 pr;
	struct lsa_ObjectAttribute attr;
	NTSTATUS status;

	h = talloc(tctx, struct policy_handle);
	if (!h) {
		return false;
	}

	attr.len = 0;
	attr.root_dir = NULL;
	attr.object_name = NULL;
	attr.attributes = 0;
	attr.sec_desc = NULL;
	attr.sec_qos = NULL;

	pr.in.system_name = "\\";
	pr.in.attr = &attr;
	pr.in.access_mask = access_mask;
	pr.out.handle = h;

	status = dcerpc_lsa_OpenPolicy2_r(p->binding_handle, tctx, &pr);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_OK,
				      "OpenPolicy2 failed");
	if (!NT_STATUS_IS_OK(pr.out.result)) {
		torture_comment(tctx, "OpenPolicy2 failed - %s\n",
				nt_errstr(pr.out.result));
		talloc_free(h);
		return false;
	}

	*handle = h;
	return true;
}

static bool test_create_trust_and_set_info(struct dcerpc_pipe *p,
					   struct torture_context *tctx,
					   const char *trust_name,
					   const char *trust_name_dns,
					   struct dom_sid *domsid,
					   struct lsa_TrustDomainInfoAuthInfoInternal *authinfo)
{
	struct policy_handle *handle;
	NTSTATUS status;
	struct lsa_lsaRSetForestTrustInformation fti;
	struct lsa_ForestTrustCollisionInfo *collision_info = NULL;
	struct lsa_Close cr;
	struct policy_handle closed_handle;
	bool ret = true;
	struct lsa_CreateTrustedDomainEx2 r;
	struct lsa_TrustDomainInfoInfoEx trustinfo;
	struct policy_handle trustdom_handle;
	struct lsa_QueryTrustedDomainInfo q;
	union lsa_TrustedDomainInfo *info = NULL;

	if (!test_get_policy_handle(tctx, p,
				   (LSA_POLICY_VIEW_LOCAL_INFORMATION |
				    LSA_POLICY_TRUST_ADMIN |
				    LSA_POLICY_CREATE_SECRET), &handle)) {
		return false;
	}

	torture_comment(tctx, "\nTesting CreateTrustedDomainEx2\n");

	trustinfo.sid = domsid;
	trustinfo.netbios_name.string = trust_name;
	trustinfo.domain_name.string = trust_name_dns;

	trustinfo.trust_direction = LSA_TRUST_DIRECTION_INBOUND |
				    LSA_TRUST_DIRECTION_OUTBOUND;

	trustinfo.trust_type = LSA_TRUST_TYPE_UPLEVEL;

	trustinfo.trust_attributes = LSA_TRUST_ATTRIBUTE_FOREST_TRANSITIVE;

	r.in.policy_handle = handle;
	r.in.info = &trustinfo;
	r.in.auth_info = authinfo;
	/* LSA_TRUSTED_QUERY_DOMAIN_NAME is needed for for following
	 * QueryTrustedDomainInfo call, although it seems that Windows does not
	 * expect this */
	r.in.access_mask = LSA_TRUSTED_SET_POSIX | LSA_TRUSTED_SET_AUTH | LSA_TRUSTED_QUERY_DOMAIN_NAME;
	r.out.trustdom_handle = &trustdom_handle;

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_lsa_CreateTrustedDomainEx2_r(p->binding_handle, tctx, &r),
				   "CreateTrustedDomainEx2 failed");
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_comment(tctx, "CreateTrustedDomainEx failed2 - %s\n", nt_errstr(r.out.result));
		ret = false;
	} else {

		q.in.trustdom_handle = &trustdom_handle;
		q.in.level = LSA_TRUSTED_DOMAIN_INFO_INFO_EX;
		q.out.info = &info;

		torture_assert_ntstatus_ok(tctx,
					   dcerpc_lsa_QueryTrustedDomainInfo_r(p->binding_handle, tctx, &q),
					   "QueryTrustedDomainInfo failed");
		if (!NT_STATUS_IS_OK(q.out.result)) {
			torture_comment(tctx,
					"QueryTrustedDomainInfo level 1 failed - %s\n",
					nt_errstr(q.out.result));
			ret = false;
		} else if (!q.out.info) {
			torture_comment(tctx,
					"QueryTrustedDomainInfo level 1 failed to return an info pointer\n");
			ret = false;
		} else {
			if (strcmp(info->info_ex.netbios_name.string, trustinfo.netbios_name.string) != 0) {
				torture_comment(tctx,
						"QueryTrustedDomainInfo returned inconsistent short name: %s != %s\n",
						info->info_ex.netbios_name.string,
						trustinfo.netbios_name.string);
				ret = false;
			}
			if (info->info_ex.trust_type != trustinfo.trust_type) {
				torture_comment(tctx,
						"QueryTrustedDomainInfo of %s returned incorrect trust type %d != %d\n",
						trust_name,
						info->info_ex.trust_type,
						trustinfo.trust_type);
				ret = false;
			}
			if (info->info_ex.trust_attributes != trustinfo.trust_attributes) {
				torture_comment(tctx,
						"QueryTrustedDomainInfo of %s returned incorrect trust attributes %d != %d\n",
						trust_name,
						info->info_ex.trust_attributes,
						trustinfo.trust_attributes);
				ret = false;
			}
			if (info->info_ex.trust_direction != trustinfo.trust_direction) {
				torture_comment(tctx,
						"QueryTrustedDomainInfo of %s returned incorrect trust direction %d != %d\n",
						trust_name,
						info->info_ex.trust_direction,
						trustinfo.trust_direction);
				ret = false;
			}
		}
	}

	if (ret != false) {
		fti.in.handle = handle;
		fti.in.trusted_domain_name = talloc_zero(tctx, struct lsa_StringLarge);
		fti.in.trusted_domain_name->string = trust_name_dns;
		fti.in.highest_record_type = 2;
		fti.in.forest_trust_info = talloc_zero(tctx, struct lsa_ForestTrustInformation);
		fti.in.forest_trust_info->count = 2;
		fti.in.forest_trust_info->entries = talloc_array(tctx, struct lsa_ForestTrustRecord *, 2);
		fti.in.forest_trust_info->entries[0] = talloc_zero(tctx, struct lsa_ForestTrustRecord);
		fti.in.forest_trust_info->entries[0]->flags = 0;
		fti.in.forest_trust_info->entries[0]->type = LSA_FOREST_TRUST_TOP_LEVEL_NAME;
		fti.in.forest_trust_info->entries[0]->time = 0;
		fti.in.forest_trust_info->entries[0]->forest_trust_data.top_level_name.string = trust_name_dns;
		fti.in.forest_trust_info->entries[1] = talloc_zero(tctx, struct lsa_ForestTrustRecord);
		fti.in.forest_trust_info->entries[1]->flags = 0;
		fti.in.forest_trust_info->entries[1]->type = LSA_FOREST_TRUST_DOMAIN_INFO;
		fti.in.forest_trust_info->entries[1]->time = 0;
		fti.in.forest_trust_info->entries[1]->forest_trust_data.domain_info.domain_sid = domsid;
		fti.in.forest_trust_info->entries[1]->forest_trust_data.domain_info.dns_domain_name.string = trust_name_dns;
		fti.in.forest_trust_info->entries[1]->forest_trust_data.domain_info.netbios_domain_name.string = trust_name;
		fti.in.check_only = 0;
		fti.out.collision_info = &collision_info;

		torture_comment(tctx, "\nTesting SetForestTrustInformation\n");

		torture_assert_ntstatus_ok(tctx,
					   dcerpc_lsa_lsaRSetForestTrustInformation_r(p->binding_handle, tctx, &fti),
					   "lsaRSetForestTrustInformation failed");
		if (!NT_STATUS_IS_OK(fti.out.result)) {
			torture_comment(tctx,
					"lsaRSetForestTrustInformation failed - %s\n",
					nt_errstr(fti.out.result));
			ret = false;
		}
	}

	cr.in.handle = handle;
	cr.out.handle = &closed_handle;
	status =  dcerpc_lsa_Close_r(p->binding_handle, tctx, &cr);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_OK,
				      "Close failed");
	if (!NT_STATUS_IS_OK(cr.out.result)) {
		torture_comment(tctx, "Close failed - %s\n",
				nt_errstr(cr.out.result));
		ret = false;
	}

	return ret;
}

static bool check_name(struct dcerpc_pipe *p, struct torture_context *tctx,
		       const char *name)
{
	struct policy_handle *handle;
	NTSTATUS status;
	struct lsa_QueryTrustedDomainInfoByName qr;
	union lsa_TrustedDomainInfo *info;
	struct lsa_Close cr;
	struct policy_handle closed_handle;

	torture_comment(tctx, "\nGetting LSA_TRUSTED_DOMAIN_INFO_FULL_INFO\n");

	if(!test_get_policy_handle(tctx, p, LSA_POLICY_VIEW_LOCAL_INFORMATION,
				   &handle)) {
		return false;
	}

	qr.in.handle = handle;
	qr.in.trusted_domain = talloc_zero(tctx, struct lsa_String);
	qr.in.trusted_domain->string = name;
	qr.in.level = LSA_TRUSTED_DOMAIN_INFO_FULL_INFO;
	qr.out.info = &info;
	status = dcerpc_lsa_QueryTrustedDomainInfoByName_r(p->binding_handle,
							   tctx, &qr);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_OK,
				      "QueryInfoPolicy2 failed");
	if (!NT_STATUS_EQUAL(qr.out.result, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
		torture_comment(tctx, "QueryInfoPolicy2 did not return "
				      "NT_STATUS_OBJECT_NAME_NOT_FOUND\n");
		return false;
	}

	cr.in.handle = handle;
	cr.out.handle = &closed_handle;
	status =  dcerpc_lsa_Close_r(p->binding_handle, tctx, &cr);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_OK,
				      "Close failed");
	if (!NT_STATUS_IS_OK(cr.out.result)) {
		torture_comment(tctx, "Close failed - %s\n",
				nt_errstr(cr.out.result));
		return false;
	}

	return true;
}
static bool get_lsa_policy_info_dns(struct dcerpc_pipe *p,
				    struct torture_context *tctx,
				    union lsa_PolicyInformation **info)
{
	struct policy_handle *handle;
	NTSTATUS status;
	struct lsa_QueryInfoPolicy2 qr;
	struct lsa_Close cr;
	struct policy_handle closed_handle;

	torture_comment(tctx, "\nGetting LSA_POLICY_INFO_DNS\n");

	if (!test_get_policy_handle(tctx, p, LSA_POLICY_VIEW_LOCAL_INFORMATION,
				    &handle)) {
		return false;
	}

	qr.in.handle = handle;
	qr.in.level = LSA_POLICY_INFO_DNS;
	qr.out.info = info;
	status = dcerpc_lsa_QueryInfoPolicy2_r(p->binding_handle, tctx, &qr);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_OK,
				      "QueryInfoPolicy2 failed");
	if (!NT_STATUS_IS_OK(qr.out.result)) {
		torture_comment(tctx, "QueryInfoPolicy2 failed - %s\n",
				nt_errstr(qr.out.result));
		return false;
	}

	cr.in.handle = handle;
	cr.out.handle = &closed_handle;
	status =  dcerpc_lsa_Close_r(p->binding_handle, tctx, &cr);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_OK,
				      "Close failed");
	if (!NT_STATUS_IS_OK(cr.out.result)) {
		torture_comment(tctx, "Close failed - %s\n",
				nt_errstr(cr.out.result));
		return false;
	}

	return true;
}

static bool delete_trusted_domain_by_sid(struct dcerpc_pipe *p,
					 struct torture_context *tctx,
					 struct dom_sid *domsid)
{
	struct policy_handle *handle;
	NTSTATUS status;
	struct lsa_Close cr;
	struct policy_handle closed_handle;
	struct lsa_DeleteTrustedDomain dr;
	bool ret = true;

	torture_comment(tctx, "\nDeleting trusted domain.\n");

	/* Against a windows server it was sufficient to have
	 * LSA_POLICY_VIEW_LOCAL_INFORMATION although the documentations says
	 * otherwise. */
	if (!test_get_policy_handle(tctx, p, LSA_POLICY_TRUST_ADMIN,
				    &handle)) {
		return false;
	}

	dr.in.handle = handle;
	dr.in.dom_sid = domsid;

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_lsa_DeleteTrustedDomain_r(p->binding_handle, tctx, &dr),
				   "DeleteTrustedDomain failed");
	if (!NT_STATUS_IS_OK(dr.out.result)) {
		torture_comment(tctx, "DeleteTrustedDomain failed - %s\n",
				nt_errstr(dr.out.result));
		return false;
	}

	cr.in.handle = handle;
	cr.out.handle = &closed_handle;
	status =  dcerpc_lsa_Close_r(p->binding_handle, tctx, &cr);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_OK,
				      "Close failed");
	if (!NT_STATUS_IS_OK(cr.out.result)) {
		torture_comment(tctx, "Close failed - %s\n",
				nt_errstr(cr.out.result));
		ret = false;
	}

	return ret;
}

static const uint8_t my_blob[] = {
0xa3,0x0b,0x32,0x45,0x8b,0x84,0x3b,0x01,0x68,0xe8,0x2b,0xbb,0x00,0x13,0x69,0x1f,
0x10,0x35,0x72,0xa9,0x4f,0x77,0xb7,0xeb,0x59,0x08,0x07,0xc3,0xe8,0x17,0x00,0xc5,
0xf2,0xa9,0x6d,0xb7,0x69,0x45,0x63,0x20,0xcb,0x44,0x44,0x22,0x02,0xe3,0x28,0x84,
0x9b,0xd5,0x43,0x6f,0x8d,0x36,0x9b,0x9b,0x3b,0x31,0x86,0x84,0x8b,0xf2,0x36,0xd4,
0xe8,0xc4,0xee,0x90,0x0c,0xcb,0x3e,0x11,0x2f,0x86,0xfe,0x87,0x6d,0xce,0xae,0x0c,
0x83,0xfb,0x21,0x22,0x6d,0x7f,0x5e,0x08,0x71,0x1a,0x35,0xf4,0x5a,0x76,0x9b,0xf7,
0x54,0x62,0xa5,0x4c,0xcd,0xf6,0xa5,0xb0,0x0b,0xc7,0x79,0xe1,0x6f,0x85,0x16,0x6f,
0x82,0xdd,0x15,0x11,0x4c,0x9d,0x26,0x01,0x74,0x7e,0xbb,0xec,0x88,0x1d,0x71,0x9e,
0x5f,0xb2,0x9c,0xab,0x66,0x20,0x08,0x3d,0xae,0x07,0x2d,0xbb,0xa6,0xfb,0xec,0xcc,
0x51,0x58,0x48,0x47,0x38,0x3b,0x47,0x66,0xe8,0x17,0xfa,0x54,0x5c,0x95,0x73,0x29,
0xdf,0x7e,0x4a,0xb4,0x45,0x30,0xf7,0xbf,0xc0,0x56,0x6d,0x80,0xf6,0x11,0x56,0x93,
0xeb,0x97,0xd5,0x10,0xd6,0xd6,0xf7,0x23,0xc3,0xc0,0x93,0xa7,0x5c,0xa9,0xc0,0x81,
0x55,0x3d,0xec,0x03,0x31,0x7e,0x9d,0xf9,0xd0,0x9e,0xb5,0xc7,0xef,0xa8,0x54,0xf6,
0x9c,0xdc,0x0d,0xd4,0xd7,0xee,0x8d,0x5f,0xbd,0x89,0x48,0x3b,0x63,0xff,0xe8,0xca,
0x10,0x64,0x61,0xdf,0xfd,0x50,0xff,0x51,0xa0,0x2c,0xd7,0x8a,0xf1,0x13,0x02,0x02,
0x71,0xe9,0xff,0x0d,0x03,0x48,0xf8,0x08,0x8d,0xd5,0xe6,0x31,0x9f,0xf0,0x26,0x07,
0x91,0x6d,0xd3,0x01,0x91,0x92,0xc7,0x28,0x18,0x58,0xd8,0xf6,0x1b,0x97,0x8d,0xd0,
0xd2,0xa1,0x7c,0xae,0xc1,0xca,0xfe,0x20,0x91,0x1c,0x4d,0x15,0x89,0x29,0x37,0xd5,
0xf5,0xca,0x40,0x2b,0x03,0x8f,0x7b,0xc2,0x10,0xb4,0xd3,0xe8,0x14,0xb0,0x9b,0x5d,
0x85,0x30,0xe5,0x13,0x24,0xf7,0x78,0xec,0xbe,0x0b,0x9a,0x3f,0xb5,0x76,0xd9,0x0d,
0x49,0x64,0xa4,0xa7,0x33,0x88,0xdd,0xe9,0xe2,0x5f,0x04,0x51,0xdd,0x89,0xe2,0x68,
0x5b,0x5f,0x64,0x35,0xe3,0x23,0x4a,0x0e,0x09,0x15,0xcc,0x97,0x47,0xf4,0xc2,0x4f,
0x06,0xc3,0x96,0xa9,0x2f,0xb3,0xde,0x29,0x10,0xc7,0xf5,0x16,0xc5,0x3c,0x84,0xd2,
0x9b,0x6b,0xaa,0x54,0x59,0x8d,0x94,0xde,0xd1,0x75,0xb6,0x08,0x0d,0x7d,0xf1,0x18,
0xc8,0xf5,0xdf,0xaa,0xcd,0xec,0xab,0xb6,0xd1,0xcb,0xdb,0xe7,0x75,0x5d,0xbe,0x76,
0xea,0x1d,0x01,0xc8,0x0b,0x2d,0x32,0xe9,0xa8,0x65,0xbb,0x4a,0xcb,0x72,0xbc,0xda,
0x04,0x7f,0x82,0xfb,0x04,0xeb,0xd8,0xe1,0xb9,0xb1,0x1e,0xdc,0xb3,0x60,0xf3,0x55,
0x1e,0xcf,0x90,0x6a,0x15,0x74,0x4d,0xff,0xb4,0xc7,0xc9,0xc2,0x4f,0x67,0x9e,0xeb,
0x00,0x61,0x02,0xe3,0x9e,0x59,0x88,0x20,0xf1,0x0c,0xbe,0xe0,0x26,0x69,0x63,0x67,
0x72,0x3c,0x06,0x00,0x9e,0x4f,0xc7,0xa6,0x4d,0x6c,0xbe,0x68,0x8e,0xf4,0x32,0x36,
0x2e,0x5f,0xa6,0xcf,0xa7,0x19,0x40,0x2b,0xbd,0xa2,0x22,0x73,0xc4,0xb6,0xe3,0x86,
0x64,0xeb,0xb1,0xc7,0x45,0x7d,0xd6,0xd9,0x36,0xf1,0x04,0xd4,0x61,0xdc,0x41,0xb7,
0x01,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,     0x30,0x00,0x00,0x00,     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x02,0x00,0x00,0x00,0x14,0x00,0x00,0x00,0x31,0x00,0x32,0x00,0x33,0x00,0x34,0x00,
0x35,0x00,0x36,0x00,0x37,0x00,0x38,0x00,0x39,0x00,0x30,0x00,0x01,0x00,0x00,0x00,
0x0c,0x00,0x00,0x00,     0x30,0x00,0x00,0x00,     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,
0x14,0x00,0x00,0x00,0x31,0x00,0x32,0x00,0x33,0x00,0x34,0x00,0x35,0x00,0x36,0x00,
0x37,0x00,0x38,0x00,0x39,0x00,0x30,0x00,0x30,0x00,0x00,0x00,0x30,0x00,0x00,0x00
};

static bool get_trust_domain_passwords_auth_blob(TALLOC_CTX *mem_ctx,
						 const char *password,
						 DATA_BLOB *auth_blob)
{
	struct trustDomainPasswords auth_struct;
	struct AuthenticationInformation *auth_info_array;
	enum ndr_err_code ndr_err;
	size_t converted_size;

	generate_random_buffer(auth_struct.confounder,
			       sizeof(auth_struct.confounder));

	auth_info_array = talloc_array(mem_ctx,
				       struct AuthenticationInformation, 1);
	if (auth_info_array == NULL) {
		return false;
	}

	auth_info_array[0].AuthType = TRUST_AUTH_TYPE_CLEAR;
	if (!convert_string_talloc(mem_ctx, CH_UNIX, CH_UTF16, password,
				  strlen(password),
				  &auth_info_array[0].AuthInfo.clear.password,
				  &converted_size,
				  false)) {
		return false;
	}

	auth_info_array[0].AuthInfo.clear.size = converted_size;

	auth_struct.outgoing.count = 1;
	auth_struct.outgoing.current.count = 1;
	auth_struct.outgoing.current.array = auth_info_array;
	auth_struct.outgoing.previous.count = 0;
	auth_struct.outgoing.previous.array = NULL;

	auth_struct.incoming.count = 1;
	auth_struct.incoming.current.count = 1;
	auth_struct.incoming.current.array = auth_info_array;
	auth_struct.incoming.previous.count = 0;
	auth_struct.incoming.previous.array = NULL;

	ndr_err = ndr_push_struct_blob(auth_blob, mem_ctx, &auth_struct,
				       (ndr_push_flags_fn_t)ndr_push_trustDomainPasswords);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return false;
	}

	return true;
}

static bool test_validate_trust(struct torture_context *tctx,
				struct dcerpc_binding *binding,
				const char *trusting_dom_name,
				const char *trusting_dom_dns_name,
				const char *trusted_dom_name,
				const char *trusted_dom_dns_name)
{
	struct netr_ServerGetTrustInfo r;

	struct netr_Authenticator a;
	struct netr_Authenticator return_authenticator;
	struct samr_Password new_owf_password;
	struct samr_Password old_owf_password;
	struct netr_TrustInfo *trust_info;

	struct netlogon_creds_CredentialState *creds;

	NTSTATUS status;
	struct cli_credentials *credentials;
	struct dcerpc_pipe *pipe;

	struct netr_GetForestTrustInformation fr;
	struct lsa_ForestTrustInformation *forest_trust_info;
	int i;


	credentials = cli_credentials_init(tctx);
	if (credentials == NULL) {
		return false;
	}

	char *dummy = talloc_asprintf(tctx, "%s$", trusted_dom_name);
	cli_credentials_set_username(credentials, dummy,
				     CRED_SPECIFIED);
	cli_credentials_set_domain(credentials, trusting_dom_name,
				   CRED_SPECIFIED);
	cli_credentials_set_realm(credentials, trusting_dom_dns_name,
				  CRED_SPECIFIED);
	cli_credentials_set_password(credentials, TPASS, CRED_SPECIFIED);
	cli_credentials_set_workstation(credentials,
					trusted_dom_name, CRED_SPECIFIED);
	cli_credentials_set_secure_channel_type(credentials, SEC_CHAN_DOMAIN);

	status = dcerpc_pipe_connect_b(tctx, &pipe, binding,
				       &ndr_table_netlogon, credentials,
				       tctx->ev, tctx->lp_ctx);

	if (NT_STATUS_IS_ERR(status)) {
		torture_comment(tctx, "Failed to connect to remote server: %s  with %s - %s\n",
				dcerpc_binding_string(tctx, binding),
				cli_credentials_get_unparsed_name(credentials, tctx),
				nt_errstr(status));
		return false;
	}

	if (!test_SetupCredentials3(pipe, tctx, NETLOGON_NEG_AUTH2_ADS_FLAGS,
				    credentials, &creds)) {
		torture_comment(tctx, "test_SetupCredentials3 failed.\n");
		return false;
	}

	netlogon_creds_client_authenticator(creds, &a);

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s",
					   dcerpc_server_name(pipe));
	r.in.account_name = talloc_asprintf(tctx, "%s$", trusted_dom_name);
	r.in.secure_channel_type = cli_credentials_get_secure_channel_type(credentials);
	r.in.computer_name = trusted_dom_name;
	r.in.credential = &a;

	r.out.return_authenticator = &return_authenticator;
	r.out.new_owf_password = &new_owf_password;
	r.out.old_owf_password = &old_owf_password;
	r.out.trust_info = &trust_info;

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_netr_ServerGetTrustInfo_r(pipe->binding_handle, tctx, &r),
				   "ServerGetTrustInfo failed");
	torture_assert_ntstatus_ok(tctx, r.out.result,
				   "ServerGetTrustInfo failed");

	if (trust_info->count != 1) {
		torture_comment(tctx, "Unexpected number of results, "
				      "expected %d, got %d.\n",
				      1, trust_info->count);
		return false;
	}
	if (trust_info->data[0] != LSA_TRUST_ATTRIBUTE_FOREST_TRANSITIVE) {
		torture_comment(tctx, "Unexpected result, "
				      "expected %d, got %d.\n",
				      LSA_TRUST_ATTRIBUTE_FOREST_TRANSITIVE,
				      trust_info->data[0]);
		return false;
	}

	netlogon_creds_client_authenticator(creds, &a);

	fr.in.server_name = talloc_asprintf(tctx, "\\\\%s",
					    dcerpc_server_name(pipe));
	fr.in.computer_name = trusted_dom_name;
	fr.in.credential = &a;
	fr.in.flags = 0;
	fr.out.return_authenticator = &return_authenticator;
	fr.out.forest_trust_info = &forest_trust_info;

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_netr_GetForestTrustInformation_r(pipe->binding_handle, tctx, &fr),
				   "netr_GetForestTrustInformation failed");
	if (NT_STATUS_IS_ERR(fr.out.result)) {
		torture_comment(tctx,
				"netr_GetForestTrustInformation failed - %s.\n",
				nt_errstr(fr.out.result));
		return false;
	}

	if (forest_trust_info->count != 2) {
		torture_comment(tctx, "Unexpected number of results, "
				      "expected %d, got %d.\n", 2,
				      forest_trust_info->count);
		return false;
	}
	for(i = 0; i < forest_trust_info->count; i++) {
		switch(forest_trust_info->entries[i]->type) {
			case LSA_FOREST_TRUST_TOP_LEVEL_NAME:
				if (strcmp(forest_trust_info->entries[i]->forest_trust_data.top_level_name.string, trusting_dom_dns_name) != 0) {
					torture_comment(tctx, "Unexpected result, "
							"expected %s, got %s.\n",
							trusting_dom_dns_name,
							forest_trust_info->entries[i]->forest_trust_data.top_level_name.string);
					return false;
				}
				break;
			case LSA_FOREST_TRUST_DOMAIN_INFO:
				if (strcmp(forest_trust_info->entries[i]->forest_trust_data.domain_info.netbios_domain_name.string, trusting_dom_name) != 0) {
					torture_comment(tctx, "Unexpected result, "
							"expected %s, got %s.\n",
							trusting_dom_dns_name,
							forest_trust_info->entries[i]->forest_trust_data.domain_info.netbios_domain_name.string);
					return false;
				}
				if (strcmp(forest_trust_info->entries[i]->forest_trust_data.domain_info.dns_domain_name.string, trusting_dom_dns_name) != 0) {
					torture_comment(tctx, "Unexpected result, "
							"expected %s, got %s.\n",
							trusting_dom_dns_name,
							forest_trust_info->entries[i]->forest_trust_data.domain_info.dns_domain_name.string);
					return false;
				}
				break;
			default:
				torture_comment(tctx, "Unexpected result type, "
					        "expected %d|%d, got %d.\n",
					        LSA_FOREST_TRUST_TOP_LEVEL_NAME,
					        LSA_FOREST_TRUST_DOMAIN_INFO,
					        forest_trust_info->entries[i]->type);
				return false;
		}
	}

	return true;
}

static bool test_setup_trust(struct torture_context *tctx,
			     struct dcerpc_pipe *p,
			     const char *netbios_name,
			     const char *dns_name,
			     struct dom_sid *sid,
			     DATA_BLOB *auth_blob)

{
	DATA_BLOB session_key;
	struct lsa_TrustDomainInfoAuthInfoInternal authinfo;
	NTSTATUS status;

	if (!check_name(p, tctx, netbios_name)) {
		return false;
	}
	if (!check_name(p, tctx, dns_name)) {
		return false;
	}

	status = dcerpc_fetch_session_key(p, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "dcerpc_fetch_session_key failed - %s\n",
				nt_errstr(status));
		return false;
	}

	authinfo.auth_blob.data = talloc_memdup(tctx, auth_blob->data,
						auth_blob->length);
	if (authinfo.auth_blob.data == NULL) {
		return false;
	}
	authinfo.auth_blob.size = auth_blob->length;

	arcfour_crypt_blob(authinfo.auth_blob.data, authinfo.auth_blob.size,
			   &session_key);

	if (!test_create_trust_and_set_info(p, tctx, netbios_name,
					    dns_name, sid, &authinfo)) {
		return false;
	}

	return true;
}

static bool testcase_ForestTrusts(struct torture_context *tctx,
				  struct dcerpc_pipe *p)
{
	bool ret = true;
	const char *dom2_binding_string;
	const char * dom2_cred_string;
	NTSTATUS status;
	struct dom_sid *domsid;
	DATA_BLOB auth_blob;
	struct dcerpc_binding *dom2_binding;
	struct dcerpc_pipe *dom2_p;
	struct cli_credentials *dom2_credentials;
	union lsa_PolicyInformation *dom1_info_dns = NULL;
	union lsa_PolicyInformation *dom2_info_dns = NULL;

	torture_comment(tctx, "Testing Forest Trusts\n");

	if (!get_trust_domain_passwords_auth_blob(tctx, TPASS, &auth_blob)) {
		torture_comment(tctx,
				"get_trust_domain_passwords_auth_blob failed\n");
		return false;
	}

#if 0
	/* Use the following if get_trust_domain_passwords_auth_blob() cannot
	 * generate a usable blob due to errors in the IDL */
	auth_blob.data = talloc_memdup(tctx, my_blob, sizeof(my_blob));
	auth_blob.length = sizeof(my_blob);
#endif

	domsid = dom_sid_parse_talloc(tctx, TEST_DOM_SID);
	if (domsid == NULL) {
		return false;
	}

	if (!test_setup_trust(tctx, p, TEST_DOM, TEST_DOM_DNS, domsid,
			      &auth_blob)) {
		ret = false;
	}

	if (!get_lsa_policy_info_dns(p, tctx, &dom1_info_dns)) {
		return false;
	}

	if (!test_validate_trust(tctx, p->binding,
				 dom1_info_dns->dns.name.string,
				 dom1_info_dns->dns.dns_domain.string,
				 TEST_DOM, TEST_DOM_DNS)) {
		ret = false;
	}

	if (!delete_trusted_domain_by_sid(p, tctx, domsid)) {
		ret = false;
	}

	dom2_binding_string = torture_setting_string(tctx,
						     "Forest_Trust_Dom2_Binding",
						     NULL);
	if (dom2_binding_string == NULL) {
		return ret;
	}

	status = dcerpc_parse_binding(tctx, dom2_binding_string, &dom2_binding);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(0,("Failed to parse dcerpc binding '%s'\n",
			 dom2_binding_string));
		return false;
	}

	dom2_cred_string = torture_setting_string(tctx,
						  "Forest_Trust_Dom2_Creds",
						  NULL);
	if (dom2_cred_string == NULL) {
		return false;
	}

	dom2_credentials = cli_credentials_init(tctx);
	if (dom2_credentials == NULL) {
		return false;
	}

	cli_credentials_parse_string(dom2_credentials, dom2_cred_string,
				     CRED_SPECIFIED);
	cli_credentials_set_workstation(dom2_credentials,
					TEST_MACHINE_NAME, CRED_SPECIFIED);

	status = dcerpc_pipe_connect_b(tctx, &dom2_p, dom2_binding,
				       &ndr_table_lsarpc, dom2_credentials,
				       tctx->ev, tctx->lp_ctx);

	if (NT_STATUS_IS_ERR(status)) {
		torture_comment(tctx, "Failed to connect to remote server: %s %s\n",
				dcerpc_binding_string(tctx, dom2_binding),
				nt_errstr(status));
		return false;
	}

	if (!get_lsa_policy_info_dns(dom2_p, tctx, &dom2_info_dns)) {
		return false;
	}

	if (strcasecmp(dom1_info_dns->dns.name.string,
		       dom2_info_dns->dns.name.string) == 0 ||
	    strcasecmp(dom1_info_dns->dns.dns_domain.string,
		       dom2_info_dns->dns.dns_domain.string) == 0)
	{
		torture_comment(tctx, "Trusting and trusted domain have the "
				      "same name.\n");
		return false;
	}

	if (!test_setup_trust(tctx, p, dom2_info_dns->dns.name.string,
			       dom2_info_dns->dns.dns_domain.string,
			       dom2_info_dns->dns.sid, &auth_blob)) {
		ret = false;
	}
	if (!test_setup_trust(tctx, dom2_p, dom1_info_dns->dns.name.string,
			      dom1_info_dns->dns.dns_domain.string,
			      dom1_info_dns->dns.sid, &auth_blob)) {
		ret = false;
	}

	if (!test_validate_trust(tctx, p->binding,
				 dom1_info_dns->dns.name.string,
				 dom1_info_dns->dns.dns_domain.string,
				 dom2_info_dns->dns.name.string,
				 dom2_info_dns->dns.dns_domain.string)) {
		ret = false;
	}

	if (!test_validate_trust(tctx, dom2_p->binding,
				 dom2_info_dns->dns.name.string,
				 dom2_info_dns->dns.dns_domain.string,
				 dom1_info_dns->dns.name.string,
				 dom1_info_dns->dns.dns_domain.string)) {
		ret = false;
	}

	if (!delete_trusted_domain_by_sid(p, tctx, dom2_info_dns->dns.sid)) {
		ret = false;
	}

	if (!delete_trusted_domain_by_sid(dom2_p, tctx, dom1_info_dns->dns.sid)) {
		ret = false;
	}

	return ret;
}

/* By default this test creates a trust object in the destination server to a
 * dummy domain. If a second server from a different domain is specified on the
 * command line a trust is created between those two domains.
 *
 * Example:
 * smbtorture ncacn_np:srv1.dom1.test[print] RPC-LSA-FOREST-TRUST \
 *  -U 'dom1\testadm1%12345678' \
 *  --option=torture:Forest_Trust_Dom2_Binding=ncacn_np:srv2.dom2.test[print]  \
 *  --option=torture:Forest_Trust_Dom2_Creds='dom2\testadm2%12345678'
 */

struct torture_suite *torture_rpc_lsa_forest_trust(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite;
	struct torture_rpc_tcase *tcase;

	suite = torture_suite_create(mem_ctx, "lsa.forest.trust");

	tcase = torture_suite_add_rpc_iface_tcase(suite, "lsa-forest-trust",
						  &ndr_table_lsarpc);
	torture_rpc_tcase_add_test(tcase, "ForestTrust", testcase_ForestTrusts);

	return suite;
}
