/*
   Unix SMB/Netbios implementation.
   Version 3.0
   handle NLTMSSP, server side

   Copyright (C) Andrew Tridgell      2001
   Copyright (C) Andrew Bartlett 2001-2010
   Copyright (C) Stefan Metzmacher 2005

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
#include "../libcli/auth/ntlmssp.h"
#include "../libcli/auth/ntlmssp_private.h"
#include "../libcli/auth/libcli_auth.h"
#include "../librpc/gen_ndr/ndr_ntlmssp.h"
#include "../libcli/auth/ntlmssp_ndr.h"
#include "../lib/crypto/md5.h"
#include "../lib/crypto/arcfour.h"
#include "../lib/crypto/hmacmd5.h"
#include "../nsswitch/libwbclient/wbclient.h"

static NTSTATUS ntlmssp_client_initial(struct ntlmssp_state *ntlmssp_state,
				       TALLOC_CTX *out_mem_ctx, /* Unused at this time */
				       DATA_BLOB reply, DATA_BLOB *next_request);
static NTSTATUS ntlmssp_client_challenge(struct ntlmssp_state *ntlmssp_state,
				         TALLOC_CTX *out_mem_ctx, /* Unused at this time */
					 const DATA_BLOB reply, DATA_BLOB *next_request);
/**
 * Callbacks for NTLMSSP - for both client and server operating modes
 *
 */

static const struct ntlmssp_callbacks {
	enum ntlmssp_role role;
	enum ntlmssp_message_type ntlmssp_command;
	NTSTATUS (*fn)(struct ntlmssp_state *ntlmssp_state,
		       TALLOC_CTX *out_mem_ctx,
		       DATA_BLOB in, DATA_BLOB *out);
} ntlmssp_callbacks[] = {
	{NTLMSSP_CLIENT, NTLMSSP_INITIAL, ntlmssp_client_initial},
	{NTLMSSP_SERVER, NTLMSSP_NEGOTIATE, ntlmssp_server_negotiate},
	{NTLMSSP_CLIENT, NTLMSSP_CHALLENGE, ntlmssp_client_challenge},
	{NTLMSSP_SERVER, NTLMSSP_AUTH, ntlmssp_server_auth},
	{NTLMSSP_CLIENT, NTLMSSP_UNKNOWN, NULL},
	{NTLMSSP_SERVER, NTLMSSP_UNKNOWN, NULL}
};


/**
 * Default challenge generation code.
 *
 */

static NTSTATUS get_challenge(const struct ntlmssp_state *ntlmssp_state,
			      uint8_t chal[8])
{
	generate_random_buffer(chal, 8);
	return NT_STATUS_OK;
}

/**
 * Default 'we can set the challenge to anything we like' implementation
 *
 */

static bool may_set_challenge(const struct ntlmssp_state *ntlmssp_state)
{
	return True;
}

/**
 * Default 'we can set the challenge to anything we like' implementation
 *
 * Does not actually do anything, as the value is always in the structure anyway.
 *
 */

static NTSTATUS set_challenge(struct ntlmssp_state *ntlmssp_state, DATA_BLOB *challenge)
{
	SMB_ASSERT(challenge->length == 8);
	return NT_STATUS_OK;
}

/**
 * Set a username on an NTLMSSP context - ensures it is talloc()ed
 *
 */

NTSTATUS ntlmssp_set_username(struct ntlmssp_state *ntlmssp_state, const char *user)
{
	ntlmssp_state->user = talloc_strdup(ntlmssp_state, user ? user : "" );
	if (!ntlmssp_state->user) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}

/**
 * Store NT and LM hashes on an NTLMSSP context - ensures they are talloc()ed
 *
 */
