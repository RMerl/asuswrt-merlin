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
#ifndef _TCPFWD_H
#define _TCPFWD_H

#include "channel.h"

struct TCPListener {

	/* For a direct-tcpip request, it's the addr/port we want the other
	 * end to connect to */
	unsigned char *sendaddr;
	unsigned int sendport;

	/* This is the address/port that we listen on. The address has special
	 * meanings as per the rfc, "" for all interfaces, "localhost" for 
	 * localhost, or a normal interface name. */
	unsigned char *listenaddr;
	unsigned int listenport;

	const struct ChanType *chantype;
	enum {direct, forwarded} tcp_type;
};

/* A link in a list of forwards */
struct TCPFwdList {

	const unsigned char* connectaddr;
	unsigned int connectport;
	unsigned int listenport;
	unsigned int have_reply; /* is set to 1 after a reply has been received
								when setting up the forwarding */
	struct TCPFwdList * next;

};

/* Server */
void recv_msg_global_request_remotetcp();

extern const struct ChanType svr_chan_tcpdirect;

/* Client */
void setup_localtcp();
void setup_remotetcp();
extern const struct ChanType cli_chan_tcpremote;
void cli_recv_msg_request_success();
void cli_recv_msg_request_failure();

/* Common */
int listen_tcpfwd(struct TCPListener* tcpinfo);


#endif
