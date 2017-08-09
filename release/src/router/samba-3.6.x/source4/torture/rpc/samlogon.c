/*
   Unix SMB/CIFS implementation.

   test suite for netlogon SamLogon operations

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2003-2004
   Copyright (C) Tim Potter      2003

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
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "librpc/gen_ndr/ndr_netlogon_c.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "../lib/crypto/crypto.h"
#include "lib/cmdline/popt_common.h"
#include "torture/rpc/torture_rpc.h"
#include "auth/gensec/gensec.h"
#include "libcli/auth/libcli_auth.h"
#include "param/param.h"

#define TEST_MACHINE_NAME "samlogontest"
#define TEST_USER_NAME "samlogontestuser"
#define TEST_USER_NAME_WRONG_WKS "samlogontest2"
#define TEST_USER_NAME_WRONG_TIME "samlogontest3"

enum ntlm_break {
	BREAK_BOTH,
	BREAK_NONE,
	BREAK_LM,
	BREAK_NT,
	NO_LM,
	NO_NT
};

struct samlogon_state {
	TALLOC_CTX *mem_ctx;
	const char *comment;
	const char *account_name;
	const char *account_domain;
	const char *netbios_name;
	const char *password;
	const char *workgroup;
	struct dcerpc_pipe *p;
	int function_level;
	uint32_t parameter_control;
	struct netr_LogonSamLogon r;
	struct netr_LogonSamLogonEx r_ex;
	struct netr_LogonSamLogonWithFlags r_flags;
	struct netr_Authenticator auth, auth2;
	struct netlogon_creds_CredentialState *creds;
	NTSTATUS expected_error;
	bool old_password; /* Allow an old password to be accepted or rejected without error, as well as session key bugs */
	DATA_BLOB chall;
};

