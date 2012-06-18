/*
   Samba CIFS implementation
   ADS convenience functions for GPO

   Copyright (C) 2001 Andrew Tridgell (from samba3 ads.c)
   Copyright (C) 2001 Remus Koos (from samba3 ads.c)
   Copyright (C) 2001 Andrew Bartlett (from samba3 ads.c)
   Copyright (C) 2008 Jelmer Vernooij, jelmer@samba.org
   Copyright (C) 2008 Wilco Baan Hofman, wilco@baanhofman.nl

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
#include "libnet/libnet.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "libgpo/ads_convenience.h"
#include "param/param.h"
#include "libcli/libcli.h"
#include "ldb_wrap.h"

static ADS_STATUS ads_connect(ADS_STRUCT *ads);

WERROR ads_startup (struct libnet_context *netctx, ADS_STRUCT **ads)
{
	*ads = talloc(netctx, ADS_STRUCT);
	(*ads)->netctx = netctx;

	ads_connect(*ads);

	return WERR_OK;
}

static ADS_STATUS ads_connect(ADS_STRUCT *ads)
{
	struct libnet_LookupDCs *io;
	char *url;

	io = talloc_zero(ads, struct libnet_LookupDCs);

	/* We are looking for the PDC of the active domain. */
	io->in.name_type = NBT_NAME_PDC;
	io->in.domain_name = lp_workgroup(ads->netctx->lp_ctx);
	libnet_LookupDCs(ads->netctx, ads, io);

	url = talloc_asprintf(ads, "ldap://%s", io->out.dcs[0].name);
	ads->ldbctx = ldb_wrap_connect(ads, ads->netctx->event_ctx, ads->netctx->lp_ctx,
	                 url, NULL, ads->netctx->cred, 0, NULL);
	if (ads->ldbctx == NULL) {
		return ADS_ERROR_NT(NT_STATUS_UNSUCCESSFUL);
	}

	return ADS_ERROR_NT(NT_STATUS_OK);
}

ADS_STATUS ads_search_dn(ADS_STRUCT *ads, LDAPMessage **res,
                         const char *dn, const char **attrs)
{
	ADS_STATUS status;

	status.err.rc = ldb_search(ads->ldbctx, ads, res,
	                              ldb_dn_new(ads, ads->ldbctx, dn),
	                              LDB_SCOPE_BASE,
                                      attrs,
	                              "(objectclass=*)");

	status.error_type = ENUM_ADS_ERROR_LDAP;
	return status;
}

const char * ads_get_dn(ADS_STRUCT *ads, LDAPMessage *res)
{
	return ldb_dn_get_linearized(res->msgs[0]->dn);
}

bool ads_pull_sd(ADS_STRUCT *ads, TALLOC_CTX *ctx, LDAPMessage *res, const char *field, struct security_descriptor **sd)
{
	const struct ldb_val *val;
	enum ndr_err_code ndr_err;

	val = ldb_msg_find_ldb_val(res->msgs[0], field);

        *sd = talloc(ctx, struct security_descriptor);
        if (*sd == NULL) {
                return -1;
        }
        /* We can't use ndr_pull_struct_blob_all because this contains relative pointers */
        ndr_err = ndr_pull_struct_blob(val, *sd, NULL, *sd,
                                           (ndr_pull_flags_fn_t)ndr_pull_security_descriptor);
        if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
                talloc_free(*sd);
                return -1;
        }
	return 0;
}

ADS_STATUS ads_search_retry_dn_sd_flags(ADS_STRUCT *ads, LDAPMessage **res, uint32_t sd_flags,
                                        const char *dn, const char **attrs)
{
	return ads_do_search_all_sd_flags(ads, dn, LDB_SCOPE_BASE, "(objectclass=*)", attrs, sd_flags, res);
}

