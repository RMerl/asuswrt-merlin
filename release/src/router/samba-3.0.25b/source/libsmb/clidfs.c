/* 
   Unix SMB/CIFS implementation.
   client connect/disconnect routines
   Copyright (C) Andrew Tridgell                  1994-1998
   Copyright (C) Gerald (Jerry) Carter            2004
   Copyright (C) Jeremy Allison                   2007
      
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

struct client_connection {
	struct client_connection *prev, *next;
	struct cli_state *cli;
	pstring mount;
};

/* global state....globals reek! */

static pstring username;
static pstring password;
static BOOL use_kerberos;
static BOOL got_pass;
static int signing_state;
int max_protocol = PROTOCOL_NT1;

static int port;
static int name_type = 0x20;
static BOOL have_ip;
static struct in_addr dest_ip;

static struct client_connection *connections;

/********************************************************************
 Return a connection to a server.
********************************************************************/

static struct cli_state *do_connect( const char *server, const char *share,
                                     BOOL show_sessetup )
{
	struct cli_state *c = NULL;
	struct nmb_name called, calling;
	const char *server_n;
	struct in_addr ip;
	pstring servicename;
	char *sharename;
	fstring newserver, newshare;
	
	/* make a copy so we don't modify the global string 'service' */
	pstrcpy(servicename, share);
	sharename = servicename;
	if (*sharename == '\\') {
		server = sharename+2;
		sharename = strchr_m(server,'\\');
		if (!sharename) return NULL;
		*sharename = 0;
		sharename++;
	}

	server_n = server;
	
	zero_ip(&ip);

	make_nmb_name(&calling, global_myname(), 0x0);
	make_nmb_name(&called , server, name_type);

 again:
	zero_ip(&ip);
	if (have_ip) 
		ip = dest_ip;

	/* have to open a new connection */
	if (!(c=cli_initialise()) || (cli_set_port(c, port) != port) ||
	    !cli_connect(c, server_n, &ip)) {
		d_printf("Connection to %s failed\n", server_n);
		return NULL;
	}

	c->protocol = max_protocol;
	c->use_kerberos = use_kerberos;
	cli_setup_signing_state(c, signing_state);
		

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

	if (!cli_negprot(c)) {
		d_printf("protocol negotiation failed\n");
		cli_shutdown(c);
		return NULL;
	}

	if (!got_pass) {
		char *pass = getpass("Password: ");
		if (pass) {
			pstrcpy(password, pass);
			got_pass = 1;
		}
	}

	if (!NT_STATUS_IS_OK(cli_session_setup(c, username, 
					       password, strlen(password),
					       password, strlen(password),
					       lp_workgroup()))) {
		/* if a password was not supplied then try again with a null username */
		if (password[0] || !username[0] || use_kerberos ||
		    !NT_STATUS_IS_OK(cli_session_setup(c, "", "", 0, "", 0,
						       lp_workgroup()))) { 
			d_printf("session setup failed: %s\n", cli_errstr(c));
			if (NT_STATUS_V(cli_nt_error(c)) == 
			    NT_STATUS_V(NT_STATUS_MORE_PROCESSING_REQUIRED))
				d_printf("did you forget to run kinit?\n");
			cli_shutdown(c);
			return NULL;
		}
		d_printf("Anonymous login successful\n");
	}

	if ( show_sessetup ) {
		if (*c->server_domain) {
			DEBUG(0,("Domain=[%s] OS=[%s] Server=[%s]\n",
				c->server_domain,c->server_os,c->server_type));
		} else if (*c->server_os || *c->server_type){
			DEBUG(0,("OS=[%s] Server=[%s]\n",
				 c->server_os,c->server_type));
		}		
	}
	DEBUG(4,(" session setup ok\n"));

	/* here's the fun part....to support 'msdfs proxy' shares
	   (on Samba or windows) we have to issues a TRANS_GET_DFS_REFERRAL 
	   here before trying to connect to the original share.
	   check_dfs_proxy() will fail if it is a normal share. */

	if ( (c->capabilities & CAP_DFS) && cli_check_msdfs_proxy( c, sharename, newserver, newshare ) ) {
		cli_shutdown(c);
		return do_connect( newserver, newshare, False );
	}

	/* must be a normal share */

	if (!cli_send_tconX(c, sharename, "?????", password, strlen(password)+1)) {
		d_printf("tree connect failed: %s\n", cli_errstr(c));
		cli_shutdown(c);
		return NULL;
	}

	DEBUG(4,(" tconx ok\n"));

	return c;
}

