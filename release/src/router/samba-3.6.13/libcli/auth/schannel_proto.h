/*
   Unix SMB/CIFS implementation.

   dcerpc schannel operations

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2005

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

#ifndef _LIBCLI_AUTH_SCHANNEL_PROTO_H__
#define _LIBCLI_AUTH_SCHANNEL_PROTO_H__

struct schannel_state;

struct tdb_wrap *open_schannel_session_store(TALLOC_CTX *mem_ctx,
					     const char *private_dir);

NTSTATUS netsec_incoming_packet(struct schannel_state *state,
				TALLOC_CTX *mem_ctx,
				bool do_unseal,
				uint8_t *data, size_t length,
				const DATA_BLOB *sig);
uint32_t netsec_outgoing_sig_size(struct schannel_state *state);
NTSTATUS netsec_outgoing_packet(struct schannel_state *state,
				TALLOC_CTX *mem_ctx,
				bool do_seal,
				uint8_t *data, size_t length,
				DATA_BLOB *sig);

#endif
