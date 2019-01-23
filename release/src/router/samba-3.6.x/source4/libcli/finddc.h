/*
   Unix SMB/CIFS implementation.

   a composite API for finding a DC and its name

   Copyright (C) Andrew Tridgell 2010

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

#include "lib/messaging/messaging.h"
#include "libcli/libcli.h"
#include "libcli/netlogon/netlogon.h"

struct finddcs {
	struct {
		const char *domain_name;
		const char *site_name; /* optional */
		struct dom_sid *domain_sid; /* optional */
		uint32_t minimum_dc_flags; /* DS_SERVER_* */
		const char *server_address; /* optional, bypass name
					       resolution */
	} in;
	struct {
		const char *address; /* IP address of server */
		struct netlogon_samlogon_response netlogon;
	} out;
};

#include "finddcs_proto.h"
