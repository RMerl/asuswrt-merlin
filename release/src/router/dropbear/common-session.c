/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
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
#include "packet.h"
#include "algo.h"
#include "buffer.h"
#include "dss.h"
#include "ssh.h"
#include "dbrandom.h"
#include "kex.h"
#include "channel.h"
#include "runopts.h"

static void checktimeouts();
static long select_timeout();
static int ident_readln(int fd, char* buf, int count);
static void read_session_identification();

struct sshsession ses; /* GLOBAL */

/* need to know if the session struct has been initialised, this way isn't the
 * cleanest, but works OK */
int sessinitdone = 0; /* GLOBAL */

/* this is set when we get SIGINT or SIGTERM, the handler is in main.c */
int exitflag = 0; /* GLOBAL */

/* called only at the start of a session, set up initial state */
void common_session_init(int sock_in, int sock_out) {
	time_t now;

	TRACE(("enter session_init"))

	ses.sock_in = sock_in;
	ses.sock_out = sock_out;
	ses.maxfd = MAX(sock_in, sock_out);

	ses.socket_prio = DROPBEAR_PRIO_DEFAULT;
	/* Sets it to lowdelay */
	update_channel_prio();

	now = monotonic_now();
	ses.last_packet_time_keepalive_recv = now;
	ses.last_packet_time_idle = now;
	ses.last_packet_time_any_sent = 0;
	ses.last_packet_time_keepalive_sent = 0;
	
	if (pipe(ses.signal_pipe) < 0) {
		dropbear_exit("Signal pipe failed");
	}
	setnonblocking(ses.signal_pipe[0]);
	setnonblocking(ses.signal_pipe[1]);

	ses.maxfd = MAX(ses.maxfd, ses.signal_pipe[0]);
	ses.maxfd = MAX(ses.maxfd, ses.signal_pipe[1]);
	
	kexfirstinitialise(); /* initialise the kex state */

	ses.writepayload = buf_new(TRANS_MAX_PAYLOAD_LEN);
	ses.transseq = 0;

	ses.readbuf = NULL;
	ses.payload = NULL;
	ses.recvseq = 0;

	initqueue(&ses.writequeue);

	ses.requirenext = SSH_MSG_KEXINIT;
	ses.dataallowed = 1; /* we can send data until we actually 
							send the SSH_MSG_KEXINIT */
	ses.ignorenext = 0;
	ses.lastpacket = 0;
	ses.reply_queue_head = NULL;
	ses.reply_queue_tail = NULL;

	/* set all the algos to none */
	ses.keys = (struct key_context*)m_malloc(sizeof(struct key_context));
	ses.newkeys = NULL;
	ses.keys->recv.algo_crypt = &dropbear_nocipher;
	ses.keys->trans.algo_crypt = &dropbear_nocipher;
	ses.keys->recv.crypt_mode = &dropbear_mode_none;
	ses.keys->trans.crypt_mode = &dropbear_mode_none;
	
	ses.keys->recv.algo_mac = &dropbear_nohash;
	ses.keys->trans.algo_mac = &dropbear_nohash;

	ses.keys->algo_kex = NULL;
	ses.keys->algo_hostkey = -1;
	ses.keys->recv.algo_comp = DROPBEAR_COMP_NONE;
	ses.keys->trans.algo_comp = DROPBEAR_COMP_NONE;

#ifndef DISABLE_ZLIB
	ses.keys->recv.zstream = NULL;
	ses.keys->trans.zstream = NULL;
#endif

	/* key exchange buffers */
	ses.session_id = NULL;
	ses.kexhashbuf = NULL;
	ses.transkexinit = NULL;
	ses.dh_K = NULL;
	ses.remoteident = NULL;

	ses.chantypes = NULL;

	ses.allowprivport = 0;

	TRACE(("leave session_init"))
}

