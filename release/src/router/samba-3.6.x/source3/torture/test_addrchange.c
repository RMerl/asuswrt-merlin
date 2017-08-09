/*
   Unix SMB/CIFS implementation.
   test for the addrchange functionality
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
#include "lib/addrchange.h"
#include "proto.h"

extern int torture_numops;

bool run_addrchange(int dummy)
{
	struct addrchange_context *ctx;
	struct tevent_context *ev;
	NTSTATUS status;
	int i;

	ev = tevent_context_init(talloc_tos());
	if (ev == NULL) {
		d_fprintf(stderr, "tevent_context_init failed\n");
		return -1;
	}

	status = addrchange_context_create(talloc_tos(), &ctx);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, "addrchange_context_create failed: %s\n",
			  nt_errstr(status));
		return false;
	}

	for (i=0; i<torture_numops; i++) {
		enum addrchange_type type;
		struct sockaddr_storage addr;
		struct tevent_req *req;
		const char *typestr;
		char addrstr[INET6_ADDRSTRLEN];

		req = addrchange_send(talloc_tos(), ev, ctx);
		if (req == NULL) {
			d_fprintf(stderr, "addrchange_send failed\n");
			return -1;
		}

		if (!tevent_req_poll_ntstatus(req, ev, &status)) {
			d_fprintf(stderr, "tevent_req_poll_ntstatus failed: "
				  "%s\n", nt_errstr(status));
			TALLOC_FREE(req);
			return -1;
		}

		status = addrchange_recv(req, &type, &addr);
		TALLOC_FREE(req);
		if (!NT_STATUS_IS_OK(status)) {
			d_fprintf(stderr, "addrchange_recv failed: %s\n",
				  nt_errstr(status));
			return -1;
		}

		switch(type) {
		case ADDRCHANGE_ADD:
			typestr = "add";
			break;
		case ADDRCHANGE_DEL:
			typestr = "del";
			break;
		default:
			typestr = talloc_asprintf(talloc_tos(), "unknown %d",
						  (int)type);
			break;
		}

		printf("Got %s %s\n", typestr,
		       print_sockaddr(addrstr, sizeof(addrstr), &addr));
	}
	TALLOC_FREE(ctx);
	TALLOC_FREE(ev);
	return 0;
}
