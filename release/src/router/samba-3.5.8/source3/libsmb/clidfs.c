/*
   Unix SMB/CIFS implementation.
   client connect/disconnect routines
   Copyright (C) Andrew Tridgell                  1994-1998
   Copyright (C) Gerald (Jerry) Carter            2004
   Copyright (C) Jeremy Allison                   2007-2009

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

/********************************************************************
 Important point.

 DFS paths are *always* of the form \server\share\<pathname> (the \ characters
 are not C escaped here).

 - but if we're using POSIX paths then <pathname> may contain
   '/' separators, not '\\' separators. So cope with '\\' or '/'
   as a separator when looking at the pathname part.... JRA.
********************************************************************/

/********************************************************************
 Ensure a connection is encrypted.
********************************************************************/

NTSTATUS cli_cm_force_encryption(struct cli_state *c,
			const char *username,
			const char *password,
			const char *domain,
			const char *sharename)
{
	NTSTATUS status = cli_force_encryption(c,
					username,
					password,
					domain);

	if (NT_STATUS_EQUAL(status,NT_STATUS_NOT_SUPPORTED)) {
		d_printf("Encryption required and "
			"server that doesn't support "
			"UNIX extensions - failing connect\n");
	} else if (NT_STATUS_EQUAL(status,NT_STATUS_UNKNOWN_REVISION)) {
		d_printf("Encryption required and "
			"can't get UNIX CIFS extensions "
			"version from server.\n");
	} else if (NT_STATUS_EQUAL(status,NT_STATUS_UNSUPPORTED_COMPRESSION)) {
		d_printf("Encryption required and "
			"share %s doesn't support "
			"encryption.\n", sharename);
	} else if (!NT_STATUS_IS_OK(status)) {
		d_printf("Encryption required and "
			"setup failed with error %s.\n",
			nt_errstr(status));
	}

	return status;
}

/********************************************************************
 Return a connection to a server.
********************************************************************/

