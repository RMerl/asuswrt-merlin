/*
   Unix SMB/CIFS implementation.

   endpoint server for the backupkey interface

   Copyright (C) Matthieu Patou <mat@samba.org> 2010

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
#include "rpc_server/dcerpc_server.h"
#include "librpc/gen_ndr/ndr_backupkey.h"
#include "dsdb/common/util.h"
#include "dsdb/samdb/samdb.h"
#include "lib/ldb/include/ldb_errors.h"
#include "../lib/util/util_ldb.h"
#include "param/param.h"
#include "auth/session.h"
#include "heimdal/lib/hx509/hx_locl.h"
#include "heimdal/lib/hcrypto/rsa.h"
#include "heimdal/lib/hcrypto/bn.h"
#include "../lib/tsocket/tsocket.h"
#include "../libcli/security/security.h"

#define BACKUPKEY_MIN_VERSION 2
#define BACKUPKEY_MAX_VERSION 3

static const unsigned rsa_with_var_num[] = { 1, 2, 840, 113549, 1, 1, 1 };
/* Equivalent to asn1_oid_id_pkcs1_rsaEncryption*/
static const AlgorithmIdentifier _hx509_signature_rsa_with_var_num = {
	{ 7, discard_const_p(unsigned, rsa_with_var_num) }, NULL
};

