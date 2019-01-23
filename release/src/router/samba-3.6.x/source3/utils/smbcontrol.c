/* 
   Unix SMB/CIFS implementation.

   Send messages to other Samba daemons

   Copyright (C) Tim Potter 2003
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Martin Pool 2001-2002
   Copyright (C) Simo Sorce 2002
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
#include "system/filesys.h"
#include "popt_common.h"
#include "librpc/gen_ndr/spoolss.h"
#include "nt_printing.h"
#include "printing/notify.h"
#include "libsmb/nmblib.h"
#include "messages.h"
#include "util_tdb.h"

#if HAVE_LIBUNWIND_H
#include <libunwind.h>
#endif

#if HAVE_LIBUNWIND_PTRACE_H
#include <libunwind-ptrace.h>
#endif

#if HAVE_SYS_PTRACE_H
#include <sys/ptrace.h>
#endif

/* Default timeout value when waiting for replies (in seconds) */

#define DEFAULT_TIMEOUT 10

static int timeout = DEFAULT_TIMEOUT;
static int num_replies;		/* Used by message callback fns */

/* Send a message to a destination pid.  Zero means broadcast smbd. */

static bool send_message(struct messaging_context *msg_ctx,
			 struct server_id pid, int msg_type,
			 const void *buf, int len)
{
	bool ret;
	int n_sent = 0;

	if (procid_to_pid(&pid) != 0)
		return NT_STATUS_IS_OK(
			messaging_send_buf(msg_ctx, pid, msg_type,
					   (uint8 *)buf, len));

	ret = message_send_all(msg_ctx, msg_type, buf, len, &n_sent);
	DEBUG(10,("smbcontrol/send_message: broadcast message to "
		  "%d processes\n", n_sent));

	return ret;
}

static void smbcontrol_timeout(struct tevent_context *event_ctx,
			       struct tevent_timer *te,
			       struct timeval now,
			       void *private_data)
{
	bool *timed_out = (bool *)private_data;
	TALLOC_FREE(te);
	*timed_out = True;
}

/* Wait for one or more reply messages */

static void wait_replies(struct messaging_context *msg_ctx,
			 bool multiple_replies)
{
	struct tevent_timer *te;
	bool timed_out = False;

	if (!(te = tevent_add_timer(messaging_event_context(msg_ctx), NULL,
				    timeval_current_ofs(timeout, 0),
				    smbcontrol_timeout, (void *)&timed_out))) {
		DEBUG(0, ("tevent_add_timer failed\n"));
		return;
	}

	while (!timed_out) {
		int ret;
		if (num_replies > 0 && !multiple_replies)
			break;
		ret = tevent_loop_once(messaging_event_context(msg_ctx));
		if (ret != 0) {
			break;
		}
	}
}

/* Message handler callback that displays the PID and a string on stdout */

static void print_pid_string_cb(struct messaging_context *msg,
				void *private_data, 
				uint32_t msg_type, 
				struct server_id pid,
				DATA_BLOB *data)
{
	char *pidstr;

	pidstr = procid_str(talloc_tos(), &pid);
	printf("PID %s: %.*s", pidstr, (int)data->length,
	       (const char *)data->data);
	TALLOC_FREE(pidstr);
	num_replies++;
}

/* Message handler callback that displays a string on stdout */

static void print_string_cb(struct messaging_context *msg,
			    void *private_data, 
			    uint32_t msg_type, 
			    struct server_id pid,
			    DATA_BLOB *data)
{
	printf("%*s", (int)data->length, (const char *)data->data);
	num_replies++;
}

/* Send no message.  Useful for testing. */

static bool do_noop(struct messaging_context *msg_ctx,
		    const struct server_id pid,
		    const int argc, const char **argv)
{
	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol <dest> noop\n");
		return False;
	}

	/* Move along, nothing to see here */

	return True;
}

/* Send a debug string */

static bool do_debug(struct messaging_context *msg_ctx,
		     const struct server_id pid,
		     const int argc, const char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: smbcontrol <dest> debug "
			"<debug-string>\n");
		return False;
	}

	return send_message(msg_ctx, pid, MSG_DEBUG, argv[1],
			    strlen(argv[1]) + 1);
}


static bool do_idmap(struct messaging_context *msg_ctx,
		     const struct server_id pid,
		     const int argc, const char **argv)
{
	static const char* usage = "Usage: "
		"smbcontrol <dest> idmap <cmd> [arg]\n"
		"\tcmd:\tflush [gid|uid]\n"
		"\t\tdelete \"UID <uid>\"|\"GID <gid>\"|<sid>\n"
		"\t\tkill \"UID <uid>\"|\"GID <gid>\"|<sid>\n";
	const char* arg = NULL;
	int arglen = 0;
	int msg_type;

	switch (argc) {
	case 2:
		break;
	case 3:
		arg = argv[2];
		arglen = strlen(arg) + 1;
		break;
	default:
		fprintf(stderr, "%s", usage);
		return false;
	}

	if (strcmp(argv[1], "flush") == 0) {
		msg_type = MSG_IDMAP_FLUSH;
	}
	else if (strcmp(argv[1], "delete") == 0) {
		msg_type = MSG_IDMAP_DELETE;
	}
	else if (strcmp(argv[1], "kill") == 0) {
		msg_type = MSG_IDMAP_KILL;
	}
	else if (strcmp(argv[1], "help") == 0) {
		fprintf(stdout, "%s", usage);
		return true;
	}
	else {
		fprintf(stderr, "%s", usage);
		return false;
	}

	return send_message(msg_ctx, pid, msg_type, arg, arglen);
}


