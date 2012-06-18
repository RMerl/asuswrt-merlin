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
 * $Id: getqueue.h,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/



#ifndef _GETQUEUE_H
#define _GETQUEUE_H

EXTERN char *CTRL_A_str DEFINE( = "\001" );

/* SSL */
EXTERN const char * ACTION				DEFINE( = "action" );
EXTERN const char * ACCNTNAME			DEFINE( = "R" );
EXTERN const char * ACTIVE_TIME			DEFINE( = "active_time" );
EXTERN const char * ADDR				DEFINE( = "addr" );
EXTERN const char * ALL					DEFINE( = "all" );
EXTERN const char * ATTEMPT				DEFINE( = "attempt" );
EXTERN const char * AUTH				DEFINE( = "auth" );
EXTERN const char * AUTHCA				DEFINE( = "authca" );
EXTERN const char * AUTHFROM			DEFINE( = "authfrom" );
EXTERN const char * AUTHTYPE			DEFINE( = "authtype" );
EXTERN const char * AUTHUSER			DEFINE( = "authuser" );
EXTERN const char * AUTOHOLD			DEFINE( = "autohold" );
EXTERN const char * BNRNAME				DEFINE( = "L" );
EXTERN const char * CALL				DEFINE( = "call" );
EXTERN const char * CF_OUT_IMAGE		DEFINE( = "cf_out_image" );
EXTERN const char * CHANGE				DEFINE( = "change" );
EXTERN const char * CLASS				DEFINE( = "C" );
EXTERN const char * CLIENT				DEFINE( = "client" );
EXTERN const char * CMD					DEFINE( = "cmd" );
EXTERN const char * COPIES				DEFINE( = "copies" );
EXTERN const char * COPY_DONE			DEFINE( = "copy_done" );
EXTERN const char * DATAFILES			DEFINE( = "datafiles" );
EXTERN const char * DATAFILE_COUNT		DEFINE( = "datafile_count" );
EXTERN const char * DATE				DEFINE( = "D" );
EXTERN const char * DEBUG				DEFINE( = "debug" );
EXTERN const char * DEBUGFV				DEFINE( = "debugfv" );
EXTERN const char * DEST				DEFINE( = "dest" );
EXTERN const char * DESTINATION			DEFINE( = "destination" );
EXTERN const char * DESTINATIONS		DEFINE( = "destinations" );
EXTERN const char * DF_NAME				DEFINE( = "df_name" );
EXTERN const char * DMALLOC_OPTIONS		DEFINE( = "DMALLOC_OPTIONS" );
EXTERN const char * DMALLOC_OUTFILE		DEFINE( = "dmalloc_outfile" );
EXTERN const char * DONE_TIME			DEFINE( = "done_time" );
EXTERN const char * DONE_REMOVE			DEFINE( = "done_remove" );
EXTERN const char * DUMP				DEFINE( = "dump" );
EXTERN const char * END					DEFINE( = "end" );
EXTERN const char * ERROR				DEFINE( = "error" );
EXTERN const char * ERROR_TIME			DEFINE( = "error_time" );
EXTERN const char * ESC_ID				DEFINE( = "esc_id" );
EXTERN const char * FILENAMES			DEFINE( = "filenames" );
EXTERN const char * FILE_HOSTNAME		DEFINE( = "file_hostname" );
EXTERN const char * FILTER				DEFINE( = "filter" );
EXTERN const char * FIRST_SCAN			DEFINE( = "first_scan" );
EXTERN const char * FORMAT				DEFINE( = "format" );
EXTERN const char * FORMAT_ERROR		DEFINE( = "format_error" );
EXTERN const char * FORWARDING			DEFINE( = "forwarding" );
EXTERN const char * FORWARD_ID			DEFINE( = "forward_id" );
EXTERN const char * FROM				DEFINE( = "from" );
EXTERN const char * FROMHOST			DEFINE( = "H" );
EXTERN const char * HELD				DEFINE( = "held" );
EXTERN const char * HF_IMAGE			DEFINE( = "hf_image" );
EXTERN const char * HF_NAME				DEFINE( = "hf_name" );
EXTERN const char * HOLD_ALL			DEFINE( = "hold_all" );
EXTERN const char * HOLD_CLASS			DEFINE( = "hold_class" );
EXTERN const char * HOLD_TIME			DEFINE( = "hold_time" );
EXTERN const char * HOST				DEFINE( = "host" );
EXTERN const char * HPFORMAT			DEFINE( = "hpformat" );
EXTERN const char * ID					DEFINE( = "id" );
EXTERN const char * IDENTIFIER			DEFINE( = "A" );
EXTERN const char * INCOMING_TIME		DEFINE( = "incoming_time" );
EXTERN const char * INPUT				DEFINE( = "input" );
EXTERN const char * JOBNAME				DEFINE( = "J" );
EXTERN const char * JOBSEQ				DEFINE( = "jobseq" );
EXTERN const char * JOBSIZE				DEFINE( = "jobsize" );
EXTERN const char * JOB_TIME			DEFINE( = "job_time" );
EXTERN const char * JOB_TIME_USEC		DEFINE( = "job_time_usec" );
EXTERN const char * KEYID				DEFINE( = "keyid" );
EXTERN const char * LOCALHOST			DEFINE( = "localhost" );
EXTERN const char * LOG					DEFINE( = "log" );
EXTERN const char * LOGGER				DEFINE( = "logger" );
EXTERN const char * LOGNAME				DEFINE( = "P" );
EXTERN const char * LP					DEFINE( = "lp" );
EXTERN const char * LPC					DEFINE( = "lpc" );
EXTERN const char * LPD					DEFINE( = "lpd" );
EXTERN const char * LPD_CONF			DEFINE( = "LPD_CONF" );
EXTERN const char * LPD_PORT			DEFINE( = "lpd_port" );
EXTERN const char * LPD_REQUEST			DEFINE( = "lpd_request" );
EXTERN const char * MAILNAME			DEFINE( = "M" );
EXTERN const char * MAIL_FD				DEFINE( = "mail_fd" );
EXTERN const char * MOVE				DEFINE( = "move" );
EXTERN const char * MOVE_DEST			DEFINE( = "move_dest" );
EXTERN const char * MSG					DEFINE( = "msg" );
EXTERN const char * NAME				DEFINE( = "name" );
EXTERN const char * NEW_DEST			DEFINE( = "new_dest" );
EXTERN const char * NONEP				DEFINE( = "none" );
EXTERN const char * NUMBER				DEFINE( = "number" );
EXTERN const char * OPENNAME			DEFINE( = "openname" );
EXTERN const char * ORIG_IDENTIFIER		DEFINE( = "orig_identifier" );
EXTERN const char * PGP					DEFINE( = "pgp" );
EXTERN const char * PORT				DEFINE( = "port" );
EXTERN const char * PRINTABLE			DEFINE( = "printable" );
EXTERN const char * PRINTER				DEFINE( = "printer" );
EXTERN const char * PRINTING_ABORTED	DEFINE( = "printing_aborted" );
EXTERN const char * PRINTING_DISABLED	DEFINE( = "printing_disabled" );
EXTERN const char * PRIORITY			DEFINE( = "priority" );
EXTERN const char * PRIORITY_TIME		DEFINE( = "priority_time" );
EXTERN const char * PROCESS				DEFINE( = "process" );
EXTERN const char * PRSTATUS			DEFINE( = "prstatus" );
EXTERN const char * QUEUE				DEFINE( = "queue" );
EXTERN const char * QUEUENAME			DEFINE( = "Q" );
EXTERN const char * QUEUE_CONTROL_FILE	DEFINE( = "queue_control_file" );
EXTERN const char * QUEUE_STATUS_FILE	DEFINE( = "queue_status_file" );
EXTERN const char * REDIRECT			DEFINE( = "redirect" );
EXTERN const char * REMOTEHOST			DEFINE( = "remotehost" );
EXTERN const char * REMOTEPORT			DEFINE( = "remoteport" );
EXTERN const char * REMOTEUSER			DEFINE( = "remoteuser" );
EXTERN const char * REMOVE_TIME			DEFINE( = "remove_time" );
EXTERN const char * SD					DEFINE( = "sd" );
EXTERN const char * SEQUENCE			DEFINE( = "sequence" );
EXTERN const char * SERVER				DEFINE( = "server" );
EXTERN const char * SERVER_ORDER		DEFINE( = "server_order" );
EXTERN const char * SERVICE				DEFINE( = "service" );
EXTERN const char * SIZE				DEFINE( = "size" );
EXTERN const char * SORT_KEY			DEFINE( = "sort_key" );
EXTERN const char * SPOOLDIR			DEFINE( = "spooldir" );
EXTERN const char * SPOOLING_DISABLED 	DEFINE( = "spooling_disabled" );
EXTERN const char * START_TIME			DEFINE( = "start_time" );
EXTERN const char * STATE				DEFINE( = "state" );
EXTERN const char * STATUS_CHANGE		DEFINE( = "status_change" );
EXTERN const char * STATUS_FD			DEFINE( = "status_fd" );
EXTERN const char * SUBSERVER			DEFINE( = "subserver" );
EXTERN const char * TRACE				DEFINE( = "trace" );
EXTERN const char * TRANSFERNAME		DEFINE( = "transfername" );
EXTERN const char * OTRANSFERNAME		DEFINE( = "otransfername" );
EXTERN const char * UNIXSOCKET			DEFINE( = "unixsocket" );
EXTERN const char * UPDATE				DEFINE( = "update" );
EXTERN const char * UPDATE_TIME			DEFINE( = "update_time" );
EXTERN const char * USER				DEFINE( = "user" );
EXTERN const char * VALUE				DEFINE( = "value" );

