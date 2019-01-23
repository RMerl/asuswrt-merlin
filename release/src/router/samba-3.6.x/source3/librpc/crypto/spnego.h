/*
 *  SPNEGO Encapsulation
 *  RPC Pipe client routines
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

#ifndef _CLI_SPNEGO_H_
#define _CLI_SPENGO_H_

enum spnego_mech {
	SPNEGO_NONE = 0,
	SPNEGO_KRB5,
	SPNEGO_NTLMSSP
};

struct spnego_context {
	enum spnego_mech mech;

	union {
		struct auth_ntlmssp_state *ntlmssp_state;
		struct gse_context *gssapi_state;
	} mech_ctx;

	char *oid_list[ASN1_MAX_OIDS];
	char *mech_oid;

	enum {
		SPNEGO_CONV_INIT = 0,
		SPNEGO_CONV_NEGO,
		SPNEGO_CONV_AUTH_MORE,
		SPNEGO_CONV_AUTH_CONFIRM,
		SPNEGO_CONV_AUTH_DONE
	} state;

	bool do_sign;
	bool do_seal;
	bool is_dcerpc;
};

NTSTATUS spnego_gssapi_init_client(TALLOC_CTX *mem_ctx,
				   bool do_sign, bool do_seal,
				   bool is_dcerpc,
				   const char *ccache_name,
				   const char *server,
				   const char *service,
				   const char *username,
				   const char *password,
				   struct spnego_context **spengo_ctx);
NTSTATUS spnego_ntlmssp_init_client(TALLOC_CTX *mem_ctx,
				    bool do_sign, bool do_seal,
				    bool is_dcerpc,
				    const char *domain,
				    const char *username,
				    const char *password,
				    struct spnego_context **spnego_ctx);

NTSTATUS spnego_get_client_auth_token(TALLOC_CTX *mem_ctx,
				      struct spnego_context *sp_ctx,
				      DATA_BLOB *spnego_in,
				      DATA_BLOB *spnego_out);

bool spnego_require_more_processing(struct spnego_context *sp_ctx);

NTSTATUS spnego_get_negotiated_mech(struct spnego_context *sp_ctx,
				    enum spnego_mech *type,
				    void **auth_context);

DATA_BLOB spnego_get_session_key(TALLOC_CTX *mem_ctx,
				 struct spnego_context *sp_ctx);

NTSTATUS spnego_sign(TALLOC_CTX *mem_ctx,
			struct spnego_context *sp_ctx,
			DATA_BLOB *data, DATA_BLOB *full_data,
			DATA_BLOB *signature);
NTSTATUS spnego_sigcheck(TALLOC_CTX *mem_ctx,
			 struct spnego_context *sp_ctx,
			 DATA_BLOB *data, DATA_BLOB *full_data,
			 DATA_BLOB *signature);
NTSTATUS spnego_seal(TALLOC_CTX *mem_ctx,
			struct spnego_context *sp_ctx,
			DATA_BLOB *data, DATA_BLOB *full_data,
			DATA_BLOB *signature);
NTSTATUS spnego_unseal(TALLOC_CTX *mem_ctx,
			struct spnego_context *sp_ctx,
			DATA_BLOB *data, DATA_BLOB *full_data,
			DATA_BLOB *signature);

#endif /* _CLI_SPENGO_H_ */
