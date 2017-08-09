/* 
   Unix SMB/CIFS mplementation.
   Helper functions for applying replicated objects
   
   Copyright (C) Stefan Metzmacher <metze@samba.org> 2007
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2009
    
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
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "../lib/crypto/crypto.h"
#include "../libcli/drsuapi/drsuapi.h"
#include "libcli/auth/libcli_auth.h"

WERROR drsuapi_decrypt_attribute_value(TALLOC_CTX *mem_ctx,
				       const DATA_BLOB *gensec_skey,
				       bool rid_crypt,
				       uint32_t rid,
				       DATA_BLOB *in,
				       DATA_BLOB *out)
{
	DATA_BLOB confounder;
	DATA_BLOB enc_buffer;

	MD5_CTX md5;
	uint8_t _enc_key[16];
	DATA_BLOB enc_key;

	DATA_BLOB dec_buffer;

	uint32_t crc32_given;
	uint32_t crc32_calc;
	DATA_BLOB checked_buffer;

	DATA_BLOB plain_buffer;

	/*
	 * users with rid == 0 should not exist
	 */
	if (rid_crypt && rid == 0) {
		return WERR_DS_DRA_INVALID_PARAMETER;
	}

	/* 
	 * the first 16 bytes at the beginning are the confounder
	 * followed by the 4 byte crc32 checksum
	 */
	if (in->length < 20) {
		return WERR_DS_DRA_INVALID_PARAMETER;
	}
	confounder = data_blob_const(in->data, 16);
	enc_buffer = data_blob_const(in->data + 16, in->length - 16);

	/* 
	 * build the encryption key md5 over the session key followed
	 * by the confounder
	 * 
	 * here the gensec session key is used and
	 * not the dcerpc ncacn_ip_tcp "SystemLibraryDTC" key!
	 */
	enc_key = data_blob_const(_enc_key, sizeof(_enc_key));
	MD5Init(&md5);
	MD5Update(&md5, gensec_skey->data, gensec_skey->length);
	MD5Update(&md5, confounder.data, confounder.length);
	MD5Final(enc_key.data, &md5);

	/*
	 * copy the encrypted buffer part and 
	 * decrypt it using the created encryption key using arcfour
	 */
	dec_buffer = data_blob_const(enc_buffer.data, enc_buffer.length);
	arcfour_crypt_blob(dec_buffer.data, dec_buffer.length, &enc_key);

	/* 
	 * the first 4 byte are the crc32 checksum
	 * of the remaining bytes
	 */
	crc32_given = IVAL(dec_buffer.data, 0);
	crc32_calc = crc32_calc_buffer(dec_buffer.data + 4 , dec_buffer.length - 4);
	checked_buffer = data_blob_const(dec_buffer.data + 4, dec_buffer.length - 4);

	plain_buffer = data_blob_talloc(mem_ctx, checked_buffer.data, checked_buffer.length);
	W_ERROR_HAVE_NO_MEMORY(plain_buffer.data);

	if (crc32_given != crc32_calc) {
		return WERR_SEC_E_DECRYPT_FAILURE;
	}
	/*
	 * The following rid_crypt obfuscation isn't session specific
	 * and not really needed here, because we allways know the rid of the
	 * user account.
	 *
	 * some attributes with this 'additional encryption' include
	 * dBCSPwd, unicodePwd, ntPwdHistory, lmPwdHistory
	 *
	 * But for the rest of samba it's easier when we remove this static
	 * obfuscation here
	 */
	if (rid_crypt) {
		uint32_t i, num_hashes;

		if ((checked_buffer.length % 16) != 0) {
			return WERR_DS_DRA_INVALID_PARAMETER;
		}

		num_hashes = plain_buffer.length / 16;
		for (i = 0; i < num_hashes; i++) {
			uint32_t offset = i * 16;
			sam_rid_crypt(rid, checked_buffer.data + offset, plain_buffer.data + offset, 0);
		}
	}

	*out = plain_buffer;
	return WERR_OK;
}

WERROR drsuapi_decrypt_attribute(TALLOC_CTX *mem_ctx, 
				 const DATA_BLOB *gensec_skey,
				 uint32_t rid,
				 struct drsuapi_DsReplicaAttribute *attr)
{
	WERROR status;
	DATA_BLOB *enc_data;
	DATA_BLOB plain_data;
	bool rid_crypt = false;

	if (attr->value_ctr.num_values == 0) {
		return WERR_OK;
	}

	switch (attr->attid) {
	case DRSUAPI_ATTID_dBCSPwd:
	case DRSUAPI_ATTID_unicodePwd:
	case DRSUAPI_ATTID_ntPwdHistory:
	case DRSUAPI_ATTID_lmPwdHistory:
		rid_crypt = true;
		break;
	case DRSUAPI_ATTID_supplementalCredentials:
	case DRSUAPI_ATTID_priorValue:
	case DRSUAPI_ATTID_currentValue:
	case DRSUAPI_ATTID_trustAuthOutgoing:
	case DRSUAPI_ATTID_trustAuthIncoming:
	case DRSUAPI_ATTID_initialAuthOutgoing:
	case DRSUAPI_ATTID_initialAuthIncoming:
		break;
	default:
		return WERR_OK;
	}

	if (attr->value_ctr.num_values > 1) {
		return WERR_DS_DRA_INVALID_PARAMETER;
	}

	if (!attr->value_ctr.values[0].blob) {
		return WERR_DS_DRA_INVALID_PARAMETER;
	}

	enc_data	= attr->value_ctr.values[0].blob;

	status = drsuapi_decrypt_attribute_value(mem_ctx,
						 gensec_skey,
						 rid_crypt,
						 rid,
						 enc_data,
						 &plain_data);
	W_ERROR_NOT_OK_RETURN(status);

