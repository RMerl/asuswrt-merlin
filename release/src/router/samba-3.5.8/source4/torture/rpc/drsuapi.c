/* 
   Unix SMB/CIFS implementation.

   DRSUapi tests

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Stefan (metze) Metzmacher 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005-2006

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
#include "librpc/gen_ndr/ndr_drsuapi_c.h"
#include "torture/rpc/rpc.h"
#include "param/param.h"

#define TEST_MACHINE_NAME "torturetest"

bool test_DsBind(struct dcerpc_pipe *p,
		 struct torture_context *tctx,
		 struct DsPrivate *priv)
{
	NTSTATUS status;
	struct drsuapi_DsBind r;

	GUID_from_string(DRSUAPI_DS_BIND_GUID, &priv->bind_guid);

	r.in.bind_guid = &priv->bind_guid;
	r.in.bind_info = NULL;
	r.out.bind_handle = &priv->bind_handle;

	torture_comment(tctx, "testing DsBind\n");

	status = dcerpc_drsuapi_DsBind(p, tctx, &r);
	torture_drsuapi_assert_call(tctx, p, status, &r, "dcerpc_drsuapi_DsBind");

	return true;
}

static bool test_DsGetDomainControllerInfo(struct torture_context *tctx,
					   struct DsPrivate *priv)
{
	NTSTATUS status;
	struct dcerpc_pipe *p = priv->pipe;
	struct drsuapi_DsGetDomainControllerInfo r;
	union drsuapi_DsGetDCInfoCtr ctr;
	int32_t level_out = 0;
	bool found = false;
	int i, j, k;
	
	struct {
		const char *name;
		WERROR expected;
	} names[] = { 
		{	
			.name = torture_join_dom_netbios_name(priv->join),
			.expected = WERR_OK
		},
		{
			.name = torture_join_dom_dns_name(priv->join),
			.expected = WERR_OK
		},
		{
			.name = "__UNKNOWN_DOMAIN__",
			.expected = WERR_DS_OBJ_NOT_FOUND
		},
		{
			.name = "unknown.domain.samba.example.com",
			.expected = WERR_DS_OBJ_NOT_FOUND
		},
	};
	int levels[] = {1, 2};
	int level;

	for (i=0; i < ARRAY_SIZE(levels); i++) {
		for (j=0; j < ARRAY_SIZE(names); j++) {
			union drsuapi_DsGetDCInfoRequest req;
			level = levels[i];
			r.in.bind_handle = &priv->bind_handle;
			r.in.level = 1;
			r.in.req = &req;
			
			r.in.req->req1.domain_name = names[j].name;
			r.in.req->req1.level = level;

			r.out.ctr = &ctr;
			r.out.level_out = &level_out;
			
			torture_comment(tctx,
				   "testing DsGetDomainControllerInfo level %d on domainname '%s'\n",
			       r.in.req->req1.level, r.in.req->req1.domain_name);
		
			status = dcerpc_drsuapi_DsGetDomainControllerInfo(p, tctx, &r);
			torture_assert_ntstatus_ok(tctx, status,
				   "dcerpc_drsuapi_DsGetDomainControllerInfo with dns domain failed");
			torture_assert_werr_equal(tctx,
									  r.out.result, names[j].expected, 
					   "DsGetDomainControllerInfo level with dns domain failed");
		
			if (!W_ERROR_IS_OK(r.out.result)) {
				/* If this was an error, we can't read the result structure */
				continue;
			}

			torture_assert_int_equal(tctx,
						 r.in.req->req1.level, *r.out.level_out,
						 "dcerpc_drsuapi_DsGetDomainControllerInfo in/out level differs");

			switch (level) {
			case 1:
				for (k=0; k < r.out.ctr->ctr1.count; k++) {
					if (strcasecmp_m(r.out.ctr->ctr1.array[k].netbios_name,
							 torture_join_netbios_name(priv->join)) == 0) {
						found = true;
						break;
					}
				}
				break;
			case 2:
				for (k=0; k < r.out.ctr->ctr2.count; k++) {
					if (strcasecmp_m(r.out.ctr->ctr2.array[k].netbios_name,
							 torture_join_netbios_name(priv->join)) == 0) {
						found = true;
						priv->dcinfo	= r.out.ctr->ctr2.array[k];
						break;
					}
				}
				break;
			}
			torture_assert(tctx, found,
				 "dcerpc_drsuapi_DsGetDomainControllerInfo: Failed to find the domain controller we just created during the join");
		}
	}

	r.in.bind_handle = &priv->bind_handle;
	r.in.level = 1;

	r.out.ctr = &ctr;
	r.out.level_out = &level_out;

	r.in.req->req1.domain_name = "__UNKNOWN_DOMAIN__"; /* This is clearly ignored for this level */
	r.in.req->req1.level = -1;
	
	torture_comment(tctx, "testing DsGetDomainControllerInfo level %d on domainname '%s'\n",
			r.in.req->req1.level, r.in.req->req1.domain_name);
	
	status = dcerpc_drsuapi_DsGetDomainControllerInfo(p, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status,
				   "dcerpc_drsuapi_DsGetDomainControllerInfo with dns domain failed");
	torture_assert_werr_ok(tctx, r.out.result,
				"DsGetDomainControllerInfo with dns domain failed");
	
	{
		const char *dc_account = talloc_asprintf(tctx, "%s\\%s$",
							 torture_join_dom_netbios_name(priv->join), 
							 priv->dcinfo.netbios_name);
		torture_comment(tctx, "%s: Enum active LDAP sessions searching for %s\n", __func__, dc_account);
		for (k=0; k < r.out.ctr->ctr01.count; k++) {
			if (strcasecmp_m(r.out.ctr->ctr01.array[k].client_account,
					 dc_account)) {
				found = true;
				break;
			}
		}
		torture_assert(tctx, found,
			"dcerpc_drsuapi_DsGetDomainControllerInfo level: Failed to find the domain controller in last logon records");
	}


	return true;
}

