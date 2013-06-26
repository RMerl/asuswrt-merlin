/*
 *  GSSAPI Security Extensions
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

#ifndef _GSE_H_
#define _GSE_H_

struct gse_context;

#ifndef GSS_C_DCE_STYLE
#define GSS_C_DCE_STYLE 0x1000
#endif

NTSTATUS gse_init_client(TALLOC_CTX *mem_ctx,
			  bool do_sign, bool do_seal,
			  const char *ccache_name,
			  const char *server,
			  const char *service,
			  const char *username,
			  const char *password,
			  uint32_t add_gss_c_flags,
			  struct gse_context **_gse_ctx);
NTSTATUS gse_get_client_auth_token(TALLOC_CTX *mem_ctx,
				   struct gse_context *gse_ctx,
				   DATA_BLOB *token_in,
				   DATA_BLOB *token_out);

NTSTATUS gse_init_server(TALLOC_CTX *mem_ctx,
			 bool do_sign, bool do_seal,
			 uint32_t add_gss_c_flags,
			 const char *keytab,
			 struct gse_context **_gse_ctx);
NTSTATUS gse_get_server_auth_token(TALLOC_CTX *mem_ctx,
				   struct gse_context *gse_ctx,
				   DATA_BLOB *token_in,
				   DATA_BLOB *token_out);
NTSTATUS gse_verify_server_auth_flags(struct gse_context *gse_ctx);

bool gse_require_more_processing(struct gse_context *gse_ctx);
DATA_BLOB gse_get_session_key(TALLOC_CTX *mem_ctx,
				struct gse_context *gse_ctx);
NTSTATUS gse_get_client_name(struct gse_context *gse_ctx,
			     TALLOC_CTX *mem_ctx, char **client_name);
NTSTATUS gse_get_authz_data(struct gse_context *gse_ctx,
			    TALLOC_CTX *mem_ctx, DATA_BLOB *pac);
NTSTATUS gse_get_pac_blob(struct gse_context *gse_ctx,
			  TALLOC_CTX *mem_ctx, DATA_BLOB *pac);

size_t gse_get_signature_length(struct gse_context *gse_ctx,
				int seal, size_t payload_size);
NTSTATUS gse_seal(TALLOC_CTX *mem_ctx, struct gse_context *gse_ctx,
		  DATA_BLOB *data, DATA_BLOB *signature);
NTSTATUS gse_unseal(TALLOC_CTX *mem_ctx, struct gse_context *gse_ctx,
		    DATA_BLOB *data, DATA_BLOB *signature);
NTSTATUS gse_sign(TALLOC_CTX *mem_ctx, struct gse_context *gse_ctx,
		  DATA_BLOB *data, DATA_BLOB *signature);
NTSTATUS gse_sigcheck(TALLOC_CTX *mem_ctx, struct gse_context *gse_ctx,
		      DATA_BLOB *data, DATA_BLOB *signature);

#endif /* _GSE_H_ */
