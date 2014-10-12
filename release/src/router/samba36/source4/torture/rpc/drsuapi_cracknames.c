/* 
   Unix SMB/CIFS implementation.

   DRSUapi tests

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Stefan (metze) Metzmacher 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005

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
#include "librpc/gen_ndr/ndr_drsuapi_c.h"
#include "torture/rpc/torture_rpc.h"
#include <ldb.h>
#include "libcli/security/security.h"

struct DsCrackNamesPrivate {
	struct DsPrivate base;

	/* following names are used in Crack Names Matrix test */
	const char *fqdn_name;
	const char *user_principal_name;
	const char *service_principal_name;
};

static bool test_DsCrackNamesMatrix(struct torture_context *tctx,
				    struct DsPrivate *priv, const char *dn,
				    const char *user_principal_name, const char *service_principal_name)
{
	NTSTATUS status;
	const char *err_msg;
	struct drsuapi_DsCrackNames r;
	union drsuapi_DsNameRequest req;
	uint32_t level_out;
	union drsuapi_DsNameCtr ctr;
	struct dcerpc_pipe *p = priv->drs_pipe;
	TALLOC_CTX *mem_ctx = priv;

	enum drsuapi_DsNameFormat formats[] = {
		DRSUAPI_DS_NAME_FORMAT_UNKNOWN,
		DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
		DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT,
		DRSUAPI_DS_NAME_FORMAT_DISPLAY,
		DRSUAPI_DS_NAME_FORMAT_GUID,
		DRSUAPI_DS_NAME_FORMAT_CANONICAL,
		DRSUAPI_DS_NAME_FORMAT_USER_PRINCIPAL,
		DRSUAPI_DS_NAME_FORMAT_CANONICAL_EX,
		DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL,
		DRSUAPI_DS_NAME_FORMAT_SID_OR_SID_HISTORY,
		DRSUAPI_DS_NAME_FORMAT_DNS_DOMAIN
	};
	struct drsuapi_DsNameString names[ARRAY_SIZE(formats)];
	int i, j;

	const char *n_matrix[ARRAY_SIZE(formats)][ARRAY_SIZE(formats)];
	const char *n_from[ARRAY_SIZE(formats)];

	ZERO_STRUCT(r);
	r.in.bind_handle		= &priv->bind_handle;
	r.in.level			= 1;
	r.in.req			= &req;
	r.in.req->req1.codepage		= 1252; /* german */
	r.in.req->req1.language		= 0x00000407; /* german */
	r.in.req->req1.count		= 1;
	r.in.req->req1.names		= names;
	r.in.req->req1.format_flags	= DRSUAPI_DS_NAME_FLAG_NO_FLAGS;

	r.out.level_out			= &level_out;
	r.out.ctr			= &ctr;

	n_matrix[0][0] = dn;

	for (i = 0; i < ARRAY_SIZE(formats); i++) {
		r.in.req->req1.format_offered	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779;
		r.in.req->req1.format_desired	= formats[i];
		names[0].str = dn;
		status = dcerpc_drsuapi_DsCrackNames_r(p->binding_handle, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			const char *errstr = nt_errstr(status);
			err_msg = talloc_asprintf(mem_ctx,
					"testing DsCrackNames (matrix prep) with name '%s' from format: %d desired format:%d failed - %s",
					names[0].str, r.in.req->req1.format_offered, r.in.req->req1.format_desired, errstr);
			torture_fail(tctx, err_msg);
		} else if (!W_ERROR_IS_OK(r.out.result)) {
			err_msg = talloc_asprintf(mem_ctx,
					"testing DsCrackNames (matrix prep) with name '%s' from format: %d desired format:%d failed - %s",
			       names[0].str, r.in.req->req1.format_offered, r.in.req->req1.format_desired, win_errstr(r.out.result));
			torture_fail(tctx, err_msg);
		}
			
		switch (formats[i]) {
		case DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL:	
			if (r.out.ctr->ctr1->array[0].status != DRSUAPI_DS_NAME_STATUS_NOT_UNIQUE) {
				err_msg = talloc_asprintf(mem_ctx,
						"Unexpected error (%d): This name lookup should fail",
						r.out.ctr->ctr1->array[0].status);
				torture_fail(tctx, err_msg);
			}
			torture_comment(tctx, __location__ ": (expected) error\n");
			break;
		case DRSUAPI_DS_NAME_FORMAT_USER_PRINCIPAL:
			if (r.out.ctr->ctr1->array[0].status != DRSUAPI_DS_NAME_STATUS_NO_MAPPING) {
				err_msg = talloc_asprintf(mem_ctx,
						"Unexpected error (%d): This name lookup should fail",
						r.out.ctr->ctr1->array[0].status);
				torture_fail(tctx, err_msg);
			}
			torture_comment(tctx, __location__ ": (expected) error\n");
			break;
		case DRSUAPI_DS_NAME_FORMAT_UNKNOWN:	/* should fail as we ask server to convert to Unknown format */
		case DRSUAPI_DS_NAME_FORMAT_DNS_DOMAIN:	
		case DRSUAPI_DS_NAME_FORMAT_SID_OR_SID_HISTORY:	
			if (r.out.ctr->ctr1->array[0].status != DRSUAPI_DS_NAME_STATUS_RESOLVE_ERROR) {
				err_msg = talloc_asprintf(mem_ctx,
						"Unexpected error (%d): This name lookup should fail",
						r.out.ctr->ctr1->array[0].status);
				torture_fail(tctx, err_msg);
			}
			torture_comment(tctx, __location__ ": (expected) error\n");
			break;
		default:
			if (r.out.ctr->ctr1->array[0].status != DRSUAPI_DS_NAME_STATUS_OK) {
				err_msg = talloc_asprintf(mem_ctx,
						"DsCrackNames error: %d",
						r.out.ctr->ctr1->array[0].status);
				torture_fail(tctx, err_msg);
			}
			break;
		}

		switch (formats[i]) {
		case DRSUAPI_DS_NAME_FORMAT_USER_PRINCIPAL:
			n_from[i] = user_principal_name;
			break;
		case DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL:	
			n_from[i] = service_principal_name;
			break;
		case DRSUAPI_DS_NAME_FORMAT_UNKNOWN:
		case DRSUAPI_DS_NAME_FORMAT_SID_OR_SID_HISTORY:	
		case DRSUAPI_DS_NAME_FORMAT_DNS_DOMAIN:	
			n_from[i] = NULL;
			break;
		default:
			n_from[i] = r.out.ctr->ctr1->array[0].result_name;
			printf("%s\n", n_from[i]);
			break;
		}
	}

	for (i = 0; i < ARRAY_SIZE(formats); i++) {
		for (j = 0; j < ARRAY_SIZE(formats); j++) {
			r.in.req->req1.format_offered	= formats[i];
			r.in.req->req1.format_desired	= formats[j];
			if (!n_from[i]) {
				n_matrix[i][j] = NULL;
				continue;
			}
			names[0].str = n_from[i];
			status = dcerpc_drsuapi_DsCrackNames_r(p->binding_handle, mem_ctx, &r);
			if (!NT_STATUS_IS_OK(status)) {
				const char *errstr = nt_errstr(status);
				err_msg = talloc_asprintf(mem_ctx,
						"testing DsCrackNames (matrix) with name '%s' from format: %d desired format:%d failed - %s",
						names[0].str, r.in.req->req1.format_offered, r.in.req->req1.format_desired, errstr);
				torture_fail(tctx, err_msg);
			} else if (!W_ERROR_IS_OK(r.out.result)) {
				err_msg = talloc_asprintf(mem_ctx,
						"testing DsCrackNames (matrix) with name '%s' from format: %d desired format:%d failed - %s",
						names[0].str, r.in.req->req1.format_offered, r.in.req->req1.format_desired,
						win_errstr(r.out.result));
				torture_fail(tctx, err_msg);
			}
			
			if (r.out.ctr->ctr1->array[0].status == DRSUAPI_DS_NAME_STATUS_OK) {
				n_matrix[i][j] = r.out.ctr->ctr1->array[0].result_name;
			} else {
				n_matrix[i][j] = NULL;
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(formats); i++) {
		for (j = 0; j < ARRAY_SIZE(formats); j++) {
			if (n_matrix[i][j] == n_from[j]) {
				
			/* We don't have a from name for these yet (and we can't map to them to find it out) */
			} else if (n_matrix[i][j] == NULL && n_from[i] == NULL) {
				
			/* we can't map to these two */
			} else if (n_matrix[i][j] == NULL && formats[j] == DRSUAPI_DS_NAME_FORMAT_USER_PRINCIPAL) {
			} else if (n_matrix[i][j] == NULL && formats[j] == DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL) {
			} else if (n_matrix[i][j] == NULL && n_from[j] != NULL) {
				err_msg = talloc_asprintf(mem_ctx,
						"dcerpc_drsuapi_DsCrackNames mismatch - from %d to %d: %s should be %s",
						formats[i], formats[j], n_matrix[i][j], n_from[j]);
				torture_fail(tctx, err_msg);
			} else if (n_matrix[i][j] != NULL && n_from[j] == NULL) {
				err_msg = talloc_asprintf(mem_ctx,
						"dcerpc_drsuapi_DsCrackNames mismatch - from %d to %d: %s should be %s",
						formats[i], formats[j], n_matrix[i][j], n_from[j]);
				torture_fail(tctx, err_msg);
			} else if (strcmp(n_matrix[i][j], n_from[j]) != 0) {
				err_msg = talloc_asprintf(mem_ctx,
						"dcerpc_drsuapi_DsCrackNames mismatch - from %d to %d: %s should be %s",
						formats[i], formats[j], n_matrix[i][j], n_from[j]);
				torture_fail(tctx, err_msg);
			}
		}
	}

	return true;
}

bool test_DsCrackNames(struct torture_context *tctx,
		       struct DsPrivate *priv)
{
	NTSTATUS status;
	const char *err_msg;
	struct drsuapi_DsCrackNames r;
	union drsuapi_DsNameRequest req;
	uint32_t level_out;
	union drsuapi_DsNameCtr ctr;
	struct drsuapi_DsNameString names[1];
	const char *dns_domain;
	const char *nt4_domain;
	const char *FQDN_1779_name;
	struct ldb_context *ldb;
	struct ldb_dn *FQDN_1779_dn;
	struct ldb_dn *realm_dn;
	const char *realm_dn_str;
	const char *realm_canonical;
	const char *realm_canonical_ex;
	const char *user_principal_name;
	char *user_principal_name_short;
	const char *service_principal_name;
	const char *canonical_name;
	const char *canonical_ex_name;
	const char *dom_sid;
	const char *test_dc = torture_join_netbios_name(priv->join);
	struct dcerpc_pipe *p = priv->drs_pipe;
	TALLOC_CTX *mem_ctx = priv;

	ZERO_STRUCT(r);
	r.in.bind_handle		= &priv->bind_handle;
	r.in.level			= 1;
	r.in.req			= &req;
	r.in.req->req1.codepage		= 1252; /* german */
	r.in.req->req1.language		= 0x00000407; /* german */
	r.in.req->req1.count		= 1;
	r.in.req->req1.names		= names;
	r.in.req->req1.format_flags	= DRSUAPI_DS_NAME_FLAG_NO_FLAGS;

	r.in.req->req1.format_offered	= DRSUAPI_DS_NAME_FORMAT_SID_OR_SID_HISTORY;
	r.in.req->req1.format_desired	= DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT;

	r.out.level_out			= &level_out;
	r.out.ctr			= &ctr;

	dom_sid = dom_sid_string(mem_ctx, torture_join_sid(priv->join));
	
	names[0].str = dom_sid;

	torture_comment(tctx, "Testing DsCrackNames with name '%s' desired format:%d\n",
			names[0].str, r.in.req->req1.format_desired);

	status = dcerpc_drsuapi_DsCrackNames_r(p->binding_handle, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		const char *errstr = nt_errstr(status);
		err_msg = talloc_asprintf(mem_ctx, "dcerpc_drsuapi_DsCrackNames failed - %s", errstr);
		torture_fail(tctx, err_msg);
	} else if (!W_ERROR_IS_OK(r.out.result)) {
		err_msg = talloc_asprintf(mem_ctx, "DsCrackNames failed - %s", win_errstr(r.out.result));
		torture_fail(tctx, err_msg);
	} else if (r.out.ctr->ctr1->array[0].status != DRSUAPI_DS_NAME_STATUS_OK) {
		err_msg = talloc_asprintf(mem_ctx, "DsCrackNames failed on name - %d",
					  r.out.ctr->ctr1->array[0].status);
		torture_fail(tctx, err_msg);
	}

	dns_domain = r.out.ctr->ctr1->array[0].dns_domain_name;
	nt4_domain = r.out.ctr->ctr1->array[0].result_name;

	r.in.req->req1.format_desired	= DRSUAPI_DS_NAME_FORMAT_GUID;

	torture_comment(tctx, "Testing DsCrackNames with name '%s' desired format:%d\n",
			names[0].str, r.in.req->req1.format_desired);

	status = dcerpc_drsuapi_DsCrackNames_r(p->binding_handle, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		const char *errstr = nt_errstr(status);
		err_msg = talloc_asprintf(mem_ctx, "dcerpc_drsuapi_DsCrackNames failed - %s", errstr);
		torture_fail(tctx, err_msg);
	} else if (!W_ERROR_IS_OK(r.out.result)) {
		err_msg = talloc_asprintf(mem_ctx, "DsCrackNames failed - %s", win_errstr(r.out.result));
		torture_fail(tctx, err_msg);
	} else if (r.out.ctr->ctr1->array[0].status != DRSUAPI_DS_NAME_STATUS_OK) {
		err_msg = talloc_asprintf(mem_ctx, "DsCrackNames failed on name - %d",
					  r.out.ctr->ctr1->array[0].status);
		torture_fail(tctx, err_msg);
	}

	priv->domain_dns_name = r.out.ctr->ctr1->array[0].dns_domain_name;
	priv->domain_guid_str = r.out.ctr->ctr1->array[0].result_name;
	GUID_from_string(priv->domain_guid_str, &priv->domain_guid);

	r.in.req->req1.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779;

	torture_comment(tctx, "Testing DsCrackNames with name '%s' desired format:%d\n",
			names[0].str, r.in.req->req1.format_desired);

	status = dcerpc_drsuapi_DsCrackNames_r(p->binding_handle, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		const char *errstr = nt_errstr(status);
		err_msg = talloc_asprintf(mem_ctx, "dcerpc_drsuapi_DsCrackNames failed - %s", errstr);
		torture_fail(tctx, err_msg);
	} else if (!W_ERROR_IS_OK(r.out.result)) {
		err_msg = talloc_asprintf(mem_ctx, "DsCrackNames failed - %s", win_errstr(r.out.result));
		torture_fail(tctx, err_msg);
	} else if (r.out.ctr->ctr1->array[0].status != DRSUAPI_DS_NAME_STATUS_OK) {
		err_msg = talloc_asprintf(mem_ctx, "DsCrackNames failed on name - %d",
					  r.out.ctr->ctr1->array[0].status);
		torture_fail(tctx, err_msg);
	}

	ldb = ldb_init(mem_ctx, tctx->ev);
	
	realm_dn_str = r.out.ctr->ctr1->array[0].result_name;
	realm_dn =  ldb_dn_new(mem_ctx, ldb, realm_dn_str);
	realm_canonical = ldb_dn_canonical_string(mem_ctx, realm_dn);

	if (strcmp(realm_canonical,
		   talloc_asprintf(mem_ctx, "%s/", dns_domain))!= 0) {
		err_msg = talloc_asprintf(mem_ctx, "local Round trip on canonical name failed: %s != %s!",
				          realm_canonical,
				          talloc_asprintf(mem_ctx, "%s/", dns_domain));
		torture_fail(tctx, err_msg);
	};

	realm_canonical_ex = ldb_dn_canonical_ex_string(mem_ctx, realm_dn);

	if (strcmp(realm_canonical_ex, 
		   talloc_asprintf(mem_ctx, "%s\n", dns_domain))!= 0) {
		err_msg = talloc_asprintf(mem_ctx, "local Round trip on canonical ex name failed: %s != %s!",
				          realm_canonical,
				          talloc_asprintf(mem_ctx, "%s\n", dns_domain));
		torture_fail(tctx, err_msg);
	};

	r.in.req->req1.format_offered	= DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT;
	r.in.req->req1.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779;
	names[0].str = nt4_domain;

	torture_comment(tctx, "Testing DsCrackNames with name '%s' desired format:%d\n",
			names[0].str, r.in.req->req1.format_desired);

	status = dcerpc_drsuapi_DsCrackNames_r(p->binding_handle, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		const char *errstr = nt_errstr(status);
		err_msg = talloc_asprintf(mem_ctx, "dcerpc_drsuapi_DsCrackNames failed - %s", errstr);
		torture_fail(tctx, err_msg);
	} else if (!W_ERROR_IS_OK(r.out.result)) {
		err_msg = talloc_asprintf(mem_ctx, "DsCrackNames failed - %s", win_errstr(r.out.result));
		torture_fail(tctx, err_msg);
	} else if (r.out.ctr->ctr1->array[0].status != DRSUAPI_DS_NAME_STATUS_OK) {
		err_msg = talloc_asprintf(mem_ctx, "DsCrackNames failed on name - %d",
					  r.out.ctr->ctr1->array[0].status);
		torture_fail(tctx, err_msg);
	}

	priv->domain_obj_dn = r.out.ctr->ctr1->array[0].result_name;

	r.in.req->req1.format_offered	= DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT;
	r.in.req->req1.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779;
	names[0].str = talloc_asprintf(mem_ctx, "%s%s$", nt4_domain, test_dc);

	torture_comment(tctx, "Testing DsCrackNames with name '%s' desired format:%d\n",
			names[0].str, r.in.req->req1.format_desired);

	status = dcerpc_drsuapi_DsCrackNames_r(p->binding_handle, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		const char *errstr = nt_errstr(status);
		err_msg = talloc_asprintf(mem_ctx, "dcerpc_drsuapi_DsCrackNames failed - %s", errstr);
		torture_fail(tctx, err_msg);
	} else if (!W_ERROR_IS_OK(r.out.result)) {
		err_msg = talloc_asprintf(mem_ctx, "DsCrackNames failed - %s", win_errstr(r.out.result));
		torture_fail(tctx, err_msg);
	} else if (r.out.ctr->ctr1->array[0].status != DRSUAPI_DS_NAME_STATUS_OK) {
		err_msg = talloc_asprintf(mem_ctx, "DsCrackNames failed on name - %d",
					  r.out.ctr->ctr1->array[0].status);
		torture_fail(tctx, err_msg);
	}

	FQDN_1779_name = r.out.ctr->ctr1->array[0].result_name;

	r.in.req->req1.format_offered	= DRSUAPI_DS_NAME_FORMAT_GUID;
	r.in.req->req1.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779;
	names[0].str = priv->domain_guid_str;

	torture_comment(tctx, "Testing DsCrackNames with name '%s' desired format:%d\n",
			names[0].str, r.in.req->req1.format_desired);

	status = dcerpc_drsuapi_DsCrackNames_r(p->binding_handle, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		const char *errstr = nt_errstr(status);
		err_msg = talloc_asprintf(mem_ctx, "dcerpc_drsuapi_DsCrackNames failed - %s", errstr);
		torture_fail(tctx, err_msg);
	} else if (!W_ERROR_IS_OK(r.out.result)) {
		err_msg = talloc_asprintf(mem_ctx, "DsCrackNames failed - %s", win_errstr(r.out.result));
		torture_fail(tctx, err_msg);
	} else if (r.out.ctr->ctr1->array[0].status != DRSUAPI_DS_NAME_STATUS_OK) {
		err_msg = talloc_asprintf(mem_ctx, "DsCrackNames failed on name - %d",
					  r.out.ctr->ctr1->array[0].status);
		torture_fail(tctx, err_msg);
	}

	if (strcmp(priv->domain_dns_name, r.out.ctr->ctr1->array[0].dns_domain_name) != 0) {
		err_msg = talloc_asprintf(mem_ctx,
				"DsCrackNames failed to return same DNS name - expected %s got %s",
				priv->domain_dns_name, r.out.ctr->ctr1->array[0].dns_domain_name);
		torture_fail(tctx, err_msg);
	}

	FQDN_1779_dn = ldb_dn_new(mem_ctx, ldb, FQDN_1779_name);

	canonical_name = ldb_dn_canonical_string(mem_ctx, FQDN_1779_dn);
	canonical_ex_name = ldb_dn_canonical_ex_string(mem_ctx, FQDN_1779_dn);

	user_principal_name = talloc_asprintf(mem_ctx, "%s$@%s", test_dc, dns_domain);

	/* form up a user@DOMAIN */
	user_principal_name_short = talloc_asprintf(mem_ctx, "%s$@%s", test_dc, nt4_domain);
	/* variable nt4_domain includs a trailing \ */
	user_principal_name_short[strlen(user_principal_name_short) - 1] = '\0';
	
	service_principal_name = talloc_asprintf(mem_ctx, "HOST/%s", test_dc);
	{
		
		struct {
			enum drsuapi_DsNameFormat format_offered;
			enum drsuapi_DsNameFormat format_desired;
			const char *comment;
			const char *str;
			const char *expected_str;
			const char *expected_dns;
			enum drsuapi_DsNameStatus status;
			enum drsuapi_DsNameStatus alternate_status;
			enum drsuapi_DsNameFlags flags;
			bool skip;
		} crack[] = {
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_USER_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = user_principal_name,
				.expected_str = FQDN_1779_name,
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_USER_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = user_principal_name_short,
				.expected_str = FQDN_1779_name,
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_USER_PRINCIPAL,
				.str = FQDN_1779_name,
				.status = DRSUAPI_DS_NAME_STATUS_NO_MAPPING
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = service_principal_name,
				.expected_str = FQDN_1779_name,
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = talloc_asprintf(mem_ctx, "cifs/%s.%s", test_dc, dns_domain),
				.comment = "ServicePrincipal Name",
				.expected_str = FQDN_1779_name,
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_CANONICAL,
				.str = FQDN_1779_name,
				.expected_str = canonical_name,
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_CANONICAL, 
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = canonical_name,
				.expected_str = FQDN_1779_name,
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_CANONICAL_EX,
				.str = FQDN_1779_name,
				.expected_str = canonical_ex_name,
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_CANONICAL_EX, 
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = canonical_ex_name,
				.expected_str = FQDN_1779_name,
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_CANONICAL,
				.str = FQDN_1779_name,
				.comment = "DN to cannoical syntactial only",
				.status = DRSUAPI_DS_NAME_STATUS_OK,
				.expected_str = canonical_name,
				.flags = DRSUAPI_DS_NAME_FLAG_SYNTACTICAL_ONLY
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_CANONICAL_EX,
				.str = FQDN_1779_name,
				.comment = "DN to cannoical EX syntactial only",
				.status = DRSUAPI_DS_NAME_STATUS_OK,
				.expected_str = canonical_ex_name,
				.flags = DRSUAPI_DS_NAME_FLAG_SYNTACTICAL_ONLY
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_DISPLAY,
				.str = FQDN_1779_name,
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_GUID,
				.str = FQDN_1779_name,
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_GUID,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT,
				.str = priv->domain_guid_str,
				.comment = "Domain GUID to NT4 ACCOUNT",
				.expected_str = nt4_domain,
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_GUID,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_CANONICAL,
				.str = priv->domain_guid_str,
				.comment = "Domain GUID to Canonical",
				.expected_str = talloc_asprintf(mem_ctx, "%s/", dns_domain),
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_GUID,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_CANONICAL_EX,
				.str = priv->domain_guid_str,
				.comment = "Domain GUID to Canonical EX",
				.expected_str = talloc_asprintf(mem_ctx, "%s\n", dns_domain),
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_DISPLAY,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = "CN=Microsoft Corporation,L=Redmond,S=Washington,C=US",
				.comment = "display name for Microsoft Support Account",
				.status = DRSUAPI_DS_NAME_STATUS_OK,
				.alternate_status = DRSUAPI_DS_NAME_STATUS_NOT_UNIQUE,
				.skip = torture_setting_bool(tctx, "samba4", false)
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_GUID,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = GUID_string2(mem_ctx, torture_join_user_guid(priv->join)),
				.comment = "Account GUID -> DN",
				.expected_str = FQDN_1779_name,
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_GUID,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT,
				.str = GUID_string2(mem_ctx, torture_join_user_guid(priv->join)),
				.comment = "Account GUID -> NT4 Account",
				.expected_str = talloc_asprintf(mem_ctx, "%s%s$", nt4_domain, test_dc),
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{		
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_GUID,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = GUID_string2(mem_ctx, &priv->dcinfo.site_guid),
				.comment = "Site GUID",
				.expected_str = priv->dcinfo.site_dn,
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_GUID,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = GUID_string2(mem_ctx, &priv->dcinfo.computer_guid),
				.comment = "Computer GUID",
				.expected_str = priv->dcinfo.computer_dn,
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_GUID,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT,
				.str = GUID_string2(mem_ctx, &priv->dcinfo.computer_guid),
				.comment = "Computer GUID -> NT4 Account",
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_GUID,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = GUID_string2(mem_ctx, &priv->dcinfo.server_guid),
				.comment = "Server GUID",
				.expected_str = priv->dcinfo.server_dn,
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_GUID,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = GUID_string2(mem_ctx, &priv->dcinfo.ntds_guid),
				.comment = "NTDS GUID",
				.expected_str = priv->dcinfo.ntds_dn,
				.status = DRSUAPI_DS_NAME_STATUS_OK,
				.skip = GUID_all_zero(&priv->dcinfo.ntds_guid)
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_DISPLAY,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = test_dc,
				.comment = "DISLPAY NAME search for DC short name",
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = talloc_asprintf(mem_ctx, "krbtgt/%s", dns_domain),
				.comment = "Looking for KRBTGT as a serivce principal",
				.status = DRSUAPI_DS_NAME_STATUS_DOMAIN_ONLY,
				.expected_dns = dns_domain
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = talloc_asprintf(mem_ctx, "bogus/%s", dns_domain),
				.comment = "Looking for bogus serivce principal",
				.status = DRSUAPI_DS_NAME_STATUS_DOMAIN_ONLY,
				.expected_dns = dns_domain
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = talloc_asprintf(mem_ctx, "bogus/%s.%s", test_dc, dns_domain),
				.comment = "Looking for bogus serivce on test DC",
				.status = DRSUAPI_DS_NAME_STATUS_DOMAIN_ONLY,
				.expected_dns = talloc_asprintf(mem_ctx, "%s.%s", test_dc, dns_domain)
			},
			{ 
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = talloc_asprintf(mem_ctx, "krbtgt"),
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			},
			{ 
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.comment = "Looking for the kadmin/changepw service as a serivce principal",
				.str = talloc_asprintf(mem_ctx, "kadmin/changepw"),
				.status = DRSUAPI_DS_NAME_STATUS_OK,
				.expected_str = talloc_asprintf(mem_ctx, "CN=krbtgt,CN=Users,%s", realm_dn_str),
				.alternate_status = DRSUAPI_DS_NAME_STATUS_NOT_UNIQUE
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = talloc_asprintf(mem_ctx, "cifs/%s.%s@%s", 
						       test_dc, dns_domain,
						       dns_domain),
				.status = DRSUAPI_DS_NAME_STATUS_DOMAIN_ONLY
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = talloc_asprintf(mem_ctx, "cifs/%s.%s@%s", 
						       test_dc, dns_domain,
						       "BOGUS"),
				.status = DRSUAPI_DS_NAME_STATUS_DOMAIN_ONLY,
				.expected_dns = "BOGUS"
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = talloc_asprintf(mem_ctx, "cifs/%s.%s@%s", 
						       test_dc, "REALLY",
						       "BOGUS"),
				.status = DRSUAPI_DS_NAME_STATUS_DOMAIN_ONLY,
				.expected_dns = "BOGUS"
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = talloc_asprintf(mem_ctx, "cifs/%s.%s", 
						       test_dc, dns_domain),
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = talloc_asprintf(mem_ctx, "cifs/%s", 
						       test_dc),
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_GUID,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = "NOT A GUID",
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SID_OR_SID_HISTORY,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = "NOT A SID",
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = "NOT AN NT4 NAME",
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_GUID,
				.comment = "Unparsable DN",
				.str = "NOT A DN",
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_USER_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.comment = "Unparsable user principal",
				.str = "NOT A PRINCIPAL",
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.comment = "Unparsable service principal",
				.str = "NOT A SERVICE PRINCIPAL",
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_GUID,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.comment = "BIND GUID (ie, not in the directory)",
				.str = GUID_string2(mem_ctx, &priv->bind_guid),
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_USER_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.comment = "Unqualified Machine account as user principal",
				.str = talloc_asprintf(mem_ctx, "%s$", test_dc),
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.comment = "Machine account as service principal",
				.str = talloc_asprintf(mem_ctx, "%s$", test_dc),
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SERVICE_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.comment = "Full Machine account as service principal",
				.str = user_principal_name,
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.comment = "Realm as an NT4 domain lookup",
				.str = talloc_asprintf(mem_ctx, "%s\\", dns_domain),
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			}, 
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.comment = "BUILTIN\\ -> DN",
				.str = "BUILTIN\\",
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			}, 
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.comment = "NT AUTHORITY\\ -> DN",
				.str = "NT AUTHORITY\\",
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			}, 
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.comment = "NT AUTHORITY\\ANONYMOUS LOGON -> DN",
				.str = "NT AUTHORITY\\ANONYMOUS LOGON",
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			}, 
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.comment = "NT AUTHORITY\\SYSTEM -> DN",
				.str = "NT AUTHORITY\\SYSTEM",
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			}, 
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SID_OR_SID_HISTORY,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT,
				.comment = "BUITIN SID -> NT4 account",
				.str = SID_BUILTIN,
				.status = DRSUAPI_DS_NAME_STATUS_NO_MAPPING,
				.alternate_status = DRSUAPI_DS_NAME_STATUS_NOT_UNIQUE
			}, 
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SID_OR_SID_HISTORY,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = SID_BUILTIN,
				.comment = "Builtin Domain SID -> DN",
				.status = DRSUAPI_DS_NAME_STATUS_OK,
				.expected_str = talloc_asprintf(mem_ctx, "CN=Builtin,%s", realm_dn_str),
				.alternate_status = DRSUAPI_DS_NAME_STATUS_NOT_UNIQUE
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SID_OR_SID_HISTORY,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.str = SID_BUILTIN_ADMINISTRATORS,
				.comment = "Builtin Administrors SID -> DN",
				.status = DRSUAPI_DS_NAME_STATUS_OK,
				.alternate_status = DRSUAPI_DS_NAME_STATUS_NOT_UNIQUE
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SID_OR_SID_HISTORY,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT,
				.str = SID_BUILTIN_ADMINISTRATORS,
				.comment = "Builtin Administrors SID -> NT4 Account",
				.status = DRSUAPI_DS_NAME_STATUS_OK,
				.alternate_status = DRSUAPI_DS_NAME_STATUS_NOT_UNIQUE
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SID_OR_SID_HISTORY,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT,
				.str = SID_NT_ANONYMOUS,
				.comment = "NT Anonymous SID -> NT4 Account",
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SID_OR_SID_HISTORY,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT,
				.str = SID_NT_SYSTEM,
				.comment = "NT SYSTEM SID -> NT4 Account",
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SID_OR_SID_HISTORY,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.comment = "Domain SID -> DN",
				.str = dom_sid,
				.expected_str = realm_dn_str,
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_SID_OR_SID_HISTORY,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT,
				.comment = "Domain SID -> NT4 account",
				.str = dom_sid,
				.expected_str = nt4_domain,
				.status = DRSUAPI_DS_NAME_STATUS_OK
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_USER_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.comment = "invalid user principal name",
				.str = "foo@bar",
				.status = DRSUAPI_DS_NAME_STATUS_DOMAIN_ONLY,
				.expected_dns = "bar"
			},
			{
				.format_offered	= DRSUAPI_DS_NAME_FORMAT_USER_PRINCIPAL,
				.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				.comment = "invalid user principal name in valid domain",
				.str = talloc_asprintf(mem_ctx, "invalidusername@%s", dns_domain),
				.status = DRSUAPI_DS_NAME_STATUS_NOT_FOUND
			}
		};
		int i;
		
		for (i=0; i < ARRAY_SIZE(crack); i++) {
			const char *comment;
			r.in.req->req1.format_flags   = crack[i].flags;
			r.in.req->req1.format_offered = crack[i].format_offered;
			r.in.req->req1.format_desired = crack[i].format_desired;
			names[0].str = crack[i].str;
			
			if (crack[i].comment) {
				comment = talloc_asprintf(mem_ctx, "'%s' with name '%s' desired format:%d\n",
							  crack[i].comment, names[0].str, r.in.req->req1.format_desired);
			} else {
				comment = talloc_asprintf(mem_ctx, "'%s' desired format:%d\n",
				       names[0].str, r.in.req->req1.format_desired);
			}
			if (crack[i].skip) {
				torture_comment(tctx, "skipping: %s", comment);
				continue;
			}
			status = dcerpc_drsuapi_DsCrackNames_r(p->binding_handle, mem_ctx, &r);
			if (!NT_STATUS_IS_OK(status)) {
				const char *errstr = nt_errstr(status);
				err_msg = talloc_asprintf(mem_ctx, "dcerpc_drsuapi_DsCrackNames failed - %s", errstr);
				torture_fail(tctx, err_msg);
			} else if (!W_ERROR_IS_OK(r.out.result)) {
				err_msg = talloc_asprintf(mem_ctx, "DsCrackNames failed - %s", win_errstr(r.out.result));
				torture_fail(tctx, err_msg);
			} else if (r.out.ctr->ctr1->array[0].status != crack[i].status) {
				if (crack[i].alternate_status) {
					if (r.out.ctr->ctr1->array[0].status != crack[i].alternate_status) {
						err_msg = talloc_asprintf(mem_ctx,
								"DsCrackNames unexpected status %d, wanted %d or %d on: %s",
								r.out.ctr->ctr1->array[0].status,
								crack[i].status,
								crack[i].alternate_status,
								comment);
						torture_fail(tctx, err_msg);
					}
				} else {
					err_msg = talloc_asprintf(mem_ctx,
							"DsCrackNames unexpected status %d, wanted %d on: %s\n",
							r.out.ctr->ctr1->array[0].status,
							crack[i].status,
							comment);
					torture_fail(tctx, err_msg);
				}
			} else if (crack[i].expected_str
				   && (strcmp(r.out.ctr->ctr1->array[0].result_name,
					      crack[i].expected_str) != 0)) {
				if (strcasecmp(r.out.ctr->ctr1->array[0].result_name,
					       crack[i].expected_str) != 0) {
					err_msg = talloc_asprintf(mem_ctx,
							"DsCrackNames failed - got %s, expected %s on %s",
							r.out.ctr->ctr1->array[0].result_name,
							crack[i].expected_str, comment);
					torture_fail(tctx, err_msg);
				} else {
					torture_comment(tctx,
							"(warning) DsCrackNames returned different case - got %s, expected %s on %s\n",
							r.out.ctr->ctr1->array[0].result_name,
							crack[i].expected_str, comment);
				}
			} else if (crack[i].expected_dns
				   && (strcmp(r.out.ctr->ctr1->array[0].dns_domain_name,
					      crack[i].expected_dns) != 0)) {
				err_msg = talloc_asprintf(mem_ctx,
						"DsCrackNames failed - got DNS name %s, expected %s on %s",
						r.out.ctr->ctr1->array[0].result_name,
						crack[i].expected_str, comment);
				torture_fail(tctx, err_msg);
			}
		}
	}

	return test_DsCrackNamesMatrix(tctx, priv, FQDN_1779_name,
					user_principal_name, service_principal_name);
}

