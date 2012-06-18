/* 
   Unix SMB/CIFS implementation.
 
   Generic Authentication Interface for Samba Servers

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2009
   
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

/* This code sets up GENSEC in the way that all Samba servers want
 * (becaue they have presumed access to the sam.ldb etc */

#include "includes.h"
#include "auth/auth.h"
#include "auth/gensec/gensec.h"
#include "param/param.h"

NTSTATUS samba_server_gensec_start(TALLOC_CTX *mem_ctx,
				   struct tevent_context *event_ctx,
				   struct messaging_context *msg_ctx,
				   struct loadparm_context *lp_ctx,
				   struct cli_credentials *server_credentials,
				   const char *target_service,
				   struct gensec_security **gensec_context)
{ 
	NTSTATUS nt_status;
	struct gensec_security *gensec_ctx;
	struct auth_context *auth_context;
	
	nt_status = auth_context_create(mem_ctx, 
					event_ctx, 
					msg_ctx, 
					lp_ctx,
					&auth_context);
	
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(1, ("Failed to start auth server code: %s\n", nt_errstr(nt_status)));
		return nt_status;
	}

	nt_status = gensec_server_start(mem_ctx, 
					event_ctx,
					lp_gensec_settings(mem_ctx, lp_ctx),
					auth_context,
					&gensec_ctx);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(auth_context);
		DEBUG(1, ("Failed to start GENSEC server code: %s\n", nt_errstr(nt_status)));
		return nt_status;
	}
	
	gensec_set_credentials(gensec_ctx, server_credentials);

	if (target_service) {
		gensec_set_target_service(gensec_ctx, target_service);
	}
	*gensec_context = gensec_ctx;
	return nt_status;
}
