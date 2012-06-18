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
 * $Id: lp.h,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/

#ifndef _LP_H_
#define _LP_H_ 1
#ifndef EXTERN
#define EXTERN extern
#define DEFINE(X)
#endif

/*****************************************************************
 * get the portability information and configuration 
 *****************************************************************/
#include "portable.h"

/*****************************************************************
 * Global variables and routines that will be common to all programs
 *****************************************************************/

/*****************************************************************
 * BUFFER SIZES
 * 
 * A common size is 1024 bytes;
 * However,  it appears that this is overkill for most purposes.
 * 180 bytes appears to be satisfactory for a line,
 * 512 for a small buffer, 1024 for a large buffer
 *
 *****************************************************************/

#define LINEBUFFER 180
#define SMALLBUFFER 512
#define LARGEBUFFER 10240
 
/*****************************************************************
 * Include files that are so common they should be included anyways
 *****************************************************************/

/* declare structures for forward references */
struct host_information;
struct line_list;

#include "linelist.h"
#include "utilities.h"
#include "debug.h"
#include "errormsg.h"
#include "plp_snprintf.h"

/*****************************************************************
 * General variables use in the common routines;
 * while in principle we could segragate these, it is not worth it
 * as the number is relatively small
 *****************************************************************/
EXTERN int Optind;			/* next argv to process */
EXTERN int Opterr DEFINE(=1); /* Zero disables errors msgs */
EXTERN char *Optarg;		/* Pointer to option argument */
EXTERN char *Name;			/* Name of program */

extern char *Copyright[];	/* Copyright info */
#define Version	Copyright[0]
EXTERN int Is_server;		/* LPD sets to non-zero */
EXTERN int Server_pid;		/* PID of server */
EXTERN int Is_lpr;			/* LPR sets to non-zero */
EXTERN int Logger_fd;		/* for logger */
EXTERN int Mail_fd;			/* for mail */
EXTERN int Lpd_request;		/* pipe to server */
EXTERN int Doing_cleanup;	/* process exiting */
EXTERN int Verbose;		/* LPD sets to non-zero */
EXTERN int Warnings;		/* set for warnings and not fatal - used with checkcp */
EXTERN int Errorcode;		/* Exit code for an error */
EXTERN int Status_fd;		/* Status file descriptor for spool queue */
EXTERN char *Outbuf, *Inbuf;	/* buffer */
EXTERN int Outlen, Outmax, Inlen, Inmax;	/* max and current len of buffer */
EXTERN uid_t OriginalEUID, OriginalRUID;   /* original EUID, RUID values */
EXTERN uid_t OriginalEGID, OriginalRGID;   /* original EGID, RGID values */
EXTERN uid_t DaemonUID;    /* Daemon UID */
EXTERN uid_t UID_root;     /* UID is root */
EXTERN gid_t DaemonGID;    /* Daemon GID */
EXTERN int Max_fd DEFINE(=4);	   /* Maximum FD opened */

extern void Max_open(int fd);


#if 0
typepdef void (*WorkerProc)( struct line_list *args );
struct call_list{
	const char *id;
	void (*p)( struct line_list *args );
};
extern struct call_list Calls[];
#endif



#ifdef HAVE_STDARGS
void setstatus( struct job *job, char *fmt, ... );
void setmessage( struct job *job, const char *header, char *fmt, ... );
#else 
void setstatus( va_alist );
void setmessage( va_alist );
#endif
void send_to_logger( int sfd, int mfd, struct job *job, const char *header, char *msg );

/***************************************************************************
 * ACKs from the remote host on job transfers
 ***************************************************************************/
#define ACK_SUCCESS 0   /* succeeded; delete remote copy */
#define ACK_STOP_Q  1   /* failed; no spooling to the remote queue */
#define ACK_RETRY   2   /* failed; retry later */
#define ACK_FAIL    3   /* failed; bad job */
 
