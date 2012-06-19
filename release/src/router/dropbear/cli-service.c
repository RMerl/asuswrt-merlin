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
#include "service.h"
#include "dbutil.h"
#include "packet.h"
#include "buffer.h"
#include "session.h"
#include "ssh.h"

void send_msg_service_request(char* servicename) {

	TRACE(("enter send_msg_service_request: servicename='%s'", servicename))

	CHECKCLEARTOWRITE();

	buf_putbyte(ses.writepayload, SSH_MSG_SERVICE_REQUEST);
	buf_putstring(ses.writepayload, servicename, strlen(servicename));

	encrypt_packet();
	TRACE(("leave send_msg_service_request"))
}

/* This just sets up the state variables right for the main client session loop
 * to deal with */
void recv_msg_service_accept() {

	unsigned char* servicename;
	unsigned int len;

	TRACE(("enter recv_msg_service_accept"))

	servicename = buf_getstring(ses.payload, &len);

	/* ssh-userauth */
	if (cli_ses.state == SERVICE_AUTH_REQ_SENT
			&& len == SSH_SERVICE_USERAUTH_LEN
			&& strncmp(SSH_SERVICE_USERAUTH, servicename, len) == 0) {

		cli_ses.state = SERVICE_AUTH_ACCEPT_RCVD;
		m_free(servicename);
		TRACE(("leave recv_msg_service_accept: done ssh-userauth"))
		return;
	}

	/* ssh-connection */
	if (cli_ses.state == SERVICE_CONN_REQ_SENT
			&& len == SSH_SERVICE_CONNECTION_LEN 
			&& strncmp(SSH_SERVICE_CONNECTION, servicename, len) == 0) {

		if (ses.authstate.authdone != 1) {
			dropbear_exit("Request for connection before auth");
		}

		cli_ses.state = SERVICE_CONN_ACCEPT_RCVD;
		m_free(servicename);
		TRACE(("leave recv_msg_service_accept: done ssh-connection"))
		return;
	}

	dropbear_exit("Unrecognised service accept");
}