void session_loop(void(*loophandler)()) {

	fd_set readfd, writefd;
	struct timeval timeout;
	int val;

	/* main loop, select()s for all sockets in use */
	for(;;) {

		timeout.tv_sec = select_timeout();
		timeout.tv_usec = 0;
		FD_ZERO(&writefd);
		FD_ZERO(&readfd);
		dropbear_assert(ses.payload == NULL);

		/* during initial setup we flush out the KEXINIT packet before
		 * attempting to read the remote version string, which might block */
		if (ses.sock_in != -1 && (ses.remoteident || isempty(&ses.writequeue))) {
			FD_SET(ses.sock_in, &readfd);
		}
		if (ses.sock_out != -1 && !isempty(&ses.writequeue)) {
			FD_SET(ses.sock_out, &writefd);
		}
		
		/* We get woken up when signal handlers write to this pipe.
		   SIGCHLD in svr-chansession is the only one currently. */
		FD_SET(ses.signal_pipe[0], &readfd);

		/* set up for channels which can be read/written */
		setchannelfds(&readfd, &writefd);

		val = select(ses.maxfd+1, &readfd, &writefd, NULL, &timeout);

		if (exitflag) {
			dropbear_exit("Terminated by signal");
		}
		
		if (val < 0 && errno != EINTR) {
			dropbear_exit("Error in select");
		}

		if (val <= 0) {
			/* If we were interrupted or the select timed out, we still
			 * want to iterate over channels etc for reading, to handle
			 * server processes exiting etc. 
			 * We don't want to read/write FDs. */
			FD_ZERO(&writefd);
			FD_ZERO(&readfd);
		}
		
		/* We'll just empty out the pipe if required. We don't do
		any thing with the data, since the pipe's purpose is purely to
		wake up the select() above. */
		if (FD_ISSET(ses.signal_pipe[0], &readfd)) {
			char x;
			while (read(ses.signal_pipe[0], &x, 1) > 0) {}
		}

		/* check for auth timeout, rekeying required etc */
		checktimeouts();

		/* process session socket's incoming data */
		if (ses.sock_in != -1) {
			if (FD_ISSET(ses.sock_in, &readfd)) {
				if (!ses.remoteident) {
					/* blocking read of the version string */
					read_session_identification();
				} else {
					read_packet();
				}
			}
			
			/* Process the decrypted packet. After this, the read buffer
			 * will be ready for a new packet */
			if (ses.payload != NULL) {
				process_packet();
			}
		}
		
		/* if required, flush out any queued reply packets that
		were being held up during a KEX */
		maybe_flush_reply_queue();

		/* process pipes etc for the channels, ses.dataallowed == 0
		 * during rekeying ) */
		channelio(&readfd, &writefd);

		/* process session socket's outgoing data */
		if (ses.sock_out != -1) {
			if (!isempty(&ses.writequeue)) {
				write_packet();
			}
		}


		if (loophandler) {
			loophandler();
		}

	} /* for(;;) */
	
	/* Not reached */
}

/* clean up a session on exit */
void session_cleanup() {
	
	TRACE(("enter session_cleanup"))
	
	/* we can't cleanup if we don't know the session state */
	if (!sessinitdone) {
		TRACE(("leave session_cleanup: !sessinitdone"))
		return;
	}

	if (ses.extra_session_cleanup) {
		ses.extra_session_cleanup();
	}

	chancleanup();
	
	/* Cleaning up keys must happen after other cleanup
	functions which might queue packets */
	if (ses.session_id) {
		buf_burn(ses.session_id);
		buf_free(ses.session_id);
		ses.session_id = NULL;
	}
	if (ses.hash) {
		buf_burn(ses.hash);
		buf_free(ses.hash);
		ses.hash = NULL;
	}
	m_burn(ses.keys, sizeof(struct key_context));
	m_free(ses.keys);

	TRACE(("leave session_cleanup"))
}

void send_session_identification() {
	buffer *writebuf = buf_new(strlen(LOCAL_IDENT "\r\n") + 1);
	buf_putbytes(writebuf, LOCAL_IDENT "\r\n", strlen(LOCAL_IDENT "\r\n"));
	buf_putbyte(writebuf, 0x0); /* packet type */
	buf_setpos(writebuf, 0);
	enqueue(&ses.writequeue, writebuf);
}

static void read_session_identification() {
	/* max length of 255 chars */
	char linebuf[256];
	int len = 0;
	char done = 0;
	int i;
	/* If they send more than 50 lines, something is wrong */
	for (i = 0; i < 50; i++) {
		len = ident_readln(ses.sock_in, linebuf, sizeof(linebuf));

		if (len < 0 && errno != EINTR) {
			/* It failed */
			break;
		}

		if (len >= 4 && memcmp(linebuf, "SSH-", 4) == 0) {
			/* start of line matches */
			done = 1;
			break;
		}
	}

	if (!done) {
		TRACE(("err: %s for '%s'\n", strerror(errno), linebuf))
		ses.remoteclosed();
	} else {
		/* linebuf is already null terminated */
		ses.remoteident = m_malloc(len);
		memcpy(ses.remoteident, linebuf, len);
	}

	/* Shall assume that 2.x will be backwards compatible. */
	if (strncmp(ses.remoteident, "SSH-2.", 6) != 0
			&& strncmp(ses.remoteident, "SSH-1.99-", 9) != 0) {
		dropbear_exit("Incompatible remote version '%s'", ses.remoteident);
	}

	TRACE(("remoteident: %s", ses.remoteident))

}

