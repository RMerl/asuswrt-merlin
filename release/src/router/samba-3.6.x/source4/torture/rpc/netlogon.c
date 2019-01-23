/* 
   Unix SMB/CIFS implementation.

   test suite for netlogon rpc operations

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2003-2004
   Copyright (C) Tim Potter      2003
   Copyright (C) Matthias Dieter Wallnöfer            2009-2010
   
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
#include "lib/events/events.h"
#include "lib/cmdline/popt_common.h"
#include "torture/rpc/torture_rpc.h"
#include "../lib/crypto/crypto.h"
#include "libcli/auth/libcli_auth.h"
#include "librpc/gen_ndr/ndr_netlogon_c.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "param/param.h"
#include "libcli/security/security.h"
#include <ldb.h>
#include "lib/util/util_ldb.h"
#include "ldb_wrap.h"
#include "lib/replace/system/network.h"
#include "dsdb/samdb/samdb.h"

#define TEST_MACHINE_NAME "torturetest"

static bool test_LogonUasLogon(struct torture_context *tctx, 
			       struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct netr_LogonUasLogon r;
	struct netr_UasInfo *info = NULL;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = NULL;
	r.in.account_name = cli_credentials_get_username(cmdline_credentials);
	r.in.workstation = TEST_MACHINE_NAME;
	r.out.info = &info;

	status = dcerpc_netr_LogonUasLogon_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "LogonUasLogon");

	return true;
}

static bool test_LogonUasLogoff(struct torture_context *tctx,
				struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct netr_LogonUasLogoff r;
	struct netr_UasLogoffInfo info;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = NULL;
	r.in.account_name = cli_credentials_get_username(cmdline_credentials);
	r.in.workstation = TEST_MACHINE_NAME;
	r.out.info = &info;

	status = dcerpc_netr_LogonUasLogoff_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "LogonUasLogoff");

	return true;
}

bool test_SetupCredentials(struct dcerpc_pipe *p, struct torture_context *tctx,
				  struct cli_credentials *credentials,
				  struct netlogon_creds_CredentialState **creds_out)
{
	struct netr_ServerReqChallenge r;
	struct netr_ServerAuthenticate a;
	struct netr_Credential credentials1, credentials2, credentials3;
	struct netlogon_creds_CredentialState *creds;
	const struct samr_Password *mach_password;
   	const char *machine_name;
	struct dcerpc_binding_handle *b = p->binding_handle;

	mach_password = cli_credentials_get_nt_hash(credentials, tctx);
	machine_name = cli_credentials_get_workstation(credentials);

	torture_comment(tctx, "Testing ServerReqChallenge\n");

	r.in.server_name = NULL;
	r.in.computer_name = machine_name;
	r.in.credentials = &credentials1;
	r.out.return_credentials = &credentials2;

	generate_random_buffer(credentials1.data, sizeof(credentials1.data));

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerReqChallenge_r(b, tctx, &r),
		"ServerReqChallenge failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "ServerReqChallenge failed");

	a.in.server_name = NULL;
	a.in.account_name = talloc_asprintf(tctx, "%s$", machine_name);
	a.in.secure_channel_type = cli_credentials_get_secure_channel_type(credentials);
	a.in.computer_name = machine_name;
	a.in.credentials = &credentials3;
	a.out.return_credentials = &credentials3;

	creds = netlogon_creds_client_init(tctx, a.in.account_name,
					   a.in.computer_name,
					   &credentials1, &credentials2, 
					   mach_password, &credentials3, 
					   0);
	torture_assert(tctx, creds != NULL, "memory allocation");


	torture_comment(tctx, "Testing ServerAuthenticate\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerAuthenticate_r(b, tctx, &a),
		"ServerAuthenticate failed");

	/* This allows the tests to continue against the more fussy windows 2008 */
	if (NT_STATUS_EQUAL(a.out.result, NT_STATUS_DOWNGRADE_DETECTED)) {
		return test_SetupCredentials2(p, tctx, NETLOGON_NEG_AUTH2_ADS_FLAGS, 
					      credentials,
					      cli_credentials_get_secure_channel_type(credentials),
					      creds_out);
	}

	torture_assert_ntstatus_ok(tctx, a.out.result, "ServerAuthenticate");

	torture_assert(tctx, netlogon_creds_client_check(creds, &credentials3), 
		       "Credential chaining failed");

	*creds_out = creds;
	return true;
}

bool test_SetupCredentials2(struct dcerpc_pipe *p, struct torture_context *tctx,
			    uint32_t negotiate_flags,
			    struct cli_credentials *machine_credentials,
			    enum netr_SchannelType sec_chan_type,
			    struct netlogon_creds_CredentialState **creds_out)
{
	struct netr_ServerReqChallenge r;
	struct netr_ServerAuthenticate2 a;
	struct netr_Credential credentials1, credentials2, credentials3;
	struct netlogon_creds_CredentialState *creds;
	const struct samr_Password *mach_password;
	const char *machine_name;
	struct dcerpc_binding_handle *b = p->binding_handle;

	mach_password = cli_credentials_get_nt_hash(machine_credentials, tctx);
	machine_name = cli_credentials_get_workstation(machine_credentials);

	torture_comment(tctx, "Testing ServerReqChallenge\n");


	r.in.server_name = NULL;
	r.in.computer_name = machine_name;
	r.in.credentials = &credentials1;
	r.out.return_credentials = &credentials2;

	generate_random_buffer(credentials1.data, sizeof(credentials1.data));

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerReqChallenge_r(b, tctx, &r),
		"ServerReqChallenge failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "ServerReqChallenge failed");

	a.in.server_name = NULL;
	a.in.account_name = talloc_asprintf(tctx, "%s$", machine_name);
	a.in.secure_channel_type = sec_chan_type;
	a.in.computer_name = machine_name;
	a.in.negotiate_flags = &negotiate_flags;
	a.out.negotiate_flags = &negotiate_flags;
	a.in.credentials = &credentials3;
	a.out.return_credentials = &credentials3;

	creds = netlogon_creds_client_init(tctx, a.in.account_name,
					   a.in.computer_name, 
					   &credentials1, &credentials2, 
					   mach_password, &credentials3, 
					   negotiate_flags);

	torture_assert(tctx, creds != NULL, "memory allocation");

	torture_comment(tctx, "Testing ServerAuthenticate2\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerAuthenticate2_r(b, tctx, &a),
		"ServerAuthenticate2 failed");
	torture_assert_ntstatus_ok(tctx, a.out.result, "ServerAuthenticate2 failed");

	torture_assert(tctx, netlogon_creds_client_check(creds, &credentials3), 
		"Credential chaining failed");

	torture_comment(tctx, "negotiate_flags=0x%08x\n", negotiate_flags);

	*creds_out = creds;
	return true;
}


bool test_SetupCredentials3(struct dcerpc_pipe *p, struct torture_context *tctx,
			    uint32_t negotiate_flags,
			    struct cli_credentials *machine_credentials,
			    struct netlogon_creds_CredentialState **creds_out)
{
	struct netr_ServerReqChallenge r;
	struct netr_ServerAuthenticate3 a;
	struct netr_Credential credentials1, credentials2, credentials3;
	struct netlogon_creds_CredentialState *creds;
	struct samr_Password mach_password;
	uint32_t rid;
	const char *machine_name;
	const char *plain_pass;
	struct dcerpc_binding_handle *b = p->binding_handle;

	machine_name = cli_credentials_get_workstation(machine_credentials);
	plain_pass = cli_credentials_get_password(machine_credentials);

	torture_comment(tctx, "Testing ServerReqChallenge\n");

	r.in.server_name = NULL;
	r.in.computer_name = machine_name;
	r.in.credentials = &credentials1;
	r.out.return_credentials = &credentials2;

	generate_random_buffer(credentials1.data, sizeof(credentials1.data));

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerReqChallenge_r(b, tctx, &r),
		"ServerReqChallenge failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "ServerReqChallenge failed");

	E_md4hash(plain_pass, mach_password.hash);

	a.in.server_name = NULL;
	a.in.account_name = talloc_asprintf(tctx, "%s$", machine_name);
	a.in.secure_channel_type = cli_credentials_get_secure_channel_type(machine_credentials);
	a.in.computer_name = machine_name;
	a.in.negotiate_flags = &negotiate_flags;
	a.in.credentials = &credentials3;
	a.out.return_credentials = &credentials3;
	a.out.negotiate_flags = &negotiate_flags;
	a.out.rid = &rid;

	creds = netlogon_creds_client_init(tctx, a.in.account_name,
					   a.in.computer_name,
					   &credentials1, &credentials2, 
					   &mach_password, &credentials3,
					   negotiate_flags);
	
	torture_assert(tctx, creds != NULL, "memory allocation");

	torture_comment(tctx, "Testing ServerAuthenticate3\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerAuthenticate3_r(b, tctx, &a),
		"ServerAuthenticate3 failed");
	torture_assert_ntstatus_ok(tctx, a.out.result, "ServerAuthenticate3 failed");
	torture_assert(tctx, netlogon_creds_client_check(creds, &credentials3), "Credential chaining failed");

	torture_comment(tctx, "negotiate_flags=0x%08x\n", negotiate_flags);
	
	/* Prove that requesting a challenge again won't break it */
	torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerReqChallenge_r(b, tctx, &r),
		"ServerReqChallenge failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "ServerReqChallenge failed");

	*creds_out = creds;
	return true;
}

/*
  try a change password for our machine account
*/
static bool test_SetPassword(struct torture_context *tctx, 
			     struct dcerpc_pipe *p,
			     struct cli_credentials *machine_credentials)
{
	struct netr_ServerPasswordSet r;
	const char *password;
	struct netlogon_creds_CredentialState *creds;
	struct netr_Authenticator credential, return_authenticator;
	struct samr_Password new_password;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_SetupCredentials(p, tctx, machine_credentials, &creds)) {
		return false;
	}

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.account_name = talloc_asprintf(tctx, "%s$", TEST_MACHINE_NAME);
	r.in.secure_channel_type = cli_credentials_get_secure_channel_type(machine_credentials);
	r.in.computer_name = TEST_MACHINE_NAME;
	r.in.credential = &credential;
	r.in.new_password = &new_password;
	r.out.return_authenticator = &return_authenticator;

	password = generate_random_password(tctx, 8, 255);
	E_md4hash(password, new_password.hash);

	netlogon_creds_des_encrypt(creds, &new_password);

	torture_comment(tctx, "Testing ServerPasswordSet on machine account\n");
	torture_comment(tctx, "Changing machine account password to '%s'\n", 
			password);

	netlogon_creds_client_authenticator(creds, &credential);

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerPasswordSet_r(b, tctx, &r),
		"ServerPasswordSet failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "ServerPasswordSet failed");

	if (!netlogon_creds_client_check(creds, &r.out.return_authenticator->cred)) {
		torture_comment(tctx, "Credential chaining failed\n");
	}

	/* by changing the machine password twice we test the
	   credentials chaining fully, and we verify that the server
	   allows the password to be set to the same value twice in a
	   row (match win2k3) */
	torture_comment(tctx, 
		"Testing a second ServerPasswordSet on machine account\n");
	torture_comment(tctx, 
		"Changing machine account password to '%s' (same as previous run)\n", password);

	netlogon_creds_client_authenticator(creds, &credential);

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerPasswordSet_r(b, tctx, &r),
		"ServerPasswordSet (2) failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "ServerPasswordSet (2) failed");

	if (!netlogon_creds_client_check(creds, &r.out.return_authenticator->cred)) {
		torture_comment(tctx, "Credential chaining failed\n");
	}

	cli_credentials_set_password(machine_credentials, password, CRED_SPECIFIED);

	torture_assert(tctx, 
		test_SetupCredentials(p, tctx, machine_credentials, &creds), 
		"ServerPasswordSet failed to actually change the password");

	return true;
}

/*
  try a change password for our machine account
*/
static bool test_SetPassword_flags(struct torture_context *tctx,
				   struct dcerpc_pipe *p,
				   struct cli_credentials *machine_credentials,
				   uint32_t negotiate_flags)
{
	struct netr_ServerPasswordSet r;
	const char *password;
	struct netlogon_creds_CredentialState *creds;
	struct netr_Authenticator credential, return_authenticator;
	struct samr_Password new_password;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_SetupCredentials2(p, tctx, negotiate_flags,
				    machine_credentials,
				    cli_credentials_get_secure_channel_type(machine_credentials),
				    &creds)) {
		return false;
	}

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.account_name = talloc_asprintf(tctx, "%s$", TEST_MACHINE_NAME);
	r.in.secure_channel_type = cli_credentials_get_secure_channel_type(machine_credentials);
	r.in.computer_name = TEST_MACHINE_NAME;
	r.in.credential = &credential;
	r.in.new_password = &new_password;
	r.out.return_authenticator = &return_authenticator;

	password = generate_random_password(tctx, 8, 255);
	E_md4hash(password, new_password.hash);

	netlogon_creds_des_encrypt(creds, &new_password);

	torture_comment(tctx, "Testing ServerPasswordSet on machine account\n");
	torture_comment(tctx, "Changing machine account password to '%s'\n",
			password);

	netlogon_creds_client_authenticator(creds, &credential);

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerPasswordSet_r(b, tctx, &r),
		"ServerPasswordSet failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "ServerPasswordSet failed");

	if (!netlogon_creds_client_check(creds, &r.out.return_authenticator->cred)) {
		torture_comment(tctx, "Credential chaining failed\n");
	}

	/* by changing the machine password twice we test the
	   credentials chaining fully, and we verify that the server
	   allows the password to be set to the same value twice in a
	   row (match win2k3) */
	torture_comment(tctx,
		"Testing a second ServerPasswordSet on machine account\n");
	torture_comment(tctx,
		"Changing machine account password to '%s' (same as previous run)\n", password);

	netlogon_creds_client_authenticator(creds, &credential);

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerPasswordSet_r(b, tctx, &r),
		"ServerPasswordSet (2) failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "ServerPasswordSet (2) failed");

	if (!netlogon_creds_client_check(creds, &r.out.return_authenticator->cred)) {
		torture_comment(tctx, "Credential chaining failed\n");
	}

	cli_credentials_set_password(machine_credentials, password, CRED_SPECIFIED);

	torture_assert(tctx,
		test_SetupCredentials(p, tctx, machine_credentials, &creds),
		"ServerPasswordSet failed to actually change the password");

	return true;
}


/*
  generate a random password for password change tests
*/
static DATA_BLOB netlogon_very_rand_pass(TALLOC_CTX *mem_ctx, int len)
{
	int i;
	DATA_BLOB password = data_blob_talloc(mem_ctx, NULL, len * 2 /* number of unicode chars */);
	generate_random_buffer(password.data, password.length);

	for (i=0; i < len; i++) {
		if (((uint16_t *)password.data)[i] == 0) {
			((uint16_t *)password.data)[i] = 1;
		}
	}

	return password;
}

