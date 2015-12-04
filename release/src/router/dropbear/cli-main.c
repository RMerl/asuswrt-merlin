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

static void cli_dropbear_exit(int exitcode, const char* format, va_list param) ATTRIB_NORETURN;
static void cli_dropbear_log(int priority, const char* format, va_list param);

#ifdef ENABLE_CLI_PROXYCMD
static void cli_proxy_cmd(int *sock_in, int *sock_out);
#endif

#if defined(DBMULTI_dbclient) || !defined(DROPBEAR_MULTI)
#if defined(DBMULTI_dbclient) && defined(DROPBEAR_MULTI)
int cli_main(int argc, char ** argv) {
#else
int main(int argc, char ** argv) {
#endif

	int sock_in, sock_out;
	char* error = NULL;

	_dropbear_exit = cli_dropbear_exit;
	_dropbear_log = cli_dropbear_log;

	disallow_core();

	seedrandom();
	crypto_init();

	cli_getopts(argc, argv);

	TRACE(("user='%s' host='%s' port='%s'", cli_opts.username,
				cli_opts.remotehost, cli_opts.remoteport))

	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		dropbear_exit("signal() error");
	}

#ifdef ENABLE_CLI_PROXYCMD
	if (cli_opts.proxycmd) {
		cli_proxy_cmd(&sock_in, &sock_out);
		m_free(cli_opts.proxycmd);
	} else
#endif
	{
		int sock = connect_remote(cli_opts.remotehost, cli_opts.remoteport, 
				0, &error);
		sock_in = sock_out = sock;
	}

	if (sock_in < 0) {
		dropbear_exit("%s", error);
	}

	cli_session(sock_in, sock_out);

	/* not reached */
	return -1;
}
#endif /* DBMULTI stuff */

static void cli_dropbear_exit(int exitcode, const char* format, va_list param) {

	char fmtbuf[300];

	if (!sessinitdone) {
		snprintf(fmtbuf, sizeof(fmtbuf), "Exited: %s",
				format);
	} else {
		snprintf(fmtbuf, sizeof(fmtbuf), 
				"Connection to %s@%s:%s exited: %s", 
				cli_opts.username, cli_opts.remotehost, 
				cli_opts.remoteport, format);
	}

	/* Do the cleanup first, since then the terminal will be reset */
	session_cleanup();
	/* Avoid printing onwards from terminal cruft */
	fprintf(stderr, "\n");

	_dropbear_log(LOG_INFO, fmtbuf, param);
	exit(exitcode);
}

static void cli_dropbear_log(int UNUSED(priority), 
		const char* format, va_list param) {

	char printbuf[1024];

	vsnprintf(printbuf, sizeof(printbuf), format, param);

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
static void cli_proxy_cmd(int *sock_in, int *sock_out) {
	int ret;

	fill_passwd(cli_opts.own_user);

	ret = spawn_command(exec_proxy_cmd, cli_opts.proxycmd,
			sock_out, sock_in, NULL, NULL);
	if (ret == DROPBEAR_FAILURE) {
		dropbear_exit("Failed running proxy command");
		*sock_in = *sock_out = -1;
	}
}
#endif /* ENABLE_CLI_PROXYCMD */
