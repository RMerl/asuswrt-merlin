/* 
   Unix SMB/CIFS implementation.

   SMB2 client request handling

   Copyright (C) Andrew Tridgell	2005
   Copyright (C) Stefan Metzmacher	2005
   
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
#include "libcli/raw/libcliraw.h"
#include "libcli/smb2/smb2.h"
#include "../lib/util/dlinklist.h"
#include "lib/events/events.h"
#include "libcli/smb2/smb2_calls.h"

/* fill in the bufinfo */
void smb2_setup_bufinfo(struct smb2_request *req)
{
	req->in.bufinfo.mem_ctx    = req;
	req->in.bufinfo.flags      = BUFINFO_FLAG_UNICODE | BUFINFO_FLAG_SMB2;
	req->in.bufinfo.align_base = req->in.buffer;
	if (req->in.dynamic) {
		req->in.bufinfo.data       = req->in.dynamic;
		req->in.bufinfo.data_size  = req->in.body_size - req->in.body_fixed;
	} else {
		req->in.bufinfo.data       = NULL;
		req->in.bufinfo.data_size  = 0;
	}
}


/* destroy a request structure */
static int smb2_request_destructor(struct smb2_request *req)
{
	if (req->transport) {
		/* remove it from the list of pending requests (a null op if
		   its not in the list) */
		DLIST_REMOVE(req->transport->pending_recv, req);
	}
	return 0;
}

/*
  initialise a smb2 request
*/
struct smb2_request *smb2_request_init(struct smb2_transport *transport, uint16_t opcode,
				       uint16_t body_fixed_size, bool body_dynamic_present,
				       uint32_t body_dynamic_size)
{
	struct smb2_request *req;
	uint64_t seqnum;
	uint32_t hdr_offset;
	uint32_t flags = 0;
	bool compound = false;

	if (body_dynamic_present) {
		if (body_dynamic_size == 0) {
			body_dynamic_size = 1;
		}
	} else {
		body_dynamic_size = 0;
	}

	req = talloc(transport, struct smb2_request);
	if (req == NULL) return NULL;

	seqnum = transport->seqnum;
	if (transport->credits.charge > 0) {
		transport->seqnum += transport->credits.charge;
	} else {
		transport->seqnum += 1;
	}

	req->state     = SMB2_REQUEST_INIT;
	req->transport = transport;
	req->session   = NULL;
	req->tree      = NULL;
	req->seqnum    = seqnum;
	req->status    = NT_STATUS_OK;
	req->async.fn  = NULL;
	req->next = req->prev = NULL;

	ZERO_STRUCT(req->cancel);
	ZERO_STRUCT(req->in);

	if (transport->compound.missing > 0) {
		compound = true;
		transport->compound.missing -= 1;
		req->out = transport->compound.buffer;
		ZERO_STRUCT(transport->compound.buffer);
		if (transport->compound.related) {
			flags |= SMB2_HDR_FLAG_CHAINED;
		}
	} else {
		ZERO_STRUCT(req->out);
	}

	if (req->out.size > 0) {
		hdr_offset = req->out.size;
	} else {
		hdr_offset = NBT_HDR_SIZE;
	}

	req->out.size      = hdr_offset + SMB2_HDR_BODY + body_fixed_size;
	req->out.allocated = req->out.size + body_dynamic_size;

	req->out.buffer = talloc_realloc(req, req->out.buffer,
					 uint8_t, req->out.allocated);
	if (req->out.buffer == NULL) {
		talloc_free(req);
		return NULL;
	}

	req->out.hdr       = req->out.buffer + hdr_offset;
	req->out.body      = req->out.hdr + SMB2_HDR_BODY;
	req->out.body_fixed= body_fixed_size;
	req->out.body_size = body_fixed_size;
	req->out.dynamic   = (body_dynamic_size ? req->out.body + body_fixed_size : NULL);

	SIVAL(req->out.hdr, 0,				SMB2_MAGIC);
	SSVAL(req->out.hdr, SMB2_HDR_LENGTH,		SMB2_HDR_BODY);
	SSVAL(req->out.hdr, SMB2_HDR_EPOCH,		transport->credits.charge);
	SIVAL(req->out.hdr, SMB2_HDR_STATUS,		0);
	SSVAL(req->out.hdr, SMB2_HDR_OPCODE,		opcode);
	SSVAL(req->out.hdr, SMB2_HDR_CREDIT,		transport->credits.ask_num);
	SIVAL(req->out.hdr, SMB2_HDR_FLAGS,		flags);
	SIVAL(req->out.hdr, SMB2_HDR_NEXT_COMMAND,	0);
	SBVAL(req->out.hdr, SMB2_HDR_MESSAGE_ID,	req->seqnum);
	SIVAL(req->out.hdr, SMB2_HDR_PID,		0);
	SIVAL(req->out.hdr, SMB2_HDR_TID,		0);
	SBVAL(req->out.hdr, SMB2_HDR_SESSION_ID,		0);
	memset(req->out.hdr+SMB2_HDR_SIGNATURE, 0, 16);

	/* set the length of the fixed body part and +1 if there's a dynamic part also */
	SSVAL(req->out.body, 0, body_fixed_size + (body_dynamic_size?1:0));

	/* 
	 * if we have a dynamic part, make sure the first byte
	 * which is always be part of the packet is initialized
	 */
	if (body_dynamic_size && !compound) {
		req->out.size += 1;
		SCVAL(req->out.dynamic, 0, 0);
	}

	talloc_set_destructor(req, smb2_request_destructor);

	return req;
}

