/*
 * Registrataion protocol messages
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: reg_proto_msg.c 292744 2011-10-28 07:54:57Z $
 */

#include <wps_dh.h>
#include <wps_sha256.h>

#include <tutrace.h>
#include <wpsheaders.h>
#include <wpscommon.h>
#include <wpserror.h>
#include <reg_protomsg.h>

static void reg_msg_m7enr_del(EsM7Enr *es, bool content_only);
static void reg_msg_m7ap_del(EsM7Ap *es, bool content_only);
static void reg_msg_m8ap_del(EsM8Ap *es, bool content_only);
static void reg_msg_m8sta_del(EsM8Sta *es, bool content_only);

/*
 * Init messages based on their types. Mainly use for structures including
 * complex tlv's like EncrSettings.
 */
void
reg_msg_init(void *m, int type)
{
	switch (type) {
	case WPS_ID_MESSAGE_M1:
		memset(m, 0, sizeof(WpsM1));
		break;
	case WPS_ID_MESSAGE_M2:
		memset(m, 0, sizeof(WpsM2));
		break;
	case WPS_ID_MESSAGE_M2D:
		memset(m, 0, sizeof(WpsM2D));
		break;
	case WPS_ID_MESSAGE_M3:
		memset(m, 0, sizeof(WpsM3));
		break;
	case WPS_ID_MESSAGE_M4:
		memset(m, 0, sizeof(WpsM4));
		break;
	case WPS_ID_MESSAGE_M5:
		memset(m, 0, sizeof(WpsM5));
		break;
	case WPS_ID_MESSAGE_M6:
		memset(m, 0, sizeof(WpsM6));
		break;
	case WPS_ID_MESSAGE_M7:
		memset(m, 0, sizeof(WpsM7));
		break;
	case WPS_ID_MESSAGE_M8:
		memset(m, 0, sizeof(WpsM8));
		break;
	case WPS_ID_MESSAGE_ACK:
		memset(m, 0, sizeof(WpsACK));
		break;
	case WPS_ID_MESSAGE_NACK:
		memset(m, 0, sizeof(WpsNACK));
		break;
	case WPS_ID_MESSAGE_DONE:
		memset(m, 0, sizeof(WpsDone));
		break;
	default:
		break;
	}
}

int
reg_msg_version_check(uint8 msgId, BufferObj *theBuf, TlvObj_uint8 *version, TlvObj_uint8 *msgType)
{
	int err = 0;

	err |= tlv_dserialize(version, WPS_ID_VERSION, theBuf, 0, 0);
	err |= tlv_dserialize(msgType, WPS_ID_MSG_TYPE, theBuf, 0, 0);

	if (err)
		return RPROT_ERR_REQD_TLV_MISSING; /* for DTM 1.1 */

	/* Must be 0x10, See WSC2.0 Clause 7.9 Version Negotiation */
	if (version->m_data != WPS_VERSION) {
		TUTRACE((TUTRACE_ERR, "message version should always set to 1\n"));
		return RPROT_ERR_INCOMPATIBLE;
	}
	if (msgId != msgType->m_data) {
		TUTRACE((TUTRACE_ERR, "message type %d differs from desired type %d\n",
			msgType->m_data, msgId));
		return RPROT_ERR_WRONG_MSGTYPE;
	}

	return WPS_SUCCESS;
}

