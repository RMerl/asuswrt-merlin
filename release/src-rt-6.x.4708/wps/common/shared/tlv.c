/*
 * TLV
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: tlv.c 383924 2013-02-08 04:14:39Z $
 */

#include <wpstypes.h>
#include "wpscommon.h"
#include <reg_prototlv.h>
#include "tutrace.h"

/* PHIL for debug only */

static uint32 tlv_freemem = 0;
static uint32 tlv_allocmem = 0;

int
tlv_gettype(uint16 theType)
{
	switch (theType) {
	/* bool */
	case WPS_ID_RADIO_ENABLED:
	case WPS_ID_REBOOT:
	case WPS_ID_REGISTRAR_ESTBLSHD:
	case WPS_ID_SEL_REGISTRAR:
	case WPS_ID_PORTABLE_DEVICE:
	case WPS_ID_AP_SETUP_LOCKED:
	case WPS_ID_KEY_PROVIDED_AUTO:
	case WPS_ID_8021X_ENABLED:
	/* uint8 */
	case WPS_ID_CONN_TYPE:
	case WPS_ID_CONN_TYPE_FLAGS:
	case WPS_ID_MSG_TYPE:
	case WPS_ID_NW_INDEX:
	case WPS_ID_NW_KEY_INDEX:
	case WPS_ID_POWER_LEVEL:
	case WPS_ID_PSK_CURRENT:
	case WPS_ID_PSK_MAX:
	case WPS_ID_REGISTRAR_CURRENT:
	case WPS_ID_REGISTRAR_MAX:
	case WPS_ID_REQ_TYPE:
	case WPS_ID_RESP_TYPE:
	case WPS_ID_RF_BAND:
	case WPS_ID_SC_STATE:
	case WPS_ID_TOT_NETWORKS:
	case WPS_ID_VERSION:
	case WPS_ID_WEP_TRANSMIT_KEY:
		return TLV_UINT8;

	/* uint16 */
	case WPS_ID_AP_CHANNEL:
	case WPS_ID_ASSOC_STATE:
	case WPS_ID_AUTH_TYPE:
	case WPS_ID_AUTH_TYPE_FLAGS:
	case WPS_ID_CONFIG_METHODS:
	case WPS_ID_CONFIG_ERROR:
	case WPS_ID_DEVICE_PWD_ID:
	case WPS_ID_ENCR_TYPE:
	case WPS_ID_ENCR_TYPE_FLAGS:
	case WPS_ID_PERM_CFG_METHODS:
	case WPS_ID_SEL_REG_CFG_METHODS:
		return TLV_UINT16;

	/* uint 32 */
	case WPS_ID_FEATURE_ID:
	case WPS_ID_OS_VERSION:
	case WPS_ID_KEY_LIFETIME:
		return TLV_UINT32;

	/* char*  */
	case WPS_ID_CONF_URL4:
	case WPS_ID_CONF_URL6:
	case WPS_ID_DEVICE_NAME:
	case WPS_ID_IDENTITY:
	case WPS_ID_IDENTITY_PROOF:
	case WPS_ID_MANUFACTURER:
	case WPS_ID_MODEL_NAME:
	case WPS_ID_MODEL_NUMBER:
	case WPS_ID_NW_KEY:
	case WPS_ID_NEW_DEVICE_NAME:
	case WPS_ID_NEW_PWD:
	case WPS_ID_SERIAL_NUM:
	case WPS_ID_APP_LIST:
		return TLV_CHAR_PTR;

	/* uint8*  */
	case WPS_ID_MAC_ADDR:
	case WPS_ID_AUTHENTICATOR:
	case WPS_ID_KEY_WRAP_AUTH:
	case WPS_ID_R_SNONCE1:
	case WPS_ID_E_SNONCE1:
	case WPS_ID_R_SNONCE2:
	case WPS_ID_E_SNONCE2:
	case WPS_ID_ENROLLEE_NONCE:
	case WPS_ID_KEY_IDENTIFIER:
	case WPS_ID_REGISTRAR_NONCE:
	case WPS_ID_UUID_E:
	case WPS_ID_UUID_R:
	case WPS_ID_PUBKEY_HASH:
	case WPS_ID_E_HASH1:
	case WPS_ID_E_HASH2:
	case WPS_ID_R_HASH1:
	case WPS_ID_R_HASH2:
	case WPS_ID_INIT_VECTOR:
	case WPS_ID_SSID:
	case WPS_ID_REKEY_KEY:
	case WPS_ID_PUBLIC_KEY:
	case WPS_ID_EAP_IDENTITY:
	case WPS_ID_X509_CERT:
	case WPS_ID_X509_CERT_REQ:
	case WPS_ID_EAP_TYPE:
		return TLV_UINT8_PTR;

	case  WPS_ID_CREDENTIAL:
		return TLV_CREDENTIAL;
	}

	return 0;
}

tlvbase_s *
tlv_new(uint16 theType)
{
	tlvbase_s *b;
	int tlvtype = tlv_gettype(theType);
	switch (tlvtype) {
	case TLV_UINT8:
	{
		b = (tlvbase_s *)malloc(sizeof(TlvObj_uint8));
	}
	break;

	case TLV_UINT16:
	{
		b = (tlvbase_s *)malloc(sizeof(TlvObj_uint16));
	}
	break;

	case TLV_UINT32:
	{
		b = (tlvbase_s *)malloc(sizeof(TlvObj_uint32));
	}
	break;

	case TLV_UINT8_PTR:
	case TLV_CHAR_PTR:
	{
		TlvObj_ptru *t;
		t = (TlvObj_ptru *)malloc(sizeof(TlvObj_ptru));
		if (t) {
			t->m_allocated = false;
			t->m_data = 0;
		}
		b = (tlvbase_s *)t;
	}
	break;

	default:
		TUTRACE((TUTRACE_ERR, "Trying to allocate an unknown tlv type \n"));
		return NULL;
	}

	if (b) {
		b->m_len = 0;
		b->m_type = theType;
	}

	return b;
}

/* allocate memory if appropriate, i.e.not for integer types of TLV */
int
tlv_allocate_imp(void *v, uint16 theType, uint16 len, const char *func, int line)
{
	tlvbase_s *b = (tlvbase_s *)v;
	int tlvtype = tlv_gettype(theType);

	switch (tlvtype) {
	case TLV_CHAR_PTR:
	case TLV_UINT8_PTR:
	{
		TlvObj_ptr *t = (TlvObj_ptr *)b;
		b->m_len = len;
		t->m_allocated = false;
		b->m_type = theType;
		if (len) {
			 /* extra one byte for NULL end */
			t->m_data = (char *)malloc(b->m_len + 1);
			if (!t->m_data) {
				TUTRACE((TUTRACE_ERR, "tlv allocate : out of memory !!"));
				return -1;
			}
			tlv_allocmem++;
			/* clear it */
			memset(t->m_data, 0, b->m_len + 1);
			t->m_allocated = true;
		}
		else {
			t->m_data = NULL;
		}
	}
	break;

	default:
		TUTRACE((TUTRACE_ERR, "tlv allocate : trying to allocate a non-pointer "
			"tlv object\n"));
	}

	return 0;
}

