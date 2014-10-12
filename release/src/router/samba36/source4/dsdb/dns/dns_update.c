/*
   Unix SMB/CIFS mplementation.

   DNS update service

   Copyright (C) Andrew Tridgell 2009

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

/*
  this module auto-creates the named.conf.update file, which tells
  bind9 what KRB5 principals it should accept for updates to our zone

  It also uses the samba_dnsupdate script to auto-create the right DNS
  names for ourselves as a DC in the domain, using TSIG-GSS
 */

#include "includes.h"
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "smbd/service.h"
#include "lib/messaging/irpc.h"
#include "param/param.h"
#include "system/filesys.h"
#include "libcli/composite/composite.h"
#include "libcli/security/dom_sid.h"
#include "librpc/gen_ndr/ndr_irpc.h"

struct dnsupdate_service {
	struct task_server *task;
	struct auth_session_info *system_session_info;
	struct ldb_context *samdb;

	/* status for periodic config file update */
	struct {
		uint32_t interval;
		struct tevent_timer *te;
		struct tevent_req *subreq;
		NTSTATUS status;
	} confupdate;

	/* status for periodic DNS name check */
	struct {
		uint32_t interval;
		struct tevent_timer *te;
		struct tevent_req *subreq;
		struct tevent_req *spnreq;
		NTSTATUS status;
	} nameupdate;
};

/*
  called when rndc reload has finished
 */
static void dnsupdate_rndc_done(struct tevent_req *subreq)
{
	struct dnsupdate_service *service = tevent_req_callback_data(subreq,
					    struct dnsupdate_service);
	int ret;
	int sys_errno;

	service->confupdate.subreq = NULL;

	ret = samba_runcmd_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret != 0) {
		service->confupdate.status = map_nt_error_from_unix(sys_errno);
	} else {
		service->confupdate.status = NT_STATUS_OK;
	}

	if (!NT_STATUS_IS_OK(service->confupdate.status)) {
		DEBUG(0,(__location__ ": Failed rndc update - %s\n",
			 nt_errstr(service->confupdate.status)));
	} else {
		DEBUG(3,("Completed rndc reload OK\n"));
	}
}

/*
  called every 'dnsupdate:conf interval' seconds
 */