static NTSTATUS set_lsa_secret(TALLOC_CTX *mem_ctx,
			       struct ldb_context *ldb,
			       const char *name,
			       const DATA_BLOB *secret)
{
	struct ldb_message *msg;
	struct ldb_result *res;
	struct ldb_dn *domain_dn;
	struct ldb_dn *system_dn;
	struct ldb_val val;
	int ret;
	char *name2;
	struct timeval now = timeval_current();
	NTTIME nt_now = timeval_to_nttime(&now);
	const char *attrs[] = {
		NULL
	};

	domain_dn = ldb_get_default_basedn(ldb);
	if (!domain_dn) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/*
	 * This function is a lot like dcesrv_lsa_CreateSecret
	 * in the rpc_server/lsa directory
	 * The reason why we duplicate the effort here is that:
	 * * we want to keep the former function static
	 * * we want to avoid the burden of doing LSA calls
	 *   when we can just manipulate the secrets directly
	 * * taillor the function to the particular needs of backup protocol
	 */

	system_dn = samdb_search_dn(ldb, msg, domain_dn, "(&(objectClass=container)(cn=System))");
	if (system_dn == NULL) {
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
	}

	name2 = talloc_asprintf(msg, "%s Secret", name);
	if (name2 == NULL) {
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
	}

	ret = ldb_search(ldb, mem_ctx, &res, system_dn, LDB_SCOPE_SUBTREE, attrs,
			   "(&(cn=%s)(objectclass=secret))",
			   ldb_binary_encode_string(mem_ctx, name2));

	if (ret != LDB_SUCCESS ||  res->count != 0 ) {
		DEBUG(2, ("Secret %s already exists !\n", name2));
		talloc_free(msg);
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	/*
	 * We don't care about previous value as we are
	 * here only if the key didn't exists before
	 */

	msg->dn = ldb_dn_copy(mem_ctx, system_dn);
	if (msg->dn == NULL) {
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
	}
	if (!ldb_dn_add_child_fmt(msg->dn, "cn=%s", name2)) {
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
	}

	ret = ldb_msg_add_string(msg, "cn", name2);
	if (ret != LDB_SUCCESS) {
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
	}
	ret = ldb_msg_add_string(msg, "objectClass", "secret");
	if (ret != LDB_SUCCESS) {
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
	}
	ret = samdb_msg_add_uint64(ldb, mem_ctx, msg, "priorSetTime", nt_now);
	if (ret != LDB_SUCCESS) {
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
	}
	val.data = secret->data;
	val.length = secret->length;
	ret = ldb_msg_add_value(msg, "currentValue", &val, NULL);
	if (ret != LDB_SUCCESS) {
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
	}
	ret = samdb_msg_add_uint64(ldb, mem_ctx, msg, "lastSetTime", nt_now);
	if (ret != LDB_SUCCESS) {
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
	}

	/*
	 * create the secret with DSDB_MODIFY_RELAX
	 * otherwise dsdb/samdb/ldb_modules/objectclass.c forbid
	 * the create of LSA secret object
	 */
	ret = dsdb_add(ldb, msg, DSDB_MODIFY_RELAX);
	if (ret != LDB_SUCCESS) {
		DEBUG(2,("Failed to create secret record %s: %s\n",
			ldb_dn_get_linearized(msg->dn),
			ldb_errstring(ldb)));
		talloc_free(msg);
		return NT_STATUS_ACCESS_DENIED;
	}

	talloc_free(msg);
	return NT_STATUS_OK;
}

/* This function is pretty much like dcesrv_lsa_QuerySecret */
static NTSTATUS get_lsa_secret(TALLOC_CTX *mem_ctx,
			       struct ldb_context *ldb,
			       const char *name,
			       DATA_BLOB *secret)
{
	TALLOC_CTX *tmp_mem;
	struct ldb_result *res;
	struct ldb_dn *domain_dn;
	struct ldb_dn *system_dn;
	const struct ldb_val *val;
	uint8_t *data;
	const char *attrs[] = {
		"currentValue",
		NULL
	};
	int ret;

	secret->data = NULL;
	secret->length = 0;

	domain_dn = ldb_get_default_basedn(ldb);
	if (!domain_dn) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	tmp_mem = talloc_new(mem_ctx);
	if (tmp_mem == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	system_dn = samdb_search_dn(ldb, tmp_mem, domain_dn, "(&(objectClass=container)(cn=System))");
	if (system_dn == NULL) {
		talloc_free(tmp_mem);
		return NT_STATUS_NO_MEMORY;
	}

	ret = ldb_search(ldb, mem_ctx, &res, system_dn, LDB_SCOPE_SUBTREE, attrs,
			   "(&(cn=%s Secret)(objectclass=secret))",
			   ldb_binary_encode_string(tmp_mem, name));

	if (ret != LDB_SUCCESS || res->count == 0) {
		talloc_free(tmp_mem);
		/*
		 * Important NOT to use NT_STATUS_OBJECT_NAME_NOT_FOUND
		 * as this return value is used to detect the case
		 * when we have the secret but without the currentValue
		 * (case RODC)
		 */
		return NT_STATUS_RESOURCE_NAME_NOT_FOUND;
	}

	if (res->count > 1) {
		DEBUG(2, ("Secret %s collision\n", name));
		talloc_free(tmp_mem);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	val = ldb_msg_find_ldb_val(res->msgs[0], "currentValue");
	if (val == NULL) {
		/*
		 * The secret object is here but we don't have the secret value
		 * The most common case is a RODC
		 */
		talloc_free(tmp_mem);
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	data = val->data;
	secret->data = talloc_move(mem_ctx, &data);
	secret->length = val->length;

	talloc_free(tmp_mem);
	return NT_STATUS_OK;
}

static DATA_BLOB *reverse_and_get_blob(TALLOC_CTX *mem_ctx, BIGNUM *bn)
{
	DATA_BLOB blob;
	DATA_BLOB *rev = talloc(mem_ctx, DATA_BLOB);
	uint32_t i;

	blob.length = BN_num_bytes(bn);
	blob.data = talloc_array(mem_ctx, uint8_t, blob.length);

	if (blob.data == NULL) {
		return NULL;
	}

	BN_bn2bin(bn, blob.data);

	rev->data = talloc_array(mem_ctx, uint8_t, blob.length);
	if (rev->data == NULL) {
		return NULL;
	}

	for(i=0; i < blob.length; i++) {
		rev->data[i] = blob.data[blob.length - i -1];
	}
	rev->length = blob.length;
	talloc_free(blob.data);
	return rev;
}

static BIGNUM *reverse_and_get_bignum(TALLOC_CTX *mem_ctx, DATA_BLOB *blob)
{
	BIGNUM *ret;
	DATA_BLOB rev;
	uint32_t i;

	rev.data = talloc_array(mem_ctx, uint8_t, blob->length);
	if (rev.data == NULL) {
		return NULL;
	}

	for(i=0; i < blob->length; i++) {
		rev.data[i] = blob->data[blob->length - i -1];
	}
	rev.length = blob->length;

	ret = BN_bin2bn(rev.data, rev.length, NULL);
	talloc_free(rev.data);

	return ret;
}

static NTSTATUS get_pk_from_raw_keypair_params(TALLOC_CTX *ctx,
				struct bkrp_exported_RSA_key_pair *keypair,
				hx509_private_key *pk)
{
	hx509_context hctx;
	RSA *rsa;
	struct hx509_private_key_ops *ops;

	hx509_context_init(&hctx);
	ops = hx509_find_private_alg(&_hx509_signature_rsa_with_var_num.algorithm);
	if (ops == NULL) {
		DEBUG(2, ("Not supported algorithm\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	if (hx509_private_key_init(pk, ops, NULL) != 0) {
		hx509_context_free(&hctx);
		return NT_STATUS_NO_MEMORY;
	}

	rsa = RSA_new();
	if (rsa ==NULL) {
		hx509_context_free(&hctx);
		return NT_STATUS_INVALID_PARAMETER;
	}

	rsa->n = reverse_and_get_bignum(ctx, &(keypair->modulus));
	if (rsa->n == NULL) {
		RSA_free(rsa);
		hx509_context_free(&hctx);
		return NT_STATUS_INVALID_PARAMETER;
	}
	rsa->d = reverse_and_get_bignum(ctx, &(keypair->private_exponent));
	if (rsa->d == NULL) {
		RSA_free(rsa);
		hx509_context_free(&hctx);
		return NT_STATUS_INVALID_PARAMETER;
	}
	rsa->p = reverse_and_get_bignum(ctx, &(keypair->prime1));
	if (rsa->p == NULL) {
		RSA_free(rsa);
		hx509_context_free(&hctx);
		return NT_STATUS_INVALID_PARAMETER;
	}
	rsa->q = reverse_and_get_bignum(ctx, &(keypair->prime2));
	if (rsa->q == NULL) {
		RSA_free(rsa);
		hx509_context_free(&hctx);
		return NT_STATUS_INVALID_PARAMETER;
	}
	rsa->dmp1 = reverse_and_get_bignum(ctx, &(keypair->exponent1));
	if (rsa->dmp1 == NULL) {
		RSA_free(rsa);
		hx509_context_free(&hctx);
		return NT_STATUS_INVALID_PARAMETER;
	}
	rsa->dmq1 = reverse_and_get_bignum(ctx, &(keypair->exponent2));
	if (rsa->dmq1 == NULL) {
		RSA_free(rsa);
		hx509_context_free(&hctx);
		return NT_STATUS_INVALID_PARAMETER;
	}
	rsa->iqmp = reverse_and_get_bignum(ctx, &(keypair->coefficient));
	if (rsa->iqmp == NULL) {
		RSA_free(rsa);
		hx509_context_free(&hctx);
		return NT_STATUS_INVALID_PARAMETER;
	}
	rsa->e = reverse_and_get_bignum(ctx, &(keypair->public_exponent));
	if (rsa->e == NULL) {
		RSA_free(rsa);
		hx509_context_free(&hctx);
		return NT_STATUS_INVALID_PARAMETER;
	}

	hx509_private_key_assign_rsa(*pk, rsa);

	hx509_context_free(&hctx);
	return NT_STATUS_OK;
}

static WERROR get_and_verify_access_check(TALLOC_CTX *sub_ctx,
					  uint32_t version,
					  uint8_t *key_and_iv,
					  uint8_t *access_check,
					  uint32_t access_check_len,
					  struct dom_sid **access_sid)
{
	heim_octet_string iv;
	heim_octet_string access_check_os;
	hx509_crypto crypto;

	DATA_BLOB blob_us;
	uint32_t key_len;
	uint32_t iv_len;
	int res;
	enum ndr_err_code ndr_err;
	hx509_context hctx;

	/* This one should not be freed */
	const AlgorithmIdentifier *alg;

	*access_sid = NULL;
	switch (version) {
	case 2:
		key_len = 24;
		iv_len = 8;
		alg = hx509_crypto_des_rsdi_ede3_cbc();
		break;

	case 3:
		key_len = 32;
		iv_len = 16;
		alg =hx509_crypto_aes256_cbc();
		break;

	default:
		return WERR_INVALID_DATA;
	}

	hx509_context_init(&hctx);
	res = hx509_crypto_init(hctx, NULL,
				&(alg->algorithm),
				&crypto);
	hx509_context_free(&hctx);

	if (res != 0) {
		return WERR_INVALID_DATA;
	}

	res = hx509_crypto_set_key_data(crypto, key_and_iv, key_len);

	iv.data = talloc_memdup(sub_ctx, key_len + key_and_iv, iv_len);
	iv.length = iv_len;

	if (res != 0) {
		hx509_crypto_destroy(crypto);
		return WERR_INVALID_DATA;
	}

	hx509_crypto_set_padding(crypto, HX509_CRYPTO_PADDING_NONE);
	res = hx509_crypto_decrypt(crypto,
		access_check,
		access_check_len,
		&iv,
		&access_check_os);

	if (res != 0) {
		hx509_crypto_destroy(crypto);
		return WERR_INVALID_DATA;
	}

	blob_us.data = access_check_os.data;
	blob_us.length = access_check_os.length;

	hx509_crypto_destroy(crypto);

	if (version == 2) {
		uint32_t hash_size = 20;
		uint8_t hash[hash_size];
		struct sha sctx;
		struct bkrp_access_check_v2 uncrypted_accesscheckv2;

		ndr_err = ndr_pull_struct_blob(&blob_us, sub_ctx, &uncrypted_accesscheckv2,
					(ndr_pull_flags_fn_t)ndr_pull_bkrp_access_check_v2);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			/* Unable to unmarshall */
			der_free_octet_string(&access_check_os);
			return WERR_INVALID_DATA;
		}
		if (uncrypted_accesscheckv2.magic != 0x1) {
			/* wrong magic */
			der_free_octet_string(&access_check_os);
			return WERR_INVALID_DATA;
		}

		SHA1_Init(&sctx);
		SHA1_Update(&sctx, blob_us.data, blob_us.length - hash_size);
		SHA1_Final(hash, &sctx);
		der_free_octet_string(&access_check_os);
		/*
		 * We free it after the sha1 calculation because blob.data
		 * point to the same area
		 */

		if (memcmp(hash, uncrypted_accesscheckv2.hash, hash_size) != 0) {
			DEBUG(2, ("Wrong hash value in the access check in backup key remote protocol\n"));
			return WERR_INVALID_DATA;
		}
		*access_sid = dom_sid_dup(sub_ctx, &(uncrypted_accesscheckv2.sid));
		if (*access_sid == NULL) {
			return WERR_NOMEM;
		}
		return WERR_OK;
	}

	if (version == 3) {
		uint32_t hash_size = 64;
		uint8_t hash[hash_size];
		struct hc_sha512state sctx;
		struct bkrp_access_check_v3 uncrypted_accesscheckv3;

		ndr_err = ndr_pull_struct_blob(&blob_us, sub_ctx, &uncrypted_accesscheckv3,
					(ndr_pull_flags_fn_t)ndr_pull_bkrp_access_check_v3);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			/* Unable to unmarshall */
			der_free_octet_string(&access_check_os);
			return WERR_INVALID_DATA;
		}
		if (uncrypted_accesscheckv3.magic != 0x1) {
			/* wrong magic */
			der_free_octet_string(&access_check_os);
			return WERR_INVALID_DATA;
		}

		SHA512_Init(&sctx);
		SHA512_Update(&sctx, blob_us.data, blob_us.length - hash_size);
		SHA512_Final(hash, &sctx);
		der_free_octet_string(&access_check_os);
		/*
		 * We free it after the sha1 calculation because blob.data
		 * point to the same area
		 */

		if (memcmp(hash, uncrypted_accesscheckv3.hash, hash_size) != 0) {
			DEBUG(2, ("Wrong hash value in the access check in backup key remote protocol\n"));
			return WERR_INVALID_DATA;
		}
		*access_sid = dom_sid_dup(sub_ctx, &(uncrypted_accesscheckv3.sid));
		if (*access_sid == NULL) {
			return WERR_NOMEM;
		}
		return WERR_OK;
	}

	/* Never reached normally as we filtered at the switch / case level */
	return WERR_INVALID_DATA;
}

static WERROR bkrp_do_uncrypt_client_wrap_key(struct dcesrv_call_state *dce_call,
					      TALLOC_CTX *mem_ctx,
					      struct bkrp_BackupKey *r,
					      struct ldb_context *ldb_ctx)
{
	struct bkrp_client_side_wrapped uncrypt_request;
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;
	char *guid_string;
	char *cert_secret_name;
	DATA_BLOB secret;
	DATA_BLOB *uncrypted;
	NTSTATUS status;

	blob.data = r->in.data_in;
	blob.length = r->in.data_in_len;

	if (r->in.data_in_len == 0 || r->in.data_in == NULL) {
		return WERR_INVALID_PARAM;
	}

	ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, &uncrypt_request,
				       (ndr_pull_flags_fn_t)ndr_pull_bkrp_client_side_wrapped);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return WERR_INVALID_PARAM;
	}

	if (uncrypt_request.version < BACKUPKEY_MIN_VERSION) {
		return WERR_INVALID_PARAMETER;
	}

	if (uncrypt_request.version > BACKUPKEY_MAX_VERSION) {
		return WERR_INVALID_PARAMETER;
	}

	guid_string = GUID_string(mem_ctx, &uncrypt_request.guid);
	if (guid_string == NULL) {
		return WERR_NOMEM;
	}

	cert_secret_name = talloc_asprintf(mem_ctx,
					   "BCKUPKEY_%s",
					   guid_string);
	if (cert_secret_name == NULL) {
		return WERR_NOMEM;
	}

	status = get_lsa_secret(mem_ctx,
				ldb_ctx,
				cert_secret_name,
				&secret);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("Error while fetching secret %s\n", cert_secret_name));
		if (NT_STATUS_EQUAL(status,NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
			/* we do not have the real secret attribute */
			return WERR_INVALID_PARAMETER;
		} else {
			return WERR_FILE_NOT_FOUND;
		}
	}

	if (secret.length != 0) {
		hx509_context hctx;
		struct bkrp_exported_RSA_key_pair keypair;
		hx509_private_key pk;
		uint32_t i, res;
		struct dom_sid *access_sid = NULL;
		heim_octet_string reversed_secret;
		heim_octet_string uncrypted_secret;
		AlgorithmIdentifier alg;
		struct dom_sid *caller_sid;
		DATA_BLOB blob_us;
		WERROR werr;

		ndr_err = ndr_pull_struct_blob(&secret, mem_ctx, &keypair, (ndr_pull_flags_fn_t)ndr_pull_bkrp_exported_RSA_key_pair);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			DEBUG(2, ("Unable to parse the ndr encoded cert in key %s\n", cert_secret_name));
			return WERR_FILE_NOT_FOUND;
		}

		status = get_pk_from_raw_keypair_params(mem_ctx, &keypair, &pk);
		if (!NT_STATUS_IS_OK(status)) {
			return WERR_INTERNAL_ERROR;
		}

		reversed_secret.data = talloc_array(mem_ctx, uint8_t,
						    uncrypt_request.encrypted_secret_len);
		if (reversed_secret.data == NULL) {
			hx509_private_key_free(&pk);
			return WERR_NOMEM;
		}

		/* The secret has to be reversed ... */
		for(i=0; i< uncrypt_request.encrypted_secret_len; i++) {
			uint8_t *reversed = (uint8_t *)reversed_secret.data;
			uint8_t *uncrypt = uncrypt_request.encrypted_secret;
			reversed[i] = uncrypt[uncrypt_request.encrypted_secret_len - 1 - i];
		}
		reversed_secret.length = uncrypt_request.encrypted_secret_len;

		/*
		 * Let's try to decrypt the secret now that
		 * we have the private key ...
		 */
		hx509_context_init(&hctx);
		res = hx509_private_key_private_decrypt(hctx, &reversed_secret,
							 &alg.algorithm, pk,
							 &uncrypted_secret);
		hx509_context_free(&hctx);
		hx509_private_key_free(&pk);
		if (res != 0) {
			/* We are not able to decrypt the secret, looks like something is wrong */
			return WERR_INVALID_DATA;
		}
		blob_us.data = uncrypted_secret.data;
		blob_us.length = uncrypted_secret.length;

		if (uncrypt_request.version == 2) {
			struct bkrp_encrypted_secret_v2 uncrypted_secretv2;

			ndr_err = ndr_pull_struct_blob(&blob_us, mem_ctx, &uncrypted_secretv2,
					(ndr_pull_flags_fn_t)ndr_pull_bkrp_encrypted_secret_v2);
			der_free_octet_string(&uncrypted_secret);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				/* Unable to unmarshall */
				return WERR_INVALID_DATA;
			}
			if (uncrypted_secretv2.magic != 0x20) {
				/* wrong magic */
				return WERR_INVALID_DATA;
			}

			werr = get_and_verify_access_check(mem_ctx, 2,
							   uncrypted_secretv2.payload_key,
							   uncrypt_request.access_check,
							   uncrypt_request.access_check_len,
							   &access_sid);
			if (!W_ERROR_IS_OK(werr)) {
				return werr;
			}
			uncrypted = talloc(mem_ctx, DATA_BLOB);
			if (uncrypted == NULL) {
				return WERR_INVALID_DATA;
			}

			uncrypted->data = uncrypted_secretv2.secret;
			uncrypted->length = uncrypted_secretv2.secret_len;
		}
		if (uncrypt_request.version == 3) {
			struct bkrp_encrypted_secret_v3 uncrypted_secretv3;

			ndr_err = ndr_pull_struct_blob(&blob_us, mem_ctx, &uncrypted_secretv3,
					(ndr_pull_flags_fn_t)ndr_pull_bkrp_encrypted_secret_v3);

			der_free_octet_string(&uncrypted_secret);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				/* Unable to unmarshall */
				return WERR_INVALID_DATA;
			}

			if (uncrypted_secretv3.magic1 != 0x30  ||
			    uncrypted_secretv3.magic2 != 0x6610 ||
			    uncrypted_secretv3.magic3 != 0x800e) {
				/* wrong magic */
				return WERR_INVALID_DATA;
			}

			werr = get_and_verify_access_check(mem_ctx, 3,
							   uncrypted_secretv3.payload_key,
							   uncrypt_request.access_check,
							   uncrypt_request.access_check_len,
							   &access_sid);
			if (!W_ERROR_IS_OK(werr)) {
				return werr;
			}

			uncrypted = talloc(mem_ctx, DATA_BLOB);
			if (uncrypted == NULL) {
				return WERR_INVALID_DATA;
			}

			uncrypted->data = uncrypted_secretv3.secret;
			uncrypted->length = uncrypted_secretv3.secret_len;
		}

		caller_sid = &dce_call->conn->auth_state.session_info->security_token->sids[PRIMARY_USER_SID_INDEX];

		if (!dom_sid_equal(caller_sid, access_sid)) {
			talloc_free(uncrypted);
			return WERR_INVALID_ACCESS;
		}

		/*
		 * Yeah if we are here all looks pretty good:
		 * - hash is ok
		 * - user sid is the same as the one in access check
		 * - we were able to decrypt the whole stuff
		 */
	}

	if (uncrypted->data == NULL) {
		return WERR_INVALID_DATA;
	}

	/* There is a magic value a the beginning of the data
	 * we can use an adhoc structure but as the
	 * parent structure is just an array of bytes it a lot of work
	 * work just prepending 4 bytes
	 */
	*(r->out.data_out) = talloc_zero_array(mem_ctx, uint8_t, uncrypted->length + 4);
	W_ERROR_HAVE_NO_MEMORY(*(r->out.data_out));
	memcpy(4+*(r->out.data_out), uncrypted->data, uncrypted->length);
	*(r->out.data_out_len) = uncrypted->length + 4;

	return WERR_OK;
}

