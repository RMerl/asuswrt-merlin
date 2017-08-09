/*
   Unix SMB/CIFS implementation.

   Winbind daemon - cached credentials funcions

   Copyright (C) Robert O'Callahan 2006
   Copyright (C) Jeremy Allison 2006 (minor fixes to fit into Samba and
				      protect against integer wrap).

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
#include "winbindd.h"
#include "../libcli/auth/ntlmssp.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

static bool client_can_access_ccache_entry(uid_t client_uid,
					struct WINBINDD_MEMORY_CREDS *entry)
{
	if (client_uid == entry->uid || client_uid == 0) {
		DEBUG(10, ("Access granted to uid %u\n", (unsigned int)client_uid));
		return True;
	}

	DEBUG(1, ("Access denied to uid %u (expected %u)\n",
		(unsigned int)client_uid, (unsigned int)entry->uid));
	return False;
}

static NTSTATUS do_ntlm_auth_with_hashes(const char *username,
					const char *domain,
					const unsigned char lm_hash[LM_HASH_LEN],
					const unsigned char nt_hash[NT_HASH_LEN],
					const DATA_BLOB initial_msg,
					const DATA_BLOB challenge_msg,
					DATA_BLOB *auth_msg,
					uint8_t session_key[16])
{
	NTSTATUS status;
	struct ntlmssp_state *ntlmssp_state = NULL;
	DATA_BLOB dummy_msg, reply;

	status = ntlmssp_client_start(NULL,
				      global_myname(),
				      lp_workgroup(),
				      lp_client_ntlmv2_auth(),
				      &ntlmssp_state);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Could not start NTLMSSP client: %s\n",
			nt_errstr(status)));
		goto done;
	}

	status = ntlmssp_set_username(ntlmssp_state, username);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Could not set username: %s\n",
			nt_errstr(status)));
		goto done;
	}

	status = ntlmssp_set_domain(ntlmssp_state, domain);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Could not set domain: %s\n",
			nt_errstr(status)));
		goto done;
	}

	status = ntlmssp_set_hashes(ntlmssp_state, lm_hash, nt_hash);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Could not set hashes: %s\n",
			nt_errstr(status)));
		goto done;
	}

	ntlmssp_want_feature(ntlmssp_state, NTLMSSP_FEATURE_SESSION_KEY);

	/* We need to get our protocol handler into the right state. So first
	   we ask it to generate the initial message. Actually the client has already
	   sent its own initial message, so we're going to drop this one on the floor.
	   The client might have sent a different message, for example with different
	   negotiation options, but as far as I can tell this won't hurt us. (Unless
	   the client sent a different username or domain, in which case that's their
	   problem for telling us the wrong username or domain.)
	   Since we have a copy of the initial message that the client sent, we could
	   resolve any discrepancies if we had to.
	*/
	dummy_msg = data_blob_null;
	reply = data_blob_null;
	status = ntlmssp_update(ntlmssp_state, dummy_msg, &reply);
	data_blob_free(&dummy_msg);
	data_blob_free(&reply);

	if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		DEBUG(1, ("Failed to create initial message! [%s]\n",
			nt_errstr(status)));
		goto done;
	}

	/* Now we are ready to handle the server's actual response. */
	status = ntlmssp_update(ntlmssp_state, challenge_msg, &reply);

	if (!NT_STATUS_EQUAL(status, NT_STATUS_OK)) {
		DEBUG(1, ("We didn't get a response to the challenge! [%s]\n",
			nt_errstr(status)));
		data_blob_free(&reply);
		goto done;
	}

	if (ntlmssp_state->session_key.length != 16) {
		DEBUG(1, ("invalid session key length %d\n",
			  (int)ntlmssp_state->session_key.length));
		data_blob_free(&reply);
		goto done;
	}

	*auth_msg = data_blob(reply.data, reply.length);
	memcpy(session_key, ntlmssp_state->session_key.data, 16);
	status = NT_STATUS_OK;

done:
	TALLOC_FREE(ntlmssp_state);
	return status;
}

static bool check_client_uid(struct winbindd_cli_state *state, uid_t uid)
{
	int ret;
	uid_t ret_uid;

	ret_uid = (uid_t)-1;

	ret = sys_getpeereid(state->sock, &ret_uid);
	if (ret != 0) {
		DEBUG(1, ("check_client_uid: Could not get socket peer uid: %s; "
			"denying access\n", strerror(errno)));
		return False;
	}

	if (uid != ret_uid) {
		DEBUG(1, ("check_client_uid: Client lied about its uid: said %u, "
			"actually was %u; denying access\n",
			(unsigned int)uid, (unsigned int)ret_uid));
		return False;
	}

	return True;
}

