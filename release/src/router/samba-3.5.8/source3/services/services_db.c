/* 
 *  Unix SMB/CIFS implementation.
 *  Service Control API Implementation
 * 
 *  Copyright (C) Marcin Krzysztof Porwit         2005.
 *  Largely Rewritten by:
 *  Copyright (C) Gerald (Jerry) Carter           2005.
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

struct rcinit_file_information {
	char *description;
};

struct service_display_info {
	const char *servicename;
	const char *daemon;
	const char *dispname;
	const char *description;
};

struct service_display_info builtin_svcs[] = {
  { "Spooler",	      "smbd", 	"Print Spooler", "Internal service for spooling files to print devices" },
  { "NETLOGON",	      "smbd", 	"Net Logon", "File service providing access to policy and profile data (not remotely manageable)" },
  { "RemoteRegistry", "smbd", 	"Remote Registry Service", "Internal service providing remote access to "
				"the Samba registry" },
  { "WINS",           "nmbd", 	"Windows Internet Name Service (WINS)", "Internal service providing a "
				"NetBIOS point-to-point name server (not remotely manageable)" },
  { NULL, NULL, NULL, NULL }
};

struct service_display_info common_unix_svcs[] = {
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
********************************************************************/

static SEC_DESC* construct_service_sd( TALLOC_CTX *ctx )
{
	SEC_ACE ace[4];
	size_t i = 0;
	SEC_DESC *sd = NULL;
	SEC_ACL *theacl = NULL;
	size_t sd_size;

	/* basic access for Everyone */

	init_sec_ace(&ace[i++], &global_sid_World,
		SEC_ACE_TYPE_ACCESS_ALLOWED, SERVICE_READ_ACCESS, 0);

	init_sec_ace(&ace[i++], &global_sid_Builtin_Power_Users,
			SEC_ACE_TYPE_ACCESS_ALLOWED, SERVICE_EXECUTE_ACCESS, 0);

	init_sec_ace(&ace[i++], &global_sid_Builtin_Server_Operators,
		SEC_ACE_TYPE_ACCESS_ALLOWED, SERVICE_ALL_ACCESS, 0);
	init_sec_ace(&ace[i++], &global_sid_Builtin_Administrators,
		SEC_ACE_TYPE_ACCESS_ALLOWED, SERVICE_ALL_ACCESS, 0);

	/* create the security descriptor */

	if ( !(theacl = make_sec_acl(ctx, NT4_ACL_REVISION, i, ace)) )
		return NULL;

	if ( !(sd = make_sec_desc(ctx, SECURITY_DESCRIPTOR_REVISION_1,
				  SEC_DESC_SELF_RELATIVE, NULL, NULL, NULL,
				  theacl, &sd_size)) )
		return NULL;

	return sd;
}

/********************************************************************
 This is where we do the dirty work of filling in things like the
 Display name, Description, etc...
********************************************************************/

static char *get_common_service_dispname( const char *servicename )
{
	int i;

	for ( i=0; common_unix_svcs[i].servicename; i++ ) {
		if (strequal(servicename, common_unix_svcs[i].servicename)) {
			char *dispname;
			if (asprintf(&dispname,
				"%s (%s)",
				common_unix_svcs[i].dispname,
				common_unix_svcs[i].servicename) < 0) {
				return NULL;
			}
			return dispname;
		}
	}

	return SMB_STRDUP(servicename );
}

/********************************************************************
********************************************************************/

