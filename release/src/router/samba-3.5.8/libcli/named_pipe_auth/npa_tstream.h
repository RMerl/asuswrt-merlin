/*
   Unix SMB/CIFS implementation.

   Copyright (C) Stefan Metzmacher 2009

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

#ifndef NPA_TSTREAM_H
#define NPA_TSTREAM_H

struct tevent_req;
struct tevent_context;
struct netr_SamInfo3;

struct tevent_req *tstream_npa_connect_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct smb_iconv_convenience *smb_iconv_c,
					const char *directory,
					const char *npipe,
					const struct tsocket_address *client,
					const char *client_name_in,
					const struct tsocket_address *server,
					const char *server_name,
					const struct netr_SamInfo3 *info3,
					DATA_BLOB session_key,
					DATA_BLOB delegated_creds);
int _tstream_npa_connect_recv(struct tevent_req *req,
			      int *perrno,
			      TALLOC_CTX *mem_ctx,
			      struct tstream_context **stream,
			      uint16_t *file_type,
			      uint16_t *device_state,
			      uint64_t *allocation_size,
			      const char *location);
#define tstream_npa_connect_recv(req, perrno, mem_ctx, stream, f, d, a) \
	_tstream_npa_connect_recv(req, perrno, mem_ctx, stream, f, d, a, \
				  __location__)

int _tstream_npa_existing_socket(TALLOC_CTX *mem_ctx,
				 int fd,
				 uint16_t file_type,
				 struct tstream_context **_stream,
				 const char *location);
#define tstream_npa_existing_socket(mem_ctx, fd, ft, stream) \
	_tstream_npa_existing_socket(mem_ctx, fd, ft, stream, \
				     __location__)

#endif /* NPA_TSTREAM_H */