static void dnsupdate_rebuild(struct dnsupdate_service *service)
{
	int ret;
	size_t size;
	struct ldb_result *res;
	const char *tmp_path, *path, *path_static;
	char *static_policies;
	int fd;
	unsigned int i;
	const char *attrs[] = { "sAMAccountName", NULL };
	const char *realm = lpcfg_realm(service->task->lp_ctx);
	TALLOC_CTX *tmp_ctx = talloc_new(service);
	const char * const *rndc_command = lpcfg_rndc_command(service->task->lp_ctx);

	/* abort any pending script run */
	TALLOC_FREE(service->confupdate.subreq);

	ret = ldb_search(service->samdb, tmp_ctx, &res, NULL, LDB_SCOPE_SUBTREE,
			 attrs, "(|(samaccountname=administrator)(&(primaryGroupID=%u)(objectClass=computer)))",
			 DOMAIN_RID_DCS);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Unable to find DCs list - %s", ldb_errstring(service->samdb)));
		talloc_free(tmp_ctx);
		return;
	}

	path = lpcfg_parm_string(service->task->lp_ctx, NULL, "dnsupdate", "path");
	if (path == NULL) {
		path = private_path(tmp_ctx, service->task->lp_ctx, "named.conf.update");
	}

	path_static = lpcfg_parm_string(service->task->lp_ctx, NULL, "dnsupdate", "extra_static_grant_rules");
	if (path_static == NULL) {
		path_static = private_path(tmp_ctx, service->task->lp_ctx, "named.conf.update.static");
	}

	tmp_path = talloc_asprintf(tmp_ctx, "%s.tmp", path);
	if (path == NULL || tmp_path == NULL || path_static == NULL ) {
		DEBUG(0,(__location__ ": Unable to get paths\n"));
		talloc_free(tmp_ctx);
		return;
	}

	static_policies = file_load(path_static, &size, 0, tmp_ctx);

	unlink(tmp_path);
	fd = open(tmp_path, O_CREAT|O_TRUNC|O_WRONLY, 0444);
	if (fd == -1) {
		DEBUG(1,(__location__ ": Unable to open %s - %s\n", tmp_path, strerror(errno)));
		talloc_free(tmp_ctx);
		return;
	}

	dprintf(fd, "/* this file is auto-generated - do not edit */\n");
	dprintf(fd, "update-policy {\n");
	if( static_policies != NULL ) {
		dprintf(fd, "/* Start of static entries */\n");
		dprintf(fd, "%s\n",static_policies);
		dprintf(fd, "/* End of static entries */\n");
	}
	dprintf(fd, "\tgrant %s ms-self * A AAAA;\n", realm);

	for (i=0; i<res->count; i++) {
		const char *acctname;
		acctname = ldb_msg_find_attr_as_string(res->msgs[i],
						       "sAMAccountName", NULL);
		if (!acctname) continue;
		dprintf(fd, "\tgrant %s@%s wildcard * A AAAA SRV CNAME;\n",
			acctname, realm);
	}
	dprintf(fd, "};\n");
	close(fd);


	if (NT_STATUS_IS_OK(service->confupdate.status) &&
	    file_compare(tmp_path, path) == true) {
		unlink(tmp_path);
		talloc_free(tmp_ctx);
		return;
	}

	if (rename(tmp_path, path) != 0) {
		DEBUG(0,(__location__ ": Failed to rename %s to %s - %s\n",
			 tmp_path, path, strerror(errno)));
		talloc_free(tmp_ctx);
		return;
	}

	DEBUG(2,("Loading new DNS update grant rules\n"));
	service->confupdate.subreq = samba_runcmd_send(service,
						       service->task->event_ctx,
						       timeval_current_ofs(10, 0),
						       2, 0,
						       rndc_command,
						       "reload", NULL);
	if (service->confupdate.subreq == NULL) {
		DEBUG(0,(__location__ ": samba_runcmd_send() failed with no memory\n"));
		talloc_free(tmp_ctx);
		return;
	}
	tevent_req_set_callback(service->confupdate.subreq,
				dnsupdate_rndc_done,
				service);

	talloc_free(tmp_ctx);
}

static NTSTATUS dnsupdate_confupdate_schedule(struct dnsupdate_service *service);

/*
  called every 'dnsupdate:conf interval' seconds
 */
static void dnsupdate_confupdate_handler_te(struct tevent_context *ev, struct tevent_timer *te,
					  struct timeval t, void *ptr)
{
	struct dnsupdate_service *service = talloc_get_type(ptr, struct dnsupdate_service);

	dnsupdate_rebuild(service);
	dnsupdate_confupdate_schedule(service);
}


static NTSTATUS dnsupdate_confupdate_schedule(struct dnsupdate_service *service)
{
	service->confupdate.te = tevent_add_timer(service->task->event_ctx, service,
						timeval_current_ofs(service->confupdate.interval, 0),
						dnsupdate_confupdate_handler_te, service);
	NT_STATUS_HAVE_NO_MEMORY(service->confupdate.te);
	return NT_STATUS_OK;
}


/*
  called when dns update script has finished
 */
static void dnsupdate_nameupdate_done(struct tevent_req *subreq)
{
	struct dnsupdate_service *service = tevent_req_callback_data(subreq,
					    struct dnsupdate_service);
	int ret;
	int sys_errno;

	service->nameupdate.subreq = NULL;

	ret = samba_runcmd_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret != 0) {
		service->nameupdate.status = map_nt_error_from_unix(sys_errno);
	} else {
		service->nameupdate.status = NT_STATUS_OK;
	}

	if (!NT_STATUS_IS_OK(service->nameupdate.status)) {
		DEBUG(0,(__location__ ": Failed DNS update - %s\n",
			 nt_errstr(service->nameupdate.status)));
	} else {
		DEBUG(3,("Completed DNS update check OK\n"));
	}
}


/*
  called when spn update script has finished
 */
