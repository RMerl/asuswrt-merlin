/* 
   Unix SMB/CIFS implementation.
   Manage smbsrv_handle structures
   Copyright (C) Stefan Metzmacher 2006
   
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

#include "includes.h"
#include "smb_server/smb_server.h"


/****************************************************************************
init the handle structures
****************************************************************************/
NTSTATUS smbsrv_init_handles(struct smbsrv_tcon *tcon, uint32_t limit)
{
	/* 
	 * the idr_* functions take 'int' as limit,
	 * and only work with a max limit 0x00FFFFFF
	 */
	limit &= 0x00FFFFFF;

	tcon->handles.idtree_hid	= idr_init(tcon);
	NT_STATUS_HAVE_NO_MEMORY(tcon->handles.idtree_hid);
	tcon->handles.idtree_limit	= limit;
	tcon->handles.list		= NULL;

	return NT_STATUS_OK;
}

/****************************************************************************
find a handle given a handle id
****************************************************************************/
static struct smbsrv_handle *smbsrv_handle_find(struct smbsrv_handles_context *handles_ctx,
						uint32_t hid, struct timeval request_time)
{
	void *p;
	struct smbsrv_handle *handle;

	if (hid == 0) return NULL;

	if (hid > handles_ctx->idtree_limit) return NULL;

	p = idr_find(handles_ctx->idtree_hid, hid);
	if (!p) return NULL;

	handle = talloc_get_type(p, struct smbsrv_handle);
	if (!handle) return NULL;

	/* only give it away when the ntvfs subsystem has made the handle valid */
	if (!handle->ntvfs) return NULL;

	handle->statistics.last_use_time = request_time;

	return handle;
}

struct smbsrv_handle *smbsrv_smb_handle_find(struct smbsrv_tcon *smb_tcon,
					     uint16_t fnum, struct timeval request_time)
{
	return smbsrv_handle_find(&smb_tcon->handles, fnum, request_time);
}

struct smbsrv_handle *smbsrv_smb2_handle_find(struct smbsrv_tcon *smb_tcon,
					      uint32_t hid, struct timeval request_time)
{
	return smbsrv_handle_find(&smb_tcon->handles, hid, request_time);
}

/*
  destroy a connection structure
*/
static int smbsrv_handle_destructor(struct smbsrv_handle *handle)
{
	struct smbsrv_handles_context *handles_ctx;

	handles_ctx = &handle->tcon->handles;

	idr_remove(handles_ctx->idtree_hid, handle->hid);
	DLIST_REMOVE(handles_ctx->list, handle);
	DLIST_REMOVE(handle->session->handles, &handle->session_item);

	/* tell the ntvfs backend that we are disconnecting */
	if (handle->ntvfs) {
		talloc_free(handle->ntvfs);
		handle->ntvfs = NULL;
	}

	return 0;
}

/*
  find first available handle slot
*/
struct smbsrv_handle *smbsrv_handle_new(struct smbsrv_session *session,
				        struct smbsrv_tcon *tcon,
				        TALLOC_CTX *mem_ctx,
					struct timeval request_time)
{
	struct smbsrv_handles_context *handles_ctx = &tcon->handles;
	struct smbsrv_handle *handle;
	int i;

	handle = talloc_zero(mem_ctx, struct smbsrv_handle);
	if (!handle) return NULL;
	handle->tcon	= tcon;
	handle->session	= session;
	
	i = idr_get_new_above(handles_ctx->idtree_hid, handle, 1, handles_ctx->idtree_limit);
	if (i == -1) {
		DEBUG(1,("ERROR! Out of handle structures\n"));
		goto failed;
	}
	handle->hid = i;
	handle->session_item.handle = handle;

	DLIST_ADD(handles_ctx->list, handle);
	DLIST_ADD(session->handles, &handle->session_item);
	talloc_set_destructor(handle, smbsrv_handle_destructor);

	/* now fill in some statistics */
	handle->statistics.open_time		= request_time;
	handle->statistics.last_use_time	= request_time;

	return handle;

failed:
	talloc_free(handle);
	return NULL;
}