/* Encrypted settings for M4, M5, M6 */
void
reg_msg_nonce_parse(TlvEsNonce *t, uint16 theType, BufferObj *theBuf, BufferObj *authKey)
{
	uint8 dataMac[BUF_SIZE_256_BITS];
	uint8 hmac_len;

	tlv_dserialize(&t->nonce, theType, theBuf, SIZE_128_BITS, 0);

	/* Skip attributes until the KeyWrapAuthenticator */
	while (buffobj_NextType(theBuf) != WPS_ID_KEY_WRAP_AUTH) {
		/* advance past the TLV */
		uint8 *Pos = buffobj_Advance(theBuf, sizeof(WpsTlvHdr) +
			WpsNtohs((theBuf->pCurrent+sizeof(uint16))));

		 if (Pos == NULL)
			return;
	}

	/* save the length deserialized so far */
	hmac_len = (uint8)(theBuf->pCurrent - theBuf->pBase);
	tlv_dserialize(&t->keyWrapAuth, WPS_ID_KEY_WRAP_AUTH, theBuf, SIZE_64_BITS, 0);

	/* calculate the hmac of the data (data only, not the last auth TLV) */
	hmac_sha256(authKey->pBase, SIZE_256_BITS, theBuf->pBase,
		hmac_len, dataMac, NULL);

	/* compare it against the received hmac */
	if (memcmp(dataMac, t->keyWrapAuth.m_data, SIZE_64_BITS) != 0) {
		TUTRACE((TUTRACE_ERR, "RPROTO: HMAC results don't match\n"));
		return;
	}
}

void
reg_msg_nonce_write(TlvEsNonce *t, BufferObj *theBuf, BufferObj *authKey)
{
	uint8 hmac[SIZE_256_BITS];

	tlv_write(&t->nonce, theBuf);

	/* calculate the hmac and append the TLV to the buffer */
	hmac_sha256(authKey->pBase, SIZE_256_BITS,
		theBuf->pBase, theBuf->m_dataLength, hmac, NULL);

	tlv_serialize(WPS_ID_KEY_WRAP_AUTH, theBuf, hmac,
		SIZE_64_BITS); /* Only the first 64 bits are sent */
}

static void
reg_msg_m7enr_del(EsM7Enr *es, bool content_only)
{
	tlv_delete(&es->nonce, 1);
	tlv_delete(&es->idProof, 1);
	tlv_delete(&es->keyWrapAuth, 1);

	if (!content_only)
		free(es);
}

/* Encrypted settings for M7 ES when M7 is from an enrollee */
uint32
reg_msg_m7enr_parse(EsM7Enr *t, BufferObj *theBuf, BufferObj *authKey, bool allocate)
{
	uint8 dataMac[BUF_SIZE_256_BITS];
	uint8 hmac_len = 0;
	uint32 err;

	err = tlv_dserialize(&t->nonce, WPS_ID_E_SNONCE2, theBuf, SIZE_128_BITS, 0);

	if (buffobj_NextType(theBuf) == WPS_ID_IDENTITY_PROOF) {
		err |= tlv_dserialize(&t->idProof, WPS_ID_IDENTITY_PROOF, theBuf, 0, allocate);
	}

	/* Skip attributes until the KeyWrapAuthenticator */
	while (buffobj_NextType(theBuf) != WPS_ID_KEY_WRAP_AUTH) {
		/* advance past the TLV */
		uint8 *Pos = buffobj_Advance(theBuf, sizeof(WpsTlvHdr) +
			WpsNtohs((theBuf->pCurrent+sizeof(uint16))));
		if (Pos == NULL)
			return RPROT_ERR_REQD_TLV_MISSING;
	}

	/* save the length deserialized so far */
	hmac_len = (uint8)(theBuf->pCurrent - theBuf->pBase);

	err |= tlv_dserialize(&t->keyWrapAuth, WPS_ID_KEY_WRAP_AUTH, theBuf, SIZE_64_BITS, 0);

	/* validate the mac */

	/* calculate the hmac of the data (data only, not the last auth TLV) */
	hmac_sha256(authKey->pBase, SIZE_256_BITS, theBuf->pBase,
		hmac_len, dataMac, NULL);

	/* compare it against the received hmac */
	if (memcmp(dataMac, t->keyWrapAuth.m_data, SIZE_64_BITS) != 0) {
		TUTRACE((TUTRACE_ERR, "RPROTO: HMAC results don't match\n"));
		return RPROT_ERR_CRYPTO;
	}

	if (err)
		return WPS_ERR_GENERIC;
	else
		return WPS_SUCCESS;
}

