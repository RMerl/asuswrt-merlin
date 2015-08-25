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
#include "packet.h"
#include "buffer.h"
#include "session.h"
#include "dbutil.h"
#include "channel.h"
#include "ssh.h"
#include "runopts.h"
#include "termcodes.h"
#include "chansession.h"
#include "agentfwd.h"

static void cli_closechansess(struct Channel *channel);
static int cli_initchansess(struct Channel *channel);
static void cli_chansessreq(struct Channel *channel);
static void send_chansess_pty_req(struct Channel *channel);
static void send_chansess_shell_req(struct Channel *channel);
static void cli_escape_handler(struct Channel *channel, unsigned char* buf, int *len);
static int cli_init_netcat(struct Channel *channel);

static void cli_tty_setup();

const struct ChanType clichansess = {
	0, /* sepfds */
	"session", /* name */
	cli_initchansess, /* inithandler */
	NULL, /* checkclosehandler */
	cli_chansessreq, /* reqhandler */
	cli_closechansess, /* closehandler */
};

static void cli_chansessreq(struct Channel *channel) {

	char* type = NULL;
	int wantreply;

	TRACE(("enter cli_chansessreq"))

	type = buf_getstring(ses.payload, NULL);
	wantreply = buf_getbool(ses.payload);

	if (strcmp(type, "exit-status") == 0) {
		cli_ses.retval = buf_getint(ses.payload);
		TRACE(("got exit-status of '%d'", cli_ses.retval))
	} else if (strcmp(type, "exit-signal") == 0) {
		TRACE(("got exit-signal, ignoring it"))
	} else {
		TRACE(("unknown request '%s'", type))
		if (wantreply) {
			send_msg_channel_failure(channel);
		}
		goto out;
	}
		
out:
	m_free(type);
}
	

/* If the main session goes, we close it up */
static void cli_closechansess(struct Channel *UNUSED(channel)) {
	cli_tty_cleanup(); /* Restore tty modes etc */

	/* This channel hasn't gone yet, so we have > 1 */
	if (ses.chancount > 1) {
		dropbear_log(LOG_INFO, "Waiting for other channels to close...");
	}
}

/* Taken from OpenSSH's sshtty.c:
 * RCSID("OpenBSD: sshtty.c,v 1.5 2003/09/19 17:43:35 markus Exp "); */
static void cli_tty_setup() {

	struct termios tio;

	TRACE(("enter cli_pty_setup"))

	if (cli_ses.tty_raw_mode == 1) {
		TRACE(("leave cli_tty_setup: already in raw mode!"))
		return;
	}

	if (tcgetattr(STDIN_FILENO, &tio) == -1) {
		dropbear_exit("Failed to set raw TTY mode");
	}

	/* make a copy */
	cli_ses.saved_tio = tio;

	tio.c_iflag |= IGNPAR;
	tio.c_iflag &= ~(ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXANY | IXOFF);
#ifdef IUCLC
	tio.c_iflag &= ~IUCLC;
#endif
	tio.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL);
#ifdef IEXTEN
	tio.c_lflag &= ~IEXTEN;
#endif
	tio.c_oflag &= ~OPOST;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSADRAIN, &tio) == -1) {
		dropbear_exit("Failed to set raw TTY mode");
	}

	cli_ses.tty_raw_mode = 1;
	TRACE(("leave cli_tty_setup"))
}

void cli_tty_cleanup() {

	TRACE(("enter cli_tty_cleanup"))

	if (cli_ses.tty_raw_mode == 0) {
		TRACE(("leave cli_tty_cleanup: not in raw mode"))
		return;
	}

	if (tcsetattr(STDIN_FILENO, TCSADRAIN, &cli_ses.saved_tio) == -1) {
		dropbear_log(LOG_WARNING, "Failed restoring TTY");
	} else {
		cli_ses.tty_raw_mode = 0; 
	}

	TRACE(("leave cli_tty_cleanup"))
}

