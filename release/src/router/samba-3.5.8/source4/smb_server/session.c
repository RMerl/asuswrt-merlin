/* 
   Unix SMB/CIFS implementation.
   Password and authentication handling
   Copyright (C) Andrew Tridgell 1992-2005
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   Copyright (C) Stefan Metzmacher 2005-2006

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
#include "smb_server/smb_server.h"


/*
 * init the sessions structures
 */
NTSTATUS smbsrv_init_sessions(struct smbsrv_connection *smb_conn, uint64_t limit)
{
	/* 
	 * the idr_* functions take 'int' as limit,
	 * and only work with a max limit 0x00FFFFFF
	 */
	limit &= 0x00FFFFFF;

	smb_conn->sessions.idtree_vuid	= idr_init(smb_conn);
	NT_STATUS_HAVE_NO_MEMORY(smb_conn->sessions.idtree_vuid);
	smb_conn->sessions.idtree_limit	= limit;
	smb_conn->sessions.list		= NULL;

	return NT_STATUS_OK;
}

/*
 * Find the session structure assoicated with a VUID
 * (not one from an in-progress session setup)
 */
struct smbsrv_session *smbsrv_session_find(struct smbsrv_connection *smb_conn,
					   uint64_t vuid, struct timeval request_time)
{
	void *p;
	struct smbsrv_session *sess;

	if (vuid == 0) return NULL;

	if (vuid > smb_conn->sessions.idtree_limit) return NULL;

	p = idr_find(smb_conn->sessions.idtree_vuid, vuid);
	if (!p) return NULL;

	/* only return a finished session */
	sess = talloc_get_type(p, struct smbsrv_session);
	if (sess && sess->session_info) {
		sess->statistics.last_request_time = request_time;
		return sess;
	}

	return NULL;
}

/*
 * Find the session structure assoicated with a VUID
 * (assoicated with an in-progress session setup)
 */
struct smbsrv_session *smbsrv_session_find_sesssetup(struct smbsrv_connection *smb_conn, uint64_t vuid)
{
	void *p;
	struct smbsrv_session *sess;

	if (vuid == 0) return NULL;

	if (vuid > smb_conn->sessions.idtree_limit) return NULL;

	p = idr_find(smb_conn->sessions.idtree_vuid, vuid);
	if (!p) return NULL;

	/* only return an unfinished session */
	sess = talloc_get_type(p, struct smbsrv_session);
	if (sess && !sess->session_info) {
		return sess;
	}
	return NULL;
}

/*
 * the session will be marked as valid for usage
 * by attaching a auth_session_info to the session.
 *
 * session_info will be talloc_stealed
 */
NTSTATUS smbsrv_session_sesssetup_finished(struct smbsrv_session *sess,
				           struct auth_session_info *session_info)
{
	/* this check is to catch programmer errors */
	if (!session_info) {
		talloc_free(sess);
		return NT_STATUS_ACCESS_DENIED;
	}

	/* mark the session as successful authenticated */
	sess->session_info = talloc_steal(sess, session_info);

	/* now fill in some statistics */
	sess->statistics.auth_time = timeval_current();

	return NT_STATUS_OK;
}

/****************************************************************************
destroy a session structure
****************************************************************************/
static int smbsrv_session_destructor(struct smbsrv_session *sess)
{
	struct smbsrv_connection *smb_conn = sess->smb_conn;

	idr_remove(smb_conn->sessions.idtree_vuid, sess->vuid);
	DLIST_REMOVE(smb_conn->sessions.list, sess);
	return 0;
}

/*
 * allocate a new session structure with a VUID.
 * gensec_ctx is optional, but talloc_steal'ed when present
 */
struct smbsrv_session *smbsrv_session_new(struct smbsrv_connection *smb_conn,
					  TALLOC_CTX *mem_ctx,
					  struct gensec_security *gensec_ctx)
{
	struct smbsrv_session *sess = NULL;
	int i;

	/* Ensure no vuid gets registered in share level security. */
	if (smb_conn->config.security == SEC_SHARE) return NULL;

	sess = talloc_zero(mem_ctx, struct smbsrv_session);
	if (!sess) return NULL;
	sess->smb_conn = smb_conn;

	i = idr_get_new_random(smb_conn->sessions.idtree_vuid, sess, smb_conn->sessions.idtree_limit);
	if (i == -1) {
		DEBUG(1,("ERROR! Out of connection structures\n"));
		talloc_free(sess);
		return NULL;
	}
	sess->vuid = i;

	/* use this to keep tabs on all our info from the authentication */
	sess->gensec_ctx = talloc_steal(sess, gensec_ctx);

	DLIST_ADD(smb_conn->sessions.list, sess);
	talloc_set_destructor(sess, smbsrv_session_destructor);

	/* now fill in some statistics */
	sess->statistics.connect_time = timeval_current();

	return sess;
}
