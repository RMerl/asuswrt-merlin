/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   uid/user handling
   Copyright (C) Andrew Tridgell 1992-1998
   
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

extern int DEBUGLEVEL;

/* what user is current? */
extern struct current_user current_user;

pstring OriginalDir;

/****************************************************************************
 Initialise the uid routines.
****************************************************************************/

void init_uid(void)
{
	current_user.uid = geteuid();
	current_user.gid = getegid();

	if (current_user.gid != 0 && current_user.uid == 0) {
		gain_root_group_privilege();
	}

	current_user.conn = NULL;
	current_user.vuid = UID_FIELD_INVALID;
	
	dos_ChDir(OriginalDir);
}

/****************************************************************************
 Become the specified uid.
****************************************************************************/

static BOOL become_uid(uid_t uid)
{
	if (uid == (uid_t)-1 || ((sizeof(uid_t) == 2) && (uid == (uid_t)65535))) {
		static int done;
		if (!done) {
			DEBUG(1,("WARNING: using uid %d is a security risk\n",(int)uid));
			done=1;
		}
	}

	set_effective_uid(uid);

	current_user.uid = uid;

#ifdef WITH_PROFILE
	profile_p->uid_changes++;
#endif

	return(True);
}


/****************************************************************************
 Become the specified gid.
****************************************************************************/

static BOOL become_gid(gid_t gid)
{
	if (gid == (gid_t)-1 || ((sizeof(gid_t) == 2) && (gid == (gid_t)65535))) {
		DEBUG(1,("WARNING: using gid %d is a security risk\n",(int)gid));    
	}
  
	set_effective_gid(gid);
	
	current_user.gid = gid;
	
	return(True);
}


/****************************************************************************
 Become the specified uid and gid.
****************************************************************************/

static BOOL become_id(uid_t uid,gid_t gid)
{
	return(become_gid(gid) && become_uid(uid));
}

/****************************************************************************
 Become the guest user.
****************************************************************************/

BOOL become_guest(void)
{
  BOOL ret;
  static struct passwd *pass=NULL;

  if (!pass)
    pass = Get_Pwnam(lp_guestaccount(-1),True);
  if (!pass) return(False);

#ifdef AIX
  /* MWW: From AIX FAQ patch to WU-ftpd: call initgroups before setting IDs */
  initgroups(pass->pw_name, (gid_t)pass->pw_gid);
#endif

  ret = become_id(pass->pw_uid,pass->pw_gid);

  if (!ret) {
    DEBUG(1,("Failed to become guest. Invalid guest account?\n"));
  }

  current_user.conn = NULL;
  current_user.vuid = UID_FIELD_INVALID;

  return(ret);
}

/*******************************************************************
 Check if a username is OK.
********************************************************************/

static BOOL check_user_ok(connection_struct *conn, user_struct *vuser,int snum)
{
  int i;
  for (i=0;i<conn->uid_cache.entries;i++)
    if (conn->uid_cache.list[i] == vuser->uid) return(True);

  if (!user_ok(vuser->name,snum)) return(False);

  i = conn->uid_cache.entries % UID_CACHE_SIZE;
  conn->uid_cache.list[i] = vuser->uid;

  if (conn->uid_cache.entries < UID_CACHE_SIZE)
    conn->uid_cache.entries++;

  return(True);
}


/****************************************************************************
 Become the user of a connection number.
****************************************************************************/

BOOL become_user(connection_struct *conn, uint16 vuid)
{
	user_struct *vuser = get_valid_user_struct(vuid);
	int snum;
    gid_t gid;
	uid_t uid;
	char group_c;

	if (!conn) {
		DEBUG(2,("Connection not open\n"));
		return(False);
	}

	/*
	 * We need a separate check in security=share mode due to vuid
	 * always being UID_FIELD_INVALID. If we don't do this then
	 * in share mode security we are *always* changing uid's between
	 * SMB's - this hurts performance - Badly.
	 */

	if((lp_security() == SEC_SHARE) && (current_user.conn == conn) &&
	   (current_user.uid == conn->uid)) {
		DEBUG(4,("Skipping become_user - already user\n"));
		return(True);
	} else if ((current_user.conn == conn) && 
		   (vuser != 0) && (current_user.vuid == vuid) && 
		   (current_user.uid == vuser->uid)) {
		DEBUG(4,("Skipping become_user - already user\n"));
		return(True);
	}

	unbecome_user();

	snum = SNUM(conn);

	if((vuser != NULL) && !check_user_ok(conn, vuser, snum))
		return False;

	if (conn->force_user || 
	    lp_security() == SEC_SHARE ||
	    !(vuser) || (vuser->guest)) {
		uid = conn->uid;
		gid = conn->gid;
		current_user.groups = conn->groups;
		current_user.ngroups = conn->ngroups;
	} else {
		if (!vuser) {
			DEBUG(2,("Invalid vuid used %d\n",vuid));
			return(False);
		}
		uid = vuser->uid;
		gid = vuser->gid;
		current_user.ngroups = vuser->n_groups;
		current_user.groups  = vuser->groups;
	}

	/*
	 * See if we should force group for this service.
	 * If so this overrides any group set in the force
	 * user code.
	 */

	if((group_c = *lp_force_group(snum))) {
		if(group_c == '+') {

			/*
			 * Only force group if the user is a member of
			 * the service group. Check the group memberships for
			 * this user (we already have this) to
			 * see if we should force the group.
			 */

			int i;
			for (i = 0; i < current_user.ngroups; i++) {
				if (current_user.groups[i] == conn->gid) {
					gid = conn->gid;
					break;
				}
			}
		} else {
			gid = conn->gid;
		}
	}
	
	if (!become_gid(gid))
		return(False);

#ifdef HAVE_SETGROUPS      
	if (!(conn && conn->ipc)) {
		/* groups stuff added by ih/wreu */
		if (current_user.ngroups > 0)
			if (sys_setgroups(current_user.ngroups,
				      current_user.groups)<0) {
				DEBUG(0,("sys_setgroups call failed!\n"));
			}
	}
#endif

	if (!conn->admin_user && !become_uid(uid))
		return(False);
	
	current_user.conn = conn;
	current_user.vuid = vuid;

	DEBUG(5,("become_user uid=(%d,%d) gid=(%d,%d)\n",
		 (int)getuid(),(int)geteuid(),(int)getgid(),(int)getegid()));
  
	return(True);
}

