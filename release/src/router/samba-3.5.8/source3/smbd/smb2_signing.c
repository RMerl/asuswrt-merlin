/*
   Unix SMB/CIFS implementation.
   SMB2 signing

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

#include "includes.h"
#include "smbd/globals.h"
#include "../libcli/smb/smb_common.h"
#include "../lib/crypto/crypto.h"

NTSTATUS smb2_signing_sign_pdu(DATA_BLOB session_key,
			       struct iovec *vector,
			       int count)
{
	uint8_t *hdr;
	uint64_t session_id;
	struct HMACSHA256Context m;
	uint8_t res[SHA256_DIGEST_LENGTH];
	int i;

	if (count < 2) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (vector[0].iov_len != SMB2_HDR_BODY) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	hdr = (uint8_t *)vector[0].iov_base;

	session_id = BVAL(hdr, SMB2_HDR_SESSION_ID);
	if (session_id == 0) {
		/*
		 * do not sign messages with a zero session_id.
		 * See MS-SMB2 3.2.4.1.1
		 */
		return NT_STATUS_OK;
	}

	if (session_key.length == 0) {
		DEBUG(2,("Wrong session key length %u for SMB2 signing\n",
			 (unsigned)session_key.length));
		return NT_STATUS_ACCESS_DENIED;
	}

	memset(hdr + SMB2_HDR_SIGNATURE, 0, 16);

	SIVAL(hdr, SMB2_HDR_FLAGS, IVAL(hdr, SMB2_HDR_FLAGS) | SMB2_HDR_FLAG_SIGNED);

	ZERO_STRUCT(m);
	hmac_sha256_init(session_key.data, MIN(session_key.length, 16), &m);
	for (i=0; i < count; i++) {
		hmac_sha256_update((const uint8_t *)vector[i].iov_base,
				   vector[i].iov_len, &m);
	}
	hmac_sha256_final(res, &m);
	DEBUG(5,("signed SMB2 message\n"));

	memcpy(hdr + SMB2_HDR_SIGNATURE, res, 16);

	return NT_STATUS_OK;
}

NTSTATUS smb2_signing_check_pdu(DATA_BLOB session_key,
				const struct iovec *vector,
				int count)
{
	const uint8_t *hdr;
	const uint8_t *sig;
	uint64_t session_id;
	struct HMACSHA256Context m;
	uint8_t res[SHA256_DIGEST_LENGTH];
	static const uint8_t zero_sig[16] = { 0, };
	int i;

	if (count < 2) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (vector[0].iov_len != SMB2_HDR_BODY) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	hdr = (const uint8_t *)vector[0].iov_base;

	session_id = BVAL(hdr, SMB2_HDR_SESSION_ID);
	if (session_id == 0) {
		/*
		 * do not sign messages with a zero session_id.
		 * See MS-SMB2 3.2.4.1.1
		 */
		return NT_STATUS_OK;
	}

	if (session_key.length == 0) {
		/* we don't have the session key yet */
		return NT_STATUS_OK;
	}

	sig = hdr+SMB2_HDR_SIGNATURE;

	ZERO_STRUCT(m);
	hmac_sha256_init(session_key.data, MIN(session_key.length, 16), &m);
	hmac_sha256_update(hdr, SMB2_HDR_SIGNATURE, &m);
	hmac_sha256_update(zero_sig, 16, &m);
	for (i=1; i < count; i++) {
		hmac_sha256_update((const uint8_t *)vector[i].iov_base,
				   vector[i].iov_len, &m);
	}
	hmac_sha256_final(res, &m);

	if (memcmp(res, sig, 16) != 0) {
		DEBUG(0,("Bad SMB2 signature for message\n"));
		dump_data(0, sig, 16);
		dump_data(0, res, 16);
		return NT_STATUS_ACCESS_DENIED;
	}

	return NT_STATUS_OK;
}
