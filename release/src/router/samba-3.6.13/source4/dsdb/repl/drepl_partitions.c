/* 
   Unix SMB/CIFS mplementation.
   DSDB replication service
   
   Copyright (C) Stefan Metzmacher 2007
    
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
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "smbd/service.h"
#include "lib/events/events.h"
#include "dsdb/repl/drepl_service.h"
#include <ldb_errors.h>
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "libcli/security/security.h"
#include "param/param.h"

WERROR dreplsrv_load_partitions(struct dreplsrv_service *s)
{
	WERROR status;
	static const char *attrs[] = { "namingContexts", NULL };
	unsigned int i;
	int ret;
	TALLOC_CTX *tmp_ctx;
	struct ldb_result *res;
	struct ldb_message_element *el;

	tmp_ctx = talloc_new(s);
	W_ERROR_HAVE_NO_MEMORY(tmp_ctx);

	ret = ldb_search(s->samdb, tmp_ctx, &res,
			 ldb_dn_new(tmp_ctx, s->samdb, ""), LDB_SCOPE_BASE, attrs, NULL);
	if (ret != LDB_SUCCESS) {
		DEBUG(1,("Searching for namingContexts in rootDSE failed: %s\n", ldb_errstring(s->samdb)));
		talloc_free(tmp_ctx);
		return WERR_DS_DRA_INTERNAL_ERROR;
       }

       el = ldb_msg_find_element(res->msgs[0], "namingContexts");
       if (!el) {
               DEBUG(1,("Finding namingContexts element in root_res failed: %s\n",
			ldb_errstring(s->samdb)));
	       talloc_free(tmp_ctx);
	       return WERR_DS_DRA_INTERNAL_ERROR;
       }

       for (i=0; i<el->num_values; i++) {
	       struct ldb_dn *pdn;
	       struct dreplsrv_partition *p;

	       pdn = ldb_dn_from_ldb_val(tmp_ctx, s->samdb, &el->values[i]);
	       if (pdn == NULL) {
		       talloc_free(tmp_ctx);
		       return WERR_DS_DRA_INTERNAL_ERROR;
	       }
	       if (!ldb_dn_validate(pdn)) {
		       return WERR_DS_DRA_INTERNAL_ERROR;
	       }

	       p = talloc_zero(s, struct dreplsrv_partition);
	       W_ERROR_HAVE_NO_MEMORY(p);

	       p->dn = talloc_steal(p, pdn);

	       DLIST_ADD(s->partitions, p);

	       DEBUG(2, ("dreplsrv_partition[%s] loaded\n", ldb_dn_get_linearized(p->dn)));
	}

	talloc_free(tmp_ctx);

	status = dreplsrv_refresh_partitions(s);
	W_ERROR_NOT_OK_RETURN(status);

	return WERR_OK;
}

/*
  work out the principal to use for DRS replication connections
 */
NTSTATUS dreplsrv_get_target_principal(struct dreplsrv_service *s,
				       TALLOC_CTX *mem_ctx,
				       const struct repsFromTo1 *rft,
				       const char **target_principal)
{
	TALLOC_CTX *tmp_ctx;
	struct ldb_result *res;
	const char *attrs[] = { "dNSHostName", NULL };
	int ret;
	const char *hostname;
	struct ldb_dn *dn;

	*target_principal = NULL;

	tmp_ctx = talloc_new(mem_ctx);

	/* we need to find their hostname */
	ret = dsdb_find_dn_by_guid(s->samdb, tmp_ctx, &rft->source_dsa_obj_guid, &dn);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		/* its OK for their NTDSDSA DN not to be in our database */
		return NT_STATUS_OK;
	}

	/* strip off the NTDS Settings */
	if (!ldb_dn_remove_child_components(dn, 1)) {
		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}

	ret = dsdb_search_dn(s->samdb, tmp_ctx, &res, dn, attrs, 0);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		/* its OK for their account DN not to be in our database */
		return NT_STATUS_OK;
	}

	hostname = ldb_msg_find_attr_as_string(res->msgs[0], "dNSHostName", NULL);
	if (hostname == NULL) {
		talloc_free(tmp_ctx);
		/* its OK to not have a dnshostname */
		return NT_STATUS_OK;
	}

	/* All DCs have the GC/hostname/realm name, but if some of the
	 * preconditions are not satisfied, then we will fall back to
	 * the
	 * E3514235-4B06-11D1-AB04-00C04FC2DCD2/${NTDSGUID}/${DNSDOMAIN}
	 * name.  This means that if a AD server has a dnsHostName set
	 * on it's record, it must also have GC/hostname/realm
	 * servicePrincipalName */

	*target_principal = talloc_asprintf(mem_ctx, "GC/%s/%s",
					    hostname,
					    lpcfg_dnsdomain(s->task->lp_ctx));
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}


WERROR dreplsrv_out_connection_attach(struct dreplsrv_service *s,
				      const struct repsFromTo1 *rft,
				      struct dreplsrv_out_connection **_conn)
{
	struct dreplsrv_out_connection *cur, *conn = NULL;
	const char *hostname;

