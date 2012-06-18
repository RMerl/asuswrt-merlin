/* 
   Unix SMB/CIFS implementation.
   Small self-tests for the NTLMSSP code
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004
   
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
#include "auth/gensec/gensec.h"
#include "auth/gensec/gensec_proto.h"
#include "auth/ntlmssp/ntlmssp.h"
#include "lib/cmdline/popt_common.h"
#include "torture/torture.h"
#include "param/param.h"

static bool torture_ntlmssp_self_check(struct torture_context *tctx)
{
	struct gensec_security *gensec_security;
	struct gensec_ntlmssp_state *gensec_ntlmssp_state;
	DATA_BLOB data;
	DATA_BLOB sig, expected_sig;
	TALLOC_CTX *mem_ctx = tctx;

	torture_assert_ntstatus_ok(tctx, 
		gensec_client_start(mem_ctx, &gensec_security,
				    tctx->ev, lp_gensec_settings(tctx, tctx->lp_ctx)),
		"gensec client start");

	gensec_set_credentials(gensec_security, cmdline_credentials);

	gensec_want_feature(gensec_security, GENSEC_FEATURE_SIGN);
	gensec_want_feature(gensec_security, GENSEC_FEATURE_SEAL);

	torture_assert_ntstatus_ok(tctx, 
			gensec_start_mech_by_oid(gensec_security, GENSEC_OID_NTLMSSP),
			"Failed to start GENSEC for NTLMSSP");

	gensec_ntlmssp_state = (struct gensec_ntlmssp_state *)gensec_security->private_data;

	gensec_ntlmssp_state->session_key = strhex_to_data_blob(tctx, "0102030405060708090a0b0c0d0e0f00");
	dump_data_pw("NTLMSSP session key: \n", 
		     gensec_ntlmssp_state->session_key.data,  
		     gensec_ntlmssp_state->session_key.length);

	gensec_ntlmssp_state->neg_flags = NTLMSSP_NEGOTIATE_SIGN | NTLMSSP_NEGOTIATE_UNICODE | NTLMSSP_NEGOTIATE_128 | NTLMSSP_NEGOTIATE_KEY_EXCH | NTLMSSP_NEGOTIATE_NTLM2;

	torture_assert_ntstatus_ok(tctx,  
		ntlmssp_sign_init(gensec_ntlmssp_state),
		"Failed to sign_init");

	data = strhex_to_data_blob(tctx, "6a43494653");
	gensec_ntlmssp_sign_packet(gensec_security, gensec_security,
				   data.data, data.length, data.data, data.length, &sig);

	expected_sig = strhex_to_data_blob(tctx, "01000000e37f97f2544f4d7e00000000");

	dump_data_pw("NTLMSSP calc sig:     ", sig.data, sig.length);
	dump_data_pw("NTLMSSP expected sig: ", expected_sig.data, expected_sig.length);

	torture_assert_int_equal(tctx, sig.length, expected_sig.length, "Wrong sig length");

	torture_assert_mem_equal(tctx, sig.data, expected_sig.data, sig.length,
				   "data mismatch");

	torture_assert_ntstatus_equal(tctx, 
				      gensec_ntlmssp_check_packet(gensec_security, gensec_security,
								  data.data, data.length, data.data, data.length, &sig),
				      NT_STATUS_ACCESS_DENIED, "Check of just signed packet (should fail, wrong end)");

	gensec_ntlmssp_state->session_key = data_blob(NULL, 0);

	torture_assert_ntstatus_equal(tctx, 
				      gensec_ntlmssp_check_packet(gensec_security, gensec_security,
								  data.data, data.length, data.data, data.length, &sig),
				      NT_STATUS_NO_USER_SESSION_KEY, "Check of just signed packet without a session key should fail");

	talloc_free(gensec_security);

	torture_assert_ntstatus_ok(tctx, 
		gensec_client_start(mem_ctx, &gensec_security,
				    tctx->ev, lp_gensec_settings(tctx, tctx->lp_ctx)),
		"Failed to start GENSEC for NTLMSSP");

	gensec_set_credentials(gensec_security, cmdline_credentials);

	gensec_want_feature(gensec_security, GENSEC_FEATURE_SIGN);
	gensec_want_feature(gensec_security, GENSEC_FEATURE_SEAL);

	torture_assert_ntstatus_ok(tctx, 
		gensec_start_mech_by_oid(gensec_security, GENSEC_OID_NTLMSSP),
		"GENSEC start mech by oid");

	gensec_ntlmssp_state = (struct gensec_ntlmssp_state *)gensec_security->private_data;

	gensec_ntlmssp_state->session_key = strhex_to_data_blob(tctx, "0102030405e538b0");
	dump_data_pw("NTLMSSP session key: \n", 
		     gensec_ntlmssp_state->session_key.data,  
		     gensec_ntlmssp_state->session_key.length);

	gensec_ntlmssp_state->neg_flags = NTLMSSP_NEGOTIATE_SIGN | NTLMSSP_NEGOTIATE_UNICODE | NTLMSSP_NEGOTIATE_KEY_EXCH;

	torture_assert_ntstatus_ok(tctx,  
		ntlmssp_sign_init(gensec_ntlmssp_state),
		"Failed to sign_init");

	data = strhex_to_data_blob(tctx, "6a43494653");
	gensec_ntlmssp_sign_packet(gensec_security, gensec_security,
			    data.data, data.length, data.data, data.length, &sig);

	expected_sig = strhex_to_data_blob(tctx, "0100000078010900397420fe0e5a0f89");

	dump_data_pw("NTLMSSP calc sig:     ", sig.data, sig.length);
	dump_data_pw("NTLMSSP expected sig: ", expected_sig.data, expected_sig.length);

	torture_assert_int_equal(tctx, sig.length, expected_sig.length, "Wrong sig length");

	torture_assert_mem_equal(tctx, sig.data+8, expected_sig.data+8, sig.length-8,
				   "data mismatch");

	torture_assert_ntstatus_equal(tctx, 
				      gensec_ntlmssp_check_packet(gensec_security, gensec_security,
								  data.data, data.length, data.data, data.length, &sig),
				      NT_STATUS_ACCESS_DENIED, "Check of just signed packet (should fail, wrong end)");

	sig.length /= 2;

	torture_assert_ntstatus_equal(tctx, 
				      gensec_ntlmssp_check_packet(gensec_security, gensec_security,
								  data.data, data.length, data.data, data.length, &sig),
				      NT_STATUS_ACCESS_DENIED, "Check of just signed packet with short sig");

	talloc_free(gensec_security);
	return true;
}

struct torture_suite *torture_ntlmssp(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, 
													   "NTLMSSP");

	torture_suite_add_simple_test(suite, "NTLMSSP self check",
								   torture_ntlmssp_self_check);

	return suite;
}