/*
    initialise a smb2 request for tree operations
*/
struct smb2_request *smb2_request_init_tree(struct smb2_tree *tree, uint16_t opcode,
					    uint16_t body_fixed_size, bool body_dynamic_present,
					    uint32_t body_dynamic_size)
{
	struct smb2_request *req = smb2_request_init(tree->session->transport, opcode, 
						     body_fixed_size, body_dynamic_present,
						     body_dynamic_size);
	if (req == NULL) return NULL;

	SBVAL(req->out.hdr,  SMB2_HDR_SESSION_ID, tree->session->uid);
	SIVAL(req->out.hdr,  SMB2_HDR_PID, tree->session->pid);
	SIVAL(req->out.hdr,  SMB2_HDR_TID, tree->tid);
	req->session = tree->session;
	req->tree = tree;

	return req;	
}

/* destroy a request structure and return final status */
NTSTATUS smb2_request_destroy(struct smb2_request *req)
{
	NTSTATUS status;

	/* this is the error code we give the application for when a
	   _send() call fails completely */
	if (!req) return NT_STATUS_UNSUCCESSFUL;

	if (req->state == SMB2_REQUEST_ERROR &&
	    NT_STATUS_IS_OK(req->status)) {
		status = NT_STATUS_INTERNAL_ERROR;
	} else {
		status = req->status;
	}

	talloc_free(req);
	return status;
}

/*
  receive a response to a packet
*/
bool smb2_request_receive(struct smb2_request *req)
{
	/* req can be NULL when a send has failed. This eliminates lots of NULL
	   checks in each module */
	if (!req) return false;

	/* keep receiving packets until this one is replied to */
	while (req->state <= SMB2_REQUEST_RECV) {
		if (event_loop_once(req->transport->socket->event.ctx) != 0) {
			return false;
		}
	}

	return req->state == SMB2_REQUEST_DONE;
}

/* Return true if the last packet was in error */
bool smb2_request_is_error(struct smb2_request *req)
{
	return NT_STATUS_IS_ERR(req->status);
}

/* Return true if the last packet was OK */
bool smb2_request_is_ok(struct smb2_request *req)
{
	return NT_STATUS_IS_OK(req->status);
}

/*
  check if a range in the reply body is out of bounds
*/
bool smb2_oob(struct smb2_request_buffer *buf, const uint8_t *ptr, size_t size)
{
	if (size == 0) {
		/* zero bytes is never out of range */
		return false;
	}
	/* be careful with wraparound! */
	if ((uintptr_t)ptr < (uintptr_t)buf->body ||
	    (uintptr_t)ptr >= (uintptr_t)buf->body + buf->body_size ||
	    size > buf->body_size ||
	    (uintptr_t)ptr + size > (uintptr_t)buf->body + buf->body_size) {
		return true;
	}
	return false;
}