NTSTATUS ntlmssp_set_hashes(struct ntlmssp_state *ntlmssp_state,
			    const uint8_t lm_hash[16],
			    const uint8_t nt_hash[16])
{
	ntlmssp_state->lm_hash = (uint8_t *)
		TALLOC_MEMDUP(ntlmssp_state, lm_hash, 16);
	ntlmssp_state->nt_hash = (uint8_t *)
		TALLOC_MEMDUP(ntlmssp_state, nt_hash, 16);
	if (!ntlmssp_state->lm_hash || !ntlmssp_state->nt_hash) {
		TALLOC_FREE(ntlmssp_state->lm_hash);
		TALLOC_FREE(ntlmssp_state->nt_hash);
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}

/**
 * Converts a password to the hashes on an NTLMSSP context.
 *
 */
NTSTATUS ntlmssp_set_password(struct ntlmssp_state *ntlmssp_state, const char *password)
{
	if (!password) {
		ntlmssp_state->lm_hash = NULL;
		ntlmssp_state->nt_hash = NULL;
	} else {
		uint8_t lm_hash[16];
		uint8_t nt_hash[16];

		E_deshash(password, lm_hash);
		E_md4hash(password, nt_hash);
		return ntlmssp_set_hashes(ntlmssp_state, lm_hash, nt_hash);
	}
	return NT_STATUS_OK;
}

/**
 * Set a domain on an NTLMSSP context - ensures it is talloc()ed
 *
 */
NTSTATUS ntlmssp_set_domain(struct ntlmssp_state *ntlmssp_state, const char *domain)
{
	ntlmssp_state->domain = talloc_strdup(ntlmssp_state,
					      domain ? domain : "" );
	if (!ntlmssp_state->domain) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}

bool ntlmssp_have_feature(struct ntlmssp_state *ntlmssp_state,
			  uint32_t feature)
{
	if (feature & NTLMSSP_FEATURE_SIGN) {
		if (ntlmssp_state->session_key.length == 0) {
			return false;
		}
		if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SIGN) {
			return true;
		}
	}

	if (feature & NTLMSSP_FEATURE_SEAL) {
		if (ntlmssp_state->session_key.length == 0) {
			return false;
		}
		if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SEAL) {
			return true;
		}
	}

	if (feature & NTLMSSP_FEATURE_SESSION_KEY) {
		if (ntlmssp_state->session_key.length > 0) {
			return true;
		}
	}

	return false;
}

/**
 * Request features for the NTLMSSP negotiation
 *
 * @param ntlmssp_state NTLMSSP state
 * @param feature_list List of space seperated features requested from NTLMSSP.
 */
void ntlmssp_want_feature_list(struct ntlmssp_state *ntlmssp_state, char *feature_list)
{
	/*
	 * We need to set this to allow a later SetPassword
	 * via the SAMR pipe to succeed. Strange.... We could
	 * also add  NTLMSSP_NEGOTIATE_SEAL here. JRA.
	 */
	if (in_list("NTLMSSP_FEATURE_SESSION_KEY", feature_list, True)) {
		ntlmssp_state->required_flags |= NTLMSSP_NEGOTIATE_SIGN;
	}
	if (in_list("NTLMSSP_FEATURE_SIGN", feature_list, True)) {
		ntlmssp_state->required_flags |= NTLMSSP_NEGOTIATE_SIGN;
	}
	if(in_list("NTLMSSP_FEATURE_SEAL", feature_list, True)) {
		ntlmssp_state->required_flags |= NTLMSSP_NEGOTIATE_SEAL;
	}
	if (in_list("NTLMSSP_FEATURE_CCACHE", feature_list, true)) {
		ntlmssp_state->use_ccache = true;
	}

	ntlmssp_state->neg_flags |= ntlmssp_state->required_flags;
}

/**
 * Request a feature for the NTLMSSP negotiation
 *
 * @param ntlmssp_state NTLMSSP state
 * @param feature Bit flag specifying the requested feature
 */
void ntlmssp_want_feature(struct ntlmssp_state *ntlmssp_state, uint32_t feature)
{
	/* As per JRA's comment above */
	if (feature & NTLMSSP_FEATURE_SESSION_KEY) {
		ntlmssp_state->required_flags |= NTLMSSP_NEGOTIATE_SIGN;
	}
	if (feature & NTLMSSP_FEATURE_SIGN) {
		ntlmssp_state->required_flags |= NTLMSSP_NEGOTIATE_SIGN;
	}
	if (feature & NTLMSSP_FEATURE_SEAL) {
		ntlmssp_state->required_flags |= NTLMSSP_NEGOTIATE_SIGN;
		ntlmssp_state->required_flags |= NTLMSSP_NEGOTIATE_SEAL;
	}
	if (feature & NTLMSSP_FEATURE_CCACHE) {
		ntlmssp_state->use_ccache = true;
	}

	ntlmssp_state->neg_flags |= ntlmssp_state->required_flags;
}

