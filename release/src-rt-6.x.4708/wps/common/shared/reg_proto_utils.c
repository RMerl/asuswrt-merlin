/*
 * Registrataion protocol utilities
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: reg_proto_utils.c 383924 2013-02-08 04:14:39Z $
 */

#include <aes.h>
#include <wps_sha256.h>
#include <wps_dh.h>
#ifdef EXTERNAL_OPENSSL
#include <wps_openssl.h>
#endif

#include <tutrace.h>
#include <wpstypes.h>
#include <wpscommon.h>
#include <wpserror.h>
#include <reg_protomsg.h>
#include <sminfo.h>
#include <reg_proto.h>

#pragma pack(push, 1)

static uint8 DH_P_VALUE[BUF_SIZE_1536_BITS] =
{
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
	0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
	0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
	0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22,
	0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
	0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B,
	0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
	0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
	0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
	0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B,
	0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
	0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5,
	0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,
	0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
	0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05,
	0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A,
	0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
	0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96,
	0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB,
	0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
	0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04,
	0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x23, 0x73, 0x27,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static uint32 DH_G_VALUE = 2;

#pragma pack(pop)

int
reg_proto_BN_bn2bin(const BIGNUM *a, unsigned char *to)
{
	int len, offset;

	/* Extract the DH public key */
	len = BN_bn2bin(a, to);
	if (len <= 0 || len > SIZE_PUB_KEY) {
		return 0;
	}

	/* Insert zero at the public key head if length is not equal SIZE_PUB_KEY */
	offset = SIZE_PUB_KEY - len;
	if (offset) {
		memmove(&to[offset], to, len);
		memset(to, 0, offset);
	}

	return SIZE_PUB_KEY;
}

uint32
reg_proto_generate_prebuild_dhkeypair(DH **DHKeyPair, uint8 *pre_privkey)
{
	BIGNUM *pub_key = NULL, *priv_key = NULL;
	BN_CTX *ctx = NULL;
	BN_MONT_CTX *mont;
	uint32 g = 0;
	uint32 ret = RPROT_ERR_CRYPTO;


	*DHKeyPair = DH_new();

	if (*DHKeyPair == NULL) {
		TUTRACE((TUTRACE_ERR, "RPROTO: DH_new failed\n"));
		return RPROT_ERR_CRYPTO;
	}

	(*DHKeyPair)->p = BN_new();
	if ((*DHKeyPair)->p == NULL) {
		TUTRACE((TUTRACE_ERR, "RPROTO: BN_new p failed\n"));
		return RPROT_ERR_CRYPTO;
	}

	(*DHKeyPair)->g = BN_new();
	if ((*DHKeyPair)->g == NULL) {
		TUTRACE((TUTRACE_ERR, "RPROTO: BN_new g failed\n"));
		return RPROT_ERR_CRYPTO;
	}

	/* 2. load the value of P */
	if (BN_bin2bn(DH_P_VALUE, BUF_SIZE_1536_BITS, (*DHKeyPair)->p) == NULL) {
		TUTRACE((TUTRACE_ERR, "RPROTO: load value p failed\n"));
		return RPROT_ERR_CRYPTO;
	}

	/* 3. load the value of G */
	g = WpsHtonl(DH_G_VALUE);
	if (BN_bin2bn((uint8 *)&g, 4, (*DHKeyPair)->g) == NULL) {
		TUTRACE((TUTRACE_ERR, "RPROTO: load value g failed\n"));
		return RPROT_ERR_CRYPTO;
	}

	/* 4. generate the DH key */
	ctx = BN_CTX_new();
	if (ctx == NULL)
		goto err;

	priv_key = BN_new();
	if (priv_key == NULL)
		goto err;

	pub_key = BN_new();
	if (pub_key == NULL)
		goto err;

	if (!BN_bin2bn(pre_privkey, SIZE_PUB_KEY, priv_key))
		goto err;

	if ((*DHKeyPair)->flags & DH_FLAG_CACHE_MONT_P) {
		if (((*DHKeyPair)->method_mont_p = BN_MONT_CTX_new()) != NULL)
			if (!BN_MONT_CTX_set((BN_MONT_CTX *)(*DHKeyPair)->method_mont_p,
			                     (*DHKeyPair)->p, ctx))
				goto err;
	}
	mont = (BN_MONT_CTX *)(*DHKeyPair)->method_mont_p;

	if ((*DHKeyPair)->g->top == 1) {
		BN_ULONG A = (*DHKeyPair)->g->d[0];
		if (!BN_mod_exp_mont_word(pub_key, A, priv_key, (*DHKeyPair)->p, ctx, mont))
			goto err;
	} else
		if (!BN_mod_exp_mont(pub_key, (*DHKeyPair)->g, priv_key, (*DHKeyPair)->p,
			ctx, mont))
			goto err;

	(*DHKeyPair)->pub_key = pub_key;
	(*DHKeyPair)->priv_key = priv_key;
	if (BN_num_bytes((*DHKeyPair)->pub_key) == 0)
		goto err;

	ret = WPS_SUCCESS;

err:
	if ((pub_key != NULL) && ((*DHKeyPair)->pub_key == NULL))
		BN_free(pub_key);
	if ((priv_key != NULL) && ((*DHKeyPair)->priv_key == NULL))
		BN_free(priv_key);
	if (ctx)
		BN_CTX_free(ctx);

	return ret;
}

uint32
reg_proto_generate_dhkeypair(DH **DHKeyPair)
{
	uint32 g = 0;


	/* 1. Initialize the DH structure */
	*DHKeyPair = DH_new();
	if (*DHKeyPair == NULL) {
		TUTRACE((TUTRACE_ERR, "RPROTO: DH_new failed\n"));
		return RPROT_ERR_CRYPTO;
	}

	(*DHKeyPair)->p = BN_new();
	if ((*DHKeyPair)->p == NULL) {
		TUTRACE((TUTRACE_ERR, "RPROTO: BN_new p failed\n"));
		return RPROT_ERR_CRYPTO;
	}

	(*DHKeyPair)->g = BN_new();
	if ((*DHKeyPair)->g == NULL) {
		TUTRACE((TUTRACE_ERR, "RPROTO: BN_new g failed\n"));
		return RPROT_ERR_CRYPTO;
	}

	/* 2. load the value of P */
	if (BN_bin2bn(DH_P_VALUE, BUF_SIZE_1536_BITS, (*DHKeyPair)->p) == NULL) {
		TUTRACE((TUTRACE_ERR, "RPROTO: load value p failed\n"));
		return RPROT_ERR_CRYPTO;
	}

	/* 3. load the value of G */
	g = WpsHtonl(DH_G_VALUE);
	if (BN_bin2bn((uint8 *)&g, 4, (*DHKeyPair)->g) == NULL) {
		TUTRACE((TUTRACE_ERR, "RPROTO: load value g failed\n"));
		return RPROT_ERR_CRYPTO;
	}

	/* 4. generate the DH key */
	if (WPS_DH_GENERATE_KEY(NULL, *DHKeyPair) == 0) {
		TUTRACE((TUTRACE_ERR, "RPROTO: DH generate key failed\n"));
		return RPROT_ERR_CRYPTO;
	}

	return WPS_SUCCESS;
}

void
reg_proto_generate_sha256hash(BufferObj *inBuf, BufferObj *outBuf)
{
	uint8 Hash[SIZE_256_BITS];
	if (SHA256(inBuf->pBase, inBuf->m_dataLength, Hash) == NULL) {
		TUTRACE((TUTRACE_ERR, "RPROTO: SHA256 calculation failed\n"));
		return;
	}
	buffobj_Append(outBuf, SIZE_256_BITS, Hash);
}

void
reg_proto_derivekey(BufferObj *KDK, BufferObj *prsnlString, uint32 keyBits, BufferObj *key)
{
	uint32 i = 0, iterations = 0;
	BufferObj *input, *output;
	uint8 hmac[SIZE_256_BITS];
	uint32 hmacLen = 0;
	uint8 *inPtr;
	uint32 temp;

	input = buffobj_new();
	if (!input)
		return;
	output = buffobj_new();
	if (!output) {
		buffobj_del(input);
		return;
	}

	TUTRACE((TUTRACE_INFO, "RPROTO: Deriving a key of %d bits\n", keyBits));

	iterations = ((keyBits/8) + PRF_DIGEST_SIZE - 1)/PRF_DIGEST_SIZE;

	/*
	 * Prepare the input buffer. During the iterations, we need only replace the
	 * value of i at the start of the buffer.
	 */
	temp = WpsHtonl(i);
	buffobj_Append(input, SIZE_4_BYTES, (uint8 *)&temp);
	buffobj_Append(input, prsnlString->m_dataLength, prsnlString->pBase);
	temp = WpsHtonl(keyBits);
	buffobj_Append(input, SIZE_4_BYTES, (uint8 *)&temp);
	inPtr = input->pBase;

	for (i = 0; i < iterations; i++) {
		/* Set the current value of i at the start of the input buffer */
		*(uint32 *)inPtr = WpsHtonl(i+1) ; /* i should start at 1 */
		hmac_sha256(KDK->pBase, SIZE_256_BITS, input->pBase,
			input->m_dataLength, hmac, &hmacLen);
		buffobj_Append(output, hmacLen, hmac);
	}

	/* Sanity check */
	if (keyBits/8 > output->m_dataLength) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Key derivation generated less bits "
			"than asked\n"));
		buffobj_del(output);
		buffobj_del(input);
		return;
	}

	/*
	 * We now have at least the number of key bits requested.
	 * Return only the number of bits asked for. Discard the excess.
	 */
	buffobj_Append(key, keyBits/8, output->pBase);
	buffobj_del(output);
	buffobj_del(input);
	TUTRACE((TUTRACE_INFO, "RPROTO: End Deriving a key of %d bits\n", keyBits));
}