static WERROR create_heimdal_rsa_key(TALLOC_CTX *ctx, hx509_context *hctx,
				     hx509_private_key *pk, RSA **_rsa)
{
	BIGNUM *pub_expo;
	RSA *rsa;
	int ret;
	uint8_t *p0, *p;
	size_t len;
	int bits = 2048;

	*_rsa = NULL;

	pub_expo = BN_new();
	if(pub_expo == NULL) {
		return WERR_INTERNAL_ERROR;
	}

	/* set the public expo to 65537 like everyone */
	BN_set_word(pub_expo, 0x10001);

	rsa = RSA_new();
	if(rsa == NULL) {
		BN_free(pub_expo);
		return WERR_INTERNAL_ERROR;
	}

	ret = RSA_generate_key_ex(rsa, bits, pub_expo, NULL);
	if(ret != 1) {
		RSA_free(rsa);
		BN_free(pub_expo);
		return WERR_INTERNAL_ERROR;
	}
	BN_free(pub_expo);

	len = i2d_RSAPrivateKey(rsa, NULL);
	if (len < 1) {
		RSA_free(rsa);
		return WERR_INTERNAL_ERROR;
	}

	p0 = p = talloc_array(ctx, uint8_t, len);
	if (p == NULL) {
		RSA_free(rsa);
		return WERR_INTERNAL_ERROR;
	}

	len = i2d_RSAPrivateKey(rsa, &p);
	if (len < 1) {
		RSA_free(rsa);
		talloc_free(p0);
		return WERR_INTERNAL_ERROR;
	}

	/*
	 * To dump the key we can use :
	 * rk_dumpdata("h5lkey", p0, len);
	 */
	ret = hx509_parse_private_key(*hctx, &_hx509_signature_rsa_with_var_num ,
				       p0, len, HX509_KEY_FORMAT_DER, pk);
	memset(p0, 0, len);
	talloc_free(p0);
	if (ret !=0) {
		RSA_free(rsa);
		return WERR_INTERNAL_ERROR;
	}

	*_rsa = rsa;
	return WERR_OK;
}

