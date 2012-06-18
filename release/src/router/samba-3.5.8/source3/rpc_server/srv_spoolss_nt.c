/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-2000,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-2000,
 *  Copyright (C) Jean François Micouleau      1998-2000,
 *  Copyright (C) Jeremy Allison               2001-2002,
 *  Copyright (C) Gerald Carter		       2000-2004,
 *  Copyright (C) Tim Potter                   2001-2002.
 *  Copyright (C) Guenther Deschner                 2009.
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
#include "../librpc/gen_ndr/srv_spoolss.h"
#include "../librpc/gen_ndr/cli_spoolss.h"

/* macros stolen from s4 spoolss server */
#define SPOOLSS_BUFFER_UNION(fn,ic,info,level) \
	((info)?ndr_size_##fn(info, level, ic, 0):0)

#define SPOOLSS_BUFFER_UNION_ARRAY(mem_ctx,fn,ic,info,level,count) \
	((info)?ndr_size_##fn##_info(mem_ctx, ic, level, count, info):0)

#define SPOOLSS_BUFFER_ARRAY(mem_ctx,fn,ic,info,count) \
	((info)?ndr_size_##fn##_info(mem_ctx, ic, count, info):0)

#define SPOOLSS_BUFFER_OK(val_true,val_false) ((r->in.offered >= *r->out.needed)?val_true:val_false)


extern userdom_struct current_user_info;

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

#ifndef MAX_OPEN_PRINTER_EXS
#define MAX_OPEN_PRINTER_EXS 50
#endif

#define MAGIC_DISPLAY_FREQUENCY 0xfade2bad
#define PHANTOM_DEVMODE_KEY "_p_f_a_n_t_0_m_"

static Printer_entry *printers_list;

typedef struct _counter_printer_0 {
	struct _counter_printer_0 *next;
	struct _counter_printer_0 *prev;

	int snum;
	uint32_t counter;
} counter_printer_0;

static counter_printer_0 *counter_list;

static struct rpc_pipe_client *notify_cli_pipe; /* print notify back-channel pipe handle*/
static uint32_t smb_connections = 0;


/* in printing/nt_printing.c */

extern struct standard_mapping printer_std_mapping, printserver_std_mapping;

/* API table for Xcv Monitor functions */

struct xcv_api_table {
	const char *name;
	WERROR(*fn) (TALLOC_CTX *mem_ctx, NT_USER_TOKEN *token, DATA_BLOB *in, DATA_BLOB *out, uint32_t *needed);
};

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

static void prune_printername_cache(void);

/***************************************************************************
 Disconnect from the client
****************************************************************************/

static void srv_spoolss_replycloseprinter(int snum, struct policy_handle *handle)
{
	WERROR result;
	NTSTATUS status;

	/*
	 * Tell the specific printing tdb we no longer want messages for this printer
	 * by deregistering our PID.
	 */

	if (!print_notify_deregister_pid(snum))
		DEBUG(0,("print_notify_register_pid: Failed to register our pid for printer %s\n", lp_const_servicename(snum) ));

	/* weird if the test succeds !!! */
	if (smb_connections==0) {
		DEBUG(0,("srv_spoolss_replycloseprinter:Trying to close non-existant notify backchannel !\n"));
		return;
	}

	status = rpccli_spoolss_ReplyClosePrinter(notify_cli_pipe, talloc_tos(),
						  handle,
						  &result);
	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(result))
		DEBUG(0,("srv_spoolss_replycloseprinter: reply_close_printer failed [%s].\n",
			win_errstr(result)));

	/* if it's the last connection, deconnect the IPC$ share */
	if (smb_connections==1) {

		cli_shutdown( rpc_pipe_np_smb_conn(notify_cli_pipe) );
		notify_cli_pipe = NULL; /* The above call shuts downn the pipe also. */

		messaging_deregister(smbd_messaging_context(),
				     MSG_PRINTER_NOTIFY2, NULL);

        	/* Tell the connections db we're no longer interested in
		 * printer notify messages. */

		register_message_flags(false, FLAG_MSG_PRINT_NOTIFY);
	}

	smb_connections--;
}

/****************************************************************************
 Functions to free a printer entry datastruct.
****************************************************************************/

static int printer_entry_destructor(Printer_entry *Printer)
{
	if (Printer->notify.client_connected == true) {
		int snum = -1;

		if ( Printer->printer_type == SPLHND_SERVER) {
			snum = -1;
			srv_spoolss_replycloseprinter(snum, &Printer->notify.client_hnd);
		} else if (Printer->printer_type == SPLHND_PRINTER) {
			snum = print_queue_snum(Printer->sharename);
			if (snum != -1)
				srv_spoolss_replycloseprinter(snum,
						&Printer->notify.client_hnd);
		}
	}

	Printer->notify.flags=0;
	Printer->notify.options=0;
	Printer->notify.localmachine[0]='\0';
	Printer->notify.printerlocal=0;
	TALLOC_FREE(Printer->notify.option);
	Printer->notify.client_connected = false;

	free_nt_devicemode( &Printer->nt_devmode );
	free_a_printer( &Printer->printer_info, 2 );

	/* Remove from the internal list. */
	DLIST_REMOVE(printers_list, Printer);
	return 0;
}

/****************************************************************************
  find printer index by handle
****************************************************************************/

static Printer_entry *find_printer_index_by_hnd(pipes_struct *p,
						struct policy_handle *hnd)
{
	Printer_entry *find_printer = NULL;

	if(!find_policy_by_hnd(p,hnd,(void **)(void *)&find_printer)) {
		DEBUG(2,("find_printer_index_by_hnd: Printer handle not found: "));
		return NULL;
	}

	return find_printer;
}

/****************************************************************************
 Close printer index by handle.
****************************************************************************/

static bool close_printer_handle(pipes_struct *p, struct policy_handle *hnd)
{
	Printer_entry *Printer = find_printer_index_by_hnd(p, hnd);

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

static WERROR delete_printer_hook(TALLOC_CTX *ctx, NT_USER_TOKEN *token, const char *sharename)
{
	char *cmd = lp_deleteprinter_cmd();
	char *command = NULL;
	int ret;
	SE_PRIV se_printop = SE_PRINT_OPERATOR;
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
		is_print_op = user_has_privileges( token, &se_printop );

	DEBUG(10,("Running [%s]\n", command));

	/********** BEGIN SePrintOperatorPrivlege BLOCK **********/

	if ( is_print_op )
		become_root();

	if ( (ret = smbrun(command, NULL)) == 0 ) {
		/* Tell everyone we updated smb.conf. */
		message_send_all(smbd_messaging_context(),
				 MSG_SMB_CONF_UPDATED, NULL, 0, NULL);
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
	reload_services(false);
	unbecome_root();

	if ( lp_servicenumber( sharename )  < 0 )
		return WERR_ACCESS_DENIED;

	return WERR_OK;
}

/****************************************************************************
 Delete a printer given a handle.
****************************************************************************/

static WERROR delete_printer_handle(pipes_struct *p, struct policy_handle *hnd)
{
	Printer_entry *Printer = find_printer_index_by_hnd(p, hnd);
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

	if (del_a_printer( Printer->sharename ) != 0) {
		DEBUG(3,("Error deleting printer %s\n", Printer->sharename));
		return WERR_BADFID;
	}

	result = delete_printer_hook(p->mem_ctx, p->server_info->ptok,
				     Printer->sharename);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}
	prune_printername_cache();
	return WERR_OK;
}

/****************************************************************************
 Return the snum of a printer corresponding to an handle.
****************************************************************************/

static bool get_printer_snum(pipes_struct *p, struct policy_handle *hnd,
			     int *number, struct share_params **params)
{
	Printer_entry *Printer = find_printer_index_by_hnd(p, hnd);

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

static bool set_printer_hnd_printertype(Printer_entry *Printer, const char *handlename)
{
	DEBUG(3,("Setting printer type=%s\n", handlename));

	if ( strlen(handlename) < 3 ) {
		DEBUGADD(4,("A print server must have at least 1 char ! %s\n", handlename));
		return false;
	}

	/* it's a print server */
	if (*handlename=='\\' && *(handlename+1)=='\\' && !strchr_m(handlename+2, '\\')) {
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

static bool set_printer_hnd_name(Printer_entry *Printer, const char *handlename)
{
	int snum;
	int n_services=lp_numservices();
	char *aprinter, *printername;
	const char *servername;
	fstring sname;
	bool found = false;
	NT_PRINTER_INFO_LEVEL *printer = NULL;
	WERROR result;

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
	} else {
		servername = global_myname();
	}

	/* save the servername to fill in replies on this handle */

	if ( !is_myname_or_ipaddr( servername ) )
		return false;

	fstrcpy( Printer->servername, servername );

	if ( Printer->printer_type == SPLHND_SERVER )
		return true;

	if ( Printer->printer_type != SPLHND_PRINTER )
		return false;

	DEBUGADD(5, ("searching for [%s]\n", aprinter ));

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
			return false;
		}
		fstrcpy(sname, tmp);
		SAFE_FREE(tmp);
	}

	/* Search all sharenames first as this is easier than pulling
	   the printer_info_2 off of disk. Don't use find_service() since
	   that calls out to map_username() */

	/* do another loop to look for printernames */

	for (snum=0; !found && snum<n_services; snum++) {

		/* no point going on if this is not a printer */

		if ( !(lp_snum_ok(snum) && lp_print_ok(snum)) )
			continue;

		fstrcpy(sname, lp_servicename(snum));
		if ( strequal( aprinter, sname ) ) {
			found = true;
			break;
		}

		/* no point looking up the printer object if
		   we aren't allowing printername != sharename */

		if ( lp_force_printername(snum) )
			continue;

		fstrcpy(sname, lp_servicename(snum));

		printer = NULL;

		/* This call doesn't fill in the location or comment from
		 * a CUPS server for efficiency with large numbers of printers.
		 * JRA.
		 */

		result = get_a_printer_search( NULL, &printer, 2, sname );
		if ( !W_ERROR_IS_OK(result) ) {
			DEBUG(0,("set_printer_hnd_name: failed to lookup printer [%s] -- result [%s]\n",
				sname, win_errstr(result)));
			continue;
		}

		/* printername is always returned as \\server\printername */
		if ( !(printername = strchr_m(&printer->info_2->printername[2], '\\')) ) {
			DEBUG(0,("set_printer_hnd_name: info2->printername in wrong format! [%s]\n",
				printer->info_2->printername));
			free_a_printer( &printer, 2);
			continue;
		}

		printername++;

		if ( strequal(printername, aprinter) ) {
			free_a_printer( &printer, 2);
			found = true;
			break;
		}

		DEBUGADD(10, ("printername: %s\n", printername));

		free_a_printer( &printer, 2);
	}

	free_a_printer( &printer, 2);

	if ( !found ) {
		if (cache_key != NULL) {
			gencache_set(cache_key, printer_not_found,
				     time(NULL)+300);
			TALLOC_FREE(cache_key);
		}
		DEBUGADD(4,("Printer not found\n"));
		return false;
	}

	if (cache_key != NULL) {
		gencache_set(cache_key, sname, time(NULL)+300);
		TALLOC_FREE(cache_key);
	}

	DEBUGADD(4,("set_printer_hnd_name: Printer found: %s -> %s\n", aprinter, sname));

	fstrcpy(Printer->sharename, sname);

	return true;
}

/****************************************************************************
 Find first available printer slot. creates a printer handle for you.
 ****************************************************************************/

static bool open_printer_hnd(pipes_struct *p, struct policy_handle *hnd,
			     const char *name, uint32_t access_granted)
{
	Printer_entry *new_printer;

	DEBUG(10,("open_printer_hnd: name [%s]\n", name));

	new_printer = TALLOC_ZERO_P(NULL, Printer_entry);
	if (new_printer == NULL) {
		return false;
	}
	talloc_set_destructor(new_printer, printer_entry_destructor);

	if (!create_policy_hnd(p, hnd, new_printer)) {
		TALLOC_FREE(new_printer);
		return false;
	}

	/* Add to the internal list. */
	DLIST_ADD(printers_list, new_printer);

	new_printer->notify.option=NULL;

	if (!set_printer_hnd_printertype(new_printer, name)) {
		close_printer_handle(p, hnd);
		return false;
	}

	if (!set_printer_hnd_name(new_printer, name)) {
		close_printer_handle(p, hnd);
		return false;
	}

	new_printer->access_granted = access_granted;

	DEBUG(5, ("%d printer handles active\n",
		  (int)num_pipe_handles(p->pipe_handles)));

	return true;
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

static bool is_monitoring_event(Printer_entry *p, uint16_t notify_type,
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

#define SETUP_SPOOLSS_NOTIFY_DATA_SECDESC(_data, _size, _sd) \
	_data->data.sd.sd = dup_sec_desc(mem_ctx, _sd); \
	if (!_data->data.sd.sd) { \
		_data->data.sd.sd_size = 0; \
	} \
	_data->data.sd.sd_size = _size;

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

/***********************************************************************
 Send a change notication message on all handles which have a call
 back registered
 **********************************************************************/

static void send_notify2_changes( SPOOLSS_NOTIFY_MSG_CTR *ctr, uint32_t idx )
{
	Printer_entry 		 *p;
	TALLOC_CTX		 *mem_ctx = notify_ctr_getctx( ctr );
	SPOOLSS_NOTIFY_MSG_GROUP *msg_group = notify_ctr_getgroup( ctr, idx );
	SPOOLSS_NOTIFY_MSG       *messages;
	int			 sending_msg_count;

	if ( !msg_group ) {
		DEBUG(5,("send_notify2_changes() called with no msg group!\n"));
		return;
	}

	messages = msg_group->msgs;

	if ( !messages ) {
		DEBUG(5,("send_notify2_changes() called with no messages!\n"));
		return;
	}

	DEBUG(8,("send_notify2_changes: Enter...[%s]\n", msg_group->printername));

	/* loop over all printers */

	for (p = printers_list; p; p = p->next) {
		struct spoolss_Notify *notifies;
		uint32_t count = 0;
		uint32_t id;
		int 	i;

		/* Is there notification on this handle? */

		if ( !p->notify.client_connected )
			continue;

		DEBUG(10,("Client connected! [\\\\%s\\%s]\n", p->servername, p->sharename));

		/* For this printer?  Print servers always receive
                   notifications. */

		if ( ( p->printer_type == SPLHND_PRINTER )  &&
		    ( !strequal(msg_group->printername, p->sharename) ) )
			continue;

		DEBUG(10,("Our printer\n"));

		/* allocate the max entries possible */

		notifies = TALLOC_ZERO_ARRAY(mem_ctx, struct spoolss_Notify, msg_group->num_msgs);
		if (!notifies) {
			return;
		}

		/* build the array of change notifications */

		sending_msg_count = 0;

		for ( i=0; i<msg_group->num_msgs; i++ ) {
			SPOOLSS_NOTIFY_MSG	*msg = &messages[i];

			/* Are we monitoring this event? */

			if (!is_monitoring_event(p, msg->type, msg->field))
				continue;

			sending_msg_count++;


			DEBUG(10,("process_notify2_message: Sending message type [0x%x] field [0x%2x] for printer [%s]\n",
				msg->type, msg->field, p->sharename));

			/*
			 * if the is a printer notification handle and not a job notification
			 * type, then set the id to 0.  Other wise just use what was specified
			 * in the message.
			 *
			 * When registering change notification on a print server handle
			 * we always need to send back the id (snum) matching the printer
			 * for which the change took place.  For change notify registered
			 * on a printer handle, this does not matter and the id should be 0.
			 *
			 * --jerry
			 */

			if ( ( p->printer_type == SPLHND_PRINTER ) && ( msg->type == PRINTER_NOTIFY_TYPE ) )
				id = 0;
			else
				id = msg->id;


			/* Convert unix jobid to smb jobid */

			if (msg->flags & SPOOLSS_NOTIFY_MSG_UNIX_JOBID) {
				id = sysjob_to_jobid(msg->id);

				if (id == -1) {
					DEBUG(3, ("no such unix jobid %d\n", msg->id));
					goto done;
				}
			}

			construct_info_data( &notifies[count], msg->type, msg->field, id );

			switch(msg->type) {
			case PRINTER_NOTIFY_TYPE:
				if ( printer_notify_table[msg->field].fn )
					printer_notify_table[msg->field].fn(msg, &notifies[count], mem_ctx);
				break;

			case JOB_NOTIFY_TYPE:
				if ( job_notify_table[msg->field].fn )
					job_notify_table[msg->field].fn(msg, &notifies[count], mem_ctx);
				break;

			default:
				DEBUG(5, ("Unknown notification type %d\n", msg->type));
				goto done;
			}

			count++;
		}

		if ( sending_msg_count ) {
			NTSTATUS status;
			WERROR werr;
			union spoolss_ReplyPrinterInfo info;
			struct spoolss_NotifyInfo info0;
			uint32_t reply_result;

			info0.version	= 0x2;
			info0.flags	= count ? 0x00020000 /* ??? */ : PRINTER_NOTIFY_INFO_DISCARDED;
			info0.count	= count;
			info0.notifies	= notifies;

			info.info0 = &info0;

			status = rpccli_spoolss_RouterReplyPrinterEx(notify_cli_pipe, mem_ctx,
								     &p->notify.client_hnd,
								     p->notify.change, /* color */
								     p->notify.flags,
								     &reply_result,
								     0, /* reply_type, must be 0 */
								     info,
								     &werr);
			if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(werr)) {
				DEBUG(1,("RouterReplyPrinterEx to client: %s failed: %s\n",
					notify_cli_pipe->srv_name_slash,
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

static bool srv_spoolss_drv_upgrade_printer(const char *drivername)
{
	int len = strlen(drivername);

	if (!len)
		return false;

	DEBUG(10,("srv_spoolss_drv_upgrade_printer: Sending message about driver upgrade [%s]\n",
		drivername));

	messaging_send_buf(smbd_messaging_context(), procid_self(),
			   MSG_PRINTER_DRVUPGRADE,
			   (uint8_t *)drivername, len+1);

	return true;
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
	fstring drivername;
	int snum;
	int n_services = lp_numservices();
	size_t len;

	len = MIN(data->length,sizeof(drivername)-1);
	strncpy(drivername, (const char *)data->data, len);

	DEBUG(10,("do_drv_upgrade_printer: Got message for new driver [%s]\n", drivername ));

	/* Iterate the printer list */

	for (snum=0; snum<n_services; snum++)
	{
		if (lp_snum_ok(snum) && lp_print_ok(snum) )
		{
			WERROR result;
			NT_PRINTER_INFO_LEVEL *printer = NULL;

			result = get_a_printer(NULL, &printer, 2, lp_const_servicename(snum));
			if (!W_ERROR_IS_OK(result))
				continue;

			if (printer && printer->info_2 && !strcmp(drivername, printer->info_2->drivername))
			{
				DEBUG(6,("Updating printer [%s]\n", printer->info_2->printername));

				/* all we care about currently is the change_id */

				result = mod_a_printer(printer, 2);
				if (!W_ERROR_IS_OK(result)) {
					DEBUG(3,("do_drv_upgrade_printer: mod_a_printer() failed with status [%s]\n",
						win_errstr(result)));
				}
			}

			free_a_printer(&printer, 2);
		}
	}

	/* all done */
}

/********************************************************************
 Update the cache for all printq's with a registered client
 connection
 ********************************************************************/

void update_monitored_printq_cache( void )
{
	Printer_entry *printer = printers_list;
	int snum;

	/* loop through all printers and update the cache where
	   client_connected == true */
	while ( printer )
	{
		if ( (printer->printer_type == SPLHND_PRINTER)
			&& printer->notify.client_connected )
		{
			snum = print_queue_snum(printer->sharename);
			print_queue_status( snum, NULL, NULL );
		}

		printer = printer->next;
	}

	return;
}
/********************************************************************
 Send a message to ourself about new driver being installed
 so we can upgrade the information for each printer bound to this
 driver
 ********************************************************************/

static bool srv_spoolss_reset_printerdata(char* drivername)
{
	int len = strlen(drivername);

	if (!len)
		return false;

	DEBUG(10,("srv_spoolss_reset_printerdata: Sending message about resetting printerdata [%s]\n",
		drivername));

	messaging_send_buf(smbd_messaging_context(), procid_self(),
			   MSG_PRINTERDATA_INIT_RESET,
			   (uint8_t *)drivername, len+1);

	return true;
}

/**********************************************************************
 callback to receive a MSG_PRINTERDATA_INIT_RESET message and interate
 over all printers, resetting printer data as neessary
 **********************************************************************/

void reset_all_printerdata(struct messaging_context *msg,
			   void *private_data,
			   uint32_t msg_type,
			   struct server_id server_id,
			   DATA_BLOB *data)
{
	fstring drivername;
	int snum;
	int n_services = lp_numservices();
	size_t len;

	len = MIN( data->length, sizeof(drivername)-1 );
	strncpy( drivername, (const char *)data->data, len );

	DEBUG(10,("reset_all_printerdata: Got message for new driver [%s]\n", drivername ));

	/* Iterate the printer list */

	for ( snum=0; snum<n_services; snum++ )
	{
		if ( lp_snum_ok(snum) && lp_print_ok(snum) )
		{
			WERROR result;
			NT_PRINTER_INFO_LEVEL *printer = NULL;

			result = get_a_printer( NULL, &printer, 2, lp_const_servicename(snum) );
			if ( !W_ERROR_IS_OK(result) )
				continue;

			/*
			 * if the printer is bound to the driver,
			 * then reset to the new driver initdata
			 */

			if ( printer && printer->info_2 && !strcmp(drivername, printer->info_2->drivername) )
			{
				DEBUG(6,("reset_all_printerdata: Updating printer [%s]\n", printer->info_2->printername));

				if ( !set_driver_init(printer, 2) ) {
					DEBUG(5,("reset_all_printerdata: Error resetting printer data for printer [%s], driver [%s]!\n",
						printer->info_2->printername, printer->info_2->drivername));
				}

				result = mod_a_printer( printer, 2 );
				if ( !W_ERROR_IS_OK(result) ) {
					DEBUG(3,("reset_all_printerdata: mod_a_printer() failed!  (%s)\n",
						get_dos_error_msg(result)));
				}
			}

			free_a_printer( &printer, 2 );
		}
	}

	/* all done */

	return;
}

/****************************************************************
 _spoolss_OpenPrinter
****************************************************************/

WERROR _spoolss_OpenPrinter(pipes_struct *p,
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

/********************************************************************
 ********************************************************************/

bool convert_devicemode(const char *printername,
			const struct spoolss_DeviceMode *devmode,
			NT_DEVICEMODE **pp_nt_devmode)
{
	NT_DEVICEMODE *nt_devmode = *pp_nt_devmode;

	/*
	 * Ensure nt_devmode is a valid pointer
	 * as we will be overwriting it.
	 */

	if (nt_devmode == NULL) {
		DEBUG(5, ("convert_devicemode: allocating a generic devmode\n"));
		if ((nt_devmode = construct_nt_devicemode(printername)) == NULL)
			return false;
	}

	fstrcpy(nt_devmode->devicename, devmode->devicename);
	fstrcpy(nt_devmode->formname, devmode->formname);

	nt_devmode->devicename[31] = '\0';
	nt_devmode->formname[31] = '\0';

	nt_devmode->specversion		= devmode->specversion;
	nt_devmode->driverversion	= devmode->driverversion;
	nt_devmode->size		= devmode->size;
	nt_devmode->fields		= devmode->fields;
	nt_devmode->orientation		= devmode->orientation;
	nt_devmode->papersize		= devmode->papersize;
	nt_devmode->paperlength		= devmode->paperlength;
	nt_devmode->paperwidth		= devmode->paperwidth;
	nt_devmode->scale		= devmode->scale;
	nt_devmode->copies		= devmode->copies;
	nt_devmode->defaultsource	= devmode->defaultsource;
	nt_devmode->printquality	= devmode->printquality;
	nt_devmode->color		= devmode->color;
	nt_devmode->duplex		= devmode->duplex;
	nt_devmode->yresolution		= devmode->yresolution;
	nt_devmode->ttoption		= devmode->ttoption;
	nt_devmode->collate		= devmode->collate;

	nt_devmode->logpixels		= devmode->logpixels;
	nt_devmode->bitsperpel		= devmode->bitsperpel;
	nt_devmode->pelswidth		= devmode->pelswidth;
	nt_devmode->pelsheight		= devmode->pelsheight;
	nt_devmode->displayflags	= devmode->displayflags;
	nt_devmode->displayfrequency	= devmode->displayfrequency;
	nt_devmode->icmmethod		= devmode->icmmethod;
	nt_devmode->icmintent		= devmode->icmintent;
	nt_devmode->mediatype		= devmode->mediatype;
	nt_devmode->dithertype		= devmode->dithertype;
	nt_devmode->reserved1		= devmode->reserved1;
	nt_devmode->reserved2		= devmode->reserved2;
	nt_devmode->panningwidth	= devmode->panningwidth;
	nt_devmode->panningheight	= devmode->panningheight;

	/*
	 * Only change private and driverextra if the incoming devmode
	 * has a new one. JRA.
	 */

	if ((devmode->__driverextra_length != 0) && (devmode->driverextra_data.data != NULL)) {
		SAFE_FREE(nt_devmode->nt_dev_private);
		nt_devmode->driverextra = devmode->__driverextra_length;
		if((nt_devmode->nt_dev_private = SMB_MALLOC_ARRAY(uint8_t, nt_devmode->driverextra)) == NULL)
			return false;
		memcpy(nt_devmode->nt_dev_private, devmode->driverextra_data.data, nt_devmode->driverextra);
	}

	*pp_nt_devmode = nt_devmode;

	return true;
}

/****************************************************************
 _spoolss_OpenPrinterEx
****************************************************************/

WERROR _spoolss_OpenPrinterEx(pipes_struct *p,
			      struct spoolss_OpenPrinterEx *r)
{
	int snum;
	Printer_entry *Printer=NULL;

	if (!r->in.printername) {
		return WERR_INVALID_PARAM;
	}

	/* some sanity check because you can open a printer or a print server */
	/* aka: \\server\printer or \\server */

	DEBUGADD(3,("checking name: %s\n", r->in.printername));

	if (!open_printer_hnd(p, r->out.handle, r->in.printername, 0)) {
		ZERO_STRUCTP(r->out.handle);
		return WERR_INVALID_PARAM;
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

		r->in.access_mask &= SPECIFIC_RIGHTS_MASK;

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
			SE_PRIV se_printop = SE_PRINT_OPERATOR;

			if (!lp_ms_add_printer_wizard()) {
				close_printer_handle(p, r->out.handle);
				ZERO_STRUCTP(r->out.handle);
				return WERR_ACCESS_DENIED;
			}

			/* if the user is not root, doesn't have SE_PRINT_OPERATOR privilege,
			   and not a printer admin, then fail */

			if ((p->server_info->utok.uid != sec_initial_uid()) &&
			    !user_has_privileges(p->server_info->ptok,
						 &se_printop ) &&
			    !token_contains_name_in_list(
				    uidtoname(p->server_info->utok.uid),
				    pdb_get_domain(p->server_info->sam_account),
				    NULL,
				    p->server_info->ptok,
				    lp_printer_admin(snum))) {
				close_printer_handle(p, r->out.handle);
				ZERO_STRUCTP(r->out.handle);
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

		if ( !check_access(get_client_fd(), lp_hostsallow(snum), lp_hostsdeny(snum)) ) {
			DEBUG(3, ("access DENIED (hosts allow/deny) for printer open\n"));
			ZERO_STRUCTP(r->out.handle);
			return WERR_ACCESS_DENIED;
		}

		if (!user_ok_token(uidtoname(p->server_info->utok.uid), NULL,
				   p->server_info->ptok, snum) ||
		    !print_access_check(p->server_info, snum,
					r->in.access_mask)) {
			DEBUG(3, ("access DENIED for printer open\n"));
			close_printer_handle(p, r->out.handle);
			ZERO_STRUCTP(r->out.handle);
			return WERR_ACCESS_DENIED;
		}

		if ((r->in.access_mask & SPECIFIC_RIGHTS_MASK)& ~(PRINTER_ACCESS_ADMINISTER|PRINTER_ACCESS_USE)) {
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

	 if ((Printer->printer_type != SPLHND_SERVER) &&
	     r->in.devmode_ctr.devmode) {
		convert_devicemode(Printer->sharename,
				   r->in.devmode_ctr.devmode,
				   &Printer->nt_devmode);
	 }

#if 0	/* JERRY -- I'm doubtful this is really effective */
	/* HACK ALERT!!! Sleep for 1/3 of a second to try trigger a LAN/WAN
	   optimization in Windows 2000 clients  --jerry */

	if ( (r->in.access_mask == PRINTER_ACCESS_ADMINISTER)
		&& (RA_WIN2K == get_remote_arch()) )
	{
		DEBUG(10,("_spoolss_OpenPrinterEx: Enabling LAN/WAN hack for Win2k clients.\n"));
		sys_usleep( 500000 );
	}
#endif

	return WERR_OK;
}

/****************************************************************************
****************************************************************************/

static bool printer_info2_to_nt_printer_info2(struct spoolss_SetPrinterInfo2 *r,
					      NT_PRINTER_INFO_LEVEL_2 *d)
{
	DEBUG(7,("printer_info2_to_nt_printer_info2\n"));

	if (!r || !d) {
		return false;
	}

	d->attributes		= r->attributes;
	d->priority		= r->priority;
	d->default_priority	= r->defaultpriority;
	d->starttime		= r->starttime;
	d->untiltime		= r->untiltime;
	d->status		= r->status;
	d->cjobs		= r->cjobs;

	fstrcpy(d->servername,	r->servername);
	fstrcpy(d->printername, r->printername);
	fstrcpy(d->sharename,	r->sharename);
	fstrcpy(d->portname,	r->portname);
	fstrcpy(d->drivername,	r->drivername);
	slprintf(d->comment, sizeof(d->comment)-1, "%s", r->comment);
	fstrcpy(d->location,	r->location);
	fstrcpy(d->sepfile,	r->sepfile);
	fstrcpy(d->printprocessor, r->printprocessor);
	fstrcpy(d->datatype,	r->datatype);
	fstrcpy(d->parameters,	r->parameters);

	return true;
}

/****************************************************************************
****************************************************************************/

static bool convert_printer_info(struct spoolss_SetPrinterInfoCtr *info_ctr,
				 NT_PRINTER_INFO_LEVEL *printer)
{
	bool ret;

	switch (info_ctr->level) {
	case 2:
		/* allocate memory if needed.  Messy because
		   convert_printer_info is used to update an existing
		   printer or build a new one */

		if (!printer->info_2) {
			printer->info_2 = TALLOC_ZERO_P(printer, NT_PRINTER_INFO_LEVEL_2);
			if (!printer->info_2) {
				DEBUG(0,("convert_printer_info: "
					"talloc() failed!\n"));
				return false;
			}
		}

		ret = printer_info2_to_nt_printer_info2(info_ctr->info.info2,
							printer->info_2);
		printer->info_2->setuptime = time(NULL);
		return ret;
	}

	return false;
}

/****************************************************************
 _spoolss_ClosePrinter
****************************************************************/

WERROR _spoolss_ClosePrinter(pipes_struct *p,
			     struct spoolss_ClosePrinter *r)
{
	Printer_entry *Printer = find_printer_index_by_hnd(p, r->in.handle);

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

WERROR _spoolss_DeletePrinter(pipes_struct *p,
			      struct spoolss_DeletePrinter *r)
{
	Printer_entry *Printer = find_printer_index_by_hnd(p, r->in.handle);
	WERROR result;

	if (Printer && Printer->document_started) {
		struct spoolss_EndDocPrinter e;

		e.in.handle = r->in.handle;

		_spoolss_EndDocPrinter(p, &e);
	}

	result = delete_printer_handle(p, r->in.handle);

	update_c_setprinter(false);

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

WERROR _spoolss_DeletePrinterDriver(pipes_struct *p,
				    struct spoolss_DeletePrinterDriver *r)
{

	struct spoolss_DriverInfo8 *info = NULL;
	struct spoolss_DriverInfo8 *info_win2k = NULL;
	int				version;
	WERROR				status;
	WERROR				status_win2k = WERR_ACCESS_DENIED;
	SE_PRIV                         se_printop = SE_PRINT_OPERATOR;

	/* if the user is not root, doesn't have SE_PRINT_OPERATOR privilege,
	   and not a printer admin, then fail */

	if ( (p->server_info->utok.uid != sec_initial_uid())
		&& !user_has_privileges(p->server_info->ptok, &se_printop )
		&& !token_contains_name_in_list(
			uidtoname(p->server_info->utok.uid),
			pdb_get_domain(p->server_info->sam_account),
			NULL,
			p->server_info->ptok,
			lp_printer_admin(-1)) )
	{
		return WERR_ACCESS_DENIED;
	}

	/* check that we have a valid driver name first */

	if ((version = get_version_id(r->in.architecture)) == -1)
		return WERR_INVALID_ENVIRONMENT;

	if (!W_ERROR_IS_OK(get_a_printer_driver(p->mem_ctx, &info, r->in.driver,
						r->in.architecture,
						version)))
	{
		/* try for Win2k driver if "Windows NT x86" */

		if ( version == 2 ) {
			version = 3;
			if (!W_ERROR_IS_OK(get_a_printer_driver(p->mem_ctx,
								&info,
								r->in.driver,
								r->in.architecture,
								version))) {
				status = WERR_UNKNOWN_PRINTER_DRIVER;
				goto done;
			}
		}
		/* otherwise it was a failure */
		else {
			status = WERR_UNKNOWN_PRINTER_DRIVER;
			goto done;
		}

	}

	if (printer_driver_in_use(info)) {
		status = WERR_PRINTER_DRIVER_IN_USE;
		goto done;
	}

	if ( version == 2 )
	{
		if (W_ERROR_IS_OK(get_a_printer_driver(p->mem_ctx,
						       &info_win2k,
						       r->in.driver,
						       r->in.architecture, 3)))
		{
			/* if we get to here, we now have 2 driver info structures to remove */
			/* remove the Win2k driver first*/

			status_win2k = delete_printer_driver(
				p, info_win2k, 3, false);
			free_a_printer_driver(info_win2k);

			/* this should not have failed---if it did, report to client */
			if ( !W_ERROR_IS_OK(status_win2k) )
			{
				status = status_win2k;
				goto done;
			}
		}
	}

	status = delete_printer_driver(p, info, version, false);

	/* if at least one of the deletes succeeded return OK */

	if ( W_ERROR_IS_OK(status) || W_ERROR_IS_OK(status_win2k) )
		status = WERR_OK;

done:
	free_a_printer_driver(info);

	return status;
}

/****************************************************************
 _spoolss_DeletePrinterDriverEx
****************************************************************/

WERROR _spoolss_DeletePrinterDriverEx(pipes_struct *p,
				      struct spoolss_DeletePrinterDriverEx *r)
{
	struct spoolss_DriverInfo8	*info = NULL;
	struct spoolss_DriverInfo8	*info_win2k = NULL;
	int				version;
	bool				delete_files;
	WERROR				status;
	WERROR				status_win2k = WERR_ACCESS_DENIED;
	SE_PRIV                         se_printop = SE_PRINT_OPERATOR;

	/* if the user is not root, doesn't have SE_PRINT_OPERATOR privilege,
	   and not a printer admin, then fail */

	if ( (p->server_info->utok.uid != sec_initial_uid())
		&& !user_has_privileges(p->server_info->ptok, &se_printop )
		&& !token_contains_name_in_list(
			uidtoname(p->server_info->utok.uid),
			pdb_get_domain(p->server_info->sam_account),
			NULL,
			p->server_info->ptok, lp_printer_admin(-1)) )
	{
		return WERR_ACCESS_DENIED;
	}

	/* check that we have a valid driver name first */
	if ((version = get_version_id(r->in.architecture)) == -1) {
		/* this is what NT returns */
		return WERR_INVALID_ENVIRONMENT;
	}

	if (r->in.delete_flags & DPD_DELETE_SPECIFIC_VERSION)
		version = r->in.version;

	status = get_a_printer_driver(p->mem_ctx, &info, r->in.driver,
				      r->in.architecture, version);

	if ( !W_ERROR_IS_OK(status) )
	{
		/*
		 * if the client asked for a specific version,
		 * or this is something other than Windows NT x86,
		 * then we've failed
		 */

		if ( (r->in.delete_flags & DPD_DELETE_SPECIFIC_VERSION) || (version !=2) )
			goto done;

		/* try for Win2k driver if "Windows NT x86" */

		version = 3;
		if (!W_ERROR_IS_OK(get_a_printer_driver(p->mem_ctx, &info, r->in.driver,
							r->in.architecture,
							version))) {
			status = WERR_UNKNOWN_PRINTER_DRIVER;
			goto done;
		}
	}

	if (printer_driver_in_use(info)) {
		status = WERR_PRINTER_DRIVER_IN_USE;
		goto done;
	}

	/*
	 * we have a couple of cases to consider.
	 * (1) Are any files in use?  If so and DPD_DELTE_ALL_FILE is set,
	 *     then the delete should fail if **any** files overlap with
	 *     other drivers
	 * (2) If DPD_DELTE_UNUSED_FILES is sert, then delete all
	 *     non-overlapping files
	 * (3) If neither DPD_DELTE_ALL_FILE nor DPD_DELTE_ALL_FILES
	 *     is set, the do not delete any files
	 * Refer to MSDN docs on DeletePrinterDriverEx() for details.
	 */

	delete_files = r->in.delete_flags & (DPD_DELETE_ALL_FILES|DPD_DELETE_UNUSED_FILES);

	/* fail if any files are in use and DPD_DELETE_ALL_FILES is set */

	if (delete_files && printer_driver_files_in_use(info, info) & (r->in.delete_flags & DPD_DELETE_ALL_FILES)) {
		/* no idea of the correct error here */
		status = WERR_ACCESS_DENIED;
		goto done;
	}


	/* also check for W32X86/3 if necessary; maybe we already have? */

	if ( (version == 2) && ((r->in.delete_flags & DPD_DELETE_SPECIFIC_VERSION) != DPD_DELETE_SPECIFIC_VERSION)  ) {
		if (W_ERROR_IS_OK(get_a_printer_driver(p->mem_ctx, &info_win2k,
						       r->in.driver,
						       r->in.architecture, 3)))
		{

			if (delete_files && printer_driver_files_in_use(info, info_win2k) & (r->in.delete_flags & DPD_DELETE_ALL_FILES) ) {
				/* no idea of the correct error here */
				free_a_printer_driver(info_win2k);
				status = WERR_ACCESS_DENIED;
				goto done;
			}

			/* if we get to here, we now have 2 driver info structures to remove */
			/* remove the Win2k driver first*/

			status_win2k = delete_printer_driver(
				p, info_win2k, 3, delete_files);
			free_a_printer_driver(info_win2k);

			/* this should not have failed---if it did, report to client */

			if ( !W_ERROR_IS_OK(status_win2k) )
				goto done;
		}
	}

	status = delete_printer_driver(p, info, version, delete_files);

	if ( W_ERROR_IS_OK(status) || W_ERROR_IS_OK(status_win2k) )
		status = WERR_OK;
done:
	free_a_printer_driver(info);

	return status;
}


/****************************************************************************
 Internal routine for removing printerdata
 ***************************************************************************/

static WERROR delete_printer_dataex( NT_PRINTER_INFO_LEVEL *printer, const char *key, const char *value )
{
	return delete_printer_data( printer->info_2, key, value );
}

/****************************************************************************
 Internal routine for storing printerdata
 ***************************************************************************/

WERROR set_printer_dataex(NT_PRINTER_INFO_LEVEL *printer,
			  const char *key, const char *value,
			  uint32_t type, uint8_t *data, int real_len)
{
	/* the registry objects enforce uniqueness based on value name */

	return add_printer_data( printer->info_2, key, value, type, data, real_len );
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

		ndr_err = ndr_push_struct_blob(&blob, mem_ctx, NULL, &os,
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

		data->string = talloc_strdup(mem_ctx, "Windows NT x86");
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

WERROR _spoolss_GetPrinterData(pipes_struct *p,
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

	if ( is_zero_addr((struct sockaddr *)client_ss) ) {
		if ( !resolve_name( remote_machine, &rm_addr, 0x20, false) ) {
			DEBUG(2,("spoolss_connect_to_client: Can't resolve address for %s\n", remote_machine));
			return false;
		}

		if (ismyaddr((struct sockaddr *)&rm_addr)) {
			DEBUG(0,("spoolss_connect_to_client: Machine %s is one of our addresses. Cannot add to ourselves.\n", remote_machine));
			return false;
		}
	} else {
		char addr[INET6_ADDRSTRLEN];
		rm_addr = *client_ss;
		print_sockaddr(addr, sizeof(addr), &rm_addr);
		DEBUG(5,("spoolss_connect_to_client: Using address %s (no name resolution necessary)\n",
			addr));
	}

	/* setup the connection */

	ret = cli_full_connection( &the_cli, global_myname(), remote_machine,
		&rm_addr, 0, "IPC$", "IPC",
		"", /* username */
		"", /* domain */
		"", /* password */
		0, lp_client_signing(), NULL );

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
					uint32_t localprinter, uint32_t type,
					struct policy_handle *handle,
					struct sockaddr_storage *client_ss)
{
	WERROR result;
	NTSTATUS status;

	/*
	 * If it's the first connection, contact the client
	 * and connect to the IPC$ share anonymously
	 */
	if (smb_connections==0) {
		fstring unix_printer;

		fstrcpy(unix_printer, printer+2); /* the +2 is to strip the leading 2 backslashs */

		if ( !spoolss_connect_to_client( &notify_cli_pipe, client_ss, unix_printer ))
			return false;

		messaging_register(smbd_messaging_context(), NULL,
				   MSG_PRINTER_NOTIFY2,
				   receive_notify2_message_list);
		/* Tell the connections db we're now interested in printer
		 * notify messages. */
		register_message_flags(true, FLAG_MSG_PRINT_NOTIFY);
	}

	/*
	 * Tell the specific printing tdb we want messages for this printer
	 * by registering our PID.
	 */

	if (!print_notify_register_pid(snum))
		DEBUG(0,("print_notify_register_pid: Failed to register our pid for printer %s\n", printer ));

	smb_connections++;

	status = rpccli_spoolss_ReplyOpenPrinter(notify_cli_pipe, talloc_tos(),
						 printer,
						 localprinter,
						 type,
						 0,
						 NULL,
						 handle,
						 &result);
	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(result))
		DEBUG(5,("srv_spoolss_reply_open_printer: Client RPC returned [%s]\n",
			win_errstr(result)));

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

WERROR _spoolss_RemoteFindFirstPrinterChangeNotifyEx(pipes_struct *p,
						     struct spoolss_RemoteFindFirstPrinterChangeNotifyEx *r)
{
	int snum = -1;
	struct spoolss_NotifyOption *option = r->in.notify_options;
	struct sockaddr_storage client_ss;

	/* store the notify value in the printer struct */

	Printer_entry *Printer = find_printer_index_by_hnd(p, r->in.handle);

	if (!Printer) {
		DEBUG(2,("_spoolss_RemoteFindFirstPrinterChangeNotifyEx: "
			"Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	Printer->notify.flags		= r->in.flags;
	Printer->notify.options		= r->in.options;
	Printer->notify.printerlocal	= r->in.printer_local;

	TALLOC_FREE(Printer->notify.option);
	Printer->notify.option = dup_spoolss_NotifyOption(Printer, option);

	fstrcpy(Printer->notify.localmachine, r->in.local_machine);

	/* Connect to the client machine and send a ReplyOpenPrinter */

	if ( Printer->printer_type == SPLHND_SERVER)
		snum = -1;
	else if ( (Printer->printer_type == SPLHND_PRINTER) &&
			!get_printer_snum(p, r->in.handle, &snum, NULL) )
		return WERR_BADFID;

	if (!interpret_string_addr(&client_ss, p->client_address,
				   AI_NUMERICHOST)) {
		return WERR_SERVER_UNAVAILABLE;
	}

	if(!srv_spoolss_replyopenprinter(snum, Printer->notify.localmachine,
					Printer->notify.printerlocal, 1,
					&Printer->notify.client_hnd, &client_ss))
		return WERR_SERVER_UNAVAILABLE;

	Printer->notify.client_connected = true;

	return WERR_OK;
}

/*******************************************************************
 * fill a notify_info_data with the servername
 ********************************************************************/

void spoolss_notify_server_name(int snum,
				       struct spoolss_Notify *data,
				       print_queue_struct *queue,
				       NT_PRINTER_INFO_LEVEL *printer,
				       TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, printer->info_2->servername);
}

/*******************************************************************
 * fill a notify_info_data with the printername (not including the servername).
 ********************************************************************/

void spoolss_notify_printer_name(int snum,
					struct spoolss_Notify *data,
					print_queue_struct *queue,
					NT_PRINTER_INFO_LEVEL *printer,
					TALLOC_CTX *mem_ctx)
{
	/* the notify name should not contain the \\server\ part */
	char *p = strrchr(printer->info_2->printername, '\\');

	if (!p) {
		p = printer->info_2->printername;
	} else {
		p++;
	}

	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, p);
}

/*******************************************************************
 * fill a notify_info_data with the servicename
 ********************************************************************/

void spoolss_notify_share_name(int snum,
				      struct spoolss_Notify *data,
				      print_queue_struct *queue,
				      NT_PRINTER_INFO_LEVEL *printer,
				      TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, lp_servicename(snum));
}

/*******************************************************************
 * fill a notify_info_data with the port name
 ********************************************************************/

void spoolss_notify_port_name(int snum,
				     struct spoolss_Notify *data,
				     print_queue_struct *queue,
				     NT_PRINTER_INFO_LEVEL *printer,
				     TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, printer->info_2->portname);
}

/*******************************************************************
 * fill a notify_info_data with the printername
 * but it doesn't exist, have to see what to do
 ********************************************************************/

void spoolss_notify_driver_name(int snum,
				       struct spoolss_Notify *data,
				       print_queue_struct *queue,
				       NT_PRINTER_INFO_LEVEL *printer,
				       TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, printer->info_2->drivername);
}

/*******************************************************************
 * fill a notify_info_data with the comment
 ********************************************************************/

void spoolss_notify_comment(int snum,
				   struct spoolss_Notify *data,
				   print_queue_struct *queue,
				   NT_PRINTER_INFO_LEVEL *printer,
				   TALLOC_CTX *mem_ctx)
{
	char *p;

	if (*printer->info_2->comment == '\0') {
		p = lp_comment(snum);
	} else {
		p = printer->info_2->comment;
	}

	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, printer->info_2->comment);
}

/*******************************************************************
 * fill a notify_info_data with the comment
 * location = "Room 1, floor 2, building 3"
 ********************************************************************/

void spoolss_notify_location(int snum,
				    struct spoolss_Notify *data,
				    print_queue_struct *queue,
				    NT_PRINTER_INFO_LEVEL *printer,
				    TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, printer->info_2->location);
}

/*******************************************************************
 * fill a notify_info_data with the device mode
 * jfm:xxxx don't to it for know but that's a real problem !!!
 ********************************************************************/

static void spoolss_notify_devmode(int snum,
				   struct spoolss_Notify *data,
				   print_queue_struct *queue,
				   NT_PRINTER_INFO_LEVEL *printer,
				   TALLOC_CTX *mem_ctx)
{
	/* for a dummy implementation we have to zero the fields */
	SETUP_SPOOLSS_NOTIFY_DATA_DEVMODE(data, NULL);
}

/*******************************************************************
 * fill a notify_info_data with the separator file name
 ********************************************************************/

void spoolss_notify_sepfile(int snum,
				   struct spoolss_Notify *data,
				   print_queue_struct *queue,
				   NT_PRINTER_INFO_LEVEL *printer,
				   TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, printer->info_2->sepfile);
}

/*******************************************************************
 * fill a notify_info_data with the print processor
 * jfm:xxxx return always winprint to indicate we don't do anything to it
 ********************************************************************/

void spoolss_notify_print_processor(int snum,
					   struct spoolss_Notify *data,
					   print_queue_struct *queue,
					   NT_PRINTER_INFO_LEVEL *printer,
					   TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, printer->info_2->printprocessor);
}

/*******************************************************************
 * fill a notify_info_data with the print processor options
 * jfm:xxxx send an empty string
 ********************************************************************/

void spoolss_notify_parameters(int snum,
				      struct spoolss_Notify *data,
				      print_queue_struct *queue,
				      NT_PRINTER_INFO_LEVEL *printer,
				      TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, printer->info_2->parameters);
}

/*******************************************************************
 * fill a notify_info_data with the data type
 * jfm:xxxx always send RAW as data type
 ********************************************************************/

void spoolss_notify_datatype(int snum,
				    struct spoolss_Notify *data,
				    print_queue_struct *queue,
				    NT_PRINTER_INFO_LEVEL *printer,
				    TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, printer->info_2->datatype);
}

/*******************************************************************
 * fill a notify_info_data with the security descriptor
 * jfm:xxxx send an null pointer to say no security desc
 * have to implement security before !
 ********************************************************************/

static void spoolss_notify_security_desc(int snum,
					 struct spoolss_Notify *data,
					 print_queue_struct *queue,
					 NT_PRINTER_INFO_LEVEL *printer,
					 TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_SECDESC(data,
					  printer->info_2->secdesc_buf->sd_size,
					  printer->info_2->secdesc_buf->sd);
}

/*******************************************************************
 * fill a notify_info_data with the attributes
 * jfm:xxxx a samba printer is always shared
 ********************************************************************/

void spoolss_notify_attributes(int snum,
				      struct spoolss_Notify *data,
				      print_queue_struct *queue,
				      NT_PRINTER_INFO_LEVEL *printer,
				      TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, printer->info_2->attributes);
}

/*******************************************************************
 * fill a notify_info_data with the priority
 ********************************************************************/

static void spoolss_notify_priority(int snum,
				    struct spoolss_Notify *data,
				    print_queue_struct *queue,
				    NT_PRINTER_INFO_LEVEL *printer,
				    TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, printer->info_2->priority);
}

/*******************************************************************
 * fill a notify_info_data with the default priority
 ********************************************************************/

static void spoolss_notify_default_priority(int snum,
					    struct spoolss_Notify *data,
					    print_queue_struct *queue,
					    NT_PRINTER_INFO_LEVEL *printer,
					    TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, printer->info_2->default_priority);
}

/*******************************************************************
 * fill a notify_info_data with the start time
 ********************************************************************/

static void spoolss_notify_start_time(int snum,
				      struct spoolss_Notify *data,
				      print_queue_struct *queue,
				      NT_PRINTER_INFO_LEVEL *printer,
				      TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, printer->info_2->starttime);
}

/*******************************************************************
 * fill a notify_info_data with the until time
 ********************************************************************/

static void spoolss_notify_until_time(int snum,
				      struct spoolss_Notify *data,
				      print_queue_struct *queue,
				      NT_PRINTER_INFO_LEVEL *printer,
				      TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, printer->info_2->untiltime);
}

/*******************************************************************
 * fill a notify_info_data with the status
 ********************************************************************/

static void spoolss_notify_status(int snum,
				  struct spoolss_Notify *data,
				  print_queue_struct *queue,
				  NT_PRINTER_INFO_LEVEL *printer,
				  TALLOC_CTX *mem_ctx)
{
	print_status_struct status;

	print_queue_length(snum, &status);
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, status.status);
}

/*******************************************************************
 * fill a notify_info_data with the number of jobs queued
 ********************************************************************/

void spoolss_notify_cjobs(int snum,
				 struct spoolss_Notify *data,
				 print_queue_struct *queue,
				 NT_PRINTER_INFO_LEVEL *printer,
				 TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, print_queue_length(snum, NULL));
}

/*******************************************************************
 * fill a notify_info_data with the average ppm
 ********************************************************************/

static void spoolss_notify_average_ppm(int snum,
				       struct spoolss_Notify *data,
				       print_queue_struct *queue,
				       NT_PRINTER_INFO_LEVEL *printer,
				       TALLOC_CTX *mem_ctx)
{
	/* always respond 8 pages per minutes */
	/* a little hard ! */
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, printer->info_2->averageppm);
}

/*******************************************************************
 * fill a notify_info_data with username
 ********************************************************************/

static void spoolss_notify_username(int snum,
				    struct spoolss_Notify *data,
				    print_queue_struct *queue,
				    NT_PRINTER_INFO_LEVEL *printer,
				    TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, queue->fs_user);
}

/*******************************************************************
 * fill a notify_info_data with job status
 ********************************************************************/

static void spoolss_notify_job_status(int snum,
				      struct spoolss_Notify *data,
				      print_queue_struct *queue,
				      NT_PRINTER_INFO_LEVEL *printer,
				      TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, nt_printj_status(queue->status));
}