/*
 * macros for the names and maximum lengths for the various fields
 */

#define M_DEFAULT		131				/* default limit on line length */
/* IDENTIFIER 		'A'	 LPRng - unique job ID */
#define M_IDENTIFIER	131				/* default limit on line length */
/* CLASSNAME 		'C'	 RFC: 31 char limit */
#define M_DATE			31
/* DATE      		'D'	 LPRng - date job started */
#define M_CLASSNAME		1024
/* FROMHOST  		'H'	 RFC: 31 char limit */
#define M_FROMHOST		31
/* INDENT    		'I'	 RFC: number */
#define M_INDENT		31
/* JOBNAME   		'J'	 RFC: 99 char limit */
#define M_JOBNAME		99
/* BNRNAME   		'L'	 RFC: ?? char limit */
#define M_BNRNAME		31
/* FILENAME  		'N'	 RFC: 131 char limit */
#define M_FILENAME		131
/* MAILNAME  		'M'	 RFC: ?? char limit */
#define M_MAILNAME		131
/* FILENAME			'N'	 RFC: 131 char limit on 'N' entries */
#define M_NAME			131
/* LOGNAME   		'P'	 RFC: 31 char limit */
#define M_LOGNAME		31
/* QUEUENAME 		'Q'	 PLP: 131 char limit */
#define M_QUEUENAME		131
/* ACCNTNAME 		'R'	 PLP: info for accounting: 131 */
#define M_ACCNTNAME		131
/* SLINKDATA 		'S'	 RFC: number " " number */
#define M_SLINKDATA		131
/* PRTITLE   		'T'	 RFC: 79 char limit */
#define M_PRTITLE		79
/* UNLNKFILE 		'U'	 RFC: flag */
#define M_UNLNKFILE		131
/* PWIDTH    		'W'	 RFC: number */
#define M_PWIDTH		31
/* ZOPTS     		'Z'	 PLP */
#define M_ZOPTS			1024
/* FONT1     		'1'	 RFC: ?? char limit */
/* FONT2     		'2'	 RFC: ?? char limit */
/* FONT3     		'3'	 RFC: ?? char limit */
/* FONT4     		'4'	 RFC: ?? char limit */
#define M_FONT		131

/*****************************************************************
 * Command file option checking:
 *  We specify the allowed order of options in the control file.
 *  Note that '*' is a wild card
 *   Berkeley-            HPJCLIMWT1234
 *   PLP-                 HPJCLIMWT1234*
 *****************************************************************/
/* error codes for return values */

#define LINK_OPEN_FAIL		-1		/* open or connect */
#define LINK_TRANSFER_FAIL	-2		/* transfer failed */
#define LINK_ACK_FAIL		-3		/* non-zero ACK on operation */
#define LINK_FILE_READ_FAIL	-4		/* read from a file failed */
#define LINK_LONG_LINE_FAIL	-5		/* a line was too long to read */
#define LINK_BIND_FAIL		-6		/* cannot bind to port */
#define LINK_PERM_FAIL		-7		/* permission failure, remove job */
#define LINK_ECONNREFUSED	-8		/* connection refused */
#define LINK_ETIMEDOUT     	-9		/* connection timedout */

/*****************************************************************
 * LPD Protocol Information
 *****************************************************************/
/*****************************************************************
 * Request types
 * A request sent to the LPD daemon has the format:
 * \Xprinter [options],  where \X is a single character or byte value.
 * The following are the values and commands
 *****************************************************************/

#define REQ_START   1   /* start printer */
#define REQ_RECV    2   /* transfer a printer job */
#define REQ_DSHORT  3   /* print short form of queue status */
#define REQ_DLONG   4   /* print long form of queue status */
#define REQ_REMOVE  5   /* remove jobs */
#define REQ_CONTROL 6   /* do control operation */
#define REQ_BLOCK   7   /* transfer a block format print job */
#define REQ_SECURE  8   /* secure command transfer */
#define REQ_VERBOSE 9   /* verbose status information */
#define REQ_LPSTAT  10  /* LPSTAT format */
#define REQ_K4AUTH  'k' /* krb4 authentication */