int
tlv_dserialize_imp(void *v, uint16 theType, BufferObj *theBuf, uint16 dataSize, bool alloc,
	const char *func, int line)
{
	tlvbase_s *b = (tlvbase_s *)v;
	int tlvtype;
	uint8 *p_data;
	uint16 org_m_len;
	uint32 remaining = buffobj_Remaining(theBuf);

	if (!remaining)
		return -1;

	b->m_pos = buffobj_Pos(theBuf);

	p_data = b->m_pos;

	if (theType != WpsNtohs(p_data)) {
		return -1;
	}

	b->m_type = theType;
	b->m_pos += sizeof(uint16); /* advance to length field */
	b->m_len = WpsNtohs(b->m_pos);
	org_m_len = b->m_len;

	if (b->m_len + sizeof(WpsTlvHdr) > remaining) {
		TUTRACE((TUTRACE_ERR, "tlv_dserialize_imp:length field too large %d", b->m_len));
		return -1;
	}

	b->m_pos += sizeof(uint16 ); /* advance to data field */

	/* now, store data according to type */
	tlvtype = tlv_gettype(theType);

	switch (tlvtype) {
	/* bool */
	case TLV_UINT8:
	{
		TlvObj_uint8 *t = (TlvObj_uint8 *)b;
		if (dataSize) {
			if (remaining < sizeof(WpsTlvHdr) + dataSize) {
				TUTRACE((TUTRACE_ERR,
					"tlvbase: insufficient buffer size\n"));
				return -1;
			}

			if (dataSize > b->m_len) {
				TUTRACE((TUTRACE_ERR, "length field too small %d",
					b->m_len));
				return -1;
			}
		}
		t->m_data = *(uint8 *)b->m_pos;
	}
	break;

	/* uint 16 */
	case TLV_UINT16:
	{
		TlvObj_uint16 *t = (TlvObj_uint16 *)b;
		if (dataSize) {
			if (remaining < sizeof(WpsTlvHdr) + dataSize) {
				TUTRACE((TUTRACE_ERR,
					"tlvbase: insufficient buffer size\n"));
				return -1;
			}

			if (dataSize > b->m_len) {
				TUTRACE((TUTRACE_ERR, "length field too small %d",
					b->m_len));
				return -1;
			}
		}
		t->m_data = WpsNtohs(b->m_pos);
	}
	break;

	case TLV_UINT32:
	{
		TlvObj_uint32 *t = (TlvObj_uint32 *)b;
		if (dataSize) {
			if (remaining < sizeof(WpsTlvHdr) + dataSize) {
				TUTRACE((TUTRACE_ERR,
					"tlvbase: insufficient buffer size\n"));
				return -1;
			}

			if (dataSize > b->m_len) {
				TUTRACE((TUTRACE_ERR, "length field too small %d",
					b->m_len));
				return -1;
			}
		}
		t->m_data = WpsNtohl(b->m_pos);
	}
	break;

	case TLV_CHAR_PTR:
	case TLV_UINT8_PTR:
	{
		TlvObj_ptr *t = (TlvObj_ptr *)b;

		if (dataSize) {
			if (b->m_len > dataSize) {
				TUTRACE((TUTRACE_ERR,
					"length %d greater than expectated %d,",
					b->m_len, dataSize));
				/* limit the m_len to dataSize */
				b->m_len = dataSize;
			}
		}

		t->m_allocated = alloc;
		if (alloc) {
			if (tlv_allocate_imp(v, theType, b->m_len, func, line) < 0) {
				TUTRACE((TUTRACE_ERR,
					"tlv deserialize : out of memory !!"));
				return -1;
			}
			if (b->m_len)
				memcpy(t->m_data, b->m_pos, b->m_len);
		}
		/* just save the pointer */
		else {
			if (b->m_len)
				t->m_data = (char *)b->m_pos;
			else
				t->m_data = "";
		}
	}
	break;

	default:
		return -1;
	}

	buffobj_Advance(theBuf, 2 * sizeof(uint16) + org_m_len); /* move to next type field */

	return 0;
}

int
tlv_find_dserialize_imp(void *v, uint16 theType, BufferObj *theBuf, uint16 dataSize, bool alloc,
	const char *func, int line)
{
	tlvbase_s *b = (tlvbase_s *)v;
	uint8 *p_data;
	uint16 org_m_len;
	uint32 remaining;

	/* rewind */
	buffobj_Rewind(theBuf);

	/* always find from begin */
	remaining = buffobj_Length(theBuf);
	if (!remaining)
		return -1;

	/* find from begin position */
	b->m_pos = buffobj_GetBuf(theBuf);
	p_data = b->m_pos;

	/* search */
	while ((theType != WpsNtohs(p_data)) && remaining) {
		/* advance to length field */
		b->m_pos += sizeof(uint16);
		b->m_len = WpsNtohs(b->m_pos);
		org_m_len = b->m_len;

		if (b->m_len + sizeof(WpsTlvHdr) > remaining) {
			TUTRACE((TUTRACE_ERR, "tlv_find_dserialize_imp:length field too large %d",
				b->m_len));
			/* clear in argument v m_len when error */
			b->m_len = 0;
			return -1;
		}

		/* move to next type field */
		buffobj_Advance(theBuf, 2 * sizeof(uint16) + org_m_len);
		remaining = buffobj_Remaining(theBuf);

		b->m_pos = buffobj_Pos(theBuf);
		p_data = b->m_pos;
	}

	/* found */
	if (theType == WpsNtohs(p_data) && remaining) {
	  b->m_len = WpsNtohs(b->m_pos + sizeof(uint16));
	  return tlv_dserialize_imp(v, theType, theBuf, dataSize, alloc, func, line);
	}

	/* not found */
	/* clear in argument v m_len when error */
	b->m_len = 0;

	return -1;
}

#ifdef WFA_WPS_20_TESTBED
int
tlv_find_primDeviceTypeParse_imp(CTlvPrimDeviceType *v, BufferObj *theBuf)
{
	tlvbase_s *b = (tlvbase_s *)v;
	uint8 *p_data;
	uint16 org_m_len;
	uint32 remaining;

	/* rewind */
	buffobj_Rewind(theBuf);

	/* always find from begin */
	remaining = buffobj_Length(theBuf);
	if (!remaining)
		return -1;

	/* find from begin position */
	b->m_pos = buffobj_GetBuf(theBuf);
	p_data = b->m_pos;

	/* search */
	while ((WPS_ID_PRIM_DEV_TYPE != WpsNtohs(p_data)) && remaining) {
		/* advance to length field */
		b->m_pos += sizeof(uint16);
		b->m_len = WpsNtohs(b->m_pos);
		org_m_len = b->m_len;

		if (b->m_len + sizeof(WpsTlvHdr) > remaining) {
			TUTRACE((TUTRACE_ERR, "tlv_find_primDeviceTypeParse_imp:length field "
				"too large %d", b->m_len));
			/* clear in argument v m_len when error */
			b->m_len = 0;
			return -1;
		}

		/* move to next type field */
		buffobj_Advance(theBuf, 2 * sizeof(uint16) + org_m_len);
		remaining = buffobj_Remaining(theBuf);

		b->m_pos = buffobj_Pos(theBuf);
		p_data = b->m_pos;
	}

	/* found */
	if (WPS_ID_PRIM_DEV_TYPE == WpsNtohs(p_data))
		return tlv_primDeviceTypeParse(v, theBuf);

	/* not found */
	/* clear in argument v m_len when error */
	b->m_len = 0;

	return -1;
}
#endif /* WFA_WPS_20_TESTBED */

/*
 * OK, this is UGLY .. it is not always easy to go from C++ to C ...
 * the "val" can be either a pointer or an integer depending of the type ...
 */
