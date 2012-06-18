/* 
   Unix SMB/CIFS implementation.
   DMAPI Support routines

   Copyright (C) James Peach 2006

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

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_DMAPI

#ifndef USE_DMAPI

uint32 dmapi_file_flags(const char * const path) { return 0; }
bool dmapi_have_session(void) { return False; }
const void * dmapi_get_current_session(void) { return NULL; }

#else /* USE_DMAPI */

#ifdef HAVE_XFS_DMAPI_H
#include <xfs/dmapi.h>
#elif defined(HAVE_SYS_DMI_H)
#include <sys/dmi.h>
#elif defined(HAVE_SYS_JFSDMAPI_H)
#include <sys/jfsdmapi.h>
#elif defined(HAVE_SYS_DMAPI_H)
#include <sys/dmapi.h>
#elif defined(HAVE_DMAPI_H)
#include <dmapi.h>
#endif

#define DMAPI_SESSION_NAME "samba"
#define DMAPI_TRACE 10

struct smbd_dmapi_context {
	dm_sessid_t session;
	unsigned session_num;
};

/* 
   Initialise DMAPI session. The session is persistant kernel state, 
   so it might already exist, in which case we merely want to 
   reconnect to it. This function should be called as root.
*/
static int dmapi_init_session(struct smbd_dmapi_context *ctx)
{
	char	buf[DM_SESSION_INFO_LEN];
	size_t	buflen;
	uint	    nsessions = 5;
	dm_sessid_t *sessions = NULL;
	char    *version;
	char    *session_name;
	TALLOC_CTX *tmp_ctx = talloc_new(NULL);

	int i, err;

	if (ctx->session_num == 0) {
		session_name = talloc_strdup(tmp_ctx, DMAPI_SESSION_NAME);
	} else {
		session_name = talloc_asprintf(tmp_ctx, "%s%u", DMAPI_SESSION_NAME,
					       ctx->session_num);
	}

	if (session_name == NULL) {
		DEBUG(0,("Out of memory in dmapi_init_session\n"));
		talloc_free(tmp_ctx);
		return -1;
	}
 

	if (dm_init_service(&version) < 0) {
		DEBUG(0, ("dm_init_service failed - disabling DMAPI\n"));
		talloc_free(tmp_ctx);
		return -1;
	}

	ZERO_STRUCT(buf);

	/* Fetch kernel DMAPI sessions until we get any of them */
	do {
		dm_sessid_t *new_sessions;
		nsessions *= 2;
		new_sessions = TALLOC_REALLOC_ARRAY(tmp_ctx, sessions, 
						    dm_sessid_t, nsessions);
		if (new_sessions == NULL) {
			talloc_free(tmp_ctx);
			return -1;
		}

		sessions = new_sessions;
		err = dm_getall_sessions(nsessions, sessions, &nsessions);
	} while (err == -1 && errno == E2BIG);

	if (err == -1) {
		DEBUGADD(DMAPI_TRACE,
			("failed to retrieve DMAPI sessions: %s\n",
			strerror(errno)));
		talloc_free(tmp_ctx);
		return -1;
	}

	/* Look through existing kernel DMAPI sessions to find out ours */
	for (i = 0; i < nsessions; ++i) {
		err = dm_query_session(sessions[i], sizeof(buf), buf, &buflen);
		buf[sizeof(buf) - 1] = '\0';
		if (err == 0 && strcmp(session_name, buf) == 0) {
			ctx->session = sessions[i];
			DEBUGADD(DMAPI_TRACE,
				("attached to existing DMAPI session "
				 "named '%s'\n", buf));
			break;
		}
	}

	/* No session already defined. */
	if (ctx->session == DM_NO_SESSION) {
		err = dm_create_session(DM_NO_SESSION, 
					session_name,
					&ctx->session);
		if (err < 0) {
			DEBUGADD(DMAPI_TRACE,
				("failed to create new DMAPI session: %s\n",
				strerror(errno)));
			ctx->session = DM_NO_SESSION;
			talloc_free(tmp_ctx);
			return -1;
		}

		DEBUG(0, ("created new DMAPI session named '%s' for %s\n",
			  session_name, version));
	}

	if (ctx->session != DM_NO_SESSION) {
		set_effective_capability(DMAPI_ACCESS_CAPABILITY);
	}

	/* 
	   Note that we never end the DMAPI session. It gets re-used if possiblie. 
	   DMAPI session is a kernel resource that is usually lives until server reboot
	   and doesn't get destroed when an application finishes.

	   However, we free list of references to DMAPI sessions we've got from the kernel
	   as it is not needed anymore once we have found (or created) our session.
	 */

	talloc_free(tmp_ctx);
	return 0;
}

/*
  Return a pointer to our DMAPI session, if available.
  This assumes that you have called dmapi_have_session() first.
*/
const void *dmapi_get_current_session(void)
{
	if (!dmapi_ctx) {
		return NULL;
	}

	if (dmapi_ctx->session == DM_NO_SESSION) {
		return NULL;
	}

	return (void *)&dmapi_ctx->session;
}
	
/*
  dmapi_have_session() must be the first DMAPI call you make in Samba. It will
  initialize DMAPI, if available, and tell you if you can get a DMAPI session.
  This should be called in the client-specific child process.
*/

