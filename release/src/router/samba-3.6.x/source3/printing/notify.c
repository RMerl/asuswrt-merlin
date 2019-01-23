/* 
   Unix SMB/Netbios implementation.
   Version 3.0
   printing backend routines
   Copyright (C) Tim Potter, 2002
   Copyright (C) Gerald Carter,         2002
   
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
#include "printing.h"
#include "../librpc/gen_ndr/spoolss.h"
#include "nt_printing.h"
#include "printing/notify.h"
#include "messages.h"
#include "util_tdb.h"

static TALLOC_CTX *send_ctx;

static unsigned int num_messages;

static struct notify_queue {
	struct notify_queue *next, *prev;
	struct spoolss_notify_msg *msg;
	struct timeval tv;
	uint8 *buf;
	size_t buflen;
} *notify_queue_head = NULL;

static struct tevent_timer *notify_event;

static bool print_notify_pid_list(const char *printername, TALLOC_CTX *mem_ctx,
				  size_t *p_num_pids, pid_t **pp_pid_list);

static bool create_send_ctx(void)
{
	if (!send_ctx)
		send_ctx = talloc_init("print notify queue");

	if (!send_ctx)
		return False;

	return True;
}

/****************************************************************************
 Turn a queue name into a snum.
****************************************************************************/

int print_queue_snum(const char *qname)
{
	int snum = lp_servicenumber(qname);
	if (snum == -1 || !lp_print_ok(snum))
		return -1;
	return snum;
}

/*******************************************************************
 Used to decide if we need a short select timeout.
*******************************************************************/

static bool print_notify_messages_pending(void)
{
	return (notify_queue_head != NULL);
}

/*******************************************************************
 Flatten data into a message.
*******************************************************************/

static bool flatten_message(struct notify_queue *q)
{
	struct spoolss_notify_msg *msg = q->msg;
	uint8 *buf = NULL;
	size_t buflen = 0, len;

again:
	len = 0;

	/* Pack header */

	len += tdb_pack(buf + len, buflen - len, "f", msg->printer);

	len += tdb_pack(buf + len, buflen - len, "ddddddd",
			(uint32)q->tv.tv_sec, (uint32)q->tv.tv_usec,
			msg->type, msg->field, msg->id, msg->len, msg->flags);

	/* Pack data */

	if (msg->len == 0)
		len += tdb_pack(buf + len, buflen - len, "dd",
				msg->notify.value[0], msg->notify.value[1]);
	else
		len += tdb_pack(buf + len, buflen - len, "B",
				msg->len, msg->notify.data);

	if (buflen != len) {
		buf = (uint8 *)TALLOC_REALLOC(send_ctx, buf, len);
		if (!buf)
			return False;
		buflen = len;
		goto again;
	}

	q->buf = buf;
	q->buflen = buflen;

	return True;
}

/*******************************************************************
 Send the batched messages - on a per-printer basis.
*******************************************************************/