int
tlv_set(void *v, uint16 theType, void *val, uint16 len)
{
	uint32 val_long = 0;
	tlvbase_s *b = (tlvbase_s *)v;
	int tlvtype = tlv_gettype(theType);
	b->m_type = theType;
	val_long = PTR2UINT(val);

	/* now, store value according to type */
	switch (tlvtype) {
	case TLV_UINT8:
	{
		TlvObj_uint8 *t = (TlvObj_uint8 *)b;
		b->m_len = sizeof(t->m_data);
		t->m_data = (uint8)(val_long & 0xff);
	}
	break;
	case TLV_UINT16:
	{
		TlvObj_uint16 *t = (TlvObj_uint16 *)b;
		b->m_len = sizeof(t->m_data);
		t->m_data = (uint16)(val_long & 0xffff);
	}
	break;
	case TLV_UINT32:
	{
		TlvObj_uint32 *t = (TlvObj_uint32 *)b;
		b->m_len = sizeof(t->m_data);
		t->m_data = PTR2UINT(val);
	}
	break;
	case TLV_CHAR_PTR:
	{
		TlvObj_ptr *t = (TlvObj_ptr *)b;
		b->m_len = len;
		t->m_data = (char *)val;
	}
	break;
	case TLV_UINT8_PTR:
	{
		TlvObj_ptru *t = (TlvObj_ptru *)b;
		b->m_len = len;
		t->m_data = (uint8 *)val;
	}
	break;
	default:
		return -1;
	}

	return 0;
}

int
tlv_write(void *v, BufferObj *theBuf)
{
	uint8 temp[sizeof(uint32)];
	uint8 *p = NULL;
	int tlvtype;
	tlvbase_s *b = (tlvbase_s *)v;

	/* Copy the type */
	WpsHtonsPtr((uint8 *)&b->m_type, temp);
	buffobj_Append(theBuf, sizeof(uint16), temp);

	/* Copy the length */
	WpsHtonsPtr((uint8 *)&b->m_len, temp);
	buffobj_Append(theBuf, sizeof(uint16), temp);

	/* Copy the value */
	if (! b->m_len)
		return 0;

	/* now, store data according to type */
	tlvtype = tlv_gettype(b->m_type);
	p = temp;
	switch (tlvtype) {
	case TLV_UINT8:
	{
		TlvObj_uint8 *t = (TlvObj_uint8 *)b;
		p = &t->m_data;
	}
	break;

	/* uint 16 */
	case TLV_UINT16:
	{
		TlvObj_uint16 *t = (TlvObj_uint16 *)b;
		WpsHtonsPtr((uint8*)&t->m_data, temp);
	}
	break;

	case TLV_UINT32:
	{
		TlvObj_uint32 *t = (TlvObj_uint32 *)b;
		WpsHtonlPtr((uint8*)&t->m_data, temp);
	}
	break;

	default:
	{
		TlvObj_ptr *t = (TlvObj_ptr *)b;
		p = (uint8 *)t->m_data;
	}
	break;
	}

	buffobj_Append(theBuf, b->m_len, p);
	return 0;
}

uint16
tlv_getType(tlvbase_s *b)
{
	return b->m_type;
}


uint16
tlv_getLength(tlvbase_s *b)
{
	return b->m_len;
}

void
tlv_serialize(uint16 theType, BufferObj * theBuf, void *data_v, uint16 m_len)
{
	uint8 temp[sizeof(uint32)];
	uint8 *data = (uint8 *)data_v;
	uint8 *p = NULL;
	int tlvtype;

	/* Copy the type */
	WpsHtonsPtr((uint8 *)&theType, temp);
	buffobj_Append(theBuf, sizeof(uint16), temp);

	/* Copy the length */
	WpsHtonsPtr((uint8 *)&m_len, temp);
	buffobj_Append(theBuf, sizeof(uint16), temp);

	/* Copy the value */
	if (!m_len)
		return;

	/* now, store data according to type */
	tlvtype = tlv_gettype(theType);
	p = temp;
	switch (tlvtype) {
	/* uint 16 */
	case TLV_UINT16:
		WpsHtonsPtr(data, temp);
	break;
	case TLV_UINT32:
		WpsHtonlPtr(data, temp);
	break;
	default:
		p = data;
	}
	buffobj_Append(theBuf, m_len, p);
}


static void
tlv_serialize_ve_msvp(uint16 theType, BufferObj *theBuf, void *data_v, uint16 m_len)
{
	uint8 temp[sizeof(uint32)];
	uint8 *data = (uint8 *)data_v;
	uint8 *p = NULL;

	/* Copy the 2 bytes type */
	WpsHtonsPtr((uint8 *)&theType, temp);
	buffobj_Append(theBuf, sizeof(uint16), temp);

	/* Copy the 2 bytes length */
	WpsHtonsPtr((uint8 *)&m_len, temp);
	buffobj_Append(theBuf, sizeof(uint16), temp);

	/* Copy the value */
	if (!m_len)
		return;

	p = temp;

	switch (theType) {
	case WPS_MS_ID_VPI:
		WpsHtonsPtr(data, temp);
		break;

	default:
		case WPS_MS_ID_TRANSPORT_UUID:
		p = data;
	}

	buffobj_Append(theBuf, m_len, p);
}


void
tlv_serialize_ve(char *pVE, uint16 theType, BufferObj *theBuf, void *data_v, uint16 m_len)
{
	if (strcmp(MS_VENDOR_EXT_ID, pVE) == 0)
		tlv_serialize_ve_msvp(theType, theBuf, data_v, m_len);


	return;
}

void
tlv_delete_imp(void *v, bool content_only, const char *func, int line)
{
	tlvbase_s *b = (tlvbase_s *)v;
	int tlvtype;
	TlvObj_ptr *t = NULL;

	if (!v)
		return;

	/* it hasn't been created, return */
	if (!b->m_type) {
		if (!content_only) {
			free(b);
		}
		return;
	}

	tlvtype = tlv_gettype(b->m_type);
	switch (tlvtype) {
	case TLV_CHAR_PTR:
	case TLV_UINT8_PTR:
		t = (TlvObj_ptr *)b;
		if (t->m_allocated && t->m_data) {
			free(t->m_data);
			t->m_data = NULL;
			tlv_freemem++;
		}
	case TLV_UINT8:
	case TLV_UINT16:
	case TLV_UINT32:
		if (! content_only) {
			free(b);
		}
		break;

	case TLV_CREDENTIAL:
		TUTRACE((TUTRACE_ERR, "Should not delete TLV_CREDENTIAL through here."));
		break;

	default:
		/* do not free an unidentified tlv  */
		/* Ignore error message of some not initialized tlv */
		if (b->m_type != 0) {
			TUTRACE((TUTRACE_ERR, "tlv delete : trying to free an unidentified "
				"tlv 0x%x object from %s at %d\n", b->m_type, func, line));
		}
	}
}

int
cplxtlv_parseHdr(tlvbase_s *b, uint16 theType, BufferObj * theBuf, uint16 minDataSize)
{
	/* Extracts the type and the length. Positions m_pos to point to the data */
	uint32 remaining = buffobj_Remaining(theBuf);
	b->m_pos = buffobj_Pos(theBuf);

	if (remaining < sizeof(WpsTlvHdr) + minDataSize) {
		TUTRACE((TUTRACE_ERR, "cplx parseHdr : insufficient buffer size\n"));
		return -1;
	}

	if (theType != WpsNtohs(b->m_pos)) {
		TUTRACE((TUTRACE_ERR, "cplx parseHdr : unexpected type\n"));
		return -1;
	}

	b->m_type = theType;
	b->m_pos += sizeof(uint16 ); /* advance to length field */

	b->m_len = WpsNtohs(b->m_pos);
	if (minDataSize > b->m_len) {
		TUTRACE((TUTRACE_ERR, "cplx parseHdr : insufficient buffer size\n"));
		return -1;
	}

	if (b->m_len + sizeof(WpsTlvHdr) > remaining) {
		TUTRACE((TUTRACE_ERR, "cplx parseHdr : buffer overflow\n"));
		return -1;
	};

	b->m_pos += sizeof(uint16 ); /* advance to data field */
	buffobj_Advance(theBuf, 2 * sizeof(uint16));
	return 0;
}

