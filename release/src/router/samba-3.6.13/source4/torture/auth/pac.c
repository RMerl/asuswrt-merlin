/* 
   Unix SMB/CIFS implementation.

   Validate the krb5 pac generation routines
   
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005

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
#include "system/kerberos.h"
#include "auth/auth.h"
#include "auth/kerberos/kerberos.h"
#include "samba3/samba3.h"
#include "libcli/security/security.h"
#include "torture/torture.h"
#include "auth/auth_sam_reply.h"
#include "param/param.h"
#include "librpc/gen_ndr/ndr_krb5pac.h"

static bool torture_pac_self_check(struct torture_context *tctx)
{
	NTSTATUS nt_status;
	DATA_BLOB tmp_blob;
	struct PAC_DATA *pac_data;
	struct PAC_LOGON_INFO *logon_info;
	union netr_Validation validation;

	/* Generate a nice, arbitary keyblock */
	uint8_t server_bytes[16];
	uint8_t krbtgt_bytes[16];
	krb5_keyblock server_keyblock;
	krb5_keyblock krbtgt_keyblock;
	
	krb5_error_code ret;

	struct smb_krb5_context *smb_krb5_context;

	struct auth_user_info_dc *user_info_dc;
	struct auth_user_info_dc *user_info_dc_out;

	krb5_principal client_principal;
	time_t logon_time = time(NULL);

	TALLOC_CTX *mem_ctx = tctx;

	torture_assert(tctx, 0 == smb_krb5_init_context(mem_ctx, 
							NULL,
							tctx->lp_ctx,
							&smb_krb5_context), 
		       "smb_krb5_init_context");

	generate_random_buffer(server_bytes, 16);
	generate_random_buffer(krbtgt_bytes, 16);

	ret = krb5_keyblock_init(smb_krb5_context->krb5_context,
				 ENCTYPE_ARCFOUR_HMAC,
				 server_bytes, sizeof(server_bytes),
				 &server_keyblock);
	torture_assert(tctx, !ret, talloc_asprintf(tctx, 
						   "(self test) Server Keyblock encoding failed: %s", 
						   smb_get_krb5_error_message(smb_krb5_context->krb5_context, 
									      ret, mem_ctx)));

	ret = krb5_keyblock_init(smb_krb5_context->krb5_context,
				 ENCTYPE_ARCFOUR_HMAC,
				 krbtgt_bytes, sizeof(krbtgt_bytes),
				 &krbtgt_keyblock);
	if (ret) {
		char *err = smb_get_krb5_error_message(smb_krb5_context->krb5_context, 
						       ret, mem_ctx);
	
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);

		torture_fail(tctx, talloc_asprintf(tctx, 
						   "(self test) KRBTGT Keyblock encoding failed: %s", err));
	}

	/* We need an input, and this one requires no underlying database */
	nt_status = auth_anonymous_user_info_dc(mem_ctx, lpcfg_netbios_name(tctx->lp_ctx), &user_info_dc);

	if (!NT_STATUS_IS_OK(nt_status)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		torture_fail(tctx, "auth_anonymous_user_info_dc");
	}

	ret = krb5_parse_name_flags(smb_krb5_context->krb5_context, 
				    user_info_dc->info->account_name,
				    KRB5_PRINCIPAL_PARSE_NO_REALM, 
				    &client_principal);
	if (ret) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		torture_fail(tctx, "krb5_parse_name_flags(norealm)");
	}

	/* OK, go ahead and make a PAC */
	ret = kerberos_create_pac(mem_ctx, 
				  user_info_dc,
				  smb_krb5_context->krb5_context,  
				  &krbtgt_keyblock,
				  &server_keyblock,
				  client_principal,
				  logon_time,
				  &tmp_blob);
	
	if (ret) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, 
				    client_principal);

		torture_fail(tctx, talloc_asprintf(tctx,
						   "(self test) PAC encoding failed: %s", 
						   smb_get_krb5_error_message(smb_krb5_context->krb5_context, 
									      ret, mem_ctx)));
	}

	dump_data(10,tmp_blob.data,tmp_blob.length);

	/* Now check that we can read it back (using full decode and validate) */
	nt_status = kerberos_decode_pac(mem_ctx, 
					&pac_data,
					tmp_blob,
					smb_krb5_context->krb5_context,
					&krbtgt_keyblock,
					&server_keyblock,
					client_principal, 
					logon_time, NULL);

	if (!NT_STATUS_IS_OK(nt_status)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, 
				    client_principal);

		torture_fail(tctx, talloc_asprintf(tctx,
						   "(self test) PAC decoding failed: %s", 
						   nt_errstr(nt_status)));
	}

	/* Now check we can read it back (using Heimdal's pac parsing) */
	nt_status = kerberos_pac_blob_to_user_info_dc(mem_ctx,
						     tmp_blob, 
						     smb_krb5_context->krb5_context,
						      &user_info_dc_out, NULL, NULL);

	/* The user's SID is the first element in the list */
	if (!dom_sid_equal(user_info_dc->sids,
			   user_info_dc_out->sids)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, 
				    client_principal);

		torture_fail(tctx,  
			     talloc_asprintf(tctx, 
					     "(self test) PAC Decode resulted in *different* domain SID: %s != %s",
					     dom_sid_string(mem_ctx, user_info_dc->sids),
					     dom_sid_string(mem_ctx, user_info_dc_out->sids)));
	}
	talloc_free(user_info_dc_out);

	/* Now check that we can read it back (yet again) */
	nt_status = kerberos_pac_logon_info(mem_ctx, 
					    &logon_info,
					    tmp_blob,
					    smb_krb5_context->krb5_context,
					    &krbtgt_keyblock,
					    &server_keyblock,
					    client_principal, 
					    logon_time, 
					    NULL);
	
	if (!NT_STATUS_IS_OK(nt_status)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, 
				    client_principal);
		
		torture_fail(tctx,  
			     talloc_asprintf(tctx, 
					     "(self test) PAC decoding (for logon info) failed: %s", 
					     nt_errstr(nt_status)));
	}
	
	krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
				    &krbtgt_keyblock);
	krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
				    &server_keyblock);
	krb5_free_principal(smb_krb5_context->krb5_context, 
			    client_principal);

	/* And make a server info from the samba-parsed PAC */
	validation.sam3 = &logon_info->info3;
	nt_status = make_user_info_dc_netlogon_validation(mem_ctx,
							 "",
							 3, &validation,
							 &user_info_dc_out);
	if (!NT_STATUS_IS_OK(nt_status)) {
		torture_fail(tctx, 
			     talloc_asprintf(tctx, 
					     "(self test) PAC decoding (make server info) failed: %s", 
					     nt_errstr(nt_status)));
	}
	
	if (!dom_sid_equal(user_info_dc->sids,
			   user_info_dc_out->sids)) {
		torture_fail(tctx,  
			     talloc_asprintf(tctx, 
					     "(self test) PAC Decode resulted in *different* domain SID: %s != %s",
					     dom_sid_string(mem_ctx, user_info_dc->sids),
					     dom_sid_string(mem_ctx, user_info_dc_out->sids)));
	}
	return true;
}


