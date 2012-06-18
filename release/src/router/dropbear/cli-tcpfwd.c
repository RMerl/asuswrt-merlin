/*
 * Dropbear SSH
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
#include "options.h"
#include "dbutil.h"
#include "tcpfwd.h"
#include "channel.h"
#include "runopts.h"
#include "session.h"
#include "ssh.h"

#ifdef ENABLE_CLI_REMOTETCPFWD
static int newtcpforwarded(struct Channel * channel);

const struct ChanType cli_chan_tcpremote = {
	1, /* sepfds */
	"forwarded-tcpip",
	newtcpforwarded,
	NULL,
	NULL,
	NULL
};
#endif

#ifdef ENABLE_CLI_LOCALTCPFWD
static int cli_localtcp(unsigned int listenport, const char* remoteaddr,
		unsigned int remoteport);
static const struct ChanType cli_chan_tcplocal = {
	1, /* sepfds */
	"direct-tcpip",
	NULL,
	NULL,
	NULL,
	NULL
};
#endif

#ifdef ENABLE_CLI_LOCALTCPFWD
void setup_localtcp() {

	int ret;

	TRACE(("enter setup_localtcp"))

	if (cli_opts.localfwds == NULL) {
		TRACE(("cli_opts.localfwds == NULL"))
	}

	while (cli_opts.localfwds != NULL) {
		ret = cli_localtcp(cli_opts.localfwds->listenport,
				cli_opts.localfwds->connectaddr,
				cli_opts.localfwds->connectport);
		if (ret == DROPBEAR_FAILURE) {
			dropbear_log(LOG_WARNING, "Failed local port forward %d:%s:%d",
					cli_opts.localfwds->listenport,
					cli_opts.localfwds->connectaddr,
					cli_opts.localfwds->connectport);
		}

		cli_opts.localfwds = cli_opts.localfwds->next;
	}
	TRACE(("leave setup_localtcp"))

}

static int cli_localtcp(unsigned int listenport, const char* remoteaddr,
		unsigned int remoteport) {

	struct TCPListener* tcpinfo = NULL;
	int ret;

	TRACE(("enter cli_localtcp: %d %s %d", listenport, remoteaddr,
				remoteport));

	tcpinfo = (struct TCPListener*)m_malloc(sizeof(struct TCPListener));

	tcpinfo->sendaddr = m_strdup(remoteaddr);
	tcpinfo->sendport = remoteport;

	if (opts.listen_fwd_all) {
		tcpinfo->listenaddr = m_strdup("");
	} else {
		tcpinfo->listenaddr = m_strdup("localhost");
	}
	tcpinfo->listenport = listenport;

	tcpinfo->chantype = &cli_chan_tcplocal;
	tcpinfo->tcp_type = direct;

	ret = listen_tcpfwd(tcpinfo);

	if (ret == DROPBEAR_FAILURE) {
		m_free(tcpinfo);
	}
	TRACE(("leave cli_localtcp: %d", ret))
	return ret;
}
#endif /* ENABLE_CLI_LOCALTCPFWD */

#ifdef  ENABLE_CLI_REMOTETCPFWD
static void send_msg_global_request_remotetcp(int port) {

	char* listenspec = NULL;
	TRACE(("enter send_msg_global_request_remotetcp"))

	CHECKCLEARTOWRITE();
	buf_putbyte(ses.writepayload, SSH_MSG_GLOBAL_REQUEST);
	buf_putstring(ses.writepayload, "tcpip-forward", 13);
	buf_putbyte(ses.writepayload, 1); /* want_reply */
	if (opts.listen_fwd_all) {
		listenspec = "";
	} else {
		listenspec = "localhost";
	}
	/* TODO: IPv6? */;
	buf_putstring(ses.writepayload, listenspec, strlen(listenspec));
	buf_putint(ses.writepayload, port);

	encrypt_packet();

	TRACE(("leave send_msg_global_request_remotetcp"))
}

/* The only global success/failure messages are for remotetcp.
 * Since there isn't any identifier in these messages, we have to rely on them
 * being in the same order as we sent the requests. This is the ordering
 * of the cli_opts.remotefwds list */
void cli_recv_msg_request_success() {

	/* Nothing in the packet. We just mark off that we have received the reply,
	 * so that we can report failure for later ones. */
	struct TCPFwdList * iter = NULL;

	iter = cli_opts.remotefwds;
	while (iter != NULL) {
		if (!iter->have_reply)
		{
			iter->have_reply = 1;
			return;
		}
		iter = iter->next;
	}
}

void cli_recv_msg_request_failure() {
	struct TCPFwdList * iter = NULL;

	iter = cli_opts.remotefwds;
	while (iter != NULL) {
		if (!iter->have_reply)
		{
			iter->have_reply = 1;
			dropbear_log(LOG_WARNING, "Remote TCP forward request failed (port %d -> %s:%d)", iter->listenport, iter->connectaddr, iter->connectport);
			return;
		}
		iter = iter->next;
	}
}

void setup_remotetcp() {

	struct TCPFwdList * iter = NULL;

	TRACE(("enter setup_remotetcp"))

	if (cli_opts.remotefwds == NULL) {
		TRACE(("cli_opts.remotefwds == NULL"))
	}

	iter = cli_opts.remotefwds;

	while (iter != NULL) {
		send_msg_global_request_remotetcp(iter->listenport);
		iter = iter->next;
	}
	TRACE(("leave setup_remotetcp"))
}

static int newtcpforwarded(struct Channel * channel) {

	unsigned int origport;
	struct TCPFwdList * iter = NULL;
	char portstring[NI_MAXSERV];
	int sock;
	int err = SSH_OPEN_ADMINISTRATIVELY_PROHIBITED;

	/* We don't care what address they connected to */
	buf_eatstring(ses.payload);

	origport = buf_getint(ses.payload);

	/* Find which port corresponds */
	iter = cli_opts.remotefwds;

	while (iter != NULL) {
		if (origport == iter->listenport) {
			break;
		}
		iter = iter->next;
	}

	if (iter == NULL) {
		/* We didn't request forwarding on that port */
		dropbear_log(LOG_INFO, "Server send unrequested port, from port %d", 
										origport);
		goto out;
	}
	
	snprintf(portstring, sizeof(portstring), "%d", iter->connectport);
	sock = connect_remote(iter->connectaddr, portstring, 1, NULL);
	if (sock < 0) {
		TRACE(("leave newtcpdirect: sock failed"))
		err = SSH_OPEN_CONNECT_FAILED;
		goto out;
	}

	ses.maxfd = MAX(ses.maxfd, sock);

	/* We don't set readfd, that will get set after the connection's
	 * progress succeeds */
	channel->writefd = sock;
	channel->initconn = 1;
	
	err = SSH_OPEN_IN_PROGRESS;

out:
	TRACE(("leave newtcpdirect: err %d", err))
	return err;
}
#endif /* ENABLE_CLI_REMOTETCPFWD */