#if defined(HAVE_LIBUNWIND_PTRACE) && defined(HAVE_LINUX_PTRACE)

/* Return the name of a process given it's PID. This will only work on Linux,
 * but that's probably moot since this whole stack tracing implementatino is
 * Linux-specific anyway.
 */
static const char * procname(pid_t pid, char * buf, size_t bufsz)
{
	char path[64];
	FILE * fp;

	snprintf(path, sizeof(path), "/proc/%llu/cmdline",
		(unsigned long long)pid);
	if ((fp = fopen(path, "r")) == NULL) {
		return NULL;
	}

	fgets(buf, bufsz, fp);

	fclose(fp);
	return buf;
}

static void print_stack_trace(pid_t pid, int * count)
{
	void *		    pinfo = NULL;
	unw_addr_space_t    aspace = NULL;
	unw_cursor_t	    cursor;
	unw_word_t	    ip, sp;

	char		    nbuf[256];
	unw_word_t	    off;

	int ret;

	if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) < 0) {
		fprintf(stderr,
			"Failed to attach to process %llu: %s\n",
			(unsigned long long)pid, strerror(errno));
		return;
	}

	/* Wait until the attach is complete. */
	waitpid(pid, NULL, 0);

	if (((pinfo = _UPT_create(pid)) == NULL) ||
	    ((aspace = unw_create_addr_space(&_UPT_accessors, 0)) == NULL)) {
		/* Probably out of memory. */
		fprintf(stderr,
			"Unable to initialize stack unwind for process %llu\n",
			(unsigned long long)pid);
		goto cleanup;
	}

	if ((ret = unw_init_remote(&cursor, aspace, pinfo))) {
		fprintf(stderr,
			"Unable to unwind stack for process %llu: %s\n",
			(unsigned long long)pid, unw_strerror(ret));
		goto cleanup;
	}

	if (*count > 0) {
		printf("\n");
	}

	if (procname(pid, nbuf, sizeof(nbuf))) {
		printf("Stack trace for process %llu (%s):\n",
			(unsigned long long)pid, nbuf);
	} else {
		printf("Stack trace for process %llu:\n",
			(unsigned long long)pid);
	}

	while (unw_step(&cursor) > 0) {
		ip = sp = off = 0;
		unw_get_reg(&cursor, UNW_REG_IP, &ip);
		unw_get_reg(&cursor, UNW_REG_SP, &sp);

		ret = unw_get_proc_name(&cursor, nbuf, sizeof(nbuf), &off);
		if (ret != 0 && ret != -UNW_ENOMEM) {
			snprintf(nbuf, sizeof(nbuf), "<unknown symbol>");
		}
		printf("    %s + %#llx [ip=%#llx] [sp=%#llx]\n",
			nbuf, (long long)off, (long long)ip,
			(long long)sp);
	}

	(*count)++;

cleanup:
	if (aspace) {
		unw_destroy_addr_space(aspace);
	}

	if (pinfo) {
		_UPT_destroy(pinfo);
	}

	ptrace(PTRACE_DETACH, pid, NULL, NULL);
}

static int stack_trace_connection(const struct connections_key *key,
				  const struct connections_data *crec,
				  void *priv)
{
	print_stack_trace(procid_to_pid(&crec->pid), (int *)priv);

	return 0;
}

static bool do_daemon_stack_trace(struct messaging_context *msg_ctx,
				  const struct server_id pid,
		       const int argc, const char **argv)
{
	pid_t	dest;
	int	count = 0;

	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol <dest> stacktrace\n");
		return False;
	}

	dest = procid_to_pid(&pid);

	if (dest != 0) {
		/* It would be nice to be able to make sure that this PID is
		 * the PID of a smbd/winbind/nmbd process, not some random PID
		 * the user liked the look of. It doesn't seem like it's worth
		 * the effort at the moment, however.
		 */
		print_stack_trace(dest, &count);
	} else {
		connections_forall_read(stack_trace_connection, &count);
	}

	return True;
}

#else /* defined(HAVE_LIBUNWIND_PTRACE) && defined(HAVE_LINUX_PTRACE) */

static bool do_daemon_stack_trace(struct messaging_context *msg_ctx,
				  const struct server_id pid,
		       const int argc, const char **argv)
{
	fprintf(stderr,
		"Daemon stack tracing is not supported on this platform\n");
	return False;
}

#endif /* defined(HAVE_LIBUNWIND_PTRACE) && defined(HAVE_LINUX_PTRACE) */

/* Inject a fault (fatal signal) into a running smbd */

static bool do_inject_fault(struct messaging_context *msg_ctx,
			    const struct server_id pid,
		       const int argc, const char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: smbcontrol <dest> inject "
			"<bus|hup|term|internal|segv>\n");
		return False;
	}

#ifndef DEVELOPER
	fprintf(stderr, "Fault injection is only available in "
		"developer builds\n");
	return False;
