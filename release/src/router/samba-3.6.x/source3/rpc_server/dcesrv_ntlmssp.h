/*
 *  NTLMSSP Acceptor
 *  DCERPC Server functions
 *  Copyright (C) Simo Sorce 2010.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _DCESRV_NTLMSSP_H_
#define _DCESRV_NTLMSSP_H_

struct auth_ntlmssp_state;

NTSTATUS ntlmssp_server_auth_start(TALLOC_CTX *mem_ctx,
				   bool do_sign,
				   bool do_seal,
				   bool is_dcerpc,
				   DATA_BLOB *token_in,
				   DATA_BLOB *token_out,
				   struct auth_ntlmssp_state **ctx);
NTSTATUS ntlmssp_server_step(struct auth_ntlmssp_state *ctx,
			     TALLOC_CTX *mem_ctx,
			     DATA_BLOB *token_in,
			     DATA_BLOB *token_out);
NTSTATUS ntlmssp_server_check_flags(struct auth_ntlmssp_state *ctx,
				    bool do_sign, bool do_seal);
NTSTATUS ntlmssp_server_get_user_info(struct auth_ntlmssp_state *ctx,
				      TALLOC_CTX *mem_ctx,
				      struct auth_serversupplied_info **session_info);

#endif /* _DCESRV_NTLMSSP_H_ */