ADS_STATUS ads_do_search_all_sd_flags (ADS_STRUCT *ads, const char *dn, int scope,
                                              const char *filter, const char **attrs,
                                              uint32_t sd_flags, LDAPMessage **res)
{
	int rv;
	struct ldb_request *req;
	struct ldb_control **controls;
	struct ldb_parse_tree *tree;
	struct ldb_dn *ldb_dn;

	controls = talloc_zero_array(ads, struct ldb_control *, 2);
	controls[0] = talloc(ads, struct ldb_control);
	controls[0]->oid = LDB_CONTROL_SD_FLAGS_OID;
	controls[0]->data = &sd_flags;
	controls[0]->critical = 1;

	tree = ldb_parse_tree(ads, filter);

	ldb_dn = ldb_dn_new(ads, ads->ldbctx, dn);

	rv = ldb_build_search_req_ex(&req, ads->ldbctx, (TALLOC_CTX *)res, ldb_dn, scope, tree, attrs, controls,
	                             res, ldb_search_default_callback, NULL);
	if (rv != LDB_SUCCESS) {
		talloc_free(*res);
		talloc_free(req);
		talloc_free(tree);
		return ADS_ERROR(rv);
	}
	rv = ldb_request(ads->ldbctx, req);
	if (rv == LDB_SUCCESS) {
		rv = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	talloc_free(req);
	talloc_free(tree);
	return ADS_ERROR(rv);

}

const char * ads_pull_string(ADS_STRUCT *ads, TALLOC_CTX *ctx, LDAPMessage *res, const char *field)
{
	return ldb_msg_find_attr_as_string(res->msgs[0], field, NULL);
}

bool ads_pull_uint32(ADS_STRUCT *ads, LDAPMessage *res, const char *field, uint32_t *ret)
{
	if (ldb_msg_find_element(res->msgs[0], field) == NULL) {
		return false;
	}
	*ret = ldb_msg_find_attr_as_uint(res->msgs[0], field, 0);
	return true;
}


int ads_count_replies(ADS_STRUCT *ads, LDAPMessage *res)
{
	return res->count;
}

ADS_STATUS ads_msgfree(ADS_STRUCT *ads, LDAPMessage *res)
{
	talloc_free(res);
	return ADS_ERROR_NT(NT_STATUS_OK);
}

/*
  do a rough conversion between ads error codes and NT status codes
  we'll need to fill this in more
*/
NTSTATUS ads_ntstatus(ADS_STATUS status)
{
	switch (status.error_type) {
	case ENUM_ADS_ERROR_NT:
		return status.err.nt_status;
	case ENUM_ADS_ERROR_SYSTEM:
		return map_nt_error_from_unix(status.err.rc);
	case ENUM_ADS_ERROR_LDAP:
		if (status.err.rc == LDB_SUCCESS) {
			return NT_STATUS_OK;
		}
		return NT_STATUS_UNSUCCESSFUL;
	default:
		break;
	}

	if (ADS_ERR_OK(status)) {
		return NT_STATUS_OK;
	}
	return NT_STATUS_UNSUCCESSFUL;
}

/*
  return a string for an error from an ads routine
*/
const char *ads_errstr(ADS_STATUS status)
{
	switch (status.error_type) {
	case ENUM_ADS_ERROR_SYSTEM:
		return strerror(status.err.rc);
	case ENUM_ADS_ERROR_LDAP:
		return ldb_strerror(status.err.rc);
	case ENUM_ADS_ERROR_NT:
		return get_friendly_nt_error_msg(ads_ntstatus(status));
	default:
		return "Unknown ADS error type!? (not compiled in?)";
	}
}

ADS_STATUS ads_build_ldap_error(int ldb_error)
{
	ADS_STATUS ret;
	ret.err.rc = ldb_error;
	ret.error_type = ENUM_ADS_ERROR_LDAP;
	return ret;
}

ADS_STATUS ads_build_nt_error(NTSTATUS nt_status)
{
	ADS_STATUS ret;
	ret.err.nt_status = nt_status;
	ret.error_type = ENUM_ADS_ERROR_NT;
	return ret;
}


bool nt_token_check_sid( const struct dom_sid *sid, const NT_USER_TOKEN *token)
{
	int i;

	if (!sid || !token) {
		return false;
	}

	if (dom_sid_equal(sid, token->user_sid)) {
		return true;
	}
	if (dom_sid_equal(sid, token->group_sid)) {
		return true;
	}
	for (i = 0; i < token->num_sids; i++) {
		if (dom_sid_equal(sid, token->sids[i])) {
			return true;
		}
	}

	return false;
}
const char *ads_get_ldap_server_name(ADS_STRUCT *ads) {
	return ads->ldap_server_name;
}


/*
  FIXME
  Stub write functions, these do not do anything, though they should. -- Wilco
*/

ADS_MODLIST ads_init_mods(TALLOC_CTX *ctx)
{
	return NULL;
}

ADS_STATUS ads_mod_str(TALLOC_CTX *ctx, ADS_MODLIST *mods, const char *name, const char *val)
{
	return ADS_ERROR_NT(NT_STATUS_NOT_IMPLEMENTED);
}

ADS_STATUS ads_gen_mod(ADS_STRUCT *ads, const char *mod_dn, ADS_MODLIST mods)
{
	return ADS_ERROR_NT(NT_STATUS_NOT_IMPLEMENTED);
}
