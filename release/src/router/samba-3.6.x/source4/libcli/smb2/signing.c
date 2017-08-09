/* 
   Unix SMB/CIFS implementation.

   SMB2 Signing Code

   Copyright (C) Andrew Tridgell <tridge@samba.org> 2008

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
#include "libcli/smb2/smb2_calls.h"
#include "../lib/crypto/crypto.h"

/*
  sign an outgoing message
 */
NTSTATUS smb2_sign_message(struct smb2_request_buffer *buf, DATA_BLOB session_key)
{
	struct HMACSHA256Context m;
	uint8_t res[32];
	uint64_t session_id;
	size_t hdr_offset;

	if (buf->size < NBT_HDR_SIZE + SMB2_HDR_SIGNATURE + 16) {
		/* can't sign non-SMB2 messages */
		return NT_STATUS_OK;
	}

	hdr_offset = buf->hdr - buf->buffer;

	session_id = BVAL(buf->hdr, SMB2_HDR_SESSION_ID);
	if (session_id == 0) {
		/* we don't sign messages with a zero session_id. See
		   MS-SMB2 3.2.4.1.1 */
		return NT_STATUS_OK;		
	}

	if (session_key.length == 0) {
		DEBUG(2,("Wrong session key length %u for SMB2 signing\n",
			 (unsigned)session_key.length));
		return NT_STATUS_ACCESS_DENIED;
	}

	memset(buf->hdr + SMB2_HDR_SIGNATURE, 0, 16);

	SIVAL(buf->hdr, SMB2_HDR_FLAGS, IVAL(buf->hdr, SMB2_HDR_FLAGS) | SMB2_HDR_FLAG_SIGNED);

	ZERO_STRUCT(m);
	hmac_sha256_init(session_key.data, MIN(session_key.length, 16), &m);
	hmac_sha256_update(buf->hdr, buf->size-hdr_offset, &m);
	hmac_sha256_final(res, &m);
	DEBUG(5,("signed SMB2 message of size %u\n", (unsigned)buf->size - NBT_HDR_SIZE));

	memcpy(buf->hdr + SMB2_HDR_SIGNATURE, res, 16);

	return NT_STATUS_OK;	
}

/*
  check an incoming signature
 */
NTSTATUS smb2_check_signature(struct smb2_request_buffer *buf, DATA_BLOB session_key)
{
	uint64_t session_id;
	struct HMACSHA256Context m;
	uint8_t res[SHA256_DIGEST_LENGTH];
	uint8_t sig[16];
	size_t hdr_offset;

	if (buf->size < NBT_HDR_SIZE + SMB2_HDR_SIGNATURE + 16) {
		/* can't check non-SMB2 messages */
		return NT_STATUS_OK;
	}

	hdr_offset = buf->hdr - buf->buffer;

	session_id = BVAL(buf->hdr, SMB2_HDR_SESSION_ID);
	if (session_id == 0) {
		/* don't sign messages with a zero session_id. See
		   MS-SMB2 3.2.4.1.1 */
		return NT_STATUS_OK;		
	}

	if (session_key.length == 0) {
		/* we don't have the session key yet */
		return NT_STATUS_OK;
	}

	memcpy(sig, buf->hdr+SMB2_HDR_SIGNATURE, 16);

	memset(buf->hdr + SMB2_HDR_SIGNATURE, 0, 16);

	ZERO_STRUCT(m);
	hmac_sha256_init(session_key.data, MIN(session_key.length, 16), &m);
	hmac_sha256_update(buf->hdr, buf->size-hdr_offset, &m);
	hmac_sha256_final(res, &m);

	memcpy(buf->hdr+SMB2_HDR_SIGNATURE, sig, 16);

	if (memcmp(res, sig, 16) != 0) {
		DEBUG(0,("Bad SMB2 signature for message of size %u\n", 
			 (unsigned)buf->size-NBT_HDR_SIZE));
		dump_data(0, sig, 16);
		dump_data(0, res, 16);
		return NT_STATUS_ACCESS_DENIED;
	}

	return NT_STATUS_OK;	
}
