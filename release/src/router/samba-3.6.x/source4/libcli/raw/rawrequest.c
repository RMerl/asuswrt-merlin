/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Andrew Tridgell  2003
   Copyright (C) James Myers 2003 <myersjj@samba.org>
   
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
  this file implements functions for manipulating the 'struct smbcli_request' structure in libsmb
*/

#include "includes.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "../lib/util/dlinklist.h"
#include "lib/events/events.h"
#include "librpc/ndr/libndr.h"
#include "librpc/gen_ndr/ndr_misc.h"

/* we over allocate the data buffer to prevent too many realloc calls */
#define REQ_OVER_ALLOCATION 0

/* assume that a character will not consume more than 3 bytes per char */
#define MAX_BYTES_PER_CHAR 3

/* setup the bufinfo used for strings and range checking */
void smb_setup_bufinfo(struct smbcli_request *req)
{
	req->in.bufinfo.mem_ctx    = req;
	req->in.bufinfo.flags      = 0;
	if (req->flags2 & FLAGS2_UNICODE_STRINGS) {
		req->in.bufinfo.flags = BUFINFO_FLAG_UNICODE;
	}
	req->in.bufinfo.align_base = req->in.buffer;
	req->in.bufinfo.data       = req->in.data;
	req->in.bufinfo.data_size  = req->in.data_size;
}


/* destroy a request structure and return final status */
_PUBLIC_ NTSTATUS smbcli_request_destroy(struct smbcli_request *req)
{
	NTSTATUS status;

	/* this is the error code we give the application for when a
	   _send() call fails completely */
	if (!req) return NT_STATUS_UNSUCCESSFUL;

	if (req->transport) {
		/* remove it from the list of pending requests (a null op if
		   its not in the list) */
		DLIST_REMOVE(req->transport->pending_recv, req);
	}

	if (req->state == SMBCLI_REQUEST_ERROR &&
	    NT_STATUS_IS_OK(req->status)) {
		req->status = NT_STATUS_INTERNAL_ERROR;
	}

	status = req->status;

	if (!req->do_not_free) {
		talloc_free(req);
	}

	return status;
}


/*
  low-level function to setup a request buffer for a non-SMB packet 
  at the transport level
*/
struct smbcli_request *smbcli_request_setup_nonsmb(struct smbcli_transport *transport, size_t size)
{
	struct smbcli_request *req;

	req = talloc(transport, struct smbcli_request);
	if (!req) {
		return NULL;
	}
	ZERO_STRUCTP(req);

	/* setup the request context */
	req->state = SMBCLI_REQUEST_INIT;
	req->transport = transport;
	req->session = NULL;
	req->tree = NULL;
	req->out.size = size;

	/* over allocate by a small amount */
	req->out.allocated = req->out.size + REQ_OVER_ALLOCATION; 

	req->out.buffer = talloc_array(req, uint8_t, req->out.allocated);
	if (!req->out.buffer) {
		return NULL;
	}

	SIVAL(req->out.buffer, 0, 0);

	return req;
}


/*
  setup a SMB packet at transport level
*/
struct smbcli_request *smbcli_request_setup_transport(struct smbcli_transport *transport,
						      uint8_t command, unsigned int wct, unsigned int buflen)
{
	struct smbcli_request *req;

	req = smbcli_request_setup_nonsmb(transport, NBT_HDR_SIZE + MIN_SMB_SIZE + wct*2 + buflen);

	if (!req) return NULL;
	
	req->out.hdr = req->out.buffer + NBT_HDR_SIZE;
	req->out.vwv = req->out.hdr + HDR_VWV;
	req->out.wct = wct;
	req->out.data = req->out.vwv + VWV(wct) + 2;
	req->out.data_size = buflen;
	req->out.ptr = req->out.data;

	SCVAL(req->out.hdr, HDR_WCT, wct);
	SSVAL(req->out.vwv, VWV(wct), buflen);

	memcpy(req->out.hdr, "\377SMB", 4);
	SCVAL(req->out.hdr,HDR_COM,command);

	SCVAL(req->out.hdr,HDR_FLG, FLAG_CASELESS_PATHNAMES);
	SSVAL(req->out.hdr,HDR_FLG2, 0);

	if (command != SMBtranss && command != SMBtranss2) {
		/* assign a mid */
		req->mid = smbcli_transport_next_mid(transport);
	}

	/* copy the pid, uid and mid to the request */
	SSVAL(req->out.hdr, HDR_PID, 0);
	SSVAL(req->out.hdr, HDR_UID, 0);
	SSVAL(req->out.hdr, HDR_MID, req->mid);
	SSVAL(req->out.hdr, HDR_TID,0);
	SSVAL(req->out.hdr, HDR_PIDHIGH,0);
	SIVAL(req->out.hdr, HDR_RCLS, 0);
	memset(req->out.hdr+HDR_SS_FIELD, 0, 10);
	
	return req;
}

