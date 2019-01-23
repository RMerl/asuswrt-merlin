/*
 *  GSSAPI Acceptor
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

#ifndef _DCESRV_GSSAPI_H_
#define _DCESRV_GSSAPI_H_

struct gse_context;

NTSTATUS gssapi_server_auth_start(TALLOC_CTX *mem_ctx,
				  bool do_sign,
				  bool do_seal,
				  bool is_dcerpc,
				  DATA_BLOB *token_in,
				  DATA_BLOB *token_out,
				  struct gse_context **ctx);
NTSTATUS gssapi_server_step(struct gse_context *gse_ctx,
			    TALLOC_CTX *mem_ctx,
			    DATA_BLOB *token_in,
			    DATA_BLOB *token_out);
NTSTATUS gssapi_server_check_flags(struct gse_context *gse_ctx);
NTSTATUS gssapi_server_get_user_info(struct gse_context *gse_ctx,
				     TALLOC_CTX *mem_ctx,
				     struct client_address *client_id,
				     struct auth_serversupplied_info **session_info);

#endif /* _DCESRV_GSSAPI_H_ */
