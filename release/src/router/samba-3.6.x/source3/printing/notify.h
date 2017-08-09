#ifndef _PRINTING_NOTIFY_H_
#define _PRINTING_NOTIFY_H_

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

/* The following definitions come from printing/notify.c  */

int print_queue_snum(const char *qname);
void print_notify_send_messages(struct messaging_context *msg_ctx,
				unsigned int timeout);
void notify_printer_status_byname(struct tevent_context *ev,
				  struct messaging_context *msg_ctx,
				  const char *sharename, uint32 status);
void notify_printer_status(struct tevent_context *ev,
			   struct messaging_context *msg_ctx,
			   int snum, uint32 status);
void notify_job_status_byname(struct tevent_context *ev,
			      struct messaging_context *msg_ctx,
			      const char *sharename, uint32 jobid,
			      uint32 status,
			      uint32 flags);
void notify_job_status(struct tevent_context *ev,
		       struct messaging_context *msg_ctx,
		       const char *sharename, uint32 jobid, uint32 status);
void notify_job_total_bytes(struct tevent_context *ev,
			    struct messaging_context *msg_ctx,
			    const char *sharename, uint32 jobid,
			    uint32 size);
void notify_job_total_pages(struct tevent_context *ev,
			    struct messaging_context *msg_ctx,
			    const char *sharename, uint32 jobid,
			    uint32 pages);
void notify_job_username(struct tevent_context *ev,
			 struct messaging_context *msg_ctx,
			 const char *sharename, uint32 jobid, char *name);
void notify_job_name(struct tevent_context *ev,
		     struct messaging_context *msg_ctx,
		     const char *sharename, uint32 jobid, char *name);
void notify_job_submitted(struct tevent_context *ev,
			  struct messaging_context *msg_ctx,
			  const char *sharename, uint32 jobid,
			  time_t submitted);
void notify_printer_driver(struct tevent_context *ev,
			   struct messaging_context *msg_ctx,
			   int snum, const char *driver_name);
void notify_printer_comment(struct tevent_context *ev,
			    struct messaging_context *msg_ctx,
			    int snum, const char *comment);
void notify_printer_sharename(struct tevent_context *ev,
			      struct messaging_context *msg_ctx,
			      int snum, const char *share_name);
void notify_printer_printername(struct tevent_context *ev,
				struct messaging_context *msg_ctx,
				int snum, const char *printername);
void notify_printer_port(struct tevent_context *ev,
			 struct messaging_context *msg_ctx,
			 int snum, const char *port_name);
void notify_printer_location(struct tevent_context *ev,
			     struct messaging_context *msg_ctx,
			     int snum, const char *location);
void notify_printer_byname(struct tevent_context *ev,
			   struct messaging_context *msg_ctx,
			   const char *printername, uint32 change,
			   const char *value);
void notify_printer_sepfile(struct tevent_context *ev,
			    struct messaging_context *msg_ctx,
			    int snum, const char *sepfile);
#endif