static void dnsupdate_spnupdate_done(struct tevent_req *subreq)
{
	struct dnsupdate_service *service = tevent_req_callback_data(subreq,
					    struct dnsupdate_service);
	int ret;
	int sys_errno;

	service->nameupdate.spnreq = NULL;

	ret = samba_runcmd_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret != 0) {
		service->nameupdate.status = map_nt_error_from_unix(sys_errno);
	} else {
		service->nameupdate.status = NT_STATUS_OK;
	}

	if (!NT_STATUS_IS_OK(service->nameupdate.status)) {
		DEBUG(0,(__location__ ": Failed SPN update - %s\n",
			 nt_errstr(service->nameupdate.status)));
	} else {
		DEBUG(3,("Completed SPN update check OK\n"));
	}
}

/*
  called every 'dnsupdate:name interval' seconds
 */
static void dnsupdate_check_names(struct dnsupdate_service *service)
{
	const char * const *dns_update_command = lpcfg_dns_update_command(service->task->lp_ctx);
	const char * const *spn_update_command = lpcfg_spn_update_command(service->task->lp_ctx);

	/* kill any existing child */
	TALLOC_FREE(service->nameupdate.subreq);

	DEBUG(3,("Calling DNS name update script\n"));
	service->nameupdate.subreq = samba_runcmd_send(service,
						       service->task->event_ctx,
						       timeval_current_ofs(20, 0),
						       2, 0,
						       dns_update_command,
						       NULL);
	if (service->nameupdate.subreq == NULL) {
		DEBUG(0,(__location__ ": samba_runcmd_send() failed with no memory\n"));
		return;
	}
	tevent_req_set_callback(service->nameupdate.subreq,
				dnsupdate_nameupdate_done,
				service);

	DEBUG(3,("Calling SPN name update script\n"));
	service->nameupdate.spnreq = samba_runcmd_send(service,
						       service->task->event_ctx,
						       timeval_current_ofs(20, 0),
						       2, 0,
						       spn_update_command,
						       NULL);
	if (service->nameupdate.spnreq == NULL) {
		DEBUG(0,(__location__ ": samba_runcmd_send() failed with no memory\n"));
		return;
	}
	tevent_req_set_callback(service->nameupdate.spnreq,
				dnsupdate_spnupdate_done,
				service);
}

static NTSTATUS dnsupdate_nameupdate_schedule(struct dnsupdate_service *service);

/*
  called every 'dnsupdate:name interval' seconds
 */
static void dnsupdate_nameupdate_handler_te(struct tevent_context *ev, struct tevent_timer *te,
					    struct timeval t, void *ptr)
{
	struct dnsupdate_service *service = talloc_get_type(ptr, struct dnsupdate_service);

	dnsupdate_check_names(service);
	dnsupdate_nameupdate_schedule(service);
}


static NTSTATUS dnsupdate_nameupdate_schedule(struct dnsupdate_service *service)
{
	service->nameupdate.te = tevent_add_timer(service->task->event_ctx, service,
						  timeval_current_ofs(service->nameupdate.interval, 0),
						  dnsupdate_nameupdate_handler_te, service);
	NT_STATUS_HAVE_NO_MEMORY(service->nameupdate.te);
	return NT_STATUS_OK;
}


struct dnsupdate_RODC_state {
	struct irpc_message *msg;
	struct dnsupdate_RODC *r;
	char *tmp_path;
	int fd;
};

static int dnsupdate_RODC_destructor(struct dnsupdate_RODC_state *st)
{
	if (st->fd != -1) {
		close(st->fd);
	}
	unlink(st->tmp_path);
	return 0;
}

/*
  called when the DNS update has completed
 */
static void dnsupdate_RODC_callback(struct tevent_req *req)
{
	struct dnsupdate_RODC_state *st =
		tevent_req_callback_data(req,
					 struct dnsupdate_RODC_state);
	int sys_errno;
	int i, ret;

	ret = samba_runcmd_recv(req, &sys_errno);
	talloc_free(req);
	if (ret != 0) {
		st->r->out.result = map_nt_error_from_unix(sys_errno);
		DEBUG(2,(__location__ ": RODC DNS Update failed: %s\n", nt_errstr(st->r->out.result)));
	} else {
		st->r->out.result = NT_STATUS_OK;
		DEBUG(3,(__location__ ": RODC DNS Update OK\n"));
	}

	for (i=0; i<st->r->in.dns_names->count; i++) {
		st->r->out.dns_names->names[i].status = NT_STATUS_V(st->r->out.result);
	}

	irpc_send_reply(st->msg, NT_STATUS_OK);
}