static char *cleanup_string( const char *string )
{
	char *clean = NULL;
	char *begin, *end;
	TALLOC_CTX *ctx = talloc_tos();

	clean = talloc_strdup(ctx, string);
	if (!clean) {
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

	while ( isspace(*end) || *end=='\n' || *end=='\r' ) {
		*end = '\0';
		end--;
	}

	return begin;
}

/********************************************************************
********************************************************************/

static bool read_init_file( const char *servicename, struct rcinit_file_information **service_info )
{
	struct rcinit_file_information *info = NULL;
	char *filepath = NULL;
	char str[1024];
	XFILE *f = NULL;
	char *p = NULL;

	if ( !(info = TALLOC_ZERO_P( NULL, struct rcinit_file_information ) ) )
		return False;

	/* attempt the file open */

	filepath = talloc_asprintf(info, "%s/%s/%s", get_dyn_MODULESDIR(),
				SVCCTL_SCRIPT_DIR, servicename);
	if (!filepath) {
		TALLOC_FREE(info);
		return false;
	}
	if (!(f = x_fopen( filepath, O_RDONLY, 0 ))) {
		DEBUG(0,("read_init_file: failed to open [%s]\n", filepath));
		TALLOC_FREE(info);
		return false;
	}

	while ( (x_fgets( str, sizeof(str)-1, f )) != NULL ) {
		/* ignore everything that is not a full line
		   comment starting with a '#' */

		if ( str[0] != '#' )
			continue;

		/* Look for a line like '^#.*Description:' */

		if ( (p = strstr( str, "Description:" )) != NULL ) {
			char *desc;

			p += strlen( "Description:" ) + 1;
			if ( !p )
				break;

			if ( (desc = cleanup_string(p)) != NULL )
				info->description = talloc_strdup( info, desc );
		}
	}

	x_fclose( f );

	if ( !info->description )
		info->description = talloc_strdup( info, "External Unix Service" );

	*service_info = info;
	TALLOC_FREE(filepath);

	return True;
}

/********************************************************************
 This is where we do the dirty work of filling in things like the
 Display name, Description, etc...
********************************************************************/

static void fill_service_values(const char *name, struct regval_ctr *values)
{
	char *dname, *ipath, *description;
	uint32 dword;
	int i;

	/* These values are hardcoded in all QueryServiceConfig() replies.
	   I'm just storing them here for cosmetic purposes */

	dword = SVCCTL_AUTO_START;
	regval_ctr_addvalue( values, "Start", REG_DWORD, (char*)&dword, sizeof(uint32));

	dword = SERVICE_TYPE_WIN32_OWN_PROCESS;
	regval_ctr_addvalue( values, "Type", REG_DWORD, (char*)&dword, sizeof(uint32));

	dword = SVCCTL_SVC_ERROR_NORMAL;
	regval_ctr_addvalue( values, "ErrorControl", REG_DWORD, (char*)&dword, sizeof(uint32));

	/* everything runs as LocalSystem */

	regval_ctr_addvalue_sz(values, "ObjectName", "LocalSystem");

	/* special considerations for internal services and the DisplayName value */

	for ( i=0; builtin_svcs[i].servicename; i++ ) {
		if ( strequal( name, builtin_svcs[i].servicename ) ) {
			ipath = talloc_asprintf(talloc_tos(), "%s/%s/%s",
					get_dyn_MODULESDIR(), SVCCTL_SCRIPT_DIR,
					builtin_svcs[i].daemon);
			description = talloc_strdup(talloc_tos(), builtin_svcs[i].description);
			dname = talloc_strdup(talloc_tos(), builtin_svcs[i].dispname);
			break;
		}
	}

	/* default to an external service if we haven't found a match */

	if ( builtin_svcs[i].servicename == NULL ) {
		char *dispname = NULL;
		struct rcinit_file_information *init_info = NULL;

		ipath = talloc_asprintf(talloc_tos(), "%s/%s/%s",
					get_dyn_MODULESDIR(), SVCCTL_SCRIPT_DIR,
					name);

		/* lookup common unix display names */
		dispname = get_common_service_dispname(name);
		dname = talloc_strdup(talloc_tos(), dispname ? dispname : "");
		SAFE_FREE(dispname);

		/* get info from init file itself */
		if ( read_init_file( name, &init_info ) ) {
			description = talloc_strdup(talloc_tos(), init_info->description);
			TALLOC_FREE( init_info );
		}
		else {
			description = talloc_strdup(talloc_tos(), "External Unix Service");
		}
	}

	/* add the new values */

	regval_ctr_addvalue_sz(values, "DisplayName", dname);
	regval_ctr_addvalue_sz(values, "ImagePath", ipath);
	regval_ctr_addvalue_sz(values, "Description", description);

	TALLOC_FREE(dname);
	TALLOC_FREE(ipath);
	TALLOC_FREE(description);

	return;
}

/********************************************************************
********************************************************************/

static void add_new_svc_name(struct registry_key_handle *key_parent,
			     struct regsubkey_ctr *subkeys,
			     const char *name )
{
	struct registry_key_handle *key_service = NULL, *key_secdesc = NULL;
	WERROR wresult;
	char *path = NULL;
	struct regval_ctr *values = NULL;
	struct regsubkey_ctr *svc_subkeys = NULL;
	SEC_DESC *sd = NULL;
	DATA_BLOB sd_blob;
	NTSTATUS status;

	/* add to the list and create the subkey path */

	regsubkey_ctr_addkey( subkeys, name );
	store_reg_keys( key_parent, subkeys );

	/* open the new service key */

	if (asprintf(&path, "%s\\%s", KEY_SERVICES, name) < 0) {
		return;
	}
	wresult = regkey_open_internal( NULL, &key_service, path,
					get_root_nt_token(), REG_KEY_ALL );
	if ( !W_ERROR_IS_OK(wresult) ) {
		DEBUG(0,("add_new_svc_name: key lookup failed! [%s] (%s)\n",
			path, win_errstr(wresult)));
		SAFE_FREE(path);
		return;
	}
	SAFE_FREE(path);

	/* add the 'Security' key */

	wresult = regsubkey_ctr_init(key_service, &svc_subkeys);
	if (!W_ERROR_IS_OK(wresult)) {
		DEBUG(0,("add_new_svc_name: talloc() failed!\n"));
		TALLOC_FREE( key_service );
		return;
	}

	fetch_reg_keys( key_service, svc_subkeys );
	regsubkey_ctr_addkey( svc_subkeys, "Security" );
	store_reg_keys( key_service, svc_subkeys );

	/* now for the service values */

	if ( !(values = TALLOC_ZERO_P( key_service, struct regval_ctr )) ) {
		DEBUG(0,("add_new_svc_name: talloc() failed!\n"));
		TALLOC_FREE( key_service );
		return;
	}

	fill_service_values( name, values );
	store_reg_values( key_service, values );

	/* cleanup the service key*/

	TALLOC_FREE( key_service );

	/* now add the security descriptor */

	if (asprintf(&path, "%s\\%s\\%s", KEY_SERVICES, name, "Security") < 0) {
		return;
	}
	wresult = regkey_open_internal( NULL, &key_secdesc, path,
					get_root_nt_token(), REG_KEY_ALL );
	if ( !W_ERROR_IS_OK(wresult) ) {
		DEBUG(0,("add_new_svc_name: key lookup failed! [%s] (%s)\n",
			path, win_errstr(wresult)));
		TALLOC_FREE( key_secdesc );
		SAFE_FREE(path);
		return;
	}
	SAFE_FREE(path);

	if ( !(values = TALLOC_ZERO_P( key_secdesc, struct regval_ctr )) ) {
		DEBUG(0,("add_new_svc_name: talloc() failed!\n"));
		TALLOC_FREE( key_secdesc );
		return;
	}

	if ( !(sd = construct_service_sd(key_secdesc)) ) {
		DEBUG(0,("add_new_svc_name: Failed to create default sec_desc!\n"));
		TALLOC_FREE( key_secdesc );
		return;
	}

	status = marshall_sec_desc(key_secdesc, sd, &sd_blob.data,
				   &sd_blob.length);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("marshall_sec_desc failed: %s\n",
			  nt_errstr(status)));
		TALLOC_FREE(key_secdesc);
		return;
	}

	regval_ctr_addvalue(values, "Security", REG_BINARY,
			    (const char *)sd_blob.data, sd_blob.length);
	store_reg_values( key_secdesc, values );

	TALLOC_FREE( key_secdesc );

	return;
}

