/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_ENDPWENT
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

struct winbindd_endpwent_state {
	uint8_t dummy;
};

struct tevent_req *winbindd_endpwent_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct winbindd_cli_state *cli,
					  struct winbindd_request *request)
{
	struct tevent_req *req;
	struct winbindd_endpwent_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_endpwent_state);
	if (req == NULL) {
		return NULL;
	}
	TALLOC_FREE(cli->pwent_state);
	tevent_req_done(req);
	return tevent_req_post(req, ev);
}

NTSTATUS winbindd_endpwent_recv(struct tevent_req *req,
				struct winbindd_response *presp)
{
	return NT_STATUS_OK;
}
