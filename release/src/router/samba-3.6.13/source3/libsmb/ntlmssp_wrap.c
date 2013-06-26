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

#include "includes.h"
#include "libcli/auth/ntlmssp.h"
#include "ntlmssp_wrap.h"

NTSTATUS auth_ntlmssp_sign_packet(struct auth_ntlmssp_state *ans,
				  TALLOC_CTX *sig_mem_ctx,
				  const uint8_t *data,
				  size_t length,
				  const uint8_t *whole_pdu,
				  size_t pdu_length,
				  DATA_BLOB *sig)
{
	return ntlmssp_sign_packet(ans->ntlmssp_state,
				   sig_mem_ctx,
				   data, length,
				   whole_pdu, pdu_length,
				   sig);
}

NTSTATUS auth_ntlmssp_check_packet(struct auth_ntlmssp_state *ans,
				   const uint8_t *data,
				   size_t length,
				   const uint8_t *whole_pdu,
				   size_t pdu_length,
				   const DATA_BLOB *sig)
{
	return ntlmssp_check_packet(ans->ntlmssp_state,
				    data, length,
				    whole_pdu, pdu_length,
				    sig);
}

NTSTATUS auth_ntlmssp_seal_packet(struct auth_ntlmssp_state *ans,
				  TALLOC_CTX *sig_mem_ctx,
				  uint8_t *data,
				  size_t length,
				  const uint8_t *whole_pdu,
				  size_t pdu_length,
				  DATA_BLOB *sig)
{
	return ntlmssp_seal_packet(ans->ntlmssp_state,
				   sig_mem_ctx,
				   data, length,
				   whole_pdu, pdu_length,
				   sig);
}

NTSTATUS auth_ntlmssp_unseal_packet(struct auth_ntlmssp_state *ans,
				    uint8_t *data,
				    size_t length,
				    const uint8_t *whole_pdu,
				    size_t pdu_length,
				    const DATA_BLOB *sig)
{
	return ntlmssp_unseal_packet(ans->ntlmssp_state,
				     data, length,
				     whole_pdu, pdu_length,
				     sig);
}

bool auth_ntlmssp_negotiated_sign(struct auth_ntlmssp_state *ans)
{
	return ans->ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SIGN;
}

bool auth_ntlmssp_negotiated_seal(struct auth_ntlmssp_state *ans)
{
	return ans->ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SEAL;
}

struct ntlmssp_state *auth_ntlmssp_get_ntlmssp_state(
					struct auth_ntlmssp_state *ans)
{
	return ans->ntlmssp_state;
}

/* Needed for 'map to guest' and 'smb username' processing */
const char *auth_ntlmssp_get_username(struct auth_ntlmssp_state *ans)
{
	return ans->ntlmssp_state->user;
}

const char *auth_ntlmssp_get_domain(struct auth_ntlmssp_state *ans)
{
	return ans->ntlmssp_state->domain;
}

const char *auth_ntlmssp_get_client(struct auth_ntlmssp_state *ans)
{
	return ans->ntlmssp_state->client.netbios_name;
}

const uint8_t *auth_ntlmssp_get_nt_hash(struct auth_ntlmssp_state *ans)
{
	return ans->ntlmssp_state->nt_hash;
}

NTSTATUS auth_ntlmssp_set_username(struct auth_ntlmssp_state *ans,
				   const char *user)
{
	return ntlmssp_set_username(ans->ntlmssp_state, user);
}

NTSTATUS auth_ntlmssp_set_domain(struct auth_ntlmssp_state *ans,
				 const char *domain)
{
	return ntlmssp_set_domain(ans->ntlmssp_state, domain);
}

NTSTATUS auth_ntlmssp_set_password(struct auth_ntlmssp_state *ans,
				   const char *password)
{
	return ntlmssp_set_password(ans->ntlmssp_state, password);
}

void auth_ntlmssp_and_flags(struct auth_ntlmssp_state *ans, uint32_t flags)
{
	ans->ntlmssp_state->neg_flags &= flags;
}

void auth_ntlmssp_or_flags(struct auth_ntlmssp_state *ans, uint32_t flags)
{
	ans->ntlmssp_state->neg_flags |= flags;
}

DATA_BLOB auth_ntlmssp_get_session_key(struct auth_ntlmssp_state *ans)
{
	return ans->ntlmssp_state->session_key;
}

NTSTATUS auth_ntlmssp_update(struct auth_ntlmssp_state *ans,
			     const DATA_BLOB request, DATA_BLOB *reply)
{
	return ntlmssp_update(ans->ntlmssp_state, request, reply);
}

NTSTATUS auth_ntlmssp_client_start(TALLOC_CTX *mem_ctx,
				   const char *netbios_name,
				   const char *netbios_domain,
				   bool use_ntlmv2,
				   struct auth_ntlmssp_state **_ans)
{
	struct auth_ntlmssp_state *ans;
	NTSTATUS status;

	ans = talloc_zero(mem_ctx, struct auth_ntlmssp_state);

	status = ntlmssp_client_start(ans,
					netbios_name, netbios_domain,
					use_ntlmv2, &ans->ntlmssp_state);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	*_ans = ans;
	return NT_STATUS_OK;
}