static bool test_DsWriteAccountSpn(struct torture_context *tctx,
				   struct DsPrivate *priv)
{
	NTSTATUS status;
	struct dcerpc_pipe *p = priv->pipe;
	struct drsuapi_DsWriteAccountSpn r;
	union drsuapi_DsWriteAccountSpnRequest req;
	struct drsuapi_DsNameString names[2];
	union drsuapi_DsWriteAccountSpnResult res;
	int32_t level_out;

	r.in.bind_handle		= &priv->bind_handle;
	r.in.level			= 1;
	r.in.req			= &req;

	torture_comment(tctx, "testing DsWriteAccountSpn\n");

	r.in.req->req1.operation	= DRSUAPI_DS_SPN_OPERATION_ADD;
	r.in.req->req1.unknown1	= 0;
	r.in.req->req1.object_dn	= priv->dcinfo.computer_dn;
	r.in.req->req1.count		= 2;
	r.in.req->req1.spn_names	= names;
	names[0].str = talloc_asprintf(tctx, "smbtortureSPN/%s",priv->dcinfo.netbios_name);
	names[1].str = talloc_asprintf(tctx, "smbtortureSPN/%s",priv->dcinfo.dns_name);

	r.out.res			= &res;
	r.out.level_out			= &level_out;

	status = dcerpc_drsuapi_DsWriteAccountSpn(p, tctx, &r);
	torture_drsuapi_assert_call(tctx, p, status, &r, "dcerpc_drsuapi_DsWriteAccountSpn");

	r.in.req->req1.operation	= DRSUAPI_DS_SPN_OPERATION_DELETE;
	r.in.req->req1.unknown1		= 0;

	status = dcerpc_drsuapi_DsWriteAccountSpn(p, tctx, &r);
	torture_drsuapi_assert_call(tctx, p, status, &r, "dcerpc_drsuapi_DsWriteAccountSpn");

	return true;
}