/* This is the PAC generated on my test network, by my test Win2k3 server.
   -- abartlet 2005-07-04
*/

static const uint8_t saved_pac[] = {
	0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xd8, 0x01, 0x00, 0x00, 
	0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
	0x20, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
	0x40, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
	0x58, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x10, 0x08, 0x00, 0xcc, 0xcc, 0xcc, 0xcc,
	0xc8, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x30, 0xdf, 0xa6, 0xcb, 
	0x4f, 0x7d, 0xc5, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0x7f, 0xc0, 0x3c, 0x4e, 0x59, 0x62, 0x73, 0xc5, 0x01, 0xc0, 0x3c, 0x4e, 0x59,
	0x62, 0x73, 0xc5, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x16, 0x00, 0x16, 0x00,
	0x04, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0c, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x14, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x02, 0x00, 0x65, 0x00, 0x00, 0x00, 
	0xed, 0x03, 0x00, 0x00, 0x04, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x02, 0x00,
	0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x16, 0x00, 0x20, 0x00, 0x02, 0x00, 0x16, 0x00, 0x18, 0x00,
	0x24, 0x00, 0x02, 0x00, 0x28, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00,
	0x57, 0x00, 0x32, 0x00, 0x30, 0x00, 0x30, 0x00, 0x33, 0x00, 0x46, 0x00, 0x49, 0x00, 0x4e, 0x00,
	0x41, 0x00, 0x4c, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x02, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
	0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x57, 0x00, 0x32, 0x00,
	0x30, 0x00, 0x30, 0x00, 0x33, 0x00, 0x46, 0x00, 0x49, 0x00, 0x4e, 0x00, 0x41, 0x00, 0x4c, 0x00,
	0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x57, 0x00, 0x49, 0x00,
	0x4e, 0x00, 0x32, 0x00, 0x4b, 0x00, 0x33, 0x00, 0x54, 0x00, 0x48, 0x00, 0x49, 0x00, 0x4e, 0x00,
	0x4b, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
	0x15, 0x00, 0x00, 0x00, 0x11, 0x2f, 0xaf, 0xb5, 0x90, 0x04, 0x1b, 0xec, 0x50, 0x3b, 0xec, 0xdc,
	0x01, 0x00, 0x00, 0x00, 0x30, 0x00, 0x02, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x80, 0x66, 0x28, 0xea, 0x37, 0x80, 0xc5, 0x01, 0x16, 0x00, 0x77, 0x00, 0x32, 0x00, 0x30, 0x00,
	0x30, 0x00, 0x33, 0x00, 0x66, 0x00, 0x69, 0x00, 0x6e, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x24, 0x00,
	0x76, 0xff, 0xff, 0xff, 0x37, 0xd5, 0xb0, 0xf7, 0x24, 0xf0, 0xd6, 0xd4, 0xec, 0x09, 0x86, 0x5a,
	0xa0, 0xe8, 0xc3, 0xa9, 0x00, 0x00, 0x00, 0x00, 0x76, 0xff, 0xff, 0xff, 0xb4, 0xd8, 0xb8, 0xfe,
	0x83, 0xb3, 0x13, 0x3f, 0xfc, 0x5c, 0x41, 0xad, 0xe2, 0x64, 0x83, 0xe0, 0x00, 0x00, 0x00, 0x00
};