#else /* DEVELOPER */
	{
		int sig = 0;

		if (strcmp(argv[1], "bus") == 0) {
			sig = SIGBUS;
		} else if (strcmp(argv[1], "hup") == 0) {
			sig = SIGHUP;
		} else if (strcmp(argv[1], "term") == 0) {
			sig = SIGTERM;
		} else if (strcmp(argv[1], "segv") == 0) {
			sig = SIGSEGV;
		} else if (strcmp(argv[1], "internal") == 0) {
			/* Force an internal error, ie. an unclean exit. */
			sig = -1;
		} else {
			fprintf(stderr, "Unknown signal name '%s'\n", argv[1]);
			return False;
		}

		return send_message(msg_ctx, pid, MSG_SMB_INJECT_FAULT,
				    &sig, sizeof(int));
	}
#endif /* DEVELOPER */
}

/* Force a browser election */

static bool do_election(struct messaging_context *msg_ctx,
			const struct server_id pid,
			const int argc, const char **argv)
{
	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol <dest> force-election\n");
		return False;
	}

	return send_message(msg_ctx, pid, MSG_FORCE_ELECTION, NULL, 0);
}

/* Ping a samba daemon process */

static void pong_cb(struct messaging_context *msg,
		    void *private_data, 
		    uint32_t msg_type, 
		    struct server_id pid,
		    DATA_BLOB *data)
{
	char *src_string = procid_str(NULL, &pid);
	printf("PONG from pid %s\n", src_string);
	TALLOC_FREE(src_string);
	num_replies++;
}

static bool do_ping(struct messaging_context *msg_ctx,
		    const struct server_id pid,
		    const int argc, const char **argv)
{
	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol <dest> ping\n");
		return False;
	}

	/* Send a message and register our interest in a reply */

	if (!send_message(msg_ctx, pid, MSG_PING, NULL, 0))
		return False;

	messaging_register(msg_ctx, NULL, MSG_PONG, pong_cb);

	wait_replies(msg_ctx, procid_to_pid(&pid) == 0);

	/* No replies were received within the timeout period */

	if (num_replies == 0)
		printf("No replies received\n");

	messaging_deregister(msg_ctx, MSG_PONG, NULL);

	return num_replies;
}

/* Set profiling options */

static bool do_profile(struct messaging_context *msg_ctx,
		       const struct server_id pid,
		       const int argc, const char **argv)
{
	int v;

	if (argc != 2) {
		fprintf(stderr, "Usage: smbcontrol <dest> profile "
			"<off|count|on|flush>\n");
		return False;
	}

	if (strcmp(argv[1], "off") == 0) {
		v = 0;
	} else if (strcmp(argv[1], "count") == 0) {
		v = 1;
	} else if (strcmp(argv[1], "on") == 0) {
		v = 2;
	} else if (strcmp(argv[1], "flush") == 0) {
		v = 3;
	} else {
		fprintf(stderr, "Unknown profile command '%s'\n", argv[1]);
		return False;
	}

	return send_message(msg_ctx, pid, MSG_PROFILE, &v, sizeof(int));
}

/* Return the profiling level */

static void profilelevel_cb(struct messaging_context *msg_ctx,
			    void *private_data, 
			    uint32_t msg_type, 
			    struct server_id pid,
			    DATA_BLOB *data)
{
	int level;
	const char *s;

	num_replies++;

	if (data->length != sizeof(int)) {
		fprintf(stderr, "invalid message length %ld returned\n", 
			(unsigned long)data->length);
		return;
	}

	memcpy(&level, data->data, sizeof(int));

	switch (level) {
	case 0:
		s = "not enabled";
		break;
	case 1:
		s = "off";
		break;
	case 3:
		s = "count only";
		break;
	case 7:
		s = "count and time";
		break;
	default:
		s = "BOGUS";
		break;
	}

	printf("Profiling %s on pid %u\n",s,(unsigned int)procid_to_pid(&pid));
}

static void profilelevel_rqst(struct messaging_context *msg_ctx,
			      void *private_data, 
			      uint32_t msg_type, 
			      struct server_id pid,
			      DATA_BLOB *data)
{
	int v = 0;

	/* Send back a dummy reply */

	send_message(msg_ctx, pid, MSG_PROFILELEVEL, &v, sizeof(int));
}

static bool do_profilelevel(struct messaging_context *msg_ctx,
			    const struct server_id pid,
			    const int argc, const char **argv)
{
	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol <dest> profilelevel\n");
		return False;
	}

	/* Send a message and register our interest in a reply */

	if (!send_message(msg_ctx, pid, MSG_REQ_PROFILELEVEL, NULL, 0))
		return False;

	messaging_register(msg_ctx, NULL, MSG_PROFILELEVEL, profilelevel_cb);
	messaging_register(msg_ctx, NULL, MSG_REQ_PROFILELEVEL,
			   profilelevel_rqst);

	wait_replies(msg_ctx, procid_to_pid(&pid) == 0);

	/* No replies were received within the timeout period */

	if (num_replies == 0)
		printf("No replies received\n");

	messaging_deregister(msg_ctx, MSG_PROFILE, NULL);

	return num_replies;
}

/* Display debug level settings */

static bool do_debuglevel(struct messaging_context *msg_ctx,
			  const struct server_id pid,
			  const int argc, const char **argv)
{
	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol <dest> debuglevel\n");
		return False;
	}

	/* Send a message and register our interest in a reply */

	if (!send_message(msg_ctx, pid, MSG_REQ_DEBUGLEVEL, NULL, 0))
		return False;

	messaging_register(msg_ctx, NULL, MSG_DEBUGLEVEL, print_pid_string_cb);

	wait_replies(msg_ctx, procid_to_pid(&pid) == 0);

	/* No replies were received within the timeout period */

	if (num_replies == 0)
		printf("No replies received\n");

	messaging_deregister(msg_ctx, MSG_DEBUGLEVEL, NULL);

	return num_replies;
}