static bool test_DsReplicaGetInfo(struct torture_context *tctx,
				  struct DsPrivate *priv)
{
	NTSTATUS status;
	struct dcerpc_pipe *p = priv->pipe;
	struct drsuapi_DsReplicaGetInfo r;
	union drsuapi_DsReplicaGetInfoRequest req;
	union drsuapi_DsReplicaInfo info;
	enum drsuapi_DsReplicaInfoType info_type;
	int i;
	struct {
		int32_t level;
		int32_t infotype;
		const char *obj_dn;
	} array[] = {
		{	
			DRSUAPI_DS_REPLICA_GET_INFO,
			DRSUAPI_DS_REPLICA_INFO_NEIGHBORS,
			NULL
		},{
			DRSUAPI_DS_REPLICA_GET_INFO,
			DRSUAPI_DS_REPLICA_INFO_CURSORS,
			NULL
		},{
			DRSUAPI_DS_REPLICA_GET_INFO,
			DRSUAPI_DS_REPLICA_INFO_OBJ_METADATA,
			NULL
		},{
			DRSUAPI_DS_REPLICA_GET_INFO,
			DRSUAPI_DS_REPLICA_INFO_KCC_DSA_CONNECT_FAILURES,
			NULL
		},{
			DRSUAPI_DS_REPLICA_GET_INFO,
			DRSUAPI_DS_REPLICA_INFO_KCC_DSA_LINK_FAILURES,
			NULL
		},{
			DRSUAPI_DS_REPLICA_GET_INFO,
			DRSUAPI_DS_REPLICA_INFO_PENDING_OPS,
			NULL
		},{
			DRSUAPI_DS_REPLICA_GET_INFO2,
			DRSUAPI_DS_REPLICA_INFO_ATTRIBUTE_VALUE_METADATA,
			NULL
		},{
			DRSUAPI_DS_REPLICA_GET_INFO2,
			DRSUAPI_DS_REPLICA_INFO_CURSORS2,
			NULL
		},{
			DRSUAPI_DS_REPLICA_GET_INFO2,
			DRSUAPI_DS_REPLICA_INFO_CURSORS3,
			NULL
		},{
			DRSUAPI_DS_REPLICA_GET_INFO2,
			DRSUAPI_DS_REPLICA_INFO_OBJ_METADATA2,
			NULL
		},{
			DRSUAPI_DS_REPLICA_GET_INFO2,
			DRSUAPI_DS_REPLICA_INFO_ATTRIBUTE_VALUE_METADATA2,
			NULL
		},{
			DRSUAPI_DS_REPLICA_GET_INFO2,
			DRSUAPI_DS_REPLICA_INFO_NEIGHBORS02,
			NULL
		},{
			DRSUAPI_DS_REPLICA_GET_INFO2,
			DRSUAPI_DS_REPLICA_INFO_CONNECTIONS04,
			"__IGNORED__"
		},{
			DRSUAPI_DS_REPLICA_GET_INFO2,
			DRSUAPI_DS_REPLICA_INFO_CURSORS05,
			NULL
		},{
			DRSUAPI_DS_REPLICA_GET_INFO2,
			DRSUAPI_DS_REPLICA_INFO_06,
			NULL
		}
	};

	if (torture_setting_bool(tctx, "samba4", false)) {
		torture_comment(tctx, "skipping DsReplicaGetInfo test against Samba4\n");
		return true;
	}

	r.in.bind_handle	= &priv->bind_handle;
	r.in.req		= &req;

	for (i=0; i < ARRAY_SIZE(array); i++) {
		const char *object_dn;

		torture_comment(tctx, "testing DsReplicaGetInfo level %d infotype %d\n",
				array[i].level, array[i].infotype);

		object_dn = (array[i].obj_dn ? array[i].obj_dn : priv->domain_obj_dn);

		r.in.level = array[i].level;
		switch(r.in.level) {
		case DRSUAPI_DS_REPLICA_GET_INFO:
			r.in.req->req1.info_type	= array[i].infotype;
			r.in.req->req1.object_dn	= object_dn;
			ZERO_STRUCT(r.in.req->req1.guid1);
			break;
		case DRSUAPI_DS_REPLICA_GET_INFO2:
			r.in.req->req2.info_type	= array[i].infotype;
			r.in.req->req2.object_dn	= object_dn;
			ZERO_STRUCT(r.in.req->req2.guid1);
			r.in.req->req2.unknown1	= 0;
			r.in.req->req2.string1	= NULL;
			r.in.req->req2.string2	= NULL;
			r.in.req->req2.unknown2	= 0;
			break;
		}

		r.out.info		= &info;
		r.out.info_type		= &info_type;

		status = dcerpc_drsuapi_DsReplicaGetInfo(p, tctx, &r);
		torture_drsuapi_assert_call(tctx, p, status, &r, "dcerpc_drsuapi_DsReplicaGetInfo");
		if (!NT_STATUS_IS_OK(status) && p->last_fault_code == DCERPC_FAULT_INVALID_TAG) {
			torture_comment(tctx,
					"DsReplicaGetInfo level %d and/or infotype %d not supported by server\n",
					array[i].level, array[i].infotype);
		} else {
			torture_drsuapi_assert_call(tctx, p, status, &r, "dcerpc_drsuapi_DsReplicaGetInfo");
		}
	}

