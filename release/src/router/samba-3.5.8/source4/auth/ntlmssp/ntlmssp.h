/* 
   Unix SMB/CIFS implementation.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell 1992-1997
   Copyright (C) Luke Kenneth Casson Leighton 1996-1997
   Copyright (C) Paul Ashton 1997
   
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

#include "librpc/gen_ndr/samr.h"
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
	NTLMSSP_DONE   = 5 /* samba final state */
};

struct gensec_ntlmssp_state
{
	struct gensec_security *gensec_security;

	enum ntlmssp_role role;
	enum samr_Role server_role;
	uint32_t expected_state;

	bool unicode;
	bool use_ntlmv2;
	bool use_nt_response;  /* Set to 'False' to debug what happens when the NT response is omited */
	bool allow_lm_key;     /* The LM_KEY code is not functional at this point, and it's not 
				  very secure anyway */

	bool server_multiple_authentications;  /* Set to 'True' to allow squid 2.5 
						  style 'challenge caching' */

	char *user;
	const char *domain;
	const char *workstation;
	char *server_domain;

	DATA_BLOB internal_chal; /* Random challenge as supplied to the client for NTLM authentication */

	DATA_BLOB chal; /* Random challenge as input into the actual NTLM (or NTLM2) authentication */
 	DATA_BLOB lm_resp;
	DATA_BLOB nt_resp;
	DATA_BLOB session_key;
	
	uint32_t neg_flags; /* the current state of negotiation with the NTLMSSP partner */

	/* internal variables used by KEY_EXCH (client-supplied user session key */
	DATA_BLOB encrypted_session_key;
	
	/**
	 * Callback to get the 'challenge' used for NTLM authentication.  
	 *
	 * @param ntlmssp_state This structure
	 * @return 8 bytes of challenge data, determined by the server to be the challenge for NTLM authentication
	 *
	 */
	const uint8_t *(*get_challenge)(const struct gensec_ntlmssp_state *);

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
	bool (*may_set_challenge)(const struct gensec_ntlmssp_state *);

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
	NTSTATUS (*set_challenge)(struct gensec_ntlmssp_state *, DATA_BLOB *challenge);

	/**
	 * Callback to check the user's password.  
	 *
	 * The callback must reads the feilds of this structure for the information it needs on the user 
	 * @param ntlmssp_state This structure
	 * @param nt_session_key If an NT session key is returned by the authentication process, return it here
	 * @param lm_session_key If an LM session key is returned by the authentication process, return it here
	 *
	 */
	NTSTATUS (*check_password)(struct gensec_ntlmssp_state *, 
				   TALLOC_CTX *mem_ctx, 
				   DATA_BLOB *nt_session_key, DATA_BLOB *lm_session_key);

	const char *server_name;

	bool doing_ntlm2; 

	union {
		/* NTLM */
		struct {
			uint32_t seq_num;
			struct arcfour_state *arcfour_state;
		} ntlm;

		/* NTLM2 */
		struct {
			uint32_t send_seq_num;
			uint32_t recv_seq_num;
			DATA_BLOB send_sign_key;
			DATA_BLOB recv_sign_key;
			struct arcfour_state *send_seal_arcfour_state;
			struct arcfour_state *recv_seal_arcfour_state;

			/* internal variables used by NTLM2 */
			uint8_t session_nonce[16];
		} ntlm2;
	} crypt;

	struct auth_context *auth_context;
	struct auth_serversupplied_info *server_info;
};

struct loadparm_context;
struct auth_session_info;

#include "auth/ntlmssp/proto.h"
