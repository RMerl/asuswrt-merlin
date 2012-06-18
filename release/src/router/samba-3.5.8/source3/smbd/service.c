/* 
   Unix SMB/CIFS implementation.
   service (connection) opening and closing
   Copyright (C) Andrew Tridgell 1992-1998
   
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
#include "smbd/globals.h"

extern userdom_struct current_user_info;

static bool canonicalize_connect_path(connection_struct *conn)
{
#ifdef REALPATH_TAKES_NULL
	bool ret;
	char *resolved_name = SMB_VFS_REALPATH(conn,conn->connectpath,NULL);
	if (!resolved_name) {
		return false;
	}
	ret = set_conn_connectpath(conn,resolved_name);
	SAFE_FREE(resolved_name);
	return ret;
#else
        char resolved_name_buf[PATH_MAX+1];
	char *resolved_name = SMB_VFS_REALPATH(conn,conn->connectpath,resolved_name_buf);
	if (!resolved_name) {
		return false;
	}
	return set_conn_connectpath(conn,resolved_name);
#endif /* REALPATH_TAKES_NULL */
}

/****************************************************************************
 Ensure when setting connectpath it is a canonicalized (no ./ // or ../)
 absolute path stating in / and not ending in /.
 Observent people will notice a similarity between this and check_path_syntax :-).
****************************************************************************/

bool set_conn_connectpath(connection_struct *conn, const char *connectpath)
{
	char *destname;
	char *d;
	const char *s = connectpath;
        bool start_of_name_component = true;

	if (connectpath == NULL || connectpath[0] == '\0') {
		return false;
	}

	/* Allocate for strlen + '\0' + possible leading '/' */
	destname = SMB_MALLOC(strlen(connectpath) + 2);
	if (!destname) {
		return false;
	}
	d = destname;

	*d++ = '/'; /* Always start with root. */

	while (*s) {
		if (*s == '/') {
			/* Eat multiple '/' */
			while (*s == '/') {
                                s++;
                        }
			if ((d > destname + 1) && (*s != '\0')) {
				*d++ = '/';
			}
			start_of_name_component = True;
			continue;
		}

		if (start_of_name_component) {
			if ((s[0] == '.') && (s[1] == '.') && (s[2] == '/' || s[2] == '\0')) {
				/* Uh oh - "/../" or "/..\0" ! */

				/* Go past the ../ or .. */
				if (s[2] == '/') {
					s += 3;
				} else {
					s += 2; /* Go past the .. */
				}

				/* If  we just added a '/' - delete it */
				if ((d > destname) && (*(d-1) == '/')) {
					*(d-1) = '\0';
					d--;
				}

				/* Are we at the start ? Can't go back further if so. */
				if (d <= destname) {
					*d++ = '/'; /* Can't delete root */
					continue;
				}
				/* Go back one level... */
				/* Decrement d first as d points to the *next* char to write into. */
				for (d--; d > destname; d--) {
					if (*d == '/') {
						break;
					}
				}
				/* We're still at the start of a name component, just the previous one. */
				continue;
			} else if ((s[0] == '.') && ((s[1] == '\0') || s[1] == '/')) {
				/* Component of pathname can't be "." only - skip the '.' . */
				if (s[1] == '/') {
					s += 2;
				} else {
					s++;
				}
				continue;
			}
		}

		if (!(*s & 0x80)) {
			*d++ = *s++;
		} else {
			size_t siz;
			/* Get the size of the next MB character. */
			next_codepoint(s,&siz);
			switch(siz) {
				case 5:
					*d++ = *s++;
					/*fall through*/
				case 4:
					*d++ = *s++;
					/*fall through*/
				case 3:
					*d++ = *s++;
					/*fall through*/
				case 2:
					*d++ = *s++;
					/*fall through*/
				case 1:
					*d++ = *s++;
					break;
				default:
					break;
			}
		}
		start_of_name_component = false;
	}
	*d = '\0';

	/* And must not end in '/' */
	if (d > destname + 1 && (*(d-1) == '/')) {
		*(d-1) = '\0';
	}

	DEBUG(10,("set_conn_connectpath: service %s, connectpath = %s\n",
		lp_servicename(SNUM(conn)), destname ));

	string_set(&conn->connectpath, destname);
	SAFE_FREE(destname);
	return true;
}

/****************************************************************************
 Load parameters specific to a connection/service.
****************************************************************************/