/*
  try a change password for our machine account
*/
static bool test_SetPassword2(struct torture_context *tctx, 
			      struct dcerpc_pipe *p, 
			      struct cli_credentials *machine_credentials)
{
	struct netr_ServerPasswordSet2 r;
	const char *password;
	DATA_BLOB new_random_pass;
	struct netlogon_creds_CredentialState *creds;
	struct samr_CryptPassword password_buf;
	struct samr_Password nt_hash;
	struct netr_Authenticator credential, return_authenticator;
	struct netr_CryptPassword new_password;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_SetupCredentials(p, tctx, machine_credentials, &creds)) {
		return false;
	}

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.account_name = talloc_asprintf(tctx, "%s$", TEST_MACHINE_NAME);
	r.in.secure_channel_type = cli_credentials_get_secure_channel_type(machine_credentials);
	r.in.computer_name = TEST_MACHINE_NAME;
	r.in.credential = &credential;
	r.in.new_password = &new_password;
	r.out.return_authenticator = &return_authenticator;

	password = generate_random_password(tctx, 8, 255);
	encode_pw_buffer(password_buf.data, password, STR_UNICODE);
	netlogon_creds_arcfour_crypt(creds, password_buf.data, 516);

	memcpy(new_password.data, password_buf.data, 512);
	new_password.length = IVAL(password_buf.data, 512);

	torture_comment(tctx, "Testing ServerPasswordSet2 on machine account\n");
	torture_comment(tctx, "Changing machine account password to '%s'\n", password);

	netlogon_creds_client_authenticator(creds, &credential);

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerPasswordSet2_r(b, tctx, &r),
		"ServerPasswordSet2 failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "ServerPasswordSet2 failed");

	if (!netlogon_creds_client_check(creds, &r.out.return_authenticator->cred)) {
		torture_comment(tctx, "Credential chaining failed\n");
	}

	cli_credentials_set_password(machine_credentials, password, CRED_SPECIFIED);

	if (!torture_setting_bool(tctx, "dangerous", false)) {
		torture_comment(tctx, 
			"Not testing ability to set password to '', enable dangerous tests to perform this test\n");
	} else {
		/* by changing the machine password to ""
		 * we check if the server uses password restrictions
		 * for ServerPasswordSet2
		 * (win2k3 accepts "")
		 */
		password = "";
		encode_pw_buffer(password_buf.data, password, STR_UNICODE);
		netlogon_creds_arcfour_crypt(creds, password_buf.data, 516);
		
		memcpy(new_password.data, password_buf.data, 512);
		new_password.length = IVAL(password_buf.data, 512);
		
		torture_comment(tctx, 
			"Testing ServerPasswordSet2 on machine account\n");
		torture_comment(tctx, 
			"Changing machine account password to '%s'\n", password);
		
		netlogon_creds_client_authenticator(creds, &credential);
		
		torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerPasswordSet2_r(b, tctx, &r),
			"ServerPasswordSet2 failed");
		torture_assert_ntstatus_ok(tctx, r.out.result, "ServerPasswordSet2 failed");
		
		if (!netlogon_creds_client_check(creds, &r.out.return_authenticator->cred)) {
			torture_comment(tctx, "Credential chaining failed\n");
		}
		
		cli_credentials_set_password(machine_credentials, password, CRED_SPECIFIED);
	}

	torture_assert(tctx, test_SetupCredentials(p, tctx, machine_credentials, &creds), 
		"ServerPasswordSet failed to actually change the password");

	/* now try a random password */
	password = generate_random_password(tctx, 8, 255);
	encode_pw_buffer(password_buf.data, password, STR_UNICODE);
	netlogon_creds_arcfour_crypt(creds, password_buf.data, 516);

	memcpy(new_password.data, password_buf.data, 512);
	new_password.length = IVAL(password_buf.data, 512);

	torture_comment(tctx, "Testing second ServerPasswordSet2 on machine account\n");
	torture_comment(tctx, "Changing machine account password to '%s'\n", password);

	netlogon_creds_client_authenticator(creds, &credential);

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerPasswordSet2_r(b, tctx, &r),
		"ServerPasswordSet2 (2) failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "ServerPasswordSet2 (2) failed");

	if (!netlogon_creds_client_check(creds, &r.out.return_authenticator->cred)) {
		torture_comment(tctx, "Credential chaining failed\n");
	}

	/* by changing the machine password twice we test the
	   credentials chaining fully, and we verify that the server
	   allows the password to be set to the same value twice in a
	   row (match win2k3) */
	torture_comment(tctx, 
		"Testing a second ServerPasswordSet2 on machine account\n");
	torture_comment(tctx, 
		"Changing machine account password to '%s' (same as previous run)\n", password);

	netlogon_creds_client_authenticator(creds, &credential);

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerPasswordSet2_r(b, tctx, &r),
		"ServerPasswordSet (3) failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "ServerPasswordSet (3) failed");

	if (!netlogon_creds_client_check(creds, &r.out.return_authenticator->cred)) {
		torture_comment(tctx, "Credential chaining failed\n");
	}

	cli_credentials_set_password(machine_credentials, password, CRED_SPECIFIED);

	torture_assert (tctx, 
		test_SetupCredentials(p, tctx, machine_credentials, &creds), 
		"ServerPasswordSet failed to actually change the password");

	new_random_pass = netlogon_very_rand_pass(tctx, 128);

	/* now try a random stream of bytes for a password */
	set_pw_in_buffer(password_buf.data, &new_random_pass);

	netlogon_creds_arcfour_crypt(creds, password_buf.data, 516);

	memcpy(new_password.data, password_buf.data, 512);
	new_password.length = IVAL(password_buf.data, 512);

	torture_comment(tctx, 
		"Testing a third ServerPasswordSet2 on machine account, with a completely random password\n");

	netlogon_creds_client_authenticator(creds, &credential);

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerPasswordSet2_r(b, tctx, &r),
		"ServerPasswordSet (3) failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "ServerPasswordSet (3) failed");

	if (!netlogon_creds_client_check(creds, &r.out.return_authenticator->cred)) {
		torture_comment(tctx, "Credential chaining failed\n");
	}

	mdfour(nt_hash.hash, new_random_pass.data, new_random_pass.length);

	cli_credentials_set_password(machine_credentials, NULL, CRED_UNINITIALISED);
	cli_credentials_set_nt_hash(machine_credentials, &nt_hash, CRED_SPECIFIED);

	torture_assert (tctx, 
		test_SetupCredentials(p, tctx, machine_credentials, &creds), 
		"ServerPasswordSet failed to actually change the password");

	return true;
}

static bool test_GetPassword(struct torture_context *tctx,
			     struct dcerpc_pipe *p,
			     struct cli_credentials *machine_credentials)
{
	struct netr_ServerPasswordGet r;
	struct netlogon_creds_CredentialState *creds;
	struct netr_Authenticator credential;
	NTSTATUS status;
	struct netr_Authenticator return_authenticator;
	struct samr_Password password;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_SetupCredentials(p, tctx, machine_credentials, &creds)) {
		return false;
	}

	netlogon_creds_client_authenticator(creds, &credential);

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.account_name = talloc_asprintf(tctx, "%s$", TEST_MACHINE_NAME);
	r.in.secure_channel_type = cli_credentials_get_secure_channel_type(machine_credentials);
	r.in.computer_name = TEST_MACHINE_NAME;
	r.in.credential = &credential;
	r.out.return_authenticator = &return_authenticator;
	r.out.password = &password;

	status = dcerpc_netr_ServerPasswordGet_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "ServerPasswordGet");

	return true;
}

static bool test_GetTrustPasswords(struct torture_context *tctx,
				   struct dcerpc_pipe *p,
				   struct cli_credentials *machine_credentials)
{
	struct netr_ServerTrustPasswordsGet r;
	struct netlogon_creds_CredentialState *creds;
	struct netr_Authenticator credential;
	struct netr_Authenticator return_authenticator;
	struct samr_Password password, password2;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_SetupCredentials(p, tctx, machine_credentials, &creds)) {
		return false;
	}

	netlogon_creds_client_authenticator(creds, &credential);

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.account_name = talloc_asprintf(tctx, "%s$", TEST_MACHINE_NAME);
	r.in.secure_channel_type = cli_credentials_get_secure_channel_type(machine_credentials);
	r.in.computer_name = TEST_MACHINE_NAME;
	r.in.credential = &credential;
	r.out.return_authenticator = &return_authenticator;
	r.out.password = &password;
	r.out.password2 = &password2;

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerTrustPasswordsGet_r(b, tctx, &r),
		"ServerTrustPasswordsGet failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "ServerTrustPasswordsGet failed");

	return true;
}

/*
  try a netlogon SamLogon
*/
static bool test_netlogon_ops_args(struct dcerpc_pipe *p, struct torture_context *tctx,
				   struct cli_credentials *credentials,
				   struct netlogon_creds_CredentialState *creds,
				   bool null_domain)
{
	NTSTATUS status;
	struct netr_LogonSamLogon r;
	struct netr_Authenticator auth, auth2;
	static const struct netr_Authenticator auth_zero;
	union netr_LogonLevel logon;
	union netr_Validation validation;
	uint8_t authoritative;
	struct netr_NetworkInfo ninfo;
	DATA_BLOB names_blob, chal, lm_resp, nt_resp;
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;
	int flags = CLI_CRED_NTLM_AUTH;
	if (lpcfg_client_lanman_auth(tctx->lp_ctx)) {
		flags |= CLI_CRED_LANMAN_AUTH;
	}

	if (lpcfg_client_ntlmv2_auth(tctx->lp_ctx) && !null_domain) {
		flags |= CLI_CRED_NTLMv2_AUTH;
	}

	cli_credentials_get_ntlm_username_domain(cmdline_credentials, tctx, 
						 &ninfo.identity_info.account_name.string,
						 &ninfo.identity_info.domain_name.string);

	if (null_domain) {
		ninfo.identity_info.domain_name.string = NULL;
	}

	generate_random_buffer(ninfo.challenge, 
			       sizeof(ninfo.challenge));
	chal = data_blob_const(ninfo.challenge, 
			       sizeof(ninfo.challenge));

	names_blob = NTLMv2_generate_names_blob(tctx, cli_credentials_get_workstation(credentials), 
						cli_credentials_get_domain(credentials));

	status = cli_credentials_get_ntlm_response(cmdline_credentials, tctx, 
						   &flags, 
						   chal,
						   names_blob,
						   &lm_resp, &nt_resp,
						   NULL, NULL);
	torture_assert_ntstatus_ok(tctx, status, "cli_credentials_get_ntlm_response failed");

	ninfo.lm.data = lm_resp.data;
	ninfo.lm.length = lm_resp.length;

	ninfo.nt.data = nt_resp.data;
	ninfo.nt.length = nt_resp.length;

	ninfo.identity_info.parameter_control = 0;
	ninfo.identity_info.logon_id_low = 0;
	ninfo.identity_info.logon_id_high = 0;
	ninfo.identity_info.workstation.string = cli_credentials_get_workstation(credentials);

	logon.network = &ninfo;

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computer_name = cli_credentials_get_workstation(credentials);
	r.in.credential = &auth;
	r.in.return_authenticator = &auth2;
	r.in.logon_level = 2;
	r.in.logon = &logon;
	r.out.validation = &validation;
	r.out.authoritative = &authoritative;

	d_printf("Testing LogonSamLogon with name %s\n", ninfo.identity_info.account_name.string);
	
	for (i=2;i<=3;i++) {
		ZERO_STRUCT(auth2);
		netlogon_creds_client_authenticator(creds, &auth);
		
		r.in.validation_level = i;
		
		torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonSamLogon_r(b, tctx, &r),
			"LogonSamLogon failed");
		torture_assert_ntstatus_ok(tctx, r.out.result, "LogonSamLogon failed");
		
		torture_assert(tctx, netlogon_creds_client_check(creds, 
								 &r.out.return_authenticator->cred), 
			"Credential chaining failed");
		torture_assert_int_equal(tctx, *r.out.authoritative, 1,
					 "LogonSamLogon invalid  *r.out.authoritative");
	}

	/* this makes sure we get the unmarshalling right for invalid levels */
	for (i=52;i<53;i++) {
		ZERO_STRUCT(auth2);
		/* the authenticator should be ignored by the server */
		generate_random_buffer((uint8_t *) &auth, sizeof(auth));

		r.in.validation_level = i;

		torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonSamLogon_r(b, tctx, &r),
					   "LogonSamLogon failed");
		torture_assert_ntstatus_equal(tctx, r.out.result,
					      NT_STATUS_INVALID_INFO_CLASS,
					      "LogonSamLogon failed");

		torture_assert_int_equal(tctx, *r.out.authoritative, 1,
					 "LogonSamLogon invalid  *r.out.authoritative");
		torture_assert(tctx,
			       memcmp(&auth2, &auth_zero, sizeof(auth2)) == 0,
			       "Return authenticator non zero");
	}

	for (i=2;i<=3;i++) {
		ZERO_STRUCT(auth2);
		netlogon_creds_client_authenticator(creds, &auth);

		r.in.validation_level = i;

		torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonSamLogon_r(b, tctx, &r),
			"LogonSamLogon failed");
		torture_assert_ntstatus_ok(tctx, r.out.result, "LogonSamLogon failed");

		torture_assert(tctx, netlogon_creds_client_check(creds,
								 &r.out.return_authenticator->cred),
			"Credential chaining failed");
		torture_assert_int_equal(tctx, *r.out.authoritative, 1,
					 "LogonSamLogon invalid  *r.out.authoritative");
	}

	r.in.logon_level = 52;

	for (i=2;i<=3;i++) {
		ZERO_STRUCT(auth2);
		/* the authenticator should be ignored by the server */
		generate_random_buffer((uint8_t *) &auth, sizeof(auth));

		r.in.validation_level = i;

		torture_comment(tctx, "Testing SamLogon with validation level %d and a NULL credential\n", i);

		torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonSamLogon_r(b, tctx, &r),
			"LogonSamLogon failed");
		torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_INVALID_PARAMETER,
			"LogonSamLogon expected INVALID_PARAMETER");

		torture_assert(tctx,
			       memcmp(&auth2, &auth_zero, sizeof(auth2)) == 0,
			       "Return authenticator non zero");
		torture_assert_int_equal(tctx, *r.out.authoritative, 1,
					 "LogonSamLogon invalid  *r.out.authoritative");
	}

	r.in.credential = NULL;

	for (i=2;i<=3;i++) {
		ZERO_STRUCT(auth2);

		r.in.validation_level = i;

		torture_comment(tctx, "Testing SamLogon with validation level %d and a NULL credential\n", i);

		torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonSamLogon_r(b, tctx, &r),
			"LogonSamLogon failed");
		torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_INVALID_PARAMETER,
			"LogonSamLogon expected INVALID_PARAMETER");

		torture_assert(tctx,
			       memcmp(&auth2, &auth_zero, sizeof(auth2)) == 0,
			       "Return authenticator non zero");
		torture_assert_int_equal(tctx, *r.out.authoritative, 1,
					 "LogonSamLogon invalid  *r.out.authoritative");
	}

	r.in.logon_level = 2;
	r.in.credential = &auth;

	for (i=2;i<=3;i++) {
		ZERO_STRUCT(auth2);
		netlogon_creds_client_authenticator(creds, &auth);

		r.in.validation_level = i;

		torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonSamLogon_r(b, tctx, &r),
			"LogonSamLogon failed");
		torture_assert_ntstatus_ok(tctx, r.out.result, "LogonSamLogon failed");

		torture_assert(tctx, netlogon_creds_client_check(creds,
								 &r.out.return_authenticator->cred),
			"Credential chaining failed");
		torture_assert_int_equal(tctx, *r.out.authoritative, 1,
					 "LogonSamLogon invalid  *r.out.authoritative");
	}

	return true;
}

bool test_netlogon_ops(struct dcerpc_pipe *p, struct torture_context *tctx,
		       struct cli_credentials *credentials,
		       struct netlogon_creds_CredentialState *creds)
{
	return test_netlogon_ops_args(p, tctx, credentials, creds, false);
}

/*
  try a netlogon SamLogon
*/
static bool test_SamLogon(struct torture_context *tctx, 
			  struct dcerpc_pipe *p,
			  struct cli_credentials *credentials)
{
	struct netlogon_creds_CredentialState *creds;

	if (!test_SetupCredentials(p, tctx, credentials, &creds)) {
		return false;
	}

	return test_netlogon_ops(p, tctx, credentials, creds);
}

static bool test_SamLogon_NULL_domain(struct torture_context *tctx,
				      struct dcerpc_pipe *p,
				      struct cli_credentials *credentials)
{
	struct netlogon_creds_CredentialState *creds;

	if (!test_SetupCredentials(p, tctx, credentials, &creds)) {
		return false;
	}

	return test_netlogon_ops_args(p, tctx, credentials, creds, true);
}

/* we remember the sequence numbers so we can easily do a DatabaseDelta */
static uint64_t sequence_nums[3];

/*
  try a netlogon DatabaseSync
*/
static bool test_DatabaseSync(struct torture_context *tctx, 
			      struct dcerpc_pipe *p,
			      struct cli_credentials *machine_credentials)
{
	struct netr_DatabaseSync r;
	struct netlogon_creds_CredentialState *creds;
	const uint32_t database_ids[] = {SAM_DATABASE_DOMAIN, SAM_DATABASE_BUILTIN, SAM_DATABASE_PRIVS}; 
	int i;
	struct netr_DELTA_ENUM_ARRAY *delta_enum_array = NULL;
	struct netr_Authenticator credential, return_authenticator;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_SetupCredentials(p, tctx, machine_credentials, &creds)) {
		return false;
	}

	ZERO_STRUCT(return_authenticator);

	r.in.logon_server = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computername = TEST_MACHINE_NAME;
	r.in.preferredmaximumlength = (uint32_t)-1;
	r.in.return_authenticator = &return_authenticator;
	r.out.delta_enum_array = &delta_enum_array;
	r.out.return_authenticator = &return_authenticator;

	for (i=0;i<ARRAY_SIZE(database_ids);i++) {

		uint32_t sync_context = 0;

		r.in.database_id = database_ids[i];
		r.in.sync_context = &sync_context;
		r.out.sync_context = &sync_context;

		torture_comment(tctx, "Testing DatabaseSync of id %d\n", r.in.database_id);

		do {
			netlogon_creds_client_authenticator(creds, &credential);

			r.in.credential = &credential;

			torture_assert_ntstatus_ok(tctx, dcerpc_netr_DatabaseSync_r(b, tctx, &r),
				"DatabaseSync failed");
			if (NT_STATUS_EQUAL(r.out.result, STATUS_MORE_ENTRIES))
			    break;

			/* Native mode servers don't do this */
			if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_NOT_SUPPORTED)) {
				return true;
			}
			torture_assert_ntstatus_ok(tctx, r.out.result, "DatabaseSync");

			if (!netlogon_creds_client_check(creds, &r.out.return_authenticator->cred)) {
				torture_comment(tctx, "Credential chaining failed\n");
			}

			if (delta_enum_array &&
			    delta_enum_array->num_deltas > 0 &&
			    delta_enum_array->delta_enum[0].delta_type == NETR_DELTA_DOMAIN &&
			    delta_enum_array->delta_enum[0].delta_union.domain) {
				sequence_nums[r.in.database_id] = 
					delta_enum_array->delta_enum[0].delta_union.domain->sequence_num;
				torture_comment(tctx, "\tsequence_nums[%d]=%llu\n",
				       r.in.database_id, 
				       (unsigned long long)sequence_nums[r.in.database_id]);
			}
		} while (NT_STATUS_EQUAL(r.out.result, STATUS_MORE_ENTRIES));
	}

	return true;
}