	talloc_free(attr->value_ctr.values[0].blob->data);
	*attr->value_ctr.values[0].blob = plain_data;

	return WERR_OK;
}

static WERROR drsuapi_encrypt_attribute_value(TALLOC_CTX *mem_ctx,
					      const DATA_BLOB *gensec_skey,
					      bool rid_crypt,
					      uint32_t rid,
					      DATA_BLOB *in,
					      DATA_BLOB *out)
{
	DATA_BLOB rid_crypt_out = data_blob(NULL, 0);
	DATA_BLOB confounder;

	MD5_CTX md5;
	uint8_t _enc_key[16];
	DATA_BLOB enc_key;

	DATA_BLOB enc_buffer;

	uint32_t crc32_calc;

	/*
	 * users with rid == 0 should not exist
	 */
	if (rid_crypt && rid == 0) {
		return WERR_DS_DRA_INVALID_PARAMETER;
	}

	/*
	 * The following rid_crypt obfuscation isn't session specific
	 * and not really needed here, because we allways know the rid of the
	 * user account.
	 *
	 * some attributes with this 'additional encryption' include
	 * dBCSPwd, unicodePwd, ntPwdHistory, lmPwdHistory
	 *
	 * But for the rest of samba it's easier when we remove this static
	 * obfuscation here
	 */
	if (rid_crypt) {
		uint32_t i, num_hashes;
		rid_crypt_out = data_blob_talloc(mem_ctx, in->data, in->length);
		W_ERROR_HAVE_NO_MEMORY(rid_crypt_out.data);

		if ((rid_crypt_out.length % 16) != 0) {
			return WERR_DS_DRA_INVALID_PARAMETER;
		}

		num_hashes = rid_crypt_out.length / 16;
		for (i = 0; i < num_hashes; i++) {
			uint32_t offset = i * 16;
			sam_rid_crypt(rid, in->data + offset, rid_crypt_out.data + offset, 1);
		}
		in = &rid_crypt_out;
	}

	/* 
	 * the first 16 bytes at the beginning are the confounder
	 * followed by the 4 byte crc32 checksum
	 */

	enc_buffer = data_blob_talloc(mem_ctx, NULL, in->length+20);
	if (!enc_buffer.data) {
		talloc_free(rid_crypt_out.data);
		return WERR_NOMEM;
	};
	
	confounder = data_blob_const(enc_buffer.data, 16);
	generate_random_buffer(confounder.data, confounder.length);

	/* 
	 * build the encryption key md5 over the session key followed
	 * by the confounder
	 * 
	 * here the gensec session key is used and
	 * not the dcerpc ncacn_ip_tcp "SystemLibraryDTC" key!
	 */
	enc_key = data_blob_const(_enc_key, sizeof(_enc_key));
	MD5Init(&md5);
	MD5Update(&md5, gensec_skey->data, gensec_skey->length);
	MD5Update(&md5, confounder.data, confounder.length);
	MD5Final(enc_key.data, &md5);

	/* 
	 * the first 4 byte are the crc32 checksum
	 * of the remaining bytes
	 */
	crc32_calc = crc32_calc_buffer(in->data, in->length);
	SIVAL(enc_buffer.data, 16, crc32_calc);

	/*
	 * copy the plain buffer part and 
	 * encrypt it using the created encryption key using arcfour
	 */
	memcpy(enc_buffer.data+20, in->data, in->length); 
	talloc_free(rid_crypt_out.data);

	arcfour_crypt_blob(enc_buffer.data+16, enc_buffer.length-16, &enc_key);

	*out = enc_buffer;

	return WERR_OK;
}

/*
  encrypt a DRSUAPI attribute ready for sending over the wire
  Only some attribute types are encrypted
 */
WERROR drsuapi_encrypt_attribute(TALLOC_CTX *mem_ctx, 
				 const DATA_BLOB *gensec_skey,
				 uint32_t rid,
				 struct drsuapi_DsReplicaAttribute *attr)
{
	WERROR status;
	DATA_BLOB *plain_data;
	DATA_BLOB enc_data;
	bool rid_crypt = false;

	if (attr->value_ctr.num_values == 0) {
		return WERR_OK;
	}

	switch (attr->attid) {
	case DRSUAPI_ATTID_dBCSPwd:
	case DRSUAPI_ATTID_unicodePwd:
	case DRSUAPI_ATTID_ntPwdHistory:
	case DRSUAPI_ATTID_lmPwdHistory:
		rid_crypt = true;
		break;
	case DRSUAPI_ATTID_supplementalCredentials:
	case DRSUAPI_ATTID_priorValue:
	case DRSUAPI_ATTID_currentValue:
	case DRSUAPI_ATTID_trustAuthOutgoing:
	case DRSUAPI_ATTID_trustAuthIncoming:
	case DRSUAPI_ATTID_initialAuthOutgoing:
	case DRSUAPI_ATTID_initialAuthIncoming:
		break;
	default:
		return WERR_OK;
	}

	if (attr->value_ctr.num_values > 1) {
		return WERR_DS_DRA_INVALID_PARAMETER;
	}

	if (!attr->value_ctr.values[0].blob) {
		return WERR_DS_DRA_INVALID_PARAMETER;
	}

	plain_data	= attr->value_ctr.values[0].blob;

	status = drsuapi_encrypt_attribute_value(mem_ctx,
						 gensec_skey,
						 rid_crypt,
						 rid,
						 plain_data,
						 &enc_data);
	W_ERROR_NOT_OK_RETURN(status);

	talloc_free(attr->value_ctr.values[0].blob->data);
	*attr->value_ctr.values[0].blob = enc_data;

	return WERR_OK;
}

