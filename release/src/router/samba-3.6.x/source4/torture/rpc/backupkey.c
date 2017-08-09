/*
   Unix SMB/CIFS implementation.
   test suite for backupkey remote protocol rpc operations

   Copyright (C) Matthieu Patou 2010-2011

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
#include "../libcli/security/security.h"
#include "librpc/gen_ndr/ndr_backupkey_c.h"
#include "librpc/gen_ndr/ndr_backupkey.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "torture/rpc/torture_rpc.h"
#include "lib/cmdline/popt_common.h"
#include "heimdal/lib/hx509/hx_locl.h"

/* Our very special and valued secret */
/* No need to put const as we cast the array in uint8_t
 * we will get a warning about the discared const
 */
static const char secret[] = "tata yoyo mais qu'est ce qu'il y a sous ton grand chapeau ?";

/* Get the SID from a user */
static const struct dom_sid *get_user_sid(struct torture_context *tctx,
					struct dcerpc_pipe *p,
					TALLOC_CTX *mem_ctx,
					const char *user)
{
	struct lsa_ObjectAttribute attr;
	struct lsa_QosInfo qos;
	struct lsa_OpenPolicy2 r;
	struct lsa_Close c;
	NTSTATUS status;
	struct policy_handle handle;
	struct lsa_LookupNames l;
	struct lsa_TransSidArray sids;
	struct lsa_RefDomainList *domains = NULL;
	struct lsa_String lsa_name;
	uint32_t count = 0;
	struct dom_sid *result;
	TALLOC_CTX *tmp_ctx;
	struct dcerpc_pipe *p2;
	struct dcerpc_binding_handle *b;

	const char *domain = cli_credentials_get_domain(cmdline_credentials);

	torture_assert_ntstatus_ok(tctx,
				torture_rpc_connection(tctx, &p2, &ndr_table_lsarpc),
				"could not open lsarpc pipe");
	b = p2->binding_handle;

	if (!(tmp_ctx = talloc_new(mem_ctx))) {
		return NULL;
	}
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

	r.in.system_name = "\\";
	r.in.attr = &attr;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle = &handle;

	status = dcerpc_lsa_OpenPolicy2_r(b, tmp_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx,
				"OpenPolicy2 failed - %s\n",
				nt_errstr(status));
		talloc_free(tmp_ctx);
		return NULL;
	}
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_comment(tctx,
				"OpenPolicy2_ failed - %s\n",
				nt_errstr(r.out.result));
		talloc_free(tmp_ctx);
		return NULL;
	}

	sids.count = 0;
	sids.sids = NULL;

	lsa_name.string = talloc_asprintf(tmp_ctx, "%s\\%s", domain, user);

	l.in.handle = &handle;
	l.in.num_names = 1;
	l.in.names = &lsa_name;
	l.in.sids = &sids;
	l.in.level = 1;
	l.in.count = &count;
	l.out.count = &count;
	l.out.sids = &sids;
	l.out.domains = &domains;

	status = dcerpc_lsa_LookupNames_r(b, tmp_ctx, &l);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx,
				"LookupNames of %s failed - %s\n",
				lsa_name.string,
				nt_errstr(status));
		talloc_free(tmp_ctx);
		return NULL;
	}

	if (domains->count == 0) {
		return NULL;
	}

	result = dom_sid_add_rid(mem_ctx,
				 domains->domains[0].sid,
				 l.out.sids->sids[0].rid);
	c.in.handle = &handle;
	c.out.handle = &handle;

	status = dcerpc_lsa_Close_r(b, tmp_ctx, &c);

	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx,
				"dcerpc_lsa_Close failed - %s\n",
				nt_errstr(status));
		talloc_free(tmp_ctx);
		return NULL;
	}

	if (!NT_STATUS_IS_OK(c.out.result)) {
		torture_comment(tctx,
				"dcerpc_lsa_Close failed - %s\n",
				nt_errstr(c.out.result));
		talloc_free(tmp_ctx);
		return NULL;
	}

	talloc_free(tmp_ctx);
	talloc_free(p2);

	torture_comment(tctx, "Get_user_sid finished\n");
	return result;
}

/*
 * Create a bkrp_encrypted_secret_vX structure
 * the version depends on the version parameter
 * the structure is returned as a blob.
 * The broken flag is to indicate if we want
 * to create a non conform to specification structre
 */
