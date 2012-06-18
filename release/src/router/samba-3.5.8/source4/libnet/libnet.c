/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Stefan Metzmacher	2004
   
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
#include "libnet/libnet.h"
#include "lib/events/events.h"
#include "param/param.h"
#include "libcli/resolve/resolve.h"

struct libnet_context *libnet_context_init(struct tevent_context *ev,
					   struct loadparm_context *lp_ctx)
{
	struct libnet_context *ctx;

	/* We require an event context here */
	if (!ev) {
		return NULL;
	}

	/* create brand new libnet context */ 
	ctx = talloc(ev, struct libnet_context);
	if (!ctx) {
		return NULL;
	}

	ctx->event_ctx = ev;
	ctx->lp_ctx = lp_ctx;

	/* name resolution methods */
	ctx->resolve_ctx = lp_resolve_context(lp_ctx);

	/* connected services' params */
	ZERO_STRUCT(ctx->samr);
	ZERO_STRUCT(ctx->lsa);	

	/* default buffer size for various operations requiring specifying a buffer */
	ctx->samr.buf_size = 128;

	return ctx;
}
