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

#ifndef DISABLE_X11FWD
#include "x11fwd.h"
#include "session.h"
#include "ssh.h"
#include "dbutil.h"
#include "chansession.h"
#include "channel.h"
#include "packet.h"
#include "buffer.h"
#include "auth.h"

#define X11BASEPORT 6000
#define X11BINDBASE 6010

static void x11accept(struct Listener* listener, int sock);
static int bindport(int fd);
static int send_msg_channel_open_x11(int fd, struct sockaddr_in* addr);

/* called as a request for a session channel, sets up listening X11 */
/* returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
int x11req(struct ChanSess * chansess) {

	int fd;

	if (!svr_pubkey_allows_x11fwd()) {
		return DROPBEAR_FAILURE;
	}

	/* we already have an x11 connection */
	if (chansess->x11listener != NULL) {
		return DROPBEAR_FAILURE;
	}

	chansess->x11singleconn = buf_getbool(ses.payload);
	chansess->x11authprot = buf_getstring(ses.payload, NULL);
	chansess->x11authcookie = buf_getstring(ses.payload, NULL);
	chansess->x11screennum = buf_getint(ses.payload);

	/* create listening socket */
	fd = socket(PF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		goto fail;
	}

	/* allocate port and bind */
	chansess->x11port = bindport(fd);
	if (chansess->x11port < 0) {
		goto fail;
	}

	/* listen */
	if (listen(fd, 20) < 0) {
		goto fail;
	}

	/* set non-blocking */
	setnonblocking(fd);

	/* listener code will handle the socket now.
	 * No cleanup handler needed, since listener_remove only happens
	 * from our cleanup anyway */
	chansess->x11listener = new_listener( &fd, 1, 0, chansess, x11accept, NULL);
	if (chansess->x11listener == NULL) {
		goto fail;
	}

	return DROPBEAR_SUCCESS;

fail:
	/* cleanup */
	m_free(chansess->x11authprot);
	m_free(chansess->x11authcookie);
	close(fd);

	return DROPBEAR_FAILURE;
}

/* accepts a new X11 socket */
/* returns DROPBEAR_FAILURE or DROPBEAR_SUCCESS */
static void x11accept(struct Listener* listener, int sock) {

	int fd;
	struct sockaddr_in addr;
	int len;
	int ret;
	struct ChanSess * chansess = (struct ChanSess *)(listener->typedata);

	len = sizeof(addr);

	fd = accept(sock, (struct sockaddr*)&addr, &len);
	if (fd < 0) {
		return;
	}

	/* if single-connection we close it up */
	if (chansess->x11singleconn) {
		x11cleanup(chansess);
	}

	ret = send_msg_channel_open_x11(fd, &addr);
	if (ret == DROPBEAR_FAILURE) {
		close(fd);
	}
}

/* This is called after switching to the user, and sets up the xauth
 * and environment variables.  */
void x11setauth(struct ChanSess *chansess) {

	char display[20]; /* space for "localhost:12345.123" */
	FILE * authprog = NULL;
	int val;

	if (chansess->x11listener == NULL) {
		return;
	}

	/* create the DISPLAY string */
	val = snprintf(display, sizeof(display), "localhost:%d.%d",
			chansess->x11port - X11BASEPORT, chansess->x11screennum);
	if (val < 0 || val >= (int)sizeof(display)) {
		/* string was truncated */
		return;
	}

	addnewvar("DISPLAY", display);

	/* create the xauth string */
	val = snprintf(display, sizeof(display), "unix:%d.%d",
			chansess->x11port - X11BASEPORT, chansess->x11screennum);
	if (val < 0 || val >= (int)sizeof(display)) {
		/* string was truncated */
		return;
	}

	/* popen is a nice function - code is strongly based on OpenSSH's */
	authprog = popen(XAUTH_COMMAND, "w");
	if (authprog) {
		fprintf(authprog, "add %s %s %s\n",
				display, chansess->x11authprot, chansess->x11authcookie);
		pclose(authprog);
	} else {
		fprintf(stderr, "Failed to run %s\n", XAUTH_COMMAND);
	}
}

void x11cleanup(struct ChanSess *chansess) {

	m_free(chansess->x11authprot);
	m_free(chansess->x11authcookie);

	TRACE(("chansess %x", chansess))
	if (chansess->x11listener != NULL) {
		remove_listener(chansess->x11listener);
		chansess->x11listener = NULL;
	}
}

static const struct ChanType chan_x11 = {
	0, /* sepfds */
	"x11",
	NULL, /* inithandler */
	NULL, /* checkclose */
	NULL, /* reqhandler */
	NULL /* closehandler */
};


static int send_msg_channel_open_x11(int fd, struct sockaddr_in* addr) {

	char* ipstring = NULL;

	if (send_msg_channel_open_init(fd, &chan_x11) == DROPBEAR_SUCCESS) {
		ipstring = inet_ntoa(addr->sin_addr);
		buf_putstring(ses.writepayload, ipstring, strlen(ipstring));
		buf_putint(ses.writepayload, addr->sin_port);

		encrypt_packet();
		return DROPBEAR_SUCCESS;
	} else {
		return DROPBEAR_FAILURE;
	}

}

/* returns the port bound to, or -1 on failure.
 * Will attempt to bind to a port X11BINDBASE (6010 usually) or upwards */
static int bindport(int fd) {

	struct sockaddr_in addr;
	uint16_t port;

	memset((void*)&addr, 0x0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	/* if we can't find one in 2000 ports free, something's wrong */
	for (port = X11BINDBASE; port < X11BINDBASE + 2000; port++) {
		addr.sin_port = htons(port);
		if (bind(fd, (struct sockaddr*)&addr, 
					sizeof(struct sockaddr_in)) == 0) {
			/* success */
			return port;
		}
		if (errno == EADDRINUSE) {
			/* try the next port */
			continue;
		}
		/* otherwise it was an error we don't know about */
		dropbear_log(LOG_DEBUG, "Failed to bind x11 socket");
		break;
	}
	return -1;
}
#endif /* DROPBEAR_X11FWD */