size_t smb2_padding_size(uint32_t offset, size_t n)
{
	if ((offset & (n-1)) == 0) return 0;
	return n - (offset & (n-1));
}

static size_t smb2_padding_fix(struct smb2_request_buffer *buf)
{
	if (buf->dynamic == (buf->body + buf->body_fixed)) {
		if (buf->dynamic != (buf->buffer + buf->size)) {
			return 1;
		}
	}
	return 0;
}

/*
  grow a SMB2 buffer by the specified amount
*/
NTSTATUS smb2_grow_buffer(struct smb2_request_buffer *buf, size_t increase)
{
	size_t hdr_ofs;
	size_t dynamic_ofs;
	uint8_t *buffer_ptr;
	uint32_t newsize = buf->size + increase;

	/* a packet size should be limited a bit */
	if (newsize >= 0x00FFFFFF) return NT_STATUS_MARSHALL_OVERFLOW;

	if (newsize <= buf->allocated) return NT_STATUS_OK;

	hdr_ofs = buf->hdr - buf->buffer;
	dynamic_ofs = buf->dynamic - buf->buffer;

	buffer_ptr = talloc_realloc(buf, buf->buffer, uint8_t, newsize);
	NT_STATUS_HAVE_NO_MEMORY(buffer_ptr);

	buf->buffer	= buffer_ptr;
	buf->hdr	= buf->buffer + hdr_ofs;
	buf->body	= buf->hdr    + SMB2_HDR_BODY;
	buf->dynamic	= buf->buffer + dynamic_ofs;
	buf->allocated	= newsize;

	return NT_STATUS_OK;
}