void
cplxtlv_writeHdr(tlvbase_s *b, uint16 theType, uint16 length, BufferObj * theBuf)
{
	/*
	 * serializes the type and length.
	 * Positions m_pos to point to the start of length
	 */
	uint8 temp[sizeof(uint32)];

	b->m_type = theType;
	b->m_len = length;

	/* Copy the Type */
	WpsHtonsPtr((uint8 *)&b->m_type, temp);
	buffobj_Append(theBuf, sizeof(uint16), temp);

	/* Copy the length */
	WpsHtonsPtr((uint8 *)&b->m_len, temp);
	buffobj_Append(theBuf, sizeof(uint16), temp);
}

/* Encrypted settings */
int
tlv_encrSettingsParse(CTlvEncrSettings *tlv, BufferObj *theBuf)
{
	int err;

	/* parse the header first. Min data size of the IV + 1 block of data */
	err = cplxtlv_parseHdr(&tlv->base, WPS_ID_ENCR_SETTINGS, theBuf,
		SIZE_ENCR_IV + ENCR_DATA_BLOCK_SIZE);
	if (err != 0) {
		TUTRACE((TUTRACE_ERR, "tlv_encrSettingsParse: cplx parseHdr error\n"));
		return err;
	}

	tlv->iv = tlv->base.m_pos;
	tlv->ip_encryptedData = buffobj_Advance(theBuf, SIZE_ENCR_IV);
	tlv->encrDataLength = (tlv->base.m_len > SIZE_ENCR_IV) ?
		(tlv->base.m_len - SIZE_ENCR_IV) : 0;
	buffobj_Advance(theBuf, tlv->encrDataLength);

	return err;
}

void
tlv_encrSettingsWrite(CTlvEncrSettings *tlv, BufferObj *theBuf)
{
	cplxtlv_writeHdr(&tlv->base, WPS_ID_ENCR_SETTINGS, SIZE_ENCR_IV +
		tlv->encrDataLength, theBuf);

	/* Append the data. Leave the pointer positioned at the start of data. */
	tlv->base.m_pos = buffobj_Append(theBuf, SIZE_ENCR_IV, tlv->iv);
	buffobj_Append(theBuf, tlv->encrDataLength, tlv->ip_encryptedData);
}

int
tlv_oobDevPwdParse(CTlvOobDevPwd *tlv, BufferObj *theBuf)
{
	int err;

	/*
	 * For min size, use the size of the hash,
	 * size of the password ID and one byte of password data
	 */
	err = cplxtlv_parseHdr(&tlv->base, WPS_ID_OOB_DEV_PWD, theBuf, SIZE_DATA_HASH +
		sizeof(uint16) + sizeof(uint8));

	if (err != 0) {
		TUTRACE((TUTRACE_ERR, "tlv_oobDevPwdParse: cplx parseHdr error\n"));
		return err;
	}

	tlv->publicKeyHash = buffobj_Pos(theBuf);
	tlv->pwdId = WpsNtohs(buffobj_Advance(theBuf, SIZE_DATA_HASH));
	tlv->ip_devPwd = buffobj_Advance(theBuf, sizeof(uint16));

	tlv->devPwdLength = tlv->base.m_len - SIZE_DATA_HASH - sizeof(uint16);
	buffobj_Advance(theBuf, tlv->devPwdLength);

	return 0;
}

void
tlv_oobDevPwdWrite(CTlvOobDevPwd *tlv, BufferObj *theBuf)
{
	uint16 netPwdId;
	cplxtlv_writeHdr(&tlv->base, WPS_ID_OOB_DEV_PWD, SIZE_DATA_HASH + sizeof(uint16) +
		tlv->devPwdLength, theBuf);

	tlv->base.m_pos = buffobj_Append(theBuf, SIZE_DATA_HASH, tlv->publicKeyHash);
	WpsHtonsPtr((uint8 *)&tlv->pwdId, (uint8 *)&netPwdId);
	buffobj_Append(theBuf, sizeof(uint16), (uint8 *)&netPwdId);
	buffobj_Append(theBuf, tlv->devPwdLength, tlv->ip_devPwd);
}

void tlv_credentialDelete(CTlvCredential  *cred, bool content_only)
{
	/* to be sure, call delete on all possible contents */
	tlv_delete(&cred->nwIndex, 1);
	tlv_delete(&cred->ssid, 1);
	tlv_delete(&cred->authType, 1);
	tlv_delete(&cred->encrType, 1);
	tlv_delete(&cred->nwKeyIndex, 1);
	tlv_delete(&cred->nwKey, 1);
	tlv_delete(&cred->macAddr, 1);

	tlv_delete(&cred->eapType, 1);
	tlv_delete(&cred->eapIdentity, 1);
	tlv_delete(&cred->keyLifetime, 1);
	tlv_delete(&cred->rekeyKey, 1);
	tlv_delete(&cred->x509Cert, 1);
	tlv_delete(&cred->keyProvidedAuto, 1);
	tlv_delete(&cred->oneXEnabled, 1);
	tlv_delete(&cred->WEPKeyIndex, 1);

	/* WSC 2.0 */
	subtlv_delete(&cred->nwKeyShareable, 1);

	if (!content_only)
		free(cred);
}

void
tlv_credentialParseAKey(CTlvCredential *tlv,  BufferObj *theBuf, bool allocate)
{
	tlv_dserialize(&tlv->nwKey, WPS_ID_NW_KEY, theBuf, SIZE_64_BYTES, allocate);
	tlv_dserialize(&tlv->macAddr, WPS_ID_MAC_ADDR, theBuf, SIZE_MAC_ADDR, allocate);

	/* Parse optional attributes */
	if (WPS_ID_EAP_TYPE == buffobj_NextType(theBuf))
		tlv_dserialize(&tlv->eapType, WPS_ID_EAP_TYPE, theBuf, SIZE_8_BYTES, allocate);

	if (WPS_ID_EAP_IDENTITY == buffobj_NextType(theBuf))
		tlv_dserialize(&tlv->eapIdentity, WPS_ID_EAP_IDENTITY, theBuf, 0, allocate);

	if (WPS_ID_KEY_PROVIDED_AUTO == buffobj_NextType(theBuf))
		tlv_dserialize(&tlv->keyProvidedAuto, WPS_ID_KEY_PROVIDED_AUTO, theBuf, 0, 0);

	if (WPS_ID_8021X_ENABLED == buffobj_NextType(theBuf))
		tlv_dserialize(&tlv->oneXEnabled, WPS_ID_8021X_ENABLED, theBuf, 0, 0);

	/*
	 * Now handle additional optional attributes that may
	 * appear in arbitrary order.
	 */
	while (buffobj_NextType(theBuf) != 0) { /* more attributes */
		uint16 nextType = buffobj_NextType(theBuf);
		uint8 *Pos;

		switch (nextType) {
		case WPS_ID_KEY_LIFETIME:
			tlv_dserialize(&tlv->keyLifetime, WPS_ID_KEY_LIFETIME, theBuf, 0, 0);
			break;
		case WPS_ID_REKEY_KEY:
			tlv_dserialize(&tlv->rekeyKey, WPS_ID_REKEY_KEY, theBuf, 0, allocate);
			break;
		case WPS_ID_X509_CERT:
			tlv_dserialize(&tlv->x509Cert, WPS_ID_X509_CERT, theBuf, 0, allocate);
			break;
		case WPS_ID_WEP_TRANSMIT_KEY:
			tlv_dserialize(&tlv->WEPKeyIndex, WPS_ID_WEP_TRANSMIT_KEY,
				theBuf, 0, allocate);
			break;
		case WPS_ID_NW_KEY_INDEX:
			return; /* we've reached the end of this network key */
		/* WSC 2.0, Get nwKeyShareable */
		case WPS_ID_VENDOR_EXT:
			/* Check subelement in WFA Vendor Extension */
			if (tlv_vendorExtParse(&tlv->vendorExt, theBuf) == 0 &&
			    memcmp(tlv->vendorExt.vendorId, (uint8 *)WFA_VENDOR_EXT_ID,
			    SIZE_3_BYTES) == 0) {

				BufferObj *vendorExt_bufObj = buffobj_new();

				/* Deserialize subelement */
				if (!vendorExt_bufObj) {
					TUTRACE((TUTRACE_ERR, "Fail to allocate vendor extension"
						" buffer, Out of memory\n"));
					return;
				}

				buffobj_dserial(vendorExt_bufObj, tlv->vendorExt.vendorData,
					tlv->vendorExt.dataLength);
				/* WSC 2.0, is next nwKeyShareable subelement */
				if (WPS_WFA_SUBID_NW_KEY_SHAREABLE ==
					buffobj_NextSubId(vendorExt_bufObj))
					subtlv_dserialize(&tlv->nwKeyShareable,
						WPS_WFA_SUBID_NW_KEY_SHAREABLE,
						vendorExt_bufObj, 0, allocate);

				buffobj_del(vendorExt_bufObj);
			}
			break;
		default: /* skip over other unknown attributes */
			Pos = buffobj_Advance(theBuf, sizeof(WpsTlvHdr) +
				WpsNtohs((buffobj_Pos(theBuf)+ sizeof(uint16))));

			if (Pos == NULL)
				return;

			if (buffobj_Pos(theBuf) - tlv->base.m_pos >= tlv->base.m_len) {
				/*
				 * Actually, an == check would be more precise, but the >=
				 * is slightly safer in preventing buffer overruns.
				 */
				return; /* we've reached the end of the Credential */
			}
			break;
		}
	}
}

