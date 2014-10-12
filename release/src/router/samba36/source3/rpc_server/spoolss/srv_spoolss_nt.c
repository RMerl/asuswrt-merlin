/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-2000,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-2000,
 *  Copyright (C) Jean François Micouleau      1998-2000,
 *  Copyright (C) Jeremy Allison               2001-2002,
 *  Copyright (C) Gerald Carter		       2000-2004,
 *  Copyright (C) Tim Potter                   2001-2002.
 *  Copyright (C) Guenther Deschner            2009-2010.
 *  Copyright (C) Andreas Schneider            2010.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* Since the SPOOLSS rpc routines are basically DOS 16-bit calls wrapped
   up, all the errors returned are DOS errors, not NT status codes. */

#include "includes.h"
#include "ntdomain.h"
#include "nt_printing.h"
#include "srv_spoolss_util.h"
#include "../librpc/gen_ndr/srv_spoolss.h"
#include "../librpc/gen_ndr/ndr_spoolss_c.h"
#include "rpc_client/init_spoolss.h"
#include "rpc_client/cli_pipe.h"
#include "../libcli/security/security.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "registry.h"
#include "registry/reg_objects.h"
#include "include/printing.h"
#include "secrets.h"
#include "../librpc/gen_ndr/netlogon.h"
#include "rpc_misc.h"
#include "printing/notify.h"
#include "serverid.h"
#include "../libcli/registry/util_reg.h"
#include "smbd/smbd.h"
#include "auth.h"
#include "messages.h"
#include "rpc_server/spoolss/srv_spoolss_nt.h"
#include "util_tdb.h"
#include "libsmb/libsmb.h"
#include "printing/printer_list.h"
#include "rpc_client/cli_winreg_spoolss.h"

/* macros stolen from s4 spoolss server */
#define SPOOLSS_BUFFER_UNION(fn,info,level) \
	((info)?ndr_size_##fn(info, level, 0):0)

#define SPOOLSS_BUFFER_UNION_ARRAY(mem_ctx,fn,info,level,count) \
	((info)?ndr_size_##fn##_info(mem_ctx, level, count, info):0)

#define SPOOLSS_BUFFER_ARRAY(mem_ctx,fn,info,count) \
	((info)?ndr_size_##fn##_info(mem_ctx, count, info):0)

#define SPOOLSS_BUFFER_OK(val_true,val_false) ((r->in.offered >= *r->out.needed)?val_true:val_false)

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

#ifndef MAX_OPEN_PRINTER_EXS
#define MAX_OPEN_PRINTER_EXS 50
#endif

struct notify_back_channel;

/* structure to store the printer handles */
/* and a reference to what it's pointing to */
/* and the notify info asked about */
/* that's the central struct */
struct printer_handle {
	struct printer_handle *prev, *next;
	bool document_started;
	bool page_started;
	uint32 jobid; /* jobid in printing backend */
	int printer_type;
	const char *servername;
	fstring sharename;
	uint32 type;
	uint32 access_granted;
	struct {
		uint32 flags;
		uint32 options;
		fstring localmachine;
		uint32 printerlocal;
		struct spoolss_NotifyOption *option;
		struct policy_handle cli_hnd;
		struct notify_back_channel *cli_chan;
		uint32 change;
		/* are we in a FindNextPrinterChangeNotify() call? */
		bool fnpcn;
		struct messaging_context *msg_ctx;
	} notify;
	struct {
		fstring machine;
		fstring user;
	} client;

	/* devmode sent in the OpenPrinter() call */
	struct spoolss_DeviceMode *devmode;

	/* TODO cache the printer info2 structure */
	struct spoolss_PrinterInfo2 *info2;

};

static struct printer_handle *printers_list;

struct printer_session_counter {
	struct printer_session_counter *next;
	struct printer_session_counter *prev;

	int snum;
	uint32_t counter;
};

static struct printer_session_counter *counter_list;

struct notify_back_channel {
	struct notify_back_channel *prev, *next;

	/* associated client */
	struct sockaddr_storage client_address;

	/* print notify back-channel pipe handle*/
	struct rpc_pipe_client *cli_pipe;
	struct dcerpc_binding_handle *binding_handle;
	uint32_t active_connections;
};

static struct notify_back_channel *back_channels;

/* Map generic permissions to printer object specific permissions */

const struct standard_mapping printer_std_mapping = {
	PRINTER_READ,
	PRINTER_WRITE,
	PRINTER_EXECUTE,
	PRINTER_ALL_ACCESS
};

/* Map generic permissions to print server object specific permissions */

const struct standard_mapping printserver_std_mapping = {
	SERVER_READ,
	SERVER_WRITE,
	SERVER_EXECUTE,
	SERVER_ALL_ACCESS
};

/* API table for Xcv Monitor functions */

struct xcv_api_table {
	const char *name;
	WERROR(*fn) (TALLOC_CTX *mem_ctx, struct security_token *token, DATA_BLOB *in, DATA_BLOB *out, uint32_t *needed);
};

static void prune_printername_cache(void);

/********************************************************************
 * Canonicalize servername.
 ********************************************************************/

static const char *canon_servername(const char *servername)
{
	const char *pservername = servername;
	while (*pservername == '\\') {
		pservername++;
	}
	return pservername;
}

/* translate between internal status numbers and NT status numbers */
static int nt_printj_status(int v)
{
	switch (v) {
	case LPQ_QUEUED:
		return 0;
	case LPQ_PAUSED:
		return JOB_STATUS_PAUSED;
	case LPQ_SPOOLING:
		return JOB_STATUS_SPOOLING;
	case LPQ_PRINTING:
		return JOB_STATUS_PRINTING;
	case LPQ_ERROR:
		return JOB_STATUS_ERROR;
	case LPQ_DELETING:
		return JOB_STATUS_DELETING;
	case LPQ_OFFLINE:
		return JOB_STATUS_OFFLINE;
	case LPQ_PAPEROUT:
		return JOB_STATUS_PAPEROUT;
	case LPQ_PRINTED:
		return JOB_STATUS_PRINTED;
	case LPQ_DELETED:
		return JOB_STATUS_DELETED;
	case LPQ_BLOCKED:
		return JOB_STATUS_BLOCKED_DEVQ;
	case LPQ_USER_INTERVENTION:
		return JOB_STATUS_USER_INTERVENTION;
	}
	return 0;
}

static int nt_printq_status(int v)
{
	switch (v) {
	case LPQ_PAUSED:
		return PRINTER_STATUS_PAUSED;
	case LPQ_QUEUED:
	case LPQ_SPOOLING:
	case LPQ_PRINTING:
		return 0;
	}
	return 0;
}

/***************************************************************************
 Disconnect from the client
****************************************************************************/

static void srv_spoolss_replycloseprinter(int snum,
					  struct printer_handle *prn_hnd)
{
	WERROR result;
	NTSTATUS status;

	/*
	 * Tell the specific printing tdb we no longer want messages for this printer
	 * by deregistering our PID.
	 */

	if (!print_notify_deregister_pid(snum)) {
		DEBUG(0, ("Failed to register our pid for printer %s\n",
			  lp_const_servicename(snum)));
	}

	/* weird if the test succeeds !!! */
	if (prn_hnd->notify.cli_chan == NULL ||
	    prn_hnd->notify.cli_chan->active_connections == 0) {
		DEBUG(0, ("Trying to close unexisting backchannel!\n"));
		DLIST_REMOVE(back_channels, prn_hnd->notify.cli_chan);
		TALLOC_FREE(prn_hnd->notify.cli_chan);
		return;
	}

	status = dcerpc_spoolss_ReplyClosePrinter(
					prn_hnd->notify.cli_chan->binding_handle,
					talloc_tos(),
					&prn_hnd->notify.cli_hnd,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("dcerpc_spoolss_ReplyClosePrinter failed [%s].\n",
			  nt_errstr(status)));
		result = ntstatus_to_werror(status);
	} else if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("reply_close_printer failed [%s].\n",
			  win_errstr(result)));
	}

	/* if it's the last connection, deconnect the IPC$ share */
	if (prn_hnd->notify.cli_chan->active_connections == 1) {

		prn_hnd->notify.cli_chan->binding_handle = NULL;
		cli_shutdown(rpc_pipe_np_smb_conn(prn_hnd->notify.cli_chan->cli_pipe));
		DLIST_REMOVE(back_channels, prn_hnd->notify.cli_chan);
		TALLOC_FREE(prn_hnd->notify.cli_chan);

		if (prn_hnd->notify.msg_ctx != NULL) {
			messaging_deregister(prn_hnd->notify.msg_ctx,
					     MSG_PRINTER_NOTIFY2, NULL);

			/*
			 * Tell the serverid.tdb we're no longer
			 * interested in printer notify messages.
			 */

			serverid_register_msg_flags(
				messaging_server_id(prn_hnd->notify.msg_ctx),
				false, FLAG_MSG_PRINT_NOTIFY);
		}
	}

	if (prn_hnd->notify.cli_chan) {
		prn_hnd->notify.cli_chan->active_connections--;
		prn_hnd->notify.cli_chan = NULL;
	}
}

/****************************************************************************
 Functions to free a printer entry datastruct.
****************************************************************************/

static int printer_entry_destructor(struct printer_handle *Printer)
{
	if (Printer->notify.cli_chan != NULL &&
	    Printer->notify.cli_chan->active_connections > 0) {
		int snum = -1;

		switch(Printer->printer_type) {
		case SPLHND_SERVER:
			srv_spoolss_replycloseprinter(snum, Printer);
			break;

		case SPLHND_PRINTER:
			snum = print_queue_snum(Printer->sharename);
			if (snum != -1) {
				srv_spoolss_replycloseprinter(snum, Printer);
			}
			break;
		default:
			break;
		}
	}

	Printer->notify.flags=0;
	Printer->notify.options=0;
	Printer->notify.localmachine[0]='\0';
	Printer->notify.printerlocal=0;
	TALLOC_FREE(Printer->notify.option);
	TALLOC_FREE(Printer->devmode);

	/* Remove from the internal list. */
	DLIST_REMOVE(printers_list, Printer);
	return 0;
}

/****************************************************************************
  find printer index by handle
****************************************************************************/

static struct printer_handle *find_printer_index_by_hnd(struct pipes_struct *p,
							struct policy_handle *hnd)
{
	struct printer_handle *find_printer = NULL;

	if(!find_policy_by_hnd(p,hnd,(void **)(void *)&find_printer)) {
		DEBUG(2,("find_printer_index_by_hnd: Printer handle not found: "));
		return NULL;
	}

	return find_printer;
}

/****************************************************************************
 Close printer index by handle.
****************************************************************************/

static bool close_printer_handle(struct pipes_struct *p, struct policy_handle *hnd)
{
	struct printer_handle *Printer = find_printer_index_by_hnd(p, hnd);

	if (!Printer) {
		DEBUG(2,("close_printer_handle: Invalid handle (%s:%u:%u)\n",
			OUR_HANDLE(hnd)));
		return false;
	}

	close_policy_hnd(p, hnd);

	return true;
}

/****************************************************************************
 Delete a printer given a handle.
****************************************************************************/

static WERROR delete_printer_hook(TALLOC_CTX *ctx, struct security_token *token,
				  const char *sharename,
				  struct messaging_context *msg_ctx)
{
	char *cmd = lp_deleteprinter_cmd();
	char *command = NULL;
	int ret;
	bool is_print_op = false;

	/* can't fail if we don't try */

	if ( !*cmd )
		return WERR_OK;

	command = talloc_asprintf(ctx,
			"%s \"%s\"",
			cmd, sharename);
	if (!command) {
		return WERR_NOMEM;
	}
	if ( token )
		is_print_op = security_token_has_privilege(token, SEC_PRIV_PRINT_OPERATOR);

	DEBUG(10,("Running [%s]\n", command));

	/********** BEGIN SePrintOperatorPrivlege BLOCK **********/

	if ( is_print_op )
		become_root();

	if ( (ret = smbrun(command, NULL)) == 0 ) {
		/* Tell everyone we updated smb.conf. */
		message_send_all(msg_ctx, MSG_SMB_CONF_UPDATED, NULL, 0, NULL);
	}

	if ( is_print_op )
		unbecome_root();

	/********** END SePrintOperatorPrivlege BLOCK **********/

	DEBUGADD(10,("returned [%d]\n", ret));

	TALLOC_FREE(command);

	if (ret != 0)
		return WERR_BADFID; /* What to return here? */

	/* go ahead and re-read the services immediately */
	become_root();
	reload_services(msg_ctx, -1, false);
	unbecome_root();

	if ( lp_servicenumber( sharename ) >= 0 )
		return WERR_ACCESS_DENIED;

	return WERR_OK;
}

/****************************************************************************
 Delete a printer given a handle.
****************************************************************************/

static WERROR delete_printer_handle(struct pipes_struct *p, struct policy_handle *hnd)
{
	struct printer_handle *Printer = find_printer_index_by_hnd(p, hnd);
	WERROR result;

	if (!Printer) {
		DEBUG(2,("delete_printer_handle: Invalid handle (%s:%u:%u)\n",
			OUR_HANDLE(hnd)));
		return WERR_BADFID;
	}

	/*
	 * It turns out that Windows allows delete printer on a handle
	 * opened by an admin user, then used on a pipe handle created
	 * by an anonymous user..... but they're working on security.... riiight !
	 * JRA.
	 */

	if (Printer->access_granted != PRINTER_ACCESS_ADMINISTER) {
		DEBUG(3, ("delete_printer_handle: denied by handle\n"));
		return WERR_ACCESS_DENIED;
	}

	/* this does not need a become root since the access check has been
	   done on the handle already */

	result = winreg_delete_printer_key_internal(p->mem_ctx,
					   get_session_info_system(),
					   p->msg_ctx,
					   Printer->sharename,
					   "");
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(3,("Error deleting printer %s\n", Printer->sharename));
		return WERR_BADFID;
	}

	result = delete_printer_hook(p->mem_ctx, p->session_info->security_token,
				     Printer->sharename, p->msg_ctx);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}
	prune_printername_cache();
	return WERR_OK;
}

/****************************************************************************
 Return the snum of a printer corresponding to an handle.
****************************************************************************/

static bool get_printer_snum(struct pipes_struct *p, struct policy_handle *hnd,
			     int *number, struct share_params **params)
{
	struct printer_handle *Printer = find_printer_index_by_hnd(p, hnd);

	if (!Printer) {
		DEBUG(2,("get_printer_snum: Invalid handle (%s:%u:%u)\n",
			OUR_HANDLE(hnd)));
		return false;
	}

	switch (Printer->printer_type) {
		case SPLHND_PRINTER:
			DEBUG(4,("short name:%s\n", Printer->sharename));
			*number = print_queue_snum(Printer->sharename);
			return (*number != -1);
		case SPLHND_SERVER:
			return false;
		default:
			return false;
	}
}

/****************************************************************************
 Set printer handle type.
 Check if it's \\server or \\server\printer
****************************************************************************/

static bool set_printer_hnd_printertype(struct printer_handle *Printer, const char *handlename)
{
	DEBUG(3,("Setting printer type=%s\n", handlename));

	/* it's a print server */
	if (handlename && *handlename=='\\' && *(handlename+1)=='\\' && !strchr_m(handlename+2, '\\')) {
		DEBUGADD(4,("Printer is a print server\n"));
		Printer->printer_type = SPLHND_SERVER;
	}
	/* it's a printer (set_printer_hnd_name() will handle port monitors */
	else {
		DEBUGADD(4,("Printer is a printer\n"));
		Printer->printer_type = SPLHND_PRINTER;
	}

	return true;
}

static void prune_printername_cache_fn(const char *key, const char *value,
				       time_t timeout, void *private_data)
{
	gencache_del(key);
}

static void prune_printername_cache(void)
{
	gencache_iterate(prune_printername_cache_fn, NULL, "PRINTERNAME/*");
}

/****************************************************************************
 Set printer handle name..  Accept names like \\server, \\server\printer,
 \\server\SHARE, & "\\server\,XcvMonitor Standard TCP/IP Port"    See
 the MSDN docs regarding OpenPrinter() for details on the XcvData() and
 XcvDataPort() interface.
****************************************************************************/

static WERROR set_printer_hnd_name(TALLOC_CTX *mem_ctx,
				   const struct auth_serversupplied_info *session_info,
				   struct messaging_context *msg_ctx,
				   struct printer_handle *Printer,
				   const char *handlename)
{
	int snum;
	int n_services=lp_numservices();
	char *aprinter;
	const char *printername;
	const char *servername = NULL;
	fstring sname;
	bool found = false;
	struct spoolss_PrinterInfo2 *info2 = NULL;
	WERROR result;
	char *p;

	/*
	 * Hopefully nobody names his printers like this. Maybe \ or ,
	 * are illegal in printer names even?
	 */
	const char printer_not_found[] = "Printer \\, !@#$%^&*( not found";
	char *cache_key;
	char *tmp;

	DEBUG(4,("Setting printer name=%s (len=%lu)\n", handlename,
		(unsigned long)strlen(handlename)));

	aprinter = CONST_DISCARD(char *, handlename);
	if ( *handlename == '\\' ) {
		servername = canon_servername(handlename);
		if ( (aprinter = strchr_m( servername, '\\' )) != NULL ) {
			*aprinter = '\0';
			aprinter++;
		}
		if (!is_myname_or_ipaddr(servername)) {
			return WERR_INVALID_PRINTER_NAME;
		}
		Printer->servername = talloc_asprintf(Printer, "\\\\%s", servername);
		if (Printer->servername == NULL) {
			return WERR_NOMEM;
		}
	}

	if (Printer->printer_type == SPLHND_SERVER) {
		return WERR_OK;
	}

	if (Printer->printer_type != SPLHND_PRINTER) {
		return WERR_INVALID_HANDLE;
	}

	DEBUGADD(5, ("searching for [%s]\n", aprinter));

	p = strchr(aprinter, ',');
	if (p != NULL) {
		char *p2 = p;
		p++;
		if (*p == ' ') {
			p++;
		}
		if (strncmp(p, "DrvConvert", strlen("DrvConvert")) == 0) {
			*p2 = '\0';
		} else if (strncmp(p, "LocalOnly", strlen("LocalOnly")) == 0) {
			*p2 = '\0';
		}
	}

	if (p) {
		DEBUGADD(5, ("stripped handlename: [%s]\n", aprinter));
	}

	/* check for the Port Monitor Interface */
	if ( strequal( aprinter, SPL_XCV_MONITOR_TCPMON ) ) {
		Printer->printer_type = SPLHND_PORTMON_TCP;
		fstrcpy(sname, SPL_XCV_MONITOR_TCPMON);
		found = true;
	}
	else if ( strequal( aprinter, SPL_XCV_MONITOR_LOCALMON ) ) {
		Printer->printer_type = SPLHND_PORTMON_LOCAL;
		fstrcpy(sname, SPL_XCV_MONITOR_LOCALMON);
		found = true;
	}

	/*
	 * With hundreds of printers, the "for" loop iterating all
	 * shares can be quite expensive, as it is done on every
	 * OpenPrinter. The loop maps "aprinter" to "sname", the
	 * result of which we cache in gencache.
	 */

	cache_key = talloc_asprintf(talloc_tos(), "PRINTERNAME/%s",
				    aprinter);
	if ((cache_key != NULL) && gencache_get(cache_key, &tmp, NULL)) {

		found = (strcmp(tmp, printer_not_found) != 0);
		if (!found) {
			DEBUG(4, ("Printer %s not found\n", aprinter));
			SAFE_FREE(tmp);
			return WERR_INVALID_PRINTER_NAME;
		}
		fstrcpy(sname, tmp);
		SAFE_FREE(tmp);
	}

	/* Search all sharenames first as this is easier than pulling
	   the printer_info_2 off of disk. Don't use find_service() since
	   that calls out to map_username() */

	/* do another loop to look for printernames */
	for (snum = 0; !found && snum < n_services; snum++) {
		const char *printer = lp_const_servicename(snum);

		/* no point going on if this is not a printer */
		if (!(lp_snum_ok(snum) && lp_print_ok(snum))) {
			continue;
		}

		/* ignore [printers] share */
		if (strequal(printer, "printers")) {
			continue;
		}

		fstrcpy(sname, printer);
		if (strequal(aprinter, printer)) {
			found = true;
			break;
		}

		/* no point looking up the printer object if
		   we aren't allowing printername != sharename */
		if (lp_force_printername(snum)) {
			continue;
		}

		result = winreg_get_printer_internal(mem_ctx,
					    session_info,
					    msg_ctx,
					    sname,
					    &info2);
		if ( !W_ERROR_IS_OK(result) ) {
			DEBUG(2,("set_printer_hnd_name: failed to lookup printer [%s] -- result [%s]\n",
				 sname, win_errstr(result)));
			continue;
		}

		printername = strrchr(info2->printername, '\\');
		if (printername == NULL) {
			printername = info2->printername;
		} else {
			printername++;
		}

		if (strequal(printername, aprinter)) {
			found = true;
			break;
		}

		DEBUGADD(10, ("printername: %s\n", printername));

		TALLOC_FREE(info2);
	}

	if ( !found ) {
		if (cache_key != NULL) {
			gencache_set(cache_key, printer_not_found,
				     time(NULL)+300);
			TALLOC_FREE(cache_key);
		}
		DEBUGADD(4,("Printer not found\n"));
		return WERR_INVALID_PRINTER_NAME;
	}

	if (cache_key != NULL) {
		gencache_set(cache_key, sname, time(NULL)+300);
		TALLOC_FREE(cache_key);
	}

	DEBUGADD(4,("set_printer_hnd_name: Printer found: %s -> %s\n", aprinter, sname));

	fstrcpy(Printer->sharename, sname);

	return WERR_OK;
}

/****************************************************************************
 Find first available printer slot. creates a printer handle for you.
 ****************************************************************************/

static WERROR open_printer_hnd(struct pipes_struct *p,
			       struct policy_handle *hnd,
			       const char *name,
			       uint32_t access_granted)
{
	struct printer_handle *new_printer;
	WERROR result;

	DEBUG(10,("open_printer_hnd: name [%s]\n", name));

	new_printer = talloc_zero(p->mem_ctx, struct printer_handle);
	if (new_printer == NULL) {
		return WERR_NOMEM;
	}
	talloc_set_destructor(new_printer, printer_entry_destructor);

	/* This also steals the printer_handle on the policy_handle */
	if (!create_policy_hnd(p, hnd, new_printer)) {
		TALLOC_FREE(new_printer);
		return WERR_INVALID_HANDLE;
	}

	/* Add to the internal list. */
	DLIST_ADD(printers_list, new_printer);

	new_printer->notify.option=NULL;

	if (!set_printer_hnd_printertype(new_printer, name)) {
		close_printer_handle(p, hnd);
		return WERR_INVALID_HANDLE;
	}

	result = set_printer_hnd_name(p->mem_ctx,
				      get_session_info_system(),
				      p->msg_ctx,
				      new_printer, name);
	if (!W_ERROR_IS_OK(result)) {
		close_printer_handle(p, hnd);
		return result;
	}

	new_printer->access_granted = access_granted;

	DEBUG(5, ("%d printer handles active\n",
		  (int)num_pipe_handles(p)));

	return WERR_OK;
}

/***************************************************************************
 check to see if the client motify handle is monitoring the notification
 given by (notify_type, notify_field).
 **************************************************************************/

static bool is_monitoring_event_flags(uint32_t flags, uint16_t notify_type,
				      uint16_t notify_field)
{
	return true;
}

static bool is_monitoring_event(struct printer_handle *p, uint16_t notify_type,
				uint16_t notify_field)
{
	struct spoolss_NotifyOption *option = p->notify.option;
	uint32_t i, j;

	/*
	 * Flags should always be zero when the change notify
	 * is registered by the client's spooler.  A user Win32 app
	 * might use the flags though instead of the NOTIFY_OPTION_INFO
	 * --jerry
	 */

	if (!option) {
		return false;
	}

	if (p->notify.flags)
		return is_monitoring_event_flags(
			p->notify.flags, notify_type, notify_field);

	for (i = 0; i < option->count; i++) {

		/* Check match for notify_type */

		if (option->types[i].type != notify_type)
			continue;

		/* Check match for field */

		for (j = 0; j < option->types[i].count; j++) {
			if (option->types[i].fields[j].field == notify_field) {
				return true;
			}
		}
	}

	DEBUG(10, ("Open handle for \\\\%s\\%s is not monitoring 0x%02x/0x%02x\n",
		   p->servername, p->sharename, notify_type, notify_field));

	return false;
}

#define SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(_data, _integer) \
	_data->data.integer[0] = _integer; \
	_data->data.integer[1] = 0;


#define SETUP_SPOOLSS_NOTIFY_DATA_STRING(_data, _p) \
	_data->data.string.string = talloc_strdup(mem_ctx, _p); \
	if (!_data->data.string.string) {\
		_data->data.string.size = 0; \
	} \
	_data->data.string.size = strlen_m_term(_p) * 2;

#define SETUP_SPOOLSS_NOTIFY_DATA_DEVMODE(_data, _devmode) \
	_data->data.devmode.devmode = _devmode;

#define SETUP_SPOOLSS_NOTIFY_DATA_SECDESC(_data, _sd) \
	_data->data.sd.sd = dup_sec_desc(mem_ctx, _sd); \
	if (!_data->data.sd.sd) { \
		_data->data.sd.sd_size = 0; \
	} \
	_data->data.sd.sd_size = \
		ndr_size_security_descriptor(_data->data.sd.sd, 0);

static void init_systemtime_buffer(TALLOC_CTX *mem_ctx,
				   struct tm *t,
				   const char **pp,
				   uint32_t *plen)
{
	struct spoolss_Time st;
	uint32_t len = 16;
	char *p;

	if (!init_systemtime(&st, t)) {
		return;
	}

	p = talloc_array(mem_ctx, char, len);
	if (!p) {
		return;
	}

	/*
	 * Systemtime must be linearized as a set of UINT16's.
	 * Fix from Benjamin (Bj) Kuit bj@it.uts.edu.au
	 */

	SSVAL(p, 0, st.year);
	SSVAL(p, 2, st.month);
	SSVAL(p, 4, st.day_of_week);
	SSVAL(p, 6, st.day);
	SSVAL(p, 8, st.hour);
	SSVAL(p, 10, st.minute);
	SSVAL(p, 12, st.second);
	SSVAL(p, 14, st.millisecond);

	*pp = p;
	*plen = len;
}

/* Convert a notification message to a struct spoolss_Notify */

static void notify_one_value(struct spoolss_notify_msg *msg,
			     struct spoolss_Notify *data,
			     TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, msg->notify.value[0]);
}

static void notify_string(struct spoolss_notify_msg *msg,
			  struct spoolss_Notify *data,
			  TALLOC_CTX *mem_ctx)
{
	/* The length of the message includes the trailing \0 */

	data->data.string.size = msg->len * 2;
	data->data.string.string = talloc_strdup(mem_ctx, msg->notify.data);
	if (!data->data.string.string) {
		data->data.string.size = 0;
		return;
	}
}

static void notify_system_time(struct spoolss_notify_msg *msg,
			       struct spoolss_Notify *data,
			       TALLOC_CTX *mem_ctx)
{
	data->data.string.string = NULL;
	data->data.string.size = 0;

	if (msg->len != sizeof(time_t)) {
		DEBUG(5, ("notify_system_time: received wrong sized message (%d)\n",
			  msg->len));
		return;
	}

	init_systemtime_buffer(mem_ctx, gmtime((time_t *)msg->notify.data),
			       &data->data.string.string,
			       &data->data.string.size);
}

struct notify2_message_table {
	const char *name;
	void (*fn)(struct spoolss_notify_msg *msg,
		   struct spoolss_Notify *data, TALLOC_CTX *mem_ctx);
};

static struct notify2_message_table printer_notify_table[] = {
	/* 0x00 */ { "PRINTER_NOTIFY_FIELD_SERVER_NAME", notify_string },
	/* 0x01 */ { "PRINTER_NOTIFY_FIELD_PRINTER_NAME", notify_string },
	/* 0x02 */ { "PRINTER_NOTIFY_FIELD_SHARE_NAME", notify_string },
	/* 0x03 */ { "PRINTER_NOTIFY_FIELD_PORT_NAME", notify_string },
	/* 0x04 */ { "PRINTER_NOTIFY_FIELD_DRIVER_NAME", notify_string },
	/* 0x05 */ { "PRINTER_NOTIFY_FIELD_COMMENT", notify_string },
	/* 0x06 */ { "PRINTER_NOTIFY_FIELD_LOCATION", notify_string },
	/* 0x07 */ { "PRINTER_NOTIFY_FIELD_DEVMODE", NULL },
	/* 0x08 */ { "PRINTER_NOTIFY_FIELD_SEPFILE", notify_string },
	/* 0x09 */ { "PRINTER_NOTIFY_FIELD_PRINT_PROCESSOR", notify_string },
	/* 0x0a */ { "PRINTER_NOTIFY_FIELD_PARAMETERS", NULL },
	/* 0x0b */ { "PRINTER_NOTIFY_FIELD_DATATYPE", notify_string },
	/* 0x0c */ { "PRINTER_NOTIFY_FIELD_SECURITY_DESCRIPTOR", NULL },
	/* 0x0d */ { "PRINTER_NOTIFY_FIELD_ATTRIBUTES", notify_one_value },
	/* 0x0e */ { "PRINTER_NOTIFY_FIELD_PRIORITY", notify_one_value },
	/* 0x0f */ { "PRINTER_NOTIFY_FIELD_DEFAULT_PRIORITY", NULL },
	/* 0x10 */ { "PRINTER_NOTIFY_FIELD_START_TIME", NULL },
	/* 0x11 */ { "PRINTER_NOTIFY_FIELD_UNTIL_TIME", NULL },
	/* 0x12 */ { "PRINTER_NOTIFY_FIELD_STATUS", notify_one_value },
};

static struct notify2_message_table job_notify_table[] = {
	/* 0x00 */ { "JOB_NOTIFY_FIELD_PRINTER_NAME", NULL },
	/* 0x01 */ { "JOB_NOTIFY_FIELD_MACHINE_NAME", NULL },
	/* 0x02 */ { "JOB_NOTIFY_FIELD_PORT_NAME", NULL },
	/* 0x03 */ { "JOB_NOTIFY_FIELD_USER_NAME", notify_string },
	/* 0x04 */ { "JOB_NOTIFY_FIELD_NOTIFY_NAME", NULL },
	/* 0x05 */ { "JOB_NOTIFY_FIELD_DATATYPE", NULL },
	/* 0x06 */ { "JOB_NOTIFY_FIELD_PRINT_PROCESSOR", NULL },
	/* 0x07 */ { "JOB_NOTIFY_FIELD_PARAMETERS", NULL },
	/* 0x08 */ { "JOB_NOTIFY_FIELD_DRIVER_NAME", NULL },
	/* 0x09 */ { "JOB_NOTIFY_FIELD_DEVMODE", NULL },
	/* 0x0a */ { "JOB_NOTIFY_FIELD_STATUS", notify_one_value },
	/* 0x0b */ { "JOB_NOTIFY_FIELD_STATUS_STRING", NULL },
	/* 0x0c */ { "JOB_NOTIFY_FIELD_SECURITY_DESCRIPTOR", NULL },
	/* 0x0d */ { "JOB_NOTIFY_FIELD_DOCUMENT", notify_string },
	/* 0x0e */ { "JOB_NOTIFY_FIELD_PRIORITY", NULL },
	/* 0x0f */ { "JOB_NOTIFY_FIELD_POSITION", NULL },
	/* 0x10 */ { "JOB_NOTIFY_FIELD_SUBMITTED", notify_system_time },
	/* 0x11 */ { "JOB_NOTIFY_FIELD_START_TIME", NULL },
	/* 0x12 */ { "JOB_NOTIFY_FIELD_UNTIL_TIME", NULL },
	/* 0x13 */ { "JOB_NOTIFY_FIELD_TIME", NULL },
	/* 0x14 */ { "JOB_NOTIFY_FIELD_TOTAL_PAGES", notify_one_value },
	/* 0x15 */ { "JOB_NOTIFY_FIELD_PAGES_PRINTED", NULL },
	/* 0x16 */ { "JOB_NOTIFY_FIELD_TOTAL_BYTES", notify_one_value },
	/* 0x17 */ { "JOB_NOTIFY_FIELD_BYTES_PRINTED", NULL },
};


/***********************************************************************
 Allocate talloc context for container object
 **********************************************************************/

static void notify_msg_ctr_init( SPOOLSS_NOTIFY_MSG_CTR *ctr )
{
	if ( !ctr )
		return;

	ctr->ctx = talloc_init("notify_msg_ctr_init %p", ctr);

	return;
}

/***********************************************************************
 release all allocated memory and zero out structure
 **********************************************************************/

static void notify_msg_ctr_destroy( SPOOLSS_NOTIFY_MSG_CTR *ctr )
{
	if ( !ctr )
		return;

	if ( ctr->ctx )
		talloc_destroy(ctr->ctx);

	ZERO_STRUCTP(ctr);

	return;
}

/***********************************************************************
 **********************************************************************/

static TALLOC_CTX* notify_ctr_getctx( SPOOLSS_NOTIFY_MSG_CTR *ctr )
{
	if ( !ctr )
		return NULL;

	return ctr->ctx;
}

/***********************************************************************
 **********************************************************************/

static SPOOLSS_NOTIFY_MSG_GROUP* notify_ctr_getgroup( SPOOLSS_NOTIFY_MSG_CTR *ctr, uint32_t idx )
{
	if ( !ctr || !ctr->msg_groups )
		return NULL;

	if ( idx >= ctr->num_groups )
		return NULL;

	return &ctr->msg_groups[idx];

}

/***********************************************************************
 How many groups of change messages do we have ?
 **********************************************************************/

static int notify_msg_ctr_numgroups( SPOOLSS_NOTIFY_MSG_CTR *ctr )
{
	if ( !ctr )
		return 0;

	return ctr->num_groups;
}

/***********************************************************************
 Add a SPOOLSS_NOTIFY_MSG_CTR to the correct group
 **********************************************************************/

static int notify_msg_ctr_addmsg( SPOOLSS_NOTIFY_MSG_CTR *ctr, SPOOLSS_NOTIFY_MSG *msg )
{
	SPOOLSS_NOTIFY_MSG_GROUP	*groups = NULL;
	SPOOLSS_NOTIFY_MSG_GROUP	*msg_grp = NULL;
	SPOOLSS_NOTIFY_MSG		*msg_list = NULL;
	int				i, new_slot;

	if ( !ctr || !msg )
		return 0;

	/* loop over all groups looking for a matching printer name */

	for ( i=0; i<ctr->num_groups; i++ ) {
		if ( strcmp(ctr->msg_groups[i].printername, msg->printer) == 0 )
			break;
	}

	/* add a new group? */

	if ( i == ctr->num_groups ) {
		ctr->num_groups++;

		if ( !(groups = TALLOC_REALLOC_ARRAY( ctr->ctx, ctr->msg_groups, SPOOLSS_NOTIFY_MSG_GROUP, ctr->num_groups)) ) {
			DEBUG(0,("notify_msg_ctr_addmsg: talloc_realloc() failed!\n"));
			return 0;
		}
		ctr->msg_groups = groups;

		/* clear the new entry and set the printer name */

		ZERO_STRUCT( ctr->msg_groups[ctr->num_groups-1] );
		fstrcpy( ctr->msg_groups[ctr->num_groups-1].printername, msg->printer );
	}

	/* add the change messages; 'i' is the correct index now regardless */

	msg_grp = &ctr->msg_groups[i];

	msg_grp->num_msgs++;

	if ( !(msg_list = TALLOC_REALLOC_ARRAY( ctr->ctx, msg_grp->msgs, SPOOLSS_NOTIFY_MSG, msg_grp->num_msgs )) ) {
		DEBUG(0,("notify_msg_ctr_addmsg: talloc_realloc() failed for new message [%d]!\n", msg_grp->num_msgs));
		return 0;
	}
	msg_grp->msgs = msg_list;

	new_slot = msg_grp->num_msgs-1;
	memcpy( &msg_grp->msgs[new_slot], msg, sizeof(SPOOLSS_NOTIFY_MSG) );

	/* need to allocate own copy of data */

	if ( msg->len != 0 )
		msg_grp->msgs[new_slot].notify.data = (char *)
			TALLOC_MEMDUP( ctr->ctx, msg->notify.data, msg->len );

	return ctr->num_groups;
}

static void construct_info_data(struct spoolss_Notify *info_data,
				enum spoolss_NotifyType type,
				uint16_t field, int id);

/***********************************************************************
 Send a change notication message on all handles which have a call
 back registered
 **********************************************************************/

