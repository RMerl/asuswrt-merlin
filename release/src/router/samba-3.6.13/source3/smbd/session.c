/* 
   Unix SMB/CIFS implementation.
   session handling for utmp and PAM

   Copyright (C) tridge@samba.org       2001
   Copyright (C) abartlet@samba.org     2001
   Copyright (C) Gerald (Jerry) Carter  2006   
   
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

/* a "session" is claimed when we do a SessionSetupX operation
   and is yielded when the corresponding vuid is destroyed.

   sessions are used to populate utmp and PAM session structures
*/

#include "includes.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "dbwrap.h"
#include "session.h"
#include "auth.h"

/********************************************************************
 called when a session is created
********************************************************************/

bool session_claim(struct smbd_server_connection *sconn, user_struct *vuser)
{
	struct server_id pid = sconn_server_id(sconn);
	TDB_DATA data;
	int i = 0;
	struct sessionid sessionid;
	fstring keystr;
	struct db_record *rec;
	NTSTATUS status;

	vuser->session_keystr = NULL;

	/* don't register sessions for the guest user - its just too
	   expensive to go through pam session code for browsing etc */
	if (vuser->session_info->guest) {
		return True;
	}

	if (!sessionid_init()) {
		return False;
	}

	ZERO_STRUCT(sessionid);

	data.dptr = NULL;
	data.dsize = 0;

	if (lp_utmp()) {

		for (i=1;i<MAX_SESSION_ID;i++) {

			/*
			 * This is very inefficient and needs fixing -- vl
			 */

			struct server_id sess_pid;

			snprintf(keystr, sizeof(keystr), "ID/%d", i);

			rec = sessionid_fetch_record(NULL, keystr);
			if (rec == NULL) {
				DEBUG(1, ("Could not lock \"%s\"\n", keystr));
				return False;
			}

			if (rec->value.dsize != sizeof(sessionid)) {
				DEBUG(1, ("Re-using invalid record\n"));
				break;
			}

			memcpy(&sess_pid,
			       ((char *)rec->value.dptr)
			       + offsetof(struct sessionid, pid),
			       sizeof(sess_pid));

			if (!process_exists(sess_pid)) {
				DEBUG(5, ("%s has died -- re-using session\n",
					  procid_str_static(&sess_pid)));
				break;
			}

			TALLOC_FREE(rec);
		}

		if (i == MAX_SESSION_ID) {
			SMB_ASSERT(rec == NULL);
			DEBUG(1,("session_claim: out of session IDs "
				 "(max is %d)\n", MAX_SESSION_ID));
			return False;
		}

		snprintf(sessionid.id_str, sizeof(sessionid.id_str),
			 SESSION_UTMP_TEMPLATE, i);
	} else
	{
		snprintf(keystr, sizeof(keystr), "ID/%s/%u",
			 procid_str_static(&pid), vuser->vuid);

		rec = sessionid_fetch_record(NULL, keystr);
		if (rec == NULL) {
			DEBUG(1, ("Could not lock \"%s\"\n", keystr));
			return False;
		}

		snprintf(sessionid.id_str, sizeof(sessionid.id_str),
			 SESSION_TEMPLATE, (long unsigned int)sys_getpid(),
			 vuser->vuid);
	}

	SMB_ASSERT(rec != NULL);

	/* If 'hostname lookup' == yes, then do the DNS lookup.  This is
           needed because utmp and PAM both expect DNS names

	   client_name() handles this case internally.
	*/

	fstrcpy(sessionid.username, vuser->session_info->unix_name);
	fstrcpy(sessionid.hostname, sconn->client_id.name);
	sessionid.id_num = i;  /* Only valid for utmp sessions */
	sessionid.pid = pid;
	sessionid.uid = vuser->session_info->utok.uid;
	sessionid.gid = vuser->session_info->utok.gid;
	fstrcpy(sessionid.remote_machine, get_remote_machine_name());
	fstrcpy(sessionid.ip_addr_str, sconn->client_id.addr);
	sessionid.connect_start = time(NULL);

	if (!smb_pam_claim_session(sessionid.username, sessionid.id_str,
				   sessionid.hostname)) {
		DEBUG(1,("pam_session rejected the session for %s [%s]\n",
				sessionid.username, sessionid.id_str));

		TALLOC_FREE(rec);
		return False;
	}

	data.dptr = (uint8 *)&sessionid;
	data.dsize = sizeof(sessionid);

	status = rec->store(rec, data, TDB_REPLACE);

	TALLOC_FREE(rec);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("session_claim: unable to create session id "
			 "record: %s\n", nt_errstr(status)));
		return False;
	}

	if (lp_utmp()) {
		sys_utmp_claim(sessionid.username, sessionid.hostname,
			       sessionid.ip_addr_str,
			       sessionid.id_str, sessionid.id_num);
	}

	vuser->session_keystr = talloc_strdup(vuser, keystr);
	if (!vuser->session_keystr) {
		DEBUG(0, ("session_claim:  talloc_strdup() failed for session_keystr\n"));
		return False;
	}
	return True;
}

/********************************************************************
 called when a session is destroyed
********************************************************************/

void session_yield(user_struct *vuser)
{
	struct sessionid sessionid;
	struct db_record *rec;

	if (!vuser->session_keystr) {
		return;
	}

	rec = sessionid_fetch_record(NULL, vuser->session_keystr);
	if (rec == NULL) {
		return;
	}

	if (rec->value.dsize != sizeof(sessionid))
		return;

	memcpy(&sessionid, rec->value.dptr, sizeof(sessionid));

	if (lp_utmp()) {
		sys_utmp_yield(sessionid.username, sessionid.hostname, 
			       sessionid.ip_addr_str,
			       sessionid.id_str, sessionid.id_num);
	}

	smb_pam_close_session(sessionid.username, sessionid.id_str,
			      sessionid.hostname);

	rec->delete_rec(rec);

	TALLOC_FREE(rec);
}

/********************************************************************
********************************************************************/

struct session_list {
	TALLOC_CTX *mem_ctx;
	int count;
	struct sessionid *sessions;
};

static int gather_sessioninfo(const char *key, struct sessionid *session,
			      void *private_data)
{
	struct session_list *sesslist = (struct session_list *)private_data;

	sesslist->sessions = TALLOC_REALLOC_ARRAY(
		sesslist->mem_ctx, sesslist->sessions, struct sessionid,
		sesslist->count+1);

	if (!sesslist->sessions) {
		sesslist->count = 0;
		return -1;
	}

	memcpy(&sesslist->sessions[sesslist->count], session,
	       sizeof(struct sessionid));

	sesslist->count++;

	DEBUG(7, ("gather_sessioninfo session from %s@%s\n",
		  session->username, session->remote_machine));

	return 0;
}

/********************************************************************
********************************************************************/

int list_sessions(TALLOC_CTX *mem_ctx, struct sessionid **session_list)
{
	struct session_list sesslist;
	int ret;

	sesslist.mem_ctx = mem_ctx;
	sesslist.count = 0;
	sesslist.sessions = NULL;

	ret = sessionid_traverse_read(gather_sessioninfo, (void *) &sesslist);
	if (ret == -1) {
		DEBUG(3, ("Session traverse failed\n"));
		SAFE_FREE(sesslist.sessions);
		*session_list = NULL;
		return 0;
	}

	*session_list = sesslist.sessions;
	return sesslist.count;
}
