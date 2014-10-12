/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Andrew Tridgell              2003
   
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
  this file implements functions for manipulating the 'struct smbsrv_request' structure in smbd
*/

#include "includes.h"
#include "smb_server/smb_server.h"
#include "smbd/service_stream.h"
#include "lib/stream/packet.h"
#include "ntvfs/ntvfs.h"


/* we over allocate the data buffer to prevent too many realloc calls */
#define REQ_OVER_ALLOCATION 0

/* setup the bufinfo used for strings and range checking */
void smbsrv_setup_bufinfo(struct smbsrv_request *req)
{
	req->in.bufinfo.mem_ctx    = req;
	req->in.bufinfo.flags      = 0;
	if (req->flags2 & FLAGS2_UNICODE_STRINGS) {
		req->in.bufinfo.flags |= BUFINFO_FLAG_UNICODE;
	}
	req->in.bufinfo.align_base = req->in.buffer;
	req->in.bufinfo.data       = req->in.data;
	req->in.bufinfo.data_size  = req->in.data_size;
}


static int smbsrv_request_destructor(struct smbsrv_request *req)
{
	DLIST_REMOVE(req->smb_conn->requests, req);
	return 0;
}

/****************************************************************************
construct a basic request packet, mostly used to construct async packets
such as change notify and oplock break requests
****************************************************************************/
struct smbsrv_request *smbsrv_init_request(struct smbsrv_connection *smb_conn)
{
	struct smbsrv_request *req;

	req = talloc_zero(smb_conn, struct smbsrv_request);
	if (!req) {
		return NULL;
	}

	/* setup the request context */
	req->smb_conn = smb_conn;

	talloc_set_destructor(req, smbsrv_request_destructor);

	return req;
}


/*
  setup a chained reply in req->out with the given word count and initial data buffer size. 
*/
static void req_setup_chain_reply(struct smbsrv_request *req, unsigned int wct, unsigned int buflen)
{
	uint32_t chain_base_size = req->out.size;

	/* we need room for the wct value, the words, the buffer length and the buffer */
	req->out.size += 1 + VWV(wct) + 2 + buflen;

	/* over allocate by a small amount */
	req->out.allocated = req->out.size + REQ_OVER_ALLOCATION; 

	req->out.buffer = talloc_realloc(req, req->out.buffer, 
					 uint8_t, req->out.allocated);
	if (!req->out.buffer) {
		smbsrv_terminate_connection(req->smb_conn, "allocation failed");
		return;
	}

	req->out.hdr = req->out.buffer + NBT_HDR_SIZE;
	req->out.vwv = req->out.buffer + chain_base_size + 1;
	req->out.wct = wct;
	req->out.data = req->out.vwv + VWV(wct) + 2;
	req->out.data_size = buflen;
	req->out.ptr = req->out.data;

	SCVAL(req->out.buffer, chain_base_size, wct);
	SSVAL(req->out.vwv, VWV(wct), buflen);
}