static int build_notify2_messages(TALLOC_CTX *mem_ctx,
				  struct printer_handle *prn_hnd,
				  SPOOLSS_NOTIFY_MSG *messages,
				  uint32_t num_msgs,
				  struct spoolss_Notify **_notifies,
				  int *_count)
{
	struct spoolss_Notify *notifies;
	SPOOLSS_NOTIFY_MSG *msg;
	int count = 0;
	uint32_t id;
	int i;

	notifies = talloc_zero_array(mem_ctx,
				     struct spoolss_Notify, num_msgs);
	if (!notifies) {
		return ENOMEM;
	}

	for (i = 0; i < num_msgs; i++) {

		msg = &messages[i];

		/* Are we monitoring this event? */

		if (!is_monitoring_event(prn_hnd, msg->type, msg->field)) {
			continue;
		}

		DEBUG(10, ("Sending message type [0x%x] field [0x%2x] "
			   "for printer [%s]\n",
			   msg->type, msg->field, prn_hnd->sharename));

		/*
		 * if the is a printer notification handle and not a job
		 * notification type, then set the id to 0.
		 * Otherwise just use what was specified in the message.
		 *
		 * When registering change notification on a print server
		 * handle we always need to send back the id (snum) matching
		 * the printer for which the change took place.
		 * For change notify registered on a printer handle,
		 * this does not matter and the id should be 0.
		 *
		 * --jerry
		 */

		if ((msg->type == PRINTER_NOTIFY_TYPE) &&
		    (prn_hnd->printer_type == SPLHND_PRINTER)) {
			id = 0;
		} else {
			id = msg->id;
		}

		/* Convert unix jobid to smb jobid */

		if (msg->flags & SPOOLSS_NOTIFY_MSG_UNIX_JOBID) {
			id = sysjob_to_jobid(msg->id);

			if (id == -1) {
				DEBUG(3, ("no such unix jobid %d\n",
					  msg->id));
				continue;
			}
		}

		construct_info_data(&notifies[count],
				    msg->type, msg->field, id);

		switch(msg->type) {
		case PRINTER_NOTIFY_TYPE:
			if (printer_notify_table[msg->field].fn) {
				printer_notify_table[msg->field].fn(msg,
						&notifies[count], mem_ctx);
			}
			break;

		case JOB_NOTIFY_TYPE:
			if (job_notify_table[msg->field].fn) {
				job_notify_table[msg->field].fn(msg,
						&notifies[count], mem_ctx);
			}
			break;

		default:
			DEBUG(5, ("Unknown notification type %d\n",
				  msg->type));
			continue;
		}

		count++;
	}

	*_notifies = notifies;
	*_count = count;

	return 0;
}

static int send_notify2_printer(TALLOC_CTX *mem_ctx,
				struct printer_handle *prn_hnd,
				SPOOLSS_NOTIFY_MSG_GROUP *msg_group)
{
	struct spoolss_Notify *notifies;
	int count = 0;
	union spoolss_ReplyPrinterInfo info;
	struct spoolss_NotifyInfo info0;
	uint32_t reply_result;
	NTSTATUS status;
	WERROR werr;
	int ret;

	/* Is there notification on this handle? */
	if (prn_hnd->notify.cli_chan == NULL ||
	    prn_hnd->notify.cli_chan->active_connections == 0) {
		return 0;
	}

	DEBUG(10, ("Client connected! [\\\\%s\\%s]\n",
		   prn_hnd->servername, prn_hnd->sharename));

	/* For this printer? Print servers always receive notifications. */
	if ((prn_hnd->printer_type == SPLHND_PRINTER)  &&
	    (!strequal(msg_group->printername, prn_hnd->sharename))) {
		return 0;
	}

	DEBUG(10,("Our printer\n"));

	/* build the array of change notifications */
	ret = build_notify2_messages(mem_ctx, prn_hnd,
				     msg_group->msgs,
				     msg_group->num_msgs,
				     &notifies, &count);
	if (ret) {
		return ret;
	}

	info0.version	= 0x2;
	info0.flags	= count ? 0x00020000 /* ??? */ : PRINTER_NOTIFY_INFO_DISCARDED;
	info0.count	= count;
	info0.notifies	= notifies;

	info.info0 = &info0;

	status = dcerpc_spoolss_RouterReplyPrinterEx(
				prn_hnd->notify.cli_chan->binding_handle,
				mem_ctx,
				&prn_hnd->notify.cli_hnd,
				prn_hnd->notify.change, /* color */
				prn_hnd->notify.flags,
				&reply_result,
				0, /* reply_type, must be 0 */
				info, &werr);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("dcerpc_spoolss_RouterReplyPrinterEx to client: %s "
			  "failed: %s\n",
			  prn_hnd->notify.cli_chan->cli_pipe->srv_name_slash,
			  nt_errstr(status)));
		werr = ntstatus_to_werror(status);
	} else if (!W_ERROR_IS_OK(werr)) {
		DEBUG(1, ("RouterReplyPrinterEx to client: %s "
			  "failed: %s\n",
			  prn_hnd->notify.cli_chan->cli_pipe->srv_name_slash,
			  win_errstr(werr)));
	}
	switch (reply_result) {
	case 0:
		break;
	case PRINTER_NOTIFY_INFO_DISCARDED:
	case PRINTER_NOTIFY_INFO_DISCARDNOTED:
	case PRINTER_NOTIFY_INFO_COLOR_MISMATCH:
		break;
	default:
		break;
	}

	return 0;
}

static void send_notify2_changes( SPOOLSS_NOTIFY_MSG_CTR *ctr, uint32_t idx )
{
	struct printer_handle 	 *p;
	TALLOC_CTX		 *mem_ctx = notify_ctr_getctx( ctr );
	SPOOLSS_NOTIFY_MSG_GROUP *msg_group = notify_ctr_getgroup( ctr, idx );
	int ret;

	if ( !msg_group ) {
		DEBUG(5,("send_notify2_changes() called with no msg group!\n"));
		return;
	}

	if (!msg_group->msgs) {
		DEBUG(5, ("send_notify2_changes() called with no messages!\n"));
		return;
	}

	DEBUG(8,("send_notify2_changes: Enter...[%s]\n", msg_group->printername));

	/* loop over all printers */

	for (p = printers_list; p; p = p->next) {
		ret = send_notify2_printer(mem_ctx, p, msg_group);
		if (ret) {
			goto done;
		}
	}

done:
	DEBUG(8,("send_notify2_changes: Exit...\n"));
	return;
}

/***********************************************************************
 **********************************************************************/

static bool notify2_unpack_msg( SPOOLSS_NOTIFY_MSG *msg, struct timeval *tv, void *buf, size_t len )
{

	uint32_t tv_sec, tv_usec;
	size_t offset = 0;

	/* Unpack message */

	offset += tdb_unpack((uint8_t *)buf + offset, len - offset, "f",
			     msg->printer);

	offset += tdb_unpack((uint8_t *)buf + offset, len - offset, "ddddddd",
				&tv_sec, &tv_usec,
				&msg->type, &msg->field, &msg->id, &msg->len, &msg->flags);

	if (msg->len == 0)
		tdb_unpack((uint8_t *)buf + offset, len - offset, "dd",
			   &msg->notify.value[0], &msg->notify.value[1]);
	else
		tdb_unpack((uint8_t *)buf + offset, len - offset, "B",
			   &msg->len, &msg->notify.data);

	DEBUG(3, ("notify2_unpack_msg: got NOTIFY2 message for printer %s, jobid %u type %d, field 0x%02x, flags 0x%04x\n",
		  msg->printer, (unsigned int)msg->id, msg->type, msg->field, msg->flags));

	tv->tv_sec = tv_sec;
	tv->tv_usec = tv_usec;

	if (msg->len == 0)
		DEBUG(3, ("notify2_unpack_msg: value1 = %d, value2 = %d\n", msg->notify.value[0],
			  msg->notify.value[1]));
	else
		dump_data(3, (uint8_t *)msg->notify.data, msg->len);

	return true;
}

/********************************************************************
 Receive a notify2 message list
 ********************************************************************/

static void receive_notify2_message_list(struct messaging_context *msg,
					 void *private_data,
					 uint32_t msg_type,
					 struct server_id server_id,
					 DATA_BLOB *data)
{
	size_t 			msg_count, i;
	char 			*buf = (char *)data->data;
	char 			*msg_ptr;
	size_t 			msg_len;
	SPOOLSS_NOTIFY_MSG	notify;
	SPOOLSS_NOTIFY_MSG_CTR	messages;
	int			num_groups;

	if (data->length < 4) {
		DEBUG(0,("receive_notify2_message_list: bad message format (len < 4)!\n"));
		return;
	}

	msg_count = IVAL(buf, 0);
	msg_ptr = buf + 4;

	DEBUG(5, ("receive_notify2_message_list: got %lu messages in list\n", (unsigned long)msg_count));

	if (msg_count == 0) {
		DEBUG(0,("receive_notify2_message_list: bad message format (msg_count == 0) !\n"));
		return;
	}

	/* initialize the container */

	ZERO_STRUCT( messages );
	notify_msg_ctr_init( &messages );

	/*
	 * build message groups for each printer identified
	 * in a change_notify msg.  Remember that a PCN message
	 * includes the handle returned for the srv_spoolss_replyopenprinter()
	 * call.  Therefore messages are grouped according to printer handle.
	 */

	for ( i=0; i<msg_count; i++ ) {
		struct timeval msg_tv;

		if (msg_ptr + 4 - buf > data->length) {
			DEBUG(0,("receive_notify2_message_list: bad message format (len > buf_size) !\n"));
			return;
		}

		msg_len = IVAL(msg_ptr,0);
		msg_ptr += 4;

		if (msg_ptr + msg_len - buf > data->length) {
			DEBUG(0,("receive_notify2_message_list: bad message format (bad len) !\n"));
			return;
		}

		/* unpack messages */

		ZERO_STRUCT( notify );
		notify2_unpack_msg( &notify, &msg_tv, msg_ptr, msg_len );
		msg_ptr += msg_len;

		/* add to correct list in container */

		notify_msg_ctr_addmsg( &messages, &notify );

		/* free memory that might have been allocated by notify2_unpack_msg() */

		if ( notify.len != 0 )
			SAFE_FREE( notify.notify.data );
	}

	/* process each group of messages */

	num_groups = notify_msg_ctr_numgroups( &messages );
	for ( i=0; i<num_groups; i++ )
		send_notify2_changes( &messages, i );


	/* cleanup */

	DEBUG(10,("receive_notify2_message_list: processed %u messages\n",
		(uint32_t)msg_count ));

	notify_msg_ctr_destroy( &messages );

	return;
}

/********************************************************************
 Send a message to ourself about new driver being installed
 so we can upgrade the information for each printer bound to this
 driver
 ********************************************************************/

static bool srv_spoolss_drv_upgrade_printer(const char *drivername,
					    struct messaging_context *msg_ctx)
{
	int len = strlen(drivername);

	if (!len)
		return false;

	DEBUG(10,("srv_spoolss_drv_upgrade_printer: Sending message about driver upgrade [%s]\n",
		drivername));

	messaging_send_buf(msg_ctx, messaging_server_id(msg_ctx),
			   MSG_PRINTER_DRVUPGRADE,
			   (uint8_t *)drivername, len+1);

	return true;
}

void srv_spoolss_cleanup(void)
{
	struct printer_session_counter *session_counter;

	for (session_counter = counter_list;
	     session_counter != NULL;
	     session_counter = counter_list) {
		DLIST_REMOVE(counter_list, session_counter);
		TALLOC_FREE(session_counter);
	}
}

/**********************************************************************
 callback to receive a MSG_PRINTER_DRVUPGRADE message and interate
 over all printers, upgrading ones as necessary
 **********************************************************************/

void do_drv_upgrade_printer(struct messaging_context *msg,
			    void *private_data,
			    uint32_t msg_type,
			    struct server_id server_id,
			    DATA_BLOB *data)
{
	TALLOC_CTX *tmp_ctx;
	struct auth_serversupplied_info *session_info = NULL;
	struct spoolss_PrinterInfo2 *pinfo2;
	NTSTATUS status;
	WERROR result;
	const char *drivername;
	int snum;
	int n_services = lp_numservices();
	struct dcerpc_binding_handle *b = NULL;

	tmp_ctx = talloc_new(NULL);
	if (!tmp_ctx) return;

	status = make_session_info_system(tmp_ctx, &session_info);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("do_drv_upgrade_printer: "
			  "Could not create system session_info\n"));
		goto done;
	}

	drivername = talloc_strndup(tmp_ctx, (const char *)data->data, data->length);
	if (!drivername) {
		DEBUG(0, ("do_drv_upgrade_printer: Out of memoery ?!\n"));
		goto done;
	}

	DEBUG(10, ("do_drv_upgrade_printer: "
		   "Got message for new driver [%s]\n", drivername));

	/* Iterate the printer list */

	for (snum = 0; snum < n_services; snum++) {
		if (!lp_snum_ok(snum) || !lp_print_ok(snum)) {
			continue;
		}

		/* ignore [printers] share */
		if (strequal(lp_const_servicename(snum), "printers")) {
			continue;
		}

		if (b == NULL) {
			result = winreg_printer_binding_handle(tmp_ctx,
							       session_info,
							       msg,
							       &b);
			if (!W_ERROR_IS_OK(result)) {
				break;
			}
		}

		result = winreg_get_printer(tmp_ctx, b,
					    lp_const_servicename(snum),
					    &pinfo2);

		if (!W_ERROR_IS_OK(result)) {
			continue;
		}

		if (!pinfo2->drivername) {
			continue;
		}

		if (strcmp(drivername, pinfo2->drivername) != 0) {
			continue;
		}

		DEBUG(6,("Updating printer [%s]\n", pinfo2->printername));

		/* all we care about currently is the change_id */
		result = winreg_printer_update_changeid(tmp_ctx, b,
							pinfo2->printername);

		if (!W_ERROR_IS_OK(result)) {
			DEBUG(3, ("do_drv_upgrade_printer: "
				  "Failed to update changeid [%s]\n",
				  win_errstr(result)));
		}
	}

	/* all done */
done:
	talloc_free(tmp_ctx);
}

/********************************************************************
 Update the cache for all printq's with a registered client
 connection
 ********************************************************************/

void update_monitored_printq_cache(struct messaging_context *msg_ctx)
{
	struct printer_handle *printer = printers_list;
	int snum;

	/* loop through all printers and update the cache where
	   a client is connected */
	while (printer) {
		if ((printer->printer_type == SPLHND_PRINTER) &&
		    ((printer->notify.cli_chan != NULL) &&
		     (printer->notify.cli_chan->active_connections > 0))) {
			snum = print_queue_snum(printer->sharename);
			print_queue_status(msg_ctx, snum, NULL, NULL);
		}

		printer = printer->next;
	}

	return;
}

/****************************************************************
 _spoolss_OpenPrinter
****************************************************************/

WERROR _spoolss_OpenPrinter(struct pipes_struct *p,
			    struct spoolss_OpenPrinter *r)
{
	struct spoolss_OpenPrinterEx e;
	WERROR werr;

	ZERO_STRUCT(e.in.userlevel);

	e.in.printername	= r->in.printername;
	e.in.datatype		= r->in.datatype;
	e.in.devmode_ctr	= r->in.devmode_ctr;
	e.in.access_mask	= r->in.access_mask;
	e.in.level		= 0;

	e.out.handle		= r->out.handle;

	werr = _spoolss_OpenPrinterEx(p, &e);

	if (W_ERROR_EQUAL(werr, WERR_INVALID_PARAM)) {
		/* OpenPrinterEx returns this for a bad
		 * printer name. We must return WERR_INVALID_PRINTER_NAME
		 * instead.
		 */
		werr = WERR_INVALID_PRINTER_NAME;
	}

	return werr;
}

static WERROR copy_devicemode(TALLOC_CTX *mem_ctx,
			      struct spoolss_DeviceMode *orig,
			      struct spoolss_DeviceMode **dest)
{
	struct spoolss_DeviceMode *dm;

	dm = talloc(mem_ctx, struct spoolss_DeviceMode);
	if (!dm) {
		return WERR_NOMEM;
	}

	/* copy all values, then duplicate strings and structs */
	*dm = *orig;

	dm->devicename = talloc_strdup(dm, orig->devicename);
	if (!dm->devicename) {
		return WERR_NOMEM;
	}
	dm->formname = talloc_strdup(dm, orig->formname);
	if (!dm->formname) {
		return WERR_NOMEM;
	}
	if (orig->driverextra_data.data) {
		dm->driverextra_data.data =
			(uint8_t *) talloc_memdup(dm, orig->driverextra_data.data,
					orig->driverextra_data.length);
		if (!dm->driverextra_data.data) {
			return WERR_NOMEM;
		}
	}

	*dest = dm;
	return WERR_OK;
}

/****************************************************************
 _spoolss_OpenPrinterEx
****************************************************************/

WERROR _spoolss_OpenPrinterEx(struct pipes_struct *p,
			      struct spoolss_OpenPrinterEx *r)
{
	int snum;
	struct printer_handle *Printer=NULL;
	WERROR result;

	if (!r->in.printername) {
		return WERR_INVALID_PARAM;
	}

	if (r->in.level > 3) {
		return WERR_INVALID_PARAM;
	}
	if ((r->in.level == 1 && !r->in.userlevel.level1) ||
	    (r->in.level == 2 && !r->in.userlevel.level2) ||
	    (r->in.level == 3 && !r->in.userlevel.level3)) {
		return WERR_INVALID_PARAM;
	}

	/* some sanity check because you can open a printer or a print server */
	/* aka: \\server\printer or \\server */

	DEBUGADD(3,("checking name: %s\n", r->in.printername));

	result = open_printer_hnd(p, r->out.handle, r->in.printername, 0);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(3,("_spoolss_OpenPrinterEx: Cannot open a printer handle "
			"for printer %s\n", r->in.printername));
		ZERO_STRUCTP(r->out.handle);
		return result;
	}

	Printer = find_printer_index_by_hnd(p, r->out.handle);
	if ( !Printer ) {
		DEBUG(0,("_spoolss_OpenPrinterEx: logic error.  Can't find printer "
			"handle we created for printer %s\n", r->in.printername));
		close_printer_handle(p, r->out.handle);
		ZERO_STRUCTP(r->out.handle);
		return WERR_INVALID_PARAM;
	}

	/*
	 * First case: the user is opening the print server:
	 *
	 * Disallow MS AddPrinterWizard if parameter disables it. A Win2k
	 * client 1st tries an OpenPrinterEx with access==0, MUST be allowed.
	 *
	 * Then both Win2k and WinNT clients try an OpenPrinterEx with
	 * SERVER_ALL_ACCESS, which we allow only if the user is root (uid=0)
	 * or if the user is listed in the smb.conf printer admin parameter.
	 *
	 * Then they try OpenPrinterEx with SERVER_READ which we allow. This lets the
	 * client view printer folder, but does not show the MSAPW.
	 *
	 * Note: this test needs code to check access rights here too. Jeremy
	 * could you look at this?
	 *
	 * Second case: the user is opening a printer:
	 * NT doesn't let us connect to a printer if the connecting user
	 * doesn't have print permission.
	 *
	 * Third case: user is opening a Port Monitor
	 * access checks same as opening a handle to the print server.
	 */

	switch (Printer->printer_type )
	{
	case SPLHND_SERVER:
	case SPLHND_PORTMON_TCP:
	case SPLHND_PORTMON_LOCAL:
		/* Printserver handles use global struct... */

		snum = -1;

		/* Map standard access rights to object specific access rights */

		se_map_standard(&r->in.access_mask,
				&printserver_std_mapping);

		/* Deny any object specific bits that don't apply to print
		   servers (i.e printer and job specific bits) */

		r->in.access_mask &= SEC_MASK_SPECIFIC;

		if (r->in.access_mask &
		    ~(SERVER_ACCESS_ADMINISTER | SERVER_ACCESS_ENUMERATE)) {
			DEBUG(3, ("access DENIED for non-printserver bits\n"));
			close_printer_handle(p, r->out.handle);
			ZERO_STRUCTP(r->out.handle);
			return WERR_ACCESS_DENIED;
		}

		/* Allow admin access */

		if ( r->in.access_mask & SERVER_ACCESS_ADMINISTER )
		{
			if (!lp_ms_add_printer_wizard()) {
				close_printer_handle(p, r->out.handle);
				ZERO_STRUCTP(r->out.handle);
				return WERR_ACCESS_DENIED;
			}

			/* if the user is not root, doesn't have SE_PRINT_OPERATOR privilege,
			   and not a printer admin, then fail */

			if ((p->session_info->utok.uid != sec_initial_uid()) &&
			    !security_token_has_privilege(p->session_info->security_token, SEC_PRIV_PRINT_OPERATOR) &&
			    !nt_token_check_sid(&global_sid_Builtin_Print_Operators, p->session_info->security_token) &&
			    !token_contains_name_in_list(
				    uidtoname(p->session_info->utok.uid),
				    p->session_info->info3->base.domain.string,
				    NULL,
				    p->session_info->security_token,
				    lp_printer_admin(snum))) {
				close_printer_handle(p, r->out.handle);
				ZERO_STRUCTP(r->out.handle);
				DEBUG(3,("access DENIED as user is not root, "
					"has no printoperator privilege, "
					"not a member of the printoperator builtin group and "
					"is not in printer admin list"));
				return WERR_ACCESS_DENIED;
			}

			r->in.access_mask = SERVER_ACCESS_ADMINISTER;
		}
		else
		{
			r->in.access_mask = SERVER_ACCESS_ENUMERATE;
		}

		DEBUG(4,("Setting print server access = %s\n", (r->in.access_mask == SERVER_ACCESS_ADMINISTER)
			? "SERVER_ACCESS_ADMINISTER" : "SERVER_ACCESS_ENUMERATE" ));

		/* We fall through to return WERR_OK */
		break;

	case SPLHND_PRINTER:
		/* NT doesn't let us connect to a printer if the connecting user
		   doesn't have print permission.  */

		if (!get_printer_snum(p, r->out.handle, &snum, NULL)) {
			close_printer_handle(p, r->out.handle);
			ZERO_STRUCTP(r->out.handle);
			return WERR_BADFID;
		}

		if (r->in.access_mask == SEC_FLAG_MAXIMUM_ALLOWED) {
			r->in.access_mask = PRINTER_ACCESS_ADMINISTER;
		}

		se_map_standard(&r->in.access_mask, &printer_std_mapping);

		/* map an empty access mask to the minimum access mask */
		if (r->in.access_mask == 0x0)
			r->in.access_mask = PRINTER_ACCESS_USE;

		/*
		 * If we are not serving the printer driver for this printer,
		 * map PRINTER_ACCESS_ADMINISTER to PRINTER_ACCESS_USE.  This
		 * will keep NT clients happy  --jerry
		 */

		if (lp_use_client_driver(snum)
			&& (r->in.access_mask & PRINTER_ACCESS_ADMINISTER))
		{
			r->in.access_mask = PRINTER_ACCESS_USE;
		}

		/* check smb.conf parameters and the the sec_desc */

		if (!allow_access(lp_hostsdeny(snum), lp_hostsallow(snum),
				  p->client_id->name, p->client_id->addr)) {
			DEBUG(3, ("access DENIED (hosts allow/deny) for printer open\n"));
			ZERO_STRUCTP(r->out.handle);
			return WERR_ACCESS_DENIED;
		}

		if (!user_ok_token(uidtoname(p->session_info->utok.uid), NULL,
				   p->session_info->security_token, snum) ||
		    !print_access_check(p->session_info,
					p->msg_ctx,
					snum,
					r->in.access_mask)) {
			DEBUG(3, ("access DENIED for printer open\n"));
			close_printer_handle(p, r->out.handle);
			ZERO_STRUCTP(r->out.handle);
			return WERR_ACCESS_DENIED;
		}

		if ((r->in.access_mask & SEC_MASK_SPECIFIC)& ~(PRINTER_ACCESS_ADMINISTER|PRINTER_ACCESS_USE)) {
			DEBUG(3, ("access DENIED for printer open - unknown bits\n"));
			close_printer_handle(p, r->out.handle);
			ZERO_STRUCTP(r->out.handle);
			return WERR_ACCESS_DENIED;
		}

		if (r->in.access_mask & PRINTER_ACCESS_ADMINISTER)
			r->in.access_mask = PRINTER_ACCESS_ADMINISTER;
		else
			r->in.access_mask = PRINTER_ACCESS_USE;

		DEBUG(4,("Setting printer access = %s\n", (r->in.access_mask == PRINTER_ACCESS_ADMINISTER)
			? "PRINTER_ACCESS_ADMINISTER" : "PRINTER_ACCESS_USE" ));

		winreg_create_printer_internal(p->mem_ctx,
				      get_session_info_system(),
				      p->msg_ctx,
				      lp_const_servicename(snum));

		break;

	default:
		/* sanity check to prevent programmer error */
		ZERO_STRUCTP(r->out.handle);
		return WERR_BADFID;
	}

	Printer->access_granted = r->in.access_mask;

	/*
	 * If the client sent a devmode in the OpenPrinter() call, then
	 * save it here in case we get a job submission on this handle
	 */

	 if ((Printer->printer_type != SPLHND_SERVER)
	  && (r->in.devmode_ctr.devmode != NULL)) {
		copy_devicemode(NULL, r->in.devmode_ctr.devmode,
				&Printer->devmode);
	 }

	return WERR_OK;
}

/****************************************************************
 _spoolss_ClosePrinter
****************************************************************/

WERROR _spoolss_ClosePrinter(struct pipes_struct *p,
			     struct spoolss_ClosePrinter *r)
{
	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);

	if (Printer && Printer->document_started) {
		struct spoolss_EndDocPrinter e;

		e.in.handle = r->in.handle;

		_spoolss_EndDocPrinter(p, &e);
	}

	if (!close_printer_handle(p, r->in.handle))
		return WERR_BADFID;

	/* clear the returned printer handle.  Observed behavior
	   from Win2k server.  Don't think this really matters.
	   Previous code just copied the value of the closed
	   handle.    --jerry */

	ZERO_STRUCTP(r->out.handle);

	return WERR_OK;
}

/****************************************************************
 _spoolss_DeletePrinter
****************************************************************/

WERROR _spoolss_DeletePrinter(struct pipes_struct *p,
			      struct spoolss_DeletePrinter *r)
{
	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);
	WERROR result;
	int snum;

	if (Printer && Printer->document_started) {
		struct spoolss_EndDocPrinter e;

		e.in.handle = r->in.handle;

		_spoolss_EndDocPrinter(p, &e);
	}

	if (get_printer_snum(p, r->in.handle, &snum, NULL)) {
		winreg_delete_printer_key_internal(p->mem_ctx,
					  get_session_info_system(),
					  p->msg_ctx,
					  lp_const_servicename(snum),
					  "");
	}

	result = delete_printer_handle(p, r->in.handle);

	return result;
}

/*******************************************************************
 * static function to lookup the version id corresponding to an
 * long architecture string
 ******************************************************************/

static const struct print_architecture_table_node archi_table[]= {

	{"Windows 4.0",          SPL_ARCH_WIN40,	0 },
	{"Windows NT x86",       SPL_ARCH_W32X86,	2 },
	{"Windows NT R4000",     SPL_ARCH_W32MIPS,	2 },
	{"Windows NT Alpha_AXP", SPL_ARCH_W32ALPHA,	2 },
	{"Windows NT PowerPC",   SPL_ARCH_W32PPC,	2 },
	{"Windows IA64",   	 SPL_ARCH_IA64,		3 },
	{"Windows x64",   	 SPL_ARCH_X64,		3 },
	{NULL,                   "",		-1 }
};

static const int drv_cversion[] = {SPOOLSS_DRIVER_VERSION_9X,
				   SPOOLSS_DRIVER_VERSION_NT35,
				   SPOOLSS_DRIVER_VERSION_NT4,
				   SPOOLSS_DRIVER_VERSION_200X,
				   -1};

static int get_version_id(const char *arch)
{
	int i;

	for (i=0; archi_table[i].long_archi != NULL; i++)
	{
		if (strcmp(arch, archi_table[i].long_archi) == 0)
			return (archi_table[i].version);
        }

	return -1;
}

/****************************************************************
 _spoolss_DeletePrinterDriver
****************************************************************/

WERROR _spoolss_DeletePrinterDriver(struct pipes_struct *p,
				    struct spoolss_DeletePrinterDriver *r)
{

	struct spoolss_DriverInfo8 *info = NULL;
	int				version;
	WERROR				status;
	struct dcerpc_binding_handle *b;
	TALLOC_CTX *tmp_ctx = NULL;
	int i;
	bool found;

	/* if the user is not root, doesn't have SE_PRINT_OPERATOR privilege,
	   and not a printer admin, then fail */

	if ( (p->session_info->utok.uid != sec_initial_uid())
	     && !security_token_has_privilege(p->session_info->security_token, SEC_PRIV_PRINT_OPERATOR)
		&& !token_contains_name_in_list(
			uidtoname(p->session_info->utok.uid),
			p->session_info->info3->base.domain.string,
			NULL,
			p->session_info->security_token,
			lp_printer_admin(-1)) )
	{
		return WERR_ACCESS_DENIED;
	}

	/* check that we have a valid driver name first */

	if ((version = get_version_id(r->in.architecture)) == -1)
		return WERR_INVALID_ENVIRONMENT;

	tmp_ctx = talloc_new(p->mem_ctx);
	if (!tmp_ctx) {
		return WERR_NOMEM;
	}

	status = winreg_printer_binding_handle(tmp_ctx,
					       get_session_info_system(),
					       p->msg_ctx,
					       &b);
	if (!W_ERROR_IS_OK(status)) {
		goto done;
	}

	for (found = false, i = 0; drv_cversion[i] >= 0; i++) {
		status = winreg_get_driver(tmp_ctx, b,
					   r->in.architecture, r->in.driver,
					   drv_cversion[i], &info);
		if (!W_ERROR_IS_OK(status)) {
			DEBUG(5, ("skipping del of driver with version %d\n",
				  drv_cversion[i]));
			continue;
		}
		found = true;

		if (printer_driver_in_use(tmp_ctx, get_session_info_system(),
					  p->msg_ctx, info)) {
			status = WERR_PRINTER_DRIVER_IN_USE;
			goto done;
		}

		status = winreg_del_driver(tmp_ctx, b, info, drv_cversion[i]);
		if (!W_ERROR_IS_OK(status)) {
			DEBUG(0, ("failed del of driver with version %d\n",
				  drv_cversion[i]));
			goto done;
		}
	}
	if (found == false) {
		DEBUG(0, ("driver %s not found for deletion\n", r->in.driver));
		status = WERR_UNKNOWN_PRINTER_DRIVER;
	} else {
		status = WERR_OK;
	}

done:
	talloc_free(tmp_ctx);
	return status;
}

static WERROR spoolss_dpd_version(TALLOC_CTX *mem_ctx,
				  struct pipes_struct *p,
				  struct spoolss_DeletePrinterDriverEx *r,
				  struct dcerpc_binding_handle *b,
				  struct spoolss_DriverInfo8 *info)
{
	WERROR status;
	bool delete_files;

	if (printer_driver_in_use(mem_ctx, get_session_info_system(),
				  p->msg_ctx, info)) {
		status = WERR_PRINTER_DRIVER_IN_USE;
		goto done;
	}

	/*
	 * we have a couple of cases to consider.
	 * (1) Are any files in use?  If so and DPD_DELETE_ALL_FILES is set,
	 *     then the delete should fail if **any** files overlap with
	 *     other drivers
	 * (2) If DPD_DELETE_UNUSED_FILES is set, then delete all
	 *     non-overlapping files
	 * (3) If neither DPD_DELETE_ALL_FILES nor DPD_DELETE_UNUSED_FILES
	 *     is set, then do not delete any files
	 * Refer to MSDN docs on DeletePrinterDriverEx() for details.
	 */

	delete_files = r->in.delete_flags
			& (DPD_DELETE_ALL_FILES | DPD_DELETE_UNUSED_FILES);

	if (delete_files) {
		bool in_use = printer_driver_files_in_use(mem_ctx,
						get_session_info_system(),
							  p->msg_ctx,
							  info);
		if (in_use && (r->in.delete_flags & DPD_DELETE_ALL_FILES)) {
			status = WERR_PRINTER_DRIVER_IN_USE;
			goto done;
		}
		/*
		 * printer_driver_files_in_use() has trimmed overlapping files
		 * from info so they are not removed on DPD_DELETE_UNUSED_FILES
		 */
	}

	status = winreg_del_driver(mem_ctx, b, info, info->version);
	if (!W_ERROR_IS_OK(status)) {
		goto done;
	}

	/*
	 * now delete any associated files if delete_files is
	 * true. Even if this part failes, we return succes
	 * because the driver doesn not exist any more
	 */
	if (delete_files) {
		delete_driver_files(get_session_info_system(), info);
	}

done:
	return status;
}

/****************************************************************
 _spoolss_DeletePrinterDriverEx
****************************************************************/

WERROR _spoolss_DeletePrinterDriverEx(struct pipes_struct *p,
				      struct spoolss_DeletePrinterDriverEx *r)
{
	struct spoolss_DriverInfo8 *info = NULL;
	WERROR				status;
	struct dcerpc_binding_handle *b;
	TALLOC_CTX *tmp_ctx = NULL;
	int i;
	bool found;

	/* if the user is not root, doesn't have SE_PRINT_OPERATOR privilege,
	   and not a printer admin, then fail */

	if ( (p->session_info->utok.uid != sec_initial_uid())
		&& !security_token_has_privilege(p->session_info->security_token, SEC_PRIV_PRINT_OPERATOR)
		&& !token_contains_name_in_list(
			uidtoname(p->session_info->utok.uid),
			p->session_info->info3->base.domain.string,
			NULL,
			p->session_info->security_token, lp_printer_admin(-1)) )
	{
		return WERR_ACCESS_DENIED;
	}

	/* check that we have a valid driver name first */
	if (get_version_id(r->in.architecture) == -1) {
		/* this is what NT returns */
		return WERR_INVALID_ENVIRONMENT;
	}

	tmp_ctx = talloc_new(p->mem_ctx);
	if (!tmp_ctx) {
		return WERR_NOMEM;
	}

	status = winreg_printer_binding_handle(tmp_ctx,
					       get_session_info_system(),
					       p->msg_ctx,
					       &b);
	if (!W_ERROR_IS_OK(status)) {
		goto done;
	}

	for (found = false, i = 0; drv_cversion[i] >= 0; i++) {
		if ((r->in.delete_flags & DPD_DELETE_SPECIFIC_VERSION)
		 && (drv_cversion[i] != r->in.version)) {
			continue;
		}

		/* check if a driver with this version exists before delete */
		status = winreg_get_driver(tmp_ctx, b,
					   r->in.architecture, r->in.driver,
					   drv_cversion[i], &info);
		if (!W_ERROR_IS_OK(status)) {
			DEBUG(5, ("skipping del of driver with version %d\n",
				  drv_cversion[i]));
			continue;
		}
		found = true;

		status = spoolss_dpd_version(tmp_ctx, p, r, b, info);
		if (!W_ERROR_IS_OK(status)) {
			DEBUG(0, ("failed to delete driver with version %d\n",
				  drv_cversion[i]));
			goto done;
		}
	}
	if (found == false) {
		DEBUG(0, ("driver %s not found for deletion\n", r->in.driver));
		status = WERR_UNKNOWN_PRINTER_DRIVER;
	} else {
		status = WERR_OK;
	}

done:
	talloc_free(tmp_ctx);
	return status;
}


/********************************************************************
 GetPrinterData on a printer server Handle.
********************************************************************/

static WERROR getprinterdata_printer_server(TALLOC_CTX *mem_ctx,
					    const char *value,
					    enum winreg_Type *type,
					    union spoolss_PrinterData *data)
{
	DEBUG(8,("getprinterdata_printer_server:%s\n", value));

	if (!StrCaseCmp(value, "W3SvcInstalled")) {
		*type = REG_DWORD;
		data->value = 0x00;
		return WERR_OK;
	}

	if (!StrCaseCmp(value, "BeepEnabled")) {
		*type = REG_DWORD;
		data->value = 0x00;
		return WERR_OK;
	}

	if (!StrCaseCmp(value, "EventLog")) {
		*type = REG_DWORD;
		/* formally was 0x1b */
		data->value = 0x00;
		return WERR_OK;
	}

	if (!StrCaseCmp(value, "NetPopup")) {
		*type = REG_DWORD;
		data->value = 0x00;
		return WERR_OK;
	}

	if (!StrCaseCmp(value, "MajorVersion")) {
		*type = REG_DWORD;

		/* Windows NT 4.0 seems to not allow uploading of drivers
		   to a server that reports 0x3 as the MajorVersion.
		   need to investigate more how Win2k gets around this .
		   -- jerry */

		if (RA_WINNT == get_remote_arch()) {
			data->value = 0x02;
		} else {
			data->value = 0x03;
		}

		return WERR_OK;
	}

	if (!StrCaseCmp(value, "MinorVersion")) {
		*type = REG_DWORD;
		data->value = 0x00;
		return WERR_OK;
	}

	/* REG_BINARY
	 *  uint32_t size 	 = 0x114
	 *  uint32_t major	 = 5
	 *  uint32_t minor	 = [0|1]
	 *  uint32_t build 	 = [2195|2600]
	 *  extra unicode string = e.g. "Service Pack 3"
	 */
	if (!StrCaseCmp(value, "OSVersion")) {
		DATA_BLOB blob;
		enum ndr_err_code ndr_err;
		struct spoolss_OSVersion os;

		os.major		= 5;	/* Windows 2000 == 5.0 */
		os.minor		= 0;
		os.build		= 2195;	/* build */
		os.extra_string		= "";	/* leave extra string empty */

		ndr_err = ndr_push_struct_blob(&blob, mem_ctx, &os,
			(ndr_push_flags_fn_t)ndr_push_spoolss_OSVersion);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return WERR_GENERAL_FAILURE;
		}

		*type = REG_BINARY;
		data->binary = blob;

		return WERR_OK;
	}


	if (!StrCaseCmp(value, "DefaultSpoolDirectory")) {
		*type = REG_SZ;

		data->string = talloc_strdup(mem_ctx, "C:\\PRINTERS");
		W_ERROR_HAVE_NO_MEMORY(data->string);

		return WERR_OK;
	}

	if (!StrCaseCmp(value, "Architecture")) {
		*type = REG_SZ;
		data->string = talloc_strdup(mem_ctx,
			lp_parm_const_string(GLOBAL_SECTION_SNUM, "spoolss", "architecture", SPOOLSS_ARCHITECTURE_NT_X86));
		W_ERROR_HAVE_NO_MEMORY(data->string);

		return WERR_OK;
	}

	if (!StrCaseCmp(value, "DsPresent")) {
		*type = REG_DWORD;

		/* only show the publish check box if we are a
		   member of a AD domain */

		if (lp_security() == SEC_ADS) {
			data->value = 0x01;
		} else {
			data->value = 0x00;
		}
		return WERR_OK;
	}

	if (!StrCaseCmp(value, "DNSMachineName")) {
		const char *hostname = get_mydnsfullname();

		if (!hostname) {
			return WERR_BADFILE;
		}

		*type = REG_SZ;
		data->string = talloc_strdup(mem_ctx, hostname);
		W_ERROR_HAVE_NO_MEMORY(data->string);

		return WERR_OK;
	}

	*type = REG_NONE;

	return WERR_INVALID_PARAM;
}