#define KLPR_SERVICE "rcmd"

#define ABORT_XFER   1       /* \1\n - abort transfer */
#define CONTROL_FILE 2       /* \2<count> <cfname>\n */
#define DATA_FILE    3       /* \3<count> <dfname>\n */

#define safestrncat( s1, s2 ) mystrncat(s1,s2,sizeof(s1))
#define safestrncpy( s1, s2 ) mystrncpy(s1,s2,sizeof(s1));

#define STR(X) #X
#ifdef ISNULL
# error - you have already defined ISNULL
xxx error - you have already defined ISNULL
#endif
#define ISNULL(X) ((X)==0||(*X)==0)

/* some handy definitions */

#define CTRL_A '\001'

/*
 * global variables with definitions done at run time
 */

EXTERN char *ShortHost_FQDN;		/* Short hostname for logging */
EXTERN char *FQDNHost_FQDN;		/* FQDN hostname */
EXTERN char* FQDNRemote_FQDN;    /* FQDN of Remote host */
EXTERN char* ShortRemote_FQDN;   /* Short form of Remote host */
/* EXTERN char* Auth_client_id_DYN;	/ * client sent/received authentication info */

EXTERN int Drop_root_DYN;				/* drop root permissions */

EXTERN int Accounting_check_DYN; /* check accounting at start */
EXTERN char* Accounting_end_DYN;/* accounting at start (see also af, la, ar) */
EXTERN char* Accounting_file_DYN; /* name of accounting file (see also la, ar) */
EXTERN char* Accounting_namefixup_DYN; /* fix up accounting name */
EXTERN int Accounting_remote_DYN; /* write remote transfer accounting (if af is set) */
EXTERN char* Accounting_start_DYN;/* accounting at start (see also af, la, ar) */
EXTERN char* Allow_user_setting_DYN;	/* allow users to set job submitter */
EXTERN int Allow_getenv_DYN;
EXTERN int Allow_user_logging_DYN; /* allow users to get log info */
EXTERN int Always_banner_DYN; /* always print banner, ignore lpr -h option */
EXTERN char* Append_Z_DYN; /* append -Z options on outgoing or filter*/
EXTERN char* Architecture_DYN;
EXTERN char* Auth_DYN;			/* authentication to use to send to server */
EXTERN char* Auth_forward_DYN;	/* server use authentication when forwarding */
EXTERN int Auto_hold_DYN;	 /* automatically hold all jobs */
EXTERN char* BK_filter_options_DYN;	/* backwards compatible filter options */
EXTERN char* BK_of_filter_options_DYN;	/* backwards compatible OF filter options */
EXTERN int Backwards_compatible_DYN; /* backwards-compatible: job file format */
EXTERN int Backwards_compatible_filter_DYN; /* backwards-compatible: filter parameters */
EXTERN char* Banner_end_DYN;	 /* end banner printing program overrides bp */
EXTERN int Banner_last_DYN; /* print banner after job instead of before */
EXTERN char* Banner_line_DYN;	 /* short banner line sent to banner printer */
EXTERN char* Banner_printer_DYN; /* banner printing program (see ep) */
EXTERN char* Banner_start_DYN;	 /* start banner printing program overrides bp */
EXTERN int Baud_rate_DYN; /* if lp is a tty, set the baud rate (see ty) */
EXTERN char* Bounce_queue_format_DYN; /* destination for bounce queue files */
EXTERN int Break_classname_priority_link_DYN; /* do not set priority from class name */
EXTERN int Check_for_nonprintable_DYN;	/* lpr check for nonprintable file */
EXTERN int Check_for_protocol_violations_DYN;	/* check for RFC1179 protocol violations */
EXTERN char* Chooser_DYN;	/* choose the destination for a load balance queue */
EXTERN int Chooser_interval_DYN;	/* interval between tests for load balance destination */
EXTERN char* Chooser_routine_DYN;	/* choose the destination for a load balance queue */
EXTERN int Class_in_status_DYN;	/* Show class in status information */
EXTERN char* Comment_tag_DYN; /* comment identifying printer (LPQ) */
EXTERN char* Config_file_DYN;
EXTERN int Connect_grace_DYN; /* grace period for reconnections */
EXTERN int Connect_interval_DYN;
EXTERN int Connect_timeout_DYN;
EXTERN char* Control_filter_DYN; /* Control filter */
EXTERN char* Control_file_line_order_DYN; /* Control file line order */
EXTERN int Create_files_DYN;		/* allow spool dir files to be created */
EXTERN char* Current_date_DYN; /* Current Date */
EXTERN char* Daemon_group_DYN;
EXTERN char* Daemon_user_DYN;
EXTERN char* Default_format_DYN;	/* default format */
EXTERN char* Default_permission_DYN;	/* default permission */
EXTERN char* Default_printer_DYN;	/* default printer */
EXTERN char* Default_priority_DYN;	/* default priority */
EXTERN char* Default_remote_host_DYN;
EXTERN char* Default_tmp_dir_DYN;	/* default temporary file directory */
EXTERN char* Destinations_DYN; /* printers that a route filter may return and we should query */
EXTERN int   Done_jobs_DYN;        /* keep the last NN done jobs */
EXTERN int   Done_jobs_max_age_DYN; /* keep the done jobs for at least max age seconds */
EXTERN int Direct_DYN;		/* allow LPR to send jobs to a socket */
EXTERN int Exit_linger_timeout_DYN;	/* we set this timeout on all of the sockets */
EXTERN int FF_on_close_DYN; /* print a form feed when device is closed */
EXTERN int FF_on_open_DYN; /* print a form feed when device is opened */
EXTERN int Fifo_DYN; /* FIFO (first in first out) order enforced */
EXTERN char* Fifo_lock_file_DYN; /* lock file for FIFO */