void
tlv_credentialInit(CTlvCredential * tlv)
{
	tlv->nwIndex.tlvbase.m_len = 0;
	tlv->ssid.tlvbase.m_len = 0;
	tlv->authType.tlvbase.m_len = 0;
	tlv->encrType.tlvbase.m_len = 0;
	tlv->nwKeyIndex.tlvbase.m_len = 0;
	tlv->nwKey.tlvbase.m_len = 0;
	tlv->macAddr.tlvbase.m_len = 0;

	tlv->eapType.tlvbase.m_len = 0;
	tlv->eapIdentity.tlvbase.m_len = 0;
	tlv->keyLifetime.tlvbase.m_len = 0;
	tlv->rekeyKey.tlvbase.m_len = 0;
	tlv->x509Cert.tlvbase.m_len = 0;
	tlv->keyProvidedAuto.tlvbase.m_len = 0;
	tlv->oneXEnabled.tlvbase.m_len = 0;
	tlv->WEPKeyIndex.tlvbase.m_len = 0;

	tlv->nwKeyShareable.subtlvbase.m_len = 0;
}

void
tlv_credentialParse(CTlvCredential *tlv, BufferObj *theBuf, bool allocate)
{
	int err;

	/*
	 * don't bother with min data size.
	 * the respective TLV will handle their sizes
	 */
	err = cplxtlv_parseHdr(&tlv->base, WPS_ID_CREDENTIAL, theBuf, 0);
	if (err != 0) {
		TUTRACE((TUTRACE_ERR, "tlv_credentialParse: cplx parseHdr error\n"));
		return;
	}

	tlv_dserialize(&tlv->nwIndex, WPS_ID_NW_INDEX, theBuf, 0, 0);
	tlv_dserialize(&tlv->ssid, WPS_ID_SSID, theBuf, SIZE_32_BYTES, allocate);
	tlv_dserialize(&tlv->authType, WPS_ID_AUTH_TYPE, theBuf, 0, 0);
	tlv_dserialize(&tlv->encrType, WPS_ID_ENCR_TYPE, theBuf, 0, 0);

	/*
	* This code only supports a single network key, but it is structured to allow
	* easy extension to parsing multiple keys.
	*/

	/* If Network Key Index is present, we may have multiple credentials */
	while (WPS_ID_NW_KEY_INDEX == buffobj_NextType(theBuf)) {
		tlv_dserialize(&tlv->nwKeyIndex, WPS_ID_NW_KEY_INDEX, theBuf, 0, 0);
		tlv_credentialParseAKey(tlv, theBuf, allocate);
		break; /* only support a single key for now */
	}
	/* make sure we haven't reached the end of the Credential attribute yet. */
	if ((buffobj_Pos(theBuf) - tlv->base.m_pos < tlv->base.m_len) && WPS_ID_NW_KEY ==
		buffobj_NextType(theBuf)) {
		 /* default to key index 1 */
		tlv_set(&tlv->nwKeyIndex, WPS_ID_NW_KEY_INDEX, (void *)1, 1);
		tlv_credentialParseAKey(tlv, theBuf, allocate);
	}
	/*
	 * Ignore any additional TLVs that might still be unparsed.  To be safe,
	 * explicitly set the buffer position at the end of the Credential TLV length.
	 */
	buffobj_Set(theBuf, tlv->base.m_pos + tlv->base.m_len);
}

void
tlv_credentialWrite(CTlvCredential *tlv, BufferObj *theBuf)
{
	uint16 len =  tlv->nwIndex.tlvbase.m_len
		+ tlv->ssid.tlvbase.m_len
		+ tlv->authType.tlvbase.m_len
		+ tlv->encrType.tlvbase.m_len
		+ tlv->nwKeyIndex.tlvbase.m_len
		+ tlv->nwKey.tlvbase.m_len
		+ tlv->macAddr.tlvbase.m_len
		+ tlv->eapType.tlvbase.m_len
		+ tlv->eapIdentity.tlvbase.m_len
		+ tlv->keyLifetime.tlvbase.m_len
		+ tlv->rekeyKey.tlvbase.m_len
		+ tlv->x509Cert.tlvbase.m_len
		+ tlv->WEPKeyIndex.tlvbase.m_len;

	/*
	 * now include the size of the TLV headers of all the TLVs
	 * We have 6 required TLVs, and 8 optional ones
	 */
	uint16 tlvCount = 6; /* required TLVs */

	/* optional */
	if (tlv->nwKeyIndex.tlvbase.m_len)
		tlvCount++;
	if (tlv->eapType.tlvbase.m_len)
		tlvCount++;
	if (tlv->eapIdentity.tlvbase.m_len)
		tlvCount++;
	if (tlv->keyLifetime.tlvbase.m_len)
		tlvCount++;
	if (tlv->rekeyKey.tlvbase.m_len)
		tlvCount++;
	if (tlv->x509Cert.tlvbase.m_len)
		tlvCount++;
	if (tlv->WEPKeyIndex.tlvbase.m_len)
		tlvCount++;

	/* WSC 2.0 */
	if (tlv->nwKeyShareable.subtlvbase.m_len) {
		/* (Only for WFA Vendor Extension), plus Vendor Extension length */
		len += sizeof(WpsSubTlvHdr) + tlv->nwKeyShareable.subtlvbase.m_len;

		/* Vendor Extension attribute */
		tlvCount++;

		/* Vendor ID length */
		len += SIZE_3_BYTES;
	}

	/* now update the length */
	len += (sizeof(WpsTlvHdr) * tlvCount);

	/* First write out the header */
	cplxtlv_writeHdr(&tlv->base, WPS_ID_CREDENTIAL, len, theBuf);

	/* Next, write out the required TLVs */
	/*
	 * Caution: WSC 2.0, "Network Index"
	 * Deprecated - use fixed value 1 for backwards compatibility
	 */
	tlv_write(&tlv->nwIndex, theBuf);
	tlv_write(&tlv->ssid, theBuf);
	tlv_write(&tlv->authType, theBuf);
	tlv_write(&tlv->encrType, theBuf);
	tlv_write(&tlv->nwKey, theBuf);
	tlv_write(&tlv->macAddr, theBuf);

	/* Finally, write the optional TLVs. */
	/*
	 * Caution: WSC 2.0, "Network Key Index"
	 * Deprecated. Only included by WSC 1.0 devices. Ignored by WSC 2.0 or newer devices
	 */
	if (tlv->nwKeyIndex.tlvbase.m_len)
		tlv_write(&tlv->nwKeyIndex, theBuf);

	if (tlv->eapType.tlvbase.m_len)
		tlv_write(&tlv->eapType, theBuf);
	if (tlv->eapIdentity.tlvbase.m_len)
		tlv_write(&tlv->eapIdentity, theBuf);
	if (tlv->keyLifetime.tlvbase.m_len)
		tlv_write(&tlv->keyLifetime, theBuf);
	if (tlv->rekeyKey.tlvbase.m_len)
		tlv_write(&tlv->rekeyKey, theBuf);
	if (tlv->x509Cert.tlvbase.m_len)
		tlv_write(&tlv->x509Cert, theBuf);

	/* Caution: WSC 2.0, deprecated WEP */
	if (tlv->WEPKeyIndex.tlvbase.m_len)
		tlv_write(&tlv->WEPKeyIndex, theBuf);

	/* WSC 2.0, Network Key Shareable */
	if (tlv->nwKeyShareable.subtlvbase.m_len) {
		CTlvVendorExt vendorExt;
		BufferObj *vendorExt_bufObj = buffobj_new();

		if (vendorExt_bufObj == NULL) {
			TUTRACE((TUTRACE_ERR, "tlv_credentialWrite: fail to allocate buffer\n"));
			return;
		}

		/* Add nwKeyShareable subelement to vendorExt_bufObj */
		subtlv_write(&tlv->nwKeyShareable, vendorExt_bufObj);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, theBuf);

		buffobj_del(vendorExt_bufObj);
	}
}

