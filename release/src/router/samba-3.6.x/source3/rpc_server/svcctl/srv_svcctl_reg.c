/*
 *  Unix SMB/CIFS implementation.
 *
 *  SVCCTL RPC server keys initialization
 *
 *  Copyright (c) 2005      Marcin Krzysztof Porwit
 *  Copyright (c) 2005      Gerald (Jerry) Carter
 *  Copyright (c) 2011      Andreas Schneider <asn@samba.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "system/filesys.h"
#include "services/services.h"
#include "services/svc_winreg_glue.h"
#include "../librpc/gen_ndr/ndr_winreg_c.h"
#include "rpc_client/cli_winreg_int.h"
#include "rpc_client/cli_winreg.h"
#include "rpc_server/svcctl/srv_svcctl_reg.h"
#include "auth.h"
#include "registry/reg_backend_db.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_REGISTRY

#define TOP_LEVEL_SERVICES_KEY "SYSTEM\\CurrentControlSet\\Services"

struct rcinit_file_information {
	char *description;
};

struct service_display_info {
	const char *servicename;
	const char *daemon;
	const char *dispname;
	const char *description;
};

static struct service_display_info builtin_svcs[] = {
	{
		"Spooler",
		"smbd",
		"Print Spooler",
		"Internal service for spooling files to print devices"
	},
	{
		"NETLOGON",
		"smbd",
		"Net Logon",
		"File service providing access to policy and profile data (not"
			"remotely manageable)"
	},
	{
		"RemoteRegistry",
		"smbd",
		"Remote Registry Service",
		"Internal service providing remote access to the Samba registry"
	},
	{
		"WINS",
		"nmbd",
		"Windows Internet Name Service (WINS)",
		"Internal service providing a NetBIOS point-to-point name server"
			"(not remotely manageable)"
	},
	{ NULL, NULL, NULL, NULL }
};

static struct service_display_info common_unix_svcs[] = {
  { "cups",          NULL, "Common Unix Printing System","Provides unified printing support for all operating systems" },
  { "postfix",       NULL, "Internet Mail Service", 	"Provides support for sending and receiving electonic mail" },
  { "sendmail",      NULL, "Internet Mail Service", 	"Provides support for sending and receiving electonic mail" },
  { "portmap",       NULL, "TCP Port to RPC PortMapper",NULL },
  { "xinetd",        NULL, "Internet Meta-Daemon", 	NULL },
  { "inet",          NULL, "Internet Meta-Daemon", 	NULL },
  { "xntpd",         NULL, "Network Time Service", 	NULL },
  { "ntpd",          NULL, "Network Time Service", 	NULL },
  { "lpd",           NULL, "BSD Print Spooler", 	NULL },
  { "nfsserver",     NULL, "Network File Service", 	NULL },
  { "cron",          NULL, "Scheduling Service", 	NULL },
  { "at",            NULL, "Scheduling Service", 	NULL },
  { "nscd",          NULL, "Name Service Cache Daemon",	NULL },
  { "slapd",         NULL, "LDAP Directory Service", 	NULL },
  { "ldap",          NULL, "LDAP DIrectory Service", 	NULL },
  { "ypbind",        NULL, "NIS Directory Service", 	NULL },
  { "courier-imap",  NULL, "IMAP4 Mail Service", 	NULL },
  { "courier-pop3",  NULL, "POP3 Mail Service", 	NULL },
  { "named",         NULL, "Domain Name Service", 	NULL },
  { "bind",          NULL, "Domain Name Service", 	NULL },
  { "httpd",         NULL, "HTTP Server", 		NULL },
  { "apache",        NULL, "HTTP Server", 		"Provides s highly scalable and flexible web server "
							"capable of implementing various protocols incluing "
							"but not limited to HTTP" },
  { "autofs",        NULL, "Automounter", 		NULL },
  { "squid",         NULL, "Web Cache Proxy ",		NULL },
  { "perfcountd",    NULL, "Performance Monitoring Daemon", NULL },
  { "pgsql",	     NULL, "PgSQL Database Server", 	"Provides service for SQL database from Postgresql.org" },
  { "arpwatch",	     NULL, "ARP Tables watcher", 	"Provides service for monitoring ARP tables for changes" },
  { "dhcpd",	     NULL, "DHCP Server", 		"Provides service for dynamic host configuration and IP assignment" },
  { "nwserv",	     NULL, "NetWare Server Emulator", 	"Provides service for emulating Novell NetWare 3.12 server" },
  { "proftpd",	     NULL, "Professional FTP Server", 	"Provides high configurable service for FTP connection and "
							"file transferring" },
  { "ssh2",	     NULL, "SSH Secure Shell", 		"Provides service for secure connection for remote administration" },
  { "sshd",	     NULL, "SSH Secure Shell", 		"Provides service for secure connection for remote administration" },
  { NULL, NULL, NULL, NULL }
};

/********************************************************************
 This is where we do the dirty work of filling in things like the
 Display name, Description, etc...
********************************************************************/
static char *svcctl_get_common_service_dispname(TALLOC_CTX *mem_ctx,
						const char *servicename)
{
	uint32_t i;

	for (i = 0; common_unix_svcs[i].servicename; i++) {
		if (strequal(servicename, common_unix_svcs[i].servicename)) {
			char *dispname;
			dispname = talloc_asprintf(mem_ctx, "%s (%s)",
					common_unix_svcs[i].dispname,
					common_unix_svcs[i].servicename);
			if (dispname == NULL) {
				return NULL;
			}
			return dispname;
		}
	}

	return talloc_strdup(mem_ctx, servicename);
}

