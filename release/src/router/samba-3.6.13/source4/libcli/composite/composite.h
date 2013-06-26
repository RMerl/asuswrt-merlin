/* 
   Unix SMB/CIFS implementation.

   composite request interfaces

   Copyright (C) Andrew Tridgell 2005
   
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

#ifndef __COMPOSITE_H__
#define __COMPOSITE_H__

#include "libcli/raw/interfaces.h"

struct tevent_context;

/*
  this defines the structures associated with "composite"
  requests. Composite requests are libcli requests that are internally
  implemented as multiple async calls, but can be treated as a
  single call via these composite calls. The composite calls are
  particularly designed to be used in async applications.
  you can also stack multiple level of composite call
*/

/*
  a composite call moves between the following 3 states.
*/
enum composite_state { COMPOSITE_STATE_INIT,         /* we are creating the request */
		       COMPOSITE_STATE_IN_PROGRESS,  /* the request is in the outgoing socket Q */
		       COMPOSITE_STATE_DONE,         /* the request is received by the caller finished */
		       COMPOSITE_STATE_ERROR };      /* a packet or transport level error has occurred */

/* the context of one "composite" call */
struct composite_context {
	/* the external state - will be queried by the caller */
	enum composite_state state;

	/* a private pointer for use by the composite function
	   implementation */
	void *private_data;

	/* status code when finished */
	NTSTATUS status;

	/* the event context we are using */
	struct tevent_context *event_ctx;

	/* information on what to do on completion */
	struct {
		void (*fn)(struct composite_context *);
		void *private_data;
	} async;

	bool used_wait;
};

struct smbcli_request;
struct smb2_request;
struct nbt_name_request;

struct composite_context *composite_create(TALLOC_CTX *mem_ctx, struct tevent_context *ev);
bool composite_nomem(const void *p, struct composite_context *ctx);
void composite_continue(struct composite_context *ctx,
				 struct composite_context *new_ctx,
				 void (*continuation)(struct composite_context *),
				 void *private_data);
void composite_continue_smb(struct composite_context *ctx,
				     struct smbcli_request *new_req,
				     void (*continuation)(struct smbcli_request *),
				     void *private_data);
void composite_continue_smb2(struct composite_context *ctx,
				      struct smb2_request *new_req,
				      void (*continuation)(struct smb2_request *),
				      void *private_data);
void composite_continue_nbt(struct composite_context *ctx,
				     struct nbt_name_request *new_req,
				     void (*continuation)(struct nbt_name_request *),
				     void *private_data);
bool composite_is_ok(struct composite_context *ctx);
void composite_done(struct composite_context *ctx);
void composite_error(struct composite_context *ctx, NTSTATUS status);
NTSTATUS composite_wait(struct composite_context *c);
NTSTATUS composite_wait_free(struct composite_context *c);


#endif /* __COMPOSITE_H__ */