static struct cli_state *do_connect(TALLOC_CTX *ctx,
					const char *server,
					const char *share,
					const struct user_auth_info *auth_info,
					bool show_sessetup,
					bool force_encrypt,
					int max_protocol,
					int port,
					int name_type)
{
	struct cli_state *c = NULL;
	struct nmb_name called, calling;
	const char *called_str;
	const char *server_n;
	struct sockaddr_storage ss;
	char *servicename;
	char *sharename;
	char *newserver, *newshare;
	const char *username;
	const char *password;
	NTSTATUS status;

	/* make a copy so we don't modify the global string 'service' */
	servicename = talloc_strdup(ctx,share);
	if (!servicename) {
		return NULL;
	}
	sharename = servicename;
	if (*sharename == '\\') {
		sharename += 2;
		called_str = sharename;
		if (server == NULL) {
			server = sharename;
		}
		sharename = strchr_m(sharename,'\\');
		if (!sharename) {
			return NULL;
		}
		*sharename = 0;
		sharename++;
	} else {
		called_str = server;
	}

	server_n = server;

	zero_sockaddr(&ss);

	make_nmb_name(&calling, global_myname(), 0x0);
	make_nmb_name(&called , called_str, name_type);

 again:
	zero_sockaddr(&ss);

	/* have to open a new connection */
	if (!(c=cli_initialise_ex(get_cmdline_auth_info_signing_state(auth_info)))) {
		d_printf("Connection to %s failed\n", server_n);
		if (c) {
			cli_shutdown(c);
		}
		return NULL;
	}
	if (port) {
		cli_set_port(c, port);
	}

	status = cli_connect(c, server_n, &ss);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("Connection to %s failed (Error %s)\n",
				server_n,
				nt_errstr(status));
		cli_shutdown(c);
		return NULL;
	}

	if (max_protocol == 0) {
		max_protocol = PROTOCOL_NT1;
	}
	c->protocol = max_protocol;
	c->use_kerberos = get_cmdline_auth_info_use_kerberos(auth_info);
	c->fallback_after_kerberos =
		get_cmdline_auth_info_fallback_after_kerberos(auth_info);
	c->use_ccache = get_cmdline_auth_info_use_ccache(auth_info);

	if (!cli_session_request(c, &calling, &called)) {
		char *p;
		d_printf("session request to %s failed (%s)\n",
			 called.name, cli_errstr(c));
		cli_shutdown(c);
		c = NULL;
		if ((p=strchr_m(called.name, '.'))) {
			*p = 0;
			goto again;
		}
		if (strcmp(called.name, "*SMBSERVER")) {
			make_nmb_name(&called , "*SMBSERVER", 0x20);
			goto again;
		}
		return NULL;
	}

	DEBUG(4,(" session request ok\n"));

	status = cli_negprot(c);

	if (!NT_STATUS_IS_OK(status)) {
		d_printf("protocol negotiation failed: %s\n",
			 nt_errstr(status));
		cli_shutdown(c);
		return NULL;
	}

	username = get_cmdline_auth_info_username(auth_info);
	password = get_cmdline_auth_info_password(auth_info);

	if (!NT_STATUS_IS_OK(cli_session_setup(c, username,
					       password, strlen(password),
					       password, strlen(password),
					       lp_workgroup()))) {
		/* If a password was not supplied then
		 * try again with a null username. */
		if (password[0] || !username[0] ||
			get_cmdline_auth_info_use_kerberos(auth_info) ||
			!NT_STATUS_IS_OK(cli_session_setup(c, "",
				    		"", 0,
						"", 0,
					       lp_workgroup()))) {
			d_printf("session setup failed: %s\n", cli_errstr(c));
			if (NT_STATUS_V(cli_nt_error(c)) ==
			    NT_STATUS_V(NT_STATUS_MORE_PROCESSING_REQUIRED))
				d_printf("did you forget to run kinit?\n");
			cli_shutdown(c);
			return NULL;
		}
		d_printf("Anonymous login successful\n");
		status = cli_init_creds(c, "", lp_workgroup(), "");
	} else {
		status = cli_init_creds(c, username, lp_workgroup(), password);
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10,("cli_init_creds() failed: %s\n", nt_errstr(status)));
		cli_shutdown(c);
		return NULL;
	}

	if ( show_sessetup ) {
		if (*c->server_domain) {
			DEBUG(0,("Domain=[%s] OS=[%s] Server=[%s]\n",
				c->server_domain,c->server_os,c->server_type));
		} else if (*c->server_os || *c->server_type) {
			DEBUG(0,("OS=[%s] Server=[%s]\n",
				 c->server_os,c->server_type));
		}
	}
	DEBUG(4,(" session setup ok\n"));

	/* here's the fun part....to support 'msdfs proxy' shares
	   (on Samba or windows) we have to issues a TRANS_GET_DFS_REFERRAL
	   here before trying to connect to the original share.
	   cli_check_msdfs_proxy() will fail if it is a normal share. */

	if ((c->capabilities & CAP_DFS) &&
			cli_check_msdfs_proxy(ctx, c, sharename,
				&newserver, &newshare,
				force_encrypt,
				username,
				password,
				lp_workgroup())) {
		cli_shutdown(c);
		return do_connect(ctx, newserver,
				newshare, auth_info, false,
				force_encrypt, max_protocol,
				port, name_type);
	}

	/* must be a normal share */

	status = cli_tcon_andx(c, sharename, "?????",
			       password, strlen(password)+1);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("tree connect failed: %s\n", nt_errstr(status));
		cli_shutdown(c);
		return NULL;
	}

	if (force_encrypt) {
		status = cli_cm_force_encryption(c,
					username,
					password,
					lp_workgroup(),
					sharename);
		if (!NT_STATUS_IS_OK(status)) {
			cli_shutdown(c);
			return NULL;
		}
	}

	DEBUG(4,(" tconx ok\n"));
	return c;
}

/****************************************************************************
****************************************************************************/

