/*
   Unix SMB/CIFS implementation.
   Wrap unix errno around tevent_req
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

#ifndef _TEVENT_NTSTATUS_H
#define _TEVENT_NTSTATUS_H

#include <stdint.h>
#include <stdbool.h>
#include "../libcli/util/ntstatus.h"
#include <tevent.h>

bool _tevent_req_nterror(struct tevent_req *req,
			 NTSTATUS status,
			 const char *location);
#define tevent_req_nterror(req, status) \
	_tevent_req_nterror(req, status, __location__)
bool tevent_req_is_nterror(struct tevent_req *req, NTSTATUS *pstatus);
NTSTATUS tevent_req_simple_recv_ntstatus(struct tevent_req *req);

/*
 * Helper routine to pass the subreq_ntstatus to the req embedded in
 * tevent_req_callback_data(subreq), which will be freed.
 */
void tevent_req_simple_finish_ntstatus(struct tevent_req *subreq,
				       NTSTATUS subreq_status);

#endif