void
reg_msg_m7enr_write(EsM7Enr *t, BufferObj *theBuf, BufferObj *authKey)
{
	uint8 hmac[SIZE_256_BITS];

	tlv_write(&t->nonce, theBuf);
	if (t->idProof.tlvbase.m_len) {
		tlv_write(&t->idProof, theBuf);
	}

	/* calculate the hmac and append the TLV to the buffer */
	hmac_sha256(authKey->pBase, SIZE_256_BITS,
		theBuf->pBase, theBuf->m_dataLength, hmac, NULL);
	tlv_serialize(WPS_ID_KEY_WRAP_AUTH, theBuf, hmac,
		SIZE_64_BITS);
}

static void
reg_msg_m7ap_del(EsM7Ap *es, bool content_only)
{
	WPS_SSLIST *itr, *next;
	CTlvNwKeyIndex *keyIndex;
	CTlvNwKey *key;

	itr = es->nwKeyIndex;

	while ((keyIndex = (CTlvNwKeyIndex *)itr)) {
		next = itr->next;
		tlv_delete((tlvbase_s *)keyIndex, 0);
		itr = next;
	}

	itr = es->nwKey;

	while ((key = (CTlvNwKey *)itr)) {
		next = itr->next;
		tlv_delete((tlvbase_s *)key, 0);
		itr = next;
	}

	tlv_delete(&es->macAddr, 1);
	tlv_delete(&es->ssid, 1);
	if (!content_only)
		free(es);
	return;
}

uint32
reg_msg_m7ap_parse(EsM7Ap *tlv, BufferObj *theBuf, BufferObj *authKey, bool allocate)
{
	CTlvNwKeyIndex *keyIndex;
	CTlvNwKey *key;
	uint8 dataMac[BUF_SIZE_256_BITS];
	uint8 hmac_len = 0;
	int err = 0;

	tlv_dserialize(&tlv->nonce, WPS_ID_E_SNONCE2, theBuf, SIZE_128_BITS, 0);
	tlv_dserialize(&tlv->ssid, WPS_ID_SSID, theBuf, SIZE_32_BYTES, allocate);
	tlv_dserialize(&tlv->macAddr, WPS_ID_MAC_ADDR, theBuf, SIZE_6_BYTES, allocate);
	tlv_dserialize(&tlv->authType, WPS_ID_AUTH_TYPE, theBuf, 0, 0);
	tlv_dserialize(&tlv->encrType, WPS_ID_ENCR_TYPE, theBuf, 0, 0);

	/* The next field is network key index. There are two possibilities: */
	/* 1. The TLV is omitted, in which case, there is only 1 network key */
	/* 2. The TLV is present, in which case, there may be 1 or more network keys */

	/* condition 1. If the next field is a network Key, the index TLV was omitted */
	if (buffobj_NextType(theBuf) == WPS_ID_NW_KEY) {
		key = (CTlvNwKey *)tlv_new(WPS_ID_NW_KEY);
		if (!key)
			return WPS_ERR_OUTOFMEMORY;
		err |= tlv_dserialize(key, WPS_ID_NW_KEY, theBuf, SIZE_64_BYTES, allocate);
		if (!wps_sslist_add(&tlv->nwKey, key)) {
			tlv_delete(key, 0);
			return WPS_ERR_OUTOFMEMORY;
		}
	}
	else {
		/* condition 2. all other possibities are illegal & will be caught later */
		while (buffobj_NextType(theBuf) == WPS_ID_NW_KEY_INDEX) {
			keyIndex = (CTlvNwKeyIndex *)tlv_new(WPS_ID_NW_KEY_INDEX);
			if (!keyIndex)
				return WPS_ERR_OUTOFMEMORY;
			err |= tlv_dserialize(keyIndex, WPS_ID_NW_KEY_INDEX, theBuf, 0, 0);
			if (!wps_sslist_add(&tlv->nwKeyIndex, keyIndex)) {
				tlv_delete(keyIndex, 0);
				return WPS_ERR_OUTOFMEMORY;
			}

			key = (CTlvNwKey *)tlv_new(WPS_ID_NW_KEY);
			if (!key)
				return WPS_ERR_OUTOFMEMORY;
			err |= tlv_dserialize(key, WPS_ID_NW_KEY, theBuf, SIZE_64_BYTES, allocate);
			if (!wps_sslist_add(&tlv->nwKey, key)) {
				tlv_delete(key, 0);
				return WPS_ERR_OUTOFMEMORY;
			}
		}
	}

	/* Set wep index default value to index 1 */
	tlv_set(&tlv->wepIdx, WPS_ID_WEP_TRANSMIT_KEY, (void *)1, 0);

	/* Skip attributes until the KeyWrapAuthenticator */
	while (buffobj_NextType(theBuf) != WPS_ID_KEY_WRAP_AUTH) {
		if (buffobj_NextType(theBuf) == WPS_ID_WEP_TRANSMIT_KEY) {
			tlv_dserialize(&tlv->wepIdx, WPS_ID_WEP_TRANSMIT_KEY, theBuf, 0, 0);
			TUTRACE((TUTRACE_ERR, "CTlvEsM7Ap::parse : deserialize wepkeyIndex "
				"%d allocate: %d\n", tlv->wepIdx.m_data, 0));
		}
		else {
			/* advance past the TLV */
			uint8 *Pos = buffobj_Advance(theBuf, sizeof(WpsTlvHdr) +
			WpsNtohs((theBuf->pCurrent+sizeof(uint16))));
			if (Pos == NULL)
				return RPROT_ERR_REQD_TLV_MISSING;
		}
	}

	/* save the length deserialized so far */
	hmac_len = (uint8)(theBuf->pCurrent - theBuf->pBase);

	err |= tlv_dserialize(&tlv->keyWrapAuth, WPS_ID_KEY_WRAP_AUTH, theBuf, SIZE_64_BITS, 0);

	/* validate the mac */

	/* calculate the hmac of the data (data only, not the last auth TLV) */
	hmac_sha256(authKey->pBase, SIZE_256_BITS, theBuf->pBase,
		hmac_len, dataMac, NULL);

	/* compare it against the received hmac */
	if (memcmp(dataMac, tlv->keyWrapAuth.m_data, SIZE_64_BITS) != 0) {
		TUTRACE((TUTRACE_ERR, "RPROTO: HMAC results don't match\n"));
		return RPROT_ERR_CRYPTO;
	}

	if (err)
		return WPS_ERR_GENERIC;
	else
		return WPS_SUCCESS;
}