EXTERN char* Filter_DYN; /* default filter */
EXTERN int   Filter_stderr_to_status_file_DYN; /* filter errors sent to :ps file */
EXTERN char* Filter_ld_path_DYN;
EXTERN char* Filter_options_DYN;
EXTERN char* Filter_path_DYN;
EXTERN int Fake_large_file_DYN; 	/* fake large file size if you cannot use 0 */
EXTERN int Filter_poll_interval_DYN; /* intervals at which to check filter */
EXTERN int Force_FQDN_hostname_DYN; /* force FQDN Host name in control file */
EXTERN int Force_IPADDR_hostname_DYN; /* force IPADDR for Host name in control file */
EXTERN int Force_localhost_DYN;	/* force localhost for client job transfer */
EXTERN char* Force_lpq_status_DYN;	/* force lpq status format */
EXTERN int Force_poll_DYN; /* force polling job queues */
EXTERN char* Force_queuename_DYN; /* force the use of this queue name */
EXTERN char* Form_feed_DYN; /* string to send for a form feed */
EXTERN char* Formats_allowed_DYN; /* valid output filter formats */
EXTERN int Full_time_DYN; /* full or complete time format in messages */
EXTERN int Half_close_DYN; /* do shutdown on socket when sending job */
EXTERN int Generate_banner_DYN; /* generate a banner when not a bounce queue */
EXTERN char* IF_Filter_DYN; /* filter command, run on a per-file basis */
EXTERN int IPV6Protocol_DYN;	/* IPV4 or IPV6 protocol */
EXTERN int Ignore_requested_user_priority_DYN;	 /* ignore requested user priority */
EXTERN char* Incoming_control_filter_DYN; /* incoming control file filter */
EXTERN int Keepalive_DYN;	/* TCP keepalive enabled */
EXTERN char* Kerberos_keytab_DYN;	/* kerberos keytab file */
EXTERN char* Kerberos_dest_id_DYN;	/* kerberos destination principle */
EXTERN char* Kerberos_life_DYN;	/* kerberos lifetime */
EXTERN char* Kerberos_renew_DYN;	/* kerberos newal time */
EXTERN char* Kerberos_forward_principal_DYN;	/* kerberos server principle */
EXTERN char* Kerberos_server_principal_DYN;	/* kerberos server principle */
EXTERN char* Kerberos_service_DYN;	/* kerberos service */
EXTERN int LPR_bsd_DYN;		/* use BSD -m mail option */
EXTERN char* Leader_on_open_DYN; /* leader string printed on printer open */
EXTERN int Local_accounting_DYN; /* write local printer accounting (if af is set) */
EXTERN int Lock_it_DYN; /* lock the IO device */
EXTERN char* Lockfile_DYN;
EXTERN char* Log_file_DYN; /* status log file */
EXTERN char* Logger_destination_DYN; /* logger host and port */
EXTERN int Logger_max_size_DYN; /* log record size */
EXTERN char* Logger_path_DYN; /* path to status log file */
EXTERN int Logger_timeout_DYN; /* logger timeout size */
EXTERN char* Logname_DYN;		/* Username for logging */
EXTERN int Long_number_DYN; /* long job number (6 digits) */
EXTERN char* Lp_device_DYN; /* device name or lp-pipe command to send output to */
EXTERN int Lpd_bounce_DYN; /* force LPD to do bounce queue filtering */
EXTERN char* Lpd_listen_port_DYN; /* lpd listens on this port, "off" does not open port */
EXTERN char* Lpd_path_DYN; /* LPD path for server use */
EXTERN char* Lpd_port_DYN;	/* client/lpd connect to remote (non-local) lpd servers on this port */
EXTERN char* Lpd_printcap_path_DYN;
EXTERN int Lpr_bounce_DYN; /* allow LPR to do bounce queue filtering */
EXTERN char* Lpq_status_file_DYN; /* cached lpq status */
EXTERN int   Lpq_status_cached_DYN;  /* how many to cache */
EXTERN int   Lpq_status_interval_DYN;  /* interval between updates */
EXTERN int   Lpq_status_stale_DYN;  /* cached lpq status is stale after this */
EXTERN char* Lpr_opts_DYN;		/* addional options for LPR */
EXTERN int Lpr_send_try_DYN; /* number of times for lpr to try sending job */
EXTERN char* Mail_from_DYN;
EXTERN char* Mail_operator_on_error_DYN;
EXTERN int Max_connect_interval_DYN;	/* maximum connect interval */
EXTERN int Max_copies_DYN; /* maximum copies allowed */
EXTERN int Max_datafiles_DYN; /* maximum datafiles */
EXTERN int Max_job_size_DYN; /* maximum job size (1Kb blocks, 0 = unlimited) */
EXTERN int Max_accounting_file_size_DYN;	/* maximum accounting file size */
EXTERN int Max_log_file_size_DYN;	/* maximum log file size */
EXTERN int Max_servers_active_DYN;	/* maximum number of servers active */
EXTERN int Max_status_line_DYN; /* maximum status line size */
EXTERN int Max_status_size_DYN;
EXTERN int Min_accounting_file_size_DYN;	/* minimum accounting file size */
EXTERN int Min_log_file_size_DYN;	/* minimum log file size */
EXTERN int Min_printable_count_DYN; /* minimum printable characters for printable check */
EXTERN int Min_status_size_DYN;
EXTERN int Minfree_DYN; /* minimum space (Kb) to be left in spool filesystem */
EXTERN int Minfree_DYN; /**/
EXTERN int Ms_time_resolution_DYN;
EXTERN int Network_connect_grace_DYN; /* grace period for reconnections */
EXTERN char* New_debug_DYN; /* debug level set for queue handler */
EXTERN int Nline_after_file_DYN;	/* Put Nxxx after fcfA... line in control file */
EXTERN int No_FF_separator_DYN; /* no interfile FF separator */ 
EXTERN int FF_separator_DYN; /* no interfile FF separator */ 
EXTERN int Nonblocking_open_DYN; /* nonblocking open on io device */
EXTERN char* OF_Filter_DYN; /* output filter, run once for all output */
EXTERN char* OF_filter_options_DYN;
EXTERN char* Originate_port_DYN;
EXTERN int Order_routine_DYN; /* use user specified order routine */
EXTERN int Page_length_DYN; /* page length (in lines) */
EXTERN int Page_width_DYN; /* page width (in characters) */
EXTERN int Page_x_DYN; /* page width in pixels (horizontal) */
EXTERN int Page_y_DYN; /* page length in pixels (vertical) */
EXTERN char* Pass_env_DYN;	/* pass these environment variables */
EXTERN char* Pgp_path_DYN;	/* pgp path */
EXTERN int Poll_time_DYN; /* force polling job queues */
EXTERN int Poll_start_interval_DYN; /* interval between trying to start servers */
EXTERN int Poll_servers_started_DYN; /* maximum servers to start at one time */
EXTERN char* Pr_program_DYN; /* pr program for p format */
EXTERN char* Prefix_Z_DYN; /* prefix -Z options on outgoing or filter*/
EXTERN char* Prefix_option_to_option_DYN; /* prefix option to option, ie, "z,o" */
EXTERN char* Printcap_path_DYN;
EXTERN char* Printer_DYN;		/* Printe r name for logging */
EXTERN char* Printer_DYN;	/* printer name */
EXTERN char* Printer_perms_path_DYN;
EXTERN char* Queue_name_DYN;	/* Queue name used for spooling */
EXTERN char* Queue_control_file_DYN; /* Queue control file name */
EXTERN char* Queue_lock_file_DYN; /* Queue lock file name */
EXTERN char* Queue_status_file_DYN; /* Queue status file name */
EXTERN char* Queue_unspooler_file_DYN; /* Unspooler PID status file name */
EXTERN int Read_write_DYN; /* open the printer for reading and writing */
EXTERN char* RemoteHost_DYN; /* remote-queue machine (hostname) (with rm) */
EXTERN char* RemotePrinter_DYN; /* remote-queue printer name (with rp) */
EXTERN char* Remote_support_DYN; /* Operations allowed to remote system */
EXTERN char* Remove_Z_DYN; /* remove -Z options on outgoing or filter*/
EXTERN char* Report_server_as_DYN; /* report server name as this value */
EXTERN int Require_configfiles_DYN; /* require lpd.conf, printcap, lpd.perms files */
EXTERN int Require_explicit_Q_DYN; /* require default queue to be explicitly defined */
EXTERN char* RestrictToGroupMembers_DYN;	/* restrict to group members */
EXTERN int Retry_ECONNREFUSED_DYN; /* retry on ECONNREFUSED  */
EXTERN int Retry_NOLINK_DYN; /* retry on link connection failure */
EXTERN char* Return_short_status_DYN;	/* return short status */
EXTERN int Reuse_addr_DYN; /* set SO_REUSEADDR on outgoing ports */
EXTERN int Reverse_priority_order_DYN; /* priority z-aZ-A order */
EXTERN char* Reverse_lpq_status_DYN;	/* change lpq format when from host */
EXTERN char* Routing_filter_DYN; /* filter to determine routing of jobs */
EXTERN char* Safe_chars_DYN; /* safe characters in control file */
EXTERN int Save_on_error_DYN; /* save this job when an error */
EXTERN int Save_when_done_DYN; /* save this job when done */
EXTERN int Send_block_format_DYN; /* send block of data */
EXTERN int Send_data_first_DYN; /* send data files first */
EXTERN char* Send_failure_action_DYN;
EXTERN int Send_job_rw_timeout_DYN;
EXTERN int Send_query_rw_timeout_DYN;
EXTERN int Send_try_DYN;
EXTERN int Sendmail_to_user_DYN;
EXTERN char* Sendmail_DYN;
EXTERN char* Server_names_DYN; /* names of servers for queue (with ss) */
EXTERN char* Server_queue_name_DYN; /* name of queue that server serves (with sv) */
EXTERN char* Server_tmp_dir_DYN;	/* default temporary file directory */
EXTERN char* Shell_DYN;
EXTERN int Short_banner_DYN; /* short banner (one line only) */
EXTERN int Short_status_length_DYN;	/* short status length */
EXTERN int Socket_linger_DYN;	/* set SO_linger for connections to remote hosts */
EXTERN char* Spool_dir_DYN; /* spool directory (only ONE printer per directory!) */
EXTERN int Spool_dir_perms_DYN;
EXTERN int Spool_file_perms_DYN;
EXTERN char *Ssl_ca_file_DYN;	/* ssl cert file */
EXTERN char *Ssl_ca_path_DYN;	/* ssl cert directory (path) */
EXTERN char *Ssl_crl_file_DYN;	/* ssl crl cert directory (path) */
EXTERN char *Ssl_crl_path_DYN;	/* ssl crl cert directory (path) */
EXTERN char *Ssl_server_cert_DYN;	/* ssl server cert file */
EXTERN char *Ssl_server_password_file_DYN;	/* ssl server password file */
EXTERN int Stalled_time_DYN; /* amount of time before reporing stalled job */
EXTERN char* Status_file_DYN; /* printer status file name */
EXTERN int Stop_on_abort_DYN; /* stop when job aborts */
EXTERN char* Stty_command_DYN; /* stty commands to set output line characteristics */
EXTERN int Suppress_header_DYN; /* suppress headers and/or banner page */
EXTERN int Suspend_OF_filter_DYN; /* suspend OF filter */
EXTERN char* Syslog_device_DYN;	/* default syslog() facility */
EXTERN char* Trailer_on_close_DYN; /* trailer string to print when queue empties */
EXTERN char* Unix_socket_path_DYN;	/* UNIX socket pathname */
EXTERN int Use_info_cache_DYN;
EXTERN int Use_queuename_DYN;	/* put queuename in control file */
EXTERN int Use_queuename_flag_DYN;	/* Specified with the -Q option */
EXTERN int Use_shorthost_DYN;	/* Use short hostname in control file information */
EXTERN char* User_printcap_DYN;	/* Allow a ${HOME}/.printcap file - name of file*/
EXTERN int User_is_authuser_DYN;	/* set user to be authentication user value */ 
EXTERN char* Xlate_incoming_format_DYN;	/* translate format ids on incoming jobs */
EXTERN char* Xlate_format_DYN;	/* translate format ids on outgoing jobs */

#if defined(DMALLOC)
#  include <dmalloc.h>
extern int dmalloc_outfile;
#endif

#endif

//JYWENG20031106status
int *currten_sock;//JY1120
char printerstatus[32];
char clientaddr[32];//JY1110
int fd_print;//JY1110
FILE *STATUSFILE;//JY1113
typedef struct lptStatus//JY1114: move from lpd.c
{
    int     pid;        //PID of the process using the printer
    char    addr[32];   //IP address of the Process
    char    busy;       //TRUE if the Printer is busy otherwise FALSE
} LPT_STATUS;
LPT_STATUS          lptstatus;  //Golbal variable to store the status of server
#define FALSE   0
#define TRUE    1
#define ONLINE	"On-line"
#define printf(fmt, args...) 
//