static void cli_set_mntpoint(struct cli_state *cli, const char *mnt)
{
	char *name = clean_name(NULL, mnt);
	if (!name) {
		return;
	}
	TALLOC_FREE(cli->dfs_mountpoint);
	cli->dfs_mountpoint = talloc_strdup(cli, name);
	TALLOC_FREE(name);
}

/********************************************************************
 Add a new connection to the list.
 referring_cli == NULL means a new initial connection.
********************************************************************/

static struct cli_state *cli_cm_connect(TALLOC_CTX *ctx,
					struct cli_state *referring_cli,
	 				const char *server,
					const char *share,
					const struct user_auth_info *auth_info,
					bool show_hdr,
					bool force_encrypt,
					int max_protocol,
					int port,
					int name_type)
{
	struct cli_state *cli;

	cli = do_connect(ctx, server, share,
				auth_info,
				show_hdr, force_encrypt, max_protocol,
				port, name_type);

	if (!cli ) {
		return NULL;
	}

	/* Enter into the list. */
	if (referring_cli) {
		DLIST_ADD_END(referring_cli, cli, struct cli_state *);
	}

	if (referring_cli && referring_cli->posix_capabilities) {
		uint16 major, minor;
		uint32 caplow, caphigh;
		NTSTATUS status;
		status = cli_unix_extensions_version(cli, &major, &minor,
						     &caplow, &caphigh);
		if (NT_STATUS_IS_OK(status)) {
			cli_set_unix_extensions_capabilities(cli,
					major, minor,
					caplow, caphigh);
		}
	}

	return cli;
}

/********************************************************************
 Return a connection to a server on a particular share.
********************************************************************/

static struct cli_state *cli_cm_find(struct cli_state *cli,
				const char *server,
				const char *share)
{
	struct cli_state *p;

	if (cli == NULL) {
		return NULL;
	}

	/* Search to the start of the list. */
	for (p = cli; p; p = p->prev) {
		if (strequal(server, p->desthost) &&
				strequal(share,p->share)) {
			return p;
		}
	}

	/* Search to the end of the list. */
	for (p = cli->next; p; p = p->next) {
		if (strequal(server, p->desthost) &&
				strequal(share,p->share)) {
			return p;
		}
	}

	return NULL;
}

/****************************************************************************
 Open a client connection to a \\server\share.
****************************************************************************/

struct cli_state *cli_cm_open(TALLOC_CTX *ctx,
				struct cli_state *referring_cli,
				const char *server,
				const char *share,
				const struct user_auth_info *auth_info,
				bool show_hdr,
				bool force_encrypt,
				int max_protocol,
				int port,
				int name_type)
{
	/* Try to reuse an existing connection in this list. */
	struct cli_state *c = cli_cm_find(referring_cli, server, share);

	if (c) {
		return c;
	}

	if (auth_info == NULL) {
		/* Can't do a new connection
		 * without auth info. */
		d_printf("cli_cm_open() Unable to open connection [\\%s\\%s] "
			"without auth info\n",
			server, share );
		return NULL;
	}

	return cli_cm_connect(ctx,
				referring_cli,
				server,
				share,
				auth_info,
				show_hdr,
				force_encrypt,
				max_protocol,
				port,
				name_type);
}

/****************************************************************************
****************************************************************************/

void cli_cm_display(const struct cli_state *cli)
{
	int i;

	for (i=0; cli; cli = cli->next,i++ ) {
		d_printf("%d:\tserver=%s, share=%s\n",
			i, cli->desthost, cli->share );
	}
}

/****************************************************************************
****************************************************************************/

/****************************************************************************
****************************************************************************/

#if 0
void cli_cm_set_credentials(struct user_auth_info *auth_info)
{
	SAFE_FREE(cm_creds.username);
	cm_creds.username = SMB_STRDUP(get_cmdline_auth_info_username(
					       auth_info));

	if (get_cmdline_auth_info_got_pass(auth_info)) {
		cm_set_password(get_cmdline_auth_info_password(auth_info));
	}

	cm_creds.use_kerberos = get_cmdline_auth_info_use_kerberos(auth_info);
	cm_creds.fallback_after_kerberos = false;
	cm_creds.signing_state = get_cmdline_auth_info_signing_state(auth_info);
}
#endif