static DATA_BLOB *create_unencryptedsecret(TALLOC_CTX *mem_ctx,
					   bool broken,
					   int version)
{
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	DATA_BLOB *blob = talloc_zero(mem_ctx, DATA_BLOB);
	enum ndr_err_code ndr_err;

	if (version == 2) {
		struct bkrp_encrypted_secret_v2 unenc_sec;

		ZERO_STRUCT(unenc_sec);
		unenc_sec.secret_len = sizeof(secret);
		unenc_sec.secret = discard_const_p(uint8_t, secret);
		generate_random_buffer(unenc_sec.payload_key,
				       sizeof(unenc_sec.payload_key));

		ndr_err = ndr_push_struct_blob(blob, blob, &unenc_sec,
				(ndr_push_flags_fn_t)ndr_push_bkrp_encrypted_secret_v2);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return NULL;
		}

		if (broken) {
			/* The magic value is correctly set by the NDR push
			 * but we want to test the behavior of the server
			 * if a differrent value is provided
			 */
			((uint8_t*)blob->data)[4] = 79; /* A great year !!! */
		}
	}

	if (version == 3) {
		struct bkrp_encrypted_secret_v3 unenc_sec;

		ZERO_STRUCT(unenc_sec);
		unenc_sec.secret_len = sizeof(secret);
		unenc_sec.secret = discard_const_p(uint8_t, secret);
		generate_random_buffer(unenc_sec.payload_key,
				       sizeof(unenc_sec.payload_key));

		ndr_err = ndr_push_struct_blob(blob, blob, &unenc_sec,
					(ndr_push_flags_fn_t)ndr_push_bkrp_encrypted_secret_v3);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return NULL;
		}

		if (broken) {
			/*
			 * The magic value is correctly set by the NDR push
			 * but we want to test the behavior of the server
			 * if a differrent value is provided
			 */
			((uint8_t*)blob->data)[4] = 79; /* A great year !!! */
		}
	}
	talloc_free(tmp_ctx);
	return blob;
}

/*
 * Create an access check structure, the format depends on the version parameter.
 * If broken is specified then we create a stucture that isn't conform to the 
 * specification.
 *
 * If the structure can't be created then NULL is returned.
 */
static DATA_BLOB *create_access_check(struct torture_context *tctx,
				      struct dcerpc_pipe *p,
				      TALLOC_CTX *mem_ctx,
				      const char *user,
				      bool broken,
				      uint32_t version)
{
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	DATA_BLOB *blob = talloc_zero(mem_ctx, DATA_BLOB);
	enum ndr_err_code ndr_err;
	const struct dom_sid *sid = get_user_sid(tctx, p, tmp_ctx, user);

	if (sid == NULL) {
		return NULL;
	}

	if (version == 2) {
		struct bkrp_access_check_v2 access_struct;
		struct sha sctx;
		uint8_t nonce[32];

		ZERO_STRUCT(access_struct);
		generate_random_buffer(nonce, sizeof(nonce));
		access_struct.nonce_len = sizeof(nonce);
		access_struct.nonce = nonce;
		access_struct.sid = *sid;

		ndr_err = ndr_push_struct_blob(blob, blob, &access_struct,
				(ndr_push_flags_fn_t)ndr_push_bkrp_access_check_v2);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return NULL;
		}

		/*
		 * We pushed the whole structure including a null hash
		 * but the hash need to be calculated only up to the hash field
		 * so we reduce the size of what has to be calculated
		 */

		SHA1_Init(&sctx);
		SHA1_Update(&sctx, blob->data,
			    blob->length - sizeof(access_struct.hash));
		SHA1_Final(blob->data + blob->length - sizeof(access_struct.hash),
			   &sctx);

		/* Altering the SHA */
		if (broken) {
			blob->data[blob->length - 1]++;
		}
	}

	if (version == 3) {
		struct bkrp_access_check_v3 access_struct;
		struct hc_sha512state sctx;
		uint8_t nonce[32];

		ZERO_STRUCT(access_struct);
		generate_random_buffer(nonce, sizeof(nonce));
		access_struct.nonce_len = sizeof(nonce);
		access_struct.nonce = nonce;
		access_struct.sid = *sid;

		ndr_err = ndr_push_struct_blob(blob, blob, &access_struct,
				(ndr_push_flags_fn_t)ndr_push_bkrp_access_check_v3);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return NULL;
		}

		/*We pushed the whole structure including a null hash
		* but the hash need to be calculated only up to the hash field
		* so we reduce the size of what has to be calculated
		*/

		SHA512_Init(&sctx);
		SHA512_Update(&sctx, blob->data,
			      blob->length - sizeof(access_struct.hash));
		SHA512_Final(blob->data + blob->length - sizeof(access_struct.hash),
			     &sctx);

		/* Altering the SHA */
		if (broken) {
			blob->data[blob->length -1]++;
		}
	}
	talloc_free(tmp_ctx);
	return blob;
}


