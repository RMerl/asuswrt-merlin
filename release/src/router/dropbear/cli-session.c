/*
 * Dropbear SSH
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
#include "session.h"
#include "dbutil.h"
#include "kex.h"
#include "ssh.h"
#include "packet.h"
#include "tcpfwd.h"
#include "channel.h"
#include "dbrandom.h"
#include "service.h"
#include "runopts.h"
#include "chansession.h"
#include "agentfwd.h"
#include "crypto_desc.h"
#include "netio.h"

static void cli_remoteclosed(void) ATTRIB_NORETURN;
static void cli_sessionloop(void);
static void cli_session_init(pid_t proxy_cmd_pid);
static void cli_finished(void) ATTRIB_NORETURN;
static void recv_msg_service_accept(void);
static void cli_session_cleanup(void);
static void recv_msg_global_request_cli(void);

struct clientsession cli_ses; /* GLOBAL */

/* Sorted in decreasing frequency will be more efficient - data and window
 * should be first */
static const packettype cli_packettypes[] = {
	/* TYPE, FUNCTION */
	{SSH_MSG_CHANNEL_DATA, recv_msg_channel_data},
	{SSH_MSG_CHANNEL_EXTENDED_DATA, recv_msg_channel_extended_data},
	{SSH_MSG_CHANNEL_WINDOW_ADJUST, recv_msg_channel_window_adjust},
	{SSH_MSG_USERAUTH_FAILURE, recv_msg_userauth_failure}, /* client */
	{SSH_MSG_USERAUTH_SUCCESS, recv_msg_userauth_success}, /* client */
	{SSH_MSG_KEXINIT, recv_msg_kexinit},
	{SSH_MSG_KEXDH_REPLY, recv_msg_kexdh_reply}, /* client */
	{SSH_MSG_NEWKEYS, recv_msg_newkeys},
	{SSH_MSG_SERVICE_ACCEPT, recv_msg_service_accept}, /* client */
	{SSH_MSG_CHANNEL_REQUEST, recv_msg_channel_request},
	{SSH_MSG_CHANNEL_OPEN, recv_msg_channel_open},
	{SSH_MSG_CHANNEL_EOF, recv_msg_channel_eof},
	{SSH_MSG_CHANNEL_CLOSE, recv_msg_channel_close},
	{SSH_MSG_CHANNEL_OPEN_CONFIRMATION, recv_msg_channel_open_confirmation},
	{SSH_MSG_CHANNEL_OPEN_FAILURE, recv_msg_channel_open_failure},
	{SSH_MSG_USERAUTH_BANNER, recv_msg_userauth_banner}, /* client */
	{SSH_MSG_USERAUTH_SPECIFIC_60, recv_msg_userauth_specific_60}, /* client */
	{SSH_MSG_GLOBAL_REQUEST, recv_msg_global_request_cli},
	{SSH_MSG_CHANNEL_SUCCESS, ignore_recv_response},
	{SSH_MSG_CHANNEL_FAILURE, ignore_recv_response},
#ifdef  ENABLE_CLI_REMOTETCPFWD
	{SSH_MSG_REQUEST_SUCCESS, cli_recv_msg_request_success}, /* client */
	{SSH_MSG_REQUEST_FAILURE, cli_recv_msg_request_failure}, /* client */
#else
	/* For keepalive */
	{SSH_MSG_REQUEST_SUCCESS, ignore_recv_response},
	{SSH_MSG_REQUEST_FAILURE, ignore_recv_response},
#endif
	{0, 0} /* End */
};

static const struct ChanType *cli_chantypes[] = {
#ifdef ENABLE_CLI_REMOTETCPFWD
	&cli_chan_tcpremote,
#endif
#ifdef ENABLE_CLI_AGENTFWD
	&cli_chan_agent,
#endif
	NULL /* Null termination */
};

void cli_connected(int result, int sock, void* userdata, const char *errstring)
{
	struct sshsession *myses = userdata;
	if (result == DROPBEAR_FAILURE) {
		dropbear_exit("Connect failed: %s", errstring);
	}
	myses->sock_in = myses->sock_out = sock;
	update_channel_prio();
}

void cli_session(int sock_in, int sock_out, struct dropbear_progress_connection *progress, pid_t proxy_cmd_pid) {

	common_session_init(sock_in, sock_out);

	if (progress) {
		connect_set_writequeue(progress, &ses.writequeue);
	}

	chaninitialise(cli_chantypes);

	/* Set up cli_ses vars */
	cli_session_init(proxy_cmd_pid);

	/* Ready to go */
	sessinitdone = 1;

	/* Exchange identification */
	send_session_identification();

	kexfirstinitialise(); /* initialise the kex state */

	send_msg_kexinit();

	session_loop(cli_sessionloop);

	/* Not reached */

}

