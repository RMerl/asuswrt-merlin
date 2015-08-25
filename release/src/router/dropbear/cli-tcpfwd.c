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
#include "netio.h"

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
static int cli_localtcp(const char* listenaddr, 
		unsigned int listenport, 
		const char* remoteaddr,
		unsigned int remoteport);
static const struct ChanType cli_chan_tcplocal = {
	1, /* sepfds */
	"direct-tcpip",
	tcp_prio_inithandler,
	NULL,
	NULL,
	NULL
};
#endif

#ifdef ENABLE_CLI_LOCALTCPFWD
void setup_localtcp() {
	m_list_elem *iter;
	int ret;

	TRACE(("enter setup_localtcp"))

	for (iter = cli_opts.localfwds->first; iter; iter = iter->next) {
		struct TCPFwdEntry * fwd = (struct TCPFwdEntry*)iter->item;
		ret = cli_localtcp(
				fwd->listenaddr,
				fwd->listenport,
				fwd->connectaddr,
				fwd->connectport);
		if (ret == DROPBEAR_FAILURE) {
			dropbear_log(LOG_WARNING, "Failed local port forward %s:%d:%s:%d",
					fwd->listenaddr,
					fwd->listenport,
					fwd->connectaddr,
					fwd->connectport);
		}		
	}
	TRACE(("leave setup_localtcp"))

}

static int cli_localtcp(const char* listenaddr, 
		unsigned int listenport, 
		const char* remoteaddr,
		unsigned int remoteport) {

	struct TCPListener* tcpinfo = NULL;
	int ret;

	TRACE(("enter cli_localtcp: %d %s %d", listenport, remoteaddr,
				remoteport));

	tcpinfo = (struct TCPListener*)m_malloc(sizeof(struct TCPListener));

	tcpinfo->sendaddr = m_strdup(remoteaddr);
	tcpinfo->sendport = remoteport;

	if (listenaddr)
	{
		tcpinfo->listenaddr = m_strdup(listenaddr);
	}
	else
	{
		if (opts.listen_fwd_all) {
			tcpinfo->listenaddr = m_strdup("");
		} else {
			tcpinfo->listenaddr = m_strdup("localhost");
		}
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
static void send_msg_global_request_remotetcp(const char *addr, int port) {

	TRACE(("enter send_msg_global_request_remotetcp"))

	CHECKCLEARTOWRITE();
	buf_putbyte(ses.writepayload, SSH_MSG_GLOBAL_REQUEST);
	buf_putstring(ses.writepayload, "tcpip-forward", 13);
	buf_putbyte(ses.writepayload, 1); /* want_reply */
	buf_putstring(ses.writepayload, addr, strlen(addr));
	buf_putint(ses.writepayload, port);

	encrypt_packet();

	TRACE(("leave send_msg_global_request_remotetcp"))
}

/* The only global success/failure messages are for remotetcp.
 * Since there isn't any identifier in these messages, we have to rely on them
 * being in the same order as we sent the requests. This is the ordering
 * of the cli_opts.remotefwds list.
 * If the requested remote port is 0 the listen port will be
 * dynamically allocated by the server and the port number will be returned
 * to client and the port number reported to the user. */
void cli_recv_msg_request_success() {
	/* We just mark off that we have received the reply,
	 * so that we can report failure for later ones. */
	m_list_elem * iter = NULL;
	for (iter = cli_opts.remotefwds->first; iter; iter = iter->next) {
		struct TCPFwdEntry *fwd = (struct TCPFwdEntry*)iter->item;
		if (!fwd->have_reply) {
			fwd->have_reply = 1;
			if (fwd->listenport == 0) {
				/* The server should let us know which port was allocated if we requested port 0 */
				int allocport = buf_getint(ses.payload);
				if (allocport > 0) {
					fwd->listenport = allocport;
					dropbear_log(LOG_INFO, "Allocated port %d for remote forward to %s:%d", 
							allocport, fwd->connectaddr, fwd->connectport);
				}
			}
			return;
		}
	}
}

void cli_recv_msg_request_failure() {
	m_list_elem *iter;
	for (iter = cli_opts.remotefwds->first; iter; iter = iter->next) {
		struct TCPFwdEntry *fwd = (struct TCPFwdEntry*)iter->item;
		if (!fwd->have_reply) {
			fwd->have_reply = 1;
			dropbear_log(LOG_WARNING, "Remote TCP forward request failed (port %d -> %s:%d)", fwd->listenport, fwd->connectaddr, fwd->connectport);
			return;
		}
	}
}

void setup_remotetcp() {
	m_list_elem *iter;
	TRACE(("enter setup_remotetcp"))

	for (iter = cli_opts.remotefwds->first; iter; iter = iter->next) {
		struct TCPFwdEntry *fwd = (struct TCPFwdEntry*)iter->item;
		if (!fwd->listenaddr)
		{
			/* we store the addresses so that we can compare them
			   when the server sends them back */
			if (opts.listen_fwd_all) {
				fwd->listenaddr = m_strdup("");
			} else {
				fwd->listenaddr = m_strdup("localhost");
			}
		}
		send_msg_global_request_remotetcp(fwd->listenaddr, fwd->listenport);
	}

	TRACE(("leave setup_remotetcp"))
}

static int newtcpforwarded(struct Channel * channel) {

    char *origaddr = NULL;
	unsigned int origport;
	m_list_elem * iter = NULL;
	struct TCPFwdEntry *fwd;
	char portstring[NI_MAXSERV];
	int err = SSH_OPEN_ADMINISTRATIVELY_PROHIBITED;

	origaddr = buf_getstring(ses.payload, NULL);
	origport = buf_getint(ses.payload);

	/* Find which port corresponds. First try and match address as well as port,
	in case they want to forward different ports separately ... */
	for (iter = cli_opts.remotefwds->first; iter; iter = iter->next) {
		fwd = (struct TCPFwdEntry*)iter->item;
		if (origport == fwd->listenport
				&& strcmp(origaddr, fwd->listenaddr) == 0) {
			break;
		}
	}

	if (!iter)
	{
		/* ... otherwise try to generically match the only forwarded port 
		without address (also handles ::1 vs 127.0.0.1 vs localhost case).
		rfc4254 is vague about the definition of "address that was connected" */
		for (iter = cli_opts.remotefwds->first; iter; iter = iter->next) {
			fwd = (struct TCPFwdEntry*)iter->item;
			if (origport == fwd->listenport) {
				break;
			}
		}
	}


	if (iter == NULL) {
		/* We didn't request forwarding on that port */
        	cleantext(origaddr);
		dropbear_log(LOG_INFO, "Server sent unrequested forward from \"%s:%d\"", 
                origaddr, origport);
		goto out;
	}
	
	snprintf(portstring, sizeof(portstring), "%d", fwd->connectport);
	channel->conn_pending = connect_remote(fwd->connectaddr, portstring, channel_connect_done, channel);

	channel->prio = DROPBEAR_CHANNEL_PRIO_UNKNOWABLE;
	
	err = SSH_OPEN_IN_PROGRESS;

out:
	m_free(origaddr);
	TRACE(("leave newtcpdirect: err %d", err))
	return err;
}
#endif /* ENABLE_CLI_REMOTETCPFWD */