/****************************************************************
 _spoolss_GetPrinterData
****************************************************************/

WERROR _spoolss_GetPrinterData(struct pipes_struct *p,
			       struct spoolss_GetPrinterData *r)
{
	struct spoolss_GetPrinterDataEx r2;

	r2.in.handle		= r->in.handle;
	r2.in.key_name		= "PrinterDriverData";
	r2.in.value_name	= r->in.value_name;
	r2.in.offered		= r->in.offered;
	r2.out.type		= r->out.type;
	r2.out.data		= r->out.data;
	r2.out.needed		= r->out.needed;

	return _spoolss_GetPrinterDataEx(p, &r2);
}

/*********************************************************
 Connect to the client machine.
**********************************************************/

static bool spoolss_connect_to_client(struct rpc_pipe_client **pp_pipe,
			struct sockaddr_storage *client_ss, const char *remote_machine)
{
	NTSTATUS ret;
	struct cli_state *the_cli;
	struct sockaddr_storage rm_addr;
	char addr[INET6_ADDRSTRLEN];

	if ( is_zero_addr(client_ss) ) {
		DEBUG(2,("spoolss_connect_to_client: resolving %s\n",
			remote_machine));
		if ( !resolve_name( remote_machine, &rm_addr, 0x20, false) ) {
			DEBUG(2,("spoolss_connect_to_client: Can't resolve address for %s\n", remote_machine));
			return false;
		}
		print_sockaddr(addr, sizeof(addr), &rm_addr);
	} else {
		rm_addr = *client_ss;
		print_sockaddr(addr, sizeof(addr), &rm_addr);
		DEBUG(5,("spoolss_connect_to_client: Using address %s (no name resolution necessary)\n",
			addr));
	}

	if (ismyaddr((struct sockaddr *)(void *)&rm_addr)) {
		DEBUG(0,("spoolss_connect_to_client: Machine %s is one of our addresses. Cannot add to ourselves.\n",
			addr));
		return false;
	}

	/* setup the connection */
	ret = cli_full_connection( &the_cli, global_myname(), remote_machine,
		&rm_addr, 0, "IPC$", "IPC",
		"", /* username */
		"", /* domain */
		"", /* password */
		0, lp_client_signing());

	if ( !NT_STATUS_IS_OK( ret ) ) {
		DEBUG(2,("spoolss_connect_to_client: connection to [%s] failed!\n",
			remote_machine ));
		return false;
	}

	if ( the_cli->protocol != PROTOCOL_NT1 ) {
		DEBUG(0,("spoolss_connect_to_client: machine %s didn't negotiate NT protocol.\n", remote_machine));
		cli_shutdown(the_cli);
		return false;
	}

	/*
	 * Ok - we have an anonymous connection to the IPC$ share.
	 * Now start the NT Domain stuff :-).
	 */

	ret = cli_rpc_pipe_open_noauth(the_cli, &ndr_table_spoolss.syntax_id, pp_pipe);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(2,("spoolss_connect_to_client: unable to open the spoolss pipe on machine %s. Error was : %s.\n",
			remote_machine, nt_errstr(ret)));
		cli_shutdown(the_cli);
		return false;
	}

	return true;
}

/***************************************************************************
 Connect to the client.
****************************************************************************/

static bool srv_spoolss_replyopenprinter(int snum, const char *printer,
					uint32_t localprinter,
					enum winreg_Type type,
					struct policy_handle *handle,
					struct notify_back_channel **_chan,
					struct sockaddr_storage *client_ss,
					struct messaging_context *msg_ctx)
{
	WERROR result;
	NTSTATUS status;
	struct notify_back_channel *chan;

	for (chan = back_channels; chan; chan = chan->next) {
		if (memcmp(&chan->client_address, client_ss,
			   sizeof(struct sockaddr_storage)) == 0) {
			break;
		}
	}

	/*
	 * If it's the first connection, contact the client
	 * and connect to the IPC$ share anonymously
	 */
	if (!chan) {
		fstring unix_printer;

		/* the +2 is to strip the leading 2 backslashs */
		fstrcpy(unix_printer, printer + 2);

		chan = talloc_zero(back_channels, struct notify_back_channel);
		if (!chan) {
			return false;
		}
		chan->client_address = *client_ss;

		if (!spoolss_connect_to_client(&chan->cli_pipe, client_ss, unix_printer)) {
			TALLOC_FREE(chan);
			return false;
		}
		chan->binding_handle = chan->cli_pipe->binding_handle;

		DLIST_ADD(back_channels, chan);

		messaging_register(msg_ctx, NULL, MSG_PRINTER_NOTIFY2,
				   receive_notify2_message_list);
		/* Tell the connections db we're now interested in printer
		 * notify messages. */
		serverid_register_msg_flags(messaging_server_id(msg_ctx),
					    true, FLAG_MSG_PRINT_NOTIFY);
	}

	/*
	 * Tell the specific printing tdb we want messages for this printer
	 * by registering our PID.
	 */

	if (!print_notify_register_pid(snum)) {
		DEBUG(0, ("Failed to register our pid for printer %s\n",
			  printer));
	}

	status = dcerpc_spoolss_ReplyOpenPrinter(chan->binding_handle,
						 talloc_tos(),
						 printer,
						 localprinter,
						 type,
						 0,
						 NULL,
						 handle,
						 &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5, ("dcerpc_spoolss_ReplyOpenPrinter returned [%s]\n", nt_errstr(status)));
		result = ntstatus_to_werror(status);
	} else if (!W_ERROR_IS_OK(result)) {
		DEBUG(5, ("ReplyOpenPrinter returned [%s]\n", win_errstr(result)));
	}

	chan->active_connections++;
	*_chan = chan;

	return (W_ERROR_IS_OK(result));
}

/****************************************************************
 ****************************************************************/

static struct spoolss_NotifyOption *dup_spoolss_NotifyOption(TALLOC_CTX *mem_ctx,
							     const struct spoolss_NotifyOption *r)
{
	struct spoolss_NotifyOption *option;
	uint32_t i,k;

	if (!r) {
		return NULL;
	}

	option = talloc_zero(mem_ctx, struct spoolss_NotifyOption);
	if (!option) {
		return NULL;
	}

	*option = *r;

	if (!option->count) {
		return option;
	}

	option->types = talloc_zero_array(option,
		struct spoolss_NotifyOptionType, option->count);
	if (!option->types) {
		talloc_free(option);
		return NULL;
	}

	for (i=0; i < option->count; i++) {
		option->types[i] = r->types[i];

		if (option->types[i].count) {
			option->types[i].fields = talloc_zero_array(option,
				union spoolss_Field, option->types[i].count);
			if (!option->types[i].fields) {
				talloc_free(option);
				return NULL;
			}
			for (k=0; k<option->types[i].count; k++) {
				option->types[i].fields[k] =
					r->types[i].fields[k];
			}
		}
	}

	return option;
}

/****************************************************************
 * _spoolss_RemoteFindFirstPrinterChangeNotifyEx
 *
 * before replying OK: status=0 a rpc call is made to the workstation
 * asking ReplyOpenPrinter
 *
 * in fact ReplyOpenPrinter is the changenotify equivalent on the spoolss pipe
 * called from api_spoolss_rffpcnex
****************************************************************/

WERROR _spoolss_RemoteFindFirstPrinterChangeNotifyEx(struct pipes_struct *p,
						     struct spoolss_RemoteFindFirstPrinterChangeNotifyEx *r)
{
	int snum = -1;
	struct spoolss_NotifyOption *option = r->in.notify_options;
	struct sockaddr_storage client_ss;

	/* store the notify value in the printer struct */

	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);

	if (!Printer) {
		DEBUG(2,("_spoolss_RemoteFindFirstPrinterChangeNotifyEx: "
			"Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	Printer->notify.flags		= r->in.flags;
	Printer->notify.options		= r->in.options;
	Printer->notify.printerlocal	= r->in.printer_local;
	Printer->notify.msg_ctx		= p->msg_ctx;

	TALLOC_FREE(Printer->notify.option);
	Printer->notify.option = dup_spoolss_NotifyOption(Printer, option);

	fstrcpy(Printer->notify.localmachine, r->in.local_machine);

	/* Connect to the client machine and send a ReplyOpenPrinter */

	if ( Printer->printer_type == SPLHND_SERVER)
		snum = -1;
	else if ( (Printer->printer_type == SPLHND_PRINTER) &&
			!get_printer_snum(p, r->in.handle, &snum, NULL) )
		return WERR_BADFID;

	DEBUG(10,("_spoolss_RemoteFindFirstPrinterChangeNotifyEx: "
		"client_address is %s\n", p->client_id->addr));

	if (!lp_print_notify_backchannel(snum)) {
		DEBUG(10, ("_spoolss_RemoteFindFirstPrinterChangeNotifyEx: "
			"backchannel disabled\n"));
		return WERR_SERVER_UNAVAILABLE;
	}

	if (!interpret_string_addr(&client_ss, p->client_id->addr,
				   AI_NUMERICHOST)) {
		return WERR_SERVER_UNAVAILABLE;
	}

	if(!srv_spoolss_replyopenprinter(snum, Printer->notify.localmachine,
					Printer->notify.printerlocal, REG_SZ,
					&Printer->notify.cli_hnd,
					&Printer->notify.cli_chan,
					&client_ss, p->msg_ctx)) {
		return WERR_SERVER_UNAVAILABLE;
	}

	return WERR_OK;
}

/*******************************************************************
 * fill a notify_info_data with the servername
 ********************************************************************/

static void spoolss_notify_server_name(struct messaging_context *msg_ctx,
				       int snum,
				       struct spoolss_Notify *data,
				       print_queue_struct *queue,
				       struct spoolss_PrinterInfo2 *pinfo2,
				       TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, pinfo2->servername);
}

/*******************************************************************
 * fill a notify_info_data with the printername (not including the servername).
 ********************************************************************/

static void spoolss_notify_printer_name(struct messaging_context *msg_ctx,
					int snum,
					struct spoolss_Notify *data,
					print_queue_struct *queue,
					struct spoolss_PrinterInfo2 *pinfo2,
					TALLOC_CTX *mem_ctx)
{
	/* the notify name should not contain the \\server\ part */
	const char *p = strrchr(pinfo2->printername, '\\');

	if (!p) {
		p = pinfo2->printername;
	} else {
		p++;
	}

	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, p);
}

/*******************************************************************
 * fill a notify_info_data with the servicename
 ********************************************************************/

static void spoolss_notify_share_name(struct messaging_context *msg_ctx,
				      int snum,
				      struct spoolss_Notify *data,
				      print_queue_struct *queue,
				      struct spoolss_PrinterInfo2 *pinfo2,
				      TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, lp_servicename(snum));
}

/*******************************************************************
 * fill a notify_info_data with the port name
 ********************************************************************/

static void spoolss_notify_port_name(struct messaging_context *msg_ctx,
				     int snum,
				     struct spoolss_Notify *data,
				     print_queue_struct *queue,
				     struct spoolss_PrinterInfo2 *pinfo2,
				     TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, pinfo2->portname);
}

/*******************************************************************
 * fill a notify_info_data with the printername
 * but it doesn't exist, have to see what to do
 ********************************************************************/

static void spoolss_notify_driver_name(struct messaging_context *msg_ctx,
				       int snum,
				       struct spoolss_Notify *data,
				       print_queue_struct *queue,
				       struct spoolss_PrinterInfo2 *pinfo2,
				       TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, pinfo2->drivername);
}

/*******************************************************************
 * fill a notify_info_data with the comment
 ********************************************************************/

static void spoolss_notify_comment(struct messaging_context *msg_ctx,
				   int snum,
				   struct spoolss_Notify *data,
				   print_queue_struct *queue,
				   struct spoolss_PrinterInfo2 *pinfo2,
				   TALLOC_CTX *mem_ctx)
{
	const char *p;

	if (*pinfo2->comment == '\0') {
		p = lp_comment(snum);
	} else {
		p = pinfo2->comment;
	}

	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, p);
}

/*******************************************************************
 * fill a notify_info_data with the comment
 * location = "Room 1, floor 2, building 3"
 ********************************************************************/

static void spoolss_notify_location(struct messaging_context *msg_ctx,
				    int snum,
				    struct spoolss_Notify *data,
				    print_queue_struct *queue,
				    struct spoolss_PrinterInfo2 *pinfo2,
				    TALLOC_CTX *mem_ctx)
{
	const char *loc = pinfo2->location;
	NTSTATUS status;

	status = printer_list_get_printer(mem_ctx,
					  pinfo2->sharename,
					  NULL,
					  &loc,
					  NULL);
	if (NT_STATUS_IS_OK(status)) {
		if (loc == NULL) {
			loc = pinfo2->location;
		}
	}

	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, loc);
}

/*******************************************************************
 * fill a notify_info_data with the device mode
 * jfm:xxxx don't to it for know but that's a real problem !!!
 ********************************************************************/

static void spoolss_notify_devmode(struct messaging_context *msg_ctx,
				   int snum,
				   struct spoolss_Notify *data,
				   print_queue_struct *queue,
				   struct spoolss_PrinterInfo2 *pinfo2,
				   TALLOC_CTX *mem_ctx)
{
	/* for a dummy implementation we have to zero the fields */
	SETUP_SPOOLSS_NOTIFY_DATA_DEVMODE(data, NULL);
}

/*******************************************************************
 * fill a notify_info_data with the separator file name
 ********************************************************************/

static void spoolss_notify_sepfile(struct messaging_context *msg_ctx,
				   int snum,
				   struct spoolss_Notify *data,
				   print_queue_struct *queue,
				   struct spoolss_PrinterInfo2 *pinfo2,
				   TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, pinfo2->sepfile);
}

/*******************************************************************
 * fill a notify_info_data with the print processor
 * jfm:xxxx return always winprint to indicate we don't do anything to it
 ********************************************************************/

static void spoolss_notify_print_processor(struct messaging_context *msg_ctx,
					   int snum,
					   struct spoolss_Notify *data,
					   print_queue_struct *queue,
					   struct spoolss_PrinterInfo2 *pinfo2,
					   TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, pinfo2->printprocessor);
}

/*******************************************************************
 * fill a notify_info_data with the print processor options
 * jfm:xxxx send an empty string
 ********************************************************************/

static void spoolss_notify_parameters(struct messaging_context *msg_ctx,
				      int snum,
				      struct spoolss_Notify *data,
				      print_queue_struct *queue,
				      struct spoolss_PrinterInfo2 *pinfo2,
				      TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, pinfo2->parameters);
}

/*******************************************************************
 * fill a notify_info_data with the data type
 * jfm:xxxx always send RAW as data type
 ********************************************************************/

static void spoolss_notify_datatype(struct messaging_context *msg_ctx,
				    int snum,
				    struct spoolss_Notify *data,
				    print_queue_struct *queue,
				    struct spoolss_PrinterInfo2 *pinfo2,
				    TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, pinfo2->datatype);
}

/*******************************************************************
 * fill a notify_info_data with the security descriptor
 * jfm:xxxx send an null pointer to say no security desc
 * have to implement security before !
 ********************************************************************/

static void spoolss_notify_security_desc(struct messaging_context *msg_ctx,
					 int snum,
					 struct spoolss_Notify *data,
					 print_queue_struct *queue,
					 struct spoolss_PrinterInfo2 *pinfo2,
					 TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_SECDESC(data, pinfo2->secdesc);
}

/*******************************************************************
 * fill a notify_info_data with the attributes
 * jfm:xxxx a samba printer is always shared
 ********************************************************************/

static void spoolss_notify_attributes(struct messaging_context *msg_ctx,
				      int snum,
				      struct spoolss_Notify *data,
				      print_queue_struct *queue,
				      struct spoolss_PrinterInfo2 *pinfo2,
				      TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, pinfo2->attributes);
}

/*******************************************************************
 * fill a notify_info_data with the priority
 ********************************************************************/

static void spoolss_notify_priority(struct messaging_context *msg_ctx,
				    int snum,
				    struct spoolss_Notify *data,
				    print_queue_struct *queue,
				    struct spoolss_PrinterInfo2 *pinfo2,
				    TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, pinfo2->priority);
}

/*******************************************************************
 * fill a notify_info_data with the default priority
 ********************************************************************/

static void spoolss_notify_default_priority(struct messaging_context *msg_ctx,
					    int snum,
					    struct spoolss_Notify *data,
					    print_queue_struct *queue,
					    struct spoolss_PrinterInfo2 *pinfo2,
					    TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, pinfo2->defaultpriority);
}

/*******************************************************************
 * fill a notify_info_data with the start time
 ********************************************************************/

static void spoolss_notify_start_time(struct messaging_context *msg_ctx,
				      int snum,
				      struct spoolss_Notify *data,
				      print_queue_struct *queue,
				      struct spoolss_PrinterInfo2 *pinfo2,
				      TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, pinfo2->starttime);
}

/*******************************************************************
 * fill a notify_info_data with the until time
 ********************************************************************/

static void spoolss_notify_until_time(struct messaging_context *msg_ctx,
				      int snum,
				      struct spoolss_Notify *data,
				      print_queue_struct *queue,
				      struct spoolss_PrinterInfo2 *pinfo2,
				      TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, pinfo2->untiltime);
}

/*******************************************************************
 * fill a notify_info_data with the status
 ********************************************************************/

static void spoolss_notify_status(struct messaging_context *msg_ctx,
				  int snum,
				  struct spoolss_Notify *data,
				  print_queue_struct *queue,
				  struct spoolss_PrinterInfo2 *pinfo2,
				  TALLOC_CTX *mem_ctx)
{
	print_status_struct status;

	print_queue_length(msg_ctx, snum, &status);
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, status.status);
}

/*******************************************************************
 * fill a notify_info_data with the number of jobs queued
 ********************************************************************/

static void spoolss_notify_cjobs(struct messaging_context *msg_ctx,
				 int snum,
				 struct spoolss_Notify *data,
				 print_queue_struct *queue,
				 struct spoolss_PrinterInfo2 *pinfo2,
				 TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(
		data, print_queue_length(msg_ctx, snum, NULL));
}

/*******************************************************************
 * fill a notify_info_data with the average ppm
 ********************************************************************/

static void spoolss_notify_average_ppm(struct messaging_context *msg_ctx,
				       int snum,
				       struct spoolss_Notify *data,
				       print_queue_struct *queue,
				       struct spoolss_PrinterInfo2 *pinfo2,
				       TALLOC_CTX *mem_ctx)
{
	/* always respond 8 pages per minutes */
	/* a little hard ! */
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, pinfo2->averageppm);
}

/*******************************************************************
 * fill a notify_info_data with username
 ********************************************************************/

static void spoolss_notify_username(struct messaging_context *msg_ctx,
				    int snum,
				    struct spoolss_Notify *data,
				    print_queue_struct *queue,
				    struct spoolss_PrinterInfo2 *pinfo2,
				    TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, queue->fs_user);
}

/*******************************************************************
 * fill a notify_info_data with job status
 ********************************************************************/

static void spoolss_notify_job_status(struct messaging_context *msg_ctx,
				      int snum,
				      struct spoolss_Notify *data,
				      print_queue_struct *queue,
				      struct spoolss_PrinterInfo2 *pinfo2,
				      TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, nt_printj_status(queue->status));
}

/*******************************************************************
 * fill a notify_info_data with job name
 ********************************************************************/

static void spoolss_notify_job_name(struct messaging_context *msg_ctx,
				    int snum,
				    struct spoolss_Notify *data,
				    print_queue_struct *queue,
				    struct spoolss_PrinterInfo2 *pinfo2,
				    TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, queue->fs_file);
}

/*******************************************************************
 * fill a notify_info_data with job status
 ********************************************************************/

static void spoolss_notify_job_status_string(struct messaging_context *msg_ctx,
					     int snum,
					     struct spoolss_Notify *data,
					     print_queue_struct *queue,
					     struct spoolss_PrinterInfo2 *pinfo2,
					     TALLOC_CTX *mem_ctx)
{
	/*
	 * Now we're returning job status codes we just return a "" here. JRA.
	 */

	const char *p = "";

#if 0 /* NO LONGER NEEDED - JRA. 02/22/2001 */
	p = "unknown";

	switch (queue->status) {
	case LPQ_QUEUED:
		p = "Queued";
		break;
	case LPQ_PAUSED:
		p = "";    /* NT provides the paused string */
		break;
	case LPQ_SPOOLING:
		p = "Spooling";
		break;
	case LPQ_PRINTING:
		p = "Printing";
		break;
	}
#endif /* NO LONGER NEEDED. */

	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, p);
}

/*******************************************************************
 * fill a notify_info_data with job time
 ********************************************************************/

static void spoolss_notify_job_time(struct messaging_context *msg_ctx,
				    int snum,
				    struct spoolss_Notify *data,
				    print_queue_struct *queue,
				    struct spoolss_PrinterInfo2 *pinfo2,
				    TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, 0);
}

/*******************************************************************
 * fill a notify_info_data with job size
 ********************************************************************/

static void spoolss_notify_job_size(struct messaging_context *msg_ctx,
				    int snum,
				    struct spoolss_Notify *data,
				    print_queue_struct *queue,
				    struct spoolss_PrinterInfo2 *pinfo2,
				    TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, queue->size);
}

/*******************************************************************
 * fill a notify_info_data with page info
 ********************************************************************/
static void spoolss_notify_total_pages(struct messaging_context *msg_ctx,
				       int snum,
				struct spoolss_Notify *data,
				print_queue_struct *queue,
				struct spoolss_PrinterInfo2 *pinfo2,
				TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, queue->page_count);
}

/*******************************************************************
 * fill a notify_info_data with pages printed info.
 ********************************************************************/
static void spoolss_notify_pages_printed(struct messaging_context *msg_ctx,
					 int snum,
				struct spoolss_Notify *data,
				print_queue_struct *queue,
				struct spoolss_PrinterInfo2 *pinfo2,
				TALLOC_CTX *mem_ctx)
{
	/* Add code when back-end tracks this */
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, 0);
}

/*******************************************************************
 Fill a notify_info_data with job position.
 ********************************************************************/

static void spoolss_notify_job_position(struct messaging_context *msg_ctx,
					int snum,
					struct spoolss_Notify *data,
					print_queue_struct *queue,
					struct spoolss_PrinterInfo2 *pinfo2,
					TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, queue->sysjob);
}

/*******************************************************************
 Fill a notify_info_data with submitted time.
 ********************************************************************/

static void spoolss_notify_submitted_time(struct messaging_context *msg_ctx,
					  int snum,
					  struct spoolss_Notify *data,
					  print_queue_struct *queue,
					  struct spoolss_PrinterInfo2 *pinfo2,
					  TALLOC_CTX *mem_ctx)
{
	data->data.string.string = NULL;
	data->data.string.size = 0;

	init_systemtime_buffer(mem_ctx, gmtime(&queue->time),
			       &data->data.string.string,
			       &data->data.string.size);

}

struct s_notify_info_data_table
{
	enum spoolss_NotifyType type;
	uint16_t field;
	const char *name;
	enum spoolss_NotifyTable variable_type;
	void (*fn) (struct messaging_context *msg_ctx,
		    int snum, struct spoolss_Notify *data,
		    print_queue_struct *queue,
		    struct spoolss_PrinterInfo2 *pinfo2,
		    TALLOC_CTX *mem_ctx);
};

/* A table describing the various print notification constants and
   whether the notification data is a pointer to a variable sized
   buffer, a one value uint32_t or a two value uint32_t. */

static const struct s_notify_info_data_table notify_info_data_table[] =
{
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_SERVER_NAME,         "PRINTER_NOTIFY_FIELD_SERVER_NAME",         NOTIFY_TABLE_STRING,   spoolss_notify_server_name },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_PRINTER_NAME,        "PRINTER_NOTIFY_FIELD_PRINTER_NAME",        NOTIFY_TABLE_STRING,   spoolss_notify_printer_name },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_SHARE_NAME,          "PRINTER_NOTIFY_FIELD_SHARE_NAME",          NOTIFY_TABLE_STRING,   spoolss_notify_share_name },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_PORT_NAME,           "PRINTER_NOTIFY_FIELD_PORT_NAME",           NOTIFY_TABLE_STRING,   spoolss_notify_port_name },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_DRIVER_NAME,         "PRINTER_NOTIFY_FIELD_DRIVER_NAME",         NOTIFY_TABLE_STRING,   spoolss_notify_driver_name },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_COMMENT,             "PRINTER_NOTIFY_FIELD_COMMENT",             NOTIFY_TABLE_STRING,   spoolss_notify_comment },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_LOCATION,            "PRINTER_NOTIFY_FIELD_LOCATION",            NOTIFY_TABLE_STRING,   spoolss_notify_location },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_DEVMODE,             "PRINTER_NOTIFY_FIELD_DEVMODE",             NOTIFY_TABLE_DEVMODE,  spoolss_notify_devmode },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_SEPFILE,             "PRINTER_NOTIFY_FIELD_SEPFILE",             NOTIFY_TABLE_STRING,   spoolss_notify_sepfile },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_PRINT_PROCESSOR,     "PRINTER_NOTIFY_FIELD_PRINT_PROCESSOR",     NOTIFY_TABLE_STRING,   spoolss_notify_print_processor },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_PARAMETERS,          "PRINTER_NOTIFY_FIELD_PARAMETERS",          NOTIFY_TABLE_STRING,   spoolss_notify_parameters },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_DATATYPE,            "PRINTER_NOTIFY_FIELD_DATATYPE",            NOTIFY_TABLE_STRING,   spoolss_notify_datatype },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_SECURITY_DESCRIPTOR, "PRINTER_NOTIFY_FIELD_SECURITY_DESCRIPTOR", NOTIFY_TABLE_SECURITYDESCRIPTOR,   spoolss_notify_security_desc },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_ATTRIBUTES,          "PRINTER_NOTIFY_FIELD_ATTRIBUTES",          NOTIFY_TABLE_DWORD,    spoolss_notify_attributes },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_PRIORITY,            "PRINTER_NOTIFY_FIELD_PRIORITY",            NOTIFY_TABLE_DWORD,    spoolss_notify_priority },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_DEFAULT_PRIORITY,    "PRINTER_NOTIFY_FIELD_DEFAULT_PRIORITY",    NOTIFY_TABLE_DWORD,    spoolss_notify_default_priority },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_START_TIME,          "PRINTER_NOTIFY_FIELD_START_TIME",          NOTIFY_TABLE_DWORD,    spoolss_notify_start_time },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_UNTIL_TIME,          "PRINTER_NOTIFY_FIELD_UNTIL_TIME",          NOTIFY_TABLE_DWORD,    spoolss_notify_until_time },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_STATUS,              "PRINTER_NOTIFY_FIELD_STATUS",              NOTIFY_TABLE_DWORD,    spoolss_notify_status },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_STATUS_STRING,       "PRINTER_NOTIFY_FIELD_STATUS_STRING",       NOTIFY_TABLE_STRING,   NULL },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_CJOBS,               "PRINTER_NOTIFY_FIELD_CJOBS",               NOTIFY_TABLE_DWORD,    spoolss_notify_cjobs },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_AVERAGE_PPM,         "PRINTER_NOTIFY_FIELD_AVERAGE_PPM",         NOTIFY_TABLE_DWORD,    spoolss_notify_average_ppm },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_TOTAL_PAGES,         "PRINTER_NOTIFY_FIELD_TOTAL_PAGES",         NOTIFY_TABLE_DWORD,    NULL },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_PAGES_PRINTED,       "PRINTER_NOTIFY_FIELD_PAGES_PRINTED",       NOTIFY_TABLE_DWORD,    NULL },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_TOTAL_BYTES,         "PRINTER_NOTIFY_FIELD_TOTAL_BYTES",         NOTIFY_TABLE_DWORD,    NULL },
{ PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_BYTES_PRINTED,       "PRINTER_NOTIFY_FIELD_BYTES_PRINTED",       NOTIFY_TABLE_DWORD,    NULL },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_PRINTER_NAME,            "JOB_NOTIFY_FIELD_PRINTER_NAME",            NOTIFY_TABLE_STRING,   spoolss_notify_printer_name },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_MACHINE_NAME,            "JOB_NOTIFY_FIELD_MACHINE_NAME",            NOTIFY_TABLE_STRING,   spoolss_notify_server_name },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_PORT_NAME,               "JOB_NOTIFY_FIELD_PORT_NAME",               NOTIFY_TABLE_STRING,   spoolss_notify_port_name },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_USER_NAME,               "JOB_NOTIFY_FIELD_USER_NAME",               NOTIFY_TABLE_STRING,   spoolss_notify_username },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_NOTIFY_NAME,             "JOB_NOTIFY_FIELD_NOTIFY_NAME",             NOTIFY_TABLE_STRING,   spoolss_notify_username },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_DATATYPE,                "JOB_NOTIFY_FIELD_DATATYPE",                NOTIFY_TABLE_STRING,   spoolss_notify_datatype },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_PRINT_PROCESSOR,         "JOB_NOTIFY_FIELD_PRINT_PROCESSOR",         NOTIFY_TABLE_STRING,   spoolss_notify_print_processor },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_PARAMETERS,              "JOB_NOTIFY_FIELD_PARAMETERS",              NOTIFY_TABLE_STRING,   spoolss_notify_parameters },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_DRIVER_NAME,             "JOB_NOTIFY_FIELD_DRIVER_NAME",             NOTIFY_TABLE_STRING,   spoolss_notify_driver_name },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_DEVMODE,                 "JOB_NOTIFY_FIELD_DEVMODE",                 NOTIFY_TABLE_DEVMODE,  spoolss_notify_devmode },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_STATUS,                  "JOB_NOTIFY_FIELD_STATUS",                  NOTIFY_TABLE_DWORD,    spoolss_notify_job_status },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_STATUS_STRING,           "JOB_NOTIFY_FIELD_STATUS_STRING",           NOTIFY_TABLE_STRING,   spoolss_notify_job_status_string },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_SECURITY_DESCRIPTOR,     "JOB_NOTIFY_FIELD_SECURITY_DESCRIPTOR",     NOTIFY_TABLE_SECURITYDESCRIPTOR,   NULL },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_DOCUMENT,                "JOB_NOTIFY_FIELD_DOCUMENT",                NOTIFY_TABLE_STRING,   spoolss_notify_job_name },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_PRIORITY,                "JOB_NOTIFY_FIELD_PRIORITY",                NOTIFY_TABLE_DWORD,    spoolss_notify_priority },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_POSITION,                "JOB_NOTIFY_FIELD_POSITION",                NOTIFY_TABLE_DWORD,    spoolss_notify_job_position },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_SUBMITTED,               "JOB_NOTIFY_FIELD_SUBMITTED",               NOTIFY_TABLE_TIME,     spoolss_notify_submitted_time },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_START_TIME,              "JOB_NOTIFY_FIELD_START_TIME",              NOTIFY_TABLE_DWORD,    spoolss_notify_start_time },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_UNTIL_TIME,              "JOB_NOTIFY_FIELD_UNTIL_TIME",              NOTIFY_TABLE_DWORD,    spoolss_notify_until_time },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_TIME,                    "JOB_NOTIFY_FIELD_TIME",                    NOTIFY_TABLE_DWORD,    spoolss_notify_job_time },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_TOTAL_PAGES,             "JOB_NOTIFY_FIELD_TOTAL_PAGES",             NOTIFY_TABLE_DWORD,    spoolss_notify_total_pages },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_PAGES_PRINTED,           "JOB_NOTIFY_FIELD_PAGES_PRINTED",           NOTIFY_TABLE_DWORD,    spoolss_notify_pages_printed },
{ JOB_NOTIFY_TYPE,     JOB_NOTIFY_FIELD_TOTAL_BYTES,             "JOB_NOTIFY_FIELD_TOTAL_BYTES",             NOTIFY_TABLE_DWORD,    spoolss_notify_job_size },
};

/*******************************************************************
 Return the variable_type of info_data structure.
********************************************************************/

static enum spoolss_NotifyTable variable_type_of_notify_info_data(enum spoolss_NotifyType type,
								  uint16_t field)
{
	int i=0;

	for (i = 0; i < ARRAY_SIZE(notify_info_data_table); i++) {
		if ( (notify_info_data_table[i].type == type) &&
		     (notify_info_data_table[i].field == field) ) {
			return notify_info_data_table[i].variable_type;
		}
	}

	DEBUG(5, ("invalid notify data type %d/%d\n", type, field));

	return (enum spoolss_NotifyTable) 0;
}

/****************************************************************************
****************************************************************************/

static bool search_notify(enum spoolss_NotifyType type,
			  uint16_t field,
			  int *value)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(notify_info_data_table); i++) {
		if (notify_info_data_table[i].type == type &&
		    notify_info_data_table[i].field == field &&
		    notify_info_data_table[i].fn != NULL) {
			*value = i;
			return true;
		}
	}

	return false;
}

/****************************************************************************
****************************************************************************/

static void construct_info_data(struct spoolss_Notify *info_data,
				enum spoolss_NotifyType type,
				uint16_t field, int id)
{
	info_data->type			= type;
	info_data->field.field		= field;
	info_data->variable_type	= variable_type_of_notify_info_data(type, field);
	info_data->job_id		= id;
}

/*******************************************************************
 *
 * fill a notify_info struct with info asked
 *
 ********************************************************************/

static bool construct_notify_printer_info(struct messaging_context *msg_ctx,
					  struct printer_handle *print_hnd,
					  struct spoolss_NotifyInfo *info,
					  struct spoolss_PrinterInfo2 *pinfo2,
					  int snum,
					  const struct spoolss_NotifyOptionType *option_type,
					  uint32_t id,
					  TALLOC_CTX *mem_ctx)
{
	int field_num,j;
	enum spoolss_NotifyType type;
	uint16_t field;

	struct spoolss_Notify *current_data;

	type = option_type->type;

	DEBUG(4,("construct_notify_printer_info: Notify type: [%s], number of notify info: [%d] on printer: [%s]\n",
		(type == PRINTER_NOTIFY_TYPE ? "PRINTER_NOTIFY_TYPE" : "JOB_NOTIFY_TYPE"),
		option_type->count, lp_servicename(snum)));

	for(field_num=0; field_num < option_type->count; field_num++) {
		field = option_type->fields[field_num].field;

		DEBUG(4,("construct_notify_printer_info: notify [%d]: type [%x], field [%x]\n", field_num, type, field));

		if (!search_notify(type, field, &j) )
			continue;

		info->notifies = TALLOC_REALLOC_ARRAY(info, info->notifies,
						      struct spoolss_Notify,
						      info->count + 1);
		if (info->notifies == NULL) {
			DEBUG(2,("construct_notify_printer_info: failed to enlarge buffer info->data!\n"));
			return false;
		}

		current_data = &info->notifies[info->count];

		construct_info_data(current_data, type, field, id);

		DEBUG(10, ("construct_notify_printer_info: "
			   "calling [%s]  snum=%d  printername=[%s])\n",
			   notify_info_data_table[j].name, snum,
			   pinfo2->printername));

		notify_info_data_table[j].fn(msg_ctx, snum, current_data,
					     NULL, pinfo2, mem_ctx);

		info->count++;
	}

	return true;
}

/*******************************************************************
 *
 * fill a notify_info struct with info asked
 *
 ********************************************************************/

static bool construct_notify_jobs_info(struct messaging_context *msg_ctx,
				       print_queue_struct *queue,
				       struct spoolss_NotifyInfo *info,
				       struct spoolss_PrinterInfo2 *pinfo2,
				       int snum,
				       const struct spoolss_NotifyOptionType *option_type,
				       uint32_t id,
				       TALLOC_CTX *mem_ctx)
{
	int field_num,j;
	enum spoolss_NotifyType type;
	uint16_t field;
	struct spoolss_Notify *current_data;

	DEBUG(4,("construct_notify_jobs_info\n"));

	type = option_type->type;

	DEBUGADD(4,("Notify type: [%s], number of notify info: [%d]\n",
		(type == PRINTER_NOTIFY_TYPE ? "PRINTER_NOTIFY_TYPE" : "JOB_NOTIFY_TYPE"),
		option_type->count));

	for(field_num=0; field_num<option_type->count; field_num++) {
		field = option_type->fields[field_num].field;

		if (!search_notify(type, field, &j) )
			continue;

		info->notifies = TALLOC_REALLOC_ARRAY(info, info->notifies,
						      struct spoolss_Notify,
						      info->count + 1);
		if (info->notifies == NULL) {
			DEBUG(2,("construct_notify_jobs_info: failed to enlarg buffer info->data!\n"));
			return false;
		}

		current_data=&(info->notifies[info->count]);

		construct_info_data(current_data, type, field, id);
		notify_info_data_table[j].fn(msg_ctx, snum, current_data,
					     queue, pinfo2, mem_ctx);
		info->count++;
	}

	return true;
}

/*
 * JFM: The enumeration is not that simple, it's even non obvious.
 *
 * let's take an example: I want to monitor the PRINTER SERVER for
 * the printer's name and the number of jobs currently queued.
 * So in the NOTIFY_OPTION, I have one NOTIFY_OPTION_TYPE structure.
 * Its type is PRINTER_NOTIFY_TYPE and it has 2 fields NAME and CJOBS.
 *
 * I have 3 printers on the back of my server.
 *
 * Now the response is a NOTIFY_INFO structure, with 6 NOTIFY_INFO_DATA
 * structures.
 *   Number	Data			Id
 *	1	printer 1 name		1
 *	2	printer 1 cjob		1
 *	3	printer 2 name		2
 *	4	printer 2 cjob		2
 *	5	printer 3 name		3
 *	6	printer 3 name		3
 *
 * that's the print server case, the printer case is even worse.
 */

