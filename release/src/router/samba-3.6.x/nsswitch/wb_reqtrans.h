/*
   Unix SMB/CIFS implementation.
   Headers for the async winbind client library
   Copyright (C) Volker Lendecke 2008

     ** NOTE! The following LGPL license applies to the wbclient
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _WB_REQTRANS_H_
#define _WB_REQTRANS_H_

#include <talloc.h>
#include <tevent.h>
#include "nsswitch/winbind_struct_protocol.h"

struct tevent_req *wb_req_read_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    int fd, size_t max_extra_data);
ssize_t wb_req_read_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			 struct winbindd_request **preq, int *err);

struct tevent_req *wb_req_write_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     struct tevent_queue *queue, int fd,
				     struct winbindd_request *wb_req);
ssize_t wb_req_write_recv(struct tevent_req *req, int *err);

struct tevent_req *wb_resp_read_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev, int fd);
ssize_t wb_resp_read_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			  struct winbindd_response **presp, int *err);

struct tevent_req *wb_resp_write_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct tevent_queue *queue, int fd,
				      struct winbindd_response *wb_resp);
ssize_t wb_resp_write_recv(struct tevent_req *req, int *err);

struct tevent_req *wb_simple_trans_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct tevent_queue *queue, int fd,
					struct winbindd_request *wb_req);
int wb_simple_trans_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			 struct winbindd_response **presponse, int *err);

#endif /*_WB_REQTRANS_H_*/