void winbindd_ccache_ntlm_auth(struct winbindd_cli_state *state)
{
	struct winbindd_domain *domain;
	fstring name_domain, name_user;
	NTSTATUS result = NT_STATUS_NOT_SUPPORTED;
	struct WINBINDD_MEMORY_CREDS *entry;
	DATA_BLOB initial, challenge, auth;
	uint32 initial_blob_len, challenge_blob_len, extra_len;

	/* Ensure null termination */
	state->request->data.ccache_ntlm_auth.user[
			sizeof(state->request->data.ccache_ntlm_auth.user)-1]='\0';

	DEBUG(3, ("[%5lu]: perform NTLM auth on behalf of user %s\n", (unsigned long)state->pid,
		state->request->data.ccache_ntlm_auth.user));

	/* Parse domain and username */

	if (!canonicalize_username(state->request->data.ccache_ntlm_auth.user,
				name_domain, name_user)) {
		DEBUG(5,("winbindd_ccache_ntlm_auth: cannot parse domain and user from name [%s]\n",
			state->request->data.ccache_ntlm_auth.user));
		request_error(state);
		return;
	}

	domain = find_auth_domain(state->request->flags, name_domain);

	if (domain == NULL) {
		DEBUG(5,("winbindd_ccache_ntlm_auth: can't get domain [%s]\n",
			name_domain));
		request_error(state);
		return;
	}

	if (!check_client_uid(state, state->request->data.ccache_ntlm_auth.uid)) {
		request_error(state);
		return;
	}

	/* validate blob lengths */
	initial_blob_len = state->request->data.ccache_ntlm_auth.initial_blob_len;
	challenge_blob_len = state->request->data.ccache_ntlm_auth.challenge_blob_len;
	extra_len = state->request->extra_len;

	if (initial_blob_len > extra_len || challenge_blob_len > extra_len ||
		initial_blob_len + challenge_blob_len > extra_len ||
		initial_blob_len + challenge_blob_len < initial_blob_len ||
		initial_blob_len + challenge_blob_len < challenge_blob_len) {

		DEBUG(10,("winbindd_dual_ccache_ntlm_auth: blob lengths overrun "
			"or wrap. Buffer [%d+%d > %d]\n",
			initial_blob_len,
			challenge_blob_len,
			extra_len));
		goto process_result;
	}

	/* Parse domain and username */
	if (!parse_domain_user(state->request->data.ccache_ntlm_auth.user, name_domain, name_user)) {
		DEBUG(10,("winbindd_dual_ccache_ntlm_auth: cannot parse "
			"domain and user from name [%s]\n",
			state->request->data.ccache_ntlm_auth.user));
		goto process_result;
	}

	entry = find_memory_creds_by_name(state->request->data.ccache_ntlm_auth.user);
	if (entry == NULL || entry->nt_hash == NULL || entry->lm_hash == NULL) {
		DEBUG(10,("winbindd_dual_ccache_ntlm_auth: could not find "
			"credentials for user %s\n", 
			state->request->data.ccache_ntlm_auth.user));
		goto process_result;
	}

	DEBUG(10,("winbindd_dual_ccache_ntlm_auth: found ccache [%s]\n", entry->username));

	if (!client_can_access_ccache_entry(state->request->data.ccache_ntlm_auth.uid, entry)) {
		goto process_result;
	}

	if (initial_blob_len == 0 && challenge_blob_len == 0) {
		/* this is just a probe to see if credentials are available. */
		result = NT_STATUS_OK;
		state->response->data.ccache_ntlm_auth.auth_blob_len = 0;
		goto process_result;
	}

	initial = data_blob_const(state->request->extra_data.data,
				  initial_blob_len);
	challenge = data_blob_const(
		state->request->extra_data.data + initial_blob_len,
		state->request->data.ccache_ntlm_auth.challenge_blob_len);

	result = do_ntlm_auth_with_hashes(
		name_user, name_domain, entry->lm_hash, entry->nt_hash,
		initial, challenge, &auth,
		state->response->data.ccache_ntlm_auth.session_key);

	if (!NT_STATUS_IS_OK(result)) {
		goto process_result;
	}

	state->response->extra_data.data = talloc_memdup(
		state->mem_ctx, auth.data, auth.length);
	if (!state->response->extra_data.data) {
		result = NT_STATUS_NO_MEMORY;
		goto process_result;
	}
	state->response->length += auth.length;
	state->response->data.ccache_ntlm_auth.auth_blob_len = auth.length;

	data_blob_free(&auth);

  process_result:
	if (!NT_STATUS_IS_OK(result)) {
		request_error(state);
		return;
	}
	request_ok(state);
}

void winbindd_ccache_save(struct winbindd_cli_state *state)
{
	struct winbindd_domain *domain;
	fstring name_domain, name_user;
	NTSTATUS status;

	/* Ensure null termination */
	state->request->data.ccache_save.user[
		sizeof(state->request->data.ccache_save.user)-1]='\0';
	state->request->data.ccache_save.pass[
		sizeof(state->request->data.ccache_save.pass)-1]='\0';

	DEBUG(3, ("[%5lu]: save password of user %s\n",
		  (unsigned long)state->pid,
		  state->request->data.ccache_save.user));

	/* Parse domain and username */

	if (!canonicalize_username(state->request->data.ccache_save.user,
				   name_domain, name_user)) {
		DEBUG(5,("winbindd_ccache_save: cannot parse domain and user "
			 "from name [%s]\n",
			 state->request->data.ccache_save.user));
		request_error(state);
		return;
	}

	/*
	 * The domain is checked here only for compatibility
	 * reasons. We used to do the winbindd memory ccache for
	 * ntlm_auth in the domain child. With that code, we had to
	 * make sure that we do have a domain around to send this
	 * to. Now we do the memory cache in the parent winbindd,
	 * where it would not matter if we have a domain or not.
	 */

	domain = find_auth_domain(state->request->flags, name_domain);
	if (domain == NULL) {
		DEBUG(5, ("winbindd_ccache_save: can't get domain [%s]\n",
			  name_domain));
		request_error(state);
		return;
	}

	if (!check_client_uid(state, state->request->data.ccache_save.uid)) {
		request_error(state);
		return;
	}

	status = winbindd_add_memory_creds(
		state->request->data.ccache_save.user,
		state->request->data.ccache_save.uid,
		state->request->data.ccache_save.pass);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("winbindd_add_memory_creds failed %s\n",
			  nt_errstr(status)));
		request_error(state);
		return;
	}
	request_ok(state);
}