/*
  setup a reply in req->out with the given word count and initial data buffer size. 
  the caller will then fill in the command words and data before calling req_send_reply() to 
  send the reply on its way
*/
void smbsrv_setup_reply(struct smbsrv_request *req, unsigned int wct, size_t buflen)
{
	uint16_t flags2;

	if (req->chain_count != 0) {
		req_setup_chain_reply(req, wct, buflen);
		return;
	}

	req->out.size = NBT_HDR_SIZE + MIN_SMB_SIZE + VWV(wct) + buflen;

	/* over allocate by a small amount */
	req->out.allocated = req->out.size + REQ_OVER_ALLOCATION; 

	req->out.buffer = talloc_size(req, req->out.allocated);
	if (!req->out.buffer) {
		smbsrv_terminate_connection(req->smb_conn, "allocation failed");
		return;
	}

	flags2 = FLAGS2_LONG_PATH_COMPONENTS | 
		FLAGS2_EXTENDED_ATTRIBUTES | 
		FLAGS2_IS_LONG_NAME;
#define _SMB_FLAGS2_ECHOED_FLAGS ( \
	FLAGS2_UNICODE_STRINGS | \
	FLAGS2_EXTENDED_SECURITY | \
	FLAGS2_SMB_SECURITY_SIGNATURES \
)
	flags2 |= (req->flags2 & _SMB_FLAGS2_ECHOED_FLAGS);
	if (req->smb_conn->negotiate.client_caps & CAP_STATUS32) {
		flags2 |= FLAGS2_32_BIT_ERROR_CODES;
	}

	req->out.hdr = req->out.buffer + NBT_HDR_SIZE;
	req->out.vwv = req->out.hdr + HDR_VWV;
	req->out.wct = wct;
	req->out.data = req->out.vwv + VWV(wct) + 2;
	req->out.data_size = buflen;
	req->out.ptr = req->out.data;

	SIVAL(req->out.hdr, HDR_RCLS, 0);

	SCVAL(req->out.hdr, HDR_WCT, wct);
	SSVAL(req->out.vwv, VWV(wct), buflen);

	memcpy(req->out.hdr, "\377SMB", 4);
	SCVAL(req->out.hdr,HDR_FLG, FLAG_REPLY | FLAG_CASELESS_PATHNAMES); 
	SSVAL(req->out.hdr,HDR_FLG2, flags2);
	SSVAL(req->out.hdr,HDR_PIDHIGH,0);
	memset(req->out.hdr + HDR_SS_FIELD, 0, 10);

	if (req->in.hdr) {
		/* copy the cmd, tid, pid, uid and mid from the request */
		SCVAL(req->out.hdr,HDR_COM,CVAL(req->in.hdr,HDR_COM));	
		SSVAL(req->out.hdr,HDR_TID,SVAL(req->in.hdr,HDR_TID));
		SSVAL(req->out.hdr,HDR_PID,SVAL(req->in.hdr,HDR_PID));
		SSVAL(req->out.hdr,HDR_UID,SVAL(req->in.hdr,HDR_UID));
		SSVAL(req->out.hdr,HDR_MID,SVAL(req->in.hdr,HDR_MID));
	} else {
		SCVAL(req->out.hdr,HDR_COM,0);
		SSVAL(req->out.hdr,HDR_TID,0);
		SSVAL(req->out.hdr,HDR_PID,0);
		SSVAL(req->out.hdr,HDR_UID,0);
		SSVAL(req->out.hdr,HDR_MID,0);
	}
}


/*
  setup a copy of a request, used when the server needs to send
  more than one reply for a single request packet
*/
struct smbsrv_request *smbsrv_setup_secondary_request(struct smbsrv_request *old_req)
{
	struct smbsrv_request *req;
	ptrdiff_t diff;

	req = talloc_memdup(old_req, old_req, sizeof(struct smbsrv_request));
	if (req == NULL) {
		return NULL;
	}

	req->out.buffer = talloc_memdup(req, req->out.buffer, req->out.allocated);
	if (req->out.buffer == NULL) {
		talloc_free(req);
		return NULL;
	}

	diff = req->out.buffer - old_req->out.buffer;

	req->out.hdr  += diff;
	req->out.vwv  += diff;
	req->out.data += diff;
	req->out.ptr  += diff;

	return req;
}

/*
  work out the maximum data size we will allow for this reply, given
  the negotiated max_xmit. The basic reply packet must be setup before
  this call

  note that this is deliberately a signed integer reply
*/
int req_max_data(struct smbsrv_request *req)
{
	int ret;
	ret = req->smb_conn->negotiate.max_send;
	ret -= PTR_DIFF(req->out.data, req->out.hdr);
	if (ret < 0) ret = 0;
	return ret;
}