static WERROR self_sign_cert(TALLOC_CTX *ctx, hx509_context *hctx, hx509_request *req,
				time_t lifetime, hx509_private_key *private_key,
				hx509_cert *cert, DATA_BLOB *guidblob)
{
	SubjectPublicKeyInfo spki;
	hx509_name subject = NULL;
	hx509_ca_tbs tbs;
	struct heim_bit_string uniqueid;
	int ret;

	uniqueid.data = talloc_memdup(ctx, guidblob->data, guidblob->length);
	/* uniqueid is a bit string in which each byte represent 1 bit (1 or 0)
	 * so as 1 byte is 8 bits we need to provision 8 times more space as in the
	 * blob
	 */
	uniqueid.length = 8 * guidblob->length;

	memset(&spki, 0, sizeof(spki));

	ret = hx509_request_get_name(*hctx, *req, &subject);
	if (ret !=0) {
		talloc_free(uniqueid.data);
		return WERR_INTERNAL_ERROR;
	}
	ret = hx509_request_get_SubjectPublicKeyInfo(*hctx, *req, &spki);
	if (ret !=0) {
		talloc_free(uniqueid.data);
		hx509_name_free(&subject);
		return WERR_INTERNAL_ERROR;
	}

	ret = hx509_ca_tbs_init(*hctx, &tbs);
	if (ret !=0) {
		talloc_free(uniqueid.data);
		hx509_name_free(&subject);
		free_SubjectPublicKeyInfo(&spki);
		return WERR_INTERNAL_ERROR;
	}

	ret = hx509_ca_tbs_set_spki(*hctx, tbs, &spki);
	if (ret !=0) {
		talloc_free(uniqueid.data);
		hx509_name_free(&subject);
		free_SubjectPublicKeyInfo(&spki);
		return WERR_INTERNAL_ERROR;
	}
	ret = hx509_ca_tbs_set_subject(*hctx, tbs, subject);
	if (ret !=0) {
		talloc_free(uniqueid.data);
		hx509_name_free(&subject);
		free_SubjectPublicKeyInfo(&spki);
		hx509_ca_tbs_free(&tbs);
		return WERR_INTERNAL_ERROR;
	}
	ret = hx509_ca_tbs_set_ca(*hctx, tbs, 1);
	if (ret !=0) {
		talloc_free(uniqueid.data);
		hx509_name_free(&subject);
		free_SubjectPublicKeyInfo(&spki);
		hx509_ca_tbs_free(&tbs);
		return WERR_INTERNAL_ERROR;
	}
	ret = hx509_ca_tbs_set_notAfter_lifetime(*hctx, tbs, lifetime);
	if (ret !=0) {
		talloc_free(uniqueid.data);
		hx509_name_free(&subject);
		free_SubjectPublicKeyInfo(&spki);
		hx509_ca_tbs_free(&tbs);
		return WERR_INTERNAL_ERROR;
	}
	ret = hx509_ca_tbs_set_unique(*hctx, tbs, &uniqueid, &uniqueid);
	if (ret !=0) {
		talloc_free(uniqueid.data);
		hx509_name_free(&subject);
		free_SubjectPublicKeyInfo(&spki);
		hx509_ca_tbs_free(&tbs);
		return WERR_INTERNAL_ERROR;
	}
	ret = hx509_ca_sign_self(*hctx, tbs, *private_key, cert);
	if (ret !=0) {
		talloc_free(uniqueid.data);
		hx509_name_free(&subject);
		free_SubjectPublicKeyInfo(&spki);
		hx509_ca_tbs_free(&tbs);
		return WERR_INTERNAL_ERROR;
	}
	hx509_name_free(&subject);
	free_SubjectPublicKeyInfo(&spki);
	hx509_ca_tbs_free(&tbs);

	return WERR_OK;
}