/**
 * Next state function for the NTLMSSP state machine
 *
 * @param ntlmssp_state NTLMSSP State
 * @param in The packet in from the NTLMSSP partner, as a DATA_BLOB
 * @param out The reply, as an allocated DATA_BLOB, caller to free.
 * @return Errors, NT_STATUS_MORE_PROCESSING_REQUIRED or NT_STATUS_OK.
 */

NTSTATUS ntlmssp_update(struct ntlmssp_state *ntlmssp_state,
			const DATA_BLOB input, DATA_BLOB *out)
{
	uint32_t ntlmssp_command;
	int i;

	if (ntlmssp_state->expected_state == NTLMSSP_DONE) {
		/* Called update after negotiations finished. */
		DEBUG(1, ("Called NTLMSSP after state machine was 'done'\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	*out = data_blob_null;

	if (!input.length) {
		switch (ntlmssp_state->role) {
		case NTLMSSP_CLIENT:
			ntlmssp_command = NTLMSSP_INITIAL;
			break;
		case NTLMSSP_SERVER:
			/* 'datagram' mode - no neg packet */
			ntlmssp_command = NTLMSSP_NEGOTIATE;
			break;
		default:
			DEBUG(1, ("Invalid role: %d\n", ntlmssp_state->role));
			return NT_STATUS_INVALID_PARAMETER;
		}
	} else {
		if (!msrpc_parse(ntlmssp_state, &input, "Cd",
				 "NTLMSSP",
				 &ntlmssp_command)) {
			DEBUG(1, ("Failed to parse NTLMSSP packet, could not extract NTLMSSP command\n"));
			dump_data(2, input.data, input.length);
			return NT_STATUS_INVALID_PARAMETER;
		}
	}

	if (ntlmssp_command != ntlmssp_state->expected_state) {
		DEBUG(1, ("got NTLMSSP command %u, expected %u\n", ntlmssp_command, ntlmssp_state->expected_state));
		return NT_STATUS_INVALID_PARAMETER;
	}

	for (i=0; ntlmssp_callbacks[i].fn; i++) {
		if (ntlmssp_callbacks[i].role == ntlmssp_state->role
		    && ntlmssp_callbacks[i].ntlmssp_command == ntlmssp_command) {
			return ntlmssp_callbacks[i].fn(ntlmssp_state, ntlmssp_state, input, out);
		}
	}

	DEBUG(1, ("failed to find NTLMSSP callback for NTLMSSP mode %u, command %u\n",
		  ntlmssp_state->role, ntlmssp_command));

	return NT_STATUS_INVALID_PARAMETER;
}

/**
 * Create an NTLMSSP state machine
 *
 * @param ntlmssp_state NTLMSSP State, allocated by this function
 */

NTSTATUS ntlmssp_server_start(TALLOC_CTX *mem_ctx,
			      bool is_standalone,
			      const char *netbios_name,
			      const char *netbios_domain,
			      const char *dns_name,
			      const char *dns_domain,
			      struct ntlmssp_state **_ntlmssp_state)
{
	struct ntlmssp_state *ntlmssp_state;

	if (!netbios_name) {
		netbios_name = "";
	}

	if (!netbios_domain) {
		netbios_domain = "";
	}

	if (!dns_domain) {
		dns_domain = "";
	}

	if (!dns_name) {
		dns_name = "";
	}

	ntlmssp_state = talloc_zero(mem_ctx, struct ntlmssp_state);
	if (!ntlmssp_state) {
		return NT_STATUS_NO_MEMORY;
	}

	ntlmssp_state->role = NTLMSSP_SERVER;

	ntlmssp_state->get_challenge = get_challenge;
	ntlmssp_state->set_challenge = set_challenge;
	ntlmssp_state->may_set_challenge = may_set_challenge;

	ntlmssp_state->server.is_standalone = is_standalone;

	ntlmssp_state->expected_state = NTLMSSP_NEGOTIATE;

	ntlmssp_state->allow_lm_key = lp_lanman_auth();

	ntlmssp_state->neg_flags =
		NTLMSSP_NEGOTIATE_128 |
		NTLMSSP_NEGOTIATE_56 |
		NTLMSSP_NEGOTIATE_VERSION |
		NTLMSSP_NEGOTIATE_ALWAYS_SIGN |
		NTLMSSP_NEGOTIATE_NTLM |
		NTLMSSP_NEGOTIATE_NTLM2 |
		NTLMSSP_NEGOTIATE_KEY_EXCH |
		NTLMSSP_NEGOTIATE_SIGN |
		NTLMSSP_NEGOTIATE_SEAL;

	ntlmssp_state->server.netbios_name = talloc_strdup(ntlmssp_state, netbios_name);
	if (!ntlmssp_state->server.netbios_name) {
		talloc_free(ntlmssp_state);
		return NT_STATUS_NO_MEMORY;
	}
	ntlmssp_state->server.netbios_domain = talloc_strdup(ntlmssp_state, netbios_domain);
	if (!ntlmssp_state->server.netbios_domain) {
		talloc_free(ntlmssp_state);
		return NT_STATUS_NO_MEMORY;
	}
	ntlmssp_state->server.dns_name = talloc_strdup(ntlmssp_state, dns_name);
	if (!ntlmssp_state->server.dns_name) {
		talloc_free(ntlmssp_state);
		return NT_STATUS_NO_MEMORY;
	}
	ntlmssp_state->server.dns_domain = talloc_strdup(ntlmssp_state, dns_domain);
	if (!ntlmssp_state->server.dns_domain) {
		talloc_free(ntlmssp_state);
		return NT_STATUS_NO_MEMORY;
	}

	*_ntlmssp_state = ntlmssp_state;
	return NT_STATUS_OK;
}

/*********************************************************************
 Client side NTLMSSP
*********************************************************************/

/**
 * Next state function for the Initial packet
 *
 * @param ntlmssp_state NTLMSSP State
 * @param request The request, as a DATA_BLOB.  reply.data must be NULL
 * @param request The reply, as an allocated DATA_BLOB, caller to free.
 * @return Errors or NT_STATUS_OK.
 */

static NTSTATUS ntlmssp_client_initial(struct ntlmssp_state *ntlmssp_state,
				  TALLOC_CTX *out_mem_ctx, /* Unused at this time */
				  DATA_BLOB reply, DATA_BLOB *next_request)
{
	NTSTATUS status;

	if (ntlmssp_state->unicode) {
		ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_UNICODE;
	} else {
		ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_OEM;
	}

	if (ntlmssp_state->use_ntlmv2) {
		ntlmssp_state->required_flags |= NTLMSSP_NEGOTIATE_NTLM2;
		ntlmssp_state->allow_lm_key = false;
	}

	if (ntlmssp_state->allow_lm_key) {
		ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_LM_KEY;
	}

	/* generate the ntlmssp negotiate packet */
	status = msrpc_gen(ntlmssp_state, next_request, "CddAA",
		  "NTLMSSP",
		  NTLMSSP_NEGOTIATE,
		  ntlmssp_state->neg_flags,
		  ntlmssp_state->client.netbios_domain,
		  ntlmssp_state->client.netbios_name);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("ntlmssp_client_initial: failed to generate "
			"ntlmssp negotiate packet\n"));
		return status;
	}

	if (DEBUGLEVEL >= 10) {
		struct NEGOTIATE_MESSAGE *negotiate = talloc(
			talloc_tos(), struct NEGOTIATE_MESSAGE);
		if (negotiate != NULL) {
			status = ntlmssp_pull_NEGOTIATE_MESSAGE(
				next_request, negotiate, negotiate);
			if (NT_STATUS_IS_OK(status)) {
				NDR_PRINT_DEBUG(NEGOTIATE_MESSAGE,
						negotiate);
			}
			TALLOC_FREE(negotiate);
		}
	}

	ntlmssp_state->expected_state = NTLMSSP_CHALLENGE;

	return NT_STATUS_MORE_PROCESSING_REQUIRED;
}

static NTSTATUS ntlmssp3_handle_neg_flags(struct ntlmssp_state *ntlmssp_state,
					  uint32_t flags)
{
	uint32_t missing_flags = ntlmssp_state->required_flags;

	if (flags & NTLMSSP_NEGOTIATE_UNICODE) {
		ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_UNICODE;
		ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_OEM;
		ntlmssp_state->unicode = true;
	} else {
		ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_UNICODE;
		ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_OEM;
		ntlmssp_state->unicode = false;
	}

	/*
	 * NTLMSSP_NEGOTIATE_NTLM2 (NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY)
	 * has priority over NTLMSSP_NEGOTIATE_LM_KEY
	 */
	if (!(flags & NTLMSSP_NEGOTIATE_NTLM2)) {
		ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_NTLM2;
	}

	if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2) {
		ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_LM_KEY;
	}

	if (!(flags & NTLMSSP_NEGOTIATE_LM_KEY)) {
		ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_LM_KEY;
	}

	if (!(flags & NTLMSSP_NEGOTIATE_ALWAYS_SIGN)) {
		ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
	}

	if (!(flags & NTLMSSP_NEGOTIATE_128)) {
		ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_128;
	}

	if (!(flags & NTLMSSP_NEGOTIATE_56)) {
		ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_56;
	}

	if (!(flags & NTLMSSP_NEGOTIATE_KEY_EXCH)) {
		ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_KEY_EXCH;
	}

	if (!(flags & NTLMSSP_NEGOTIATE_SIGN)) {
		ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_SIGN;
	}

	if (!(flags & NTLMSSP_NEGOTIATE_SEAL)) {
		ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_SEAL;
	}

	if ((flags & NTLMSSP_REQUEST_TARGET)) {
		ntlmssp_state->neg_flags |= NTLMSSP_REQUEST_TARGET;
	}

	missing_flags &= ~ntlmssp_state->neg_flags;
	if (missing_flags != 0) {
		NTSTATUS status = NT_STATUS_RPC_SEC_PKG_ERROR;
		DEBUG(1, ("%s: Got challenge flags[0x%08x] "
			  "- possible downgrade detected! "
			  "missing_flags[0x%08x] - %s\n",
			  __func__,
			  (unsigned)flags,
			  (unsigned)missing_flags,
			  nt_errstr(status)));
		debug_ntlmssp_flags(missing_flags);
		DEBUGADD(4, ("neg_flags[0x%08x]\n",
			     (unsigned)ntlmssp_state->neg_flags));
		debug_ntlmssp_flags(ntlmssp_state->neg_flags);

		return status;
	}

	return NT_STATUS_OK;
}

