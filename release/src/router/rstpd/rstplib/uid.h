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

#include "uid_sock.h"


typedef enum {
  UID_CNTRL = 0,
  UID_BPDU
} UID_CMD_TYPE_T;

typedef enum {
  UID_PORT_CONNECT,
  UID_PORT_DISCONNECT,
  UID_BRIDGE_SHUTDOWN,
  UID_BRIDGE_HANDSHAKE,
  UID_LAST_DUMMY
} UID_CNTRL_CMD_T;

typedef struct uid_port_control_s {
  UID_CNTRL_CMD_T cmd;
  unsigned long  param1;  
  unsigned long  param2;  
} UID_CNTRL_BODY_T;

typedef struct uid_msg_header_s {
  UID_CMD_TYPE_T    cmd_type;
  long          sender_pid;
  int           destination_port;
  int           source_port;
  size_t        body_len;
} UID_MSG_HEADER_T;

typedef struct uid_msg_s {
  UID_MSG_HEADER_T  header;
  union {
    UID_CNTRL_BODY_T    cntrl;
    char bpdu[64];
  } body;

} UID_MSG_T;

#define MAX_UID_MSG_SIZE    sizeof(UID_MSG_T)