/* Send a print notify message */

static bool do_printnotify(struct messaging_context *msg_ctx,
			   const struct server_id pid,
			   const int argc, const char **argv)
{
	const char *cmd;

	/* Check for subcommand */

	if (argc == 1) {
		fprintf(stderr, "Must specify subcommand:\n");
		fprintf(stderr, "\tqueuepause <printername>\n");
		fprintf(stderr, "\tqueueresume <printername>\n");
		fprintf(stderr, "\tjobpause <printername> <unix jobid>\n");
		fprintf(stderr, "\tjobresume <printername> <unix jobid>\n");
		fprintf(stderr, "\tjobdelete <printername> <unix jobid>\n");
		fprintf(stderr, "\tprinter <printername> <comment|port|"
			"driver> <value>\n");

		return False;
	}

	cmd = argv[1];

	if (strcmp(cmd, "queuepause") == 0) {

		if (argc != 3) {
			fprintf(stderr, "Usage: smbcontrol <dest> printnotify"
				" queuepause <printername>\n");
			return False;
		}

		notify_printer_status_byname(messaging_event_context(msg_ctx),
					     msg_ctx, argv[2],
					     PRINTER_STATUS_PAUSED);

		goto send;

	} else if (strcmp(cmd, "queueresume") == 0) {

		if (argc != 3) {
			fprintf(stderr, "Usage: smbcontrol <dest> printnotify"
				" queuereume <printername>\n");
			return False;
		}

		notify_printer_status_byname(messaging_event_context(msg_ctx),
					     msg_ctx, argv[2],
					     PRINTER_STATUS_OK);

		goto send;

	} else if (strcmp(cmd, "jobpause") == 0) {
		int jobid;

		if (argc != 4) {
			fprintf(stderr, "Usage: smbcontrol <dest> printnotify"
				" jobpause <printername> <unix-jobid>\n");
			return False;
		}

		jobid = atoi(argv[3]);

		notify_job_status_byname(
			messaging_event_context(msg_ctx), msg_ctx,
			argv[2], jobid, JOB_STATUS_PAUSED,
			SPOOLSS_NOTIFY_MSG_UNIX_JOBID);

		goto send;

	} else if (strcmp(cmd, "jobresume") == 0) {
		int jobid;

		if (argc != 4) {
			fprintf(stderr, "Usage: smbcontrol <dest> printnotify"
				" jobpause <printername> <unix-jobid>\n");
			return False;
		}

		jobid = atoi(argv[3]);

		notify_job_status_byname(
			messaging_event_context(msg_ctx), msg_ctx,
			argv[2], jobid, JOB_STATUS_QUEUED, 
			SPOOLSS_NOTIFY_MSG_UNIX_JOBID);

		goto send;

	} else if (strcmp(cmd, "jobdelete") == 0) {
		int jobid;

		if (argc != 4) {
			fprintf(stderr, "Usage: smbcontrol <dest> printnotify"
				" jobpause <printername> <unix-jobid>\n");
			return False;
		}

		jobid = atoi(argv[3]);

		notify_job_status_byname(
			messaging_event_context(msg_ctx), msg_ctx,
			argv[2], jobid, JOB_STATUS_DELETING,
			SPOOLSS_NOTIFY_MSG_UNIX_JOBID);

		notify_job_status_byname(
			messaging_event_context(msg_ctx), msg_ctx,
			argv[2], jobid, JOB_STATUS_DELETING|
			JOB_STATUS_DELETED,
			SPOOLSS_NOTIFY_MSG_UNIX_JOBID);

		goto send;

	} else if (strcmp(cmd, "printer") == 0) {
		uint32 attribute;

		if (argc != 5) {
			fprintf(stderr, "Usage: smbcontrol <dest> printnotify "
				"printer <printername> <comment|port|driver> "
				"<value>\n");
			return False;
		}

		if (strcmp(argv[3], "comment") == 0) {
			attribute = PRINTER_NOTIFY_FIELD_COMMENT;
		} else if (strcmp(argv[3], "port") == 0) {
			attribute = PRINTER_NOTIFY_FIELD_PORT_NAME;
		} else if (strcmp(argv[3], "driver") == 0) {
			attribute = PRINTER_NOTIFY_FIELD_DRIVER_NAME;
		} else {
			fprintf(stderr, "Invalid printer command '%s'\n",
				argv[3]);
			return False;
		}

		notify_printer_byname(messaging_event_context(msg_ctx),
				      msg_ctx, argv[2], attribute,
				      CONST_DISCARD(char *, argv[4]));

		goto send;
	}

	fprintf(stderr, "Invalid subcommand '%s'\n", cmd);
	return False;

send:
	print_notify_send_messages(msg_ctx, 0);
	return True;
}

/* Close a share */

static bool do_closeshare(struct messaging_context *msg_ctx,
			  const struct server_id pid,
			  const int argc, const char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: smbcontrol <dest> close-share "
			"<sharename>\n");
		return False;
	}

	return send_message(msg_ctx, pid, MSG_SMB_FORCE_TDIS, argv[1],
			    strlen(argv[1]) + 1);
}