static DATA_BLOB *encrypt_blob(struct torture_context *tctx,
				    TALLOC_CTX *mem_ctx,
				    DATA_BLOB *key,
				    DATA_BLOB *iv,
				    DATA_BLOB *to_encrypt,
				    const AlgorithmIdentifier *alg)
{
	hx509_crypto crypto;
	hx509_context hctx;
	heim_octet_string ivos;
	heim_octet_string *encrypted;
	DATA_BLOB *blob = talloc_zero(mem_ctx, DATA_BLOB);
	int res;

	ivos.data = talloc_array(mem_ctx, uint8_t, iv->length);
	ivos.length = iv->length;
	memcpy(ivos.data, iv->data, iv->length);

	hx509_context_init(&hctx);
	res = hx509_crypto_init(hctx, NULL, &alg->algorithm, &crypto);
	if (res) {
		torture_comment(tctx,
				"error while doing the init of the crypto object\n");
		hx509_context_free(&hctx);
		return NULL;
	}
	res = hx509_crypto_set_key_data(crypto, key->data, key->length);
	if (res) {
		torture_comment(tctx,
				"error while setting the key of the crypto object\n");
		hx509_context_free(&hctx);
		return NULL;
	}

	hx509_crypto_set_padding(crypto, HX509_CRYPTO_PADDING_NONE);
	res = hx509_crypto_encrypt(crypto,
				   to_encrypt->data,
				   to_encrypt->length,
				   &ivos,
				   &encrypted);
	if (res) {
		torture_comment(tctx, "error while encrypting\n");
		hx509_crypto_destroy(crypto);
		hx509_context_free(&hctx);
		return NULL;
	}

	*blob = data_blob_talloc(blob, encrypted->data, encrypted->length);
	der_free_octet_string(encrypted);
	free(encrypted);
	hx509_crypto_destroy(crypto);
	hx509_context_free(&hctx);
	return blob;
}

/*
 * Certs used for this protocol have a GUID in the issuer_uniq_id field.
 * This function fetch it.
 */
static struct GUID *get_cert_guid(struct torture_context *tctx,
				  TALLOC_CTX *mem_ctx,
				  uint8_t *cert_data,
				  uint32_t cert_len)
{
	hx509_context hctx;
	hx509_cert cert;
	heim_bit_string subjectuniqid;
	DATA_BLOB data;
	int hret;
	uint32_t size;
	struct GUID *guid = talloc_zero(mem_ctx, struct GUID);
	NTSTATUS status;

	hx509_context_init(&hctx);

	hret = hx509_cert_init_data(hctx, cert_data, cert_len, &cert);
	if (hret) {
		torture_comment(tctx, "error while loading the cert\n");
		hx509_context_free(&hctx);
		return NULL;
	}
	hret = hx509_cert_get_issuer_unique_id(hctx, cert, &subjectuniqid);
	if (hret) {
		torture_comment(tctx, "error while getting the issuer_uniq_id\n");
		hx509_cert_free(cert);
		hx509_context_free(&hctx);
		return NULL;
	}

	/* The subjectuniqid is a bit string,
	 * which means that the real size has to be divided by 8
	 * to have the number of bytes
	 */
	hx509_cert_free(cert);
	hx509_context_free(&hctx);
	size = subjectuniqid.length / 8;
	data = data_blob_const(subjectuniqid.data, size);

	status = GUID_from_data_blob(&data, guid);
	der_free_bit_string(&subjectuniqid);
	if (!NT_STATUS_IS_OK(status)) {
		return NULL;
	}

	return guid;
}

/*
 * Encrypt a blob with the private key of the certificate
 * passed as a parameter.
 */
static DATA_BLOB *encrypt_blob_pk(struct torture_context *tctx,
				  TALLOC_CTX *mem_ctx,
				  uint8_t *cert_data,
				  uint32_t cert_len,
				  DATA_BLOB *to_encrypt)
{
	hx509_context hctx;
	hx509_cert cert;
	heim_octet_string secretdata;
	heim_octet_string encrypted;
	heim_oid encryption_oid;
	DATA_BLOB *blob;
	int hret;

	hx509_context_init(&hctx);

	hret = hx509_cert_init_data(hctx, cert_data, cert_len, &cert);
	if (hret) {
		torture_comment(tctx, "error while loading the cert\n");
		hx509_context_free(&hctx);
		return NULL;
	}

	secretdata.data = to_encrypt->data;
	secretdata.length = to_encrypt->length;
	hret = hx509_cert_public_encrypt(hctx, &secretdata,
					  cert, &encryption_oid,
					  &encrypted);
	hx509_cert_free(cert);
	hx509_context_free(&hctx);
	if (hret) {
		torture_comment(tctx, "error while encrypting\n");
		return NULL;
	}

	blob = talloc_zero(mem_ctx, DATA_BLOB);
	if (blob == NULL) {
		der_free_oid(&encryption_oid);
		der_free_octet_string(&encrypted);
		return NULL;
	}

	*blob = data_blob_talloc(blob, encrypted.data, encrypted.length);
	der_free_octet_string(&encrypted);
	der_free_oid(&encryption_oid);
	if (blob->data == NULL) {
		return NULL;
	}

	return blob;
}


static struct bkrp_BackupKey *createRetreiveBackupKeyGUIDStruct(struct torture_context *tctx,
				struct dcerpc_pipe *p, int version, DATA_BLOB *out)
{
	struct dcerpc_binding *binding = p->binding;
	struct bkrp_client_side_wrapped data;
	struct GUID *g = talloc(tctx, struct GUID);
	struct bkrp_BackupKey *r = talloc_zero(tctx, struct bkrp_BackupKey);
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	NTSTATUS status;