/*
  try a netlogon DatabaseDeltas
*/
static bool test_DatabaseDeltas(struct torture_context *tctx, 
				struct dcerpc_pipe *p,
				struct cli_credentials *machine_credentials)
{
	struct netr_DatabaseDeltas r;
	struct netlogon_creds_CredentialState *creds;
	struct netr_Authenticator credential;
	struct netr_Authenticator return_authenticator;
	struct netr_DELTA_ENUM_ARRAY *delta_enum_array = NULL;
	const uint32_t database_ids[] = {0, 1, 2}; 
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_SetupCredentials(p, tctx, machine_credentials, &creds)) {
		return false;
	}

	r.in.logon_server = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computername = TEST_MACHINE_NAME;
	r.in.preferredmaximumlength = (uint32_t)-1;
	ZERO_STRUCT(r.in.return_authenticator);
	r.out.return_authenticator = &return_authenticator;
	r.out.delta_enum_array = &delta_enum_array;

	for (i=0;i<ARRAY_SIZE(database_ids);i++) {
		r.in.database_id = database_ids[i];
		r.in.sequence_num = &sequence_nums[r.in.database_id];

		if (*r.in.sequence_num == 0) continue;

		*r.in.sequence_num -= 1;

		torture_comment(tctx, "Testing DatabaseDeltas of id %d at %llu\n", 
		       r.in.database_id, (unsigned long long)*r.in.sequence_num);

		do {
			netlogon_creds_client_authenticator(creds, &credential);

			torture_assert_ntstatus_ok(tctx, dcerpc_netr_DatabaseDeltas_r(b, tctx, &r),
				"DatabaseDeltas failed");
			if (NT_STATUS_EQUAL(r.out.result,
					     NT_STATUS_SYNCHRONIZATION_REQUIRED)) {
				torture_comment(tctx, "not considering %s to be an error\n",
				       nt_errstr(r.out.result));
				return true;
			}
			if (NT_STATUS_EQUAL(r.out.result, STATUS_MORE_ENTRIES))
			    break;

			torture_assert_ntstatus_ok(tctx, r.out.result, "DatabaseDeltas");

			if (!netlogon_creds_client_check(creds, &return_authenticator.cred)) {
				torture_comment(tctx, "Credential chaining failed\n");
			}

			(*r.in.sequence_num)++;
		} while (NT_STATUS_EQUAL(r.out.result, STATUS_MORE_ENTRIES));
	}

	return true;
}

static bool test_DatabaseRedo(struct torture_context *tctx,
			      struct dcerpc_pipe *p,
			      struct cli_credentials *machine_credentials)
{
	struct netr_DatabaseRedo r;
	struct netlogon_creds_CredentialState *creds;
	struct netr_Authenticator credential;
	struct netr_Authenticator return_authenticator;
	struct netr_DELTA_ENUM_ARRAY *delta_enum_array = NULL;
	struct netr_ChangeLogEntry e;
	struct dom_sid null_sid, *sid;
	int i,d;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(null_sid);

	sid = dom_sid_parse_talloc(tctx, "S-1-5-21-1111111111-2222222222-333333333-500");

	{

	struct {
		uint32_t rid;
		uint16_t flags;
		uint8_t db_index;
		uint8_t delta_type;
		struct dom_sid sid;
		const char *name;
		NTSTATUS expected_error;
		uint32_t expected_num_results;
		uint8_t expected_delta_type_1;
		uint8_t expected_delta_type_2;
		const char *comment;
	} changes[] = {

		/* SAM_DATABASE_DOMAIN */

		{
			.rid			= 0,
			.flags			= 0,
			.db_index		= SAM_DATABASE_DOMAIN,
			.delta_type		= NETR_DELTA_MODIFY_COUNT,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_SYNCHRONIZATION_REQUIRED,
			.expected_num_results   = 0,
			.comment		= "NETR_DELTA_MODIFY_COUNT"
		},
		{
			.rid			= 0,
			.flags			= 0,
			.db_index		= SAM_DATABASE_DOMAIN,
			.delta_type		= 0,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results	= 1,
			.expected_delta_type_1	= NETR_DELTA_DOMAIN,
			.comment		= "NULL DELTA"
		},
		{
			.rid			= 0,
			.flags			= 0,
			.db_index		= SAM_DATABASE_DOMAIN,
			.delta_type		= NETR_DELTA_DOMAIN,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results	= 1,
			.expected_delta_type_1	= NETR_DELTA_DOMAIN,
			.comment		= "NETR_DELTA_DOMAIN"
		},
		{
			.rid			= DOMAIN_RID_ADMINISTRATOR,
			.flags			= 0,
			.db_index		= SAM_DATABASE_DOMAIN,
			.delta_type		= NETR_DELTA_USER,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_USER,
			.comment		= "NETR_DELTA_USER by rid 500"
		},
		{
			.rid			= DOMAIN_RID_GUEST,
			.flags			= 0,
			.db_index		= SAM_DATABASE_DOMAIN,
			.delta_type		= NETR_DELTA_USER,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_USER,
			.comment		= "NETR_DELTA_USER by rid 501"
		},
		{
			.rid			= 0,
			.flags			= NETR_CHANGELOG_SID_INCLUDED,
			.db_index		= SAM_DATABASE_DOMAIN,
			.delta_type		= NETR_DELTA_USER,
			.sid			= *sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_DELETE_USER,
			.comment		= "NETR_DELTA_USER by sid and flags"
		},
		{
			.rid			= 0,
			.flags			= NETR_CHANGELOG_SID_INCLUDED,
			.db_index		= SAM_DATABASE_DOMAIN,
			.delta_type		= NETR_DELTA_USER,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_DELETE_USER,
			.comment		= "NETR_DELTA_USER by null_sid and flags"
		},
		{
			.rid			= 0,
			.flags			= NETR_CHANGELOG_NAME_INCLUDED,
			.db_index		= SAM_DATABASE_DOMAIN,
			.delta_type		= NETR_DELTA_USER,
			.sid			= null_sid,
			.name			= "administrator",
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_DELETE_USER,
			.comment		= "NETR_DELTA_USER by name 'administrator'"
		},
		{
			.rid			= DOMAIN_RID_ADMINS,
			.flags			= 0,
			.db_index		= SAM_DATABASE_DOMAIN,
			.delta_type		= NETR_DELTA_GROUP,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 2,
			.expected_delta_type_1	= NETR_DELTA_GROUP,
			.expected_delta_type_2	= NETR_DELTA_GROUP_MEMBER,
			.comment		= "NETR_DELTA_GROUP by rid 512"
		},
		{
			.rid			= DOMAIN_RID_ADMINS,
			.flags			= 0,
			.db_index		= SAM_DATABASE_DOMAIN,
			.delta_type		= NETR_DELTA_GROUP_MEMBER,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 2,
			.expected_delta_type_1	= NETR_DELTA_GROUP,
			.expected_delta_type_2	= NETR_DELTA_GROUP_MEMBER,
			.comment		= "NETR_DELTA_GROUP_MEMBER by rid 512"
		},


		/* SAM_DATABASE_BUILTIN */

		{
			.rid			= 0,
			.flags			= 0,
			.db_index		= SAM_DATABASE_BUILTIN,
			.delta_type		= NETR_DELTA_MODIFY_COUNT,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_SYNCHRONIZATION_REQUIRED,
			.expected_num_results   = 0,
			.comment		= "NETR_DELTA_MODIFY_COUNT"
		},
		{
			.rid			= 0,
			.flags			= 0,
			.db_index		= SAM_DATABASE_BUILTIN,
			.delta_type		= NETR_DELTA_DOMAIN,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_DOMAIN,
			.comment		= "NETR_DELTA_DOMAIN"
		},
		{
			.rid			= DOMAIN_RID_ADMINISTRATOR,
			.flags			= 0,
			.db_index		= SAM_DATABASE_BUILTIN,
			.delta_type		= NETR_DELTA_USER,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_DELETE_USER,
			.comment		= "NETR_DELTA_USER by rid 500"
		},
		{
			.rid			= 0,
			.flags			= 0,
			.db_index		= SAM_DATABASE_BUILTIN,
			.delta_type		= NETR_DELTA_USER,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_DELETE_USER,
			.comment		= "NETR_DELTA_USER"
		},
		{
			.rid			= 544,
			.flags			= 0,
			.db_index		= SAM_DATABASE_BUILTIN,
			.delta_type		= NETR_DELTA_ALIAS,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 2,
			.expected_delta_type_1	= NETR_DELTA_ALIAS,
			.expected_delta_type_2	= NETR_DELTA_ALIAS_MEMBER,
			.comment		= "NETR_DELTA_ALIAS by rid 544"
		},
		{
			.rid			= 544,
			.flags			= 0,
			.db_index		= SAM_DATABASE_BUILTIN,
			.delta_type		= NETR_DELTA_ALIAS_MEMBER,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 2,
			.expected_delta_type_1	= NETR_DELTA_ALIAS,
			.expected_delta_type_2	= NETR_DELTA_ALIAS_MEMBER,
			.comment		= "NETR_DELTA_ALIAS_MEMBER by rid 544"
		},
		{
			.rid			= 544,
			.flags			= 0,
			.db_index		= SAM_DATABASE_BUILTIN,
			.delta_type		= 0,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_DOMAIN,
			.comment		= "NULL DELTA by rid 544"
		},
		{
			.rid			= 544,
			.flags			= NETR_CHANGELOG_SID_INCLUDED,
			.db_index		= SAM_DATABASE_BUILTIN,
			.delta_type		= 0,
			.sid			= *dom_sid_parse_talloc(tctx, "S-1-5-32-544"),
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_DOMAIN,
			.comment		= "NULL DELTA by rid 544 sid S-1-5-32-544 and flags"
		},
		{
			.rid			= 544,
			.flags			= NETR_CHANGELOG_SID_INCLUDED,
			.db_index		= SAM_DATABASE_BUILTIN,
			.delta_type		= NETR_DELTA_ALIAS,
			.sid			= *dom_sid_parse_talloc(tctx, "S-1-5-32-544"),
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 2,
			.expected_delta_type_1	= NETR_DELTA_ALIAS,
			.expected_delta_type_2	= NETR_DELTA_ALIAS_MEMBER,
			.comment		= "NETR_DELTA_ALIAS by rid 544 and sid S-1-5-32-544 and flags"
		},
		{
			.rid			= 0,
			.flags			= NETR_CHANGELOG_SID_INCLUDED,
			.db_index		= SAM_DATABASE_BUILTIN,
			.delta_type		= NETR_DELTA_ALIAS,
			.sid			= *dom_sid_parse_talloc(tctx, "S-1-5-32-544"),
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_DELETE_ALIAS,
			.comment		= "NETR_DELTA_ALIAS by sid S-1-5-32-544 and flags"
		},

		/* SAM_DATABASE_PRIVS */

		{
			.rid			= 0,
			.flags			= 0,
			.db_index		= SAM_DATABASE_PRIVS,
			.delta_type		= 0,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_ACCESS_DENIED,
			.expected_num_results   = 0,
			.comment		= "NULL DELTA"
		},
		{
			.rid			= 0,
			.flags			= 0,
			.db_index		= SAM_DATABASE_PRIVS,
			.delta_type		= NETR_DELTA_MODIFY_COUNT,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_SYNCHRONIZATION_REQUIRED,
			.expected_num_results   = 0,
			.comment		= "NETR_DELTA_MODIFY_COUNT"
		},
		{
			.rid			= 0,
			.flags			= 0,
			.db_index		= SAM_DATABASE_PRIVS,
			.delta_type		= NETR_DELTA_POLICY,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_POLICY,
			.comment		= "NETR_DELTA_POLICY"
		},
		{
			.rid			= 0,
			.flags			= NETR_CHANGELOG_SID_INCLUDED,
			.db_index		= SAM_DATABASE_PRIVS,
			.delta_type		= NETR_DELTA_POLICY,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_POLICY,
			.comment		= "NETR_DELTA_POLICY by null sid and flags"
		},
		{
			.rid			= 0,
			.flags			= NETR_CHANGELOG_SID_INCLUDED,
			.db_index		= SAM_DATABASE_PRIVS,
			.delta_type		= NETR_DELTA_POLICY,
			.sid			= *dom_sid_parse_talloc(tctx, "S-1-5-32"),
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_POLICY,
			.comment		= "NETR_DELTA_POLICY by sid S-1-5-32 and flags"
		},
		{
			.rid			= DOMAIN_RID_ADMINISTRATOR,
			.flags			= 0,
			.db_index		= SAM_DATABASE_PRIVS,
			.delta_type		= NETR_DELTA_ACCOUNT,
			.sid			= null_sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_SYNCHRONIZATION_REQUIRED, /* strange */
			.expected_num_results   = 0,
			.comment		= "NETR_DELTA_ACCOUNT by rid 500"
		},
		{
			.rid			= 0,
			.flags			= NETR_CHANGELOG_SID_INCLUDED,
			.db_index		= SAM_DATABASE_PRIVS,
			.delta_type		= NETR_DELTA_ACCOUNT,
			.sid			= *dom_sid_parse_talloc(tctx, "S-1-1-0"),
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_ACCOUNT,
			.comment		= "NETR_DELTA_ACCOUNT by sid S-1-1-0 and flags"
		},
		{
			.rid			= 0,
			.flags			= NETR_CHANGELOG_SID_INCLUDED |
						  NETR_CHANGELOG_IMMEDIATE_REPL_REQUIRED,
			.db_index		= SAM_DATABASE_PRIVS,
			.delta_type		= NETR_DELTA_ACCOUNT,
			.sid			= *dom_sid_parse_talloc(tctx, "S-1-1-0"),
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_ACCOUNT,
			.comment		= "NETR_DELTA_ACCOUNT by sid S-1-1-0 and 2 flags"
		},
		{
			.rid			= 0,
			.flags			= NETR_CHANGELOG_SID_INCLUDED |
						  NETR_CHANGELOG_NAME_INCLUDED,
			.db_index		= SAM_DATABASE_PRIVS,
			.delta_type		= NETR_DELTA_ACCOUNT,
			.sid			= *dom_sid_parse_talloc(tctx, "S-1-1-0"),
			.name			= NULL,
			.expected_error		= NT_STATUS_INVALID_PARAMETER,
			.expected_num_results   = 0,
			.comment		= "NETR_DELTA_ACCOUNT by sid S-1-1-0 and invalid flags"
		},
		{
			.rid			= DOMAIN_RID_ADMINISTRATOR,
			.flags			= NETR_CHANGELOG_SID_INCLUDED,
			.db_index		= SAM_DATABASE_PRIVS,
			.delta_type		= NETR_DELTA_ACCOUNT,
			.sid			= *sid,
			.name			= NULL,
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_DELETE_ACCOUNT,
			.comment		= "NETR_DELTA_ACCOUNT by rid 500, sid and flags"
		},
		{
			.rid			= 0,
			.flags			= NETR_CHANGELOG_NAME_INCLUDED,
			.db_index		= SAM_DATABASE_PRIVS,
			.delta_type		= NETR_DELTA_SECRET,
			.sid			= null_sid,
			.name			= "IsurelydontexistIhope",
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_DELETE_SECRET,
			.comment		= "NETR_DELTA_SECRET by name 'IsurelydontexistIhope' and flags"
		},
		{
			.rid			= 0,
			.flags			= NETR_CHANGELOG_NAME_INCLUDED,
			.db_index		= SAM_DATABASE_PRIVS,
			.delta_type		= NETR_DELTA_SECRET,
			.sid			= null_sid,
			.name			= "G$BCKUPKEY_P",
			.expected_error		= NT_STATUS_OK,
			.expected_num_results   = 1,
			.expected_delta_type_1	= NETR_DELTA_SECRET,
			.comment		= "NETR_DELTA_SECRET by name 'G$BCKUPKEY_P' and flags"
		}
	};

	ZERO_STRUCT(return_authenticator);

	r.in.logon_server = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computername = TEST_MACHINE_NAME;
	r.in.return_authenticator = &return_authenticator;
	r.out.return_authenticator = &return_authenticator;
	r.out.delta_enum_array = &delta_enum_array;

	for (d=0; d<3; d++) {
		const char *database = NULL;

		switch (d) {
		case 0:
			database = "SAM";
			break;
		case 1:
			database = "BUILTIN";
			break;
		case 2:
			database = "LSA";
			break;
		default:
			break;
		}

		torture_comment(tctx, "Testing DatabaseRedo\n");

		if (!test_SetupCredentials(p, tctx, machine_credentials, &creds)) {
			return false;
		}

		for (i=0;i<ARRAY_SIZE(changes);i++) {

			if (d != changes[i].db_index) {
				continue;
			}

			netlogon_creds_client_authenticator(creds, &credential);

			r.in.credential = &credential;

			e.serial_number1	= 0;
			e.serial_number2	= 0;
			e.object_rid		= changes[i].rid;
			e.flags			= changes[i].flags;
			e.db_index		= changes[i].db_index;
			e.delta_type		= changes[i].delta_type;

			switch (changes[i].flags & (NETR_CHANGELOG_NAME_INCLUDED | NETR_CHANGELOG_SID_INCLUDED)) {
			case NETR_CHANGELOG_SID_INCLUDED:
				e.object.object_sid		= changes[i].sid;
				break;
			case NETR_CHANGELOG_NAME_INCLUDED:
				e.object.object_name		= changes[i].name;
				break;
			default:
				break;
			}

			r.in.change_log_entry = e;

			torture_comment(tctx, "Testing DatabaseRedo with database %s and %s\n",
				database, changes[i].comment);

			torture_assert_ntstatus_ok(tctx, dcerpc_netr_DatabaseRedo_r(b, tctx, &r),
				"DatabaseRedo failed");
			if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_NOT_SUPPORTED)) {
				return true;
			}

			torture_assert_ntstatus_equal(tctx, r.out.result, changes[i].expected_error, changes[i].comment);
			if (delta_enum_array) {
				torture_assert_int_equal(tctx,
					delta_enum_array->num_deltas,
					changes[i].expected_num_results,
					changes[i].comment);
				if (delta_enum_array->num_deltas > 0) {
					torture_assert_int_equal(tctx,
						delta_enum_array->delta_enum[0].delta_type,
						changes[i].expected_delta_type_1,
						changes[i].comment);
				}
				if (delta_enum_array->num_deltas > 1) {
					torture_assert_int_equal(tctx,
						delta_enum_array->delta_enum[1].delta_type,
						changes[i].expected_delta_type_2,
						changes[i].comment);
				}
			}

			if (!netlogon_creds_client_check(creds, &return_authenticator.cred)) {
				torture_comment(tctx, "Credential chaining failed\n");
				if (!test_SetupCredentials(p, tctx, machine_credentials, &creds)) {
					return false;
				}
			}
		}
	}
	}

	return true;
}