/* Tell winbindd an IP got dropped */

static bool do_ip_dropped(struct messaging_context *msg_ctx,
			  const struct server_id pid,
			  const int argc, const char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: smbcontrol <dest> ip-dropped "
			"<ip-address>\n");
		return False;
	}

	return send_message(msg_ctx, pid, MSG_WINBIND_IP_DROPPED, argv[1],
			    strlen(argv[1]) + 1);
}

/* force a blocking lock retry */

static bool do_lockretry(struct messaging_context *msg_ctx,
			 const struct server_id pid,
			 const int argc, const char **argv)
{
	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol <dest> lockretry\n");
		return False;
	}

	return send_message(msg_ctx, pid, MSG_SMB_UNLOCK, NULL, 0);
}

/* force a validation of all brl entries, including re-sends. */

static bool do_brl_revalidate(struct messaging_context *msg_ctx,
			      const struct server_id pid,
			      const int argc, const char **argv)
{
	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol <dest> brl-revalidate\n");
		return False;
	}

	return send_message(msg_ctx, pid, MSG_SMB_BRL_VALIDATE, NULL, 0);
}

/* Force a SAM synchronisation */

static bool do_samsync(struct messaging_context *msg_ctx,
		       const struct server_id pid,
		       const int argc, const char **argv)
{
	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol <dest> samsync\n");
		return False;
	}

	return send_message(msg_ctx, pid, MSG_SMB_SAM_SYNC, NULL, 0);
}

/* Force a SAM replication */

static bool do_samrepl(struct messaging_context *msg_ctx,
		       const struct server_id pid,
		       const int argc, const char **argv)
{
	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol <dest> samrepl\n");
		return False;
	}

	return send_message(msg_ctx, pid, MSG_SMB_SAM_REPL, NULL, 0);
}

/* Display talloc pool usage */

static bool do_poolusage(struct messaging_context *msg_ctx,
			 const struct server_id pid,
			 const int argc, const char **argv)
{
	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol <dest> pool-usage\n");
		return False;
	}

	messaging_register(msg_ctx, NULL, MSG_POOL_USAGE, print_string_cb);

	/* Send a message and register our interest in a reply */

	if (!send_message(msg_ctx, pid, MSG_REQ_POOL_USAGE, NULL, 0))
		return False;

	wait_replies(msg_ctx, procid_to_pid(&pid) == 0);

	/* No replies were received within the timeout period */

	if (num_replies == 0)
		printf("No replies received\n");

	messaging_deregister(msg_ctx, MSG_POOL_USAGE, NULL);

	return num_replies;
}

/* Perform a dmalloc mark */

static bool do_dmalloc_mark(struct messaging_context *msg_ctx,
			    const struct server_id pid,
			    const int argc, const char **argv)
{
	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol <dest> dmalloc-mark\n");
		return False;
	}

	return send_message(msg_ctx, pid, MSG_REQ_DMALLOC_MARK, NULL, 0);
}

/* Perform a dmalloc changed */

static bool do_dmalloc_changed(struct messaging_context *msg_ctx,
			       const struct server_id pid,
			       const int argc, const char **argv)
{
	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol <dest> "
			"dmalloc-log-changed\n");
		return False;
	}

	return send_message(msg_ctx, pid, MSG_REQ_DMALLOC_LOG_CHANGED,
			    NULL, 0);
}

/* Shutdown a server process */

static bool do_shutdown(struct messaging_context *msg_ctx,
			const struct server_id pid,
			const int argc, const char **argv)
{
	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol <dest> shutdown\n");
		return False;
	}

	return send_message(msg_ctx, pid, MSG_SHUTDOWN, NULL, 0);
}

/* Notify a driver upgrade */

static bool do_drvupgrade(struct messaging_context *msg_ctx,
			  const struct server_id pid,
			  const int argc, const char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: smbcontrol <dest> drvupgrade "
			"<driver-name>\n");
		return False;
	}

	return send_message(msg_ctx, pid, MSG_PRINTER_DRVUPGRADE, argv[1],
			    strlen(argv[1]) + 1);
}

static bool do_winbind_online(struct messaging_context *msg_ctx,
			      const struct server_id pid,
			     const int argc, const char **argv)
{
	TDB_CONTEXT *tdb;

	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol winbindd online\n");
		return False;
	}

	/* Remove the entry in the winbindd_cache tdb to tell a later
	   starting winbindd that we're online. */

	tdb = tdb_open_log(cache_path("winbindd_cache.tdb"), 0, TDB_DEFAULT, O_RDWR, 0600);
	if (!tdb) {
		fprintf(stderr, "Cannot open the tdb %s for writing.\n",
			cache_path("winbindd_cache.tdb"));
		return False;
	}

	tdb_delete_bystring(tdb, "WINBINDD_OFFLINE");
	tdb_close(tdb);

	return send_message(msg_ctx, pid, MSG_WINBIND_ONLINE, NULL, 0);
}

