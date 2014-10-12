/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_SIDS_TO_XIDS
   Copyright (C) Volker Lendecke 2011

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
#include "../libcli/security/security.h"
#include "librpc/gen_ndr/ndr_wbint_c.h"
#include "idmap_cache.h"

struct winbindd_sids_to_xids_state {
	struct tevent_context *ev;
	struct dom_sid *sids;
	uint32_t num_sids;

	struct id_map *cached;

	struct dom_sid *non_cached;
	uint32_t num_non_cached;

	struct lsa_RefDomainList *domains;
	struct lsa_TransNameArray *names;

	struct wbint_TransIDArray ids;
};

static bool winbindd_sids_to_xids_in_cache(struct dom_sid *sid,
					   struct id_map *map);
static void winbindd_sids_to_xids_lookupsids_done(struct tevent_req *subreq);
static void winbindd_sids_to_xids_done(struct tevent_req *subreq);

struct tevent_req *winbindd_sids_to_xids_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct winbindd_cli_state *cli,
					      struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_sids_to_xids_state *state;
	uint32_t i;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_sids_to_xids_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;

	DEBUG(3, ("sids_to_xids\n"));

	if (request->extra_len == 0) {
		tevent_req_done(req);
		return tevent_req_post(req, ev);
	}
	if (request->extra_data.data[request->extra_len-1] != '\0') {
		DEBUG(10, ("Got invalid sids list\n"));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}
	if (!parse_sidlist(state, request->extra_data.data,
			   &state->sids, &state->num_sids)) {
		DEBUG(10, ("parse_sidlist failed\n"));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	DEBUG(10, ("num_sids: %d\n", (int)state->num_sids));

	state->cached = TALLOC_ZERO_ARRAY(state, struct id_map,
					  state->num_sids);
	if (tevent_req_nomem(state->cached, req)) {
		return tevent_req_post(req, ev);
	}
	state->non_cached = TALLOC_ARRAY(state, struct dom_sid,
					 state->num_sids);
	if (tevent_req_nomem(state->non_cached, req)) {
		return tevent_req_post(req, ev);
	}

	for (i=0; i<state->num_sids; i++) {

		DEBUG(10, ("SID %d: %s\n", (int)i,
			   sid_string_dbg(&state->sids[i])));

		if (winbindd_sids_to_xids_in_cache(&state->sids[i],
						   &state->cached[i])) {
			continue;
		}
		sid_copy(&state->non_cached[state->num_non_cached],
			 &state->sids[i]);
		state->num_non_cached += 1;
	}

        if (state->num_non_cached == 0) {
                tevent_req_done(req);
                return tevent_req_post(req, ev);
        }

	subreq = wb_lookupsids_send(state, ev, state->non_cached,
				    state->num_non_cached);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_sids_to_xids_lookupsids_done,
				req);
	return req;
}

static bool winbindd_sids_to_xids_in_cache(struct dom_sid *sid,
					   struct id_map *map)
{
	bool is_online = is_domain_online(find_our_domain());
	gid_t gid = (gid_t)-1;
	bool gid_expired = false;
	bool gid_cached = false;
	bool gid_negative = false;
	uid_t uid = (uid_t)-1;
	bool uid_expired = false;
	bool uid_cached = false;
	bool uid_negative = false;

	if (!winbindd_use_idmap_cache()) {
		return false;
	}

	/*
	 * SIDS_TO_XIDS is primarily used to resolve the user's group
	 * sids. So we check groups before users.
	 */
	gid_cached = idmap_cache_find_sid2gid(sid, &gid, &gid_expired);
	if (!is_online) {
		gid_expired = false;
	}
	if (gid_cached && !gid_expired) {
		if (gid != (gid_t)-1) {
			map->sid = sid;
			map->xid.id = gid;
			map->xid.type = ID_TYPE_GID;
			map->status = ID_MAPPED;
			return true;
		}
		gid_negative = true;
	}
	uid_cached = idmap_cache_find_sid2uid(sid, &uid, &uid_expired);
	if (!is_online) {
		uid_expired = false;
	}
	if (uid_cached && !uid_expired) {
		if (uid != (uid_t)-1) {
			map->sid = sid;
			map->xid.id = uid;
			map->xid.type = ID_TYPE_UID;
			map->status = ID_MAPPED;
			return true;
		}
		uid_negative = true;
	}

	/*
	 * Here we know that we only have negative
	 * or no entries.
	 *
	 * All valid cases already returned to the caller.
	 */

	if (gid_negative && uid_negative) {
		map->sid = sid;
		map->xid.id = UINT32_MAX;
		map->xid.type = ID_TYPE_NOT_SPECIFIED;
		map->status = ID_MAPPED;
		return true;
	}