/**
 * Next state function for the Challenge Packet.  Generate an auth packet.
 *
 * @param ntlmssp_state NTLMSSP State
 * @param request The request, as a DATA_BLOB.  reply.data must be NULL
 * @param request The reply, as an allocated DATA_BLOB, caller to free.
 * @return Errors or NT_STATUS_OK.
 */

static NTSTATUS ntlmssp_client_challenge(struct ntlmssp_state *ntlmssp_state,
				         TALLOC_CTX *out_mem_ctx, /* Unused at this time */
					 const DATA_BLOB reply, DATA_BLOB *next_request)
{
	uint32_t chal_flags, ntlmssp_command, unkn1, unkn2;
	DATA_BLOB server_domain_blob;
	DATA_BLOB challenge_blob;
	DATA_BLOB struct_blob = data_blob_null;
	char *server_domain;
	const char *chal_parse_string;
	const char *auth_gen_string;
	DATA_BLOB lm_response = data_blob_null;
	DATA_BLOB nt_response = data_blob_null;
	DATA_BLOB session_key = data_blob_null;
	DATA_BLOB encrypted_session_key = data_blob_null;
	NTSTATUS nt_status = NT_STATUS_OK;

	if (!msrpc_parse(ntlmssp_state, &reply, "CdBd",
			 "NTLMSSP",
			 &ntlmssp_command,
			 &server_domain_blob,
			 &chal_flags)) {
		DEBUG(1, ("Failed to parse the NTLMSSP Challenge: (#1)\n"));
		dump_data(2, reply.data, reply.length);

		return NT_STATUS_INVALID_PARAMETER;
	}
	data_blob_free(&server_domain_blob);

	DEBUG(3, ("Got challenge flags:\n"));
	debug_ntlmssp_flags(chal_flags);

	nt_status = ntlmssp3_handle_neg_flags(ntlmssp_state, chal_flags);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	if (ntlmssp_state->use_ccache) {
		struct wbcCredentialCacheParams params;
		struct wbcCredentialCacheInfo *info = NULL;
		struct wbcAuthErrorInfo *error = NULL;
		struct wbcNamedBlob auth_blob;
		struct wbcBlob *wbc_next = NULL;
		struct wbcBlob *wbc_session_key = NULL;
		wbcErr wbc_status;
		int i;

		params.account_name = ntlmssp_state->user;
		params.domain_name = ntlmssp_state->domain;
		params.level = WBC_CREDENTIAL_CACHE_LEVEL_NTLMSSP;

		auth_blob.name = "challenge_blob";
		auth_blob.flags = 0;
		auth_blob.blob.data = reply.data;
		auth_blob.blob.length = reply.length;
		params.num_blobs = 1;
		params.blobs = &auth_blob;

		wbc_status = wbcCredentialCache(&params, &info, &error);
		wbcFreeMemory(error);
		if (!WBC_ERROR_IS_OK(wbc_status)) {
			goto noccache;
		}

		for (i=0; i<info->num_blobs; i++) {
			if (strequal(info->blobs[i].name, "auth_blob")) {
				wbc_next = &info->blobs[i].blob;
			}
			if (strequal(info->blobs[i].name, "session_key")) {
				wbc_session_key = &info->blobs[i].blob;
			}
		}
		if ((wbc_next == NULL) || (wbc_session_key == NULL)) {
			wbcFreeMemory(info);
			goto noccache;
		}

		*next_request = data_blob(wbc_next->data, wbc_next->length);
		ntlmssp_state->session_key = data_blob(
			wbc_session_key->data, wbc_session_key->length);

		wbcFreeMemory(info);
		goto done;
	}

noccache:

	if (DEBUGLEVEL >= 10) {
		struct CHALLENGE_MESSAGE *challenge = talloc(
			talloc_tos(), struct CHALLENGE_MESSAGE);
		if (challenge != NULL) {
			NTSTATUS status;
			challenge->NegotiateFlags = chal_flags;
			status = ntlmssp_pull_CHALLENGE_MESSAGE(
				&reply, challenge, challenge);
			if (NT_STATUS_IS_OK(status)) {
				NDR_PRINT_DEBUG(CHALLENGE_MESSAGE,
						challenge);
			}
			TALLOC_FREE(challenge);
		}
	}

	if (ntlmssp_state->unicode) {
		if (chal_flags & NTLMSSP_NEGOTIATE_TARGET_INFO) {
			chal_parse_string = "CdUdbddB";
		} else {
			chal_parse_string = "CdUdbdd";
		}
		auth_gen_string = "CdBBUUUBd";
	} else {
		if (chal_flags & NTLMSSP_NEGOTIATE_TARGET_INFO) {
			chal_parse_string = "CdAdbddB";
		} else {
			chal_parse_string = "CdAdbdd";
		}

		auth_gen_string = "CdBBAAABd";
	}

	DEBUG(3, ("NTLMSSP: Set final flags:\n"));
	debug_ntlmssp_flags(ntlmssp_state->neg_flags);

	if (!msrpc_parse(ntlmssp_state, &reply, chal_parse_string,
			 "NTLMSSP",
			 &ntlmssp_command,
			 &server_domain,
			 &chal_flags,
			 &challenge_blob, 8,
			 &unkn1, &unkn2,
			 &struct_blob)) {
		DEBUG(1, ("Failed to parse the NTLMSSP Challenge: (#2)\n"));
		dump_data(2, reply.data, reply.length);
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (chal_flags & NTLMSSP_TARGET_TYPE_SERVER) {
		ntlmssp_state->server.is_standalone = true;
	} else {
		ntlmssp_state->server.is_standalone = false;
	}
	/* TODO: parse struct_blob and fill in the rest */
	ntlmssp_state->server.netbios_name = "";
	ntlmssp_state->server.netbios_domain = server_domain;
	ntlmssp_state->server.dns_name = "";
	ntlmssp_state->server.dns_domain = "";

	if (challenge_blob.length != 8) {
		data_blob_free(&struct_blob);
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!ntlmssp_state->nt_hash || !ntlmssp_state->lm_hash) {
		static const uint8_t zeros[16] = {0, };
		/* do nothing - blobs are zero length */

		/* session key is all zeros */
		session_key = data_blob_talloc(ntlmssp_state, zeros, 16);

		/* not doing NLTM2 without a password */
		ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_NTLM2;
	} else if (ntlmssp_state->use_ntlmv2) {
		if (!struct_blob.length) {
			/* be lazy, match win2k - we can't do NTLMv2 without it */
			DEBUG(1, ("Server did not provide 'target information', required for NTLMv2\n"));
			return NT_STATUS_INVALID_PARAMETER;
		}

		/* TODO: if the remote server is standalone, then we should replace 'domain'
		   with the server name as supplied above */

		if (!SMBNTLMv2encrypt_hash(ntlmssp_state,
					   ntlmssp_state->user,
					   ntlmssp_state->domain,
					   ntlmssp_state->nt_hash, &challenge_blob,
					   &struct_blob,
					   &lm_response, &nt_response, NULL,
					   &session_key)) {
			data_blob_free(&challenge_blob);
			data_blob_free(&struct_blob);
			return NT_STATUS_NO_MEMORY;
		}
	} else if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2) {
		MD5_CTX md5_session_nonce_ctx;
		uint8_t session_nonce[16];
		uint8_t session_nonce_hash[16];
		uint8_t user_session_key[16];

		lm_response = data_blob_talloc(ntlmssp_state, NULL, 24);
		generate_random_buffer(lm_response.data, 8);
		memset(lm_response.data+8, 0, 16);

		memcpy(session_nonce, challenge_blob.data, 8);
		memcpy(&session_nonce[8], lm_response.data, 8);

		MD5Init(&md5_session_nonce_ctx);
		MD5Update(&md5_session_nonce_ctx, challenge_blob.data, 8);
		MD5Update(&md5_session_nonce_ctx, lm_response.data, 8);
		MD5Final(session_nonce_hash, &md5_session_nonce_ctx);

		DEBUG(5, ("NTLMSSP challenge set by NTLM2\n"));
		DEBUG(5, ("challenge is: \n"));
		dump_data(5, session_nonce_hash, 8);

		nt_response = data_blob_talloc(ntlmssp_state, NULL, 24);
		SMBNTencrypt_hash(ntlmssp_state->nt_hash,
				  session_nonce_hash,
				  nt_response.data);

		session_key = data_blob_talloc(ntlmssp_state, NULL, 16);

		SMBsesskeygen_ntv1(ntlmssp_state->nt_hash, user_session_key);
		hmac_md5(user_session_key, session_nonce, sizeof(session_nonce), session_key.data);
		dump_data_pw("NTLM2 session key:\n", session_key.data, session_key.length);
	} else {
		/* lanman auth is insecure, it may be disabled */
		if (lp_client_lanman_auth()) {
			lm_response = data_blob_talloc(ntlmssp_state,
						       NULL, 24);
			SMBencrypt_hash(ntlmssp_state->lm_hash,challenge_blob.data,
				   lm_response.data);
		}

		nt_response = data_blob_talloc(ntlmssp_state, NULL, 24);
		SMBNTencrypt_hash(ntlmssp_state->nt_hash,challenge_blob.data,
			     nt_response.data);

		session_key = data_blob_talloc(ntlmssp_state, NULL, 16);
		if ((ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_LM_KEY)
		    && lp_client_lanman_auth()) {
			SMBsesskeygen_lm_sess_key(ntlmssp_state->lm_hash, lm_response.data,
					session_key.data);
			dump_data_pw("LM session key\n", session_key.data, session_key.length);
		} else {
			SMBsesskeygen_ntv1(ntlmssp_state->nt_hash, session_key.data);
			dump_data_pw("NT session key:\n", session_key.data, session_key.length);
		}
	}
	data_blob_free(&struct_blob);

	/* Key exchange encryptes a new client-generated session key with
	   the password-derived key */
	if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_KEY_EXCH) {
		/* Make up a new session key */
		uint8_t client_session_key[16];
		generate_random_buffer(client_session_key, sizeof(client_session_key));

		/* Encrypt the new session key with the old one */
		encrypted_session_key = data_blob(client_session_key, sizeof(client_session_key));
		dump_data_pw("KEY_EXCH session key:\n", encrypted_session_key.data, encrypted_session_key.length);
		arcfour_crypt_blob(encrypted_session_key.data, encrypted_session_key.length, &session_key);
		dump_data_pw("KEY_EXCH session key (enc):\n", encrypted_session_key.data, encrypted_session_key.length);

		/* Mark the new session key as the 'real' session key */
		data_blob_free(&session_key);
		session_key = data_blob_talloc(ntlmssp_state,
					       client_session_key,
					       sizeof(client_session_key));
	}

	/* this generates the actual auth packet */
	nt_status = msrpc_gen(ntlmssp_state, next_request, auth_gen_string,
		       "NTLMSSP",
		       NTLMSSP_AUTH,
		       lm_response.data, lm_response.length,
		       nt_response.data, nt_response.length,
		       ntlmssp_state->domain,
		       ntlmssp_state->user,
		       ntlmssp_state->client.netbios_name,
		       encrypted_session_key.data, encrypted_session_key.length,
		       ntlmssp_state->neg_flags);

	if (!NT_STATUS_IS_OK(nt_status)) {
		return NT_STATUS_NO_MEMORY;
	}

	if (DEBUGLEVEL >= 10) {
		struct AUTHENTICATE_MESSAGE *authenticate = talloc(
			talloc_tos(), struct AUTHENTICATE_MESSAGE);
		if (authenticate != NULL) {
			NTSTATUS status;
			authenticate->NegotiateFlags =
				ntlmssp_state->neg_flags;
			status = ntlmssp_pull_AUTHENTICATE_MESSAGE(
				next_request, authenticate, authenticate);
			if (NT_STATUS_IS_OK(status)) {
				NDR_PRINT_DEBUG(AUTHENTICATE_MESSAGE,
						authenticate);
			}
			TALLOC_FREE(authenticate);
		}
	}

	data_blob_free(&encrypted_session_key);

	data_blob_free(&ntlmssp_state->chal);

	ntlmssp_state->session_key = session_key;

	ntlmssp_state->chal = challenge_blob;
	ntlmssp_state->lm_resp = lm_response;
	ntlmssp_state->nt_resp = nt_response;