static bool do_winbind_offline(struct messaging_context *msg_ctx,
			       const struct server_id pid,
			     const int argc, const char **argv)
{
	TDB_CONTEXT *tdb;
	bool ret = False;
	int retry = 0;

	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol winbindd offline\n");
		return False;
	}

	/* Create an entry in the winbindd_cache tdb to tell a later
	   starting winbindd that we're offline. We may actually create
	   it here... */

	tdb = tdb_open_log(cache_path("winbindd_cache.tdb"),
				WINBINDD_CACHE_TDB_DEFAULT_HASH_SIZE,
				TDB_DEFAULT|TDB_INCOMPATIBLE_HASH /* TDB_CLEAR_IF_FIRST */,
				O_RDWR|O_CREAT, 0600);

	if (!tdb) {
		fprintf(stderr, "Cannot open the tdb %s for writing.\n",
			cache_path("winbindd_cache.tdb"));
		return False;
	}

	/* There's a potential race condition that if a child
	   winbindd detects a domain is online at the same time
	   we're trying to tell it to go offline that it might 
	   delete the record we add between us adding it and
	   sending the message. Minimize this by retrying up to
	   5 times. */

	for (retry = 0; retry < 5; retry++) {
		TDB_DATA d;
		uint8 buf[4];

		ZERO_STRUCT(d);

		SIVAL(buf, 0, time(NULL));
		d.dptr = buf;
		d.dsize = 4;

		tdb_store_bystring(tdb, "WINBINDD_OFFLINE", d, TDB_INSERT);

		ret = send_message(msg_ctx, pid, MSG_WINBIND_OFFLINE,
				   NULL, 0);

		/* Check that the entry "WINBINDD_OFFLINE" still exists. */
		d = tdb_fetch_bystring( tdb, "WINBINDD_OFFLINE" );

		if (!d.dptr || d.dsize != 4) {
			SAFE_FREE(d.dptr);
			DEBUG(10,("do_winbind_offline: offline state not set - retrying.\n"));
		} else {
			SAFE_FREE(d.dptr);
			break;
		}
	}

	tdb_close(tdb);
	return ret;
}

static bool do_winbind_onlinestatus(struct messaging_context *msg_ctx,
				    const struct server_id pid,
				    const int argc, const char **argv)
{
	struct server_id myid;

	myid = messaging_server_id(msg_ctx);

	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol winbindd onlinestatus\n");
		return False;
	}

	messaging_register(msg_ctx, NULL, MSG_WINBIND_ONLINESTATUS,
			   print_pid_string_cb);

	if (!send_message(msg_ctx, pid, MSG_WINBIND_ONLINESTATUS, &myid,
			  sizeof(myid)))
		return False;

	wait_replies(msg_ctx, procid_to_pid(&pid) == 0);

	/* No replies were received within the timeout period */

	if (num_replies == 0)
		printf("No replies received\n");

	messaging_deregister(msg_ctx, MSG_WINBIND_ONLINESTATUS, NULL);

	return num_replies;
}

static bool do_dump_event_list(struct messaging_context *msg_ctx,
			       const struct server_id pid,
			       const int argc, const char **argv)
{
	struct server_id myid;

	myid = messaging_server_id(msg_ctx);

	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol <dest> dump-event-list\n");
		return False;
	}

	return send_message(msg_ctx, pid, MSG_DUMP_EVENT_LIST, NULL, 0);
}

static bool do_winbind_dump_domain_list(struct messaging_context *msg_ctx,
					const struct server_id pid,
					const int argc, const char **argv)
{
	const char *domain = NULL;
	int domain_len = 0;
	struct server_id myid;
	uint8_t *buf = NULL;
	int buf_len = 0;

	myid = messaging_server_id(msg_ctx);

	if (argc < 1 || argc > 2) {
		fprintf(stderr, "Usage: smbcontrol <dest> dump-domain-list "
			"<domain>\n");
		return false;
	}

	if (argc == 2) {
		domain = argv[1];
		domain_len = strlen(argv[1]) + 1;
	}

	messaging_register(msg_ctx, NULL, MSG_WINBIND_DUMP_DOMAIN_LIST,
			   print_pid_string_cb);

	buf_len = sizeof(myid)+domain_len;
	buf = SMB_MALLOC_ARRAY(uint8_t, buf_len);
	if (!buf) {
		return false;
	}

	memcpy(buf, &myid, sizeof(myid));
	memcpy(&buf[sizeof(myid)], domain, domain_len);

	if (!send_message(msg_ctx, pid, MSG_WINBIND_DUMP_DOMAIN_LIST,
			  buf, buf_len))
	{
		SAFE_FREE(buf);
		return false;
	}

	wait_replies(msg_ctx, procid_to_pid(&pid) == 0);

	/* No replies were received within the timeout period */

	SAFE_FREE(buf);
	if (num_replies == 0) {
		printf("No replies received\n");
	}

	messaging_deregister(msg_ctx, MSG_WINBIND_DUMP_DOMAIN_LIST, NULL);

	return num_replies;
}

static void winbind_validate_cache_cb(struct messaging_context *msg,
				      void *private_data,
				      uint32_t msg_type,
				      struct server_id pid,
				      DATA_BLOB *data)
{
	char *src_string = procid_str(NULL, &pid);
	printf("Winbindd cache is %svalid. (answer from pid %s)\n",
	       (*(data->data) == 0 ? "" : "NOT "), src_string);
	TALLOC_FREE(src_string);
	num_replies++;
}