bool set_current_service(connection_struct *conn, uint16 flags, bool do_chdir)
{
	int snum;

	if (!conn)  {
		last_conn = NULL;
		return(False);
	}

	conn->lastused_count++;

	snum = SNUM(conn);
  
	if (do_chdir &&
	    vfs_ChDir(conn,conn->connectpath) != 0 &&
	    vfs_ChDir(conn,conn->origpath) != 0) {
                DEBUG(((errno!=EACCES)?0:3),("chdir (%s) failed, reason: %s\n",
                         conn->connectpath, strerror(errno)));
		return(False);
	}

	if ((conn == last_conn) && (last_flags == flags)) {
		return(True);
	}

	last_conn = conn;
	last_flags = flags;
	
	/* Obey the client case sensitivity requests - only for clients that support it. */
	switch (lp_casesensitive(snum)) {
		case Auto:
			{
				/* We need this uglyness due to DOS/Win9x clients that lie about case insensitivity. */
				enum remote_arch_types ra_type = get_remote_arch();
				if ((ra_type != RA_SAMBA) && (ra_type != RA_CIFSFS)) {
					/* Client can't support per-packet case sensitive pathnames. */
					conn->case_sensitive = False;
				} else {
					conn->case_sensitive = !(flags & FLAG_CASELESS_PATHNAMES);
				}
			}
			break;
		case True:
			conn->case_sensitive = True;
			break;
		default:
			conn->case_sensitive = False;
			break;
	}
	return(True);
}

static int load_registry_service(const char *servicename)
{
	if (!lp_registry_shares()) {
		return -1;
	}

	if ((servicename == NULL) || (*servicename == '\0')) {
		return -1;
	}

	if (strequal(servicename, GLOBAL_NAME)) {
		return -2;
	}

	if (!process_registry_service(servicename)) {
		return -1;
	}

	return lp_servicenumber(servicename);
}

void load_registry_shares(void)
{
	DEBUG(8, ("load_registry_shares()\n"));
	if (!lp_registry_shares()) {
		return;
	}

	process_registry_shares();

	return;
}

/****************************************************************************
 Add a home service. Returns the new service number or -1 if fail.
****************************************************************************/

int add_home_service(const char *service, const char *username, const char *homedir)
{
	int iHomeService;

	if (!service || !homedir || homedir[0] == '\0')
		return -1;

	if ((iHomeService = lp_servicenumber(HOMES_NAME)) < 0) {
		if ((iHomeService = load_registry_service(HOMES_NAME)) < 0) {
			return -1;
		}
	}

	/*
	 * If this is a winbindd provided username, remove
	 * the domain component before adding the service.
	 * Log a warning if the "path=" parameter does not
	 * include any macros.
	 */

	{
		const char *p = strchr(service,*lp_winbind_separator());

		/* We only want the 'user' part of the string */
		if (p) {
			service = p + 1;
		}
	}

	if (!lp_add_home(service, iHomeService, username, homedir)) {
		return -1;
	}
	
	return lp_servicenumber(service);

}

/**
 * Find a service entry.
 *
 * @param service is modified (to canonical form??)
 **/

int find_service(fstring service)
{
	int iService;
	struct smbd_server_connection *sconn = smbd_server_conn;

	all_string_sub(service,"\\","/",0);

	iService = lp_servicenumber(service);

	/* now handle the special case of a home directory */
	if (iService < 0) {
		char *phome_dir = get_user_home_dir(talloc_tos(), service);

		if(!phome_dir) {
			/*
			 * Try mapping the servicename, it may
			 * be a Windows to unix mapped user name.
			 */
			if(map_username(sconn, service))
				phome_dir = get_user_home_dir(
					talloc_tos(), service);
		}

		DEBUG(3,("checking for home directory %s gave %s\n",service,
			phome_dir?phome_dir:"(NULL)"));

		iService = add_home_service(service,service /* 'username' */, phome_dir);
	}

	/* If we still don't have a service, attempt to add it as a printer. */
	if (iService < 0) {
		int iPrinterService;

		if ((iPrinterService = lp_servicenumber(PRINTERS_NAME)) < 0) {
			iPrinterService = load_registry_service(PRINTERS_NAME);
		}
		if (iPrinterService >= 0) {
			DEBUG(3,("checking whether %s is a valid printer name...\n", service));
			if (pcap_printername_ok(service)) {
				DEBUG(3,("%s is a valid printer name\n", service));
				DEBUG(3,("adding %s as a printer service\n", service));
				lp_add_printer(service, iPrinterService);
				iService = lp_servicenumber(service);
				if (iService < 0) {
					DEBUG(0,("failed to add %s as a printer service!\n", service));
				}
			} else {
				DEBUG(3,("%s is not a valid printer name\n", service));
			}
		}
	}

	/* Check for default vfs service?  Unsure whether to implement this */
	if (iService < 0) {
	}

	if (iService < 0) {
		iService = load_registry_service(service);
	}

	/* Is it a usershare service ? */
	if (iService < 0 && *lp_usershare_path()) {
		/* Ensure the name is canonicalized. */
		strlower_m(service);
		iService = load_usershare_service(service);
	}

	/* just possibly it's a default service? */
	if (iService < 0) {
		char *pdefservice = lp_defaultservice();
		if (pdefservice && *pdefservice && !strequal(pdefservice,service) && !strstr_m(service,"..")) {
			/*
			 * We need to do a local copy here as lp_defaultservice() 
			 * returns one of the rotating lp_string buffers that
			 * could get overwritten by the recursive find_service() call
			 * below. Fix from Josef Hinteregger <joehtg@joehtg.co.at>.
			 */
			char *defservice = SMB_STRDUP(pdefservice);

			if (!defservice) {
				goto fail;
			}

			/* Disallow anything except explicit share names. */
			if (strequal(defservice,HOMES_NAME) ||
					strequal(defservice, PRINTERS_NAME) ||
					strequal(defservice, "IPC$")) {
				SAFE_FREE(defservice);
				goto fail;
			}

			iService = find_service(defservice);
			if (iService >= 0) {
				all_string_sub(service, "_","/",0);
				iService = lp_add_service(service, iService);
			}
			SAFE_FREE(defservice);
		}
	}

	if (iService >= 0) {
		if (!VALID_SNUM(iService)) {
			DEBUG(0,("Invalid snum %d for %s\n",iService, service));
			iService = -1;
		}
	}

  fail:

	if (iService < 0)
		DEBUG(3,("find_service() failed to find service %s\n", service));

	return (iService);
}