/*
  setup a reply in req->out with the given word count and initial data
  buffer size.  the caller will then fill in the command words and
  data before calling smbcli_request_send() to send the reply on its
  way. This interface is used before a session is setup.
*/
struct smbcli_request *smbcli_request_setup_session(struct smbcli_session *session,
						    uint8_t command, unsigned int wct, size_t buflen)
{
	struct smbcli_request *req;

	req = smbcli_request_setup_transport(session->transport, command, wct, buflen);

	if (!req) return NULL;

	req->session = session;

	SSVAL(req->out.hdr, HDR_FLG2, session->flags2);
	SSVAL(req->out.hdr, HDR_PID, session->pid & 0xFFFF);
	SSVAL(req->out.hdr, HDR_PIDHIGH, session->pid >> 16);
	SSVAL(req->out.hdr, HDR_UID, session->vuid);
	
	return req;
}

/*
  setup a request for tree based commands
*/
struct smbcli_request *smbcli_request_setup(struct smbcli_tree *tree,
					    uint8_t command, 
					    unsigned int wct, unsigned int buflen)
{
	struct smbcli_request *req;

	req = smbcli_request_setup_session(tree->session, command, wct, buflen);
	if (req) {
		req->tree = tree;
		SSVAL(req->out.hdr,HDR_TID,tree->tid);
	}
	return req;
}


/*
  grow the allocation of the data buffer portion of a reply
  packet. Note that as this can reallocate the packet buffer this
  invalidates any local pointers into the packet.

  To cope with this req->out.ptr is supplied. This will be updated to
  point at the same offset into the packet as before this call
*/
static void smbcli_req_grow_allocation(struct smbcli_request *req, unsigned int new_size)
{
	int delta;
	uint8_t *buf2;

	delta = new_size - req->out.data_size;
	if (delta + req->out.size <= req->out.allocated) {
		/* it fits in the preallocation */
		return;
	}

	/* we need to realloc */
	req->out.allocated = req->out.size + delta + REQ_OVER_ALLOCATION;
	buf2 = talloc_realloc(req, req->out.buffer, uint8_t, req->out.allocated);
	if (buf2 == NULL) {
		smb_panic("out of memory in req_grow_allocation");
	}

	if (buf2 == req->out.buffer) {
		/* the malloc library gave us the same pointer */
		return;
	}
	
	/* update the pointers into the packet */
	req->out.data = buf2 + PTR_DIFF(req->out.data, req->out.buffer);
	req->out.ptr  = buf2 + PTR_DIFF(req->out.ptr,  req->out.buffer);
	req->out.vwv  = buf2 + PTR_DIFF(req->out.vwv,  req->out.buffer);
	req->out.hdr  = buf2 + PTR_DIFF(req->out.hdr,  req->out.buffer);

	req->out.buffer = buf2;
}