/*
  grow the allocation of the data buffer portion of a reply
  packet. Note that as this can reallocate the packet buffer this
  invalidates any local pointers into the packet.

  To cope with this req->out.ptr is supplied. This will be updated to
  point at the same offset into the packet as before this call
*/
static void req_grow_allocation(struct smbsrv_request *req, unsigned int new_size)
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
void req_grow_data(struct smbsrv_request *req, size_t new_size)
{
	int delta;

	if (!(req->control_flags & SMBSRV_REQ_CONTROL_LARGE) && new_size > req_max_data(req)) {
		smb_panic("reply buffer too large!");
	}

	req_grow_allocation(req, new_size);

	delta = new_size - req->out.data_size;

	req->out.size += delta;
	req->out.data_size += delta;

	/* set the BCC to the new data size */
	SSVAL(req->out.vwv, VWV(req->out.wct), new_size);
}

/*
  send a reply and destroy the request buffer

  note that this only looks at req->out.buffer and req->out.size, allowing manually 
  constructed packets to be sent
*/
void smbsrv_send_reply_nosign(struct smbsrv_request *req)
{
	DATA_BLOB blob;
	NTSTATUS status;

	if (req->smb_conn->connection->event.fde == NULL) {
		/* we are in the process of shutting down this connection */
		talloc_free(req);
		return;
	}

	if (req->out.size > NBT_HDR_SIZE) {
		_smb_setlen(req->out.buffer, req->out.size - NBT_HDR_SIZE);
	}

	blob = data_blob_const(req->out.buffer, req->out.size);
	status = packet_send(req->smb_conn->packet, blob);
	if (!NT_STATUS_IS_OK(status)) {
		smbsrv_terminate_connection(req->smb_conn, nt_errstr(status));
	}
	talloc_free(req);
}

/*
  possibly sign a message then send a reply and destroy the request buffer

  note that this only looks at req->out.buffer and req->out.size, allowing manually 
  constructed packets to be sent
*/
void smbsrv_send_reply(struct smbsrv_request *req)
{
	if (req->smb_conn->connection->event.fde == NULL) {
		/* we are in the process of shutting down this connection */
		talloc_free(req);
		return;
	}
	smbsrv_sign_packet(req);

	smbsrv_send_reply_nosign(req);
}

/* 
   setup the header of a reply to include an NTSTATUS code
*/
void smbsrv_setup_error(struct smbsrv_request *req, NTSTATUS status)
{
	if (!req->smb_conn->config.nt_status_support || !(req->smb_conn->negotiate.client_caps & CAP_STATUS32)) {
		/* convert to DOS error codes */
		uint8_t eclass;
		uint32_t ecode;
		ntstatus_to_dos(status, &eclass, &ecode);
		SCVAL(req->out.hdr, HDR_RCLS, eclass);
		SSVAL(req->out.hdr, HDR_ERR, ecode);
		SSVAL(req->out.hdr, HDR_FLG2, SVAL(req->out.hdr, HDR_FLG2) & ~FLAGS2_32_BIT_ERROR_CODES);
		return;
	}

	if (NT_STATUS_IS_DOS(status)) {
		/* its a encoded DOS error, using the reserved range */
		SSVAL(req->out.hdr, HDR_RCLS, NT_STATUS_DOS_CLASS(status));
		SSVAL(req->out.hdr, HDR_ERR,  NT_STATUS_DOS_CODE(status));
		SSVAL(req->out.hdr, HDR_FLG2, SVAL(req->out.hdr, HDR_FLG2) & ~FLAGS2_32_BIT_ERROR_CODES);
	} else {
		SIVAL(req->out.hdr, HDR_RCLS, NT_STATUS_V(status));
		SSVAL(req->out.hdr, HDR_FLG2, SVAL(req->out.hdr, HDR_FLG2) | FLAGS2_32_BIT_ERROR_CODES);
	}
}

/* 
   construct and send an error packet, then destroy the request 
   auto-converts to DOS error format when appropriate
*/
void smbsrv_send_error(struct smbsrv_request *req, NTSTATUS status)
{
	if (req->smb_conn->connection->event.fde == NULL) {
		/* the socket has been destroyed - no point trying to send an error! */
		talloc_free(req);
		return;
	}
	smbsrv_setup_reply(req, 0, 0);

	/* error returns never have any data */
	req_grow_data(req, 0);

	smbsrv_setup_error(req, status);
	smbsrv_send_reply(req);
}


