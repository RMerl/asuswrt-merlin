/*
   Unix SMB/CIFS implementation.

   async seqnums, update the seqnums in winbindd_cache.c

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

struct wb_seqnums_state {
	int num_domains;
	int num_received;

	struct tevent_req **subreqs;
	struct winbindd_domain **domains;
	NTSTATUS *stati;
	uint32_t *seqnums;
};

static void wb_seqnums_done(struct tevent_req *subreq);

struct tevent_req *wb_seqnums_send(TALLOC_CTX *mem_ctx,
				   struct tevent_context *ev)
{
	struct tevent_req *req;
	struct wb_seqnums_state *state;
	struct winbindd_domain *domain;
	int i;

	req = tevent_req_create(mem_ctx, &state, struct wb_seqnums_state);
	if (req == NULL) {
		return NULL;
	}
	state->num_received = 0;
	state->num_domains = 0;

	for (domain = domain_list(); domain != NULL; domain = domain->next) {
		state->num_domains += 1;
	}

	state->subreqs = talloc_array(state, struct tevent_req *,
				      state->num_domains);
	state->domains = talloc_array(state, struct winbindd_domain *,
				      state->num_domains);
	state->stati = talloc_array(state, NTSTATUS, state->num_domains);
	state->seqnums = talloc_array(state, uint32_t, state->num_domains);

	if ((state->subreqs == NULL) || (state->domains == NULL) ||
	    (state->stati == NULL) || (state->seqnums == NULL)) {
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return tevent_req_post(req, ev);
	}

	i = 0;

	for (domain = domain_list(); domain != NULL; domain = domain->next) {
		state->domains[i] = domain;
		state->subreqs[i] = wb_seqnum_send(state->subreqs, ev, domain);
		if (tevent_req_nomem(state->subreqs[i], req)) {
			/* Don't even start all the other requests */
			TALLOC_FREE(state->subreqs);
			return tevent_req_post(req, ev);
		}
		tevent_req_set_callback(state->subreqs[i], wb_seqnums_done,
					req);
		i += 1;
	}
	return req;
}

static void wb_seqnums_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_seqnums_state *state = tevent_req_data(
		req, struct wb_seqnums_state);
	NTSTATUS status;
	uint32_t seqnum;
	int i;

	status = wb_seqnum_recv(subreq, &seqnum);

	for (i=0; i<state->num_domains; i++) {
		if (subreq == state->subreqs[i]) {
			break;
		}
	}
	if (i < state->num_domains) {
		/* found one */

		state->subreqs[i] = NULL;
		state->stati[i] = status;
		if (NT_STATUS_IS_OK(status)) {
			state->seqnums[i] = seqnum;

			/*
			 * This first assignment might be removed
			 * later
			 */
			state->domains[i]->sequence_number = seqnum;

			if (!wcache_store_seqnum(state->domains[i]->name,
						 state->seqnums[i],
						 time(NULL))) {
				DEBUG(1, ("wcache_store_seqnum failed for "
					  "domain %s\n",
					  state->domains[i]->name));
			}
		}
	}

	TALLOC_FREE(subreq);

	state->num_received += 1;

	if (state->num_received >= state->num_domains) {
		tevent_req_done(req);
	}
}

NTSTATUS wb_seqnums_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			 int *num_domains, struct winbindd_domain ***domains,
			 NTSTATUS **stati, uint32_t **seqnums)
{
	struct wb_seqnums_state *state = tevent_req_data(
		req, struct wb_seqnums_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*num_domains = state->num_domains;
	*domains = talloc_move(mem_ctx, &state->domains);
	*stati = talloc_move(mem_ctx, &state->stati);
	*seqnums = talloc_move(mem_ctx, &state->seqnums);
	return NT_STATUS_OK;
}