/*
  try a netlogon AccountDeltas
*/
static bool test_AccountDeltas(struct torture_context *tctx, 
			       struct dcerpc_pipe *p,
			       struct cli_credentials *machine_credentials)
{
	struct netr_AccountDeltas r;
	struct netlogon_creds_CredentialState *creds;

	struct netr_AccountBuffer buffer;
	uint32_t count_returned = 0;
	uint32_t total_entries = 0;
	struct netr_UAS_INFO_0 recordid;
	struct netr_Authenticator return_authenticator;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_SetupCredentials(p, tctx, machine_credentials, &creds)) {
		return false;
	}

	ZERO_STRUCT(return_authenticator);

	r.in.logon_server = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computername = TEST_MACHINE_NAME;
	r.in.return_authenticator = &return_authenticator;
	netlogon_creds_client_authenticator(creds, &r.in.credential);
	ZERO_STRUCT(r.in.uas);
	r.in.count=10;
	r.in.level=0;
	r.in.buffersize=100;
	r.out.buffer = &buffer;
	r.out.count_returned = &count_returned;
	r.out.total_entries = &total_entries;
	r.out.recordid = &recordid;
	r.out.return_authenticator = &return_authenticator;

	/* w2k3 returns "NOT IMPLEMENTED" for this call */
	torture_assert_ntstatus_ok(tctx, dcerpc_netr_AccountDeltas_r(b, tctx, &r),
		"AccountDeltas failed");
	torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_NOT_IMPLEMENTED, "AccountDeltas");

	return true;
}

/*
  try a netlogon AccountSync
*/
static bool test_AccountSync(struct torture_context *tctx, struct dcerpc_pipe *p, 
			     struct cli_credentials *machine_credentials)
{
	struct netr_AccountSync r;
	struct netlogon_creds_CredentialState *creds;

	struct netr_AccountBuffer buffer;
	uint32_t count_returned = 0;
	uint32_t total_entries = 0;
	uint32_t next_reference = 0;
	struct netr_UAS_INFO_0 recordid;
	struct netr_Authenticator return_authenticator;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(recordid);
	ZERO_STRUCT(return_authenticator);

	if (!test_SetupCredentials(p, tctx, machine_credentials, &creds)) {
		return false;
	}

	r.in.logon_server = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computername = TEST_MACHINE_NAME;
	r.in.return_authenticator = &return_authenticator;
	netlogon_creds_client_authenticator(creds, &r.in.credential);
	r.in.recordid = &recordid;
	r.in.reference=0;
	r.in.level=0;
	r.in.buffersize=100;
	r.out.buffer = &buffer;
	r.out.count_returned = &count_returned;
	r.out.total_entries = &total_entries;
	r.out.next_reference = &next_reference;
	r.out.recordid = &recordid;
	r.out.return_authenticator = &return_authenticator;

	/* w2k3 returns "NOT IMPLEMENTED" for this call */
	torture_assert_ntstatus_ok(tctx, dcerpc_netr_AccountSync_r(b, tctx, &r),
		"AccountSync failed");
	torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_NOT_IMPLEMENTED, "AccountSync");

	return true;
}

/*
  try a netlogon GetDcName
*/
static bool test_GetDcName(struct torture_context *tctx, 
			   struct dcerpc_pipe *p)
{
	struct netr_GetDcName r;
	const char *dcname = NULL;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.logon_server = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.domainname = lpcfg_workgroup(tctx->lp_ctx);
	r.out.dcname = &dcname;

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_GetDcName_r(b, tctx, &r),
		"GetDcName failed");
	torture_assert_werr_ok(tctx, r.out.result, "GetDcName failed");

	torture_comment(tctx, "\tDC is at '%s'\n", dcname);

	return true;
}

static const char *function_code_str(TALLOC_CTX *mem_ctx,
				     enum netr_LogonControlCode function_code)
{
	switch (function_code) {
	case NETLOGON_CONTROL_QUERY:
		return "NETLOGON_CONTROL_QUERY";
	case NETLOGON_CONTROL_REPLICATE:
		return "NETLOGON_CONTROL_REPLICATE";
	case NETLOGON_CONTROL_SYNCHRONIZE:
		return "NETLOGON_CONTROL_SYNCHRONIZE";
	case NETLOGON_CONTROL_PDC_REPLICATE:
		return "NETLOGON_CONTROL_PDC_REPLICATE";
	case NETLOGON_CONTROL_REDISCOVER:
		return "NETLOGON_CONTROL_REDISCOVER";
	case NETLOGON_CONTROL_TC_QUERY:
		return "NETLOGON_CONTROL_TC_QUERY";
	case NETLOGON_CONTROL_TRANSPORT_NOTIFY:
		return "NETLOGON_CONTROL_TRANSPORT_NOTIFY";
	case NETLOGON_CONTROL_FIND_USER:
		return "NETLOGON_CONTROL_FIND_USER";
	case NETLOGON_CONTROL_CHANGE_PASSWORD:
		return "NETLOGON_CONTROL_CHANGE_PASSWORD";
	case NETLOGON_CONTROL_TC_VERIFY:
		return "NETLOGON_CONTROL_TC_VERIFY";
	case NETLOGON_CONTROL_FORCE_DNS_REG:
		return "NETLOGON_CONTROL_FORCE_DNS_REG";
	case NETLOGON_CONTROL_QUERY_DNS_REG:
		return "NETLOGON_CONTROL_QUERY_DNS_REG";
	case NETLOGON_CONTROL_BACKUP_CHANGE_LOG:
		return "NETLOGON_CONTROL_BACKUP_CHANGE_LOG";
	case NETLOGON_CONTROL_TRUNCATE_LOG:
		return "NETLOGON_CONTROL_TRUNCATE_LOG";
	case NETLOGON_CONTROL_SET_DBFLAG:
		return "NETLOGON_CONTROL_SET_DBFLAG";
	case NETLOGON_CONTROL_BREAKPOINT:
		return "NETLOGON_CONTROL_BREAKPOINT";
	default:
		return talloc_asprintf(mem_ctx, "unknown function code: %d",
				       function_code);
	}
}


/*
  try a netlogon LogonControl 
*/
static bool test_LogonControl(struct torture_context *tctx, 
			      struct dcerpc_pipe *p,
			      struct cli_credentials *machine_credentials)

{
	NTSTATUS status;
	struct netr_LogonControl r;
	union netr_CONTROL_QUERY_INFORMATION query;
	int i,f;
	enum netr_SchannelType secure_channel_type = SEC_CHAN_NULL;
	struct dcerpc_binding_handle *b = p->binding_handle;

	uint32_t function_codes[] = {
		NETLOGON_CONTROL_QUERY,
		NETLOGON_CONTROL_REPLICATE,
		NETLOGON_CONTROL_SYNCHRONIZE,
		NETLOGON_CONTROL_PDC_REPLICATE,
		NETLOGON_CONTROL_REDISCOVER,
		NETLOGON_CONTROL_TC_QUERY,
		NETLOGON_CONTROL_TRANSPORT_NOTIFY,
		NETLOGON_CONTROL_FIND_USER,
		NETLOGON_CONTROL_CHANGE_PASSWORD,
		NETLOGON_CONTROL_TC_VERIFY,
		NETLOGON_CONTROL_FORCE_DNS_REG,
		NETLOGON_CONTROL_QUERY_DNS_REG,
		NETLOGON_CONTROL_BACKUP_CHANGE_LOG,
		NETLOGON_CONTROL_TRUNCATE_LOG,
		NETLOGON_CONTROL_SET_DBFLAG,
		NETLOGON_CONTROL_BREAKPOINT
	};

	if (machine_credentials) {
		secure_channel_type = cli_credentials_get_secure_channel_type(machine_credentials);
	}

	torture_comment(tctx, "Testing LogonControl with secure channel type: %d\n",
		secure_channel_type);

	r.in.logon_server = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.function_code = 1;
	r.out.query = &query;

	for (f=0;f<ARRAY_SIZE(function_codes); f++) {
	for (i=1;i<5;i++) {

		r.in.function_code = function_codes[f];
		r.in.level = i;

		torture_comment(tctx, "Testing LogonControl function code %s (%d) level %d\n",
				function_code_str(tctx, r.in.function_code), r.in.function_code, r.in.level);

		status = dcerpc_netr_LogonControl_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "LogonControl");

		switch (r.in.level) {
		case 1:
			switch (r.in.function_code) {
			case NETLOGON_CONTROL_REPLICATE:
			case NETLOGON_CONTROL_SYNCHRONIZE:
			case NETLOGON_CONTROL_PDC_REPLICATE:
			case NETLOGON_CONTROL_BREAKPOINT:
			case NETLOGON_CONTROL_BACKUP_CHANGE_LOG:
				if ((secure_channel_type == SEC_CHAN_BDC) ||
				    (secure_channel_type == SEC_CHAN_WKSTA)) {
					torture_assert_werr_equal(tctx, r.out.result, WERR_ACCESS_DENIED,
						"LogonControl returned unexpected error code");
				} else {
					torture_assert_werr_equal(tctx, r.out.result, WERR_NOT_SUPPORTED,
						"LogonControl returned unexpected error code");
				}
				break;

			case NETLOGON_CONTROL_REDISCOVER:
			case NETLOGON_CONTROL_TC_QUERY:
			case NETLOGON_CONTROL_TRANSPORT_NOTIFY:
			case NETLOGON_CONTROL_FIND_USER:
			case NETLOGON_CONTROL_CHANGE_PASSWORD:
			case NETLOGON_CONTROL_TC_VERIFY:
			case NETLOGON_CONTROL_FORCE_DNS_REG:
			case NETLOGON_CONTROL_QUERY_DNS_REG:
			case NETLOGON_CONTROL_SET_DBFLAG:
				torture_assert_werr_equal(tctx, r.out.result, WERR_NOT_SUPPORTED,
					"LogonControl returned unexpected error code");
				break;
			case NETLOGON_CONTROL_TRUNCATE_LOG:
				if ((secure_channel_type == SEC_CHAN_BDC) ||
				    (secure_channel_type == SEC_CHAN_WKSTA)) {
					torture_assert_werr_equal(tctx, r.out.result, WERR_ACCESS_DENIED,
						"LogonControl returned unexpected error code");
				} else {
					torture_assert_werr_ok(tctx, r.out.result,
						"LogonControl returned unexpected result");
				}
				break;
			default:
				torture_assert_werr_ok(tctx, r.out.result,
					"LogonControl returned unexpected result");
				break;
			}
			break;
		case 2:
			torture_assert_werr_equal(tctx, r.out.result, WERR_NOT_SUPPORTED,
				"LogonControl returned unexpected error code");
			break;
		default:
			torture_assert_werr_equal(tctx, r.out.result, WERR_UNKNOWN_LEVEL,
				"LogonControl returned unexpected error code");
			break;
		}
	}
	}

	r.in.level = 52;
	torture_comment(tctx, "Testing LogonControl function code %s (%d) level %d\n",
			function_code_str(tctx, r.in.function_code), r.in.function_code, r.in.level);
	status = dcerpc_netr_LogonControl_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "LogonControl");
	torture_assert_werr_equal(tctx, r.out.result, WERR_INVALID_LEVEL, "LogonControl");

	return true;
}


/*
  try a netlogon GetAnyDCName
*/
static bool test_GetAnyDCName(struct torture_context *tctx, 
			      struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct netr_GetAnyDCName r;
	const char *dcname = NULL;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.domainname = lpcfg_workgroup(tctx->lp_ctx);
	r.in.logon_server = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.out.dcname = &dcname;

	status = dcerpc_netr_GetAnyDCName_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "GetAnyDCName");
	if ((!W_ERROR_IS_OK(r.out.result)) &&
	    (!W_ERROR_EQUAL(r.out.result, WERR_NO_SUCH_DOMAIN))) {
		return false;
	}

	if (dcname) {
	    torture_comment(tctx, "\tDC is at '%s'\n", dcname);
	}

	r.in.domainname = NULL;

	status = dcerpc_netr_GetAnyDCName_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "GetAnyDCName");
	if ((!W_ERROR_IS_OK(r.out.result)) &&
	    (!W_ERROR_EQUAL(r.out.result, WERR_NO_SUCH_DOMAIN))) {
		return false;
	}

	r.in.domainname = "";

	status = dcerpc_netr_GetAnyDCName_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "GetAnyDCName");
	if ((!W_ERROR_IS_OK(r.out.result)) &&
	    (!W_ERROR_EQUAL(r.out.result, WERR_NO_SUCH_DOMAIN))) {
		return false;
	}

	return true;
}


/*
  try a netlogon LogonControl2
*/
static bool test_LogonControl2(struct torture_context *tctx, 
			       struct dcerpc_pipe *p,
			       struct cli_credentials *machine_credentials)

{
	NTSTATUS status;
	struct netr_LogonControl2 r;
	union netr_CONTROL_DATA_INFORMATION data;
	union netr_CONTROL_QUERY_INFORMATION query;
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	data.domain = lpcfg_workgroup(tctx->lp_ctx);

