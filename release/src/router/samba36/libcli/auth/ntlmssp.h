/*
   Unix SMB/CIFS implementation.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell 1992-1997
   Copyright (C) Luke Kenneth Casson Leighton 1996-1997
   Copyright (C) Paul Ashton 1997
   Copyright (C) Andrew Bartlett 2010

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

#include "../librpc/gen_ndr/ntlmssp.h"

/* NTLMSSP mode */
enum ntlmssp_role
{
	NTLMSSP_SERVER,
	NTLMSSP_CLIENT
};

/* NTLMSSP message types */
enum ntlmssp_message_type
{
	NTLMSSP_INITIAL = 0 /* samba internal state */,
	NTLMSSP_NEGOTIATE = 1,
	NTLMSSP_CHALLENGE = 2,
	NTLMSSP_AUTH      = 3,
	NTLMSSP_UNKNOWN   = 4,
	NTLMSSP_DONE      = 5 /* samba final state */
};

#define NTLMSSP_FEATURE_SESSION_KEY        0x00000001
#define NTLMSSP_FEATURE_SIGN               0x00000002
#define NTLMSSP_FEATURE_SEAL               0x00000004
#define NTLMSSP_FEATURE_CCACHE		   0x00000008

union ntlmssp_crypt_state;

struct ntlmssp_state
{
	enum ntlmssp_role role;
	uint32_t expected_state;

	bool unicode;
	bool use_ntlmv2;
	bool use_ccache;
	bool use_nt_response;  /* Set to 'False' to debug what happens when the NT response is omited */
	bool allow_lm_key;     /* The LM_KEY code is not very secure... */

	const char *user;
	const char *domain;
	uint8_t *nt_hash;
	uint8_t *lm_hash;

	struct {
		const char *netbios_name;
		const char *netbios_domain;
	} client;

	struct {
		bool is_standalone;
		const char *netbios_name;
		const char *netbios_domain;
		const char *dns_name;
		const char *dns_domain;
	} server;

	DATA_BLOB internal_chal; /* Random challenge as supplied to the client for NTLM authentication */

	DATA_BLOB chal; /* Random challenge as input into the actual NTLM (or NTLM2) authentication */
	DATA_BLOB lm_resp;
	DATA_BLOB nt_resp;
	DATA_BLOB session_key;

	uint32_t required_flags;
	uint32_t neg_flags; /* the current state of negotiation with the NTLMSSP partner */

	/**
	 * Private data for the callback functions
	 */
	void *callback_private;

	/**
	 * Callback to get the 'challenge' used for NTLM authentication.
	 *
	 * @param ntlmssp_state This structure
	 * @return 8 bytes of challenge data, determined by the server to be the challenge for NTLM authentication
	 *
	 */
	NTSTATUS (*get_challenge)(const struct ntlmssp_state *ntlmssp_state,
				  uint8_t challenge[8]);

	/**
	 * Callback to find if the challenge used by NTLM authentication may be modified
	 *
	 * The NTLM2 authentication scheme modifies the effective challenge, but this is not compatiable with the
	 * current 'security=server' implementation..
	 *
	 * @param ntlmssp_state This structure
	 * @return Can the challenge be set to arbitary values?
	 *
	 */
	bool (*may_set_challenge)(const struct ntlmssp_state *ntlmssp_state);

	/**
	 * Callback to set the 'challenge' used for NTLM authentication.
	 *
	 * The callback may use the void *auth_context to store state information, but the same value is always available
	 * from the DATA_BLOB chal on this structure.
	 *
	 * @param ntlmssp_state This structure
	 * @param challenge 8 bytes of data, agreed by the client and server to be the effective challenge for NTLM2 authentication
	 *
	 */
	NTSTATUS (*set_challenge)(struct ntlmssp_state *ntlmssp_state, DATA_BLOB *challenge);

	/**
	 * Callback to check the user's password.
	 *
	 * The callback must reads the feilds of this structure for the information it needs on the user
	 * @param ntlmssp_state This structure
	 * @param mem_ctx Talloc context for LM and NT session key to be returned on
	 * @param nt_session_key If an NT session key is returned by the authentication process, return it here
	 * @param lm_session_key If an LM session key is returned by the authentication process, return it here
	 *
	 */
	NTSTATUS (*check_password)(struct ntlmssp_state *ntlmssp_state, TALLOC_CTX *mem_ctx,
				   DATA_BLOB *nt_session_key, DATA_BLOB *lm_session_key);

	union ntlmssp_crypt_state *crypt;
};

/* The following definitions come from libcli/auth/ntlmssp_sign.c  */

NTSTATUS ntlmssp_sign_packet(struct ntlmssp_state *ntlmssp_state,
			     TALLOC_CTX *sig_mem_ctx,
			     const uint8_t *data, size_t length,
			     const uint8_t *whole_pdu, size_t pdu_length,
			     DATA_BLOB *sig);
NTSTATUS ntlmssp_check_packet(struct ntlmssp_state *ntlmssp_state,
			      const uint8_t *data, size_t length,
			      const uint8_t *whole_pdu, size_t pdu_length,
			      const DATA_BLOB *sig) ;
NTSTATUS ntlmssp_seal_packet(struct ntlmssp_state *ntlmssp_state,
			     TALLOC_CTX *sig_mem_ctx,
			     uint8_t *data, size_t length,
			     const uint8_t *whole_pdu, size_t pdu_length,
			     DATA_BLOB *sig);
NTSTATUS ntlmssp_unseal_packet(struct ntlmssp_state *ntlmssp_state,
			       uint8_t *data, size_t length,
			       const uint8_t *whole_pdu, size_t pdu_length,
			       const DATA_BLOB *sig);
NTSTATUS ntlmssp_wrap(struct ntlmssp_state *ntlmssp_state,
		      TALLOC_CTX *out_mem_ctx,
		      const DATA_BLOB *in,
		      DATA_BLOB *out);
NTSTATUS ntlmssp_unwrap(struct ntlmssp_state *ntlmssp_stae,
			TALLOC_CTX *out_mem_ctx,
			const DATA_BLOB *in,
			DATA_BLOB *out);
NTSTATUS ntlmssp_sign_init(struct ntlmssp_state *ntlmssp_state);
