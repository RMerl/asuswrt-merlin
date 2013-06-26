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

NTSTATUS ntlmssp_pull_NEGOTIATE_MESSAGE(const DATA_BLOB *blob,
					TALLOC_CTX *mem_ctx,
					struct NEGOTIATE_MESSAGE *r);
NTSTATUS ntlmssp_pull_CHALLENGE_MESSAGE(const DATA_BLOB *blob,
					TALLOC_CTX *mem_ctx,
					struct CHALLENGE_MESSAGE *r);
NTSTATUS ntlmssp_pull_AUTHENTICATE_MESSAGE(const DATA_BLOB *blob,
					   TALLOC_CTX *mem_ctx,
					   struct AUTHENTICATE_MESSAGE *r);
NTSTATUS ntlmssp_push_NEGOTIATE_MESSAGE(DATA_BLOB *blob,
					TALLOC_CTX *mem_ctx,
					const struct NEGOTIATE_MESSAGE *r);
NTSTATUS ntlmssp_push_CHALLENGE_MESSAGE(DATA_BLOB *blob,
					TALLOC_CTX *mem_ctx,
					const struct CHALLENGE_MESSAGE *r);
NTSTATUS ntlmssp_push_AUTHENTICATE_MESSAGE(DATA_BLOB *blob,
					   TALLOC_CTX *mem_ctx,
					   const struct AUTHENTICATE_MESSAGE *r);