static void print_notify_send_messages_to_printer(struct messaging_context *msg_ctx,
						  const char *printer,
						  unsigned int timeout)
{
	char *buf;
	struct notify_queue *pq, *pq_next;
	size_t msg_count = 0, offset = 0;
	size_t num_pids = 0;
	size_t i;
	pid_t *pid_list = NULL;
	struct timeval end_time = timeval_zero();

	/* Count the space needed to send the messages. */
	for (pq = notify_queue_head; pq; pq = pq->next) {
		if (strequal(printer, pq->msg->printer)) {
			if (!flatten_message(pq)) {
				DEBUG(0,("print_notify_send_messages: Out of memory\n"));
				talloc_free_children(send_ctx);
				num_messages = 0;
				return;
			}
			offset += (pq->buflen + 4);
			msg_count++;
		}	
	}
	offset += 4; /* For count. */

	buf = (char *)TALLOC(send_ctx, offset);
	if (!buf) {
		DEBUG(0,("print_notify_send_messages: Out of memory\n"));
		talloc_free_children(send_ctx);
		num_messages = 0;
		return;
	}

	offset = 0;
	SIVAL(buf,offset,msg_count);
	offset += 4;
	for (pq = notify_queue_head; pq; pq = pq_next) {
		pq_next = pq->next;

		if (strequal(printer, pq->msg->printer)) {
			SIVAL(buf,offset,pq->buflen);
			offset += 4;
			memcpy(buf + offset, pq->buf, pq->buflen);
			offset += pq->buflen;

			/* Remove from list. */
			DLIST_REMOVE(notify_queue_head, pq);
		}
	}

	DEBUG(5, ("print_notify_send_messages_to_printer: sending %lu print notify message%s to printer %s\n", 
		  (unsigned long)msg_count, msg_count != 1 ? "s" : "", printer));

	/*
	 * Get the list of PID's to send to.
	 */

	if (!print_notify_pid_list(printer, send_ctx, &num_pids, &pid_list))
		return;

	if (timeout != 0) {
		end_time = timeval_current_ofs(timeout, 0);
	}

	for (i = 0; i < num_pids; i++) {
		messaging_send_buf(msg_ctx,
				   pid_to_procid(pid_list[i]),
				   MSG_PRINTER_NOTIFY2 | MSG_FLAG_LOWPRIORITY,
				   (uint8 *)buf, offset);

		if ((timeout != 0) && timeval_expired(&end_time)) {
			break;
		}
	}
}

/*******************************************************************
 Actually send the batched messages.
*******************************************************************/

void print_notify_send_messages(struct messaging_context *msg_ctx,
				unsigned int timeout)
{
	if (!print_notify_messages_pending())
		return;

	if (!create_send_ctx())
		return;

	while (print_notify_messages_pending())
		print_notify_send_messages_to_printer(
			msg_ctx, notify_queue_head->msg->printer, timeout);

	talloc_free_children(send_ctx);
	num_messages = 0;
}

/*******************************************************************
 Event handler to send the messages.
*******************************************************************/

static void print_notify_event_send_messages(struct tevent_context *event_ctx,
					     struct tevent_timer *te,
					     struct timeval now,
					     void *private_data)
{
	struct messaging_context *msg_ctx = talloc_get_type_abort(
		private_data, struct messaging_context);
	/* Remove this timed event handler. */
	TALLOC_FREE(notify_event);

	change_to_root_user();
	print_notify_send_messages(msg_ctx, 0);
}

/**********************************************************************
 deep copy a SPOOLSS_NOTIFY_MSG structure using a TALLOC_CTX
 *********************************************************************/
 
static bool copy_notify2_msg( SPOOLSS_NOTIFY_MSG *to, SPOOLSS_NOTIFY_MSG *from )
{

	if ( !to || !from )
		return False;
	
	memcpy( to, from, sizeof(SPOOLSS_NOTIFY_MSG) );
	
	if ( from->len ) {
		to->notify.data = (char *)TALLOC_MEMDUP(send_ctx, from->notify.data, from->len );
		if ( !to->notify.data ) {
			DEBUG(0,("copy_notify2_msg: TALLOC_MEMDUP() of size [%d] failed!\n", from->len ));
			return False;
		}
	}
	

	return True;
}

/*******************************************************************
 Batch up print notify messages.
*******************************************************************/