/**********************************************************************
 split a dfs path into the server, share name, and extrapath components
**********************************************************************/

static void split_dfs_path(TALLOC_CTX *ctx,
				const char *nodepath,
				char **pp_server,
				char **pp_share,
				char **pp_extrapath)
{
	char *p, *q;
	char *path;

	*pp_server = NULL;
	*pp_share = NULL;
	*pp_extrapath = NULL;

	path = talloc_strdup(ctx, nodepath);
	if (!path) {
		return;
	}

	if ( path[0] != '\\' ) {
		return;
	}

	p = strchr_m( path + 1, '\\' );
	if ( !p ) {
		return;
	}

	*p = '\0';
	p++;

	/* Look for any extra/deep path */
	q = strchr_m(p, '\\');
	if (q != NULL) {
		*q = '\0';
		q++;
		*pp_extrapath = talloc_strdup(ctx, q);
	} else {
		*pp_extrapath = talloc_strdup(ctx, "");
	}

	*pp_share = talloc_strdup(ctx, p);
	*pp_server = talloc_strdup(ctx, &path[1]);
}

/****************************************************************************
 Return the original path truncated at the directory component before
 the first wildcard character. Trust the caller to provide a NULL
 terminated string
****************************************************************************/

static char *clean_path(TALLOC_CTX *ctx, const char *path)
{
	size_t len;
	char *p1, *p2, *p;
	char *path_out;

	/* No absolute paths. */
	while (IS_DIRECTORY_SEP(*path)) {
		path++;
	}

	path_out = talloc_strdup(ctx, path);
	if (!path_out) {
		return NULL;
	}

	p1 = strchr_m(path_out, '*');
	p2 = strchr_m(path_out, '?');

	if (p1 || p2) {
		if (p1 && p2) {
			p = MIN(p1,p2);
		} else if (!p1) {
			p = p2;
		} else {
			p = p1;
		}
		*p = '\0';

		/* Now go back to the start of this component. */
		p1 = strrchr_m(path_out, '/');
		p2 = strrchr_m(path_out, '\\');
		p = MAX(p1,p2);
		if (p) {
			*p = '\0';
		}
	}

	/* Strip any trailing separator */

	len = strlen(path_out);
	if ( (len > 0) && IS_DIRECTORY_SEP(path_out[len-1])) {
		path_out[len-1] = '\0';
	}

	return path_out;
}

/****************************************************************************
****************************************************************************/

static char *cli_dfs_make_full_path(TALLOC_CTX *ctx,
					struct cli_state *cli,
					const char *dir)
{
	char path_sep = '\\';

	/* Ensure the extrapath doesn't start with a separator. */
	while (IS_DIRECTORY_SEP(*dir)) {
		dir++;
	}

	if (cli->posix_capabilities & CIFS_UNIX_POSIX_PATHNAMES_CAP) {
		path_sep = '/';
	}
	return talloc_asprintf(ctx, "%c%s%c%s%c%s",
			path_sep,
			cli->desthost,
			path_sep,
			cli->share,
			path_sep,
			dir);
}

/********************************************************************
 check for dfs referral
********************************************************************/

static bool cli_dfs_check_error( struct cli_state *cli, NTSTATUS status )
{
	uint32 flgs2 = SVAL(cli->inbuf,smb_flg2);

	/* only deal with DS when we negotiated NT_STATUS codes and UNICODE */

	if (!((flgs2&FLAGS2_32_BIT_ERROR_CODES) &&
				(flgs2&FLAGS2_UNICODE_STRINGS)))
		return false;

	if (NT_STATUS_EQUAL(status, NT_STATUS(IVAL(cli->inbuf,smb_rcls))))
		return true;

	return false;
}

/********************************************************************
 Get the dfs referral link.
********************************************************************/

