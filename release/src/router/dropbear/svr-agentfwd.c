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

/* This file (agentfwd.c) handles authentication agent forwarding, for OpenSSH
 * style agents. */

#include "includes.h"

#ifdef ENABLE_SVR_AGENTFWD

#include "agentfwd.h"
#include "session.h"
#include "ssh.h"
#include "dbutil.h"
#include "chansession.h"
#include "channel.h"
#include "packet.h"
#include "buffer.h"
#include "dbrandom.h"
#include "listener.h"
#include "auth.h"

#define AGENTDIRPREFIX "/tmp/dropbear-"

static int send_msg_channel_open_agent(int fd);
static int bindagent(int fd, struct ChanSess * chansess);
static void agentaccept(struct Listener * listener, int sock);

/* Handles client requests to start agent forwarding, sets up listening socket.
 * Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
int svr_agentreq(struct ChanSess * chansess) {
	int fd = -1;

	if (!svr_pubkey_allows_agentfwd()) {
		return DROPBEAR_FAILURE;
	}

	if (chansess->agentlistener != NULL) {
		return DROPBEAR_FAILURE;
	}

	/* create listening socket */
	fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		goto fail;
	}

	/* create the unix socket dir and file */
	if (bindagent(fd, chansess) == DROPBEAR_FAILURE) {
		goto fail;
	}

	/* listen */
	if (listen(fd, 20) < 0) {
		goto fail;
	}

	/* set non-blocking */
	setnonblocking(fd);

	/* pass if off to listener */
	chansess->agentlistener = new_listener( &fd, 1, 0, chansess, 
								agentaccept, NULL);

	if (chansess->agentlistener == NULL) {
		goto fail;
	}

	return DROPBEAR_SUCCESS;

fail:
	m_close(fd);
	/* cleanup */
	svr_agentcleanup(chansess);

	return DROPBEAR_FAILURE;
}

/* accepts a connection on the forwarded socket and opens a new channel for it
 * back to the client */
/* returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
static void agentaccept(struct Listener *UNUSED(listener), int sock) {

	int fd;

	fd = accept(sock, NULL, NULL);
	if (fd < 0) {
		TRACE(("accept failed"))
		return;
	}

	if (send_msg_channel_open_agent(fd) != DROPBEAR_SUCCESS) {
		close(fd);
	}

}

/* set up the environment variable pointing to the socket. This is called
 * just before command/shell execution, after dropping priveleges */
void svr_agentset(struct ChanSess * chansess) {

	char *path = NULL;
	int len;

	if (chansess->agentlistener == NULL) {
		return;
	}

	/* 2 for "/" and "\0" */
	len = strlen(chansess->agentdir) + strlen(chansess->agentfile) + 2;

	path = m_malloc(len);
	snprintf(path, len, "%s/%s", chansess->agentdir, chansess->agentfile);
	addnewvar("SSH_AUTH_SOCK", path);
	m_free(path);
}

/* close the socket, remove the socket-file */
void svr_agentcleanup(struct ChanSess * chansess) {

	char *path = NULL;
	uid_t uid;
	gid_t gid;
	int len;

	if (chansess->agentlistener != NULL) {
		remove_listener(chansess->agentlistener);
		chansess->agentlistener = NULL;
	}

	if (chansess->agentfile != NULL && chansess->agentdir != NULL) {

		/* Remove the dir as the user. That way they can't cause problems except
		 * for themselves */
		uid = getuid();
		gid = getgid();
		if ((setegid(ses.authstate.pw_gid)) < 0 ||
			(seteuid(ses.authstate.pw_uid)) < 0) {
			dropbear_exit("Failed to set euid");
		}

		/* 2 for "/" and "\0" */
		len = strlen(chansess->agentdir) + strlen(chansess->agentfile) + 2;

		path = m_malloc(len);
		snprintf(path, len, "%s/%s", chansess->agentdir, chansess->agentfile);
		unlink(path);
		m_free(path);

		rmdir(chansess->agentdir);

		if ((seteuid(uid)) < 0 ||
			(setegid(gid)) < 0) {
			dropbear_exit("Failed to revert euid");
		}

		m_free(chansess->agentfile);
		m_free(chansess->agentdir);
	}

}

static const struct ChanType chan_svr_agent = {
	0, /* sepfds */
	"auth-agent@openssh.com",
	NULL,
	NULL,
	NULL,
	NULL
};


/* helper for accepting an agent request */
static int send_msg_channel_open_agent(int fd) {

	if (send_msg_channel_open_init(fd, &chan_svr_agent) == DROPBEAR_SUCCESS) {
		encrypt_packet();
		return DROPBEAR_SUCCESS;
	} else {
		return DROPBEAR_FAILURE;
	}
}

/* helper for creating the agent socket-file
   returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
static int bindagent(int fd, struct ChanSess * chansess) {

	struct sockaddr_un addr;
	unsigned int prefix;
	char path[sizeof(addr.sun_path)], sockfile[sizeof(addr.sun_path)];
	mode_t mode;
	int i;
	uid_t uid;
	gid_t gid;
	int ret = DROPBEAR_FAILURE;

	/* drop to user privs to make the dir/file */
	uid = getuid();
	gid = getgid();
	if ((setegid(ses.authstate.pw_gid)) < 0 ||
		(seteuid(ses.authstate.pw_uid)) < 0) {
		dropbear_exit("Failed to set euid");
	}

	memset((void*)&addr, 0x0, sizeof(addr));
	addr.sun_family = AF_UNIX;

	mode = S_IRWXU;

	for (i = 0; i < 20; i++) {
		genrandom((unsigned char*)&prefix, sizeof(prefix));
		/* we want 32 bits (8 hex digits) - "/tmp/dropbear-f19c62c0" */
		snprintf(path, sizeof(path), AGENTDIRPREFIX "%.8x", prefix);

		if (mkdir(path, mode) == 0) {
			goto bindsocket;
		}
		if (errno != EEXIST) {
			break;
		}
	}
	/* couldn't make a dir */
	goto out;

bindsocket:
	/* Format is "/tmp/dropbear-0246dead/auth-d00f7654-23".
	 * The "23" is the file desc, the random data is to avoid collisions
	 * between subsequent user processes reusing socket fds (odds are now
	 * 1/(2^64) */
	genrandom((unsigned char*)&prefix, sizeof(prefix));
	snprintf(sockfile, sizeof(sockfile), "auth-%.8x-%d", prefix, fd);
			
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/%s", path, sockfile);

	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
		chansess->agentdir = m_strdup(path);
		chansess->agentfile = m_strdup(sockfile);
		ret = DROPBEAR_SUCCESS;
	}


out:
	if ((seteuid(uid)) < 0 ||
		(setegid(gid)) < 0) {
		dropbear_exit("Failed to revert euid");
	}
	return ret;
}

#endif