static WERROR create_req(TALLOC_CTX *ctx, hx509_context *hctx, hx509_request *req,
			 hx509_private_key *signer,RSA **rsa, const char *dn)
{
	int ret;
	SubjectPublicKeyInfo key;

	hx509_name name;
	WERROR w_err;

	w_err = create_heimdal_rsa_key(ctx, hctx, signer, rsa);
	if (!W_ERROR_IS_OK(w_err)) {
		return w_err;
	}

	hx509_request_init(*hctx, req);
	ret = hx509_parse_name(*hctx, dn, &name);
	if (ret != 0) {
		RSA_free(*rsa);
		hx509_private_key_free(signer);
		hx509_request_free(req);
		hx509_name_free(&name);
		return WERR_INTERNAL_ERROR;
	}

	ret = hx509_request_set_name(*hctx, *req, name);
	if (ret != 0) {
		RSA_free(*rsa);
		hx509_private_key_free(signer);
		hx509_request_free(req);
		hx509_name_free(&name);
		return WERR_INTERNAL_ERROR;
	}
	hx509_name_free(&name);

	ret = hx509_private_key2SPKI(*hctx, *signer, &key);
	if (ret != 0) {
		RSA_free(*rsa);
		hx509_private_key_free(signer);
		hx509_request_free(req);
		return WERR_INTERNAL_ERROR;
	}
	ret = hx509_request_set_SubjectPublicKeyInfo(*hctx, *req, &key);
	if (ret != 0) {
		RSA_free(*rsa);
		hx509_private_key_free(signer);
		free_SubjectPublicKeyInfo(&key);
		hx509_request_free(req);
		return WERR_INTERNAL_ERROR;
	}

	free_SubjectPublicKeyInfo(&key);

	return WERR_OK;
}