/****************************************************************************
 do some basic sainity checks on the share.  
 This function modifies dev, ecode.
****************************************************************************/

static NTSTATUS share_sanity_checks(int snum, fstring dev) 
{
	
	if (!lp_snum_ok(snum) || 
	    !check_access(smbd_server_fd(), 
			  lp_hostsallow(snum), lp_hostsdeny(snum))) {    
		return NT_STATUS_ACCESS_DENIED;
	}

	if (dev[0] == '?' || !dev[0]) {
		if (lp_print_ok(snum)) {
			fstrcpy(dev,"LPT1:");
		} else if (strequal(lp_fstype(snum), "IPC")) {
			fstrcpy(dev, "IPC");
		} else {
			fstrcpy(dev,"A:");
		}
	}

	strupper_m(dev);

	if (lp_print_ok(snum)) {
		if (!strequal(dev, "LPT1:")) {
			return NT_STATUS_BAD_DEVICE_TYPE;
		}
	} else if (strequal(lp_fstype(snum), "IPC")) {
		if (!strequal(dev, "IPC")) {
			return NT_STATUS_BAD_DEVICE_TYPE;
		}
	} else if (!strequal(dev, "A:")) {
		return NT_STATUS_BAD_DEVICE_TYPE;
	}

	/* Behave as a printer if we are supposed to */
	if (lp_print_ok(snum) && (strcmp(dev, "A:") == 0)) {
		fstrcpy(dev, "LPT1:");
	}

	return NT_STATUS_OK;
}

/*
 * Go through lookup_name etc to find the force'd group.  
 *
 * Create a new token from src_token, replacing the primary group sid with the
 * one found.
 */

static NTSTATUS find_forced_group(bool force_user,
				  int snum, const char *username,
				  DOM_SID *pgroup_sid,
				  gid_t *pgid)
{
	NTSTATUS result = NT_STATUS_NO_SUCH_GROUP;
	TALLOC_CTX *frame = talloc_stackframe();
	DOM_SID group_sid;
	enum lsa_SidType type;
	char *groupname;
	bool user_must_be_member = False;
	gid_t gid;

	groupname = talloc_strdup(talloc_tos(), lp_force_group(snum));
	if (groupname == NULL) {
		DEBUG(1, ("talloc_strdup failed\n"));
		result = NT_STATUS_NO_MEMORY;
		goto done;
	}

	if (groupname[0] == '+') {
		user_must_be_member = True;
		groupname += 1;
	}

	groupname = talloc_string_sub(talloc_tos(), groupname,
				      "%S", lp_servicename(snum));
	if (groupname == NULL) {
		DEBUG(1, ("talloc_string_sub failed\n"));
		result = NT_STATUS_NO_MEMORY;
		goto done;
	}

	if (!lookup_name_smbconf(talloc_tos(), groupname,
			 LOOKUP_NAME_ALL|LOOKUP_NAME_GROUP,
			 NULL, NULL, &group_sid, &type)) {
		DEBUG(10, ("lookup_name_smbconf(%s) failed\n",
			   groupname));
		goto done;
	}

	if ((type != SID_NAME_DOM_GRP) && (type != SID_NAME_ALIAS) &&
	    (type != SID_NAME_WKN_GRP)) {
		DEBUG(10, ("%s is a %s, not a group\n", groupname,
			   sid_type_lookup(type)));
		goto done;
	}

	if (!sid_to_gid(&group_sid, &gid)) {
		DEBUG(10, ("sid_to_gid(%s) for %s failed\n",
			   sid_string_dbg(&group_sid), groupname));
		goto done;
	}

	/*
	 * If the user has been forced and the forced group starts with a '+',
	 * then we only set the group to be the forced group if the forced
	 * user is a member of that group.  Otherwise, the meaning of the '+'
	 * would be ignored.
	 */

	if (force_user && user_must_be_member) {
		if (user_in_group_sid(username, &group_sid)) {
			sid_copy(pgroup_sid, &group_sid);
			*pgid = gid;
			DEBUG(3,("Forced group %s for member %s\n",
				 groupname, username));
		} else {
			DEBUG(0,("find_forced_group: forced user %s is not a member "
				"of forced group %s. Disallowing access.\n",
				username, groupname ));
			result = NT_STATUS_MEMBER_NOT_IN_GROUP;
			goto done;
		}
	} else {
		sid_copy(pgroup_sid, &group_sid);
		*pgid = gid;
		DEBUG(3,("Forced group %s\n", groupname));
	}

	result = NT_STATUS_OK;
 done:
	TALLOC_FREE(frame);
	return result;
}

