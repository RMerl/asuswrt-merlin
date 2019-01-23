/*
 *  OpenVPN -- An application to securely tunnel IP networks
 *             over a single TCP/UDP port, with support for SSL/TLS-based
 *             session authentication and key exchange,
 *             packet encryption, packet authentication, and
 *             packet compression.
 *
 *  Copyright (C) 2002-2017 OpenVPN Technologies, Inc. <sales@openvpn.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef PUSH_H
#define PUSH_H

#if P2MP

#include "forward.h"

#define PUSH_MSG_ERROR            0
#define PUSH_MSG_REQUEST          1
#define PUSH_MSG_REPLY            2
#define PUSH_MSG_REQUEST_DEFERRED 3
#define PUSH_MSG_AUTH_FAILURE     4
#define PUSH_MSG_CONTINUATION     5
#define PUSH_MSG_ALREADY_REPLIED  6

int process_incoming_push_request(struct context *c);

int process_incoming_push_msg(struct context *c,
                              const struct buffer *buffer,
                              bool honor_received_options,
                              unsigned int permission_mask,
                              unsigned int *option_types_found);

bool send_push_request(struct context *c);

void receive_auth_failed(struct context *c, const struct buffer *buffer);

void server_pushed_signal(struct context *c, const struct buffer *buffer, const bool restart, const int adv);

void incoming_push_message(struct context *c, const struct buffer *buffer);

#if P2MP_SERVER
void clone_push_list(struct options *o);

void push_option(struct options *o, const char *opt, int msglevel);

void push_options(struct options *o, char **p, int msglevel, struct gc_arena *gc);

void push_reset(struct options *o);

void push_remove_option(struct options *o, const char *p);

void remove_iroutes_from_push_route_list(struct options *o);

void send_auth_failed(struct context *c, const char *client_reason);

void send_restart(struct context *c, const char *kill_msg);

#endif
#endif /* if P2MP */
#endif /* ifndef PUSH_H */