void
reg_msg_m7ap_write(EsM7Ap *tlv, BufferObj *theBuf, BufferObj *authKey)
{
	WPS_SSLIST *indexItr, *keyItr;
	CTlvNwKeyIndex *keyIndex;
	CTlvNwKey *key;
	uint8 hmac[SIZE_256_BITS];

	indexItr = tlv->nwKeyIndex;

	keyItr = tlv->nwKey;

	tlv_write(&tlv->nonce, theBuf);
	tlv_write(&tlv->ssid, theBuf);
	tlv_write(&tlv->macAddr, theBuf);
	tlv_write(&tlv->authType, theBuf);
	tlv_write(&tlv->encrType, theBuf);

	/* write the network index and network key to the buffer */
	if (tlv->nwKeyIndex == 0) {
		/* Condition1. There is no key index, so there can only be 1 nw key */
		if (!(key = (CTlvNwKey *)keyItr)) {
			TUTRACE((TUTRACE_ERR, "reg_msg_m7ap_write : out of memory\n"));
			goto done;
		}
		tlv_write(key, theBuf);
	}
	else {
		/* Condition2. There are multiple network keys. */
		for (; (keyIndex = (CTlvNwKeyIndex *)indexItr);
			indexItr = indexItr->next, keyItr = keyItr->next) {
			if (!(key = (CTlvNwKey *)keyItr)) {
				TUTRACE((TUTRACE_ERR, "reg_msg_m7ap_write : out of memory\n"));
				goto done;
			}
			tlv_write(keyIndex, theBuf);
			tlv_write(key, theBuf);
		}
	}

	if (tlv->wepIdx.tlvbase.m_len && tlv->wepIdx.m_data != 1) {
		tlv_write(&tlv->wepIdx, theBuf);
		TUTRACE((TUTRACE_ERR, "write wep index!! index = %x\n",
			tlv->wepIdx.m_data));
	}

	/* calculate the hmac and append the TLV to the buffer */
	hmac_sha256(authKey->pBase, SIZE_256_BITS,
		theBuf->pBase, theBuf->m_dataLength, hmac, NULL);
	tlv_serialize(WPS_ID_KEY_WRAP_AUTH, theBuf, hmac,
		SIZE_64_BITS);

done:
	return;
}