int
tlv_primDeviceTypeParse(CTlvPrimDeviceType *tlv, BufferObj *theBuf)
{
	int err;

	/* parse the header first. Data size is a fixed 8 bytes */
	err = cplxtlv_parseHdr(&tlv->base, WPS_ID_PRIM_DEV_TYPE, theBuf, SIZE_8_BYTES);
	if (err != 0) {
		TUTRACE((TUTRACE_ERR, "tlv_primDeviceTypeParse: cplx parseHdr error\n"));
		return err;
	}

	tlv->categoryId = WpsNtohs(tlv->base.m_pos);
	WpsHtonlPtr(buffobj_Advance(theBuf, SIZE_2_BYTES), (uint8 *)&tlv->oui);
	tlv->subCategoryId = WpsNtohs(buffobj_Advance(theBuf, SIZE_4_BYTES));
	buffobj_Advance(theBuf, SIZE_2_BYTES);

	return 0;
}

void
tlv_primDeviceTypeWrite(CTlvPrimDeviceType *tlv, BufferObj *theBuf)
{
	uint8 temp[sizeof(uint32)];

	cplxtlv_writeHdr(&tlv->base, WPS_ID_PRIM_DEV_TYPE, SIZE_8_BYTES, theBuf);

	WpsHtonsPtr((uint8 *)&tlv->categoryId, temp);
	tlv->base.m_pos = buffobj_Append(theBuf, SIZE_2_BYTES, temp);

	WpsHtonlPtr((uint8 *)&tlv->oui, temp);
	buffobj_Append(theBuf, SIZE_4_BYTES, temp);

	WpsHtonsPtr((uint8 *)&tlv->subCategoryId, temp);
	buffobj_Append(theBuf, SIZE_2_BYTES, temp);
}

void
tlv_reqDeviceTypeWrite(CTlvReqDeviceType *tlv, BufferObj *theBuf)
{
	uint8 temp[sizeof(uint32)];

	cplxtlv_writeHdr(&tlv->base, WPS_ID_REQ_DEV_TYPE, SIZE_8_BYTES, theBuf);

	WpsHtonsPtr((uint8 *)&tlv->categoryId, temp);
	tlv->base.m_pos = buffobj_Append(theBuf, SIZE_2_BYTES, temp);

	WpsHtonlPtr((uint8 *)&tlv->oui, temp);
	buffobj_Append(theBuf, SIZE_4_BYTES, temp);

	WpsHtonsPtr((uint8 *)&tlv->subCategoryId, temp);
	buffobj_Append(theBuf, SIZE_2_BYTES, temp);
}

int
tlv_vendorExt_set(CTlvVendorExt *v, uint8 *id, uint8 *data, uint16 datalen)
{
	tlvbase_s *b = (tlvbase_s *)v;

	b->m_type = WPS_ID_VENDOR_EXT;
	b->m_len = datalen + SIZE_3_BYTES;

	v->vendorId = id;
	v->vendorData = data;

	v->dataLength = datalen;

	return 0;
}

void
tlv_vendorExtWrite(CTlvVendorExt *tlv, BufferObj *theBuf)
{
	cplxtlv_writeHdr(&tlv->base, WPS_ID_VENDOR_EXT, SIZE_3_BYTES + tlv->dataLength, theBuf);

	tlv->base.m_pos = buffobj_Append(theBuf, SIZE_3_BYTES, tlv->vendorId);
	buffobj_Append(theBuf, tlv->dataLength, tlv->vendorData);
}

int
tlv_vendorExtParse(CTlvVendorExt *tlv, BufferObj *theBuf)
{
	int err;

	/*
	 * For min size, use the size of the hash,
	 * size of the password ID and one byte of password data
	 */
	err = cplxtlv_parseHdr(&tlv->base, WPS_ID_VENDOR_EXT, theBuf, SIZE_3_BYTES);
	if (err) {
		TUTRACE((TUTRACE_ERR, "tlv_vendorExtParse: cplx parseHdr error\n"));
		return err;
	}

	tlv->vendorId = buffobj_Pos(theBuf);
	tlv->vendorData = buffobj_Advance(theBuf, SIZE_3_BYTES);
	tlv->dataLength = tlv->base.m_len - SIZE_3_BYTES;
	buffobj_Advance(theBuf, tlv->dataLength);

	return 0;
}