/*******************************************************************
 *
 * enumerate all printers on the printserver
 * fill a notify_info struct with info asked
 *
 ********************************************************************/

static WERROR printserver_notify_info(struct pipes_struct *p,
				      struct policy_handle *hnd,
				      struct spoolss_NotifyInfo *info,
				      TALLOC_CTX *mem_ctx)
{
	int snum;
	struct printer_handle *Printer = find_printer_index_by_hnd(p, hnd);
	int n_services=lp_numservices();
	int i;
	struct spoolss_NotifyOption *option;
	struct spoolss_NotifyOptionType option_type;
	struct spoolss_PrinterInfo2 *pinfo2 = NULL;
	WERROR result;

	DEBUG(4,("printserver_notify_info\n"));

	if (!Printer)
		return WERR_BADFID;

	option = Printer->notify.option;

	info->version	= 2;
	info->notifies	= NULL;
	info->count	= 0;

	/* a bug in xp sp2 rc2 causes it to send a fnpcn request without
	   sending a ffpcn() request first */

	if ( !option )
		return WERR_BADFID;

	for (i=0; i<option->count; i++) {
		option_type = option->types[i];

		if (option_type.type != PRINTER_NOTIFY_TYPE)
			continue;

		for (snum = 0; snum < n_services; snum++) {
			if (!lp_browseable(snum) ||
			    !lp_snum_ok(snum) ||
			    !lp_print_ok(snum)) {
				continue; /* skip */
			}

			/* Maybe we should use the SYSTEM session_info here... */
			result = winreg_get_printer_internal(mem_ctx,
						    get_session_info_system(),
						    p->msg_ctx,
						    lp_servicename(snum),
						    &pinfo2);
			if (!W_ERROR_IS_OK(result)) {
				DEBUG(4, ("printserver_notify_info: "
					  "Failed to get printer [%s]\n",
					  lp_servicename(snum)));
				continue;
			}


			construct_notify_printer_info(p->msg_ctx,
						      Printer, info,
						      pinfo2, snum,
						      &option_type, snum,
						      mem_ctx);

			TALLOC_FREE(pinfo2);
		}
	}

#if 0
	/*
	 * Debugging information, don't delete.
	 */

	DEBUG(1,("dumping the NOTIFY_INFO\n"));
	DEBUGADD(1,("info->version:[%d], info->flags:[%d], info->count:[%d]\n", info->version, info->flags, info->count));
	DEBUGADD(1,("num\ttype\tfield\tres\tid\tsize\tenc_type\n"));

	for (i=0; i<info->count; i++) {
		DEBUGADD(1,("[%d]\t[%d]\t[%d]\t[%d]\t[%d]\t[%d]\t[%d]\n",
		i, info->data[i].type, info->data[i].field, info->data[i].reserved,
		info->data[i].id, info->data[i].size, info->data[i].enc_type));
	}
#endif

	return WERR_OK;
}

/*******************************************************************
 *
 * fill a notify_info struct with info asked
 *
 ********************************************************************/

static WERROR printer_notify_info(struct pipes_struct *p,
				  struct policy_handle *hnd,
				  struct spoolss_NotifyInfo *info,
				  TALLOC_CTX *mem_ctx)
{
	int snum;
	struct printer_handle *Printer = find_printer_index_by_hnd(p, hnd);
	int i;
	uint32_t id;
	struct spoolss_NotifyOption *option;
	struct spoolss_NotifyOptionType option_type;
	int count,j;
	print_queue_struct *queue=NULL;
	print_status_struct status;
	struct spoolss_PrinterInfo2 *pinfo2 = NULL;
	WERROR result;

	DEBUG(4,("printer_notify_info\n"));

	if (!Printer)
		return WERR_BADFID;

	option = Printer->notify.option;
	id = 0x0;

	info->version	= 2;
	info->notifies	= NULL;
	info->count	= 0;

	/* a bug in xp sp2 rc2 causes it to send a fnpcn request without
	   sending a ffpcn() request first */

	if ( !option )
		return WERR_BADFID;

	if (!get_printer_snum(p, hnd, &snum, NULL)) {
		return WERR_BADFID;
	}

	/* Maybe we should use the SYSTEM session_info here... */
	result = winreg_get_printer_internal(mem_ctx,
				    get_session_info_system(),
				    p->msg_ctx,
				    lp_servicename(snum), &pinfo2);
	if (!W_ERROR_IS_OK(result)) {
		return WERR_BADFID;
	}

	/*
	 * When sending a PRINTER_NOTIFY_FIELD_SERVER_NAME we should send the
	 * correct servername.
	 */
	pinfo2->servername = talloc_strdup(pinfo2, Printer->servername);
	if (pinfo2->servername == NULL) {
		return WERR_NOMEM;
	}

	for (i=0; i<option->count; i++) {
		option_type = option->types[i];

		switch (option_type.type) {
		case PRINTER_NOTIFY_TYPE:
			if (construct_notify_printer_info(p->msg_ctx,
							  Printer, info,
							  pinfo2, snum,
							  &option_type, id,
							  mem_ctx)) {
				id--;
			}
			break;

		case JOB_NOTIFY_TYPE:

			count = print_queue_status(p->msg_ctx, snum, &queue,
						   &status);

			for (j=0; j<count; j++) {
				construct_notify_jobs_info(p->msg_ctx,
							   &queue[j], info,
							   pinfo2, snum,
							   &option_type,
							   queue[j].sysjob,
							   mem_ctx);
			}

			SAFE_FREE(queue);
			break;
		}
	}

	/*
	 * Debugging information, don't delete.
	 */
	/*
	DEBUG(1,("dumping the NOTIFY_INFO\n"));
	DEBUGADD(1,("info->version:[%d], info->flags:[%d], info->count:[%d]\n", info->version, info->flags, info->count));
	DEBUGADD(1,("num\ttype\tfield\tres\tid\tsize\tenc_type\n"));

	for (i=0; i<info->count; i++) {
		DEBUGADD(1,("[%d]\t[%d]\t[%d]\t[%d]\t[%d]\t[%d]\t[%d]\n",
		i, info->data[i].type, info->data[i].field, info->data[i].reserved,
		info->data[i].id, info->data[i].size, info->data[i].enc_type));
	}
	*/

	talloc_free(pinfo2);
	return WERR_OK;
}

/****************************************************************
 _spoolss_RouterRefreshPrinterChangeNotify
****************************************************************/

WERROR _spoolss_RouterRefreshPrinterChangeNotify(struct pipes_struct *p,
						 struct spoolss_RouterRefreshPrinterChangeNotify *r)
{
	struct spoolss_NotifyInfo *info;

	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);
	WERROR result = WERR_BADFID;

	/* we always have a spoolss_NotifyInfo struct */
	info = talloc_zero(p->mem_ctx, struct spoolss_NotifyInfo);
	if (!info) {
		result = WERR_NOMEM;
		goto done;
	}

	*r->out.info = info;

	if (!Printer) {
		DEBUG(2,("_spoolss_RouterRefreshPrinterChangeNotify: "
			"Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		goto done;
	}

	DEBUG(4,("Printer type %x\n",Printer->printer_type));

	/*
	 * 	We are now using the change value, and
	 *	I should check for PRINTER_NOTIFY_OPTIONS_REFRESH but as
	 *	I don't have a global notification system, I'm sending back all the
	 *	information even when _NOTHING_ has changed.
	 */

	/* We need to keep track of the change value to send back in
           RRPCN replies otherwise our updates are ignored. */

	Printer->notify.fnpcn = true;

	if (Printer->notify.cli_chan != NULL &&
	    Printer->notify.cli_chan->active_connections > 0) {
		DEBUG(10,("_spoolss_RouterRefreshPrinterChangeNotify: "
			"Saving change value in request [%x]\n",
			r->in.change_low));
		Printer->notify.change = r->in.change_low;
	}

	/* just ignore the spoolss_NotifyOption */

	switch (Printer->printer_type) {
		case SPLHND_SERVER:
			result = printserver_notify_info(p, r->in.handle,
							 info, p->mem_ctx);
			break;

		case SPLHND_PRINTER:
			result = printer_notify_info(p, r->in.handle,
						     info, p->mem_ctx);
			break;
	}

	Printer->notify.fnpcn = false;

done:
	return result;
}

/********************************************************************
 ********************************************************************/

static WERROR create_printername(TALLOC_CTX *mem_ctx,
				 const char *servername,
				 const char *printername,
				 const char **printername_p)
{
	/* FIXME: add lp_force_printername() */

	if (servername == NULL) {
		*printername_p = talloc_strdup(mem_ctx, printername);
		W_ERROR_HAVE_NO_MEMORY(*printername_p);
		return WERR_OK;
	}

	if (servername[0] == '\\' && servername[1] == '\\') {
		servername += 2;
	}

	*printername_p = talloc_asprintf(mem_ctx, "\\\\%s\\%s", servername, printername);
	W_ERROR_HAVE_NO_MEMORY(*printername_p);

	return WERR_OK;
}

/********************************************************************
 ********************************************************************/

static void compose_devicemode_devicename(struct spoolss_DeviceMode *dm,
					  const char *printername)
{
	if (dm == NULL) {
		return;
	}

	dm->devicename = talloc_strndup(dm, printername,
					MIN(strlen(printername), 31));
}

/********************************************************************
 * construct_printer_info_0
 * fill a printer_info_0 struct
 ********************************************************************/

static WERROR construct_printer_info0(TALLOC_CTX *mem_ctx,
				      const struct auth_serversupplied_info *session_info,
				      struct messaging_context *msg_ctx,
				      struct spoolss_PrinterInfo2 *info2,
				      const char *servername,
				      struct spoolss_PrinterInfo0 *r,
				      int snum)
{
	int count;
	struct printer_session_counter *session_counter;
	struct timeval setuptime;
	print_status_struct status;
	WERROR result;

	result = create_printername(mem_ctx, servername, info2->printername, &r->printername);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	if (servername) {
		r->servername = talloc_strdup(mem_ctx, servername);
		W_ERROR_HAVE_NO_MEMORY(r->servername);
	} else {
		r->servername = NULL;
	}

	count = print_queue_length(msg_ctx, snum, &status);

	/* check if we already have a counter for this printer */
	for (session_counter = counter_list; session_counter; session_counter = session_counter->next) {
		if (session_counter->snum == snum)
			break;
	}

	/* it's the first time, add it to the list */
	if (session_counter == NULL) {
		session_counter = talloc_zero(counter_list, struct printer_session_counter);
		W_ERROR_HAVE_NO_MEMORY(session_counter);
		session_counter->snum		= snum;
		session_counter->counter	= 0;
		DLIST_ADD(counter_list, session_counter);
	}

	/* increment it */
	session_counter->counter++;

	r->cjobs			= count;
	r->total_jobs			= 0;
	r->total_bytes			= 0;

	get_startup_time(&setuptime);
	init_systemtime(&r->time, gmtime(&setuptime.tv_sec));

	/* JFM:
	 * the global_counter should be stored in a TDB as it's common to all the clients
	 * and should be zeroed on samba startup
	 */
	r->global_counter		= session_counter->counter;
	r->total_pages			= 0;
	/* in 2.2 we reported ourselves as 0x0004 and 0x0565 */
	SSVAL(&r->version, 0, 0x0005); /* NT 5 */
	SSVAL(&r->version, 2, 0x0893); /* build 2195 */
	r->free_build			= SPOOLSS_RELEASE_BUILD;
	r->spooling			= 0;
	r->max_spooling			= 0;
	r->session_counter		= session_counter->counter;
	r->num_error_out_of_paper	= 0x0;
	r->num_error_not_ready		= 0x0;		/* number of print failure */
	r->job_error			= 0x0;
	r->number_of_processors		= 0x1;
	r->processor_type		= PROCESSOR_INTEL_PENTIUM; /* 586 Pentium ? */
	r->high_part_total_bytes	= 0x0;

	/* ChangeID in milliseconds*/
	winreg_printer_get_changeid_internal(mem_ctx, session_info, msg_ctx,
				    info2->sharename, &r->change_id);

	r->last_error			= WERR_OK;
	r->status			= nt_printq_status(status.status);
	r->enumerate_network_printers	= 0x0;
	r->c_setprinter			= 0x0;
	r->processor_architecture	= PROCESSOR_ARCHITECTURE_INTEL;
	r->processor_level		= 0x6; 		/* 6  ???*/
	r->ref_ic			= 0;
	r->reserved2			= 0;
	r->reserved3			= 0;

	return WERR_OK;
}


/********************************************************************
 * construct_printer_info1
 * fill a spoolss_PrinterInfo1 struct
********************************************************************/

static WERROR construct_printer_info1(TALLOC_CTX *mem_ctx,
				      const struct spoolss_PrinterInfo2 *info2,
				      uint32_t flags,
				      const char *servername,
				      struct spoolss_PrinterInfo1 *r,
				      int snum)
{
	WERROR result;

	r->flags		= flags;

	if (info2->comment == NULL || info2->comment[0] == '\0') {
		r->comment	= talloc_strdup(mem_ctx, lp_comment(snum));
	} else {
		r->comment	= talloc_strdup(mem_ctx, info2->comment); /* saved comment */
	}
	W_ERROR_HAVE_NO_MEMORY(r->comment);

	result = create_printername(mem_ctx, servername, info2->printername, &r->name);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	r->description		= talloc_asprintf(mem_ctx, "%s,%s,%s",
						  r->name,
						  info2->drivername,
						  r->comment);
	W_ERROR_HAVE_NO_MEMORY(r->description);

	return WERR_OK;
}

/********************************************************************
 * construct_printer_info2
 * fill a spoolss_PrinterInfo2 struct
********************************************************************/

static WERROR construct_printer_info2(TALLOC_CTX *mem_ctx,
				      struct messaging_context *msg_ctx,
				      const struct spoolss_PrinterInfo2 *info2,
				      const char *servername,
				      struct spoolss_PrinterInfo2 *r,
				      int snum)
{
	int count;
	print_status_struct status;
	WERROR result;

	count = print_queue_length(msg_ctx, snum, &status);

	if (servername) {
		r->servername		= talloc_strdup(mem_ctx, servername);
		W_ERROR_HAVE_NO_MEMORY(r->servername);
	} else {
		r->servername		= NULL;
	}

	result = create_printername(mem_ctx, servername, info2->printername, &r->printername);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	r->sharename		= talloc_strdup(mem_ctx, lp_servicename(snum));
	W_ERROR_HAVE_NO_MEMORY(r->sharename);
	r->portname		= talloc_strdup(mem_ctx, info2->portname);
	W_ERROR_HAVE_NO_MEMORY(r->portname);
	r->drivername		= talloc_strdup(mem_ctx, info2->drivername);
	W_ERROR_HAVE_NO_MEMORY(r->drivername);

	if (info2->comment[0] == '\0') {
		r->comment	= talloc_strdup(mem_ctx, lp_comment(snum));
	} else {
		r->comment	= talloc_strdup(mem_ctx, info2->comment);
	}
	W_ERROR_HAVE_NO_MEMORY(r->comment);

	r->location	= talloc_strdup(mem_ctx, info2->location);
	if (info2->location[0] == '\0') {
		const char *loc = NULL;
		NTSTATUS nt_status;

		nt_status = printer_list_get_printer(mem_ctx,
						     info2->sharename,
						     NULL,
						     &loc,
						     NULL);
		if (NT_STATUS_IS_OK(nt_status)) {
			if (loc != NULL) {
				r->location = talloc_strdup(mem_ctx, loc);
			}
		}
	}
	W_ERROR_HAVE_NO_MEMORY(r->location);

	r->sepfile		= talloc_strdup(mem_ctx, info2->sepfile);
	W_ERROR_HAVE_NO_MEMORY(r->sepfile);
	r->printprocessor	= talloc_strdup(mem_ctx, info2->printprocessor);
	W_ERROR_HAVE_NO_MEMORY(r->printprocessor);
	r->datatype		= talloc_strdup(mem_ctx, info2->datatype);
	W_ERROR_HAVE_NO_MEMORY(r->datatype);
	r->parameters		= talloc_strdup(mem_ctx, info2->parameters);
	W_ERROR_HAVE_NO_MEMORY(r->parameters);

	r->attributes		= info2->attributes;

	r->priority		= info2->priority;
	r->defaultpriority	= info2->defaultpriority;
	r->starttime		= info2->starttime;
	r->untiltime		= info2->untiltime;
	r->status		= nt_printq_status(status.status);
	r->cjobs		= count;
	r->averageppm		= info2->averageppm;

	if (info2->devmode != NULL) {
		result = copy_devicemode(mem_ctx,
					 info2->devmode,
					 &r->devmode);
		if (!W_ERROR_IS_OK(result)) {
			return result;
		}
	} else if (lp_default_devmode(snum)) {
		result = spoolss_create_default_devmode(mem_ctx,
							info2->printername,
							&r->devmode);
		if (!W_ERROR_IS_OK(result)) {
			return result;
		}
	} else {
		r->devmode = NULL;
		DEBUG(8,("Returning NULL Devicemode!\n"));
	}

	compose_devicemode_devicename(r->devmode, r->printername);

	r->secdesc = NULL;

	if (info2->secdesc != NULL) {
		/* don't use talloc_steal() here unless you do a deep steal of all
		   the SEC_DESC members */

		r->secdesc	= dup_sec_desc(mem_ctx, info2->secdesc);
	}

	return WERR_OK;
}

/********************************************************************
 * construct_printer_info3
 * fill a spoolss_PrinterInfo3 struct
 ********************************************************************/

static WERROR construct_printer_info3(TALLOC_CTX *mem_ctx,
				      const struct spoolss_PrinterInfo2 *info2,
				      const char *servername,
				      struct spoolss_PrinterInfo3 *r,
				      int snum)
{
	/* These are the components of the SD we are returning. */

	if (info2->secdesc != NULL) {
		/* don't use talloc_steal() here unless you do a deep steal of all
		   the SEC_DESC members */

		r->secdesc = dup_sec_desc(mem_ctx, info2->secdesc);
		W_ERROR_HAVE_NO_MEMORY(r->secdesc);
	}

	return WERR_OK;
}

/********************************************************************
 * construct_printer_info4
 * fill a spoolss_PrinterInfo4 struct
 ********************************************************************/

static WERROR construct_printer_info4(TALLOC_CTX *mem_ctx,
				      const struct spoolss_PrinterInfo2 *info2,
				      const char *servername,
				      struct spoolss_PrinterInfo4 *r,
				      int snum)
{
	WERROR result;

	result = create_printername(mem_ctx, servername, info2->printername, &r->printername);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	if (servername) {
		r->servername	= talloc_strdup(mem_ctx, servername);
		W_ERROR_HAVE_NO_MEMORY(r->servername);
	} else {
		r->servername = NULL;
	}

	r->attributes	= info2->attributes;

	return WERR_OK;
}

/********************************************************************
 * construct_printer_info5
 * fill a spoolss_PrinterInfo5 struct
 ********************************************************************/

static WERROR construct_printer_info5(TALLOC_CTX *mem_ctx,
				      const struct spoolss_PrinterInfo2 *info2,
				      const char *servername,
				      struct spoolss_PrinterInfo5 *r,
				      int snum)
{
	WERROR result;

	result = create_printername(mem_ctx, servername, info2->printername, &r->printername);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	r->portname	= talloc_strdup(mem_ctx, info2->portname);
	W_ERROR_HAVE_NO_MEMORY(r->portname);

	r->attributes	= info2->attributes;

	/* these two are not used by NT+ according to MSDN */
	r->device_not_selected_timeout		= 0x0;  /* have seen 0x3a98 */
	r->transmission_retry_timeout		= 0x0;  /* have seen 0xafc8 */

	return WERR_OK;
}

/********************************************************************
 * construct_printer_info_6
 * fill a spoolss_PrinterInfo6 struct
 ********************************************************************/

static WERROR construct_printer_info6(TALLOC_CTX *mem_ctx,
				      struct messaging_context *msg_ctx,
				      const struct spoolss_PrinterInfo2 *info2,
				      const char *servername,
				      struct spoolss_PrinterInfo6 *r,
				      int snum)
{
	int count;
	print_status_struct status;

	count = print_queue_length(msg_ctx, snum, &status);

	r->status = nt_printq_status(status.status);

	return WERR_OK;
}

/********************************************************************
 * construct_printer_info7
 * fill a spoolss_PrinterInfo7 struct
 ********************************************************************/

static WERROR construct_printer_info7(TALLOC_CTX *mem_ctx,
				      struct messaging_context *msg_ctx,
				      const char *servername,
				      struct spoolss_PrinterInfo7 *r,
				      int snum)
{
	struct auth_serversupplied_info *session_info;
	char *printer;
	NTSTATUS status;
	WERROR werr;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	status = make_session_info_system(tmp_ctx, &session_info);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("construct_printer_info7: "
			  "Could not create system session_info\n"));
		werr = WERR_NOMEM;
		goto out_tmp_free;
	}

	printer = lp_servicename(snum);
	if (printer == NULL) {
		DEBUG(0, ("invalid printer snum %d\n", snum));
		werr = WERR_INVALID_PARAM;
		goto out_tmp_free;
	}

	if (is_printer_published(tmp_ctx, session_info, msg_ctx,
				 servername, printer, NULL)) {
		struct GUID guid;
		werr = nt_printer_guid_get(tmp_ctx, session_info, msg_ctx,
					   printer, &guid);
		if (!W_ERROR_IS_OK(werr)) {
			goto out_tmp_free;
		}
		r->guid = talloc_strdup_upper(mem_ctx, GUID_string2(mem_ctx, &guid));
		r->action = DSPRINT_PUBLISH;
	} else {
		r->guid = talloc_strdup(mem_ctx, "");
		r->action = DSPRINT_UNPUBLISH;
	}
	if (r->guid == NULL) {
		werr = WERR_NOMEM;
		goto out_tmp_free;
	}

	werr = WERR_OK;
out_tmp_free:
	talloc_free(tmp_ctx);
	return werr;
}

/********************************************************************
 * construct_printer_info8
 * fill a spoolss_PrinterInfo8 struct
 ********************************************************************/

static WERROR construct_printer_info8(TALLOC_CTX *mem_ctx,
				      const struct spoolss_PrinterInfo2 *info2,
				      const char *servername,
				      struct spoolss_DeviceModeInfo *r,
				      int snum)
{
	WERROR result;
	const char *printername;

	result = create_printername(mem_ctx, servername, info2->printername, &printername);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	if (info2->devmode != NULL) {
		result = copy_devicemode(mem_ctx,
					 info2->devmode,
					 &r->devmode);
		if (!W_ERROR_IS_OK(result)) {
			return result;
		}
	} else if (lp_default_devmode(snum)) {
		result = spoolss_create_default_devmode(mem_ctx,
							info2->printername,
							&r->devmode);
		if (!W_ERROR_IS_OK(result)) {
			return result;
		}
	} else {
		r->devmode = NULL;
		DEBUG(8,("Returning NULL Devicemode!\n"));
	}

	compose_devicemode_devicename(r->devmode, printername);

	return WERR_OK;
}


/********************************************************************
********************************************************************/

static bool snum_is_shared_printer(int snum)
{
	return (lp_browseable(snum) && lp_snum_ok(snum) && lp_print_ok(snum));
}

/********************************************************************
 Spoolss_enumprinters.
********************************************************************/

static WERROR enum_all_printers_info_level(TALLOC_CTX *mem_ctx,
					   const struct auth_serversupplied_info *session_info,
					   struct messaging_context *msg_ctx,
					   const char *servername,
					   uint32_t level,
					   uint32_t flags,
					   union spoolss_PrinterInfo **info_p,
					   uint32_t *count_p)
{
	int snum;
	int n_services = lp_numservices();
	union spoolss_PrinterInfo *info = NULL;
	uint32_t count = 0;
	WERROR result = WERR_OK;
	struct dcerpc_binding_handle *b = NULL;

	*count_p = 0;
	*info_p = NULL;

	for (snum = 0; snum < n_services; snum++) {

		const char *printer;
		struct spoolss_PrinterInfo2 *info2;

		if (!snum_is_shared_printer(snum)) {
			continue;
		}

		printer = lp_const_servicename(snum);

		DEBUG(4,("Found a printer in smb.conf: %s[%x]\n",
			printer, snum));

		if (b == NULL) {
			result = winreg_printer_binding_handle(mem_ctx,
							       session_info,
							       msg_ctx,
							       &b);
			if (!W_ERROR_IS_OK(result)) {
				goto out;
			}
		}

		result = winreg_create_printer(mem_ctx, b,
					       printer);
		if (!W_ERROR_IS_OK(result)) {
			goto out;
		}

		info = TALLOC_REALLOC_ARRAY(mem_ctx, info,
					    union spoolss_PrinterInfo,
					    count + 1);
		if (!info) {
			result = WERR_NOMEM;
			goto out;
		}

		result = winreg_get_printer(mem_ctx, b,
					    printer, &info2);
		if (!W_ERROR_IS_OK(result)) {
			goto out;
		}

		switch (level) {
		case 0:
			result = construct_printer_info0(info, session_info,
							 msg_ctx, info2,
							 servername,
							 &info[count].info0, snum);
			break;
		case 1:
			result = construct_printer_info1(info, info2, flags,
							 servername,
							 &info[count].info1, snum);
			break;
		case 2:
			result = construct_printer_info2(info, msg_ctx, info2,
							 servername,
							 &info[count].info2, snum);
			break;
		case 4:
			result = construct_printer_info4(info, info2,
							 servername,
							 &info[count].info4, snum);
			break;
		case 5:
			result = construct_printer_info5(info, info2,
							 servername,
							 &info[count].info5, snum);
			break;

		default:
			result = WERR_UNKNOWN_LEVEL;
			goto out;
		}

		if (!W_ERROR_IS_OK(result)) {
			goto out;
		}

		count++;
	}

	*count_p = count;
	*info_p = info;

 out:
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(info);
		return result;
	}

	*info_p = info;

	return WERR_OK;
}

/********************************************************************
 * handle enumeration of printers at level 0
 ********************************************************************/

static WERROR enumprinters_level0(TALLOC_CTX *mem_ctx,
				  const struct auth_serversupplied_info *session_info,
				  struct messaging_context *msg_ctx,
				  uint32_t flags,
				  const char *servername,
				  union spoolss_PrinterInfo **info,
				  uint32_t *count)
{
	DEBUG(4,("enum_all_printers_info_0\n"));

	return enum_all_printers_info_level(mem_ctx, session_info, msg_ctx,
					    servername, 0, flags, info, count);
}


/********************************************************************
********************************************************************/

static WERROR enum_all_printers_info_1(TALLOC_CTX *mem_ctx,
				       const struct auth_serversupplied_info *session_info,
				       struct messaging_context *msg_ctx,
				       const char *servername,
				       uint32_t flags,
				       union spoolss_PrinterInfo **info,
				       uint32_t *count)
{
	DEBUG(4,("enum_all_printers_info_1\n"));

	return enum_all_printers_info_level(mem_ctx, session_info, msg_ctx,
					    servername, 1, flags, info, count);
}

/********************************************************************
 enum_all_printers_info_1_local.
*********************************************************************/

static WERROR enum_all_printers_info_1_local(TALLOC_CTX *mem_ctx,
					     const struct auth_serversupplied_info *session_info,
					     struct messaging_context *msg_ctx,
					     const char *servername,
					     union spoolss_PrinterInfo **info,
					     uint32_t *count)
{
	DEBUG(4,("enum_all_printers_info_1_local\n"));

	return enum_all_printers_info_1(mem_ctx, session_info, msg_ctx,
					servername, PRINTER_ENUM_ICON8, info, count);
}

/********************************************************************
 enum_all_printers_info_1_name.
*********************************************************************/

static WERROR enum_all_printers_info_1_name(TALLOC_CTX *mem_ctx,
					    const struct auth_serversupplied_info *session_info,
					    struct messaging_context *msg_ctx,
					    const char *servername,
					    union spoolss_PrinterInfo **info,
					    uint32_t *count)
{
	const char *s = servername;

	DEBUG(4,("enum_all_printers_info_1_name\n"));

	if (servername != NULL &&
	    (servername[0] == '\\') && (servername[1] == '\\')) {
		s = servername + 2;
	}

	if (!is_myname_or_ipaddr(s)) {
		return WERR_INVALID_NAME;
	}

	return enum_all_printers_info_1(mem_ctx, session_info, msg_ctx,
					servername, PRINTER_ENUM_ICON8, info, count);
}

/********************************************************************
 enum_all_printers_info_1_network.
*********************************************************************/

static WERROR enum_all_printers_info_1_network(TALLOC_CTX *mem_ctx,
					       const struct auth_serversupplied_info *session_info,
					       struct messaging_context *msg_ctx,
					       const char *servername,
					       union spoolss_PrinterInfo **info,
					       uint32_t *count)
{
	const char *s = servername;

	DEBUG(4,("enum_all_printers_info_1_network\n"));

	/* If we respond to a enum_printers level 1 on our name with flags
	   set to PRINTER_ENUM_REMOTE with a list of printers then these
	   printers incorrectly appear in the APW browse list.
	   Specifically the printers for the server appear at the workgroup
	   level where all the other servers in the domain are
	   listed. Windows responds to this call with a
	   WERR_CAN_NOT_COMPLETE so we should do the same. */

	if (servername != NULL &&
	    (servername[0] == '\\') && (servername[1] == '\\')) {
		 s = servername + 2;
	}

	if (is_myname_or_ipaddr(s)) {
		 return WERR_CAN_NOT_COMPLETE;
	}

	return enum_all_printers_info_1(mem_ctx, session_info, msg_ctx,
					servername, PRINTER_ENUM_NAME, info, count);
}

/********************************************************************
 * api_spoolss_enumprinters
 *
 * called from api_spoolss_enumprinters (see this to understand)
 ********************************************************************/

static WERROR enum_all_printers_info_2(TALLOC_CTX *mem_ctx,
				       const struct auth_serversupplied_info *session_info,
				       struct messaging_context *msg_ctx,
				       const char *servername,
				       union spoolss_PrinterInfo **info,
				       uint32_t *count)
{
	DEBUG(4,("enum_all_printers_info_2\n"));

	return enum_all_printers_info_level(mem_ctx, session_info, msg_ctx,
					    servername, 2, 0, info, count);
}

/********************************************************************
 * handle enumeration of printers at level 1
 ********************************************************************/

static WERROR enumprinters_level1(TALLOC_CTX *mem_ctx,
				  const struct auth_serversupplied_info *session_info,
				  struct messaging_context *msg_ctx,
				  uint32_t flags,
				  const char *servername,
				  union spoolss_PrinterInfo **info,
				  uint32_t *count)
{
	/* Not all the flags are equals */

	if (flags & PRINTER_ENUM_LOCAL) {
		return enum_all_printers_info_1_local(mem_ctx, session_info,
						      msg_ctx, servername, info, count);
	}

	if (flags & PRINTER_ENUM_NAME) {
		return enum_all_printers_info_1_name(mem_ctx, session_info,
						     msg_ctx, servername, info,
						     count);
	}

	if (flags & PRINTER_ENUM_NETWORK) {
		return enum_all_printers_info_1_network(mem_ctx, session_info,
							msg_ctx, servername, info,
							count);
	}

	return WERR_OK; /* NT4sp5 does that */
}

/********************************************************************
 * handle enumeration of printers at level 2
 ********************************************************************/

static WERROR enumprinters_level2(TALLOC_CTX *mem_ctx,
				  const struct auth_serversupplied_info *session_info,
				  struct messaging_context *msg_ctx,
				  uint32_t flags,
				  const char *servername,
				  union spoolss_PrinterInfo **info,
				  uint32_t *count)
{
	if (flags & PRINTER_ENUM_LOCAL) {

		return enum_all_printers_info_2(mem_ctx, session_info, msg_ctx,
						servername,
						info, count);
	}

	if (flags & PRINTER_ENUM_NAME) {
		if (servername && !is_myname_or_ipaddr(canon_servername(servername))) {
			return WERR_INVALID_NAME;
		}

		return enum_all_printers_info_2(mem_ctx, session_info, msg_ctx,
						servername,
						info, count);
	}

	if (flags & PRINTER_ENUM_REMOTE) {
		return WERR_UNKNOWN_LEVEL;
	}

	return WERR_OK;
}

/********************************************************************
 * handle enumeration of printers at level 4
 ********************************************************************/

static WERROR enumprinters_level4(TALLOC_CTX *mem_ctx,
				  const struct auth_serversupplied_info *session_info,
				  struct messaging_context *msg_ctx,
				  uint32_t flags,
				  const char *servername,
				  union spoolss_PrinterInfo **info,
				  uint32_t *count)
{
	DEBUG(4,("enum_all_printers_info_4\n"));

	return enum_all_printers_info_level(mem_ctx, session_info, msg_ctx,
					    servername, 4, flags, info, count);
}


/********************************************************************
 * handle enumeration of printers at level 5
 ********************************************************************/

static WERROR enumprinters_level5(TALLOC_CTX *mem_ctx,
				  const struct auth_serversupplied_info *session_info,
				  struct messaging_context *msg_ctx,
				  uint32_t flags,
				  const char *servername,
				  union spoolss_PrinterInfo **info,
				  uint32_t *count)
{
	DEBUG(4,("enum_all_printers_info_5\n"));

	return enum_all_printers_info_level(mem_ctx, session_info, msg_ctx,
					    servername, 5, flags, info, count);
}

/****************************************************************
 _spoolss_EnumPrinters
****************************************************************/

WERROR _spoolss_EnumPrinters(struct pipes_struct *p,
			     struct spoolss_EnumPrinters *r)
{
	const struct auth_serversupplied_info *session_info = get_session_info_system();
	WERROR result;

	/* that's an [in out] buffer */

	if (!r->in.buffer && (r->in.offered != 0)) {
		return WERR_INVALID_PARAM;
	}

	DEBUG(4,("_spoolss_EnumPrinters\n"));

	*r->out.needed = 0;
	*r->out.count = 0;
	*r->out.info = NULL;

	/*
	 * Level 1:
	 *	    flags==PRINTER_ENUM_NAME
	 *	     if name=="" then enumerates all printers
	 *	     if name!="" then enumerate the printer
	 *	    flags==PRINTER_ENUM_REMOTE
	 *	    name is NULL, enumerate printers
	 * Level 2: name!="" enumerates printers, name can't be NULL
	 * Level 3: doesn't exist
	 * Level 4: does a local registry lookup
	 * Level 5: same as Level 2
	 */

	if (r->in.server && r->in.server[0] == '\0') {
		r->in.server = NULL;
	}

