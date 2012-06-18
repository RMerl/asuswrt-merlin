/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: printjob.h,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/



#ifndef _PRINTJOB_H_
#define _PRINTJOB_H_ 1

/* PROTOTYPES */
int Print_job( int output, int status_device, struct job *job,
	int send_job_rw_timeout, int poll_for_status, char *user_filter );
int Run_OF_filter( int send_job_rw_timeout, int *of_pid, int *of_stdin, int *of_stderr,
	int output, char **outbuf, int *outmax, int *outlen,
	struct job *job, char *id, int terminate_of,
	char *msgbuffer, int msglen );
void Print_banner( char *name, char *pgm, struct job *job );
int Write_outbuf_to_OF( struct job *job, char *title,
	int of_fd, char *buffer, int outlen,
	int of_error, char *msg, int msgmax,
	int timeout, int poll_for_status, char *status_file );
int Get_status_from_OF( struct job *job, char *title, int of_pid,
	int of_error, char *msg, int msgmax,
	int timeout, int suspend, int max_wait, char *status_file );
int Wait_for_pid( int of_pid, char *name, int suspend, int timeout );

#endif