/*
  push a string into the data portion of the request packet, growing it if necessary
  this gets quite tricky - please be very careful to cover all cases when modifying this

  if dest is NULL, then put the string at the end of the data portion of the packet

  if dest_len is -1 then no limit applies
*/
size_t req_push_str(struct smbsrv_request *req, uint8_t *dest, const char *str, int dest_len, size_t flags)
{
	size_t len;
	unsigned int grow_size;
	uint8_t *buf0;
	const int max_bytes_per_char = 3;

	if (!(flags & (STR_ASCII|STR_UNICODE))) {
		flags |= (req->flags2 & FLAGS2_UNICODE_STRINGS) ? STR_UNICODE : STR_ASCII;
	}

	if (dest == NULL) {
		dest = req->out.data + req->out.data_size;
	}

	if (dest_len != -1) {
		len = dest_len;
	} else {
		len = (strlen(str)+2) * max_bytes_per_char;
	}

	grow_size = len + PTR_DIFF(dest, req->out.data);
	buf0 = req->out.buffer;

	req_grow_allocation(req, grow_size);

	if (buf0 != req->out.buffer) {
		dest = req->out.buffer + PTR_DIFF(dest, buf0);
	}

	len = push_string(dest, str, len, flags);

	grow_size = len + PTR_DIFF(dest, req->out.data);

	if (grow_size > req->out.data_size) {
		req_grow_data(req, grow_size);
	}

	return len;
}

/*
  append raw bytes into the data portion of the request packet
  return the number of bytes added
*/
size_t req_append_bytes(struct smbsrv_request *req, 
			const uint8_t *bytes, size_t byte_len)
{
	req_grow_allocation(req, byte_len + req->out.data_size);
	memcpy(req->out.data + req->out.data_size, bytes, byte_len);
	req_grow_data(req, byte_len + req->out.data_size);
	return byte_len;
}
/*
  append variable block (type 5 buffer) into the data portion of the request packet
  return the number of bytes added
*/
size_t req_append_var_block(struct smbsrv_request *req, 
		const uint8_t *bytes, uint16_t byte_len)
{
	req_grow_allocation(req, byte_len + 3 + req->out.data_size);
	SCVAL(req->out.data + req->out.data_size, 0, 5);
	SSVAL(req->out.data + req->out.data_size, 1, byte_len);		/* add field length */
	if (byte_len > 0) {
		memcpy(req->out.data + req->out.data_size + 3, bytes, byte_len);
	}
	req_grow_data(req, byte_len + 3 + req->out.data_size);
	return byte_len + 3;
}
/**
  pull a UCS2 string from a request packet, returning a talloced unix string

  the string length is limited by the 3 things:
   - the data size in the request (end of packet)
   - the passed 'byte_len' if it is not -1
   - the end of string (null termination)

  Note that 'byte_len' is the number of bytes in the packet

  on failure zero is returned and *dest is set to NULL, otherwise the number
  of bytes consumed in the packet is returned
*/
static size_t req_pull_ucs2(struct request_bufinfo *bufinfo, const char **dest, const uint8_t *src, int byte_len, unsigned int flags)
{
	int src_len, src_len2, alignment=0;
	bool ret;
	char *dest2;

	if (!(flags & STR_NOALIGN) && ucs2_align(bufinfo->align_base, src, flags)) {
		src++;
		alignment=1;
		if (byte_len != -1) {
			byte_len--;
		}
	}

	if (flags & STR_NO_RANGE_CHECK) {
		src_len = byte_len;
	} else {
		src_len = bufinfo->data_size - PTR_DIFF(src, bufinfo->data);
		if (byte_len != -1 && src_len > byte_len) {
			src_len = byte_len;
		}
	}

	if (src_len < 0) {
		*dest = NULL;
		return 0;
	}
	
	src_len2 = utf16_len_n(src, src_len);
	if (src_len2 == 0) {
		*dest = talloc_strdup(bufinfo->mem_ctx, "");
		return src_len2 + alignment;
	}

	ret = convert_string_talloc(bufinfo->mem_ctx, CH_UTF16, CH_UNIX, src, src_len2, (void **)&dest2, NULL, false);

	if (!ret) {
		*dest = NULL;
		return 0;
	}
	*dest = dest2;

	return src_len2 + alignment;
}