	if (r == NULL) {
		return NULL;
	}

	binding->flags = binding->flags & (DCERPC_SEAL|DCERPC_AUTH_SPNEGO);
	ZERO_STRUCT(data);
	status = GUID_from_string(BACKUPKEY_RETRIEVE_BACKUP_KEY_GUID, g);
	if (!NT_STATUS_IS_OK(status)) {
		return NULL;
	}

	r->in.guidActionAgent = g;
	data.version = version;
	ndr_err = ndr_push_struct_blob(&blob, tctx, &data,
			(ndr_push_flags_fn_t)ndr_push_bkrp_client_side_wrapped);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return NULL;
	}
	r->in.data_in = blob.data;
	r->in.data_in_len = blob.length;
	r->out.data_out = &out->data;
	r->out.data_out_len = talloc(r, uint32_t);
	return r;
}

static struct bkrp_BackupKey *createRestoreGUIDStruct(struct torture_context *tctx,
				struct dcerpc_pipe *p, int version, DATA_BLOB *out,
				bool norevert,
				bool broken_version,
				bool broken_user,
				bool broken_magic_secret,
				bool broken_magic_access,
				bool broken_hash_access,
				bool broken_cert_guid)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct bkrp_client_side_wrapped data;
	DATA_BLOB *xs;
	DATA_BLOB *sec;
	DATA_BLOB *enc_sec;
	DATA_BLOB *enc_xs;
	DATA_BLOB *blob2;
	DATA_BLOB enc_sec_reverted;
	DATA_BLOB des3_key;
	DATA_BLOB aes_key;
	DATA_BLOB iv;
	DATA_BLOB out_blob;
	struct GUID *guid, *g;
	int t;
	uint32_t size;
	enum ndr_err_code ndr_err;
	NTSTATUS status;
	const char *user;
	struct bkrp_BackupKey *r = createRetreiveBackupKeyGUIDStruct(tctx, p, version, &out_blob);
	if (r == NULL) {
		return NULL;
	}

	if (broken_user) {
		/* we take a fake user*/
		user = "guest";
	} else {
		user = cli_credentials_get_username(cmdline_credentials);
	}


	torture_assert_ntstatus_ok(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r),
					"Get GUID");
	/*
	 * We have to set it outside of the function createRetreiveBackupKeyGUIDStruct
	 * the len of the blob, this is due to the fact that they don't have the
	 * same size (one is 32bits the other 64bits)
	 */
	out_blob.length = *r->out.data_out_len;

	sec = create_unencryptedsecret(tctx, broken_magic_secret, version);
	if (sec == NULL) {
		return NULL;
	}

	xs = create_access_check(tctx, p, tctx, user, broken_hash_access, version);
	if (xs == NULL) {
		return NULL;
	}

	if (broken_magic_access){
		/* The start of the access_check structure contains the 
		 * GUID of the certificate
		 */
		xs->data[0]++;
	}

	enc_sec = encrypt_blob_pk(tctx, tctx, out_blob.data, out_blob.length, sec);
	if (!enc_sec) {
		return NULL;
	}
	enc_sec_reverted.data = talloc_array(tctx, uint8_t, enc_sec->length);
	if (enc_sec_reverted.data == NULL) {
		return NULL;
	}
	enc_sec_reverted.length = enc_sec->length;

	/*
	* We DO NOT revert the array on purpose it's in order to check that
	* when the server is not able to decrypt then it answer the correct error
	*/
	if (norevert) {
		for(t=0; t< enc_sec->length; t++) {
			enc_sec_reverted.data[t] = ((uint8_t*)enc_sec->data)[t];
		}
	} else {
		for(t=0; t< enc_sec->length; t++) {
			enc_sec_reverted.data[t] = ((uint8_t*)enc_sec->data)[enc_sec->length - t -1];
		}
	}

	size = sec->length;
	if (version ==2) {
		const AlgorithmIdentifier *alg = hx509_crypto_des_rsdi_ede3_cbc();
		iv.data = sec->data+(size - 8);
		iv.length = 8;

		des3_key.data = sec->data+(size - 32);
		des3_key.length = 24;

		enc_xs = encrypt_blob(tctx, tctx, &des3_key, &iv, xs, alg);
	}
	if (version == 3) {
		const AlgorithmIdentifier *alg = hx509_crypto_aes256_cbc();
		iv.data = sec->data+(size-16);
		iv.length = 16;

		aes_key.data = sec->data+(size-48);
		aes_key.length = 32;

		enc_xs = encrypt_blob(tctx, tctx, &aes_key, &iv, xs, alg);
	}

	if (!enc_xs) {
		return NULL;
	}

	/* To cope with the fact that heimdal do padding at the end for the moment */
	enc_xs->length = xs->length;

	guid = get_cert_guid(tctx, tctx, out_blob.data, out_blob.length);
	if (guid == NULL) {
		return NULL;
	}

	if (broken_version) {
		data.version = 1;
	} else {
		data.version = version;
	}

	data.guid = *guid;
	data.encrypted_secret = enc_sec_reverted.data;
	data.access_check = enc_xs->data;
	data.encrypted_secret_len = enc_sec->length;
	data.access_check_len = enc_xs->length;

	/* We want the blob to persist after this function so we don't
	 * allocate it in the stack
	 */
	blob2 = talloc(tctx, DATA_BLOB);
	if (blob2 == NULL) {
		return NULL;
	}

	ndr_err = ndr_push_struct_blob(blob2, tctx, &data,
			(ndr_push_flags_fn_t)ndr_push_bkrp_client_side_wrapped);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return NULL;
	}

	if (broken_cert_guid) {
		blob2->data[12]++;
	}

	ZERO_STRUCT(*r);

	g = talloc(tctx, struct GUID);
	if (g == NULL) {
		return NULL;
	}

	status = GUID_from_string(BACKUPKEY_RESTORE_GUID, g);
	if (!NT_STATUS_IS_OK(status)) {
		return NULL;
	}

	r->in.guidActionAgent = g;
	r->in.data_in = blob2->data;
	r->in.data_in_len = blob2->length;
	r->in.param = 0;
	r->out.data_out = &(out->data);
	r->out.data_out_len = talloc(r, uint32_t);
	return r;
}