#ifdef USE_KEX_FIRST_FOLLOWS
static void cli_send_kex_first_guess() {
	send_msg_kexdh_init();
}
#endif

static void cli_session_init(pid_t proxy_cmd_pid) {

	cli_ses.state = STATE_NOTHING;
	cli_ses.kex_state = KEX_NOTHING;

	cli_ses.tty_raw_mode = 0;
	cli_ses.winchange = 0;

	/* We store std{in,out,err}'s flags, so we can set them back on exit
	 * (otherwise busybox's ash isn't happy */
	cli_ses.stdincopy = dup(STDIN_FILENO);
	cli_ses.stdinflags = fcntl(STDIN_FILENO, F_GETFL, 0);
	cli_ses.stdoutcopy = dup(STDOUT_FILENO);
	cli_ses.stdoutflags = fcntl(STDOUT_FILENO, F_GETFL, 0);
	cli_ses.stderrcopy = dup(STDERR_FILENO);
	cli_ses.stderrflags = fcntl(STDERR_FILENO, F_GETFL, 0);

	cli_ses.retval = EXIT_SUCCESS; /* Assume it's clean if we don't get a
									  specific exit status */
	cli_ses.proxy_cmd_pid = proxy_cmd_pid;
	TRACE(("proxy command PID='%d'", proxy_cmd_pid));

	/* Auth */
	cli_ses.lastprivkey = NULL;
	cli_ses.lastauthtype = 0;

#ifdef DROPBEAR_NONE_CIPHER
	cli_ses.cipher_none_after_auth = get_algo_usable(sshciphers, "none");
	set_algo_usable(sshciphers, "none", 0);
#else
	cli_ses.cipher_none_after_auth = 0;
#endif

	/* For printing "remote host closed" for the user */
	ses.remoteclosed = cli_remoteclosed;

	ses.extra_session_cleanup = cli_session_cleanup;

	/* packet handlers */
	ses.packettypes = cli_packettypes;

	ses.isserver = 0;

#ifdef USE_KEX_FIRST_FOLLOWS
	ses.send_kex_first_guess = cli_send_kex_first_guess;
#endif

}

static void send_msg_service_request(char* servicename) {

	TRACE(("enter send_msg_service_request: servicename='%s'", servicename))

	CHECKCLEARTOWRITE();

	buf_putbyte(ses.writepayload, SSH_MSG_SERVICE_REQUEST);
	buf_putstring(ses.writepayload, servicename, strlen(servicename));

	encrypt_packet();
	TRACE(("leave send_msg_service_request"))
}

static void recv_msg_service_accept(void) {
	/* do nothing, if it failed then the server MUST have disconnected */
}

/* This function drives the progress of the session - it initiates KEX,
 * service, userauth and channel requests */