static void
reg_msg_m8ap_del(EsM8Ap *es, bool content_only)
{
	WPS_SSLIST *itr, *next;
	CTlvNwKeyIndex *keyIndex;
	CTlvNwKey *key;

	itr = es->nwKeyIndex;

	while ((keyIndex = (CTlvNwKeyIndex *)itr)) {
		next = itr->next;
		tlv_delete((tlvbase_s *)keyIndex, 0);
		itr = next;
	}

	itr = es->nwKey;

	while ((key = (CTlvNwKey *)itr)) {
		next = itr->next;
		tlv_delete((tlvbase_s *)key, 0);
		itr = next;
	}

	tlv_delete(&es->ssid, 1);
	tlv_delete(&es->macAddr, 1);
	tlv_delete(&es->new_pwd, 1);
	if (!content_only)
		free(es);
}

void
reg_msg_m8ap_parse(EsM8Ap *t, BufferObj *theBuf, BufferObj *authKey, bool allocate)
{
	CTlvNwKeyIndex *keyIndex;
	CTlvNwKey *key;
	uint8 dataMac[BUF_SIZE_256_BITS];
	uint8 hmac_len = 0;

	/* NW Index is optional */
	if (buffobj_NextType(theBuf) == WPS_ID_NW_INDEX)
		tlv_dserialize(&t->nwIndex, WPS_ID_NW_INDEX, theBuf, 0, 0);

	tlv_dserialize(&t->ssid, WPS_ID_SSID, theBuf, SIZE_32_BYTES, allocate);
	tlv_dserialize(&t->authType, WPS_ID_AUTH_TYPE, theBuf, 0, 0);
	tlv_dserialize(&t->encrType, WPS_ID_ENCR_TYPE, theBuf, 0, 0);

	/* The next field is network key index. There are two possibilities: */
	/* 1. The TLV is omitted, in which case, there is only 1 network key */
	/* 2. The TLV is present, in which case, there may be 1 or more network keys */

	/* condition 1. If the next field is a network Key, the index TLV was omitted */
	if (buffobj_NextType(theBuf) == WPS_ID_NW_KEY) {
		key = (CTlvNwKey *)tlv_new(WPS_ID_NW_KEY);
		if (!key)
			return;
		tlv_dserialize(key, WPS_ID_NW_KEY, theBuf, SIZE_64_BYTES, allocate);
		if (!wps_sslist_add(&t->nwKey, key)) {
			tlv_delete(key, 0);
			return;
		}
	}
	else {
		/* condition 2. any other possibities are illegal & will be caught later */
		while (buffobj_NextType(theBuf) == WPS_ID_NW_KEY_INDEX) {
			keyIndex = (CTlvNwKeyIndex *)tlv_new(WPS_ID_NW_KEY_INDEX);
			if (!keyIndex)
				return;

			tlv_dserialize(keyIndex, WPS_ID_NW_KEY_INDEX, theBuf, 0, 0);
			if (!wps_sslist_add(&t->nwKeyIndex, keyIndex)) {
				tlv_delete(keyIndex, 0);
				return;
			}

			key = (CTlvNwKey *)tlv_new(WPS_ID_NW_KEY);
			if (!key)
				return;
			tlv_dserialize(key, WPS_ID_NW_KEY, theBuf, SIZE_64_BYTES, allocate);
			if (!wps_sslist_add(&t->nwKey, key)) {
				tlv_delete(key, 0);
				return;
			}
		}
	}

	tlv_dserialize(&t->macAddr, WPS_ID_MAC_ADDR, theBuf, SIZE_6_BYTES, allocate);
	if (buffobj_NextType(theBuf) == WPS_ID_NEW_PWD) {
		/* If the New Password TLV is included, the Device password ID is required */
		tlv_dserialize(&t->new_pwd, WPS_ID_NEW_PWD, theBuf, SIZE_64_BYTES, allocate);
		tlv_dserialize(&t->pwdId, WPS_ID_DEVICE_PWD_ID, theBuf, 0, 0);
	}

	if (buffobj_NextType(theBuf) == WPS_ID_WEP_TRANSMIT_KEY)
		tlv_dserialize(&t->wepIdx, WPS_ID_WEP_TRANSMIT_KEY, theBuf, 0, 0);

	if (t->wepIdx.m_data < 1 || t->wepIdx.m_data > 4)
		tlv_set(&t->wepIdx, WPS_ID_WEP_TRANSMIT_KEY, (void *)1, 0);

	/* skip Permitted Config Methods field. */
	if (buffobj_NextType(theBuf) == WPS_ID_PERM_CFG_METHODS) {
		/* advance past the TLV */
		buffobj_Advance(theBuf,  sizeof(WpsTlvHdr) +
			WpsNtohs((theBuf->pCurrent+sizeof(uint16))));
	}

	/* Skip attributes until the KeyWrapAuthenticator */
	while (buffobj_NextType(theBuf) != WPS_ID_KEY_WRAP_AUTH) {
		/* advance past the TLV */
		uint8 *Pos = buffobj_Advance(theBuf,  sizeof(WpsTlvHdr) +
			WpsNtohs((theBuf->pCurrent+sizeof(uint16))));
		if (Pos == NULL)
			return;
	}

	/* save the length deserialized so far */
	hmac_len = (uint8)(theBuf->pCurrent - theBuf->pBase);

	tlv_dserialize(&t->keyWrapAuth, WPS_ID_KEY_WRAP_AUTH, theBuf, SIZE_64_BITS, 0);

	/* validate the mac */

	/* calculate the hmac of the data (data only, not the last auth TLV) */
	hmac_sha256(authKey->pBase, SIZE_256_BITS, theBuf->pBase,
		hmac_len, dataMac, NULL);

	/* compare it against the received hmac */
	if (memcmp(dataMac, t->keyWrapAuth.m_data, SIZE_64_BITS) != 0) {
		TUTRACE((TUTRACE_ERR, "RPROTO: HMAC results don't match\n"));
		return;
	}
}

