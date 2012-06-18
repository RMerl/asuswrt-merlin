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
 *
 ***************************************************************************/

 static char *const _id =
"$Id: vars.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


/* force local definitions */
#define EXTERN
#define DEFINE(X) X
#define DEFS

#include "lp.h"
#include "defs.h"
#include "child.h"
#include "gethostinfo.h"
#include "getqueue.h"
#include "accounting.h"
#include "permission.h"
#include "printjob.h"
/**** ENDINCLUDE ****/
#ifdef REMOVE
/***************************************************************************

Commentary:
Patrick Powell Tue Nov 26 08:10:12 PST 1996
Put all of the variables in a separate file.

 ***************************************************************************/

/*
 * configuration values that are checked and expanded
 */

#if !defined(GETENV)
#define GETENV "0"
#endif
#if !defined(TARGETARCHITECTURE)
#define TARGETARCHITECTURE "unknown"
#endif
#if !defined(LPD_CONF_PATH)
#error Missing LPD_CONF_PATH definition
#endif
#if !defined(LPD_PERMS_PATH)
#error Missing LPD_PERMS_PATH definition
#endif
#if !defined(PRINTCAP_PATH)
#error Missing PRINTCAP_PATH definition
#endif
#if !defined(LPD_PRINTCAP_PATH)
#error Missing LPD_PRINTCAP_PATH definition
#endif
#if !defined(FORCE_LOCALHOST)
#error Missing FORCE_LOCALHOST definition
#endif
#if !defined(REQUIRE_CONFIGFILES)
#error Missing REQUIRE_CONFIGFILES definition
#endif
#if !defined(FILTER_PATH)
#define FILTER_PATH "/bin:/usr/bin:/usr/contrib/bin:/usr/local/bin:/usr/ucb:/usr/sbin:/usr/etc:/etc"
#endif
#if !defined(LD_LIBRARY_PATH)
#define LD_LIBRARY_PATH "/lib:/usr/lib:/usr/5lib:/usr/ucblib"
#endif
#if !defined(LOCKFILE)
#error Missing LOCKFILE definition
#endif
#if !defined(USERID)
#error Missing USERID definition
#endif
#if !defined(GROUPID)
#error Missing GROUPID definition
#endif
#if !defined(DONE_JOBS)
#error Missing DONE_JOBS definition
#endif
#if !defined(DONE_JOBS_MAX_AGE)
#error Missing DONE_JOBS_MAX_AGE definition
#endif
#if !defined(UNIXSOCKETPATH)
#error Missing UNIXSOCKETPATH definition
#endif
#if !defined(PRUTIL)
#error Missing PRUTIL definition
#endif

/*
 * printcap variables used by LPD for printing
 * THESE MUST BE IN SORTED ORDER
 *  NOTE:  the maxval field is used to suppress clearing
 *   these values when initializing the printcap variable
 *   values.
 */