/****************************************************************************
  Create an auth_serversupplied_info structure for a connection_struct
****************************************************************************/

static NTSTATUS create_connection_server_info(struct smbd_server_connection *sconn,
					      TALLOC_CTX *mem_ctx, int snum,
                                              struct auth_serversupplied_info *vuid_serverinfo,
					      DATA_BLOB password,
                                              struct auth_serversupplied_info **presult)
{
        if (lp_guest_only(snum)) {
                return make_server_info_guest(mem_ctx, presult);
        }

        if (vuid_serverinfo != NULL) {

		struct auth_serversupplied_info *result;

                /*
                 * This is the normal security != share case where we have a
                 * valid vuid from the session setup.                 */

                if (vuid_serverinfo->guest) {
                        if (!lp_guest_ok(snum)) {
                                DEBUG(2, ("guest user (from session setup) "
                                          "not permitted to access this share "
                                          "(%s)\n", lp_servicename(snum)));
                                return NT_STATUS_ACCESS_DENIED;
                        }
                } else {
                        if (!user_ok_token(vuid_serverinfo->unix_name,
					   pdb_get_domain(vuid_serverinfo->sam_account),
                                           vuid_serverinfo->ptok, snum)) {
                                DEBUG(2, ("user '%s' (from session setup) not "
                                          "permitted to access this share "
                                          "(%s)\n",
                                          vuid_serverinfo->unix_name,
                                          lp_servicename(snum)));
                                return NT_STATUS_ACCESS_DENIED;
                        }
                }

                result = copy_serverinfo(mem_ctx, vuid_serverinfo);
		if (result == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		*presult = result;
		return NT_STATUS_OK;
        }

        if (lp_security() == SEC_SHARE) {

                fstring user;
		bool guest;

                /* add the sharename as a possible user name if we
                   are in share mode security */

                add_session_user(sconn, lp_servicename(snum));

                /* shall we let them in? */

                if (!authorise_login(sconn, snum,user,password,&guest)) {
                        DEBUG( 2, ( "Invalid username/password for [%s]\n",
                                    lp_servicename(snum)) );
			return NT_STATUS_WRONG_PASSWORD;
                }

		return make_serverinfo_from_username(mem_ctx, user, guest,
						     presult);
        }

	DEBUG(0, ("invalid VUID (vuser) but not in security=share\n"));
	return NT_STATUS_ACCESS_DENIED;
}


/****************************************************************************
  Make a connection, given the snum to connect to, and the vuser of the
  connecting user if appropriate.
****************************************************************************/

connection_struct *make_connection_snum(struct smbd_server_connection *sconn,
					int snum, user_struct *vuser,
					DATA_BLOB password,
					const char *pdev,
					NTSTATUS *pstatus)
{
	connection_struct *conn;
	struct smb_filename *smb_fname_cpath = NULL;
	fstring dev;
	int ret;
	char addr[INET6_ADDRSTRLEN];
	NTSTATUS status;

	fstrcpy(dev, pdev);

	if (NT_STATUS_IS_ERR(*pstatus = share_sanity_checks(snum, dev))) {
		return NULL;
	}	

	conn = conn_new(sconn);
	if (!conn) {
		DEBUG(0,("Couldn't find free connection.\n"));
		*pstatus = NT_STATUS_INSUFFICIENT_RESOURCES;
		return NULL;
	}

	conn->params->service = snum;

	status = create_connection_server_info(sconn,
		conn, snum, vuser ? vuser->server_info : NULL, password,
		&conn->server_info);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("create_connection_server_info failed: %s\n",
			  nt_errstr(status)));
		*pstatus = status;
		conn_free(conn);
		return NULL;
	}

	if ((lp_guest_only(snum)) || (lp_security() == SEC_SHARE)) {
		conn->force_user = true;
	}

	add_session_user(sconn, conn->server_info->unix_name);

	safe_strcpy(conn->client_address,
			client_addr(get_client_fd(),addr,sizeof(addr)), 
			sizeof(conn->client_address)-1);
	conn->num_files_open = 0;
	conn->lastused = conn->lastused_count = time(NULL);
	conn->used = True;
	conn->printer = (strncmp(dev,"LPT",3) == 0);
	conn->ipc = ( (strncmp(dev,"IPC",3) == 0) ||
		      ( lp_enable_asu_support() && strequal(dev,"ADMIN$")) );

	/* Case options for the share. */
	if (lp_casesensitive(snum) == Auto) {
		/* We will be setting this per packet. Set to be case
		 * insensitive for now. */
		conn->case_sensitive = False;
	} else {
		conn->case_sensitive = (bool)lp_casesensitive(snum);
	}

	conn->case_preserve = lp_preservecase(snum);
	conn->short_case_preserve = lp_shortpreservecase(snum);

	conn->encrypt_level = lp_smb_encrypt(snum);

	conn->veto_list = NULL;
	conn->hide_list = NULL;
	conn->veto_oplock_list = NULL;
	conn->aio_write_behind_list = NULL;

	conn->read_only = lp_readonly(SNUM(conn));
	conn->admin_user = False;

	if (*lp_force_user(snum)) {

		/*
		 * Replace conn->server_info with a completely faked up one
		 * from the username we are forced into :-)
		 */

		char *fuser;
		struct auth_serversupplied_info *forced_serverinfo;

		fuser = talloc_string_sub(conn, lp_force_user(snum), "%S",
					  lp_servicename(snum));
		if (fuser == NULL) {
			conn_free(conn);
			*pstatus = NT_STATUS_NO_MEMORY;
			return NULL;
		}

		status = make_serverinfo_from_username(
			conn, fuser, conn->server_info->guest,
			&forced_serverinfo);
		if (!NT_STATUS_IS_OK(status)) {
			conn_free(conn);
			*pstatus = status;
			return NULL;
		}

		TALLOC_FREE(conn->server_info);
		conn->server_info = forced_serverinfo;

		conn->force_user = True;
		DEBUG(3,("Forced user %s\n", fuser));
	}

	/*
	 * If force group is true, then override
	 * any groupid stored for the connecting user.
	 */

	if (*lp_force_group(snum)) {

		status = find_forced_group(
			conn->force_user, snum, conn->server_info->unix_name,
			&conn->server_info->ptok->user_sids[1],
			&conn->server_info->utok.gid);

		if (!NT_STATUS_IS_OK(status)) {
			conn_free(conn);
			*pstatus = status;
			return NULL;
		}

		/*
		 * We need to cache this gid, to use within
 		 * change_to_user() separately from the conn->server_info
 		 * struct. We only use conn->server_info directly if
 		 * "force_user" was set.
 		 */
		conn->force_group_gid = conn->server_info->utok.gid;
	}

	conn->vuid = (vuser != NULL) ? vuser->vuid : UID_FIELD_INVALID;

	{
		char *s = talloc_sub_advanced(talloc_tos(),
					lp_servicename(SNUM(conn)),
					conn->server_info->unix_name,
					conn->connectpath,
					conn->server_info->utok.gid,
					conn->server_info->sanitized_username,
					pdb_get_domain(conn->server_info->sam_account),
					lp_pathname(snum));
		if (!s) {
			conn_free(conn);
			*pstatus = NT_STATUS_NO_MEMORY;
			return NULL;
		}

		if (!set_conn_connectpath(conn,s)) {
			TALLOC_FREE(s);
			conn_free(conn);
			*pstatus = NT_STATUS_NO_MEMORY;
			return NULL;
		}
		DEBUG(3,("Connect path is '%s' for service [%s]\n",s,
			 lp_servicename(snum)));
		TALLOC_FREE(s);
	}

	/*
	 * New code to check if there's a share security descripter
	 * added from NT server manager. This is done after the
	 * smb.conf checks are done as we need a uid and token. JRA.
	 *
	 */

	{
		bool can_write = False;

		can_write = share_access_check(conn->server_info->ptok,
					       lp_servicename(snum),
					       FILE_WRITE_DATA);

		if (!can_write) {
			if (!share_access_check(conn->server_info->ptok,
						lp_servicename(snum),
						FILE_READ_DATA)) {
				/* No access, read or write. */
				DEBUG(0,("make_connection: connection to %s "
					 "denied due to security "
					 "descriptor.\n",
					  lp_servicename(snum)));
				conn_free(conn);
				*pstatus = NT_STATUS_ACCESS_DENIED;
				return NULL;
			} else {
				conn->read_only = True;
			}
		}
	}
	/* Initialise VFS function pointers */

	if (!smbd_vfs_init(conn)) {
		DEBUG(0, ("vfs_init failed for service %s\n",
			  lp_servicename(snum)));
		conn_free(conn);
		*pstatus = NT_STATUS_BAD_NETWORK_NAME;
		return NULL;
	}

	if ((!conn->printer) && (!conn->ipc)) {
		conn->notify_ctx = notify_init(conn, server_id_self(),
					       smbd_messaging_context(),
					       smbd_event_context(),
					       conn);
	}