void
reg_msg_m8ap_write(EsM8Ap *t, BufferObj *theBuf, BufferObj *authKey, bool b_wps_version2)
{
	WPS_SSLIST *indexItr, *keyItr;
	CTlvNwKeyIndex *keyIndex;
	CTlvNwKey *key;
	uint8 hmac[SIZE_256_BITS];

	indexItr = t->nwKeyIndex;

	keyItr = t->nwKey;

	/*
	 * Caution: WSC 2.0, Page 77 Table 20, page 113.
	 * Remove "Network Index" in M8ap encrypted settings
	 */
	if (!b_wps_version2) {
		/* nwIndex is an optional field */
		if (t->nwIndex.tlvbase.m_len)
			tlv_write(&t->nwIndex, theBuf);
	}

	tlv_write(&t->ssid, theBuf);
	tlv_write(&t->authType, theBuf);
	tlv_write(&t->encrType, theBuf);

	/* write the network index and network key to the buffer */
	if (t->nwKeyIndex == 0) {
		/* Condition1. There is no key index, so there can only be 1 nw key */
		if (!(key = (CTlvNwKey *)keyItr))
			goto done;
		tlv_write(key, theBuf);
	}
	else {
		/* Condition2. There are multiple network keys. */
		for (; (keyIndex = (CTlvNwKeyIndex *)indexItr);
			indexItr = indexItr->next, keyItr = keyItr->next) {
			if (!(key = (CTlvNwKey *)keyItr))
				goto done;
			tlv_write(keyIndex, theBuf);
			tlv_write(key, theBuf);
		}
	}

	/* write the mac address */
	tlv_write(&t->macAddr, theBuf);

	/* write the optional new password and device password ID */
	if (t->new_pwd.tlvbase.m_len) {
		tlv_write(&t->new_pwd, theBuf);
		tlv_write(&t->pwdId, theBuf);
	}

	if (t->wepIdx.tlvbase.m_len && t->wepIdx.m_data != 1) {
		tlv_write(&t->wepIdx, theBuf);
		TUTRACE((TUTRACE_ERR, "CTlvEsM8Ap_write : wepIndex != 1, "
			"Optional field added! \n"));
	}

	/* calculate the hmac and append the TLV to the buffer */
	hmac_sha256(authKey->pBase, SIZE_256_BITS,
		theBuf->pBase, theBuf->m_dataLength, hmac, NULL);
	tlv_serialize(WPS_ID_KEY_WRAP_AUTH, theBuf, hmac,
		SIZE_64_BITS);

done:
	return;
}