	switch (r->in.level) {
	case 0:
		result = enumprinters_level0(p->mem_ctx, session_info,
					     p->msg_ctx, r->in.flags,
					     r->in.server,
					     r->out.info, r->out.count);
		break;
	case 1:
		result = enumprinters_level1(p->mem_ctx, session_info,
					     p->msg_ctx, r->in.flags,
					     r->in.server,
					     r->out.info, r->out.count);
		break;
	case 2:
		result = enumprinters_level2(p->mem_ctx, session_info,
					     p->msg_ctx, r->in.flags,
					     r->in.server,
					     r->out.info, r->out.count);
		break;
	case 4:
		result = enumprinters_level4(p->mem_ctx, session_info,
					     p->msg_ctx, r->in.flags,
					     r->in.server,
					     r->out.info, r->out.count);
		break;
	case 5:
		result = enumprinters_level5(p->mem_ctx, session_info,
					     p->msg_ctx, r->in.flags,
					     r->in.server,
					     r->out.info, r->out.count);
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(p->mem_ctx,
						     spoolss_EnumPrinters,
						     *r->out.info, r->in.level,
						     *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************
 _spoolss_GetPrinter
****************************************************************/

WERROR _spoolss_GetPrinter(struct pipes_struct *p,
			   struct spoolss_GetPrinter *r)
{
	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);
	struct spoolss_PrinterInfo2 *info2 = NULL;
	WERROR result = WERR_OK;
	int snum;

	/* that's an [in out] buffer */

	if (!r->in.buffer && (r->in.offered != 0)) {
		return WERR_INVALID_PARAM;
	}

	*r->out.needed = 0;

	if (Printer == NULL) {
		return WERR_BADFID;
	}

	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}

	result = winreg_get_printer_internal(p->mem_ctx,
				    get_session_info_system(),
				    p->msg_ctx,
				    lp_const_servicename(snum),
				    &info2);
	if (!W_ERROR_IS_OK(result)) {
		goto out;
	}

	switch (r->in.level) {
	case 0:
		result = construct_printer_info0(p->mem_ctx,
						 get_session_info_system(),
						 p->msg_ctx,
						 info2,
						 Printer->servername,
						 &r->out.info->info0,
						 snum);
		break;
	case 1:
		result = construct_printer_info1(p->mem_ctx, info2,
						 PRINTER_ENUM_ICON8,
						 Printer->servername,
						 &r->out.info->info1, snum);
		break;
	case 2:
		result = construct_printer_info2(p->mem_ctx, p->msg_ctx, info2,
						 Printer->servername,
						 &r->out.info->info2, snum);
		break;
	case 3:
		result = construct_printer_info3(p->mem_ctx, info2,
						 Printer->servername,
						 &r->out.info->info3, snum);
		break;
	case 4:
		result = construct_printer_info4(p->mem_ctx, info2,
						 Printer->servername,
						 &r->out.info->info4, snum);
		break;
	case 5:
		result = construct_printer_info5(p->mem_ctx, info2,
						 Printer->servername,
						 &r->out.info->info5, snum);
		break;
	case 6:
		result = construct_printer_info6(p->mem_ctx, p->msg_ctx, info2,
						 Printer->servername,
						 &r->out.info->info6, snum);
		break;
	case 7:
		result = construct_printer_info7(p->mem_ctx, p->msg_ctx,
						 Printer->servername,
						 &r->out.info->info7, snum);
		break;
	case 8:
		result = construct_printer_info8(p->mem_ctx, info2,
						 Printer->servername,
						 &r->out.info->info8, snum);
		break;
	default:
		result = WERR_UNKNOWN_LEVEL;
		break;
	}

 out:
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("_spoolss_GetPrinter: failed to construct printer info level %d - %s\n",
			  r->in.level, win_errstr(result)));
		TALLOC_FREE(r->out.info);
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION(spoolss_PrinterInfo,
					       r->out.info, r->in.level);
	r->out.info	= SPOOLSS_BUFFER_OK(r->out.info, NULL);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/********************************************************************
 ********************************************************************/

#define FILL_DRIVER_STRING(mem_ctx, in, out) \
	do { \
		if (in && strlen(in)) { \
			out = talloc_strdup(mem_ctx, in); \
		} else { \
			out = talloc_strdup(mem_ctx, ""); \
		} \
		W_ERROR_HAVE_NO_MEMORY(out); \
	} while (0);

#define FILL_DRIVER_UNC_STRING(mem_ctx, server, arch, ver, in, out) \
	do { \
		if (in && strlen(in)) { \
			out = talloc_asprintf(mem_ctx, "\\\\%s\\print$\\%s\\%d\\%s", server, get_short_archi(arch), ver, in); \
		} else { \
			out = talloc_strdup(mem_ctx, ""); \
		} \
		W_ERROR_HAVE_NO_MEMORY(out); \
	} while (0);

static WERROR string_array_from_driver_info(TALLOC_CTX *mem_ctx,
						  const char **string_array,
						  const char ***presult,
						  const char *cservername,
						  const char *arch,
						  int version)
{
	int i, num_strings = 0;
	const char **array = NULL;

	if (string_array == NULL) {
		return WERR_INVALID_PARAMETER;
	}

	for (i=0; string_array[i] && string_array[i][0] != '\0'; i++) {
		const char *str = NULL;

		if (cservername == NULL || arch == NULL) {
			FILL_DRIVER_STRING(mem_ctx, string_array[i], str);
		} else {
			FILL_DRIVER_UNC_STRING(mem_ctx, cservername, arch, version, string_array[i], str);
		}

		if (!add_string_to_array(mem_ctx, str, &array, &num_strings)) {
			TALLOC_FREE(array);
			return WERR_NOMEM;
		}
	}

	if (i > 0) {
		ADD_TO_ARRAY(mem_ctx, const char *, NULL,
			     &array, &num_strings);
	}

	if (presult) {
		*presult = array;
	}

	return WERR_OK;
}

/********************************************************************
 * fill a spoolss_DriverInfo1 struct
 ********************************************************************/

static WERROR fill_printer_driver_info1(TALLOC_CTX *mem_ctx,
					struct spoolss_DriverInfo1 *r,
					const struct spoolss_DriverInfo8 *driver,
					const char *servername)
{
	r->driver_name		= talloc_strdup(mem_ctx, driver->driver_name);
	W_ERROR_HAVE_NO_MEMORY(r->driver_name);

	return WERR_OK;
}

/********************************************************************
 * fill a spoolss_DriverInfo2 struct
 ********************************************************************/

static WERROR fill_printer_driver_info2(TALLOC_CTX *mem_ctx,
					struct spoolss_DriverInfo2 *r,
					const struct spoolss_DriverInfo8 *driver,
					const char *servername)

{
	const char *cservername = canon_servername(servername);

	r->version		= driver->version;

	r->driver_name		= talloc_strdup(mem_ctx, driver->driver_name);
	W_ERROR_HAVE_NO_MEMORY(r->driver_name);
	r->architecture		= talloc_strdup(mem_ctx, driver->architecture);
	W_ERROR_HAVE_NO_MEMORY(r->architecture);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->driver_path,
			       r->driver_path);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->data_file,
			       r->data_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->config_file,
			       r->config_file);

	return WERR_OK;
}

/********************************************************************
 * fill a spoolss_DriverInfo3 struct
 ********************************************************************/

static WERROR fill_printer_driver_info3(TALLOC_CTX *mem_ctx,
					struct spoolss_DriverInfo3 *r,
					const struct spoolss_DriverInfo8 *driver,
					const char *servername)
{
	const char *cservername = canon_servername(servername);

	r->version		= driver->version;

	r->driver_name		= talloc_strdup(mem_ctx, driver->driver_name);
	W_ERROR_HAVE_NO_MEMORY(r->driver_name);
	r->architecture		= talloc_strdup(mem_ctx, driver->architecture);
	W_ERROR_HAVE_NO_MEMORY(r->architecture);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->driver_path,
			       r->driver_path);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->data_file,
			       r->data_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->config_file,
			       r->config_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->help_file,
			       r->help_file);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->monitor_name,
			   r->monitor_name);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->default_datatype,
			   r->default_datatype);

	return string_array_from_driver_info(mem_ctx,
					     driver->dependent_files,
					     &r->dependent_files,
					     cservername,
					     driver->architecture,
					     driver->version);
}

/********************************************************************
 * fill a spoolss_DriverInfo4 struct
 ********************************************************************/

static WERROR fill_printer_driver_info4(TALLOC_CTX *mem_ctx,
					struct spoolss_DriverInfo4 *r,
					const struct spoolss_DriverInfo8 *driver,
					const char *servername)
{
	const char *cservername = canon_servername(servername);
	WERROR result;

	r->version		= driver->version;

	r->driver_name		= talloc_strdup(mem_ctx, driver->driver_name);
	W_ERROR_HAVE_NO_MEMORY(r->driver_name);
	r->architecture		= talloc_strdup(mem_ctx, driver->architecture);
	W_ERROR_HAVE_NO_MEMORY(r->architecture);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->driver_path,
			       r->driver_path);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->data_file,
			       r->data_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->config_file,
			       r->config_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->help_file,
			       r->help_file);

	result = string_array_from_driver_info(mem_ctx,
					       driver->dependent_files,
					       &r->dependent_files,
					       cservername,
					       driver->architecture,
					       driver->version);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	FILL_DRIVER_STRING(mem_ctx,
			   driver->monitor_name,
			   r->monitor_name);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->default_datatype,
			   r->default_datatype);


	result = string_array_from_driver_info(mem_ctx,
					       driver->previous_names,
					       &r->previous_names,
					       NULL, NULL, 0);

	return result;
}

/********************************************************************
 * fill a spoolss_DriverInfo5 struct
 ********************************************************************/

static WERROR fill_printer_driver_info5(TALLOC_CTX *mem_ctx,
					struct spoolss_DriverInfo5 *r,
					const struct spoolss_DriverInfo8 *driver,
					const char *servername)
{
	const char *cservername = canon_servername(servername);

	r->version		= driver->version;

	r->driver_name		= talloc_strdup(mem_ctx, driver->driver_name);
	W_ERROR_HAVE_NO_MEMORY(r->driver_name);
	r->architecture		= talloc_strdup(mem_ctx, driver->architecture);
	W_ERROR_HAVE_NO_MEMORY(r->architecture);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->driver_path,
			       r->driver_path);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->data_file,
			       r->data_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->config_file,
			       r->config_file);

	r->driver_attributes	= 0;
	r->config_version	= 0;
	r->driver_version	= 0;

	return WERR_OK;
}
/********************************************************************
 * fill a spoolss_DriverInfo6 struct
 ********************************************************************/

static WERROR fill_printer_driver_info6(TALLOC_CTX *mem_ctx,
					struct spoolss_DriverInfo6 *r,
					const struct spoolss_DriverInfo8 *driver,
					const char *servername)
{
	const char *cservername = canon_servername(servername);
	WERROR result;

	r->version		= driver->version;

	r->driver_name		= talloc_strdup(mem_ctx, driver->driver_name);
	W_ERROR_HAVE_NO_MEMORY(r->driver_name);
	r->architecture		= talloc_strdup(mem_ctx, driver->architecture);
	W_ERROR_HAVE_NO_MEMORY(r->architecture);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->driver_path,
			       r->driver_path);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->data_file,
			       r->data_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->config_file,
			       r->config_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->help_file,
			       r->help_file);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->monitor_name,
			   r->monitor_name);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->default_datatype,
			   r->default_datatype);

	result = string_array_from_driver_info(mem_ctx,
					       driver->dependent_files,
					       &r->dependent_files,
					       cservername,
					       driver->architecture,
					       driver->version);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	result = string_array_from_driver_info(mem_ctx,
					       driver->previous_names,
					       &r->previous_names,
					       NULL, NULL, 0);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	r->driver_date		= driver->driver_date;
	r->driver_version	= driver->driver_version;

	FILL_DRIVER_STRING(mem_ctx,
			   driver->manufacturer_name,
			   r->manufacturer_name);
	FILL_DRIVER_STRING(mem_ctx,
			   driver->manufacturer_url,
			   r->manufacturer_url);
	FILL_DRIVER_STRING(mem_ctx,
			   driver->hardware_id,
			   r->hardware_id);
	FILL_DRIVER_STRING(mem_ctx,
			   driver->provider,
			   r->provider);

	return WERR_OK;
}

/********************************************************************
 * fill a spoolss_DriverInfo8 struct
 ********************************************************************/

static WERROR fill_printer_driver_info8(TALLOC_CTX *mem_ctx,
					struct spoolss_DriverInfo8 *r,
					const struct spoolss_DriverInfo8 *driver,
					const char *servername)
{
	const char *cservername = canon_servername(servername);
	WERROR result;

	r->version		= driver->version;

	r->driver_name		= talloc_strdup(mem_ctx, driver->driver_name);
	W_ERROR_HAVE_NO_MEMORY(r->driver_name);
	r->architecture		= talloc_strdup(mem_ctx, driver->architecture);
	W_ERROR_HAVE_NO_MEMORY(r->architecture);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->driver_path,
			       r->driver_path);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->data_file,
			       r->data_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->config_file,
			       r->config_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->architecture,
			       driver->version,
			       driver->help_file,
			       r->help_file);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->monitor_name,
			   r->monitor_name);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->default_datatype,
			   r->default_datatype);

	result = string_array_from_driver_info(mem_ctx,
					       driver->dependent_files,
					       &r->dependent_files,
					       cservername,
					       driver->architecture,
					       driver->version);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	result = string_array_from_driver_info(mem_ctx,
					       driver->previous_names,
					       &r->previous_names,
					       NULL, NULL, 0);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	r->driver_date		= driver->driver_date;
	r->driver_version	= driver->driver_version;

	FILL_DRIVER_STRING(mem_ctx,
			   driver->manufacturer_name,
			   r->manufacturer_name);
	FILL_DRIVER_STRING(mem_ctx,
			   driver->manufacturer_url,
			   r->manufacturer_url);
	FILL_DRIVER_STRING(mem_ctx,
			   driver->hardware_id,
			   r->hardware_id);
	FILL_DRIVER_STRING(mem_ctx,
			   driver->provider,
			   r->provider);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->print_processor,
			   r->print_processor);
	FILL_DRIVER_STRING(mem_ctx,
			   driver->vendor_setup,
			   r->vendor_setup);

	result = string_array_from_driver_info(mem_ctx,
					       driver->color_profiles,
					       &r->color_profiles,
					       NULL, NULL, 0);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	FILL_DRIVER_STRING(mem_ctx,
			   driver->inf_path,
			   r->inf_path);

	r->printer_driver_attributes	= driver->printer_driver_attributes;

	result = string_array_from_driver_info(mem_ctx,
					       driver->core_driver_dependencies,
					       &r->core_driver_dependencies,
					       NULL, NULL, 0);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	r->min_inbox_driver_ver_date	= driver->min_inbox_driver_ver_date;
	r->min_inbox_driver_ver_version	= driver->min_inbox_driver_ver_version;

	return WERR_OK;
}

#if 0 /* disabled until marshalling issues are resolved - gd */
/********************************************************************
 ********************************************************************/

static WERROR fill_spoolss_DriverFileInfo(TALLOC_CTX *mem_ctx,
					  struct spoolss_DriverFileInfo *r,
					  const char *cservername,
					  const char *file_name,
					  enum spoolss_DriverFileType file_type,
					  uint32_t file_version)
{
	r->file_name	= talloc_asprintf(mem_ctx, "\\\\%s%s",
					  cservername, file_name);
	W_ERROR_HAVE_NO_MEMORY(r->file_name);
	r->file_type	= file_type;
	r->file_version	= file_version;

	return WERR_OK;
}

/********************************************************************
 ********************************************************************/

static WERROR spoolss_DriverFileInfo_from_driver(TALLOC_CTX *mem_ctx,
						 const struct spoolss_DriverInfo8 *driver,
						 const char *cservername,
						 struct spoolss_DriverFileInfo **info_p,
						 uint32_t *count_p)
{
	struct spoolss_DriverFileInfo *info = NULL;
	uint32_t count = 0;
	WERROR result;
	uint32_t i;

	*info_p = NULL;
	*count_p = 0;

	if (strlen(driver->driver_path)) {
		info = TALLOC_REALLOC_ARRAY(mem_ctx, info,
					    struct spoolss_DriverFileInfo,
					    count + 1);
		W_ERROR_HAVE_NO_MEMORY(info);
		result = fill_spoolss_DriverFileInfo(info,
						     &info[count],
						     cservername,
						     driver->driver_path,
						     SPOOLSS_DRIVER_FILE_TYPE_RENDERING,
						     0);
		W_ERROR_NOT_OK_RETURN(result);
		count++;
	}

	if (strlen(driver->config_file)) {
		info = TALLOC_REALLOC_ARRAY(mem_ctx, info,
					    struct spoolss_DriverFileInfo,
					    count + 1);
		W_ERROR_HAVE_NO_MEMORY(info);
		result = fill_spoolss_DriverFileInfo(info,
						     &info[count],
						     cservername,
						     driver->config_file,
						     SPOOLSS_DRIVER_FILE_TYPE_CONFIGURATION,
						     0);
		W_ERROR_NOT_OK_RETURN(result);
		count++;
	}

	if (strlen(driver->data_file)) {
		info = TALLOC_REALLOC_ARRAY(mem_ctx, info,
					    struct spoolss_DriverFileInfo,
					    count + 1);
		W_ERROR_HAVE_NO_MEMORY(info);
		result = fill_spoolss_DriverFileInfo(info,
						     &info[count],
						     cservername,
						     driver->data_file,
						     SPOOLSS_DRIVER_FILE_TYPE_DATA,
						     0);
		W_ERROR_NOT_OK_RETURN(result);
		count++;
	}

	if (strlen(driver->help_file)) {
		info = TALLOC_REALLOC_ARRAY(mem_ctx, info,
					    struct spoolss_DriverFileInfo,
					    count + 1);
		W_ERROR_HAVE_NO_MEMORY(info);
		result = fill_spoolss_DriverFileInfo(info,
						     &info[count],
						     cservername,
						     driver->help_file,
						     SPOOLSS_DRIVER_FILE_TYPE_HELP,
						     0);
		W_ERROR_NOT_OK_RETURN(result);
		count++;
	}

	for (i=0; driver->dependent_files[i] && driver->dependent_files[i][0] != '\0'; i++) {
		info = TALLOC_REALLOC_ARRAY(mem_ctx, info,
					    struct spoolss_DriverFileInfo,
					    count + 1);
		W_ERROR_HAVE_NO_MEMORY(info);
		result = fill_spoolss_DriverFileInfo(info,
						     &info[count],
						     cservername,
						     driver->dependent_files[i],
						     SPOOLSS_DRIVER_FILE_TYPE_OTHER,
						     0);
		W_ERROR_NOT_OK_RETURN(result);
		count++;
	}

	*info_p = info;
	*count_p = count;

	return WERR_OK;
}

/********************************************************************
 * fill a spoolss_DriverInfo101 struct
 ********************************************************************/

static WERROR fill_printer_driver_info101(TALLOC_CTX *mem_ctx,
					  struct spoolss_DriverInfo101 *r,
					  const struct spoolss_DriverInfo8 *driver,
					  const char *servername)
{
	const char *cservername = canon_servername(servername);
	WERROR result;

	r->version		= driver->version;

	r->driver_name		= talloc_strdup(mem_ctx, driver->driver_name);
	W_ERROR_HAVE_NO_MEMORY(r->driver_name);
	r->architecture		= talloc_strdup(mem_ctx, driver->architecture);
	W_ERROR_HAVE_NO_MEMORY(r->architecture);

	result = spoolss_DriverFileInfo_from_driver(mem_ctx, driver,
						    cservername,
						    &r->file_info,
						    &r->file_count);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	FILL_DRIVER_STRING(mem_ctx,
			   driver->monitor_name,
			   r->monitor_name);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->default_datatype,
			   r->default_datatype);

	result = string_array_from_driver_info(mem_ctx,
					       driver->previous_names,
					       &r->previous_names,
					       NULL, NULL, 0);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	r->driver_date		= driver->driver_date;
	r->driver_version	= driver->driver_version;

	FILL_DRIVER_STRING(mem_ctx,
			   driver->manufacturer_name,
			   r->manufacturer_name);
	FILL_DRIVER_STRING(mem_ctx,
			   driver->manufacturer_url,
			   r->manufacturer_url);
	FILL_DRIVER_STRING(mem_ctx,
			   driver->hardware_id,
			   r->hardware_id);
	FILL_DRIVER_STRING(mem_ctx,
			   driver->provider,
			   r->provider);

	return WERR_OK;
}
#endif
/********************************************************************
 ********************************************************************/

static WERROR construct_printer_driver_info_level(TALLOC_CTX *mem_ctx,
						  const struct auth_serversupplied_info *session_info,
						  struct messaging_context *msg_ctx,
						  uint32_t level,
						  union spoolss_DriverInfo *r,
						  int snum,
						  const char *servername,
						  const char *architecture,
						  uint32_t version)
{
	struct spoolss_PrinterInfo2 *pinfo2 = NULL;
	struct spoolss_DriverInfo8 *driver;
	WERROR result;
	struct dcerpc_binding_handle *b;

	if (level == 101) {
		return WERR_UNKNOWN_LEVEL;
	}

	result = winreg_printer_binding_handle(mem_ctx,
					       session_info,
					       msg_ctx,
					       &b);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	result = winreg_get_printer(mem_ctx, b,
				    lp_const_servicename(snum),
				    &pinfo2);

	DEBUG(8,("construct_printer_driver_info_level: status: %s\n",
		win_errstr(result)));

	if (!W_ERROR_IS_OK(result)) {
		return WERR_INVALID_PRINTER_NAME;
	}

	result = winreg_get_driver(mem_ctx, b,
				   architecture,
				   pinfo2->drivername, version, &driver);

	DEBUG(8,("construct_printer_driver_info_level: status: %s\n",
		win_errstr(result)));

	if (!W_ERROR_IS_OK(result)) {
		/*
		 * Is this a W2k client ?
		 */

		if (version < 3) {
			talloc_free(pinfo2);
			return WERR_UNKNOWN_PRINTER_DRIVER;
		}

		/* Yes - try again with a WinNT driver. */
		version = 2;
		result = winreg_get_driver(mem_ctx, b,
					   architecture,
					   pinfo2->drivername,
					   version, &driver);
		DEBUG(8,("construct_printer_driver_level: status: %s\n",
			win_errstr(result)));
		if (!W_ERROR_IS_OK(result)) {
			talloc_free(pinfo2);
			return WERR_UNKNOWN_PRINTER_DRIVER;
		}
	}

	switch (level) {
	case 1:
		result = fill_printer_driver_info1(mem_ctx, &r->info1, driver, servername);
		break;
	case 2:
		result = fill_printer_driver_info2(mem_ctx, &r->info2, driver, servername);
		break;
	case 3:
		result = fill_printer_driver_info3(mem_ctx, &r->info3, driver, servername);
		break;
	case 4:
		result = fill_printer_driver_info4(mem_ctx, &r->info4, driver, servername);
		break;
	case 5:
		result = fill_printer_driver_info5(mem_ctx, &r->info5, driver, servername);
		break;
	case 6:
		result = fill_printer_driver_info6(mem_ctx, &r->info6, driver, servername);
		break;
	case 8:
		result = fill_printer_driver_info8(mem_ctx, &r->info8, driver, servername);
		break;
#if 0 /* disabled until marshalling issues are resolved - gd */
	case 101:
		result = fill_printer_driver_info101(mem_ctx, &r->info101, driver, servername);
		break;
#endif
	default:
		result = WERR_UNKNOWN_LEVEL;
		break;
	}

	talloc_free(pinfo2);
	talloc_free(driver);

	return result;
}

/****************************************************************
 _spoolss_GetPrinterDriver2
****************************************************************/

WERROR _spoolss_GetPrinterDriver2(struct pipes_struct *p,
				  struct spoolss_GetPrinterDriver2 *r)
{
	struct printer_handle *printer;
	WERROR result;
	uint32_t version = r->in.client_major_version;

	int snum;

	/* that's an [in out] buffer */

	if (!r->in.buffer && (r->in.offered != 0)) {
		return WERR_INVALID_PARAM;
	}

	DEBUG(4,("_spoolss_GetPrinterDriver2\n"));

	if (!(printer = find_printer_index_by_hnd(p, r->in.handle))) {
		DEBUG(0,("_spoolss_GetPrinterDriver2: invalid printer handle!\n"));
		return WERR_INVALID_PRINTER_NAME;
	}

	*r->out.needed = 0;
	*r->out.server_major_version = 0;
	*r->out.server_minor_version = 0;

	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}

	if (r->in.client_major_version == SPOOLSS_DRIVER_VERSION_2012) {
		DEBUG(3,("_spoolss_GetPrinterDriver2: v4 driver requested, "
			"downgrading to v3\n"));
		version = SPOOLSS_DRIVER_VERSION_200X;
	}

	result = construct_printer_driver_info_level(p->mem_ctx,
						     get_session_info_system(),
						     p->msg_ctx,
						     r->in.level, r->out.info,
						     snum, printer->servername,
						     r->in.architecture,
						     version);
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(r->out.info);
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION(spoolss_DriverInfo,
					       r->out.info, r->in.level);
	r->out.info	= SPOOLSS_BUFFER_OK(r->out.info, NULL);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}


/****************************************************************
 _spoolss_StartPagePrinter
****************************************************************/

WERROR _spoolss_StartPagePrinter(struct pipes_struct *p,
				 struct spoolss_StartPagePrinter *r)
{
	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);

	if (!Printer) {
		DEBUG(3,("_spoolss_StartPagePrinter: "
			"Error in startpageprinter printer handle\n"));
		return WERR_BADFID;
	}

	Printer->page_started = true;
	return WERR_OK;
}

/****************************************************************
 _spoolss_EndPagePrinter
****************************************************************/