bool cli_dfs_get_referral(TALLOC_CTX *ctx,
			struct cli_state *cli,
			const char *path,
			CLIENT_DFS_REFERRAL**refs,
			size_t *num_refs,
			size_t *consumed)
{
	unsigned int data_len = 0;
	unsigned int param_len = 0;
	uint16 setup = TRANSACT2_GET_DFS_REFERRAL;
	char *param = NULL;
	char *rparam=NULL, *rdata=NULL;
	char *p;
	char *endp;
	size_t pathlen = 2*(strlen(path)+1);
	smb_ucs2_t *path_ucs;
	char *consumed_path = NULL;
	uint16_t consumed_ucs;
	uint16 num_referrals;
	CLIENT_DFS_REFERRAL *referrals = NULL;
	bool ret = false;

	*num_refs = 0;
	*refs = NULL;

	param = SMB_MALLOC_ARRAY(char, 2+pathlen+2);
	if (!param) {
		goto out;
	}
	SSVAL(param, 0, 0x03);	/* max referral level */
	p = &param[2];

	path_ucs = (smb_ucs2_t *)p;
	p += clistr_push(cli, p, path, pathlen, STR_TERMINATE);
	param_len = PTR_DIFF(p, param);

	if (!cli_send_trans(cli, SMBtrans2,
			NULL,                        /* name */
			-1, 0,                          /* fid, flags */
			&setup, 1, 0,                   /* setup, length, max */
			param, param_len, 2,            /* param, length, max */
			NULL, 0, cli->max_xmit /* data, length, max */
			)) {
		goto out;
	}

	if (!cli_receive_trans(cli, SMBtrans2,
		&rparam, &param_len,
		&rdata, &data_len)) {
		goto out;
	}

	if (data_len < 4) {
		goto out;
	}

	endp = rdata + data_len;

	consumed_ucs  = SVAL(rdata, 0);
	num_referrals = SVAL(rdata, 2);

	/* consumed_ucs is the number of bytes
	 * of the UCS2 path consumed not counting any
	 * terminating null. We need to convert
	 * back to unix charset and count again
	 * to get the number of bytes consumed from
	 * the incoming path. */

	if (pull_string_talloc(talloc_tos(),
			NULL,
			0,
			&consumed_path,
			path_ucs,
			consumed_ucs,
			STR_UNICODE) == 0) {
		goto out;
	}
	if (consumed_path == NULL) {
		goto out;
	}
	*consumed = strlen(consumed_path);

	if (num_referrals != 0) {
		uint16 ref_version;
		uint16 ref_size;
		int i;
		uint16 node_offset;

		referrals = TALLOC_ARRAY(ctx, CLIENT_DFS_REFERRAL,
				num_referrals);

		if (!referrals) {
			goto out;
		}
		/* start at the referrals array */

		p = rdata+8;
		for (i=0; i<num_referrals && p < endp; i++) {
			if (p + 18 > endp) {
				goto out;
			}
			ref_version = SVAL(p, 0);
			ref_size    = SVAL(p, 2);
			node_offset = SVAL(p, 16);

			if (ref_version != 3) {
				p += ref_size;
				continue;
			}

			referrals[i].proximity = SVAL(p, 8);
			referrals[i].ttl       = SVAL(p, 10);

			if (p + node_offset > endp) {
				goto out;
			}
			clistr_pull_talloc(ctx, cli->inbuf,
					   &referrals[i].dfspath,
					   p+node_offset, -1,
					   STR_TERMINATE|STR_UNICODE);

			if (!referrals[i].dfspath) {
				goto out;
			}
			p += ref_size;
		}
		if (i < num_referrals) {
			goto out;
		}
	}

	ret = true;

	*num_refs = num_referrals;
	*refs = referrals;

  out:

	TALLOC_FREE(consumed_path);
	SAFE_FREE(param);
	SAFE_FREE(rdata);
	SAFE_FREE(rparam);
	return ret;
}

/********************************************************************
********************************************************************/

