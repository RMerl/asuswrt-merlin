/*
   Unix SMB/CIFS implementation.
   Wrap win32 errors around tevent_req
   Copyright (C) Kai Blin 2010

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

#ifndef _TEVENT_WERROR_H
#define _TEVENT_WERROR_H

#include <stdint.h>
#include <stdbool.h>
#include "../libcli/util/werror.h"
#include <tevent.h>

bool _tevent_req_werror(struct tevent_req *req,
			WERROR werror,
			const char *location);
#define tevent_req_werror(req, werror) \
	_tevent_req_werror(req, werror, __location__)
bool tevent_req_is_werror(struct tevent_req *req, WERROR *error);
WERROR tevent_req_simple_recv_werror(struct tevent_req *req);

/*
 * Helper routine to pass the subreq_werror to the req embedded in
 * tevent_req_callback_data(subreq), which will be freed.
 */
void tevent_req_simple_finish_werror(struct tevent_req *subreq,
				     WERROR subreq_error);

#endif