	if (gid_negative) {
		map->sid = sid;
		map->xid.id = gid; /* this is (gid_t)-1 */
		map->xid.type = ID_TYPE_GID;
		map->status = ID_MAPPED;
		return true;
	}

	if (uid_negative) {
		map->sid = sid;
		map->xid.id = uid; /* this is (uid_t)-1 */
		map->xid.type = ID_TYPE_UID;
		map->status = ID_MAPPED;
		return true;
	}

	return false;
}

static void winbindd_sids_to_xids_lookupsids_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_sids_to_xids_state *state = tevent_req_data(
		req, struct winbindd_sids_to_xids_state);
	struct winbindd_child *child;
	NTSTATUS status;
	int i;

	status = wb_lookupsids_recv(subreq, state, &state->domains,
				    &state->names);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	state->ids.num_ids = state->num_non_cached;
	state->ids.ids = TALLOC_ARRAY(state, struct wbint_TransID,
				      state->num_non_cached);
	if (tevent_req_nomem(state->ids.ids, req)) {
		return;
	}

	for (i=0; i<state->num_non_cached; i++) {
		struct lsa_TranslatedName *n = &state->names->names[i];
		struct wbint_TransID *t = &state->ids.ids[i];

		switch (n->sid_type) {
		case SID_NAME_USER:
		case SID_NAME_COMPUTER:
			t->type = WBC_ID_TYPE_UID;
			break;
		case SID_NAME_DOM_GRP:
		case SID_NAME_ALIAS:
		case SID_NAME_WKN_GRP:
			t->type = WBC_ID_TYPE_GID;
			break;
		default:
			t->type = WBC_ID_TYPE_NOT_SPECIFIED;
			break;
		};
		t->domain_index = n->sid_index;
		sid_peek_rid(&state->non_cached[i], &t->rid);
		t->unix_id = (uint64_t)-1;
	}

	child = idmap_child();

	subreq = dcerpc_wbint_Sids2UnixIDs_send(
		state, state->ev, child->binding_handle, state->domains,
		&state->ids);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, winbindd_sids_to_xids_done, req);
}

static void winbindd_sids_to_xids_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_sids_to_xids_state *state = tevent_req_data(
		req, struct winbindd_sids_to_xids_state);
	NTSTATUS status, result;

	status = dcerpc_wbint_Sids2UnixIDs_recv(subreq, state, &result);
	TALLOC_FREE(subreq);
	if (any_nt_status_not_ok(status, result, &status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS winbindd_sids_to_xids_recv(struct tevent_req *req,
				    struct winbindd_response *response)
{
	struct winbindd_sids_to_xids_state *state = tevent_req_data(
		req, struct winbindd_sids_to_xids_state);
	NTSTATUS status;
	char *result;
	uint32_t i, num_non_cached;

	if (tevent_req_is_nterror(req, &status)) {
		DEBUG(5, ("wb_sids_to_xids failed: %s\n", nt_errstr(status)));
		return status;
	}

	result = talloc_strdup(response, "");
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	num_non_cached = 0;

	for (i=0; i<state->num_sids; i++) {
		char type;
		uint32_t unix_id = UINT32_MAX;
		bool found = true;

		if (state->cached[i].sid != NULL) {
			unix_id = state->cached[i].xid.id;

			switch (state->cached[i].xid.type) {
			case ID_TYPE_UID:
				type = 'U';
				break;
			case ID_TYPE_GID:
				type = 'G';
				break;
			default:
				found = false;
				break;
			}
		} else {
			unix_id = state->ids.ids[num_non_cached].unix_id;

			switch(state->ids.ids[num_non_cached].type) {
			case WBC_ID_TYPE_UID:
				type = 'U';
				idmap_cache_set_sid2uid(
					&state->non_cached[num_non_cached],
					unix_id);
				break;
			case WBC_ID_TYPE_GID:
				type = 'G';
				idmap_cache_set_sid2gid(
					&state->non_cached[num_non_cached],
					unix_id);
				break;
			default:
				found = false;
				break;
			}
			num_non_cached += 1;
		}

		if (unix_id == UINT32_MAX) {
			found = false;
		}

		if (found) {
			result = talloc_asprintf_append_buffer(
				result, "%c%lu\n", type,
				(unsigned long)unix_id);
		} else {
			result = talloc_asprintf_append_buffer(result, "\n");
		}
		if (result == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	response->extra_data.data = result;
	response->length += talloc_get_size(result);
	return NT_STATUS_OK;
}