/**
 * Called when we get a RODC DNS update request from the netlogon
 * rpc server
 */
static NTSTATUS dnsupdate_dnsupdate_RODC(struct irpc_message *msg,
					 struct dnsupdate_RODC *r)
{
	struct dnsupdate_service *s = talloc_get_type(msg->private_data,
						      struct dnsupdate_service);
	const char * const *dns_update_command = lpcfg_dns_update_command(s->task->lp_ctx);
	struct dnsupdate_RODC_state *st;
	struct tevent_req *req;
	int i, ret;
	struct GUID ntds_guid;
	const char *site, *dnsdomain, *dnsforest, *ntdsguid, *hostname;
	struct ldb_dn *sid_dn;
	const char *attrs[] = { "dNSHostName", NULL };
	struct ldb_result *res;

	st = talloc_zero(msg, struct dnsupdate_RODC_state);
	if (!st) {
		r->out.result = NT_STATUS_NO_MEMORY;
		return NT_STATUS_OK;
	}

	st->r = r;
	st->msg = msg;

	st->tmp_path = smbd_tmp_path(st, s->task->lp_ctx, "rodcdns.XXXXXX");
	if (!st->tmp_path) {
		talloc_free(st);
		r->out.result = NT_STATUS_NO_MEMORY;
		return NT_STATUS_OK;
	}

	st->fd = mkstemp(st->tmp_path);
	if (st->fd == -1) {
		DEBUG(0,("Unable to create a temporary file for RODC dnsupdate\n"));
		talloc_free(st);
		r->out.result = NT_STATUS_INTERNAL_DB_CORRUPTION;
		return NT_STATUS_OK;
	}

	talloc_set_destructor(st, dnsupdate_RODC_destructor);

	sid_dn = ldb_dn_new_fmt(st, s->samdb, "<SID=%s>", dom_sid_string(st, r->in.dom_sid));
	if (!sid_dn) {
		talloc_free(st);
		r->out.result = NT_STATUS_NO_MEMORY;
		return NT_STATUS_OK;
	}

	/* work out the site */
	ret = samdb_find_site_for_computer(s->samdb, st, sid_dn, &site);
	if (ret != LDB_SUCCESS) {
		DEBUG(2, (__location__ ": Unable to find site for computer %s\n",
			  ldb_dn_get_linearized(sid_dn)));
		talloc_free(st);
		r->out.result = NT_STATUS_NO_SUCH_USER;
		return NT_STATUS_OK;
	}

	/* work out the ntdsguid */
	ret = samdb_find_ntdsguid_for_computer(s->samdb, sid_dn, &ntds_guid);
	ntdsguid = GUID_string(st, &ntds_guid);
	if (ret != LDB_SUCCESS || !ntdsguid) {
		DEBUG(2, (__location__ ": Unable to find NTDS GUID for computer %s\n",
			  ldb_dn_get_linearized(sid_dn)));
		talloc_free(st);
		r->out.result = NT_STATUS_NO_SUCH_USER;
		return NT_STATUS_OK;
	}


	/* find dnsdomain and dnsforest */
	dnsdomain = lpcfg_realm(s->task->lp_ctx);
	dnsforest = dnsdomain;

	/* find the hostname */
	ret = dsdb_search_dn(s->samdb, st, &res, sid_dn, attrs, 0);
	if (ret == LDB_SUCCESS) {
		hostname = ldb_msg_find_attr_as_string(res->msgs[0], "dNSHostName", NULL);
	}
	if (ret != LDB_SUCCESS || !hostname) {
		DEBUG(2, (__location__ ": Unable to find NTDS GUID for computer %s\n",
			  ldb_dn_get_linearized(sid_dn)));
		talloc_free(st);
		r->out.result = NT_STATUS_NO_SUCH_USER;
		return NT_STATUS_OK;
	}