/* Check that we are able to receive the certificate of the DCs
 * used for client wrap version of the backup key protocol
 */
static bool test_RetreiveBackupKeyGUID(struct torture_context *tctx,
					struct dcerpc_pipe *p)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	DATA_BLOB out_blob;
	struct bkrp_BackupKey *r = createRetreiveBackupKeyGUIDStruct(tctx, p, 2, &out_blob);

	if (r == NULL) {
		return false;
	}

	if (p->conn->security_state.auth_info != NULL &&
	    p->conn->security_state.auth_info->auth_level == 6) {
		torture_assert_ntstatus_ok(tctx,
				dcerpc_bkrp_BackupKey_r(b, tctx, r),
				"Get GUID");

		out_blob.length = *r->out.data_out_len;
		torture_assert_werr_equal(tctx,
						r->out.result,
						WERR_OK,
						"Wrong dce/rpc error code");
	} else {
		torture_assert_ntstatus_equal(tctx,
						dcerpc_bkrp_BackupKey_r(b, tctx, r),
						NT_STATUS_ACCESS_DENIED,
						"Get GUID");
	}
	return true;
}

/* Test to check the failure to recover a secret because the 
 * secret blob is not reversed
 */
static bool test_RestoreGUID_ko(struct torture_context *tctx,
				struct dcerpc_pipe *p)
{
	enum ndr_err_code ndr_err;
	struct dcerpc_binding_handle *b = p->binding_handle;
	DATA_BLOB out_blob;
	struct bkrp_client_side_unwrapped resp;

	if (p->conn->security_state.auth_info != NULL &&
	    p->conn->security_state.auth_info->auth_level == 6) {
		struct bkrp_BackupKey *r = createRestoreGUIDStruct(tctx, p, 2, &out_blob,
					true, false, false, false, false, false, false);
		torture_assert_ntstatus_ok(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r), "Restore GUID");
		out_blob.length = *r->out.data_out_len;
		ndr_err = ndr_pull_struct_blob(&out_blob, tctx, &resp, (ndr_pull_flags_fn_t)ndr_pull_bkrp_client_side_unwrapped);
		torture_assert_int_equal(tctx, NDR_ERR_CODE_IS_SUCCESS(ndr_err), 0, "Unable to unmarshall bkrp_client_side_unwrapped");
		torture_assert_werr_equal(tctx, r->out.result, WERR_INVALID_DATA, "Wrong error code");
	} else {
		struct bkrp_BackupKey *r = createRetreiveBackupKeyGUIDStruct(tctx, p, 2, &out_blob);
		torture_assert_ntstatus_equal(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r),
			NT_STATUS_ACCESS_DENIED, "Get GUID");
	}
	return true;
}

static bool test_RestoreGUID_wrongversion(struct torture_context *tctx,
					  struct dcerpc_pipe *p)
{
	enum ndr_err_code ndr_err;
	struct dcerpc_binding_handle *b = p->binding_handle;
	DATA_BLOB out_blob;
	struct bkrp_client_side_unwrapped resp;