/* PROTOTYPES */
int Scan_queue( struct line_list *spool_control,
	struct line_list *sort_order, int *pprintable, int *pheld, int *pmove,
		int only_queue_process, int *perr, int *pdone,
		const char *remove_prefix, const char *remove_suffix );
char *Get_fd_image( int fd, off_t maxsize );
char *Get_file_image( const char *file, off_t maxsize );
int Get_fd_image_and_split( int fd,
	int maxsize, int clean,
	struct line_list *l, const char *sep,
	int sort, const char *keysep, int uniq, int trim, int nocomments,
	char **return_image );
int Get_file_image_and_split( const char *file,
	int maxsize, int clean,
	struct line_list *l, const char *sep,
	int sort, const char *keysep, int uniq, int trim, int nocomments,
	char **return_image );
void Check_for_hold( struct job *job, struct line_list *spool_control );
void Setup_job( struct job *job, struct line_list *spool_control,
	const char *cf_name, const char *hf_name, int check_for_existence );
int Get_hold_class( struct line_list *info, struct line_list *sq );
void Append_Z_value( struct job *job, char *s );
int Setup_cf_info( struct job *job, int check_for_existence );
char *Make_hf_image( struct job *job );
int Set_hold_file( struct job *job, struct line_list *perm_check, int fd );
void Get_hold_file( struct job *job, char *hf_name );
void Get_spool_control( const char *file, struct line_list *info );
void Set_spool_control( struct line_list *perm_check, const char *file,
	struct line_list *info );