/****************************************************************************
****************************************************************************/

static void cli_cm_set_mntpoint( struct cli_state *c, const char *mnt )
{
	struct client_connection *p;
	int i;

	for ( p=connections,i=0; p; p=p->next,i++ ) {
		if ( strequal(p->cli->desthost, c->desthost) && strequal(p->cli->share, c->share) )
			break;
	}
	
	if ( p ) {
		pstrcpy( p->mount, mnt );
		clean_name(p->mount);
	}
}

/****************************************************************************
****************************************************************************/

const char *cli_cm_get_mntpoint( struct cli_state *c )
{
	struct client_connection *p;
	int i;

	for ( p=connections,i=0; p; p=p->next,i++ ) {
		if ( strequal(p->cli->desthost, c->desthost) && strequal(p->cli->share, c->share) )
			break;
	}
	
	if ( p )
		return p->mount;
		
	return NULL;
}

/********************************************************************
 Add a new connection to the list
********************************************************************/

static struct cli_state *cli_cm_connect( const char *server,
					const char *share,
					BOOL show_hdr)
{
	struct client_connection *node;
	
	node = SMB_XMALLOC_P( struct client_connection );
	
	node->cli = do_connect( server, share, show_hdr );

	if ( !node->cli ) {
		SAFE_FREE( node );
		return NULL;
	}

	DLIST_ADD( connections, node );

	cli_cm_set_mntpoint( node->cli, "" );

	return node->cli;

}

/********************************************************************
 Return a connection to a server.
********************************************************************/

static struct cli_state *cli_cm_find( const char *server, const char *share )
{
	struct client_connection *p;

	for ( p=connections; p; p=p->next ) {
		if ( strequal(server, p->cli->desthost) && strequal(share,p->cli->share) )
			return p->cli;
	}

	return NULL;
}

/****************************************************************************
 open a client connection to a \\server\share.  Set's the current *cli 
 global variable as a side-effect (but only if the connection is successful).
****************************************************************************/

struct cli_state *cli_cm_open(const char *server,
				const char *share,
				BOOL show_hdr)
{
	struct cli_state *c;
	
	/* try to reuse an existing connection */

	c = cli_cm_find( server, share );
	
	if ( !c ) {
		c = cli_cm_connect(server, share, show_hdr);
	}

	return c;
}

/****************************************************************************
****************************************************************************/

void cli_cm_shutdown( void )
{

	struct client_connection *p, *x;

	for ( p=connections; p; ) {
		cli_shutdown( p->cli );
		x = p;
		p = p->next;

		SAFE_FREE( x );
	}

	connections = NULL;
	return;
}

/****************************************************************************
****************************************************************************/

void cli_cm_display(void)
{
	struct client_connection *p;
	int i;

	for ( p=connections,i=0; p; p=p->next,i++ ) {
		d_printf("%d:\tserver=%s, share=%s\n", 
			i, p->cli->desthost, p->cli->share );
	}
}

/****************************************************************************
****************************************************************************/

void cli_cm_set_credentials( struct user_auth_info *user )
{
	pstrcpy( username, user->username );
	
	if ( user->got_pass ) {
		pstrcpy( password, user->password );
		got_pass = True;
	}
	
	use_kerberos = user->use_kerberos;	
	signing_state = user->signing_state;
}

