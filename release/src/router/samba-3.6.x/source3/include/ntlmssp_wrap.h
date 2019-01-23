/*
   NLTMSSP wrappers

   Copyright (C) Andrew Tridgell      2001
   Copyright (C) Andrew Bartlett 2001-2003

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

#ifndef _NTLMSSP_WRAP_
#define _NTLMSSP_WRAP_

struct auth_ntlmssp_state {
	/* used only by server implementation */
	struct auth_context *auth_context;
	struct auth_serversupplied_info *server_info;

	/* used by both client and server implementation */
	struct ntlmssp_state *ntlmssp_state;
};

NTSTATUS auth_ntlmssp_sign_packet(struct auth_ntlmssp_state *ans,
				  TALLOC_CTX *sig_mem_ctx,
				  const uint8_t *data,
				  size_t length,
				  const uint8_t *whole_pdu,
				  size_t pdu_length,
				  DATA_BLOB *sig);
NTSTATUS auth_ntlmssp_check_packet(struct auth_ntlmssp_state *ans,
				   const uint8_t *data,
				   size_t length,
				   const uint8_t *whole_pdu,
				   size_t pdu_length,
				   const DATA_BLOB *sig);
NTSTATUS auth_ntlmssp_seal_packet(struct auth_ntlmssp_state *ans,
				  TALLOC_CTX *sig_mem_ctx,
				  uint8_t *data,
				  size_t length,
				  const uint8_t *whole_pdu,
				  size_t pdu_length,
				  DATA_BLOB *sig);
NTSTATUS auth_ntlmssp_unseal_packet(struct auth_ntlmssp_state *ans,
				    uint8_t *data,
				    size_t length,
				    const uint8_t *whole_pdu,
				    size_t pdu_length,
				    const DATA_BLOB *sig);
bool auth_ntlmssp_negotiated_sign(struct auth_ntlmssp_state *ans);
bool auth_ntlmssp_negotiated_seal(struct auth_ntlmssp_state *ans);
struct ntlmssp_state *auth_ntlmssp_get_ntlmssp_state(
					struct auth_ntlmssp_state *ans);
const char *auth_ntlmssp_get_username(struct auth_ntlmssp_state *ans);
const char *auth_ntlmssp_get_domain(struct auth_ntlmssp_state *ans);
const char *auth_ntlmssp_get_client(struct auth_ntlmssp_state *ans);
const uint8_t *auth_ntlmssp_get_nt_hash(struct auth_ntlmssp_state *ans);
NTSTATUS auth_ntlmssp_set_username(struct auth_ntlmssp_state *ans,
				   const char *user);
NTSTATUS auth_ntlmssp_set_domain(struct auth_ntlmssp_state *ans,
				 const char *domain);
NTSTATUS auth_ntlmssp_set_password(struct auth_ntlmssp_state *ans,
				   const char *password);
void auth_ntlmssp_and_flags(struct auth_ntlmssp_state *ans, uint32_t flags);
void auth_ntlmssp_or_flags(struct auth_ntlmssp_state *ans, uint32_t flags);
DATA_BLOB auth_ntlmssp_get_session_key(struct auth_ntlmssp_state *ans);

NTSTATUS auth_ntlmssp_update(struct auth_ntlmssp_state *ans,
			     const DATA_BLOB request, DATA_BLOB *reply);

NTSTATUS auth_ntlmssp_client_start(TALLOC_CTX *mem_ctx,
				   const char *netbios_name,
				   const char *netbios_domain,
				   bool use_ntlmv2,
				   struct auth_ntlmssp_state **_ans);
#endif /* _NTLMSSP_WRAP_ */