void intval( const char *key, struct line_list *list, struct job *job );
void revintval( const char *key, struct line_list *list, struct job *job );
void strzval( const char *key, struct line_list *list, struct job *job );
void strnzval( const char *key, struct line_list *list, struct job *job );
void strval( const char *key, struct line_list *list, struct job *job,
	int reverse );
void Make_sort_key( struct job *job );
int Setup_printer( char *prname, char *error, int errlen, int subserver );
int Read_pid( int fd, char *str, int len );
int Write_pid( int fd, int pid, char *str );
int Patselect( struct line_list *token, struct line_list *cf, int starting );
int Check_format( int type, const char *name, struct job *job );
char *Find_start(char *str, const char *key );
char *Frwarding(struct line_list *l);
int Pr_disabled(struct line_list *l);
int Sp_disabled(struct line_list *l);
int Pr_aborted(struct line_list *l);
int Hld_all(struct line_list *l);
char *Clsses(struct line_list *l);
char *Cntrol_debug(struct line_list *l);
char *Srver_order(struct line_list *l);
void Init_job( struct job *job );
void Free_job( struct job *job );
void Copy_job( struct job *dest, struct job *src );
char *Fix_job_number( struct job *job, int n );
char *Make_identifier( struct job *job );
void Dump_job( char *title, struct job *job );
void Job_printable( struct job *job, struct line_list *spool_control,
	int *pprintable, int *pheld, int *pmove, int *perr, int *pdone );
int Server_active( char *file );
void Update_destination( struct job *job );
int Get_destination( struct job *job, int n );
int Get_destination_by_name( struct job *job, char *name );
int Trim_status_file( int status_fd, char *file, int max, int min );
char *Fix_datafile_info( struct job *job, char *number, char *suffix,
	char *xlate_format );
int ordercomp(  const void *left, const void *right, const void *orderp);
void Fix_control( struct job *job, char *filter, char *xlate_format );
int Create_control( struct job *job, char *error, int errlen,
	char *xlate_format );
void Init_buf(char **buf, int *max, int *len);
void Put_buf_len( const char *s, int cnt, char **buf, int *max, int *len );
void Put_buf_str( const char *s, char **buf, int *max, int *len );
void Free_buf(char **buf, int *max, int *len);

#endif