/****************************************************************************
****************************************************************************/

void cli_cm_set_port( int port_number )
{
	port = port_number;
}

/****************************************************************************
****************************************************************************/

void cli_cm_set_dest_name_type( int type )
{
	name_type = type;
}

/****************************************************************************
****************************************************************************/

void cli_cm_set_dest_ip(struct in_addr ip )
{
	dest_ip = ip;
	have_ip = True;
}

/**********************************************************************
 split a dfs path into the server, share name, and extrapath components
**********************************************************************/

static void split_dfs_path( const char *nodepath, fstring server, fstring share, pstring extrapath )
{
	char *p, *q;
	pstring path;

	pstrcpy( path, nodepath );

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
		pstrcpy( extrapath, q );
	} else {
		pstrcpy( extrapath, '\0' );
	}
	
	fstrcpy( share, p );
	fstrcpy( server, &path[1] );
}

/****************************************************************************
 Return the original path truncated at the directory component before
 the first wildcard character. Trust the caller to provide a NULL 
 terminated string
****************************************************************************/

static void clean_path(const char *path, pstring path_out)
{
	size_t len;
	char *p1, *p2, *p;
		
	/* No absolute paths. */
	while (IS_DIRECTORY_SEP(*path)) {
		path++;
	}

	pstrcpy(path_out, path);

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
}

/****************************************************************************
****************************************************************************/

static void cli_dfs_make_full_path( struct cli_state *cli,
					const char *dir,
					pstring path_out)
{
	/* Ensure the extrapath doesn't start with a separator. */
	while (IS_DIRECTORY_SEP(*dir)) {
		dir++;
	}

	pstr_sprintf( path_out, "\\%s\\%s\\%s", cli->desthost, cli->share, dir);
}

/********************************************************************
 check for dfs referral
********************************************************************/

static BOOL cli_dfs_check_error( struct cli_state *cli, NTSTATUS status )
{
	uint32 flgs2 = SVAL(cli->inbuf,smb_flg2);

	/* only deal with DS when we negotiated NT_STATUS codes and UNICODE */

	if ( !( (flgs2&FLAGS2_32_BIT_ERROR_CODES) && (flgs2&FLAGS2_UNICODE_STRINGS) ) )
		return False;

	if ( NT_STATUS_EQUAL( status, NT_STATUS(IVAL(cli->inbuf,smb_rcls)) ) )
		return True;

	return False;
}

/********************************************************************
 get the dfs referral link
********************************************************************/

BOOL cli_dfs_get_referral( struct cli_state *cli,
			const char *path, 
			CLIENT_DFS_REFERRAL**refs,
			size_t *num_refs,
			uint16 *consumed)
{
	unsigned int data_len = 0;
	unsigned int param_len = 0;
	uint16 setup = TRANSACT2_GET_DFS_REFERRAL;
	char param[sizeof(pstring)+2];
	pstring data;
	char *rparam=NULL, *rdata=NULL;
	char *p;
	size_t pathlen = 2*(strlen(path)+1);
	uint16 num_referrals;
	CLIENT_DFS_REFERRAL *referrals = NULL;
	
	memset(param, 0, sizeof(param));
	SSVAL(param, 0, 0x03);	/* max referral level */
	p = &param[2];

	p += clistr_push(cli, p, path, MIN(pathlen, sizeof(param)-2), STR_TERMINATE);
	param_len = PTR_DIFF(p, param);

	if (!cli_send_trans(cli, SMBtrans2,
		NULL,                        /* name */
		-1, 0,                          /* fid, flags */
		&setup, 1, 0,                   /* setup, length, max */
		param, param_len, 2,            /* param, length, max */
		(char *)&data,  data_len, cli->max_xmit /* data, length, max */
		)) {
			return False;
	}

	if (!cli_receive_trans(cli, SMBtrans2,
		&rparam, &param_len,
		&rdata, &data_len)) {
			return False;
	}
	
	*consumed     = SVAL( rdata, 0 );
	num_referrals = SVAL( rdata, 2 );
	
	if ( num_referrals != 0 ) {
		uint16 ref_version;
		uint16 ref_size;
		int i;
		uint16 node_offset;

		referrals = SMB_XMALLOC_ARRAY( CLIENT_DFS_REFERRAL, num_referrals );

		/* start at the referrals array */
	
		p = rdata+8;
		for ( i=0; i<num_referrals; i++ ) {
			ref_version = SVAL( p, 0 );
			ref_size    = SVAL( p, 2 );
			node_offset = SVAL( p, 16 );
			
			if ( ref_version != 3 ) {
				p += ref_size;
				continue;
			}
			
			referrals[i].proximity = SVAL( p, 8 );
			referrals[i].ttl       = SVAL( p, 10 );

			clistr_pull( cli, referrals[i].dfspath, p+node_offset, 
				sizeof(referrals[i].dfspath), -1, STR_TERMINATE|STR_UNICODE );

			p += ref_size;
		}
	}
	
	*num_refs = num_referrals;
	*refs = referrals;

	SAFE_FREE(rdata);
	SAFE_FREE(rparam);

	return True;
}