/********************************************************************
********************************************************************/
static char *svcctl_cleanup_string(TALLOC_CTX *mem_ctx,
				   const char *string)
{
	char *clean = NULL;
	char *begin, *end;

	clean = talloc_strdup(mem_ctx, string);
	if (clean == NULL) {
		return NULL;
	}
	begin = clean;

	/* trim any beginning whilespace */
	while (isspace(*begin)) {
		begin++;
	}

	if (*begin == '\0') {
		return NULL;
	}

	/* trim any trailing whitespace or carriage returns.
	   Start at the end and move backwards */

	end = begin + strlen(begin) - 1;

	while (isspace(*end) || *end=='\n' || *end=='\r') {
		*end = '\0';
		end--;
	}

	return begin;
}

/********************************************************************
********************************************************************/
static bool read_init_file(TALLOC_CTX *mem_ctx,
			   const char *servicename,
			   struct rcinit_file_information **service_info)
{
	struct rcinit_file_information *info = NULL;
	char *filepath = NULL;
	char str[1024];
	XFILE *f = NULL;
	char *p = NULL;

	info = talloc_zero(mem_ctx, struct rcinit_file_information);
	if (info == NULL) {
		return false;
	}

	/* attempt the file open */

	filepath = talloc_asprintf(mem_ctx,
				   "%s/%s/%s",
				   get_dyn_MODULESDIR(),
				   SVCCTL_SCRIPT_DIR,
				   servicename);
	if (filepath == NULL) {
		return false;
	}
	f = x_fopen( filepath, O_RDONLY, 0 );
	if (f == NULL) {
		DEBUG(0,("read_init_file: failed to open [%s]\n", filepath));
		return false;
	}

	while ((x_fgets(str, sizeof(str) - 1, f)) != NULL) {
		/* ignore everything that is not a full line
		   comment starting with a '#' */

		if (str[0] != '#') {
			continue;
		}

		/* Look for a line like '^#.*Description:' */

		p = strstr(str, "Description:");
		if (p != NULL) {
			char *desc;

			p += strlen( "Description:" ) + 1;
			if (p == NULL) {
				break;
			}

			desc = svcctl_cleanup_string(mem_ctx, p);
			if (desc != NULL) {
				info->description = talloc_strdup(info, desc);
			}
		}
	}

	x_fclose(f);

	if (info->description == NULL) {
		info->description = talloc_strdup(info,
						  "External Unix Service");
		if (info->description == NULL) {
			return false;
		}
	}

	*service_info = info;

	return true;
}

