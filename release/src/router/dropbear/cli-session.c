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
#include "random.h"
#include "service.h"
#include "runopts.h"
#include "chansession.h"
#include "agentfwd.h"

static void cli_remoteclosed();
static void cli_sessionloop();
static void cli_session_init();
static void cli_finished();

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
#ifdef  ENABLE_CLI_REMOTETCPFWD
	{SSH_MSG_REQUEST_SUCCESS, cli_recv_msg_request_success}, /* client */
	{SSH_MSG_REQUEST_FAILURE, cli_recv_msg_request_failure}, /* client */
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

void cli_session(int sock_in, int sock_out) {

	seedrandom();

	crypto_init();

	common_session_init(sock_in, sock_out);

	chaninitialise(cli_chantypes);

	/* Set up cli_ses vars */
	cli_session_init();

	/* Ready to go */
	sessinitdone = 1;

	/* Exchange identification */
	session_identification();

	send_msg_kexinit();

	session_loop(cli_sessionloop);

	/* Not reached */

}

static void cli_session_init() {

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

	/* Auth */
	cli_ses.lastprivkey = NULL;
	cli_ses.lastauthtype = 0;

	/* For printing "remote host closed" for the user */
	ses.remoteclosed = cli_remoteclosed;
	ses.buf_match_algo = cli_buf_match_algo;

	/* packet handlers */
	ses.packettypes = cli_packettypes;

	ses.isserver = 0;
}

/* This function drives the progress of the session - it initiates KEX,
 * service, userauth and channel requests */
static void cli_sessionloop() {

	TRACE(("enter cli_sessionloop"))

	if (ses.lastpacket == SSH_MSG_KEXINIT && cli_ses.kex_state == KEX_NOTHING) {
		cli_ses.kex_state = KEXINIT_RCVD;
	}

	if (cli_ses.kex_state == KEXINIT_RCVD) {

		/* We initiate the KEXDH. If DH wasn't the correct type, the KEXINIT
		 * negotiation would have failed. */
		send_msg_kexdh_init();
		cli_ses.kex_state = KEXDH_INIT_SENT;
		TRACE(("leave cli_sessionloop: done with KEXINIT_RCVD"))
		return;
	}

	/* A KEX has finished, so we should go back to our KEX_NOTHING state */
	if (cli_ses.kex_state != KEX_NOTHING && ses.kexstate.recvkexinit == 0
			&& ses.kexstate.sentkexinit == 0) {
		cli_ses.kex_state = KEX_NOTHING;
	}

	/* We shouldn't do anything else if a KEX is in progress */
	if (cli_ses.kex_state != KEX_NOTHING) {
		TRACE(("leave cli_sessionloop: kex_state != KEX_NOTHING"))
		return;
	}

	/* We should exit if we haven't donefirstkex: we shouldn't reach here
	 * in normal operation */
	if (ses.kexstate.donefirstkex == 0) {
		TRACE(("XXX XXX might be bad! leave cli_sessionloop: haven't donefirstkex"))
		return;
	}

	switch (cli_ses.state) {

		case STATE_NOTHING:
			/* We've got the transport layer sorted, we now need to request
			 * userauth */
			send_msg_service_request(SSH_SERVICE_USERAUTH);
			cli_ses.state = SERVICE_AUTH_REQ_SENT;
			TRACE(("leave cli_sessionloop: sent userauth service req"))
			return;

		/* userauth code */
		case SERVICE_AUTH_ACCEPT_RCVD:
			cli_auth_getmethods();
			cli_ses.state = USERAUTH_REQ_SENT;
			TRACE(("leave cli_sessionloop: sent userauth methods req"))
			return;
			
		case USERAUTH_FAIL_RCVD:
			cli_auth_try();
			cli_ses.state = USERAUTH_REQ_SENT;
			TRACE(("leave cli_sessionloop: cli_auth_try"))
			return;

		case USERAUTH_SUCCESS_RCVD:

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
			
#ifdef ENABLE_CLI_LOCALTCPFWD
			setup_localtcp();
#endif
#ifdef ENABLE_CLI_REMOTETCPFWD
			setup_remotetcp();
#endif

#ifdef ENABLE_CLI_NETCAT
			if (cli_opts.netcat_host) {
				cli_send_netcat_request();
			} else 
#endif
			if (!cli_opts.no_cmd) {
				cli_send_chansess_request();
			}
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

	TRACE(("leave cli_sessionloop: fell out"))

}

void cli_session_cleanup() {

	if (!sessinitdone) {
		return;
	}

	/* Set std{in,out,err} back to non-blocking - busybox ash dies nastily if
	 * we don't revert the flags */
	fcntl(cli_ses.stdincopy, F_SETFL, cli_ses.stdinflags);
	fcntl(cli_ses.stdoutcopy, F_SETFL, cli_ses.stdoutflags);
	fcntl(cli_ses.stderrcopy, F_SETFL, cli_ses.stderrflags);

	cli_tty_cleanup();

}

static void cli_finished() {

	cli_session_cleanup();
	common_session_cleanup();
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
void cleantext(unsigned char* dirtytext) {

	unsigned int i, j;
	unsigned char c;

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
