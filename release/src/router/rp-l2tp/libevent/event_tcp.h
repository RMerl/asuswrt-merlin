/***********************************************************************
*
* event-tcp.h
*
* Event-driven TCP functions to allow for single-threaded "concurrent"
* server.
*
* Copyright (C) 2001 Roaring Penguin Software Inc.
*
* $Id: event_tcp.h 3323 2011-09-21 18:45:48Z lly.dev $
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

#ifndef INCLUDE_EVENT_TCP_H
#define INCLUDE_EVENT_TCP_H 1

#include "event.h"
#include <sys/socket.h>

typedef void (*EventTcpAcceptFunc)(EventSelector *es,
				   int fd);

typedef void (*EventTcpConnectFunc)(EventSelector *es,
				    int fd,
				    int flag,
				    void *data);

typedef void (*EventTcpIOFinishedFunc)(EventSelector *es,
				       int fd,
				       char *buf,
				       int len,
				       int flag,
				       void *data);

#define EVENT_TCP_FLAG_COMPLETE 0
#define EVENT_TCP_FLAG_IOERROR  1
#define EVENT_TCP_FLAG_EOF      2
#define EVENT_TCP_FLAG_TIMEOUT  3

typedef struct EventTcpState_t {
    int socket;
    char *buf;
    char *cur;
    int len;
    int delim;
    EventTcpIOFinishedFunc f;
    EventSelector *es;
    EventHandler *eh;
    void *data;
} EventTcpState;

extern EventHandler *EventTcp_CreateAcceptor(EventSelector *es,
					     int socket,
					     EventTcpAcceptFunc f);

extern void EventTcp_Connect(EventSelector *es,
			     int fd,
			     struct sockaddr const *addr,
			     socklen_t addrlen,
			     EventTcpConnectFunc f,
			     int timeout,
			     void *data);

extern EventTcpState *EventTcp_ReadBuf(EventSelector *es,
				       int socket,
				       int len,
				       int delim,
				       EventTcpIOFinishedFunc f,
				       int timeout,
				       void *data);

extern EventTcpState *EventTcp_WriteBuf(EventSelector *es,
					int socket,
					char *buf,
					int len,
					EventTcpIOFinishedFunc f,
					int timeout,
					void *data);

extern void EventTcp_CancelPending(EventTcpState *s);

#endif