static bool svcctl_add_service(TALLOC_CTX *mem_ctx,
			       struct dcerpc_binding_handle *h,
			       struct policy_handle *hive_hnd,
			       const char *key,
			       uint32_t access_mask,
			       const char *name)
{
	enum winreg_CreateAction action = REG_ACTION_NONE;
	struct security_descriptor *sd = NULL;
	struct policy_handle key_hnd;
	struct winreg_String wkey;
	struct winreg_String wkeyclass;
	char *description = NULL;
	char *dname = NULL;
	char *ipath = NULL;
	bool ok = false;
	uint32_t i;
	NTSTATUS status;
	WERROR result = WERR_OK;

	ZERO_STRUCT(key_hnd);

	ZERO_STRUCT(wkey);
	wkey.name = talloc_asprintf(mem_ctx, "%s\\%s", key, name);
	if (wkey.name == NULL) {
		goto done;
	}

	ZERO_STRUCT(wkeyclass);
	wkeyclass.name = "";

	status = dcerpc_winreg_CreateKey(h,
					 mem_ctx,
					 hive_hnd,
					 wkey,
					 wkeyclass,
					 0,
					 access_mask,
					 NULL,
					 &key_hnd,
					 &action,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create key %s: %s\n",
			wkey.name, nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create key %s: %s\n",
			wkey.name, win_errstr(result)));
		goto done;
	}

	/* These values are hardcoded in all QueryServiceConfig() replies.
	   I'm just storing them here for cosmetic purposes */
	status = dcerpc_winreg_set_dword(mem_ctx,
					 h,
					 &key_hnd,
					 "Start",
					 SVCCTL_AUTO_START,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create value: %s\n",
			  nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create value: %s\n",
			  win_errstr(result)));
		goto done;
	}

	status = dcerpc_winreg_set_dword(mem_ctx,
					 h,
					 &key_hnd,
					 "Type",
					 SERVICE_TYPE_WIN32_OWN_PROCESS,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create value: %s\n",
			  nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create value: %s\n",
			  win_errstr(result)));
		goto done;
	}

	status = dcerpc_winreg_set_dword(mem_ctx,
					 h,
					 &key_hnd,
					 "ErrorControl",
					 SVCCTL_SVC_ERROR_NORMAL,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create value: %s\n",
			  nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create value: %s\n",
			  win_errstr(result)));
		goto done;
	}

	status = dcerpc_winreg_set_sz(mem_ctx,
				      h,
				      &key_hnd,
				      "ObjectName",
				      "LocalSystem",
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create value: %s\n",
			  nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create value: %s\n",
			  win_errstr(result)));
		goto done;
	}

	/*
	 * Special considerations for internal services and the DisplayName
	 * value.
	 */
	for (i = 0; builtin_svcs[i].servicename; i++) {
		if (strequal(name, builtin_svcs[i].servicename)) {
			ipath = talloc_asprintf(mem_ctx,
						"%s/%s/%s",
						get_dyn_MODULESDIR(),
						SVCCTL_SCRIPT_DIR,
						builtin_svcs[i].daemon);
			description = talloc_strdup(mem_ctx, builtin_svcs[i].description);
			dname = talloc_strdup(mem_ctx, builtin_svcs[i].dispname);
			break;
		}
	}

	/* Default to an external service if we haven't found a match */
	if (builtin_svcs[i].servicename == NULL) {
		struct rcinit_file_information *init_info = NULL;
		char *dispname = NULL;

		ipath = talloc_asprintf(mem_ctx,
					"%s/%s/%s",
					get_dyn_MODULESDIR(),
					SVCCTL_SCRIPT_DIR,
					name);

		/* lookup common unix display names */
		dispname = svcctl_get_common_service_dispname(mem_ctx, name);
		dname = talloc_strdup(mem_ctx, dispname ? dispname : "");

		/* get info from init file itself */
		if (read_init_file(mem_ctx, name, &init_info)) {
			description = talloc_strdup(mem_ctx,
						    init_info->description);
		} else {
			description = talloc_strdup(mem_ctx,
						    "External Unix Service");
		}
	}

	if (ipath == NULL || dname == NULL || description == NULL) {
		goto done;
	}

	status = dcerpc_winreg_set_sz(mem_ctx,
				      h,
				      &key_hnd,
				      "DisplayName",
				      dname,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create value: %s\n",
			  nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create value: %s\n",
			  win_errstr(result)));
		goto done;
	}

	status = dcerpc_winreg_set_sz(mem_ctx,
				      h,
				      &key_hnd,
				      "ImagePath",
				      ipath,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create value: %s\n",
			  nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create value: %s\n",
			  win_errstr(result)));
		goto done;
	}

	status = dcerpc_winreg_set_sz(mem_ctx,
				      h,
				      &key_hnd,
				      "Description",
				      description,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create value: %s\n",
			  nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create value: %s\n",
			  win_errstr(result)));
		goto done;
	}

	sd = svcctl_gen_service_sd(mem_ctx);
	if (sd == NULL) {
		DEBUG(0, ("add_new_svc_name: Failed to create default "
			  "sec_desc!\n"));
		goto done;
	}

	if (is_valid_policy_hnd(&key_hnd)) {
		dcerpc_winreg_CloseKey(h, mem_ctx, &key_hnd, &result);
	}
	ZERO_STRUCT(key_hnd);

	ZERO_STRUCT(wkey);
	wkey.name = talloc_asprintf(mem_ctx, "%s\\%s\\Security", key, name);
	if (wkey.name == NULL) {
		result = WERR_NOMEM;
		goto done;
	}

	ZERO_STRUCT(wkeyclass);
	wkeyclass.name = "";

	status = dcerpc_winreg_CreateKey(h,
					 mem_ctx,
					 hive_hnd,
					 wkey,
					 wkeyclass,
					 0,
					 access_mask,
					 NULL,
					 &key_hnd,
					 &action,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("eventlog_init_winreg_keys: Could not create key %s: %s\n",
			wkey.name, nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("eventlog_init_winreg_keys: Could not create key %s: %s\n",
			wkey.name, win_errstr(result)));
		goto done;
	}

	status = dcerpc_winreg_set_sd(mem_ctx,
				      h,
				      &key_hnd,
				      "Security",
				      sd,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create value: %s\n",
			  nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("svcctl_init_winreg_keys: Could not create value: %s\n",
			  win_errstr(result)));
		goto done;
	}

	ok = true;
