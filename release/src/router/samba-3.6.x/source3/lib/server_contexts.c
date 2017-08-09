/*
   Unix SMB/CIFS implementation.
   Common server globals

   Copyright (C) Simo Sorce <idra@samba.org> 2010

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
#include "messages.h"

struct tevent_context *server_event_ctx = NULL;

struct tevent_context *server_event_context(void)
{
	if (!server_event_ctx) {
		/*
		 * Note we MUST use the NULL context here, not the
		 * autofree context, to avoid side effects in forked
		 * children exiting.
		 */
		server_event_ctx = s3_tevent_context_init(NULL);
	}
	if (!server_event_ctx) {
		smb_panic("Could not init server's event context");
	}
	return server_event_ctx;
}

void server_event_context_free(void)
{
	TALLOC_FREE(server_event_ctx);
}

struct messaging_context *server_msg_ctx = NULL;

struct messaging_context *server_messaging_context(void)
{
	if (server_msg_ctx == NULL) {
		/*
		 * Note we MUST use the NULL context here, not the
		 * autofree context, to avoid side effects in forked
		 * children exiting.
		 */
		server_msg_ctx = messaging_init(NULL,
					        procid_self(),
					        server_event_context());
	}
	return server_msg_ctx;
}

void server_messaging_context_free(void)
{
	TALLOC_FREE(server_msg_ctx);
}