	return true;
}

static bool test_DsReplicaSync(struct torture_context *tctx,
				struct DsPrivate *priv)
{
	NTSTATUS status;
	struct dcerpc_pipe *p = priv->pipe;
	int i;
	struct drsuapi_DsReplicaSync r;
	struct drsuapi_DsReplicaObjectIdentifier nc;
	struct GUID null_guid;
	struct dom_sid null_sid;
	struct {
		int32_t level;
	} array[] = {
		{	
			1
		}
	};

	if (!torture_setting_bool(tctx, "dangerous", false)) {
		torture_comment(tctx, "DsReplicaSync disabled - enable dangerous tests to use\n");
		return true;
	}

	if (torture_setting_bool(tctx, "samba4", false)) {
		torture_comment(tctx, "skipping DsReplicaSync test against Samba4\n");
		return true;
	}

	ZERO_STRUCT(null_guid);
	ZERO_STRUCT(null_sid);

	r.in.bind_handle	= &priv->bind_handle;

	for (i=0; i < ARRAY_SIZE(array); i++) {
		torture_comment(tctx, "testing DsReplicaSync level %d\n",
				array[i].level);

		r.in.level = array[i].level;
		switch(r.in.level) {
		case 1:
			nc.guid					= null_guid;
			nc.sid					= null_sid;
			nc.dn					= priv->domain_obj_dn?priv->domain_obj_dn:"";

			r.in.req.req1.naming_context		= &nc;
			r.in.req.req1.source_dsa_guid		= priv->dcinfo.ntds_guid;
			r.in.req.req1.other_info		= NULL;
			r.in.req.req1.options			= 16;
			break;
		}

		status = dcerpc_drsuapi_DsReplicaSync(p, tctx, &r);
		torture_drsuapi_assert_call(tctx, p, status, &r, "dcerpc_drsuapi_DsReplicaSync");
	}

	return true;
}

static bool test_DsReplicaUpdateRefs(struct torture_context *tctx,
				     struct DsPrivate *priv)
{
	NTSTATUS status;
	struct dcerpc_pipe *p = priv->pipe;
	int i;
	struct drsuapi_DsReplicaUpdateRefs r;
	struct drsuapi_DsReplicaObjectIdentifier nc;
	struct GUID null_guid;
	struct dom_sid null_sid;
	struct {
		int32_t level;
	} array[] = {
		{	
			1
		}
	};

	if (torture_setting_bool(tctx, "samba4", false)) {
		torture_comment(tctx, "skipping DsReplicaUpdateRefs test against Samba4\n");
		return true;
	}

	ZERO_STRUCT(null_guid);
	ZERO_STRUCT(null_sid);

	r.in.bind_handle	= &priv->bind_handle;

	for (i=0; i < ARRAY_SIZE(array); i++) {
		torture_comment(tctx, "testing DsReplicaUpdateRefs level %d\n",
				array[i].level);

		r.in.level = array[i].level;
		switch(r.in.level) {
		case 1:
			nc.guid				= null_guid;
			nc.sid				= null_sid;
			nc.dn				= priv->domain_obj_dn ? priv->domain_obj_dn : "";

			r.in.req.req1.naming_context	= &nc;
			r.in.req.req1.dest_dsa_dns_name	= talloc_asprintf(tctx, "__some_dest_dsa_guid_string._msdn.%s",
										priv->domain_dns_name);
			r.in.req.req1.dest_dsa_guid	= null_guid;
			r.in.req.req1.options		= 0;
			break;
		}

		status = dcerpc_drsuapi_DsReplicaUpdateRefs(p, tctx, &r);
		torture_drsuapi_assert_call(tctx, p, status, &r, "dcerpc_drsuapi_DsReplicaUpdateRefs");
	}

	return true;
}

static bool test_DsGetNCChanges(struct torture_context *tctx,
				struct DsPrivate *priv)
{
	NTSTATUS status;
	struct dcerpc_pipe *p = priv->pipe;
	int i;
	struct drsuapi_DsGetNCChanges r;
	union drsuapi_DsGetNCChangesRequest req;
	union drsuapi_DsGetNCChangesCtr ctr;
	struct drsuapi_DsReplicaObjectIdentifier nc;
	struct GUID null_guid;
	struct dom_sid null_sid;
	int32_t level_out;
	struct {
		int32_t level;
	} array[] = {
		{	
			5
		},
		{	
			8
		}
	};