static void
reg_msg_m8sta_del(EsM8Sta *es, bool content_only)
{
	WPS_SSLIST *itr, *next;
	CTlvCredential *pCredential;

	itr = es->credential;

	while ((pCredential = (CTlvCredential *)itr)) {
		next = itr->next;
		tlv_credentialDelete(pCredential, 0);
		itr = next;
	}

	tlv_delete(&es->new_pwd, 1);
	tlv_delete(&es->pwdId, 1);
	tlv_delete(&es->keyWrapAuth, 1);
	if (!content_only)
		free(es);
}

void
reg_msg_m8sta_parse(EsM8Sta *t, BufferObj *theBuf, BufferObj *authKey, bool allocate)
{
	/* There should be at least 1 credential TLV */
	CTlvCredential *pCredential;
	uint8 hmac_len = 0;
	uint8 dataMac[BUF_SIZE_256_BITS];

	pCredential = (CTlvCredential *)malloc(sizeof(CTlvCredential));
	if (!pCredential)
		return;

	memset(pCredential, 0, sizeof(CTlvCredential));

	tlv_credentialParse(pCredential, theBuf, allocate);
	if (!wps_sslist_add(&t->credential, pCredential)) {
		tlv_credentialDelete(pCredential, 0);
		return;
	}

	/* now parse any additional credential TLVs */
	while (buffobj_NextType(theBuf) == WPS_ID_CREDENTIAL) {
		pCredential = (CTlvCredential *)malloc(sizeof(CTlvCredential));
		if (!pCredential)
			return;

		memset(pCredential, 0, sizeof(CTlvCredential));
		tlv_credentialParse(pCredential, theBuf, allocate);
		if (!wps_sslist_add(&t->credential, pCredential)) {
			tlv_credentialDelete(pCredential, 0);
			return;
		}
	}

	if (buffobj_NextType(theBuf) == WPS_ID_NEW_PWD) {
		/* If the New Password TLV is included, the Device password ID is required */
		tlv_dserialize(&t->new_pwd, WPS_ID_NEW_PWD, theBuf, SIZE_64_BYTES, allocate);
		tlv_dserialize(&t->pwdId, WPS_ID_DEVICE_PWD_ID, theBuf, 0, 0);
	}

	/* Skip attributes until the KeyWrapAuthenticator */
	while (buffobj_NextType(theBuf) != WPS_ID_KEY_WRAP_AUTH) {
		/* advance past the TLV */
		uint8 *Pos = buffobj_Advance(theBuf, sizeof(WpsTlvHdr) +
			WpsNtohs((theBuf->pCurrent+sizeof(uint16))));
		if (Pos == NULL)
			return;
	}

	/* save the length deserialized so far */
	hmac_len = (uint8)(theBuf->pCurrent - theBuf->pBase);

	tlv_dserialize(&t->keyWrapAuth, WPS_ID_KEY_WRAP_AUTH, theBuf, SIZE_64_BITS, 0);

	/* validate the mac */

	/* calculate the hmac of the data (data only, not the last auth TLV) */
	hmac_sha256(authKey->pBase, SIZE_256_BITS, theBuf->pBase,
		hmac_len, dataMac, NULL);

	/* compare it against the received hmac */
	if (memcmp(dataMac, t->keyWrapAuth.m_data, SIZE_64_BITS) != 0) {
		TUTRACE((TUTRACE_ERR, "RPROTO: HMAC results don't match\n"));
		return;
	}
}