	if (p->conn->security_state.auth_info != NULL &&
	    p->conn->security_state.auth_info->auth_level == 6) {
		struct bkrp_BackupKey *r = createRestoreGUIDStruct(tctx, p, 2, &out_blob,
					false, true, false, false, false, false, false);
		torture_assert_ntstatus_ok(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r), "Restore GUID");
		out_blob.length = *r->out.data_out_len;
		ndr_err = ndr_pull_struct_blob(&out_blob, tctx, &resp, (ndr_pull_flags_fn_t)ndr_pull_bkrp_client_side_unwrapped);
		torture_assert_int_equal(tctx, NDR_ERR_CODE_IS_SUCCESS(ndr_err), 0, "Unable to unmarshall bkrp_client_side_unwrapped");
		torture_assert_werr_equal(tctx, r->out.result, WERR_INVALID_PARAM, "Wrong error code on wrong version");
	} else {
		struct bkrp_BackupKey *r = createRetreiveBackupKeyGUIDStruct(tctx, p, 2, &out_blob);
		torture_assert_ntstatus_equal(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r),
			NT_STATUS_ACCESS_DENIED, "Get GUID");
	}
	return true;
}

static bool test_RestoreGUID_wronguser(struct torture_context *tctx,
				       struct dcerpc_pipe *p)
{
	enum ndr_err_code ndr_err;
	struct dcerpc_binding_handle *b = p->binding_handle;
	DATA_BLOB out_blob;
	struct bkrp_client_side_unwrapped resp;

	if (p->conn->security_state.auth_info != NULL &&
	    p->conn->security_state.auth_info->auth_level == 6) {
		struct bkrp_BackupKey *r = createRestoreGUIDStruct(tctx, p, 2, &out_blob,
					false, false, true, false, false, false, false);
		torture_assert_ntstatus_ok(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r), "Restore GUID");
		out_blob.length = *r->out.data_out_len;
		ndr_err = ndr_pull_struct_blob(&out_blob, tctx, &resp, (ndr_pull_flags_fn_t)ndr_pull_bkrp_client_side_unwrapped);
		torture_assert_int_equal(tctx, NDR_ERR_CODE_IS_SUCCESS(ndr_err), 0, "Unable to unmarshall bkrp_client_side_unwrapped");
		torture_assert_werr_equal(tctx, r->out.result, WERR_INVALID_ACCESS, "Restore GUID");
	} else {
		struct bkrp_BackupKey *r = createRetreiveBackupKeyGUIDStruct(tctx, p, 2, &out_blob);
		torture_assert_ntstatus_equal(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r),
			NT_STATUS_ACCESS_DENIED, "Get GUID");
	}
	return true;
}

static bool test_RestoreGUID_v3(struct torture_context *tctx,
				struct dcerpc_pipe *p)
{
	enum ndr_err_code ndr_err;
	struct dcerpc_binding_handle *b = p->binding_handle;
	DATA_BLOB out_blob;
	struct bkrp_client_side_unwrapped resp;

	if (p->conn->security_state.auth_info != NULL &&
	    p->conn->security_state.auth_info->auth_level == 6) {
		struct bkrp_BackupKey *r = createRestoreGUIDStruct(tctx, p, 3, &out_blob,
					false, false, false, false, false, false, false);
		torture_assert_ntstatus_ok(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r), "Restore GUID");
		out_blob.length = *r->out.data_out_len;
		ndr_err = ndr_pull_struct_blob(&out_blob, tctx, &resp, (ndr_pull_flags_fn_t)ndr_pull_bkrp_client_side_unwrapped);
		torture_assert_int_equal(tctx, NDR_ERR_CODE_IS_SUCCESS(ndr_err), 1, "Unable to unmarshall bkrp_client_side_unwrapped");
		torture_assert_werr_equal(tctx, r->out.result, WERR_OK, "Restore GUID");
		torture_assert_str_equal(tctx, (char*)resp.secret.data, secret, "Wrong secret");
	} else {
		struct bkrp_BackupKey *r = createRetreiveBackupKeyGUIDStruct(tctx, p, 2, &out_blob);
		torture_assert_ntstatus_equal(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r),
			NT_STATUS_ACCESS_DENIED, "Get GUID");
	}
	return true;
}

static bool test_RestoreGUID(struct torture_context *tctx,
			     struct dcerpc_pipe *p)
{
	enum ndr_err_code ndr_err;
	struct dcerpc_binding_handle *b = p->binding_handle;
	DATA_BLOB out_blob;
	struct bkrp_client_side_unwrapped resp;

	if (p->conn->security_state.auth_info != NULL &&
	    p->conn->security_state.auth_info->auth_level == 6) {
		struct bkrp_BackupKey *r = createRestoreGUIDStruct(tctx, p, 2, &out_blob,
					false, false, false, false, false, false, false);
		torture_assert_ntstatus_ok(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r), "Restore GUID");
		out_blob.length = *r->out.data_out_len;
		ndr_err = ndr_pull_struct_blob(&out_blob, tctx, &resp, (ndr_pull_flags_fn_t)ndr_pull_bkrp_client_side_unwrapped);
		torture_assert_int_equal(tctx, NDR_ERR_CODE_IS_SUCCESS(ndr_err), 1, "Unable to unmarshall bkrp_client_side_unwrapped");
		torture_assert_werr_equal(tctx, r->out.result, WERR_OK, "Restore GUID");
		torture_assert_str_equal(tctx, (char*)resp.secret.data, secret, "Wrong secret");
	} else {
		struct bkrp_BackupKey *r = createRetreiveBackupKeyGUIDStruct(tctx, p, 2, &out_blob);
		torture_assert_ntstatus_equal(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r),
			NT_STATUS_ACCESS_DENIED, "Get GUID");
	}
	return true;
}