	if (torture_setting_bool(tctx, "samba4", false)) {
		torture_comment(tctx, "skipping DsGetNCChanges test against Samba4\n");
		return true;
	}

	ZERO_STRUCT(null_guid);
	ZERO_STRUCT(null_sid);

	for (i=0; i < ARRAY_SIZE(array); i++) {
		torture_comment(tctx,
				"testing DsGetNCChanges level %d\n",
				array[i].level);

		r.in.bind_handle	= &priv->bind_handle;
		r.in.level		= array[i].level;
		r.out.level_out		= &level_out;
		r.out.ctr		= &ctr;

		switch (r.in.level) {
		case 5:
			nc.guid	= null_guid;
			nc.sid	= null_sid;
			nc.dn	= priv->domain_obj_dn ? priv->domain_obj_dn : "";

			r.in.req					= &req;
			r.in.req->req5.destination_dsa_guid		= GUID_random();
			r.in.req->req5.source_dsa_invocation_id		= null_guid;
			r.in.req->req5.naming_context			= &nc;
			r.in.req->req5.highwatermark.tmp_highest_usn	= 0;
			r.in.req->req5.highwatermark.reserved_usn	= 0;
			r.in.req->req5.highwatermark.highest_usn	= 0;
			r.in.req->req5.uptodateness_vector		= NULL;
			r.in.req->req5.replica_flags			= 0;
			if (lp_parm_bool(tctx->lp_ctx, NULL, "drsuapi", "compression", false)) {
				r.in.req->req5.replica_flags		|= DRSUAPI_DS_REPLICA_NEIGHBOUR_COMPRESS_CHANGES;
			}
			r.in.req->req5.max_object_count			= 0;
			r.in.req->req5.max_ndr_size			= 0;
			r.in.req->req5.extended_op			= DRSUAPI_EXOP_NONE;
			r.in.req->req5.fsmo_info			= 0;

			break;
		case 8:
			nc.guid	= null_guid;
			nc.sid	= null_sid;
			nc.dn	= priv->domain_obj_dn ? priv->domain_obj_dn : "";

			r.in.req					= &req;
			r.in.req->req8.destination_dsa_guid		= GUID_random();
			r.in.req->req8.source_dsa_invocation_id		= null_guid;
			r.in.req->req8.naming_context			= &nc;
			r.in.req->req8.highwatermark.tmp_highest_usn	= 0;
			r.in.req->req8.highwatermark.reserved_usn	= 0;
			r.in.req->req8.highwatermark.highest_usn	= 0;
			r.in.req->req8.uptodateness_vector		= NULL;
			r.in.req->req8.replica_flags			= 0;
			if (lp_parm_bool(tctx->lp_ctx, NULL, "drsuapi", "compression", false)) {
				r.in.req->req8.replica_flags		|= DRSUAPI_DS_REPLICA_NEIGHBOUR_COMPRESS_CHANGES;
			}
			if (lp_parm_bool(tctx->lp_ctx, NULL, "drsuapi", "neighbour_writeable", true)) {
				r.in.req->req8.replica_flags		|= DRSUAPI_DS_REPLICA_NEIGHBOUR_WRITEABLE;
			}
			r.in.req->req8.replica_flags			|= DRSUAPI_DS_REPLICA_NEIGHBOUR_SYNC_ON_STARTUP
									| DRSUAPI_DS_REPLICA_NEIGHBOUR_DO_SCHEDULED_SYNCS
									| DRSUAPI_DS_REPLICA_NEIGHBOUR_RETURN_OBJECT_PARENTS
									| DRSUAPI_DS_REPLICA_NEIGHBOUR_NEVER_SYNCED
									;
			r.in.req->req8.max_object_count			= 402;
			r.in.req->req8.max_ndr_size			= 402116;
			r.in.req->req8.extended_op			= DRSUAPI_EXOP_NONE;
			r.in.req->req8.fsmo_info			= 0;
			r.in.req->req8.partial_attribute_set		= NULL;
			r.in.req->req8.partial_attribute_set_ex		= NULL;
			r.in.req->req8.mapping_ctr.num_mappings		= 0;
			r.in.req->req8.mapping_ctr.mappings		= NULL;

			break;
		}

		status = dcerpc_drsuapi_DsGetNCChanges(p, tctx, &r);
		torture_drsuapi_assert_call(tctx, p, status, &r, "dcerpc_drsuapi_DsGetNCChanges");
	}