/* Vendor Extension Attribut */
int
tlv_find_vendorExtParse_imp(void *v, BufferObj *theBuf, uint8 *vendorId)
{
	tlvbase_s *b = (tlvbase_s *)v;
	uint8 *p_data, *vid;
	uint16 org_m_len;
	uint32 remaining;


	/* rewind */
	buffobj_Rewind(theBuf);

	/* always find from begin */
	remaining = buffobj_Length(theBuf);
	if (!remaining)
		return -1;

	/* find from begin position */
	b->m_pos = buffobj_GetBuf(theBuf);
	p_data = b->m_pos;

	/* search */
	while (remaining) {
		if (WPS_ID_VENDOR_EXT == WpsNtohs(p_data)) {
			/* Compare Vendor ID with first 3 bytes of data */
			if (WpsNtohs(p_data + sizeof(WpsTlvHdr)) < SIZE_3_BYTES) {
				TUTRACE((TUTRACE_ERR, "tlv_find_vendor_ext_dserialize_imp: invalid"
					"vendor extension length field too short\n"));
				/* clear in argument v m_len when error */
				b->m_len = 0;
				return -1;
			}

			vid = p_data + sizeof(WpsTlvHdr);
			if (memcmp(vid, vendorId, SIZE_3_BYTES) == 0)
				break; /* Found */
		}

		/* advance to length field */
		b->m_pos += sizeof(uint16);
		b->m_len = WpsNtohs(b->m_pos);
		org_m_len = b->m_len;

		if (b->m_len + sizeof(WpsTlvHdr) > remaining) {
			TUTRACE((TUTRACE_ERR, "tlv_find_vendorExtParse_imp:length field "
				"too large %d", b->m_len));
			/* clear in argument v m_len when error */
			b->m_len = 0;
			return -1;
		}

		/* move to next type field */
		buffobj_Advance(theBuf, 2 * sizeof(uint16) + org_m_len);
		remaining = buffobj_Remaining(theBuf);

		b->m_pos = buffobj_Pos(theBuf);
		p_data = b->m_pos;
	}

	/* found */
	if (remaining && WPS_ID_VENDOR_EXT == WpsNtohs(p_data)) {
		b->m_len = WpsNtohs(b->m_pos + sizeof(uint16));
		return tlv_vendorExtParse(v, theBuf);
	}

	/* not found */
	/* clear in argument v m_len when error */
	b->m_len = 0;

	return -1;
}

int
subtlv_getid(uint8 theId)
{
	switch (theId) {
	/* bool */
	case WPS_WFA_SUBID_NW_KEY_SHAREABLE:
	case WPS_WFA_SUBID_REQ_TO_ENROLL:
	/* uint8 */
	case WPS_WFA_SUBID_VERSION2:
	case WPS_WFA_SUBID_SETTINGS_DELAY_TIME:
		return TLV_UINT8;
	/* uint8*  */
	case WPS_WFA_SUBID_AUTHORIZED_MACS:
		return TLV_UINT8_PTR;
	}

	return 0;
}

subtlvbase_s *
subtlv_new(uint8 theId)
{
	subtlvbase_s *b;
	int subtlvid = subtlv_getid(theId);
	switch (subtlvid) {
	case TLV_UINT8:
		b = (subtlvbase_s *)malloc(sizeof(SubTlvObj_uint8));
		break;
	case TLV_UINT16:
		b = (subtlvbase_s *)malloc(sizeof(SubTlvObj_uint16));
		break;
	case TLV_UINT32:
		b = (subtlvbase_s *)malloc(sizeof(SubTlvObj_uint32));
		break;
	case TLV_UINT8_PTR:
	case TLV_CHAR_PTR:
	{
		SubTlvObj_ptru *t;
		t = (SubTlvObj_ptru *)malloc(sizeof(SubTlvObj_ptru));
		if (t) {
			t->m_allocated = false;
			t->m_data = 0;
		}
		b = (subtlvbase_s *)t;
		break;
	}
	default:
		TUTRACE((TUTRACE_ERR, "Trying to allocate an unknown sub tlv id\n"));
		return NULL;
	}

	if (b) {
		b->m_len = 0;
		b->m_id = theId;
	}

	return b;
}

int
subtlv_allocate_imp(void *v, uint8 theId, uint8 len, const char *func, int line)
{
	subtlvbase_s *b = (subtlvbase_s *)v;
	int subtlvid = subtlv_getid(theId);

	switch (subtlvid) {
	case TLV_CHAR_PTR:
	case TLV_UINT8_PTR:
	{
		SubTlvObj_ptr *t = (SubTlvObj_ptr *)b;
		b->m_len = len;
		t->m_allocated = false;
		b->m_id = theId;
		if (len) {
			 /* extra one byte for NULL end */
			t->m_data = (char *)malloc(b->m_len + 1);
			if (!t->m_data) {
				TUTRACE((TUTRACE_ERR, "sub tlv allocate : out of memory !!"));
				return -1;
			}
			tlv_allocmem++;
			/* clear it */
			memset(t->m_data, 0, b->m_len + 1);
			t->m_allocated = true;
		}
		else {
			t->m_data = NULL;
		}
	}
	break;

	default:
		TUTRACE((TUTRACE_ERR, "sub tlv allocate : trying to allocate a non-pointer "
			"sub tlv object\n"));
	}

	return 0;
}

int
subtlv_dserialize_imp(void *v, uint8 theId, BufferObj *theBuf, uint8 dataSize, bool alloc,
	const char *func, int line)
{
	subtlvbase_s *b = (subtlvbase_s *)v;
	int subtlvid;
	uint8 *p_data;
	uint8 org_m_len;
	uint32 remaining = buffobj_Remaining(theBuf);

	if (!remaining)
		return -1;

	b->m_pos = buffobj_Pos(theBuf);

	p_data = b->m_pos;

	if (theId != *p_data) {
		return -1;
	}

	b->m_id = theId;
	b->m_pos += sizeof(uint8); /* advance to length field */
	b->m_len = *b->m_pos;
	org_m_len = b->m_len;

	if (b->m_len + sizeof(WpsSubTlvHdr) > remaining) {
		TUTRACE((TUTRACE_ERR, "subtlv_dserialize_imp:length field too large %d", b->m_len));
		return -1;
	}

	b->m_pos += sizeof(uint8); /* advance to data field */

	/* now, store data according to type */
	subtlvid = subtlv_getid(theId);

	switch (subtlvid) {
	/* bool */
	case TLV_UINT8:
	{
		SubTlvObj_uint8 *t = (SubTlvObj_uint8 *)b;
		if (dataSize) {
			if (remaining < sizeof(WpsSubTlvHdr) + dataSize) {
				TUTRACE((TUTRACE_ERR,
					"subtlvbase: insufficient buffer size\n"));
				return -1;
			}

			if (dataSize > b->m_len) {
				TUTRACE((TUTRACE_ERR, "sub element length field too small %d",
					b->m_len));
				return -1;
			}
		}
		t->m_data = *(uint8 *)b->m_pos;
	}
	break;

	/* uint 16 */
	case TLV_UINT16:
	{
		SubTlvObj_uint16 *t = (SubTlvObj_uint16 *)b;
		if (dataSize) {
			if (remaining < sizeof(WpsSubTlvHdr) + dataSize) {
				TUTRACE((TUTRACE_ERR,
					"subtlvbase: insufficient buffer size\n"));
				return -1;
			}

			if (dataSize > b->m_len) {
				TUTRACE((TUTRACE_ERR, "sub element length field too small %d",
					b->m_len));
				return -1;
			}
		}
		t->m_data = WpsNtohs(b->m_pos);
	}
	break;

	case TLV_UINT32:
	{
		SubTlvObj_uint32 *t = (SubTlvObj_uint32 *)b;
		if (dataSize) {
			if (remaining < sizeof(WpsSubTlvHdr) + dataSize) {
				TUTRACE((TUTRACE_ERR,
					"subtlvbase: insufficient buffer size\n"));
				return -1;
			}

			if (dataSize > b->m_len) {
				TUTRACE((TUTRACE_ERR, "sub element length field too small %d",
					b->m_len));
				return -1;
			}
		}
		t->m_data = WpsNtohl(b->m_pos);
	}
	break;

	case TLV_CHAR_PTR:
	case TLV_UINT8_PTR:
	{
		SubTlvObj_ptr *t = (SubTlvObj_ptr *)b;

		if (dataSize) {
			if (b->m_len > dataSize) {
				TUTRACE((TUTRACE_ERR,
					"length %d greater than expectated %d,",
					b->m_len, dataSize));
				/* limit the m_len to dataSize */
				b->m_len = dataSize;
			}
		}

		t->m_allocated = alloc;
		if (alloc) {
			if (subtlv_allocate_imp(v, theId, b->m_len, func, line) < 0) {
				TUTRACE((TUTRACE_ERR,
					"subtlv deserialize : out of memory !!"));
				return -1;
			}
			if (b->m_len)
				memcpy(t->m_data, b->m_pos, b->m_len);
		}
		/* just save the pointer */
		else {
			t->m_data = (char *)b->m_pos;
		}
	}
	break;

	default:
		return -1;
	}

	/* Move to next type field */
	buffobj_Advance(theBuf, sizeof(WpsSubTlvHdr) + org_m_len);

	return 0;
}

