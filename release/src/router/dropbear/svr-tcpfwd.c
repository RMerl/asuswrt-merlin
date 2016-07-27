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
#include "ssh.h"
#include "tcpfwd.h"
#include "dbutil.h"
#include "session.h"
#include "buffer.h"
#include "packet.h"
#include "listener.h"
#include "runopts.h"
#include "auth.h"
#include "netio.h"

#ifndef ENABLE_SVR_REMOTETCPFWD

/* This is better than SSH_MSG_UNIMPLEMENTED */
void recv_msg_global_request_remotetcp() {
		TRACE(("recv_msg_global_request_remotetcp: remote tcp forwarding not compiled in"))
		send_msg_request_failure();
}

/* */
#endif /* !ENABLE_SVR_REMOTETCPFWD */

static int svr_cancelremotetcp(void);
static int svr_remotetcpreq(void);
static int newtcpdirect(struct Channel * channel);

#ifdef ENABLE_SVR_REMOTETCPFWD
static const struct ChanType svr_chan_tcpremote = {
	1, /* sepfds */
	"forwarded-tcpip",
	tcp_prio_inithandler,
	NULL,
	NULL,
	NULL
};

/* At the moment this is completely used for tcp code (with the name reflecting
 * that). If new request types are added, this should be replaced with code
 * similar to the request-switching in chansession.c */
void recv_msg_global_request_remotetcp() {

	char* reqname = NULL;
	unsigned int namelen;
	unsigned int wantreply = 0;
	int ret = DROPBEAR_FAILURE;

	TRACE(("enter recv_msg_global_request_remotetcp"))

	if (svr_opts.noremotetcp || !svr_pubkey_allows_tcpfwd()) {
		TRACE(("leave recv_msg_global_request_remotetcp: remote tcp forwarding disabled"))
		goto out;
	}

	reqname = buf_getstring(ses.payload, &namelen);
	wantreply = buf_getbool(ses.payload);

	if (namelen > MAX_NAME_LEN) {
		TRACE(("name len is wrong: %d", namelen))
		goto out;
	}

	if (strcmp("tcpip-forward", reqname) == 0) {
		ret = svr_remotetcpreq();
	} else if (strcmp("cancel-tcpip-forward", reqname) == 0) {
		ret = svr_cancelremotetcp();
	} else {
		TRACE(("reqname isn't tcpip-forward: '%s'", reqname))
	}

out:
	if (wantreply) {
		if (ret == DROPBEAR_SUCCESS) {
			send_msg_request_success();
		} else {
			send_msg_request_failure();
		}
	}

	m_free(reqname);

	TRACE(("leave recv_msg_global_request"))
}

static int matchtcp(void* typedata1, void* typedata2) {

	const struct TCPListener *info1 = (struct TCPListener*)typedata1;
	const struct TCPListener *info2 = (struct TCPListener*)typedata2;

	return (info1->listenport == info2->listenport)
			&& (info1->chantype == info2->chantype)
			&& (strcmp(info1->listenaddr, info2->listenaddr) == 0);
}

static int svr_cancelremotetcp() {

	int ret = DROPBEAR_FAILURE;
	char * bindaddr = NULL;
	unsigned int addrlen;
	unsigned int port;
	struct Listener * listener = NULL;
	struct TCPListener tcpinfo;

	TRACE(("enter cancelremotetcp"))

	bindaddr = buf_getstring(ses.payload, &addrlen);
	if (addrlen > MAX_IP_LEN) {
		TRACE(("addr len too long: %d", addrlen))
		goto out;
	}

	port = buf_getint(ses.payload);

	tcpinfo.sendaddr = NULL;
	tcpinfo.sendport = 0;
	tcpinfo.listenaddr = bindaddr;
	tcpinfo.listenport = port;
	listener = get_listener(CHANNEL_ID_TCPFORWARDED, &tcpinfo, matchtcp);
	if (listener) {
		remove_listener( listener );
		ret = DROPBEAR_SUCCESS;
	}

out:
	m_free(bindaddr);
	TRACE(("leave cancelremotetcp"))
	return ret;
}