	for (i=0; i<st->r->in.dns_names->count; i++) {
		struct NL_DNS_NAME_INFO *n = &r->in.dns_names->names[i];
		switch (n->type) {
		case NlDnsLdapAtSite:
			dprintf(st->fd, "SRV _ldap._tcp.%s._sites.%s. %s %u\n",
				site, dnsdomain, hostname, n->port);
			break;
		case NlDnsGcAtSite:
			dprintf(st->fd, "SRV _ldap._tcp.%s._sites.gc._msdcs.%s. %s %u\n",
				site, dnsdomain, hostname, n->port);
			break;
		case NlDnsDsaCname:
			dprintf(st->fd, "CNAME %s._msdcs.%s. %s\n",
				ntdsguid, dnsforest, hostname);
			break;
		case NlDnsKdcAtSite:
			dprintf(st->fd, "SRV _kerberos._tcp.%s._sites.dc._msdcs.%s. %s %u\n",
				site, dnsdomain, hostname, n->port);
			break;
		case NlDnsDcAtSite:
			dprintf(st->fd, "SRV _ldap._tcp.%s._sites.dc._msdcs.%s. %s %u\n",
				site, dnsdomain, hostname, n->port);
			break;
		case NlDnsRfc1510KdcAtSite:
			dprintf(st->fd, "SRV _kerberos._tcp.%s._sites.%s. %s %u\n",
				site, dnsdomain, hostname, n->port);
			break;
		case NlDnsGenericGcAtSite:
			dprintf(st->fd, "SRV _gc._tcp.%s._sites.%s. %s %u\n",
				site, dnsforest, hostname, n->port);
			break;
		}
	}

	close(st->fd);
	st->fd = -1;

	DEBUG(3,("Calling RODC DNS name update script %s\n", st->tmp_path));
	req = samba_runcmd_send(st,
				s->task->event_ctx,
				timeval_current_ofs(20, 0),
				2, 0,
				dns_update_command,
				"--update-list",
				st->tmp_path,
				NULL);
	NT_STATUS_HAVE_NO_MEMORY(req);

	/* setup the callback */
	tevent_req_set_callback(req, dnsupdate_RODC_callback, st);

	msg->defer_reply = true;

	return NT_STATUS_OK;
}

/*
  startup the dns update task
*/
static void dnsupdate_task_init(struct task_server *task)
{
	NTSTATUS status;
	struct dnsupdate_service *service;

	if (lpcfg_server_role(task->lp_ctx) != ROLE_DOMAIN_CONTROLLER) {
		/* not useful for non-DC */
		return;
	}

	task_server_set_title(task, "task[dnsupdate]");

	service = talloc_zero(task, struct dnsupdate_service);
	if (!service) {
		task_server_terminate(task, "dnsupdate_task_init: out of memory", true);
		return;
	}
	service->task		= task;
	task->private_data	= service;

	service->system_session_info = system_session(service->task->lp_ctx);
	if (!service->system_session_info) {
		task_server_terminate(task,
				      "dnsupdate: Failed to obtain server credentials\n",
				      true);
		return;
	}

	service->samdb = samdb_connect(service, service->task->event_ctx, task->lp_ctx,
				       service->system_session_info, 0);
	if (!service->samdb) {
		task_server_terminate(task, "dnsupdate: Failed to connect to local samdb\n",
				      true);
		return;
	}

	service->confupdate.interval	= lpcfg_parm_int(task->lp_ctx, NULL,
						      "dnsupdate", "config interval", 60); /* in seconds */

	service->nameupdate.interval	= lpcfg_parm_int(task->lp_ctx, NULL,
						      "dnsupdate", "name interval", 600); /* in seconds */

	dnsupdate_rebuild(service);
	status = dnsupdate_confupdate_schedule(service);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, talloc_asprintf(task,
				      "dnsupdate: Failed to confupdate schedule: %s\n",
							    nt_errstr(status)), true);
		return;
	}

	dnsupdate_check_names(service);
	status = dnsupdate_nameupdate_schedule(service);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, talloc_asprintf(task,
				      "dnsupdate: Failed to nameupdate schedule: %s\n",
							    nt_errstr(status)), true);
		return;
	}

	irpc_add_name(task->msg_ctx, "dnsupdate");

	IRPC_REGISTER(task->msg_ctx, irpc, DNSUPDATE_RODC,
		      dnsupdate_dnsupdate_RODC, service);

	/* create the intial file */
	dnsupdate_rebuild(service);

}

/*
  register ourselves as a available server
*/
NTSTATUS server_service_dnsupdate_init(void)
{
	return register_server_service("dnsupdate", dnsupdate_task_init);
}
