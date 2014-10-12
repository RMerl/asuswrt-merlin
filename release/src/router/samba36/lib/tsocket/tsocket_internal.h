/*
   Unix SMB/CIFS implementation.

   Copyright (C) Stefan Metzmacher 2009

     ** NOTE! The following LGPL license applies to the tsocket
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _TSOCKET_INTERNAL_H
#define _TSOCKET_INTERNAL_H

#include <unistd.h>
#include <sys/uio.h>

struct tsocket_address_ops {
	const char *name;

	char *(*string)(const struct tsocket_address *addr,
			TALLOC_CTX *mem_ctx);

	struct tsocket_address *(*copy)(const struct tsocket_address *addr,
					TALLOC_CTX *mem_ctx,
					const char *location);
};

struct tsocket_address {
	const char *location;
	const struct tsocket_address_ops *ops;

	void *private_data;
};

struct tsocket_address *_tsocket_address_create(TALLOC_CTX *mem_ctx,
					const struct tsocket_address_ops *ops,
					void *pstate,
					size_t psize,
					const char *type,
					const char *location);
#define tsocket_address_create(mem_ctx, ops, state, type, location) \
	_tsocket_address_create(mem_ctx, ops, state, sizeof(type), \
				#type, location)

struct tdgram_context_ops {
	const char *name;

	struct tevent_req *(*recvfrom_send)(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct tdgram_context *dgram);
	ssize_t (*recvfrom_recv)(struct tevent_req *req,
				 int *perrno,
				 TALLOC_CTX *mem_ctx,
				 uint8_t **buf,
				 struct tsocket_address **src);

	struct tevent_req *(*sendto_send)(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct tdgram_context *dgram,
					  const uint8_t *buf, size_t len,
					  const struct tsocket_address *dst);
	ssize_t (*sendto_recv)(struct tevent_req *req,
			       int *perrno);

	struct tevent_req *(*disconnect_send)(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct tdgram_context *dgram);
	int (*disconnect_recv)(struct tevent_req *req,
			       int *perrno);
};

struct tdgram_context *_tdgram_context_create(TALLOC_CTX *mem_ctx,
					const struct tdgram_context_ops *ops,
					void *pstate,
					size_t psize,
					const char *type,
					const char *location);
#define tdgram_context_create(mem_ctx, ops, state, type, location) \
	_tdgram_context_create(mem_ctx, ops, state, sizeof(type), \
				#type, location)

void *_tdgram_context_data(struct tdgram_context *dgram);
#define tdgram_context_data(_req, _type) \
	talloc_get_type_abort(_tdgram_context_data(_req), _type)

struct tstream_context_ops {
	const char *name;

	ssize_t (*pending_bytes)(struct tstream_context *stream);

	struct tevent_req *(*readv_send)(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 struct tstream_context *stream,
					 struct iovec *vector,
					 size_t count);
	int (*readv_recv)(struct tevent_req *req,
			  int *perrno);

	struct tevent_req *(*writev_send)(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct tstream_context *stream,
					  const struct iovec *vector,
					  size_t count);
	int (*writev_recv)(struct tevent_req *req,
			   int *perrno);

	struct tevent_req *(*disconnect_send)(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct tstream_context *stream);
	int (*disconnect_recv)(struct tevent_req *req,
			       int *perrno);
};

struct tstream_context *_tstream_context_create(TALLOC_CTX *mem_ctx,
					const struct tstream_context_ops *ops,
					void *pstate,
					size_t psize,
					const char *type,
					const char *location);
#define tstream_context_create(mem_ctx, ops, state, type, location) \
	_tstream_context_create(mem_ctx, ops, state, sizeof(type), \
				#type, location)

void *_tstream_context_data(struct tstream_context *stream);
#define tstream_context_data(_req, _type) \
	talloc_get_type_abort(_tstream_context_data(_req), _type)

int tsocket_simple_int_recv(struct tevent_req *req, int *perrno);

#endif /* _TSOCKET_H */