bool dmapi_have_session(void)
{
	if (!dmapi_ctx) {
		dmapi_ctx = talloc(talloc_autofree_context(),
				   struct smbd_dmapi_context);
		if (!dmapi_ctx) {
			exit_server("unable to allocate smbd_dmapi_context");
		}
		dmapi_ctx->session = DM_NO_SESSION;
		dmapi_ctx->session_num = 0;

		become_root();
		dmapi_init_session(dmapi_ctx);
		unbecome_root();

	}

	return dmapi_ctx->session != DM_NO_SESSION;
}

/*
  only call this when you get back an EINVAL error indicating that the
  session you are using is invalid. This destroys the existing session
  and creates a new one.
 */
bool dmapi_new_session(void)
{
	if (dmapi_have_session()) {
		/* try to destroy the old one - this may not succeed */
		dm_destroy_session(dmapi_ctx->session);
	}
	dmapi_ctx->session = DM_NO_SESSION;
	become_root();
	dmapi_ctx->session_num++;
	dmapi_init_session(dmapi_ctx);
	unbecome_root();
	return dmapi_ctx->session != DM_NO_SESSION;
}

/* 
    only call this when exiting from master smbd process. DMAPI sessions
    are long-lived kernel resources we ought to share across smbd processes.
    However, we must free them when all smbd processes are finished to
    allow other subsystems clean up properly. Not freeing DMAPI session
    blocks certain HSM implementations from proper shutdown.
*/
bool dmapi_destroy_session(void)
{
	if (!dmapi_ctx) {
		return true;
	}
	if (dmapi_ctx->session != DM_NO_SESSION) {
		become_root();
		if (0 == dm_destroy_session(dmapi_ctx->session)) {
			dmapi_ctx->session_num--;
			dmapi_ctx->session = DM_NO_SESSION;
		} else {
			DEBUG(0,("Couldn't destroy DMAPI session: %s\n",
				 strerror(errno)));
		}
		unbecome_root();
	}
	return dmapi_ctx->session == DM_NO_SESSION;
}


/* 
   This is default implementation of dmapi_file_flags() that is 
   called from VFS is_offline() call to know whether file is offline.
   For GPFS-specific version see modules/vfs_tsmsm.c. It might be
   that approach on quering existence of a specific attribute that
   is used in vfs_tsmsm.c will work with other DMAPI-based HSM 
   implementations as well.
*/
uint32 dmapi_file_flags(const char * const path)
{
	int		err;
	dm_eventset_t   events = {0};
	uint		nevents;

	dm_sessid_t     dmapi_session;
	const void      *dmapi_session_ptr;
	void	        *dm_handle = NULL;
	size_t	        dm_handle_len = 0;

	uint32	        flags = 0;

	dmapi_session_ptr = dmapi_get_current_session();
	if (dmapi_session_ptr == NULL) {
		return 0;
	}

	dmapi_session = *(dm_sessid_t *)dmapi_session_ptr;
	if (dmapi_session == DM_NO_SESSION) {
		return 0;
	}

	/* AIX has DMAPI but no POSIX capablities support. In this case,
	 * we need to be root to do DMAPI manipulations.
	 */
#ifndef HAVE_POSIX_CAPABILITIES
	become_root();
#endif

	err = dm_path_to_handle(CONST_DISCARD(char *, path),
		&dm_handle, &dm_handle_len);
	if (err < 0) {
		DEBUG(DMAPI_TRACE, ("dm_path_to_handle(%s): %s\n",
			    path, strerror(errno)));

		if (errno != EPERM) {
			goto done;
		}

		/* Linux capabilities are broken in that changing our
		 * user ID will clobber out effective capabilities irrespective
		 * of whether we have set PR_SET_KEEPCAPS. Fortunately, the
		 * capabilities are not removed from our permitted set, so we
		 * can re-acquire them if necessary.
		 */

		set_effective_capability(DMAPI_ACCESS_CAPABILITY);

		err = dm_path_to_handle(CONST_DISCARD(char *, path),
			&dm_handle, &dm_handle_len);
		if (err < 0) {
			DEBUG(DMAPI_TRACE,
			    ("retrying dm_path_to_handle(%s): %s\n",
			    path, strerror(errno)));
			goto done;
		}
	}

	err = dm_get_eventlist(dmapi_session, dm_handle, dm_handle_len,
		DM_NO_TOKEN, DM_EVENT_MAX, &events, &nevents);
	if (err < 0) {
		DEBUG(DMAPI_TRACE, ("dm_get_eventlist(%s): %s\n",
			    path, strerror(errno)));
		dm_handle_free(dm_handle, dm_handle_len);
		goto done;
	}

	/* We figure that the only reason a DMAPI application would be
	 * interested in trapping read events is that part of the file is
	 * offline.
	 */
	DEBUG(DMAPI_TRACE, ("DMAPI event list for %s\n", path));
	if (DMEV_ISSET(DM_EVENT_READ, events)) {
		flags = FILE_ATTRIBUTE_OFFLINE;
	}

	dm_handle_free(dm_handle, dm_handle_len);

	if (flags & FILE_ATTRIBUTE_OFFLINE) {
		DEBUG(DMAPI_TRACE, ("%s is OFFLINE\n", path));
	}

done:

#ifndef HAVE_POSIX_CAPABILITIES
	unbecome_root();
#endif

	return flags;
}


#endif /* USE_DMAPI */