/* ROOT Activities: */
	/* explicitly check widelinks here so that we can correctly warn
	 * in the logs. */
	widelinks_warning(snum);

	/*
	 * Enforce the max connections parameter.
	 */

	if ((lp_max_connections(snum) > 0)
	    && (count_current_connections(lp_servicename(SNUM(conn)), True) >=
		lp_max_connections(snum))) {

		DEBUG(1, ("Max connections (%d) exceeded for %s\n",
			  lp_max_connections(snum), lp_servicename(snum)));
		conn_free(conn);
		*pstatus = NT_STATUS_INSUFFICIENT_RESOURCES;
		return NULL;
	}  

	/*
	 * Get us an entry in the connections db
	 */
	if (!claim_connection(conn, lp_servicename(snum), 0)) {
		DEBUG(1, ("Could not store connections entry\n"));
		conn_free(conn);
		*pstatus = NT_STATUS_INTERNAL_DB_ERROR;
		return NULL;
	}  

	/* Invoke VFS make connection hook - must be the first
	   VFS operation we do. */

	if (SMB_VFS_CONNECT(conn, lp_servicename(snum),
			    conn->server_info->unix_name) < 0) {
		DEBUG(0,("make_connection: VFS make connection failed!\n"));
		yield_connection(conn, lp_servicename(snum));
		conn_free(conn);
		*pstatus = NT_STATUS_UNSUCCESSFUL;
		return NULL;
	}

	/*
	 * Fix compatibility issue pointed out by Volker.
	 * We pass the conn->connectpath to the preexec
	 * scripts as a parameter, so attempt to canonicalize
	 * it here before calling the preexec scripts.
	 * We ignore errors here, as it is possible that
	 * the conn->connectpath doesn't exist yet and
	 * the preexec scripts will create them.
	 */

	(void)canonicalize_connect_path(conn);

	/* Preexecs are done here as they might make the dir we are to ChDir
	 * to below */
	/* execute any "root preexec = " line */
	if (*lp_rootpreexec(snum)) {
		char *cmd = talloc_sub_advanced(talloc_tos(),
					lp_servicename(SNUM(conn)),
					conn->server_info->unix_name,
					conn->connectpath,
					conn->server_info->utok.gid,
					conn->server_info->sanitized_username,
					pdb_get_domain(conn->server_info->sam_account),
					lp_rootpreexec(snum));
		DEBUG(5,("cmd=%s\n",cmd));
		ret = smbrun(cmd,NULL);
		TALLOC_FREE(cmd);
		if (ret != 0 && lp_rootpreexec_close(snum)) {
			DEBUG(1,("root preexec gave %d - failing "
				 "connection\n", ret));
			SMB_VFS_DISCONNECT(conn);
			yield_connection(conn, lp_servicename(snum));
			conn_free(conn);
			*pstatus = NT_STATUS_ACCESS_DENIED;
			return NULL;
		}
	}

