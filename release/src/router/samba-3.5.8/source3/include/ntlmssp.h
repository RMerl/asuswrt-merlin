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

/* NTLMSSP mode */
enum NTLMSSP_ROLE
{
	NTLMSSP_SERVER,
	NTLMSSP_CLIENT
};

/* NTLMSSP message types */
enum NTLM_MESSAGE_TYPE
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

typedef struct ntlmssp_state
{
	unsigned int ref_count;
	enum NTLMSSP_ROLE role;
	enum server_types server_role;
	uint32 expected_state;

	bool unicode;
	bool use_ntlmv2;
	bool use_ccache;
	char *user;
	char *domain;
	char *workstation;
	unsigned char *nt_hash;
	unsigned char *lm_hash;
	char *server_domain;

	DATA_BLOB internal_chal; /* Random challenge as supplied to the client for NTLM authentication */

	DATA_BLOB chal; /* Random challenge as input into the actual NTLM (or NTLM2) authentication */
 	DATA_BLOB lm_resp;
	DATA_BLOB nt_resp;
	DATA_BLOB session_key;

	uint32 neg_flags; /* the current state of negotiation with the NTLMSSP partner */

	void *auth_context;

	/**
	 * Callback to get the 'challenge' used for NTLM authentication.
	 *
	 * @param ntlmssp_state This structure
	 * @return 8 bytes of challnege data, determined by the server to be the challenge for NTLM authentication
	 *
	 */
	void (*get_challenge)(const struct ntlmssp_state *ntlmssp_state,
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
	 * @param nt_session_key If an NT session key is returned by the authentication process, return it here
	 * @param lm_session_key If an LM session key is returned by the authentication process, return it here
	 *
	 */
	NTSTATUS (*check_password)(struct ntlmssp_state *ntlmssp_state, DATA_BLOB *nt_session_key, DATA_BLOB *lm_session_key);

	const char *(*get_global_myname)(void);
	const char *(*get_domain)(void);

	/* ntlmv2 */

	unsigned char send_sign_key[16];
	unsigned char send_seal_key[16];
	unsigned char recv_sign_key[16];
	unsigned char recv_seal_key[16];

	struct arcfour_state send_seal_arc4_state;
	struct arcfour_state recv_seal_arc4_state;

	uint32 ntlm2_send_seq_num;
	uint32 ntlm2_recv_seq_num;

	/* ntlmv1 */
	struct arcfour_state ntlmv1_arc4_state;
	uint32 ntlmv1_seq_num;

	/* it turns out that we don't always get the
	   response in at the time we want to process it.
	   Store it here, until we need it */
	DATA_BLOB stored_response;
} NTLMSSP_STATE;