/****************************************************************************
 Unbecome the user of a connection number.
****************************************************************************/

BOOL unbecome_user(void )
{
	if (!current_user.conn)
		return(False);

	dos_ChDir(OriginalDir);

	set_effective_uid(0);
	set_effective_gid(0);

	if (geteuid() != 0) {
		DEBUG(0,("Warning: You appear to have a trapdoor uid system\n"));
	}
	if (getegid() != 0) {
		DEBUG(0,("Warning: You appear to have a trapdoor gid system\n"));
	}

	current_user.uid = 0;
	current_user.gid = 0;
  
	if (dos_ChDir(OriginalDir) != 0)
		DEBUG( 0, ( "chdir(%s) failed in unbecome_user\n", OriginalDir ) );

	DEBUG(5,("unbecome_user now uid=(%d,%d) gid=(%d,%d)\n",
		(int)getuid(),(int)geteuid(),(int)getgid(),(int)getegid()));

	current_user.conn = NULL;
	current_user.vuid = UID_FIELD_INVALID;

	return(True);
}

/****************************************************************************
 Become the user of an authenticated connected named pipe.
 When this is called we are currently running as the connection
 user.
****************************************************************************/

BOOL become_authenticated_pipe_user(pipes_struct *p)
{
	/*
	 * Go back to root.
	 */

	if(!unbecome_user())
		return False;

	/*
	 * Now become the authenticated user stored in the pipe struct.
	 */

	if(!become_id(p->uid, p->gid)) {
		/* Go back to the connection user. */
		become_user(p->conn, p->vuid);
		return False;
	}

	return True;	 
}

/****************************************************************************
 Unbecome the user of an authenticated connected named pipe.
 When this is called we are running as the authenticated pipe
 user and need to go back to being the connection user.
****************************************************************************/

BOOL unbecome_authenticated_pipe_user(pipes_struct *p)
{
	if(!become_id(0,0)) {
		DEBUG(0,("unbecome_authenticated_pipe_user: Unable to go back to root.\n"));
		return False;
	}

	return become_user(p->conn, p->vuid);
}

static struct current_user current_user_saved;
static int become_root_depth;
static pstring become_root_dir;

/****************************************************************************
This is used when we need to do a privileged operation (such as mucking
with share mode files) and temporarily need root access to do it. This
call should always be paired with an unbecome_root() call immediately
after the operation

Set save_dir if you also need to save/restore the CWD 
****************************************************************************/

void become_root(BOOL save_dir) 
{
	if (become_root_depth) {
		DEBUG(0,("ERROR: become root depth is non zero\n"));
	}
	if (save_dir)
		dos_GetWd(become_root_dir);

	current_user_saved = current_user;
	become_root_depth = 1;

	become_uid(0);
	become_gid(0);
}

/****************************************************************************
When the privileged operation is over call this

Set save_dir if you also need to save/restore the CWD 
****************************************************************************/

void unbecome_root(BOOL restore_dir)
{
	if (become_root_depth != 1) {
		DEBUG(0,("ERROR: unbecome root depth is %d\n",
			 become_root_depth));
	}

	/* we might have done a become_user() while running as root,
	   if we have then become root again in order to become 
	   non root! */
	if (current_user.uid != 0) {
		become_uid(0);
	}

	/* restore our gid first */
	if (!become_gid(current_user_saved.gid)) {
		DEBUG(0,("ERROR: Failed to restore gid\n"));
		exit_server("Failed to restore gid");
	}

#ifdef HAVE_SETGROUPS      
	if (current_user_saved.ngroups > 0) {
		if (sys_setgroups(current_user_saved.ngroups,
			      current_user_saved.groups)<0)
			DEBUG(0,("ERROR: sys_setgroups call failed!\n"));
	}
#endif

	/* now restore our uid */
	if (!become_uid(current_user_saved.uid)) {
		DEBUG(0,("ERROR: Failed to restore uid\n"));
		exit_server("Failed to restore uid");
	}

	if (restore_dir)
		dos_ChDir(become_root_dir);

	current_user = current_user_saved;

	become_root_depth = 0;
}
