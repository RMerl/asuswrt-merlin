/*
   Unix SMB/CIFS implementation.

   Extract the user/system database from a remote SamSync server

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2005
   Copyright (C) Guenther Deschner <gd@samba.org> 2008

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
#include "../lib/crypto/crypto.h"
#include "../libcli/auth/libcli_auth.h"
#include "../libcli/samsync/samsync.h"
#include "librpc/gen_ndr/ndr_netlogon.h"

/**
 * Decrypt and extract the user's passwords.
 *
 * The writes decrypted (no longer 'RID encrypted' or arcfour encrypted)
 * passwords back into the structure
 */

static NTSTATUS fix_user(TALLOC_CTX *mem_ctx,
			 struct netlogon_creds_CredentialState *creds,
			 enum netr_SamDatabaseID database_id,
			 struct netr_DELTA_ENUM *delta)
{

	uint32_t rid = delta->delta_id_union.rid;
	struct netr_DELTA_USER *user = delta->delta_union.user;
	struct samr_Password lm_hash;
	struct samr_Password nt_hash;
	unsigned char zero_buf[16];

	memset(zero_buf, '\0', sizeof(zero_buf));

	/* Note that win2000 may send us all zeros
	 * for the hashes if it doesn't
	 * think this channel is secure enough. */
	if (user->lm_password_present) {
		if (memcmp(user->lmpassword.hash, zero_buf, 16) != 0) {
			sam_rid_crypt(rid, user->lmpassword.hash, lm_hash.hash, 0);
		} else {
			memset(lm_hash.hash, '\0', sizeof(lm_hash.hash));
		}
		user->lmpassword = lm_hash;
	}

	if (user->nt_password_present) {
		if (memcmp(user->ntpassword.hash, zero_buf, 16) != 0) {
			sam_rid_crypt(rid, user->ntpassword.hash, nt_hash.hash, 0);
		} else {
			memset(nt_hash.hash, '\0', sizeof(nt_hash.hash));
		}
		user->ntpassword = nt_hash;
	}

	if (user->user_private_info.SensitiveData) {
		DATA_BLOB data;
		struct netr_USER_KEYS keys;
		enum ndr_err_code ndr_err;
		data.data = user->user_private_info.SensitiveData;
		data.length = user->user_private_info.DataLength;
		netlogon_creds_arcfour_crypt(creds, data.data, data.length);
		user->user_private_info.SensitiveData = data.data;
		user->user_private_info.DataLength = data.length;

		ndr_err = ndr_pull_struct_blob(&data, mem_ctx, &keys,
			(ndr_pull_flags_fn_t)ndr_pull_netr_USER_KEYS);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			dump_data(10, data.data, data.length);
			return ndr_map_error2ntstatus(ndr_err);
		}

		/* Note that win2000 may send us all zeros
		 * for the hashes if it doesn't
		 * think this channel is secure enough. */
		if (keys.keys.keys2.lmpassword.length == 16) {
			if (memcmp(keys.keys.keys2.lmpassword.pwd.hash,
					zero_buf, 16) != 0) {
				sam_rid_crypt(rid,
					      keys.keys.keys2.lmpassword.pwd.hash,
					      lm_hash.hash, 0);
			} else {
				memset(lm_hash.hash, '\0', sizeof(lm_hash.hash));
			}
			user->lmpassword = lm_hash;
			user->lm_password_present = true;
		}
		if (keys.keys.keys2.ntpassword.length == 16) {
			if (memcmp(keys.keys.keys2.ntpassword.pwd.hash,
						zero_buf, 16) != 0) {
				sam_rid_crypt(rid,
					      keys.keys.keys2.ntpassword.pwd.hash,
					      nt_hash.hash, 0);
			} else {
				memset(nt_hash.hash, '\0', sizeof(nt_hash.hash));
			}
			user->ntpassword = nt_hash;
			user->nt_password_present = true;
		}
		/* TODO: rid decrypt history fields */
	}
	return NT_STATUS_OK;
}

/**
 * Decrypt and extract the secrets
 * 
 * The writes decrypted secrets back into the structure
 */
static NTSTATUS fix_secret(TALLOC_CTX *mem_ctx,
			   struct netlogon_creds_CredentialState *creds,
			   enum netr_SamDatabaseID database,
			   struct netr_DELTA_ENUM *delta) 
{
	struct netr_DELTA_SECRET *secret = delta->delta_union.secret;
	netlogon_creds_arcfour_crypt(creds, secret->current_cipher.cipher_data, 
			    secret->current_cipher.maxlen); 

	netlogon_creds_arcfour_crypt(creds, secret->old_cipher.cipher_data, 
			    secret->old_cipher.maxlen); 

	return NT_STATUS_OK;
}

/**
 * Fix up the delta, dealing with encryption issues so that the final
 * callback need only do the printing or application logic
 */

NTSTATUS samsync_fix_delta(TALLOC_CTX *mem_ctx,
			   struct netlogon_creds_CredentialState *creds,
			   enum netr_SamDatabaseID database_id,
			   struct netr_DELTA_ENUM *delta)
{
	NTSTATUS status = NT_STATUS_OK;

	switch (delta->delta_type) {
		case NETR_DELTA_USER:

			status = fix_user(mem_ctx,
					  creds,
					  database_id,
					  delta);
			break;
		case NETR_DELTA_SECRET:

			status = fix_secret(mem_ctx,
					    creds,
					    database_id,
					    delta);
			break;
		default:
			break;
	}

	return status;
}