/********************************************************************
********************************************************************/

void svcctl_init_keys( void )
{
	const char **service_list = lp_svcctl_list();
	int i;
	struct regsubkey_ctr *subkeys = NULL;
	struct registry_key_handle *key = NULL;
	WERROR wresult;

	/* bad mojo here if the lookup failed.  Should not happen */

	wresult = regkey_open_internal( NULL, &key, KEY_SERVICES,
					get_root_nt_token(), REG_KEY_ALL );

	if ( !W_ERROR_IS_OK(wresult) ) {
		DEBUG(0,("svcctl_init_keys: key lookup failed! (%s)\n",
			win_errstr(wresult)));
		return;
	}

	/* lookup the available subkeys */

	wresult = regsubkey_ctr_init(key, &subkeys);
	if (!W_ERROR_IS_OK(wresult)) {
		DEBUG(0,("svcctl_init_keys: talloc() failed!\n"));
		TALLOC_FREE( key );
		return;
	}

	fetch_reg_keys( key, subkeys );

	/* the builtin services exist */

	for ( i=0; builtin_svcs[i].servicename; i++ )
		add_new_svc_name( key, subkeys, builtin_svcs[i].servicename );

	for ( i=0; service_list && service_list[i]; i++ ) {

		/* only add new services */
		if ( regsubkey_ctr_key_exists( subkeys, service_list[i] ) )
			continue;

		/* Add the new service key and initialize the appropriate values */

		add_new_svc_name( key, subkeys, service_list[i] );
	}

	TALLOC_FREE( key );

	/* initialize the control hooks */

	init_service_op_table();

	return;
}