/* Return an error when we fail to generate a certificate */
static WERROR generate_bkrp_cert(TALLOC_CTX *ctx, struct dcesrv_call_state *dce_call, struct ldb_context *ldb_ctx, const char *dn)
{

	struct heim_octet_string data;
	WERROR w_err;
	RSA *rsa;
	hx509_context hctx;
	hx509_private_key pk;
	hx509_request req;
	hx509_cert cert;
	DATA_BLOB blob;
	DATA_BLOB blobkeypair;
	DATA_BLOB *tmp;
	int ret;
	bool ok = true;
	struct GUID guid = GUID_random();
	NTSTATUS status;
	char *secret_name;
	struct bkrp_exported_RSA_key_pair keypair;
	enum ndr_err_code ndr_err;
	uint32_t nb_days_validity = 365;

	DEBUG(6, ("Trying to generate a certificate\n"));
	hx509_context_init(&hctx);
	w_err = create_req(ctx, &hctx, &req, &pk, &rsa, dn);
	if (!W_ERROR_IS_OK(w_err)) {
		hx509_context_free(&hctx);
		return w_err;
	}

	status = GUID_to_ndr_blob(&guid, ctx, &blob);
	if (!NT_STATUS_IS_OK(status)) {
		hx509_context_free(&hctx);
		hx509_private_key_free(&pk);
		RSA_free(rsa);
		return WERR_INVALID_DATA;
	}

	w_err = self_sign_cert(ctx, &hctx, &req, nb_days_validity, &pk, &cert, &blob);
	if (!W_ERROR_IS_OK(w_err)) {
		hx509_private_key_free(&pk);
		hx509_context_free(&hctx);
		return WERR_INVALID_DATA;
	}

	ret = hx509_cert_binary(hctx, cert, &data);
	if (ret !=0) {
		hx509_cert_free(cert);
		hx509_private_key_free(&pk);
		hx509_context_free(&hctx);
		return WERR_INVALID_DATA;
	}

	keypair.cert.data = talloc_memdup(ctx, data.data, data.length);
	keypair.cert.length = data.length;

	/*
	 * Heimdal's bignum are big endian and the
	 * structure expect it to be in little endian
	 * so we reverse the buffer to make it work
	 */
	tmp = reverse_and_get_blob(ctx, rsa->e);
	if (tmp == NULL) {
		ok = false;
	} else {
		keypair.public_exponent = *tmp;
		SMB_ASSERT(tmp->length <= 4);
		/*
		 * The value is now in little endian but if can happen that the length is
		 * less than 4 bytes.
		 * So if we have less than 4 bytes we pad with zeros so that it correctly
		 * fit into the structure.
		 */
		if (tmp->length < 4) {
			/*
			 * We need the expo to fit 4 bytes
			 */
			keypair.public_exponent.data = talloc_zero_array(ctx, uint8_t, 4);
			memcpy(keypair.public_exponent.data, tmp->data, tmp->length);
			keypair.public_exponent.length = 4;
		}
	}

	tmp = reverse_and_get_blob(ctx,rsa->d);
	if (tmp == NULL) {
		ok = false;
	} else {
		keypair.private_exponent = *tmp;
	}

	tmp = reverse_and_get_blob(ctx,rsa->n);
	if (tmp == NULL) {
		ok = false;
	} else {
		keypair.modulus = *tmp;
	}

	tmp = reverse_and_get_blob(ctx,rsa->p);
	if (tmp == NULL) {
		ok = false;
	} else {
		keypair.prime1 = *tmp;
	}

	tmp = reverse_and_get_blob(ctx,rsa->q);
	if (tmp == NULL) {
		ok = false;
	} else {
		keypair.prime2 = *tmp;
	}

	tmp = reverse_and_get_blob(ctx,rsa->dmp1);
	if (tmp == NULL) {
		ok = false;
	} else {
		keypair.exponent1 = *tmp;
	}

	tmp = reverse_and_get_blob(ctx,rsa->dmq1);
	if (tmp == NULL) {
		ok = false;
	} else {
		keypair.exponent2 = *tmp;
	}

	tmp = reverse_and_get_blob(ctx,rsa->iqmp);
	if (tmp == NULL) {
		ok = false;
	} else {
		keypair.coefficient = *tmp;
	}

	/* One of the keypair allocation was wrong */
	if (ok == false) {
		der_free_octet_string(&data);
		hx509_cert_free(cert);
		hx509_private_key_free(&pk);
		hx509_context_free(&hctx);
		RSA_free(rsa);
		return WERR_INVALID_DATA;
	}
	keypair.certificate_len = keypair.cert.length;
	ndr_err = ndr_push_struct_blob(&blobkeypair, ctx, &keypair, (ndr_push_flags_fn_t)ndr_push_bkrp_exported_RSA_key_pair);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		der_free_octet_string(&data);
		hx509_cert_free(cert);
		hx509_private_key_free(&pk);
		hx509_context_free(&hctx);
		RSA_free(rsa);
		return WERR_INVALID_DATA;
	}

	secret_name = talloc_asprintf(ctx, "BCKUPKEY_%s", GUID_string(ctx, &guid));
	if (secret_name == NULL) {
		der_free_octet_string(&data);
		hx509_cert_free(cert);
		hx509_private_key_free(&pk);
		hx509_context_free(&hctx);
		RSA_free(rsa);
		return WERR_OUTOFMEMORY;
	}

	status = set_lsa_secret(ctx, ldb_ctx, secret_name, &blobkeypair);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(2, ("Failed to save the secret %s\n", secret_name));
	}
	talloc_free(secret_name);

	GUID_to_ndr_blob(&guid, ctx, &blob);
	status = set_lsa_secret(ctx, ldb_ctx, "BCKUPKEY_PREFERRED", &blob);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(2, ("Failed to save the secret BCKUPKEY_PREFERRED\n"));
	}

	der_free_octet_string(&data);
	hx509_cert_free(cert);
	hx509_private_key_free(&pk);
	hx509_context_free(&hctx);
	RSA_free(rsa);
	return WERR_OK;
}

