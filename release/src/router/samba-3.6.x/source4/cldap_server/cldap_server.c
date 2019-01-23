/* 
   Unix SMB/CIFS implementation.

   CLDAP server task

   Copyright (C) Andrew Tridgell	2005
   
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
#include <talloc.h>
#include "lib/messaging/irpc.h"
#include "smbd/service_task.h"
#include "smbd/service.h"
#include "cldap_server/cldap_server.h"
#include "system/network.h"
#include "lib/socket/netif.h"
#include <ldb.h>
#include <ldb_errors.h>
#include "dsdb/samdb/samdb.h"
#include "ldb_wrap.h"
#include "auth/auth.h"
#include "param/param.h"
#include "../lib/tsocket/tsocket.h"

/*
  handle incoming cldap requests
*/
static void cldapd_request_handler(struct cldap_socket *cldap,
				   void *private_data,
				   struct cldap_incoming *in)
{
	struct cldapd_server *cldapd = talloc_get_type(private_data,
				       struct cldapd_server);
	struct ldap_SearchRequest *search;

	if (in->ldap_msg->type != LDAP_TAG_SearchRequest) {
		DEBUG(0,("Invalid CLDAP request type %d from %s\n",
			 in->ldap_msg->type,
			 tsocket_address_string(in->src, in)));
		cldap_error_reply(cldap, in->ldap_msg->messageid, in->src,
				  LDAP_OPERATIONS_ERROR, "Invalid CLDAP request");
		talloc_free(in);
		return;
	}

	search = &in->ldap_msg->r.SearchRequest;

	if (strcmp("", search->basedn) != 0) {
		DEBUG(0,("Invalid CLDAP basedn '%s' from %s\n",
			 search->basedn,
			 tsocket_address_string(in->src, in)));
		cldap_error_reply(cldap, in->ldap_msg->messageid, in->src,
				  LDAP_OPERATIONS_ERROR, "Invalid CLDAP basedn");
		talloc_free(in);
		return;
	}

	if (search->scope != LDAP_SEARCH_SCOPE_BASE) {
		DEBUG(0,("Invalid CLDAP scope %d from %s\n",
			 search->scope,
			 tsocket_address_string(in->src, in)));
		cldap_error_reply(cldap, in->ldap_msg->messageid, in->src,
				  LDAP_OPERATIONS_ERROR, "Invalid CLDAP scope");
		talloc_free(in);
		return;
	}

	if (search->num_attributes == 1 &&
	    strcasecmp(search->attributes[0], "netlogon") == 0) {
		cldapd_netlogon_request(cldap,
					cldapd,
					in,
					in->ldap_msg->messageid,
					search->tree,
					in->src);
		talloc_free(in);
		return;
	}

	cldapd_rootdse_request(cldap, cldapd, in,
			       in->ldap_msg->messageid,
			       search, in->src);
	talloc_free(in);
}


/*
  start listening on the given address
*/
static NTSTATUS cldapd_add_socket(struct cldapd_server *cldapd, struct loadparm_context *lp_ctx,
				  const char *address)
{
	struct cldap_socket *cldapsock;
	struct tsocket_address *socket_address;
	NTSTATUS status;
	int ret;

	ret = tsocket_address_inet_from_strings(cldapd,
						"ip",
						address,
						lpcfg_cldap_port(lp_ctx),
						&socket_address);
	if (ret != 0) {
		status = map_nt_error_from_unix(errno);
		DEBUG(0,("invalid address %s:%d - %s:%s\n",
			 address, lpcfg_cldap_port(lp_ctx),
			 gai_strerror(ret), nt_errstr(status)));
		return status;
	}

	/* listen for unicasts on the CLDAP port (389) */
	status = cldap_socket_init(cldapd,
				   cldapd->task->event_ctx,
				   socket_address,
				   NULL,
				   &cldapsock);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Failed to bind to %s - %s\n", 
			 tsocket_address_string(socket_address, socket_address),
			 nt_errstr(status)));
		talloc_free(socket_address);
		return status;
	}
	talloc_free(socket_address);

	cldap_set_incoming_handler(cldapsock, cldapd_request_handler, cldapd);

	return NT_STATUS_OK;
}

/*
  setup our listening sockets on the configured network interfaces
*/
static NTSTATUS cldapd_startup_interfaces(struct cldapd_server *cldapd, struct loadparm_context *lp_ctx,
					  struct interface *ifaces)
{
	int i, num_interfaces;
	TALLOC_CTX *tmp_ctx = talloc_new(cldapd);
	NTSTATUS status;

	num_interfaces = iface_count(ifaces);

	/* if we are allowing incoming packets from any address, then
	   we need to bind to the wildcard address */
	if (!lpcfg_bind_interfaces_only(lp_ctx)) {
		status = cldapd_add_socket(cldapd, lp_ctx, "0.0.0.0");
		NT_STATUS_NOT_OK_RETURN(status);
	}

	/* now we have to also listen on the specific interfaces,
	   so that replies always come from the right IP */
	for (i=0; i<num_interfaces; i++) {
		const char *address = talloc_strdup(tmp_ctx, iface_n_ip(ifaces, i));
		status = cldapd_add_socket(cldapd, lp_ctx, address);
		NT_STATUS_NOT_OK_RETURN(status);
	}

	talloc_free(tmp_ctx);

	return NT_STATUS_OK;
}

/*
  startup the cldapd task
*/
static void cldapd_task_init(struct task_server *task)
{
	struct cldapd_server *cldapd;
	NTSTATUS status;
	struct interface *ifaces;
	
	load_interfaces(task, lpcfg_interfaces(task->lp_ctx), &ifaces);

	if (iface_count(ifaces) == 0) {
		task_server_terminate(task, "cldapd: no network interfaces configured", false);
		return;
	}

	switch (lpcfg_server_role(task->lp_ctx)) {
	case ROLE_STANDALONE:
		task_server_terminate(task, "cldap_server: no CLDAP server required in standalone configuration", 
				      false);
		return;
	case ROLE_DOMAIN_MEMBER:
		task_server_terminate(task, "cldap_server: no CLDAP server required in member server configuration",
				      false);
		return;
	case ROLE_DOMAIN_CONTROLLER:
		/* Yes, we want an CLDAP server */
		break;
	}

	task_server_set_title(task, "task[cldapd]");

	cldapd = talloc(task, struct cldapd_server);
	if (cldapd == NULL) {
		task_server_terminate(task, "cldapd: out of memory", true);
		return;
	}

	cldapd->task = task;
	cldapd->samctx = samdb_connect(cldapd, task->event_ctx, task->lp_ctx, system_session(task->lp_ctx), 0);
	if (cldapd->samctx == NULL) {
		task_server_terminate(task, "cldapd failed to open samdb", true);
		return;
	}

	/* start listening on the configured network interfaces */
	status = cldapd_startup_interfaces(cldapd, task->lp_ctx, ifaces);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "cldapd failed to setup interfaces", true);
		return;
	}

	irpc_add_name(task->msg_ctx, "cldap_server");
}


/*
  register ourselves as a available server
*/
NTSTATUS server_service_cldapd_init(void)
{
	return register_server_service("cldap", cldapd_task_init);
}