struct keywords Pc_var_list[] = {

/* XXSTARTXX */
   /*  always print banner, ignore lpr -h option */
{ "ab", 0,  FLAG_K,  &Always_banner_DYN,0,0,0},
   /*  set accounting name in control file based on host name */
{ "accounting_namefixup", 0,  STRING_K,  &Accounting_namefixup_DYN,0,0,0},
   /*  query accounting server when connected */
{ "achk", 0,  FLAG_K,  &Accounting_check_DYN,0,0,0},
   /*  accounting at end (see also af, la, ar, as) */
{ "ae", 0,  STRING_K,  &Accounting_end_DYN,0,0,"=jobend $H $n $P $k $b $t $'C $'J $'M"},
   /*  name of accounting file (see also la, ar) */
{ "af", 0,  STRING_K,  &Accounting_file_DYN,0,0,"=acct"},
   /*  automatically hold all jobs */
{ "ah", 0,  FLAG_K,  &Auto_hold_DYN,0,0,0},
   /* Allow use of LPD_CONF environment variable */
{ "allow_getenv", 0, FLAG_K, &Allow_getenv_DYN,1,0,GETENV},
   /* allow users to request logging info using lpr -mhost%port */
{ "allow_user_logging", 0, FLAG_K, &Allow_user_logging_DYN,0,0,0},
   /* allow these users or UIDs to set owner of job.  For Samba front ending */
{ "allow_user_setting", 0, STRING_K, &Allow_user_setting_DYN,0,0,0},
   /* append these -Z options to end of options list on outgoing or filters */
{ "append_z", 0, STRING_K, &Append_Z_DYN,0,0,0},
   /*  write remote transfer accounting (if af is set) */
{ "ar", 0,  FLAG_K,  &Accounting_remote_DYN,0,0,"1"},
   /* host architecture */
{ "architecture", 0, STRING_K, &Architecture_DYN,1,0,TARGETARCHITECTURE},
   /*  accounting at start (see also af, la, ar) */
{ "as", 0,  STRING_K,  &Accounting_start_DYN,0,0,"=jobstart $H $n $P $k $b $t $'C $'J $'M"},
	/* authentication type for client to server */
{ "auth", 0,  STRING_K, &Auth_DYN,0,0,0},
   /*  client to server authentication filter */
{ "auth_forward", 0, STRING_K, &Auth_forward_DYN,0,0,0},
   /*  end banner printing program overides bp */
{ "be", 0,  STRING_K,  &Banner_end_DYN,0,0,0},
   /*  Berkeley LPD: job file strictly RFC-compliant */
{ "bk", 0,  FLAG_K,  &Backwards_compatible_DYN,0,0,0},
   /*  Berkeley LPD filter options */
{ "bk_filter_options", 0, STRING_K, &BK_filter_options_DYN,0,0,"=$P $w $l $x $y $F $c $L $i $J $C $0n $0h $-a"},
   /*  Berkeley LPD OF filter options */
{ "bk_of_filter_options", 0, STRING_K, &BK_of_filter_options_DYN,0,0,"=$w $l $x $y"},
   /*  backwards-compatible filters: use simple paramters */
{ "bkf", 0,  FLAG_K,  &Backwards_compatible_filter_DYN,0,0,0},
   /*  short banner line sent to banner printer */
{ "bl", 0,  STRING_K,  &Banner_line_DYN,0,0,"=$-C:$-n Job: $-J Date: $-t"},
   /*  banner printing program (see bs, be) */
{ "bp", 0,  STRING_K,  &Banner_printer_DYN,0,0,0},
   /*  format for bounce queue output */
{ "bq_format", 0,  STRING_K,  &Bounce_queue_format_DYN,0,0,"=f"},
   /*  if lp is a tty, set the baud rate (see ty) */
{ "br", 0,  INTEGER_K,  &Baud_rate_DYN,0,0,0},
   /* do not set priority from class name */
{ "break_classname_priority_link", 0,  FLAG_K,  &Break_classname_priority_link_DYN,0,0,0},
   /*  banner printing program overrides bp */
{ "bs", 0,  STRING_K,  &Banner_start_DYN,0,0,0},
   /* check for nonprintable file */
{ "check_for_nonprintable", 0, FLAG_K, &Check_for_nonprintable_DYN,0,0,0},
   /* check for RFC1179 protocol violations */
{ "check_for_protocol_violations", 0, FLAG_K, &Check_for_protocol_violations_DYN,0,0,0},
   /* filter selects the destination for a load balance queue */
{ "chooser", 0, STRING_K, &Chooser_DYN,0,0,0},
   /* interval between checks for available destination for load balance queue */
{ "chooser_interval", 0, INTEGER_K, &Chooser_interval_DYN,0,0,"=10"},
   /* user provided routine selects the destination for a load balance queue */
{ "chooser_routine", 0, FLAG_K, &Chooser_routine_DYN,0,0,0},
   /* show classname in status display */
{ "class_in_status", 0, FLAG_K, &Class_in_status_DYN,0,0,0},
   /*  comment identifying printer (LPQ) */
{ "cm", 0,  STRING_K,  &Comment_tag_DYN,0,0,0},
   /* configuration file */
{ "config_file", 0, STRING_K, &Config_file_DYN,1,0,"=" LPD_CONF_PATH},
   /* minimum interval between connections and jobs */
{ "connect_grace", 0, INTEGER_K, &Connect_grace_DYN,0,0,"=0"},
   /* interval between connection or open attempts */
{ "connect_interval", 0, INTEGER_K, &Connect_interval_DYN,0,0,"=10"},
   /* connection timeout for remote printers */
{ "connect_timeout", 0, INTEGER_K, &Connect_timeout_DYN,0,0,"=10"},
   /* control file line order */
{ "control_file_line_order", 0, STRING_K, &Control_file_line_order_DYN,0,0,0},
   /* control file filter */
{ "control_filter", 0, STRING_K, &Control_filter_DYN,0,0,0},
   /* create files in spool directory */
{ "create_files", 0, FLAG_K, &Create_files_DYN,0,0,0},
   /*  debug level set for queue handler */
{ "db", 0,  STRING_K,  &New_debug_DYN,0,0,0},
   /* default job format */
{ "default_format", 0, STRING_K, &Default_format_DYN,0,0,"=f"},
   /* default permission for files */
{ "default_permission", 0, STRING_K, &Default_permission_DYN,0,0,"=ACCEPT"},
   /* default printer */
{ "default_printer", 0, STRING_K, &Default_printer_DYN,0,0,"=missingprinter"},
   /* default job priority */
{ "default_priority", 0, STRING_K, &Default_priority_DYN,0,0,"=A"},
   /* default remote host */
{ "default_remote_host", 0, STRING_K, &Default_remote_host_DYN,0,0,"=localhost"},
   /* default temp directory for temp files */
{ "default_tmp_dir", 0, STRING_K, &Default_tmp_dir_DYN,0,0,"=/tmp"},
   /* printers that we should query for status information */
{ "destinations", 0, STRING_K, &Destinations_DYN,0,0,0},
   /* allow LPR to make direct socket connection to printer */
{ "direct", 0, FLAG_K, &Direct_DYN,0,0,0},
   /* keep the last NN done jobs for status purposes */
{ "done_jobs", 0, INTEGER_K, &Done_jobs_DYN,0,0,"=" DONE_JOBS},
   /* keep done jobs for at most max age seconds */
{ "done_jobs_max_age", 0, INTEGER_K, &Done_jobs_max_age_DYN,0,0,"=" DONE_JOBS_MAX_AGE},
   /* drop root permissions after binding to listening port */
{ "drop_root", 0, FLAG_K, &Drop_root_DYN,0,0,0},
   /* exit linger timeout to wait for socket to close */
{ "exit_linger_timeout", 0, INTEGER_K, &Exit_linger_timeout_DYN,0,0,"=600"},
   /* use this size (in Kbytes) when sending 'unknown' size files to a spooler */
{ "fakelargefile", 0,  INTEGER_K,  &Fake_large_file_DYN,0,0,0},
   /*  string to send for a form feed */
{ "ff", 0,  STRING_K,  &Form_feed_DYN,0,0,"=\\f"},
   /*  send a form feed (value set by ff) between files of a job */
{ "ff_separator", 0,  FLAG_K,  &FF_separator_DYN,0,0,0},
   /* enforce FIFO (first in, first out) sequential job printing order */
{ "fifo", 0, FLAG_K, &Fifo_DYN,0,0,0},
   /* FIFO lock file */
{ "fifo_lock_file", 0, STRING_K, &Fifo_lock_file_DYN,0,0,"=fifo.lock"},
   /* default filter */
{ "filter", 0, STRING_K, &Filter_DYN,0,0,0},
   /* filter LD_LIBRARY_PATH value */
{ "filter_ld_path", 0, STRING_K, &Filter_ld_path_DYN,0,0,"=" LD_LIBRARY_PATH },
   /* filter options */
{ "filter_options", 0, STRING_K, &Filter_options_DYN,0,0,"=$A $B $C $D $E $F $G $H $I $J $K $L $M $N $O $P $Q $R $S $T $U $V $W $X $Y $Z $a $b $c $d $e $f $g $h $i $j $k $l $m $n $o $p $q $r $s $t $u $v $w $x $y $z $-a"},
   /* filter PATH environment variable */
{ "filter_path", 0, STRING_K, &Filter_path_DYN,0,0,"=" FILTER_PATH },
   /* interval at which to check OF filter for error status */
{ "filter_poll_interval", 0, INTEGER_K, &Filter_poll_interval_DYN,0,0,"=30"},
   /* write filter errors to the :ps=status file if there is one */
{ "filter_stderr_to_status_file", 0, FLAG_K, &Filter_stderr_to_status_file_DYN,0,0,0},
   /*  print a form feed when device is opened */
{ "fo", 0,  FLAG_K,  &FF_on_open_DYN,0,0,0},
   /* force FQDN HOST value in control file */
{ "force_fqdn_hostname", 0,  FLAG_K,  &Force_FQDN_hostname_DYN,0,0,0},
   /* force IPADDR of Originating host for host value in control file */
{ "force_ipaddr_hostname", 0,  FLAG_K,  &Force_IPADDR_hostname_DYN,0,0,0},
   /* force clients to send all requests to localhost */
{ "force_localhost", 0,  FLAG_K,  &Force_localhost_DYN,0,0,FORCE_LOCALHOST},
   /*  force lpq status format for specified hostnames */
{ "force_lpq_status", 0, STRING_K, &Force_lpq_status_DYN,0,0,0},
   /*  force use of this queuename if none provided */
{ "force_queuename", 0, STRING_K, &Force_queuename_DYN,0,0,0},
   /*  print a form feed when device is closed */
{ "fq", 0,  FLAG_K,  &FF_on_close_DYN,0,0,0},
   /* full or complete time format */
{ "full_time", 0,  FLAG_K,  &Full_time_DYN,0,0,0},
   /*  valid output filter formats */
{ "fx", 0,  STRING_K,  &Formats_allowed_DYN,0,0,0},
   /* generate a banner when forwarding job */
{ "generate_banner", 0, FLAG_K, &Generate_banner_DYN,0,0,0},
   /* group to run SUID ROOT programs */
{ "group", 0, STRING_K, &Daemon_group_DYN,1,0,"=" GROUPID},
   /*  do a 'half close' on socket when sending job to remote printer */
{ "half_close", 0,  FLAG_K,  &Half_close_DYN,0,0,"=1"},
   /*  print banner after job instead of before */
{ "hl", 0,  FLAG_K,  &Banner_last_DYN,0,0,0},
   /*  filter command, run on a per-file basis */
{ "if", 0,  STRING_K,  &IF_Filter_DYN,0,0,0},
   /*  ignore requested user priority */
{ "ignore_requested_user_priority", 0,  FLAG_K,  &Ignore_requested_user_priority_DYN,0,0,0},
   /*  incoming job control file filter */
{ "incoming_control_filter", 0,  STRING_K,  &Incoming_control_filter_DYN,0,0,0},
   /*  Running IPV6 */
{ "ipv6", 0,  FLAG_K,  &IPV6Protocol_DYN,0,0,0},
	/* TCP keepalive enabled */
{ "keepalive", 0, FLAG_K, &Keepalive_DYN,0,0,"1"},
	/* remote server principal for server to server forwarding */
{ "kerberos_forward_principal", 0, STRING_K, &Kerberos_forward_principal_DYN,0,0,0},
	/* keytab file location for kerberos, used by server */
{ "kerberos_keytab", 0, STRING_K, &Kerberos_keytab_DYN,0,0,"=/etc/lpd.keytab"},
	/* key lifetime for kerberos, used by server */
{ "kerberos_life", 0, STRING_K, &Kerberos_life_DYN,0,0,0},
	/* key renewal time for kerberos, used by server */
{ "kerberos_renew", 0, STRING_K, &Kerberos_renew_DYN,0,0,0},
	/* remote server principle, overides default */
{ "kerberos_server_principal", 0, STRING_K, &Kerberos_server_principal_DYN,0,0,0},
	/* default service */
{ "kerberos_service", 0, STRING_K, &Kerberos_service_DYN,0,0,"=lpr"},
   /*  write local printer accounting (if af is set) */
{ "la", 0,  FLAG_K,  &Local_accounting_DYN,0,0,"1"},
   /*  leader string printed on printer open */
{ "ld", 0,  STRING_K,  &Leader_on_open_DYN,0,0,0},
   /*  error log file (servers, filters and prefilters) */
{ "lf", 0,  STRING_K,  &Log_file_DYN,0,0,"=log"},
   /* lock the IO device */
{ "lk", 0, FLAG_K,  &Lock_it_DYN,0,0,0},
   /* lpd lock file */
{ "lockfile", 0, STRING_K, &Lockfile_DYN,1,0,"=" LOCKFILE},
   /* where to send status information for logging */
{ "logger_destination", 0,  STRING_K,  &Logger_destination_DYN,0,0,0},
   /* maximum size in K of logger file */
{ "logger_max_size", 0,  INTEGER_K,  &Logger_max_size_DYN,0,0,0},
   /* path of file to hold logger information */
{ "logger_path", 0,  STRING_K,  &Logger_path_DYN,0,0,0},
   /* timeout between connection attempts to remote logger */
{ "logger_timeout", 0,  INTEGER_K,  &Logger_timeout_DYN,0,0,0},
   /*  use long job number (0 - 999999) when a job is submitted */
{ "longnumber", 0,  FLAG_K,  &Long_number_DYN,0,0,0},
   /*  device name or lp-pipe command to send output to */
{ "lp", 0,  STRING_K,  &Lp_device_DYN,0,0,0},
   /* force lpd to filter jobs (bounce) before sending to remote queue */
{ "lpd_bounce", 0, FLAG_K, &Lpd_bounce_DYN,0,0,0},
   /* force a poll operation */
{ "lpd_force_poll", 0, FLAG_K, &Force_poll_DYN,0,0,0},
   /* lpd server listen port port, "off" does not open port */
{ "lpd_listen_port", 0, STRING_K, &Lpd_listen_port_DYN,0,0,0},
   /*  lpd pathname for server use */
{ "lpd_path", 0,  STRING_K,  &Lpd_path_DYN,0,0,0},
   /* max number of queues to start at a time */
{ "lpd_poll_servers_started", 0, INTEGER_K, &Poll_servers_started_DYN,1,0,"=3"},
   /*  interval in secs between starting small numbers of queue servers */
{ "lpd_poll_start_interval", 0,  INTEGER_K,  &Poll_start_interval_DYN,0,0,"=1"},
   /*  interval in secs between servicing all queues */
{ "lpd_poll_time", 0,  INTEGER_K,  &Poll_time_DYN,0,0,"=600"},
   /* lpd port */
{ "lpd_port", 0, STRING_K, &Lpd_port_DYN,0,0,"=515"},
   /* lpd printcap path */
{ "lpd_printcap_path", 0, STRING_K, &Lpd_printcap_path_DYN,1,0,"=" LPD_PRINTCAP_PATH},
   /* maximum number of lpq status queries kept in cache */
{ "lpq_status_cached", 0, INTEGER_K, &Lpq_status_cached_DYN,0,0,"=10"},
   /* cached lpq status file */
{ "lpq_status_file", 0, STRING_K, &Lpq_status_file_DYN,0,0,"=lpq"},
   /* minimum interval between updates */
{ "lpq_status_interval", 0, INTEGER_K, &Lpq_status_interval_DYN,0,0,"=2"},
   /* cached lpq status timeout - refresh after this time */
{ "lpq_status_stale", 0, INTEGER_K, &Lpq_status_stale_DYN,0,0,"=3600"},
   /* Additional options for LPR */
{ "lpr", 0, STRING_K, &Lpr_opts_DYN,0,0,0},
   /* lpr will run job through filters and send single file */
{ "lpr_bounce", 0, FLAG_K, &Lpr_bounce_DYN,0,0,0},
   /* BSD LPR -m flag, does not require mail address */
{ "lpr_bsd", 0, FLAG_K, &LPR_bsd_DYN,0,0,0},
   /* numbers of times for lpr to try sending job - 0 is infinite */
{ "lpr_send_try", 0, INTEGER_K, &Lpr_send_try_DYN,0,0,"=3"},
   /* from address to use in mail messages */
{ "mail_from", 0, STRING_K, &Mail_from_DYN,0,0,0},
   /* mail to this operator on error */
{ "mail_operator_on_error", 0, STRING_K, &Mail_operator_on_error_DYN,0,0,0},
   /* maximum accounting file size in Kbytes */
{ "max_accounting_file_size", 0, INTEGER_K, &Max_accounting_file_size_DYN,0,0,"=1000"},
   /* maximum interval between connection attempts */
{ "max_connect_interval", 0, INTEGER_K, &Max_connect_interval_DYN,0,0,"=60"},
   /* maximum number of datafiles */
{ "max_datafiles", 0, INTEGER_K, &Max_datafiles_DYN,0,0,"=52"},
   /* maximum log file size in Kbytes */
{ "max_log_file_size", 0, INTEGER_K, &Max_log_file_size_DYN,0,0,"=1000"},
   /* maximum number of servers that can be active */
{ "max_servers_active", 0, INTEGER_K, &Max_servers_active_DYN,1,0,"=1024"},
   /* maximum length of status line */
{ "max_status_line", 0, INTEGER_K, &Max_status_line_DYN,0,0,"=79"},
   /* maximum size (in K) of status file */
{ "max_status_size", 0, INTEGER_K, &Max_status_size_DYN,0,0,"=10"},
   /*  maximum copies allowed */
{ "mc", 0,  INTEGER_K,  &Max_copies_DYN,0,0,"=1"},
   /* minimum accounting file size in Kbytes */
{ "min_accounting_file_size", 0, INTEGER_K, &Min_accounting_file_size_DYN,0,0,0},
   /* minimum log file size in Kbytes */
{ "min_log_file_size", 0, INTEGER_K, &Min_log_file_size_DYN,0,0,0},
   /* minimum status file size in Kbytes */
{ "min_status_size", 0, INTEGER_K, &Min_status_size_DYN,0,0,0},
   /* minimum amount of free space needed in K bytes */
{ "minfree", 0, INTEGER_K, &Minfree_DYN,0,0,0},
   /*  minimum number of printable characters for printable check */
{ "ml", 0,  INTEGER_K,  &Min_printable_count_DYN,0,0,0},
   /* millisecond time resolution */
{ "ms_time_resolution", 0,  FLAG_K,  &Ms_time_resolution_DYN,0,0,"=1"},
   /*  maximum job size (1Kb blocks, 0 = unlimited) */
{ "mx", 0,  INTEGER_K,  &Max_job_size_DYN,0,0,0},
   /*  use nonblocking open */
{ "nb", 0,  FLAG_K,  &Nonblocking_open_DYN,0,0,0},
   /* connection control for remote network printers */
{ "network_connect_grace", 0, INTEGER_K, &Network_connect_grace_DYN,0,0,0},
   /*  N line after cfA000... line in control file */
{ "nline_after_file", 0,  FLAG_K,  &Nline_after_file_DYN,0,0,0},
   /*  output filter, run once for all output */
{ "of", 0,  STRING_K,  &OF_Filter_DYN,0,0,0},
   /* OF filter options */
{ "of_filter_options", 0, STRING_K, &OF_filter_options_DYN,0,0,0},
   /* use user supplied queue order routine */
{ "order_routine", 0, FLAG_K, &Order_routine_DYN,0,0,0},
   /* orginate connections from these ports */
{ "originate_port", 0, STRING_K, &Originate_port_DYN,0,0,"=512 1023"},
   /* pass these environment variables to filters (clients and lpd)*/
{ "pass_env", 0,  STRING_K,  &Pass_env_DYN,0,0,"=PGPPASS,PGPPATH,PGPPASSFD,LANG,LC_CTYPE,LC_NUMERIC,LC_TIME,LC_COLLATE,LC_MONETARY,LC_MESSAGES,LC_PAPER,LC_NAME,LC_ADDRESS,LC_TELEPHONE,LC_MEASUREMENT,LC_IDENTIFICATION,LC_ALL" },
   /* lpd.perms files */
{ "perms_path", 0, STRING_K, &Printer_perms_path_DYN,1,0,"=" LPD_PERMS_PATH },
   /*  pgp path */
{ "pgp_path", 0,  STRING_K,  &Pgp_path_DYN,0,0,"=" PGP_PATH },
   /*  page length (in lines) */
{ "pl", 0,  INTEGER_K,  &Page_length_DYN,0,0,"=66"},
   /*  pr program for p format */
{ "pr", 0,  STRING_K,  &Pr_program_DYN,0,0,PRUTIL},
   /* prefix control file line to line, "Z O" -> Z to O, "OS Z" does O and S to Z */
{ "prefix_option_to_option", 0, STRING_K, &Prefix_option_to_option_DYN,0,0,0},
   /* prefix these -Z options to start of options list on outgoing or filters */
{ "prefix_z", 0, STRING_K, &Prefix_Z_DYN,0,0,0},
   /* /etc/printcap files */
{ "printcap_path", 0, STRING_K, &Printcap_path_DYN,1,0,"=" PRINTCAP_PATH},
   /*  printer status file name */
{ "ps", 0,  STRING_K,  &Status_file_DYN,0,0,"=status"},
   /*  page width (in characters) */
{ "pw", 0,  INTEGER_K,  &Page_width_DYN,0,0,"=80"},
   /*  page width in pixels (horizontal) */
{ "px", 0,  INTEGER_K,  &Page_x_DYN,0,0,0},
   /*  page length in pixels (vertical) */
{ "py", 0,  INTEGER_K,  &Page_y_DYN,0,0,0},
   /*  put queue name in control file */
{ "qq", 0,  FLAG_K,  &Use_queuename_DYN,0,0,"=1"},
   /*  print queue control file name */
{ "queue_control_file", 0,  STRING_K,  &Queue_control_file_DYN,0,0,"=control.pr"},
   /*  print queue lock file name */
{ "queue_lock_file", 0,  STRING_K,  &Queue_lock_file_DYN,0,0,"=lock.pr"},
   /*  print queue status file name */
{ "queue_status_file", 0,  STRING_K,  &Queue_status_file_DYN,0,0,"=status.pr"},
   /*  print queue unspooler pid file name */
{ "queue_unspooler_file", 0,  STRING_K,  &Queue_unspooler_file_DYN,0,0,"=unspooler.pr"},
   /*  operations allowed to remote host (lpr=R,lprm=M,lpq=Q,lpq -v=V,lpc=C) */
{ "remote_support", 0,  STRING_K,  &Remote_support_DYN,0,0,"=RMQVC"},
   /* remove these -Z options from options list on outgoing or filters */
{ "remove_z", 0, STRING_K, &Remove_Z_DYN,0,0,0},
   /*  report server as this value for LPQ status */
{ "report_server_as", 0,  STRING_K,  &Report_server_as_DYN,0,0,0},
   /*  client requires lpd.conf, printcap */
{ "require_configfiles", 0,  FLAG_K,  &Require_configfiles_DYN,0,0,"=" REQUIRE_CONFIGFILES},
   /* require default queue to be explicitly set */
{ "require_explicit_q", 0, FLAG_K, &Require_explicit_Q_DYN,0,0,"0"},
   /*  retry on ECONNREFUSED error */
{ "retry_econnrefused", 0,  FLAG_K,  &Retry_ECONNREFUSED_DYN,0,0,"1"},
   /*  retry making connection even when link is down */
{ "retry_nolink", 0,  FLAG_K,  &Retry_NOLINK_DYN,0,0,"1"},
   /*  return short status when specified remotehost */
{ "return_short_status", 0,  STRING_K,  &Return_short_status_DYN,0,0,0},
   /*  set SO_REUSEADDR on outgoing ports */
{ "reuse_addr", 0,  FLAG_K,  &Reuse_addr_DYN,0,0,0},
   /*  reverse LPQ status format when specified remotehost */
{ "reverse_lpq_status", 0,  STRING_K,  &Reverse_lpq_status_DYN,0,0,0},
   /*  reverse priority order, z-aZ-A, i.e.- A is highest, z is lowest */
{ "reverse_priority_order", 0,  FLAG_K,  &Reverse_priority_order_DYN,0,0,0},
   /*  restrict queue use to members of specified user groups */
{ "rg", 0,  STRING_K,  &RestrictToGroupMembers_DYN,0,0,0},
   /*  remote-queue machine (hostname) (with rp) */
{ "rm", 0,  STRING_K,  &RemoteHost_DYN,0,0,0},
   /*  routing filter, returns destinations */
{ "router", 0,  STRING_K,  &Routing_filter_DYN,0,0,0},
   /*  remote-queue printer name (with rp) */
{ "rp", 0,  STRING_K,  &RemotePrinter_DYN,0,0,0},
   /*  open the printer for reading and writing */
{ "rw", 0,  FLAG_K,  &Read_write_DYN,0,0,0},
   /*  additional safe characters to use in control files */
{ "safe_chars", 0,  STRING_K,  &Safe_chars_DYN,0,0,0},
   /*  save job when an error */
{ "save_on_error", 0,  FLAG_K,  &Save_on_error_DYN,0,0,0},
   /*  save job when done */
{ "save_when_done", 0,  FLAG_K,  &Save_when_done_DYN,0,0,0},
   /*  short banner (one line only) */
{ "sb", 0,  FLAG_K,  &Short_banner_DYN,0,0,0},
   /*  spool directory (only ONE printer per directory!) */
{ "sd", 0,  STRING_K,  &Spool_dir_DYN,0,0,0},
   /* send block of data, rather than individual files */
{ "send_block_format", 0, FLAG_K, &Send_block_format_DYN,0,0,0},
   /* send data files first, then control file */
{ "send_data_first", 0, FLAG_K, &Send_data_first_DYN,0,0,0},
   /* failure action for server to take after send_try attempts failed */
{ "send_failure_action", 0, STRING_K, &Send_failure_action_DYN,0,0,"=remove"},
   /* timeout for status or job file completion by filter or printer (default 0 - never time out) */
{ "send_job_rw_timeout", 0, INTEGER_K, &Send_job_rw_timeout_DYN,0,0,"=0"},
   /* timeout for read/write status or control operatons */
{ "send_query_rw_timeout", 0, INTEGER_K, &Send_query_rw_timeout_DYN,0,0,"=30"},
   /* numbers of times for server to try sending job - 0 is infinite */
{ "send_try", 0, INTEGER_K, &Send_try_DYN,0,0,"=3"},
   /* sendmail program */
{ "sendmail", 0, STRING_K, &Sendmail_DYN,0,0,"=/usr/sbin/sendmail -oi -t"},
   /* allow mail to user using the sendmail program */
{ "sendmail_to_user", 0, FLAG_K, &Sendmail_to_user_DYN,0,0,"=1"},
   /* server temporary file directory */
{ "server_tmp_dir", 0, STRING_K, &Server_tmp_dir_DYN,0,0,"=/tmp"},
   /*  no form feed separator between job files */
{ "sf", 0,  FLAG_K,  &No_FF_separator_DYN,0,0,"=1"},
   /*  suppress headers and/or banner page */
{ "sh", 0,  FLAG_K,  &Suppress_header_DYN,0,0,0},
   /*  SHELL enviornment variable value for filters */
{ "shell", 0,  STRING_K,  &Shell_DYN,0,0,"=/bin/sh"},
   /*  short status length in lines */
{ "short_status_length", 0,  INTEGER_K,  &Short_status_length_DYN,0,0,"=3"},
   /* set the SO_LINGER socket option */
{ "socket_linger", 0, INTEGER_K,  &Socket_linger_DYN,0,0,"=10"},
   /* spool directory permissions */
{ "spool_dir_perms", 0, INTEGER_K, &Spool_dir_perms_DYN,0,0,"=000700"},
   /* spool file permissions */
{ "spool_file_perms", 0, INTEGER_K, &Spool_file_perms_DYN,0,0,"=000600"},
   /*  name of queue that server serves (with sv) */
{ "ss", 0,  STRING_K,  &Server_queue_name_DYN,0,0,0},
   /*  ssl signer cert file directory */
{ "ssl_ca_file", 0,  STRING_K,  &Ssl_ca_file_DYN,0,0,"=" SSL_CA_FILE },
   /*  ssl signer cert files (concatenated) */
{ "ssl_ca_path", 0,  STRING_K,  &Ssl_ca_path_DYN,0,0,0 },
   /*  ssl crl file (certificate revocation list) */
{ "ssl_crl_file", 0,  STRING_K,  &Ssl_crl_file_DYN,0,0,"=" SSL_CRL_FILE },
   /*  ssl crl directory (certificate revocation list) */
{ "ssl_crl_path", 0,  STRING_K,  &Ssl_crl_path_DYN,0,0,0 },
   /*  ssl server certificate */
{ "ssl_server_cert", 0,  STRING_K,  &Ssl_server_cert_DYN,0,0,"=" SSL_SERVER_CERT },
   /*  ssl server cert password is in this file */
{ "ssl_server_password_file", 0,  STRING_K,  &Ssl_server_password_file_DYN,0,0,"=" SSL_SERVER_PASSWORD_FILE },
   /*  stalled job timeout */
{ "stalled_time", 0, INTEGER_K, &Stalled_time_DYN,0,0,"=120"},
   /*  stop processing queue on filter abort */
{ "stop_on_abort", 0,  FLAG_K,  &Stop_on_abort_DYN,0,0,0},
   /*  stty commands to set output line characteristics */
{ "stty", 0,  STRING_K,  &Stty_command_DYN,0,0,0},
   /*  suspend the OF filter or rerun it */
{ "suspend_of_filter", 0,  FLAG_K,  &Suspend_OF_filter_DYN,0,0,"=1"},
   /*  names of servers for queue (with ss) */
{ "sv", 0,  STRING_K,  &Server_names_DYN,0,0,0},
   /* name of syslog device */
{ "syslog_device", 0, STRING_K, &Syslog_device_DYN,0,0,"=/dev/console"},
   /*  trailer string to print when queue empties */
{ "tr", 0,  STRING_K,  &Trailer_on_close_DYN,0,0,0},
   /*  translate outgoing job file formats - similar to tr(1) utility */
{ "translate_format", 0,  STRING_K,  &Xlate_format_DYN,0,0,0},
   /*  translate incoming job file formats - similar to tr(1) utility */
{ "translate_incoming_format", 0,  STRING_K,  &Xlate_incoming_format_DYN,0,0,0},
   /*  path for UNIX socket for localhost connections */
{ "unix_socket_path", 0,  STRING_K,  &Unix_socket_path_DYN,0,0,"=" UNIXSOCKETPATH},
   /*  read and cache information */
{ "use_info_cache", 0, FLAG_K, &Use_info_cache_DYN,0,0,"1"},
   /*  put queue name in control file */
{ "use_shorthost", 0,  FLAG_K,  &Use_shorthost_DYN,0,0,0},
   /*  server user for SUID purposes */
{ "user", 0, STRING_K, &Daemon_user_DYN,1,0,"=" USERID},
   /*  set user to be authenication user identification */
{ "user_is_authuser", 0, FLAG_K, &User_is_authuser_DYN,0,0,0},
   /*  allow users to use local ${HOME}/.printcap */
{ "user_printcap", 0, STRING_K, &User_printcap_DYN,1,0,".printcap"},
/* END */
{ (char *)0,0,0,0,0,0,0 }
} ;