	return true;
}

bool test_QuerySitesByCost(struct torture_context *tctx,
			   struct DsPrivate *priv)
{
	NTSTATUS status;
	struct dcerpc_pipe *p = priv->pipe;
	struct drsuapi_QuerySitesByCost r;
	union drsuapi_QuerySitesByCostRequest req;

	const char *my_site = "Default-First-Site-Name";
	const char *remote_site1 = "smbtorture-nonexisting-site1";
	const char *remote_site2 = "smbtorture-nonexisting-site2";

	req.req1.site_from = talloc_strdup(tctx, my_site);
	req.req1.num_req = 2;
	req.req1.site_to = talloc_zero_array(tctx, const char *, 2);
	req.req1.site_to[0] = talloc_strdup(tctx, remote_site1);
	req.req1.site_to[1] = talloc_strdup(tctx, remote_site2);
	req.req1.flags = 0;

	r.in.bind_handle = &priv->bind_handle;
	r.in.level = 1;
	r.in.req = &req;

	status = dcerpc_drsuapi_QuerySitesByCost(p, tctx, &r);
	torture_drsuapi_assert_call(tctx, p, status, &r, "dcerpc_drsuapi_QuerySitesByCost");

	if (W_ERROR_IS_OK(r.out.result)) {
		torture_assert_werr_equal(tctx,
					  r.out.ctr->ctr1.info[0].error_code, WERR_DS_OBJ_NOT_FOUND,
					  "dcerpc_drsuapi_QuerySitesByCost");
		torture_assert_werr_equal(tctx,
					  r.out.ctr->ctr1.info[1].error_code, WERR_DS_OBJ_NOT_FOUND,
					  "dcerpc_drsuapi_QuerySitesByCost expected error_code WERR_DS_OBJ_NOT_FOUND");

		torture_assert_int_equal(tctx,
					 r.out.ctr->ctr1.info[0].site_cost, -1,
					 "dcerpc_drsuapi_QuerySitesByCost");
		torture_assert_int_equal(tctx,
					 r.out.ctr->ctr1.info[1].site_cost, -1,
					 "dcerpc_drsuapi_QuerySitesByCost exptected site cost");
	}

	return true;


}

bool test_DsUnbind(struct dcerpc_pipe *p,
		   struct torture_context *tctx,
		   struct DsPrivate *priv)
{
	NTSTATUS status;
	struct drsuapi_DsUnbind r;

	r.in.bind_handle = &priv->bind_handle;
	r.out.bind_handle = &priv->bind_handle;

	torture_comment(tctx, "testing DsUnbind\n");

	status = dcerpc_drsuapi_DsUnbind(p, tctx, &r);
	torture_drsuapi_assert_call(tctx, p, status, &r, "dcerpc_drsuapi_DsUnbind");

	return true;
}


/**
 * Helper func to collect DC information for testing purposes.
 * This function is almost identical to test_DsGetDomainControllerInfo
 */
bool torture_rpc_drsuapi_get_dcinfo(struct torture_context *torture,
				    struct DsPrivate *priv)
{
	NTSTATUS status;
	int32_t level_out = 0;
	struct drsuapi_DsGetDomainControllerInfo r;
	union drsuapi_DsGetDCInfoCtr ctr;
	int j, k;
	const char *names[] = {
				torture_join_dom_netbios_name(priv->join),
				torture_join_dom_dns_name(priv->join)};

	for (j=0; j < ARRAY_SIZE(names); j++) {
		union drsuapi_DsGetDCInfoRequest req;
		r.in.bind_handle = &priv->bind_handle;
		r.in.level = 1;
		r.in.req = &req;

		r.in.req->req1.domain_name = names[j];
		r.in.req->req1.level = 2;

		r.out.ctr = &ctr;
		r.out.level_out = &level_out;

		status = dcerpc_drsuapi_DsGetDomainControllerInfo(priv->pipe, torture, &r);
		if (!NT_STATUS_IS_OK(status)) {
			continue;
		}
		if (!W_ERROR_IS_OK(r.out.result)) {
			/* If this was an error, we can't read the result structure */
			continue;
		}

		for (k=0; k < r.out.ctr->ctr2.count; k++) {
			if (strcasecmp_m(r.out.ctr->ctr2.array[k].netbios_name,
					 torture_join_netbios_name(priv->join)) == 0) {
				priv->dcinfo	= r.out.ctr->ctr2.array[k];
				return true;
			}
		}
	}

