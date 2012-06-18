/*
   Unix SMB/CIFS implementation.

   Winbind client library.

   Copyright (C) 2008 Kai Blin  <kai@samba.org>

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
#include "lib/messaging/irpc.h"
#include "libcli/composite/composite.h"
#include "librpc/gen_ndr/ndr_winbind.h"

struct wbc_context {
	struct messaging_context *msg_ctx;
	struct tevent_context *event_ctx;
	struct server_id *ids;
};

struct wbc_context *wbc_init(TALLOC_CTX *mem_ctx,
			     struct messaging_context *msg_ctx,
			     struct tevent_context *event_ctx);

struct composite_context *wbc_sids_to_xids_send(struct wbc_context *wbc_ctx,
						TALLOC_CTX *mem_ctx,
						uint32_t count,
						struct id_mapping *ids);

NTSTATUS wbc_sids_to_xids_recv(struct composite_context *ctx,
			       struct id_mapping **ids);

struct composite_context *wbc_xids_to_sids_send(struct wbc_context *wbc_ctx,
						TALLOC_CTX *mem_ctx,
						uint32_t count,
						struct id_mapping *ids);

NTSTATUS wbc_xids_to_sids_recv(struct composite_context *ctx,
			       struct id_mapping **ids);