/* Check with a known 'well formed' PAC, from my test server */
static bool torture_pac_saved_check(struct torture_context *tctx)
{
	NTSTATUS nt_status;
	enum ndr_err_code ndr_err;
	DATA_BLOB tmp_blob, validate_blob;
	struct PAC_DATA *pac_data, pac_data2;
	struct PAC_LOGON_INFO *logon_info;
	union netr_Validation validation;
	const char *pac_file, *pac_kdc_key, *pac_member_key;
	struct auth_user_info_dc *user_info_dc_out;

	krb5_keyblock server_keyblock;
	krb5_keyblock krbtgt_keyblock, *krbtgt_keyblock_p;
	struct samr_Password *krbtgt_bytes, *krbsrv_bytes;
	
	krb5_error_code ret;
	struct smb_krb5_context *smb_krb5_context;

	const char *principal_string;
	char *broken_principal_string;
	krb5_principal client_principal;
	const char *authtime_string;
	time_t authtime;
	TALLOC_CTX *mem_ctx = tctx;

	torture_assert(tctx, 0 == smb_krb5_init_context(mem_ctx, NULL,
							tctx->lp_ctx,
							&smb_krb5_context),
		       "smb_krb5_init_context");

	pac_kdc_key = torture_setting_string(tctx, "pac_kdc_key", 
					     "B286757148AF7FD252C53603A150B7E7");

	pac_member_key = torture_setting_string(tctx, "pac_member_key", 
						"D217FAEAE5E6B5F95CCC94077AB8A5FC");

	torture_comment(tctx, "Using pac_kdc_key '%s'\n", pac_kdc_key);
	torture_comment(tctx, "Using pac_member_key '%s'\n", pac_member_key);

	/* The krbtgt key in use when the above PAC was generated.
	 * This is an arcfour-hmac-md5 key, extracted with our 'net
	 * samdump' tool. */
	if (*pac_kdc_key == 0) {
		krbtgt_bytes = NULL;
	} else {
		krbtgt_bytes = smbpasswd_gethexpwd(mem_ctx, pac_kdc_key);
		if (!krbtgt_bytes) {
			torture_fail(tctx, "(saved test) Could not interpret krbtgt key");
		}
	}

	krbsrv_bytes = smbpasswd_gethexpwd(mem_ctx, pac_member_key);
	if (!krbsrv_bytes) {
		torture_fail(tctx, "(saved test) Could not interpret krbsrv key");
	}

	ret = krb5_keyblock_init(smb_krb5_context->krb5_context,
				 ENCTYPE_ARCFOUR_HMAC,
				 krbsrv_bytes->hash, sizeof(krbsrv_bytes->hash),
				 &server_keyblock);
	torture_assert(tctx, !ret,
		       talloc_asprintf(tctx,
				       "(saved test) Server Keyblock encoding failed: %s", 
				       smb_get_krb5_error_message(smb_krb5_context->krb5_context, 
								  ret, mem_ctx)));

	if (krbtgt_bytes) {
		ret = krb5_keyblock_init(smb_krb5_context->krb5_context,
					 ENCTYPE_ARCFOUR_HMAC,
					 krbtgt_bytes->hash, sizeof(krbtgt_bytes->hash),
					 &krbtgt_keyblock);
		if (ret) {
			krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
						    &server_keyblock);
			torture_fail(tctx, 
				     talloc_asprintf(tctx, 
						     "(saved test) Server Keyblock encoding failed: %s", 
						     smb_get_krb5_error_message(smb_krb5_context->krb5_context, 
										ret, mem_ctx)));
		}
		krbtgt_keyblock_p = &krbtgt_keyblock;
	} else {
		krbtgt_keyblock_p = NULL;
	}

	pac_file = torture_setting_string(tctx, "pac_file", NULL);
	if (pac_file) {
		tmp_blob.data = (uint8_t *)file_load(pac_file, &tmp_blob.length, 0, mem_ctx);
		torture_comment(tctx, "(saved test) Loaded pac of size %ld from %s\n", (long)tmp_blob.length, pac_file);
	} else {
		tmp_blob = data_blob_talloc(mem_ctx, saved_pac, sizeof(saved_pac));
	}
	
	dump_data(10,tmp_blob.data,tmp_blob.length);

	principal_string = torture_setting_string(tctx, "pac_client_principal", 
						  "w2003final$@WIN2K3.THINKER.LOCAL");

	authtime_string = torture_setting_string(tctx, "pac_authtime", "1120440609");
	authtime = strtoull(authtime_string, NULL, 0);

	ret = krb5_parse_name(smb_krb5_context->krb5_context, principal_string, 
			      &client_principal);
	if (ret) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    krbtgt_keyblock_p);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		torture_fail(tctx,  
			     talloc_asprintf(tctx, 
					     "(saved test) parsing of client principal [%s] failed: %s", 
					     principal_string, 
					     smb_get_krb5_error_message(smb_krb5_context->krb5_context, ret, mem_ctx)));
	}

	/* Decode and verify the signaure on the PAC */
	nt_status = kerberos_decode_pac(mem_ctx, 
					&pac_data,
					tmp_blob,
					smb_krb5_context->krb5_context,
					krbtgt_keyblock_p,
					&server_keyblock, 
					client_principal, authtime, NULL);
	if (!NT_STATUS_IS_OK(nt_status)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    krbtgt_keyblock_p);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, client_principal);
		
		torture_fail(tctx, talloc_asprintf(tctx, 
						   "(saved test) PAC decoding failed: %s", 
						   nt_errstr(nt_status)));
	}

	/* Now check we can read it back (using Heimdal's pac parsing) */
	nt_status = kerberos_pac_blob_to_user_info_dc(mem_ctx,
						     tmp_blob, 
						     smb_krb5_context->krb5_context,
						      &user_info_dc_out,
						      NULL, NULL);

	if (!NT_STATUS_IS_OK(nt_status)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    krbtgt_keyblock_p);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, client_principal);
		
		torture_fail(tctx, talloc_asprintf(tctx, 
						   "(saved test) Heimdal PAC decoding failed: %s", 
						   nt_errstr(nt_status)));
	}

	if (!pac_file &&
	    !dom_sid_equal(dom_sid_parse_talloc(mem_ctx, 
						"S-1-5-21-3048156945-3961193616-3706469200-1005"), 
			   user_info_dc_out->sids)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    krbtgt_keyblock_p);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, client_principal);

		torture_fail(tctx,  
			     talloc_asprintf(tctx, 
					     "(saved test) Heimdal PAC Decode resulted in *different* domain SID: %s != %s",
					     "S-1-5-21-3048156945-3961193616-3706469200-1005", 
					     dom_sid_string(mem_ctx, user_info_dc_out->sids)));
	}

	talloc_free(user_info_dc_out);

	/* Parse the PAC again, for the logon info this time (using Samba4's parsing) */
	nt_status = kerberos_pac_logon_info(mem_ctx, 
					    &logon_info,
					    tmp_blob,
					    smb_krb5_context->krb5_context,
					    krbtgt_keyblock_p,
					    &server_keyblock,
					    client_principal, authtime, NULL);

	if (!NT_STATUS_IS_OK(nt_status)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    krbtgt_keyblock_p);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, client_principal);
	
		torture_fail(tctx,  
			     talloc_asprintf(tctx, 
					     "(saved test) PAC decoding (for logon info) failed: %s", 
					     nt_errstr(nt_status)));
	}

	validation.sam3 = &logon_info->info3;
	nt_status = make_user_info_dc_netlogon_validation(mem_ctx,
							 "",
							 3, &validation,
							 &user_info_dc_out);
	if (!NT_STATUS_IS_OK(nt_status)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    krbtgt_keyblock_p);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, client_principal);

		torture_fail(tctx,  
			     talloc_asprintf(tctx, 
					     "(saved test) PAC decoding (make server info) failed: %s", 
					     nt_errstr(nt_status)));
	}

	if (!pac_file &&
	    !dom_sid_equal(dom_sid_parse_talloc(mem_ctx, 
						"S-1-5-21-3048156945-3961193616-3706469200-1005"), 
			   user_info_dc_out->sids)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    krbtgt_keyblock_p);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, client_principal);

		torture_fail(tctx,  
			     talloc_asprintf(tctx, 
					     "(saved test) PAC Decode resulted in *different* domain SID: %s != %s",
					     "S-1-5-21-3048156945-3961193616-3706469200-1005", 
					     dom_sid_string(mem_ctx, user_info_dc_out->sids)));
	}

	if (krbtgt_bytes == NULL) {
		torture_comment(tctx, "skipping PAC encoding tests as non kdc key\n");
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, client_principal);
		return true;
	}

	ret = kerberos_encode_pac(mem_ctx, 
				  pac_data,
				  smb_krb5_context->krb5_context,
				  krbtgt_keyblock_p,
				  &server_keyblock,
				  &validate_blob);

	if (ret != 0) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    krbtgt_keyblock_p);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, client_principal);

		torture_fail(tctx, "(saved test) PAC push failed");
	}

	dump_data(10, validate_blob.data, validate_blob.length);

	/* compare both the length and the data bytes after a
	 * pull/push cycle.  This ensures we use the exact same
	 * pointer, padding etc algorithms as win2k3.
	 */
	if (tmp_blob.length != validate_blob.length) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    krbtgt_keyblock_p);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, client_principal);

		torture_fail(tctx, 
			     talloc_asprintf(tctx, 
					     "(saved test) PAC push failed: original buffer length[%u] != created buffer length[%u]",
					     (unsigned)tmp_blob.length, (unsigned)validate_blob.length));
	}

	if (memcmp(tmp_blob.data, validate_blob.data, tmp_blob.length) != 0) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    krbtgt_keyblock_p);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, client_principal);

		DEBUG(0, ("tmp_data:\n"));
		dump_data(0, tmp_blob.data, tmp_blob.length);
		DEBUG(0, ("validate_blob:\n"));
		dump_data(0, validate_blob.data, validate_blob.length);

		torture_fail(tctx, talloc_asprintf(tctx, "(saved test) PAC push failed: length[%u] matches, but data does not", (unsigned)tmp_blob.length));
	}

	ret = kerberos_create_pac(mem_ctx, 
				  user_info_dc_out,
				  smb_krb5_context->krb5_context,
				  krbtgt_keyblock_p,
				  &server_keyblock,
				  client_principal, authtime,
				  &validate_blob);

	if (ret != 0) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    krbtgt_keyblock_p);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, client_principal);

		torture_fail(tctx, "(saved test) regnerated PAC create failed");
	}

	dump_data(10,validate_blob.data,validate_blob.length);

	/* compare both the length and the data bytes after a
	 * pull/push cycle.  This ensures we use the exact same
	 * pointer, padding etc algorithms as win2k3.
	 */
	if (tmp_blob.length != validate_blob.length) {
		ndr_err = ndr_pull_struct_blob(&validate_blob, mem_ctx, 
					       &pac_data2,
					       (ndr_pull_flags_fn_t)ndr_pull_PAC_DATA);
		nt_status = ndr_map_error2ntstatus(ndr_err);
		torture_assert_ntstatus_ok(tctx, nt_status, "can't parse the PAC");
		
		NDR_PRINT_DEBUG(PAC_DATA, pac_data);

		NDR_PRINT_DEBUG(PAC_DATA, &pac_data2);

		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    krbtgt_keyblock_p);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, client_principal);

		torture_fail(tctx, talloc_asprintf(tctx, 
						   "(saved test) PAC regenerate failed: original buffer length[%u] != created buffer length[%u]",
						   (unsigned)tmp_blob.length, (unsigned)validate_blob.length));
	}

	if (memcmp(tmp_blob.data, validate_blob.data, tmp_blob.length) != 0) {
		ndr_err = ndr_pull_struct_blob(&validate_blob, mem_ctx, 
					       &pac_data2,
					       (ndr_pull_flags_fn_t)ndr_pull_PAC_DATA);
		nt_status = ndr_map_error2ntstatus(ndr_err);
		torture_assert_ntstatus_ok(tctx, nt_status, "can't parse the PAC");
		
		NDR_PRINT_DEBUG(PAC_DATA, pac_data);

		NDR_PRINT_DEBUG(PAC_DATA, &pac_data2);

		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    krbtgt_keyblock_p);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, client_principal);

		DEBUG(0, ("tmp_data:\n"));
		dump_data(0, tmp_blob.data, tmp_blob.length);
		DEBUG(0, ("validate_blob:\n"));
		dump_data(0, validate_blob.data, validate_blob.length);

		torture_fail(tctx, talloc_asprintf(tctx, 
						   "(saved test) PAC regenerate failed: length[%u] matches, but data does not", (unsigned)tmp_blob.length));
	}

	/* Break the auth time, to ensure we check this vital detail (not setting this caused all the pain in the first place... */
	nt_status = kerberos_decode_pac(mem_ctx, 
					&pac_data,
					tmp_blob,
					smb_krb5_context->krb5_context,
					krbtgt_keyblock_p,
					&server_keyblock,
					client_principal, 
					authtime + 1, NULL);
	if (NT_STATUS_IS_OK(nt_status)) {

		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    krbtgt_keyblock_p);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_principal(smb_krb5_context->krb5_context, client_principal);
		torture_fail(tctx, "(saved test) PAC decoding DID NOT fail on broken auth time (time + 1)");
	}

	/* Break the client principal */
	krb5_free_principal(smb_krb5_context->krb5_context, client_principal);

	broken_principal_string = talloc_strdup(mem_ctx, principal_string);
	broken_principal_string[0]++;

	ret = krb5_parse_name(smb_krb5_context->krb5_context,
			      broken_principal_string, &client_principal);
	if (ret) {

		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    krbtgt_keyblock_p);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		torture_fail(tctx, talloc_asprintf(tctx, 
						   "(saved test) parsing of broken client principal failed: %s", 
						   smb_get_krb5_error_message(smb_krb5_context->krb5_context, ret, mem_ctx)));
	}

	nt_status = kerberos_decode_pac(mem_ctx, 
					&pac_data,
					tmp_blob,
					smb_krb5_context->krb5_context,
					krbtgt_keyblock_p,
					&server_keyblock,
					client_principal, 
					authtime, NULL);
	if (NT_STATUS_IS_OK(nt_status)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    krbtgt_keyblock_p);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		torture_fail(tctx, "(saved test) PAC decoding DID NOT fail on modified principal");
	}

	/* Finally...  Bugger up the signature, and check we fail the checksum */
	tmp_blob.data[tmp_blob.length - 2]++;

	nt_status = kerberos_decode_pac(mem_ctx, 
					&pac_data,
					tmp_blob,
					smb_krb5_context->krb5_context,
					krbtgt_keyblock_p,
					&server_keyblock,
					client_principal, 
					authtime, NULL);
	if (NT_STATUS_IS_OK(nt_status)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    krbtgt_keyblock_p);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		torture_fail(tctx, "(saved test) PAC decoding DID NOT fail on broken checksum");
	}

	krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
				    krbtgt_keyblock_p);
	krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
				    &server_keyblock);
	return true;
}

struct torture_suite *torture_pac(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "pac");

	torture_suite_add_simple_test(suite, "self check", 
				      torture_pac_self_check);
	torture_suite_add_simple_test(suite, "saved check",
				      torture_pac_saved_check);
	return suite;
}
