/* 
   Unix SMB/CIFS implementation.
   NTVFS utility code
   Copyright (C) Stefan Metzmacher 2004

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
/*
  this implements common utility functions that many NTVFS backends may wish to use
*/

#include "includes.h"
#include "../lib/util/dlinklist.h"
#include "ntvfs/ntvfs.h"


struct ntvfs_request *ntvfs_request_create(struct ntvfs_context *ctx, TALLOC_CTX *mem_ctx,
						    struct auth_session_info *session_info,
						    uint16_t smbpid,
						    struct timeval request_time,
						    void *private_data,
						    void (*send_fn)(struct ntvfs_request *),
						    uint32_t state)
{
	struct ntvfs_request *req;
	struct ntvfs_async_state *async;

	req = talloc(mem_ctx, struct ntvfs_request);
	if (!req) return NULL;
	req->ctx			= ctx;
	req->async_states		= NULL;
	req->session_info		= session_info;
	req->smbpid			= smbpid;
	req->client_caps		= ctx->client_caps;
	req->statistics.request_time	= request_time;

	async = talloc(req, struct ntvfs_async_state);
	if (!async) goto failed;

	async->state		= state;
	async->private_data	= private_data;
	async->send_fn		= send_fn;
	async->status		= NT_STATUS_INTERNAL_ERROR;
	async->ntvfs		= NULL;

	DLIST_ADD(req->async_states, async);

	return req;
failed:
	talloc_free(req);
	return NULL;
}

NTSTATUS ntvfs_async_state_push(struct ntvfs_module_context *ntvfs,
					 struct ntvfs_request *req,
					 void *private_data,
					 void (*send_fn)(struct ntvfs_request *))
{
	struct ntvfs_async_state *async;

	async = talloc(req, struct ntvfs_async_state);
	NT_STATUS_HAVE_NO_MEMORY(async);

	async->state		= req->async_states->state;
	async->private_data	= private_data;
	async->send_fn		= send_fn;
	async->status		= NT_STATUS_INTERNAL_ERROR;

	async->ntvfs		= ntvfs;

	DLIST_ADD(req->async_states, async);

	return NT_STATUS_OK;
}

void ntvfs_async_state_pop(struct ntvfs_request *req)
{
	struct ntvfs_async_state *async;

	async = req->async_states;

	DLIST_REMOVE(req->async_states, async);

	req->async_states->state	= async->state;
	req->async_states->status	= async->status;

	talloc_free(async);
}

NTSTATUS ntvfs_handle_new(struct ntvfs_module_context *ntvfs,
				   struct ntvfs_request *req,
				   struct ntvfs_handle **h)
{
	if (!ntvfs->ctx->handles.create_new) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}
	return ntvfs->ctx->handles.create_new(ntvfs->ctx->handles.private_data, req, h);
}

NTSTATUS ntvfs_handle_set_backend_data(struct ntvfs_handle *h,
						struct ntvfs_module_context *ntvfs,
						TALLOC_CTX *private_data)
{
	struct ntvfs_handle_data *d;
	bool first_time = h->backend_data?false:true;

	for (d=h->backend_data; d; d = d->next) {
		if (d->owner != ntvfs) continue;
		d->private_data = talloc_steal(d, private_data);
		return NT_STATUS_OK;
	}

	d = talloc(h, struct ntvfs_handle_data);
	NT_STATUS_HAVE_NO_MEMORY(d);
	d->owner = ntvfs;
	d->private_data = talloc_steal(d, private_data);

	DLIST_ADD(h->backend_data, d);

	if (first_time) {
		NTSTATUS status;
		status = h->ctx->handles.make_valid(h->ctx->handles.private_data, h);
		NT_STATUS_NOT_OK_RETURN(status);
	}

	return NT_STATUS_OK;
}

void *ntvfs_handle_get_backend_data(struct ntvfs_handle *h,
					     struct ntvfs_module_context *ntvfs)
{
	struct ntvfs_handle_data *d;

	for (d=h->backend_data; d; d = d->next) {
		if (d->owner != ntvfs) continue;
		return d->private_data;
	}

	return NULL;
}

void ntvfs_handle_remove_backend_data(struct ntvfs_handle *h,
					       struct ntvfs_module_context *ntvfs)
{
	struct ntvfs_handle_data *d,*n;

	for (d=h->backend_data; d; d = n) {
		n = d->next;
		if (d->owner != ntvfs) continue;
		DLIST_REMOVE(h->backend_data, d);
		talloc_free(d);
		d = NULL;
	}

	if (h->backend_data) return;

	/* if there's no backend_data anymore, destroy the handle */
	h->ctx->handles.destroy(h->ctx->handles.private_data, h);
}

struct ntvfs_handle *ntvfs_handle_search_by_wire_key(struct ntvfs_module_context *ntvfs,
							      struct ntvfs_request *req,
							      const DATA_BLOB *key)
{
	if (!ntvfs->ctx->handles.search_by_wire_key) {
		return NULL;
	}
	return ntvfs->ctx->handles.search_by_wire_key(ntvfs->ctx->handles.private_data, req, key);
}

DATA_BLOB ntvfs_handle_get_wire_key(struct ntvfs_handle *h, TALLOC_CTX *mem_ctx)
{
	return h->ctx->handles.get_wire_key(h->ctx->handles.private_data, h, mem_ctx);
}

NTSTATUS ntvfs_set_handle_callbacks(struct ntvfs_context *ntvfs,
					     NTSTATUS (*create_new)(void *private_data, struct ntvfs_request *req, struct ntvfs_handle **h),
					     NTSTATUS (*make_valid)(void *private_data, struct ntvfs_handle *h),
					     void (*destroy)(void *private_data, struct ntvfs_handle *h),
					     struct ntvfs_handle *(*search_by_wire_key)(void *private_data, struct ntvfs_request *req, const DATA_BLOB *key),
					     DATA_BLOB (*get_wire_key)(void *private_data, struct ntvfs_handle *handle, TALLOC_CTX *mem_ctx),
					     void *private_data)
{
	ntvfs->handles.create_new		= create_new;
	ntvfs->handles.make_valid		= make_valid;
	ntvfs->handles.destroy			= destroy;
	ntvfs->handles.search_by_wire_key	= search_by_wire_key;
	ntvfs->handles.get_wire_key		= get_wire_key;
	ntvfs->handles.private_data		= private_data;
	return NT_STATUS_OK;
}