bool
reg_proto_validate_mac(BufferObj *data, uint8 *hmac, BufferObj *key)
{
	uint8 dataMac[BUF_SIZE_256_BITS];

	/* First calculate the hmac of the data */
	hmac_sha256(key->pBase, SIZE_256_BITS, data->pBase,
		data->m_dataLength, dataMac, NULL);

	/* next, compare it against the received hmac */
	TUTRACE((TUTRACE_INFO, "RPROTO: Verifying the first 64 bits of the generated HMAC\n"));

	if (memcmp(dataMac, hmac, SIZE_64_BITS) != 0) {
		TUTRACE((TUTRACE_ERR, "RPROTO: HMAC results don't match\n"));
		return false;
	}
	return true;
}

bool reg_proto_validate_keywrapauth(BufferObj *data, uint8 *hmac, BufferObj *key)
{
	/* Same as ValidateMac, except only the first 64 bits are validated */
	uint8 dataMac[BUF_SIZE_256_BITS];

	/* First calculate the hmac of the data */
	hmac_sha256(key->pBase, SIZE_256_BITS, data->pBase,
		data->m_dataLength, dataMac, NULL);

	/* next, compare it against the received hmac */
	if (memcmp(dataMac, hmac, SIZE_64_BITS) != 0) {
		TUTRACE((TUTRACE_ERR, "RPROTO: HMAC results don't match\n"));
		return false;
	}

	return true;
}

