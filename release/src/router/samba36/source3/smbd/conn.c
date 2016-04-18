/*
   Unix SMB/CIFS implementation.
   Manage connections_struct structures
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Alexander Bokovoy 2002
   Copyright (C) Jeremy Allison 2010

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
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "rpc_server/rpc_ncacn_np.h"
#include "lib/util/bitmap.h"

/* The connections bitmap is expanded in increments of BITMAP_BLOCK_SZ. The
 * maximum size of the bitmap is the largest positive integer, but you will hit
 * the "max connections" limit, looong before that.
 */

#define BITMAP_BLOCK_SZ 128

/****************************************************************************
 Init the conn structures.
****************************************************************************/

void conn_init(struct smbd_server_connection *sconn)
{
	sconn->smb1.tcons.Connections = NULL;
	sconn->smb1.tcons.bmap = bitmap_talloc(sconn, BITMAP_BLOCK_SZ);
}

/****************************************************************************
 Return the number of open connections.
****************************************************************************/

int conn_num_open(struct smbd_server_connection *sconn)
{
	return sconn->num_tcons_open;
}

/****************************************************************************
 Check if a snum is in use.
****************************************************************************/

bool conn_snum_used(int snum)
{
	struct smbd_server_connection *sconn = smbd_server_conn;

	if (sconn->using_smb2) {
		/* SMB2 */
		struct smbd_smb2_session *sess;
		for (sess = sconn->smb2.sessions.list; sess; sess = sess->next) {
			struct smbd_smb2_tcon *ptcon;

			for (ptcon = sess->tcons.list; ptcon; ptcon = ptcon->next) {
				if (ptcon->compat_conn &&
						ptcon->compat_conn->params &&
						(ptcon->compat_conn->params->service == snum)) {
					return true;
				}
			}
		}
	} else {
		/* SMB1 */
		connection_struct *conn;
		for (conn=sconn->smb1.tcons.Connections;conn;conn=conn->next) {
			if (conn->params->service == snum) {
				return true;
			}
		}
	}
	return false;
}

/****************************************************************************
 Find a conn given a cnum.
****************************************************************************/

connection_struct *conn_find(struct smbd_server_connection *sconn,unsigned cnum)
{
	if (sconn->using_smb2) {
		/* SMB2 */
		struct smbd_smb2_session *sess;
		for (sess = sconn->smb2.sessions.list; sess; sess = sess->next) {
			struct smbd_smb2_tcon *ptcon;

			for (ptcon = sess->tcons.list; ptcon; ptcon = ptcon->next) {
				if (ptcon->compat_conn &&
						ptcon->compat_conn->cnum == cnum) {
					return ptcon->compat_conn;
				}
			}
		}
	} else {
		/* SMB1 */
		int count=0;
		connection_struct *conn;
		for (conn=sconn->smb1.tcons.Connections;conn;conn=conn->next,count++) {
			if (conn->cnum == cnum) {
				if (count > 10) {
					DLIST_PROMOTE(sconn->smb1.tcons.Connections,
						conn);
				}
				return conn;
			}
		}
	}

	return NULL;
}

/****************************************************************************
 Find first available connection slot, starting from a random position.
 The randomisation stops problems with the server dieing and clients
 thinking the server is still available.
****************************************************************************/

connection_struct *conn_new(struct smbd_server_connection *sconn)
{
	connection_struct *conn;
	int i;
        int find_offset = 1;

	if (sconn->using_smb2) {
		/* SMB2 */
		if (!(conn=TALLOC_ZERO_P(NULL, connection_struct)) ||
		    !(conn->params = TALLOC_P(conn, struct share_params))) {
			DEBUG(0,("TALLOC_ZERO() failed!\n"));
			TALLOC_FREE(conn);
			return NULL;
		}
		conn->sconn = sconn;
		return conn;
	}

	/* SMB1 */
find_again:
	i = bitmap_find(sconn->smb1.tcons.bmap, find_offset);

	if (i == -1) {
                /* Expand the connections bitmap. */
                int             oldsz = sconn->smb1.tcons.bmap->n;
                int             newsz = sconn->smb1.tcons.bmap->n +
					BITMAP_BLOCK_SZ;
                struct bitmap * nbmap;

                if (newsz <= oldsz) {
                        /* Integer wrap. */
		        DEBUG(0,("ERROR! Out of connection structures\n"));
                        return NULL;
                }

		DEBUG(4,("resizing connections bitmap from %d to %d\n",
                        oldsz, newsz));

                nbmap = bitmap_talloc(sconn, newsz);
		if (!nbmap) {
			DEBUG(0,("ERROR! malloc fail.\n"));
			return NULL;
		}

                bitmap_copy(nbmap, sconn->smb1.tcons.bmap);
		TALLOC_FREE(sconn->smb1.tcons.bmap);

                sconn->smb1.tcons.bmap = nbmap;
                find_offset = oldsz; /* Start next search in the new portion. */

                goto find_again;
	}

	/* The bitmap position is used below as the connection number
	 * conn->cnum). This ends up as the TID field in the SMB header,
	 * which is limited to 16 bits (we skip 0xffff which is the
	 * NULL TID).
	 */
	if (i > 65534) {
		DEBUG(0, ("Maximum connection limit reached\n"));
		return NULL;
	}

	if (!(conn=TALLOC_ZERO_P(NULL, connection_struct)) ||
	    !(conn->params = TALLOC_P(conn, struct share_params))) {
		DEBUG(0,("TALLOC_ZERO() failed!\n"));
		TALLOC_FREE(conn);
		return NULL;
	}
	conn->sconn = sconn;
	conn->cnum = i;
	conn->force_group_gid = (gid_t)-1;

	bitmap_set(sconn->smb1.tcons.bmap, i);

	sconn->num_tcons_open++;

	string_set(&conn->connectpath,"");
	string_set(&conn->origpath,"");

	DLIST_ADD(sconn->smb1.tcons.Connections, conn);

	return conn;
}