/**
  pull a ascii string from a request packet, returning a talloced string

  the string length is limited by the 3 things:
   - the data size in the request (end of packet)
   - the passed 'byte_len' if it is not -1
   - the end of string (null termination)

  Note that 'byte_len' is the number of bytes in the packet

  on failure zero is returned and *dest is set to NULL, otherwise the number
  of bytes consumed in the packet is returned
*/
static size_t req_pull_ascii(struct request_bufinfo *bufinfo, const char **dest, const uint8_t *src, int byte_len, unsigned int flags)
{
	int src_len, src_len2;
	bool ret;
	char *dest2;

	if (flags & STR_NO_RANGE_CHECK) {
		src_len = byte_len;
	} else {
		src_len = bufinfo->data_size - PTR_DIFF(src, bufinfo->data);
		if (src_len < 0) {
			*dest = NULL;
			return 0;
		}
		if (byte_len != -1 && src_len > byte_len) {
			src_len = byte_len;
		}
	}

	src_len2 = strnlen((const char *)src, src_len);
	if (src_len2 <= src_len - 1) {
		/* include the termination if we didn't reach the end of the packet */
		src_len2++;
	}

	ret = convert_string_talloc(bufinfo->mem_ctx, CH_DOS, CH_UNIX, src, src_len2, (void **)&dest2, NULL, false);

	if (!ret) {
		*dest = NULL;
		return 0;
	}
	*dest = dest2;

	return src_len2;
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
size_t req_pull_string(struct request_bufinfo *bufinfo, const char **dest, const uint8_t *src, int byte_len, unsigned int flags)
{
	if (!(flags & STR_ASCII) && 
	    (((flags & STR_UNICODE) || (bufinfo->flags & BUFINFO_FLAG_UNICODE)))) {
		return req_pull_ucs2(bufinfo, dest, src, byte_len, flags);
	}

	return req_pull_ascii(bufinfo, dest, src, byte_len, flags);
}


/**
  pull a ASCII4 string buffer from a request packet, returning a talloced string
  
  an ASCII4 buffer is a null terminated string that has a prefix
  of the character 0x4. It tends to be used in older parts of the protocol.

  on failure *dest is set to the zero length string. This seems to
  match win2000 behaviour
*/
size_t req_pull_ascii4(struct request_bufinfo *bufinfo, const char **dest, const uint8_t *src, unsigned int flags)
{
	ssize_t ret;

	if (PTR_DIFF(src, bufinfo->data) + 1 > bufinfo->data_size) {
		/* win2000 treats this as the empty string! */
		(*dest) = talloc_strdup(bufinfo->mem_ctx, "");
		return 0;
	}

	/* this consumes the 0x4 byte. We don't check whether the byte
	   is actually 0x4 or not. This matches win2000 server
	   behaviour */
	src++;

	ret = req_pull_string(bufinfo, dest, src, -1, flags);
	if (ret == -1) {
		(*dest) = talloc_strdup(bufinfo->mem_ctx, "");
		return 1;
	}
	
	return ret + 1;
}

/**
  pull a DATA_BLOB from a request packet, returning a talloced blob

  return false if any part is outside the data portion of the packet
*/
bool req_pull_blob(struct request_bufinfo *bufinfo, const uint8_t *src, int len, DATA_BLOB *blob)
{
	if (len != 0 && req_data_oob(bufinfo, src, len)) {
		return false;
	}

	(*blob) = data_blob_talloc(bufinfo->mem_ctx, src, len);

	return true;
}

/* check that a lump of data in a request is within the bounds of the data section of
   the packet */
bool req_data_oob(struct request_bufinfo *bufinfo, const uint8_t *ptr, uint32_t count)
{
	if (count == 0) {
		return false;
	}
	
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
   pull an open file handle from a packet, taking account of the chained_fnum
*/
static uint16_t req_fnum(struct smbsrv_request *req, const uint8_t *base, unsigned int offset)
{
	if (req->chained_fnum != -1) {
		return req->chained_fnum;
	}
	return SVAL(base, offset);
}

struct ntvfs_handle *smbsrv_pull_fnum(struct smbsrv_request *req, const uint8_t *base, unsigned int offset)
{
	struct smbsrv_handle *handle;
	uint16_t fnum = req_fnum(req, base, offset);

	handle = smbsrv_smb_handle_find(req->tcon, fnum, req->request_time);
	if (!handle) {
		return NULL;
	}

	/*
	 * For SMB tcons and sessions can be mixed!
	 * But we need to make sure that file handles
	 * are only accessed by the opening session!
	 *
	 * So check if the handle is valid for the given session!
	 */
	if (handle->session != req->session) {
		return NULL;
	}

	return handle->ntvfs;
}

void smbsrv_push_fnum(uint8_t *base, unsigned int offset, struct ntvfs_handle *ntvfs)
{
	struct smbsrv_handle *handle = talloc_get_type(ntvfs->frontend_data.private_data,
				       struct smbsrv_handle);
	SSVAL(base, offset, handle->hid);
}

NTSTATUS smbsrv_handle_create_new(void *private_data, struct ntvfs_request *ntvfs, struct ntvfs_handle **_h)
{
	struct smbsrv_request *req = talloc_get_type(ntvfs->frontend_data.private_data,
				     struct smbsrv_request);
	struct smbsrv_handle *handle;
	struct ntvfs_handle *h;

	handle = smbsrv_handle_new(req->session, req->tcon, req, req->request_time);
	if (!handle) return NT_STATUS_INSUFFICIENT_RESOURCES;

	h = talloc_zero(handle, struct ntvfs_handle);
	if (!h) goto nomem;

	/* 
	 * note: we don't set handle->ntvfs yet,
	 *       this will be done by smbsrv_handle_make_valid()
	 *       this makes sure the handle is invalid for clients
	 *       until the ntvfs subsystem has made it valid
	 */
	h->ctx		= ntvfs->ctx;
	h->session_info	= ntvfs->session_info;
	h->smbpid	= ntvfs->smbpid;

	h->frontend_data.private_data = handle;

	*_h = h;
	return NT_STATUS_OK;
nomem:
	talloc_free(handle);
	return NT_STATUS_NO_MEMORY;
}

NTSTATUS smbsrv_handle_make_valid(void *private_data, struct ntvfs_handle *h)
{
	struct smbsrv_tcon *tcon = talloc_get_type(private_data, struct smbsrv_tcon);
	struct smbsrv_handle *handle = talloc_get_type(h->frontend_data.private_data,
						       struct smbsrv_handle);
	/* this tells the frontend that the handle is valid */
	handle->ntvfs = h;
	/* this moves the smbsrv_request to the smbsrv_tcon memory context */
	talloc_steal(tcon, handle);
	return NT_STATUS_OK;
}

void smbsrv_handle_destroy(void *private_data, struct ntvfs_handle *h)
{
	struct smbsrv_handle *handle = talloc_get_type(h->frontend_data.private_data,
						       struct smbsrv_handle);
	talloc_free(handle);
}

struct ntvfs_handle *smbsrv_handle_search_by_wire_key(void *private_data, struct ntvfs_request *ntvfs, const DATA_BLOB *key)
{
	struct smbsrv_request *req = talloc_get_type(ntvfs->frontend_data.private_data,
				     struct smbsrv_request);

	if (key->length != 2) return NULL;

	return smbsrv_pull_fnum(req, key->data, 0);
}

DATA_BLOB smbsrv_handle_get_wire_key(void *private_data, struct ntvfs_handle *handle, TALLOC_CTX *mem_ctx)
{
	uint8_t key[2];

	smbsrv_push_fnum(key, 0, handle);

	return data_blob_talloc(mem_ctx, key, sizeof(key));
}