/* returns the length including null-terminating zero on success,
 * or -1 on failure */
static int ident_readln(int fd, char* buf, int count) {
	
	char in;
	int pos = 0;
	int num = 0;
	fd_set fds;
	struct timeval timeout;

	TRACE(("enter ident_readln"))

	if (count < 1) {
		return -1;
	}

	FD_ZERO(&fds);

	/* select since it's a non-blocking fd */
	
	/* leave space to null-terminate */
	while (pos < count-1) {

		FD_SET(fd, &fds);

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		if (select(fd+1, &fds, NULL, NULL, &timeout) < 0) {
			if (errno == EINTR) {
				continue;
			}
			TRACE(("leave ident_readln: select error"))
			return -1;
		}

		checktimeouts();
		
		/* Have to go one byte at a time, since we don't want to read past
		 * the end, and have to somehow shove bytes back into the normal
		 * packet reader */
		if (FD_ISSET(fd, &fds)) {
			num = read(fd, &in, 1);
			/* a "\n" is a newline, "\r" we want to read in and keep going
			 * so that it won't be read as part of the next line */
			if (num < 0) {
				/* error */
				if (errno == EINTR) {
					continue; /* not a real error */
				}
				TRACE(("leave ident_readln: read error"))
				return -1;
			}
			if (num == 0) {
				/* EOF */
				TRACE(("leave ident_readln: EOF"))
				return -1;
			}
			if (in == '\n') {
				/* end of ident string */
				break;
			}
			/* we don't want to include '\r's */
			if (in != '\r') {
				buf[pos] = in;
				pos++;
			}
		}
	}

	buf[pos] = '\0';
	TRACE(("leave ident_readln: return %d", pos+1))
	return pos+1;
}

void ignore_recv_response() {
	// Do nothing
	TRACE(("Ignored msg_request_response"))
}

static void send_msg_keepalive() {
	CHECKCLEARTOWRITE();
	time_t old_time_idle = ses.last_packet_time_idle;

	struct Channel *chan = get_any_ready_channel();

	if (chan) {
		/* Channel requests are preferable, more implementations
		handle them than SSH_MSG_GLOBAL_REQUEST */
		TRACE(("keepalive channel request %d", chan->index))
		start_send_channel_request(chan, DROPBEAR_KEEPALIVE_STRING);
	} else {
		TRACE(("keepalive global request"))
		/* Some peers will reply with SSH_MSG_REQUEST_FAILURE, 
		some will reply with SSH_MSG_UNIMPLEMENTED, some will exit. */
		buf_putbyte(ses.writepayload, SSH_MSG_GLOBAL_REQUEST); 
		buf_putstring(ses.writepayload, DROPBEAR_KEEPALIVE_STRING,
			strlen(DROPBEAR_KEEPALIVE_STRING));
	}
	buf_putbyte(ses.writepayload, 1); /* want_reply */
	encrypt_packet();

	ses.last_packet_time_keepalive_sent = monotonic_now();

	/* keepalives shouldn't update idle timeout, reset it back */
	ses.last_packet_time_idle = old_time_idle;
}

/* Check all timeouts which are required. Currently these are the time for
 * user authentication, and the automatic rekeying. */
static void checktimeouts() {

	time_t now;
	now = monotonic_now();
	
	/* we can't rekey if we haven't done remote ident exchange yet */
	if (ses.remoteident == NULL) {
		return;
	}

	if (!ses.kexstate.sentkexinit
			&& (now - ses.kexstate.lastkextime >= KEX_REKEY_TIMEOUT
			|| ses.kexstate.datarecv+ses.kexstate.datatrans >= KEX_REKEY_DATA)) {
		TRACE(("rekeying after timeout or max data reached"))
		send_msg_kexinit();
	}
	
	if (opts.keepalive_secs > 0 && ses.authstate.authdone) {
		/* Avoid sending keepalives prior to auth - those are
		not valid pre-auth packet types */

		/* Send keepalives if we've been idle */
		if (now - ses.last_packet_time_any_sent >= opts.keepalive_secs) {
			send_msg_keepalive();
		}

		/* Also send an explicit keepalive message to trigger a response
		if the remote end hasn't sent us anything */
		if (now - ses.last_packet_time_keepalive_recv >= opts.keepalive_secs
			&& now - ses.last_packet_time_keepalive_sent >= opts.keepalive_secs) {
			send_msg_keepalive();
		}

		if (now - ses.last_packet_time_keepalive_recv 
			>= opts.keepalive_secs * DEFAULT_KEEPALIVE_LIMIT) {
			dropbear_exit("Keepalive timeout");
		}
	}

	if (opts.idle_timeout_secs > 0 
			&& now - ses.last_packet_time_idle >= opts.idle_timeout_secs) {
		dropbear_close("Idle timeout");
	}
}