static WERROR bkrp_do_retreive_client_wrap_key(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		struct bkrp_BackupKey *r ,struct ldb_context *ldb_ctx)
{
	struct GUID guid;
	char *guid_string;
	DATA_BLOB secret;
	enum ndr_err_code ndr_err;
	NTSTATUS status;

	/*
	 * here we basicaly need to return our certificate
	 * search for lsa secret BCKUPKEY_PREFERRED first
	 */

	status = get_lsa_secret(mem_ctx,
				ldb_ctx,
				"BCKUPKEY_PREFERRED",
				&secret);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("Error while fetching secret BCKUPKEY_PREFERRED\n"));
		if (!NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
			/* Ok we can be in this case if there was no certs */
			struct loadparm_context *lp_ctx = dce_call->conn->dce_ctx->lp_ctx;
			char *dn = talloc_asprintf(mem_ctx, "CN=%s.%s",
							lpcfg_netbios_name(lp_ctx),
							lpcfg_realm(lp_ctx));

			WERROR werr =  generate_bkrp_cert(mem_ctx, dce_call, ldb_ctx, dn);
			if (!W_ERROR_IS_OK(werr)) {
				return WERR_INVALID_PARAMETER;
			}
			status = get_lsa_secret(mem_ctx,
					ldb_ctx,
					"BCKUPKEY_PREFERRED",
					&secret);

			if (!NT_STATUS_IS_OK(status)) {
				/* Ok we really don't manage to get this certs ...*/
				DEBUG(2, ("Unable to locate BCKUPKEY_PREFERRED after cert generation\n"));
				return WERR_FILE_NOT_FOUND;
			}
		} else {
			/* In theory we should NEVER reach this point as it
			   should only appear in a rodc server */
			/* we do not have the real secret attribute */
			return WERR_INVALID_PARAMETER;
		}
	}

	if (secret.length != 0) {
		char *cert_secret_name;

		status = GUID_from_ndr_blob(&secret, &guid);
		if (!NT_STATUS_IS_OK(status)) {
			return WERR_FILE_NOT_FOUND;
		}

		guid_string = GUID_string(mem_ctx, &guid);
		if (guid_string == NULL) {
			/* We return file not found because the client
			 * expect this error
			 */
			return WERR_FILE_NOT_FOUND;
		}
				
		cert_secret_name = talloc_asprintf(mem_ctx,
							"BCKUPKEY_%s",
							guid_string);
		status = get_lsa_secret(mem_ctx,
					ldb_ctx,
					cert_secret_name,
					&secret);
		if (!NT_STATUS_IS_OK(status)) {
			return WERR_FILE_NOT_FOUND;
		}

		if (secret.length != 0) {
			struct bkrp_exported_RSA_key_pair keypair;
			ndr_err = ndr_pull_struct_blob(&secret, mem_ctx, &keypair,
					(ndr_pull_flags_fn_t)ndr_pull_bkrp_exported_RSA_key_pair);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				return WERR_FILE_NOT_FOUND;
			}
			*(r->out.data_out_len) = keypair.cert.length;
			*(r->out.data_out) = talloc_memdup(mem_ctx, keypair.cert.data, keypair.cert.length);
			W_ERROR_HAVE_NO_MEMORY(*(r->out.data_out));
			return WERR_OK;
		} else {
			DEBUG(10, ("No or broken secret called %s\n", cert_secret_name));
			return WERR_FILE_NOT_FOUND;
		}
	} else {
		DEBUG(10, ("No secret BCKUPKEY_PREFERRED\n"));
		return WERR_FILE_NOT_FOUND;
	}

	return WERR_NOT_SUPPORTED;
}