static void send_spoolss_notify2_msg(struct tevent_context *ev,
				     struct messaging_context *msg_ctx,
				     SPOOLSS_NOTIFY_MSG *msg)
{
	struct notify_queue *pnqueue, *tmp_ptr;

	/*
	 * Ensure we only have one job total_bytes and job total_pages for
	 * each job. There is no point in sending multiple messages that match
	 * as they will just cause flickering updates in the client.
	 */

	if ((num_messages < 100) && (msg->type == JOB_NOTIFY_TYPE) 
		&& (msg->field == JOB_NOTIFY_FIELD_TOTAL_BYTES
		    || msg->field == JOB_NOTIFY_FIELD_TOTAL_PAGES ))
	{

		for (tmp_ptr = notify_queue_head; tmp_ptr; tmp_ptr = tmp_ptr->next) 
		{
			if (tmp_ptr->msg->type == msg->type &&
					tmp_ptr->msg->field == msg->field &&
					tmp_ptr->msg->id == msg->id &&
					tmp_ptr->msg->flags == msg->flags &&
					strequal(tmp_ptr->msg->printer, msg->printer)) {

				DEBUG(5,("send_spoolss_notify2_msg: replacing message 0x%02x/0x%02x for "
					 "printer %s in notify_queue\n", msg->type, msg->field, msg->printer));

				tmp_ptr->msg = msg;
				return;
			}
		}
	}

	/* Store the message on the pending queue. */

	pnqueue = TALLOC_P(send_ctx, struct notify_queue);
	if (!pnqueue) {
		DEBUG(0,("send_spoolss_notify2_msg: Out of memory.\n"));
		return;
	}

	/* allocate a new msg structure and copy the fields */
	
	if ( !(pnqueue->msg = TALLOC_P(send_ctx, SPOOLSS_NOTIFY_MSG)) ) {
		DEBUG(0,("send_spoolss_notify2_msg: talloc() of size [%lu] failed!\n", 
			(unsigned long)sizeof(SPOOLSS_NOTIFY_MSG)));
		return;
	}
	copy_notify2_msg(pnqueue->msg, msg);
	GetTimeOfDay(&pnqueue->tv);
	pnqueue->buf = NULL;
	pnqueue->buflen = 0;

	DEBUG(5, ("send_spoolss_notify2_msg: appending message 0x%02x/0x%02x for printer %s \
to notify_queue_head\n", msg->type, msg->field, msg->printer));

	/*
	 * Note we add to the end of the list to ensure
	 * the messages are sent in the order they were received. JRA.
	 */

	DLIST_ADD_END(notify_queue_head, pnqueue, struct notify_queue *);
	num_messages++;

	if ((notify_event == NULL) && (ev != NULL)) {
		/* Add an event for 1 second's time to send this queue. */
		notify_event = tevent_add_timer(
			ev, NULL, timeval_current_ofs(1,0),
			print_notify_event_send_messages, msg_ctx);
	}

}

static void send_notify_field_values(struct tevent_context *ev,
				     struct messaging_context *msg_ctx,
				     const char *sharename, uint32 type,
				     uint32 field, uint32 id, uint32 value1, 
				     uint32 value2, uint32 flags)
{
	struct spoolss_notify_msg *msg;

	if (lp_disable_spoolss())
		return;

	if (!create_send_ctx())
		return;

	msg = TALLOC_P(send_ctx, struct spoolss_notify_msg);
	if (!msg)
		return;

	ZERO_STRUCTP(msg);

	fstrcpy(msg->printer, sharename);
	msg->type = type;
	msg->field = field;
	msg->id = id;
	msg->notify.value[0] = value1;
	msg->notify.value[1] = value2;
	msg->flags = flags;

	send_spoolss_notify2_msg(ev, msg_ctx, msg);
}

static void send_notify_field_buffer(struct tevent_context *ev,
				     struct messaging_context *msg_ctx,
				     const char *sharename, uint32 type,
				     uint32 field, uint32 id, uint32 len,
				     const char *buffer)
{
	struct spoolss_notify_msg *msg;

	if (lp_disable_spoolss())
		return;

	if (!create_send_ctx())
		return;

	msg = TALLOC_P(send_ctx, struct spoolss_notify_msg);
	if (!msg)
		return;

	ZERO_STRUCTP(msg);

	fstrcpy(msg->printer, sharename);
	msg->type = type;
	msg->field = field;
	msg->id = id;
	msg->len = len;
	msg->notify.data = CONST_DISCARD(char *,buffer);

	send_spoolss_notify2_msg(ev, msg_ctx, msg);
}

/* Send a message that the printer status has changed */

void notify_printer_status_byname(struct tevent_context *ev,
				  struct messaging_context *msg_ctx,
				  const char *sharename, uint32 status)
{
	/* Printer status stored in value1 */

	int snum = print_queue_snum(sharename);

	send_notify_field_values(ev, msg_ctx, sharename, PRINTER_NOTIFY_TYPE,
				 PRINTER_NOTIFY_FIELD_STATUS, snum,
				 status, 0, 0);
}

