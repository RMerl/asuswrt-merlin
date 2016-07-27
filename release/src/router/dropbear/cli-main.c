/*
 * Dropbear - a SSH2 server
 * SSH client implementation
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * Copyright (c) 2004 by Mihnea Stoenescu
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#include "includes.h"
#include "dbutil.h"
#include "runopts.h"
#include "session.h"
#include "dbrandom.h"
#include "crypto_desc.h"
#include "netio.h"

static void cli_dropbear_exit(int exitcode, const char* format, va_list param) ATTRIB_NORETURN;
static void cli_dropbear_log(int priority, const char* format, va_list param);

#ifdef ENABLE_CLI_PROXYCMD
static void cli_proxy_cmd(int *sock_in, int *sock_out, pid_t *pid_out);
static void kill_proxy_sighandler(int signo);
#endif

#if defined(DBMULTI_dbclient) || !defined(DROPBEAR_MULTI)
#if defined(DBMULTI_dbclient) && defined(DROPBEAR_MULTI)
int cli_main(int argc, char ** argv) {
#else
int main(int argc, char ** argv) {
#endif

	int sock_in, sock_out;
	struct dropbear_progress_connection *progress = NULL;

	_dropbear_exit = cli_dropbear_exit;
	_dropbear_log = cli_dropbear_log;

	disallow_core();

	seedrandom();
	crypto_init();

	cli_getopts(argc, argv);

#ifndef DISABLE_SYSLOG
	if (opts.usingsyslog) {
		startsyslog("dbclient");
	}
#endif

	TRACE(("user='%s' host='%s' port='%s'", cli_opts.username,
				cli_opts.remotehost, cli_opts.remoteport))

	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		dropbear_exit("signal() error");
	}

	pid_t proxy_cmd_pid = 0;
#ifdef ENABLE_CLI_PROXYCMD
	if (cli_opts.proxycmd) {
		cli_proxy_cmd(&sock_in, &sock_out, &proxy_cmd_pid);
		m_free(cli_opts.proxycmd);
		if (signal(SIGINT, kill_proxy_sighandler) == SIG_ERR ||
			signal(SIGTERM, kill_proxy_sighandler) == SIG_ERR ||
			signal(SIGHUP, kill_proxy_sighandler) == SIG_ERR) {
			dropbear_exit("signal() error");
		}
	} else
#endif
	{
		progress = connect_remote(cli_opts.remotehost, cli_opts.remoteport, cli_connected, &ses);
		sock_in = sock_out = -1;
	}

	cli_session(sock_in, sock_out, progress, proxy_cmd_pid);

	/* not reached */
	return -1;
}
#endif /* DBMULTI stuff */

static void cli_dropbear_exit(int exitcode, const char* format, va_list param) {
	char exitmsg[150];
	char fullmsg[300];

	/* Note that exit message must be rendered before session cleanup */

	/* Render the formatted exit message */
	vsnprintf(exitmsg, sizeof(exitmsg), format, param);

	/* Add the prefix depending on session/auth state */
	if (!sessinitdone) {
		snprintf(fullmsg, sizeof(fullmsg), "Exited: %s", exitmsg);
	} else {
		snprintf(fullmsg, sizeof(fullmsg), 
				"Connection to %s@%s:%s exited: %s", 
				cli_opts.username, cli_opts.remotehost, 
				cli_opts.remoteport, exitmsg);
	}

	/* Do the cleanup first, since then the terminal will be reset */
	session_cleanup();
	/* Avoid printing onwards from terminal cruft */
	fprintf(stderr, "\n");

	dropbear_log(LOG_INFO, "%s", fullmsg);
	exit(exitcode);
}

static void cli_dropbear_log(int priority,
		const char* format, va_list param) {

	char printbuf[1024];

	vsnprintf(printbuf, sizeof(printbuf), format, param);

#ifndef DISABLE_SYSLOG
	if (opts.usingsyslog) {
		syslog(priority, "%s", printbuf);
	}
#endif

	fprintf(stderr, "%s: %s\n", cli_opts.progname, printbuf);
	fflush(stderr);
}

static void exec_proxy_cmd(void *user_data_cmd) {
	const char *cmd = user_data_cmd;
	char *usershell;

	usershell = m_strdup(get_user_shell());
	run_shell_command(cmd, ses.maxfd, usershell);
	dropbear_exit("Failed to run '%s'\n", cmd);
}

#ifdef ENABLE_CLI_PROXYCMD
static void cli_proxy_cmd(int *sock_in, int *sock_out, pid_t *pid_out) {
	char * ex_cmd = NULL;
	size_t ex_cmdlen;
	int ret;

	fill_passwd(cli_opts.own_user);

	ex_cmdlen = strlen(cli_opts.proxycmd) + 6; /* "exec " + command + '\0' */
	ex_cmd = m_malloc(ex_cmdlen);
	snprintf(ex_cmd, ex_cmdlen, "exec %s", cli_opts.proxycmd);

	ret = spawn_command(exec_proxy_cmd, ex_cmd,
			sock_out, sock_in, NULL, pid_out);
	m_free(ex_cmd);
	if (ret == DROPBEAR_FAILURE) {
		dropbear_exit("Failed running proxy command");
		*sock_in = *sock_out = -1;
	}
}

static void kill_proxy_sighandler(int UNUSED(signo)) {
	kill_proxy_command();
	_exit(1);
}
#endif /* ENABLE_CLI_PROXYCMD */