static bool do_winbind_validate_cache(struct messaging_context *msg_ctx,
				      const struct server_id pid,
				      const int argc, const char **argv)
{
	struct server_id myid;

	myid = messaging_server_id(msg_ctx);

	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol winbindd validate-cache\n");
		return False;
	}

	messaging_register(msg_ctx, NULL, MSG_WINBIND_VALIDATE_CACHE,
			   winbind_validate_cache_cb);

	if (!send_message(msg_ctx, pid, MSG_WINBIND_VALIDATE_CACHE, &myid,
			  sizeof(myid))) {
		return False;
	}

	wait_replies(msg_ctx, procid_to_pid(&pid) == 0);

	if (num_replies == 0) {
		printf("No replies received\n");
	}

	messaging_deregister(msg_ctx, MSG_WINBIND_VALIDATE_CACHE, NULL);

	return num_replies;
}

static bool do_reload_config(struct messaging_context *msg_ctx,
			     const struct server_id pid,
			     const int argc, const char **argv)
{
	if (argc != 1) {
		fprintf(stderr, "Usage: smbcontrol <dest> reload-config\n");
		return False;
	}

	return send_message(msg_ctx, pid, MSG_SMB_CONF_UPDATED, NULL, 0);
}

static void my_make_nmb_name( struct nmb_name *n, const char *name, int type)
{
	fstring unix_name;
	memset( (char *)n, '\0', sizeof(struct nmb_name) );
	fstrcpy(unix_name, name);
	strupper_m(unix_name);
	push_ascii(n->name, unix_name, sizeof(n->name), STR_TERMINATE);
	n->name_type = (unsigned int)type & 0xFF;
	push_ascii(n->scope,  global_scope(), 64, STR_TERMINATE);
}

static bool do_nodestatus(struct messaging_context *msg_ctx,
			  const struct server_id pid,
			  const int argc, const char **argv)
{
	struct packet_struct p;

	if (argc != 2) {
		fprintf(stderr, "Usage: smbcontrol nmbd nodestatus <ip>\n");
		return False;
	}

	ZERO_STRUCT(p);

	p.ip = interpret_addr2(argv[1]);
	p.port = 137;
	p.packet_type = NMB_PACKET;

	p.packet.nmb.header.name_trn_id = 10;
	p.packet.nmb.header.opcode = 0;
	p.packet.nmb.header.response = False;
	p.packet.nmb.header.nm_flags.bcast = False;
	p.packet.nmb.header.nm_flags.recursion_available = False;
	p.packet.nmb.header.nm_flags.recursion_desired = False;
	p.packet.nmb.header.nm_flags.trunc = False;
	p.packet.nmb.header.nm_flags.authoritative = False;
	p.packet.nmb.header.rcode = 0;
	p.packet.nmb.header.qdcount = 1;
	p.packet.nmb.header.ancount = 0;
	p.packet.nmb.header.nscount = 0;
	p.packet.nmb.header.arcount = 0;
	my_make_nmb_name(&p.packet.nmb.question.question_name, "*", 0x00);
	p.packet.nmb.question.question_type = 0x21;
	p.packet.nmb.question.question_class = 0x1;

	return send_message(msg_ctx, pid, MSG_SEND_PACKET, &p, sizeof(p));
}

/* A list of message type supported */

static const struct {
	const char *name;	/* Option name */
	bool (*fn)(struct messaging_context *msg_ctx,
		   const struct server_id pid,
		   const int argc, const char **argv);
	const char *help;	/* Short help text */
} msg_types[] = {
	{ "debug", do_debug, "Set debuglevel"  },
	{ "idmap", do_idmap, "Manipulate idmap cache" },
	{ "force-election", do_election,
	  "Force a browse election" },
	{ "ping", do_ping, "Elicit a response" },
	{ "profile", do_profile, "" },
	{ "inject", do_inject_fault,
	    "Inject a fatal signal into a running smbd"},
	{ "stacktrace", do_daemon_stack_trace,
	    "Display a stack trace of a daemon" },
	{ "profilelevel", do_profilelevel, "" },
	{ "debuglevel", do_debuglevel, "Display current debuglevels" },
	{ "printnotify", do_printnotify, "Send a print notify message" },
	{ "close-share", do_closeshare, "Forcibly disconnect a share" },
	{ "ip-dropped", do_ip_dropped, "Tell winbind that an IP got dropped" },
	{ "lockretry", do_lockretry, "Force a blocking lock retry" },
	{ "brl-revalidate", do_brl_revalidate, "Revalidate all brl entries" },
        { "samsync", do_samsync, "Initiate SAM synchronisation" },
        { "samrepl", do_samrepl, "Initiate SAM replication" },
	{ "pool-usage", do_poolusage, "Display talloc memory usage" },
	{ "dmalloc-mark", do_dmalloc_mark, "" },
	{ "dmalloc-log-changed", do_dmalloc_changed, "" },
	{ "shutdown", do_shutdown, "Shut down daemon" },
	{ "drvupgrade", do_drvupgrade, "Notify a printer driver has changed" },
	{ "reload-config", do_reload_config, "Force smbd or winbindd to reload config file"},
	{ "nodestatus", do_nodestatus, "Ask nmbd to do a node status request"},
	{ "online", do_winbind_online, "Ask winbind to go into online state"},
	{ "offline", do_winbind_offline, "Ask winbind to go into offline state"},
	{ "onlinestatus", do_winbind_onlinestatus, "Request winbind online status"},
	{ "dump-event-list", do_dump_event_list, "Dump event list"},
	{ "validate-cache" , do_winbind_validate_cache,
	  "Validate winbind's credential cache" },
	{ "dump-domain-list", do_winbind_dump_domain_list, "Dump winbind domain list"},
	{ "noop", do_noop, "Do nothing" },
	{ NULL }
};