	r.in.logon_server = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));

	r.in.function_code = NETLOGON_CONTROL_REDISCOVER;
	r.in.data = &data;
	r.out.query = &query;

	for (i=1;i<4;i++) {
		r.in.level = i;

		torture_comment(tctx, "Testing LogonControl2 function code %s (%d) level %d\n",
			function_code_str(tctx, r.in.function_code), r.in.function_code, r.in.level);

		status = dcerpc_netr_LogonControl2_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "LogonControl2");
	}

	data.domain = lpcfg_workgroup(tctx->lp_ctx);

	r.in.function_code = NETLOGON_CONTROL_TC_QUERY;
	r.in.data = &data;

	for (i=1;i<4;i++) {
		r.in.level = i;

		torture_comment(tctx, "Testing LogonControl2 function code %s (%d) level %d\n",
			function_code_str(tctx, r.in.function_code), r.in.function_code, r.in.level);

		status = dcerpc_netr_LogonControl2_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "LogonControl2");
	}

	data.domain = lpcfg_workgroup(tctx->lp_ctx);

	r.in.function_code = NETLOGON_CONTROL_TRANSPORT_NOTIFY;
	r.in.data = &data;

	for (i=1;i<4;i++) {
		r.in.level = i;

		torture_comment(tctx, "Testing LogonControl2 function code %s (%d) level %d\n",
			function_code_str(tctx, r.in.function_code), r.in.function_code, r.in.level);

		status = dcerpc_netr_LogonControl2_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "LogonControl2");
	}

	data.debug_level = ~0;

	r.in.function_code = NETLOGON_CONTROL_SET_DBFLAG;
	r.in.data = &data;

	for (i=1;i<4;i++) {
		r.in.level = i;

		torture_comment(tctx, "Testing LogonControl2 function code %s (%d) level %d\n",
			function_code_str(tctx, r.in.function_code), r.in.function_code, r.in.level);

		status = dcerpc_netr_LogonControl2_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "LogonControl2");
	}

	ZERO_STRUCT(data);
	r.in.function_code = 52;
	r.in.data = &data;

	torture_comment(tctx, "Testing LogonControl2 function code %s (%d) level %d\n",
			function_code_str(tctx, r.in.function_code), r.in.function_code, r.in.level);

	status = dcerpc_netr_LogonControl2_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "LogonControl2");
	torture_assert_werr_equal(tctx, r.out.result, WERR_UNKNOWN_LEVEL, "LogonControl2");

	data.debug_level = ~0;

	r.in.function_code = NETLOGON_CONTROL_SET_DBFLAG;
	r.in.data = &data;

	r.in.level = 52;
	torture_comment(tctx, "Testing LogonControl2 function code %s (%d) level %d\n",
			function_code_str(tctx, r.in.function_code), r.in.function_code, r.in.level);

	status = dcerpc_netr_LogonControl2_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "LogonControl2");
	torture_assert_werr_equal(tctx, r.out.result, WERR_UNKNOWN_LEVEL, "LogonControl2");

	return true;
}

/*
  try a netlogon DatabaseSync2
*/
static bool test_DatabaseSync2(struct torture_context *tctx, 
			       struct dcerpc_pipe *p,
			       struct cli_credentials *machine_credentials)
{
	struct netr_DatabaseSync2 r;
	struct netr_DELTA_ENUM_ARRAY *delta_enum_array = NULL;
	struct netr_Authenticator return_authenticator, credential;

	struct netlogon_creds_CredentialState *creds;
	const uint32_t database_ids[] = {0, 1, 2}; 
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_SetupCredentials2(p, tctx, NETLOGON_NEG_AUTH2_FLAGS, 
				    machine_credentials,
				    cli_credentials_get_secure_channel_type(machine_credentials),
				    &creds)) {
		return false;
	}

	ZERO_STRUCT(return_authenticator);

	r.in.logon_server = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computername = TEST_MACHINE_NAME;
	r.in.preferredmaximumlength = (uint32_t)-1;
	r.in.return_authenticator = &return_authenticator;
	r.out.return_authenticator = &return_authenticator;
	r.out.delta_enum_array = &delta_enum_array;

	for (i=0;i<ARRAY_SIZE(database_ids);i++) {

		uint32_t sync_context = 0;

		r.in.database_id = database_ids[i];
		r.in.sync_context = &sync_context;
		r.out.sync_context = &sync_context;
		r.in.restart_state = 0;

		torture_comment(tctx, "Testing DatabaseSync2 of id %d\n", r.in.database_id);

		do {
			netlogon_creds_client_authenticator(creds, &credential);

			r.in.credential = &credential;

			torture_assert_ntstatus_ok(tctx, dcerpc_netr_DatabaseSync2_r(b, tctx, &r),
				"DatabaseSync2 failed");
			if (NT_STATUS_EQUAL(r.out.result, STATUS_MORE_ENTRIES))
			    break;

			/* Native mode servers don't do this */
			if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_NOT_SUPPORTED)) {
				return true;
			}

			torture_assert_ntstatus_ok(tctx, r.out.result, "DatabaseSync2");

			if (!netlogon_creds_client_check(creds, &r.out.return_authenticator->cred)) {
				torture_comment(tctx, "Credential chaining failed\n");
			}

		} while (NT_STATUS_EQUAL(r.out.result, STATUS_MORE_ENTRIES));
	}

	return true;
}


/*
  try a netlogon LogonControl2Ex
*/
static bool test_LogonControl2Ex(struct torture_context *tctx, 
				 struct dcerpc_pipe *p,
				 struct cli_credentials *machine_credentials)

{
	NTSTATUS status;
	struct netr_LogonControl2Ex r;
	union netr_CONTROL_DATA_INFORMATION data;
	union netr_CONTROL_QUERY_INFORMATION query;
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	data.domain = lpcfg_workgroup(tctx->lp_ctx);

	r.in.logon_server = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));

	r.in.function_code = NETLOGON_CONTROL_REDISCOVER;
	r.in.data = &data;
	r.out.query = &query;

	for (i=1;i<4;i++) {
		r.in.level = i;

		torture_comment(tctx, "Testing LogonControl2Ex level %d function %d\n", 
		       i, r.in.function_code);

		status = dcerpc_netr_LogonControl2Ex_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "LogonControl");
	}

	data.domain = lpcfg_workgroup(tctx->lp_ctx);

	r.in.function_code = NETLOGON_CONTROL_TC_QUERY;
	r.in.data = &data;

	for (i=1;i<4;i++) {
		r.in.level = i;

		torture_comment(tctx, "Testing LogonControl2Ex level %d function %d\n", 
		       i, r.in.function_code);

		status = dcerpc_netr_LogonControl2Ex_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "LogonControl");
	}

	data.domain = lpcfg_workgroup(tctx->lp_ctx);

	r.in.function_code = NETLOGON_CONTROL_TRANSPORT_NOTIFY;
	r.in.data = &data;

	for (i=1;i<4;i++) {
		r.in.level = i;

		torture_comment(tctx, "Testing LogonControl2Ex level %d function %d\n", 
		       i, r.in.function_code);

		status = dcerpc_netr_LogonControl2Ex_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "LogonControl");
	}

	data.debug_level = ~0;

	r.in.function_code = NETLOGON_CONTROL_SET_DBFLAG;
	r.in.data = &data;

	for (i=1;i<4;i++) {
		r.in.level = i;

		torture_comment(tctx, "Testing LogonControl2Ex level %d function %d\n", 
		       i, r.in.function_code);

		status = dcerpc_netr_LogonControl2Ex_r(b, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "LogonControl");
	}

	return true;
}

static bool test_netr_GetForestTrustInformation(struct torture_context *tctx,
						struct dcerpc_pipe *p,
						struct cli_credentials *machine_credentials)
{
	struct netr_GetForestTrustInformation r;
	struct netlogon_creds_CredentialState *creds;
	struct netr_Authenticator a;
	struct netr_Authenticator return_authenticator;
	struct lsa_ForestTrustInformation *forest_trust_info;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_SetupCredentials3(p, tctx, NETLOGON_NEG_AUTH2_ADS_FLAGS,
				    machine_credentials, &creds)) {
		return false;
	}

	netlogon_creds_client_authenticator(creds, &a);

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computer_name = TEST_MACHINE_NAME;
	r.in.credential = &a;
	r.in.flags = 0;
	r.out.return_authenticator = &return_authenticator;
	r.out.forest_trust_info = &forest_trust_info;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_netr_GetForestTrustInformation_r(b, tctx, &r),
		"netr_GetForestTrustInformation failed");
	if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_NOT_IMPLEMENTED)) {
		torture_comment(tctx, "not considering NT_STATUS_NOT_IMPLEMENTED as an error\n");
	} else {
		torture_assert_ntstatus_ok(tctx, r.out.result,
			"netr_GetForestTrustInformation failed");
	}

	torture_assert(tctx,
		netlogon_creds_client_check(creds, &return_authenticator.cred),
		"Credential chaining failed");

	return true;
}

static bool test_netr_DsRGetForestTrustInformation(struct torture_context *tctx, 
						   struct dcerpc_pipe *p, const char *trusted_domain_name) 
{
	NTSTATUS status;
	struct netr_DsRGetForestTrustInformation r;
	struct lsa_ForestTrustInformation info, *info_ptr;
	struct dcerpc_binding_handle *b = p->binding_handle;

	info_ptr = &info;

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.trusted_domain_name = trusted_domain_name;
	r.in.flags = 0;
	r.out.forest_trust_info = &info_ptr;

	torture_comment(tctx ,"Testing netr_DsRGetForestTrustInformation\n");

	status = dcerpc_netr_DsRGetForestTrustInformation_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "DsRGetForestTrustInformation");
	torture_assert_werr_ok(tctx, r.out.result, "DsRGetForestTrustInformation");

	return true;
}

/*
  try a netlogon netr_DsrEnumerateDomainTrusts
*/
static bool test_DsrEnumerateDomainTrusts(struct torture_context *tctx, 
					  struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct netr_DsrEnumerateDomainTrusts r;
	struct netr_DomainTrustList trusts;
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.trust_flags = 0x3f;
	r.out.trusts = &trusts;

	status = dcerpc_netr_DsrEnumerateDomainTrusts_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "DsrEnumerateDomaintrusts");
	torture_assert_werr_ok(tctx, r.out.result, "DsrEnumerateDomaintrusts");

	/* when trusted_domain_name is NULL, netr_DsRGetForestTrustInformation
	 * will show non-forest trusts and all UPN suffixes of the own forest
	 * as LSA_FOREST_TRUST_TOP_LEVEL_NAME types */

	if (r.out.trusts->count) {
		if (!test_netr_DsRGetForestTrustInformation(tctx, p, NULL)) {
			return false;
		}
	}

	for (i=0; i<r.out.trusts->count; i++) {

		/* get info for transitive forest trusts */

		if (r.out.trusts->array[i].trust_attributes & NETR_TRUST_ATTRIBUTE_FOREST_TRANSITIVE) {
			if (!test_netr_DsRGetForestTrustInformation(tctx, p, 
								    r.out.trusts->array[i].dns_name)) {
				return false;
			}
		}
	}

	return true;
}

static bool test_netr_NetrEnumerateTrustedDomains(struct torture_context *tctx,
						  struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct netr_NetrEnumerateTrustedDomains r;
	struct netr_Blob trusted_domains_blob;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.out.trusted_domains_blob = &trusted_domains_blob;

	status = dcerpc_netr_NetrEnumerateTrustedDomains_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "netr_NetrEnumerateTrustedDomains");
	torture_assert_ntstatus_ok(tctx, r.out.result, "NetrEnumerateTrustedDomains");

	return true;
}

static bool test_netr_NetrEnumerateTrustedDomainsEx(struct torture_context *tctx,
						    struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct netr_NetrEnumerateTrustedDomainsEx r;
	struct netr_DomainTrustList dom_trust_list;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.out.dom_trust_list = &dom_trust_list;

	status = dcerpc_netr_NetrEnumerateTrustedDomainsEx_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "netr_NetrEnumerateTrustedDomainsEx");
	torture_assert_werr_ok(tctx, r.out.result, "NetrEnumerateTrustedDomainsEx");

	return true;
}


static bool test_netr_DsRGetSiteName(struct dcerpc_pipe *p, struct torture_context *tctx,
				     const char *computer_name, 
				     const char *expected_site) 
{
	NTSTATUS status;
	struct netr_DsRGetSiteName r;
	const char *site = NULL;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.computer_name		= computer_name;
	r.out.site			= &site;
	torture_comment(tctx, "Testing netr_DsRGetSiteName\n");

	status = dcerpc_netr_DsRGetSiteName_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "DsRGetSiteName");
	torture_assert_werr_ok(tctx, r.out.result, "DsRGetSiteName");
	torture_assert_str_equal(tctx, expected_site, site, "netr_DsRGetSiteName");

	if (torture_setting_bool(tctx, "samba4", false))
		torture_skip(tctx, "skipping computer name check against Samba4");

	r.in.computer_name		= talloc_asprintf(tctx, "\\\\%s", computer_name);
	torture_comment(tctx, 
			"Testing netr_DsRGetSiteName with broken computer name: %s\n", r.in.computer_name);

	status = dcerpc_netr_DsRGetSiteName_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "DsRGetSiteName");
	torture_assert_werr_equal(tctx, r.out.result, WERR_INVALID_COMPUTERNAME, "netr_DsRGetSiteName");

	return true;
}

/*
  try a netlogon netr_DsRGetDCName
*/
static bool test_netr_DsRGetDCName(struct torture_context *tctx, 
				   struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct netr_DsRGetDCName r;
	struct netr_DsRGetDCNameInfo *info = NULL;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_unc		= talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.domain_name	= lpcfg_dnsdomain(tctx->lp_ctx);
	r.in.domain_guid	= NULL;
	r.in.site_guid	        = NULL;
	r.in.flags		= DS_RETURN_DNS_NAME;
	r.out.info		= &info;

	status = dcerpc_netr_DsRGetDCName_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "DsRGetDCName");
	torture_assert_werr_ok(tctx, r.out.result, "DsRGetDCName");

	r.in.domain_name	= lpcfg_workgroup(tctx->lp_ctx);

	status = dcerpc_netr_DsRGetDCName_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "DsRGetDCName");
	torture_assert_werr_ok(tctx, r.out.result, "DsRGetDCName");

	return test_netr_DsRGetSiteName(p, tctx, 
				       info->dc_unc,
				       info->dc_site_name);
}

/*
  try a netlogon netr_DsRGetDCNameEx
*/
static bool test_netr_DsRGetDCNameEx(struct torture_context *tctx, 
				     struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct netr_DsRGetDCNameEx r;
	struct netr_DsRGetDCNameInfo *info = NULL;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_unc		= talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.domain_name	= lpcfg_dnsdomain(tctx->lp_ctx);
	r.in.domain_guid	= NULL;
	r.in.site_name	        = NULL;
	r.in.flags		= DS_RETURN_DNS_NAME;
	r.out.info		= &info;

	status = dcerpc_netr_DsRGetDCNameEx_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "netr_DsRGetDCNameEx");
	torture_assert_werr_ok(tctx, r.out.result, "netr_DsRGetDCNameEx");

	r.in.domain_name	= lpcfg_workgroup(tctx->lp_ctx);

	status = dcerpc_netr_DsRGetDCNameEx_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "netr_DsRGetDCNameEx");
	torture_assert_werr_ok(tctx, r.out.result, "netr_DsRGetDCNameEx");

	return test_netr_DsRGetSiteName(p, tctx, info->dc_unc,
				        info->dc_site_name);
}

/*
  try a netlogon netr_DsRGetDCNameEx2
*/
static bool test_netr_DsRGetDCNameEx2(struct torture_context *tctx, 
				      struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct netr_DsRGetDCNameEx2 r;
	struct netr_DsRGetDCNameInfo *info = NULL;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_comment(tctx, "Testing netr_DsRGetDCNameEx2 with no inputs\n");
	ZERO_STRUCT(r.in);
	r.in.flags		= DS_RETURN_DNS_NAME;
	r.out.info		= &info;

	status = dcerpc_netr_DsRGetDCNameEx2_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "netr_DsRGetDCNameEx2");
	torture_assert_werr_ok(tctx, r.out.result, "netr_DsRGetDCNameEx2");

	r.in.server_unc		= talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.client_account	= NULL;
	r.in.mask		= 0x00000000;
	r.in.domain_name	= lpcfg_dnsdomain(tctx->lp_ctx);
	r.in.domain_guid	= NULL;
	r.in.site_name		= NULL;
	r.in.flags		= DS_RETURN_DNS_NAME;
	r.out.info		= &info;

	torture_comment(tctx, "Testing netr_DsRGetDCNameEx2 without client account\n");

	status = dcerpc_netr_DsRGetDCNameEx2_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "netr_DsRGetDCNameEx2");
	torture_assert_werr_ok(tctx, r.out.result, "netr_DsRGetDCNameEx2");

	r.in.domain_name	= lpcfg_workgroup(tctx->lp_ctx);

	status = dcerpc_netr_DsRGetDCNameEx2_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "netr_DsRGetDCNameEx2");
	torture_assert_werr_ok(tctx, r.out.result, "netr_DsRGetDCNameEx2");

	torture_comment(tctx, "Testing netr_DsRGetDCNameEx2 with client account\n");
	r.in.client_account	= TEST_MACHINE_NAME"$";
	r.in.mask		= ACB_SVRTRUST;
	r.in.flags		= DS_RETURN_FLAT_NAME;
	r.out.info		= &info;

	status = dcerpc_netr_DsRGetDCNameEx2_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "netr_DsRGetDCNameEx2");
	torture_assert_werr_ok(tctx, r.out.result, "netr_DsRGetDCNameEx2");

	return test_netr_DsRGetSiteName(p, tctx, info->dc_unc,
					info->dc_site_name);
}

/* This is a substitution for "samdb_server_site_name" which relies on the
 * correct "lp_ctx" and therefore can't be used here. */
static const char *server_site_name(struct torture_context *tctx,
				    struct ldb_context *ldb)
{
	TALLOC_CTX *tmp_ctx;
	struct ldb_dn *dn, *server_dn;
	const struct ldb_val *site_name_val;
	const char *server_dn_str, *site_name;