/*******************************************************************
 * fill a notify_info_data with job name
 ********************************************************************/

static void spoolss_notify_job_name(int snum,
				    struct spoolss_Notify *data,
				    print_queue_struct *queue,
				    NT_PRINTER_INFO_LEVEL *printer,
				    TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_STRING(data, queue->fs_file);
}

/*******************************************************************
 * fill a notify_info_data with job status
 ********************************************************************/

static void spoolss_notify_job_status_string(int snum,
					     struct spoolss_Notify *data,
					     print_queue_struct *queue,
					     NT_PRINTER_INFO_LEVEL *printer,
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

static void spoolss_notify_job_time(int snum,
				    struct spoolss_Notify *data,
				    print_queue_struct *queue,
				    NT_PRINTER_INFO_LEVEL *printer,
				    TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, 0);
}

/*******************************************************************
 * fill a notify_info_data with job size
 ********************************************************************/

static void spoolss_notify_job_size(int snum,
				    struct spoolss_Notify *data,
				    print_queue_struct *queue,
				    NT_PRINTER_INFO_LEVEL *printer,
				    TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, queue->size);
}

/*******************************************************************
 * fill a notify_info_data with page info
 ********************************************************************/
static void spoolss_notify_total_pages(int snum,
				struct spoolss_Notify *data,
				print_queue_struct *queue,
				NT_PRINTER_INFO_LEVEL *printer,
				TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, queue->page_count);
}