/* USER Activites: */
	if (!change_to_user(conn, conn->vuid)) {
		/* No point continuing if they fail the basic checks */
		DEBUG(0,("Can't become connected user!\n"));
		SMB_VFS_DISCONNECT(conn);
		yield_connection(conn, lp_servicename(snum));
		conn_free(conn);
		*pstatus = NT_STATUS_LOGON_FAILURE;
		return NULL;
	}

	/* Remember that a different vuid can connect later without these
	 * checks... */
	
	/* Preexecs are done here as they might make the dir we are to ChDir
	 * to below */

	/* execute any "preexec = " line */
	if (*lp_preexec(snum)) {
		char *cmd = talloc_sub_advanced(talloc_tos(),
					lp_servicename(SNUM(conn)),
					conn->server_info->unix_name,
					conn->connectpath,
					conn->server_info->utok.gid,
					conn->server_info->sanitized_username,
					pdb_get_domain(conn->server_info->sam_account),
					lp_preexec(snum));
		ret = smbrun(cmd,NULL);
		TALLOC_FREE(cmd);
		if (ret != 0 && lp_preexec_close(snum)) {
			DEBUG(1,("preexec gave %d - failing connection\n",
				 ret));
			*pstatus = NT_STATUS_ACCESS_DENIED;
			goto err_root_exit;
		}
	}

	/*
	 * If widelinks are disallowed we need to canonicalise the connect
	 * path here to ensure we don't have any symlinks in the
	 * connectpath. We will be checking all paths on this connection are
	 * below this directory. We must do this after the VFS init as we
	 * depend on the realpath() pointer in the vfs table. JRA.
	 */
	if (!lp_widelinks(snum)) {
		if (!canonicalize_connect_path(conn)) {
			DEBUG(0, ("canonicalize_connect_path failed "
			"for service %s, path %s\n",
				lp_servicename(snum),
				conn->connectpath));
			*pstatus = NT_STATUS_BAD_NETWORK_NAME;
			goto err_root_exit;
		}
	}

#ifdef WITH_FAKE_KASERVER
	if (lp_afs_share(snum)) {
		afs_login(conn);
	}
