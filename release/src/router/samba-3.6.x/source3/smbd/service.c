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
#include "system/filesys.h"
#include "../lib/tsocket/tsocket.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "../librpc/gen_ndr/netlogon.h"
#include "../libcli/security/security.h"
#include "printing/pcap.h"
#include "passdb/lookup_sid.h"
#include "auth.h"

extern userdom_struct current_user_info;

static bool canonicalize_connect_path(connection_struct *conn)
{
	bool ret;
	char *resolved_name = SMB_VFS_REALPATH(conn,conn->connectpath);
	if (!resolved_name) {
		return false;
	}
	ret = set_conn_connectpath(conn,resolved_name);
	SAFE_FREE(resolved_name);
	return ret;
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
	destname = (char *)SMB_MALLOC(strlen(connectpath) + 2);
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

int find_service(TALLOC_CTX *ctx, const char *service_in, char **p_service_out)
{
	int iService;

	if (!service_in) {
		return -1;
	}

	/* First make a copy. */
	*p_service_out = talloc_strdup(ctx, service_in);
	if (!*p_service_out) {
		return -1;
	}

	all_string_sub(*p_service_out,"\\","/",0);

	iService = lp_servicenumber(*p_service_out);

	/* now handle the special case of a home directory */
	if (iService < 0) {
		char *phome_dir = get_user_home_dir(ctx, *p_service_out);

		if(!phome_dir) {
			/*
			 * Try mapping the servicename, it may
			 * be a Windows to unix mapped user name.
			 */
			if(map_username(ctx, *p_service_out, p_service_out)) {
				if (*p_service_out == NULL) {
					/* Out of memory. */
					return -1;
				}
				phome_dir = get_user_home_dir(
						ctx, *p_service_out);
			}
		}

		DEBUG(3,("checking for home directory %s gave %s\n",*p_service_out,
			phome_dir?phome_dir:"(NULL)"));

		iService = add_home_service(*p_service_out,*p_service_out /* 'username' */, phome_dir);
	}

	/* If we still don't have a service, attempt to add it as a printer. */
	if (iService < 0) {
		int iPrinterService;

		if ((iPrinterService = lp_servicenumber(PRINTERS_NAME)) < 0) {
			iPrinterService = load_registry_service(PRINTERS_NAME);
		}
		if (iPrinterService >= 0) {
			DEBUG(3,("checking whether %s is a valid printer name...\n",
				*p_service_out));
			if (pcap_printername_ok(*p_service_out)) {
				DEBUG(3,("%s is a valid printer name\n",
					*p_service_out));
				DEBUG(3,("adding %s as a printer service\n",
					*p_service_out));
				lp_add_printer(*p_service_out, iPrinterService);
				iService = lp_servicenumber(*p_service_out);
				if (iService < 0) {
					DEBUG(0,("failed to add %s as a printer service!\n",
						*p_service_out));
				}
			} else {
				DEBUG(3,("%s is not a valid printer name\n",
					*p_service_out));
			}
		}
	}

	/* Check for default vfs service?  Unsure whether to implement this */
	if (iService < 0) {
	}

	if (iService < 0) {
		iService = load_registry_service(*p_service_out);
	}

	/* Is it a usershare service ? */
	if (iService < 0 && *lp_usershare_path()) {
		/* Ensure the name is canonicalized. */
		strlower_m(*p_service_out);
		iService = load_usershare_service(*p_service_out);
	}

	/* just possibly it's a default service? */
	if (iService < 0) {
		char *pdefservice = lp_defaultservice();
		if (pdefservice &&
				*pdefservice &&
				!strequal(pdefservice, *p_service_out)
				&& !strstr_m(*p_service_out,"..")) {
			/*
			 * We need to do a local copy here as lp_defaultservice() 
			 * returns one of the rotating lp_string buffers that
			 * could get overwritten by the recursive find_service() call
			 * below. Fix from Josef Hinteregger <joehtg@joehtg.co.at>.
			 */
			char *defservice = talloc_strdup(ctx, pdefservice);

			if (!defservice) {
				goto fail;
			}

			/* Disallow anything except explicit share names. */
			if (strequal(defservice,HOMES_NAME) ||
					strequal(defservice, PRINTERS_NAME) ||
					strequal(defservice, "IPC$")) {
				TALLOC_FREE(defservice);
				goto fail;
			}

			iService = find_service(ctx, defservice, p_service_out);
			if (!*p_service_out) {
				TALLOC_FREE(defservice);
				iService = -1;
				goto fail;
			}
			if (iService >= 0) {
				all_string_sub(*p_service_out, "_","/",0);
				iService = lp_add_service(*p_service_out, iService);
			}
			TALLOC_FREE(defservice);
		}
	}

	if (iService >= 0) {
		if (!VALID_SNUM(iService)) {
			DEBUG(0,("Invalid snum %d for %s\n",iService,
				*p_service_out));
			iService = -1;
		}
	}

  fail:

	if (iService < 0) {
		DEBUG(3,("find_service() failed to find service %s\n",
			*p_service_out));
	}

	return (iService);
}


/****************************************************************************
 do some basic sainity checks on the share.  
 This function modifies dev, ecode.
****************************************************************************/

static NTSTATUS share_sanity_checks(struct client_address *client_id, int snum,
				    fstring dev)
{
	if (!lp_snum_ok(snum) || 
	    !allow_access(lp_hostsdeny(snum), lp_hostsallow(snum),
			  client_id->name, client_id->addr)) {
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
				  struct dom_sid *pgroup_sid,
				  gid_t *pgid)
{
	NTSTATUS result = NT_STATUS_NO_SUCH_GROUP;
	TALLOC_CTX *frame = talloc_stackframe();
	struct dom_sid group_sid;
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

static NTSTATUS create_connection_session_info(struct smbd_server_connection *sconn,
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
					   vuid_serverinfo->info3->base.domain.string,
                                           vuid_serverinfo->security_token, snum)) {
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

		return make_serverinfo_from_username(mem_ctx, user, guest, guest,
						     presult);
        }

	DEBUG(0, ("invalid VUID (vuser) but not in security=share\n"));
	return NT_STATUS_ACCESS_DENIED;
}

/****************************************************************************
  set relavent user and group settings corresponding to force user/group
  configuration for the given snum.
****************************************************************************/

NTSTATUS set_conn_force_user_group(connection_struct *conn, int snum)
{
	NTSTATUS status;

	if (*lp_force_user(snum)) {

		/*
		 * Replace conn->session_info with a completely faked up one
		 * from the username we are forced into :-)
		 */

		char *fuser;
		struct auth_serversupplied_info *forced_serverinfo;

		fuser = talloc_string_sub(conn, lp_force_user(snum), "%S",
					  lp_const_servicename(snum));
		if (fuser == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		status = make_serverinfo_from_username(
			conn, fuser, false, conn->session_info->guest,
			&forced_serverinfo);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		/* We don't want to replace the original sanitized_username
		   as it is the original user given in the connect attempt.
		   This is used in '%U' substitutions. */
		TALLOC_FREE(forced_serverinfo->sanitized_username);
		forced_serverinfo->sanitized_username =
			talloc_move(forced_serverinfo,
				&conn->session_info->sanitized_username);

		TALLOC_FREE(conn->session_info);
		conn->session_info = forced_serverinfo;

		conn->force_user = true;
		DEBUG(3,("Forced user %s\n", fuser));
	}

	/*
	 * If force group is true, then override
	 * any groupid stored for the connecting user.
	 */

	if (*lp_force_group(snum)) {

		status = find_forced_group(
			conn->force_user, snum, conn->session_info->unix_name,
			&conn->session_info->security_token->sids[1],
			&conn->session_info->utok.gid);

		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		/*
		 * We need to cache this gid, to use within
		 * change_to_user() separately from the conn->session_info
		 * struct. We only use conn->session_info directly if
		 * "force_user" was set.
		 */
		conn->force_group_gid = conn->session_info->utok.gid;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
  Setup the share access mask for a connection.
****************************************************************************/

static void create_share_access_mask(connection_struct *conn, int snum)
{
	const struct security_token *token = conn->session_info->security_token;

	share_access_check(token,
			lp_servicename(snum),
			MAXIMUM_ALLOWED_ACCESS,
			&conn->share_access);

	if (security_token_has_privilege(token, SEC_PRIV_SECURITY)) {
		conn->share_access |= SEC_FLAG_SYSTEM_SECURITY;
	}
	if (security_token_has_privilege(token, SEC_PRIV_RESTORE)) {
		conn->share_access |= (SEC_RIGHTS_PRIV_RESTORE);
	}
	if (security_token_has_privilege(token, SEC_PRIV_BACKUP)) {
		conn->share_access |= (SEC_RIGHTS_PRIV_BACKUP);
	}
	if (security_token_has_privilege(token, SEC_PRIV_TAKE_OWNERSHIP)) {
		conn->share_access |= (SEC_STD_WRITE_OWNER);
	}
}

/****************************************************************************
  Make a connection, given the snum to connect to, and the vuser of the
  connecting user if appropriate.
****************************************************************************/

static connection_struct *make_connection_snum(struct smbd_server_connection *sconn,
					connection_struct *conn,
					int snum, user_struct *vuser,
					DATA_BLOB password,
					const char *pdev,
					NTSTATUS *pstatus)
{
	struct smb_filename *smb_fname_cpath = NULL;
	fstring dev;
	int ret;
	bool on_err_call_dis_hook = false;
	bool claimed_connection = false;
	uid_t effuid;
	gid_t effgid;
	NTSTATUS status;

	fstrcpy(dev, pdev);

	*pstatus = share_sanity_checks(&sconn->client_id, snum, dev);
	if (NT_STATUS_IS_ERR(*pstatus)) {
		goto err_root_exit;
	}

	conn->params->service = snum;

	status = create_connection_session_info(sconn,
		conn, snum, vuser ? vuser->session_info : NULL, password,
		&conn->session_info);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("create_connection_session_info failed: %s\n",
			  nt_errstr(status)));
		*pstatus = status;
		goto err_root_exit;
	}

	if ((lp_guest_only(snum)) || (lp_security() == SEC_SHARE)) {
		conn->force_user = true;
	}

	add_session_user(sconn, conn->session_info->unix_name);

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

	status = set_conn_force_user_group(conn, snum);
	if (!NT_STATUS_IS_OK(status)) {
		*pstatus = status;
		return NULL;
	}

	conn->vuid = (vuser != NULL) ? vuser->vuid : UID_FIELD_INVALID;

	{
		char *s = talloc_sub_advanced(talloc_tos(),
					lp_servicename(SNUM(conn)),
					conn->session_info->unix_name,
					conn->connectpath,
					conn->session_info->utok.gid,
					conn->session_info->sanitized_username,
					conn->session_info->info3->base.domain.string,
					lp_pathname(snum));
		if (!s) {
			*pstatus = NT_STATUS_NO_MEMORY;
			goto err_root_exit;
		}

		if (!set_conn_connectpath(conn,s)) {
			TALLOC_FREE(s);
			*pstatus = NT_STATUS_NO_MEMORY;
			goto err_root_exit;
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

	create_share_access_mask(conn, snum);

	if ((conn->share_access & FILE_WRITE_DATA) == 0) {
		if ((conn->share_access & FILE_READ_DATA) == 0) {
			/* No access, read or write. */
			DEBUG(0,("make_connection: connection to %s "
				 "denied due to security "
				 "descriptor.\n",
				 lp_servicename(snum)));
			*pstatus = NT_STATUS_ACCESS_DENIED;
			goto err_root_exit;
		} else {
			conn->read_only = True;
		}
	}
	/* Initialise VFS function pointers */

	if (!smbd_vfs_init(conn)) {
		DEBUG(0, ("vfs_init failed for service %s\n",
			  lp_servicename(snum)));
		*pstatus = NT_STATUS_BAD_NETWORK_NAME;
		goto err_root_exit;
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
		*pstatus = NT_STATUS_INSUFFICIENT_RESOURCES;
		goto err_root_exit;
	}

	/*
	 * Get us an entry in the connections db
	 */
	if (!claim_connection(conn, lp_servicename(snum))) {
		DEBUG(1, ("Could not store connections entry\n"));
		*pstatus = NT_STATUS_INTERNAL_DB_ERROR;
		goto err_root_exit;
	}
	claimed_connection = true;

	/* Invoke VFS make connection hook - this must be the first
	   filesystem operation that we do. */

	if (SMB_VFS_CONNECT(conn, lp_servicename(snum),
			    conn->session_info->unix_name) < 0) {
		DEBUG(0,("make_connection: VFS make connection failed!\n"));
		*pstatus = NT_STATUS_UNSUCCESSFUL;
		goto err_root_exit;
	}

	/* Any error exit after here needs to call the disconnect hook. */
	on_err_call_dis_hook = true;

	if ((!conn->printer) && (!conn->ipc)) {
		conn->notify_ctx = notify_init(conn,
					       sconn_server_id(sconn),
					       sconn->msg_ctx,
					       smbd_event_context(),
					       conn);
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
					conn->session_info->unix_name,
					conn->connectpath,
					conn->session_info->utok.gid,
					conn->session_info->sanitized_username,
					conn->session_info->info3->base.domain.string,
					lp_rootpreexec(snum));
		DEBUG(5,("cmd=%s\n",cmd));
		ret = smbrun(cmd,NULL);
		TALLOC_FREE(cmd);
		if (ret != 0 && lp_rootpreexec_close(snum)) {
			DEBUG(1,("root preexec gave %d - failing "
				 "connection\n", ret));
			*pstatus = NT_STATUS_ACCESS_DENIED;
			goto err_root_exit;
		}
	}

/* USER Activites: */
	if (!change_to_user(conn, conn->vuid)) {
		/* No point continuing if they fail the basic checks */
		DEBUG(0,("Can't become connected user!\n"));
		*pstatus = NT_STATUS_LOGON_FAILURE;
		goto err_root_exit;
	}

	effuid = geteuid();
	effgid = getegid();

	/* Remember that a different vuid can connect later without these
	 * checks... */

	/* Preexecs are done here as they might make the dir we are to ChDir
	 * to below */

	/* execute any "preexec = " line */
	if (*lp_preexec(snum)) {
		char *cmd = talloc_sub_advanced(talloc_tos(),
					lp_servicename(SNUM(conn)),
					conn->session_info->unix_name,
					conn->connectpath,
					conn->session_info->utok.gid,
					conn->session_info->sanitized_username,
					conn->session_info->info3->base.domain.string,
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

#ifdef WITH_FAKE_KASERVER
	if (lp_afs_share(snum)) {
		afs_login(conn);
	}
#endif

	/*
	 * we've finished with the user stuff - go back to root
	 * so the SMB_VFS_STAT call will only fail on path errors,
	 * not permission problems.
	 */
	change_to_root_user();
/* ROOT Activites: */

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
			 conn->sconn->client_id.addr );
		dbgtext( "%s", srv_is_signing_active(sconn) ? "signed " : "");
		dbgtext( "connect to service %s ", lp_servicename(snum) );
		dbgtext( "initially as user %s ",
			 conn->session_info->unix_name );
		dbgtext( "(uid=%d, gid=%d) ", (int)effuid, (int)effgid );
		dbgtext( "(pid %d)\n", (int)sys_getpid() );
	}

	return(conn);

  err_root_exit:
	TALLOC_FREE(smb_fname_cpath);
	/* We must exit this function as root. */
	if (geteuid() != 0) {
		change_to_root_user();
	}
	if (on_err_call_dis_hook) {
		/* Call VFS disconnect hook */
		SMB_VFS_DISCONNECT(conn);
	}
	if (claimed_connection) {
		yield_connection(conn, lp_servicename(snum));
	}
	return NULL;
}

/****************************************************************************
 Make a connection to a service from SMB1. Internal interface.
****************************************************************************/

static connection_struct *make_connection_smb1(struct smbd_server_connection *sconn,
					int snum, user_struct *vuser,
					DATA_BLOB password,
					const char *pdev,
					NTSTATUS *pstatus)
{
	connection_struct *ret_conn = NULL;
	connection_struct *conn = conn_new(sconn);
	if (!conn) {
		DEBUG(0,("make_connection_smb1: Couldn't find free connection.\n"));
		*pstatus = NT_STATUS_INSUFFICIENT_RESOURCES;
		return NULL;
	}
	ret_conn = make_connection_snum(sconn,
					conn,
					snum,
					vuser,
                                        password,
					pdev,
					pstatus);
	if (ret_conn != conn) {
		conn_free(conn);
		return NULL;
	}
	return conn;
}

/****************************************************************************
 Make a connection to a service from SMB2. External SMB2 interface.
 We must set cnum before claiming connection.
****************************************************************************/

connection_struct *make_connection_smb2(struct smbd_server_connection *sconn,
					struct smbd_smb2_tcon *tcon,
					user_struct *vuser,
					DATA_BLOB password,
					const char *pdev,
					NTSTATUS *pstatus)
{
	connection_struct *ret_conn = NULL;
	connection_struct *conn = conn_new(sconn);
	if (!conn) {
		DEBUG(0,("make_connection_smb2: Couldn't find free connection.\n"));
		*pstatus = NT_STATUS_INSUFFICIENT_RESOURCES;
		return NULL;
	}
	conn->cnum = tcon->tid;
	ret_conn = make_connection_snum(sconn,
					conn,
					tcon->snum,
					vuser,
                                        password,
					pdev,
					pstatus);
	if (ret_conn != conn) {
		conn_free(conn);
		return NULL;
	}
	return conn;
}

/****************************************************************************
 Make a connection to a service. External SMB1 interface.
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
	char *service = NULL;
	fstring dev;
	int snum = -1;

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
			return make_connection_smb1(sconn,
						    vuser->homes_snum,
						    vuser, no_pw, 
						    dev, status);
		} else {
			/* Security = share. Try with
			 * current_user_info.smb_name as the username.  */
			if (*current_user_info.smb_name) {
				char *unix_username = NULL;
				(void)map_username(talloc_tos(),
						current_user_info.smb_name,
						&unix_username);
				snum = find_service(talloc_tos(),
						unix_username,
						&unix_username);
				if (!unix_username) {
					*status = NT_STATUS_NO_MEMORY;
				}
				return NULL;
			}
			if (snum != -1) {
				DEBUG(5, ("making a connection to 'homes' "
					  "service %s based on "
					  "security=share\n", service_in));
				return make_connection_smb1(sconn,
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
		return make_connection_smb1(sconn,
					    vuser->homes_snum,
					    vuser, no_pw, 
					    dev, status);
	}

	service = talloc_strdup(talloc_tos(), service_in);
	if (!service) {
		*status = NT_STATUS_NO_MEMORY;
		return NULL;
	}

	strlower_m(service);

	snum = find_service(talloc_tos(), service, &service);
	if (!service) {
		*status = NT_STATUS_NO_MEMORY;
		return NULL;
	}

	if (snum < 0) {
		if (strequal(service,"IPC$") ||
		    (lp_enable_asu_support() && strequal(service,"ADMIN$"))) {
			DEBUG(3,("refusing IPC connection to %s\n", service));
			*status = NT_STATUS_ACCESS_DENIED;
			return NULL;
		}

		DEBUG(3,("%s (%s) couldn't find service %s\n",
			get_remote_machine_name(),
			tsocket_address_string(
				sconn->remote_address, talloc_tos()),
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

	return make_connection_smb1(sconn, snum, vuser,
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
				 conn->sconn->client_id.addr,
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
					conn->session_info->unix_name,
					conn->connectpath,
					conn->session_info->utok.gid,
					conn->session_info->sanitized_username,
					conn->session_info->info3->base.domain.string,
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
					conn->session_info->unix_name,
					conn->connectpath,
					conn->session_info->utok.gid,
					conn->session_info->sanitized_username,
					conn->session_info->info3->base.domain.string,
					lp_rootpostexec(SNUM(conn)));
		smbrun(cmd,NULL);
		TALLOC_FREE(cmd);
	}

	conn_free(conn);
}