void
reg_proto_encrypt_data(BufferObj *plainText, BufferObj *encrKey, BufferObj *authKey,
	BufferObj *cipherText, BufferObj *iv)
{
	BufferObj *buf = buffobj_new();
	uint8 ivBuf[SIZE_128_BITS];
	/*  10 rounds for cbc 128  = (10+1) * 4 uint32 */
	uint32 rk[44];
	uint8 outBuf[1024];
	int encrypt_len;

	if (!buf)
		return;

	if (plainText->m_dataLength == 0) {
		TUTRACE((TUTRACE_ERR, "Invalid parameters \n"));
		buffobj_del(buf);
		return;
	}

	/* Generate a random iv */
	RAND_bytes(ivBuf, SIZE_128_BITS);
	buffobj_Reset(iv);
	buffobj_Append(iv, SIZE_128_BITS, ivBuf);

	/* Now encrypt the plaintext and mac using the encryption key and IV. */
	buffobj_Append(buf, plainText->m_dataLength, plainText->pBase);

	TUTRACE((TUTRACE_ERR, "RPROTO: calling encryption of %d bytes\n", buf->m_dataLength));
	rijndaelKeySetupEnc(rk, encrKey->pBase, 128);
	encrypt_len = aes_cbc_encrypt_pad(rk, 16, ivBuf, buf->m_dataLength,
		plainText->pBase, outBuf, PAD_LEN_PADDING);
	buffobj_Append(cipherText, encrypt_len, outBuf);
	buffobj_del(buf);
}