/*
  pull a uint16_t ofs/ uint16_t length/blob triple from a data blob
  the ptr points to the start of the offset/length pair
*/
NTSTATUS smb2_pull_o16s16_blob(struct smb2_request_buffer *buf, TALLOC_CTX *mem_ctx, uint8_t *ptr, DATA_BLOB *blob)
{
	uint16_t ofs, size;
	if (smb2_oob(buf, ptr, 4)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	ofs  = SVAL(ptr, 0);
	size = SVAL(ptr, 2);
	if (ofs == 0) {
		*blob = data_blob(NULL, 0);
		return NT_STATUS_OK;
	}
	if (smb2_oob(buf, buf->hdr + ofs, size)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	*blob = data_blob_talloc(mem_ctx, buf->hdr + ofs, size);
	NT_STATUS_HAVE_NO_MEMORY(blob->data);
	return NT_STATUS_OK;
}

/*
  push a uint16_t ofs/ uint16_t length/blob triple into a data blob
  the ofs points to the start of the offset/length pair, and is relative
  to the body start
*/
NTSTATUS smb2_push_o16s16_blob(struct smb2_request_buffer *buf, 
			       uint16_t ofs, DATA_BLOB blob)
{
	NTSTATUS status;
	size_t offset;
	size_t padding_length;
	size_t padding_fix;
	uint8_t *ptr = buf->body+ofs;

	if (buf->dynamic == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* we have only 16 bit for the size */
	if (blob.length > 0xFFFF) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* check if there're enough room for ofs and size */
	if (smb2_oob(buf, ptr, 4)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (blob.data == NULL) {
		if (blob.length != 0) {
			return NT_STATUS_INTERNAL_ERROR;
		}
		SSVAL(ptr, 0, 0);
		SSVAL(ptr, 2, 0);
		return NT_STATUS_OK;
	}

	offset = buf->dynamic - buf->hdr;
	padding_length = smb2_padding_size(offset, 2);
	offset += padding_length;
	padding_fix = smb2_padding_fix(buf);

	SSVAL(ptr, 0, offset);
	SSVAL(ptr, 2, blob.length);

	status = smb2_grow_buffer(buf, blob.length + padding_length - padding_fix);
	NT_STATUS_NOT_OK_RETURN(status);

	memset(buf->dynamic, 0, padding_length);
	buf->dynamic += padding_length;

	memcpy(buf->dynamic, blob.data, blob.length);
	buf->dynamic += blob.length;

	buf->size += blob.length + padding_length - padding_fix;
	buf->body_size += blob.length + padding_length;

	return NT_STATUS_OK;
}


/*
  push a uint16_t ofs/ uint32_t length/blob triple into a data blob
  the ofs points to the start of the offset/length pair, and is relative
  to the body start
*/
NTSTATUS smb2_push_o16s32_blob(struct smb2_request_buffer *buf, 
			       uint16_t ofs, DATA_BLOB blob)
{
	NTSTATUS status;
	size_t offset;
	size_t padding_length;
	size_t padding_fix;
	uint8_t *ptr = buf->body+ofs;

	if (buf->dynamic == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* check if there're enough room for ofs and size */
	if (smb2_oob(buf, ptr, 6)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (blob.data == NULL) {
		if (blob.length != 0) {
			return NT_STATUS_INTERNAL_ERROR;
		}
		SSVAL(ptr, 0, 0);
		SIVAL(ptr, 2, 0);
		return NT_STATUS_OK;
	}

	offset = buf->dynamic - buf->hdr;
	padding_length = smb2_padding_size(offset, 2);
	offset += padding_length;
	padding_fix = smb2_padding_fix(buf);

	SSVAL(ptr, 0, offset);
	SIVAL(ptr, 2, blob.length);

	status = smb2_grow_buffer(buf, blob.length + padding_length - padding_fix);
	NT_STATUS_NOT_OK_RETURN(status);

	memset(buf->dynamic, 0, padding_length);
	buf->dynamic += padding_length;

	memcpy(buf->dynamic, blob.data, blob.length);
	buf->dynamic += blob.length;

	buf->size += blob.length + padding_length - padding_fix;
	buf->body_size += blob.length + padding_length;

	return NT_STATUS_OK;
}


/*
  push a uint32_t ofs/ uint32_t length/blob triple into a data blob
  the ofs points to the start of the offset/length pair, and is relative
  to the body start
*/
NTSTATUS smb2_push_o32s32_blob(struct smb2_request_buffer *buf, 
			       uint32_t ofs, DATA_BLOB blob)
{
	NTSTATUS status;
	size_t offset;
	size_t padding_length;
	size_t padding_fix;
	uint8_t *ptr = buf->body+ofs;

	if (buf->dynamic == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* check if there're enough room for ofs and size */
	if (smb2_oob(buf, ptr, 8)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (blob.data == NULL) {
		if (blob.length != 0) {
			return NT_STATUS_INTERNAL_ERROR;
		}
		SIVAL(ptr, 0, 0);
		SIVAL(ptr, 4, 0);
		return NT_STATUS_OK;
	}

	offset = buf->dynamic - buf->hdr;
	padding_length = smb2_padding_size(offset, 8);
	offset += padding_length;
	padding_fix = smb2_padding_fix(buf);

	SIVAL(ptr, 0, offset);
	SIVAL(ptr, 4, blob.length);

	status = smb2_grow_buffer(buf, blob.length + padding_length - padding_fix);
	NT_STATUS_NOT_OK_RETURN(status);

	memset(buf->dynamic, 0, padding_length);
	buf->dynamic += padding_length;

	memcpy(buf->dynamic, blob.data, blob.length);
	buf->dynamic += blob.length;

	buf->size += blob.length + padding_length - padding_fix;
	buf->body_size += blob.length + padding_length;

	return NT_STATUS_OK;
}


/*
  push a uint32_t length/ uint32_t ofs/blob triple into a data blob
  the ofs points to the start of the length/offset pair, and is relative
  to the body start
*/
NTSTATUS smb2_push_s32o32_blob(struct smb2_request_buffer *buf, 
			       uint32_t ofs, DATA_BLOB blob)
{
	NTSTATUS status;
	size_t offset;
	size_t padding_length;
	size_t padding_fix;
	uint8_t *ptr = buf->body+ofs;

	if (buf->dynamic == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* check if there're enough room for ofs and size */
	if (smb2_oob(buf, ptr, 8)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (blob.data == NULL) {
		if (blob.length != 0) {
			return NT_STATUS_INTERNAL_ERROR;
		}
		SIVAL(ptr, 0, 0);
		SIVAL(ptr, 4, 0);
		return NT_STATUS_OK;
	}

	offset = buf->dynamic - buf->hdr;
	padding_length = smb2_padding_size(offset, 8);
	offset += padding_length;
	padding_fix = smb2_padding_fix(buf);

	SIVAL(ptr, 0, blob.length);
	SIVAL(ptr, 4, offset);

	status = smb2_grow_buffer(buf, blob.length + padding_length - padding_fix);
	NT_STATUS_NOT_OK_RETURN(status);

	memset(buf->dynamic, 0, padding_length);
	buf->dynamic += padding_length;

	memcpy(buf->dynamic, blob.data, blob.length);
	buf->dynamic += blob.length;

	buf->size += blob.length + padding_length - padding_fix;
	buf->body_size += blob.length + padding_length;

	return NT_STATUS_OK;
}

/*
  pull a uint16_t ofs/ uint32_t length/blob triple from a data blob
  the ptr points to the start of the offset/length pair
*/
NTSTATUS smb2_pull_o16s32_blob(struct smb2_request_buffer *buf, TALLOC_CTX *mem_ctx, uint8_t *ptr, DATA_BLOB *blob)
{
	uint16_t ofs;
	uint32_t size;

	if (smb2_oob(buf, ptr, 6)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	ofs  = SVAL(ptr, 0);
	size = IVAL(ptr, 2);
	if (ofs == 0) {
		*blob = data_blob(NULL, 0);
		return NT_STATUS_OK;
	}
	if (smb2_oob(buf, buf->hdr + ofs, size)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	*blob = data_blob_talloc(mem_ctx, buf->hdr + ofs, size);
	NT_STATUS_HAVE_NO_MEMORY(blob->data);
	return NT_STATUS_OK;
}

/*
  pull a uint32_t ofs/ uint32_t length/blob triple from a data blob
  the ptr points to the start of the offset/length pair
*/
NTSTATUS smb2_pull_o32s32_blob(struct smb2_request_buffer *buf, TALLOC_CTX *mem_ctx, uint8_t *ptr, DATA_BLOB *blob)
{
	uint32_t ofs, size;
	if (smb2_oob(buf, ptr, 8)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	ofs  = IVAL(ptr, 0);
	size = IVAL(ptr, 4);
	if (ofs == 0) {
		*blob = data_blob(NULL, 0);
		return NT_STATUS_OK;
	}
	if (smb2_oob(buf, buf->hdr + ofs, size)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	*blob = data_blob_talloc(mem_ctx, buf->hdr + ofs, size);
	NT_STATUS_HAVE_NO_MEMORY(blob->data);
	return NT_STATUS_OK;
}

/*
  pull a uint16_t ofs/ uint32_t length/blob triple from a data blob
  the ptr points to the start of the offset/length pair
  
  In this varient the uint16_t is padded by an extra 2 bytes, making
  the size aligned on 4 byte boundary
*/
NTSTATUS smb2_pull_o16As32_blob(struct smb2_request_buffer *buf, TALLOC_CTX *mem_ctx, uint8_t *ptr, DATA_BLOB *blob)
{
	uint32_t ofs, size;
	if (smb2_oob(buf, ptr, 8)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	ofs  = SVAL(ptr, 0);
	size = IVAL(ptr, 4);
	if (ofs == 0) {
		*blob = data_blob(NULL, 0);
		return NT_STATUS_OK;
	}
	if (smb2_oob(buf, buf->hdr + ofs, size)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	*blob = data_blob_talloc(mem_ctx, buf->hdr + ofs, size);
	NT_STATUS_HAVE_NO_MEMORY(blob->data);
	return NT_STATUS_OK;
}

/*
  pull a uint32_t length/ uint32_t ofs/blob triple from a data blob
  the ptr points to the start of the offset/length pair
*/
NTSTATUS smb2_pull_s32o32_blob(struct smb2_request_buffer *buf, TALLOC_CTX *mem_ctx, uint8_t *ptr, DATA_BLOB *blob)
{
	uint32_t ofs, size;
	if (smb2_oob(buf, ptr, 8)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	size = IVAL(ptr, 0);
	ofs  = IVAL(ptr, 4);
	if (ofs == 0) {
		*blob = data_blob(NULL, 0);
		return NT_STATUS_OK;
	}
	if (smb2_oob(buf, buf->hdr + ofs, size)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	*blob = data_blob_talloc(mem_ctx, buf->hdr + ofs, size);
	NT_STATUS_HAVE_NO_MEMORY(blob->data);
	return NT_STATUS_OK;
}

/*
  pull a uint32_t length/ uint16_t ofs/blob triple from a data blob
  the ptr points to the start of the offset/length pair
*/
NTSTATUS smb2_pull_s32o16_blob(struct smb2_request_buffer *buf, TALLOC_CTX *mem_ctx, uint8_t *ptr, DATA_BLOB *blob)
{
	uint32_t ofs, size;
	if (smb2_oob(buf, ptr, 8)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	size = IVAL(ptr, 0);
	ofs  = SVAL(ptr, 4);
	if (ofs == 0) {
		*blob = data_blob(NULL, 0);
		return NT_STATUS_OK;
	}
	if (smb2_oob(buf, buf->hdr + ofs, size)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	*blob = data_blob_talloc(mem_ctx, buf->hdr + ofs, size);
	NT_STATUS_HAVE_NO_MEMORY(blob->data);
	return NT_STATUS_OK;
}

/*
  pull a string in a uint16_t ofs/ uint16_t length/blob format
  UTF-16 without termination
*/
NTSTATUS smb2_pull_o16s16_string(struct smb2_request_buffer *buf, TALLOC_CTX *mem_ctx,
				 uint8_t *ptr, const char **str)
{
	DATA_BLOB blob;
	NTSTATUS status;
	void *vstr;
	bool ret;

	status = smb2_pull_o16s16_blob(buf, mem_ctx, ptr, &blob);
	NT_STATUS_NOT_OK_RETURN(status);

	if (blob.data == NULL) {
		*str = NULL;
		return NT_STATUS_OK;
	}

	if (blob.length == 0) {
		char *s;
		s = talloc_strdup(mem_ctx, "");
		NT_STATUS_HAVE_NO_MEMORY(s);
		*str = s;
		return NT_STATUS_OK;
	}

	ret = convert_string_talloc(mem_ctx, CH_UTF16, CH_UNIX, 
				     blob.data, blob.length, &vstr, NULL, false);
	data_blob_free(&blob);
	(*str) = (char *)vstr;
	if (!ret) {
		return NT_STATUS_ILLEGAL_CHARACTER;
	}
	return NT_STATUS_OK;
}

/*
  push a string in a uint16_t ofs/ uint16_t length/blob format
  UTF-16 without termination
*/
NTSTATUS smb2_push_o16s16_string(struct smb2_request_buffer *buf,
				 uint16_t ofs, const char *str)
{
	DATA_BLOB blob;
	NTSTATUS status;
	bool ret;

	if (str == NULL) {
		return smb2_push_o16s16_blob(buf, ofs, data_blob(NULL, 0));
	}

	if (*str == 0) {
		blob.data = discard_const_p(uint8_t, str);
		blob.length = 0;
		return smb2_push_o16s16_blob(buf, ofs, blob);
	}

	ret = convert_string_talloc(buf->buffer, CH_UNIX, CH_UTF16, 
				     str, strlen(str), (void **)&blob.data, &blob.length, 
					 false);
	if (!ret) {
		return NT_STATUS_ILLEGAL_CHARACTER;
	}

	status = smb2_push_o16s16_blob(buf, ofs, blob);
	data_blob_free(&blob);
	return status;
}

/*
  push a file handle into a buffer
*/
void smb2_push_handle(uint8_t *data, struct smb2_handle *h)
{
	SBVAL(data, 0, h->data[0]);
	SBVAL(data, 8, h->data[1]);
}

/*
  pull a file handle from a buffer
*/
void smb2_pull_handle(uint8_t *ptr, struct smb2_handle *h)
{
	h->data[0] = BVAL(ptr, 0);
	h->data[1] = BVAL(ptr, 8);
}
