/*
   Unix SMB/CIFS implementation.

   Copyright (C) Stefan Metzmacher 2010

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

#ifndef _CLI_NP_TSTREAM_H_
#define _CLI_NP_TSTREAM_H_

struct tevent_context;
struct tevent_req;
struct cli_state;
struct tstream_context;

struct tevent_req *tstream_cli_np_open_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct cli_state *cli,
					    const char *npipe);
NTSTATUS _tstream_cli_np_open_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   struct tstream_context **_stream,
				   const char *location);
#define tstream_cli_np_open_recv(req, mem_ctx, stream) \
		_tstream_cli_np_open_recv(req, mem_ctx, stream, __location__)

NTSTATUS _tstream_cli_np_existing(TALLOC_CTX *mem_ctx,
				  struct cli_state *cli,
				  uint16_t fnum,
				  struct tstream_context **_stream,
				  const char *location);
#define tstream_cli_np_existing(mem_ctx, cli, npipe, stream) \
	_tstream_cli_np_existing(mem_ctx, cli, npipe, stream, __location__)

bool tstream_is_cli_np(struct tstream_context *stream);

NTSTATUS tstream_cli_np_use_trans(struct tstream_context *stream);

unsigned int tstream_cli_np_set_timeout(struct tstream_context *stream,
					unsigned int timeout);

struct cli_state *tstream_cli_np_get_cli_state(struct tstream_context *stream);

#endif /*  _CLI_NP_TSTREAM_H_ */