/*
  grow the data buffer portion of a reply packet. Note that as this
  can reallocate the packet buffer this invalidates any local pointers
  into the packet. 

  To cope with this req->out.ptr is supplied. This will be updated to
  point at the same offset into the packet as before this call
*/
static void smbcli_req_grow_data(struct smbcli_request *req, unsigned int new_size)
{
	int delta;

	smbcli_req_grow_allocation(req, new_size);

	delta = new_size - req->out.data_size;

	req->out.size += delta;
	req->out.data_size += delta;

	/* set the BCC to the new data size */
	SSVAL(req->out.vwv, VWV(req->out.wct), new_size);
}


/*
  setup a chained reply in req->out with the given word count and
  initial data buffer size.
*/
NTSTATUS smbcli_chained_request_setup(struct smbcli_request *req,
				      uint8_t command, 
				      unsigned int wct, size_t buflen)
{
	unsigned int new_size = 1 + (wct*2) + 2 + buflen;

	SSVAL(req->out.vwv, VWV(0), command);
	SSVAL(req->out.vwv, VWV(1), req->out.size - NBT_HDR_SIZE);

	smbcli_req_grow_allocation(req, req->out.data_size + new_size);

	req->out.vwv = req->out.buffer + req->out.size + 1;
	SCVAL(req->out.vwv, -1, wct);
	SSVAL(req->out.vwv, VWV(wct), buflen);

	req->out.size += new_size;
	req->out.data_size += new_size;

	return NT_STATUS_OK;
}