/********************************************************************
********************************************************************/

BOOL cli_resolve_path( const char *mountpt,
			struct cli_state *rootcli,
			const char *path,
			struct cli_state **targetcli,
			pstring targetpath)
{
	CLIENT_DFS_REFERRAL *refs = NULL;
	size_t num_refs;
	uint16 consumed;
	struct cli_state *cli_ipc;
	pstring dfs_path, cleanpath, extrapath;
	int pathlen;
	fstring server, share;
	struct cli_state *newcli;
	pstring newpath;
	pstring newmount;
	char *ppath, *temppath = NULL;
	
	SMB_STRUCT_STAT sbuf;
	uint32 attributes;
	
	if ( !rootcli || !path || !targetcli ) {
		return False;
	}
		
	/* Don't do anything if this is not a DFS root. */

	if ( !rootcli->dfsroot) {
		*targetcli = rootcli;
		pstrcpy( targetpath, path );
		return True;
	}

	*targetcli = NULL;

	/* Send a trans2_query_path_info to check for a referral. */

	clean_path(path, cleanpath);
	cli_dfs_make_full_path(rootcli, cleanpath, dfs_path );

	if (cli_qpathinfo_basic( rootcli, dfs_path, &sbuf, &attributes ) ) {
		/* This is an ordinary path, just return it. */
		*targetcli = rootcli;
		pstrcpy( targetpath, path );
		goto done;
	}

	/* Special case where client asked for a path that does not exist */

	if ( cli_dfs_check_error(rootcli, NT_STATUS_OBJECT_NAME_NOT_FOUND) ) {
		*targetcli = rootcli;
		pstrcpy( targetpath, path );
		goto done;
	}

	/* We got an error, check for DFS referral. */

	if ( !cli_dfs_check_error(rootcli, NT_STATUS_PATH_NOT_COVERED))  {
		return False;
	}

	/* Check for the referral. */

	if ( !(cli_ipc = cli_cm_open( rootcli->desthost, "IPC$", False )) ) {
		return False;
	}
	
	if ( !cli_dfs_get_referral(cli_ipc, dfs_path, &refs, &num_refs, &consumed) 
			|| !num_refs ) {
		return False;
	}
	
	/* Just store the first referral for now. */

	split_dfs_path( refs[0].dfspath, server, share, extrapath );
	SAFE_FREE(refs);

	/* Make sure to recreate the original string including any wildcards. */
	
	cli_dfs_make_full_path( rootcli, path, dfs_path);
	pathlen = strlen( dfs_path )*2;
	consumed = MIN(pathlen, consumed );
	pstrcpy( targetpath, &dfs_path[consumed/2] );
	dfs_path[consumed/2] = '\0';

	/*
 	 * targetpath is now the unconsumed part of the path.
 	 * dfs_path is now the consumed part of the path (in \server\share\path format).
 	 */

	/* Open the connection to the target server & share */
	
	if ( (*targetcli = cli_cm_open(server, share, False)) == NULL ) {
		d_printf("Unable to follow dfs referral [\\%s\\%s]\n",
			server, share );
		return False;
	}
	
	if (strlen(extrapath) > 0) {
		string_append(&temppath, extrapath);
		string_append(&temppath, targetpath);
		pstrcpy( targetpath, temppath );
	}
	
	/* parse out the consumed mount path */
	/* trim off the \server\share\ */

	ppath = dfs_path;

	if (*ppath != '\\') {
		d_printf("cli_resolve_path: dfs_path (%s) not in correct format.\n",
			dfs_path );
		return False;
	}

	ppath++; /* Now pointing at start of server name. */
	
	if ((ppath = strchr_m( dfs_path, '\\' )) == NULL) {
		return False;
	}

	ppath++; /* Now pointing at start of share name. */

	if ((ppath = strchr_m( ppath+1, '\\' )) == NULL) {
		return False;
	}

	ppath++; /* Now pointing at path component. */

	pstr_sprintf( newmount, "%s\\%s", mountpt, ppath );

	cli_cm_set_mntpoint( *targetcli, newmount );

	/* Check for another dfs referral, note that we are not 
	   checking for loops here. */

	if ( !strequal( targetpath, "\\" ) &&  !strequal( targetpath, "/")) {
		if ( cli_resolve_path( newmount, *targetcli, targetpath, &newcli, newpath ) ) {
			/*
			 * When cli_resolve_path returns true here it's always
 			 * returning the complete path in newpath, so we're done
 			 * here.
 			 */
			*targetcli = newcli;
			pstrcpy( targetpath, newpath );
			return True;
		}
	}

  done:

	/* If returning True ensure we return a dfs root full path. */
	if ( (*targetcli)->dfsroot ) {
		pstrcpy(dfs_path, targetpath );
		cli_dfs_make_full_path( *targetcli, dfs_path, targetpath); 
	}

	return True;
}