	return false;
}

/**
 * Common test case setup function to be used
 * in DRS suit of test when appropriate
 */
bool torture_drsuapi_tcase_setup_common(struct torture_context *tctx, struct DsPrivate *priv)
{
        NTSTATUS status;
	struct cli_credentials *machine_credentials;

	torture_assert(tctx, priv, "Invalid argument");

	torture_comment(tctx, "Create DRSUAPI pipe\n");
	status = torture_rpc_connection(tctx,
					&priv->pipe,
					&ndr_table_drsuapi);
	torture_assert(tctx, NT_STATUS_IS_OK(status), "Unable to connect to DRSUAPI pipe");

	torture_comment(tctx, "About to join domain\n");
	priv->join = torture_join_domain(tctx, TEST_MACHINE_NAME, ACB_SVRTRUST,
					 &machine_credentials);
	torture_assert(tctx, priv->join, "Failed to join as BDC");

	if (!test_DsBind(priv->pipe, tctx, priv)) {
		/* clean up */
		torture_drsuapi_tcase_teardown_common(tctx, priv);
		torture_fail(tctx, "Failed execute test_DsBind()");
	}

	/* try collect some information for testing */
	torture_rpc_drsuapi_get_dcinfo(tctx, priv);

	return true;
}

/**
 * Common test case teardown function to be used
 * in DRS suit of test when appropriate
 */
bool torture_drsuapi_tcase_teardown_common(struct torture_context *tctx, struct DsPrivate *priv)
{
	if (priv->join) {
		torture_leave_domain(tctx, priv->join);
	}

	return true;
}

/**
 * Test case setup for DRSUAPI test case
 */
static bool torture_drsuapi_tcase_setup(struct torture_context *tctx, void **data)
{
	struct DsPrivate *priv;

	*data = priv = talloc_zero(tctx, struct DsPrivate);

	return torture_drsuapi_tcase_setup_common(tctx, priv);
}

/**
 * Test case tear-down for DRSUAPI test case
 */
static bool torture_drsuapi_tcase_teardown(struct torture_context *tctx, void *data)
{
	bool ret;
	struct DsPrivate *priv = talloc_get_type(data, struct DsPrivate);

	ret = torture_drsuapi_tcase_teardown_common(tctx, priv);

	talloc_free(priv);
	return ret;
}

/**
 * DRSUAPI test case implementation
 */
void torture_rpc_drsuapi_tcase(struct torture_suite *suite)
{
	typedef bool (*run_func) (struct torture_context *test, void *tcase_data);

	struct torture_test *test;
	struct torture_tcase *tcase = torture_suite_add_tcase(suite, "DRSUAPI");

	torture_tcase_set_fixture(tcase,
				  torture_drsuapi_tcase_setup,
				  torture_drsuapi_tcase_teardown);

#if 0
	test = torture_tcase_add_simple_test(tcase, "QuerySitesByCost", (run_func)test_QuerySitesByCost);
#endif

	test = torture_tcase_add_simple_test(tcase, "DsGetDomainControllerInfo", (run_func)test_DsGetDomainControllerInfo);

	test = torture_tcase_add_simple_test(tcase, "DsCrackNames", (run_func)test_DsCrackNames);

	test = torture_tcase_add_simple_test(tcase, "DsWriteAccountSpn", (run_func)test_DsWriteAccountSpn);

	test = torture_tcase_add_simple_test(tcase, "DsReplicaGetInfo", (run_func)test_DsReplicaGetInfo);

	test = torture_tcase_add_simple_test(tcase, "DsReplicaSync", (run_func)test_DsReplicaSync);

	test = torture_tcase_add_simple_test(tcase, "DsReplicaUpdateRefs", (run_func)test_DsReplicaUpdateRefs);

	test = torture_tcase_add_simple_test(tcase, "DsGetNCChanges", (run_func)test_DsGetNCChanges);
}