/********************************************************************
 This is where we do the dirty work of filling in things like the
 Display name, Description, etc...Always return a default secdesc
 in case of any failure.
********************************************************************/

SEC_DESC *svcctl_get_secdesc( TALLOC_CTX *ctx, const char *name, NT_USER_TOKEN *token )
{
	struct registry_key_handle *key = NULL;
	struct regval_ctr *values = NULL;
	struct regval_blob *val = NULL;
	SEC_DESC *ret_sd = NULL;
	char *path= NULL;
	WERROR wresult;
	NTSTATUS status;

	/* now add the security descriptor */

	if (asprintf(&path, "%s\\%s\\%s", KEY_SERVICES, name, "Security") < 0) {
		return NULL;
	}
	wresult = regkey_open_internal( NULL, &key, path, token,
					REG_KEY_ALL );
	if ( !W_ERROR_IS_OK(wresult) ) {
		DEBUG(0,("svcctl_get_secdesc: key lookup failed! [%s] (%s)\n",
			path, win_errstr(wresult)));
		goto done;
	}

	if ( !(values = TALLOC_ZERO_P( key, struct regval_ctr )) ) {
		DEBUG(0,("svcctl_get_secdesc: talloc() failed!\n"));
		goto done;
	}

	if (fetch_reg_values( key, values ) == -1) {
		DEBUG(0, ("Error getting registry values\n"));
		goto done;
	}

	if ( !(val = regval_ctr_getvalue( values, "Security" )) ) {
		goto fallback_to_default_sd;
	}

	/* stream the service security descriptor */

	status = unmarshall_sec_desc(ctx, regval_data_p(val),
				     regval_size(val), &ret_sd);

	if (NT_STATUS_IS_OK(status)) {
		goto done;
	}

fallback_to_default_sd:
	DEBUG(6, ("svcctl_get_secdesc: constructing default secdesc for "
		  "service [%s]\n", name));
	ret_sd = construct_service_sd(ctx);

done:
	SAFE_FREE(path);
	TALLOC_FREE(key);
	return ret_sd;
}

/********************************************************************
 Wrapper to make storing a Service sd easier
********************************************************************/

bool svcctl_set_secdesc( TALLOC_CTX *ctx, const char *name, SEC_DESC *sec_desc, NT_USER_TOKEN *token )
{
	struct registry_key_handle *key = NULL;
	WERROR wresult;
	char *path = NULL;
	struct regval_ctr *values = NULL;
	DATA_BLOB blob;
	NTSTATUS status;
	bool ret = False;

	/* now add the security descriptor */

	if (asprintf(&path, "%s\\%s\\%s", KEY_SERVICES, name, "Security") < 0) {
		return false;
	}
	wresult = regkey_open_internal( NULL, &key, path, token,
					REG_KEY_ALL );
	if ( !W_ERROR_IS_OK(wresult) ) {
		DEBUG(0,("svcctl_get_secdesc: key lookup failed! [%s] (%s)\n",
			path, win_errstr(wresult)));
		SAFE_FREE(path);
		return False;
	}
	SAFE_FREE(path);

	if ( !(values = TALLOC_ZERO_P( key, struct regval_ctr )) ) {
		DEBUG(0,("svcctl_set_secdesc: talloc() failed!\n"));
		TALLOC_FREE( key );
		return False;
	}

	/* stream the printer security descriptor */

	status = marshall_sec_desc(ctx, sec_desc, &blob.data, &blob.length);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("svcctl_set_secdesc: ndr_push_struct_blob() failed!\n"));
		TALLOC_FREE( key );
		return False;
	}

	regval_ctr_addvalue( values, "Security", REG_BINARY, (const char *)blob.data, blob.length);
	ret = store_reg_values( key, values );

	/* cleanup */

	TALLOC_FREE( key);

	return ret;
}