	tmp_ctx = talloc_new(ldb);
	if (tmp_ctx == NULL) {
		goto failed;
	}

	dn = ldb_dn_new(tmp_ctx, ldb, "");
	if (dn == NULL) {
		goto failed;
	}

	server_dn_str = samdb_search_string(ldb, tmp_ctx, dn, "serverName",
					    NULL);
	if (server_dn_str == NULL) {
		goto failed;
	}

	server_dn = ldb_dn_new(tmp_ctx, ldb, server_dn_str);
	if (server_dn == NULL) {
		goto failed;
	}

	/* CN=<Server name>, CN=Servers, CN=<Site name>, CN=Sites, ... */
	site_name_val = ldb_dn_get_component_val(server_dn, 2);
	if (site_name_val == NULL) {
		goto failed;
	}

	site_name = (const char *) site_name_val->data;

	talloc_steal(tctx, site_name);
	talloc_free(tmp_ctx);

	return site_name;

failed:
	talloc_free(tmp_ctx);
	return NULL;
}

static bool test_netr_DsrGetDcSiteCoverageW(struct torture_context *tctx, 
					    struct dcerpc_pipe *p)
{
	char *url;
	struct ldb_context *sam_ctx = NULL;
	NTSTATUS status;
	struct netr_DsrGetDcSiteCoverageW r;
	struct DcSitesCtr *ctr = NULL;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_comment(tctx, "This does only pass with the default site\n");

	/* We won't double-check this when we are over 'local' transports */
	if (dcerpc_server_name(p)) {
		/* Set up connection to SAMDB on DC */
		url = talloc_asprintf(tctx, "ldap://%s", dcerpc_server_name(p));
		sam_ctx = ldb_wrap_connect(tctx, tctx->ev, tctx->lp_ctx, url,
					   NULL,
					   cmdline_credentials,
					   0);

		torture_assert(tctx, sam_ctx, "Connection to the SAMDB on DC failed!");
        }

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.out.ctr = &ctr;

	status = dcerpc_netr_DsrGetDcSiteCoverageW_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "failed");
	torture_assert_werr_ok(tctx, r.out.result, "failed");

	torture_assert(tctx, ctr->num_sites == 1,
		       "we should per default only get the default site");
	if (sam_ctx != NULL) {
		torture_assert_casestr_equal(tctx, ctr->sites[0].string,
					     server_site_name(tctx, sam_ctx),
					     "didn't return default site");
	}

	return true;
}

static bool test_netr_DsRAddressToSitenamesW(struct torture_context *tctx,
					     struct dcerpc_pipe *p)
{
	char *url;
	struct ldb_context *sam_ctx = NULL;
	NTSTATUS status;
	struct netr_DsRAddressToSitenamesW r;
	struct netr_DsRAddress addrs[6];
	struct sockaddr_in *addr;
#ifdef HAVE_IPV6
	struct sockaddr_in6 *addr6;
#endif
	struct netr_DsRAddressToSitenamesWCtr *ctr;
	struct dcerpc_binding_handle *b = p->binding_handle;
	uint32_t i;
	int ret;

	torture_comment(tctx, "This does only pass with the default site\n");

	/* We won't double-check this when we are over 'local' transports */
	if (dcerpc_server_name(p)) {
		/* Set up connection to SAMDB on DC */
		url = talloc_asprintf(tctx, "ldap://%s", dcerpc_server_name(p));
		sam_ctx = ldb_wrap_connect(tctx, tctx->ev, tctx->lp_ctx, url,
					   NULL,
					   cmdline_credentials,
					   0);

		torture_assert(tctx, sam_ctx, "Connection to the SAMDB on DC failed!");
        }

	/* First try valid IP addresses */

