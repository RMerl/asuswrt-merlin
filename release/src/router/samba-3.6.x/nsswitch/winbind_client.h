/*
   Unix SMB/CIFS implementation.

   winbind client common code

   Copyright (C) Tim Potter 2000
   Copyright (C) Andrew Tridgell 2000
   Copyright (C) Andrew Bartlett 2002


   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _NSSWITCH_WINBIND_CLIENT_H_
#define _NSSWITCH_WINBIND_CLIENT_H_

#include "winbind_nss_config.h"
#include "winbind_struct_protocol.h"

void winbindd_free_response(struct winbindd_response *response);
NSS_STATUS winbindd_send_request(int req_type, int need_priv,
				 struct winbindd_request *request);
NSS_STATUS winbindd_get_response(struct winbindd_response *response);
NSS_STATUS winbindd_request_response(int req_type,
			    struct winbindd_request *request,
			    struct winbindd_response *response);
NSS_STATUS winbindd_priv_request_response(int req_type,
					  struct winbindd_request *request,
					  struct winbindd_response *response);
#define winbind_env_set() \
	(strcmp(getenv(WINBINDD_DONT_ENV)?getenv(WINBINDD_DONT_ENV):"0","1") == 0)

#define winbind_off() \
	(setenv(WINBINDD_DONT_ENV, "1", 1) == 0)

#define winbind_on() \
	(setenv(WINBINDD_DONT_ENV, "0", 1) == 0)

#endif /* _NSSWITCH_WINBIND_CLIENT_H_ */