/**
 * Test case setup for CrackNames
 */
static bool torture_drsuapi_cracknames_setup(struct torture_context *tctx, void **data)
{
	struct DsCrackNamesPrivate *priv;

	*data = priv = talloc_zero(tctx, struct DsCrackNamesPrivate);

	return torture_drsuapi_tcase_setup_common(tctx, &priv->base);
}

/**
 * Test case tear-down for CrackNames
 */
static bool torture_drsuapi_cracknames_teardown(struct torture_context *tctx, void *data)
{
	struct DsCrackNamesPrivate *priv = talloc_get_type(data, struct DsCrackNamesPrivate);

	return torture_drsuapi_tcase_teardown_common(tctx, &priv->base);
}

/**
 * CRACKNAMES test suite implementation
 */
void torture_rpc_drsuapi_cracknames_tcase(struct torture_suite *suite)
{
	typedef bool (*run_func) (struct torture_context *test, void *tcase_data);

	struct torture_test *test;
	struct torture_tcase *tcase = torture_suite_add_tcase(suite, "cracknames");

	torture_tcase_set_fixture(tcase,
				  torture_drsuapi_cracknames_setup,
				  torture_drsuapi_cracknames_teardown);

	test = torture_tcase_add_simple_test(tcase, "cracknames-test", (run_func)test_DsCrackNames);
}