/*******************************************************************
 * fill a notify_info_data with pages printed info.
 ********************************************************************/
static void spoolss_notify_pages_printed(int snum,
				struct spoolss_Notify *data,
				print_queue_struct *queue,
				NT_PRINTER_INFO_LEVEL *printer,
				TALLOC_CTX *mem_ctx)
{
	/* Add code when back-end tracks this */
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, 0);
}

/*******************************************************************
 Fill a notify_info_data with job position.
 ********************************************************************/

static void spoolss_notify_job_position(int snum,
					struct spoolss_Notify *data,
					print_queue_struct *queue,
					NT_PRINTER_INFO_LEVEL *printer,
					TALLOC_CTX *mem_ctx)
{
	SETUP_SPOOLSS_NOTIFY_DATA_INTEGER(data, queue->job);
}

/*******************************************************************
 Fill a notify_info_data with submitted time.
 ********************************************************************/

static void spoolss_notify_submitted_time(int snum,
					  struct spoolss_Notify *data,
					  print_queue_struct *queue,
					  NT_PRINTER_INFO_LEVEL *printer,
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
	void (*fn) (int snum, struct spoolss_Notify *data,
		    print_queue_struct *queue,
		    NT_PRINTER_INFO_LEVEL *printer, TALLOC_CTX *mem_ctx);
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

static uint32_t variable_type_of_notify_info_data(enum spoolss_NotifyType type,
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

	return 0;
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

void construct_info_data(struct spoolss_Notify *info_data,
			 enum spoolss_NotifyType type,
			 uint16_t field,
			 int id)
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

static bool construct_notify_printer_info(Printer_entry *print_hnd,
					  struct spoolss_NotifyInfo *info,
					  int snum,
					  const struct spoolss_NotifyOptionType *option_type,
					  uint32_t id,
					  TALLOC_CTX *mem_ctx)
{
	int field_num,j;
	enum spoolss_NotifyType type;
	uint16_t field;

	struct spoolss_Notify *current_data;
	NT_PRINTER_INFO_LEVEL *printer = NULL;
	print_queue_struct *queue=NULL;

	type = option_type->type;

	DEBUG(4,("construct_notify_printer_info: Notify type: [%s], number of notify info: [%d] on printer: [%s]\n",
		(type == PRINTER_NOTIFY_TYPE ? "PRINTER_NOTIFY_TYPE" : "JOB_NOTIFY_TYPE"),
		option_type->count, lp_servicename(snum)));

	if (!W_ERROR_IS_OK(get_a_printer(print_hnd, &printer, 2, lp_const_servicename(snum))))
		return false;

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
			free_a_printer(&printer, 2);
			return false;
		}

		current_data = &info->notifies[info->count];

		construct_info_data(current_data, type, field, id);

		DEBUG(10,("construct_notify_printer_info: calling [%s]  snum=%d  printername=[%s])\n",
				notify_info_data_table[j].name, snum, printer->info_2->printername ));

		notify_info_data_table[j].fn(snum, current_data, queue,
					     printer, mem_ctx);

		info->count++;
	}

	free_a_printer(&printer, 2);
	return true;
}

/*******************************************************************
 *
 * fill a notify_info struct with info asked
 *
 ********************************************************************/

