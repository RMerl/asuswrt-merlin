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

#ifndef _CHANNEL_H_
#define _CHANNEL_H_

#include "includes.h"
#include "buffer.h"
#include "circbuffer.h"

#define SSH_OPEN_ADMINISTRATIVELY_PROHIBITED    1
#define SSH_OPEN_CONNECT_FAILED                 2
#define SSH_OPEN_UNKNOWN_CHANNEL_TYPE           3
#define SSH_OPEN_RESOURCE_SHORTAGE              4

/* Not a real type */
#define SSH_OPEN_IN_PROGRESS					99

#define CHAN_EXTEND_SIZE 3 /* how many extra slots to add when we need more */

struct ChanType;

enum dropbear_channel_prio {
	DROPBEAR_CHANNEL_PRIO_INTERACTIVE, /* pty shell, x11 */
	DROPBEAR_CHANNEL_PRIO_UNKNOWABLE, /* tcp - can't know what's being forwarded */
	DROPBEAR_CHANNEL_PRIO_BULK, /* the rest - probably scp or something */
	DROPBEAR_CHANNEL_PRIO_EARLY, /* channel is still being set up */
};

struct Channel {

	unsigned int index; /* the local channel index */
	unsigned int remotechan;
	unsigned int recvwindow, transwindow;
	unsigned int recvdonelen;
	unsigned int recvmaxpacket, transmaxpacket;
	void* typedata; /* a pointer to type specific data */
	int writefd; /* read from wire, written to insecure side */
	int readfd; /* read from insecure side, written to wire */
	int errfd; /* used like writefd or readfd, depending if it's client or server.
				  Doesn't exactly belong here, but is cleaner here */
	circbuffer *writebuf; /* data from the wire, for local consumption. Can be
							 initially NULL */
	circbuffer *extrabuf; /* extended-data for the program - used like writebuf
					     but for stderr */

	/* whether close/eof messages have been exchanged */
	int sent_close, recv_close;
	int recv_eof, sent_eof;

	/* Set after running the ChanType-specific close hander
	 * to ensure we don't run it twice (nor type->checkclose()). */
	int close_handler_done;

	int initconn; /* used for TCP forwarding, whether the channel has been
					 fully initialised */

	int await_open; /* flag indicating whether we've sent an open request
					   for this channel (and are awaiting a confirmation
					   or failure). */

	int flushing;

	/* Used by client chansession to handle ~ escaping, NULL ignored otherwise */
	void (*read_mangler)(struct Channel*, unsigned char* bytes, int *len);

	const struct ChanType* type;

	enum dropbear_channel_prio prio;
};

struct ChanType {

	int sepfds; /* Whether this channel has seperate pipes for in/out or not */
	char *name;
	int (*inithandler)(struct Channel*);
	int (*check_close)(struct Channel*);
	void (*reqhandler)(struct Channel*);
	void (*closehandler)(struct Channel*);
};

void chaninitialise(const struct ChanType *chantypes[]);
void chancleanup();
void setchannelfds(fd_set *readfd, fd_set *writefd);
void channelio(fd_set *readfd, fd_set *writefd);
struct Channel* getchannel();
/* Returns an arbitrary channel that is in a ready state - not
being initialised and no EOF in either direction. NULL if none. */
struct Channel* get_any_ready_channel();

void recv_msg_channel_open();
void recv_msg_channel_request();
void send_msg_channel_failure(struct Channel *channel);
void send_msg_channel_success(struct Channel *channel);
void recv_msg_channel_data();
void recv_msg_channel_extended_data();
void recv_msg_channel_window_adjust();
void recv_msg_channel_close();
void recv_msg_channel_eof();

void common_recv_msg_channel_data(struct Channel *channel, int fd, 
		circbuffer * buf);

#ifdef DROPBEAR_CLIENT
extern const struct ChanType clichansess;
#endif

#if defined(USING_LISTENERS) || defined(DROPBEAR_CLIENT)
int send_msg_channel_open_init(int fd, const struct ChanType *type);
void recv_msg_channel_open_confirmation();
void recv_msg_channel_open_failure();
#endif
void start_send_channel_request(struct Channel *channel, unsigned char *type);

void send_msg_request_success();
void send_msg_request_failure();


#endif /* _CHANNEL_H_ */