#endif
	
	/* Add veto/hide lists */
	if (!IS_IPC(conn) && !IS_PRINT(conn)) {
		set_namearray( &conn->veto_list, lp_veto_files(snum));
		set_namearray( &conn->hide_list, lp_hide_files(snum));
		set_namearray( &conn->veto_oplock_list, lp_veto_oplocks(snum));
		set_namearray( &conn->aio_write_behind_list,
				lp_aio_write_behind(snum));
	}
	
	status = create_synthetic_smb_fname(talloc_tos(), conn->connectpath,
					    NULL, NULL, &smb_fname_cpath);
	if (!NT_STATUS_IS_OK(status)) {
		*pstatus = status;
		goto err_root_exit;
	}

	/* win2000 does not check the permissions on the directory
	   during the tree connect, instead relying on permission
	   check during individual operations. To match this behaviour
	   I have disabled this chdir check (tridge) */
	/* the alternative is just to check the directory exists */
	if ((ret = SMB_VFS_STAT(conn, smb_fname_cpath)) != 0 ||
	    !S_ISDIR(smb_fname_cpath->st.st_ex_mode)) {
		if (ret == 0 && !S_ISDIR(smb_fname_cpath->st.st_ex_mode)) {
			DEBUG(0,("'%s' is not a directory, when connecting to "
				 "[%s]\n", conn->connectpath,
				 lp_servicename(snum)));
		} else {
			DEBUG(0,("'%s' does not exist or permission denied "
				 "when connecting to [%s] Error was %s\n",
				 conn->connectpath, lp_servicename(snum),
				 strerror(errno) ));
		}
		*pstatus = NT_STATUS_BAD_NETWORK_NAME;
		goto err_root_exit;
	}
	conn->base_share_dev = smb_fname_cpath->st.st_ex_dev;

	string_set(&conn->origpath,conn->connectpath);

#if SOFTLINK_OPTIMISATION
	/* resolve any soft links early if possible */
	if (vfs_ChDir(conn,conn->connectpath) == 0) {
		TALLOC_CTX *ctx = talloc_tos();
		char *s = vfs_GetWd(ctx,s);
		if (!s) {
			*status = map_nt_error_from_unix(errno);
			goto err_root_exit;
		}
		if (!set_conn_connectpath(conn,s)) {
			*status = NT_STATUS_NO_MEMORY;
			goto err_root_exit;
		}
		vfs_ChDir(conn,conn->connectpath);
	}
#endif

	/* Figure out the characteristics of the underlying filesystem. This
	 * assumes that all the filesystem mounted withing a share path have
	 * the same characteristics, which is likely but not guaranteed.
	 */

	conn->fs_capabilities = SMB_VFS_FS_CAPABILITIES(conn, &conn->ts_res);

	/*
	 * Print out the 'connected as' stuff here as we need
	 * to know the effective uid and gid we will be using
	 * (at least initially).
	 */

	if( DEBUGLVL( IS_IPC(conn) ? 3 : 1 ) ) {
		dbgtext( "%s (%s) ", get_remote_machine_name(),
			 conn->client_address );
		dbgtext( "%s", srv_is_signing_active(smbd_server_conn) ? "signed " : "");
		dbgtext( "connect to service %s ", lp_servicename(snum) );
		dbgtext( "initially as user %s ",
			 conn->server_info->unix_name );
		dbgtext( "(uid=%d, gid=%d) ", (int)geteuid(), (int)getegid() );
		dbgtext( "(pid %d)\n", (int)sys_getpid() );
	}

	/* we've finished with the user stuff - go back to root */
	change_to_root_user();
	return(conn);

  err_root_exit:
	TALLOC_FREE(smb_fname_cpath);
	change_to_root_user();
	/* Call VFS disconnect hook */
	SMB_VFS_DISCONNECT(conn);
	yield_connection(conn, lp_servicename(snum));
	conn_free(conn);
	return NULL;
}

/****************************************************************************
 Make a connection to a service.
 *
 * @param service 
****************************************************************************/