static bool construct_notify_jobs_info(print_queue_struct *queue,
				       struct spoolss_NotifyInfo *info,
				       NT_PRINTER_INFO_LEVEL *printer,
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
		notify_info_data_table[j].fn(snum, current_data, queue,
					     printer, mem_ctx);
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

static WERROR printserver_notify_info(pipes_struct *p,
				      struct policy_handle *hnd,
				      struct spoolss_NotifyInfo *info,
				      TALLOC_CTX *mem_ctx)
{
	int snum;
	Printer_entry *Printer = find_printer_index_by_hnd(p, hnd);
	int n_services=lp_numservices();
	int i;
	struct spoolss_NotifyOption *option;
	struct spoolss_NotifyOptionType option_type;

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

		for (snum=0; snum<n_services; snum++)
		{
			if ( lp_browseable(snum) && lp_snum_ok(snum) && lp_print_ok(snum) )
				construct_notify_printer_info ( Printer, info, snum, &option_type, snum, mem_ctx );
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

static WERROR printer_notify_info(pipes_struct *p, struct policy_handle *hnd,
				  struct spoolss_NotifyInfo *info,
				  TALLOC_CTX *mem_ctx)
{
	int snum;
	Printer_entry *Printer = find_printer_index_by_hnd(p, hnd);
	int i;
	uint32_t id;
	struct spoolss_NotifyOption *option;
	struct spoolss_NotifyOptionType option_type;
	int count,j;
	print_queue_struct *queue=NULL;
	print_status_struct status;

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

	get_printer_snum(p, hnd, &snum, NULL);

	for (i=0; i<option->count; i++) {
		option_type = option->types[i];

		switch (option_type.type) {
		case PRINTER_NOTIFY_TYPE:
			if(construct_notify_printer_info(Printer, info, snum,
							 &option_type, id,
							 mem_ctx))
				id--;
			break;

		case JOB_NOTIFY_TYPE: {
			NT_PRINTER_INFO_LEVEL *printer = NULL;

			count = print_queue_status(snum, &queue, &status);

			if (!W_ERROR_IS_OK(get_a_printer(Printer, &printer, 2, lp_const_servicename(snum))))
				goto done;

			for (j=0; j<count; j++) {
				construct_notify_jobs_info(&queue[j], info,
							   printer, snum,
							   &option_type,
							   queue[j].job,
							   mem_ctx);
			}

			free_a_printer(&printer, 2);

		done:
			SAFE_FREE(queue);
			break;
		}
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
	return WERR_OK;
}

/****************************************************************
 _spoolss_RouterRefreshPrinterChangeNotify
****************************************************************/

WERROR _spoolss_RouterRefreshPrinterChangeNotify(pipes_struct *p,
						 struct spoolss_RouterRefreshPrinterChangeNotify *r)
{
	struct spoolss_NotifyInfo *info;

	Printer_entry *Printer = find_printer_index_by_hnd(p, r->in.handle);
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
	 *	informations even when _NOTHING_ has changed.
	 */

	/* We need to keep track of the change value to send back in
           RRPCN replies otherwise our updates are ignored. */

	Printer->notify.fnpcn = true;

	if (Printer->notify.client_connected) {
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
 * construct_printer_info_0
 * fill a printer_info_0 struct
 ********************************************************************/

static WERROR construct_printer_info0(TALLOC_CTX *mem_ctx,
				      const NT_PRINTER_INFO_LEVEL *ntprinter,
				      struct spoolss_PrinterInfo0 *r,
				      int snum)
{
	int count;
	counter_printer_0 *session_counter;
	time_t setuptime;
	print_status_struct status;

	r->printername		= talloc_strdup(mem_ctx, ntprinter->info_2->printername);
	W_ERROR_HAVE_NO_MEMORY(r->printername);

	r->servername		= talloc_strdup(mem_ctx, ntprinter->info_2->servername);
	W_ERROR_HAVE_NO_MEMORY(r->servername);

	count = print_queue_length(snum, &status);

	/* check if we already have a counter for this printer */
	for (session_counter = counter_list; session_counter; session_counter = session_counter->next) {
		if (session_counter->snum == snum)
			break;
	}

	/* it's the first time, add it to the list */
	if (session_counter == NULL) {
		session_counter = SMB_MALLOC_P(counter_printer_0);
		W_ERROR_HAVE_NO_MEMORY(session_counter);
		ZERO_STRUCTP(session_counter);
		session_counter->snum		= snum;
		session_counter->counter	= 0;
		DLIST_ADD(counter_list, session_counter);
	}

	/* increment it */
	session_counter->counter++;

	r->cjobs			= count;
	r->total_jobs			= 0;
	r->total_bytes			= 0;

	setuptime = (time_t)ntprinter->info_2->setuptime;

	init_systemtime(&r->time, gmtime(&setuptime));

	/* JFM:
	 * the global_counter should be stored in a TDB as it's common to all the clients
	 * and should be zeroed on samba startup
	 */
	r->global_counter		= session_counter->counter;
	r->total_pages			= 0;
	/* in 2.2 we reported ourselves as 0x0004 and 0x0565 */
	r->version			= 0x0005; 	/* NT 5 */
	r->free_build			= 0x0893; 	/* build 2195 */
	r->spooling			= 0;
	r->max_spooling			= 0;
	r->session_counter		= session_counter->counter;
	r->num_error_out_of_paper	= 0x0;
	r->num_error_not_ready		= 0x0;		/* number of print failure */
	r->job_error			= 0x0;
	r->number_of_processors		= 0x1;
	r->processor_type		= PROCESSOR_INTEL_PENTIUM; /* 586 Pentium ? */
	r->high_part_total_bytes	= 0x0;
	r->change_id			= ntprinter->info_2->changeid; /* ChangeID in milliseconds*/
	r->last_error			= WERR_OK;
	r->status			= nt_printq_status(status.status);
	r->enumerate_network_printers	= 0x0;
	r->c_setprinter			= get_c_setprinter(); /* monotonically increasing sum of delta printer counts */
	r->processor_architecture	= 0x0;
	r->processor_level		= 0x6; 		/* 6  ???*/
	r->ref_ic			= 0;
	r->reserved2			= 0;
	r->reserved3			= 0;

	return WERR_OK;
}

/****************************************************************************
 Convert an NT_DEVICEMODE to a spoolss_DeviceMode structure.  Both pointers
 should be valid upon entry
****************************************************************************/

static WERROR convert_nt_devicemode(TALLOC_CTX *mem_ctx,
				    struct spoolss_DeviceMode *r,
				    const NT_DEVICEMODE *ntdevmode)
{
	if (!r || !ntdevmode) {
		return WERR_INVALID_PARAM;
	}

	r->devicename		= talloc_strdup(mem_ctx, ntdevmode->devicename);
	W_ERROR_HAVE_NO_MEMORY(r->devicename);

	r->specversion		= ntdevmode->specversion;
	r->driverversion	= ntdevmode->driverversion;
	r->size			= ntdevmode->size;
	r->__driverextra_length	= ntdevmode->driverextra;
	r->fields		= ntdevmode->fields;

	r->orientation		= ntdevmode->orientation;
	r->papersize		= ntdevmode->papersize;
	r->paperlength		= ntdevmode->paperlength;
	r->paperwidth		= ntdevmode->paperwidth;
	r->scale		= ntdevmode->scale;
	r->copies		= ntdevmode->copies;
	r->defaultsource	= ntdevmode->defaultsource;
	r->printquality		= ntdevmode->printquality;
	r->color		= ntdevmode->color;
	r->duplex		= ntdevmode->duplex;
	r->yresolution		= ntdevmode->yresolution;
	r->ttoption		= ntdevmode->ttoption;
	r->collate		= ntdevmode->collate;

	r->formname		= talloc_strdup(mem_ctx, ntdevmode->formname);
	W_ERROR_HAVE_NO_MEMORY(r->formname);

	/* all 0 below are values that have not been set in the old parsing/copy
	 * function, maybe they should... - gd */

	r->logpixels		= 0;
	r->bitsperpel		= 0;
	r->pelswidth		= 0;
	r->pelsheight		= 0;
	r->displayflags		= 0;
	r->displayfrequency	= 0;
	r->icmmethod		= ntdevmode->icmmethod;
	r->icmintent		= ntdevmode->icmintent;
	r->mediatype		= ntdevmode->mediatype;
	r->dithertype		= ntdevmode->dithertype;
	r->reserved1		= 0;
	r->reserved2		= 0;
	r->panningwidth		= 0;
	r->panningheight	= 0;

	if (ntdevmode->nt_dev_private != NULL) {
		r->driverextra_data = data_blob_talloc(mem_ctx,
			ntdevmode->nt_dev_private,
			ntdevmode->driverextra);
		W_ERROR_HAVE_NO_MEMORY(r->driverextra_data.data);
	}

	return WERR_OK;
}


/****************************************************************************
 Create a spoolss_DeviceMode struct. Returns talloced memory.
****************************************************************************/

struct spoolss_DeviceMode *construct_dev_mode(TALLOC_CTX *mem_ctx,
					      const char *servicename)
{
	WERROR result;
	NT_PRINTER_INFO_LEVEL 	*printer = NULL;
	struct spoolss_DeviceMode *devmode = NULL;

	DEBUG(7,("construct_dev_mode\n"));

	DEBUGADD(8,("getting printer characteristics\n"));

	if (!W_ERROR_IS_OK(get_a_printer(NULL, &printer, 2, servicename)))
		return NULL;

	if (!printer->info_2->devmode) {
		DEBUG(5, ("BONG! There was no device mode!\n"));
		goto done;
	}

	devmode = TALLOC_ZERO_P(mem_ctx, struct spoolss_DeviceMode);
	if (!devmode) {
		DEBUG(2,("construct_dev_mode: talloc fail.\n"));
		goto done;
	}

	DEBUGADD(8,("loading DEVICEMODE\n"));

	result = convert_nt_devicemode(mem_ctx, devmode, printer->info_2->devmode);
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(devmode);
	}

done:
	free_a_printer(&printer,2);

	return devmode;
}

/********************************************************************
 * construct_printer_info1
 * fill a spoolss_PrinterInfo1 struct
********************************************************************/

static WERROR construct_printer_info1(TALLOC_CTX *mem_ctx,
				      const NT_PRINTER_INFO_LEVEL *ntprinter,
				      uint32_t flags,
				      struct spoolss_PrinterInfo1 *r,
				      int snum)
{
	r->flags		= flags;

	r->description		= talloc_asprintf(mem_ctx, "%s,%s,%s",
						  ntprinter->info_2->printername,
						  ntprinter->info_2->drivername,
						  ntprinter->info_2->location);
	W_ERROR_HAVE_NO_MEMORY(r->description);

	if (*ntprinter->info_2->comment == '\0') {
		r->comment	= talloc_strdup(mem_ctx, lp_comment(snum));
	} else {
		r->comment	= talloc_strdup(mem_ctx, ntprinter->info_2->comment); /* saved comment */
	}
	W_ERROR_HAVE_NO_MEMORY(r->comment);

	r->name			= talloc_strdup(mem_ctx, ntprinter->info_2->printername);
	W_ERROR_HAVE_NO_MEMORY(r->name);

	return WERR_OK;
}

/********************************************************************
 * construct_printer_info2
 * fill a spoolss_PrinterInfo2 struct
********************************************************************/

static WERROR construct_printer_info2(TALLOC_CTX *mem_ctx,
				      const NT_PRINTER_INFO_LEVEL *ntprinter,
				      struct spoolss_PrinterInfo2 *r,
				      int snum)
{
	int count;

	print_status_struct status;

	count = print_queue_length(snum, &status);

	r->servername		= talloc_strdup(mem_ctx, ntprinter->info_2->servername);
	W_ERROR_HAVE_NO_MEMORY(r->servername);
	r->printername		= talloc_strdup(mem_ctx, ntprinter->info_2->printername);
	W_ERROR_HAVE_NO_MEMORY(r->printername);
	r->sharename		= talloc_strdup(mem_ctx, lp_servicename(snum));
	W_ERROR_HAVE_NO_MEMORY(r->sharename);
	r->portname		= talloc_strdup(mem_ctx, ntprinter->info_2->portname);
	W_ERROR_HAVE_NO_MEMORY(r->portname);
	r->drivername		= talloc_strdup(mem_ctx, ntprinter->info_2->drivername);
	W_ERROR_HAVE_NO_MEMORY(r->drivername);

	if (*ntprinter->info_2->comment == '\0') {
		r->comment	= talloc_strdup(mem_ctx, lp_comment(snum));
	} else {
		r->comment	= talloc_strdup(mem_ctx, ntprinter->info_2->comment);
	}
	W_ERROR_HAVE_NO_MEMORY(r->comment);

	r->location		= talloc_strdup(mem_ctx, ntprinter->info_2->location);
	W_ERROR_HAVE_NO_MEMORY(r->location);
	r->sepfile		= talloc_strdup(mem_ctx, ntprinter->info_2->sepfile);
	W_ERROR_HAVE_NO_MEMORY(r->sepfile);
	r->printprocessor	= talloc_strdup(mem_ctx, ntprinter->info_2->printprocessor);
	W_ERROR_HAVE_NO_MEMORY(r->printprocessor);
	r->datatype		= talloc_strdup(mem_ctx, ntprinter->info_2->datatype);
	W_ERROR_HAVE_NO_MEMORY(r->datatype);
	r->parameters		= talloc_strdup(mem_ctx, ntprinter->info_2->parameters);
	W_ERROR_HAVE_NO_MEMORY(r->parameters);

	r->attributes		= ntprinter->info_2->attributes;

	r->priority		= ntprinter->info_2->priority;
	r->defaultpriority	= ntprinter->info_2->default_priority;
	r->starttime		= ntprinter->info_2->starttime;
	r->untiltime		= ntprinter->info_2->untiltime;
	r->status		= nt_printq_status(status.status);
	r->cjobs		= count;
	r->averageppm		= ntprinter->info_2->averageppm;

	r->devmode = construct_dev_mode(mem_ctx, lp_const_servicename(snum));
	if (!r->devmode) {
		DEBUG(8,("Returning NULL Devicemode!\n"));
	}

	r->secdesc		= NULL;

	if (ntprinter->info_2->secdesc_buf && ntprinter->info_2->secdesc_buf->sd_size != 0) {
		/* don't use talloc_steal() here unless you do a deep steal of all
		   the SEC_DESC members */

		r->secdesc	= dup_sec_desc(mem_ctx, ntprinter->info_2->secdesc_buf->sd);
	}

	return WERR_OK;
}

/********************************************************************
 * construct_printer_info3
 * fill a spoolss_PrinterInfo3 struct
 ********************************************************************/

static WERROR construct_printer_info3(TALLOC_CTX *mem_ctx,
				      const NT_PRINTER_INFO_LEVEL *ntprinter,
				      struct spoolss_PrinterInfo3 *r,
				      int snum)
{
	/* These are the components of the SD we are returning. */

	if (ntprinter->info_2->secdesc_buf && ntprinter->info_2->secdesc_buf->sd_size != 0) {
		/* don't use talloc_steal() here unless you do a deep steal of all
		   the SEC_DESC members */

		r->secdesc = dup_sec_desc(mem_ctx,
					  ntprinter->info_2->secdesc_buf->sd);
		W_ERROR_HAVE_NO_MEMORY(r->secdesc);
	}

	return WERR_OK;
}

/********************************************************************
 * construct_printer_info4
 * fill a spoolss_PrinterInfo4 struct
 ********************************************************************/

static WERROR construct_printer_info4(TALLOC_CTX *mem_ctx,
				      const NT_PRINTER_INFO_LEVEL *ntprinter,
				      struct spoolss_PrinterInfo4 *r,
				      int snum)
{
	r->printername	= talloc_strdup(mem_ctx, ntprinter->info_2->printername);
	W_ERROR_HAVE_NO_MEMORY(r->printername);
	r->servername	= talloc_strdup(mem_ctx, ntprinter->info_2->servername);
	W_ERROR_HAVE_NO_MEMORY(r->servername);

	r->attributes	= ntprinter->info_2->attributes;

	return WERR_OK;
}

/********************************************************************
 * construct_printer_info5
 * fill a spoolss_PrinterInfo5 struct
 ********************************************************************/

static WERROR construct_printer_info5(TALLOC_CTX *mem_ctx,
				      const NT_PRINTER_INFO_LEVEL *ntprinter,
				      struct spoolss_PrinterInfo5 *r,
				      int snum)
{
	r->printername	= talloc_strdup(mem_ctx, ntprinter->info_2->printername);
	W_ERROR_HAVE_NO_MEMORY(r->printername);
	r->portname	= talloc_strdup(mem_ctx, ntprinter->info_2->portname);
	W_ERROR_HAVE_NO_MEMORY(r->portname);

	r->attributes	= ntprinter->info_2->attributes;

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
				      const NT_PRINTER_INFO_LEVEL *ntprinter,
				      struct spoolss_PrinterInfo6 *r,
				      int snum)
{
	int count;
	print_status_struct status;

	count = print_queue_length(snum, &status);

	r->status = nt_printq_status(status.status);

	return WERR_OK;
}

/********************************************************************
 * construct_printer_info7
 * fill a spoolss_PrinterInfo7 struct
 ********************************************************************/

static WERROR construct_printer_info7(TALLOC_CTX *mem_ctx,
				      Printer_entry *print_hnd,
				      struct spoolss_PrinterInfo7 *r,
				      int snum)
{
	struct GUID guid;

	if (is_printer_published(print_hnd, snum, &guid)) {
		r->guid = talloc_strdup_upper(mem_ctx, GUID_string2(mem_ctx, &guid));
		r->action = DSPRINT_PUBLISH;
	} else {
		r->guid = talloc_strdup(mem_ctx, "");
		r->action = DSPRINT_UNPUBLISH;
	}
	W_ERROR_HAVE_NO_MEMORY(r->guid);

	return WERR_OK;
}

/********************************************************************
 * construct_printer_info8
 * fill a spoolss_PrinterInfo8 struct
 ********************************************************************/

static WERROR construct_printer_info8(TALLOC_CTX *mem_ctx,
				      const NT_PRINTER_INFO_LEVEL *ntprinter,
				      struct spoolss_DeviceModeInfo *r,
				      int snum)
{
	struct spoolss_DeviceMode *devmode;
	WERROR result;

	if (!ntprinter->info_2->devmode) {
		r->devmode = NULL;
		return WERR_OK;
	}

	devmode = TALLOC_ZERO_P(mem_ctx, struct spoolss_DeviceMode);
	W_ERROR_HAVE_NO_MEMORY(devmode);

	result = convert_nt_devicemode(mem_ctx, devmode, ntprinter->info_2->devmode);
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(devmode);
		return result;
	}

	r->devmode	= devmode;

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

	*count_p = 0;
	*info_p = NULL;

	for (snum = 0; snum < n_services; snum++) {

		NT_PRINTER_INFO_LEVEL *ntprinter = NULL;

		if (!snum_is_shared_printer(snum)) {
			continue;
		}

		DEBUG(4,("Found a printer in smb.conf: %s[%x]\n",
			lp_servicename(snum), snum));

		info = TALLOC_REALLOC_ARRAY(mem_ctx, info,
					    union spoolss_PrinterInfo,
					    count + 1);
		if (!info) {
			result = WERR_NOMEM;
			goto out;
		}

		result = get_a_printer(NULL, &ntprinter, 2,
				       lp_const_servicename(snum));
		if (!W_ERROR_IS_OK(result)) {
			goto out;
		}

		switch (level) {
		case 0:
			result = construct_printer_info0(info, ntprinter,
							 &info[count].info0, snum);
			break;
		case 1:
			result = construct_printer_info1(info, ntprinter, flags,
							 &info[count].info1, snum);
			break;
		case 2:
			result = construct_printer_info2(info, ntprinter,
							 &info[count].info2, snum);
			break;
		case 4:
			result = construct_printer_info4(info, ntprinter,
							 &info[count].info4, snum);
			break;
		case 5:
			result = construct_printer_info5(info, ntprinter,
							 &info[count].info5, snum);
			break;

		default:
			result = WERR_UNKNOWN_LEVEL;
			free_a_printer(&ntprinter, 2);
			goto out;
		}

		free_a_printer(&ntprinter, 2);
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
				  uint32_t flags,
				  const char *servername,
				  union spoolss_PrinterInfo **info,
				  uint32_t *count)
{
	DEBUG(4,("enum_all_printers_info_0\n"));

	return enum_all_printers_info_level(mem_ctx, 0, flags, info, count);
}


/********************************************************************
********************************************************************/

static WERROR enum_all_printers_info_1(TALLOC_CTX *mem_ctx,
				       uint32_t flags,
				       union spoolss_PrinterInfo **info,
				       uint32_t *count)
{
	DEBUG(4,("enum_all_printers_info_1\n"));

	return enum_all_printers_info_level(mem_ctx, 1, flags, info, count);
}

/********************************************************************
 enum_all_printers_info_1_local.
*********************************************************************/

static WERROR enum_all_printers_info_1_local(TALLOC_CTX *mem_ctx,
					     union spoolss_PrinterInfo **info,
					     uint32_t *count)
{
	DEBUG(4,("enum_all_printers_info_1_local\n"));

	return enum_all_printers_info_1(mem_ctx, PRINTER_ENUM_ICON8, info, count);
}

/********************************************************************
 enum_all_printers_info_1_name.
*********************************************************************/

static WERROR enum_all_printers_info_1_name(TALLOC_CTX *mem_ctx,
					    const char *name,
					    union spoolss_PrinterInfo **info,
					    uint32_t *count)
{
	const char *s = name;

	DEBUG(4,("enum_all_printers_info_1_name\n"));

	if ((name[0] == '\\') && (name[1] == '\\')) {
		s = name + 2;
	}

	if (!is_myname_or_ipaddr(s)) {
		return WERR_INVALID_NAME;
	}

	return enum_all_printers_info_1(mem_ctx, PRINTER_ENUM_ICON8, info, count);
}

/********************************************************************
 enum_all_printers_info_1_network.
*********************************************************************/

static WERROR enum_all_printers_info_1_network(TALLOC_CTX *mem_ctx,
					       const char *name,
					       union spoolss_PrinterInfo **info,
					       uint32_t *count)
{
	const char *s = name;

	DEBUG(4,("enum_all_printers_info_1_network\n"));

	/* If we respond to a enum_printers level 1 on our name with flags
	   set to PRINTER_ENUM_REMOTE with a list of printers then these
	   printers incorrectly appear in the APW browse list.
	   Specifically the printers for the server appear at the workgroup
	   level where all the other servers in the domain are
	   listed. Windows responds to this call with a
	   WERR_CAN_NOT_COMPLETE so we should do the same. */

	if (name[0] == '\\' && name[1] == '\\') {
		 s = name + 2;
	}

	if (is_myname_or_ipaddr(s)) {
		 return WERR_CAN_NOT_COMPLETE;
	}

	return enum_all_printers_info_1(mem_ctx, PRINTER_ENUM_NAME, info, count);
}

/********************************************************************
 * api_spoolss_enumprinters
 *
 * called from api_spoolss_enumprinters (see this to understand)
 ********************************************************************/

static WERROR enum_all_printers_info_2(TALLOC_CTX *mem_ctx,
				       union spoolss_PrinterInfo **info,
				       uint32_t *count)
{
	DEBUG(4,("enum_all_printers_info_2\n"));

	return enum_all_printers_info_level(mem_ctx, 2, 0, info, count);
}

/********************************************************************
 * handle enumeration of printers at level 1
 ********************************************************************/

static WERROR enumprinters_level1(TALLOC_CTX *mem_ctx,
				  uint32_t flags,
				  const char *name,
				  union spoolss_PrinterInfo **info,
				  uint32_t *count)
{
	/* Not all the flags are equals */

	if (flags & PRINTER_ENUM_LOCAL) {
		return enum_all_printers_info_1_local(mem_ctx, info, count);
	}

	if (flags & PRINTER_ENUM_NAME) {
		return enum_all_printers_info_1_name(mem_ctx, name, info, count);
	}

	if (flags & PRINTER_ENUM_NETWORK) {
		return enum_all_printers_info_1_network(mem_ctx, name, info, count);
	}

	return WERR_OK; /* NT4sp5 does that */
}

/********************************************************************
 * handle enumeration of printers at level 2
 ********************************************************************/

static WERROR enumprinters_level2(TALLOC_CTX *mem_ctx,
				  uint32_t flags,
				  const char *servername,
				  union spoolss_PrinterInfo **info,
				  uint32_t *count)
{
	if (flags & PRINTER_ENUM_LOCAL) {
		return enum_all_printers_info_2(mem_ctx, info, count);
	}

	if (flags & PRINTER_ENUM_NAME) {
		if (!is_myname_or_ipaddr(canon_servername(servername))) {
			return WERR_INVALID_NAME;
		}

		return enum_all_printers_info_2(mem_ctx, info, count);
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
				  uint32_t flags,
				  const char *servername,
				  union spoolss_PrinterInfo **info,
				  uint32_t *count)
{
	DEBUG(4,("enum_all_printers_info_4\n"));

	return enum_all_printers_info_level(mem_ctx, 4, flags, info, count);
}


/********************************************************************
 * handle enumeration of printers at level 5
 ********************************************************************/

static WERROR enumprinters_level5(TALLOC_CTX *mem_ctx,
				  uint32_t flags,
				  const char *servername,
				  union spoolss_PrinterInfo **info,
				  uint32_t *count)
{
	DEBUG(4,("enum_all_printers_info_5\n"));

	return enum_all_printers_info_level(mem_ctx, 5, flags, info, count);
}

/****************************************************************
 _spoolss_EnumPrinters
****************************************************************/

WERROR _spoolss_EnumPrinters(pipes_struct *p,
			     struct spoolss_EnumPrinters *r)
{
	const char *name = NULL;
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

	if (r->in.server) {
		name = talloc_strdup_upper(p->mem_ctx, r->in.server);
		W_ERROR_HAVE_NO_MEMORY(name);
	}

	switch (r->in.level) {
	case 0:
		result = enumprinters_level0(p->mem_ctx, r->in.flags, name,
					     r->out.info, r->out.count);
		break;
	case 1:
		result = enumprinters_level1(p->mem_ctx, r->in.flags, name,
					     r->out.info, r->out.count);
		break;
	case 2:
		result = enumprinters_level2(p->mem_ctx, r->in.flags, name,
					     r->out.info, r->out.count);
		break;
	case 4:
		result = enumprinters_level4(p->mem_ctx, r->in.flags, name,
					     r->out.info, r->out.count);
		break;
	case 5:
		result = enumprinters_level5(p->mem_ctx, r->in.flags, name,
					     r->out.info, r->out.count);
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(p->mem_ctx,
						     spoolss_EnumPrinters, NULL,
						     *r->out.info, r->in.level,
						     *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************
 _spoolss_GetPrinter
****************************************************************/

WERROR _spoolss_GetPrinter(pipes_struct *p,
			   struct spoolss_GetPrinter *r)
{
	Printer_entry *Printer = find_printer_index_by_hnd(p, r->in.handle);
	NT_PRINTER_INFO_LEVEL *ntprinter = NULL;
	WERROR result = WERR_OK;

	int snum;

	/* that's an [in out] buffer */

	if (!r->in.buffer && (r->in.offered != 0)) {
		return WERR_INVALID_PARAM;
	}

	*r->out.needed = 0;

	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}

	result = get_a_printer(Printer, &ntprinter, 2,
			       lp_const_servicename(snum));
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	switch (r->in.level) {
	case 0:
		result = construct_printer_info0(p->mem_ctx, ntprinter,
						 &r->out.info->info0, snum);
		break;
	case 1:
		result = construct_printer_info1(p->mem_ctx, ntprinter,
						 PRINTER_ENUM_ICON8,
						 &r->out.info->info1, snum);
		break;
	case 2:
		result = construct_printer_info2(p->mem_ctx, ntprinter,
						 &r->out.info->info2, snum);
		break;
	case 3:
		result = construct_printer_info3(p->mem_ctx, ntprinter,
						 &r->out.info->info3, snum);
		break;
	case 4:
		result = construct_printer_info4(p->mem_ctx, ntprinter,
						 &r->out.info->info4, snum);
		break;
	case 5:
		result = construct_printer_info5(p->mem_ctx, ntprinter,
						 &r->out.info->info5, snum);
		break;
	case 6:
		result = construct_printer_info6(p->mem_ctx, ntprinter,
						 &r->out.info->info6, snum);
		break;
	case 7:
		result = construct_printer_info7(p->mem_ctx, Printer,
						 &r->out.info->info7, snum);
		break;
	case 8:
		result = construct_printer_info8(p->mem_ctx, ntprinter,
						 &r->out.info->info8, snum);
		break;
	default:
		result = WERR_UNKNOWN_LEVEL;
		break;
	}

	free_a_printer(&ntprinter, 2);

	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(r->out.info);
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION(spoolss_PrinterInfo, NULL,
					       r->out.info, r->in.level);
	r->out.info	= SPOOLSS_BUFFER_OK(r->out.info, NULL);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/********************************************************************
 ********************************************************************/

static const char **string_array_from_driver_info(TALLOC_CTX *mem_ctx,
						  const char **string_array,
						  const char *cservername)
{
	int i, num_strings = 0;
	const char **array = NULL;

	if (!string_array) {
		return NULL;
	}

	for (i=0; string_array[i] && string_array[i][0] != '\0'; i++) {

		const char *str = talloc_asprintf(mem_ctx, "\\\\%s%s",
						  cservername, string_array[i]);
		if (!str) {
			TALLOC_FREE(array);
			return NULL;
		}


		if (!add_string_to_array(mem_ctx, str, &array, &num_strings)) {
			TALLOC_FREE(array);
			return NULL;
		}
	}

	if (i > 0) {
		ADD_TO_ARRAY(mem_ctx, const char *, NULL,
			     &array, &num_strings);
	}

	return array;
}

#define FILL_DRIVER_STRING(mem_ctx, in, out) \
	do { \
		if (in && strlen(in)) { \
			out = talloc_strdup(mem_ctx, in); \
			W_ERROR_HAVE_NO_MEMORY(out); \
		} else { \
			out = NULL; \
		} \
	} while (0);

#define FILL_DRIVER_UNC_STRING(mem_ctx, server, in, out) \
	do { \
		if (in && strlen(in)) { \
			out = talloc_asprintf(mem_ctx, "\\\\%s%s", server, in); \
		} else { \
			out = talloc_strdup(mem_ctx, ""); \
		} \
		W_ERROR_HAVE_NO_MEMORY(out); \
	} while (0);

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
			       driver->driver_path,
			       r->driver_path);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->data_file,
			       r->data_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
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
			       driver->driver_path,
			       r->driver_path);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->data_file,
			       r->data_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->config_file,
			       r->config_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->help_file,
			       r->help_file);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->monitor_name,
			   r->monitor_name);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->default_datatype,
			   r->default_datatype);

	r->dependent_files = string_array_from_driver_info(mem_ctx,
							   driver->dependent_files,
							   cservername);
	return WERR_OK;
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

	r->version		= driver->version;

	r->driver_name		= talloc_strdup(mem_ctx, driver->driver_name);
	W_ERROR_HAVE_NO_MEMORY(r->driver_name);
	r->architecture		= talloc_strdup(mem_ctx, driver->architecture);
	W_ERROR_HAVE_NO_MEMORY(r->architecture);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->driver_path,
			       r->driver_path);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->data_file,
			       r->data_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->config_file,
			       r->config_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->help_file,
			       r->help_file);

	r->dependent_files = string_array_from_driver_info(mem_ctx,
							   driver->dependent_files,
							   cservername);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->monitor_name,
			   r->monitor_name);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->default_datatype,
			   r->default_datatype);

	r->previous_names = string_array_from_driver_info(mem_ctx,
							  driver->previous_names,
							  cservername);

	return WERR_OK;
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
			       driver->driver_path,
			       r->driver_path);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->data_file,
			       r->data_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
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

	r->version		= driver->version;

	r->driver_name		= talloc_strdup(mem_ctx, driver->driver_name);
	W_ERROR_HAVE_NO_MEMORY(r->driver_name);
	r->architecture		= talloc_strdup(mem_ctx, driver->architecture);
	W_ERROR_HAVE_NO_MEMORY(r->architecture);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->driver_path,
			       r->driver_path);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->data_file,
			       r->data_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->config_file,
			       r->config_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->help_file,
			       r->help_file);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->monitor_name,
			   r->monitor_name);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->default_datatype,
			   r->default_datatype);

	r->dependent_files = string_array_from_driver_info(mem_ctx,
							   driver->dependent_files,
							   cservername);
	r->previous_names = string_array_from_driver_info(mem_ctx,
							  driver->previous_names,
							  cservername);

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

	r->version		= driver->version;

	r->driver_name		= talloc_strdup(mem_ctx, driver->driver_name);
	W_ERROR_HAVE_NO_MEMORY(r->driver_name);
	r->architecture		= talloc_strdup(mem_ctx, driver->architecture);
	W_ERROR_HAVE_NO_MEMORY(r->architecture);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->driver_path,
			       r->driver_path);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->data_file,
			       r->data_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->config_file,
			       r->config_file);

	FILL_DRIVER_UNC_STRING(mem_ctx, cservername,
			       driver->help_file,
			       r->help_file);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->monitor_name,
			   r->monitor_name);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->default_datatype,
			   r->default_datatype);

	r->dependent_files = string_array_from_driver_info(mem_ctx,
							   driver->dependent_files,
							   cservername);
	r->previous_names = string_array_from_driver_info(mem_ctx,
							  driver->previous_names,
							  cservername);

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

	r->color_profiles = string_array_from_driver_info(mem_ctx,
							  driver->color_profiles,
							  cservername);

	FILL_DRIVER_STRING(mem_ctx,
			   driver->inf_path,
			   r->inf_path);

	r->printer_driver_attributes	= driver->printer_driver_attributes;

	r->core_driver_dependencies = string_array_from_driver_info(mem_ctx,
								    driver->core_driver_dependencies,
								    cservername);

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

	r->previous_names = string_array_from_driver_info(mem_ctx,
							  driver->previous_names,
							  cservername);
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
						  uint32_t level,
						  union spoolss_DriverInfo *r,
						  int snum,
						  const char *servername,
						  const char *architecture,
						  uint32_t version)
{
	NT_PRINTER_INFO_LEVEL *printer = NULL;
	struct spoolss_DriverInfo8 *driver;
	WERROR result;

	result = get_a_printer(NULL, &printer, 2, lp_const_servicename(snum));

	DEBUG(8,("construct_printer_driver_info_level: status: %s\n",
		win_errstr(result)));

	if (!W_ERROR_IS_OK(result)) {
		return WERR_INVALID_PRINTER_NAME;
	}

	result = get_a_printer_driver(mem_ctx, &driver, printer->info_2->drivername,
				      architecture, version);

	DEBUG(8,("construct_printer_driver_info_level: status: %s\n",
		win_errstr(result)));

	if (!W_ERROR_IS_OK(result)) {
		/*
		 * Is this a W2k client ?
		 */

		if (version < 3) {
			free_a_printer(&printer, 2);
			return WERR_UNKNOWN_PRINTER_DRIVER;
		}

		/* Yes - try again with a WinNT driver. */
		version = 2;
		result = get_a_printer_driver(mem_ctx, &driver, printer->info_2->drivername,
					      architecture, version);
		DEBUG(8,("construct_printer_driver_level: status: %s\n",
			win_errstr(result)));
		if (!W_ERROR_IS_OK(result)) {
			free_a_printer(&printer, 2);
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

	free_a_printer(&printer, 2);
	free_a_printer_driver(driver);

	return result;
}

/****************************************************************
 _spoolss_GetPrinterDriver2
****************************************************************/

WERROR _spoolss_GetPrinterDriver2(pipes_struct *p,
				  struct spoolss_GetPrinterDriver2 *r)
{
	Printer_entry *printer;
	WERROR result;

	const char *servername;
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

	servername = get_server_name(printer);

	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}

	result = construct_printer_driver_info_level(p->mem_ctx, r->in.level,
						     r->out.info, snum,
						     servername,
						     r->in.architecture,
						     r->in.client_major_version);
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(r->out.info);
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION(spoolss_DriverInfo, NULL,
					       r->out.info, r->in.level);
	r->out.info	= SPOOLSS_BUFFER_OK(r->out.info, NULL);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}


/****************************************************************
 _spoolss_StartPagePrinter
****************************************************************/

WERROR _spoolss_StartPagePrinter(pipes_struct *p,
				 struct spoolss_StartPagePrinter *r)
{
	Printer_entry *Printer = find_printer_index_by_hnd(p, r->in.handle);

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

WERROR _spoolss_EndPagePrinter(pipes_struct *p,
			       struct spoolss_EndPagePrinter *r)
{
	int snum;

	Printer_entry *Printer = find_printer_index_by_hnd(p, r->in.handle);

	if (!Printer) {
		DEBUG(2,("_spoolss_EndPagePrinter: Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	if (!get_printer_snum(p, r->in.handle, &snum, NULL))
		return WERR_BADFID;

	Printer->page_started = false;
	print_job_endpage(snum, Printer->jobid);

	return WERR_OK;
}

/****************************************************************
 _spoolss_StartDocPrinter
****************************************************************/

WERROR _spoolss_StartDocPrinter(pipes_struct *p,
				struct spoolss_StartDocPrinter *r)
{
	struct spoolss_DocumentInfo1 *info_1;
	int snum;
	Printer_entry *Printer = find_printer_index_by_hnd(p, r->in.handle);

	if (!Printer) {
		DEBUG(2,("_spoolss_StartDocPrinter: "
			"Invalid handle (%s:%u:%u)\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
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
		if (strcmp(info_1->datatype, "RAW") != 0) {
			*r->out.job_id = 0;
			return WERR_INVALID_DATATYPE;
		}
	}

	/* get the share number of the printer */
	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}

	Printer->jobid = print_job_start(p->server_info, snum,
					 info_1->document_name,
					 Printer->nt_devmode);

	/* An error occured in print_job_start() so return an appropriate
	   NT error code. */

	if (Printer->jobid == -1) {
		return map_werror_from_unix(errno);
	}

	Printer->document_started = true;
	*r->out.job_id = Printer->jobid;

	return WERR_OK;
}

/****************************************************************
 _spoolss_EndDocPrinter
****************************************************************/

WERROR _spoolss_EndDocPrinter(pipes_struct *p,
			      struct spoolss_EndDocPrinter *r)
{
	Printer_entry *Printer = find_printer_index_by_hnd(p, r->in.handle);
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
	print_job_end(snum, Printer->jobid, NORMAL_CLOSE);
	/* error codes unhandled so far ... */

	return WERR_OK;
}

/****************************************************************
 _spoolss_WritePrinter
****************************************************************/

WERROR _spoolss_WritePrinter(pipes_struct *p,
			     struct spoolss_WritePrinter *r)
{
	uint32_t buffer_written;
	int snum;
	Printer_entry *Printer = find_printer_index_by_hnd(p, r->in.handle);

	if (!Printer) {
		DEBUG(2,("_spoolss_WritePrinter: Invalid handle (%s:%u:%u)\n",
			OUR_HANDLE(r->in.handle)));
		*r->out.num_written = r->in._data_size;
		return WERR_BADFID;
	}

	if (!get_printer_snum(p, r->in.handle, &snum, NULL))
		return WERR_BADFID;

	buffer_written = (uint32_t)print_job_write(snum, Printer->jobid,
						   (const char *)r->in.data.data,
						   (SMB_OFF_T)-1,
						   (size_t)r->in._data_size);
	if (buffer_written == (uint32_t)-1) {
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
			      pipes_struct *p)
{
	int snum;
	WERROR errcode = WERR_BADFUNC;
	Printer_entry *Printer = find_printer_index_by_hnd(p, handle);

	if (!Printer) {
		DEBUG(2,("control_printer: Invalid handle (%s:%u:%u)\n",
			OUR_HANDLE(handle)));
		return WERR_BADFID;
	}

	if (!get_printer_snum(p, handle, &snum, NULL))
		return WERR_BADFID;

	switch (command) {
	case SPOOLSS_PRINTER_CONTROL_PAUSE:
		errcode = print_queue_pause(p->server_info, snum);
		break;
	case SPOOLSS_PRINTER_CONTROL_RESUME:
	case SPOOLSS_PRINTER_CONTROL_UNPAUSE:
		errcode = print_queue_resume(p->server_info, snum);
		break;
	case SPOOLSS_PRINTER_CONTROL_PURGE:
		errcode = print_queue_purge(p->server_info, snum);
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

WERROR _spoolss_AbortPrinter(pipes_struct *p,
			     struct spoolss_AbortPrinter *r)
{
	Printer_entry 	*Printer = find_printer_index_by_hnd(p, r->in.handle);
	int		snum;
	WERROR 		errcode = WERR_OK;

	if (!Printer) {
		DEBUG(2,("_spoolss_AbortPrinter: Invalid handle (%s:%u:%u)\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	if (!get_printer_snum(p, r->in.handle, &snum, NULL))
		return WERR_BADFID;

	print_job_delete(p->server_info, snum, Printer->jobid, &errcode );

	return errcode;
}

/********************************************************************
 * called by spoolss_api_setprinter
 * when updating a printer description
 ********************************************************************/

static WERROR update_printer_sec(struct policy_handle *handle,
				 pipes_struct *p, SEC_DESC_BUF *secdesc_ctr)
{
	SEC_DESC_BUF *new_secdesc_ctr = NULL, *old_secdesc_ctr = NULL;
	WERROR result;
	int snum;

	Printer_entry *Printer = find_printer_index_by_hnd(p, handle);

	if (!Printer || !get_printer_snum(p, handle, &snum, NULL)) {
		DEBUG(2,("update_printer_sec: Invalid handle (%s:%u:%u)\n",
			 OUR_HANDLE(handle)));

		result = WERR_BADFID;
		goto done;
	}

	if (!secdesc_ctr) {
		DEBUG(10,("update_printer_sec: secdesc_ctr is NULL !\n"));
		result = WERR_INVALID_PARAM;
		goto done;
	}

	/* Check the user has permissions to change the security
	   descriptor.  By experimentation with two NT machines, the user
	   requires Full Access to the printer to change security
	   information. */

	if ( Printer->access_granted != PRINTER_ACCESS_ADMINISTER ) {
		DEBUG(4,("update_printer_sec: updated denied by printer permissions\n"));
		result = WERR_ACCESS_DENIED;
		goto done;
	}

	/* NT seems to like setting the security descriptor even though
	   nothing may have actually changed. */

	if ( !nt_printing_getsec(p->mem_ctx, Printer->sharename, &old_secdesc_ctr)) {
		DEBUG(2,("update_printer_sec: nt_printing_getsec() failed\n"));
		result = WERR_BADFID;
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		SEC_ACL *the_acl;
		int i;

		the_acl = old_secdesc_ctr->sd->dacl;
		DEBUG(10, ("old_secdesc_ctr for %s has %d aces:\n",
			   PRINTERNAME(snum), the_acl->num_aces));

		for (i = 0; i < the_acl->num_aces; i++) {
			DEBUG(10, ("%s 0x%08x\n", sid_string_dbg(
					   &the_acl->aces[i].trustee),
				  the_acl->aces[i].access_mask));
		}

		the_acl = secdesc_ctr->sd->dacl;

		if (the_acl) {
			DEBUG(10, ("secdesc_ctr for %s has %d aces:\n",
				   PRINTERNAME(snum), the_acl->num_aces));

			for (i = 0; i < the_acl->num_aces; i++) {
				DEBUG(10, ("%s 0x%08x\n", sid_string_dbg(
						   &the_acl->aces[i].trustee),
					   the_acl->aces[i].access_mask));
			}
		} else {
			DEBUG(10, ("dacl for secdesc_ctr is NULL\n"));
		}
	}

	new_secdesc_ctr = sec_desc_merge(p->mem_ctx, secdesc_ctr, old_secdesc_ctr);
	if (!new_secdesc_ctr) {
		result = WERR_NOMEM;
		goto done;
	}

	if (security_descriptor_equal(new_secdesc_ctr->sd, old_secdesc_ctr->sd)) {
		result = WERR_OK;
		goto done;
	}

	result = nt_printing_setsec(Printer->sharename, new_secdesc_ctr);

 done:

	return result;
}

/********************************************************************
 Canonicalize printer info from a client

 ATTN: It does not matter what we set the servername to hear
 since we do the necessary work in get_a_printer() to set it to
 the correct value based on what the client sent in the
 _spoolss_open_printer_ex().
 ********************************************************************/

static bool check_printer_ok(NT_PRINTER_INFO_LEVEL_2 *info, int snum)
{
	fstring printername;
	const char *p;

	DEBUG(5,("check_printer_ok: servername=%s printername=%s sharename=%s "
		"portname=%s drivername=%s comment=%s location=%s\n",
		info->servername, info->printername, info->sharename,
		info->portname, info->drivername, info->comment, info->location));

	/* we force some elements to "correct" values */
	slprintf(info->servername, sizeof(info->servername)-1, "\\\\%s", global_myname());
	fstrcpy(info->sharename, lp_servicename(snum));

	/* check to see if we allow printername != sharename */

	if ( lp_force_printername(snum) ) {
		slprintf(info->printername, sizeof(info->printername)-1, "\\\\%s\\%s",
			global_myname(), info->sharename );
	} else {

		/* make sure printername is in \\server\printername format */

		fstrcpy( printername, info->printername );
		p = printername;
		if ( printername[0] == '\\' && printername[1] == '\\' ) {
			if ( (p = strchr_m( &printername[2], '\\' )) != NULL )
				p++;
		}

		slprintf(info->printername, sizeof(info->printername)-1, "\\\\%s\\%s",
			 global_myname(), p );
	}

	info->attributes |= PRINTER_ATTRIBUTE_SAMBA;
	info->attributes &= ~PRINTER_ATTRIBUTE_NOT_SAMBA;



	return true;
}

/****************************************************************************
****************************************************************************/

static WERROR add_port_hook(TALLOC_CTX *ctx, NT_USER_TOKEN *token, const char *portname, const char *uri)
{
	char *cmd = lp_addport_cmd();
	char *command = NULL;
	int ret;
	SE_PRIV se_printop = SE_PRINT_OPERATOR;
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
		is_print_op = user_has_privileges( token, &se_printop );

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

bool add_printer_hook(TALLOC_CTX *ctx, NT_USER_TOKEN *token, NT_PRINTER_INFO_LEVEL *printer)
{
	char *cmd = lp_addprinter_cmd();
	char **qlines;
	char *command = NULL;
	int numlines;
	int ret;
	int fd;
	SE_PRIV se_printop = SE_PRINT_OPERATOR;
	bool is_print_op = false;
	char *remote_machine = talloc_strdup(ctx, "%m");

	if (!remote_machine) {
		return false;
	}
	remote_machine = talloc_sub_basic(ctx,
				current_user_info.smb_name,
				current_user_info.domain,
				remote_machine);
	if (!remote_machine) {
		return false;
	}

	command = talloc_asprintf(ctx,
			"%s \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"",
			cmd, printer->info_2->printername, printer->info_2->sharename,
			printer->info_2->portname, printer->info_2->drivername,
			printer->info_2->location, printer->info_2->comment, remote_machine);
	if (!command) {
		return false;
	}

	if ( token )
		is_print_op = user_has_privileges( token, &se_printop );

	DEBUG(10,("Running [%s]\n", command));

	/********* BEGIN SePrintOperatorPrivilege **********/

	if ( is_print_op )
		become_root();

	if ( (ret = smbrun(command, &fd)) == 0 ) {
		/* Tell everyone we updated smb.conf. */
		message_send_all(smbd_messaging_context(),
				 MSG_SMB_CONF_UPDATED, NULL, 0, NULL);
	}

	if ( is_print_op )
		unbecome_root();

	/********* END SePrintOperatorPrivilege **********/

	DEBUGADD(10,("returned [%d]\n", ret));

	TALLOC_FREE(command);
	TALLOC_FREE(remote_machine);

	if ( ret != 0 ) {
		if (fd != -1)
			close(fd);
		return false;
	}

	/* reload our services immediately */
	become_root();
	reload_services(false);
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
		strncpy(printer->info_2->portname, qlines[0], sizeof(printer->info_2->portname));
		DEBUGADD(6,("Line[0] = [%s]\n", qlines[0]));
	}

	TALLOC_FREE(qlines);
	return true;
}


/********************************************************************
 * Called by spoolss_api_setprinter
 * when updating a printer description.
 ********************************************************************/

static WERROR update_printer(pipes_struct *p, struct policy_handle *handle,
			     struct spoolss_SetPrinterInfoCtr *info_ctr,
			     struct spoolss_DeviceMode *devmode)
{
	int snum;
	NT_PRINTER_INFO_LEVEL *printer = NULL, *old_printer = NULL;
	Printer_entry *Printer = find_printer_index_by_hnd(p, handle);
	WERROR result;
	DATA_BLOB buffer;
	fstring asc_buffer;

	DEBUG(8,("update_printer\n"));

	result = WERR_OK;

	if (!Printer) {
		result = WERR_BADFID;
		goto done;
	}

	if (!get_printer_snum(p, handle, &snum, NULL)) {
		result = WERR_BADFID;
		goto done;
	}

	if (!W_ERROR_IS_OK(get_a_printer(Printer, &printer, 2, lp_const_servicename(snum))) ||
	    (!W_ERROR_IS_OK(get_a_printer(Printer, &old_printer, 2, lp_const_servicename(snum))))) {
		result = WERR_BADFID;
		goto done;
	}

	DEBUGADD(8,("Converting info_2 struct\n"));

	/*
	 * convert_printer_info converts the incoming
	 * info from the client and overwrites the info
	 * just read from the tdb in the pointer 'printer'.
	 */

	if (!convert_printer_info(info_ctr, printer)) {
		result =  WERR_NOMEM;
		goto done;
	}

	if (devmode) {
		/* we have a valid devmode
		   convert it and link it*/

		DEBUGADD(8,("update_printer: Converting the devicemode struct\n"));
		if (!convert_devicemode(printer->info_2->printername, devmode,
					&printer->info_2->devmode)) {
			result =  WERR_NOMEM;
			goto done;
		}
	}

	/* Do sanity check on the requested changes for Samba */

	if (!check_printer_ok(printer->info_2, snum)) {
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

	if ( *lp_addprinter_cmd()
		&& (!strequal(printer->info_2->drivername, old_printer->info_2->drivername)
			|| !strequal(printer->info_2->comment, old_printer->info_2->comment)
			|| !strequal(printer->info_2->portname, old_printer->info_2->portname)
			|| !strequal(printer->info_2->location, old_printer->info_2->location)) )
	{
		/* add_printer_hook() will call reload_services() */

		if ( !add_printer_hook(p->mem_ctx, p->server_info->ptok,
				       printer) ) {
			result = WERR_ACCESS_DENIED;
			goto done;
		}
	}

	/*
	 * When a *new* driver is bound to a printer, the drivername is used to
	 * lookup previously saved driver initialization info, which is then
	 * bound to the printer, simulating what happens in the Windows arch.
	 */
	if (!strequal(printer->info_2->drivername, old_printer->info_2->drivername))
	{
		if (!set_driver_init(printer, 2))
		{
			DEBUG(5,("update_printer: Error restoring driver initialization data for driver [%s]!\n",
				printer->info_2->drivername));
		}

		DEBUG(10,("update_printer: changing driver [%s]!  Sending event!\n",
			printer->info_2->drivername));

		notify_printer_driver(snum, printer->info_2->drivername);
	}

	/*
	 * flag which changes actually occured.  This is a small subset of
	 * all the possible changes.  We also have to update things in the
	 * DsSpooler key.
	 */

	if (!strequal(printer->info_2->comment, old_printer->info_2->comment)) {
		push_reg_sz(talloc_tos(), &buffer, printer->info_2->comment);
		set_printer_dataex( printer, SPOOL_DSSPOOLER_KEY, "description",
			REG_SZ, buffer.data, buffer.length);

		notify_printer_comment(snum, printer->info_2->comment);
	}

	if (!strequal(printer->info_2->sharename, old_printer->info_2->sharename)) {
		push_reg_sz(talloc_tos(), &buffer, printer->info_2->sharename);
		set_printer_dataex( printer, SPOOL_DSSPOOLER_KEY, "shareName",
			REG_SZ, buffer.data, buffer.length);

		notify_printer_sharename(snum, printer->info_2->sharename);
	}

	if (!strequal(printer->info_2->printername, old_printer->info_2->printername)) {
		char *pname;

		if ( (pname = strchr_m( printer->info_2->printername+2, '\\' )) != NULL )
			pname++;
		else
			pname = printer->info_2->printername;


		push_reg_sz(talloc_tos(), &buffer, pname);
		set_printer_dataex( printer, SPOOL_DSSPOOLER_KEY, "printerName",
			REG_SZ, buffer.data, buffer.length);

		notify_printer_printername( snum, pname );
	}

	if (!strequal(printer->info_2->portname, old_printer->info_2->portname)) {
		push_reg_sz(talloc_tos(), &buffer, printer->info_2->portname);
		set_printer_dataex( printer, SPOOL_DSSPOOLER_KEY, "portName",
			REG_SZ, buffer.data, buffer.length);

		notify_printer_port(snum, printer->info_2->portname);
	}

	if (!strequal(printer->info_2->location, old_printer->info_2->location)) {
		push_reg_sz(talloc_tos(), &buffer, printer->info_2->location);
		set_printer_dataex( printer, SPOOL_DSSPOOLER_KEY, "location",
			REG_SZ, buffer.data, buffer.length);

		notify_printer_location(snum, printer->info_2->location);
	}

	/* here we need to update some more DsSpooler keys */
	/* uNCName, serverName, shortServerName */

	push_reg_sz(talloc_tos(), &buffer, global_myname());
	set_printer_dataex( printer, SPOOL_DSSPOOLER_KEY, "serverName",
		REG_SZ, buffer.data, buffer.length);
	set_printer_dataex( printer, SPOOL_DSSPOOLER_KEY, "shortServerName",
		REG_SZ, buffer.data, buffer.length);

	slprintf( asc_buffer, sizeof(asc_buffer)-1, "\\\\%s\\%s",
                 global_myname(), printer->info_2->sharename );
	push_reg_sz(talloc_tos(), &buffer, asc_buffer);
	set_printer_dataex( printer, SPOOL_DSSPOOLER_KEY, "uNCName",
		REG_SZ, buffer.data, buffer.length);

	/* Update printer info */
	result = mod_a_printer(printer, 2);

done:
	free_a_printer(&printer, 2);
	free_a_printer(&old_printer, 2);


	return result;
}

/****************************************************************************
****************************************************************************/
static WERROR publish_or_unpublish_printer(pipes_struct *p,
					   struct policy_handle *handle,
					   struct spoolss_SetPrinterInfo7 *info7)
{
#ifdef HAVE_ADS
	int snum;
	Printer_entry *Printer;

	if ( lp_security() != SEC_ADS ) {
		return WERR_UNKNOWN_LEVEL;
	}

	Printer = find_printer_index_by_hnd(p, handle);

	DEBUG(5,("publish_or_unpublish_printer, action = %d\n",info7->action));

	if (!Printer)
		return WERR_BADFID;

	if (!get_printer_snum(p, handle, &snum, NULL))
		return WERR_BADFID;

	nt_printer_publish(Printer, snum, info7->action);

	return WERR_OK;
#else
	return WERR_UNKNOWN_LEVEL;
#endif
}

/****************************************************************
 _spoolss_SetPrinter
****************************************************************/

WERROR _spoolss_SetPrinter(pipes_struct *p,
			   struct spoolss_SetPrinter *r)
{
	WERROR result;

	Printer_entry *Printer = find_printer_index_by_hnd(p, r->in.handle);

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
		default:
			return WERR_UNKNOWN_LEVEL;
	}
}

/****************************************************************
 _spoolss_FindClosePrinterNotify
****************************************************************/

WERROR _spoolss_FindClosePrinterNotify(pipes_struct *p,
				       struct spoolss_FindClosePrinterNotify *r)
{
	Printer_entry *Printer = find_printer_index_by_hnd(p, r->in.handle);

	if (!Printer) {
		DEBUG(2,("_spoolss_FindClosePrinterNotify: "
			"Invalid handle (%s:%u:%u)\n", OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	if (Printer->notify.client_connected == true) {
		int snum = -1;

		if ( Printer->printer_type == SPLHND_SERVER)
			snum = -1;
		else if ( (Printer->printer_type == SPLHND_PRINTER) &&
				!get_printer_snum(p, r->in.handle, &snum, NULL) )
			return WERR_BADFID;

		srv_spoolss_replycloseprinter(snum, &Printer->notify.client_hnd);
	}

	Printer->notify.flags=0;
	Printer->notify.options=0;
	Printer->notify.localmachine[0]='\0';
	Printer->notify.printerlocal=0;
	TALLOC_FREE(Printer->notify.option);
	Printer->notify.client_connected = false;

	return WERR_OK;
}

/****************************************************************
 _spoolss_AddJob
****************************************************************/

WERROR _spoolss_AddJob(pipes_struct *p,
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
			     const NT_PRINTER_INFO_LEVEL *ntprinter)
{
	struct tm *t;

	t = gmtime(&queue->time);

	r->job_id		= queue->job;

	r->printer_name		= talloc_strdup(mem_ctx, lp_servicename(snum));
	W_ERROR_HAVE_NO_MEMORY(r->printer_name);
	r->server_name		= talloc_strdup(mem_ctx, ntprinter->info_2->servername);
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
			     const NT_PRINTER_INFO_LEVEL *ntprinter,
			     struct spoolss_DeviceMode *devmode)
{
	struct tm *t;

	t = gmtime(&queue->time);

	r->job_id		= queue->job;

	r->printer_name		= talloc_strdup(mem_ctx, lp_servicename(snum));
	W_ERROR_HAVE_NO_MEMORY(r->printer_name);
	r->server_name		= talloc_strdup(mem_ctx, ntprinter->info_2->servername);
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
	r->driver_name		= talloc_strdup(mem_ctx, ntprinter->info_2->drivername);
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
			     const NT_PRINTER_INFO_LEVEL *ntprinter)
{
	r->job_id		= queue->job;
	r->next_job_id		= 0;
	if (next_queue) {
		r->next_job_id	= next_queue->job;
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
                              const NT_PRINTER_INFO_LEVEL *ntprinter,
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
					ntprinter);
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
                              const NT_PRINTER_INFO_LEVEL *ntprinter,
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

		devmode = construct_dev_mode(info, lp_const_servicename(snum));
		if (!devmode) {
			result = WERR_NOMEM;
			goto out;
		}

		result = fill_job_info2(info,
					&info[i].info2,
					&queue[i],
					i,
					snum,
					ntprinter,
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
                              const NT_PRINTER_INFO_LEVEL *ntprinter,
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
					ntprinter);
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

WERROR _spoolss_EnumJobs(pipes_struct *p,
			 struct spoolss_EnumJobs *r)
{
	WERROR result;
	NT_PRINTER_INFO_LEVEL *ntprinter = NULL;
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

	result = get_a_printer(NULL, &ntprinter, 2, lp_servicename(snum));
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	count = print_queue_status(snum, &queue, &prt_status);
	DEBUGADD(4,("count:[%d], status:[%d], [%s]\n",
		count, prt_status.status, prt_status.message));

	if (count == 0) {
		SAFE_FREE(queue);
		free_a_printer(&ntprinter, 2);
		return WERR_OK;
	}

	switch (r->in.level) {
	case 1:
		result = enumjobs_level1(p->mem_ctx, queue, count, snum,
					 ntprinter, r->out.info, r->out.count);
		break;
	case 2:
		result = enumjobs_level2(p->mem_ctx, queue, count, snum,
					 ntprinter, r->out.info, r->out.count);
		break;
	case 3:
		result = enumjobs_level3(p->mem_ctx, queue, count, snum,
					 ntprinter, r->out.info, r->out.count);
		break;
	default:
		result = WERR_UNKNOWN_LEVEL;
		break;
	}

	SAFE_FREE(queue);
	free_a_printer(&ntprinter, 2);

	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(p->mem_ctx,
						     spoolss_EnumJobs, NULL,
						     *r->out.info, r->in.level,
						     *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************
 _spoolss_ScheduleJob
****************************************************************/

WERROR _spoolss_ScheduleJob(pipes_struct *p,
			    struct spoolss_ScheduleJob *r)
{
	return WERR_OK;
}

/****************************************************************
 _spoolss_SetJob
****************************************************************/

WERROR _spoolss_SetJob(pipes_struct *p,
		       struct spoolss_SetJob *r)
{
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
		if (print_job_delete(p->server_info, snum, r->in.job_id, &errcode)) {
			errcode = WERR_OK;
		}
		break;
	case SPOOLSS_JOB_CONTROL_PAUSE:
		if (print_job_pause(p->server_info, snum, r->in.job_id, &errcode)) {
			errcode = WERR_OK;
		}
		break;
	case SPOOLSS_JOB_CONTROL_RESTART:
	case SPOOLSS_JOB_CONTROL_RESUME:
		if (print_job_resume(p->server_info, snum, r->in.job_id, &errcode)) {
			errcode = WERR_OK;
		}
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	return errcode;
}

/****************************************************************************
 Enumerates all printer drivers by level and architecture.
****************************************************************************/

static WERROR enumprinterdrivers_level_by_architecture(TALLOC_CTX *mem_ctx,
						       const char *servername,
						       const char *architecture,
						       uint32_t level,
						       union spoolss_DriverInfo **info_p,
						       uint32_t *count_p)
{
	int i;
	int ndrivers;
	uint32_t version;
	fstring *list = NULL;
	struct spoolss_DriverInfo8 *driver;
	union spoolss_DriverInfo *info = NULL;
	uint32_t count = 0;
	WERROR result = WERR_OK;

	*count_p = 0;
	*info_p = NULL;

	for (version=0; version<DRIVER_MAX_VERSION; version++) {
		list = NULL;
		ndrivers = get_ntdrivers(&list, architecture, version);
		DEBUGADD(4,("we have:[%d] drivers in environment [%s] and version [%d]\n",
			ndrivers, architecture, version));

		if (ndrivers == -1) {
			result = WERR_NOMEM;
			goto out;
		}

		if (ndrivers != 0) {
			info = TALLOC_REALLOC_ARRAY(mem_ctx, info,
						    union spoolss_DriverInfo,
						    count + ndrivers);
			if (!info) {
				DEBUG(0,("enumprinterdrivers_level_by_architecture: "
					"failed to enlarge driver info buffer!\n"));
				result = WERR_NOMEM;
				goto out;
			}
		}

		for (i=0; i<ndrivers; i++) {
			DEBUGADD(5,("\tdriver: [%s]\n", list[i]));
			ZERO_STRUCT(driver);
			result = get_a_printer_driver(mem_ctx, &driver, list[i],
						      architecture, version);
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

			free_a_printer_driver(driver);

			if (!W_ERROR_IS_OK(result)) {
				goto out;
			}
		}

		count += ndrivers;
		SAFE_FREE(list);
	}

 out:
	SAFE_FREE(list);

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
							servername,
							architecture,
							level,
							info_p,
							count_p);
}

/****************************************************************
 _spoolss_EnumPrinterDrivers
****************************************************************/

WERROR _spoolss_EnumPrinterDrivers(pipes_struct *p,
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

	result = enumprinterdrivers_level(p->mem_ctx, cservername,
					  r->in.environment,
					  r->in.level,
					  r->out.info,
					  r->out.count);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(p->mem_ctx,
						     spoolss_EnumPrinterDrivers, NULL,
						     *r->out.info, r->in.level,
						     *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************************
****************************************************************************/

static WERROR fill_form_info_1(TALLOC_CTX *mem_ctx,
			       struct spoolss_FormInfo1 *r,
			       const nt_forms_struct *form)
{
	r->form_name	= talloc_strdup(mem_ctx, form->name);
	W_ERROR_HAVE_NO_MEMORY(r->form_name);

	r->flags	= form->flag;
	r->size.width	= form->width;
	r->size.height	= form->length;
	r->area.left	= form->left;
	r->area.top	= form->top;
	r->area.right	= form->right;
	r->area.bottom	= form->bottom;

	return WERR_OK;
}

/****************************************************************
 spoolss_enumforms_level1
****************************************************************/

static WERROR spoolss_enumforms_level1(TALLOC_CTX *mem_ctx,
				       const nt_forms_struct *builtin_forms,
				       uint32_t num_builtin_forms,
				       const nt_forms_struct *user_forms,
				       uint32_t num_user_forms,
				       union spoolss_FormInfo **info_p,
				       uint32_t *count)
{
	union spoolss_FormInfo *info;
	WERROR result = WERR_OK;
	int i;

	*count = num_builtin_forms + num_user_forms;

	info = TALLOC_ARRAY(mem_ctx, union spoolss_FormInfo, *count);
	W_ERROR_HAVE_NO_MEMORY(info);

	/* construct the list of form structures */
	for (i=0; i<num_builtin_forms; i++) {
		DEBUGADD(6,("Filling builtin form number [%d]\n",i));
		result = fill_form_info_1(info, &info[i].info1,
					  &builtin_forms[i]);
		if (!W_ERROR_IS_OK(result)) {
			goto out;
		}
	}

	for (i=0; i<num_user_forms; i++) {
		DEBUGADD(6,("Filling user form number [%d]\n",i));
		result = fill_form_info_1(info, &info[i+num_builtin_forms].info1,
					  &user_forms[i]);
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
 _spoolss_EnumForms
****************************************************************/

WERROR _spoolss_EnumForms(pipes_struct *p,
			  struct spoolss_EnumForms *r)
{
	WERROR result;
	nt_forms_struct *user_forms = NULL;
	nt_forms_struct *builtin_forms = NULL;
	uint32_t num_user_forms;
	uint32_t num_builtin_forms;

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

	num_builtin_forms = get_builtin_ntforms(&builtin_forms);
	DEBUGADD(5,("Number of builtin forms [%d]\n", num_builtin_forms));
	num_user_forms = get_ntforms(&user_forms);
	DEBUGADD(5,("Number of user forms [%d]\n", num_user_forms));

	if (num_user_forms + num_builtin_forms == 0) {
		SAFE_FREE(builtin_forms);
		SAFE_FREE(user_forms);
		return WERR_NO_MORE_ITEMS;
	}

	switch (r->in.level) {
	case 1:
		result = spoolss_enumforms_level1(p->mem_ctx,
						  builtin_forms,
						  num_builtin_forms,
						  user_forms,
						  num_user_forms,
						  r->out.info,
						  r->out.count);
		break;
	default:
		result = WERR_UNKNOWN_LEVEL;
		break;
	}

	SAFE_FREE(user_forms);
	SAFE_FREE(builtin_forms);

	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(p->mem_ctx,
						     spoolss_EnumForms, NULL,
						     *r->out.info, r->in.level,
						     *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************
****************************************************************/

static WERROR find_form_byname(const char *name,
			       nt_forms_struct *form)
{
	nt_forms_struct *list = NULL;
	int num_forms = 0, i = 0;

	if (get_a_builtin_ntform_by_string(name, form)) {
		return WERR_OK;
	}

	num_forms = get_ntforms(&list);
	DEBUGADD(5,("Number of forms [%d]\n", num_forms));

	if (num_forms == 0) {
		return WERR_BADFID;
	}

	/* Check if the requested name is in the list of form structures */
	for (i = 0; i < num_forms; i++) {

		DEBUG(4,("checking form %s (want %s)\n", list[i].name, name));

		if (strequal(name, list[i].name)) {
			DEBUGADD(6,("Found form %s number [%d]\n", name, i));
			*form = list[i];
			SAFE_FREE(list);
			return WERR_OK;
		}
	}

	SAFE_FREE(list);

	return WERR_BADFID;
}

/****************************************************************
 _spoolss_GetForm
****************************************************************/

WERROR _spoolss_GetForm(pipes_struct *p,
			struct spoolss_GetForm *r)
{
	WERROR result;
	nt_forms_struct form;

	/* that's an [in out] buffer */

	if (!r->in.buffer && (r->in.offered != 0)) {
		return WERR_INVALID_PARAM;
	}

	DEBUG(4,("_spoolss_GetForm\n"));
	DEBUGADD(5,("Offered buffer size [%d]\n", r->in.offered));
	DEBUGADD(5,("Info level [%d]\n",          r->in.level));

	result = find_form_byname(r->in.form_name, &form);
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(r->out.info);
		return result;
	}

	switch (r->in.level) {
	case 1:
		result = fill_form_info_1(p->mem_ctx,
					  &r->out.info->info1,
					  &form);
		break;

	default:
		result = WERR_UNKNOWN_LEVEL;
		break;
	}

	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(r->out.info);
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION(spoolss_FormInfo, NULL,
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

WERROR _spoolss_EnumPorts(pipes_struct *p,
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
						     spoolss_EnumPorts, NULL,
						     *r->out.info, r->in.level,
						     *r->out.count);
	*r->out.info	= SPOOLSS_BUFFER_OK(*r->out.info, NULL);
	*r->out.count	= SPOOLSS_BUFFER_OK(*r->out.count, 0);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************************
****************************************************************************/

static WERROR spoolss_addprinterex_level_2(pipes_struct *p,
					   const char *server,
					   struct spoolss_SetPrinterInfoCtr *info_ctr,
					   struct spoolss_DeviceMode *devmode,
					   struct security_descriptor *sec_desc,
					   struct spoolss_UserLevelCtr *user_ctr,
					   struct policy_handle *handle)
{
	NT_PRINTER_INFO_LEVEL *printer = NULL;
	fstring	name;
	int	snum;
	WERROR err = WERR_OK;

	if ( !(printer = TALLOC_ZERO_P(NULL, NT_PRINTER_INFO_LEVEL)) ) {
		DEBUG(0,("spoolss_addprinterex_level_2: malloc fail.\n"));
		return WERR_NOMEM;
	}

	/* convert from UNICODE to ASCII - this allocates the info_2 struct inside *printer.*/
	if (!convert_printer_info(info_ctr, printer)) {
		free_a_printer(&printer, 2);
		return WERR_NOMEM;
	}

	/* check to see if the printer already exists */

	if ((snum = print_queue_snum(printer->info_2->sharename)) != -1) {
		DEBUG(5, ("spoolss_addprinterex_level_2: Attempted to add a printer named [%s] when one already existed!\n",
			printer->info_2->sharename));
		free_a_printer(&printer, 2);
		return WERR_PRINTER_ALREADY_EXISTS;
	}

	/* FIXME!!!  smbd should check to see if the driver is installed before
	   trying to add a printer like this  --jerry */

	if (*lp_addprinter_cmd() ) {
		if ( !add_printer_hook(p->mem_ctx, p->server_info->ptok,
				       printer) ) {
			free_a_printer(&printer,2);
			return WERR_ACCESS_DENIED;
		}
	} else {
		DEBUG(0,("spoolss_addprinterex_level_2: add printer for printer %s called and no"
			"smb.conf parameter \"addprinter command\" is defined. This"
			"parameter must exist for this call to succeed\n",
			printer->info_2->sharename ));
	}

	/* use our primary netbios name since get_a_printer() will convert
	   it to what the client expects on a case by case basis */

	slprintf(name, sizeof(name)-1, "\\\\%s\\%s", global_myname(),
             printer->info_2->sharename);


	if ((snum = print_queue_snum(printer->info_2->sharename)) == -1) {
		free_a_printer(&printer,2);
		return WERR_ACCESS_DENIED;
	}

	/* you must be a printer admin to add a new printer */
	if (!print_access_check(p->server_info, snum, PRINTER_ACCESS_ADMINISTER)) {
		free_a_printer(&printer,2);
		return WERR_ACCESS_DENIED;
	}

	/*
	 * Do sanity check on the requested changes for Samba.
	 */

	if (!check_printer_ok(printer->info_2, snum)) {
		free_a_printer(&printer,2);
		return WERR_INVALID_PARAM;
	}

	/*
	 * When a printer is created, the drivername bound to the printer is used
	 * to lookup previously saved driver initialization info, which is then
	 * bound to the new printer, simulating what happens in the Windows arch.
	 */

	if (!devmode)
	{
		set_driver_init(printer, 2);
	}
	else
	{
		/* A valid devmode was included, convert and link it
		*/
		DEBUGADD(10, ("spoolss_addprinterex_level_2: devmode included, converting\n"));

		if (!convert_devicemode(printer->info_2->printername, devmode,
					&printer->info_2->devmode)) {
			return  WERR_NOMEM;
		}
	}

	/* write the ASCII on disk */
	err = mod_a_printer(printer, 2);
	if (!W_ERROR_IS_OK(err)) {
		free_a_printer(&printer,2);
		return err;
	}

	if (!open_printer_hnd(p, handle, name, PRINTER_ACCESS_ADMINISTER)) {
		/* Handle open failed - remove addition. */
		del_a_printer(printer->info_2->sharename);
		free_a_printer(&printer,2);
		ZERO_STRUCTP(handle);
		return WERR_ACCESS_DENIED;
	}

	update_c_setprinter(false);
	free_a_printer(&printer,2);

	return WERR_OK;
}

/****************************************************************
 _spoolss_AddPrinterEx
****************************************************************/

WERROR _spoolss_AddPrinterEx(pipes_struct *p,
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

WERROR _spoolss_AddPrinter(pipes_struct *p,
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
 _spoolss_AddPrinterDriver
****************************************************************/

WERROR _spoolss_AddPrinterDriver(pipes_struct *p,
				 struct spoolss_AddPrinterDriver *r)
{
	WERROR err = WERR_OK;
	char *driver_name = NULL;
	uint32_t version;
	const char *fn;

	switch (p->hdr_req.opnum) {
		case NDR_SPOOLSS_ADDPRINTERDRIVER:
			fn = "_spoolss_AddPrinterDriver";
			break;
		case NDR_SPOOLSS_ADDPRINTERDRIVEREX:
			fn = "_spoolss_AddPrinterDriverEx";
			break;
		default:
			return WERR_INVALID_PARAM;
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
	err = clean_up_driver_struct(p, r->in.info_ctr);
	if (!W_ERROR_IS_OK(err))
		goto done;

	DEBUG(5,("Moving driver to final destination\n"));
	if( !W_ERROR_IS_OK(err = move_driver_to_download_area(p, r->in.info_ctr,
							      &err)) ) {
		goto done;
	}

	if (add_a_printer_driver(p->mem_ctx, r->in.info_ctr, &driver_name, &version)!=0) {
		err = WERR_ACCESS_DENIED;
		goto done;
	}

	/*
	 * I think this is where he DrvUpgradePrinter() hook would be
	 * be called in a driver's interface DLL on a Windows NT 4.0/2k
	 * server.  Right now, we just need to send ourselves a message
	 * to update each printer bound to this driver.   --jerry
	 */

	if (!srv_spoolss_drv_upgrade_printer(driver_name)) {
		DEBUG(0,("%s: Failed to send message about upgrading driver [%s]!\n",
			fn, driver_name));
	}

	/*
	 * Based on the version (e.g. driver destination dir: 0=9x,2=Nt/2k,3=2k/Xp),
	 * decide if the driver init data should be deleted. The rules are:
	 *  1) never delete init data if it is a 9x driver, they don't use it anyway
	 *  2) delete init data only if there is no 2k/Xp driver
	 *  3) always delete init data
	 * The generalized rule is always use init data from the highest order driver.
	 * It is necessary to follow the driver install by an initialization step to
	 * finish off this process.
	*/

	switch (version) {
		/*
		 * 9x printer driver - never delete init data
		*/
		case 0:
			DEBUG(10,("%s: init data not deleted for 9x driver [%s]\n",
				fn, driver_name));
			break;

		/*
		 * Nt or 2k (compatiblity mode) printer driver - only delete init data if
		 * there is no 2k/Xp driver init data for this driver name.
		*/
		case 2:
		{
			struct spoolss_DriverInfo8 *driver1;

			if (!W_ERROR_IS_OK(get_a_printer_driver(p->mem_ctx, &driver1, driver_name, "Windows NT x86", 3))) {
				/*
				 * No 2k/Xp driver found, delete init data (if any) for the new Nt driver.
				*/
				if (!del_driver_init(driver_name))
					DEBUG(6,("%s: del_driver_init(%s) Nt failed!\n",
						fn, driver_name));
			} else {
				/*
				 * a 2k/Xp driver was found, don't delete init data because Nt driver will use it.
				*/
				free_a_printer_driver(driver1);
				DEBUG(10,("%s: init data not deleted for Nt driver [%s]\n",
					fn, driver_name));
			}
		}
		break;

		/*
		 * 2k or Xp printer driver - always delete init data
		*/
		case 3:
			if (!del_driver_init(driver_name))
				DEBUG(6,("%s: del_driver_init(%s) 2k/Xp failed!\n",
					fn, driver_name));
			break;

		default:
			DEBUG(0,("%s: invalid level=%d\n", fn,
				r->in.info_ctr->level));
			break;
 	}


done:
	return err;
}

/****************************************************************
 _spoolss_AddPrinterDriverEx
****************************************************************/

WERROR _spoolss_AddPrinterDriverEx(pipes_struct *p,
				   struct spoolss_AddPrinterDriverEx *r)
{
	struct spoolss_AddPrinterDriver a;

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

	a.in.servername		= r->in.servername;
	a.in.info_ctr		= r->in.info_ctr;

	return _spoolss_AddPrinterDriver(p, &a);
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

WERROR _spoolss_GetPrinterDriverDirectory(pipes_struct *p,
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

	*r->out.needed	= SPOOLSS_BUFFER_UNION(spoolss_DriverDirectoryInfo, NULL,
					       r->out.info, r->in.level);
	r->out.info	= SPOOLSS_BUFFER_OK(r->out.info, NULL);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************
 _spoolss_EnumPrinterData
****************************************************************/

WERROR _spoolss_EnumPrinterData(pipes_struct *p,
				struct spoolss_EnumPrinterData *r)
{
	NT_PRINTER_INFO_LEVEL *printer = NULL;
	Printer_entry 	*Printer = find_printer_index_by_hnd(p, r->in.handle);
	int 		snum;
	WERROR 		result;
	struct regval_blob	*val = NULL;
	NT_PRINTER_DATA *p_data;
	int		i, key_index, num_values;
	int		name_length;

	*r->out.value_needed	= 0;
	*r->out.type		= REG_NONE;
	*r->out.data_needed	= 0;

	DEBUG(5,("_spoolss_EnumPrinterData\n"));

	if (!Printer) {
		DEBUG(2,("_spoolss_EnumPrinterData: Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}

	result = get_a_printer(Printer, &printer, 2, lp_const_servicename(snum));
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	p_data = printer->info_2->data;
	key_index = lookup_printerkey( p_data, SPOOL_PRINTERDATA_KEY );

	result = WERR_OK;

	/*
	 * The NT machine wants to know the biggest size of value and data
	 *
	 * cf: MSDN EnumPrinterData remark section
	 */

	if (!r->in.value_offered && !r->in.data_offered && (key_index != -1)) {

		uint32_t biggest_valuesize = 0;
		uint32_t biggest_datasize = 0;

		DEBUGADD(6,("Activating NT mega-hack to find sizes\n"));

		num_values = regval_ctr_numvals( p_data->keys[key_index].values );

		for ( i=0; i<num_values; i++ )
		{
			val = regval_ctr_specific_value( p_data->keys[key_index].values, i );

			name_length = strlen(val->valuename);
			if ( strlen(val->valuename) > biggest_valuesize )
				biggest_valuesize = name_length;

			if ( val->size > biggest_datasize )
				biggest_datasize = val->size;

			DEBUG(6,("current values: [%d], [%d]\n", biggest_valuesize,
				biggest_datasize));
		}

		/* the value is an UNICODE string but real_value_size is the length
		   in bytes including the trailing 0 */

		*r->out.value_needed = 2 * (1 + biggest_valuesize);
		*r->out.data_needed  = biggest_datasize;

		DEBUG(6,("final values: [%d], [%d]\n",
			*r->out.value_needed, *r->out.data_needed));

		goto done;
	}

	/*
	 * the value len is wrong in NT sp3
	 * that's the number of bytes not the number of unicode chars
	 */

	if (key_index != -1) {
		val = regval_ctr_specific_value(p_data->keys[key_index].values,
						r->in.enum_index);
	}

	if (!val) {

		/* out_value should default to "" or else NT4 has
		   problems unmarshalling the response */

		if (r->in.value_offered) {
			*r->out.value_needed = 1;
			r->out.value_name = talloc_strdup(r, "");
			if (!r->out.value_name) {
				result = WERR_NOMEM;
				goto done;
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
			r->out.value_name = talloc_strdup(r, regval_name(val));
			if (!r->out.value_name) {
				result = WERR_NOMEM;
				goto done;
			}
			*r->out.value_needed = strlen_m_term(regval_name(val)) * 2;
		} else {
			r->out.value_name = NULL;
			*r->out.value_needed = 0;
		}

		/* type */

		*r->out.type = regval_type(val);

		/* data - counted in bytes */

		/*
		 * See the section "Dynamically Typed Query Parameters"
		 * in MS-RPRN.
		 */

		if (r->out.data && regval_data_p(val) &&
				regval_size(val) && r->in.data_offered) {
			memcpy(r->out.data, regval_data_p(val),
				MIN(regval_size(val),r->in.data_offered));
		}

		*r->out.data_needed = regval_size(val);
	}

done:
	free_a_printer(&printer, 2);
	return result;
}

/****************************************************************
 _spoolss_SetPrinterData
****************************************************************/

WERROR _spoolss_SetPrinterData(pipes_struct *p,
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

WERROR _spoolss_ResetPrinter(pipes_struct *p,
			     struct spoolss_ResetPrinter *r)
{
	Printer_entry 	*Printer = find_printer_index_by_hnd(p, r->in.handle);
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

WERROR _spoolss_DeletePrinterData(pipes_struct *p,
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

WERROR _spoolss_AddForm(pipes_struct *p,
			struct spoolss_AddForm *r)
{
	struct spoolss_AddFormInfo1 *form = r->in.info.info1;
	nt_forms_struct tmpForm;
	int snum = -1;
	WERROR status = WERR_OK;
	NT_PRINTER_INFO_LEVEL *printer = NULL;
	SE_PRIV se_printop = SE_PRINT_OPERATOR;

	int count=0;
	nt_forms_struct *list=NULL;
	Printer_entry *Printer = find_printer_index_by_hnd(p, r->in.handle);

	DEBUG(5,("_spoolss_AddForm\n"));

	if (!Printer) {
		DEBUG(2,("_spoolss_AddForm: Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}


	/* forms can be added on printer of on the print server handle */

	if ( Printer->printer_type == SPLHND_PRINTER )
	{
		if (!get_printer_snum(p, r->in.handle, &snum, NULL))
	                return WERR_BADFID;

		status = get_a_printer(Printer, &printer, 2, lp_const_servicename(snum));
        	if (!W_ERROR_IS_OK(status))
			goto done;
	}

	/* if the user is not root, doesn't have SE_PRINT_OPERATOR privilege,
	   and not a printer admin, then fail */

	if ((p->server_info->utok.uid != sec_initial_uid()) &&
	     !user_has_privileges(p->server_info->ptok, &se_printop) &&
	     !token_contains_name_in_list(uidtoname(p->server_info->utok.uid),
					  pdb_get_domain(p->server_info->sam_account),
					  NULL,
					  p->server_info->ptok,
					  lp_printer_admin(snum))) {
		DEBUG(2,("_spoolss_Addform: denied by insufficient permissions.\n"));
		return WERR_ACCESS_DENIED;
	}

	/* can't add if builtin */

	if (get_a_builtin_ntform_by_string(form->form_name, &tmpForm)) {
		status = WERR_FILE_EXISTS;
		goto done;
	}

	count = get_ntforms(&list);

	if(!add_a_form(&list, form, &count)) {
		status =  WERR_NOMEM;
		goto done;
	}

	become_root();
	write_ntforms(&list, count);
	unbecome_root();

	/*
	 * ChangeID must always be set if this is a printer
	 */

	if ( Printer->printer_type == SPLHND_PRINTER )
		status = mod_a_printer(printer, 2);

done:
	if ( printer )
		free_a_printer(&printer, 2);
	SAFE_FREE(list);

	return status;
}

/****************************************************************
 _spoolss_DeleteForm
****************************************************************/

WERROR _spoolss_DeleteForm(pipes_struct *p,
			   struct spoolss_DeleteForm *r)
{
	const char *form_name = r->in.form_name;
	nt_forms_struct tmpForm;
	int count=0;
	nt_forms_struct *list=NULL;
	Printer_entry *Printer = find_printer_index_by_hnd(p, r->in.handle);
	int snum = -1;
	WERROR status = WERR_OK;
	NT_PRINTER_INFO_LEVEL *printer = NULL;
	SE_PRIV se_printop = SE_PRINT_OPERATOR;
	bool ret = false;

	DEBUG(5,("_spoolss_DeleteForm\n"));

	if (!Printer) {
		DEBUG(2,("_spoolss_DeleteForm: Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	/* forms can be deleted on printer of on the print server handle */

	if ( Printer->printer_type == SPLHND_PRINTER )
	{
		if (!get_printer_snum(p, r->in.handle, &snum, NULL))
	                return WERR_BADFID;

		status = get_a_printer(Printer, &printer, 2, lp_const_servicename(snum));
        	if (!W_ERROR_IS_OK(status))
			goto done;
	}

	if ((p->server_info->utok.uid != sec_initial_uid()) &&
	     !user_has_privileges(p->server_info->ptok, &se_printop) &&
	     !token_contains_name_in_list(uidtoname(p->server_info->utok.uid),
					  pdb_get_domain(p->server_info->sam_account),
					  NULL,
					  p->server_info->ptok,
					  lp_printer_admin(snum))) {
		DEBUG(2,("_spoolss_DeleteForm: denied by insufficient permissions.\n"));
		return WERR_ACCESS_DENIED;
	}


	/* can't delete if builtin */

	if (get_a_builtin_ntform_by_string(form_name,&tmpForm)) {
		status = WERR_INVALID_PARAM;
		goto done;
	}

	count = get_ntforms(&list);

	become_root();
	ret = delete_a_form(&list, form_name, &count, &status);
	unbecome_root();
	if (ret == false) {
		goto done;
	}

	/*
	 * ChangeID must always be set if this is a printer
	 */

	if ( Printer->printer_type == SPLHND_PRINTER )
		status = mod_a_printer(printer, 2);

done:
	if ( printer )
		free_a_printer(&printer, 2);
	SAFE_FREE(list);

	return status;
}

/****************************************************************
 _spoolss_SetForm
****************************************************************/

WERROR _spoolss_SetForm(pipes_struct *p,
			struct spoolss_SetForm *r)
{
	struct spoolss_AddFormInfo1 *form = r->in.info.info1;
	nt_forms_struct tmpForm;
	int snum = -1;
	WERROR status = WERR_OK;
	NT_PRINTER_INFO_LEVEL *printer = NULL;
	SE_PRIV se_printop = SE_PRINT_OPERATOR;

	int count=0;
	nt_forms_struct *list=NULL;
	Printer_entry *Printer = find_printer_index_by_hnd(p, r->in.handle);

	DEBUG(5,("_spoolss_SetForm\n"));

	if (!Printer) {
		DEBUG(2,("_spoolss_SetForm: Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	/* forms can be modified on printer of on the print server handle */

	if ( Printer->printer_type == SPLHND_PRINTER )
	{
		if (!get_printer_snum(p, r->in.handle, &snum, NULL))
	                return WERR_BADFID;

		status = get_a_printer(Printer, &printer, 2, lp_const_servicename(snum));
        	if (!W_ERROR_IS_OK(status))
			goto done;
	}

	/* if the user is not root, doesn't have SE_PRINT_OPERATOR privilege,
	   and not a printer admin, then fail */

	if ((p->server_info->utok.uid != sec_initial_uid()) &&
	     !user_has_privileges(p->server_info->ptok, &se_printop) &&
	     !token_contains_name_in_list(uidtoname(p->server_info->utok.uid),
					  pdb_get_domain(p->server_info->sam_account),
					  NULL,
					  p->server_info->ptok,
					  lp_printer_admin(snum))) {
		DEBUG(2,("_spoolss_Setform: denied by insufficient permissions.\n"));
		return WERR_ACCESS_DENIED;
	}

	/* can't set if builtin */
	if (get_a_builtin_ntform_by_string(form->form_name, &tmpForm)) {
		status = WERR_INVALID_PARAM;
		goto done;
	}

	count = get_ntforms(&list);
	update_a_form(&list, form, count);
	become_root();
	write_ntforms(&list, count);
	unbecome_root();

	/*
	 * ChangeID must always be set if this is a printer
	 */

	if ( Printer->printer_type == SPLHND_PRINTER )
		status = mod_a_printer(printer, 2);


done:
	if ( printer )
		free_a_printer(&printer, 2);
	SAFE_FREE(list);

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

WERROR _spoolss_EnumPrintProcessors(pipes_struct *p,
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
						     spoolss_EnumPrintProcessors, NULL,
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

WERROR _spoolss_EnumPrintProcDataTypes(pipes_struct *p,
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

	switch (r->in.level) {
	case 1:
		result = enumprintprocdatatypes_level_1(p->mem_ctx, r->out.info,
							r->out.count);
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION_ARRAY(p->mem_ctx,
						     spoolss_EnumPrintProcDataTypes, NULL,
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

WERROR _spoolss_EnumMonitors(pipes_struct *p,
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
						     spoolss_EnumMonitors, NULL,
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
			     const NT_PRINTER_INFO_LEVEL *ntprinter,
			     uint32_t jobid,
			     struct spoolss_JobInfo1 *r)
{
	int i = 0;
	bool found = false;

	for (i=0; i<count && found == false; i++) {
		if (queue[i].job == (int)jobid) {
			found = true;
		}
	}

	if (found == false) {
		/* NT treats not found as bad param... yet another bad choice */
		return WERR_INVALID_PARAM;
	}

	return fill_job_info1(mem_ctx,
			      r,
			      &queue[i-1],
			      i,
			      snum,
			      ntprinter);
}

/****************************************************************************
****************************************************************************/

static WERROR getjob_level_2(TALLOC_CTX *mem_ctx,
			     const print_queue_struct *queue,
			     int count, int snum,
			     const NT_PRINTER_INFO_LEVEL *ntprinter,
			     uint32_t jobid,
			     struct spoolss_JobInfo2 *r)
{
	int i = 0;
	bool found = false;
	struct spoolss_DeviceMode *devmode;
	NT_DEVICEMODE *nt_devmode;
	WERROR result;

	for (i=0; i<count && found == false; i++) {
		if (queue[i].job == (int)jobid) {
			found = true;
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

	nt_devmode = print_job_devmode(lp_const_servicename(snum), jobid);
	if (nt_devmode) {
		devmode = TALLOC_ZERO_P(mem_ctx, struct spoolss_DeviceMode);
		W_ERROR_HAVE_NO_MEMORY(devmode);
		result = convert_nt_devicemode(devmode, devmode, nt_devmode);
		if (!W_ERROR_IS_OK(result)) {
			return result;
		}
	} else {
		devmode = construct_dev_mode(mem_ctx, lp_const_servicename(snum));
		W_ERROR_HAVE_NO_MEMORY(devmode);
	}

	return fill_job_info2(mem_ctx,
			      r,
			      &queue[i-1],
			      i,
			      snum,
			      ntprinter,
			      devmode);
}

/****************************************************************
 _spoolss_GetJob
****************************************************************/

WERROR _spoolss_GetJob(pipes_struct *p,
		       struct spoolss_GetJob *r)
{
	WERROR result = WERR_OK;
	NT_PRINTER_INFO_LEVEL *ntprinter = NULL;
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

	result = get_a_printer(NULL, &ntprinter, 2, lp_servicename(snum));
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	count = print_queue_status(snum, &queue, &prt_status);

	DEBUGADD(4,("count:[%d], prt_status:[%d], [%s]\n",
	             count, prt_status.status, prt_status.message));

	switch (r->in.level) {
	case 1:
		result = getjob_level_1(p->mem_ctx,
					queue, count, snum, ntprinter,
					r->in.job_id, &r->out.info->info1);
		break;
	case 2:
		result = getjob_level_2(p->mem_ctx,
					queue, count, snum, ntprinter,
					r->in.job_id, &r->out.info->info2);
		break;
	default:
		result = WERR_UNKNOWN_LEVEL;
		break;
	}

	SAFE_FREE(queue);
	free_a_printer(&ntprinter, 2);

	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(r->out.info);
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION(spoolss_JobInfo, NULL,
					       r->out.info, r->in.level);
	r->out.info	= SPOOLSS_BUFFER_OK(r->out.info, NULL);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_INSUFFICIENT_BUFFER);
}

/****************************************************************
 _spoolss_GetPrinterDataEx
****************************************************************/

WERROR _spoolss_GetPrinterDataEx(pipes_struct *p,
				 struct spoolss_GetPrinterDataEx *r)
{

	Printer_entry 	*Printer = find_printer_index_by_hnd(p, r->in.handle);
	struct regval_blob		*val = NULL;
	NT_PRINTER_INFO_LEVEL 	*printer = NULL;
	int 			snum = 0;
	WERROR result = WERR_OK;
	DATA_BLOB blob;

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
			goto done;
		}

		result = push_spoolss_PrinterData(p->mem_ctx, &blob,
						  *r->out.type, &data);
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}

		*r->out.needed = blob.length;

		if (r->in.offered >= *r->out.needed) {
			memcpy(r->out.data, blob.data, blob.length);
		}

		return SPOOLSS_BUFFER_OK(WERR_OK, WERR_MORE_DATA);
	}

	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		result = WERR_BADFID;
		goto done;
	}

	result = get_a_printer(Printer, &printer, 2, lp_servicename(snum));
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	/* check to see if the keyname is valid */
	if (!strlen(r->in.key_name)) {
		result = WERR_INVALID_PARAM;
		goto done;
	}

	/* XP sends this and wants to change id value from the PRINTER_INFO_0 */

	if (strequal(r->in.key_name, SPOOL_PRINTERDATA_KEY) &&
	    strequal(r->in.value_name, "ChangeId")) {
		*r->out.type = REG_DWORD;
		*r->out.needed = 4;
		if (r->in.offered >= *r->out.needed) {
			SIVAL(r->out.data, 0, printer->info_2->changeid);
			result = WERR_OK;
		}
		goto done;
	}

	if (lookup_printerkey(printer->info_2->data, r->in.key_name) == -1) {
		DEBUG(4,("_spoolss_GetPrinterDataEx: "
			"Invalid keyname [%s]\n", r->in.key_name ));
		result = WERR_BADFILE;
		goto done;
	}

	val = get_printer_data(printer->info_2,
			       r->in.key_name, r->in.value_name);
	if (!val) {
		result = WERR_BADFILE;
		goto done;
	}

	*r->out.needed = regval_size(val);
	*r->out.type = regval_type(val);

	if (r->in.offered >= *r->out.needed) {
		memcpy(r->out.data, regval_data_p(val), regval_size(val));
	}
 done:
	if (printer) {
		free_a_printer(&printer, 2);
	}

	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	*r->out.type    = SPOOLSS_BUFFER_OK(*r->out.type, REG_NONE);
	r->out.data     = SPOOLSS_BUFFER_OK(r->out.data, r->out.data);

	return SPOOLSS_BUFFER_OK(WERR_OK, WERR_MORE_DATA);
}

/****************************************************************
 _spoolss_SetPrinterDataEx
****************************************************************/

WERROR _spoolss_SetPrinterDataEx(pipes_struct *p,
				 struct spoolss_SetPrinterDataEx *r)
{
	NT_PRINTER_INFO_LEVEL 	*printer = NULL;
	int 			snum = 0;
	WERROR 			result = WERR_OK;
	Printer_entry 		*Printer = find_printer_index_by_hnd(p, r->in.handle);
	char			*oid_string;

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

	result = get_a_printer(Printer, &printer, 2, lp_servicename(snum));
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	/* check for OID in valuename */

	oid_string = strchr(r->in.value_name, ',');
	if (oid_string) {
		*oid_string = '\0';
		oid_string++;
	}

	/*
	 * When client side code sets a magic printer data key, detect it and save
	 * the current printer data and the magic key's data (its the DEVMODE) for
	 * future printer/driver initializations.
	 */
	if ((r->in.type == REG_BINARY) && strequal(r->in.value_name, PHANTOM_DEVMODE_KEY)) {
		/* Set devmode and printer initialization info */
		result = save_driver_init(printer, 2, r->in.data, r->in.offered);

		srv_spoolss_reset_printerdata(printer->info_2->drivername);

		goto done;
	}

	/* save the registry data */

	result = set_printer_dataex(printer, r->in.key_name, r->in.value_name,
				    r->in.type, r->in.data, r->in.offered);

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

			set_printer_dataex(printer, str, r->in.value_name,
					   REG_SZ, (uint8_t *)oid_string,
					   strlen(oid_string)+1);
		}

		result = mod_a_printer(printer, 2);
	}

 done:
	free_a_printer(&printer, 2);

	return result;
}

/****************************************************************
 _spoolss_DeletePrinterDataEx
****************************************************************/

WERROR _spoolss_DeletePrinterDataEx(pipes_struct *p,
				    struct spoolss_DeletePrinterDataEx *r)
{
	NT_PRINTER_INFO_LEVEL 	*printer = NULL;
	int 		snum=0;
	WERROR 		status = WERR_OK;
	Printer_entry 	*Printer = find_printer_index_by_hnd(p, r->in.handle);

	DEBUG(5,("_spoolss_DeletePrinterDataEx\n"));

	if (!Printer) {
		DEBUG(2,("_spoolss_DeletePrinterDataEx: "
			"Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	if (!get_printer_snum(p, r->in.handle, &snum, NULL))
		return WERR_BADFID;

	if (Printer->access_granted != PRINTER_ACCESS_ADMINISTER) {
		DEBUG(3, ("_spoolss_DeletePrinterDataEx: "
			"printer properties change denied by handle\n"));
		return WERR_ACCESS_DENIED;
	}

	if (!r->in.value_name || !r->in.key_name) {
		return WERR_NOMEM;
	}

	status = get_a_printer(Printer, &printer, 2, lp_const_servicename(snum));
	if (!W_ERROR_IS_OK(status))
		return status;

	status = delete_printer_dataex( printer, r->in.key_name, r->in.value_name );

	if ( W_ERROR_IS_OK(status) )
		mod_a_printer( printer, 2 );

	free_a_printer(&printer, 2);

	return status;
}

/****************************************************************
 _spoolss_EnumPrinterKey
****************************************************************/

WERROR _spoolss_EnumPrinterKey(pipes_struct *p,
			       struct spoolss_EnumPrinterKey *r)
{
	fstring		*keynames = NULL;
	int		num_keys;
	Printer_entry 	*Printer = find_printer_index_by_hnd(p, r->in.handle);
	NT_PRINTER_DATA	*data;
	NT_PRINTER_INFO_LEVEL 	*printer = NULL;
	int 		snum = 0;
	WERROR		result = WERR_BADFILE;
	int i;
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

	result = get_a_printer(Printer, &printer, 2, lp_const_servicename(snum));
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	/* get the list of subkey names */

	data = printer->info_2->data;

	num_keys = get_printer_subkeys(data, r->in.key_name, &keynames);
	if (num_keys == -1) {
		result = WERR_BADFILE;
		goto done;
	}

	array = talloc_zero_array(r->out.key_buffer, const char *, num_keys + 2);
	if (!array) {
		result = WERR_NOMEM;
		goto done;
	}

	if (!num_keys) {
		array[0] = talloc_strdup(array, "");
		if (!array[0]) {
			result = WERR_NOMEM;
			goto done;
		}
	}

	for (i=0; i < num_keys; i++) {

		DEBUG(10,("_spoolss_EnumPrinterKey: adding keyname: %s\n",
			keynames[i]));

		array[i] = talloc_strdup(array, keynames[i]);
		if (!array[i]) {
			result = WERR_NOMEM;
			goto done;
		}
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

	free_a_printer(&printer, 2);
	SAFE_FREE(keynames);

	return result;
}

/****************************************************************
 _spoolss_DeletePrinterKey
****************************************************************/

WERROR _spoolss_DeletePrinterKey(pipes_struct *p,
				 struct spoolss_DeletePrinterKey *r)
{
	Printer_entry 		*Printer = find_printer_index_by_hnd(p, r->in.handle);
	NT_PRINTER_INFO_LEVEL 	*printer = NULL;
	int 			snum=0;
	WERROR			status;

	DEBUG(5,("_spoolss_DeletePrinterKey\n"));

	if (!Printer) {
		DEBUG(2,("_spoolss_DeletePrinterKey: Invalid handle (%s:%u:%u).\n",
			OUR_HANDLE(r->in.handle)));
		return WERR_BADFID;
	}

	/* if keyname == NULL, return error */

	if ( !r->in.key_name )
		return WERR_INVALID_PARAM;

	if (!get_printer_snum(p, r->in.handle, &snum, NULL))
		return WERR_BADFID;

	if (Printer->access_granted != PRINTER_ACCESS_ADMINISTER) {
		DEBUG(3, ("_spoolss_DeletePrinterKey: "
			"printer properties change denied by handle\n"));
		return WERR_ACCESS_DENIED;
	}

	status = get_a_printer(Printer, &printer, 2, lp_const_servicename(snum));
	if (!W_ERROR_IS_OK(status))
		return status;

	/* delete the key and all subneys */

	status = delete_all_printer_data( printer->info_2, r->in.key_name );

	if ( W_ERROR_IS_OK(status) )
		status = mod_a_printer(printer, 2);

	free_a_printer( &printer, 2 );

	return status;
}

/****************************************************************
****************************************************************/

static WERROR registry_value_to_printer_enum_value(TALLOC_CTX *mem_ctx,
						   struct regval_blob *v,
						   struct spoolss_PrinterEnumValues *r)
{
	r->data = TALLOC_ZERO_P(mem_ctx, DATA_BLOB);
	W_ERROR_HAVE_NO_MEMORY(r->data);

	r->value_name	= talloc_strdup(mem_ctx, regval_name(v));
	W_ERROR_HAVE_NO_MEMORY(r->value_name);

	r->type		= regval_type(v);
	r->data_length	= regval_size(v);

	if (r->data_length) {
		*r->data = data_blob_talloc(r->data, regval_data_p(v), regval_size(v));
	}

	return WERR_OK;
}

/****************************************************************
 _spoolss_EnumPrinterDataEx
****************************************************************/

WERROR _spoolss_EnumPrinterDataEx(pipes_struct *p,
				  struct spoolss_EnumPrinterDataEx *r)
{
	uint32_t	count = 0;
	NT_PRINTER_INFO_LEVEL 	*printer = NULL;
	struct spoolss_PrinterEnumValues *info = NULL;
	NT_PRINTER_DATA		*p_data;
	Printer_entry 	*Printer = find_printer_index_by_hnd(p, r->in.handle);
	int 		snum;
	WERROR 		result;
	int		key_index;
	int		i;

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

	/* get the printer off of disk */

	if (!get_printer_snum(p, r->in.handle, &snum, NULL)) {
		return WERR_BADFID;
	}

	ZERO_STRUCT(printer);
	result = get_a_printer(Printer, &printer, 2, lp_const_servicename(snum));
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	/* now look for a match on the key name */

	p_data = printer->info_2->data;

	key_index = lookup_printerkey(p_data, r->in.key_name);
	if (key_index == -1) {
		DEBUG(10,("_spoolss_EnumPrinterDataEx: Unknown keyname [%s]\n",
			r->in.key_name));
		result = WERR_INVALID_PARAM;
		goto done;
	}

	/* allocate the memory for the array of pointers -- if necessary */

	count = regval_ctr_numvals(p_data->keys[key_index].values);
	if (!count) {
		result = WERR_OK; /* ??? */
		goto done;
	}

	info = TALLOC_ZERO_ARRAY(p->mem_ctx,
				 struct spoolss_PrinterEnumValues,
				 count);
	if (!info) {
		DEBUG(0,("_spoolss_EnumPrinterDataEx: talloc() failed\n"));
		result = WERR_NOMEM;
		goto done;
	}

	/*
	 * loop through all params and build the array to pass
	 * back to the  client
	 */

	for (i=0; i < count; i++) {

		struct regval_blob	*val;

		/* lookup the registry value */

		val = regval_ctr_specific_value(p_data->keys[key_index].values, i);

		DEBUG(10,("retrieved value number [%d] [%s]\n", i, regval_name(val)));

		/* copy the data */

		result = registry_value_to_printer_enum_value(info, val, &info[i]);
		if (!W_ERROR_IS_OK(result)) {
			goto done;
		}
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

	if (printer) {
		free_a_printer(&printer, 2);
	}

	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_ARRAY(p->mem_ctx,
					       spoolss_EnumPrinterDataEx, NULL,
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

WERROR _spoolss_GetPrintProcessorDirectory(pipes_struct *p,
					   struct spoolss_GetPrintProcessorDirectory *r)
{
	WERROR result;

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
	 * server - Guenther */

	result = getprintprocessordirectory_level_1(p->mem_ctx,
						    NULL, /* r->in.server */
						    r->in.environment,
						    &r->out.info->info1);
	if (!W_ERROR_IS_OK(result)) {
		TALLOC_FREE(r->out.info);
		return result;
	}

	*r->out.needed	= SPOOLSS_BUFFER_UNION(spoolss_PrintProcessorDirectoryInfo, NULL,
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

	ndr_err = ndr_push_struct_blob(buf, mem_ctx, NULL, &ui,
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
			       NT_USER_TOKEN *token, DATA_BLOB *in,
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
	ndr_err = ndr_pull_struct_blob(buf, mem_ctx, NULL, port1,
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
	ndr_err = ndr_pull_struct_blob(buf, mem_ctx, NULL, port2,
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
			     NT_USER_TOKEN *token, DATA_BLOB *in,
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
				     NT_USER_TOKEN *token, const char *command,
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
				 NT_USER_TOKEN *token, DATA_BLOB *in,
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
				       NT_USER_TOKEN *token, const char *command,
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

WERROR _spoolss_XcvData(pipes_struct *p,
			struct spoolss_XcvData *r)
{
	Printer_entry *Printer = find_printer_index_by_hnd(p, r->in.handle);
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
						p->server_info->ptok,
						r->in.function_name,
						&r->in.in_data, &out_data,
						r->out.needed);
		break;
	case SPLHND_PORTMON_LOCAL:
		werror = process_xcvlocal_command(p->mem_ctx,
						  p->server_info->ptok,
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

WERROR _spoolss_AddPrintProcessor(pipes_struct *p,
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

WERROR _spoolss_AddPort(pipes_struct *p,
			struct spoolss_AddPort *r)
{
	/* do what w2k3 does */

	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_GetPrinterDriver
****************************************************************/

WERROR _spoolss_GetPrinterDriver(pipes_struct *p,
				 struct spoolss_GetPrinterDriver *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_ReadPrinter
****************************************************************/

WERROR _spoolss_ReadPrinter(pipes_struct *p,
			    struct spoolss_ReadPrinter *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_WaitForPrinterChange
****************************************************************/

WERROR _spoolss_WaitForPrinterChange(pipes_struct *p,
				     struct spoolss_WaitForPrinterChange *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_ConfigurePort
****************************************************************/

WERROR _spoolss_ConfigurePort(pipes_struct *p,
			      struct spoolss_ConfigurePort *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_DeletePort
****************************************************************/

WERROR _spoolss_DeletePort(pipes_struct *p,
			   struct spoolss_DeletePort *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_CreatePrinterIC
****************************************************************/

WERROR _spoolss_CreatePrinterIC(pipes_struct *p,
				struct spoolss_CreatePrinterIC *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_PlayGDIScriptOnPrinterIC
****************************************************************/

WERROR _spoolss_PlayGDIScriptOnPrinterIC(pipes_struct *p,
					 struct spoolss_PlayGDIScriptOnPrinterIC *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_DeletePrinterIC
****************************************************************/

WERROR _spoolss_DeletePrinterIC(pipes_struct *p,
				struct spoolss_DeletePrinterIC *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_AddPrinterConnection
****************************************************************/

WERROR _spoolss_AddPrinterConnection(pipes_struct *p,
				     struct spoolss_AddPrinterConnection *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_DeletePrinterConnection
****************************************************************/

WERROR _spoolss_DeletePrinterConnection(pipes_struct *p,
					struct spoolss_DeletePrinterConnection *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_PrinterMessageBox
****************************************************************/

WERROR _spoolss_PrinterMessageBox(pipes_struct *p,
				  struct spoolss_PrinterMessageBox *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_AddMonitor
****************************************************************/

WERROR _spoolss_AddMonitor(pipes_struct *p,
			   struct spoolss_AddMonitor *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_DeleteMonitor
****************************************************************/

WERROR _spoolss_DeleteMonitor(pipes_struct *p,
			      struct spoolss_DeleteMonitor *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_DeletePrintProcessor
****************************************************************/

WERROR _spoolss_DeletePrintProcessor(pipes_struct *p,
				     struct spoolss_DeletePrintProcessor *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_AddPrintProvidor
****************************************************************/

WERROR _spoolss_AddPrintProvidor(pipes_struct *p,
				 struct spoolss_AddPrintProvidor *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_DeletePrintProvidor
****************************************************************/

WERROR _spoolss_DeletePrintProvidor(pipes_struct *p,
				    struct spoolss_DeletePrintProvidor *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_FindFirstPrinterChangeNotification
****************************************************************/

WERROR _spoolss_FindFirstPrinterChangeNotification(pipes_struct *p,
						   struct spoolss_FindFirstPrinterChangeNotification *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_FindNextPrinterChangeNotification
****************************************************************/

WERROR _spoolss_FindNextPrinterChangeNotification(pipes_struct *p,
						  struct spoolss_FindNextPrinterChangeNotification *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_RouterFindFirstPrinterChangeNotificationOld
****************************************************************/

WERROR _spoolss_RouterFindFirstPrinterChangeNotificationOld(pipes_struct *p,
							    struct spoolss_RouterFindFirstPrinterChangeNotificationOld *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_ReplyOpenPrinter
****************************************************************/

WERROR _spoolss_ReplyOpenPrinter(pipes_struct *p,
				 struct spoolss_ReplyOpenPrinter *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_RouterReplyPrinter
****************************************************************/

WERROR _spoolss_RouterReplyPrinter(pipes_struct *p,
				   struct spoolss_RouterReplyPrinter *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_ReplyClosePrinter
****************************************************************/

WERROR _spoolss_ReplyClosePrinter(pipes_struct *p,
				  struct spoolss_ReplyClosePrinter *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_AddPortEx
****************************************************************/

WERROR _spoolss_AddPortEx(pipes_struct *p,
			  struct spoolss_AddPortEx *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_RouterFindFirstPrinterChangeNotification
****************************************************************/

WERROR _spoolss_RouterFindFirstPrinterChangeNotification(pipes_struct *p,
							 struct spoolss_RouterFindFirstPrinterChangeNotification *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_SpoolerInit
****************************************************************/

WERROR _spoolss_SpoolerInit(pipes_struct *p,
			    struct spoolss_SpoolerInit *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_ResetPrinterEx
****************************************************************/

WERROR _spoolss_ResetPrinterEx(pipes_struct *p,
			       struct spoolss_ResetPrinterEx *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_RouterReplyPrinterEx
****************************************************************/

WERROR _spoolss_RouterReplyPrinterEx(pipes_struct *p,
				     struct spoolss_RouterReplyPrinterEx *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_44
****************************************************************/

WERROR _spoolss_44(pipes_struct *p,
		   struct spoolss_44 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_47
****************************************************************/

WERROR _spoolss_47(pipes_struct *p,
		   struct spoolss_47 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_4a
****************************************************************/

WERROR _spoolss_4a(pipes_struct *p,
		   struct spoolss_4a *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_4b
****************************************************************/

WERROR _spoolss_4b(pipes_struct *p,
		   struct spoolss_4b *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_4c
****************************************************************/

WERROR _spoolss_4c(pipes_struct *p,
		   struct spoolss_4c *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_53
****************************************************************/

WERROR _spoolss_53(pipes_struct *p,
		   struct spoolss_53 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_55
****************************************************************/

WERROR _spoolss_55(pipes_struct *p,
		   struct spoolss_55 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_56
****************************************************************/

WERROR _spoolss_56(pipes_struct *p,
		   struct spoolss_56 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_57
****************************************************************/

WERROR _spoolss_57(pipes_struct *p,
		   struct spoolss_57 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_5a
****************************************************************/

WERROR _spoolss_5a(pipes_struct *p,
		   struct spoolss_5a *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_5b
****************************************************************/

WERROR _spoolss_5b(pipes_struct *p,
		   struct spoolss_5b *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_5c
****************************************************************/

WERROR _spoolss_5c(pipes_struct *p,
		   struct spoolss_5c *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_5d
****************************************************************/

WERROR _spoolss_5d(pipes_struct *p,
		   struct spoolss_5d *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_5e
****************************************************************/

WERROR _spoolss_5e(pipes_struct *p,
		   struct spoolss_5e *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_5f
****************************************************************/

WERROR _spoolss_5f(pipes_struct *p,
		   struct spoolss_5f *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_60
****************************************************************/

WERROR _spoolss_60(pipes_struct *p,
		   struct spoolss_60 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_61
****************************************************************/

WERROR _spoolss_61(pipes_struct *p,
		   struct spoolss_61 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_62
****************************************************************/

WERROR _spoolss_62(pipes_struct *p,
		   struct spoolss_62 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_63
****************************************************************/

WERROR _spoolss_63(pipes_struct *p,
		   struct spoolss_63 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_64
****************************************************************/

WERROR _spoolss_64(pipes_struct *p,
		   struct spoolss_64 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_65
****************************************************************/

WERROR _spoolss_65(pipes_struct *p,
		   struct spoolss_65 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_GetCorePrinterDrivers
****************************************************************/

WERROR _spoolss_GetCorePrinterDrivers(pipes_struct *p,
				      struct spoolss_GetCorePrinterDrivers *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_67
****************************************************************/

WERROR _spoolss_67(pipes_struct *p,
		   struct spoolss_67 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_GetPrinterDriverPackagePath
****************************************************************/

WERROR _spoolss_GetPrinterDriverPackagePath(pipes_struct *p,
					    struct spoolss_GetPrinterDriverPackagePath *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_69
****************************************************************/

WERROR _spoolss_69(pipes_struct *p,
		   struct spoolss_69 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_6a
****************************************************************/

WERROR _spoolss_6a(pipes_struct *p,
		   struct spoolss_6a *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_6b
****************************************************************/

WERROR _spoolss_6b(pipes_struct *p,
		   struct spoolss_6b *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_6c
****************************************************************/

WERROR _spoolss_6c(pipes_struct *p,
		   struct spoolss_6c *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
 _spoolss_6d
****************************************************************/

WERROR _spoolss_6d(pipes_struct *p,
		   struct spoolss_6d *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}
