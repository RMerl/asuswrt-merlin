/*
 * $Id: rend-unix.h 801 2006-03-05 08:09:27Z rpedde $
 * General unix rendezvous routines
 *
 * Copyright (C) 2003 Ron Pedde (ron@pedde.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _REND_UNIX_H_
#define _REND_UNIX_H_

#define MAX_NAME_LEN 200
/* Is there a posixly correct constant for this? */
#define MAX_IFACE_NAME_LEN 20
#define MAX_TEXT_LEN 255

typedef struct tag_rend_message {
    int cmd;
    int port;
    char name[MAX_NAME_LEN];
    char type[MAX_NAME_LEN];
    char iface[MAX_IFACE_NAME_LEN];
    char txt[MAX_TEXT_LEN];
} REND_MESSAGE;

#define REND_MSG_TYPE_REGISTER     0
#define REND_MSG_TYPE_UNREGISTER   1
#define REND_MSG_TYPE_STOP         2
#define REND_MSG_TYPE_STATUS       3

#define RD_SIDE 0
#define WR_SIDE 1

extern int rend_pipe_to[2];
extern int rend_pipe_from[2];

extern int rend_send_message(REND_MESSAGE *pmsg);
extern int rend_send_response(int value);
extern int rend_private_init(char *user);
extern int rend_read_message(REND_MESSAGE *pmsg);

#endif /* _REND_UNIX_H_ */