done:
	if (is_valid_policy_hnd(&key_hnd)) {
		dcerpc_winreg_CloseKey(h, mem_ctx, &key_hnd, &result);
	}

	return ok;
}

bool svcctl_init_winreg(struct messaging_context *msg_ctx)
{
	struct dcerpc_binding_handle *h = NULL;
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	const char **service_list = lp_svcctl_list();
	const char **subkeys = NULL;
	uint32_t num_subkeys = 0;
	char *key = NULL;
	uint32_t i;
	NTSTATUS status;
	WERROR result = WERR_OK;
	bool ok = false;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return false;
	}

	DEBUG(3, ("Initialise the svcctl registry keys if needed.\n"));

	ZERO_STRUCT(hive_hnd);
	ZERO_STRUCT(key_hnd);

	key = talloc_strdup(tmp_ctx, TOP_LEVEL_SERVICES_KEY);
	if (key == NULL) {
		goto done;
	}

	result = regdb_open();
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(10, ("regdb_open failed: %s\n",
			   win_errstr(result)));
		goto done;
	}
	result = regdb_transaction_start();
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(10, ("regdb_transaction_start failed: %s\n",
			   win_errstr(result)));
		goto done;
	}

	status = dcerpc_winreg_int_hklm_openkey(tmp_ctx,
						get_session_info_system(),
						msg_ctx,
						&h,
						key,
						false,
						access_mask,
						&hive_hnd,
						&key_hnd,
						&result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("svcctl_init_winreg: Could not open %s - %s\n",
			  key, nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("svcctl_init_winreg: Could not open %s - %s\n",
			  key, win_errstr(result)));
		goto done;
	}

	/* get all subkeys */
	status = dcerpc_winreg_enum_keys(tmp_ctx,
					 h,
					 &key_hnd,
					 &num_subkeys,
					 &subkeys,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("svcctl_init_winreg: Could enum keys at %s - %s\n",
			  key, nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("svcctl_init_winreg: Could enum keys at %s - %s\n",
			  key, win_errstr(result)));
		goto done;
	}

	for (i = 0; builtin_svcs[i].servicename != NULL; i++) {
		uint32_t j;
		bool skip = false;

		for (j = 0; j < num_subkeys; j++) {
			if (strequal(subkeys[i], builtin_svcs[i].servicename)) {
				skip = true;
			}
		}

		if (skip) {
			continue;
		}

		ok = svcctl_add_service(tmp_ctx,
					h,
					&hive_hnd,
					key,
					access_mask,
					builtin_svcs[i].servicename);
		if (!ok) {
			goto done;
		}
	}

	for (i = 0; service_list && service_list[i]; i++) {
		uint32_t j;
		bool skip = false;

		for (j = 0; j < num_subkeys; j++) {
			if (strequal(subkeys[i], service_list[i])) {
				skip = true;
			}
		}

		if (skip) {
			continue;
		}

		ok = svcctl_add_service(tmp_ctx,
					h,
					&hive_hnd,
					key,
					access_mask,
					service_list[i]);
		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(h, tmp_ctx, &key_hnd, &result);
		}
		ZERO_STRUCT(key_hnd);

		if (!ok) {
			goto done;
		}
	}

done:
	if (is_valid_policy_hnd(&key_hnd)) {
		dcerpc_winreg_CloseKey(h, tmp_ctx, &key_hnd, &result);
	}

	if (ok) {
		result = regdb_transaction_commit();
		if (!W_ERROR_IS_OK(result)) {
			DEBUG(10, ("regdb_transaction_commit failed: %s\n",
				   win_errstr(result)));
		}
	} else {
		result = regdb_transaction_cancel();
		if (!W_ERROR_IS_OK(result)) {
			DEBUG(10, ("regdb_transaction_cancel failed: %s\n",
				   win_errstr(result)));
		}
	}
	regdb_close();
	return ok;
}

/* vim: set ts=8 sw=8 noet cindent syntax=c.doxygen: */
