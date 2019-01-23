/* 
   Unix SMB/CIFS implementation.

   SERVER SERVICE code

   Copyright (C) Andrew Tridgell 2003-2005
   Copyright (C) Stefan (metze) Metzmacher	2004
   
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
#include "smbd/process_model.h"

/*
  a linked list of registered servers
*/
static struct registered_server {
	struct registered_server *next, *prev;
	const char *service_name;
	void (*task_init)(struct task_server *);
} *registered_servers;

/*
  register a server service. 
*/
NTSTATUS register_server_service(const char *name,
				 void (*task_init)(struct task_server *))
{
	struct registered_server *srv;
	srv = talloc(talloc_autofree_context(), struct registered_server);
	NT_STATUS_HAVE_NO_MEMORY(srv);
	srv->service_name = name;
	srv->task_init = task_init;
	DLIST_ADD_END(registered_servers, srv, struct registered_server *);
	return NT_STATUS_OK;
}


/*
  initialise a server service
*/
static NTSTATUS server_service_init(const char *name,
				    struct tevent_context *event_context,
				    struct loadparm_context *lp_ctx,
				    const struct model_ops *model_ops)
{
	struct registered_server *srv;
	for (srv=registered_servers; srv; srv=srv->next) {
		if (strcasecmp(name, srv->service_name) == 0) {
			return task_server_startup(event_context, lp_ctx, srv->service_name,
						   model_ops, srv->task_init);
		}
	}
	return NT_STATUS_INVALID_SYSTEM_SERVICE;
}


/*
  startup all of our server services
*/
NTSTATUS server_service_startup(struct tevent_context *event_ctx, 
				struct loadparm_context *lp_ctx,
				const char *model, const char **server_services)
{
	int i;
	const struct model_ops *model_ops;

	if (!server_services) {
		DEBUG(0,("server_service_startup: no endpoint servers configured\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	model_ops = process_model_startup(model);
	if (!model_ops) {
		DEBUG(0,("process_model_startup('%s') failed\n", model));
		return NT_STATUS_INTERNAL_ERROR;
	}

	for (i=0;server_services[i];i++) {
		NTSTATUS status;

		status = server_service_init(server_services[i], event_ctx, lp_ctx, model_ops);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("Failed to start service '%s' - %s\n", 
				 server_services[i], nt_errstr(status)));
		}
		NT_STATUS_NOT_OK_RETURN(status);
	}

	return NT_STATUS_OK;
}