/*
   Authenticate a user with a challenge/response, checking session key
   and valid authentication types
*/
static NTSTATUS check_samlogon(struct samlogon_state *samlogon_state,
			       enum ntlm_break break_which,
			       uint32_t parameter_control,
			       DATA_BLOB *chall,
			       DATA_BLOB *lm_response,
			       DATA_BLOB *nt_response,
			       uint8_t lm_key[8],
			       uint8_t user_session_key[16],
			       char **error_string)
{
	NTSTATUS status;
	struct netr_LogonSamLogon *r = &samlogon_state->r;
	struct netr_LogonSamLogonEx *r_ex = &samlogon_state->r_ex;
	struct netr_LogonSamLogonWithFlags *r_flags = &samlogon_state->r_flags;
	struct netr_NetworkInfo ninfo;
	struct netr_SamBaseInfo *base = NULL;
	uint16_t validation_level = 0;

	samlogon_state->r.in.logon->network = &ninfo;
	samlogon_state->r_ex.in.logon->network = &ninfo;
	samlogon_state->r_flags.in.logon->network = &ninfo;

	ninfo.identity_info.domain_name.string = samlogon_state->account_domain;
	ninfo.identity_info.parameter_control = parameter_control;
	ninfo.identity_info.logon_id_low = 0;
	ninfo.identity_info.logon_id_high = 0;
	ninfo.identity_info.account_name.string = samlogon_state->account_name;
	ninfo.identity_info.workstation.string = TEST_MACHINE_NAME;

	memcpy(ninfo.challenge, chall->data, 8);

	switch (break_which) {
	case BREAK_NONE:
		break;
	case BREAK_LM:
		if (lm_response && lm_response->data) {
			lm_response->data[0]++;
		}
		break;
	case BREAK_NT:
		if (nt_response && nt_response->data) {
			nt_response->data[0]++;
		}
		break;
	case BREAK_BOTH:
		if (lm_response && lm_response->data) {
			lm_response->data[0]++;
		}
		if (nt_response && nt_response->data) {
			nt_response->data[0]++;
		}
		break;
	case NO_LM:
		data_blob_free(lm_response);
		break;
	case NO_NT:
		data_blob_free(nt_response);
		break;
	}

	if (nt_response) {
		ninfo.nt.data = nt_response->data;
		ninfo.nt.length = nt_response->length;
	} else {
		ninfo.nt.data = NULL;
		ninfo.nt.length = 0;
	}

	if (lm_response) {
		ninfo.lm.data = lm_response->data;
		ninfo.lm.length = lm_response->length;
	} else {
		ninfo.lm.data = NULL;
		ninfo.lm.length = 0;
	}

	switch (samlogon_state->function_level) {
	case NDR_NETR_LOGONSAMLOGON:
		ZERO_STRUCT(samlogon_state->auth2);
		netlogon_creds_client_authenticator(samlogon_state->creds, &samlogon_state->auth);

		r->out.return_authenticator = NULL;
		status = dcerpc_netr_LogonSamLogon_r(samlogon_state->p->binding_handle,
						     samlogon_state->mem_ctx, r);
		if (!NT_STATUS_IS_OK(status)) {
			if (error_string) {
				*error_string = strdup(nt_errstr(status));
			}
			return status;
		}
		if (!r->out.return_authenticator ||
		    !netlogon_creds_client_check(samlogon_state->creds, &r->out.return_authenticator->cred)) {
			d_printf("Credential chaining failed\n");
		}
		if (!NT_STATUS_IS_OK(r->out.result)) {
			if (error_string) {
				*error_string = strdup(nt_errstr(r->out.result));
			}
			return r->out.result;
		}

		validation_level = r->in.validation_level;

		netlogon_creds_decrypt_samlogon(samlogon_state->creds, validation_level, r->out.validation);

		switch (validation_level) {
		case 2:
			base = &r->out.validation->sam2->base;
			break;
		case 3:
			base = &r->out.validation->sam3->base;
			break;
		case 6:
			base = &r->out.validation->sam6->base;
			break;
		}
		break;
	case NDR_NETR_LOGONSAMLOGONEX:
		status = dcerpc_netr_LogonSamLogonEx_r(samlogon_state->p->binding_handle,
						       samlogon_state->mem_ctx, r_ex);
		if (!NT_STATUS_IS_OK(status)) {
			if (error_string) {
				*error_string = strdup(nt_errstr(status));
			}
			return status;
		}
		if (!NT_STATUS_IS_OK(r_ex->out.result)) {
			if (error_string) {
				*error_string = strdup(nt_errstr(r_ex->out.result));
			}
			return r_ex->out.result;
		}

		validation_level = r_ex->in.validation_level;

		netlogon_creds_decrypt_samlogon(samlogon_state->creds, validation_level, r_ex->out.validation);

		switch (validation_level) {
		case 2:
			base = &r_ex->out.validation->sam2->base;
			break;
		case 3:
			base = &r_ex->out.validation->sam3->base;
			break;
		case 6:
			base = &r_ex->out.validation->sam6->base;
			break;
		}
		break;
	case NDR_NETR_LOGONSAMLOGONWITHFLAGS:
		ZERO_STRUCT(samlogon_state->auth2);
		netlogon_creds_client_authenticator(samlogon_state->creds, &samlogon_state->auth);

		r_flags->out.return_authenticator = NULL;
		status = dcerpc_netr_LogonSamLogonWithFlags_r(samlogon_state->p->binding_handle,
							      samlogon_state->mem_ctx, r_flags);
		if (!NT_STATUS_IS_OK(status)) {
			if (error_string) {
				*error_string = strdup(nt_errstr(status));
			}
			return status;
		}
		if (!r_flags->out.return_authenticator ||
		    !netlogon_creds_client_check(samlogon_state->creds, &r_flags->out.return_authenticator->cred)) {
			d_printf("Credential chaining failed\n");
		}
		if (!NT_STATUS_IS_OK(r_flags->out.result)) {
			if (error_string) {
				*error_string = strdup(nt_errstr(r_flags->out.result));
			}
			return r_flags->out.result;
		}

		validation_level = r_flags->in.validation_level;

		netlogon_creds_decrypt_samlogon(samlogon_state->creds, validation_level, r_flags->out.validation);

		switch (validation_level) {
		case 2:
			base = &r_flags->out.validation->sam2->base;
			break;
		case 3:
			base = &r_flags->out.validation->sam3->base;
			break;
		case 6:
			base = &r_flags->out.validation->sam6->base;
			break;
		}
		break;
	default:
		/* can't happen */
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!base) {
		d_printf("No user info returned from 'successful' SamLogon*() call!\n");
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (user_session_key) {
		memcpy(user_session_key, base->key.key, 16);
	}
	if (lm_key) {
		memcpy(lm_key, base->LMSessKey.key, 8);
	}

	return status;
}


/*
 * Test the normal 'LM and NTLM' combination
 */

static bool test_lm_ntlm_broken(struct samlogon_state *samlogon_state, enum ntlm_break break_which, char **error_string)
{
	bool pass = true;
	bool lm_good;
	NTSTATUS nt_status;
	DATA_BLOB lm_response = data_blob_talloc(samlogon_state->mem_ctx, NULL, 24);
	DATA_BLOB nt_response = data_blob_talloc(samlogon_state->mem_ctx, NULL, 24);
	DATA_BLOB session_key = data_blob_talloc(samlogon_state->mem_ctx, NULL, 16);

	uint8_t lm_key[8];
	uint8_t user_session_key[16];
	uint8_t lm_hash[16];
	uint8_t nt_hash[16];

	ZERO_STRUCT(lm_key);
	ZERO_STRUCT(user_session_key);

	lm_good = SMBencrypt(samlogon_state->password, samlogon_state->chall.data, lm_response.data);
	if (!lm_good) {
		ZERO_STRUCT(lm_hash);
	} else {
		E_deshash(samlogon_state->password, lm_hash);
	}

	SMBNTencrypt(samlogon_state->password, samlogon_state->chall.data, nt_response.data);

	E_md4hash(samlogon_state->password, nt_hash);
	SMBsesskeygen_ntv1(nt_hash, session_key.data);

	nt_status = check_samlogon(samlogon_state,
				   break_which,
				   samlogon_state->parameter_control,
				   &samlogon_state->chall,
				   &lm_response,
				   &nt_response,
				   lm_key,
				   user_session_key,
				   error_string);

	data_blob_free(&lm_response);

	if (NT_STATUS_EQUAL(NT_STATUS_WRONG_PASSWORD, nt_status)) {
		/* for 'long' passwords, the LM password is invalid */
		if (break_which == NO_NT && !lm_good) {
			return true;
		}
		/* for 'old' passwords, we allow the server to be OK or wrong password */
		if (samlogon_state->old_password) {
			return true;
		}
		return ((break_which == BREAK_NT) || (break_which == BREAK_BOTH));
	} else if (NT_STATUS_EQUAL(NT_STATUS_NOT_FOUND, nt_status) && strchr_m(samlogon_state->account_name, '@')) {
		return ((break_which == BREAK_NT) || (break_which == BREAK_BOTH) || (break_which == NO_NT));
	} else if (!NT_STATUS_EQUAL(samlogon_state->expected_error, nt_status)) {
		SAFE_FREE(*error_string);
		asprintf(error_string, "Expected error: %s, got %s", nt_errstr(samlogon_state->expected_error), nt_errstr(nt_status));
		return false;
	} else if (NT_STATUS_EQUAL(samlogon_state->expected_error, nt_status) && !NT_STATUS_IS_OK(nt_status)) {
		return true;
	} else if (!NT_STATUS_IS_OK(nt_status)) {
		return false;
	}

	if (break_which == NO_NT && !lm_good) {
	        *error_string = strdup("LM password is 'long' (> 14 chars and therefore invalid) but login did not fail!");
		return false;
	}

	if (memcmp(lm_hash, lm_key,
		   sizeof(lm_key)) != 0) {
		d_printf("LM Key does not match expectations!\n");
		d_printf("lm_key:\n");
		dump_data(1, lm_key, 8);
		d_printf("expected:\n");
		dump_data(1, lm_hash, 8);
		pass = false;
	}

	switch (break_which) {
	case NO_NT:
	{
		uint8_t lm_key_expected[16];
		memcpy(lm_key_expected, lm_hash, 8);
		memset(lm_key_expected+8, '\0', 8);
		if (memcmp(lm_key_expected, user_session_key,
			   16) != 0) {
			*error_string = strdup("NT Session Key does not match expectations (should be first-8 LM hash)!\n");
			d_printf("user_session_key:\n");
			dump_data(1, user_session_key, sizeof(user_session_key));
			d_printf("expected:\n");
			dump_data(1, lm_key_expected, sizeof(lm_key_expected));
			pass = false;
		}
		break;
	}
	default:
		if (memcmp(session_key.data, user_session_key,
			   sizeof(user_session_key)) != 0) {
			*error_string = strdup("NT Session Key does not match expectations!\n");
			d_printf("user_session_key:\n");
			dump_data(1, user_session_key, 16);
			d_printf("expected:\n");
			dump_data(1, session_key.data, session_key.length);
			pass = false;
		}
	}
        return pass;
}

/*
 * Test LM authentication, no NT response supplied
 */

static bool test_lm(struct samlogon_state *samlogon_state, char **error_string)
{

	return test_lm_ntlm_broken(samlogon_state, NO_NT, error_string);
}

/*
 * Test the NTLM response only, no LM.
 */

static bool test_ntlm(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lm_ntlm_broken(samlogon_state, NO_LM, error_string);
}

/*
 * Test the NTLM response only, but in the LM field.
 */

static bool test_ntlm_in_lm(struct samlogon_state *samlogon_state, char **error_string)
{
	bool lm_good;
	bool pass = true;
	NTSTATUS nt_status;
	DATA_BLOB nt_response = data_blob_talloc(samlogon_state->mem_ctx, NULL, 24);
	DATA_BLOB session_key = data_blob_talloc(samlogon_state->mem_ctx, NULL, 16);

	uint8_t lm_key[8];
	uint8_t lm_hash[16];
	uint8_t user_session_key[16];
	uint8_t nt_hash[16];

	ZERO_STRUCT(lm_key);
	ZERO_STRUCT(user_session_key);

	SMBNTencrypt(samlogon_state->password, samlogon_state->chall.data,
		     nt_response.data);
	E_md4hash(samlogon_state->password, nt_hash);
	SMBsesskeygen_ntv1(nt_hash,
			   session_key.data);

	lm_good = E_deshash(samlogon_state->password, lm_hash);
	if (!lm_good) {
		ZERO_STRUCT(lm_hash);
	}
	nt_status = check_samlogon(samlogon_state,
				   BREAK_NONE,
				   samlogon_state->parameter_control,
				   &samlogon_state->chall,
				   &nt_response,
				   NULL,
				   lm_key,
				   user_session_key,
				   error_string);

	if (NT_STATUS_EQUAL(NT_STATUS_WRONG_PASSWORD, nt_status)) {
		/* for 'old' passwords, we allow the server to be OK or wrong password */
		if (samlogon_state->old_password) {
			return true;
		}
		return false;
	} else if (!NT_STATUS_EQUAL(samlogon_state->expected_error, nt_status)) {
		SAFE_FREE(*error_string);
		asprintf(error_string, "Expected error: %s, got %s", nt_errstr(samlogon_state->expected_error), nt_errstr(nt_status));
		return false;
	} else if (NT_STATUS_EQUAL(samlogon_state->expected_error, nt_status) && !NT_STATUS_IS_OK(nt_status)) {
		return true;
	} else if (!NT_STATUS_IS_OK(nt_status)) {
		return false;
	}

	if (lm_good) {
		if (memcmp(lm_hash, lm_key,
			   sizeof(lm_key)) != 0) {
			d_printf("LM Key does not match expectations!\n");
			d_printf("lm_key:\n");
			dump_data(1, lm_key, 8);
			d_printf("expected:\n");
			dump_data(1, lm_hash, 8);
			pass = false;
		}
#if 0
	} else {
		if (memcmp(session_key.data, lm_key,
			   sizeof(lm_key)) != 0) {
			d_printf("LM Key does not match expectations (first 8 session key)!\n");
			d_printf("lm_key:\n");
			dump_data(1, lm_key, 8);
			d_printf("expected:\n");
			dump_data(1, session_key.data, 8);
			pass = false;
		}
#endif
	}
	if (lm_good && memcmp(lm_hash, user_session_key, 8) != 0) {
		uint8_t lm_key_expected[16];
		memcpy(lm_key_expected, lm_hash, 8);
		memset(lm_key_expected+8, '\0', 8);
		if (memcmp(lm_key_expected, user_session_key,
			   16) != 0) {
			d_printf("NT Session Key does not match expectations (should be first-8 LM hash)!\n");
			d_printf("user_session_key:\n");
			dump_data(1, user_session_key, sizeof(user_session_key));
			d_printf("expected:\n");
			dump_data(1, lm_key_expected, sizeof(lm_key_expected));
			pass = false;
		}
	}
        return pass;
}

/*
 * Test the NTLM response only, but in the both the NT and LM fields.
 */

static bool test_ntlm_in_both(struct samlogon_state *samlogon_state, char **error_string)
{
	bool pass = true;
	bool lm_good;
	NTSTATUS nt_status;
	DATA_BLOB nt_response = data_blob_talloc(samlogon_state->mem_ctx, NULL, 24);
	DATA_BLOB session_key = data_blob_talloc(samlogon_state->mem_ctx, NULL, 16);

	uint8_t lm_key[8];
	uint8_t lm_hash[16];
	uint8_t user_session_key[16];
	uint8_t nt_hash[16];

	ZERO_STRUCT(lm_key);
	ZERO_STRUCT(user_session_key);

	SMBNTencrypt(samlogon_state->password, samlogon_state->chall.data,
		     nt_response.data);
	E_md4hash(samlogon_state->password, nt_hash);
	SMBsesskeygen_ntv1(nt_hash,
			   session_key.data);

	lm_good = E_deshash(samlogon_state->password, lm_hash);
	if (!lm_good) {
		ZERO_STRUCT(lm_hash);
	}

	nt_status = check_samlogon(samlogon_state,
				   BREAK_NONE,
				   samlogon_state->parameter_control,
				   &samlogon_state->chall,
				   NULL,
				   &nt_response,
				   lm_key,
				   user_session_key,
				   error_string);

	if (NT_STATUS_EQUAL(NT_STATUS_WRONG_PASSWORD, nt_status)) {
		/* for 'old' passwords, we allow the server to be OK or wrong password */
		if (samlogon_state->old_password) {
			return true;
		}
		return false;
	} else if (!NT_STATUS_EQUAL(samlogon_state->expected_error, nt_status)) {
		SAFE_FREE(*error_string);
		asprintf(error_string, "Expected error: %s, got %s", nt_errstr(samlogon_state->expected_error), nt_errstr(nt_status));
		return false;
	} else if (NT_STATUS_EQUAL(samlogon_state->expected_error, nt_status) && !NT_STATUS_IS_OK(nt_status)) {
		return true;
	} else if (!NT_STATUS_IS_OK(nt_status)) {
		return false;
	}

	if (!NT_STATUS_IS_OK(nt_status)) {
		return false;
	}

	if (memcmp(lm_hash, lm_key,
		   sizeof(lm_key)) != 0) {
		d_printf("LM Key does not match expectations!\n");
 		d_printf("lm_key:\n");
		dump_data(1, lm_key, 8);
		d_printf("expected:\n");
		dump_data(1, lm_hash, 8);
		pass = false;
	}
	if (memcmp(session_key.data, user_session_key,
		   sizeof(user_session_key)) != 0) {
		d_printf("NT Session Key does not match expectations!\n");
 		d_printf("user_session_key:\n");
		dump_data(1, user_session_key, 16);
 		d_printf("expected:\n");
		dump_data(1, session_key.data, session_key.length);
		pass = false;
	}


        return pass;
}

/*
 * Test the NTLMv2 and LMv2 responses
 */

enum ntlmv2_domain {
	UPPER_DOMAIN,
	NO_DOMAIN
};

static bool test_lmv2_ntlmv2_broken(struct samlogon_state *samlogon_state,
				    enum ntlm_break break_which,
				    enum ntlmv2_domain ntlmv2_domain,
				    char **error_string)
{
	bool pass = true;
	NTSTATUS nt_status;
	DATA_BLOB ntlmv2_response = data_blob(NULL, 0);
	DATA_BLOB lmv2_response = data_blob(NULL, 0);
	DATA_BLOB lmv2_session_key = data_blob(NULL, 0);
	DATA_BLOB ntlmv2_session_key = data_blob(NULL, 0);
	DATA_BLOB names_blob = NTLMv2_generate_names_blob(samlogon_state->mem_ctx, TEST_MACHINE_NAME, samlogon_state->workgroup);

	uint8_t lm_session_key[8];
	uint8_t user_session_key[16];

	ZERO_STRUCT(lm_session_key);
	ZERO_STRUCT(user_session_key);

	switch (ntlmv2_domain) {
	case UPPER_DOMAIN:
		if (!SMBNTLMv2encrypt(samlogon_state->mem_ctx,
				      samlogon_state->account_name, samlogon_state->account_domain,
				      samlogon_state->password, &samlogon_state->chall,
				      &names_blob,
				      &lmv2_response, &ntlmv2_response,
				      &lmv2_session_key, &ntlmv2_session_key)) {
			data_blob_free(&names_blob);
			return false;
		}
		break;
	case NO_DOMAIN:
		if (!SMBNTLMv2encrypt(samlogon_state->mem_ctx,
				      samlogon_state->account_name, "",
				      samlogon_state->password, &samlogon_state->chall,
				      &names_blob,
				      &lmv2_response, &ntlmv2_response,
				      &lmv2_session_key, &ntlmv2_session_key)) {
			data_blob_free(&names_blob);
			return false;
		}
		break;
	}
	data_blob_free(&names_blob);

	nt_status = check_samlogon(samlogon_state,
				   break_which,
				   samlogon_state->parameter_control,
				   &samlogon_state->chall,
				   &lmv2_response,
				   &ntlmv2_response,
				   lm_session_key,
				   user_session_key,
				   error_string);

	data_blob_free(&lmv2_response);
	data_blob_free(&ntlmv2_response);


	if (NT_STATUS_EQUAL(NT_STATUS_WRONG_PASSWORD, nt_status)) {
		/* for 'old' passwords, we allow the server to be OK or wrong password */
		if (samlogon_state->old_password) {
			return true;
		}
		return break_which == BREAK_BOTH;
	} else if (NT_STATUS_EQUAL(NT_STATUS_NOT_FOUND, nt_status) && strchr_m(samlogon_state->account_name, '@')) {
		return ((break_which == BREAK_NT) || (break_which == BREAK_BOTH) || (break_which == NO_NT));
	} else if (!NT_STATUS_EQUAL(samlogon_state->expected_error, nt_status)) {
		SAFE_FREE(*error_string);
		asprintf(error_string, "Expected error: %s, got %s", nt_errstr(samlogon_state->expected_error), nt_errstr(nt_status));
		return false;
	} else if (NT_STATUS_EQUAL(samlogon_state->expected_error, nt_status) && !NT_STATUS_IS_OK(nt_status)) {
		return true;
	} else if (!NT_STATUS_IS_OK(nt_status)) {
		return false;
	}


	switch (break_which) {
	case NO_NT:
		if (memcmp(lmv2_session_key.data, user_session_key,
			   sizeof(user_session_key)) != 0) {
			d_printf("USER (LMv2) Session Key does not match expectations!\n");
			d_printf("user_session_key:\n");
			dump_data(1, user_session_key, 16);
			d_printf("expected:\n");
			dump_data(1, lmv2_session_key.data, ntlmv2_session_key.length);
			pass = false;
		}
		if (memcmp(lmv2_session_key.data, lm_session_key,
				   sizeof(lm_session_key)) != 0) {
			d_printf("LM (LMv2) Session Key does not match expectations!\n");
			d_printf("lm_session_key:\n");
			dump_data(1, lm_session_key, 8);
			d_printf("expected:\n");
			dump_data(1, lmv2_session_key.data, 8);
			pass = false;
		}
		break;
	default:
		if (memcmp(ntlmv2_session_key.data, user_session_key,
			   sizeof(user_session_key)) != 0) {
			if (memcmp(lmv2_session_key.data, user_session_key,
				   sizeof(user_session_key)) == 0) {
				d_printf("USER (NTLMv2) Session Key expected, got LMv2 sessesion key instead:\n");
				d_printf("user_session_key:\n");
				dump_data(1, user_session_key, 16);
				d_printf("expected:\n");
				dump_data(1, ntlmv2_session_key.data, ntlmv2_session_key.length);
				pass = false;

			} else {
				d_printf("USER (NTLMv2) Session Key does not match expectations!\n");
				d_printf("user_session_key:\n");
				dump_data(1, user_session_key, 16);
				d_printf("expected:\n");
				dump_data(1, ntlmv2_session_key.data, ntlmv2_session_key.length);
				pass = false;
			}
		}
		if (memcmp(ntlmv2_session_key.data, lm_session_key,
			   sizeof(lm_session_key)) != 0) {
			if (memcmp(lmv2_session_key.data, lm_session_key,
				   sizeof(lm_session_key)) == 0) {
				d_printf("LM (NTLMv2) Session Key expected, got LMv2 sessesion key instead:\n");
				d_printf("user_session_key:\n");
				dump_data(1, lm_session_key, 8);
				d_printf("expected:\n");
				dump_data(1, ntlmv2_session_key.data, 8);
				pass = false;
			} else {
				d_printf("LM (NTLMv2) Session Key does not match expectations!\n");
				d_printf("lm_session_key:\n");
				dump_data(1, lm_session_key, 8);
				d_printf("expected:\n");
				dump_data(1, ntlmv2_session_key.data, 8);
				pass = false;
			}
		}
	}

        return pass;
}

/*
 * Test the NTLM and LMv2 responses
 */

static bool test_lmv2_ntlm_broken(struct samlogon_state *samlogon_state,
				  enum ntlm_break break_which,
				  enum ntlmv2_domain ntlmv2_domain,
				  char **error_string)
{
	bool pass = true;
	NTSTATUS nt_status;
	DATA_BLOB ntlmv2_response = data_blob(NULL, 0);
	DATA_BLOB lmv2_response = data_blob(NULL, 0);
	DATA_BLOB lmv2_session_key = data_blob(NULL, 0);
	DATA_BLOB ntlmv2_session_key = data_blob(NULL, 0);
	DATA_BLOB names_blob = NTLMv2_generate_names_blob(samlogon_state->mem_ctx, samlogon_state->netbios_name, samlogon_state->workgroup);

	DATA_BLOB ntlm_response = data_blob_talloc(samlogon_state->mem_ctx, NULL, 24);
	DATA_BLOB ntlm_session_key = data_blob_talloc(samlogon_state->mem_ctx, NULL, 16);

	bool lm_good;
	uint8_t lm_hash[16];
	uint8_t lm_session_key[8];
	uint8_t user_session_key[16];
	uint8_t nt_hash[16];

	SMBNTencrypt(samlogon_state->password, samlogon_state->chall.data,
		     ntlm_response.data);
	E_md4hash(samlogon_state->password, nt_hash);
	SMBsesskeygen_ntv1(nt_hash,
			   ntlm_session_key.data);

	lm_good = E_deshash(samlogon_state->password, lm_hash);
	if (!lm_good) {
		ZERO_STRUCT(lm_hash);
	}

	ZERO_STRUCT(lm_session_key);
	ZERO_STRUCT(user_session_key);

	switch (ntlmv2_domain) {
	case UPPER_DOMAIN:
		/* TODO - test with various domain cases, and without domain */
		if (!SMBNTLMv2encrypt(samlogon_state->mem_ctx,
				      samlogon_state->account_name, samlogon_state->account_domain,
				      samlogon_state->password, &samlogon_state->chall,
				      &names_blob,
				      &lmv2_response, &ntlmv2_response,
				      &lmv2_session_key, &ntlmv2_session_key)) {
			data_blob_free(&names_blob);
			return false;
		}
		break;
	case NO_DOMAIN:
		/* TODO - test with various domain cases, and without domain */
		if (!SMBNTLMv2encrypt(samlogon_state->mem_ctx,
				      samlogon_state->account_name, "",
				      samlogon_state->password, &samlogon_state->chall,
				      &names_blob,
				      &lmv2_response, &ntlmv2_response,
				      &lmv2_session_key, &ntlmv2_session_key)) {
			data_blob_free(&names_blob);
			return false;
		}
		break;
	}

	data_blob_free(&names_blob);

	nt_status = check_samlogon(samlogon_state,
				   break_which,
				   samlogon_state->parameter_control,
				   &samlogon_state->chall,
				   &lmv2_response,
				   &ntlm_response,
				   lm_session_key,
				   user_session_key,
				   error_string);

	data_blob_free(&lmv2_response);
	data_blob_free(&ntlmv2_response);


	if (NT_STATUS_EQUAL(NT_STATUS_WRONG_PASSWORD, nt_status)) {
		/* for 'old' passwords, we allow the server to be OK or wrong password */
		if (samlogon_state->old_password) {
			return true;
		}
		return ((break_which == BREAK_NT) || (break_which == BREAK_BOTH));
	} else if (NT_STATUS_EQUAL(NT_STATUS_NOT_FOUND, nt_status) && strchr_m(samlogon_state->account_name, '@')) {
		return ((break_which == BREAK_NT) || (break_which == BREAK_BOTH));
	} else if (!NT_STATUS_EQUAL(samlogon_state->expected_error, nt_status)) {
		SAFE_FREE(*error_string);
		asprintf(error_string, "Expected error: %s, got %s", nt_errstr(samlogon_state->expected_error), nt_errstr(nt_status));
		return false;
	} else if (NT_STATUS_EQUAL(samlogon_state->expected_error, nt_status) && !NT_STATUS_IS_OK(nt_status)) {
		return true;
	} else if (!NT_STATUS_IS_OK(nt_status)) {
		return false;
	}

	switch (break_which) {
	case NO_NT:
		if (memcmp(lmv2_session_key.data, user_session_key,
			   sizeof(user_session_key)) != 0) {
			d_printf("USER (LMv2) Session Key does not match expectations!\n");
			d_printf("user_session_key:\n");
			dump_data(1, user_session_key, 16);
			d_printf("expected:\n");
			dump_data(1, lmv2_session_key.data, ntlmv2_session_key.length);
			pass = false;
		}
		if (memcmp(lmv2_session_key.data, lm_session_key,
			   sizeof(lm_session_key)) != 0) {
			d_printf("LM (LMv2) Session Key does not match expectations!\n");
			d_printf("lm_session_key:\n");
			dump_data(1, lm_session_key, 8);
			d_printf("expected:\n");
			dump_data(1, lmv2_session_key.data, 8);
			pass = false;
		}
		break;
	case BREAK_LM:
		if (memcmp(ntlm_session_key.data, user_session_key,
			   sizeof(user_session_key)) != 0) {
			d_printf("USER (NTLMv2) Session Key does not match expectations!\n");
			d_printf("user_session_key:\n");
			dump_data(1, user_session_key, 16);
			d_printf("expected:\n");
			dump_data(1, ntlm_session_key.data, ntlm_session_key.length);
			pass = false;
		}
		if (lm_good) {
			if (memcmp(lm_hash, lm_session_key,
				   sizeof(lm_session_key)) != 0) {
				d_printf("LM Session Key does not match expectations!\n");
				d_printf("lm_session_key:\n");
				dump_data(1, lm_session_key, 8);
				d_printf("expected:\n");
				dump_data(1, lm_hash, 8);
				pass = false;
			}
		} else {
			static const uint8_t zeros[8];
			if (memcmp(zeros, lm_session_key,
				   sizeof(lm_session_key)) != 0) {
				d_printf("LM Session Key does not match expectations (zeros)!\n");
				d_printf("lm_session_key:\n");
				dump_data(1, lm_session_key, 8);
				d_printf("expected:\n");
				dump_data(1, zeros, 8);
				pass = false;
			}
		}
		break;
	default:
		if (memcmp(ntlm_session_key.data, user_session_key,
			   sizeof(user_session_key)) != 0) {
			d_printf("USER (NTLMv2) Session Key does not match expectations!\n");
			d_printf("user_session_key:\n");
			dump_data(1, user_session_key, 16);
			d_printf("expected:\n");
			dump_data(1, ntlm_session_key.data, ntlm_session_key.length);
			pass = false;
		}
		if (memcmp(ntlm_session_key.data, lm_session_key,
			   sizeof(lm_session_key)) != 0) {
			d_printf("LM (NTLMv2) Session Key does not match expectations!\n");
			d_printf("lm_session_key:\n");
			dump_data(1, lm_session_key, 8);
			d_printf("expected:\n");
			dump_data(1, ntlm_session_key.data, 8);
			pass = false;
		}
	}

        return pass;
}

/*
 * Test the NTLMv2 and LMv2 responses
 */

static bool test_lmv2_ntlmv2(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlmv2_broken(samlogon_state, BREAK_NONE, UPPER_DOMAIN, error_string);
}

#if 0
static bool test_lmv2_ntlmv2_no_dom(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlmv2_broken(samlogon_state, BREAK_NONE, NO_DOMAIN, error_string);
}
#endif

/*
 * Test the LMv2 response only
 */

static bool test_lmv2(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlmv2_broken(samlogon_state, NO_NT, UPPER_DOMAIN, error_string);
}

static bool test_lmv2_no_dom(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlmv2_broken(samlogon_state, NO_NT, NO_DOMAIN, error_string);
}

/*
 * Test the NTLMv2 response only
 */

static bool test_ntlmv2(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlmv2_broken(samlogon_state, NO_LM, UPPER_DOMAIN, error_string);
}

static bool test_ntlmv2_no_dom(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlmv2_broken(samlogon_state, NO_LM, NO_DOMAIN, error_string);
}

static bool test_lm_ntlm(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lm_ntlm_broken(samlogon_state, BREAK_NONE, error_string);
}

static bool test_ntlm_lm_broken(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lm_ntlm_broken(samlogon_state, BREAK_LM, error_string);
}

static bool test_ntlm_ntlm_broken(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lm_ntlm_broken(samlogon_state, BREAK_NT, error_string);
}

static bool test_lm_ntlm_both_broken(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lm_ntlm_broken(samlogon_state, BREAK_BOTH, error_string);
}
static bool test_ntlmv2_lmv2_broken(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlmv2_broken(samlogon_state, BREAK_LM, UPPER_DOMAIN, error_string);
}

static bool test_ntlmv2_lmv2_broken_no_dom(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlmv2_broken(samlogon_state, BREAK_LM, NO_DOMAIN, error_string);
}

static bool test_ntlmv2_ntlmv2_broken(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlmv2_broken(samlogon_state, BREAK_NT, UPPER_DOMAIN, error_string);
}

#if 0
static bool test_ntlmv2_ntlmv2_broken_no_dom(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlmv2_broken(samlogon_state, BREAK_NT, NO_DOMAIN, error_string);
}
#endif

static bool test_ntlmv2_both_broken(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlmv2_broken(samlogon_state, BREAK_BOTH, UPPER_DOMAIN, error_string);
}

static bool test_ntlmv2_both_broken_no_dom(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlmv2_broken(samlogon_state, BREAK_BOTH, NO_DOMAIN, error_string);
}

static bool test_lmv2_ntlm_both_broken(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlm_broken(samlogon_state, BREAK_BOTH, UPPER_DOMAIN, error_string);
}

static bool test_lmv2_ntlm_both_broken_no_dom(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlm_broken(samlogon_state, BREAK_BOTH, NO_DOMAIN, error_string);
}

static bool test_lmv2_ntlm_break_ntlm(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlm_broken(samlogon_state, BREAK_NT, UPPER_DOMAIN, error_string);
}

static bool test_lmv2_ntlm_break_ntlm_no_dom(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlm_broken(samlogon_state, BREAK_NT, NO_DOMAIN, error_string);
}

static bool test_lmv2_ntlm_break_lm(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlm_broken(samlogon_state, BREAK_LM, UPPER_DOMAIN, error_string);
}

static bool test_lmv2_ntlm_break_lm_no_dom(struct samlogon_state *samlogon_state, char **error_string)
{
	return test_lmv2_ntlm_broken(samlogon_state, BREAK_LM, NO_DOMAIN, error_string);
}

/*
 * Test the NTLM2 response (extra challenge in LM feild)
 *
 * This test is the same as the 'break LM' test, but checks that the
 * server implements NTLM2 session security in the right place
 * (NETLOGON is the wrong place).
 */

static bool test_ntlm2(struct samlogon_state *samlogon_state, char **error_string)
{
	bool pass = true;
	NTSTATUS nt_status;
	DATA_BLOB lm_response = data_blob_talloc(samlogon_state->mem_ctx, NULL, 24);
	DATA_BLOB nt_response = data_blob_talloc(samlogon_state->mem_ctx, NULL, 24);

	bool lm_good;
	uint8_t lm_key[8];
	uint8_t nt_hash[16];
	uint8_t lm_hash[16];
	uint8_t nt_key[16];
	uint8_t user_session_key[16];
	uint8_t expected_user_session_key[16];
	uint8_t session_nonce_hash[16];
	uint8_t client_chall[8];

	MD5_CTX md5_session_nonce_ctx;
	HMACMD5Context hmac_ctx;

	ZERO_STRUCT(user_session_key);
	ZERO_STRUCT(lm_key);
	generate_random_buffer(client_chall, 8);

	MD5Init(&md5_session_nonce_ctx);
	MD5Update(&md5_session_nonce_ctx, samlogon_state->chall.data, 8);
	MD5Update(&md5_session_nonce_ctx, client_chall, 8);
	MD5Final(session_nonce_hash, &md5_session_nonce_ctx);

	E_md4hash(samlogon_state->password, (uint8_t *)nt_hash);
	lm_good = E_deshash(samlogon_state->password, (uint8_t *)lm_hash);
	SMBsesskeygen_ntv1((const uint8_t *)nt_hash,
			   nt_key);

	SMBNTencrypt(samlogon_state->password, samlogon_state->chall.data, nt_response.data);

	memcpy(lm_response.data, session_nonce_hash, 8);
	memset(lm_response.data + 8, 0, 16);

	hmac_md5_init_rfc2104(nt_key, 16, &hmac_ctx);
	hmac_md5_update(samlogon_state->chall.data, 8, &hmac_ctx);
	hmac_md5_update(client_chall, 8, &hmac_ctx);
	hmac_md5_final(expected_user_session_key, &hmac_ctx);

	nt_status = check_samlogon(samlogon_state,
				   BREAK_NONE,
				   samlogon_state->parameter_control,
				   &samlogon_state->chall,
				   &lm_response,
				   &nt_response,
				   lm_key,
				   user_session_key,
				   error_string);

	if (NT_STATUS_EQUAL(NT_STATUS_WRONG_PASSWORD, nt_status)) {
		/* for 'old' passwords, we allow the server to be OK or wrong password */
		if (samlogon_state->old_password) {
			return true;
		}
		return false;
	} else if (!NT_STATUS_EQUAL(samlogon_state->expected_error, nt_status)) {
		SAFE_FREE(*error_string);
		asprintf(error_string, "Expected error: %s, got %s", nt_errstr(samlogon_state->expected_error), nt_errstr(nt_status));
		return false;
	} else if (NT_STATUS_EQUAL(samlogon_state->expected_error, nt_status) && !NT_STATUS_IS_OK(nt_status)) {
		return true;
	} else if (!NT_STATUS_IS_OK(nt_status)) {
		return false;
	}

	if (lm_good) {
		if (memcmp(lm_hash, lm_key,
			   sizeof(lm_key)) != 0) {
			d_printf("LM Key does not match expectations!\n");
			d_printf("lm_key:\n");
			dump_data(1, lm_key, 8);
			d_printf("expected:\n");
			dump_data(1, lm_hash, 8);
			pass = false;
		}
	} else {
		static const uint8_t zeros[8];
		if (memcmp(zeros, lm_key,
			   sizeof(lm_key)) != 0) {
			d_printf("LM Session Key does not match expectations (zeros)!\n");
			d_printf("lm_key:\n");
			dump_data(1, lm_key, 8);
			d_printf("expected:\n");
			dump_data(1, zeros, 8);
			pass = false;
		}
	}
	if (memcmp(nt_key, user_session_key, 16) != 0) {
		d_printf("NT Session Key does not match expectations (should be NT Key)!\n");
		d_printf("user_session_key:\n");
		dump_data(1, user_session_key, sizeof(user_session_key));
		d_printf("expected:\n");
		dump_data(1, nt_key, sizeof(nt_key));
		pass = false;
	}
        return pass;
}

static bool test_plaintext(struct samlogon_state *samlogon_state, enum ntlm_break break_which, char **error_string)
{
	NTSTATUS nt_status;
	DATA_BLOB nt_response = data_blob(NULL, 0);
	DATA_BLOB lm_response = data_blob(NULL, 0);
	char *password;
	char *dospw;
	smb_ucs2_t *unicodepw;

	uint8_t user_session_key[16];
	uint8_t lm_key[16];
	uint8_t lm_hash[16];
	static const uint8_t zeros[8];
	DATA_BLOB chall = data_blob_talloc(samlogon_state->mem_ctx, zeros, sizeof(zeros));
	bool lm_good = E_deshash(samlogon_state->password, lm_hash);

	ZERO_STRUCT(user_session_key);

	if (!push_ucs2_talloc(samlogon_state->mem_ctx,
			      &unicodepw, samlogon_state->password, NULL)) {
		DEBUG(0, ("push_ucs2_allocate failed!\n"));
		exit(1);
	}

	nt_response = data_blob_talloc(samlogon_state->mem_ctx, unicodepw, strlen_m(samlogon_state->password)*2);

	password = strupper_talloc(samlogon_state->mem_ctx, samlogon_state->password);

	if (!convert_string_talloc(samlogon_state->mem_ctx,
				   CH_UNIX, CH_DOS,
				   password, strlen(password)+1,
				   (void**)&dospw, NULL, false)) {
		DEBUG(0, ("convert_string_talloc failed!\n"));
		exit(1);
	}

	lm_response = data_blob_talloc(samlogon_state->mem_ctx, dospw, strlen(dospw));

	nt_status = check_samlogon(samlogon_state,
				   break_which,
				   samlogon_state->parameter_control | MSV1_0_CLEARTEXT_PASSWORD_ALLOWED,
				   &chall,
				   &lm_response,
				   &nt_response,
				   lm_key,
				   user_session_key,
				   error_string);

	if (NT_STATUS_EQUAL(NT_STATUS_WRONG_PASSWORD, nt_status)) {
		/* for 'old' passwords, we allow the server to be OK or wrong password */
		if (samlogon_state->old_password) {
			return true;
		}
		/* for 'long' passwords, the LM password is invalid */
		if (break_which == NO_NT && !lm_good) {
			return true;
		}
		return ((break_which == BREAK_NT) || (break_which == BREAK_BOTH));
	} else if (NT_STATUS_EQUAL(NT_STATUS_NOT_FOUND, nt_status) && strchr_m(samlogon_state->account_name, '@')) {
		return ((break_which == BREAK_NT) || (break_which == BREAK_BOTH) || (break_which == NO_NT));
	} else if (!NT_STATUS_EQUAL(samlogon_state->expected_error, nt_status)) {
		SAFE_FREE(*error_string);
		asprintf(error_string, "Expected error: %s, got %s", nt_errstr(samlogon_state->expected_error), nt_errstr(nt_status));
		return false;
	} else if (NT_STATUS_EQUAL(samlogon_state->expected_error, nt_status) && !NT_STATUS_IS_OK(nt_status)) {
		return true;
	} else if (!NT_STATUS_IS_OK(nt_status)) {
		return false;
	}

	if (break_which == NO_NT && !lm_good) {
	        *error_string = strdup("LM password is 'long' (> 14 chars and therefore invalid) but login did not fail!");
		return false;
	}

	return true;
}

static bool test_plaintext_none_broken(struct samlogon_state *samlogon_state,
				       char **error_string) {
	return test_plaintext(samlogon_state, BREAK_NONE, error_string);
}

static bool test_plaintext_lm_broken(struct samlogon_state *samlogon_state,
				     char **error_string) {
	return test_plaintext(samlogon_state, BREAK_LM, error_string);
}

static bool test_plaintext_nt_broken(struct samlogon_state *samlogon_state,
				     char **error_string) {
	return test_plaintext(samlogon_state, BREAK_NT, error_string);
}

static bool test_plaintext_nt_only(struct samlogon_state *samlogon_state,
				   char **error_string) {
	return test_plaintext(samlogon_state, NO_LM, error_string);
}

static bool test_plaintext_lm_only(struct samlogon_state *samlogon_state,
				   char **error_string) {
	return test_plaintext(samlogon_state, NO_NT, error_string);
}

/*
   Tests:

   - LM only
   - NT and LM
   - NT
   - NT in LM field
   - NT in both fields
   - NTLMv2
   - NTLMv2 and LMv2
   - LMv2
   - plaintext tests (in challenge-response fields)

   check we get the correct session key in each case
   check what values we get for the LM session key

*/

static const struct ntlm_tests {
	bool (*fn)(struct samlogon_state *, char **);
	const char *name;
	bool expect_fail;
} test_table[] = {
	{test_lmv2_ntlmv2, "NTLMv2 and LMv2", false},
#if 0
	{test_lmv2_ntlmv2_no_dom, "NTLMv2 and LMv2 (no domain)", false},
#endif
	{test_lm, "LM", false},
	{test_lm_ntlm, "LM and NTLM", false},
	{test_lm_ntlm_both_broken, "LM and NTLM, both broken", false},
	{test_ntlm, "NTLM", false},
	{test_ntlm_in_lm, "NTLM in LM", false},
	{test_ntlm_in_both, "NTLM in both", false},
	{test_ntlmv2, "NTLMv2", false},
	{test_ntlmv2_no_dom, "NTLMv2 (no domain)", false},
	{test_lmv2, "LMv2", false},
	{test_lmv2_no_dom, "LMv2 (no domain)", false},
	{test_ntlmv2_lmv2_broken, "NTLMv2 and LMv2, LMv2 broken", false},
	{test_ntlmv2_lmv2_broken_no_dom, "NTLMv2 and LMv2, LMv2 broken (no domain)", false},
	{test_ntlmv2_ntlmv2_broken, "NTLMv2 and LMv2, NTLMv2 broken", false},
#if 0
	{test_ntlmv2_ntlmv2_broken_no_dom, "NTLMv2 and LMv2, NTLMv2 broken (no domain)", false},
#endif
	{test_ntlmv2_both_broken, "NTLMv2 and LMv2, both broken", false},
	{test_ntlmv2_both_broken_no_dom, "NTLMv2 and LMv2, both broken (no domain)", false},
	{test_ntlm_lm_broken, "NTLM and LM, LM broken", false},
	{test_ntlm_ntlm_broken, "NTLM and LM, NTLM broken", false},
	{test_ntlm2, "NTLM2 (NTLMv2 session security)", false},
	{test_lmv2_ntlm_both_broken, "LMv2 and NTLM, both broken", false},
	{test_lmv2_ntlm_both_broken_no_dom, "LMv2 and NTLM, both broken (no domain)", false},
	{test_lmv2_ntlm_break_ntlm, "LMv2 and NTLM, NTLM broken", false},
	{test_lmv2_ntlm_break_ntlm_no_dom, "LMv2 and NTLM, NTLM broken (no domain)", false},
	{test_lmv2_ntlm_break_lm, "LMv2 and NTLM, LMv2 broken", false},
	{test_lmv2_ntlm_break_lm_no_dom, "LMv2 and NTLM, LMv2 broken (no domain)", false},
	{test_plaintext_none_broken, "Plaintext", false},
	{test_plaintext_lm_broken, "Plaintext LM broken", false},
	{test_plaintext_nt_broken, "Plaintext NT broken", false},
	{test_plaintext_nt_only, "Plaintext NT only", false},
	{test_plaintext_lm_only, "Plaintext LM only", false},
	{NULL, NULL}
};

/*
  try a netlogon SamLogon
*/
static bool test_SamLogon(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			  struct torture_context *tctx,
			  struct netlogon_creds_CredentialState *creds,
			  const char *comment,
			  const char *account_domain, const char *account_name,
			  const char *plain_pass, uint32_t parameter_control,
			  NTSTATUS expected_error, bool old_password,
			  int n_subtests)
{
	TALLOC_CTX *fn_ctx = talloc_named(mem_ctx, 0, "test_SamLogon function-level context");
	int i, v, l, f;
	bool ret = true;
	int validation_levels[] = {2,3,6};
	int logon_levels[] = { 2, 6 };
	int function_levels[] = {
		NDR_NETR_LOGONSAMLOGON,
		NDR_NETR_LOGONSAMLOGONEX,
		NDR_NETR_LOGONSAMLOGONWITHFLAGS };
	struct samlogon_state samlogon_state;

	union netr_LogonLevel logon;
	union netr_Validation validation;
	uint8_t authoritative = 0;
	uint32_t flags = 0;

	ZERO_STRUCT(logon);

	d_printf("Testing netr_LogonSamLogon and netr_LogonSamLogonWithFlags\n");

	samlogon_state.comment = comment;
	samlogon_state.account_name = account_name;
	samlogon_state.account_domain = account_domain;
	samlogon_state.password = plain_pass;
	samlogon_state.workgroup = lpcfg_workgroup(tctx->lp_ctx);
	samlogon_state.netbios_name = lpcfg_netbios_name(tctx->lp_ctx);
	samlogon_state.p = p;
	samlogon_state.creds = creds;
	samlogon_state.expected_error = expected_error;
	samlogon_state.chall = data_blob_talloc(fn_ctx, NULL, 8);
	samlogon_state.parameter_control = parameter_control;
	samlogon_state.old_password = old_password;

	generate_random_buffer(samlogon_state.chall.data, 8);
	samlogon_state.r_flags.in.server_name = talloc_asprintf(fn_ctx, "\\\\%s", dcerpc_server_name(p));
	samlogon_state.r_flags.in.computer_name = TEST_MACHINE_NAME;
	samlogon_state.r_flags.in.credential = &samlogon_state.auth;
	samlogon_state.r_flags.in.return_authenticator = &samlogon_state.auth2;
	samlogon_state.r_flags.in.flags = &flags;
	samlogon_state.r_flags.in.logon = &logon;
	samlogon_state.r_flags.out.validation = &validation;
	samlogon_state.r_flags.out.authoritative = &authoritative;
	samlogon_state.r_flags.out.flags = &flags;

	samlogon_state.r_ex.in.server_name = talloc_asprintf(fn_ctx, "\\\\%s", dcerpc_server_name(p));
	samlogon_state.r_ex.in.computer_name = TEST_MACHINE_NAME;
	samlogon_state.r_ex.in.flags = &flags;
	samlogon_state.r_ex.in.logon = &logon;
	samlogon_state.r_ex.out.validation = &validation;
	samlogon_state.r_ex.out.authoritative = &authoritative;
	samlogon_state.r_ex.out.flags = &flags;

	samlogon_state.r.in.server_name = talloc_asprintf(fn_ctx, "\\\\%s", dcerpc_server_name(p));
	samlogon_state.r.in.computer_name = TEST_MACHINE_NAME;
	samlogon_state.r.in.credential = &samlogon_state.auth;
	samlogon_state.r.in.return_authenticator = &samlogon_state.auth2;
	samlogon_state.r.in.logon = &logon;
	samlogon_state.r.out.validation = &validation;
	samlogon_state.r.out.authoritative = &authoritative;


	for (f=0;f<ARRAY_SIZE(function_levels);f++) {
		for (i=0; test_table[i].fn; i++) {
			if (n_subtests && (i > n_subtests)) {
				continue;
			}
			for (v=0;v<ARRAY_SIZE(validation_levels);v++) {
				for (l=0;l<ARRAY_SIZE(logon_levels);l++) {
					char *error_string = NULL;
					TALLOC_CTX *tmp_ctx = talloc_named(fn_ctx, 0, "test_SamLogon inner loop");
					samlogon_state.mem_ctx = tmp_ctx;
					samlogon_state.function_level = function_levels[f];
					samlogon_state.r.in.validation_level = validation_levels[v];
					samlogon_state.r.in.logon_level = logon_levels[l];
					samlogon_state.r_ex.in.validation_level = validation_levels[v];
					samlogon_state.r_ex.in.logon_level = logon_levels[l];
					samlogon_state.r_flags.in.validation_level = validation_levels[v];
					samlogon_state.r_flags.in.logon_level = logon_levels[l];
					if (!test_table[i].fn(&samlogon_state, &error_string)) {
						d_printf("Testing '%s' [%s]\\[%s] '%s' at validation level %d, logon level %d, function %d: \n",
						       samlogon_state.comment,
						       samlogon_state.account_domain,
						       samlogon_state.account_name,
						       test_table[i].name, validation_levels[v],
						       logon_levels[l], function_levels[f]);

						if (test_table[i].expect_fail) {
							d_printf(" failed (expected, test incomplete): %s\n", error_string);
						} else {
							d_printf(" failed: %s\n", error_string);
							ret = false;
						}
						SAFE_FREE(error_string);
					}
					talloc_free(tmp_ctx);
				}
			}
		}
	}
	talloc_free(fn_ctx);
	return ret;
}

/*
  test an ADS style interactive domain logon
*/
bool test_InteractiveLogon(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			   struct netlogon_creds_CredentialState *creds,
			   const char *comment,
			   const char *workstation_name,
			   const char *account_domain, const char *account_name,
			   const char *plain_pass, uint32_t parameter_control,
			   NTSTATUS expected_error)
{
	NTSTATUS status;
	TALLOC_CTX *fn_ctx = talloc_named(mem_ctx, 0, "test_InteractiveLogon function-level context");
	struct netr_LogonSamLogonWithFlags r;
	struct netr_Authenticator a, ra;
	struct netr_PasswordInfo pinfo;
	uint32_t flags = 0;

	union netr_LogonLevel logon;
	union netr_Validation validation;
	uint8_t authoritative = 0;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(a);
	ZERO_STRUCT(r);
	ZERO_STRUCT(ra);

	ZERO_STRUCT(logon);
	ZERO_STRUCT(validation);

	netlogon_creds_client_authenticator(creds, &a);

	logon.password = &pinfo;

	r.in.server_name = talloc_asprintf(fn_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computer_name = TEST_MACHINE_NAME;
	r.in.credential = &a;
	r.in.return_authenticator = &ra;
	r.in.logon_level = 5;
	r.in.logon = &logon;
	r.in.validation_level = 6;
	r.in.flags = &flags;
	r.out.validation = &validation;
	r.out.authoritative = &authoritative;
	r.out.flags = &flags;

	pinfo.identity_info.domain_name.string = account_domain;
	pinfo.identity_info.parameter_control = parameter_control;
	pinfo.identity_info.logon_id_low = 0;
	pinfo.identity_info.logon_id_high = 0;
	pinfo.identity_info.account_name.string = account_name;
	pinfo.identity_info.workstation.string = workstation_name;

	if (!E_deshash(plain_pass, pinfo.lmpassword.hash)) {
		ZERO_STRUCT(pinfo.lmpassword.hash);
	}
	E_md4hash(plain_pass, pinfo.ntpassword.hash);

	if (creds->negotiate_flags & NETLOGON_NEG_ARCFOUR) {
		netlogon_creds_arcfour_crypt(creds, pinfo.lmpassword.hash, 16);
		netlogon_creds_arcfour_crypt(creds, pinfo.ntpassword.hash, 16);
	} else {
		netlogon_creds_des_encrypt(creds, &pinfo.lmpassword);
		netlogon_creds_des_encrypt(creds, &pinfo.ntpassword);
	}

	d_printf("Testing netr_LogonSamLogonWithFlags '%s' (Interactive Logon)\n", comment);

	status = dcerpc_netr_LogonSamLogonWithFlags_r(b, fn_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s: netr_LogonSamLogonWithFlags - %s\n",
			 __location__, nt_errstr(status));
		return false;
	}
	if (!r.out.return_authenticator
	    || !netlogon_creds_client_check(creds, &r.out.return_authenticator->cred)) {
		d_printf("Credential chaining failed\n");
		talloc_free(fn_ctx);
		return false;
	}

	talloc_free(fn_ctx);

	if (!NT_STATUS_EQUAL(expected_error, r.out.result)) {
		d_printf("[%s]\\[%s] netr_LogonSamLogonWithFlags - expected %s got %s\n",
		       account_domain, account_name, nt_errstr(expected_error), nt_errstr(r.out.result));
		return false;
	}

	return true;
}

/* This sets and resets the "minPwdAge" (in order to allow immediate user
 * password changes). The behaviour is controlled by the "set" boolean. */
static bool handle_minPwdAge(struct torture_context *torture,
			     TALLOC_CTX *mem_ctx, bool set)
{
	struct dcerpc_pipe *p;
	struct policy_handle connect_handle, domain_handle;
	struct samr_Connect c_r;
	struct samr_LookupDomain ld_r;
	struct samr_OpenDomain od_r;
	struct samr_QueryDomainInfo qdi_r;
	struct samr_SetDomainInfo sdi_r;
	struct samr_Close cl_r;
	struct lsa_String domName;
	struct dom_sid *domSid = NULL;
	union samr_DomainInfo *domInfo = NULL;
	static int64_t old_minPwdAge = 0;
	NTSTATUS status;

	status = torture_rpc_connection(torture, &p, &ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	c_r.in.system_name = 0;
	c_r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	c_r.out.connect_handle = &connect_handle;

	torture_assert_ntstatus_ok(torture,
				   dcerpc_samr_Connect_r(p->binding_handle, mem_ctx, &c_r),
				   "Connect failed");
	torture_assert_ntstatus_ok(torture, c_r.out.result, "Connect failed");

	ld_r.in.connect_handle = &connect_handle;
	ld_r.in.domain_name = &domName;
	ld_r.in.domain_name->string = lpcfg_workgroup(torture->lp_ctx);
	ld_r.out.sid = &domSid;

	torture_assert_ntstatus_ok(torture,
				   dcerpc_samr_LookupDomain_r(p->binding_handle, mem_ctx, &ld_r),
				   "LookupDomain failed");
	torture_assert_ntstatus_ok(torture, ld_r.out.result,
				   "LookupDomain failed");

	od_r.in.connect_handle = &connect_handle;
	od_r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	od_r.in.sid = *ld_r.out.sid;
	od_r.out.domain_handle = &domain_handle;

	torture_assert_ntstatus_ok(torture,
				   dcerpc_samr_OpenDomain_r(p->binding_handle, mem_ctx, &od_r),
				   "OpenDomain failed");
	torture_assert_ntstatus_ok(torture, od_r.out.result,
				   "OpenDomain failed");

	qdi_r.in.domain_handle = &domain_handle;
	qdi_r.in.level = DomainPasswordInformation;
	qdi_r.out.info = &domInfo;

	torture_assert_ntstatus_ok(torture,
				   dcerpc_samr_QueryDomainInfo_r(p->binding_handle, mem_ctx, &qdi_r),
				   "QueryDomainInfo failed");
	torture_assert_ntstatus_ok(torture, qdi_r.out.result,
				   "QueryDomainInfo failed");

	if (set) {
		old_minPwdAge = domInfo->info1.min_password_age;
		domInfo->info1.min_password_age = 0;
	} else {
		domInfo->info1.min_password_age = old_minPwdAge;
	}

	sdi_r.in.domain_handle = &domain_handle;
	sdi_r.in.level = DomainPasswordInformation;
	sdi_r.in.info = domInfo;

	torture_assert_ntstatus_ok(torture,
				   dcerpc_samr_SetDomainInfo_r(p->binding_handle, mem_ctx, &sdi_r),
				   "SetDomainInfo failed");
	torture_assert_ntstatus_ok(torture, sdi_r.out.result,
				   "SetDomainInfo failed");

	cl_r.in.handle = &connect_handle;
	cl_r.out.handle = &connect_handle;

	torture_assert_ntstatus_ok(torture,
				   dcerpc_samr_Close_r(p->binding_handle, mem_ctx, &cl_r),
				   "Close failed");
	torture_assert_ntstatus_ok(torture, cl_r.out.result, "Close failed");

	return true;
}

bool torture_rpc_samlogon(struct torture_context *torture)
{
        NTSTATUS status;
        struct dcerpc_pipe *p;
	struct dcerpc_binding *b;
	struct cli_credentials *machine_credentials;
	TALLOC_CTX *mem_ctx = talloc_init("torture_rpc_netlogon");
	bool ret = true;
	struct test_join *join_ctx = NULL;
	struct test_join *user_ctx = NULL, *user_ctx_wrong_wks = NULL, *user_ctx_wrong_time = NULL;
	char *user_password, *user_password_wrong_wks, *user_password_wrong_time;
	const char *old_user_password;
	char *test_machine_account;
	const char *userdomain;
	struct samr_SetUserInfo s;
	union samr_UserInfo u;
	int i;
	int ci;

	unsigned int credential_flags[] = {
		NETLOGON_NEG_AUTH2_FLAGS,
		NETLOGON_NEG_ARCFOUR,
		NETLOGON_NEG_ARCFOUR | NETLOGON_NEG_128BIT,
		NETLOGON_NEG_AUTH2_ADS_FLAGS,
		0 /* yes, this is a valid flag, causes the use of DES */
	};

	struct netlogon_creds_CredentialState *creds;
	struct dcerpc_pipe *tmp_p = NULL;

	torture_assert(torture, handle_minPwdAge(torture, mem_ctx, true),
		       "handle_minPwdAge error!");

	test_machine_account = talloc_asprintf(mem_ctx, "%s$", TEST_MACHINE_NAME);
	/* We only need to join as a workstation here, and in future,
	 * if we wish to test against trusted domains, we must be a
	 * workstation here */
	join_ctx = torture_join_domain(torture, TEST_MACHINE_NAME, ACB_WSTRUST,
				       &machine_credentials);
	if (!join_ctx) {
		d_printf("Failed to join as Workstation\n");
		return false;
	}

	userdomain = torture_setting_string(torture, "userdomain", lpcfg_workgroup(torture->lp_ctx));

	user_ctx = torture_create_testuser(torture,
					   TEST_USER_NAME,
					   userdomain,
					   ACB_NORMAL,
					   (const char **)&user_password);
	if (!user_ctx) {
		d_printf("Failed to create a test user\n");
		return false;
	}

	old_user_password = user_password;

	tmp_p = torture_join_samr_pipe(user_ctx);
	test_ChangePasswordUser3(tmp_p, torture,
				 TEST_USER_NAME, 16 /* > 14 */, &user_password,
				 NULL, 0, false);

	user_ctx_wrong_wks = torture_create_testuser(torture,
						     TEST_USER_NAME_WRONG_WKS,
					   userdomain,
					   ACB_NORMAL,
					   (const char **)&user_password_wrong_wks);
	if (!user_ctx_wrong_wks) {
		d_printf("Failed to create a test user (wrong workstation test)\n");
		return false;
	}

	ZERO_STRUCT(u);
	s.in.user_handle = torture_join_samr_user_policy(user_ctx_wrong_wks);
	s.in.info = &u;
	s.in.level = 21;

	u.info21.fields_present = SAMR_FIELD_WORKSTATIONS;
	u.info21.workstations.string = "not" TEST_MACHINE_NAME;

	tmp_p = torture_join_samr_pipe(user_ctx_wrong_wks);
	status = dcerpc_samr_SetUserInfo_r(tmp_p->binding_handle, mem_ctx, &s);
	if (!NT_STATUS_IS_OK(status)) {
		printf("SetUserInfo (list of workstations) failed - %s\n", nt_errstr(status));
		ret = false;
		goto failed;
	}
	if (!NT_STATUS_IS_OK(s.out.result)) {
		printf("SetUserInfo (list of workstations) failed - %s\n", nt_errstr(s.out.result));
		ret = false;
		goto failed;
	}

	user_ctx_wrong_time
		= torture_create_testuser(torture, TEST_USER_NAME_WRONG_TIME,
					   userdomain,
					   ACB_NORMAL,
					   (const char **)&user_password_wrong_time);
	if (!user_ctx_wrong_time) {
		d_printf("Failed to create a test user (wrong workstation test)\n");
		return false;
	}

	ZERO_STRUCT(u);
	s.in.user_handle = torture_join_samr_user_policy(user_ctx_wrong_time);
	s.in.info = &u;
	s.in.level = 21;

	u.info21.fields_present = SAMR_FIELD_WORKSTATIONS | SAMR_FIELD_LOGON_HOURS;
	u.info21.workstations.string = TEST_MACHINE_NAME;
	u.info21.logon_hours.units_per_week = 168;
	u.info21.logon_hours.bits = talloc_zero_array(mem_ctx, uint8_t, 168);

	tmp_p = torture_join_samr_pipe(user_ctx_wrong_time);
	status = dcerpc_samr_SetUserInfo_r(tmp_p->binding_handle, mem_ctx, &s);
	if (!NT_STATUS_IS_OK(status)) {
		printf("SetUserInfo (logon times and list of workstations) failed - %s\n", nt_errstr(status));
		ret = false;
		goto failed;
	}
	if (!NT_STATUS_IS_OK(s.out.result)) {
		printf("SetUserInfo (list of workstations) failed - %s\n", nt_errstr(s.out.result));
		ret = false;
		goto failed;
	}

	status = torture_rpc_binding(torture, &b);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
		goto failed;
	}

	/* We have to use schannel, otherwise the SamLogonEx fails
	 * with INTERNAL_ERROR */

	b->flags &= ~DCERPC_AUTH_OPTIONS;
	b->flags |= DCERPC_SCHANNEL | DCERPC_SIGN | DCERPC_SCHANNEL_128;

	status = dcerpc_pipe_connect_b(mem_ctx, &p, b,
				       &ndr_table_netlogon,
				       machine_credentials, torture->ev, torture->lp_ctx);

	if (!NT_STATUS_IS_OK(status)) {
		d_printf("RPC pipe connect as domain member failed: %s\n", nt_errstr(status));
		ret = false;
		goto failed;
	}

	status = dcerpc_schannel_creds(p->conn->security_state.generic_state, mem_ctx, &creds);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
		goto failed;
	}

	{

		struct {
			const char *comment;
			const char *domain;
			const char *username;
			const char *password;
			bool network_login;
			NTSTATUS expected_interactive_error;
			NTSTATUS expected_network_error;
			uint32_t parameter_control;
			bool old_password; /* Allow an old password to be accepted or rejected without error, as well as session key bugs */
		} usercreds[] = {
			{
				.comment       = "domain\\user",
				.domain        = cli_credentials_get_domain(cmdline_credentials),
				.username      = cli_credentials_get_username(cmdline_credentials),
				.password      = cli_credentials_get_password(cmdline_credentials),
				.network_login = true,
				.expected_interactive_error = NT_STATUS_OK,
				.expected_network_error     = NT_STATUS_OK
			},
			{
				.comment       = "realm\\user",
				.domain        = cli_credentials_get_realm(cmdline_credentials),
				.username      = cli_credentials_get_username(cmdline_credentials),
				.password      = cli_credentials_get_password(cmdline_credentials),
				.network_login = true,
				.expected_interactive_error = NT_STATUS_OK,
				.expected_network_error     = NT_STATUS_OK
			},
			{
				.comment       = "user@domain",
				.domain        = NULL,
				.username      = talloc_asprintf(mem_ctx,
						"%s@%s",
						cli_credentials_get_username(cmdline_credentials),
						cli_credentials_get_domain(cmdline_credentials)
					),
				.password      = cli_credentials_get_password(cmdline_credentials),
				.network_login = false, /* works for some things, but not NTLMv2.  Odd */
				.expected_interactive_error = NT_STATUS_OK,
				.expected_network_error     = NT_STATUS_OK
			},
			{
				.comment       = "user@realm",
				.domain        = NULL,
				.username      = talloc_asprintf(mem_ctx,
						"%s@%s",
						cli_credentials_get_username(cmdline_credentials),
						cli_credentials_get_realm(cmdline_credentials)
					),
				.password      = cli_credentials_get_password(cmdline_credentials),
				.network_login = true,
				.expected_interactive_error = NT_STATUS_OK,
				.expected_network_error     = NT_STATUS_OK
			},
			{
				.comment      = "machine domain\\user",
				.domain       = cli_credentials_get_domain(machine_credentials),
				.username     = cli_credentials_get_username(machine_credentials),
				.password     = cli_credentials_get_password(machine_credentials),
				.network_login = true,
				.expected_interactive_error = NT_STATUS_NO_SUCH_USER,
				.parameter_control = MSV1_0_ALLOW_WORKSTATION_TRUST_ACCOUNT
			},
			{
				.comment      = "machine domain\\user",
				.domain       = cli_credentials_get_domain(machine_credentials),
				.username     = cli_credentials_get_username(machine_credentials),
				.password     = cli_credentials_get_password(machine_credentials),
				.network_login = true,
				.expected_interactive_error = NT_STATUS_NO_SUCH_USER,
				.expected_network_error = NT_STATUS_NOLOGON_WORKSTATION_TRUST_ACCOUNT
			},
			{
				.comment       = "machine realm\\user",
				.domain        = cli_credentials_get_realm(machine_credentials),
				.username      = cli_credentials_get_username(machine_credentials),
				.password      = cli_credentials_get_password(machine_credentials),
				.network_login = true,
				.expected_interactive_error = NT_STATUS_NO_SUCH_USER,
				.parameter_control = MSV1_0_ALLOW_WORKSTATION_TRUST_ACCOUNT
			},
			{
				.comment       = "machine user@domain",
				.domain        = NULL,
				.username      = talloc_asprintf(mem_ctx,
								"%s@%s",
								cli_credentials_get_username(machine_credentials),
								cli_credentials_get_domain(machine_credentials)
					),
				.password      = cli_credentials_get_password(machine_credentials),
				.network_login = false, /* works for some things, but not NTLMv2.  Odd */
				.expected_interactive_error = NT_STATUS_NO_SUCH_USER,
				.parameter_control = MSV1_0_ALLOW_WORKSTATION_TRUST_ACCOUNT
			},
			{
				.comment       = "machine user@realm",
				.domain        = NULL,
				.username      = talloc_asprintf(mem_ctx,
								"%s@%s",
								cli_credentials_get_username(machine_credentials),
								cli_credentials_get_realm(machine_credentials)
					),
				.password      = cli_credentials_get_password(machine_credentials),
				.network_login = true,
				.expected_interactive_error = NT_STATUS_NO_SUCH_USER,
				.parameter_control = MSV1_0_ALLOW_WORKSTATION_TRUST_ACCOUNT
			},
			{
				.comment       = "test user (long pw): domain\\user",
				.domain        = userdomain,
				.username      = TEST_USER_NAME,
				.password      = user_password,
				.network_login = true,
				.expected_interactive_error = NT_STATUS_OK,
				.expected_network_error     = NT_STATUS_OK
			},
			{
				.comment       = "test user (long pw): user@realm",
				.domain        = NULL,
				.username      = talloc_asprintf(mem_ctx,
								 "%s@%s",
								 TEST_USER_NAME,
								 lpcfg_realm(torture->lp_ctx)),
				.password      = user_password,
				.network_login = true,
				.expected_interactive_error = NT_STATUS_OK,
				.expected_network_error     = NT_STATUS_OK
			},
			{
				.comment       = "test user (long pw): user@domain",
				.domain        = NULL,
				.username      = talloc_asprintf(mem_ctx,
								 "%s@%s",
								 TEST_USER_NAME,
								 userdomain),
				.password      = user_password,
				.network_login = false, /* works for some things, but not NTLMv2.  Odd */
				.expected_interactive_error = NT_STATUS_OK,
				.expected_network_error     = NT_STATUS_OK
			},
			/* Oddball, can we use the old password ? */
			{
				.comment       = "test user: user\\domain OLD PASSWORD",
				.domain        = userdomain,
				.username      = TEST_USER_NAME,
				.password      = old_user_password,
				.network_login = true,
				.expected_interactive_error = NT_STATUS_WRONG_PASSWORD,
				.expected_network_error     = NT_STATUS_OK,
				.old_password  = true
			},
			{
				.comment       = "test user (wong workstation): domain\\user",
				.domain        = userdomain,
				.username      = TEST_USER_NAME_WRONG_WKS,
				.password      = user_password_wrong_wks,
				.network_login = true,
				.expected_interactive_error = NT_STATUS_INVALID_WORKSTATION,
				.expected_network_error     = NT_STATUS_INVALID_WORKSTATION
			}
		};

		/* Try all the tests for different username forms */
		for (ci = 0; ci < ARRAY_SIZE(usercreds); ci++) {

			if (!test_InteractiveLogon(p, mem_ctx, creds,
						   usercreds[ci].comment,
						   TEST_MACHINE_NAME,
						   usercreds[ci].domain,
						   usercreds[ci].username,
						   usercreds[ci].password,
						   usercreds[ci].parameter_control,
						   usercreds[ci].expected_interactive_error)) {
				ret = false;
			}

			if (usercreds[ci].network_login) {
				if (!test_SamLogon(p, mem_ctx, torture, creds,
						   usercreds[ci].comment,
						   usercreds[ci].domain,
						   usercreds[ci].username,
						   usercreds[ci].password,
						   usercreds[ci].parameter_control,
						   usercreds[ci].expected_network_error,
						   usercreds[ci].old_password,
						   0)) {
					ret = false;
				}
			}
		}

		/* Using the first username form, try the different
		 * credentials flag setups, on only one of the tests (checks
		 * session key encryption) */

		for (i=0; i < ARRAY_SIZE(credential_flags); i++) {
			/* TODO:  Somehow we lost setting up the different credential flags here! */

			if (!test_InteractiveLogon(p, mem_ctx, creds,
						   usercreds[0].comment,
						   TEST_MACHINE_NAME,
						   usercreds[0].domain,
						   usercreds[0].username,
						   usercreds[0].password,
						   usercreds[0].parameter_control,
						   usercreds[0].expected_interactive_error)) {
				ret = false;
			}

			if (usercreds[0].network_login) {
				if (!test_SamLogon(p, mem_ctx, torture, creds,
						   usercreds[0].comment,
						   usercreds[0].domain,
						   usercreds[0].username,
						   usercreds[0].password,
						   usercreds[0].parameter_control,
						   usercreds[0].expected_network_error,
						   usercreds[0].old_password,
						   1)) {
					ret = false;
				}
			}
		}

	}
failed:
	torture_assert(torture, handle_minPwdAge(torture, mem_ctx, false),
		       "handle_minPwdAge error!");

	talloc_free(mem_ctx);

	torture_leave_domain(torture, join_ctx);
	torture_leave_domain(torture, user_ctx);
	torture_leave_domain(torture, user_ctx_wrong_wks);
	torture_leave_domain(torture, user_ctx_wrong_time);
	return ret;
}