static WERROR dcesrv_bkrp_BackupKey(struct dcesrv_call_state *dce_call,
				    TALLOC_CTX *mem_ctx, struct bkrp_BackupKey *r)
{
	WERROR error = WERR_INVALID_PARAM;
	struct ldb_context *ldb_ctx;
	bool is_rodc;
	const char *addr = "unknown";
	/* At which level we start to add more debug of what is done in the protocol */
	const int debuglevel = 4;

	if (DEBUGLVL(debuglevel)) {
		const struct tsocket_address *remote_address;
		remote_address = dcesrv_connection_get_remote_address(dce_call->conn);
		if (tsocket_address_is_inet(remote_address, "ip")) {
			addr = tsocket_address_inet_addr_string(remote_address, mem_ctx);
			W_ERROR_HAVE_NO_MEMORY(addr);
		}
	}

	if (lpcfg_server_role(dce_call->conn->dce_ctx->lp_ctx) != ROLE_DOMAIN_CONTROLLER) {
		return WERR_NOT_SUPPORTED;
	}

	if (!dce_call->conn->auth_state.auth_info ||
		dce_call->conn->auth_state.auth_info->auth_level != DCERPC_AUTH_LEVEL_PRIVACY) {
		DCESRV_FAULT(DCERPC_FAULT_ACCESS_DENIED);
	}

	ldb_ctx = samdb_connect(mem_ctx, dce_call->event_ctx,
				dce_call->conn->dce_ctx->lp_ctx,
				system_session(dce_call->conn->dce_ctx->lp_ctx), 0);

	if (samdb_rodc(ldb_ctx, &is_rodc) != LDB_SUCCESS) {
		talloc_unlink(mem_ctx, ldb_ctx);
		return WERR_INVALID_PARAM;
	}

	if (!is_rodc) {
		if(strncasecmp(GUID_string(mem_ctx, r->in.guidActionAgent),
			BACKUPKEY_RESTORE_GUID, strlen(BACKUPKEY_RESTORE_GUID)) == 0) {
			DEBUG(debuglevel, ("Client %s requested to decrypt a client side wrapped secret\n", addr));
			error = bkrp_do_uncrypt_client_wrap_key(dce_call, mem_ctx, r, ldb_ctx);
		}

		if (strncasecmp(GUID_string(mem_ctx, r->in.guidActionAgent),
			BACKUPKEY_RETRIEVE_BACKUP_KEY_GUID, strlen(BACKUPKEY_RETRIEVE_BACKUP_KEY_GUID)) == 0) {
			DEBUG(debuglevel, ("Client %s requested certificate for client wrapped secret\n", addr));
			error = bkrp_do_retreive_client_wrap_key(dce_call, mem_ctx, r, ldb_ctx);
		}
	}
	/*else: I am a RODC so I don't handle backup key protocol */

	talloc_unlink(mem_ctx, ldb_ctx);
	return error;
}

/* include the generated boilerplate */
#include "librpc/gen_ndr/ndr_backupkey_s.c"
