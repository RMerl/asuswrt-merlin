/* 
   Unix SMB/CIFS implementation.
   uid/user handling
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

/* what user is current? */
extern struct current_user current_user;

/****************************************************************************
 Become the guest user without changing the security context stack.
****************************************************************************/

bool change_to_guest(void)
{
	struct passwd *pass;

	pass = Get_Pwnam_alloc(talloc_autofree_context(), lp_guestaccount());
	if (!pass) {
		return false;
	}

#ifdef AIX
	/* MWW: From AIX FAQ patch to WU-ftpd: call initgroups before 
	   setting IDs */
	initgroups(pass->pw_name, pass->pw_gid);
#endif

	set_sec_ctx(pass->pw_uid, pass->pw_gid, 0, NULL, NULL);

	current_user.conn = NULL;
	current_user.vuid = UID_FIELD_INVALID;

	TALLOC_FREE(pass);

	return true;
}

/****************************************************************************
 talloc free the conn->server_info if not used in the vuid cache.
****************************************************************************/

static void free_conn_server_info_if_unused(connection_struct *conn)
{
	unsigned int i;

	for (i = 0; i < VUID_CACHE_SIZE; i++) {
		struct vuid_cache_entry *ent;
		ent = &conn->vuid_cache.array[i];
		if (ent->vuid != UID_FIELD_INVALID &&
				conn->server_info == ent->server_info) {
			return;
		}
	}
	/* Not used, safe to free. */
	TALLOC_FREE(conn->server_info);
}

/*******************************************************************
 Check if a username is OK.

 This sets up conn->server_info with a copy related to this vuser that
 later code can then mess with.
********************************************************************/

static bool check_user_ok(connection_struct *conn,
			uint16_t vuid,
			const struct auth_serversupplied_info *server_info,
			int snum)
{
	bool valid_vuid = (vuid != UID_FIELD_INVALID);
	unsigned int i;
	bool readonly_share;
	bool admin_user;

	if (valid_vuid) {
		struct vuid_cache_entry *ent;

		for (i=0; i<VUID_CACHE_SIZE; i++) {
			ent = &conn->vuid_cache.array[i];
			if (ent->vuid == vuid) {
				free_conn_server_info_if_unused(conn);
				conn->server_info = ent->server_info;
				conn->read_only = ent->read_only;
				conn->admin_user = ent->admin_user;
				return(True);
			}
		}
	}

	if (!user_ok_token(server_info->unix_name,
			   pdb_get_domain(server_info->sam_account),
			   server_info->ptok, snum))
		return(False);

	readonly_share = is_share_read_only_for_token(
		server_info->unix_name,
		pdb_get_domain(server_info->sam_account),
		server_info->ptok,
		conn);

	if (!readonly_share &&
	    !share_access_check(server_info->ptok, lp_servicename(snum),
				FILE_WRITE_DATA)) {
		/* smb.conf allows r/w, but the security descriptor denies
		 * write. Fall back to looking at readonly. */
		readonly_share = True;
		DEBUG(5,("falling back to read-only access-evaluation due to "
			 "security descriptor\n"));
	}

	if (!share_access_check(server_info->ptok, lp_servicename(snum),
				readonly_share ?
				FILE_READ_DATA : FILE_WRITE_DATA)) {
		return False;
	}

	admin_user = token_contains_name_in_list(
		server_info->unix_name,
		pdb_get_domain(server_info->sam_account),
		NULL, server_info->ptok, lp_admin_users(snum));

	if (valid_vuid) {
		struct vuid_cache_entry *ent =
			&conn->vuid_cache.array[conn->vuid_cache.next_entry];

		conn->vuid_cache.next_entry =
			(conn->vuid_cache.next_entry + 1) % VUID_CACHE_SIZE;

		TALLOC_FREE(ent->server_info);

		/*
		 * If force_user was set, all server_info's are based on the same
		 * username-based faked one.
		 */

		ent->server_info = copy_serverinfo(
			conn, conn->force_user ? conn->server_info : server_info);

		if (ent->server_info == NULL) {
			ent->vuid = UID_FIELD_INVALID;
			return false;
		}

		ent->vuid = vuid;
		ent->read_only = readonly_share;
		ent->admin_user = admin_user;
		free_conn_server_info_if_unused(conn);
		conn->server_info = ent->server_info;
	}

	conn->read_only = readonly_share;
	conn->admin_user = admin_user;

	return(True);
}

/****************************************************************************
 Clear a vuid out of the connection's vuid cache
 This is only called on SMBulogoff.
****************************************************************************/

