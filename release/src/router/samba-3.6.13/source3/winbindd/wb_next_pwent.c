/*
   Unix SMB/CIFS implementation.
   async next_pwent
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
#include "passdb/machine_sid.h"

struct wb_next_pwent_state {
	struct tevent_context *ev;
	struct getpwent_state *gstate;
	struct winbindd_pw *pw;
};

static void wb_next_pwent_fetch_done(struct tevent_req *subreq);
static void wb_next_pwent_fill_done(struct tevent_req *subreq);

static struct winbindd_domain *wb_next_find_domain(struct winbindd_domain *domain)
{
	if (domain == NULL) {
		domain = domain_list();
	} else {
		domain = domain->next;
	}

	if ((domain != NULL)
	    && sid_check_is_domain(&domain->sid)) {
		domain = domain->next;
	}

	if (domain == NULL) {
		return NULL;
	}

	return domain;
}

struct tevent_req *wb_next_pwent_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct getpwent_state *gstate,
				      struct winbindd_pw *pw)
{
	struct tevent_req *req, *subreq;
	struct wb_next_pwent_state *state;

	req = tevent_req_create(mem_ctx, &state, struct wb_next_pwent_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->gstate = gstate;
	state->pw = pw;

	if (state->gstate->next_user >= state->gstate->num_users) {
		TALLOC_FREE(state->gstate->users);

		state->gstate->domain = wb_next_find_domain(state->gstate->domain);
		if (state->gstate->domain == NULL) {
			tevent_req_nterror(req, NT_STATUS_NO_MORE_ENTRIES);
			return tevent_req_post(req, ev);
		}
		subreq = wb_query_user_list_send(state, state->ev,
						 state->gstate->domain);
		if (tevent_req_nomem(subreq, req)) {
			return tevent_req_post(req, ev);
		}
		tevent_req_set_callback(subreq, wb_next_pwent_fetch_done, req);
		return req;
	}

	subreq = wb_fill_pwent_send(
		state, state->ev,
		&state->gstate->users[state->gstate->next_user],
		state->pw);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wb_next_pwent_fill_done, req);
	return req;
}

static void wb_next_pwent_fetch_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_next_pwent_state *state = tevent_req_data(
		req, struct wb_next_pwent_state);
	NTSTATUS status;

	status = wb_query_user_list_recv(subreq, state->gstate,
					 &state->gstate->num_users,
					 &state->gstate->users);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		/* Ignore errors here, just log it */
		DEBUG(10, ("query_user_list for domain %s returned %s\n",
			   state->gstate->domain->name,
			   nt_errstr(status)));
		state->gstate->num_users = 0;
	}

	if (state->gstate->num_users == 0) {
		state->gstate->domain = state->gstate->domain->next;

		if ((state->gstate->domain != NULL)
		    && sid_check_is_domain(&state->gstate->domain->sid)) {
			state->gstate->domain = state->gstate->domain->next;
		}

		if (state->gstate->domain == NULL) {
			tevent_req_nterror(req, NT_STATUS_NO_MORE_ENTRIES);
			return;
		}
		subreq = wb_query_user_list_send(state, state->ev,
						 state->gstate->domain);
		if (tevent_req_nomem(subreq, req)) {
			return;
		}
		tevent_req_set_callback(subreq, wb_next_pwent_fetch_done, req);
		return;
	}

	state->gstate->next_user = 0;

	subreq = wb_fill_pwent_send(
		state, state->ev,
		&state->gstate->users[state->gstate->next_user],
		state->pw);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, wb_next_pwent_fill_done, req);
}

static void wb_next_pwent_fill_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_next_pwent_state *state = tevent_req_data(
		req, struct wb_next_pwent_state);
	NTSTATUS status;

	status = wb_fill_pwent_recv(subreq);
	TALLOC_FREE(subreq);
	/*
	 * When you try to enumerate users with 'getent passwd' and the user
	 * doesn't have a uid set we should just move on.
	 */
	if (NT_STATUS_EQUAL(status, NT_STATUS_NONE_MAPPED)) {
		state->gstate->next_user += 1;

		if (state->gstate->next_user >= state->gstate->num_users) {
			TALLOC_FREE(state->gstate->users);

			state->gstate->domain = wb_next_find_domain(state->gstate->domain);
			if (state->gstate->domain == NULL) {
				tevent_req_nterror(req, NT_STATUS_NO_MORE_ENTRIES);
				return;
			}

			subreq = wb_query_user_list_send(state, state->ev,
					state->gstate->domain);
			if (tevent_req_nomem(subreq, req)) {
				return;
			}
			tevent_req_set_callback(subreq, wb_next_pwent_fetch_done, req);
			return;
		}

		subreq = wb_fill_pwent_send(state,
					    state->ev,
					    &state->gstate->users[state->gstate->next_user],
					    state->pw);
		if (tevent_req_nomem(subreq, req)) {
			return;
		}
		tevent_req_set_callback(subreq, wb_next_pwent_fill_done, req);

		return;
	} else if (tevent_req_nterror(req, status)) {
		return;
	}
	state->gstate->next_user += 1;
	tevent_req_done(req);
}

NTSTATUS wb_next_pwent_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}
