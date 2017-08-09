/*
   Unix SMB/Netbios implementation.
   NTLMSSP ndr functions

   Copyright (C) Guenther Deschner 2009

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
#include "../librpc/gen_ndr/ndr_ntlmssp.h"
#include "../libcli/auth/ntlmssp_ndr.h"

#define NTLMSSP_PULL_MESSAGE(type, blob, mem_ctx, r) \
do { \
	enum ndr_err_code __ndr_err; \
	__ndr_err = ndr_pull_struct_blob(blob, mem_ctx, r, \
			(ndr_pull_flags_fn_t)ndr_pull_ ##type); \
	if (!NDR_ERR_CODE_IS_SUCCESS(__ndr_err)) { \
		return ndr_map_error2ntstatus(__ndr_err); \
	} \
	if (memcmp(r->Signature, "NTLMSSP\0", 8)) {\
		return NT_STATUS_INVALID_PARAMETER; \
	} \
	return NT_STATUS_OK; \
} while(0);

#define NTLMSSP_PUSH_MESSAGE(type, blob, mem_ctx, r) \
do { \
	enum ndr_err_code __ndr_err; \
	__ndr_err = ndr_push_struct_blob(blob, mem_ctx, r, \
			(ndr_push_flags_fn_t)ndr_push_ ##type); \
	if (!NDR_ERR_CODE_IS_SUCCESS(__ndr_err)) { \
		return ndr_map_error2ntstatus(__ndr_err); \
	} \
	return NT_STATUS_OK; \
} while(0);


/**
 * Pull NTLMSSP NEGOTIATE_MESSAGE struct from a blob
 * @param blob The plain packet blob
 * @param mem_ctx A talloc context
 * @param r Pointer to a NTLMSSP NEGOTIATE_MESSAGE structure
 */

NTSTATUS ntlmssp_pull_NEGOTIATE_MESSAGE(const DATA_BLOB *blob,
					TALLOC_CTX *mem_ctx,
					struct NEGOTIATE_MESSAGE *r)
{
	NTLMSSP_PULL_MESSAGE(NEGOTIATE_MESSAGE, blob, mem_ctx, r);
}

/**
 * Pull NTLMSSP CHALLENGE_MESSAGE struct from a blob
 * @param blob The plain packet blob
 * @param mem_ctx A talloc context
 * @param r Pointer to a NTLMSSP CHALLENGE_MESSAGE structure
 */

NTSTATUS ntlmssp_pull_CHALLENGE_MESSAGE(const DATA_BLOB *blob,
					TALLOC_CTX *mem_ctx,
					struct CHALLENGE_MESSAGE *r)
{
	NTLMSSP_PULL_MESSAGE(CHALLENGE_MESSAGE, blob, mem_ctx, r);
}

/**
 * Pull NTLMSSP AUTHENTICATE_MESSAGE struct from a blob
 * @param blob The plain packet blob
 * @param mem_ctx A talloc context
 * @param r Pointer to a NTLMSSP AUTHENTICATE_MESSAGE structure
 */

NTSTATUS ntlmssp_pull_AUTHENTICATE_MESSAGE(const DATA_BLOB *blob,
					   TALLOC_CTX *mem_ctx,
					   struct AUTHENTICATE_MESSAGE *r)
{
	NTLMSSP_PULL_MESSAGE(AUTHENTICATE_MESSAGE, blob, mem_ctx, r);
}

/**
 * Push NTLMSSP NEGOTIATE_MESSAGE struct into a blob
 * @param blob The plain packet blob
 * @param mem_ctx A talloc context
 * @param r Pointer to a NTLMSSP NEGOTIATE_MESSAGE structure
 */

NTSTATUS ntlmssp_push_NEGOTIATE_MESSAGE(DATA_BLOB *blob,
					TALLOC_CTX *mem_ctx,
					const struct NEGOTIATE_MESSAGE *r)
{
	NTLMSSP_PUSH_MESSAGE(NEGOTIATE_MESSAGE, blob, mem_ctx, r);
}

/**
 * Push NTLMSSP CHALLENGE_MESSAGE struct into a blob
 * @param blob The plain packet blob
 * @param mem_ctx A talloc context
 * @param r Pointer to a NTLMSSP CHALLENGE_MESSAGE structure
 */

NTSTATUS ntlmssp_push_CHALLENGE_MESSAGE(DATA_BLOB *blob,
					TALLOC_CTX *mem_ctx,
					const struct CHALLENGE_MESSAGE *r)
{
	NTLMSSP_PUSH_MESSAGE(CHALLENGE_MESSAGE, blob, mem_ctx, r);
}

/**
 * Push NTLMSSP AUTHENTICATE_MESSAGE struct into a blob
 * @param blob The plain packet blob
 * @param mem_ctx A talloc context
 * @param r Pointer to a NTLMSSP AUTHENTICATE_MESSAGE structure
 */

NTSTATUS ntlmssp_push_AUTHENTICATE_MESSAGE(DATA_BLOB *blob,
					   TALLOC_CTX *mem_ctx,
					   const struct AUTHENTICATE_MESSAGE *r)
{
	NTLMSSP_PUSH_MESSAGE(AUTHENTICATE_MESSAGE, blob, mem_ctx, r);
}