static long select_timeout() {
	/* determine the minimum timeout that might be required, so
	as to avoid waking when unneccessary */
	long ret = LONG_MAX;
	if (KEX_REKEY_TIMEOUT > 0)
		ret = MIN(KEX_REKEY_TIMEOUT, ret);
	/* AUTH_TIMEOUT is only relevant before authdone */
	if (ses.authstate.authdone != 1 && AUTH_TIMEOUT > 0)
		ret = MIN(AUTH_TIMEOUT, ret);
	if (opts.keepalive_secs > 0)
		ret = MIN(opts.keepalive_secs, ret);
	if (opts.idle_timeout_secs > 0)
		ret = MIN(opts.idle_timeout_secs, ret);
	return ret;
}

const char* get_user_shell() {
	/* an empty shell should be interpreted as "/bin/sh" */
	if (ses.authstate.pw_shell[0] == '\0') {
		return "/bin/sh";
	} else {
		return ses.authstate.pw_shell;
	}
}
void fill_passwd(const char* username) {
	struct passwd *pw = NULL;
	if (ses.authstate.pw_name)
		m_free(ses.authstate.pw_name);
	if (ses.authstate.pw_dir)
		m_free(ses.authstate.pw_dir);
	if (ses.authstate.pw_shell)
		m_free(ses.authstate.pw_shell);
	if (ses.authstate.pw_passwd)
		m_free(ses.authstate.pw_passwd);

	pw = getpwnam(username);
	if (!pw) {
		return;
	}
	ses.authstate.pw_uid = pw->pw_uid;
	ses.authstate.pw_gid = pw->pw_gid;
	ses.authstate.pw_name = m_strdup(pw->pw_name);
	ses.authstate.pw_dir = m_strdup(pw->pw_dir);
	ses.authstate.pw_shell = m_strdup(pw->pw_shell);
	{
		char *passwd_crypt = pw->pw_passwd;
#ifdef HAVE_SHADOW_H
		/* get the shadow password if possible */
		struct spwd *spasswd = getspnam(ses.authstate.pw_name);
		if (spasswd && spasswd->sp_pwdp) {
			passwd_crypt = spasswd->sp_pwdp;
		}
#endif
		if (!passwd_crypt) {
			/* android supposedly returns NULL */
			passwd_crypt = "!!";
		}
		ses.authstate.pw_passwd = m_strdup(passwd_crypt);
	}
}

/* Called when channels are modified */
void update_channel_prio() {
	enum dropbear_prio new_prio;
	int any = 0;
	unsigned int i;

	TRACE(("update_channel_prio"))

	new_prio = DROPBEAR_PRIO_BULK;
	for (i = 0; i < ses.chansize; i++) {
		struct Channel *channel = ses.channels[i];
		if (!channel || channel->prio == DROPBEAR_CHANNEL_PRIO_EARLY) {
			if (channel && channel->prio == DROPBEAR_CHANNEL_PRIO_EARLY) {
				TRACE(("update_channel_prio: early %d", channel->index))
			}
			continue;
		}
		any = 1;
		if (channel->prio == DROPBEAR_CHANNEL_PRIO_INTERACTIVE)
		{
			TRACE(("update_channel_prio: lowdelay %d", channel->index))
			new_prio = DROPBEAR_PRIO_LOWDELAY;
			break;
		} else if (channel->prio == DROPBEAR_CHANNEL_PRIO_UNKNOWABLE
			&& new_prio == DROPBEAR_PRIO_BULK)
		{
			TRACE(("update_channel_prio: unknowable %d", channel->index))
			new_prio = DROPBEAR_PRIO_DEFAULT;
		}
	}

	if (any == 0) {
		/* lowdelay during setup */
		TRACE(("update_channel_prio: not any"))
		new_prio = DROPBEAR_PRIO_LOWDELAY;
	}

	if (new_prio != ses.socket_prio) {
		TRACE(("Dropbear priority transitioning %4.4s -> %4.4s", (char*)&ses.socket_prio, (char*)&new_prio))
		set_sock_priority(ses.sock_out, new_prio);
		ses.socket_prio = new_prio;
	}
}