int
subtlv_find_dserialize_imp(void *v, uint8 theId, BufferObj *theBuf, uint8 dataSize, bool alloc,
	const char *func, int line)
{
	subtlvbase_s *b = (subtlvbase_s *)v;
	uint8 *p_data;
	uint8 org_m_len;
	uint32 remaining;

	/* rewind */
	buffobj_Rewind(theBuf);

	/* always find from begin */
	remaining = buffobj_Length(theBuf);
	if (!remaining)
		return -1;

	/* find from begin position */
	b->m_pos = buffobj_GetBuf(theBuf);
	p_data = b->m_pos;

	/* search */
	while ((theId != *p_data) && remaining) {
		/* advance to length field */
		b->m_pos += sizeof(uint8);
		b->m_len = *b->m_pos;
		org_m_len = b->m_len;

		if (b->m_len + sizeof(WpsSubTlvHdr) > remaining) {
			TUTRACE((TUTRACE_ERR, "subtlv_find_dserialize_imp:length field too "
				"large %d", b->m_len));
			/* clear in argument v m_len when error */
			b->m_len = 0;
			return -1;
		}

		/* move to next type field */
		buffobj_Advance(theBuf, sizeof(WpsSubTlvHdr) + org_m_len);
		remaining = buffobj_Remaining(theBuf);

		b->m_pos = buffobj_Pos(theBuf);
		p_data = b->m_pos;
	}

	/* found */
	if (theId == *p_data && remaining) {
	  b->m_len = *(b->m_pos + sizeof(uint8));
	  return subtlv_dserialize_imp(v, theId, theBuf, dataSize, alloc, func, line);
	}

	/* not found */
	/* clear in argument v m_len when error */
	b->m_len = 0;

	return -1;
}

int
subtlv_set(void *v, uint8 theId, void *val, uint8 len)
{
	uint32 val_long = 0;
	subtlvbase_s *b = (subtlvbase_s *)v;
	int subtlvid = subtlv_getid(theId);
	b->m_id = theId;
	val_long = PTR2UINT(val);

	/* now, store value according to type */
	switch (subtlvid) {
	case TLV_UINT8:
	{
		SubTlvObj_uint8 *t = (SubTlvObj_uint8 *)b;
		b->m_len = sizeof(t->m_data);
		t->m_data = (uint8)(val_long & 0xff);
	}
	break;
	case TLV_UINT16:
	{
		SubTlvObj_uint16 *t = (SubTlvObj_uint16 *)b;
		b->m_len = sizeof(t->m_data);
		t->m_data = (uint16)(val_long & 0xffff);
	}
	break;
	case TLV_UINT32:
	{
		SubTlvObj_uint32 *t = (SubTlvObj_uint32 *)b;
		b->m_len = sizeof(t->m_data);
		t->m_data = PTR2UINT(val);
	}
	break;
	case TLV_CHAR_PTR:
	{
		SubTlvObj_ptr *t = (SubTlvObj_ptr *)b;
		b->m_len = len;
		t->m_data = (char *)val;
	}
	break;
	case TLV_UINT8_PTR:
	{
		SubTlvObj_ptru *t = (SubTlvObj_ptru *)b;
		b->m_len = len;
		t->m_data = (uint8 *)val;
	}
	break;
	default:
		return -1;
	}

	return 0;
}

int
subtlv_write(void *v, BufferObj *theBuf)
{
	uint8 temp[sizeof(uint32)];
	uint8 *p = NULL;
	int subtlvid;
	subtlvbase_s *b = (subtlvbase_s *)v;

	/* Copy the type */
	buffobj_Append(theBuf, sizeof(uint8), &b->m_id);

	/* Copy the length */
	buffobj_Append(theBuf, sizeof(uint8), &b->m_len);

	/* Copy the value */
	if (!b->m_len)
		return 0;

	/* now, store data according to type */
	subtlvid = subtlv_getid(b->m_id);
	p = temp;
	switch (subtlvid) {
	case TLV_UINT8:
	{
		SubTlvObj_uint8 *t = (SubTlvObj_uint8 *)b;
		p = &t->m_data;
		break;
	}

	/* uint 16 */
	case TLV_UINT16:
	{
		SubTlvObj_uint16 *t = (SubTlvObj_uint16 *)b;
		WpsHtonsPtr((uint8*)&t->m_data, temp);
		break;
	}

	case TLV_UINT32:
	{
		TlvObj_uint32 *t = (TlvObj_uint32 *)b;
		WpsHtonlPtr((uint8*)&t->m_data, temp);
		break;
	}

	default:
	{
		SubTlvObj_ptr *t = (SubTlvObj_ptr *)b;
		p = (uint8 *)t->m_data;
		break;
	}
	}

	buffobj_Append(theBuf, b->m_len, p);
	return 0;
}

uint16
subtlv_getId(subtlvbase_s *b)
{
	return b->m_id;
}


uint16
subtlv_getLength(subtlvbase_s *b)
{
	return b->m_len;
}

void
subtlv_serialize(uint8 theId, BufferObj *theBuf, void *data_v, uint8 m_len)
{
	uint8 temp[sizeof(uint32)];
	uint8 *data = (uint8 *)data_v;
	uint8 *p = NULL;
	int subtlvid;

	/* Copy the type */
	buffobj_Append(theBuf, sizeof(uint8), &theId);

	/* Copy the length */
	buffobj_Append(theBuf, sizeof(uint8), &m_len);

	/* Copy the value */
	if (!m_len)
		return;

	/* now, store data according to type */
	subtlvid = subtlv_getid(theId);
	p = temp;
	switch (subtlvid) {
	/* uint 16 */
	case TLV_UINT16:
		WpsHtonsPtr(data, temp);
	break;
	case TLV_UINT32:
		WpsHtonlPtr(data, temp);
	break;
	default:
		p = data;
	}

	buffobj_Append(theBuf, m_len, p);
}

void
subtlv_delete_imp(void *v, bool content_only, const char *func, int line)
{
	subtlvbase_s *b = (subtlvbase_s *)v;
	int subtlvid;
	SubTlvObj_ptr *t = NULL;

	if (!v)
		return;

	subtlvid = subtlv_getid(b->m_id);
	switch (subtlvid) {
	case TLV_CHAR_PTR:
	case TLV_UINT8_PTR:
		t = (SubTlvObj_ptr *)b;
		if (t->m_allocated && t->m_data) {
			free(t->m_data);
			t->m_data = NULL;
			tlv_freemem++;
		}
	/* fall through */
	case TLV_UINT8:
	case TLV_UINT16:
	case TLV_UINT32:
		if (!content_only) {
			free(b);
		}
		break;

	default:
		/* do not free an unidentified tlv  */
		TUTRACE((TUTRACE_ERR, "subtlv delete : trying to free an unidentified tlv "
			"object from %s at %d\n", func, line));
	}
}

char *
tlv_strncpy(void *v, char *buf, int buflen)
{
	TlvObj_ptr *t = (TlvObj_ptr *)v;
	int len;

	if (!v || !buf || buflen == 0)
		return NULL;

	len = t->tlvbase.m_len;
	if (len >= buflen)
		len = buflen - 1;

	if (len)
		memcpy(buf, t->m_data, len);

	buf[len] = 0;
	return buf;
}
