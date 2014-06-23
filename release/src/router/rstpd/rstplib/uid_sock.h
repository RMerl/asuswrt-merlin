/************************************************************************ 
 * RSTP library - Rapid Spanning Tree (802.1t, 802.1w) 
 * Copyright (C) 2001-2003 Optical Access 
 * Author: Alex Rozin 
 * 
 * This file is part of RSTP library. 
 * 
 * RSTP library is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU Lesser General Public License as published by the 
 * Free Software Foundation; version 2.1 
 * 
 * RSTP library is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser 
 * General Public License for more details. 
 * 
 * You should have received a copy of the GNU Lesser General Public License 
 * along with RSTP library; see the file COPYING.  If not, write to the Free 
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
 * 02111-1307, USA. 
 **********************************************************************/

#ifndef _UID_SOCKET_H__ 
#define _UID_SOCKET_H__

/* Socket API */

#include        <sys/socket.h>  /* basic socket definitions */
#include        <netinet/in.h>
#include        <linux/un.h>              /* for Unix domain sockets */

#define UID_REPL_PATH   "/tmp/UidSocketFile"

typedef struct sockaddr SA;

#define SOCKET_NAME_LENGTH 108
#define SIZE_OF_ADDRESS sizeof(struct sockaddr_un)

typedef enum {
  UID_BIND_AS_CLIENT,
  UID_BIND_AS_SERVER
} TYPE_OF_BINDING;


typedef char        UID_SOCK_ID[SOCKET_NAME_LENGTH];

typedef struct{
  int           sock_fd;
  struct sockaddr_un    clientAddr;
  struct sockaddr_un    serverAddr; // Only for socket of UID_BIND_AS_CLIENT
  UID_SOCK_ID       socket_id;
  TYPE_OF_BINDING   binding;
} UID_SOCKET_T;

#define MESSAGE_SIZE        2048

int UiD_SocketInit(UID_SOCKET_T* sock,
            UID_SOCK_ID id,
            TYPE_OF_BINDING binding);

int UiD_SocketRecvfrom (UID_SOCKET_T* sock,
            void* msg_buffer,
            int buffer_size,
            UID_SOCKET_T* sock_4_reply);

int UiD_SocketSendto (UID_SOCKET_T* sock,
            void* msg_buffer,
            int buffer_size);

int UiD_SocketClose(UID_SOCKET_T* sock);

int UiD_SocketSetReadTimeout (UID_SOCKET_T* s, int timeout);

int
UiD_SocketCompare (UID_SOCKET_T* s, UID_SOCKET_T* t);

#define GET_FILE_DESCRIPTOR(sock) (((UID_SOCKET_T*)sock)->sock_fd)

#endif /* _UID_SOCKET_H__ */