WERROR _spoolss_EndPagePrinter(struct pipes_struct *p,
			       struct spoolss_EndPagePrinter *r)
{
	int snum;

	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);

	if (!Printer) {
		DEBUG(2,("_spoolss_EndPagePrinter: Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	if (!get_printer_snum(p, r->in.handle, &snum, NULL))
		return WERR_BADFID;

	Printer->page_started = false;
	print_job_endpage(p->msg_ctx, snum, Printer->jobid);

	return WERR_OK;
}

/****************************************************************
 _spoolss_StartDocPrinter
****************************************************************/

WERROR _spoolss_StartDocPrinter(struct pipes_struct *p,
				struct spoolss_StartDocPrinter *r)
{
	struct spoolss_DocumentInfo1 *info_1;
	int snum;
	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);
	WERROR werr;

	if (!Printer) {
		DEBUG(2,("_spoolss_StartDocPrinter: "
			"Invalid handle (%s:%u:%u)\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	if (Printer->jobid) {
		DEBUG(2, ("_spoolss_StartDocPrinter: "
			  "StartDocPrinter called twice! "
			  "(existing jobid = %d)\n", Printer->jobid));
		return WERR_INVALID_HANDLE;
	}

	if (r->in.level != 1) {
		return WERR_UNKNOWN_LEVEL;
	}

	info_1 = r->in.info.info1;

	/*
	 * a nice thing with NT is it doesn't listen to what you tell it.
	 * when asked to send _only_ RAW datas, it tries to send datas
	 * in EMF format.
	 *
	 * So I add checks like in NT Server ...
	 */

	if (info_1->datatype) {
		/*
		 * The v4 driver model used in Windows 8 declares print jobs
		 * intended to bypass the XPS processing layer by setting
		 * datatype to "XPS_PASS" instead of "RAW".
		 */
                if ((strcmp(info_1->datatype, "RAW") != 0)
                 && (strcmp(info_1->datatype, "XPS_PASS") != 0)) {
			*r->out.job_id = 0;
			return WERR_INVALID_DATATYPE;
		}
	}

	/* get the share number of the printer */
	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}

	werr = print_job_start(p->session_info,
			       p->msg_ctx,
			       p->client_id->name,
			       snum,
			       info_1->document_name,
			       info_1->output_file,
			       Printer->devmode,
			       &Printer->jobid);

	/* An error occured in print_job_start() so return an appropriate
	   NT error code. */

	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	Printer->document_started = true;
	*r->out.job_id = Printer->jobid;

	return WERR_OK;
}

/****************************************************************
 _spoolss_EndDocPrinter
****************************************************************/

WERROR _spoolss_EndDocPrinter(struct pipes_struct *p,
			      struct spoolss_EndDocPrinter *r)
{
	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);
	NTSTATUS status;
	int snum;

	if (!Printer) {
		DEBUG(2,("_spoolss_EndDocPrinter: Invalid handle (%s:%u:%u)\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}

	Printer->document_started = false;
	status = print_job_end(p->msg_ctx, snum, Printer->jobid, NORMAL_CLOSE);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(2, ("_spoolss_EndDocPrinter: "
			  "print_job_end failed [%s]\n",
			  nt_errstr(status)));
	}

	Printer->jobid = 0;
	return ntstatus_to_werror(status);
}

/****************************************************************
 _spoolss_WritePrinter
****************************************************************/

WERROR _spoolss_WritePrinter(struct pipes_struct *p,
			     struct spoolss_WritePrinter *r)
{
	ssize_t buffer_written;
	int snum;
	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);

	if (!Printer) {
		DEBUG(2,("_spoolss_WritePrinter: Invalid handle (%s:%u:%u)\n",
			OUR_HANDLE(r->in.handle)));
		*r->out.num_written = r->in._data_size;
		return WERR_BADFID;
	}

	if (!get_printer_snum(p, r->in.handle, &snum, NULL))
		return WERR_BADFID;

	/* print_job_write takes care of checking for PJOB_SMBD_SPOOLING */
	buffer_written = print_job_write(server_event_context(),p->msg_ctx,
						   snum, Printer->jobid,
						   (const char *)r->in.data.data,
						   (size_t)r->in._data_size);
	if (buffer_written == (ssize_t)-1) {
		*r->out.num_written = 0;
		if (errno == ENOSPC)
			return WERR_NO_SPOOL_SPACE;
		else
			return WERR_ACCESS_DENIED;
	}

	*r->out.num_written = r->in._data_size;

	return WERR_OK;
}

/********************************************************************
 * api_spoolss_getprinter
 * called from the spoolss dispatcher
 *
 ********************************************************************/

static WERROR control_printer(struct policy_handle *handle, uint32_t command,
			      struct pipes_struct *p)
{
	const struct auth_serversupplied_info *session_info = p->session_info;
	int snum;
	WERROR errcode = WERR_BADFUNC;
	struct printer_handle *Printer = find_printer_index_by_hnd(p, handle);

	if (!Printer) {
		DEBUG(2,("control_printer: Invalid handle (%s:%u:%u)\n",
			OUR_HANDLE(handle)));
		return WERR_BADFID;
	}

	if (!get_printer_snum(p, handle, &snum, NULL))
		return WERR_BADFID;

	switch (command) {
	case SPOOLSS_PRINTER_CONTROL_PAUSE:
		errcode = print_queue_pause(session_info, p->msg_ctx, snum);
		break;
	case SPOOLSS_PRINTER_CONTROL_RESUME:
	case SPOOLSS_PRINTER_CONTROL_UNPAUSE:
		errcode = print_queue_resume(session_info, p->msg_ctx, snum);
		break;
	case SPOOLSS_PRINTER_CONTROL_PURGE:
		errcode = print_queue_purge(session_info, p->msg_ctx, snum);
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	return errcode;
}


/****************************************************************
 _spoolss_AbortPrinter
 * From MSDN: "Deletes printer's spool file if printer is configured
 * for spooling"
****************************************************************/

WERROR _spoolss_AbortPrinter(struct pipes_struct *p,
			     struct spoolss_AbortPrinter *r)
{
	struct printer_handle 	*Printer = find_printer_index_by_hnd(p, r->in.handle);
	int		snum;
	WERROR 		errcode = WERR_OK;

	if (!Printer) {
		DEBUG(2,("_spoolss_AbortPrinter: Invalid handle (%s:%u:%u)\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	if (!get_printer_snum(p, r->in.handle, &snum, NULL))
		return WERR_BADFID;

	if (!Printer->document_started) {
		return WERR_SPL_NO_STARTDOC;
	}

	errcode = print_job_delete(p->session_info,
				   p->msg_ctx,
				   snum,
				   Printer->jobid);

	return errcode;
}

/********************************************************************
 * called by spoolss_api_setprinter
 * when updating a printer description
 ********************************************************************/

static WERROR update_printer_sec(struct policy_handle *handle,
				 struct pipes_struct *p,
				 struct sec_desc_buf *secdesc_ctr)
{
	struct spoolss_security_descriptor *new_secdesc = NULL;
	struct spoolss_security_descriptor *old_secdesc = NULL;
	const char *printer;
	WERROR result;
	int snum;
	struct printer_handle *Printer = find_printer_index_by_hnd(p, handle);
	struct dcerpc_binding_handle *b;

	if (!Printer || !get_printer_snum(p, handle, &snum, NULL)) {
		DEBUG(2,("update_printer_sec: Invalid handle (%s:%u:%u)\n",
			 OUR_HANDLE(handle)));

		result = WERR_BADFID;
		goto done;
	}

	if (secdesc_ctr == NULL) {
		DEBUG(10,("update_printer_sec: secdesc_ctr is NULL !\n"));
		result = WERR_INVALID_PARAM;
		goto done;
	}
	printer = lp_const_servicename(snum);

	/* Check the user has permissions to change the security
	   descriptor.  By experimentation with two NT machines, the user
	   requires Full Access to the printer to change security
	   information. */

	if ( Printer->access_granted != PRINTER_ACCESS_ADMINISTER ) {
		DEBUG(4,("update_printer_sec: updated denied by printer permissions\n"));
		result = WERR_ACCESS_DENIED;
		goto done;
	}

	result = winreg_printer_binding_handle(p->mem_ctx,
					       get_session_info_system(),
					       p->msg_ctx,
					       &b);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	/* NT seems to like setting the security descriptor even though
	   nothing may have actually changed. */
	result = winreg_get_printer_secdesc(p->mem_ctx, b,
					    printer,
					    &old_secdesc);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(2,("update_printer_sec: winreg_get_printer_secdesc_internal() failed\n"));
		result = WERR_BADFID;
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		struct security_acl *the_acl;
		int i;

		the_acl = old_secdesc->dacl;
		DEBUG(10, ("old_secdesc_ctr for %s has %d aces:\n",
			   printer, the_acl->num_aces));

		for (i = 0; i < the_acl->num_aces; i++) {
			DEBUG(10, ("%s 0x%08x\n", sid_string_dbg(
					   &the_acl->aces[i].trustee),
				  the_acl->aces[i].access_mask));
		}

		the_acl = secdesc_ctr->sd->dacl;

		if (the_acl) {
			DEBUG(10, ("secdesc_ctr for %s has %d aces:\n",
				   printer, the_acl->num_aces));

			for (i = 0; i < the_acl->num_aces; i++) {
				DEBUG(10, ("%s 0x%08x\n", sid_string_dbg(
						   &the_acl->aces[i].trustee),
					   the_acl->aces[i].access_mask));
			}
		} else {
			DEBUG(10, ("dacl for secdesc_ctr is NULL\n"));
		}
	}

	new_secdesc = sec_desc_merge(p->mem_ctx, secdesc_ctr->sd, old_secdesc);
	if (new_secdesc == NULL) {
		result = WERR_NOMEM;
		goto done;
	}

	if (security_descriptor_equal(new_secdesc, old_secdesc)) {
		result = WERR_OK;
		goto done;
	}

	result = winreg_set_printer_secdesc(p->mem_ctx, b,
					    printer,
					    new_secdesc);

 done:
	return result;
}

/********************************************************************
 Canonicalize printer info from a client
 ********************************************************************/

static bool check_printer_ok(TALLOC_CTX *mem_ctx,
			     struct spoolss_SetPrinterInfo2 *info2,
			     int snum)
{
	fstring printername;
	const char *p;

	DEBUG(5,("check_printer_ok: servername=%s printername=%s sharename=%s "
		"portname=%s drivername=%s comment=%s location=%s\n",
		info2->servername, info2->printername, info2->sharename,
		info2->portname, info2->drivername, info2->comment,
		info2->location));

	/* we force some elements to "correct" values */
	info2->servername = talloc_asprintf(mem_ctx, "\\\\%s", global_myname());
	if (info2->servername == NULL) {
		return false;
	}
	info2->sharename = talloc_strdup(mem_ctx, lp_const_servicename(snum));
	if (info2->sharename == NULL) {
		return false;
	}

	/* check to see if we allow printername != sharename */
	if (lp_force_printername(snum)) {
		info2->printername = talloc_asprintf(mem_ctx, "\\\\%s\\%s",
					global_myname(), info2->sharename);
	} else {
		/* make sure printername is in \\server\printername format */
		fstrcpy(printername, info2->printername);
		p = printername;
		if ( printername[0] == '\\' && printername[1] == '\\' ) {
			if ( (p = strchr_m( &printername[2], '\\' )) != NULL )
				p++;
		}

		info2->printername = talloc_asprintf(mem_ctx, "\\\\%s\\%s",
					global_myname(), p);
	}
	if (info2->printername == NULL) {
		return false;
	}

	info2->attributes |= PRINTER_ATTRIBUTE_SAMBA;
	info2->attributes &= ~PRINTER_ATTRIBUTE_NOT_SAMBA;

	return true;
}

/****************************************************************************
****************************************************************************/

static WERROR add_port_hook(TALLOC_CTX *ctx, struct security_token *token, const char *portname, const char *uri)
{
	char *cmd = lp_addport_cmd();
	char *command = NULL;
	int ret;
	bool is_print_op = false;

	if ( !*cmd ) {
		return WERR_ACCESS_DENIED;
	}

	command = talloc_asprintf(ctx,
			"%s \"%s\" \"%s\"", cmd, portname, uri );
	if (!command) {
		return WERR_NOMEM;
	}

	if ( token )
		is_print_op = security_token_has_privilege(token, SEC_PRIV_PRINT_OPERATOR);

	DEBUG(10,("Running [%s]\n", command));

	/********* BEGIN SePrintOperatorPrivilege **********/

	if ( is_print_op )
		become_root();

	ret = smbrun(command, NULL);

	if ( is_print_op )
		unbecome_root();

	/********* END SePrintOperatorPrivilege **********/

	DEBUGADD(10,("returned [%d]\n", ret));

	TALLOC_FREE(command);

	if ( ret != 0 ) {
		return WERR_ACCESS_DENIED;
	}

	return WERR_OK;
}

/****************************************************************************
****************************************************************************/

static bool add_printer_hook(TALLOC_CTX *ctx, struct security_token *token,
			     struct spoolss_SetPrinterInfo2 *info2,
			     const char *remote_machine,
			     struct messaging_context *msg_ctx)
{
	char *cmd = lp_addprinter_cmd();
	char **qlines;
	char *command = NULL;
	int numlines;
	int ret;
	int fd;
	bool is_print_op = false;

	if (!remote_machine) {
		return false;
	}

	command = talloc_asprintf(ctx,
			"%s \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"",
			cmd, info2->printername, info2->sharename,
			info2->portname, info2->drivername,
			info2->location, info2->comment, remote_machine);
	if (!command) {
		return false;
	}

	if ( token )
		is_print_op = security_token_has_privilege(token, SEC_PRIV_PRINT_OPERATOR);

	DEBUG(10,("Running [%s]\n", command));

	/********* BEGIN SePrintOperatorPrivilege **********/

	if ( is_print_op )
		become_root();

	if ( (ret = smbrun(command, &fd)) == 0 ) {
		/* Tell everyone we updated smb.conf. */
		message_send_all(msg_ctx, MSG_SMB_CONF_UPDATED, NULL, 0, NULL);
	}

	if ( is_print_op )
		unbecome_root();

	/********* END SePrintOperatorPrivilege **********/

	DEBUGADD(10,("returned [%d]\n", ret));

	TALLOC_FREE(command);

	if ( ret != 0 ) {
		if (fd != -1)
			close(fd);
		return false;
	}

	/* reload our services immediately */
	become_root();
	reload_services(msg_ctx, -1, false);
	unbecome_root();

	numlines = 0;
	/* Get lines and convert them back to dos-codepage */
	qlines = fd_lines_load(fd, &numlines, 0, NULL);
	DEBUGADD(10,("Lines returned = [%d]\n", numlines));
	close(fd);

	/* Set the portname to what the script says the portname should be. */
	/* but don't require anything to be return from the script exit a good error code */

	if (numlines) {
		/* Set the portname to what the script says the portname should be. */
		info2->portname = talloc_strdup(ctx, qlines[0]);
		DEBUGADD(6,("Line[0] = [%s]\n", qlines[0]));
	}

	TALLOC_FREE(qlines);
	return true;
}

static WERROR update_dsspooler(TALLOC_CTX *mem_ctx,
			       const struct auth_serversupplied_info *session_info,
			       struct messaging_context *msg_ctx,
			       int snum,
			       struct spoolss_SetPrinterInfo2 *printer,
			       struct spoolss_PrinterInfo2 *old_printer)
{
	bool force_update = (old_printer == NULL);
	const char *dnsdomname;
	const char *longname;
	const char *uncname;
	const char *spooling;
	DATA_BLOB buffer;
	WERROR result = WERR_OK;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx,
					       session_info,
					       msg_ctx,
					       &b);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	if (force_update || !strequal(printer->drivername, old_printer->drivername)) {
		push_reg_sz(mem_ctx, &buffer, printer->drivername);
		winreg_set_printer_dataex(mem_ctx, b,
					  printer->sharename,
					  SPOOL_DSSPOOLER_KEY,
					  SPOOL_REG_DRIVERNAME,
					  REG_SZ,
					  buffer.data,
					  buffer.length);

		if (!force_update) {
			DEBUG(10,("update_printer: changing driver [%s]!  Sending event!\n",
				printer->drivername));

			notify_printer_driver(server_event_context(), msg_ctx,
					      snum, printer->drivername ?
					      printer->drivername : "");
		}
	}

	if (force_update || !strequal(printer->comment, old_printer->comment)) {
		push_reg_sz(mem_ctx, &buffer, printer->comment);
		winreg_set_printer_dataex(mem_ctx, b,
					  printer->sharename,
					  SPOOL_DSSPOOLER_KEY,
					  SPOOL_REG_DESCRIPTION,
					  REG_SZ,
					  buffer.data,
					  buffer.length);

		if (!force_update) {
			notify_printer_comment(server_event_context(), msg_ctx,
					       snum, printer->comment ?
					       printer->comment : "");
		}
	}

	if (force_update || !strequal(printer->sharename, old_printer->sharename)) {
		push_reg_sz(mem_ctx, &buffer, printer->sharename);
		winreg_set_printer_dataex(mem_ctx, b,
					  printer->sharename,
					  SPOOL_DSSPOOLER_KEY,
					  SPOOL_REG_PRINTSHARENAME,
					  REG_SZ,
					  buffer.data,
					  buffer.length);

		if (!force_update) {
			notify_printer_sharename(server_event_context(),
						 msg_ctx,
						 snum, printer->sharename ?
						 printer->sharename : "");
		}
	}

	if (force_update || !strequal(printer->printername, old_printer->printername)) {
		const char *p;

		p = strrchr(printer->printername, '\\' );
		if (p != NULL) {
			p++;
		} else {
			p = printer->printername;
		}

		push_reg_sz(mem_ctx, &buffer, p);
		winreg_set_printer_dataex(mem_ctx, b,
					  printer->sharename,
					  SPOOL_DSSPOOLER_KEY,
					  SPOOL_REG_PRINTERNAME,
					  REG_SZ,
					  buffer.data,
					  buffer.length);

		if (!force_update) {
			notify_printer_printername(server_event_context(),
						   msg_ctx, snum, p ? p : "");
		}
	}

	if (force_update || !strequal(printer->portname, old_printer->portname)) {
		push_reg_sz(mem_ctx, &buffer, printer->portname);
		winreg_set_printer_dataex(mem_ctx, b,
					  printer->sharename,
					  SPOOL_DSSPOOLER_KEY,
					  SPOOL_REG_PORTNAME,
					  REG_SZ,
					  buffer.data,
					  buffer.length);

		if (!force_update) {
			notify_printer_port(server_event_context(),
					    msg_ctx, snum, printer->portname ?
					    printer->portname : "");
		}
	}

	if (force_update || !strequal(printer->location, old_printer->location)) {
		push_reg_sz(mem_ctx, &buffer, printer->location);
		winreg_set_printer_dataex(mem_ctx, b,
					  printer->sharename,
					  SPOOL_DSSPOOLER_KEY,
					  SPOOL_REG_LOCATION,
					  REG_SZ,
					  buffer.data,
					  buffer.length);

		if (!force_update) {
			notify_printer_location(server_event_context(),
						msg_ctx, snum,
						printer->location ?
						printer->location : "");
		}
	}

	if (force_update || !strequal(printer->sepfile, old_printer->sepfile)) {
		push_reg_sz(mem_ctx, &buffer, printer->sepfile);
		winreg_set_printer_dataex(mem_ctx, b,
					  printer->sharename,
					  SPOOL_DSSPOOLER_KEY,
					  SPOOL_REG_PRINTSEPARATORFILE,
					  REG_SZ,
					  buffer.data,
					  buffer.length);

		if (!force_update) {
			notify_printer_sepfile(server_event_context(),
					       msg_ctx, snum,
					       printer->sepfile ?
					       printer->sepfile : "");
		}
	}

	if (force_update || printer->starttime != old_printer->starttime) {
		buffer = data_blob_talloc(mem_ctx, NULL, 4);
		SIVAL(buffer.data, 0, printer->starttime);
		winreg_set_printer_dataex(mem_ctx, b,
					  printer->sharename,
					  SPOOL_DSSPOOLER_KEY,
					  SPOOL_REG_PRINTSTARTTIME,
					  REG_DWORD,
					  buffer.data,
					  buffer.length);
	}

	if (force_update || printer->untiltime != old_printer->untiltime) {
		buffer = data_blob_talloc(mem_ctx, NULL, 4);
		SIVAL(buffer.data, 0, printer->untiltime);
		winreg_set_printer_dataex(mem_ctx, b,
					  printer->sharename,
					  SPOOL_DSSPOOLER_KEY,
					  SPOOL_REG_PRINTENDTIME,
					  REG_DWORD,
					  buffer.data,
					  buffer.length);
	}

	if (force_update || printer->priority != old_printer->priority) {
		buffer = data_blob_talloc(mem_ctx, NULL, 4);
		SIVAL(buffer.data, 0, printer->priority);
		winreg_set_printer_dataex(mem_ctx, b,
					  printer->sharename,
					  SPOOL_DSSPOOLER_KEY,
					  SPOOL_REG_PRIORITY,
					  REG_DWORD,
					  buffer.data,
					  buffer.length);
	}

	if (force_update || printer->attributes != old_printer->attributes) {
		buffer = data_blob_talloc(mem_ctx, NULL, 4);
		SIVAL(buffer.data, 0, (printer->attributes &
				       PRINTER_ATTRIBUTE_KEEPPRINTEDJOBS));
		winreg_set_printer_dataex(mem_ctx, b,
					  printer->sharename,
					  SPOOL_DSSPOOLER_KEY,
					  SPOOL_REG_PRINTKEEPPRINTEDJOBS,
					  REG_DWORD,
					  buffer.data,
					  buffer.length);

		switch (printer->attributes & 0x3) {
			case 0:
				spooling = SPOOL_REGVAL_PRINTWHILESPOOLING;
				break;
			case 1:
				spooling = SPOOL_REGVAL_PRINTAFTERSPOOLED;
				break;
			case 2:
				spooling = SPOOL_REGVAL_PRINTDIRECT;
				break;
			default:
				spooling = "unknown";
		}
		push_reg_sz(mem_ctx, &buffer, spooling);
		winreg_set_printer_dataex(mem_ctx, b,
					  printer->sharename,
					  SPOOL_DSSPOOLER_KEY,
					  SPOOL_REG_PRINTSPOOLING,
					  REG_SZ,
					  buffer.data,
					  buffer.length);
	}

	push_reg_sz(mem_ctx, &buffer, global_myname());
	winreg_set_printer_dataex(mem_ctx, b,
				  printer->sharename,
				  SPOOL_DSSPOOLER_KEY,
				  SPOOL_REG_SHORTSERVERNAME,
				  REG_SZ,
				  buffer.data,
				  buffer.length);

	dnsdomname = get_mydnsfullname();
	if (dnsdomname != NULL && dnsdomname[0] != '\0') {
		longname = talloc_strdup(mem_ctx, dnsdomname);
	} else {
		longname = talloc_strdup(mem_ctx, global_myname());
	}
	if (longname == NULL) {
		result = WERR_NOMEM;
		goto done;
	}

	push_reg_sz(mem_ctx, &buffer, longname);
	winreg_set_printer_dataex(mem_ctx, b,
				  printer->sharename,
				  SPOOL_DSSPOOLER_KEY,
				  SPOOL_REG_SERVERNAME,
				  REG_SZ,
				  buffer.data,
				  buffer.length);

	uncname = talloc_asprintf(mem_ctx, "\\\\%s\\%s",
				  global_myname(), printer->sharename);
	push_reg_sz(mem_ctx, &buffer, uncname);
	winreg_set_printer_dataex(mem_ctx, b,
				  printer->sharename,
				  SPOOL_DSSPOOLER_KEY,
				  SPOOL_REG_UNCNAME,
				  REG_SZ,
				  buffer.data,
				  buffer.length);

done:
	return result;
}

/********************************************************************
 * Called by spoolss_api_setprinter
 * when updating a printer description.
 ********************************************************************/

static WERROR update_printer(struct pipes_struct *p,
			     struct policy_handle *handle,
			     struct spoolss_SetPrinterInfoCtr *info_ctr,
			     struct spoolss_DeviceMode *devmode)
{
	uint32_t printer_mask = SPOOLSS_PRINTER_INFO_ALL;
	struct spoolss_SetPrinterInfo2 *printer = info_ctr->info.info2;
	struct spoolss_PrinterInfo2 *old_printer;
	struct printer_handle *Printer = find_printer_index_by_hnd(p, handle);
	int snum;
	WERROR result = WERR_OK;
	TALLOC_CTX *tmp_ctx;
	struct dcerpc_binding_handle *b;

	DEBUG(8,("update_printer\n"));

	tmp_ctx = talloc_new(p->mem_ctx);
	if (tmp_ctx == NULL) {
		return WERR_NOMEM;
	}

	if (!Printer) {
		result = WERR_BADFID;
		goto done;
	}

	if (!get_printer_snum(p, handle, &snum, NULL)) {
		result = WERR_BADFID;
		goto done;
	}

	result = winreg_printer_binding_handle(tmp_ctx,
					       get_session_info_system(),
					       p->msg_ctx,
					       &b);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	result = winreg_get_printer(tmp_ctx, b,
				    lp_const_servicename(snum),
				    &old_printer);
	if (!W_ERROR_IS_OK(result)) {
		result = WERR_BADFID;
		goto done;
	}

	/* Do sanity check on the requested changes for Samba */
	if (!check_printer_ok(tmp_ctx, printer, snum)) {
		result = WERR_INVALID_PARAM;
		goto done;
	}

	/* FIXME!!! If the driver has changed we really should verify that
	   it is installed before doing much else   --jerry */

	/* Check calling user has permission to update printer description */
	if (Printer->access_granted != PRINTER_ACCESS_ADMINISTER) {
		DEBUG(3, ("update_printer: printer property change denied by handle\n"));
		result = WERR_ACCESS_DENIED;
		goto done;
	}

	/* Call addprinter hook */
	/* Check changes to see if this is really needed */

	if (*lp_addprinter_cmd() &&
			(!strequal(printer->drivername, old_printer->drivername) ||
			 !strequal(printer->comment, old_printer->comment) ||
			 !strequal(printer->portname, old_printer->portname) ||
			 !strequal(printer->location, old_printer->location)) )
	{
		/* add_printer_hook() will call reload_services() */
		if (!add_printer_hook(tmp_ctx, p->session_info->security_token,
				      printer, p->client_id->addr,
				      p->msg_ctx)) {
			result = WERR_ACCESS_DENIED;
			goto done;
		}
	}

	update_dsspooler(tmp_ctx,
			 get_session_info_system(),
			 p->msg_ctx,
			 snum,
			 printer,
			 old_printer);

	printer_mask &= ~SPOOLSS_PRINTER_INFO_SECDESC;

	if (devmode == NULL) {
		printer_mask &= ~SPOOLSS_PRINTER_INFO_DEVMODE;
	}
	result = winreg_update_printer(tmp_ctx, b,
				       printer->sharename,
				       printer_mask,
				       printer,
				       devmode,
				       NULL);

done:
	talloc_free(tmp_ctx);

	return result;
}

/****************************************************************************
****************************************************************************/
static WERROR publish_or_unpublish_printer(struct pipes_struct *p,
					   struct policy_handle *handle,
					   struct spoolss_SetPrinterInfo7 *info7)
{
#ifdef HAVE_ADS
	struct spoolss_PrinterInfo2 *pinfo2 = NULL;
	WERROR result;
	int snum;
	struct printer_handle *Printer;

	if ( lp_security() != SEC_ADS ) {
		return WERR_UNKNOWN_LEVEL;
	}

	Printer = find_printer_index_by_hnd(p, handle);

	DEBUG(5,("publish_or_unpublish_printer, action = %d\n",info7->action));

	if (!Printer)
		return WERR_BADFID;

	if (!get_printer_snum(p, handle, &snum, NULL))
		return WERR_BADFID;

	result = winreg_get_printer_internal(p->mem_ctx,
				    get_session_info_system(),
				    p->msg_ctx,
				    lp_servicename(snum),
				    &pinfo2);
	if (!W_ERROR_IS_OK(result)) {
		return WERR_BADFID;
	}

	nt_printer_publish(pinfo2,
			   get_session_info_system(),
			   p->msg_ctx,
			   pinfo2,
			   info7->action);

	TALLOC_FREE(pinfo2);
	return WERR_OK;
#else
	return WERR_UNKNOWN_LEVEL;
#endif
}

/********************************************************************
 ********************************************************************/

static WERROR update_printer_devmode(struct pipes_struct *p,
				     struct policy_handle *handle,
				     struct spoolss_DeviceMode *devmode)
{
	int snum;
	struct printer_handle *Printer = find_printer_index_by_hnd(p, handle);
	uint32_t info2_mask = SPOOLSS_PRINTER_INFO_DEVMODE;

	DEBUG(8,("update_printer_devmode\n"));

	if (!Printer) {
		return WERR_BADFID;
	}

	if (!get_printer_snum(p, handle, &snum, NULL)) {
		return WERR_BADFID;
	}

	/* Check calling user has permission to update printer description */
	if (Printer->access_granted != PRINTER_ACCESS_ADMINISTER) {
		DEBUG(3, ("update_printer: printer property change denied by handle\n"));
		return WERR_ACCESS_DENIED;
	}

	return winreg_update_printer_internal(p->mem_ctx,
				     get_session_info_system(),
				     p->msg_ctx,
				     lp_const_servicename(snum),
				     info2_mask,
				     NULL,
				     devmode,
				     NULL);
}


/****************************************************************
 _spoolss_SetPrinter
****************************************************************/

WERROR _spoolss_SetPrinter(struct pipes_struct *p,
			   struct spoolss_SetPrinter *r)
{
	WERROR result;

	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);

	if (!Printer) {
		DEBUG(2,("_spoolss_SetPrinter: Invalid handle (%s:%u:%u)\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	/* check the level */
	switch (r->in.info_ctr->level) {
		case 0:
			return control_printer(r->in.handle, r->in.command, p);
		case 2:
			result = update_printer(p, r->in.handle,
						r->in.info_ctr,
						r->in.devmode_ctr->devmode);
			if (!W_ERROR_IS_OK(result))
				return result;
			if (r->in.secdesc_ctr->sd)
				result = update_printer_sec(r->in.handle, p,
							    r->in.secdesc_ctr);
			return result;
		case 3:
			return update_printer_sec(r->in.handle, p,
						  r->in.secdesc_ctr);
		case 7:
			return publish_or_unpublish_printer(p, r->in.handle,
							    r->in.info_ctr->info.info7);
		case 8:
			return update_printer_devmode(p, r->in.handle,
						      r->in.devmode_ctr->devmode);
		default:
			return WERR_UNKNOWN_LEVEL;
	}
}

/****************************************************************
 _spoolss_FindClosePrinterNotify
****************************************************************/

WERROR _spoolss_FindClosePrinterNotify(struct pipes_struct *p,
				       struct spoolss_FindClosePrinterNotify *r)
{
	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);

	if (!Printer) {
		DEBUG(2,("_spoolss_FindClosePrinterNotify: "
			"Invalid handle (%s:%u:%u)\n", OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	if (Printer->notify.cli_chan != NULL &&
	    Printer->notify.cli_chan->active_connections > 0) {
		int snum = -1;

		if (Printer->printer_type == SPLHND_PRINTER) {
			if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
				return WERR_BADFID;
			}
		}

		srv_spoolss_replycloseprinter(snum, Printer);
	}

	Printer->notify.flags=0;
	Printer->notify.options=0;
	Printer->notify.localmachine[0]='\0';
	Printer->notify.printerlocal=0;
	TALLOC_FREE(Printer->notify.option);

	return WERR_OK;
}

/****************************************************************
 _spoolss_AddJob
****************************************************************/

WERROR _spoolss_AddJob(struct pipes_struct *p,
		       struct spoolss_AddJob *r)
{
	if (!r->in.buffer && (r->in.offered != 0)) {
		return WERR_INVALID_PARAM;
	}

	/* this is what a NT server returns for AddJob. AddJob must fail on
	 * non-local printers */

	if (r->in.level != 1) {
		return WERR_UNKNOWN_LEVEL;
	}

	return WERR_INVALID_PARAM;
}

/****************************************************************************
fill_job_info1
****************************************************************************/

static WERROR fill_job_info1(TALLOC_CTX *mem_ctx,
			     struct spoolss_JobInfo1 *r,
			     const print_queue_struct *queue,
			     int position, int snum,
			     struct spoolss_PrinterInfo2 *pinfo2)
{
	struct tm *t;

	t = gmtime(&queue->time);

	r->job_id		= queue->sysjob;

	r->printer_name		= talloc_strdup(mem_ctx, lp_servicename(snum));
	W_ERROR_HAVE_NO_MEMORY(r->printer_name);
	r->server_name		= talloc_strdup(mem_ctx, pinfo2->servername);
	W_ERROR_HAVE_NO_MEMORY(r->server_name);
	r->user_name		= talloc_strdup(mem_ctx, queue->fs_user);
	W_ERROR_HAVE_NO_MEMORY(r->user_name);
	r->document_name	= talloc_strdup(mem_ctx, queue->fs_file);
	W_ERROR_HAVE_NO_MEMORY(r->document_name);
	r->data_type		= talloc_strdup(mem_ctx, "RAW");
	W_ERROR_HAVE_NO_MEMORY(r->data_type);
	r->text_status		= talloc_strdup(mem_ctx, "");
	W_ERROR_HAVE_NO_MEMORY(r->text_status);

	r->status		= nt_printj_status(queue->status);
	r->priority		= queue->priority;
	r->position		= position;
	r->total_pages		= queue->page_count;
	r->pages_printed	= 0; /* ??? */

	init_systemtime(&r->submitted, t);

	return WERR_OK;
}

/****************************************************************************
fill_job_info2
****************************************************************************/

static WERROR fill_job_info2(TALLOC_CTX *mem_ctx,
			     struct spoolss_JobInfo2 *r,
			     const print_queue_struct *queue,
			     int position, int snum,
			     struct spoolss_PrinterInfo2 *pinfo2,
			     struct spoolss_DeviceMode *devmode)
{
	struct tm *t;

	t = gmtime(&queue->time);

	r->job_id		= queue->sysjob;

	r->printer_name		= talloc_strdup(mem_ctx, lp_servicename(snum));
	W_ERROR_HAVE_NO_MEMORY(r->printer_name);
	r->server_name		= talloc_strdup(mem_ctx, pinfo2->servername);
	W_ERROR_HAVE_NO_MEMORY(r->server_name);
	r->user_name		= talloc_strdup(mem_ctx, queue->fs_user);
	W_ERROR_HAVE_NO_MEMORY(r->user_name);
	r->document_name	= talloc_strdup(mem_ctx, queue->fs_file);
	W_ERROR_HAVE_NO_MEMORY(r->document_name);
	r->notify_name		= talloc_strdup(mem_ctx, queue->fs_user);
	W_ERROR_HAVE_NO_MEMORY(r->notify_name);
	r->data_type		= talloc_strdup(mem_ctx, "RAW");
	W_ERROR_HAVE_NO_MEMORY(r->data_type);
	r->print_processor	= talloc_strdup(mem_ctx, "winprint");
	W_ERROR_HAVE_NO_MEMORY(r->print_processor);
	r->parameters		= talloc_strdup(mem_ctx, "");
	W_ERROR_HAVE_NO_MEMORY(r->parameters);
	r->driver_name		= talloc_strdup(mem_ctx, pinfo2->drivername);
	W_ERROR_HAVE_NO_MEMORY(r->driver_name);

	r->devmode		= devmode;

	r->text_status		= talloc_strdup(mem_ctx, "");
	W_ERROR_HAVE_NO_MEMORY(r->text_status);

	r->secdesc		= NULL;

	r->status		= nt_printj_status(queue->status);
	r->priority		= queue->priority;
	r->position		= position;
	r->start_time		= 0;
	r->until_time		= 0;
	r->total_pages		= queue->page_count;
	r->size			= queue->size;
	init_systemtime(&r->submitted, t);
	r->time			= 0;
	r->pages_printed	= 0; /* ??? */

	return WERR_OK;
}

/****************************************************************************
fill_job_info3
****************************************************************************/

static WERROR fill_job_info3(TALLOC_CTX *mem_ctx,
			     struct spoolss_JobInfo3 *r,
			     const print_queue_struct *queue,
			     const print_queue_struct *next_queue,
			     int position, int snum,
			     struct spoolss_PrinterInfo2 *pinfo2)
{
	r->job_id		= queue->sysjob;
	r->next_job_id		= 0;
	if (next_queue) {
		r->next_job_id	= next_queue->sysjob;
	}
	r->reserved		= 0;

	return WERR_OK;
}

/****************************************************************************
 Enumjobs at level 1.
****************************************************************************/

static WERROR enumjobs_level1(TALLOC_CTX *mem_ctx,
			      const print_queue_struct *queue,
			      uint32_t num_queues, int snum,
                              struct spoolss_PrinterInfo2 *pinfo2,
			      union spoolss_JobInfo **info_p,
			      uint32_t *count)
{
	union spoolss_JobInfo *info;
	int i;
	WERROR result = WERR_OK;

	info = TALLOC_ARRAY(mem_ctx, union spoolss_JobInfo, num_queues);
	W_ERROR_HAVE_NO_MEMORY(info);

	*count = num_queues;

	for (i=0; i<*count; i++) {
		result = fill_job_info1(info,
					&info[i].info1,
					&queue[i],
					i,
					snum,
					pinfo2);
		if (!W_ERROR_IS_OK(result)) {
			goto out;
		}
	}

 out:
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(info);
		*count = 0;
		return result;
	}

	*info_p = info;

	return WERR_OK;
}

/****************************************************************************
 Enumjobs at level 2.
****************************************************************************/

static WERROR enumjobs_level2(TALLOC_CTX *mem_ctx,
			      const print_queue_struct *queue,
			      uint32_t num_queues, int snum,
                              struct spoolss_PrinterInfo2 *pinfo2,
			      union spoolss_JobInfo **info_p,
			      uint32_t *count)
{
	union spoolss_JobInfo *info;
	int i;
	WERROR result = WERR_OK;

	info = TALLOC_ARRAY(mem_ctx, union spoolss_JobInfo, num_queues);
	W_ERROR_HAVE_NO_MEMORY(info);

	*count = num_queues;

	for (i=0; i<*count; i++) {
		struct spoolss_DeviceMode *devmode;

		result = spoolss_create_default_devmode(info,
							pinfo2->printername,
							&devmode);
		if (!W_ERROR_IS_OK(result)) {
			DEBUG(3, ("Can't proceed w/o a devmode!"));
			goto out;
		}

		result = fill_job_info2(info,
					&info[i].info2,
					&queue[i],
					i,
					snum,
					pinfo2,
					devmode);
		if (!W_ERROR_IS_OK(result)) {
			goto out;
		}
	}

 out:
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(info);
		*count = 0;
		return result;
	}

	*info_p = info;

	return WERR_OK;
}

/****************************************************************************
 Enumjobs at level 3.
****************************************************************************/

static WERROR enumjobs_level3(TALLOC_CTX *mem_ctx,
			      const print_queue_struct *queue,
			      uint32_t num_queues, int snum,
                              struct spoolss_PrinterInfo2 *pinfo2,
			      union spoolss_JobInfo **info_p,
			      uint32_t *count)
{
	union spoolss_JobInfo *info;
	int i;
	WERROR result = WERR_OK;

	info = TALLOC_ARRAY(mem_ctx, union spoolss_JobInfo, num_queues);
	W_ERROR_HAVE_NO_MEMORY(info);

	*count = num_queues;

	for (i=0; i<*count; i++) {
		const print_queue_struct *next_queue = NULL;

		if (i+1 < *count) {
			next_queue = &queue[i+1];
		}

		result = fill_job_info3(info,
					&info[i].info3,
					&queue[i],
					next_queue,
					i,
					snum,
					pinfo2);
		if (!W_ERROR_IS_OK(result)) {
			goto out;
		}
	}

 out:
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(info);
		*count = 0;
		return result;
	}

	*info_p = info;

	return WERR_OK;
}

/****************************************************************
 _spoolss_EnumJobs
****************************************************************/

WERROR _spoolss_EnumJobs(struct pipes_struct *p,
			 struct spoolss_EnumJobs *r)
{
	WERROR result;
	struct spoolss_PrinterInfo2 *pinfo2 = NULL;
	int snum;
	print_status_struct prt_status;
	print_queue_struct *queue = NULL;
	uint32_t count;

	/* that's an [in out] buffer */

	if (!r->in.buffer && (r->in.offered != 0)) {
		return WERR_INVALID_PARAM;
	}

	DEBUG(4,("_spoolss_EnumJobs\n"));

	*r->out.needed = 0;
	*r->out.count = 0;
	*r->out.info = NULL;

	/* lookup the printer snum and tdb entry */

	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}

	result = winreg_get_printer_internal(p->mem_ctx,
				    get_session_info_system(),
				    p->msg_ctx,
				    lp_const_servicename(snum),
				    &pinfo2);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	count = print_queue_status(p->msg_ctx, snum, &queue, &prt_status);
	DEBUGADD(4,("count:[%d], status:[%d], [%s]\n",
		count, prt_status.status, prt_status.message));

	if (count == 0) {
		SAFE_FREE(queue);
		TALLOC_FREE(pinfo2);
		return WERR_OK;
	}

	switch (r->in.level) {
	case 1:
		result = enumjobs_level1(p->mem_ctx, queue, count, snum,
					 pinfo2, r->out.info, r->out.count);
		break;
	case 2:
		result = enumjobs_level2(p->mem_ctx, queue, count, snum,
					 pinfo2, r->out.info, r->out.count);
		break;
	case 3:
		result = enumjobs_level3(p->mem_ctx, queue, count, snum,
					 pinfo2, r->out.info, r->out.count);
		break;
	default:
		result = WERR_UNKNOWN_LEVEL;
		break;
	}

	SAFE_FREE(queue);
	TALLOC_FREE(pinfo2);

	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(p->mem_ctx,
						     spoolss_EnumJobs,
						     *r->out.info, r->in.level,
						     *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************
 _spoolss_ScheduleJob
****************************************************************/

WERROR _spoolss_ScheduleJob(struct pipes_struct *p,
			    struct spoolss_ScheduleJob *r)
{
	return WERR_OK;
}

/****************************************************************
****************************************************************/

static WERROR spoolss_setjob_1(TALLOC_CTX *mem_ctx,
			       struct messaging_context *msg_ctx,
			       const char *printer_name,
			       uint32_t job_id,
			       struct spoolss_SetJobInfo1 *r)
{
	char *old_doc_name;

	if (!print_job_get_name(mem_ctx, printer_name, job_id, &old_doc_name)) {
		return WERR_BADFID;
	}

	if (strequal(old_doc_name, r->document_name)) {
		return WERR_OK;
	}

	if (!print_job_set_name(server_event_context(), msg_ctx,
				printer_name, job_id, r->document_name)) {
		return WERR_BADFID;
	}

	return WERR_OK;
}

/****************************************************************
 _spoolss_SetJob
****************************************************************/

WERROR _spoolss_SetJob(struct pipes_struct *p,
		       struct spoolss_SetJob *r)
{
	const struct auth_serversupplied_info *session_info = p->session_info;
	int snum;
	WERROR errcode = WERR_BADFUNC;

	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}

	if (!print_job_exists(lp_const_servicename(snum), r->in.job_id)) {
		return WERR_INVALID_PRINTER_NAME;
	}

	switch (r->in.command) {
	case SPOOLSS_JOB_CONTROL_CANCEL:
	case SPOOLSS_JOB_CONTROL_DELETE:
		errcode = print_job_delete(session_info, p->msg_ctx,
					   snum, r->in.job_id);
		if (W_ERROR_EQUAL(errcode, WERR_PRINTER_HAS_JOBS_QUEUED)) {
			errcode = WERR_OK;
		}
		break;
	case SPOOLSS_JOB_CONTROL_PAUSE:
		errcode = print_job_pause(session_info, p->msg_ctx,
					  snum, r->in.job_id);
		break;
	case SPOOLSS_JOB_CONTROL_RESTART:
	case SPOOLSS_JOB_CONTROL_RESUME:
		errcode = print_job_resume(session_info, p->msg_ctx,
					   snum, r->in.job_id);
		break;
	case 0:
		errcode = WERR_OK;
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	if (!W_ERROR_IS_OK(errcode)) {
		return errcode;
	}

	if (r->in.ctr == NULL) {
		return errcode;
	}

	switch (r->in.ctr->level) {
	case 1:
		errcode = spoolss_setjob_1(p->mem_ctx, p->msg_ctx,
					   lp_const_servicename(snum),
					   r->in.job_id,
					   r->in.ctr->info.info1);
		break;
	case 2:
	case 3:
	case 4:
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	return errcode;
}

/****************************************************************************
 Enumerates all printer drivers by level and architecture.
****************************************************************************/

static WERROR enumprinterdrivers_level_by_architecture(TALLOC_CTX *mem_ctx,
						       const struct auth_serversupplied_info *session_info,
						       struct messaging_context *msg_ctx,
						       const char *servername,
						       const char *architecture,
						       uint32_t level,
						       union spoolss_DriverInfo **info_p,
						       uint32_t *count_p)
{
	int i;
	uint32_t version;
	struct spoolss_DriverInfo8 *driver;
	union spoolss_DriverInfo *info = NULL;
	uint32_t count = 0;
	WERROR result = WERR_OK;
	uint32_t num_drivers;
	const char **drivers;
	struct dcerpc_binding_handle *b;

	*count_p = 0;
	*info_p = NULL;

	result = winreg_printer_binding_handle(mem_ctx,
					       session_info,
					       msg_ctx,
					       &b);
	if (!W_ERROR_IS_OK(result)) {
		goto out;
	}

	for (version=0; version<DRIVER_MAX_VERSION; version++) {
		result = winreg_get_driver_list(mem_ctx, b,
						architecture, version,
						&num_drivers, &drivers);
		if (!W_ERROR_IS_OK(result)) {
			goto out;
		}
		DEBUG(4, ("we have:[%d] drivers in environment"
			  " [%s] and version [%d]\n",
			  num_drivers, architecture, version));

		if (num_drivers != 0) {
			info = TALLOC_REALLOC_ARRAY(mem_ctx, info,
						    union spoolss_DriverInfo,
						    count + num_drivers);
			if (!info) {
				DEBUG(0,("enumprinterdrivers_level_by_architecture: "
					"failed to enlarge driver info buffer!\n"));
				result = WERR_NOMEM;
				goto out;
			}
		}

		for (i = 0; i < num_drivers; i++) {
			DEBUG(5, ("\tdriver: [%s]\n", drivers[i]));

			result = winreg_get_driver(mem_ctx, b,
						   architecture, drivers[i],
						   version, &driver);
			if (!W_ERROR_IS_OK(result)) {
				goto out;
			}

			switch (level) {
			case 1:
				result = fill_printer_driver_info1(info, &info[count+i].info1,
								   driver, servername);
				break;
			case 2:
				result = fill_printer_driver_info2(info, &info[count+i].info2,
								   driver, servername);
				break;
			case 3:
				result = fill_printer_driver_info3(info, &info[count+i].info3,
								   driver, servername);
				break;
			case 4:
				result = fill_printer_driver_info4(info, &info[count+i].info4,
								   driver, servername);
				break;
			case 5:
				result = fill_printer_driver_info5(info, &info[count+i].info5,
								   driver, servername);
				break;
			case 6:
				result = fill_printer_driver_info6(info, &info[count+i].info6,
								   driver, servername);
				break;
			case 8:
				result = fill_printer_driver_info8(info, &info[count+i].info8,
								   driver, servername);
				break;
			default:
				result = WERR_UNKNOWN_LEVEL;
				break;
			}

			TALLOC_FREE(driver);

			if (!W_ERROR_IS_OK(result)) {
				goto out;
			}
		}

		count += num_drivers;
		TALLOC_FREE(drivers);
	}

 out:
	TALLOC_FREE(drivers);

	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(info);
		return result;
	}

	*info_p = info;
	*count_p = count;

	return WERR_OK;
}

/****************************************************************************
 Enumerates all printer drivers by level.
****************************************************************************/

static WERROR enumprinterdrivers_level(TALLOC_CTX *mem_ctx,
				       const struct auth_serversupplied_info *session_info,
				       struct messaging_context *msg_ctx,
				       const char *servername,
				       const char *architecture,
				       uint32_t level,
				       union spoolss_DriverInfo **info_p,
				       uint32_t *count_p)
{
	uint32_t a,i;
	WERROR result = WERR_OK;

	if (strequal(architecture, SPOOLSS_ARCHITECTURE_ALL)) {

		for (a=0; archi_table[a].long_archi != NULL; a++) {

			union spoolss_DriverInfo *info = NULL;
			uint32_t count = 0;

			result = enumprinterdrivers_level_by_architecture(mem_ctx,
									  session_info,
									  msg_ctx,
									  servername,
									  archi_table[a].long_archi,
									  level,
									  &info,
									  &count);
			if (!W_ERROR_IS_OK(result)) {
				continue;
			}

			for (i=0; i < count; i++) {
				ADD_TO_ARRAY(mem_ctx, union spoolss_DriverInfo,
					     info[i], info_p, count_p);
			}
		}

		return result;
	}

	return enumprinterdrivers_level_by_architecture(mem_ctx,
							session_info,
							msg_ctx,
							servername,
							architecture,
							level,
							info_p,
							count_p);
}

/****************************************************************
 _spoolss_EnumPrinterDrivers
****************************************************************/

WERROR _spoolss_EnumPrinterDrivers(struct pipes_struct *p,
				   struct spoolss_EnumPrinterDrivers *r)
{
	const char *cservername;
	WERROR result;

	/* that's an [in out] buffer */

	if (!r->in.buffer && (r->in.offered != 0)) {
		return WERR_INVALID_PARAM;
	}

	DEBUG(4,("_spoolss_EnumPrinterDrivers\n"));

	*r->out.needed = 0;
	*r->out.count = 0;
	*r->out.info = NULL;

	cservername = canon_servername(r->in.server);

	if (!is_myname_or_ipaddr(cservername)) {
		return WERR_UNKNOWN_PRINTER_DRIVER;
	}

	result = enumprinterdrivers_level(p->mem_ctx,
					  get_session_info_system(),
					  p->msg_ctx,
					  cservername,
					  r->in.environment,
					  r->in.level,
					  r->out.info,
					  r->out.count);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(p->mem_ctx,
						     spoolss_EnumPrinterDrivers,
						     *r->out.info, r->in.level,
						     *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************
 _spoolss_EnumForms
****************************************************************/

WERROR _spoolss_EnumForms(struct pipes_struct *p,
			  struct spoolss_EnumForms *r)
{
	WERROR result;

	*r->out.count = 0;
	*r->out.needed = 0;
	*r->out.info = NULL;

	/* that's an [in out] buffer */

	if (!r->in.buffer && (r->in.offered != 0) ) {
		return WERR_INVALID_PARAM;
	}

	DEBUG(4,("_spoolss_EnumForms\n"));
	DEBUGADD(5,("Offered buffer size [%d]\n", r->in.offered));
	DEBUGADD(5,("Info level [%d]\n",          r->in.level));

	switch (r->in.level) {
	case 1:
		result = winreg_printer_enumforms1_internal(p->mem_ctx,
						   get_session_info_system(),
						   p->msg_ctx,
						   r->out.count,
						   r->out.info);
		break;
	default:
		result = WERR_UNKNOWN_LEVEL;
		break;
	}

	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	if (*r->out.count == 0) {
		return WERR_NO_MORE_ITEMS;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(p->mem_ctx,
						     spoolss_EnumForms,
						     *r->out.info, r->in.level,
						     *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************
 _spoolss_GetForm
****************************************************************/

WERROR _spoolss_GetForm(struct pipes_struct *p,
			struct spoolss_GetForm *r)
{
	WERROR result;

	/* that's an [in out] buffer */

	if (!r->in.buffer && (r->in.offered != 0)) {
		return WERR_INVALID_PARAM;
	}

	DEBUG(4,("_spoolss_GetForm\n"));
	DEBUGADD(5,("Offered buffer size [%d]\n", r->in.offered));
	DEBUGADD(5,("Info level [%d]\n",          r->in.level));

	switch (r->in.level) {
	case 1:
		result = winreg_printer_getform1_internal(p->mem_ctx,
						 get_session_info_system(),
						 p->msg_ctx,
						 r->in.form_name,
						 &r->out.info->info1);
		break;
	default:
		result = WERR_UNKNOWN_LEVEL;
		break;
	}

	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(r->out.info);
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION(spoolss_FormInfo,
					       r->out.info, r->in.level);
	r->out.info	= SPOOLSS_BUFFER_OK(r->out.info, NULL);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************************
****************************************************************************/

static WERROR fill_port_1(TALLOC_CTX *mem_ctx,
			  struct spoolss_PortInfo1 *r,
			  const char *name)
{
	r->port_name = talloc_strdup(mem_ctx, name);
	W_ERROR_HAVE_NO_MEMORY(r->port_name);

	return WERR_OK;
}

/****************************************************************************
 TODO: This probably needs distinguish between TCP/IP and Local ports
 somehow.
****************************************************************************/

static WERROR fill_port_2(TALLOC_CTX *mem_ctx,
			  struct spoolss_PortInfo2 *r,
			  const char *name)
{
	r->port_name = talloc_strdup(mem_ctx, name);
	W_ERROR_HAVE_NO_MEMORY(r->port_name);

	r->monitor_name = talloc_strdup(mem_ctx, "Local Monitor");
	W_ERROR_HAVE_NO_MEMORY(r->monitor_name);

	r->description = talloc_strdup(mem_ctx, SPL_LOCAL_PORT);
	W_ERROR_HAVE_NO_MEMORY(r->description);

	r->port_type = SPOOLSS_PORT_TYPE_WRITE;
	r->reserved = 0;

	return WERR_OK;
}


/****************************************************************************
 wrapper around the enumer ports command
****************************************************************************/

static WERROR enumports_hook(TALLOC_CTX *ctx, int *count, char ***lines)
{
	char *cmd = lp_enumports_cmd();
	char **qlines = NULL;
	char *command = NULL;
	int numlines;
	int ret;
	int fd;

	*count = 0;
	*lines = NULL;

	/* if no hook then just fill in the default port */

	if ( !*cmd ) {
		if (!(qlines = TALLOC_ARRAY( NULL, char*, 2 ))) {
			return WERR_NOMEM;
		}
		if (!(qlines[0] = talloc_strdup(qlines, SAMBA_PRINTER_PORT_NAME ))) {
			TALLOC_FREE(qlines);
			return WERR_NOMEM;
		}
		qlines[1] = NULL;
		numlines = 1;
	}
	else {
		/* we have a valid enumport command */

		command = talloc_asprintf(ctx, "%s \"%d\"", cmd, 1);
		if (!command) {
			return WERR_NOMEM;
		}

		DEBUG(10,("Running [%s]\n", command));
		ret = smbrun(command, &fd);
		DEBUG(10,("Returned [%d]\n", ret));
		TALLOC_FREE(command);
		if (ret != 0) {
			if (fd != -1) {
				close(fd);
			}
			return WERR_ACCESS_DENIED;
		}

		numlines = 0;
		qlines = fd_lines_load(fd, &numlines, 0, NULL);
		DEBUGADD(10,("Lines returned = [%d]\n", numlines));
		close(fd);
	}

	*count = numlines;
	*lines = qlines;

	return WERR_OK;
}

/****************************************************************************
 enumports level 1.
****************************************************************************/

static WERROR enumports_level_1(TALLOC_CTX *mem_ctx,
				union spoolss_PortInfo **info_p,
				uint32_t *count)
{
	union spoolss_PortInfo *info = NULL;
	int i=0;
	WERROR result = WERR_OK;
	char **qlines = NULL;
	int numlines = 0;

	result = enumports_hook(talloc_tos(), &numlines, &qlines );
	if (!W_ERROR_IS_OK(result)) {
		goto out;
	}

	if (numlines) {
		info = TALLOC_ARRAY(mem_ctx, union spoolss_PortInfo, numlines);
		if (!info) {
			DEBUG(10,("Returning WERR_NOMEM\n"));
			result = WERR_NOMEM;
			goto out;
		}

		for (i=0; i<numlines; i++) {
			DEBUG(6,("Filling port number [%d] with port [%s]\n", i, qlines[i]));
			result = fill_port_1(info, &info[i].info1, qlines[i]);
			if (!W_ERROR_IS_OK(result)) {
				goto out;
			}
		}
	}
	TALLOC_FREE(qlines);

out:
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(info);
		TALLOC_FREE(qlines);
		*count = 0;
		*info_p = NULL;
		return result;
	}

	*info_p = info;
	*count = numlines;

	return WERR_OK;
}

/****************************************************************************
 enumports level 2.
****************************************************************************/

static WERROR enumports_level_2(TALLOC_CTX *mem_ctx,
				union spoolss_PortInfo **info_p,
				uint32_t *count)
{
	union spoolss_PortInfo *info = NULL;
	int i=0;
	WERROR result = WERR_OK;
	char **qlines = NULL;
	int numlines = 0;

	result = enumports_hook(talloc_tos(), &numlines, &qlines );
	if (!W_ERROR_IS_OK(result)) {
		goto out;
	}

	if (numlines) {
		info = TALLOC_ARRAY(mem_ctx, union spoolss_PortInfo, numlines);
		if (!info) {
			DEBUG(10,("Returning WERR_NOMEM\n"));
			result = WERR_NOMEM;
			goto out;
		}

		for (i=0; i<numlines; i++) {
			DEBUG(6,("Filling port number [%d] with port [%s]\n", i, qlines[i]));
			result = fill_port_2(info, &info[i].info2, qlines[i]);
			if (!W_ERROR_IS_OK(result)) {
				goto out;
			}
		}
	}
	TALLOC_FREE(qlines);

out:
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(info);
		TALLOC_FREE(qlines);
		*count = 0;
		*info_p = NULL;
		return result;
	}

	*info_p = info;
	*count = numlines;

	return WERR_OK;
}

/****************************************************************
 _spoolss_EnumPorts
****************************************************************/

WERROR _spoolss_EnumPorts(struct pipes_struct *p,
			  struct spoolss_EnumPorts *r)
{
	WERROR result;

	/* that's an [in out] buffer */

	if (!r->in.buffer && (r->in.offered != 0)) {
		return WERR_INVALID_PARAM;
	}

	DEBUG(4,("_spoolss_EnumPorts\n"));

	*r->out.count = 0;
	*r->out.needed = 0;
	*r->out.info = NULL;

	switch (r->in.level) {
	case 1:
		result = enumports_level_1(p->mem_ctx, r->out.info,
					   r->out.count);
		break;
	case 2:
		result = enumports_level_2(p->mem_ctx, r->out.info,
					   r->out.count);
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(p->mem_ctx,
						     spoolss_EnumPorts,
						     *r->out.info, r->in.level,
						     *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************************
****************************************************************************/

static WERROR spoolss_addprinterex_level_2(struct pipes_struct *p,
					   const char *server,
					   struct spoolss_SetPrinterInfoCtr *info_ctr,
					   struct spoolss_DeviceMode *devmode,
					   struct security_descriptor *secdesc,
					   struct spoolss_UserLevelCtr *user_ctr,
					   struct policy_handle *handle)
{
	struct spoolss_SetPrinterInfo2 *info2 = info_ctr->info.info2;
	uint32_t info2_mask = SPOOLSS_PRINTER_INFO_ALL;
	int	snum;
	WERROR err = WERR_OK;

	/* samba does not have a concept of local, non-shared printers yet, so
	 * make sure we always setup sharename - gd */
	if ((info2->sharename == NULL || info2->sharename[0] == '\0') &&
	    (info2->printername != NULL && info2->printername[0] != '\0')) {
		DEBUG(5, ("spoolss_addprinterex_level_2: "
			"no sharename has been set, setting printername %s as sharename\n",
			info2->printername));
		info2->sharename = info2->printername;
	}

	/* check to see if the printer already exists */
	if ((snum = print_queue_snum(info2->sharename)) != -1) {
		DEBUG(5, ("spoolss_addprinterex_level_2: Attempted to add a printer named [%s] when one already existed!\n",
			info2->sharename));
		return WERR_PRINTER_ALREADY_EXISTS;
	}

	if (!lp_force_printername(GLOBAL_SECTION_SNUM)) {
		if ((snum = print_queue_snum(info2->printername)) != -1) {
			DEBUG(5, ("spoolss_addprinterex_level_2: Attempted to add a printer named [%s] when one already existed!\n",
				info2->printername));
			return WERR_PRINTER_ALREADY_EXISTS;
		}
	}

	/* validate printer info struct */
	if (!info2->printername || strlen(info2->printername) == 0) {
		return WERR_INVALID_PRINTER_NAME;
	}
	if (!info2->portname || strlen(info2->portname) == 0) {
		return WERR_UNKNOWN_PORT;
	}
	if (!info2->drivername || strlen(info2->drivername) == 0) {
		return WERR_UNKNOWN_PRINTER_DRIVER;
	}
	if (!info2->printprocessor || strlen(info2->printprocessor) == 0) {
		return WERR_UNKNOWN_PRINTPROCESSOR;
	}

	/* FIXME!!!  smbd should check to see if the driver is installed before
	   trying to add a printer like this  --jerry */

	if (*lp_addprinter_cmd() ) {
		if ( !add_printer_hook(p->mem_ctx, p->session_info->security_token,
				       info2, p->client_id->addr,
				       p->msg_ctx) ) {
			return WERR_ACCESS_DENIED;
		}
	} else {
		DEBUG(0,("spoolss_addprinterex_level_2: add printer for printer %s called and no "
			"smb.conf parameter \"addprinter command\" is defined. This "
			"parameter must exist for this call to succeed\n",
			info2->sharename ));
	}

	if ((snum = print_queue_snum(info2->sharename)) == -1) {
		return WERR_ACCESS_DENIED;
	}

	/* you must be a printer admin to add a new printer */
	if (!print_access_check(p->session_info,
				p->msg_ctx,
				snum,
				PRINTER_ACCESS_ADMINISTER)) {
		return WERR_ACCESS_DENIED;
	}

	/*
	 * Do sanity check on the requested changes for Samba.
	 */

	if (!check_printer_ok(p->mem_ctx, info2, snum)) {
		return WERR_INVALID_PARAM;
	}

	if (devmode == NULL) {
		info2_mask = ~SPOOLSS_PRINTER_INFO_DEVMODE;
	}

	update_dsspooler(p->mem_ctx,
			 get_session_info_system(),
			 p->msg_ctx,
			 0,
			 info2,
			 NULL);

	err = winreg_update_printer_internal(p->mem_ctx,
				    get_session_info_system(),
				    p->msg_ctx,
				    info2->sharename,
				    info2_mask,
				    info2,
				    devmode,
				    secdesc);
	if (!W_ERROR_IS_OK(err)) {
		return err;
	}

	err = open_printer_hnd(p, handle, info2->printername, PRINTER_ACCESS_ADMINISTER);
	if (!W_ERROR_IS_OK(err)) {
		/* Handle open failed - remove addition. */
		ZERO_STRUCTP(handle);
		return err;
	}

	return WERR_OK;
}

/****************************************************************
 _spoolss_AddPrinterEx
****************************************************************/

WERROR _spoolss_AddPrinterEx(struct pipes_struct *p,
			     struct spoolss_AddPrinterEx *r)
{
	switch (r->in.info_ctr->level) {
	case 1:
		/* we don't handle yet */
		/* but I know what to do ... */
		return WERR_UNKNOWN_LEVEL;
	case 2:
		return spoolss_addprinterex_level_2(p, r->in.server,
						    r->in.info_ctr,
						    r->in.devmode_ctr->devmode,
						    r->in.secdesc_ctr->sd,
						    r->in.userlevel_ctr,
						    r->out.handle);
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}

/****************************************************************
 _spoolss_AddPrinter
****************************************************************/

WERROR _spoolss_AddPrinter(struct pipes_struct *p,
			   struct spoolss_AddPrinter *r)
{
	struct spoolss_AddPrinterEx a;
	struct spoolss_UserLevelCtr userlevel_ctr;

	ZERO_STRUCT(userlevel_ctr);

	userlevel_ctr.level = 1;

	a.in.server		= r->in.server;
	a.in.info_ctr		= r->in.info_ctr;
	a.in.devmode_ctr	= r->in.devmode_ctr;
	a.in.secdesc_ctr	= r->in.secdesc_ctr;
	a.in.userlevel_ctr	= &userlevel_ctr;
	a.out.handle		= r->out.handle;

	return _spoolss_AddPrinterEx(p, &a);
}

/****************************************************************
 _spoolss_AddPrinterDriverEx
****************************************************************/

WERROR _spoolss_AddPrinterDriverEx(struct pipes_struct *p,
				   struct spoolss_AddPrinterDriverEx *r)
{
	WERROR err = WERR_OK;
	const char *driver_name = NULL;
	uint32_t version;
	const char *fn;

	switch (p->opnum) {
		case NDR_SPOOLSS_ADDPRINTERDRIVER:
			fn = "_spoolss_AddPrinterDriver";
			break;
		case NDR_SPOOLSS_ADDPRINTERDRIVEREX:
			fn = "_spoolss_AddPrinterDriverEx";
			break;
		default:
			return WERR_INVALID_PARAM;
	}

	/*
	 * we only support the semantics of AddPrinterDriver()
	 * i.e. only copy files that are newer than existing ones
	 */

	if (r->in.flags == 0) {
		return WERR_INVALID_PARAM;
	}

	if (r->in.flags != APD_COPY_NEW_FILES) {
		return WERR_ACCESS_DENIED;
	}

	/* FIXME */
	if (r->in.info_ctr->level != 3 && r->in.info_ctr->level != 6) {
		/* Clever hack from Martin Zielinski <mz@seh.de>
		 * to allow downgrade from level 8 (Vista).
		 */
		DEBUG(0,("%s: level %d not yet implemented\n", fn,
			r->in.info_ctr->level));
		return WERR_UNKNOWN_LEVEL;
	}

	DEBUG(5,("Cleaning driver's information\n"));
	err = clean_up_driver_struct(p->mem_ctx, p->session_info, r->in.info_ctr);
	if (!W_ERROR_IS_OK(err))
		goto done;

	DEBUG(5,("Moving driver to final destination\n"));
	err = move_driver_to_download_area(p->session_info, r->in.info_ctr);
	if (!W_ERROR_IS_OK(err)) {
		goto done;
	}

	err = winreg_add_driver_internal(p->mem_ctx,
				get_session_info_system(),
				p->msg_ctx,
				r->in.info_ctr,
				&driver_name,
				&version);
	if (!W_ERROR_IS_OK(err)) {
		goto done;
	}

	/*
	 * I think this is where he DrvUpgradePrinter() hook would be
	 * be called in a driver's interface DLL on a Windows NT 4.0/2k
	 * server.  Right now, we just need to send ourselves a message
	 * to update each printer bound to this driver.   --jerry
	 */

	if (!srv_spoolss_drv_upgrade_printer(driver_name, p->msg_ctx)) {
		DEBUG(0,("%s: Failed to send message about upgrading driver [%s]!\n",
			fn, driver_name));
	}

done:
	return err;
}

/****************************************************************
 _spoolss_AddPrinterDriver
****************************************************************/

WERROR _spoolss_AddPrinterDriver(struct pipes_struct *p,
				 struct spoolss_AddPrinterDriver *r)
{
	struct spoolss_AddPrinterDriverEx a;

	switch (r->in.info_ctr->level) {
	case 2:
	case 3:
	case 4:
	case 5:
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	a.in.servername		= r->in.servername;
	a.in.info_ctr		= r->in.info_ctr;
	a.in.flags		= APD_COPY_NEW_FILES;

	return _spoolss_AddPrinterDriverEx(p, &a);
}

/****************************************************************************
****************************************************************************/

struct _spoolss_paths {
	int type;
	const char *share;
	const char *dir;
};

enum { SPOOLSS_DRIVER_PATH, SPOOLSS_PRTPROCS_PATH };

static const struct _spoolss_paths spoolss_paths[]= {
	{ SPOOLSS_DRIVER_PATH,		"print$",	"DRIVERS" },
	{ SPOOLSS_PRTPROCS_PATH,	"prnproc$",	"PRTPROCS" }
};

static WERROR compose_spoolss_server_path(TALLOC_CTX *mem_ctx,
					  const char *servername,
					  const char *environment,
					  int component,
					  char **path)
{
	const char *pservername = NULL;
	const char *long_archi = SPOOLSS_ARCHITECTURE_NT_X86;
	const char *short_archi;

	*path = NULL;

	/* environment may be empty */
	if (environment && strlen(environment)) {
		long_archi = environment;
	}

	/* servername may be empty */
	if (servername && strlen(servername)) {
		pservername = canon_servername(servername);

		if (!is_myname_or_ipaddr(pservername)) {
			return WERR_INVALID_PARAM;
		}
	}

	if (!(short_archi = get_short_archi(long_archi))) {
		return WERR_INVALID_ENVIRONMENT;
	}

	switch (component) {
	case SPOOLSS_PRTPROCS_PATH:
	case SPOOLSS_DRIVER_PATH:
		if (pservername) {
			*path = talloc_asprintf(mem_ctx,
					"\\\\%s\\%s\\%s",
					pservername,
					spoolss_paths[component].share,
					short_archi);
		} else {
			*path = talloc_asprintf(mem_ctx, "%s\\%s\\%s",
					SPOOLSS_DEFAULT_SERVER_PATH,
					spoolss_paths[component].dir,
					short_archi);
		}
		break;
	default:
		return WERR_INVALID_PARAM;
	}

	if (!*path) {
		return WERR_NOMEM;
	}

	return WERR_OK;
}

/****************************************************************************
****************************************************************************/

static WERROR getprinterdriverdir_level_1(TALLOC_CTX *mem_ctx,
					  const char *servername,
					  const char *environment,
					  struct spoolss_DriverDirectoryInfo1 *r)
{
	WERROR werr;
	char *path = NULL;

	werr = compose_spoolss_server_path(mem_ctx,
					   servername,
					   environment,
					   SPOOLSS_DRIVER_PATH,
					   &path);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	DEBUG(4,("printer driver directory: [%s]\n", path));

	r->directory_name = path;

	return WERR_OK;
}

/****************************************************************
 _spoolss_GetPrinterDriverDirectory
****************************************************************/

WERROR _spoolss_GetPrinterDriverDirectory(struct pipes_struct *p,
					  struct spoolss_GetPrinterDriverDirectory *r)
{
	WERROR werror;

	/* that's an [in out] buffer */

	if (!r->in.buffer && (r->in.offered != 0)) {
		return WERR_INVALID_PARAM;
	}

	DEBUG(5,("_spoolss_GetPrinterDriverDirectory: level %d\n",
		r->in.level));

	*r->out.needed = 0;

	/* r->in.level is ignored */

	werror = getprinterdriverdir_level_1(p->mem_ctx,
					     r->in.server,
					     r->in.environment,
					     &r->out.info->info1);
	if (!W_ERROR_IS_OK(werror)) {
		TALLOC_FREE(r->out.info);
		return werror;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION(spoolss_DriverDirectoryInfo,
					       r->out.info, r->in.level);
	r->out.info	= SPOOLSS_BUFFER_OK(r->out.info, NULL);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************
 _spoolss_EnumPrinterData
****************************************************************/

WERROR _spoolss_EnumPrinterData(struct pipes_struct *p,
				struct spoolss_EnumPrinterData *r)
{
	WERROR result;
	struct spoolss_EnumPrinterDataEx r2;
	uint32_t count;
	struct spoolss_PrinterEnumValues *info, *val = NULL;
	uint32_t needed;

	r2.in.handle	= r->in.handle;
	r2.in.key_name	= "PrinterDriverData";
	r2.in.offered	= 0;
	r2.out.count	= &count;
	r2.out.info	= &info;
	r2.out.needed	= &needed;

	result = _spoolss_EnumPrinterDataEx(p, &r2);
	if (W_ERROR_EQUAL(result, WERR_MORE_DATA)) {
		r2.in.offered = needed;
		result = _spoolss_EnumPrinterDataEx(p, &r2);
	}
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	/*
	 * The NT machine wants to know the biggest size of value and data
	 *
	 * cf: MSDN EnumPrinterData remark section
	 */

	if (!r->in.value_offered && !r->in.data_offered) {
		uint32_t biggest_valuesize = 0;
		uint32_t biggest_datasize = 0;
		int i, name_length;

		DEBUGADD(6,("Activating NT mega-hack to find sizes\n"));

		for (i=0; i<count; i++) {

			name_length = strlen(info[i].value_name);
			if (strlen(info[i].value_name) > biggest_valuesize) {
				biggest_valuesize = name_length;
			}

			if (info[i].data_length > biggest_datasize) {
				biggest_datasize = info[i].data_length;
			}

			DEBUG(6,("current values: [%d], [%d]\n", biggest_valuesize,
				biggest_datasize));
		}

		/* the value is an UNICODE string but real_value_size is the length
		   in bytes including the trailing 0 */

		*r->out.value_needed = 2 * (1 + biggest_valuesize);
		*r->out.data_needed  = biggest_datasize;

		DEBUG(6,("final values: [%d], [%d]\n",
			*r->out.value_needed, *r->out.data_needed));

		return WERR_OK;
	}

	if (r->in.enum_index < count) {
		val = &info[r->in.enum_index];
	}

	if (val == NULL) {
		/* out_value should default to "" or else NT4 has
		   problems unmarshalling the response */

		if (r->in.value_offered) {
			*r->out.value_needed = 1;
			r->out.value_name = talloc_strdup(r, "");
			if (!r->out.value_name) {
				return WERR_NOMEM;
			}
		} else {
			r->out.value_name = NULL;
			*r->out.value_needed = 0;
		}

		/* the data is counted in bytes */

		*r->out.data_needed = r->in.data_offered;

		result = WERR_NO_MORE_ITEMS;
	} else {
		/*
		 * the value is:
		 * - counted in bytes in the request
		 * - counted in UNICODE chars in the max reply
		 * - counted in bytes in the real size
		 *
		 * take a pause *before* coding not *during* coding
		 */

		/* name */
		if (r->in.value_offered) {
			r->out.value_name = talloc_strdup(r, val->value_name);
			if (!r->out.value_name) {
				return WERR_NOMEM;
			}
			*r->out.value_needed = val->value_name_len;
		} else {
			r->out.value_name = NULL;
			*r->out.value_needed = 0;
		}

		/* type */

		*r->out.type = val->type;

		/* data - counted in bytes */

		/*
		 * See the section "Dynamically Typed Query Parameters"
		 * in MS-RPRN.
		 */

		if (r->out.data && val->data && val->data->data &&
				val->data_length && r->in.data_offered) {
			memcpy(r->out.data, val->data->data,
				MIN(val->data_length,r->in.data_offered));
		}

		*r->out.data_needed = val->data_length;

		result = WERR_OK;
	}

	return result;
}

/****************************************************************
 _spoolss_SetPrinterData
****************************************************************/

WERROR _spoolss_SetPrinterData(struct pipes_struct *p,
			       struct spoolss_SetPrinterData *r)
{
	struct spoolss_SetPrinterDataEx r2;

	r2.in.handle		= r->in.handle;
	r2.in.key_name		= "PrinterDriverData";
	r2.in.value_name	= r->in.value_name;
	r2.in.type		= r->in.type;
	r2.in.data		= r->in.data;
	r2.in.offered		= r->in.offered;

	return _spoolss_SetPrinterDataEx(p, &r2);
}

/****************************************************************
 _spoolss_ResetPrinter
****************************************************************/

WERROR _spoolss_ResetPrinter(struct pipes_struct *p,
			     struct spoolss_ResetPrinter *r)
{
	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);
	int 		snum;

	DEBUG(5,("_spoolss_ResetPrinter\n"));

	/*
	 * All we do is to check to see if the handle and queue is valid.
	 * This call really doesn't mean anything to us because we only
	 * support RAW printing.   --jerry
	 */

	if (!Printer) {
		DEBUG(2,("_spoolss_ResetPrinter: Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	if (!get_printer_snum(p, r->in.handle, &snum, NULL))
		return WERR_BADFID;


	/* blindly return success */
	return WERR_OK;
}

/****************************************************************
 _spoolss_DeletePrinterData
****************************************************************/

WERROR _spoolss_DeletePrinterData(struct pipes_struct *p,
				  struct spoolss_DeletePrinterData *r)
{
	struct spoolss_DeletePrinterDataEx r2;

	r2.in.handle		= r->in.handle;
	r2.in.key_name		= "PrinterDriverData";
	r2.in.value_name	= r->in.value_name;

	return _spoolss_DeletePrinterDataEx(p, &r2);
}

/****************************************************************
 _spoolss_AddForm
****************************************************************/

WERROR _spoolss_AddForm(struct pipes_struct *p,
			struct spoolss_AddForm *r)
{
	struct spoolss_AddFormInfo1 *form = r->in.info.info1;
	int snum = -1;
	WERROR status = WERR_OK;
	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);
	struct dcerpc_binding_handle *b;

	DEBUG(5,("_spoolss_AddForm\n"));

	if (!Printer) {
		DEBUG(2,("_spoolss_AddForm: Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	/* if the user is not root, doesn't have SE_PRINT_OPERATOR privilege,
	   and not a printer admin, then fail */

	if ((p->session_info->utok.uid != sec_initial_uid()) &&
	    !security_token_has_privilege(p->session_info->security_token, SEC_PRIV_PRINT_OPERATOR) &&
	    !token_contains_name_in_list(uidtoname(p->session_info->utok.uid),
					  p->session_info->info3->base.domain.string,
					  NULL,
					  p->session_info->security_token,
					  lp_printer_admin(snum))) {
		DEBUG(2,("_spoolss_Addform: denied by insufficient permissions.\n"));
		return WERR_ACCESS_DENIED;
	}

	switch (form->flags) {
	case SPOOLSS_FORM_USER:
	case SPOOLSS_FORM_BUILTIN:
	case SPOOLSS_FORM_PRINTER:
		break;
	default:
		return WERR_INVALID_PARAM;
	}

	status = winreg_printer_binding_handle(p->mem_ctx,
					       get_session_info_system(),
					       p->msg_ctx,
					       &b);
	if (!W_ERROR_IS_OK(status)) {
		return status;
	}

	status = winreg_printer_addform1(p->mem_ctx, b,
					 form);
	if (!W_ERROR_IS_OK(status)) {
		return status;
	}

	/*
	 * ChangeID must always be set if this is a printer
	 */
	if (Printer->printer_type == SPLHND_PRINTER) {
		if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
			return WERR_BADFID;
		}

		status = winreg_printer_update_changeid(p->mem_ctx, b,
							lp_const_servicename(snum));
		if (!W_ERROR_IS_OK(status)) {
			return status;
		}
	}

	return status;
}

/****************************************************************
 _spoolss_DeleteForm
****************************************************************/

WERROR _spoolss_DeleteForm(struct pipes_struct *p,
			   struct spoolss_DeleteForm *r)
{
	const char *form_name = r->in.form_name;
	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);
	int snum = -1;
	WERROR status = WERR_OK;
	struct dcerpc_binding_handle *b;

	DEBUG(5,("_spoolss_DeleteForm\n"));

	if (!Printer) {
		DEBUG(2,("_spoolss_DeleteForm: Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	if ((p->session_info->utok.uid != sec_initial_uid()) &&
	    !security_token_has_privilege(p->session_info->security_token, SEC_PRIV_PRINT_OPERATOR) &&
	    !token_contains_name_in_list(uidtoname(p->session_info->utok.uid),
					  p->session_info->info3->base.domain.string,
					  NULL,
					  p->session_info->security_token,
					  lp_printer_admin(snum))) {
		DEBUG(2,("_spoolss_DeleteForm: denied by insufficient permissions.\n"));
		return WERR_ACCESS_DENIED;
	}

	status = winreg_printer_binding_handle(p->mem_ctx,
					       get_session_info_system(),
					       p->msg_ctx,
					       &b);
	if (!W_ERROR_IS_OK(status)) {
		return status;
	}

	status = winreg_printer_deleteform1(p->mem_ctx, b,
					    form_name);
	if (!W_ERROR_IS_OK(status)) {
		return status;
	}

	/*
	 * ChangeID must always be set if this is a printer
	 */
	if (Printer->printer_type == SPLHND_PRINTER) {
		if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
			return WERR_BADFID;
		}

		status = winreg_printer_update_changeid(p->mem_ctx, b,
							lp_const_servicename(snum));
		if (!W_ERROR_IS_OK(status)) {
			return status;
		}
	}

	return status;
}

/****************************************************************
 _spoolss_SetForm
****************************************************************/

WERROR _spoolss_SetForm(struct pipes_struct *p,
			struct spoolss_SetForm *r)
{
	struct spoolss_AddFormInfo1 *form = r->in.info.info1;
	const char *form_name = r->in.form_name;
	int snum = -1;
	WERROR status = WERR_OK;
	struct dcerpc_binding_handle *b;

	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);

	DEBUG(5,("_spoolss_SetForm\n"));

	if (!Printer) {
		DEBUG(2,("_spoolss_SetForm: Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	/* if the user is not root, doesn't have SE_PRINT_OPERATOR privilege,
	   and not a printer admin, then fail */

	if ((p->session_info->utok.uid != sec_initial_uid()) &&
	     !security_token_has_privilege(p->session_info->security_token, SEC_PRIV_PRINT_OPERATOR) &&
	     !token_contains_name_in_list(uidtoname(p->session_info->utok.uid),
					  p->session_info->info3->base.domain.string,
					  NULL,
					  p->session_info->security_token,
					  lp_printer_admin(snum))) {
		DEBUG(2,("_spoolss_Setform: denied by insufficient permissions.\n"));
		return WERR_ACCESS_DENIED;
	}

	status = winreg_printer_binding_handle(p->mem_ctx,
					       get_session_info_system(),
					       p->msg_ctx,
					       &b);
	if (!W_ERROR_IS_OK(status)) {
		return status;
	}

	status = winreg_printer_setform1(p->mem_ctx, b,
					 form_name,
					 form);
	if (!W_ERROR_IS_OK(status)) {
		return status;
	}

	/*
	 * ChangeID must always be set if this is a printer
	 */
	if (Printer->printer_type == SPLHND_PRINTER) {
		if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
			return WERR_BADFID;
		}

		status = winreg_printer_update_changeid(p->mem_ctx, b,
							lp_const_servicename(snum));
		if (!W_ERROR_IS_OK(status)) {
			return status;
		}
	}

	return status;
}

/****************************************************************************
 fill_print_processor1
****************************************************************************/

static WERROR fill_print_processor1(TALLOC_CTX *mem_ctx,
				    struct spoolss_PrintProcessorInfo1 *r,
				    const char *print_processor_name)
{
	r->print_processor_name = talloc_strdup(mem_ctx, print_processor_name);
	W_ERROR_HAVE_NO_MEMORY(r->print_processor_name);

	return WERR_OK;
}

/****************************************************************************
 enumprintprocessors level 1.
****************************************************************************/

static WERROR enumprintprocessors_level_1(TALLOC_CTX *mem_ctx,
					  union spoolss_PrintProcessorInfo **info_p,
					  uint32_t *count)
{
	union spoolss_PrintProcessorInfo *info;
	WERROR result;

	info = TALLOC_ARRAY(mem_ctx, union spoolss_PrintProcessorInfo, 1);
	W_ERROR_HAVE_NO_MEMORY(info);

	*count = 1;

	result = fill_print_processor1(info, &info[0].info1, "winprint");
	if (!W_ERROR_IS_OK(result)) {
		goto out;
	}

 out:
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(info);
		*count = 0;
		return result;
	}

	*info_p = info;

	return WERR_OK;
}

/****************************************************************
 _spoolss_EnumPrintProcessors
****************************************************************/

WERROR _spoolss_EnumPrintProcessors(struct pipes_struct *p,
				    struct spoolss_EnumPrintProcessors *r)
{
	WERROR result;

	/* that's an [in out] buffer */

	if (!r->in.buffer && (r->in.offered != 0)) {
		return WERR_INVALID_PARAM;
	}

	DEBUG(5,("_spoolss_EnumPrintProcessors\n"));

	/*
	 * Enumerate the print processors ...
	 *
	 * Just reply with "winprint", to keep NT happy
	 * and I can use my nice printer checker.
	 */

	*r->out.count = 0;
	*r->out.needed = 0;
	*r->out.info = NULL;

	if (!get_short_archi(r->in.environment)) {
		return WERR_INVALID_ENVIRONMENT;
	}

	switch (r->in.level) {
	case 1:
		result = enumprintprocessors_level_1(p->mem_ctx, r->out.info,
						     r->out.count);
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(p->mem_ctx,
						     spoolss_EnumPrintProcessors,
						     *r->out.info, r->in.level,
						     *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************************
 fill_printprocdatatype1
****************************************************************************/

static WERROR fill_printprocdatatype1(TALLOC_CTX *mem_ctx,
				      struct spoolss_PrintProcDataTypesInfo1 *r,
				      const char *name_array)
{
	r->name_array = talloc_strdup(mem_ctx, name_array);
	W_ERROR_HAVE_NO_MEMORY(r->name_array);

	return WERR_OK;
}

/****************************************************************************
 enumprintprocdatatypes level 1.
****************************************************************************/

static WERROR enumprintprocdatatypes_level_1(TALLOC_CTX *mem_ctx,
					     union spoolss_PrintProcDataTypesInfo **info_p,
					     uint32_t *count)
{
	WERROR result;
	union spoolss_PrintProcDataTypesInfo *info;

	info = TALLOC_ARRAY(mem_ctx, union spoolss_PrintProcDataTypesInfo, 1);
	W_ERROR_HAVE_NO_MEMORY(info);

	*count = 1;

	result = fill_printprocdatatype1(info, &info[0].info1, "RAW");
	if (!W_ERROR_IS_OK(result)) {
		goto out;
	}

 out:
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(info);
		*count = 0;
		return result;
	}

	*info_p = info;

	return WERR_OK;
}

/****************************************************************
 _spoolss_EnumPrintProcDataTypes
****************************************************************/

WERROR _spoolss_EnumPrintProcDataTypes(struct pipes_struct *p,
				       struct spoolss_EnumPrintProcDataTypes *r)
{
	WERROR result;

	/* that's an [in out] buffer */

	if (!r->in.buffer && (r->in.offered != 0)) {
		return WERR_INVALID_PARAM;
	}

	DEBUG(5,("_spoolss_EnumPrintProcDataTypes\n"));

	*r->out.count = 0;
	*r->out.needed = 0;
	*r->out.info = NULL;

	if (r->in.print_processor_name == NULL ||
	    !strequal(r->in.print_processor_name, "winprint")) {
		return WERR_UNKNOWN_PRINTPROCESSOR;
	}

	switch (r->in.level) {
	case 1:
		result = enumprintprocdatatypes_level_1(p->mem_ctx, r->out.info,
							r->out.count);
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(p->mem_ctx,
						     spoolss_EnumPrintProcDataTypes,
						     *r->out.info, r->in.level,
						     *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************************
 fill_monitor_1
****************************************************************************/

static WERROR fill_monitor_1(TALLOC_CTX *mem_ctx,
			     struct spoolss_MonitorInfo1 *r,
			     const char *monitor_name)
{
	r->monitor_name			= talloc_strdup(mem_ctx, monitor_name);
	W_ERROR_HAVE_NO_MEMORY(r->monitor_name);

	return WERR_OK;
}

/****************************************************************************
 fill_monitor_2
****************************************************************************/

static WERROR fill_monitor_2(TALLOC_CTX *mem_ctx,
			     struct spoolss_MonitorInfo2 *r,
			     const char *monitor_name,
			     const char *environment,
			     const char *dll_name)
{
	r->monitor_name			= talloc_strdup(mem_ctx, monitor_name);
	W_ERROR_HAVE_NO_MEMORY(r->monitor_name);
	r->environment			= talloc_strdup(mem_ctx, environment);
	W_ERROR_HAVE_NO_MEMORY(r->environment);
	r->dll_name			= talloc_strdup(mem_ctx, dll_name);
	W_ERROR_HAVE_NO_MEMORY(r->dll_name);

	return WERR_OK;
}

/****************************************************************************
 enumprintmonitors level 1.
****************************************************************************/

static WERROR enumprintmonitors_level_1(TALLOC_CTX *mem_ctx,
					union spoolss_MonitorInfo **info_p,
					uint32_t *count)
{
	union spoolss_MonitorInfo *info;
	WERROR result = WERR_OK;

	info = TALLOC_ARRAY(mem_ctx, union spoolss_MonitorInfo, 2);
	W_ERROR_HAVE_NO_MEMORY(info);

	*count = 2;

	result = fill_monitor_1(info, &info[0].info1,
				SPL_LOCAL_PORT);
	if (!W_ERROR_IS_OK(result)) {
		goto out;
	}

	result = fill_monitor_1(info, &info[1].info1,
				SPL_TCPIP_PORT);
	if (!W_ERROR_IS_OK(result)) {
		goto out;
	}

out:
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(info);
		*count = 0;
		return result;
	}

	*info_p = info;

	return WERR_OK;
}

/****************************************************************************
 enumprintmonitors level 2.
****************************************************************************/

static WERROR enumprintmonitors_level_2(TALLOC_CTX *mem_ctx,
					union spoolss_MonitorInfo **info_p,
					uint32_t *count)
{
	union spoolss_MonitorInfo *info;
	WERROR result = WERR_OK;

	info = TALLOC_ARRAY(mem_ctx, union spoolss_MonitorInfo, 2);
	W_ERROR_HAVE_NO_MEMORY(info);

	*count = 2;

	result = fill_monitor_2(info, &info[0].info2,
				SPL_LOCAL_PORT,
				"Windows NT X86", /* FIXME */
				"localmon.dll");
	if (!W_ERROR_IS_OK(result)) {
		goto out;
	}

	result = fill_monitor_2(info, &info[1].info2,
				SPL_TCPIP_PORT,
				"Windows NT X86", /* FIXME */
				"tcpmon.dll");
	if (!W_ERROR_IS_OK(result)) {
		goto out;
	}

out:
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(info);
		*count = 0;
		return result;
	}

	*info_p = info;

	return WERR_OK;
}

/****************************************************************
 _spoolss_EnumMonitors
****************************************************************/

WERROR _spoolss_EnumMonitors(struct pipes_struct *p,
			     struct spoolss_EnumMonitors *r)
{
	WERROR result;

	/* that's an [in out] buffer */

	if (!r->in.buffer && (r->in.offered != 0)) {
		return WERR_INVALID_PARAM;
	}

	DEBUG(5,("_spoolss_EnumMonitors\n"));

	/*
	 * Enumerate the print monitors ...
	 *
	 * Just reply with "Local Port", to keep NT happy
	 * and I can use my nice printer checker.
	 */

	*r->out.count = 0;
	*r->out.needed = 0;
	*r->out.info = NULL;

	switch (r->in.level) {
	case 1:
		result = enumprintmonitors_level_1(p->mem_ctx, r->out.info,
						   r->out.count);
		break;
	case 2:
		result = enumprintmonitors_level_2(p->mem_ctx, r->out.info,
						   r->out.count);
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(p->mem_ctx,
						     spoolss_EnumMonitors,
						     *r->out.info, r->in.level,
						     *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************************
****************************************************************************/

static WERROR getjob_level_1(TALLOC_CTX *mem_ctx,
			     const print_queue_struct *queue,
			     int count, int snum,
			     struct spoolss_PrinterInfo2 *pinfo2,
			     uint32_t jobid,
			     struct spoolss_JobInfo1 *r)
{
	int i = 0;
	bool found = false;

	for (i=0; i<count; i++) {
		if (queue[i].sysjob == (int)jobid) {
			found = true;
			break;
		}
	}

	if (found == false) {
		/* NT treats not found as bad param... yet another bad choice */
		return WERR_INVALID_PARAM;
	}

	return fill_job_info1(mem_ctx,
			      r,
			      &queue[i],
			      i,
			      snum,
			      pinfo2);
}

/****************************************************************************
****************************************************************************/

static WERROR getjob_level_2(TALLOC_CTX *mem_ctx,
			     const print_queue_struct *queue,
			     int count, int snum,
			     struct spoolss_PrinterInfo2 *pinfo2,
			     uint32_t jobid,
			     struct spoolss_JobInfo2 *r)
{
	int i = 0;
	bool found = false;
	struct spoolss_DeviceMode *devmode;
	WERROR result;

	for (i=0; i<count; i++) {
		if (queue[i].sysjob == (int)jobid) {
			found = true;
			break;
		}
	}

	if (found == false) {
		/* NT treats not found as bad param... yet another bad
		   choice */
		return WERR_INVALID_PARAM;
	}

	/*
	 * if the print job does not have a DEVMODE associated with it,
	 * just use the one for the printer. A NULL devicemode is not
	 *  a failure condition
	 */

	devmode = print_job_devmode(mem_ctx, lp_const_servicename(snum), jobid);
	if (!devmode) {
		result = spoolss_create_default_devmode(mem_ctx,
						pinfo2->printername,
						&devmode);
		if (!W_ERROR_IS_OK(result)) {
			DEBUG(3, ("Can't proceed w/o a devmode!"));
			return result;
		}
	}

	return fill_job_info2(mem_ctx,
			      r,
			      &queue[i],
			      i,
			      snum,
			      pinfo2,
			      devmode);
}

/****************************************************************
 _spoolss_GetJob
****************************************************************/

WERROR _spoolss_GetJob(struct pipes_struct *p,
		       struct spoolss_GetJob *r)
{
	WERROR result = WERR_OK;
	struct spoolss_PrinterInfo2 *pinfo2 = NULL;
	int snum;
	int count;
	print_queue_struct 	*queue = NULL;
	print_status_struct prt_status;

	/* that's an [in out] buffer */

	if (!r->in.buffer && (r->in.offered != 0)) {
		return WERR_INVALID_PARAM;
	}

	DEBUG(5,("_spoolss_GetJob\n"));

	*r->out.needed = 0;

	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}

	result = winreg_get_printer_internal(p->mem_ctx,
				    get_session_info_system(),
				    p->msg_ctx,
				    lp_const_servicename(snum),
				    &pinfo2);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	count = print_queue_status(p->msg_ctx, snum, &queue, &prt_status);

	DEBUGADD(4,("count:[%d], prt_status:[%d], [%s]\n",
	             count, prt_status.status, prt_status.message));

	switch (r->in.level) {
	case 1:
		result = getjob_level_1(p->mem_ctx,
					queue, count, snum, pinfo2,
					r->in.job_id, &r->out.info->info1);
		break;
	case 2:
		result = getjob_level_2(p->mem_ctx,
					queue, count, snum, pinfo2,
					r->in.job_id, &r->out.info->info2);
		break;
	default:
		result = WERR_UNKNOWN_LEVEL;
		break;
	}

	SAFE_FREE(queue);
	TALLOC_FREE(pinfo2);

	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(r->out.info);
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION(spoolss_JobInfo, r->out.info,
										   r->in.level);
	r->out.info	= SPOOLSS_BUFFER_OK(r->out.info, NULL);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************
 _spoolss_GetPrinterDataEx
****************************************************************/

WERROR _spoolss_GetPrinterDataEx(struct pipes_struct *p,
				 struct spoolss_GetPrinterDataEx *r)
{

	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);
	const char *printer;
	int 			snum = 0;
	WERROR result = WERR_OK;
	DATA_BLOB blob;
	enum winreg_Type val_type = REG_NONE;
	uint8_t *val_data = NULL;
	uint32_t val_size = 0;
	struct dcerpc_binding_handle *b;

	DEBUG(4,("_spoolss_GetPrinterDataEx\n"));

	DEBUG(10, ("_spoolss_GetPrinterDataEx: key => [%s], value => [%s]\n",
		r->in.key_name, r->in.value_name));

	/* in case of problem, return some default values */

	*r->out.needed	= 0;
	*r->out.type	= REG_NONE;

	if (!Printer) {
		DEBUG(2,("_spoolss_GetPrinterDataEx: Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		result = WERR_BADFID;
		goto done;
	}

	/* Is the handle to a printer or to the server? */

	if (Printer->printer_type == SPLHND_SERVER) {

		union spoolss_PrinterData data;

		result = getprinterdata_printer_server(p->mem_ctx,
						       r->in.value_name,
						       r->out.type,
						       &data);
		if (!W_ERROR_IS_OK(result)) {
			return result;
		}

		result = push_spoolss_PrinterData(p->mem_ctx, &blob,
						  *r->out.type, &data);
		if (!W_ERROR_IS_OK(result)) {
			return result;
		}

		*r->out.needed = blob.length;

		if (r->in.offered >= *r->out.needed) {
			memcpy(r->out.data, blob.data, blob.length);
		}

		return SPOOLSS_BUFFER_OK(WERR_OK, WERR_MORE_DATA);
	}

	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}
	printer = lp_const_servicename(snum);

	/* check to see if the keyname is valid */
	if (!strlen(r->in.key_name)) {
		return WERR_INVALID_PARAM;
	}

	result = winreg_printer_binding_handle(p->mem_ctx,
					       get_session_info_system(),
					       p->msg_ctx,
					       &b);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	/* XP sends this and wants the ChangeID value from PRINTER_INFO_0 */
	if (strequal(r->in.key_name, SPOOL_PRINTERDATA_KEY) &&
	    strequal(r->in.value_name, "ChangeId")) {
		*r->out.type = REG_DWORD;
		*r->out.needed = 4;
		if (r->in.offered >= *r->out.needed) {
			uint32_t changeid = 0;

			result = winreg_printer_get_changeid(p->mem_ctx, b,
							     printer,
							     &changeid);
			if (!W_ERROR_IS_OK(result)) {
				return result;
			}

			SIVAL(r->out.data, 0, changeid);
			result = WERR_OK;
		}
		goto done;
	}

	result = winreg_get_printer_dataex(p->mem_ctx, b,
					   printer,
					   r->in.key_name,
					   r->in.value_name,
					   &val_type,
					   &val_data,
					   &val_size);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	*r->out.needed = val_size;
	*r->out.type = val_type;

	if (r->in.offered >= *r->out.needed) {
		memcpy(r->out.data, val_data, val_size);
	}

 done:
	/* retain type when returning WERR_MORE_DATA */
	r->out.data     = SPOOLSS_BUFFER_OK(r->out.data, r->out.data);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_MORE_DATA);
}

/****************************************************************
 _spoolss_SetPrinterDataEx
****************************************************************/

WERROR _spoolss_SetPrinterDataEx(struct pipes_struct *p,
				 struct spoolss_SetPrinterDataEx *r)
{
	struct spoolss_PrinterInfo2 *pinfo2 = NULL;
	int 			snum = 0;
	WERROR 			result = WERR_OK;
	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);
	char			*oid_string;
	struct dcerpc_binding_handle *b;

	DEBUG(4,("_spoolss_SetPrinterDataEx\n"));

        /* From MSDN documentation of SetPrinterDataEx: pass request to
           SetPrinterData if key is "PrinterDriverData" */

	if (!Printer) {
		DEBUG(2,("_spoolss_SetPrinterDataEx: Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	if (Printer->printer_type == SPLHND_SERVER) {
		DEBUG(10,("_spoolss_SetPrinterDataEx: "
			"Not implemented for server handles yet\n"));
		return WERR_INVALID_PARAM;
	}

	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}

	/*
	 * Access check : NT returns "access denied" if you make a
	 * SetPrinterData call without the necessary privildge.
	 * we were originally returning OK if nothing changed
	 * which made Win2k issue **a lot** of SetPrinterData
	 * when connecting to a printer  --jerry
	 */

	if (Printer->access_granted != PRINTER_ACCESS_ADMINISTER) {
		DEBUG(3, ("_spoolss_SetPrinterDataEx: "
			"change denied by handle access permissions\n"));
		return WERR_ACCESS_DENIED;
	}

	result = winreg_printer_binding_handle(p->mem_ctx,
					       get_session_info_system(),
					       p->msg_ctx,
					       &b);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	result = winreg_get_printer(Printer, b,
				    lp_servicename(snum),
				    &pinfo2);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	/* check for OID in valuename */

	oid_string = strchr(r->in.value_name, ',');
	if (oid_string) {
		*oid_string = '\0';
		oid_string++;
	}

	/* save the registry data */

	result = winreg_set_printer_dataex(p->mem_ctx, b,
					   pinfo2->sharename,
					   r->in.key_name,
					   r->in.value_name,
					   r->in.type,
					   r->in.data,
					   r->in.offered);

	if (W_ERROR_IS_OK(result)) {
		/* save the OID if one was specified */
		if (oid_string) {
			char *str = talloc_asprintf(p->mem_ctx, "%s\\%s",
				r->in.key_name, SPOOL_OID_KEY);
			if (!str) {
				result = WERR_NOMEM;
				goto done;
			}

			/*
			 * I'm not checking the status here on purpose.  Don't know
			 * if this is right, but I'm returning the status from the
			 * previous set_printer_dataex() call.  I have no idea if
			 * this is right.    --jerry
			 */
			winreg_set_printer_dataex(p->mem_ctx, b,
						  pinfo2->sharename,
						  str,
						  r->in.value_name,
						  REG_SZ,
						  (uint8_t *) oid_string,
						  strlen(oid_string) + 1);
		}

		result = winreg_printer_update_changeid(p->mem_ctx, b,
							lp_const_servicename(snum));

	}

done:
	talloc_free(pinfo2);
	return result;
}

/****************************************************************
 _spoolss_DeletePrinterDataEx
****************************************************************/

WERROR _spoolss_DeletePrinterDataEx(struct pipes_struct *p,
				    struct spoolss_DeletePrinterDataEx *r)
{
	const char *printer;
	int 		snum=0;
	WERROR 		status = WERR_OK;
	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);

	DEBUG(5,("_spoolss_DeletePrinterDataEx\n"));

	if (!Printer) {
		DEBUG(2,("_spoolss_DeletePrinterDataEx: "
			"Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	if (Printer->access_granted != PRINTER_ACCESS_ADMINISTER) {
		DEBUG(3, ("_spoolss_DeletePrinterDataEx: "
			"printer properties change denied by handle\n"));
		return WERR_ACCESS_DENIED;
	}

	if (!r->in.value_name || !r->in.key_name) {
		return WERR_NOMEM;
	}

	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}
	printer = lp_const_servicename(snum);

	status = winreg_delete_printer_dataex_internal(p->mem_ctx,
					      get_session_info_system(),
					      p->msg_ctx,
					      printer,
					      r->in.key_name,
					      r->in.value_name);
	if (W_ERROR_IS_OK(status)) {
		status = winreg_printer_update_changeid_internal(p->mem_ctx,
							get_session_info_system(),
							p->msg_ctx,
							printer);
	}

	return status;
}

/****************************************************************
 _spoolss_EnumPrinterKey
****************************************************************/

WERROR _spoolss_EnumPrinterKey(struct pipes_struct *p,
			       struct spoolss_EnumPrinterKey *r)
{
	uint32_t	num_keys;
	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);
	int 		snum = 0;
	WERROR		result = WERR_BADFILE;
	const char **array = NULL;
	DATA_BLOB blob;

	DEBUG(4,("_spoolss_EnumPrinterKey\n"));

	if (!Printer) {
		DEBUG(2,("_spoolss_EnumPrinterKey: Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}

	result = winreg_enum_printer_key_internal(p->mem_ctx,
					 get_session_info_system(),
					 p->msg_ctx,
					 lp_const_servicename(snum),
					 r->in.key_name,
					 &num_keys,
					 &array);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	if (!push_reg_multi_sz(p->mem_ctx, &blob, array)) {
		result = WERR_NOMEM;
		goto done;
	}

	*r->out._ndr_size = r->in.offered / 2;
	*r->out.needed = blob.length;

	if (r->in.offered < *r->out.needed) {
		result = WERR_MORE_DATA;
	} else {
		result = WERR_OK;
		r->out.key_buffer->string_array = array;
	}

 done:
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(array);
		if (!W_ERROR_EQUAL(result, WERR_MORE_DATA)) {
			*r->out.needed = 0;
		}
	}

	return result;
}

/****************************************************************
 _spoolss_DeletePrinterKey
****************************************************************/

WERROR _spoolss_DeletePrinterKey(struct pipes_struct *p,
				 struct spoolss_DeletePrinterKey *r)
{
	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);
	int 			snum=0;
	WERROR			status;
	const char *printer;
	struct dcerpc_binding_handle *b;

	DEBUG(5,("_spoolss_DeletePrinterKey\n"));

	if (!Printer) {
		DEBUG(2,("_spoolss_DeletePrinterKey: Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	/* if keyname == NULL, return error */
	if ( !r->in.key_name )
		return WERR_INVALID_PARAM;

	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}

	if (Printer->access_granted != PRINTER_ACCESS_ADMINISTER) {
		DEBUG(3, ("_spoolss_DeletePrinterKey: "
			"printer properties change denied by handle\n"));
		return WERR_ACCESS_DENIED;
	}

	printer = lp_const_servicename(snum);

	status = winreg_printer_binding_handle(p->mem_ctx,
					       get_session_info_system(),
					       p->msg_ctx,
					       &b);
	if (!W_ERROR_IS_OK(status)) {
		return status;
	}

	/* delete the key and all subkeys */
	status = winreg_delete_printer_key(p->mem_ctx, b,
					   printer,
					   r->in.key_name);
	if (W_ERROR_IS_OK(status)) {
		status = winreg_printer_update_changeid(p->mem_ctx, b,
							printer);
	}

	return status;
}

/****************************************************************
 _spoolss_EnumPrinterDataEx
****************************************************************/

WERROR _spoolss_EnumPrinterDataEx(struct pipes_struct *p,
				  struct spoolss_EnumPrinterDataEx *r)
{
	uint32_t	count = 0;
	struct spoolss_PrinterEnumValues *info = NULL;
	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);
	int 		snum;
	WERROR 		result;

	DEBUG(4,("_spoolss_EnumPrinterDataEx\n"));

	*r->out.count = 0;
	*r->out.needed = 0;
	*r->out.info = NULL;

	if (!Printer) {
		DEBUG(2,("_spoolss_EnumPrinterDataEx: Invalid handle (%s:%u:%u1<).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	/*
	 * first check for a keyname of NULL or "".  Win2k seems to send
	 * this a lot and we should send back WERR_INVALID_PARAM
	 * no need to spend time looking up the printer in this case.
	 * --jerry
	 */

	if (!strlen(r->in.key_name)) {
		result = WERR_INVALID_PARAM;
		goto done;
	}

	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}

	/* now look for a match on the key name */
	result = winreg_enum_printer_dataex_internal(p->mem_ctx,
					    get_session_info_system(),
					    p->msg_ctx,
					    lp_const_servicename(snum),
					    r->in.key_name,
					    &count,
					    &info);
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

#if 0 /* FIXME - gd */
	/* housekeeping information in the reply */

	/* Fix from Martin Zielinski <mz@seh.de> - ensure
	 * the hand marshalled container size is a multiple
	 * of 4 bytes for RPC alignment.
	 */

	if (needed % 4) {
		needed += 4-(needed % 4);
	}
#endif
	*r->out.count	= count;
	*r->out.info	= info;

 done:
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_ARRAY(p->mem_ctx,
					       spoolss_EnumPrinterDataEx,
					       *r->out.info,
					       *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, *r->out.count);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_MORE_DATA);
}

/****************************************************************************
****************************************************************************/

static WERROR getprintprocessordirectory_level_1(TALLOC_CTX *mem_ctx,
						 const char *servername,
						 const char *environment,
						 struct spoolss_PrintProcessorDirectoryInfo1 *r)
{
	WERROR werr;
	char *path = NULL;

	werr = compose_spoolss_server_path(mem_ctx,
					   servername,
					   environment,
					   SPOOLSS_PRTPROCS_PATH,
					   &path);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	DEBUG(4,("print processor directory: [%s]\n", path));

	r->directory_name = path;

	return WERR_OK;
}

/****************************************************************
 _spoolss_GetPrintProcessorDirectory
****************************************************************/

WERROR _spoolss_GetPrintProcessorDirectory(struct pipes_struct *p,
					   struct spoolss_GetPrintProcessorDirectory *r)
{
	WERROR result;
	char *prnproc_share = NULL;
	bool prnproc_share_exists = false;
	int snum;

	/* that's an [in out] buffer */

	if (!r->in.buffer && (r->in.offered != 0)) {
		return WERR_INVALID_PARAM;
	}

	DEBUG(5,("_spoolss_GetPrintProcessorDirectory: level %d\n",
		r->in.level));

	*r->out.needed = 0;

	/* r->in.level is ignored */

	/* We always should reply with a local print processor directory so that
	 * users are not forced to have a [prnproc$] share on the Samba spoolss
	 * server, if users decide to do so, lets announce it though - Guenther */

	snum = find_service(talloc_tos(), "prnproc$", &prnproc_share);
	if (!prnproc_share) {
		return WERR_NOMEM;
	}
	if (snum != -1) {
		prnproc_share_exists = true;
	}

	result = getprintprocessordirectory_level_1(p->mem_ctx,
						    prnproc_share_exists ? r->in.server : NULL,
						    r->in.environment,
						    &r->out.info->info1);
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(r->out.info);
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION(spoolss_PrintProcessorDirectoryInfo,
										   r->out.info, r->in.level);
	r->out.info	= SPOOLSS_BUFFER_OK(r->out.info, NULL);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/*******************************************************************
 ********************************************************************/

static bool push_monitorui_buf(TALLOC_CTX *mem_ctx, DATA_BLOB *buf,
			       const char *dllname)
{
	enum ndr_err_code ndr_err;
	struct spoolss_MonitorUi ui;

	ui.dll_name = dllname;

	ndr_err = ndr_push_struct_blob(buf, mem_ctx, &ui,
		       (ndr_push_flags_fn_t)ndr_push_spoolss_MonitorUi);
	if (NDR_ERR_CODE_IS_SUCCESS(ndr_err) && (DEBUGLEVEL >= 10)) {
		NDR_PRINT_DEBUG(spoolss_MonitorUi, &ui);
	}
	return NDR_ERR_CODE_IS_SUCCESS(ndr_err);
}

/*******************************************************************
 Streams the monitor UI DLL name in UNICODE
*******************************************************************/

static WERROR xcvtcp_monitorui(TALLOC_CTX *mem_ctx,
			       struct security_token *token, DATA_BLOB *in,
			       DATA_BLOB *out, uint32_t *needed)
{
	const char *dllname = "tcpmonui.dll";

	*needed = (strlen(dllname)+1) * 2;

	if (out->length < *needed) {
		return WERR_INSUFFICIENT_BUFFER;
	}

	if (!push_monitorui_buf(mem_ctx, out, dllname)) {
		return WERR_NOMEM;
	}

	return WERR_OK;
}

/*******************************************************************
 ********************************************************************/

static bool pull_port_data_1(TALLOC_CTX *mem_ctx,
			     struct spoolss_PortData1 *port1,
			     const DATA_BLOB *buf)
{
	enum ndr_err_code ndr_err;
	ndr_err = ndr_pull_struct_blob(buf, mem_ctx, port1,
		       (ndr_pull_flags_fn_t)ndr_pull_spoolss_PortData1);
	if (NDR_ERR_CODE_IS_SUCCESS(ndr_err) && (DEBUGLEVEL >= 10)) {
		NDR_PRINT_DEBUG(spoolss_PortData1, port1);
	}
	return NDR_ERR_CODE_IS_SUCCESS(ndr_err);
}

/*******************************************************************
 ********************************************************************/

static bool pull_port_data_2(TALLOC_CTX *mem_ctx,
			     struct spoolss_PortData2 *port2,
			     const DATA_BLOB *buf)
{
	enum ndr_err_code ndr_err;
	ndr_err = ndr_pull_struct_blob(buf, mem_ctx, port2,
		       (ndr_pull_flags_fn_t)ndr_pull_spoolss_PortData2);
	if (NDR_ERR_CODE_IS_SUCCESS(ndr_err) && (DEBUGLEVEL >= 10)) {
		NDR_PRINT_DEBUG(spoolss_PortData2, port2);
	}
	return NDR_ERR_CODE_IS_SUCCESS(ndr_err);
}

/*******************************************************************
 Create a new TCP/IP port
*******************************************************************/

static WERROR xcvtcp_addport(TALLOC_CTX *mem_ctx,
			     struct security_token *token, DATA_BLOB *in,
			     DATA_BLOB *out, uint32_t *needed)
{
	struct spoolss_PortData1 port1;
	struct spoolss_PortData2 port2;
	char *device_uri = NULL;
	uint32_t version;

	const char *portname;
	const char *hostaddress;
	const char *queue;
	uint32_t port_number;
	uint32_t protocol;

	/* peek for spoolss_PortData version */

	if (!in || (in->length < (128 + 4))) {
		return WERR_GENERAL_FAILURE;
	}

	version = IVAL(in->data, 128);

	switch (version) {
		case 1:
			ZERO_STRUCT(port1);

			if (!pull_port_data_1(mem_ctx, &port1, in)) {
				return WERR_NOMEM;
			}

			portname	= port1.portname;
			hostaddress	= port1.hostaddress;
			queue		= port1.queue;
			protocol	= port1.protocol;
			port_number	= port1.port_number;

			break;
		case 2:
			ZERO_STRUCT(port2);

			if (!pull_port_data_2(mem_ctx, &port2, in)) {
				return WERR_NOMEM;
			}

			portname	= port2.portname;
			hostaddress	= port2.hostaddress;
			queue		= port2.queue;
			protocol	= port2.protocol;
			port_number	= port2.port_number;

			break;
		default:
			DEBUG(1,("xcvtcp_addport: "
				"unknown version of port_data: %d\n", version));
			return WERR_UNKNOWN_PORT;
	}

	/* create the device URI and call the add_port_hook() */

	switch (protocol) {
	case PROTOCOL_RAWTCP_TYPE:
		device_uri = talloc_asprintf(mem_ctx,
				"socket://%s:%d/", hostaddress,
				port_number);
		break;

	case PROTOCOL_LPR_TYPE:
		device_uri = talloc_asprintf(mem_ctx,
			"lpr://%s/%s", hostaddress, queue );
		break;

	default:
		return WERR_UNKNOWN_PORT;
	}

	if (!device_uri) {
		return WERR_NOMEM;
	}

	return add_port_hook(mem_ctx, token, portname, device_uri);
}

/*******************************************************************
*******************************************************************/

struct xcv_api_table xcvtcp_cmds[] = {
	{ "MonitorUI",	xcvtcp_monitorui },
	{ "AddPort",	xcvtcp_addport},
	{ NULL,		NULL }
};

static WERROR process_xcvtcp_command(TALLOC_CTX *mem_ctx,
				     struct security_token *token, const char *command,
				     DATA_BLOB *inbuf,
				     DATA_BLOB *outbuf,
				     uint32_t *needed )
{
	int i;

	DEBUG(10,("process_xcvtcp_command: Received command \"%s\"\n", command));

	for ( i=0; xcvtcp_cmds[i].name; i++ ) {
		if ( strcmp( command, xcvtcp_cmds[i].name ) == 0 )
			return xcvtcp_cmds[i].fn(mem_ctx, token, inbuf, outbuf, needed);
	}

	return WERR_BADFUNC;
}

/*******************************************************************
*******************************************************************/
#if 0 	/* don't support management using the "Local Port" monitor */

static WERROR xcvlocal_monitorui(TALLOC_CTX *mem_ctx,
				 struct security_token *token, DATA_BLOB *in,
				 DATA_BLOB *out, uint32_t *needed)
{
	const char *dllname = "localui.dll";

	*needed = (strlen(dllname)+1) * 2;

	if (out->length < *needed) {
		return WERR_INSUFFICIENT_BUFFER;
	}

	if (!push_monitorui_buf(mem_ctx, out, dllname)) {
		return WERR_NOMEM;
	}

	return WERR_OK;
}

/*******************************************************************
*******************************************************************/

struct xcv_api_table xcvlocal_cmds[] = {
	{ "MonitorUI",	xcvlocal_monitorui },
	{ NULL,		NULL }
};
#else
struct xcv_api_table xcvlocal_cmds[] = {
	{ NULL,		NULL }
};
#endif



/*******************************************************************
*******************************************************************/

static WERROR process_xcvlocal_command(TALLOC_CTX *mem_ctx,
				       struct security_token *token, const char *command,
				       DATA_BLOB *inbuf, DATA_BLOB *outbuf,
				       uint32_t *needed)
{
	int i;

	DEBUG(10,("process_xcvlocal_command: Received command \"%s\"\n", command));

	for ( i=0; xcvlocal_cmds[i].name; i++ ) {
		if ( strcmp( command, xcvlocal_cmds[i].name ) == 0 )
			return xcvlocal_cmds[i].fn(mem_ctx, token, inbuf, outbuf, needed);
	}
	return WERR_BADFUNC;
}

/****************************************************************
 _spoolss_XcvData
****************************************************************/

WERROR _spoolss_XcvData(struct pipes_struct *p,
			struct spoolss_XcvData *r)
{
	struct printer_handle *Printer = find_printer_index_by_hnd(p, r->in.handle);
	DATA_BLOB out_data = data_blob_null;
	WERROR werror;

	if (!Printer) {
		DEBUG(2,("_spoolss_XcvData: Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	/* Has to be a handle to the TCP/IP port monitor */

	if ( !(Printer->printer_type & (SPLHND_PORTMON_LOCAL|SPLHND_PORTMON_TCP)) ) {
		DEBUG(2,("_spoolss_XcvData: Call only valid for Port Monitors\n"));
		return WERR_BADFID;
	}

	/* requires administrative access to the server */

	if ( !(Printer->access_granted & SERVER_ACCESS_ADMINISTER) ) {
		DEBUG(2,("_spoolss_XcvData: denied by handle permissions.\n"));
		return WERR_ACCESS_DENIED;
	}

	/* Allocate the outgoing buffer */

	if (r->in.out_data_size) {
		out_data = data_blob_talloc_zero(p->mem_ctx, r->in.out_data_size);
		if (out_data.data == NULL) {
			return WERR_NOMEM;
		}
	}

	switch ( Printer->printer_type ) {
	case SPLHND_PORTMON_TCP:
		werror = process_xcvtcp_command(p->mem_ctx,
						p->session_info->security_token,
						r->in.function_name,
						&r->in.in_data, &out_data,
						r->out.needed);
		break;
	case SPLHND_PORTMON_LOCAL:
		werror = process_xcvlocal_command(p->mem_ctx,
						  p->session_info->security_token,
						  r->in.function_name,
						  &r->in.in_data, &out_data,
						  r->out.needed);
		break;
	default:
		werror = WERR_INVALID_PRINT_MONITOR;
	}

	if (!W_ERROR_IS_OK(werror)) {
		return werror;
	}

	*r->out.status_code = 0;

	if (r->out.out_data && out_data.data && r->in.out_data_size && out_data.length) {
		memcpy(r->out.out_data, out_data.data,
			MIN(r->in.out_data_size, out_data.length));
	}

	return WERR_OK;
}

/****************************************************************
 _spoolss_AddPrintProcessor
****************************************************************/

WERROR _spoolss_AddPrintProcessor(struct pipes_struct *p,
				  struct spoolss_AddPrintProcessor *r)
{
	/* for now, just indicate success and ignore the add.  We'll
	   automatically set the winprint processor for printer
	   entries later.  Used to debug the LexMark Optra S 1855 PCL
	   driver --jerry */

	return WERR_OK;
}

/****************************************************************
 _spoolss_AddPort
****************************************************************/

WERROR _spoolss_AddPort(struct pipes_struct *p,
			struct spoolss_AddPort *r)
{
	/* do what w2k3 does */

	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_GetPrinterDriver
****************************************************************/

WERROR _spoolss_GetPrinterDriver(struct pipes_struct *p,
				 struct spoolss_GetPrinterDriver *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_ReadPrinter
****************************************************************/

WERROR _spoolss_ReadPrinter(struct pipes_struct *p,
			    struct spoolss_ReadPrinter *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_WaitForPrinterChange
****************************************************************/

WERROR _spoolss_WaitForPrinterChange(struct pipes_struct *p,
				     struct spoolss_WaitForPrinterChange *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_ConfigurePort
****************************************************************/

WERROR _spoolss_ConfigurePort(struct pipes_struct *p,
			      struct spoolss_ConfigurePort *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_DeletePort
****************************************************************/

WERROR _spoolss_DeletePort(struct pipes_struct *p,
			   struct spoolss_DeletePort *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_CreatePrinterIC
****************************************************************/

WERROR _spoolss_CreatePrinterIC(struct pipes_struct *p,
				struct spoolss_CreatePrinterIC *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_PlayGDIScriptOnPrinterIC
****************************************************************/

WERROR _spoolss_PlayGDIScriptOnPrinterIC(struct pipes_struct *p,
					 struct spoolss_PlayGDIScriptOnPrinterIC *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_DeletePrinterIC
****************************************************************/

WERROR _spoolss_DeletePrinterIC(struct pipes_struct *p,
				struct spoolss_DeletePrinterIC *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_AddPrinterConnection
****************************************************************/

WERROR _spoolss_AddPrinterConnection(struct pipes_struct *p,
				     struct spoolss_AddPrinterConnection *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_DeletePrinterConnection
****************************************************************/

WERROR _spoolss_DeletePrinterConnection(struct pipes_struct *p,
					struct spoolss_DeletePrinterConnection *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_PrinterMessageBox
****************************************************************/

WERROR _spoolss_PrinterMessageBox(struct pipes_struct *p,
				  struct spoolss_PrinterMessageBox *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_AddMonitor
****************************************************************/

WERROR _spoolss_AddMonitor(struct pipes_struct *p,
			   struct spoolss_AddMonitor *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_DeleteMonitor
****************************************************************/

WERROR _spoolss_DeleteMonitor(struct pipes_struct *p,
			      struct spoolss_DeleteMonitor *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_DeletePrintProcessor
****************************************************************/

WERROR _spoolss_DeletePrintProcessor(struct pipes_struct *p,
				     struct spoolss_DeletePrintProcessor *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_AddPrintProvidor
****************************************************************/

WERROR _spoolss_AddPrintProvidor(struct pipes_struct *p,
				 struct spoolss_AddPrintProvidor *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_DeletePrintProvidor
****************************************************************/

WERROR _spoolss_DeletePrintProvidor(struct pipes_struct *p,
				    struct spoolss_DeletePrintProvidor *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_FindFirstPrinterChangeNotification
****************************************************************/

WERROR _spoolss_FindFirstPrinterChangeNotification(struct pipes_struct *p,
						   struct spoolss_FindFirstPrinterChangeNotification *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_FindNextPrinterChangeNotification
****************************************************************/

WERROR _spoolss_FindNextPrinterChangeNotification(struct pipes_struct *p,
						  struct spoolss_FindNextPrinterChangeNotification *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_RouterFindFirstPrinterChangeNotificationOld
****************************************************************/

WERROR _spoolss_RouterFindFirstPrinterChangeNotificationOld(struct pipes_struct *p,
							    struct spoolss_RouterFindFirstPrinterChangeNotificationOld *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_ReplyOpenPrinter
****************************************************************/

WERROR _spoolss_ReplyOpenPrinter(struct pipes_struct *p,
				 struct spoolss_ReplyOpenPrinter *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_RouterReplyPrinter
****************************************************************/

WERROR _spoolss_RouterReplyPrinter(struct pipes_struct *p,
				   struct spoolss_RouterReplyPrinter *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_ReplyClosePrinter
****************************************************************/

WERROR _spoolss_ReplyClosePrinter(struct pipes_struct *p,
				  struct spoolss_ReplyClosePrinter *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_AddPortEx
****************************************************************/

WERROR _spoolss_AddPortEx(struct pipes_struct *p,
			  struct spoolss_AddPortEx *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_RouterFindFirstPrinterChangeNotification
****************************************************************/

WERROR _spoolss_RouterFindFirstPrinterChangeNotification(struct pipes_struct *p,
							 struct spoolss_RouterFindFirstPrinterChangeNotification *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_SpoolerInit
****************************************************************/

WERROR _spoolss_SpoolerInit(struct pipes_struct *p,
			    struct spoolss_SpoolerInit *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_ResetPrinterEx
****************************************************************/

WERROR _spoolss_ResetPrinterEx(struct pipes_struct *p,
			       struct spoolss_ResetPrinterEx *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_RouterReplyPrinterEx
****************************************************************/

WERROR _spoolss_RouterReplyPrinterEx(struct pipes_struct *p,
				     struct spoolss_RouterReplyPrinterEx *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_44
****************************************************************/

WERROR _spoolss_44(struct pipes_struct *p,
		   struct spoolss_44 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_SetPort
****************************************************************/

WERROR _spoolss_SetPort(struct pipes_struct *p,
			struct spoolss_SetPort *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_4a
****************************************************************/

WERROR _spoolss_4a(struct pipes_struct *p,
		   struct spoolss_4a *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_4b
****************************************************************/

WERROR _spoolss_4b(struct pipes_struct *p,
		   struct spoolss_4b *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_4c
****************************************************************/

WERROR _spoolss_4c(struct pipes_struct *p,
		   struct spoolss_4c *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_53
****************************************************************/

WERROR _spoolss_53(struct pipes_struct *p,
		   struct spoolss_53 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_AddPerMachineConnection
****************************************************************/

WERROR _spoolss_AddPerMachineConnection(struct pipes_struct *p,
					struct spoolss_AddPerMachineConnection *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_DeletePerMachineConnection
****************************************************************/

WERROR _spoolss_DeletePerMachineConnection(struct pipes_struct *p,
					   struct spoolss_DeletePerMachineConnection *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_EnumPerMachineConnections
****************************************************************/

WERROR _spoolss_EnumPerMachineConnections(struct pipes_struct *p,
					  struct spoolss_EnumPerMachineConnections *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_5a
****************************************************************/

WERROR _spoolss_5a(struct pipes_struct *p,
		   struct spoolss_5a *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_5b
****************************************************************/

WERROR _spoolss_5b(struct pipes_struct *p,
		   struct spoolss_5b *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_5c
****************************************************************/

WERROR _spoolss_5c(struct pipes_struct *p,
		   struct spoolss_5c *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_5d
****************************************************************/

WERROR _spoolss_5d(struct pipes_struct *p,
		   struct spoolss_5d *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_5e
****************************************************************/

WERROR _spoolss_5e(struct pipes_struct *p,
		   struct spoolss_5e *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_5f
****************************************************************/

WERROR _spoolss_5f(struct pipes_struct *p,
		   struct spoolss_5f *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_60
****************************************************************/

WERROR _spoolss_60(struct pipes_struct *p,
		   struct spoolss_60 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_61
****************************************************************/

WERROR _spoolss_61(struct pipes_struct *p,
		   struct spoolss_61 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_62
****************************************************************/

WERROR _spoolss_62(struct pipes_struct *p,
		   struct spoolss_62 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_63
****************************************************************/

WERROR _spoolss_63(struct pipes_struct *p,
		   struct spoolss_63 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_64
****************************************************************/

WERROR _spoolss_64(struct pipes_struct *p,
		   struct spoolss_64 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_65
****************************************************************/

WERROR _spoolss_65(struct pipes_struct *p,
		   struct spoolss_65 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_GetCorePrinterDrivers
****************************************************************/

WERROR _spoolss_GetCorePrinterDrivers(struct pipes_struct *p,
				      struct spoolss_GetCorePrinterDrivers *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_67
****************************************************************/

WERROR _spoolss_67(struct pipes_struct *p,
		   struct spoolss_67 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_GetPrinterDriverPackagePath
****************************************************************/

WERROR _spoolss_GetPrinterDriverPackagePath(struct pipes_struct *p,
					    struct spoolss_GetPrinterDriverPackagePath *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_69
****************************************************************/

WERROR _spoolss_69(struct pipes_struct *p,
		   struct spoolss_69 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_6a
****************************************************************/

WERROR _spoolss_6a(struct pipes_struct *p,
		   struct spoolss_6a *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_6b
****************************************************************/

WERROR _spoolss_6b(struct pipes_struct *p,
		   struct spoolss_6b *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_6c
****************************************************************/

WERROR _spoolss_6c(struct pipes_struct *p,
		   struct spoolss_6c *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_6d
****************************************************************/

WERROR _spoolss_6d(struct pipes_struct *p,
		   struct spoolss_6d *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}