static bool test_RestoreGUID_badmagiconsecret(struct torture_context *tctx,
					      struct dcerpc_pipe *p)
{
	enum ndr_err_code ndr_err;
	struct dcerpc_binding_handle *b = p->binding_handle;
	DATA_BLOB out_blob;
	struct bkrp_client_side_unwrapped resp;

	if (p->conn->security_state.auth_info != NULL &&
	    p->conn->security_state.auth_info->auth_level == 6) {
		struct bkrp_BackupKey *r = createRestoreGUIDStruct(tctx, p, 3, &out_blob,
					false, false, false, true, false, false, false);
		torture_assert_ntstatus_ok(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r), "Restore GUID");
		out_blob.length = *r->out.data_out_len;
		ndr_err = ndr_pull_struct_blob(&out_blob, tctx, &resp, (ndr_pull_flags_fn_t)ndr_pull_bkrp_client_side_unwrapped);
		torture_assert_int_equal(tctx, NDR_ERR_CODE_IS_SUCCESS(ndr_err), 0, "Unable to unmarshall bkrp_client_side_unwrapped");
		torture_assert_werr_equal(tctx, r->out.result, WERR_INVALID_DATA, "Wrong error code while providing bad magic in secret");
	} else {
		struct bkrp_BackupKey *r = createRetreiveBackupKeyGUIDStruct(tctx, p, 2, &out_blob);
		torture_assert_ntstatus_equal(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r),
			NT_STATUS_ACCESS_DENIED, "Get GUID");
	}
	return true;
}

static bool test_RestoreGUID_emptyrequest(struct torture_context *tctx,
					  struct dcerpc_pipe *p)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	DATA_BLOB out_blob;

	if (p->conn->security_state.auth_info != NULL &&
	    p->conn->security_state.auth_info->auth_level == 6) {
		struct bkrp_BackupKey *r = createRestoreGUIDStruct(tctx, p, 3, &out_blob,
					false, false, false, true, false, false, true);

		torture_assert_int_equal(tctx, r != NULL, 1, "Error while creating the restoreGUID struct");
		r->in.data_in = talloc(tctx, uint8_t);
		r->in.data_in_len = 0;
		r->in.param = 0;
		torture_assert_ntstatus_ok(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r), "Restore GUID");
		out_blob.length = *r->out.data_out_len;
		torture_assert_werr_equal(tctx, r->out.result, WERR_INVALID_PARAM, "Bad error code on wrong has in access check");
	} else {
		struct bkrp_BackupKey *r = createRetreiveBackupKeyGUIDStruct(tctx, p, 2, &out_blob);
		torture_assert_ntstatus_equal(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r),
			NT_STATUS_ACCESS_DENIED, "Get GUID");
	}
	return true;
}

static bool test_RestoreGUID_badcertguid(struct torture_context *tctx,
					 struct dcerpc_pipe *p)
{
	enum ndr_err_code ndr_err;
	struct dcerpc_binding_handle *b = p->binding_handle;
	DATA_BLOB out_blob;
	struct bkrp_client_side_unwrapped resp;

	if (p->conn->security_state.auth_info != NULL &&
	    p->conn->security_state.auth_info->auth_level == 6) {
		struct bkrp_BackupKey *r = createRestoreGUIDStruct(tctx, p, 3, &out_blob,
					false, false, false, false, false, false, true);
		torture_assert_ntstatus_ok(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r), "Restore GUID");
		out_blob.length = *r->out.data_out_len;
		ndr_err = ndr_pull_struct_blob(&out_blob, tctx, &resp, (ndr_pull_flags_fn_t)ndr_pull_bkrp_client_side_unwrapped);
		torture_assert_int_equal(tctx, NDR_ERR_CODE_IS_SUCCESS(ndr_err), 0, "Unable to unmarshall bkrp_client_side_unwrapped");
		torture_assert_werr_equal(tctx, r->out.result, WERR_FILE_NOT_FOUND, "Bad error code on wrong has in access check");
	} else {
		struct bkrp_BackupKey *r = createRetreiveBackupKeyGUIDStruct(tctx, p, 2, &out_blob);
		torture_assert_ntstatus_equal(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r),
			NT_STATUS_ACCESS_DENIED, "Get GUID");
	}
	return true;
}

static bool test_RestoreGUID_badmagicaccesscheck(struct torture_context *tctx,
						 struct dcerpc_pipe *p)
{
	enum ndr_err_code ndr_err;
	struct dcerpc_binding_handle *b = p->binding_handle;
	DATA_BLOB out_blob;
	struct bkrp_client_side_unwrapped resp;