void notify_printer_status(struct tevent_context *ev,
			   struct messaging_context *msg_ctx,
			   int snum, uint32 status)
{
	const char *sharename = lp_servicename(snum);

	if (sharename)
		notify_printer_status_byname(ev, msg_ctx, sharename, status);
}

void notify_job_status_byname(struct tevent_context *ev,
			      struct messaging_context *msg_ctx,
			      const char *sharename, uint32 jobid,
			      uint32 status,
			      uint32 flags)
{
	/* Job id stored in id field, status in value1 */

	send_notify_field_values(ev, msg_ctx,
				 sharename, JOB_NOTIFY_TYPE,
				 JOB_NOTIFY_FIELD_STATUS, jobid,
				 status, 0, flags);
}

void notify_job_status(struct tevent_context *ev,
		       struct messaging_context *msg_ctx,
		       const char *sharename, uint32 jobid, uint32 status)
{
	notify_job_status_byname(ev, msg_ctx, sharename, jobid, status, 0);
}

void notify_job_total_bytes(struct tevent_context *ev,
			    struct messaging_context *msg_ctx,
			    const char *sharename, uint32 jobid,
			    uint32 size)
{
	/* Job id stored in id field, status in value1 */

	send_notify_field_values(ev, msg_ctx,
				 sharename, JOB_NOTIFY_TYPE,
				 JOB_NOTIFY_FIELD_TOTAL_BYTES, jobid,
				 size, 0, 0);
}

void notify_job_total_pages(struct tevent_context *ev,
			    struct messaging_context *msg_ctx,
			    const char *sharename, uint32 jobid,
			    uint32 pages)
{
	/* Job id stored in id field, status in value1 */

	send_notify_field_values(ev, msg_ctx,
				 sharename, JOB_NOTIFY_TYPE,
				 JOB_NOTIFY_FIELD_TOTAL_PAGES, jobid,
				 pages, 0, 0);
}

void notify_job_username(struct tevent_context *ev,
			 struct messaging_context *msg_ctx,
			 const char *sharename, uint32 jobid, char *name)
{
	send_notify_field_buffer(
		ev, msg_ctx,
		sharename, JOB_NOTIFY_TYPE, JOB_NOTIFY_FIELD_USER_NAME,
		jobid, strlen(name) + 1, name);
}

void notify_job_name(struct tevent_context *ev,
		     struct messaging_context *msg_ctx,
		     const char *sharename, uint32 jobid, char *name)
{
	send_notify_field_buffer(
		ev, msg_ctx,
		sharename, JOB_NOTIFY_TYPE, JOB_NOTIFY_FIELD_DOCUMENT,
		jobid, strlen(name) + 1, name);
}

void notify_job_submitted(struct tevent_context *ev,
			  struct messaging_context *msg_ctx,
			  const char *sharename, uint32 jobid,
			  time_t submitted)
{
	send_notify_field_buffer(
		ev, msg_ctx,
		sharename, JOB_NOTIFY_TYPE, JOB_NOTIFY_FIELD_SUBMITTED,
		jobid, sizeof(submitted), (char *)&submitted);
}

void notify_printer_driver(struct tevent_context *ev,
			   struct messaging_context *msg_ctx,
			   int snum, const char *driver_name)
{
	const char *sharename = lp_servicename(snum);

	send_notify_field_buffer(
		ev, msg_ctx,
		sharename, PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_DRIVER_NAME,
		snum, strlen(driver_name) + 1, driver_name);
}

void notify_printer_comment(struct tevent_context *ev,
			    struct messaging_context *msg_ctx,
			    int snum, const char *comment)
{
	const char *sharename = lp_servicename(snum);

	send_notify_field_buffer(
		ev, msg_ctx,
		sharename, PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_COMMENT,
		snum, strlen(comment) + 1, comment);
}

