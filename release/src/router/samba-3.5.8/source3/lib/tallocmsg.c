/* 
   samba -- Unix SMB/CIFS implementation.
   Copyright (C) 2001, 2002 by Martin Pool
   
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

/**
 * @file tallocmsg.c
 *
 * Glue code between talloc profiling and the Samba messaging system.
 **/

struct msg_pool_usage_state {
	TALLOC_CTX *mem_ctx;
	ssize_t len;
	size_t buflen;
	char *s;
};

static void msg_pool_usage_helper(const void *ptr, int depth, int max_depth, int is_ref, void *_s)
{
	const char *name = talloc_get_name(ptr);
	struct msg_pool_usage_state *state = (struct msg_pool_usage_state *)_s;

	if (is_ref) {
		sprintf_append(state->mem_ctx, &state->s, &state->len, &state->buflen,
			       "%*sreference to: %s\n", depth*4, "", name);
		return;
	}

	if (depth == 0) {
		sprintf_append(state->mem_ctx, &state->s, &state->len, &state->buflen,
			       "%stalloc report on '%s' (total %6lu bytes in %3lu blocks)\n", 
			       (max_depth < 0 ? "full " :""), name,
			       (unsigned long)talloc_total_size(ptr),
			       (unsigned long)talloc_total_blocks(ptr));
		return;
	}

	sprintf_append(state->mem_ctx, &state->s, &state->len, &state->buflen,
		       "%*s%-30s contains %6lu bytes in %3lu blocks (ref %d)\n", 
		       depth*4, "",
		       name,
		       (unsigned long)talloc_total_size(ptr),
		       (unsigned long)talloc_total_blocks(ptr),
		       talloc_reference_count(ptr));
}

/**
 * Respond to a POOL_USAGE message by sending back string form of memory
 * usage stats.
 **/
static void msg_pool_usage(struct messaging_context *msg_ctx,
			   void *private_data, 
			   uint32_t msg_type, 
			   struct server_id src,
			   DATA_BLOB *data)
{
	struct msg_pool_usage_state state;

	SMB_ASSERT(msg_type == MSG_REQ_POOL_USAGE);
	
	DEBUG(2,("Got POOL_USAGE\n"));

	state.mem_ctx = talloc_init("msg_pool_usage");
	if (!state.mem_ctx) {
		return;
	}
	state.len	= 0;
	state.buflen	= 512;
	state.s		= NULL;

	talloc_report_depth_cb(NULL, 0, -1, msg_pool_usage_helper, &state);

	if (!state.s) {
		talloc_destroy(state.mem_ctx);
		return;
	}
	
	messaging_send_buf(msg_ctx, src, MSG_POOL_USAGE,
			   (uint8 *)state.s, strlen(state.s)+1);

	talloc_destroy(state.mem_ctx);
}

/**
 * Register handler for MSG_REQ_POOL_USAGE
 **/
void register_msg_pool_usage(struct messaging_context *msg_ctx)
{
	messaging_register(msg_ctx, NULL, MSG_REQ_POOL_USAGE, msg_pool_usage);
	DEBUG(2, ("Registered MSG_REQ_POOL_USAGE\n"));
}	