static int svr_remotetcpreq() {

	int ret = DROPBEAR_FAILURE;
	char * request_addr = NULL;
	unsigned int addrlen;
	struct TCPListener *tcpinfo = NULL;
	unsigned int port;

	TRACE(("enter remotetcpreq"))

	request_addr = buf_getstring(ses.payload, &addrlen);
	if (addrlen > MAX_IP_LEN) {
		TRACE(("addr len too long: %d", addrlen))
		goto out;
	}

	port = buf_getint(ses.payload);

	if (port == 0) {
		dropbear_log(LOG_INFO, "Server chosen tcpfwd ports are unsupported");
		goto out;
	}

	if (port < 1 || port > 65535) {
		TRACE(("invalid port: %d", port))
		goto out;
	}

	if (!ses.allowprivport && port < IPPORT_RESERVED) {
		TRACE(("can't assign port < 1024 for non-root"))
		goto out;
	}

	tcpinfo = (struct TCPListener*)m_malloc(sizeof(struct TCPListener));
	tcpinfo->sendaddr = NULL;
	tcpinfo->sendport = 0;
	tcpinfo->listenport = port;
	tcpinfo->chantype = &svr_chan_tcpremote;
	tcpinfo->tcp_type = forwarded;

	tcpinfo->request_listenaddr = request_addr;
	if (!opts.listen_fwd_all || (strcmp(request_addr, "localhost") == 0) ) {
		/* NULL means "localhost only" */
		tcpinfo->listenaddr = NULL;
	}
	else
	{
		tcpinfo->listenaddr = request_addr;
	}

	ret = listen_tcpfwd(tcpinfo);

out:
	if (ret == DROPBEAR_FAILURE) {
		/* we only free it if a listener wasn't created, since the listener
		 * has to remember it if it's to be cancelled */
		m_free(request_addr);
		m_free(tcpinfo);
	}
	TRACE(("leave remotetcpreq"))
	return ret;
}

#endif /* ENABLE_SVR_REMOTETCPFWD */

#ifdef ENABLE_SVR_LOCALTCPFWD

const struct ChanType svr_chan_tcpdirect = {
	1, /* sepfds */
	"direct-tcpip",
	newtcpdirect, /* init */
	NULL, /* checkclose */
	NULL, /* reqhandler */
	NULL /* closehandler */
};

/* Called upon creating a new direct tcp channel (ie we connect out to an
 * address */
static int newtcpdirect(struct Channel * channel) {

	char* desthost = NULL;
	unsigned int destport;
	char* orighost = NULL;
	unsigned int origport;
	char portstring[NI_MAXSERV];
	unsigned int len;
	int err = SSH_OPEN_ADMINISTRATIVELY_PROHIBITED;

	TRACE(("newtcpdirect channel %d", channel->index))

	if (svr_opts.nolocaltcp || !svr_pubkey_allows_tcpfwd()) {
		TRACE(("leave newtcpdirect: local tcp forwarding disabled"))
		goto out;
	}

	desthost = buf_getstring(ses.payload, &len);
	if (len > MAX_HOST_LEN) {
		TRACE(("leave newtcpdirect: desthost too long"))
		goto out;
	}

	destport = buf_getint(ses.payload);
	
	orighost = buf_getstring(ses.payload, &len);
	if (len > MAX_HOST_LEN) {
		TRACE(("leave newtcpdirect: orighost too long"))
		goto out;
	}

	origport = buf_getint(ses.payload);

	/* best be sure */
	if (origport > 65535 || destport > 65535) {
		TRACE(("leave newtcpdirect: port > 65535"))
		goto out;
	}

	snprintf(portstring, sizeof(portstring), "%u", destport);
	channel->conn_pending = connect_remote(desthost, portstring, channel_connect_done, channel);

	channel->prio = DROPBEAR_CHANNEL_PRIO_UNKNOWABLE;
	
	err = SSH_OPEN_IN_PROGRESS;

out:
	m_free(desthost);
	m_free(orighost);
	TRACE(("leave newtcpdirect: err %d", err))
	return err;
}

#endif /* ENABLE_SVR_LOCALTCPFWD */