/********************************************************************
********************************************************************/

const char *svcctl_lookup_dispname(TALLOC_CTX *ctx, const char *name, NT_USER_TOKEN *token )
{
	const char *display_name = NULL;
	struct registry_key_handle *key = NULL;
	struct regval_ctr *values = NULL;
	struct regval_blob *val = NULL;
	char *path = NULL;
	WERROR wresult;
	DATA_BLOB blob;

	/* now add the security descriptor */

	if (asprintf(&path, "%s\\%s", KEY_SERVICES, name) < 0) {
		return NULL;
	}
	wresult = regkey_open_internal( NULL, &key, path, token,
					REG_KEY_READ );
	if ( !W_ERROR_IS_OK(wresult) ) {
		DEBUG(0,("svcctl_lookup_dispname: key lookup failed! [%s] (%s)\n", 
			path, win_errstr(wresult)));
		SAFE_FREE(path);
		goto fail;
	}
	SAFE_FREE(path);

	if ( !(values = TALLOC_ZERO_P( key, struct regval_ctr )) ) {
		DEBUG(0,("svcctl_lookup_dispname: talloc() failed!\n"));
		TALLOC_FREE( key );
		goto fail;
	}

	fetch_reg_values( key, values );

	if ( !(val = regval_ctr_getvalue( values, "DisplayName" )) )
		goto fail;

	blob = data_blob_const(regval_data_p(val), regval_size(val));
	pull_reg_sz(ctx, &blob, &display_name);

	TALLOC_FREE( key );

	return display_name;

fail:
	/* default to returning the service name */
	TALLOC_FREE( key );
	return talloc_strdup(ctx, name);
}

/********************************************************************
********************************************************************/

const char *svcctl_lookup_description(TALLOC_CTX *ctx, const char *name, NT_USER_TOKEN *token )
{
	const char *description = NULL;
	struct registry_key_handle *key = NULL;
	struct regval_ctr *values = NULL;
	struct regval_blob *val = NULL;
	char *path = NULL;
	WERROR wresult;
	DATA_BLOB blob;

	/* now add the security descriptor */

	if (asprintf(&path, "%s\\%s", KEY_SERVICES, name) < 0) {
		return NULL;
	}
	wresult = regkey_open_internal( NULL, &key, path, token,
					REG_KEY_READ );
	if ( !W_ERROR_IS_OK(wresult) ) {
		DEBUG(0,("svcctl_lookup_description: key lookup failed! [%s] (%s)\n", 
			path, win_errstr(wresult)));
		SAFE_FREE(path);
		return NULL;
	}
	SAFE_FREE(path);

	if ( !(values = TALLOC_ZERO_P( key, struct regval_ctr )) ) {
		DEBUG(0,("svcctl_lookup_description: talloc() failed!\n"));
		TALLOC_FREE( key );
		return NULL;
	}

	fetch_reg_values( key, values );

	if ( !(val = regval_ctr_getvalue( values, "Description" )) ) {
		TALLOC_FREE( key );
		return "Unix Service";
	}

	blob = data_blob_const(regval_data_p(val), regval_size(val));
	pull_reg_sz(ctx, &blob, &description);

	TALLOC_FREE(key);

	return description;
}


/********************************************************************
********************************************************************/

struct regval_ctr *svcctl_fetch_regvalues(const char *name, NT_USER_TOKEN *token)
{
	struct registry_key_handle *key = NULL;
	struct regval_ctr *values = NULL;
	char *path = NULL;
	WERROR wresult;

	/* now add the security descriptor */

	if (asprintf(&path, "%s\\%s", KEY_SERVICES, name) < 0) {
		return NULL;
	}
	wresult = regkey_open_internal( NULL, &key, path, token,
					REG_KEY_READ );
	if ( !W_ERROR_IS_OK(wresult) ) {
		DEBUG(0,("svcctl_fetch_regvalues: key lookup failed! [%s] (%s)\n",
			path, win_errstr(wresult)));
		SAFE_FREE(path);
		return NULL;
	}
	SAFE_FREE(path);

	if ( !(values = TALLOC_ZERO_P( NULL, struct regval_ctr )) ) {
		DEBUG(0,("svcctl_fetch_regvalues: talloc() failed!\n"));
		TALLOC_FREE( key );
		return NULL;
	}
	fetch_reg_values( key, values );

	TALLOC_FREE( key );
	return values;
}