void
reg_msg_m8sta_write(EsM8Sta *t, BufferObj *theBuf)
{
	WPS_SSLIST *itr;
	CTlvCredential *pCredential;

	/* there should be at least one credential TLV */
	if (t->credential == 0) {
		TUTRACE((TUTRACE_ERR, "Error %d in function %s \n",
			RPROT_ERR_REQD_TLV_MISSING, __FUNCTION__));
		return;
	}

	itr = t->credential;

	for (; (pCredential = (CTlvCredential *)itr); itr = itr->next) {
		tlv_credentialWrite(pCredential, theBuf);
	}
}

void
reg_msg_m8sta_write_key(EsM8Sta *t, BufferObj *theBuf, BufferObj *authKey)
{
	WPS_SSLIST *itr;
	CTlvCredential *pCredential;
	uint8 hmac[SIZE_256_BITS];

	/* there should be at least one credential TLV */
	if (t->credential == 0) {
		TUTRACE((TUTRACE_ERR, "Error %d in function %s \n",
			RPROT_ERR_REQD_TLV_MISSING, __FUNCTION__));
		return;
	}

	itr = t->credential;

	for (; (pCredential = (CTlvCredential *)itr); itr = itr->next) {
		tlv_credentialWrite(pCredential, theBuf);
	}

	/* write the optional new password and device password ID */
	if (t->new_pwd.tlvbase.m_len) {
		tlv_write(&t->new_pwd, theBuf);
		tlv_write(&t->pwdId, theBuf);
	}

	/* calculate the hmac and append the TLV to the buffer */
	hmac_sha256(authKey->pBase, SIZE_256_BITS,
		theBuf->pBase, theBuf->m_dataLength, hmac, NULL);

	tlv_serialize(WPS_ID_KEY_WRAP_AUTH, theBuf, hmac,
		SIZE_64_BITS);
}

void *
reg_msg_es_new(int es_type)
{
	int size;
	int *es;

	switch (es_type) {
	case ES_TYPE_M7ENR:
		size = sizeof(EsM7Enr);
		break;
	case ES_TYPE_M7AP:
		size = sizeof(EsM7Ap);
		break;
	case ES_TYPE_M8AP:
		size = sizeof(EsM8Ap);
		break;
	case ES_TYPE_M8STA:
		size = sizeof(EsM8Sta);
		break;
	default:
		TUTRACE((TUTRACE_ERR, "Unknown ES type %d\n", es_type));
		return NULL;
	}

	if ((es = (int *)malloc(size)) == NULL) {
		TUTRACE((TUTRACE_ERR, "out of memory\n"));
		return NULL;
	}
	memset(es, 0, size);

	*es = es_type;
	return es;
}

void
reg_msg_es_del(void *es, bool content_only)
{
	int *es_type = (int *)es;

	if (es == NULL) {
		TUTRACE((TUTRACE_ERR, "Try to delete NULL ES pointer\n"));
		return;
	}

	switch (*es_type) {
	case ES_TYPE_M7ENR:
		reg_msg_m7enr_del((EsM7Enr *)es, content_only);
		break;
	case ES_TYPE_M7AP:
		reg_msg_m7ap_del((EsM7Ap *)es, content_only);
		break;
	case ES_TYPE_M8AP:
		reg_msg_m8ap_del((EsM8Ap *)es, content_only);
		break;
	case ES_TYPE_M8STA:
		reg_msg_m8sta_del((EsM8Sta *)es, content_only);
		break;
	default:
		TUTRACE((TUTRACE_ERR, "Unknown ES type %d\n", *es_type));
		break;
	}
}