/*
  aadvance to the next chained reply in a request
*/
NTSTATUS smbcli_chained_advance(struct smbcli_request *req)
{
	uint8_t *buffer;

	if (CVAL(req->in.vwv, VWV(0)) == SMB_CHAIN_NONE) {
		return NT_STATUS_NOT_FOUND;
	}

	buffer = req->in.hdr + SVAL(req->in.vwv, VWV(1));

	if (buffer + 3 > req->in.buffer + req->in.size) {
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	req->in.vwv = buffer + 1;
	req->in.wct = CVAL(buffer, 0);
	if (buffer + 3 + req->in.wct*2 > req->in.buffer + req->in.size) {
		return NT_STATUS_BUFFER_TOO_SMALL;
	}
	req->in.data = req->in.vwv + 2 + req->in.wct * 2;
	req->in.data_size = SVAL(req->in.vwv, VWV(req->in.wct));

	/* fix the bufinfo */
	smb_setup_bufinfo(req);

	if (buffer + 3 + req->in.wct*2 + req->in.data_size > 
	    req->in.buffer + req->in.size) {
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	return NT_STATUS_OK;
}


/*
  send a message
*/
bool smbcli_request_send(struct smbcli_request *req)
{
	if (IVAL(req->out.buffer, 0) == 0) {
		_smb_setlen(req->out.buffer, req->out.size - NBT_HDR_SIZE);
	}

	smbcli_request_calculate_sign_mac(req);

	smbcli_transport_send(req);

	return true;
}


/*
  receive a response to a packet
*/
bool smbcli_request_receive(struct smbcli_request *req)
{
	/* req can be NULL when a send has failed. This eliminates lots of NULL
	   checks in each module */
	if (!req) return false;

	/* keep receiving packets until this one is replied to */
	while (req->state <= SMBCLI_REQUEST_RECV) {
		if (event_loop_once(req->transport->socket->event.ctx) != 0) {
			return false;
		}
	}

	return req->state == SMBCLI_REQUEST_DONE;
}


/*
  handle oplock break requests from the server - return true if the request was
  an oplock break
*/
bool smbcli_handle_oplock_break(struct smbcli_transport *transport, unsigned int len, const uint8_t *hdr, const uint8_t *vwv)
{
	/* we must be very fussy about what we consider an oplock break to avoid
	   matching readbraw replies */
	if (len != MIN_SMB_SIZE + VWV(8) + NBT_HDR_SIZE ||
	    (CVAL(hdr, HDR_FLG) & FLAG_REPLY) ||
	    CVAL(hdr,HDR_COM) != SMBlockingX ||
	    SVAL(hdr, HDR_MID) != 0xFFFF ||
	    SVAL(vwv,VWV(6)) != 0 ||
	    SVAL(vwv,VWV(7)) != 0) {
		return false;
	}

	if (transport->oplock.handler) {
		uint16_t tid = SVAL(hdr, HDR_TID);
		uint16_t fnum = SVAL(vwv,VWV(2));
		uint8_t level = CVAL(vwv,VWV(3)+1);
		transport->oplock.handler(transport, tid, fnum, level, transport->oplock.private_data);
	}

	return true;
}

/*
  wait for a reply to be received for a packet that just returns an error
  code and nothing more
*/
_PUBLIC_ NTSTATUS smbcli_request_simple_recv(struct smbcli_request *req)
{
	(void) smbcli_request_receive(req);
	return smbcli_request_destroy(req);
}


/* Return true if the last packet was in error */
bool smbcli_request_is_error(struct smbcli_request *req)
{
	return NT_STATUS_IS_ERR(req->status);
}

/*
  append a string into the data portion of the request packet

  return the number of bytes added to the packet
*/
size_t smbcli_req_append_string(struct smbcli_request *req, const char *str, unsigned int flags)
{
	size_t len;

	/* determine string type to use */
	if (!(flags & (STR_ASCII|STR_UNICODE))) {
		flags |= (req->transport->negotiate.capabilities & CAP_UNICODE) ? STR_UNICODE : STR_ASCII;
	}

	len = (strlen(str)+2) * MAX_BYTES_PER_CHAR;		

	smbcli_req_grow_allocation(req, len + req->out.data_size);

	len = push_string(req->out.data + req->out.data_size, str, len, flags);

	smbcli_req_grow_data(req, len + req->out.data_size);

	return len;
}


/*
  this is like smbcli_req_append_string but it also return the
  non-terminated string byte length, which can be less than the number
  of bytes consumed in the packet for 2 reasons:

   1) the string in the packet may be null terminated
   2) the string in the packet may need a 1 byte UCS2 alignment

 this is used in places where the non-terminated string byte length is
 placed in the packet as a separate field  
*/
size_t smbcli_req_append_string_len(struct smbcli_request *req, const char *str, unsigned int flags, int *len)
{
	int diff = 0;
	size_t ret;

	/* determine string type to use */
	if (!(flags & (STR_ASCII|STR_UNICODE))) {
		flags |= (req->transport->negotiate.capabilities & CAP_UNICODE) ? STR_UNICODE : STR_ASCII;
	}

	/* see if an alignment byte will be used */
	if ((flags & STR_UNICODE) && !(flags & STR_NOALIGN)) {
		diff = ucs2_align(NULL, req->out.data + req->out.data_size, flags);
	}

	/* do the hard work */
	ret = smbcli_req_append_string(req, str, flags);

	/* see if we need to subtract the termination */
	if (flags & STR_TERMINATE) {
		diff += (flags & STR_UNICODE) ? 2 : 1;
	}

	if (ret >= diff) {
		(*len) = ret - diff;
	} else {
		(*len) = ret;
	}

	return ret;
}


/*
  push a string into the data portion of the request packet, growing it if necessary
  this gets quite tricky - please be very careful to cover all cases when modifying this

  if dest is NULL, then put the string at the end of the data portion of the packet

  if dest_len is -1 then no limit applies
*/
size_t smbcli_req_append_ascii4(struct smbcli_request *req, const char *str, unsigned int flags)
{
	size_t size;
	smbcli_req_append_bytes(req, (const uint8_t *)"\4", 1);
	size = smbcli_req_append_string(req, str, flags);
	return size + 1;
}


/*
  push a blob into the data portion of the request packet, growing it if necessary
  this gets quite tricky - please be very careful to cover all cases when modifying this

  if dest is NULL, then put the blob at the end of the data portion of the packet
*/
size_t smbcli_req_append_blob(struct smbcli_request *req, const DATA_BLOB *blob)
{
	smbcli_req_grow_allocation(req, req->out.data_size + blob->length);
	memcpy(req->out.data + req->out.data_size, blob->data, blob->length);
	smbcli_req_grow_data(req, req->out.data_size + blob->length);
	return blob->length;
}

/*
  append raw bytes into the data portion of the request packet
  return the number of bytes added
*/
size_t smbcli_req_append_bytes(struct smbcli_request *req, const uint8_t *bytes, size_t byte_len)
{
	smbcli_req_grow_allocation(req, byte_len + req->out.data_size);
	memcpy(req->out.data + req->out.data_size, bytes, byte_len);
	smbcli_req_grow_data(req, byte_len + req->out.data_size);
	return byte_len;
}

/*
  append variable block (type 5 buffer) into the data portion of the request packet
  return the number of bytes added
*/
size_t smbcli_req_append_var_block(struct smbcli_request *req, const uint8_t *bytes, uint16_t byte_len)
{
	smbcli_req_grow_allocation(req, byte_len + 3 + req->out.data_size);
	SCVAL(req->out.data + req->out.data_size, 0, 5);
	SSVAL(req->out.data + req->out.data_size, 1, byte_len);		/* add field length */
	if (byte_len > 0) {
		memcpy(req->out.data + req->out.data_size + 3, bytes, byte_len);
	}
	smbcli_req_grow_data(req, byte_len + 3 + req->out.data_size);
	return byte_len + 3;
}


/*
  pull a UCS2 string from a request packet, returning a talloced unix string

  the string length is limited by the 3 things:
   - the data size in the request (end of packet)
   - the passed 'byte_len' if it is not -1
   - the end of string (null termination)

  Note that 'byte_len' is the number of bytes in the packet

  on failure zero is returned and *dest is set to NULL, otherwise the number
  of bytes consumed in the packet is returned
*/
static size_t smbcli_req_pull_ucs2(struct request_bufinfo *bufinfo, TALLOC_CTX *mem_ctx,
				char **dest, const uint8_t *src, int byte_len, unsigned int flags)
{
	int src_len, src_len2, alignment=0;
	bool ret;
	size_t ret_size;

	if (!(flags & STR_NOALIGN) && ucs2_align(bufinfo->align_base, src, flags)) {
		src++;
		alignment=1;
		if (byte_len != -1) {
			byte_len--;
		}
	}

	src_len = bufinfo->data_size - PTR_DIFF(src, bufinfo->data);
	if (src_len < 0) {
		*dest = NULL;
		return 0;
	}
	if (byte_len != -1 && src_len > byte_len) {
		src_len = byte_len;
	}

	src_len2 = utf16_len_n(src, src_len);

	/* ucs2 strings must be at least 2 bytes long */
	if (src_len2 < 2) {
		*dest = NULL;
		return 0;
	}

	ret = convert_string_talloc(mem_ctx, CH_UTF16, CH_UNIX, src, src_len2, (void **)dest, &ret_size, false);
	if (!ret) {
		*dest = NULL;
		return 0;
	}

	return src_len2 + alignment;
}

/*
  pull a ascii string from a request packet, returning a talloced string

  the string length is limited by the 3 things:
   - the data size in the request (end of packet)
   - the passed 'byte_len' if it is not -1
   - the end of string (null termination)

  Note that 'byte_len' is the number of bytes in the packet

  on failure zero is returned and *dest is set to NULL, otherwise the number
  of bytes consumed in the packet is returned
*/
size_t smbcli_req_pull_ascii(struct request_bufinfo *bufinfo, TALLOC_CTX *mem_ctx,
			     char **dest, const uint8_t *src, int byte_len, unsigned int flags)
{
	int src_len, src_len2;
	bool ret;
	size_t ret_size;

	src_len = bufinfo->data_size - PTR_DIFF(src, bufinfo->data);
	if (src_len < 0) {
		*dest = NULL;
		return 0;
	}
	if (byte_len != -1 && src_len > byte_len) {
		src_len = byte_len;
	}
	src_len2 = strnlen((const char *)src, src_len);
	if (src_len2 < src_len - 1) {
		/* include the termination if we didn't reach the end of the packet */
		src_len2++;
	}

	ret = convert_string_talloc(mem_ctx, CH_DOS, CH_UNIX, src, src_len2, (void **)dest, &ret_size, false);

	if (!ret) {
		*dest = NULL;
		return 0;
	}

	return ret_size;
}

/**
  pull a string from a request packet, returning a talloced string

  the string length is limited by the 3 things:
   - the data size in the request (end of packet)
   - the passed 'byte_len' if it is not -1
   - the end of string (null termination)

  Note that 'byte_len' is the number of bytes in the packet

  on failure zero is returned and *dest is set to NULL, otherwise the number
  of bytes consumed in the packet is returned
*/
size_t smbcli_req_pull_string(struct request_bufinfo *bufinfo, TALLOC_CTX *mem_ctx, 
			   char **dest, const uint8_t *src, int byte_len, unsigned int flags)
{
	if (!(flags & STR_ASCII) && 
	    (((flags & STR_UNICODE) || (bufinfo->flags & BUFINFO_FLAG_UNICODE)))) {
		return smbcli_req_pull_ucs2(bufinfo, mem_ctx, dest, src, byte_len, flags);
	}

	return smbcli_req_pull_ascii(bufinfo, mem_ctx, dest, src, byte_len, flags);
}


/**
  pull a DATA_BLOB from a reply packet, returning a talloced blob
  make sure we don't go past end of packet

  if byte_len is -1 then limit the blob only by packet size
*/
DATA_BLOB smbcli_req_pull_blob(struct request_bufinfo *bufinfo, TALLOC_CTX *mem_ctx, const uint8_t *src, int byte_len)
{
	int src_len;

	src_len = bufinfo->data_size - PTR_DIFF(src, bufinfo->data);

	if (src_len < 0) {
		return data_blob(NULL, 0);
	}

	if (byte_len != -1 && src_len > byte_len) {
		src_len = byte_len;
	}

	return data_blob_talloc(mem_ctx, src, src_len);
}

/* check that a lump of data in a request is within the bounds of the data section of
   the packet */
static bool smbcli_req_data_oob(struct request_bufinfo *bufinfo, const uint8_t *ptr, uint32_t count)
{
	/* be careful with wraparound! */
	if ((uintptr_t)ptr < (uintptr_t)bufinfo->data ||
	    (uintptr_t)ptr >= (uintptr_t)bufinfo->data + bufinfo->data_size ||
	    count > bufinfo->data_size ||
	    (uintptr_t)ptr + count > (uintptr_t)bufinfo->data + bufinfo->data_size) {
		return true;
	}
	return false;
}

/*
  pull a lump of data from a request packet

  return false if any part is outside the data portion of the packet
*/
bool smbcli_raw_pull_data(struct request_bufinfo *bufinfo, const uint8_t *src, int len, uint8_t *dest)
{
	if (len == 0) return true;

	if (smbcli_req_data_oob(bufinfo, src, len)) {
		return false;
	}

	memcpy(dest, src, len);
	return true;
}


/*
  put a NTTIME into a packet
*/
void smbcli_push_nttime(void *base, uint16_t offset, NTTIME t)
{
	SBVAL(base, offset, t);
}

/*
  pull a NTTIME from a packet
*/
NTTIME smbcli_pull_nttime(void *base, uint16_t offset)
{
	NTTIME ret = BVAL(base, offset);
	return ret;
}

/**
  pull a UCS2 string from a blob, returning a talloced unix string

  the string length is limited by the 3 things:
   - the data size in the blob
   - the passed 'byte_len' if it is not -1
   - the end of string (null termination)

  Note that 'byte_len' is the number of bytes in the packet

  on failure zero is returned and *dest is set to NULL, otherwise the number
  of bytes consumed in the blob is returned
*/
size_t smbcli_blob_pull_ucs2(TALLOC_CTX* mem_ctx,
			     const DATA_BLOB *blob, const char **dest, 
			     const uint8_t *src, int byte_len, unsigned int flags)
{
	int src_len, src_len2, alignment=0;
	size_t ret_size;
	bool ret;
	char *dest2;

	if (src < blob->data ||
	    src >= (blob->data + blob->length)) {
		*dest = NULL;
		return 0;
	}

	src_len = blob->length - PTR_DIFF(src, blob->data);

	if (byte_len != -1 && src_len > byte_len) {
		src_len = byte_len;
	}

	if (!(flags & STR_NOALIGN) && ucs2_align(blob->data, src, flags)) {
		src++;
		alignment=1;
		src_len--;
	}

	if (src_len < 2) {
		*dest = NULL;
		return 0;
	}

	src_len2 = utf16_len_n(src, src_len);

	ret = convert_string_talloc(mem_ctx, CH_UTF16, CH_UNIX, src, src_len2, (void **)&dest2, &ret_size, false);
	if (!ret) {
		*dest = NULL;
		return 0;
	}
	*dest = dest2;

	return src_len2 + alignment;
}

/**
  pull a ascii string from a blob, returning a talloced string

  the string length is limited by the 3 things:
   - the data size in the blob
   - the passed 'byte_len' if it is not -1
   - the end of string (null termination)

  Note that 'byte_len' is the number of bytes in the blob

  on failure zero is returned and *dest is set to NULL, otherwise the number
  of bytes consumed in the blob is returned
*/
static size_t smbcli_blob_pull_ascii(TALLOC_CTX *mem_ctx,
				     const DATA_BLOB *blob, const char **dest, 
				     const uint8_t *src, int byte_len, unsigned int flags)
{
	int src_len, src_len2;
	size_t ret_size;
	bool ret;
	char *dest2;

	src_len = blob->length - PTR_DIFF(src, blob->data);
	if (src_len < 0) {
		*dest = NULL;
		return 0;
	}
	if (byte_len != -1 && src_len > byte_len) {
		src_len = byte_len;
	}
	src_len2 = strnlen((const char *)src, src_len);

	if (src_len2 < src_len - 1) {
		/* include the termination if we didn't reach the end of the packet */
		src_len2++;
	}

	ret = convert_string_talloc(mem_ctx, CH_DOS, CH_UNIX, src, src_len2, (void **)&dest2, &ret_size, false);

	if (!ret) {
		*dest = NULL;
		return 0;
	}
	*dest = dest2;

	return ret_size;
}

/**
  pull a string from a blob, returning a talloced struct smb_wire_string

  the string length is limited by the 3 things:
   - the data size in the blob
   - length field on the wire
   - the end of string (null termination)

   if STR_LEN8BIT is set in the flags then assume the length field is
   8 bits, instead of 32

  on failure zero is returned and dest->s is set to NULL, otherwise the number
  of bytes consumed in the blob is returned
*/
size_t smbcli_blob_pull_string(struct smbcli_session *session,
			       TALLOC_CTX *mem_ctx,
			       const DATA_BLOB *blob, 
			       struct smb_wire_string *dest, 
			       uint16_t len_offset, uint16_t str_offset, 
			       unsigned int flags)
{
	int extra;
	dest->s = NULL;

	if (!(flags & STR_ASCII)) {
		/* this is here to cope with SMB2 calls using the SMB
		   parsers. SMB2 will pass smbcli_session==NULL, which forces
		   unicode on (as used by SMB2) */
		if (session == NULL) {
			flags |= STR_UNICODE;
		} else if (session->transport->negotiate.capabilities & CAP_UNICODE) {
			flags |= STR_UNICODE;
		}
	}

	if (flags & STR_LEN8BIT) {
		if (len_offset > blob->length-1) {
			return 0;
		}
		dest->private_length = CVAL(blob->data, len_offset);
	} else {
		if (len_offset > blob->length-4) {
			return 0;
		}
		dest->private_length = IVAL(blob->data, len_offset);
	}
	extra = 0;
	dest->s = NULL;
	if (!(flags & STR_ASCII) && (flags & STR_UNICODE)) {
		int align = 0;
		if ((str_offset&1) && !(flags & STR_NOALIGN)) {
			align = 1;
		}
		if (flags & STR_LEN_NOTERM) {
			extra = 2;
		}
		return align + extra + smbcli_blob_pull_ucs2(mem_ctx, blob, &dest->s, 
							  blob->data+str_offset+align, 
							  dest->private_length, flags);
	}

	if (flags & STR_LEN_NOTERM) {
		extra = 1;
	}

	return extra + smbcli_blob_pull_ascii(mem_ctx, blob, &dest->s, 
					   blob->data+str_offset, dest->private_length, flags);
}

/**
  pull a string from a blob, returning a talloced char *

  Currently only used by the UNIX search info level.

  the string length is limited by 2 things:
   - the data size in the blob
   - the end of string (null termination)

  on failure zero is returned and dest->s is set to NULL, otherwise the number
  of bytes consumed in the blob is returned
*/
size_t smbcli_blob_pull_unix_string(struct smbcli_session *session,
			    TALLOC_CTX *mem_ctx,
			    DATA_BLOB *blob, 
			    const char **dest, 
			    uint16_t str_offset, 
			    unsigned int flags)
{
	int extra = 0;
	*dest = NULL;
	
	if (!(flags & STR_ASCII) && 
	    ((flags & STR_UNICODE) || 
	     (session->transport->negotiate.capabilities & CAP_UNICODE))) {
		int align = 0;
		if ((str_offset&1) && !(flags & STR_NOALIGN)) {
			align = 1;
		}
		if (flags & STR_LEN_NOTERM) {
			extra = 2;
		}
		return align + extra + smbcli_blob_pull_ucs2(mem_ctx, blob, dest, 
							  blob->data+str_offset+align, 
							  -1, flags);
	}

	if (flags & STR_LEN_NOTERM) {
		extra = 1;
	}

	return extra + smbcli_blob_pull_ascii(mem_ctx, blob, dest,
					   blob->data+str_offset, -1, flags);
}


/*
  append a string into a blob
*/
size_t smbcli_blob_append_string(struct smbcli_session *session,
			      TALLOC_CTX *mem_ctx, DATA_BLOB *blob, 
			      const char *str, unsigned int flags)
{
	size_t max_len;
	int len;

	if (!str) return 0;

	/* determine string type to use */
	if (!(flags & (STR_ASCII|STR_UNICODE))) {
		flags |= (session->transport->negotiate.capabilities & CAP_UNICODE) ? STR_UNICODE : STR_ASCII;
	}

	max_len = (strlen(str)+2) * MAX_BYTES_PER_CHAR;		

	blob->data = talloc_realloc(mem_ctx, blob->data, uint8_t, blob->length + max_len);
	if (!blob->data) {
		return 0;
	}

	len = push_string(blob->data + blob->length, str, max_len, flags);

	blob->length += len;

	return len;
}

/*
  pull a GUID structure from the wire. The buffer must be at least 16
  bytes long
 */
NTSTATUS smbcli_pull_guid(void *base, uint16_t offset,
			  struct GUID *guid)
{
	DATA_BLOB blob;

	ZERO_STRUCTP(guid);

	blob.data       = offset + (uint8_t *)base;
	blob.length     = 16;

	return GUID_from_ndr_blob(&blob, guid);
}

/*
  push a guid onto the wire. The buffer must hold 16 bytes
 */
NTSTATUS smbcli_push_guid(void *base, uint16_t offset, const struct GUID *guid)
{
	TALLOC_CTX *tmp_ctx = talloc_new(NULL);
	NTSTATUS status;
	DATA_BLOB blob;
	status = GUID_to_ndr_blob(guid, tmp_ctx, &blob);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return status;
	}
	memcpy(offset + (uint8_t *)base, blob.data, blob.length);
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}