void
reg_proto_decrypt_data(BufferObj *cipherText, BufferObj *iv, BufferObj *encrKey,
	BufferObj *authKey, BufferObj *plainText)
{
	/*  10 rounds for cbc 128  = (10+1) * 4 uint32 */
	uint32 rk[44];
	uint8 outBuf[1024];
	int plaintext_len;

	TUTRACE((TUTRACE_ERR, "RPROTO: calling decryption\n"));
	rijndaelKeySetupDec(rk, encrKey->pBase, 128);
	plaintext_len = aes_cbc_decrypt_pad(rk, 16, iv->pBase, cipherText->m_dataLength,
		cipherText->pBase, outBuf, PAD_LEN_PADDING);
	buffobj_Append(plainText, plaintext_len, outBuf);
	buffobj_RewindLength(plainText, plainText->m_dataLength);
}

uint32
reg_proto_generate_psk(IN uint32 length, OUT BufferObj *PSK)
{
	uint8 temp[1024];

	if (length == 0 || length > 1024)
		return WPS_ERR_INVALID_PARAMETERS;

	RAND_bytes(temp, length);
	buffobj_Append(PSK, length, temp);

	return WPS_SUCCESS;
}

uint32
reg_proto_check_nonce(IN uint8 *nonce, IN BufferObj *msg, IN int nonceType)
{
	uint16 type;

	if ((nonceType != WPS_ID_REGISTRAR_NONCE) &&
		(nonceType !=  WPS_ID_ENROLLEE_NONCE)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Invalid attribute ID passed to"
			" CheckNonce\n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	while (1) {
		type = buffobj_NextType(msg);
		if (!type)
			break;
		if (nonceType == type) {
			if (!(memcmp(nonce, msg->pCurrent+sizeof(WpsTlvHdr),
				SIZE_128_BITS))) {
				buffobj_Rewind(msg);
				return WPS_SUCCESS;
			}
			else {
				TUTRACE((TUTRACE_ERR, "RPROTO: Nonce mismatch\n"));
				buffobj_Rewind(msg);
				return RPROT_ERR_NONCE_MISMATCH;
			}
		}

		/*
		 * advance past the TLV - the total number of bytes to advance is
		 * the size of the TLV header + the length indicated in the header
		 */
		if (!(buffobj_Advance(msg, sizeof(WpsTlvHdr) +
			WpsNtohs((msg->pCurrent+sizeof(uint16)))))) {
			TUTRACE((TUTRACE_ERR, "RPROTO: Didn't find nonce\n"));
			break;
		 }
	}

	buffobj_Rewind(msg);
	return RPROT_ERR_REQD_TLV_MISSING;
}

uint32
reg_proto_get_msg_type(uint32 *msgType, BufferObj *msg)
{
	CTlvVersion version;
	CTlvMsgType bufMsgType;
	int err = 0;

	err |= tlv_dserialize(&version, WPS_ID_VERSION, msg, 0, 0);
	memset(&bufMsgType, 0, sizeof(CTlvMsgType));
	err |= tlv_dserialize(&bufMsgType, WPS_ID_MSG_TYPE, msg, 0, 0);
	*msgType = bufMsgType.m_data;
	buffobj_Rewind(msg);

	if (err)
		return WPS_ERR_INVALID_PARAMETERS;
	else
		return WPS_SUCCESS;
}