	if (p->conn->security_state.auth_info != NULL &&
	    p->conn->security_state.auth_info->auth_level == 6) {
		struct bkrp_BackupKey *r = createRestoreGUIDStruct(tctx, p, 2, &out_blob,
					false, false, false, false, true, false, false);
		torture_assert_ntstatus_ok(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r), "Restore GUID");
		out_blob.length = *r->out.data_out_len;
		ndr_err = ndr_pull_struct_blob(&out_blob, tctx, &resp, (ndr_pull_flags_fn_t)ndr_pull_bkrp_client_side_unwrapped);
		torture_assert_int_equal(tctx, NDR_ERR_CODE_IS_SUCCESS(ndr_err), 0, "Unable to unmarshall bkrp_client_side_unwrapped");
		torture_assert_werr_equal(tctx, r->out.result, WERR_INVALID_DATA, "Bad error code on wrong has in access check");
	} else {
		struct bkrp_BackupKey *r = createRetreiveBackupKeyGUIDStruct(tctx, p, 2, &out_blob);
		torture_assert_ntstatus_equal(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r),
			NT_STATUS_ACCESS_DENIED, "Get GUID");
	}
	return true;
}

static bool test_RestoreGUID_badhashaccesscheck(struct torture_context *tctx,
						struct dcerpc_pipe *p)
{
	enum ndr_err_code ndr_err;
	struct dcerpc_binding_handle *b = p->binding_handle;
	DATA_BLOB out_blob;
	struct bkrp_client_side_unwrapped resp;

	if (p->conn->security_state.auth_info != NULL &&
	    p->conn->security_state.auth_info->auth_level == 6) {
		struct bkrp_BackupKey *r = createRestoreGUIDStruct(tctx, p, 2, &out_blob,
					false, false, false, false, false, true, false);
		torture_assert_ntstatus_ok(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r), "Restore GUID");
		out_blob.length = *r->out.data_out_len;
		ndr_err = ndr_pull_struct_blob(&out_blob, tctx, &resp, (ndr_pull_flags_fn_t)ndr_pull_bkrp_client_side_unwrapped);
		torture_assert_int_equal(tctx, NDR_ERR_CODE_IS_SUCCESS(ndr_err), 0, "Unable to unmarshall bkrp_client_side_unwrapped");
		torture_assert_werr_equal(tctx, r->out.result, WERR_INVALID_DATA, "Bad error code on wrong has in access check");
	} else {
		struct bkrp_BackupKey *r = createRetreiveBackupKeyGUIDStruct(tctx, p, 2, &out_blob);
		torture_assert_ntstatus_equal(tctx, dcerpc_bkrp_BackupKey_r(b, tctx, r),
			NT_STATUS_ACCESS_DENIED, "Get GUID");
	}
	return true;
}

struct torture_suite *torture_rpc_backupkey(TALLOC_CTX *mem_ctx)
{
	struct torture_rpc_tcase *tcase;
	struct torture_suite *suite = torture_suite_create(mem_ctx, "backupkey");
	struct torture_test *test;

	tcase = torture_suite_add_rpc_iface_tcase(suite, "backupkey",
						  &ndr_table_backupkey);

	test = torture_rpc_tcase_add_test(tcase, "retreive_backup_key_guid",
					  test_RetreiveBackupKeyGUID);

	test = torture_rpc_tcase_add_test(tcase, "restore_guid",
					  test_RestoreGUID);

	test = torture_rpc_tcase_add_test(tcase, "restore_guid version 3",
					  test_RestoreGUID_v3);

/* We double the test in order to be sure that we don't mess stuff (ie. freeing static stuff */

	test = torture_rpc_tcase_add_test(tcase, "restore_guid_2nd",
					  test_RestoreGUID);

	test = torture_rpc_tcase_add_test(tcase, "unable_to_decrypt_secret",
					  test_RestoreGUID_ko);

	test = torture_rpc_tcase_add_test(tcase, "wrong_user_restore_guid",
					  test_RestoreGUID_wronguser);

	test = torture_rpc_tcase_add_test(tcase, "wrong_version_restore_guid",
					  test_RestoreGUID_wrongversion);

	test = torture_rpc_tcase_add_test(tcase, "bad_magic_on_secret_restore_guid",
					  test_RestoreGUID_badmagiconsecret);

	test = torture_rpc_tcase_add_test(tcase, "bad_hash_on_secret_restore_guid",
					  test_RestoreGUID_badhashaccesscheck);

	test = torture_rpc_tcase_add_test(tcase, "bad_magic_on_accesscheck_restore_guid",
					  test_RestoreGUID_badmagicaccesscheck);

	test = torture_rpc_tcase_add_test(tcase, "bad_cert_guid_restore_guid",
					  test_RestoreGUID_badcertguid);

	test = torture_rpc_tcase_add_test(tcase, "empty_request_restore_guid",
					  test_RestoreGUID_emptyrequest);

	return suite;
}