/****************************************************************************
 Close all conn structures.
 Return true if any were closed.
****************************************************************************/

bool conn_close_all(struct smbd_server_connection *sconn)
{
	bool ret = false;
	if (sconn->using_smb2) {
		/* SMB2 */
		struct smbd_smb2_session *sess;
		for (sess = sconn->smb2.sessions.list; sess; sess = sess->next) {
			struct smbd_smb2_tcon *tcon, *tc_next;

			for (tcon = sess->tcons.list; tcon; tcon = tc_next) {
				tc_next = tcon->next;
				TALLOC_FREE(tcon);
				ret = true;
			}
		}
	} else {
		/* SMB1 */
		connection_struct *conn, *next;

		for (conn=sconn->smb1.tcons.Connections;conn;conn=next) {
			next=conn->next;
			set_current_service(conn, 0, True);
			close_cnum(conn, conn->vuid);
			ret = true;
		}
	}
	return ret;
}

/****************************************************************************
 Update last used timestamps.
****************************************************************************/

static void conn_lastused_update(struct smbd_server_connection *sconn,time_t t)
{
	if (sconn->using_smb2) {
		/* SMB2 */
		struct smbd_smb2_session *sess;
		for (sess = sconn->smb2.sessions.list; sess; sess = sess->next) {
			struct smbd_smb2_tcon *ptcon;

			for (ptcon = sess->tcons.list; ptcon; ptcon = ptcon->next) {
				connection_struct *conn = ptcon->compat_conn;
				/* Update if connection wasn't idle. */
				if (conn && conn->lastused != conn->lastused_count) {
					conn->lastused = t;
					conn->lastused_count = t;
				}
			}
		}
	} else {
		/* SMB1 */
		connection_struct *conn;
		for (conn=sconn->smb1.tcons.Connections;conn;conn=conn->next) {
			/* Update if connection wasn't idle. */
			if (conn->lastused != conn->lastused_count) {
				conn->lastused = t;
				conn->lastused_count = t;
			}
		}
	}
}

/****************************************************************************
 Idle inactive connections.
****************************************************************************/

bool conn_idle_all(struct smbd_server_connection *sconn, time_t t)
{
	int deadtime = lp_deadtime()*60;

	conn_lastused_update(sconn, t);

	if (deadtime <= 0) {
		deadtime = DEFAULT_SMBD_TIMEOUT;
	}

	if (sconn->using_smb2) {
		/* SMB2 */
		struct smbd_smb2_session *sess;
		for (sess = sconn->smb2.sessions.list; sess; sess = sess->next) {
			struct smbd_smb2_tcon *ptcon;

			for (ptcon = sess->tcons.list; ptcon; ptcon = ptcon->next) {
				time_t age;
				connection_struct *conn = ptcon->compat_conn;

				if (conn == NULL) {
					continue;
				}

				age = t - conn->lastused;
				/* close dirptrs on connections that are idle */
				if (age > DPTR_IDLE_TIMEOUT) {
					dptr_idlecnum(conn);
				}

				if (conn->num_files_open > 0 || age < deadtime) {
					return false;
				}
			}
		}
	} else {
		/* SMB1 */
		connection_struct *conn;
		for (conn=sconn->smb1.tcons.Connections;conn;conn=conn->next) {
			time_t age = t - conn->lastused;

			/* close dirptrs on connections that are idle */
			if (age > DPTR_IDLE_TIMEOUT) {
				dptr_idlecnum(conn);
			}

			if (conn->num_files_open > 0 || age < deadtime) {
				return false;
			}
		}
	}

	/*
	 * Check all pipes for any open handles. We cannot
	 * idle with a handle open.
	 */
	if (check_open_pipes()) {
		return false;
	}

	return true;
}

/****************************************************************************
 Clear a vuid out of the validity cache, and as the 'owner' of a connection.
****************************************************************************/