	addrs[0].size = sizeof(struct sockaddr_in);
	addrs[0].buffer = talloc_zero_array(tctx, uint8_t, addrs[0].size);
	addr = (struct sockaddr_in *) addrs[0].buffer;
	addrs[0].buffer[0] = AF_INET;
	ret = inet_pton(AF_INET, "127.0.0.1", &addr->sin_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");

	addrs[1].size = sizeof(struct sockaddr_in);
	addrs[1].buffer = talloc_zero_array(tctx, uint8_t, addrs[1].size);
	addr = (struct sockaddr_in *) addrs[1].buffer;
	addrs[1].buffer[0] = AF_INET;
	ret = inet_pton(AF_INET, "0.0.0.0", &addr->sin_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");

	addrs[2].size = sizeof(struct sockaddr_in);
	addrs[2].buffer = talloc_zero_array(tctx, uint8_t, addrs[2].size);
	addr = (struct sockaddr_in *) addrs[2].buffer;
	addrs[2].buffer[0] = AF_INET;
	ret = inet_pton(AF_INET, "255.255.255.255", &addr->sin_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");

#ifdef HAVE_IPV6
	addrs[3].size = sizeof(struct sockaddr_in6);
	addrs[3].buffer = talloc_zero_array(tctx, uint8_t, addrs[3].size);
	addr6 = (struct sockaddr_in6 *) addrs[3].buffer;
	addrs[3].buffer[0] = AF_INET6;
	ret = inet_pton(AF_INET6, "::1", &addr6->sin6_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");

	addrs[4].size = sizeof(struct sockaddr_in6);
	addrs[4].buffer = talloc_zero_array(tctx, uint8_t, addrs[4].size);
	addr6 = (struct sockaddr_in6 *) addrs[4].buffer;
	addrs[4].buffer[0] = AF_INET6;
	ret = inet_pton(AF_INET6, "::", &addr6->sin6_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");

	addrs[5].size = sizeof(struct sockaddr_in6);
	addrs[5].buffer = talloc_zero_array(tctx, uint8_t, addrs[5].size);
	addr6 = (struct sockaddr_in6 *) addrs[5].buffer;
	addrs[5].buffer[0] = AF_INET6;
	ret = inet_pton(AF_INET6, "ff02::1", &addr6->sin6_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");
#else
	/* the test cases are repeated to have exactly 6. This is for
	 * compatibility with IPv4-only machines */
	addrs[3].size = sizeof(struct sockaddr_in);
	addrs[3].buffer = talloc_zero_array(tctx, uint8_t, addrs[3].size);
	addr = (struct sockaddr_in *) addrs[3].buffer;
	addrs[3].buffer[0] = AF_INET;
	ret = inet_pton(AF_INET, "127.0.0.1", &addr->sin_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");

	addrs[4].size = sizeof(struct sockaddr_in);
	addrs[4].buffer = talloc_zero_array(tctx, uint8_t, addrs[4].size);
	addr = (struct sockaddr_in *) addrs[4].buffer;
	addrs[4].buffer[0] = AF_INET;
	ret = inet_pton(AF_INET, "0.0.0.0", &addr->sin_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");

	addrs[5].size = sizeof(struct sockaddr_in);
	addrs[5].buffer = talloc_zero_array(tctx, uint8_t, addrs[5].size);
	addr = (struct sockaddr_in *) addrs[5].buffer;
	addrs[5].buffer[0] = AF_INET;
	ret = inet_pton(AF_INET, "255.255.255.255", &addr->sin_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");
#endif

	ctr = talloc(tctx, struct netr_DsRAddressToSitenamesWCtr);

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.count = 6;
	r.in.addresses = addrs;
	r.out.ctr = &ctr;

	status = dcerpc_netr_DsRAddressToSitenamesW_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "failed");
	torture_assert_werr_ok(tctx, r.out.result, "failed");

	if (sam_ctx != NULL) {
		for (i = 0; i < 3; i++) {
			torture_assert_casestr_equal(tctx,
						     ctr->sitename[i].string,
						     server_site_name(tctx, sam_ctx),
						     "didn't return default site");
		}
		for (i = 3; i < 6; i++) {
			/* Windows returns "NULL" for the sitename if it isn't
			 * IPv6 configured */
			if (torture_setting_bool(tctx, "samba4", false)) {
				torture_assert_casestr_equal(tctx,
							     ctr->sitename[i].string,
							     server_site_name(tctx, sam_ctx),
							     "didn't return default site");
			}
		}
	}

	/* Now try invalid ones (too short buffers) */

	addrs[0].size = 0;
	addrs[1].size = 1;
	addrs[2].size = 4;

	addrs[3].size = 0;
	addrs[4].size = 1;
	addrs[5].size = 4;

	status = dcerpc_netr_DsRAddressToSitenamesW_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "failed");
	torture_assert_werr_ok(tctx, r.out.result, "failed");

	for (i = 0; i < 6; i++) {
		torture_assert(tctx, ctr->sitename[i].string == NULL,
			       "sitename should be null");
	}

	/* Now try invalid ones (wrong address types) */

	addrs[0].size = 10;
	addrs[0].buffer[0] = AF_UNSPEC;
	addrs[1].size = 10;
	addrs[1].buffer[0] = AF_UNIX; /* AF_LOCAL = AF_UNIX */
	addrs[2].size = 10;
	addrs[2].buffer[0] = AF_UNIX;

	addrs[3].size = 10;
	addrs[3].buffer[0] = 250;
	addrs[4].size = 10;
	addrs[4].buffer[0] = 251;
	addrs[5].size = 10;
	addrs[5].buffer[0] = 252;

	status = dcerpc_netr_DsRAddressToSitenamesW_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "failed");
	torture_assert_werr_ok(tctx, r.out.result, "failed");

	for (i = 0; i < 6; i++) {
		torture_assert(tctx, ctr->sitename[i].string == NULL,
			       "sitename should be null");
	}

	return true;
}

static bool test_netr_DsRAddressToSitenamesExW(struct torture_context *tctx,
					       struct dcerpc_pipe *p)
{
	char *url;
	struct ldb_context *sam_ctx = NULL;
	NTSTATUS status;
	struct netr_DsRAddressToSitenamesExW r;
	struct netr_DsRAddress addrs[6];
	struct sockaddr_in *addr;
#ifdef HAVE_IPV6
	struct sockaddr_in6 *addr6;
#endif
	struct netr_DsRAddressToSitenamesExWCtr *ctr;
	struct dcerpc_binding_handle *b = p->binding_handle;
	uint32_t i;
	int ret;

	torture_comment(tctx, "This does pass with the default site\n");

	/* We won't double-check this when we are over 'local' transports */
	if (dcerpc_server_name(p)) {
		/* Set up connection to SAMDB on DC */
		url = talloc_asprintf(tctx, "ldap://%s", dcerpc_server_name(p));
		sam_ctx = ldb_wrap_connect(tctx, tctx->ev, tctx->lp_ctx, url,
					   NULL,
					   cmdline_credentials,
					   0);

		torture_assert(tctx, sam_ctx, "Connection to the SAMDB on DC failed!");
        }

	/* First try valid IP addresses */

	addrs[0].size = sizeof(struct sockaddr_in);
	addrs[0].buffer = talloc_zero_array(tctx, uint8_t, addrs[0].size);
	addr = (struct sockaddr_in *) addrs[0].buffer;
	addrs[0].buffer[0] = AF_INET;
	ret = inet_pton(AF_INET, "127.0.0.1", &addr->sin_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");

	addrs[1].size = sizeof(struct sockaddr_in);
	addrs[1].buffer = talloc_zero_array(tctx, uint8_t, addrs[1].size);
	addr = (struct sockaddr_in *) addrs[1].buffer;
	addrs[1].buffer[0] = AF_INET;
	ret = inet_pton(AF_INET, "0.0.0.0", &addr->sin_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");

	addrs[2].size = sizeof(struct sockaddr_in);
	addrs[2].buffer = talloc_zero_array(tctx, uint8_t, addrs[2].size);
	addr = (struct sockaddr_in *) addrs[2].buffer;
	addrs[2].buffer[0] = AF_INET;
	ret = inet_pton(AF_INET, "255.255.255.255", &addr->sin_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");

#ifdef HAVE_IPV6
	addrs[3].size = sizeof(struct sockaddr_in6);
	addrs[3].buffer = talloc_zero_array(tctx, uint8_t, addrs[3].size);
	addr6 = (struct sockaddr_in6 *) addrs[3].buffer;
	addrs[3].buffer[0] = AF_INET6;
	ret = inet_pton(AF_INET6, "::1", &addr6->sin6_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");

	addrs[4].size = sizeof(struct sockaddr_in6);
	addrs[4].buffer = talloc_zero_array(tctx, uint8_t, addrs[4].size);
	addr6 = (struct sockaddr_in6 *) addrs[4].buffer;
	addrs[4].buffer[0] = AF_INET6;
	ret = inet_pton(AF_INET6, "::", &addr6->sin6_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");

	addrs[5].size = sizeof(struct sockaddr_in6);
	addrs[5].buffer = talloc_zero_array(tctx, uint8_t, addrs[5].size);
	addr6 = (struct sockaddr_in6 *) addrs[5].buffer;
	addrs[5].buffer[0] = AF_INET6;
	ret = inet_pton(AF_INET6, "ff02::1", &addr6->sin6_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");
#else
	/* the test cases are repeated to have exactly 6. This is for
	 * compatibility with IPv4-only machines */
	addrs[3].size = sizeof(struct sockaddr_in);
	addrs[3].buffer = talloc_zero_array(tctx, uint8_t, addrs[3].size);
	addr = (struct sockaddr_in *) addrs[3].buffer;
	addrs[3].buffer[0] = AF_INET;
	ret = inet_pton(AF_INET, "127.0.0.1", &addr->sin_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");

	addrs[4].size = sizeof(struct sockaddr_in);
	addrs[4].buffer = talloc_zero_array(tctx, uint8_t, addrs[4].size);
	addr = (struct sockaddr_in *) addrs[4].buffer;
	addrs[4].buffer[0] = AF_INET;
	ret = inet_pton(AF_INET, "0.0.0.0", &addr->sin_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");

	addrs[5].size = sizeof(struct sockaddr_in);
	addrs[5].buffer = talloc_zero_array(tctx, uint8_t, addrs[5].size);
	addr = (struct sockaddr_in *) addrs[5].buffer;
	addrs[5].buffer[0] = AF_INET;
	ret = inet_pton(AF_INET, "255.255.255.255", &addr->sin_addr);
	torture_assert(tctx, ret > 0, "inet_pton failed");
#endif

	ctr = talloc(tctx, struct netr_DsRAddressToSitenamesExWCtr);

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.count = 6;
	r.in.addresses = addrs;
	r.out.ctr = &ctr;

	status = dcerpc_netr_DsRAddressToSitenamesExW_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "failed");
	torture_assert_werr_ok(tctx, r.out.result, "failed");

	if (sam_ctx != NULL) {
		for (i = 0; i < 3; i++) {
			torture_assert_casestr_equal(tctx,
						     ctr->sitename[i].string,
						     server_site_name(tctx, sam_ctx),
						     "didn't return default site");
			torture_assert(tctx, ctr->subnetname[i].string == NULL,
				       "subnet should be null");
		}
		for (i = 3; i < 6; i++) {
			/* Windows returns "NULL" for the sitename if it isn't
			 * IPv6 configured */
			if (torture_setting_bool(tctx, "samba4", false)) {
				torture_assert_casestr_equal(tctx,
							     ctr->sitename[i].string,
							     server_site_name(tctx, sam_ctx),
							     "didn't return default site");
			}
			torture_assert(tctx, ctr->subnetname[i].string == NULL,
				       "subnet should be null");
		}
	}

	/* Now try invalid ones (too short buffers) */

	addrs[0].size = 0;
	addrs[1].size = 1;
	addrs[2].size = 4;

	addrs[3].size = 0;
	addrs[4].size = 1;
	addrs[5].size = 4;

	status = dcerpc_netr_DsRAddressToSitenamesExW_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "failed");
	torture_assert_werr_ok(tctx, r.out.result, "failed");

	for (i = 0; i < 6; i++) {
		torture_assert(tctx, ctr->sitename[i].string == NULL,
			       "sitename should be null");
		torture_assert(tctx, ctr->subnetname[i].string == NULL,
			       "subnet should be null");
	}

	addrs[0].size = 10;
	addrs[0].buffer[0] = AF_UNSPEC;
	addrs[1].size = 10;
	addrs[1].buffer[0] = AF_UNIX; /* AF_LOCAL = AF_UNIX */
	addrs[2].size = 10;
	addrs[2].buffer[0] = AF_UNIX;

	addrs[3].size = 10;
	addrs[3].buffer[0] = 250;
	addrs[4].size = 10;
	addrs[4].buffer[0] = 251;
	addrs[5].size = 10;
	addrs[5].buffer[0] = 252;

	status = dcerpc_netr_DsRAddressToSitenamesExW_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "failed");
	torture_assert_werr_ok(tctx, r.out.result, "failed");

	for (i = 0; i < 6; i++) {
		torture_assert(tctx, ctr->sitename[i].string == NULL,
			       "sitename should be null");
		torture_assert(tctx, ctr->subnetname[i].string == NULL,
			       "subnet should be null");
	}

	return true;
}

static bool test_netr_ServerGetTrustInfo(struct torture_context *tctx,
					 struct dcerpc_pipe *p,
					 struct cli_credentials *machine_credentials)
{
	struct netr_ServerGetTrustInfo r;

	struct netr_Authenticator a;
	struct netr_Authenticator return_authenticator;
	struct samr_Password new_owf_password;
	struct samr_Password old_owf_password;
	struct netr_TrustInfo *trust_info;

	struct netlogon_creds_CredentialState *creds;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_SetupCredentials3(p, tctx, NETLOGON_NEG_AUTH2_ADS_FLAGS,
				    machine_credentials, &creds)) {
		return false;
	}

	netlogon_creds_client_authenticator(creds, &a);

	r.in.server_name		= talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.account_name		= talloc_asprintf(tctx, "%s$", TEST_MACHINE_NAME);
	r.in.secure_channel_type	= cli_credentials_get_secure_channel_type(machine_credentials);
	r.in.computer_name		= TEST_MACHINE_NAME;
	r.in.credential			= &a;

	r.out.return_authenticator	= &return_authenticator;
	r.out.new_owf_password		= &new_owf_password;
	r.out.old_owf_password		= &old_owf_password;
	r.out.trust_info		= &trust_info;

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_ServerGetTrustInfo_r(b, tctx, &r),
		"ServerGetTrustInfo failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "ServerGetTrustInfo failed");
	torture_assert(tctx, netlogon_creds_client_check(creds, &return_authenticator.cred), "Credential chaining failed");

	return true;
}


static bool test_GetDomainInfo(struct torture_context *tctx, 
			       struct dcerpc_pipe *p,
			       struct cli_credentials *machine_credentials)
{
	struct netr_LogonGetDomainInfo r;
	struct netr_WorkstationInformation q1;
	struct netr_Authenticator a;
	struct netlogon_creds_CredentialState *creds;
	struct netr_OsVersion os;
	union netr_WorkstationInfo query;
	union netr_DomainInfo info;
	const char* const attrs[] = { "dNSHostName", "operatingSystem",
		"operatingSystemServicePack", "operatingSystemVersion",
		"servicePrincipalName", NULL };
	char *url;
	struct ldb_context *sam_ctx = NULL;
	struct ldb_message **res;
	struct ldb_message_element *spn_el;
	int ret, i;
	char *version_str;
	const char *old_dnsname = NULL;
	char **spns = NULL;
	int num_spns = 0;
	char *temp_str;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_comment(tctx, "Testing netr_LogonGetDomainInfo\n");

	if (!test_SetupCredentials3(p, tctx, NETLOGON_NEG_AUTH2_ADS_FLAGS, 
				    machine_credentials, &creds)) {
		return false;
	}

	/* We won't double-check this when we are over 'local' transports */
	if (dcerpc_server_name(p)) {
		/* Set up connection to SAMDB on DC */
		url = talloc_asprintf(tctx, "ldap://%s", dcerpc_server_name(p));
		sam_ctx = ldb_wrap_connect(tctx, tctx->ev, tctx->lp_ctx, url,
					   NULL,
					   cmdline_credentials,
					   0);
		
		torture_assert(tctx, sam_ctx, "Connection to the SAMDB on DC failed!");
	}

	torture_comment(tctx, "Testing netr_LogonGetDomainInfo 1st call (no variation of DNS hostname)\n");
	netlogon_creds_client_authenticator(creds, &a);

	ZERO_STRUCT(r);
	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computer_name = TEST_MACHINE_NAME;
	r.in.credential = &a;
	r.in.level = 1;
	r.in.return_authenticator = &a;
	r.in.query = &query;
	r.out.return_authenticator = &a;
	r.out.info = &info;

	ZERO_STRUCT(os);
	os.os.MajorVersion = 123;
	os.os.MinorVersion = 456;
	os.os.BuildNumber = 789;
	os.os.CSDVersion = "Service Pack 10";
	os.os.ServicePackMajor = 10;
	os.os.ServicePackMinor = 1;
	os.os.SuiteMask = NETR_VER_SUITE_SINGLEUSERTS;
	os.os.ProductType = NETR_VER_NT_SERVER;
	os.os.Reserved = 0;

	version_str = talloc_asprintf(tctx, "%d.%d (%d)", os.os.MajorVersion,
		os.os.MinorVersion, os.os.BuildNumber);

	ZERO_STRUCT(q1);
	q1.dns_hostname = talloc_asprintf(tctx, "%s.%s", TEST_MACHINE_NAME,
		lpcfg_dnsdomain(tctx->lp_ctx));
	q1.sitename = "Default-First-Site-Name";
	q1.os_version.os = &os;
	q1.os_name.string = talloc_asprintf(tctx,
					    "Tortured by Samba4 RPC-NETLOGON: %s",
					    timestring(tctx, time(NULL)));

	/* The workstation handles the "servicePrincipalName" and DNS hostname
	   updates */
	q1.workstation_flags = NETR_WS_FLAG_HANDLES_SPN_UPDATE;

	query.workstation_info = &q1;

	if (sam_ctx) {
		/* Gets back the old DNS hostname in AD */
		ret = gendb_search(sam_ctx, tctx, NULL, &res, attrs,
				   "(sAMAccountName=%s$)", TEST_MACHINE_NAME);
		old_dnsname =
			ldb_msg_find_attr_as_string(res[0], "dNSHostName", NULL);
		
		/* Gets back the "servicePrincipalName"s in AD */
		spn_el = ldb_msg_find_element(res[0], "servicePrincipalName");
		if (spn_el != NULL) {
			for (i=0; i < spn_el->num_values; i++) {
				spns = talloc_realloc(tctx, spns, char *, i + 1);
				spns[i] = (char *) spn_el->values[i].data;
			}
			num_spns = i;
		}
	}

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonGetDomainInfo_r(b, tctx, &r),
		"LogonGetDomainInfo failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "LogonGetDomainInfo failed");
	torture_assert(tctx, netlogon_creds_client_check(creds, &a.cred), "Credential chaining failed");

	smb_msleep(250);

	if (sam_ctx) {
		/* AD workstation infos entry check */
		ret = gendb_search(sam_ctx, tctx, NULL, &res, attrs,
				   "(sAMAccountName=%s$)", TEST_MACHINE_NAME);
		torture_assert(tctx, ret == 1, "Test machine account not found in SAMDB on DC! Has the workstation been joined?");
		torture_assert_str_equal(tctx,
					 ldb_msg_find_attr_as_string(res[0], "operatingSystem", NULL),
					 q1.os_name.string, "'operatingSystem' wrong!");
		torture_assert_str_equal(tctx,
					 ldb_msg_find_attr_as_string(res[0], "operatingSystemServicePack", NULL),
					 os.os.CSDVersion, "'operatingSystemServicePack' wrong!");
		torture_assert_str_equal(tctx,
					 ldb_msg_find_attr_as_string(res[0], "operatingSystemVersion", NULL),
					 version_str, "'operatingSystemVersion' wrong!");

		if (old_dnsname != NULL) {
			/* If before a DNS hostname was set then it should remain
			   the same in combination with the "servicePrincipalName"s.
			   The DNS hostname should also be returned by our
			   "LogonGetDomainInfo" call (in the domain info structure). */
			
			torture_assert_str_equal(tctx,
						 ldb_msg_find_attr_as_string(res[0], "dNSHostName", NULL),
						 old_dnsname, "'DNS hostname' was not set!");
			
			spn_el = ldb_msg_find_element(res[0], "servicePrincipalName");
			torture_assert(tctx, ((spns != NULL) && (spn_el != NULL)),
				       "'servicePrincipalName's not set!");
			torture_assert(tctx, spn_el->num_values == num_spns,
				       "'servicePrincipalName's incorrect!");
			for (i=0; (i < spn_el->num_values) && (i < num_spns); i++)
				torture_assert_str_equal(tctx,
							 (char *) spn_el->values[i].data,
				spns[i], "'servicePrincipalName's incorrect!");

			torture_assert_str_equal(tctx,
						 info.domain_info->dns_hostname.string,
						 old_dnsname,
						 "Out 'DNS hostname' doesn't match the old one!");
		} else {
			/* If no DNS hostname was set then also now none should be set,
			   the "servicePrincipalName"s should remain empty and no DNS
			   hostname should be returned by our "LogonGetDomainInfo"
			   call (in the domain info structure). */
			
			torture_assert(tctx,
				       ldb_msg_find_attr_as_string(res[0], "dNSHostName", NULL) == NULL,
				       "'DNS hostname' was set!");
			
			spn_el = ldb_msg_find_element(res[0], "servicePrincipalName");
			torture_assert(tctx, ((spns == NULL) && (spn_el == NULL)),
				       "'servicePrincipalName's were set!");
			
			torture_assert(tctx,
				       info.domain_info->dns_hostname.string == NULL,
				       "Out 'DNS host name' was set!");
		}
	}

	/* Checks "workstation flags" */
	torture_assert(tctx,
		info.domain_info->workstation_flags
		== NETR_WS_FLAG_HANDLES_SPN_UPDATE,
		"Out 'workstation flags' don't match!");


	torture_comment(tctx, "Testing netr_LogonGetDomainInfo 2nd call (variation of DNS hostname doesn't work)\n");
	netlogon_creds_client_authenticator(creds, &a);

	/* Wipe out the osVersion, and prove which values still 'stick' */
	q1.os_version.os = NULL;

	/* Change also the DNS hostname to test differences in behaviour */
	talloc_free(discard_const_p(char, q1.dns_hostname));
	q1.dns_hostname = talloc_asprintf(tctx, "%s2.%s", TEST_MACHINE_NAME,
		lpcfg_dnsdomain(tctx->lp_ctx));

	/* The workstation handles the "servicePrincipalName" and DNS hostname
	   updates */
	q1.workstation_flags = NETR_WS_FLAG_HANDLES_SPN_UPDATE;

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonGetDomainInfo_r(b, tctx, &r),
		"LogonGetDomainInfo failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "LogonGetDomainInfo failed");

	torture_assert(tctx, netlogon_creds_client_check(creds, &a.cred), "Credential chaining failed");

	smb_msleep(250);

	if (sam_ctx) {
		/* AD workstation infos entry check */
		ret = gendb_search(sam_ctx, tctx, NULL, &res, attrs,
				   "(sAMAccountName=%s$)", TEST_MACHINE_NAME);
		torture_assert(tctx, ret == 1, "Test machine account not found in SAMDB on DC! Has the workstation been joined?");

		torture_assert_str_equal(tctx,
					 ldb_msg_find_attr_as_string(res[0], "operatingSystem", NULL),
					 q1.os_name.string, "'operatingSystem' should stick!");
		torture_assert(tctx,
			       ldb_msg_find_attr_as_string(res[0], "operatingSystemServicePack", NULL) == NULL,
			       "'operatingSystemServicePack' shouldn't stick!");
		torture_assert(tctx,
			       ldb_msg_find_attr_as_string(res[0], "operatingSystemVersion", NULL) == NULL,
			       "'operatingSystemVersion' shouldn't stick!");

		/* The DNS host name shouldn't have been updated by the server */

		torture_assert_str_equal(tctx,
					 ldb_msg_find_attr_as_string(res[0], "dNSHostName", NULL),
					 old_dnsname, "'DNS host name' did change!");
		
		/* Find the two "servicePrincipalName"s which the DC shouldn't have been
		   updated (HOST/<Netbios name> and HOST/<FQDN name>) - see MS-NRPC
		   3.5.4.3.9 */
		spn_el = ldb_msg_find_element(res[0], "servicePrincipalName");
		torture_assert(tctx, spn_el != NULL,
			       "There should exist 'servicePrincipalName's in AD!");
		temp_str = talloc_asprintf(tctx, "HOST/%s", TEST_MACHINE_NAME);
		for (i=0; i < spn_el->num_values; i++)
			if (strcasecmp((char *) spn_el->values[i].data, temp_str) == 0)
				break;
		torture_assert(tctx, i != spn_el->num_values,
			       "'servicePrincipalName' HOST/<Netbios name> not found!");
		temp_str = talloc_asprintf(tctx, "HOST/%s", old_dnsname);
		for (i=0; i < spn_el->num_values; i++)
			if (strcasecmp((char *) spn_el->values[i].data, temp_str) == 0)
				break;
		torture_assert(tctx, i != spn_el->num_values,
			       "'servicePrincipalName' HOST/<FQDN name> not found!");
		
		/* Check that the out DNS hostname was set properly */
		torture_assert_str_equal(tctx, info.domain_info->dns_hostname.string,
					 old_dnsname, "Out 'DNS hostname' doesn't match the old one!");
	}

	/* Checks "workstation flags" */
	torture_assert(tctx,
		info.domain_info->workstation_flags == NETR_WS_FLAG_HANDLES_SPN_UPDATE,
		"Out 'workstation flags' don't match!");


	/* Now try the same but the workstation flags set to 0 */

	torture_comment(tctx, "Testing netr_LogonGetDomainInfo 3rd call (variation of DNS hostname doesn't work)\n");
	netlogon_creds_client_authenticator(creds, &a);

	/* Change also the DNS hostname to test differences in behaviour */
	talloc_free(discard_const_p(char, q1.dns_hostname));
	q1.dns_hostname = talloc_asprintf(tctx, "%s2.%s", TEST_MACHINE_NAME,
		lpcfg_dnsdomain(tctx->lp_ctx));

	/* Wipe out the osVersion, and prove which values still 'stick' */
	q1.os_version.os = NULL;

	/* Let the DC handle the "servicePrincipalName" and DNS hostname
	   updates */
	q1.workstation_flags = 0;

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonGetDomainInfo_r(b, tctx, &r),
		"LogonGetDomainInfo failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "LogonGetDomainInfo failed");
	torture_assert(tctx, netlogon_creds_client_check(creds, &a.cred), "Credential chaining failed");

	smb_msleep(250);

	if (sam_ctx) {
		/* AD workstation infos entry check */
		ret = gendb_search(sam_ctx, tctx, NULL, &res, attrs,
				   "(sAMAccountName=%s$)", TEST_MACHINE_NAME);
		torture_assert(tctx, ret == 1, "Test machine account not found in SAMDB on DC! Has the workstation been joined?");

		torture_assert_str_equal(tctx,
					 ldb_msg_find_attr_as_string(res[0], "operatingSystem", NULL),
					 q1.os_name.string, "'operatingSystem' should stick!");
		torture_assert(tctx,
			       ldb_msg_find_attr_as_string(res[0], "operatingSystemServicePack", NULL) == NULL,
			       "'operatingSystemServicePack' shouldn't stick!");
		torture_assert(tctx,
			       ldb_msg_find_attr_as_string(res[0], "operatingSystemVersion", NULL) == NULL,
			       "'operatingSystemVersion' shouldn't stick!");

		/* The DNS host name shouldn't have been updated by the server */

		torture_assert_str_equal(tctx,
					 ldb_msg_find_attr_as_string(res[0], "dNSHostName", NULL),
					 old_dnsname, "'DNS host name' did change!");

		/* Find the two "servicePrincipalName"s which the DC shouldn't have been
		   updated (HOST/<Netbios name> and HOST/<FQDN name>) - see MS-NRPC
		   3.5.4.3.9 */
		spn_el = ldb_msg_find_element(res[0], "servicePrincipalName");
		torture_assert(tctx, spn_el != NULL,
			       "There should exist 'servicePrincipalName's in AD!");
		temp_str = talloc_asprintf(tctx, "HOST/%s", TEST_MACHINE_NAME);
		for (i=0; i < spn_el->num_values; i++)
			if (strcasecmp((char *) spn_el->values[i].data, temp_str) == 0)
				break;
		torture_assert(tctx, i != spn_el->num_values,
			       "'servicePrincipalName' HOST/<Netbios name> not found!");
		temp_str = talloc_asprintf(tctx, "HOST/%s", old_dnsname);
		for (i=0; i < spn_el->num_values; i++)
			if (strcasecmp((char *) spn_el->values[i].data, temp_str) == 0)
				break;
		torture_assert(tctx, i != spn_el->num_values,
			       "'servicePrincipalName' HOST/<FQDN name> not found!");

		/* Here the server gives us NULL as the out DNS hostname */
		torture_assert(tctx, info.domain_info->dns_hostname.string == NULL,
			       "Out 'DNS hostname' should be NULL!");
	}

	/* Checks "workstation flags" */
	torture_assert(tctx,
		info.domain_info->workstation_flags == 0,
		"Out 'workstation flags' don't match!");


	torture_comment(tctx, "Testing netr_LogonGetDomainInfo 4th call (verification of DNS hostname and check for trusted domains)\n");
	netlogon_creds_client_authenticator(creds, &a);

	/* Put the DNS hostname back */
	talloc_free(discard_const_p(char, q1.dns_hostname));
	q1.dns_hostname = talloc_asprintf(tctx, "%s.%s", TEST_MACHINE_NAME,
		lpcfg_dnsdomain(tctx->lp_ctx));

	/* The workstation handles the "servicePrincipalName" and DNS hostname
	   updates */
	q1.workstation_flags = NETR_WS_FLAG_HANDLES_SPN_UPDATE;

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonGetDomainInfo_r(b, tctx, &r),
		"LogonGetDomainInfo failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "LogonGetDomainInfo failed");
	torture_assert(tctx, netlogon_creds_client_check(creds, &a.cred), "Credential chaining failed");

	smb_msleep(250);

	/* Now the in/out DNS hostnames should be the same */
	torture_assert_str_equal(tctx,
		info.domain_info->dns_hostname.string,
		query.workstation_info->dns_hostname,
		"In/Out 'DNS hostnames' don't match!");
	old_dnsname = info.domain_info->dns_hostname.string;

	/* Checks "workstation flags" */
	torture_assert(tctx,
		info.domain_info->workstation_flags
		== NETR_WS_FLAG_HANDLES_SPN_UPDATE,
		"Out 'workstation flags' don't match!");

	/* Checks for trusted domains */
	torture_assert(tctx,
		(info.domain_info->trusted_domain_count != 0)
		&& (info.domain_info->trusted_domains != NULL),
		"Trusted domains have been requested!");


	torture_comment(tctx, "Testing netr_LogonGetDomainInfo 5th call (check for trusted domains)\n");
	netlogon_creds_client_authenticator(creds, &a);

	/* The workstation handles the "servicePrincipalName" and DNS hostname
	   updates and requests inbound trusts */
	q1.workstation_flags = NETR_WS_FLAG_HANDLES_SPN_UPDATE
		| NETR_WS_FLAG_HANDLES_INBOUND_TRUSTS;

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonGetDomainInfo_r(b, tctx, &r),
		"LogonGetDomainInfo failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "LogonGetDomainInfo failed");
	torture_assert(tctx, netlogon_creds_client_check(creds, &a.cred), "Credential chaining failed");

	smb_msleep(250);

	/* Checks "workstation flags" */
	torture_assert(tctx,
		info.domain_info->workstation_flags
		== (NETR_WS_FLAG_HANDLES_SPN_UPDATE
			| NETR_WS_FLAG_HANDLES_INBOUND_TRUSTS),
		"Out 'workstation flags' don't match!");

	/* Checks for trusted domains */
	torture_assert(tctx,
		(info.domain_info->trusted_domain_count != 0)
		&& (info.domain_info->trusted_domains != NULL),
		"Trusted domains have been requested!");


	torture_comment(tctx, "Testing netr_LogonGetDomainInfo 6th call (no DNS hostname)\n");
	netlogon_creds_client_authenticator(creds, &a);

	query.workstation_info->dns_hostname = NULL;

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonGetDomainInfo_r(b, tctx, &r),
		"LogonGetDomainInfo failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "LogonGetDomainInfo failed");
	torture_assert(tctx, netlogon_creds_client_check(creds, &a.cred), "Credential chaining failed");

	/* The old DNS hostname should stick */
	torture_assert_str_equal(tctx,
		info.domain_info->dns_hostname.string,
		old_dnsname,
		"'DNS hostname' changed!");


	if (!torture_setting_bool(tctx, "dangerous", false)) {
		torture_comment(tctx, "Not testing netr_LogonGetDomainInfo 7th call (no workstation info) - enable dangerous tests in order to do so\n");
	} else {
		/* Try a call without the workstation information structure */

		torture_comment(tctx, "Testing netr_LogonGetDomainInfo 7th call (no workstation info)\n");
		netlogon_creds_client_authenticator(creds, &a);

		query.workstation_info = NULL;

		torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonGetDomainInfo_r(b, tctx, &r),
			"LogonGetDomainInfo failed");
		torture_assert_ntstatus_ok(tctx, r.out.result, "LogonGetDomainInfo failed");
		torture_assert(tctx, netlogon_creds_client_check(creds, &a.cred), "Credential chaining failed");
	}

	return true;
}

static bool test_GetDomainInfo_async(struct torture_context *tctx, 
				     struct dcerpc_pipe *p,
				     struct cli_credentials *machine_credentials)
{
	NTSTATUS status;
	struct netr_LogonGetDomainInfo r;
	struct netr_WorkstationInformation q1;
	struct netr_Authenticator a;
#define ASYNC_COUNT 100
	struct netlogon_creds_CredentialState *creds;
	struct netlogon_creds_CredentialState *creds_async[ASYNC_COUNT];
	struct tevent_req *req[ASYNC_COUNT];
	int i;
	union netr_WorkstationInfo query;
	union netr_DomainInfo info;

	torture_comment(tctx, "Testing netr_LogonGetDomainInfo - async count %d\n", ASYNC_COUNT);

	if (!test_SetupCredentials3(p, tctx, NETLOGON_NEG_AUTH2_ADS_FLAGS, 
				    machine_credentials, &creds)) {
		return false;
	}

	ZERO_STRUCT(r);
	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computer_name = TEST_MACHINE_NAME;
	r.in.credential = &a;
	r.in.level = 1;
	r.in.return_authenticator = &a;
	r.in.query = &query;
	r.out.return_authenticator = &a;
	r.out.info = &info;

	ZERO_STRUCT(q1);
	q1.dns_hostname = talloc_asprintf(tctx, "%s.%s", TEST_MACHINE_NAME,
		lpcfg_dnsdomain(tctx->lp_ctx));
	q1.sitename = "Default-First-Site-Name";
	q1.os_name.string = "UNIX/Linux or similar";

	query.workstation_info = &q1;

	for (i=0;i<ASYNC_COUNT;i++) {
		netlogon_creds_client_authenticator(creds, &a);

		creds_async[i] = (struct netlogon_creds_CredentialState *)talloc_memdup(creds, creds, sizeof(*creds));
		req[i] = dcerpc_netr_LogonGetDomainInfo_r_send(tctx, tctx->ev, p->binding_handle, &r);

		/* even with this flush per request a w2k3 server seems to 
		   clag with multiple outstanding requests. bleergh. */
		torture_assert_int_equal(tctx, event_loop_once(dcerpc_event_context(p)), 0, 
					 "event_loop_once failed");
	}

	for (i=0;i<ASYNC_COUNT;i++) {
		torture_assert_int_equal(tctx, tevent_req_poll(req[i], tctx->ev), true,
					 "tevent_req_poll() failed");

		status = dcerpc_netr_LogonGetDomainInfo_r_recv(req[i], tctx);

		torture_assert_ntstatus_ok(tctx, status, "netr_LogonGetDomainInfo_async");
		torture_assert_ntstatus_ok(tctx, r.out.result, "netr_LogonGetDomainInfo_async"); 

		torture_assert(tctx, netlogon_creds_client_check(creds_async[i], &a.cred), 
			"Credential chaining failed at async");
	}

	torture_comment(tctx, 
			"Testing netr_LogonGetDomainInfo - async count %d OK\n", ASYNC_COUNT);

	return true;
}

static bool test_ManyGetDCName(struct torture_context *tctx, 
			       struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct dcerpc_pipe *p2;
	struct lsa_ObjectAttribute attr;
	struct lsa_QosInfo qos;
	struct lsa_OpenPolicy2 o;
	struct policy_handle lsa_handle;
	struct lsa_DomainList domains;

	struct lsa_EnumTrustDom t;
	uint32_t resume_handle = 0;
	struct netr_GetAnyDCName d;
	const char *dcname = NULL;
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct dcerpc_binding_handle *b2;

	int i;

	if (p->conn->transport.transport != NCACN_NP) {
		return true;
	}

	torture_comment(tctx, "Torturing GetDCName\n");

	status = dcerpc_secondary_connection(p, &p2, p->binding);
	torture_assert_ntstatus_ok(tctx, status, "Failed to create secondary connection");

	status = dcerpc_bind_auth_none(p2, &ndr_table_lsarpc);
	torture_assert_ntstatus_ok(tctx, status, "Failed to create bind on secondary connection");
	b2 = p2->binding_handle;

	qos.len = 0;
	qos.impersonation_level = 2;
	qos.context_mode = 1;
	qos.effective_only = 0;

	attr.len = 0;
	attr.root_dir = NULL;
	attr.object_name = NULL;
	attr.attributes = 0;
	attr.sec_desc = NULL;
	attr.sec_qos = &qos;

	o.in.system_name = "\\";
	o.in.attr = &attr;
	o.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	o.out.handle = &lsa_handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_lsa_OpenPolicy2_r(b2, tctx, &o),
		"OpenPolicy2 failed");
	torture_assert_ntstatus_ok(tctx, o.out.result, "OpenPolicy2 failed");

	t.in.handle = &lsa_handle;
	t.in.resume_handle = &resume_handle;
	t.in.max_size = 1000;
	t.out.domains = &domains;
	t.out.resume_handle = &resume_handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_lsa_EnumTrustDom_r(b2, tctx, &t),
		"EnumTrustDom failed");

	if ((!NT_STATUS_IS_OK(t.out.result) &&
	     (!NT_STATUS_EQUAL(t.out.result, NT_STATUS_NO_MORE_ENTRIES))))
		torture_fail(tctx, "Could not list domains");

	talloc_free(p2);

	d.in.logon_server = talloc_asprintf(tctx, "\\\\%s",
					    dcerpc_server_name(p));
	d.out.dcname = &dcname;

	for (i=0; i<domains.count * 4; i++) {
		struct lsa_DomainInfo *info =
			&domains.domains[rand()%domains.count];

		d.in.domainname = info->name.string;

		status = dcerpc_netr_GetAnyDCName_r(b, tctx, &d);
		torture_assert_ntstatus_ok(tctx, status, "GetAnyDCName");

		torture_comment(tctx, "\tDC for domain %s is %s\n", info->name.string,
		       dcname ? dcname : "unknown");
	}

	return true;
}

static bool test_SetPassword_with_flags(struct torture_context *tctx,
					struct dcerpc_pipe *p,
					struct cli_credentials *machine_credentials)
{
	uint32_t flags[] = { 0, NETLOGON_NEG_STRONG_KEYS };
	struct netlogon_creds_CredentialState *creds;
	int i;

	if (!test_SetupCredentials2(p, tctx, 0,
				    machine_credentials,
				    cli_credentials_get_secure_channel_type(machine_credentials),
				    &creds)) {
		torture_skip(tctx, "DC does not support negotiation of 64bit session keys");
	}

	for (i=0; i < ARRAY_SIZE(flags); i++) {
		torture_assert(tctx,
			test_SetPassword_flags(tctx, p, machine_credentials, flags[i]),
			talloc_asprintf(tctx, "failed to test SetPassword negotiating with 0x%08x flags", flags[i]));
	}

	return true;
}

struct torture_suite *torture_rpc_netlogon(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "netlogon");
	struct torture_rpc_tcase *tcase;
	struct torture_test *test;

	tcase = torture_suite_add_machine_bdc_rpc_iface_tcase(suite, "netlogon",
						  &ndr_table_netlogon, TEST_MACHINE_NAME);

	torture_rpc_tcase_add_test(tcase, "LogonUasLogon", test_LogonUasLogon);
	torture_rpc_tcase_add_test(tcase, "LogonUasLogoff", test_LogonUasLogoff);
	torture_rpc_tcase_add_test_creds(tcase, "SamLogon", test_SamLogon);
	torture_rpc_tcase_add_test_creds(tcase, "SetPassword", test_SetPassword);
	torture_rpc_tcase_add_test_creds(tcase, "SetPassword2", test_SetPassword2);
	torture_rpc_tcase_add_test_creds(tcase, "GetPassword", test_GetPassword);
	torture_rpc_tcase_add_test_creds(tcase, "GetTrustPasswords", test_GetTrustPasswords);
	torture_rpc_tcase_add_test_creds(tcase, "GetDomainInfo", test_GetDomainInfo);
	torture_rpc_tcase_add_test_creds(tcase, "DatabaseSync", test_DatabaseSync);
	torture_rpc_tcase_add_test_creds(tcase, "DatabaseDeltas", test_DatabaseDeltas);
	torture_rpc_tcase_add_test_creds(tcase, "DatabaseRedo", test_DatabaseRedo);
	torture_rpc_tcase_add_test_creds(tcase, "AccountDeltas", test_AccountDeltas);
	torture_rpc_tcase_add_test_creds(tcase, "AccountSync", test_AccountSync);
	torture_rpc_tcase_add_test(tcase, "GetDcName", test_GetDcName);
	torture_rpc_tcase_add_test(tcase, "ManyGetDCName", test_ManyGetDCName);
	torture_rpc_tcase_add_test(tcase, "GetAnyDCName", test_GetAnyDCName);
	torture_rpc_tcase_add_test_creds(tcase, "DatabaseSync2", test_DatabaseSync2);
	torture_rpc_tcase_add_test(tcase, "DsrEnumerateDomainTrusts", test_DsrEnumerateDomainTrusts);
	torture_rpc_tcase_add_test(tcase, "NetrEnumerateTrustedDomains", test_netr_NetrEnumerateTrustedDomains);
	torture_rpc_tcase_add_test(tcase, "NetrEnumerateTrustedDomainsEx", test_netr_NetrEnumerateTrustedDomainsEx);
	test = torture_rpc_tcase_add_test_creds(tcase, "GetDomainInfo_async", test_GetDomainInfo_async);
	test->dangerous = true;
	torture_rpc_tcase_add_test(tcase, "DsRGetDCName", test_netr_DsRGetDCName);
	torture_rpc_tcase_add_test(tcase, "DsRGetDCNameEx", test_netr_DsRGetDCNameEx);
	torture_rpc_tcase_add_test(tcase, "DsRGetDCNameEx2", test_netr_DsRGetDCNameEx2);
	torture_rpc_tcase_add_test(tcase, "DsrGetDcSiteCoverageW", test_netr_DsrGetDcSiteCoverageW);
	torture_rpc_tcase_add_test(tcase, "DsRAddressToSitenamesW", test_netr_DsRAddressToSitenamesW);
	torture_rpc_tcase_add_test(tcase, "DsRAddressToSitenamesExW", test_netr_DsRAddressToSitenamesExW);
	torture_rpc_tcase_add_test_creds(tcase, "ServerGetTrustInfo", test_netr_ServerGetTrustInfo);
	torture_rpc_tcase_add_test_creds(tcase, "GetForestTrustInformation", test_netr_GetForestTrustInformation);

	return suite;
}

struct torture_suite *torture_rpc_netlogon_s3(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "netlogon-s3");
	struct torture_rpc_tcase *tcase;

	tcase = torture_suite_add_machine_bdc_rpc_iface_tcase(suite, "netlogon",
						  &ndr_table_netlogon, TEST_MACHINE_NAME);

	torture_rpc_tcase_add_test_creds(tcase, "SamLogon", test_SamLogon);
	torture_rpc_tcase_add_test_creds(tcase, "SamLogon_NULL_domain", test_SamLogon_NULL_domain);
	torture_rpc_tcase_add_test_creds(tcase, "SetPassword", test_SetPassword);
	torture_rpc_tcase_add_test_creds(tcase, "SetPassword_with_flags", test_SetPassword_with_flags);
	torture_rpc_tcase_add_test_creds(tcase, "SetPassword2", test_SetPassword2);
	torture_rpc_tcase_add_test(tcase, "NetrEnumerateTrustedDomains", test_netr_NetrEnumerateTrustedDomains);

	return suite;
}

struct torture_suite *torture_rpc_netlogon_admin(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "netlogon.admin");
	struct torture_rpc_tcase *tcase;

	tcase = torture_suite_add_machine_bdc_rpc_iface_tcase(suite, "netlogon",
						  &ndr_table_netlogon, TEST_MACHINE_NAME);
	torture_rpc_tcase_add_test_creds(tcase, "LogonControl", test_LogonControl);
	torture_rpc_tcase_add_test_creds(tcase, "LogonControl2", test_LogonControl2);
	torture_rpc_tcase_add_test_creds(tcase, "LogonControl2Ex", test_LogonControl2Ex);

	tcase = torture_suite_add_machine_workstation_rpc_iface_tcase(suite, "netlogon",
						  &ndr_table_netlogon, TEST_MACHINE_NAME);
	torture_rpc_tcase_add_test_creds(tcase, "LogonControl", test_LogonControl);
	torture_rpc_tcase_add_test_creds(tcase, "LogonControl2", test_LogonControl2);
	torture_rpc_tcase_add_test_creds(tcase, "LogonControl2Ex", test_LogonControl2Ex);

	tcase = torture_suite_add_rpc_iface_tcase(suite, "netlogon",
						  &ndr_table_netlogon);
	torture_rpc_tcase_add_test_creds(tcase, "LogonControl", test_LogonControl);
	torture_rpc_tcase_add_test_creds(tcase, "LogonControl2", test_LogonControl2);
	torture_rpc_tcase_add_test_creds(tcase, "LogonControl2Ex", test_LogonControl2Ex);

	return suite;
}