bool cli_resolve_path(TALLOC_CTX *ctx,
			const char *mountpt,
			const struct user_auth_info *dfs_auth_info,
			struct cli_state *rootcli,
			const char *path,
			struct cli_state **targetcli,
			char **pp_targetpath)
{
	CLIENT_DFS_REFERRAL *refs = NULL;
	size_t num_refs = 0;
	size_t consumed = 0;
	struct cli_state *cli_ipc = NULL;
	char *dfs_path = NULL;
	char *cleanpath = NULL;
	char *extrapath = NULL;
	int pathlen;
	char *server = NULL;
	char *share = NULL;
	struct cli_state *newcli = NULL;
	char *newpath = NULL;
	char *newmount = NULL;
	char *ppath = NULL;
	SMB_STRUCT_STAT sbuf;
	uint32 attributes;

	if ( !rootcli || !path || !targetcli ) {
		return false;
	}

	/* Don't do anything if this is not a DFS root. */

	if ( !rootcli->dfsroot) {
		*targetcli = rootcli;
		*pp_targetpath = talloc_strdup(ctx, path);
		if (!*pp_targetpath) {
			return false;
		}
		return true;
	}

	*targetcli = NULL;

	/* Send a trans2_query_path_info to check for a referral. */

	cleanpath = clean_path(ctx, path);
	if (!cleanpath) {
		return false;
	}

	dfs_path = cli_dfs_make_full_path(ctx, rootcli, cleanpath);
	if (!dfs_path) {
		return false;
	}

	if (cli_qpathinfo_basic( rootcli, dfs_path, &sbuf, &attributes)) {
		/* This is an ordinary path, just return it. */
		*targetcli = rootcli;
		*pp_targetpath = talloc_strdup(ctx, path);
		if (!*pp_targetpath) {
			return false;
		}
		goto done;
	}

	/* Special case where client asked for a path that does not exist */

	if (cli_dfs_check_error(rootcli, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
		*targetcli = rootcli;
		*pp_targetpath = talloc_strdup(ctx, path);
		if (!*pp_targetpath) {
			return false;
		}
		goto done;
	}

	/* We got an error, check for DFS referral. */

	if (!cli_dfs_check_error(rootcli, NT_STATUS_PATH_NOT_COVERED)) {
		return false;
	}

	/* Check for the referral. */

	if (!(cli_ipc = cli_cm_open(ctx,
				rootcli,
				rootcli->desthost,
				"IPC$",
				dfs_auth_info,
				false,
				(rootcli->trans_enc_state != NULL),
				rootcli->protocol,
				0,
				0x20))) {
		return false;
	}

	if (!cli_dfs_get_referral(ctx, cli_ipc, dfs_path, &refs,
			&num_refs, &consumed) || !num_refs) {
		return false;
	}

	/* Just store the first referral for now. */

	if (!refs[0].dfspath) {
		return false;
	}
	split_dfs_path(ctx, refs[0].dfspath, &server, &share, &extrapath );

	if (!server || !share) {
		return false;
	}

	/* Make sure to recreate the original string including any wildcards. */

	dfs_path = cli_dfs_make_full_path(ctx, rootcli, path);
	if (!dfs_path) {
		return false;
	}
	pathlen = strlen(dfs_path);
	consumed = MIN(pathlen, consumed);
	*pp_targetpath = talloc_strdup(ctx, &dfs_path[consumed]);
	if (!*pp_targetpath) {
		return false;
	}
	dfs_path[consumed] = '\0';

	/*
 	 * *pp_targetpath is now the unconsumed part of the path.
 	 * dfs_path is now the consumed part of the path
	 * (in \server\share\path format).
 	 */

	/* Open the connection to the target server & share */
	if ((*targetcli = cli_cm_open(ctx, rootcli,
					server,
					share,
					dfs_auth_info,
					false,
					(rootcli->trans_enc_state != NULL),
					rootcli->protocol,
					0,
					0x20)) == NULL) {
		d_printf("Unable to follow dfs referral [\\%s\\%s]\n",
			server, share );
		return false;
	}

	if (extrapath && strlen(extrapath) > 0) {
		*pp_targetpath = talloc_asprintf(ctx,
						"%s%s",
						extrapath,
						*pp_targetpath);
		if (!*pp_targetpath) {
			return false;
		}
	}

	/* parse out the consumed mount path */
	/* trim off the \server\share\ */

	ppath = dfs_path;

	if (*ppath != '\\') {
		d_printf("cli_resolve_path: "
			"dfs_path (%s) not in correct format.\n",
			dfs_path );
		return false;
	}

	ppath++; /* Now pointing at start of server name. */

	if ((ppath = strchr_m( dfs_path, '\\' )) == NULL) {
		return false;
	}

	ppath++; /* Now pointing at start of share name. */

	if ((ppath = strchr_m( ppath+1, '\\' )) == NULL) {
		return false;
	}

	ppath++; /* Now pointing at path component. */

	newmount = talloc_asprintf(ctx, "%s\\%s", mountpt, ppath );
	if (!newmount) {
		return false;
	}

	cli_set_mntpoint(*targetcli, newmount);

	/* Check for another dfs referral, note that we are not
	   checking for loops here. */

	if (!strequal(*pp_targetpath, "\\") && !strequal(*pp_targetpath, "/")) {
		if (cli_resolve_path(ctx,
					newmount,
					dfs_auth_info,
					*targetcli,
					*pp_targetpath,
					&newcli,
					&newpath)) {
			/*
			 * When cli_resolve_path returns true here it's always
 			 * returning the complete path in newpath, so we're done
 			 * here.
 			 */
			*targetcli = newcli;
			*pp_targetpath = newpath;
			return true;
		}
	}

  done:

	/* If returning true ensure we return a dfs root full path. */
	if ((*targetcli)->dfsroot) {
		dfs_path = talloc_strdup(ctx, *pp_targetpath);
		if (!dfs_path) {
			return false;
		}
		*pp_targetpath = cli_dfs_make_full_path(ctx, *targetcli, dfs_path);
	}

	return true;
}

/********************************************************************
********************************************************************/

bool cli_check_msdfs_proxy(TALLOC_CTX *ctx,
				struct cli_state *cli,
				const char *sharename,
				char **pp_newserver,
				char **pp_newshare,
				bool force_encrypt,
				const char *username,
				const char *password,
				const char *domain)
{
	CLIENT_DFS_REFERRAL *refs = NULL;
	size_t num_refs = 0;
	size_t consumed = 0;
	char *fullpath = NULL;
	bool res;
	uint16 cnum;
	char *newextrapath = NULL;

	if (!cli || !sharename) {
		return false;
	}

	cnum = cli->cnum;

	/* special case.  never check for a referral on the IPC$ share */

	if (strequal(sharename, "IPC$")) {
		return false;
	}

	/* send a trans2_query_path_info to check for a referral */

	fullpath = talloc_asprintf(ctx, "\\%s\\%s", cli->desthost, sharename );
	if (!fullpath) {
		return false;
	}

	/* check for the referral */

	if (!NT_STATUS_IS_OK(cli_tcon_andx(cli, "IPC$", "IPC", NULL, 0))) {
		return false;
	}

	if (force_encrypt) {
		NTSTATUS status = cli_cm_force_encryption(cli,
					username,
					password,
					lp_workgroup(),
					"IPC$");
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
	}

	res = cli_dfs_get_referral(ctx, cli, fullpath, &refs, &num_refs, &consumed);

	if (!cli_tdis(cli)) {
		return false;
	}

	cli->cnum = cnum;

	if (!res || !num_refs) {
		return false;
	}

	if (!refs[0].dfspath) {
		return false;
	}

	split_dfs_path(ctx, refs[0].dfspath, pp_newserver,
			pp_newshare, &newextrapath );

	if ((*pp_newserver == NULL) || (*pp_newshare == NULL)) {
		return false;
	}

	/* check that this is not a self-referral */

	if (strequal(cli->desthost, *pp_newserver) &&
			strequal(sharename, *pp_newshare)) {
		return false;
	}

	return true;
}