done:

	ntlmssp_state->expected_state = NTLMSSP_DONE;

	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_sign_init(ntlmssp_state))) {
		DEBUG(1, ("Could not setup NTLMSSP signing/sealing system (error was: %s)\n", nt_errstr(nt_status)));
	}

	return nt_status;
}

NTSTATUS ntlmssp_client_start(TALLOC_CTX *mem_ctx,
			      const char *netbios_name,
			      const char *netbios_domain,
			      bool use_ntlmv2,
			      struct ntlmssp_state **_ntlmssp_state)
{
	struct ntlmssp_state *ntlmssp_state;

	if (!netbios_name) {
		netbios_name = "";
	}

	if (!netbios_domain) {
		netbios_domain = "";
	}

	ntlmssp_state = talloc_zero(mem_ctx, struct ntlmssp_state);
	if (!ntlmssp_state) {
		return NT_STATUS_NO_MEMORY;
	}

	ntlmssp_state->role = NTLMSSP_CLIENT;

	ntlmssp_state->unicode = True;

	ntlmssp_state->use_ntlmv2 = use_ntlmv2;
	ntlmssp_state->allow_lm_key = lp_client_lanman_auth();

	ntlmssp_state->expected_state = NTLMSSP_INITIAL;

	ntlmssp_state->neg_flags =
		NTLMSSP_NEGOTIATE_128 |
		NTLMSSP_NEGOTIATE_ALWAYS_SIGN |
		NTLMSSP_NEGOTIATE_NTLM |
		NTLMSSP_NEGOTIATE_NTLM2 |
		NTLMSSP_NEGOTIATE_KEY_EXCH |
		NTLMSSP_REQUEST_TARGET;

	if (ntlmssp_state->use_ntlmv2) {
		ntlmssp_state->allow_lm_key = false;
	}

	ntlmssp_state->client.netbios_name = talloc_strdup(ntlmssp_state, netbios_name);
	if (!ntlmssp_state->client.netbios_name) {
		talloc_free(ntlmssp_state);
		return NT_STATUS_NO_MEMORY;
	}
	ntlmssp_state->client.netbios_domain = talloc_strdup(ntlmssp_state, netbios_domain);
	if (!ntlmssp_state->client.netbios_domain) {
		talloc_free(ntlmssp_state);
		return NT_STATUS_NO_MEMORY;
	}

	*_ntlmssp_state = ntlmssp_state;
	return NT_STATUS_OK;
}
