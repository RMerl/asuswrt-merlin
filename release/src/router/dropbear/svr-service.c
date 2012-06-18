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
#include "dbutil.h"
#include "service.h"
#include "session.h"
#include "packet.h"
#include "ssh.h"
#include "auth.h"

static void send_msg_service_accept(unsigned char *name, int len);

/* processes a SSH_MSG_SERVICE_REQUEST, returning 0 if finished,
 * 1 if not */
void recv_msg_service_request() {

	unsigned char * name;
	unsigned int len;

	TRACE(("enter recv_msg_service_request"))

	name = buf_getstring(ses.payload, &len);

	/* ssh-userauth */
	if (len == SSH_SERVICE_USERAUTH_LEN && 
			strncmp(SSH_SERVICE_USERAUTH, name, len) == 0) {

		send_msg_service_accept(name, len);
		m_free(name);
		TRACE(("leave recv_msg_service_request: done ssh-userauth"))
		return;
	}

	/* ssh-connection */
	if (len == SSH_SERVICE_CONNECTION_LEN &&
			(strncmp(SSH_SERVICE_CONNECTION, name, len) == 0)) {
		if (ses.authstate.authdone != 1) {
			dropbear_exit("request for connection before auth");
		}

		send_msg_service_accept(name, len);
		m_free(name);
		TRACE(("leave recv_msg_service_request: done ssh-connection"))
		return;
	}

	m_free(name);
	/* TODO this should be a MSG_DISCONNECT */
	dropbear_exit("unrecognised SSH_MSG_SERVICE_REQUEST");


}

static void send_msg_service_accept(unsigned char *name, int len) {

	TRACE(("accepting service %s", name))

	CHECKCLEARTOWRITE();

	buf_putbyte(ses.writepayload, SSH_MSG_SERVICE_ACCEPT);
	buf_putstring(ses.writepayload, name, len);

	encrypt_packet();

}
