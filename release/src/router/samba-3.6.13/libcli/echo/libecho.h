/*
   Unix SMB/CIFS implementation.

   Echo structures and headers, example async client library

   Copyright (C) 2010 Kai Blin  <kai@samba.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __LIBECHO_H__
#define __LIBECHO_H__

/* The echo port is fixed, so just set a constant. */
#define ECHO_PORT 7

/** Send an echo request to an echo server
 *
 *@param mem_ctx        talloc memory context to use
 *@param ev             tevent context to use
 *@param server_address address of the server as a string
 *@param message        echo message to send
 *@return tevent_req with the active request or NULL on out-of-memory
 */
struct tevent_req *echo_request_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     const char *server_address,
				     const char *message);

/** Get the echo response from an echo server
 *
 * Once the echo_request_send async request is finished, you can call
 * this function to collect the echo reply.
 *
 *@param req      tevent_req struct returned from echo_request_send
 *@param mem_ctx  talloc memory context to use for the reply string
 *@param message  pointer to a string that will be allocated and filled with
                  the echo reply.
 *@return NTSTATUS code depending on the async request result
 */
NTSTATUS echo_request_recv(struct tevent_req *req,
			   TALLOC_CTX *mem_ctx,
			   char **message);

#endif /*__LIBECHO_H__*/