void conn_clear_vuid_cache(connection_struct *conn, uint16_t vuid)
{
	int i;

	for (i=0; i<VUID_CACHE_SIZE; i++) {
		struct vuid_cache_entry *ent;

		ent = &conn->vuid_cache.array[i];

		if (ent->vuid == vuid) {
			ent->vuid = UID_FIELD_INVALID;
			/*
			 * We need to keep conn->server_info around
			 * if it's equal to ent->server_info as a SMBulogoff
			 * is often followed by a SMBtdis (with an invalid
			 * vuid). The debug code (or regular code in
			 * vfs_full_audit) wants to refer to the
			 * conn->server_info pointer to print debug
			 * statements. Theoretically this is a bug,
			 * as once the vuid is gone the server_info
			 * on the conn struct isn't valid any more,
			 * but there's enough code that assumes
			 * conn->server_info is never null that
			 * it's easier to hold onto the old pointer
			 * until we get a new sessionsetupX.
			 * As everything is hung off the
			 * conn pointer as a talloc context we're not
			 * leaking memory here. See bug #6315. JRA.
			 */
			if (conn->server_info == ent->server_info) {
				ent->server_info = NULL;
			} else {
				TALLOC_FREE(ent->server_info);
			}
			ent->read_only = False;
			ent->admin_user = False;
		}
	}
}

/****************************************************************************
 Become the user of a connection number without changing the security context
 stack, but modify the current_user entries.
****************************************************************************/

bool change_to_user(connection_struct *conn, uint16 vuid)
{
	const struct auth_serversupplied_info *server_info = NULL;
	struct smbd_server_connection *sconn = smbd_server_conn;
	user_struct *vuser = get_valid_user_struct(sconn, vuid);
	int snum;
	gid_t gid;
	uid_t uid;
	char group_c;
	int num_groups = 0;
	gid_t *group_list = NULL;

	if (!conn) {
		DEBUG(2,("change_to_user: Connection not open\n"));
		return(False);
	}

	/*
	 * We need a separate check in security=share mode due to vuid
	 * always being UID_FIELD_INVALID. If we don't do this then
	 * in share mode security we are *always* changing uid's between
	 * SMB's - this hurts performance - Badly.
	 */

	if((lp_security() == SEC_SHARE) && (current_user.conn == conn) &&
	   (current_user.ut.uid == conn->server_info->utok.uid)) {
		DEBUG(4,("change_to_user: Skipping user change - already "
			 "user\n"));
		return(True);
	} else if ((current_user.conn == conn) && 
		   (vuser != NULL) && (current_user.vuid == vuid) &&
		   (current_user.ut.uid == vuser->server_info->utok.uid)) {
		DEBUG(4,("change_to_user: Skipping user change - already "
			 "user\n"));
		return(True);
	}

	snum = SNUM(conn);

	server_info = vuser ? vuser->server_info : conn->server_info;

	if (!server_info) {
		/* Invalid vuid sent - even with security = share. */
		DEBUG(2,("change_to_user: Invalid vuid %d used on "
			 "share %s.\n",vuid, lp_servicename(snum) ));
		return false;
	}

	if (!check_user_ok(conn, vuid, server_info, snum)) {
		DEBUG(2,("change_to_user: SMB user %s (unix user %s, vuid %d) "
			 "not permitted access to share %s.\n",
			 server_info->sanitized_username,
			 server_info->unix_name, vuid,
			 lp_servicename(snum)));
		return false;
	}

	/*
	 * conn->server_info is now correctly set up with a copy we can mess
	 * with for force_group etc.
	 */

	if (conn->force_user) /* security = share sets this too */ {
		uid = conn->server_info->utok.uid;
		gid = conn->server_info->utok.gid;
	        group_list = conn->server_info->utok.groups;
		num_groups = conn->server_info->utok.ngroups;
	} else if (vuser) {
		uid = conn->admin_user ? 0 : vuser->server_info->utok.uid;
		gid = conn->server_info->utok.gid;
		num_groups = conn->server_info->utok.ngroups;
		group_list  = conn->server_info->utok.groups;
	} else {
		DEBUG(2,("change_to_user: Invalid vuid used %d in accessing "
			 "share %s.\n",vuid, lp_servicename(snum) ));
		return False;
	}

	/*
	 * See if we should force group for this service.
	 * If so this overrides any group set in the force
	 * user code.
	 */

	if((group_c = *lp_force_group(snum))) {

		SMB_ASSERT(conn->force_group_gid != (gid_t)-1);

		if(group_c == '+') {

			/*
			 * Only force group if the user is a member of
			 * the service group. Check the group memberships for
			 * this user (we already have this) to
			 * see if we should force the group.
			 */

			int i;
			for (i = 0; i < num_groups; i++) {
				if (group_list[i]
				    == conn->force_group_gid) {
					conn->server_info->utok.gid =
						conn->force_group_gid;
					gid = conn->force_group_gid;
					gid_to_sid(&conn->server_info->ptok
						   ->user_sids[1], gid);
					break;
				}
			}
		} else {
			conn->server_info->utok.gid = conn->force_group_gid;
			gid = conn->force_group_gid;
			gid_to_sid(&conn->server_info->ptok->user_sids[1],
				   gid);
		}
	}

	/* Now set current_user since we will immediately also call
	   set_sec_ctx() */

	current_user.ut.ngroups = num_groups;
	current_user.ut.groups  = group_list;	

	set_sec_ctx(uid, gid, current_user.ut.ngroups, current_user.ut.groups,
		    conn->server_info->ptok);

	current_user.conn = conn;
	current_user.vuid = vuid;

	DEBUG(5,("change_to_user uid=(%d,%d) gid=(%d,%d)\n",
		 (int)getuid(),(int)geteuid(),(int)getgid(),(int)getegid()));

	return(True);
}