/********************************************************************
********************************************************************/

BOOL cli_check_msdfs_proxy( struct cli_state *cli, const char *sharename,
                            fstring newserver, fstring newshare )
{
	CLIENT_DFS_REFERRAL *refs = NULL;
	size_t num_refs;
	uint16 consumed;
	pstring fullpath;
	BOOL res;
	uint16 cnum;
	pstring newextrapath;
	
	if ( !cli || !sharename )
		return False;

	cnum = cli->cnum;

	/* special case.  never check for a referral on the IPC$ share */

	if ( strequal( sharename, "IPC$" ) ) {
		return False;
	}
		
	/* send a trans2_query_path_info to check for a referral */
	
	pstr_sprintf( fullpath, "\\%s\\%s", cli->desthost, sharename );

	/* check for the referral */

	if (!cli_send_tconX(cli, "IPC$", "IPC", NULL, 0)) {
		return False;
	}

	res = cli_dfs_get_referral(cli, fullpath, &refs, &num_refs, &consumed);

	if (!cli_tdis(cli)) {
		SAFE_FREE( refs );
		return False;
	}

	cli->cnum = cnum;

	if (!res || !num_refs ) {
		SAFE_FREE( refs );
		return False;
	}
	
	split_dfs_path( refs[0].dfspath, newserver, newshare, newextrapath );

	/* check that this is not a self-referral */

	if ( strequal( cli->desthost, newserver ) && strequal( sharename, newshare ) ) {
		SAFE_FREE( refs );
		return False;
	}
	
	SAFE_FREE( refs );
	
	return True;
}