uint32
reg_proto_get_nonce(uint8 *nonce, BufferObj *msg, int nonceType)
{
	uint16 type;

	if ((nonceType != WPS_ID_REGISTRAR_NONCE) &&
	    (nonceType !=  WPS_ID_ENROLLEE_NONCE)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Invalid attribute ID passed to CheckNonce\n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	while (1) {
		type = buffobj_NextType(msg);
		if (!type)
			break;
		if (nonceType == type) {
			/* copy it */
			memcpy(nonce, msg->pCurrent+sizeof(WpsTlvHdr), SIZE_128_BITS);
			buffobj_Rewind(msg);
			return WPS_SUCCESS;
		}

		/*
		 * advance past the TLV - the total number of bytes to advance is
		 * the size of the TLV header + the length indicated in the header
		 */
		if (!(buffobj_Advance(msg, sizeof(WpsTlvHdr) +
			WpsNtohs((msg->pCurrent+sizeof(uint16)))))) {
			TUTRACE((TUTRACE_ERR, "RPROTO: Didn't find nonce\n"));
			break;
		 }
	}

	buffobj_Rewind(msg);
	return RPROT_ERR_REQD_TLV_MISSING;
}

/* Add support for Windows Rally Vertical Pairing */
uint32
reg_proto_vendor_ext_vp(DevInfo *devInfo, BufferObj *msg)
{
#ifdef WCN_NET_SUPPORT
	CTlvVendorExt   vendorExt;
	uint16          data = 0;
	BufferObj       *pBufObj = NULL;

	if ((devInfo == NULL) || (msg == NULL)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Invalid parameters! \n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	TUTRACE((TUTRACE_INFO, "RPROTO: Adding WCN-NET VP Vendor Extension \n"));

	pBufObj   = buffobj_new();
	data = (WPS_MS_VPI_TRANSPORT_UPNP << 8) | WPS_MS_VPI_PROFILE_REQ;

	if (pBufObj == NULL) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Ran out of memory!\n"));
		return WPS_ERR_OUTOFMEMORY;
	}

	/* Add the VPI TLV */
	tlv_serialize_ve(MS_VENDOR_EXT_ID, WPS_MS_ID_VPI, pBufObj, &data, sizeof(data));

	/* Add the Transport UUID TLV */
	tlv_serialize_ve(MS_VENDOR_EXT_ID, WPS_MS_ID_TRANSPORT_UUID, pBufObj,
		devInfo->transport_uuid, SIZE_16_BYTES);

	/* Serialize subelemetns to Vendor Extension */
	vendorExt.vendorId = (uint8 *)MS_VENDOR_EXT_ID;
	vendorExt.vendorData = buffobj_GetBuf(pBufObj);
	vendorExt.dataLength = buffobj_Length(pBufObj);
	tlv_vendorExtWrite(&vendorExt, msg);

	buffobj_del(pBufObj);
	TUTRACE((TUTRACE_INFO, "RPROTO: Finished Adding WCN-NET VP Vendor Extension \n"));
#endif /* WCN_NET_SUPPORT */
	return WPS_SUCCESS;
}

unsigned char *
reg_proto_generate_priv_key(unsigned char *priv_key, unsigned char *pub_key_hash)
{
	DH *DHKeyPair = NULL;
	unsigned char hash[SIZE_256_BITS], pub_key[SIZE_PUB_KEY];
	unsigned char *ret_key = NULL;

	if (!priv_key || !pub_key_hash)
		return NULL;

	if (reg_proto_generate_dhkeypair(&DHKeyPair) != WPS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Generate DH key pair failed\n"));
		goto error;
	}

	if (reg_proto_BN_bn2bin(DHKeyPair->priv_key, priv_key) != SIZE_PUB_KEY) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Private key BN to bin failed\n"));
		goto error;
	}

	if (reg_proto_BN_bn2bin(DHKeyPair->pub_key, pub_key) != SIZE_PUB_KEY) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Public key BN to bin failed\n"));
		goto error;
	}

	if (SHA256(pub_key, SIZE_PUB_KEY, hash) == NULL) {
		TUTRACE((TUTRACE_ERR, "RPROTO: SHA256 calculation failed\n"));
		goto error;
	}

	ret_key = priv_key;
	memcpy(pub_key_hash, hash, SIZE_160_BITS);

error:
	if (DHKeyPair)
		DH_free(DHKeyPair);

	return ret_key;
}