static void put_termcodes() {

	struct termios tio;
	unsigned int sshcode;
	const struct TermCode *termcode;
	unsigned int value;
	unsigned int mapcode;

	unsigned int bufpos1, bufpos2;

	TRACE(("enter put_termcodes"))

	if (tcgetattr(STDIN_FILENO, &tio) == -1) {
		dropbear_log(LOG_WARNING, "Failed reading termmodes");
		buf_putint(ses.writepayload, 1); /* Just the terminator */
		buf_putbyte(ses.writepayload, 0); /* TTY_OP_END */
		return;
	}

	bufpos1 = ses.writepayload->pos;
	buf_putint(ses.writepayload, 0); /* A placeholder for the final length */

	/* As with Dropbear server, we ignore baud rates for now */
	for (sshcode = 1; sshcode < MAX_TERMCODE; sshcode++) {

		termcode = &termcodes[sshcode];
		mapcode = termcode->mapcode;

		switch (termcode->type) {

			case TERMCODE_NONE:
				continue;

			case TERMCODE_CONTROLCHAR:
				value = tio.c_cc[mapcode];
				break;

			case TERMCODE_INPUT:
				value = tio.c_iflag & mapcode;
				break;

			case TERMCODE_OUTPUT:
				value = tio.c_oflag & mapcode;
				break;

			case TERMCODE_LOCAL:
				value = tio.c_lflag & mapcode;
				break;

			case TERMCODE_CONTROL:
				value = tio.c_cflag & mapcode;
				break;

			default:
				continue;

		}

		/* If we reach here, we have something to say */
		buf_putbyte(ses.writepayload, sshcode);
		buf_putint(ses.writepayload, value);
	}

	buf_putbyte(ses.writepayload, 0); /* THE END, aka TTY_OP_END */

	/* Put the string length at the start of the buffer */
	bufpos2 = ses.writepayload->pos;

	buf_setpos(ses.writepayload, bufpos1); /* Jump back */
	buf_putint(ses.writepayload, bufpos2 - bufpos1 - 4); /* len(termcodes) */
	buf_setpos(ses.writepayload, bufpos2); /* Back where we were */

	TRACE(("leave put_termcodes"))
}

static void put_winsize() {

	struct winsize ws;

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) < 0) {
		/* Some sane defaults */
		ws.ws_row = 25;
		ws.ws_col = 80;
		ws.ws_xpixel = 0;
		ws.ws_ypixel = 0;
	}

	buf_putint(ses.writepayload, ws.ws_col); /* Cols */
	buf_putint(ses.writepayload, ws.ws_row); /* Rows */
	buf_putint(ses.writepayload, ws.ws_xpixel); /* Width */
	buf_putint(ses.writepayload, ws.ws_ypixel); /* Height */

}

static void sigwinch_handler(int UNUSED(unused)) {

	cli_ses.winchange = 1;

}

void cli_chansess_winchange() {

	unsigned int i;
	struct Channel *channel = NULL;

	for (i = 0; i < ses.chansize; i++) {
		channel = ses.channels[i];
		if (channel != NULL && channel->type == &clichansess) {
			CHECKCLEARTOWRITE();
			buf_putbyte(ses.writepayload, SSH_MSG_CHANNEL_REQUEST);
			buf_putint(ses.writepayload, channel->remotechan);
			buf_putstring(ses.writepayload, "window-change", 13);
			buf_putbyte(ses.writepayload, 0); /* FALSE says the spec */
			put_winsize();
			encrypt_packet();
		}
	}
	cli_ses.winchange = 0;
}

static void send_chansess_pty_req(struct Channel *channel) {

	char* term = NULL;

	TRACE(("enter send_chansess_pty_req"))

	start_send_channel_request(channel, "pty-req");

	/* Don't want replies */
	buf_putbyte(ses.writepayload, 0);

	/* Get the terminal */
	term = getenv("TERM");
	if (term == NULL) {
		term = "vt100"; /* Seems a safe default */
	}
	buf_putstring(ses.writepayload, term, strlen(term));

	/* Window size */
	put_winsize();

	/* Terminal mode encoding */
	put_termcodes();

	encrypt_packet();

	/* Set up a window-change handler */
	if (signal(SIGWINCH, sigwinch_handler) == SIG_ERR) {
		dropbear_exit("Signal error");
	}
	TRACE(("leave send_chansess_pty_req"))
}

static void send_chansess_shell_req(struct Channel *channel) {

	char* reqtype = NULL;

	TRACE(("enter send_chansess_shell_req"))

	if (cli_opts.cmd) {
		if (cli_opts.is_subsystem) {
			reqtype = "subsystem";
		} else {
			reqtype = "exec";
		}
	} else {
		reqtype = "shell";
	}

	start_send_channel_request(channel, reqtype);

	/* XXX TODO */
	buf_putbyte(ses.writepayload, 0); /* Don't want replies */
	if (cli_opts.cmd) {
		buf_putstring(ses.writepayload, cli_opts.cmd, strlen(cli_opts.cmd));
	}

	encrypt_packet();
	TRACE(("leave send_chansess_shell_req"))
}