struct keywords DYN_var_list[] = {
{ "Logname_DYN", 0, STRING_K, &Logname_DYN,0,0,0},
{ "ShortHost_FQDN", 0, STRING_K, &ShortHost_FQDN,0,0,0},
{ "FQDNHost_FQDN", 0, STRING_K, &FQDNHost_FQDN,0,0,0},
{ "Printer_DYN", 0,  STRING_K, &Printer_DYN ,0,0,0},
{ "Queue_name_DYN", 0,  STRING_K, &Queue_name_DYN ,0,0,0},
{ "Lp_device_DYN", 0,  STRING_K, &Lp_device_DYN ,0,0,0},
{ "RemotePrinter_DYN", 0,  STRING_K, &RemotePrinter_DYN ,0,0,0},
{ "RemoteHost_DYN", 0,  STRING_K, &RemoteHost_DYN ,0,0,0},
{ "FQDNRemote_FQDN", 0,  STRING_K, &FQDNRemote_FQDN ,0,0,0},
{ "ShortRemote_FQDN", 0,  STRING_K, &ShortRemote_FQDN ,0,0,0},
{ "Current_date_DYN", 0,  STRING_K, &Current_date_DYN ,0,0,0},

{ (char *)0,0,0,0,0,0,0 }
} ;
#endif