void conn_clear_vuid_caches(struct smbd_server_connection *sconn,uint16_t vuid)
{
	connection_struct *conn;

	if (sconn->using_smb2) {
		/* SMB2 */
		struct smbd_smb2_session *sess;
		for (sess = sconn->smb2.sessions.list; sess; sess = sess->next) {
			struct smbd_smb2_tcon *ptcon;

			for (ptcon = sess->tcons.list; ptcon; ptcon = ptcon->next) {
				if (ptcon->compat_conn) {
					if (ptcon->compat_conn->vuid == vuid) {
						ptcon->compat_conn->vuid = UID_FIELD_INVALID;
					}
					conn_clear_vuid_cache(ptcon->compat_conn, vuid);
				}
			}
		}
	} else {
		/* SMB1 */
		for (conn=sconn->smb1.tcons.Connections;conn;conn=conn->next) {
			if (conn->vuid == vuid) {
				conn->vuid = UID_FIELD_INVALID;
			}
			conn_clear_vuid_cache(conn, vuid);
		}
	}
}

/****************************************************************************
 Free a conn structure - internal part.
****************************************************************************/

static void conn_free_internal(connection_struct *conn)
{
	vfs_handle_struct *handle = NULL, *thandle = NULL;
	struct trans_state *state = NULL;

	/* Free vfs_connection_struct */
	handle = conn->vfs_handles;
	while(handle) {
		thandle = handle->next;
		DLIST_REMOVE(conn->vfs_handles, handle);
		if (handle->free_data)
			handle->free_data(&handle->data);
		handle = thandle;
	}

	/* Free any pending transactions stored on this conn. */
	for (state = conn->pending_trans; state; state = state->next) {
		/* state->setup is a talloc child of state. */
		SAFE_FREE(state->param);
		SAFE_FREE(state->data);
	}

	free_namearray(conn->veto_list);
	free_namearray(conn->hide_list);
	free_namearray(conn->veto_oplock_list);
	free_namearray(conn->aio_write_behind_list);

	string_free(&conn->connectpath);
	string_free(&conn->origpath);

	ZERO_STRUCTP(conn);
	talloc_destroy(conn);
}

/****************************************************************************
 Free a conn structure.
****************************************************************************/

void conn_free(connection_struct *conn)
{
	if (conn->sconn == NULL) {
		conn_free_internal(conn);
		return;
	}

	if (conn->sconn->using_smb2) {
		/* SMB2 */
		conn_free_internal(conn);
		return;
	}

	/* SMB1 */
	DLIST_REMOVE(conn->sconn->smb1.tcons.Connections, conn);

	if (conn->sconn->smb1.tcons.bmap != NULL) {
		/*
		 * Can be NULL for fake connections created by
		 * create_conn_struct()
		 */
		bitmap_clear(conn->sconn->smb1.tcons.bmap, conn->cnum);
	}

	SMB_ASSERT(conn->sconn->num_tcons_open > 0);
	conn->sconn->num_tcons_open--;

	conn_free_internal(conn);
}

/****************************************************************************
 Receive a smbcontrol message to forcibly unmount a share.
 The message contains just a share name and all instances of that
 share are unmounted.
 The special sharename '*' forces unmount of all shares.
****************************************************************************/

void msg_force_tdis(struct messaging_context *msg,
		    void *private_data,
		    uint32_t msg_type,
		    struct server_id server_id,
		    DATA_BLOB *data)
{
	struct smbd_server_connection *sconn;
	connection_struct *conn, *next;
	fstring sharename;

	sconn = msg_ctx_to_sconn(msg);
	if (sconn == NULL) {
		DEBUG(1, ("could not find sconn\n"));
		return;
	}

	fstrcpy(sharename, (const char *)data->data);

	if (strcmp(sharename, "*") == 0) {
		DEBUG(1,("Forcing close of all shares\n"));
		conn_close_all(sconn);
		goto done;
	}

	if (sconn->using_smb2) {
		/* SMB2 */
		struct smbd_smb2_session *sess;
		for (sess = sconn->smb2.sessions.list; sess; sess = sess->next) {
			struct smbd_smb2_tcon *tcon, *tc_next;

			for (tcon = sess->tcons.list; tcon; tcon = tc_next) {
				tc_next = tcon->next;
				if (tcon->compat_conn &&
						strequal(lp_servicename(SNUM(tcon->compat_conn)),
								sharename)) {
					DEBUG(1,("Forcing close of share %s cnum=%d\n",
						sharename, tcon->compat_conn->cnum));
					TALLOC_FREE(tcon);
				}
			}
		}
	} else {
		/* SMB1 */
		for (conn=sconn->smb1.tcons.Connections;conn;conn=next) {
			next=conn->next;
			if (strequal(lp_servicename(SNUM(conn)), sharename)) {
				DEBUG(1,("Forcing close of share %s cnum=%d\n",
					sharename, conn->cnum));
				close_cnum(conn, (uint16)-1);
			}
		}
	}

 done:

	change_to_root_user();
	reload_services(msg, -1, true);
}