/* Display usage information */

static void usage(poptContext pc)
{
	int i;

	poptPrintHelp(pc, stderr, 0);

	fprintf(stderr, "\n");
	fprintf(stderr, "<destination> is one of \"nmbd\", \"smbd\", \"winbindd\" or a "
		"process ID\n");

	fprintf(stderr, "\n");
	fprintf(stderr, "<message-type> is one of:\n");

	for (i = 0; msg_types[i].name; i++) 
	    fprintf(stderr, "\t%-30s%s\n", msg_types[i].name, 
		    msg_types[i].help);

	fprintf(stderr, "\n");

	exit(1);
}

/* Return the pid number for a string destination */

static struct server_id parse_dest(struct messaging_context *msg,
				   const char *dest)
{
	struct server_id result = {-1};
	pid_t pid;

	/* Zero is a special return value for broadcast to all processes */

	if (strequal(dest, "all")) {
		return interpret_pid(MSG_BROADCAST_PID_STR);
	}

	/* Try self - useful for testing */

	if (strequal(dest, "self")) {
		return messaging_server_id(msg);
	}

	/* Fix winbind typo. */
	if (strequal(dest, "winbind")) {
		dest = "winbindd";
	}

	/* Check for numeric pid number */
	result = interpret_pid(dest);

	/* Zero isn't valid if not "all". */
	if (result.pid && procid_valid(&result)) {
		return result;
	}

	/* Look up other destinations in pidfile directory */

	if ((pid = pidfile_pid(dest)) != 0) {
		return pid_to_procid(pid);
	}

	fprintf(stderr,"Can't find pid for destination '%s'\n", dest);

	return result;
}

/* Execute smbcontrol command */

static bool do_command(struct messaging_context *msg_ctx,
		       int argc, const char **argv)
{
	const char *dest = argv[0], *command = argv[1];
	struct server_id pid;
	int i;

	/* Check destination */

	pid = parse_dest(msg_ctx, dest);
	if (!procid_valid(&pid)) {
		return False;
	}

	/* Check command */

	for (i = 0; msg_types[i].name; i++) {
		if (strequal(command, msg_types[i].name))
			return msg_types[i].fn(msg_ctx, pid,
					       argc - 1, argv + 1);
	}

	fprintf(stderr, "smbcontrol: unknown command '%s'\n", command);

	return False;
}

static void smbcontrol_help(poptContext pc,
		    enum poptCallbackReason preason,
		    struct poptOption * poption,
		    const char * parg,
		    void * pdata)
{
	if (poption->shortName != '?') {
		poptPrintUsage(pc, stdout, 0);
	} else {
		usage(pc);
	}

	exit(0);
}

struct poptOption help_options[] = {
	{ NULL, '\0', POPT_ARG_CALLBACK, (void *)&smbcontrol_help, '\0',
	  NULL, NULL },
	{ "help", '?', 0, NULL, '?', "Show this help message", NULL },
	{ "usage", '\0', 0, NULL, 'u', "Display brief usage message", NULL },
	{ NULL }
} ;

/* Main program */

int main(int argc, const char **argv)
{
	poptContext pc;
	int opt;
	struct tevent_context *evt_ctx;
	struct messaging_context *msg_ctx;

	static struct poptOption long_options[] = {
		/* POPT_AUTOHELP */
		{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, help_options,
		                        0, "Help options:", NULL },
		{ "timeout", 't', POPT_ARG_INT, &timeout, 't', 
		  "Set timeout value in seconds", "TIMEOUT" },

		POPT_COMMON_SAMBA
		POPT_TABLEEND
	};
	TALLOC_CTX *frame = talloc_stackframe();
	int ret = 0;

	load_case_tables();

	setup_logging(argv[0], DEBUG_STDOUT);

	/* Parse command line arguments using popt */

	pc = poptGetContext(
		"smbcontrol", argc, (const char **)argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "[OPTION...] <destination> <message-type> "
			       "<parameters>");

	if (argc == 1)
		usage(pc);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch(opt) {
		case 't':	/* --timeout */
			break;
		default:
			fprintf(stderr, "Invalid option\n");
			poptPrintHelp(pc, stderr, 0);
			break;
		}
	}

	/* We should now have the remaining command line arguments in
           argv.  The argc parameter should have been decremented to the
           correct value in the above switch statement. */

	argv = (const char **)poptGetArgs(pc);
	argc = 0;
	if (argv != NULL) {
		while (argv[argc] != NULL) {
			argc++;
		}
	}

	if (argc <= 1)
		usage(pc);

	lp_load(get_dyn_CONFIGFILE(),False,False,False,True);

	/* Need to invert sense of return code -- samba
         * routines mostly return True==1 for success, but
         * shell needs 0. */ 

	if (!(evt_ctx = tevent_context_init(NULL)) ||
	    !(msg_ctx = messaging_init(NULL, procid_self(), evt_ctx))) {
		fprintf(stderr, "could not init messaging context\n");
		TALLOC_FREE(frame);
		exit(1);
	}

	ret = !do_command(msg_ctx, argc, argv);
	TALLOC_FREE(frame);
	return ret;
}