	if (!rft->other_info) {
		return WERR_FOOBAR;
	}

	if (!rft->other_info->dns_name) {
		return WERR_FOOBAR;
	}

	hostname = rft->other_info->dns_name;

	for (cur = s->connections; cur; cur = cur->next) {		
		if (strcmp(cur->binding->host, hostname) == 0) {
			conn = cur;
			break;
		}
	}

	if (!conn) {
		NTSTATUS nt_status;
		char *binding_str;

		conn = talloc_zero(s, struct dreplsrv_out_connection);
		W_ERROR_HAVE_NO_MEMORY(conn);

		conn->service	= s;

		binding_str = talloc_asprintf(conn, "ncacn_ip_tcp:%s[krb5,seal]",
					      hostname);
		W_ERROR_HAVE_NO_MEMORY(binding_str);
		nt_status = dcerpc_parse_binding(conn, binding_str, &conn->binding);
		talloc_free(binding_str);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return ntstatus_to_werror(nt_status);
		}

		/* use the GC principal for DRS replication */
		nt_status = dreplsrv_get_target_principal(s, conn->binding,
							  rft, &conn->binding->target_principal);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return ntstatus_to_werror(nt_status);
		}

		DLIST_ADD_END(s->connections, conn, struct dreplsrv_out_connection *);

		DEBUG(4,("dreplsrv_out_connection_attach(%s): create\n", conn->binding->host));
	} else {
		DEBUG(4,("dreplsrv_out_connection_attach(%s): attach\n", conn->binding->host));
	}

	*_conn = conn;
	return WERR_OK;
}

/*
  find an existing source dsa in a list
 */
static struct dreplsrv_partition_source_dsa *dreplsrv_find_source_dsa(struct dreplsrv_partition_source_dsa *list,
								      struct GUID *guid)
{
	struct dreplsrv_partition_source_dsa *s;
	for (s=list; s; s=s->next) {
		if (GUID_compare(&s->repsFrom1->source_dsa_obj_guid, guid) == 0) {
			return s;
		}
	}
	return NULL;	
}