/* Shared for normal client channel and netcat-alike */
static int cli_init_stdpipe_sess(struct Channel *channel) {
	channel->writefd = STDOUT_FILENO;
	setnonblocking(STDOUT_FILENO);

	channel->readfd = STDIN_FILENO;
	setnonblocking(STDIN_FILENO);

	channel->errfd = STDERR_FILENO;
	setnonblocking(STDERR_FILENO);

	channel->extrabuf = cbuf_new(opts.recv_window);
	return 0;
}

static int cli_init_netcat(struct Channel *channel) {
	channel->prio = DROPBEAR_CHANNEL_PRIO_UNKNOWABLE;
	return cli_init_stdpipe_sess(channel);
}

static int cli_initchansess(struct Channel *channel) {

	cli_init_stdpipe_sess(channel);

#ifdef ENABLE_CLI_AGENTFWD
	if (cli_opts.agent_fwd) {
		cli_setup_agent(channel);
	}
#endif

	if (cli_opts.wantpty) {
		send_chansess_pty_req(channel);
		channel->prio = DROPBEAR_CHANNEL_PRIO_INTERACTIVE;
	} else {
		channel->prio = DROPBEAR_CHANNEL_PRIO_BULK;
	}

	send_chansess_shell_req(channel);

	if (cli_opts.wantpty) {
		cli_tty_setup();
		channel->read_mangler = cli_escape_handler;
		cli_ses.last_char = '\r';
	}	

	return 0; /* Success */
}

#ifdef ENABLE_CLI_NETCAT

static const struct ChanType cli_chan_netcat = {
	0, /* sepfds */
	"direct-tcpip",
	cli_init_netcat, /* inithandler */
	NULL,
	NULL,
	cli_closechansess
};

void cli_send_netcat_request() {

	const char* source_host = "127.0.0.1";
	const int source_port = 22;

	TRACE(("enter cli_send_netcat_request"))
	cli_opts.wantpty = 0;

	if (send_msg_channel_open_init(STDIN_FILENO, &cli_chan_netcat) 
			== DROPBEAR_FAILURE) {
		dropbear_exit("Couldn't open initial channel");
	}

	buf_putstring(ses.writepayload, cli_opts.netcat_host,
			strlen(cli_opts.netcat_host));
	buf_putint(ses.writepayload, cli_opts.netcat_port);

	/* originator ip - localhost is accurate enough */
	buf_putstring(ses.writepayload, source_host, strlen(source_host));
	buf_putint(ses.writepayload, source_port);

	encrypt_packet();
	TRACE(("leave cli_send_netcat_request"))
}
#endif

void cli_send_chansess_request() {

	TRACE(("enter cli_send_chansess_request"))

	if (send_msg_channel_open_init(STDIN_FILENO, &clichansess) 
			== DROPBEAR_FAILURE) {
		dropbear_exit("Couldn't open initial channel");
	}

	/* No special channel request data */
	encrypt_packet();
	TRACE(("leave cli_send_chansess_request"))

}

/* returns 1 if the character should be consumed, 0 to pass through */
static int
do_escape(unsigned char c) {
	switch (c) {
		case '.':
			dropbear_exit("Terminated");
			return 1;
			break;
		case 0x1a:
			/* ctrl-z */
			cli_tty_cleanup();
			kill(getpid(), SIGTSTP);
			/* after continuation */
			cli_tty_setup();
			cli_ses.winchange = 1;
			return 1;
			break;
	}
	return 0;
}

static
void cli_escape_handler(struct Channel* UNUSED(channel), unsigned char* buf, int *len) {
	char c;
	int skip_char = 0;

	/* only handle escape characters if they are read one at a time. simplifies 
	   the code and avoids nasty people putting ~. at the start of a line to paste  */
	if (*len != 1) {
		cli_ses.last_char = 0x0;
		return;
	}

	c = buf[0];

	if (cli_ses.last_char == DROPBEAR_ESCAPE_CHAR) {
		skip_char = do_escape(c);
		cli_ses.last_char = 0x0;
	} else {
		if (c == DROPBEAR_ESCAPE_CHAR) {
			if (cli_ses.last_char == '\r') {
				cli_ses.last_char = DROPBEAR_ESCAPE_CHAR;
				skip_char = 1;
			} else {
				cli_ses.last_char = 0x0;
			}
		} else {
			cli_ses.last_char = c;
		}
	}

	if (skip_char) {
		*len = 0;
	}
}