connection_struct *make_connection(struct smbd_server_connection *sconn,
				   const char *service_in, DATA_BLOB password,
				   const char *pdev, uint16 vuid,
				   NTSTATUS *status)
{
	uid_t euid;
	user_struct *vuser = NULL;
	fstring service;
	fstring dev;
	int snum = -1;
	char addr[INET6_ADDRSTRLEN];

	fstrcpy(dev, pdev);

	/* This must ONLY BE CALLED AS ROOT. As it exits this function as
	 * root. */
	if (!non_root_mode() && (euid = geteuid()) != 0) {
		DEBUG(0,("make_connection: PANIC ERROR. Called as nonroot "
			 "(%u)\n", (unsigned int)euid ));
		smb_panic("make_connection: PANIC ERROR. Called as nonroot\n");
	}

	if (conn_num_open(sconn) > 2047) {
		*status = NT_STATUS_INSUFF_SERVER_RESOURCES;
		return NULL;
	}

	if(lp_security() != SEC_SHARE) {
		vuser = get_valid_user_struct(sconn, vuid);
		if (!vuser) {
			DEBUG(1,("make_connection: refusing to connect with "
				 "no session setup\n"));
			*status = NT_STATUS_ACCESS_DENIED;
			return NULL;
		}
	}

	/* Logic to try and connect to the correct [homes] share, preferably
	   without too many getpwnam() lookups.  This is particulary nasty for
	   winbind usernames, where the share name isn't the same as unix
	   username.

	   The snum of the homes share is stored on the vuser at session setup
	   time.
	*/

	if (strequal(service_in,HOMES_NAME)) {
		if(lp_security() != SEC_SHARE) {
			DATA_BLOB no_pw = data_blob_null;
			if (vuser->homes_snum == -1) {
				DEBUG(2, ("[homes] share not available for "
					  "this user because it was not found "
					  "or created at session setup "
					  "time\n"));
				*status = NT_STATUS_BAD_NETWORK_NAME;
				return NULL;
			}
			DEBUG(5, ("making a connection to [homes] service "
				  "created at session setup time\n"));
			return make_connection_snum(sconn,
						    vuser->homes_snum,
						    vuser, no_pw, 
						    dev, status);
		} else {
			/* Security = share. Try with
			 * current_user_info.smb_name as the username.  */
			if (*current_user_info.smb_name) {
				fstring unix_username;
				fstrcpy(unix_username,
					current_user_info.smb_name);
				map_username(sconn, unix_username);
				snum = find_service(unix_username);
			} 
			if (snum != -1) {
				DEBUG(5, ("making a connection to 'homes' "
					  "service %s based on "
					  "security=share\n", service_in));
				return make_connection_snum(sconn,
							    snum, NULL,
							    password,
							    dev, status);
			}
		}
	} else if ((lp_security() != SEC_SHARE) && (vuser->homes_snum != -1)
		   && strequal(service_in,
			       lp_servicename(vuser->homes_snum))) {
		DATA_BLOB no_pw = data_blob_null;
		DEBUG(5, ("making a connection to 'homes' service [%s] "
			  "created at session setup time\n", service_in));
		return make_connection_snum(sconn,
					    vuser->homes_snum,
					    vuser, no_pw, 
					    dev, status);
	}
	
	fstrcpy(service, service_in);

	strlower_m(service);

	snum = find_service(service);

	if (snum < 0) {
		if (strequal(service,"IPC$") ||
		    (lp_enable_asu_support() && strequal(service,"ADMIN$"))) {
			DEBUG(3,("refusing IPC connection to %s\n", service));
			*status = NT_STATUS_ACCESS_DENIED;
			return NULL;
		}

		DEBUG(3,("%s (%s) couldn't find service %s\n",
			get_remote_machine_name(),
			client_addr(get_client_fd(),addr,sizeof(addr)),
			service));
		*status = NT_STATUS_BAD_NETWORK_NAME;
		return NULL;
	}

	/* Handle non-Dfs clients attempting connections to msdfs proxy */
	if (lp_host_msdfs() && (*lp_msdfs_proxy(snum) != '\0'))  {
		DEBUG(3, ("refusing connection to dfs proxy share '%s' "
			  "(pointing to %s)\n", 
			service, lp_msdfs_proxy(snum)));
		*status = NT_STATUS_BAD_NETWORK_NAME;
		return NULL;
	}

	DEBUG(5, ("making a connection to 'normal' service %s\n", service));

	return make_connection_snum(sconn, snum, vuser,
				    password,
				    dev, status);
}

/****************************************************************************
 Close a cnum.
****************************************************************************/

void close_cnum(connection_struct *conn, uint16 vuid)
{
	file_close_conn(conn);

	if (!IS_IPC(conn)) {
		dptr_closecnum(conn);
	}

	change_to_root_user();

	DEBUG(IS_IPC(conn)?3:1, ("%s (%s) closed connection to service %s\n",
				 get_remote_machine_name(),
				 conn->client_address,
				 lp_servicename(SNUM(conn))));

	/* Call VFS disconnect hook */    
	SMB_VFS_DISCONNECT(conn);

	yield_connection(conn, lp_servicename(SNUM(conn)));

	/* make sure we leave the directory available for unmount */
	vfs_ChDir(conn, "/");

	/* execute any "postexec = " line */
	if (*lp_postexec(SNUM(conn)) && 
	    change_to_user(conn, vuid))  {
		char *cmd = talloc_sub_advanced(talloc_tos(),
					lp_servicename(SNUM(conn)),
					conn->server_info->unix_name,
					conn->connectpath,
					conn->server_info->utok.gid,
					conn->server_info->sanitized_username,
					pdb_get_domain(conn->server_info->sam_account),
					lp_postexec(SNUM(conn)));
		smbrun(cmd,NULL);
		TALLOC_FREE(cmd);
		change_to_root_user();
	}

	change_to_root_user();
	/* execute any "root postexec = " line */
	if (*lp_rootpostexec(SNUM(conn)))  {
		char *cmd = talloc_sub_advanced(talloc_tos(),
					lp_servicename(SNUM(conn)),
					conn->server_info->unix_name,
					conn->connectpath,
					conn->server_info->utok.gid,
					conn->server_info->sanitized_username,
					pdb_get_domain(conn->server_info->sam_account),
					lp_rootpostexec(SNUM(conn)));
		smbrun(cmd,NULL);
		TALLOC_FREE(cmd);
	}

	conn_free(conn);
}