static void cli_sessionloop() {

	TRACE2(("enter cli_sessionloop"))

	if (ses.lastpacket == 0) {
		TRACE2(("exit cli_sessionloop: no real packets yet"))
		return;
	}

	if (ses.lastpacket == SSH_MSG_KEXINIT && cli_ses.kex_state == KEX_NOTHING) {
		/* We initiate the KEXDH. If DH wasn't the correct type, the KEXINIT
		 * negotiation would have failed. */
		if (!ses.kexstate.our_first_follows_matches) {
			send_msg_kexdh_init();
		}
		cli_ses.kex_state = KEXDH_INIT_SENT;			
		TRACE(("leave cli_sessionloop: done with KEXINIT_RCVD"))
		return;
	}

	/* A KEX has finished, so we should go back to our KEX_NOTHING state */
	if (cli_ses.kex_state != KEX_NOTHING && ses.kexstate.sentnewkeys) {
		cli_ses.kex_state = KEX_NOTHING;
	}

	/* We shouldn't do anything else if a KEX is in progress */
	if (cli_ses.kex_state != KEX_NOTHING) {
		TRACE(("leave cli_sessionloop: kex_state != KEX_NOTHING"))
		return;
	}

	if (ses.kexstate.donefirstkex == 0) {
		/* We might reach here if we have partial packet reads or have
		 * received SSG_MSG_IGNORE etc. Just skip it */
		TRACE2(("donefirstkex false\n"))
		return;
	}

	switch (cli_ses.state) {

		case STATE_NOTHING:
			/* We've got the transport layer sorted, we now need to request
			 * userauth */
			send_msg_service_request(SSH_SERVICE_USERAUTH);
			cli_auth_getmethods();
			cli_ses.state = USERAUTH_REQ_SENT;
			TRACE(("leave cli_sessionloop: sent userauth methods req"))
			return;

		case USERAUTH_REQ_SENT:
			TRACE(("leave cli_sessionloop: waiting, req_sent"))
			return;
			
		case USERAUTH_FAIL_RCVD:
			if (cli_auth_try() == DROPBEAR_FAILURE) {
				dropbear_exit("No auth methods could be used.");
			}
			cli_ses.state = USERAUTH_REQ_SENT;
			TRACE(("leave cli_sessionloop: cli_auth_try"))
			return;

		case USERAUTH_SUCCESS_RCVD:
#ifndef DISABLE_SYSLOG
			if (opts.usingsyslog) {
				dropbear_log(LOG_INFO, "Authentication succeeded.");
			}
#endif

#ifdef DROPBEAR_NONE_CIPHER
			if (cli_ses.cipher_none_after_auth)
			{
				set_algo_usable(sshciphers, "none", 1);
				send_msg_kexinit();
			}
#endif

			if (cli_opts.backgrounded) {
				int devnull;
				/* keeping stdin open steals input from the terminal and
				   is confusing, though stdout/stderr could be useful. */
				devnull = open(_PATH_DEVNULL, O_RDONLY);
				if (devnull < 0) {
					dropbear_exit("Opening /dev/null: %d %s",
							errno, strerror(errno));
				}
				dup2(devnull, STDIN_FILENO);
				if (daemon(0, 1) < 0) {
					dropbear_exit("Backgrounding failed: %d %s", 
							errno, strerror(errno));
				}
			}
			
#ifdef ENABLE_CLI_NETCAT
			if (cli_opts.netcat_host) {
				cli_send_netcat_request();
			} else 
#endif
			if (!cli_opts.no_cmd) {
				cli_send_chansess_request();
			}

#ifdef ENABLE_CLI_LOCALTCPFWD
			setup_localtcp();
#endif
#ifdef ENABLE_CLI_REMOTETCPFWD
			setup_remotetcp();
#endif

			TRACE(("leave cli_sessionloop: running"))
			cli_ses.state = SESSION_RUNNING;
			return;

		case SESSION_RUNNING:
			if (ses.chancount < 1 && !cli_opts.no_cmd) {
				cli_finished();
			}

			if (cli_ses.winchange) {
				cli_chansess_winchange();
			}
			return;

		/* XXX more here needed */


	default:
		break;
	}

	TRACE2(("leave cli_sessionloop: fell out"))

}

void kill_proxy_command(void) {
	/*
	 * Send SIGHUP to proxy command if used. We don't wait() in
	 * case it hangs and instead rely on init to reap the child
	 */
	if (cli_ses.proxy_cmd_pid > 1) {
		TRACE(("killing proxy command with PID='%d'", cli_ses.proxy_cmd_pid));
		kill(cli_ses.proxy_cmd_pid, SIGHUP);
	}
}

static void cli_session_cleanup(void) {

	if (!sessinitdone) {
		return;
	}

	kill_proxy_command();

	/* Set std{in,out,err} back to non-blocking - busybox ash dies nastily if
	 * we don't revert the flags */
	/* Ignore return value since there's nothing we can do */
	(void)fcntl(cli_ses.stdincopy, F_SETFL, cli_ses.stdinflags);
	(void)fcntl(cli_ses.stdoutcopy, F_SETFL, cli_ses.stdoutflags);
	(void)fcntl(cli_ses.stderrcopy, F_SETFL, cli_ses.stderrflags);

	cli_tty_cleanup();

}

static void cli_finished() {

	session_cleanup();
	fprintf(stderr, "Connection to %s@%s:%s closed.\n", cli_opts.username,
			cli_opts.remotehost, cli_opts.remoteport);
	exit(cli_ses.retval);
}


/* called when the remote side closes the connection */
static void cli_remoteclosed() {

	/* XXX TODO perhaps print a friendlier message if we get this but have
	 * already sent/received disconnect message(s) ??? */
	m_close(ses.sock_in);
	m_close(ses.sock_out);
	ses.sock_in = -1;
	ses.sock_out = -1;
	dropbear_exit("Remote closed the connection");
}

/* Operates in-place turning dirty (untrusted potentially containing control
 * characters) text into clean text. 
 * Note: this is safe only with ascii - other charsets could have problems. */
void cleantext(char* dirtytext) {

	unsigned int i, j;
	char c;

	j = 0;
	for (i = 0; dirtytext[i] != '\0'; i++) {

		c = dirtytext[i];
		/* We can ignore '\r's */
		if ( (c >= ' ' && c <= '~') || c == '\n' || c == '\t') {
			dirtytext[j] = c;
			j++;
		}
	}
	/* Null terminate */
	dirtytext[j] = '\0';
}

static void recv_msg_global_request_cli(void) {
	TRACE(("recv_msg_global_request_cli"))
	/* Send a proper rejection */
	send_msg_request_failure();
}