void notify_printer_sharename(struct tevent_context *ev,
			      struct messaging_context *msg_ctx,
			      int snum, const char *share_name)
{
	const char *sharename = lp_servicename(snum);

	send_notify_field_buffer(
		ev, msg_ctx,
		sharename, PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_SHARE_NAME,
		snum, strlen(share_name) + 1, share_name);
}

void notify_printer_printername(struct tevent_context *ev,
				struct messaging_context *msg_ctx,
				int snum, const char *printername)
{
	const char *sharename = lp_servicename(snum);

	send_notify_field_buffer(
		ev, msg_ctx,
		sharename, PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_PRINTER_NAME,
		snum, strlen(printername) + 1, printername);
}

void notify_printer_port(struct tevent_context *ev,
			 struct messaging_context *msg_ctx,
			 int snum, const char *port_name)
{
	const char *sharename = lp_servicename(snum);

	send_notify_field_buffer(
		ev, msg_ctx,
		sharename, PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_PORT_NAME,
		snum, strlen(port_name) + 1, port_name);
}

void notify_printer_location(struct tevent_context *ev,
			     struct messaging_context *msg_ctx,
			     int snum, const char *location)
{
	const char *sharename = lp_servicename(snum);

	send_notify_field_buffer(
		ev, msg_ctx,
		sharename, PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_LOCATION,
		snum, strlen(location) + 1, location);
}

void notify_printer_sepfile(struct tevent_context *ev,
			    struct messaging_context *msg_ctx,
			    int snum, const char *sepfile)
{
	const char *sharename = lp_servicename(snum);

	send_notify_field_buffer(
		ev, msg_ctx,
		sharename, PRINTER_NOTIFY_TYPE, PRINTER_NOTIFY_FIELD_SEPFILE,
		snum, strlen(sepfile) + 1, sepfile);
}


void notify_printer_byname(struct tevent_context *ev,
			   struct messaging_context *msg_ctx,
			   const char *printername, uint32 change,
			   const char *value)
{
	int snum = print_queue_snum(printername);
	int type = PRINTER_NOTIFY_TYPE;
	
	if ( snum == -1 )
		return;
		
	send_notify_field_buffer(
		ev, msg_ctx,
		printername, type, change, snum, strlen(value)+1, value );
} 


/****************************************************************************
 Return a malloced list of pid_t's that are interested in getting update
 messages on this print queue. Used in printing/notify to send the messages.
****************************************************************************/

static bool print_notify_pid_list(const char *printername, TALLOC_CTX *mem_ctx,
				  size_t *p_num_pids, pid_t **pp_pid_list)
{
	struct tdb_print_db *pdb = NULL;
	TDB_CONTEXT *tdb = NULL;
	TDB_DATA data;
	bool ret = True;
	size_t i, num_pids, offset;
	pid_t *pid_list;

	*p_num_pids = 0;
	*pp_pid_list = NULL;

	pdb = get_print_db_byname(printername);
	if (!pdb)
		return False;
	tdb = pdb->tdb;

	if (tdb_read_lock_bystring_with_timeout(tdb, NOTIFY_PID_LIST_KEY, 10) == -1) {
		DEBUG(0,("print_notify_pid_list: Failed to lock printer %s database\n",
					printername));
		if (pdb)
			release_print_db(pdb);
		return False;
	}

	data = get_printer_notify_pid_list( tdb, printername, True );

	if (!data.dptr) {
		ret = True;
		goto done;
	}

	num_pids = data.dsize / 8;

	if (num_pids) {
		if ((pid_list = TALLOC_ARRAY(mem_ctx, pid_t, num_pids)) == NULL) {
			ret = False;
			goto done;
		}
	} else {
		pid_list = NULL;
	}

	for( i = 0, offset = 0; i < num_pids; offset += 8, i++)
		pid_list[i] = (pid_t)IVAL(data.dptr, offset);

	*pp_pid_list = pid_list;
	*p_num_pids = num_pids;

	ret = True;

  done:

	tdb_read_unlock_bystring(tdb, NOTIFY_PID_LIST_KEY);
	if (pdb)
		release_print_db(pdb);
	SAFE_FREE(data.dptr);
	return ret;
}
