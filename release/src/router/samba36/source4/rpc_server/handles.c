/* 
   Unix SMB/CIFS implementation.

   server side dcerpc handle code

   Copyright (C) Andrew Tridgell 2003
   
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
#include "../lib/util/dlinklist.h"
#include "rpc_server/dcerpc_server.h"
#include "libcli/security/security.h"
#include "auth/session.h"

/*
  destroy a rpc handle
*/
static int dcesrv_handle_destructor(struct dcesrv_handle *h)
{
	DLIST_REMOVE(h->assoc_group->handles, h);
	return 0;
}


/*
  allocate a new rpc handle
*/
_PUBLIC_ struct dcesrv_handle *dcesrv_handle_new(struct dcesrv_connection_context *context, 
						 uint8_t handle_type)
{
	struct dcesrv_handle *h;
	struct dom_sid *sid;

	sid = &context->conn->auth_state.session_info->security_token->sids[PRIMARY_USER_SID_INDEX];

	h = talloc(context->assoc_group, struct dcesrv_handle);
	if (!h) {
		return NULL;
	}
	h->data = NULL;
	h->sid = dom_sid_dup(h, sid);
	if (h->sid == NULL) {
		talloc_free(h);
		return NULL;
	}
	h->assoc_group = context->assoc_group;
	h->iface = context->iface;
	h->wire_handle.handle_type = handle_type;
	h->wire_handle.uuid = GUID_random();
	
	DLIST_ADD(context->assoc_group->handles, h);

	talloc_set_destructor(h, dcesrv_handle_destructor);

	return h;
}

/**
  find an internal handle given a wire handle. If the wire handle is NULL then
  allocate a new handle
*/
_PUBLIC_ struct dcesrv_handle *dcesrv_handle_fetch(
					  struct dcesrv_connection_context *context, 
					  struct policy_handle *p,
					  uint8_t handle_type)
{
	struct dcesrv_handle *h;
	struct dom_sid *sid;

	sid = &context->conn->auth_state.session_info->security_token->sids[PRIMARY_USER_SID_INDEX];

	if (policy_handle_empty(p)) {
		/* TODO: we should probably return a NULL handle here */
		return dcesrv_handle_new(context, handle_type);
	}

	for (h=context->assoc_group->handles; h; h=h->next) {
		if (h->wire_handle.handle_type == p->handle_type &&
		    GUID_equal(&p->uuid, &h->wire_handle.uuid)) {
			if (handle_type != DCESRV_HANDLE_ANY &&
			    p->handle_type != handle_type) {
				DEBUG(0,("client gave us the wrong handle type (%d should be %d)\n",
					 p->handle_type, handle_type));
				return NULL;
			}
			if (!dom_sid_equal(h->sid, sid)) {
				DEBUG(0,(__location__ ": Attempt to use invalid sid %s - %s\n",
					 dom_sid_string(context, h->sid),
					 dom_sid_string(context, sid)));
				return NULL;
			}
			if (h->iface != context->iface) {
				DEBUG(0,(__location__ ": Attempt to use invalid iface\n"));
				return NULL;
			}
			return h;
		}
	}

	return NULL;
}