static WERROR dreplsrv_partition_add_source_dsa(struct dreplsrv_service *s,
						struct dreplsrv_partition *p,
						struct dreplsrv_partition_source_dsa **listp,
						struct dreplsrv_partition_source_dsa *check_list,
						const struct ldb_val *val)
{
	WERROR status;
	enum ndr_err_code ndr_err;
	struct dreplsrv_partition_source_dsa *source, *s2;

	source = talloc_zero(p, struct dreplsrv_partition_source_dsa);
	W_ERROR_HAVE_NO_MEMORY(source);

	ndr_err = ndr_pull_struct_blob(val, source, 
				       &source->_repsFromBlob,
				       (ndr_pull_flags_fn_t)ndr_pull_repsFromToBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS nt_status = ndr_map_error2ntstatus(ndr_err);
		talloc_free(source);
		return ntstatus_to_werror(nt_status);
	}
	/* NDR_PRINT_DEBUG(repsFromToBlob, &source->_repsFromBlob); */
	if (source->_repsFromBlob.version != 1) {
		talloc_free(source);
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	source->partition	= p;
	source->repsFrom1	= &source->_repsFromBlob.ctr.ctr1;

	status = dreplsrv_out_connection_attach(s, source->repsFrom1, &source->conn);
	W_ERROR_NOT_OK_RETURN(status);

	if (check_list && 
	    dreplsrv_find_source_dsa(check_list, &source->repsFrom1->source_dsa_obj_guid)) {
		/* its in the check list, don't add it again */
		talloc_free(source);
		return WERR_OK;
	}

	/* re-use an existing source if found */
	for (s2=*listp; s2; s2=s2->next) {
		if (GUID_compare(&s2->repsFrom1->source_dsa_obj_guid, 
				 &source->repsFrom1->source_dsa_obj_guid) == 0) {
			talloc_free(s2->repsFrom1->other_info);
			*s2->repsFrom1 = *source->repsFrom1;
			talloc_steal(s2, s2->repsFrom1->other_info);
			talloc_free(source);
			return WERR_OK;
		}
	}

	DLIST_ADD_END(*listp, source, struct dreplsrv_partition_source_dsa *);
	return WERR_OK;
}

WERROR dreplsrv_partition_find_for_nc(struct dreplsrv_service *s,
				      const struct GUID *nc_guid,
				      const struct dom_sid *nc_sid,
				      const char *nc_dn_str,
				      struct dreplsrv_partition **_p)
{
	struct dreplsrv_partition *p;
	bool valid_sid, valid_guid;
	struct dom_sid null_sid;
	ZERO_STRUCT(null_sid);

	SMB_ASSERT(_p);

	valid_sid  = nc_sid && !dom_sid_equal(&null_sid, nc_sid);
	valid_guid = nc_guid && !GUID_all_zero(nc_guid);

	if (!valid_sid && !valid_guid && !nc_dn_str) {
		return WERR_DS_DRA_INVALID_PARAMETER;
	}

	for (p = s->partitions; p; p = p->next) {
		if ((valid_guid && GUID_equal(&p->nc.guid, nc_guid))
		    || strequal(p->nc.dn, nc_dn_str)
		    || (valid_sid && dom_sid_equal(&p->nc.sid, nc_sid)))
		{
			*_p = p;
			return WERR_OK;
		}
	}

	return WERR_DS_DRA_BAD_NC;
}

WERROR dreplsrv_partition_source_dsa_by_guid(struct dreplsrv_partition *p,
					     const struct GUID *dsa_guid,
					     struct dreplsrv_partition_source_dsa **_dsa)
{
	struct dreplsrv_partition_source_dsa *dsa;

	SMB_ASSERT(dsa_guid != NULL);
	SMB_ASSERT(!GUID_all_zero(dsa_guid));
	SMB_ASSERT(_dsa);

	for (dsa = p->sources; dsa; dsa = dsa->next) {
		if (GUID_equal(dsa_guid, &dsa->repsFrom1->source_dsa_obj_guid)) {
			*_dsa = dsa;
			return WERR_OK;
		}
	}

	return WERR_DS_DRA_NO_REPLICA;
}

WERROR dreplsrv_partition_source_dsa_by_dns(const struct dreplsrv_partition *p,
					    const char *dsa_dns,
					    struct dreplsrv_partition_source_dsa **_dsa)
{
	struct dreplsrv_partition_source_dsa *dsa;

	SMB_ASSERT(dsa_dns != NULL);
	SMB_ASSERT(_dsa);

	for (dsa = p->sources; dsa; dsa = dsa->next) {
		if (strequal(dsa_dns, dsa->repsFrom1->other_info->dns_name)) {
			*_dsa = dsa;
			return WERR_OK;
		}
	}

	return WERR_DS_DRA_NO_REPLICA;
}


static WERROR dreplsrv_refresh_partition(struct dreplsrv_service *s,
					 struct dreplsrv_partition *p)
{
	WERROR status;
	struct dom_sid *nc_sid;
	struct ldb_message_element *orf_el = NULL;
	struct ldb_result *r;
	unsigned int i;
	int ret;
	TALLOC_CTX *mem_ctx = talloc_new(p);
	static const char *attrs[] = {
		"objectSid",
		"objectGUID",
		"repsFrom",
		"repsTo",
		NULL
	};

	DEBUG(4, ("dreplsrv_refresh_partition(%s)\n",
		ldb_dn_get_linearized(p->dn)));

	ret = ldb_search(s->samdb, mem_ctx, &r, p->dn, LDB_SCOPE_BASE, attrs,
			 "(objectClass=*)");
	if (ret != LDB_SUCCESS) {
		talloc_free(mem_ctx);
		return WERR_FOOBAR;
	}
	
	talloc_free(discard_const(p->nc.dn));
	ZERO_STRUCT(p->nc);
	p->nc.dn	= ldb_dn_alloc_linearized(p, p->dn);
	W_ERROR_HAVE_NO_MEMORY(p->nc.dn);
	p->nc.guid	= samdb_result_guid(r->msgs[0], "objectGUID");
	nc_sid		= samdb_result_dom_sid(p, r->msgs[0], "objectSid");
	if (nc_sid) {
		p->nc.sid	= *nc_sid;
		talloc_free(nc_sid);
	}

	talloc_free(p->uptodatevector.cursors);
	talloc_free(p->uptodatevector_ex.cursors);
	ZERO_STRUCT(p->uptodatevector);
	ZERO_STRUCT(p->uptodatevector_ex);

	ret = dsdb_load_udv_v2(s->samdb, p->dn, p, &p->uptodatevector.cursors, &p->uptodatevector.count);
	if (ret != LDB_SUCCESS) {
		DEBUG(4,(__location__ ": no UDV available for %s\n", ldb_dn_get_linearized(p->dn)));
	}

	orf_el = ldb_msg_find_element(r->msgs[0], "repsFrom");
	if (orf_el) {
		for (i=0; i < orf_el->num_values; i++) {
			status = dreplsrv_partition_add_source_dsa(s, p, &p->sources, 
								   NULL, &orf_el->values[i]);
			W_ERROR_NOT_OK_RETURN(status);	
		}
	}

	orf_el = ldb_msg_find_element(r->msgs[0], "repsTo");
	if (orf_el) {
		for (i=0; i < orf_el->num_values; i++) {
			status = dreplsrv_partition_add_source_dsa(s, p, &p->notifies, 
								   p->sources, &orf_el->values[i]);
			W_ERROR_NOT_OK_RETURN(status);	
		}
	}

	talloc_free(mem_ctx);

	return WERR_OK;
}

WERROR dreplsrv_refresh_partitions(struct dreplsrv_service *s)
{
	WERROR status;
	struct dreplsrv_partition *p;

	for (p = s->partitions; p; p = p->next) {
		status = dreplsrv_refresh_partition(s, p);
		W_ERROR_NOT_OK_RETURN(status);
	}

	return WERR_OK;
}
