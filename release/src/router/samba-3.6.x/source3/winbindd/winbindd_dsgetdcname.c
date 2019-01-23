/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_DSGETDCNAME
   Copyright (C) Volker Lendecke 2009

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
#include "winbindd.h"
#include "librpc/gen_ndr/ndr_wbint_c.h"

struct winbindd_dsgetdcname_state {
	struct GUID guid;
	struct netr_DsRGetDCNameInfo *dc_info;
};

static uint32_t get_dsgetdc_flags(uint32_t wbc_flags);
static void winbindd_dsgetdcname_done(struct tevent_req *subreq);

struct tevent_req *winbindd_dsgetdcname_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct winbindd_cli_state *cli,
					     struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_child *child;
	struct winbindd_dsgetdcname_state *state;
	struct GUID *guid_ptr = NULL;
	uint32_t ds_flags = 0;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_dsgetdcname_state);
	if (req == NULL) {
		return NULL;
	}

	request->data.dsgetdcname.domain_name
		[sizeof(request->data.dsgetdcname.domain_name)-1] = '\0';
	request->data.dsgetdcname.site_name
		[sizeof(request->data.dsgetdcname.site_name)-1] = '\0';
	request->data.dsgetdcname.domain_guid
		[sizeof(request->data.dsgetdcname.domain_guid)-1] = '\0';

	DEBUG(3, ("[%5lu]: dsgetdcname for %s\n", (unsigned long)cli->pid,
		  request->data.dsgetdcname.domain_name));

	ds_flags = get_dsgetdc_flags(request->flags);

	status = GUID_from_string(request->data.dsgetdcname.domain_guid,
				  &state->guid);
	if (NT_STATUS_IS_OK(status) && !GUID_all_zero(&state->guid)) {
		guid_ptr = &state->guid;
	}

	child = locator_child();

	subreq = dcerpc_wbint_DsGetDcName_send(
		state, ev, child->binding_handle,
		request->data.dsgetdcname.domain_name, guid_ptr,
		request->data.dsgetdcname.site_name,
		ds_flags, &state->dc_info);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_dsgetdcname_done, req);
	return req;
}

static void winbindd_dsgetdcname_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_dsgetdcname_state *state = tevent_req_data(
		req, struct winbindd_dsgetdcname_state);
	NTSTATUS status, result;

	status = dcerpc_wbint_DsGetDcName_recv(subreq, state, &result);
	TALLOC_FREE(subreq);
	if (any_nt_status_not_ok(status, result, &status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS winbindd_dsgetdcname_recv(struct tevent_req *req,
				   struct winbindd_response *response)
{
	struct winbindd_dsgetdcname_state *state = tevent_req_data(
		req, struct winbindd_dsgetdcname_state);
	char *guid_str;
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		DEBUG(5, ("dsgetdcname failed: %s\n", nt_errstr(status)));
		return status;
	}

	guid_str = GUID_string(talloc_tos(), &state->dc_info->domain_guid);
	if (guid_str == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	fstrcpy(response->data.dsgetdcname.dc_unc,
		state->dc_info->dc_unc);
	fstrcpy(response->data.dsgetdcname.dc_address,
		state->dc_info->dc_address);
	response->data.dsgetdcname.dc_address_type =
		state->dc_info->dc_address_type;
	fstrcpy(response->data.dsgetdcname.domain_guid, guid_str);
	TALLOC_FREE(guid_str);
	fstrcpy(response->data.dsgetdcname.domain_name,
		state->dc_info->domain_name);
	fstrcpy(response->data.dsgetdcname.forest_name,
		state->dc_info->forest_name);
	response->data.dsgetdcname.dc_flags = state->dc_info->dc_flags;
	fstrcpy(response->data.dsgetdcname.dc_site_name,
		state->dc_info->dc_site_name);
	fstrcpy(response->data.dsgetdcname.client_site_name,
		state->dc_info->client_site_name);

	return NT_STATUS_OK;
}

static uint32_t get_dsgetdc_flags(uint32_t wbc_flags)
{
	struct wbc_flag_map {
		uint32_t wbc_dc_flag;
		uint32_t ds_dc_flags;
	} lookup_dc_flags[] = {
		{ WBC_LOOKUP_DC_FORCE_REDISCOVERY,
		  DS_FORCE_REDISCOVERY },
		{ WBC_LOOKUP_DC_DS_REQUIRED,
		  DS_DIRECTORY_SERVICE_REQUIRED },
		{ WBC_LOOKUP_DC_DS_PREFERRED,
		  DS_DIRECTORY_SERVICE_PREFERRED},
		{ WBC_LOOKUP_DC_GC_SERVER_REQUIRED,
		  DS_GC_SERVER_REQUIRED },
		{ WBC_LOOKUP_DC_PDC_REQUIRED,
		  DS_PDC_REQUIRED},
		{ WBC_LOOKUP_DC_BACKGROUND_ONLY,
		  DS_BACKGROUND_ONLY  },
		{ WBC_LOOKUP_DC_IP_REQUIRED,
		  DS_IP_REQUIRED },
		{ WBC_LOOKUP_DC_KDC_REQUIRED,
		  DS_KDC_REQUIRED },
		{ WBC_LOOKUP_DC_TIMESERV_REQUIRED,
		  DS_TIMESERV_REQUIRED },
		{ WBC_LOOKUP_DC_WRITABLE_REQUIRED,
		  DS_WRITABLE_REQUIRED },
		{ WBC_LOOKUP_DC_GOOD_TIMESERV_PREFERRED,
		  DS_GOOD_TIMESERV_PREFERRED },
		{ WBC_LOOKUP_DC_AVOID_SELF,
		  DS_AVOID_SELF },
		{ WBC_LOOKUP_DC_ONLY_LDAP_NEEDED,
		  DS_ONLY_LDAP_NEEDED },
		{ WBC_LOOKUP_DC_IS_FLAT_NAME,
		  DS_IS_FLAT_NAME },
		{ WBC_LOOKUP_DC_IS_DNS_NAME,
		  DS_IS_DNS_NAME },
		{ WBC_LOOKUP_DC_TRY_NEXTCLOSEST_SITE,
		  DS_TRY_NEXTCLOSEST_SITE },
		{ WBC_LOOKUP_DC_DS_6_REQUIRED,
		  DS_DIRECTORY_SERVICE_6_REQUIRED },
		{ WBC_LOOKUP_DC_RETURN_DNS_NAME,
		  DS_RETURN_DNS_NAME },
		{ WBC_LOOKUP_DC_RETURN_FLAT_NAME,
		  DS_RETURN_FLAT_NAME }
	};

	uint32_t ds_flags = 0;
	int i = 0 ;

	for (i=0; i<ARRAY_SIZE(lookup_dc_flags); i++) {
		if (wbc_flags & lookup_dc_flags[i].wbc_dc_flag) {
			ds_flags |= lookup_dc_flags[i].ds_dc_flags;
		}
	}

	return ds_flags;
}
