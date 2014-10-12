/*
   Unix SMB/CIFS implementation.
   Main SMB server routines
   Copyright (C) Andrew Tridgell		1992-1998
   Copyright (C) Martin Pool			2002
   Copyright (C) Jelmer Vernooij		2002-2003
   Copyright (C) Volker Lendecke		1993-2007
   Copyright (C) Jeremy Allison			1993-2007
   Copyright (C) Andrew Bartlett                2010

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
#include "ntdomain.h"
#include "../librpc/gen_ndr/srv_dfs.h"
#include "../librpc/gen_ndr/srv_dssetup.h"
#include "../librpc/gen_ndr/srv_echo.h"
#include "../librpc/gen_ndr/srv_eventlog.h"
#include "../librpc/gen_ndr/srv_initshutdown.h"
#include "../librpc/gen_ndr/srv_lsa.h"
#include "../librpc/gen_ndr/srv_netlogon.h"
#include "../librpc/gen_ndr/srv_ntsvcs.h"
#include "../librpc/gen_ndr/srv_samr.h"
#include "../librpc/gen_ndr/srv_spoolss.h"
#include "../librpc/gen_ndr/srv_srvsvc.h"
#include "../librpc/gen_ndr/srv_svcctl.h"
#include "../librpc/gen_ndr/srv_winreg.h"
#include "../librpc/gen_ndr/srv_wkssvc.h"
#include "printing/notify.h"
#include "printing.h"
#include "serverid.h"

static struct files_struct *log_writeable_file_fn(
	struct files_struct *fsp, void *private_data)
{
	bool *found = (bool *)private_data;
	char *path;

	if (!fsp->can_write) {
		return NULL;
	}
	if (!(*found)) {
		DEBUG(0, ("Writable files open at exit:\n"));
		*found = true;
	}

	path = talloc_asprintf(talloc_tos(), "%s/%s", fsp->conn->connectpath,
			       smb_fname_str_dbg(fsp->fsp_name));
	if (path == NULL) {
		DEBUGADD(0, ("<NOMEM>\n"));
	}

	DEBUGADD(0, ("%s\n", path));

	TALLOC_FREE(path);
	return NULL;
}

/****************************************************************************
 Exit the server.
****************************************************************************/

/* Reasons for shutting down a server process. */
enum server_exit_reason { SERVER_EXIT_NORMAL, SERVER_EXIT_ABNORMAL };

static void exit_server_common(enum server_exit_reason how,
	const char *const reason) _NORETURN_;

static void exit_server_common(enum server_exit_reason how,
	const char *const reason)
{
	struct smbd_server_connection *sconn = smbd_server_conn;

	if (!exit_firsttime)
		exit(0);
	exit_firsttime = false;

	change_to_root_user();

	if (sconn && sconn->smb1.negprot.auth_context) {
		TALLOC_FREE(sconn->smb1.negprot.auth_context);
	}

	if (sconn) {
		if (lp_log_writeable_files_on_exit()) {
			bool found = false;
			files_forall(sconn, log_writeable_file_fn, &found);
		}
		(void)conn_close_all(sconn);
		invalidate_all_vuids(sconn);
	}

	/* 3 second timeout. */
	print_notify_send_messages(sconn->msg_ctx, 3);

	/* delete our entry in the serverid database. */
	if (am_parent) {
		/*
		 * For children the parent takes care of cleaning up
		 */
		serverid_deregister(sconn_server_id(sconn));
	}

#ifdef WITH_DFS
	if (dcelogin_atmost_once) {
		dfs_unlogin();
	}
#endif

#ifdef USE_DMAPI
	/* Destroy Samba DMAPI session only if we are master smbd process */
	if (am_parent) {
		if (!dmapi_destroy_session()) {
			DEBUG(0,("Unable to close Samba DMAPI session\n"));
		}
	}
#endif

	if (am_parent) {
		rpc_wkssvc_shutdown();
#ifdef ACTIVE_DIRECTORY
		rpc_dssetup_shutdown();
#endif
#ifdef DEVELOPER
		rpc_rpcecho_shutdown();
#endif
#ifdef DFS_SUPPORT
		rpc_netdfs_shutdown();
#endif
		rpc_initshutdown_shutdown();
#ifdef EXTRA_SERVICES
		rpc_eventlog_shutdown();
		rpc_svcctl_shutdown();
		rpc_ntsvcs_shutdown();
#endif
#ifdef PRINTER_SUPPORT
		rpc_spoolss_shutdown();
#endif

		rpc_srvsvc_shutdown();
#ifdef WINREG_SUPPORT
		rpc_winreg_shutdown();
#endif

#ifdef NETLOGON_SUPPORT
		rpc_netlogon_shutdown();
#endif
#ifdef SAMR_SUPPORT
		rpc_samr_shutdown();
#endif
#ifdef LSA_SUPPORT
		rpc_lsarpc_shutdown();
#endif
	}

	/*
	 * we need to force the order of freeing the following,
	 * because smbd_msg_ctx is not a talloc child of smbd_server_conn.
	 */
	sconn = NULL;
	TALLOC_FREE(smbd_server_conn);
	server_messaging_context_free();
	server_event_context_free();
	TALLOC_FREE(smbd_memcache_ctx);

	locking_end();
	printing_end();

	if (how != SERVER_EXIT_NORMAL) {
		DEBUGSEP(0);
		DEBUG(0,("Abnormal server exit: %s\n",
			reason ? reason : "no explanation provided"));
		DEBUGSEP(0);

		log_stack_trace();

		dump_core();

		/* Notreached. */
		exit(1);
	} else {
		DEBUG(3,("Server exit (%s)\n",
			(reason ? reason : "normal exit")));
		if (am_parent) {
			pidfile_unlink();
		}
		gencache_stabilize();
	}

	exit(0);
}

void exit_server(const char *const explanation)
{
	exit_server_common(SERVER_EXIT_ABNORMAL, explanation);
}

void exit_server_cleanly(const char *const explanation)
{
	exit_server_common(SERVER_EXIT_NORMAL, explanation);
}

void exit_server_fault(void)
{
	exit_server("critical server fault");
}