/****************************************************************************
 Go back to being root without changing the security context stack,
 but modify the current_user entries.
****************************************************************************/

bool change_to_root_user(void)
{
	set_root_sec_ctx();

	DEBUG(5,("change_to_root_user: now uid=(%d,%d) gid=(%d,%d)\n",
		(int)getuid(),(int)geteuid(),(int)getgid(),(int)getegid()));

	current_user.conn = NULL;
	current_user.vuid = UID_FIELD_INVALID;

	return(True);
}

/****************************************************************************
 Become the user of an authenticated connected named pipe.
 When this is called we are currently running as the connection
 user. Doesn't modify current_user.
****************************************************************************/

bool become_authenticated_pipe_user(pipes_struct *p)
{
	if (!push_sec_ctx())
		return False;

	set_sec_ctx(p->server_info->utok.uid, p->server_info->utok.gid,
		    p->server_info->utok.ngroups, p->server_info->utok.groups,
		    p->server_info->ptok);

	return True;
}

/****************************************************************************
 Unbecome the user of an authenticated connected named pipe.
 When this is called we are running as the authenticated pipe
 user and need to go back to being the connection user. Doesn't modify
 current_user.
****************************************************************************/

bool unbecome_authenticated_pipe_user(void)
{
	return pop_sec_ctx();
}

/****************************************************************************
 Utility functions used by become_xxx/unbecome_xxx.
****************************************************************************/

static void push_conn_ctx(void)
{
	struct conn_ctx *ctx_p;

	/* Check we don't overflow our stack */

	if (conn_ctx_stack_ndx == MAX_SEC_CTX_DEPTH) {
		DEBUG(0, ("Connection context stack overflow!\n"));
		smb_panic("Connection context stack overflow!\n");
	}

	/* Store previous user context */
	ctx_p = &conn_ctx_stack[conn_ctx_stack_ndx];

	ctx_p->conn = current_user.conn;
	ctx_p->vuid = current_user.vuid;

	DEBUG(3, ("push_conn_ctx(%u) : conn_ctx_stack_ndx = %d\n",
		(unsigned int)ctx_p->vuid, conn_ctx_stack_ndx ));

	conn_ctx_stack_ndx++;
}

static void pop_conn_ctx(void)
{
	struct conn_ctx *ctx_p;

	/* Check for stack underflow. */

	if (conn_ctx_stack_ndx == 0) {
		DEBUG(0, ("Connection context stack underflow!\n"));
		smb_panic("Connection context stack underflow!\n");
	}

	conn_ctx_stack_ndx--;
	ctx_p = &conn_ctx_stack[conn_ctx_stack_ndx];

	current_user.conn = ctx_p->conn;
	current_user.vuid = ctx_p->vuid;

	ctx_p->conn = NULL;
	ctx_p->vuid = UID_FIELD_INVALID;
}

/****************************************************************************
 Temporarily become a root user.  Must match with unbecome_root(). Saves and
 restores the connection context.
****************************************************************************/

void become_root(void)
{
	 /*
	  * no good way to handle push_sec_ctx() failing without changing
	  * the prototype of become_root()
	  */
	if (!push_sec_ctx()) {
		smb_panic("become_root: push_sec_ctx failed");
	}
	push_conn_ctx();
	set_root_sec_ctx();
}

/* Unbecome the root user */

void unbecome_root(void)
{
	pop_sec_ctx();
	pop_conn_ctx();
}

/****************************************************************************
 Push the current security context then force a change via change_to_user().
 Saves and restores the connection context.
****************************************************************************/

bool become_user(connection_struct *conn, uint16 vuid)
{
	if (!push_sec_ctx())
		return False;

	push_conn_ctx();

	if (!change_to_user(conn, vuid)) {
		pop_sec_ctx();
		pop_conn_ctx();
		return False;
	}

	return True;
}

bool unbecome_user(void)
{
	pop_sec_ctx();
	pop_conn_ctx();
	return True;
}
