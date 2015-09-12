/*
 *   bcmwpa.c - shared WPA-related functions
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: bcmwpa.c 508617 2014-10-16 09:30:52Z $
 */

#include <bcm_cfg.h>
#include <bcmendian.h>

/* include wl driver config file if this file is compiled for driver */
#ifdef BCMDRIVER
#include <osl.h>
#elif defined(BCMEXTSUP)
#include <string.h>
#include <bcm_osl.h>
#else
#if defined(__GNUC__)
extern void bcopy(const void *src, void *dst, uint len);
extern int bcmp(const void *b1, const void *b2, uint len);
extern void bzero(void *b, uint len);
#else
#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
#define	bzero(b, len)		memset((b), 0, (len))
#endif /* defined(__GNUC__) */

#endif /* BCMDRIVER */


#include <wlioctl.h>
#include <proto/802.11.h>
#if defined(BCMSUP_PSK) || defined(BCMSUPPL) || defined(BCM_OL_DEV)
#include <proto/eapol.h>
#endif	/* defined(BCMSUP_PSK) || defined(BCMSUPPL) */
#include <bcmutils.h>
#include <bcmwpa.h>

#if defined(BCMSUP_PSK) || defined(WLFBT) || defined(BCMAUTH_PSK) || \
	defined(BCM_OL_DEV) || defined(WL_OKC)

#include <bcmcrypto/prf.h>
#include <bcmcrypto/hmac_sha256.h>
#endif

#ifdef WLTDLS
#include <bcmcrypto/sha256.h>
#endif
#if defined(BCMSUP_PSK) || defined(WLFBT) || defined(WL_OKC)
#include <bcmcrypto/rc4.h>

void
BCMROMFN(wpa_calc_pmkid)(struct ether_addr *auth_ea, struct ether_addr *sta_ea,
                         uint8 *pmk, uint pmk_len, uint8 *pmkid, uint8 *data, uint8 *digest)
{
	/* PMKID = HMAC-SHA1-128(PMK, "PMK Name" | AA | SPA) */
	const char prefix[] = "PMK Name";
	int data_len = 0;

	/* create the the data portion */
	bcopy(prefix, data, strlen(prefix));
	data_len += strlen(prefix);
	bcopy((uint8 *)auth_ea, &data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	bcopy((uint8 *)sta_ea, &data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;

	/* generate the pmkid */
	hmac_sha1(data, data_len, pmk, pmk_len, digest);
	bcopy(digest, pmkid, WPA2_PMKID_LEN);
}

#ifdef MFP
void
kdf_calc_pmkid(struct ether_addr *auth_ea, struct ether_addr *sta_ea,
	uint8 *pmk, uint pmk_len, uint8 *pmkid, uint8 *data, uint8 *digest)
{
	/* PMKID = Truncate-128(HMAC-SHA-256(PMK, "PMK Name" | AA | SPA)) */
	char prefix[] = "PMK Name";
	int data_len = 0;

	/* create the the data portion */
	bcopy(prefix, data, strlen(prefix));
	data_len += strlen(prefix);
	bcopy((uint8 *)auth_ea, &data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	bcopy((uint8 *)sta_ea, &data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;

	/* generate the pmkid */
	hmac_sha256(pmk, pmk_len, data, data_len, digest, NULL);
	bcopy(digest, pmkid, WPA2_PMKID_LEN);
}
#endif /* MFP */

#ifdef WLFBT
void
wpa_calc_pmkR0(uchar *ssid, int ssid_len, uint16 mdid, uint8 *r0kh,
	uint r0kh_len, struct ether_addr *sta_ea,
	uint8 *pmk, uint pmk_len, uint8 *pmkr0, uint8 *pmkr0name)
{
	uint8 data[128], digest[PRF_OUTBUF_LEN];
	const char prefix[] = "FT-R0";
	const char prefix1[] = "FT-R0N";
	int data_len = 0;

	bcopy(prefix, data, strlen(prefix));
	data_len += strlen(prefix);
	data[data_len++] = (uint8)ssid_len;
	bcopy(ssid, &data[data_len], ssid_len);
	data_len += ssid_len;
	htol16_ua_store(mdid, &data[data_len]);
	data_len += sizeof(uint16);
	data[data_len++] = (uint8)r0kh_len;	/* ROKHlength */
	bcopy(r0kh, &data[data_len], r0kh_len);
	data_len += r0kh_len;
	bcopy(sta_ea, &data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;

	hmac_sha256_n(pmk, pmk_len, data, data_len, digest, 48);
	bcopy(digest, pmkr0, 32);

	/* calc and return PMKR0Name */
	bcopy(prefix1, data, strlen(prefix1));
	data_len = strlen(prefix1);
	bcopy(&digest[32], &data[data_len], 16);
	data_len += 16;
	sha256(data, data_len, digest, 0);
	bcopy(digest, pmkr0name, WPA2_PMKID_LEN);
}

void
wpa_calc_pmkR1(struct ether_addr *r1kh, struct ether_addr *sta_ea, uint8 *pmk,
	uint pmk_len, uint8 *pmkr0name, uint8 *pmkr1, uint8 *pmkr1name)
{
	uint8 data[128], digest[PRF_OUTBUF_LEN];
	const char prefix[] = "FT-R1";
	const char prefix1[] = "FT-R1N";
	int data_len = 0;

	bcopy(prefix, data, strlen(prefix));
	data_len += strlen(prefix);
	bcopy(r1kh, &data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	bcopy(sta_ea, &data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;

	hmac_sha256_n(pmk, pmk_len, data, data_len, digest, 32);
	bcopy(digest, pmkr1, 32);

	/* calc and return PMKR1Name */
	bcopy(prefix1, data, strlen(prefix1));
	data_len = strlen(prefix1);
	bcopy(pmkr0name, &data[data_len], WPA2_PMKID_LEN);
	data_len += WPA2_PMKID_LEN;
	bcopy(r1kh, &data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	bcopy(sta_ea, &data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	sha256(data, data_len, digest, 0);
	bcopy(digest, pmkr1name, WPA2_PMKID_LEN);
}

void
wpa_calc_ft_ptk(struct ether_addr *bssid, struct ether_addr *sta_ea,
	uint8 *anonce, uint8* snonce, uint8 *pmk, uint pmk_len,
	uint8 *ptk, uint ptk_len)
{
	uchar data[128], prf_buff[PRF_OUTBUF_LEN];
	const char prefix[] = "FT-PTK";
	uint data_len = 0;

	bcopy(prefix, data, strlen(prefix));
	data_len += strlen(prefix);
	bcopy(snonce, (char *)&data[data_len], EAPOL_WPA_KEY_NONCE_LEN);
	data_len += EAPOL_WPA_KEY_NONCE_LEN;
	bcopy(anonce, (char *)&data[data_len], EAPOL_WPA_KEY_NONCE_LEN);
	data_len += EAPOL_WPA_KEY_NONCE_LEN;

	bcopy((uint8 *)bssid, (char *)&data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	bcopy((uint8 *)sta_ea, (char *)&data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;

	/* generate the PTK */
	hmac_sha256_n(pmk, pmk_len, data, data_len, prf_buff, ptk_len);
	bcopy(prf_buff, (char*)ptk, ptk_len);
}
#endif /* WLFBT */
#endif /* BCMSUP_PSK || WLFBT || WL_OKC */

#if defined(BCMSUP_PSK) || defined(BCM_OL_DEV)
/* Decrypt a key data from a WPA key message */
bool
BCMROMFN(wpa_decr_key_data)(eapol_wpa_key_header_t *body, uint16 key_info, uint8 *ekey,
                            uint8 *gtk, uint8 *data, uint8 *encrkey, rc4_ks_t *rc4key)
{
	uint16 len;

	switch (key_info & (WPA_KEY_DESC_V1 | WPA_KEY_DESC_V2)) {
	case WPA_KEY_DESC_V1:
		bcopy(body->iv, encrkey, WPA_MIC_KEY_LEN);
		bcopy(ekey, &encrkey[WPA_MIC_KEY_LEN], WPA_MIC_KEY_LEN);
		/* decrypt the key data */
		prepare_key(encrkey, WPA_MIC_KEY_LEN*2, rc4key);
		rc4(data, WPA_KEY_DATA_LEN_256, rc4key); /* dump 256 bytes */
		if (gtk)
			len = ntoh16_ua((uint8 *)&body->key_len);
		else
			len = ntoh16_ua((uint8 *)&body->data_len);
		rc4(body->data, len, rc4key);
		if (gtk)
			bcopy(body->data, gtk, len);
		break;

	case WPA_KEY_DESC_V2:
	case WPA_KEY_DESC_V3:
		len = ntoh16_ua((uint8 *)&body->data_len);
		if (aes_unwrap(WPA_MIC_KEY_LEN, ekey, len, body->data,
		               gtk ? gtk : body->data)) {
			return FALSE;
		}
		break;

	default:
		return FALSE;
	}
	return TRUE;
}

#ifdef MFP
void
kdf_calc_ptk(struct ether_addr *auth_ea, struct ether_addr *sta_ea,
                       uint8 *anonce, uint8* snonce, uint8 *pmk, uint pmk_len,
                       uint8 *ptk, uint ptk_len)
{
	uchar data[128], prf_buff[PRF_OUTBUF_LEN];
	const char prefix[] = "Pairwise key expansion";
	uint data_len = 0;

	/* Create the the data portion:
	 * the lesser of the EAs, followed by the greater of the EAs,
	 * followed by the lesser of the the nonces, followed by the
	 * greater of the nonces.
	 */
	bcopy(wpa_array_cmp(MIN_ARRAY, (uint8 *)auth_ea, (uint8 *)sta_ea,
	                    ETHER_ADDR_LEN),
	      (char *)&data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	bcopy(wpa_array_cmp(MAX_ARRAY, (uint8 *)auth_ea, (uint8 *)sta_ea,
	                    ETHER_ADDR_LEN),
	      (char *)&data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	bcopy(wpa_array_cmp(MIN_ARRAY, snonce, anonce,
	                    EAPOL_WPA_KEY_NONCE_LEN),
	      (char *)&data[data_len], EAPOL_WPA_KEY_NONCE_LEN);
	data_len += EAPOL_WPA_KEY_NONCE_LEN;
	bcopy(wpa_array_cmp(MAX_ARRAY, snonce, anonce,
	                    EAPOL_WPA_KEY_NONCE_LEN),
	      (char *)&data[data_len], EAPOL_WPA_KEY_NONCE_LEN);
	data_len += EAPOL_WPA_KEY_NONCE_LEN;

	/* generate the PTK */
	ASSERT(strlen(prefix) + data_len + 1 <= PRF_MAX_I_D_LEN);
	KDF(pmk, (int)pmk_len, (uchar *)prefix, strlen(prefix), data, data_len,
	     prf_buff, (int)ptk_len);
	bcopy(prf_buff, (char*)ptk, ptk_len);
}
#endif /* MFP */
/* Decrypt a group transient key from a WPA key message */
bool
BCMROMFN(wpa_decr_gtk)(eapol_wpa_key_header_t *body, uint16 key_info, uint8 *ekey,
	uint8 *gtk, uint8 *data, uint8 *encrkey, rc4_ks_t *rc4key)
{
	return wpa_decr_key_data(body, key_info, ekey, gtk, data, encrkey, rc4key);
}
#endif	/* BCMSUP_PSK */

#if defined(BCMSUP_PSK) || defined(BCMAUTH_PSK) || defined(WLFBT) || \
	defined(BCM_OL_DEV)
/* Compute Message Integrity Code (MIC) over EAPOL message */
bool
BCMROMFN(wpa_make_mic)(eapol_header_t *eapol, uint key_desc, uint8 *mic_key, uchar *mic)
{
	int mic_length;

	/* length of eapol pkt from the version field on */
	mic_length = 4 + ntoh16_ua((uint8 *)&eapol->length);

	/* Create the MIC for the pkt */
	switch (key_desc) {
	case WPA_KEY_DESC_V1:
		hmac_md5(&eapol->version, mic_length, mic_key,
		         EAPOL_WPA_KEY_MIC_LEN, mic);
		break;
	case WPA_KEY_DESC_V2:
		hmac_sha1(&eapol->version, mic_length, mic_key,
		          EAPOL_WPA_KEY_MIC_LEN, mic);
		break;
	case WPA_KEY_DESC_V3:
		aes_cmac_calc(&eapol->version, mic_length, mic_key,
		          EAPOL_WPA_KEY_MIC_LEN, mic);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

void
BCMROMFN(wpa_calc_ptk)(struct ether_addr *auth_ea, struct ether_addr *sta_ea,
                       uint8 *anonce, uint8* snonce, uint8 *pmk, uint pmk_len,
                       uint8 *ptk, uint ptk_len)
{
	uchar data[128], prf_buff[PRF_OUTBUF_LEN];
	const char prefix[] = "Pairwise key expansion";
	uint data_len = 0;

	/* Create the the data portion:
	 * the lesser of the EAs, followed by the greater of the EAs,
	 * followed by the lesser of the the nonces, followed by the
	 * greater of the nonces.
	 */
	bcopy(wpa_array_cmp(MIN_ARRAY, (uint8 *)auth_ea, (uint8 *)sta_ea,
	                    ETHER_ADDR_LEN),
	      (char *)&data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	bcopy(wpa_array_cmp(MAX_ARRAY, (uint8 *)auth_ea, (uint8 *)sta_ea,
	                    ETHER_ADDR_LEN),
	      (char *)&data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	bcopy(wpa_array_cmp(MIN_ARRAY, snonce, anonce,
	                    EAPOL_WPA_KEY_NONCE_LEN),
	      (char *)&data[data_len], EAPOL_WPA_KEY_NONCE_LEN);
	data_len += EAPOL_WPA_KEY_NONCE_LEN;
	bcopy(wpa_array_cmp(MAX_ARRAY, snonce, anonce,
	                    EAPOL_WPA_KEY_NONCE_LEN),
	      (char *)&data[data_len], EAPOL_WPA_KEY_NONCE_LEN);
	data_len += EAPOL_WPA_KEY_NONCE_LEN;

	/* generate the PTK */
	ASSERT(strlen(prefix) + data_len + 1 <= PRF_MAX_I_D_LEN);
	fPRF(pmk, (int)pmk_len, (uchar *)prefix, strlen(prefix), data, data_len,
	     prf_buff, (int)ptk_len);
	bcopy(prf_buff, (char*)ptk, ptk_len);
}

bool
wpa_encr_key_data(eapol_wpa_key_header_t *body, uint16 key_info, uint8 *ekey,
	uint8 *gtk,	uint8 *data, uint8 *encrkey, rc4_ks_t *rc4key)
{
	uint16 len;

	switch (key_info & (WPA_KEY_DESC_V1 | WPA_KEY_DESC_V2)) {
	case WPA_KEY_DESC_V1:
		if (gtk)
			len = ntoh16_ua((uint8 *)&body->key_len);
		else
			len = ntoh16_ua((uint8 *)&body->data_len);

		/* create the iv/ptk key */
		bcopy(body->iv, encrkey, 16);
		bcopy(ekey, &encrkey[16], 16);
		/* encrypt the key data */
		prepare_key(encrkey, 32, rc4key);
		rc4(data, WPA_KEY_DATA_LEN_256, rc4key); /* dump 256 bytes */
		rc4(body->data, len, rc4key);
		break;
	case WPA_KEY_DESC_V2:
	case WPA_KEY_DESC_V3:
		len = ntoh16_ua((uint8 *)&body->data_len);
		/* pad if needed - min. 16 bytes, 8 byte aligned */
		/* padding is 0xdd followed by 0's */
		if (len < 2*AKW_BLOCK_LEN) {
			body->data[len] = WPA2_KEY_DATA_PAD;
			bzero(&body->data[len+1], 2*AKW_BLOCK_LEN - (len+1));
			len = 2*AKW_BLOCK_LEN;
		} else if (len % AKW_BLOCK_LEN) {
			body->data[len] = WPA2_KEY_DATA_PAD;
			bzero(&body->data[len+1], AKW_BLOCK_LEN - ((len+1) % AKW_BLOCK_LEN));
			len += AKW_BLOCK_LEN - (len % AKW_BLOCK_LEN);
		}
		if (aes_wrap(WPA_MIC_KEY_LEN, ekey, len, body->data, body->data)) {
			return FALSE;
		}
		len += 8;
		hton16_ua_store(len, (uint8 *)&body->data_len);
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

/* Check MIC of EAPOL message */
bool
BCMROMFN(wpa_check_mic)(eapol_header_t *eapol, uint key_desc, uint8 *mic_key)
{
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	uchar digest[PRF_OUTBUF_LEN];
	uchar mic[EAPOL_WPA_KEY_MIC_LEN];

	/* save MIC and clear its space in message */
	bcopy(&body->mic, mic, EAPOL_WPA_KEY_MIC_LEN);
	bzero(&body->mic, EAPOL_WPA_KEY_MIC_LEN);

	if (!wpa_make_mic(eapol, key_desc, mic_key, digest)) {
		return FALSE;
	}
	return !bcmp(digest, mic, EAPOL_WPA_KEY_MIC_LEN);
}
#endif /* BCMSUP_PSK || BCMAUTH_PSK  || WLFBT */

#ifdef WLTDLS
void
wpa_calc_tpk(struct ether_addr *init_ea, struct ether_addr *resp_ea,
	struct ether_addr *bssid, uint8 *anonce, uint8* snonce,
                       uint8 *tpk, uint tpk_len)
{
	uchar key_input[SHA256_DIGEST_LENGTH];
	uchar data[128], tpk_buff[160];	/* TK_bits + 128, where TK_bits is 16 bytes for CCMP */
	char prefix[] = "TDLS PMK";
	uint data_len = 0;

	/* Generate TPK-Key-Input = SHA-256(min(SN, AN) || max(SN, AN)) first */
	bcopy(wpa_array_cmp(MIN_ARRAY, snonce, anonce,
	                    EAPOL_WPA_KEY_NONCE_LEN),
	      (char *)&data[0], EAPOL_WPA_KEY_NONCE_LEN);
	prhex("min(sn,an):", data, EAPOL_WPA_KEY_NONCE_LEN);
	data_len += EAPOL_WPA_KEY_NONCE_LEN;
	bcopy(wpa_array_cmp(MAX_ARRAY, snonce, anonce,
	                    EAPOL_WPA_KEY_NONCE_LEN),
	      (char *)&data[data_len], EAPOL_WPA_KEY_NONCE_LEN);
	prhex("max(sn,an):", &data[data_len], EAPOL_WPA_KEY_NONCE_LEN);
	data_len += EAPOL_WPA_KEY_NONCE_LEN;
	prhex("data:", &data[data_len], 2*EAPOL_WPA_KEY_NONCE_LEN);
	sha256(data, data_len, key_input, SHA256_DIGEST_LENGTH);
	prhex("input_key", key_input, SHA256_DIGEST_LENGTH);

	/* Create the the data portion:
	 * the lesser of the EAs, followed by the greater of the EAs,
	 * followed by BSSID
	 */
	data_len = 0;
	bcopy(prefix, data, strlen(prefix));
	prhex("prefix:", data, strlen(prefix));
	data_len  += strlen(prefix);

	bcopy(wpa_array_cmp(MIN_ARRAY, (uint8 *)init_ea, (uint8 *)resp_ea,
	                    ETHER_ADDR_LEN),
	      (char *)&data[data_len], ETHER_ADDR_LEN);
	prhex("min(init_ea, resp_ea:", &data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	bcopy(wpa_array_cmp(MAX_ARRAY, (uint8 *)init_ea, (uint8 *)resp_ea,
	                    ETHER_ADDR_LEN),
	      (char *)&data[data_len], ETHER_ADDR_LEN);
	prhex("min(init_ea, resp_ea:", &data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	bcopy((char*)bssid, (char *)&data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	prhex("data", data, data_len);

	/* generate the TPK */
	hmac_sha256_n(key_input, SHA256_DIGEST_LENGTH, data, data_len, tpk_buff, tpk_len);
	bcopy(tpk_buff, (char*)tpk, tpk_len);
}
#endif /* WLTDLS */

/* Convert WPA/WPA2 IE cipher suite to locally used value */
static bool
rsn_cipher(wpa_suite_t *suite, ushort *cipher, const uint8 *std_oui, bool wep_ok)
{
	bool ret = TRUE;

	if (!bcmp((const char *)suite->oui, std_oui, DOT11_OUI_LEN)) {
		switch (suite->type) {
		case WPA_CIPHER_TKIP:
			*cipher = CRYPTO_ALGO_TKIP;
			break;
		case WPA_CIPHER_AES_CCM:
			*cipher = CRYPTO_ALGO_AES_CCM;
			break;
		case WPA_CIPHER_WEP_40:
			if (wep_ok)
				*cipher = CRYPTO_ALGO_WEP1;
			else
				ret = FALSE;
			break;
		case WPA_CIPHER_WEP_104:
			if (wep_ok)
				*cipher = CRYPTO_ALGO_WEP128;
			else
				ret = FALSE;
			break;
		default:
			ret = FALSE;
			break;
		}
		return ret;
	}

	/* check for other vendor OUIs */
	return FALSE;
}

bool
BCMROMFN(wpa_cipher)(wpa_suite_t *suite, ushort *cipher, bool wep_ok)
{
	return rsn_cipher(suite, cipher, (const uchar*)WPA_OUI, wep_ok);
}

bool
BCMROMFN(wpa2_cipher)(wpa_suite_t *suite, ushort *cipher, bool wep_ok)
{
	return rsn_cipher(suite, cipher, (const uchar*)WPA2_OUI, wep_ok);
}

/* Is any of the tlvs the expected entry? If
 * not update the tlvs buffer pointer/length.
 */
bool
bcm_has_ie(uint8 *ie, uint8 **tlvs, uint *tlvs_len, const uint8 *oui, int oui_len, uint8 type)
{
	/* If the contents match the OUI and the type */
	if (ie[TLV_LEN_OFF] >= oui_len + 1 &&
	    !bcmp(&ie[TLV_BODY_OFF], oui, oui_len) &&
	    type == ie[TLV_BODY_OFF + oui_len]) {
		return TRUE;
	}

	/* point to the next ie */
	ie += ie[TLV_LEN_OFF] + TLV_HDR_LEN;
	/* calculate the length of the rest of the buffer */
	*tlvs_len -= (int)(ie - *tlvs);
	/* update the pointer to the start of the buffer */
	*tlvs = ie;

	return FALSE;
}

wpa_ie_fixed_t *
BCMROMFN(bcm_find_wpaie)(uint8 *parse, uint len)
{
	bcm_tlv_t *ie;

	while ((ie = bcm_parse_tlvs(parse, (int)len, DOT11_MNG_VS_ID))) {
		if (bcm_is_wpa_ie((uint8 *)ie, &parse, &len)) {
			return (wpa_ie_fixed_t *)ie;
		}
	}
	return NULL;
}

bcm_tlv_t *
bcm_find_wmeie(uint8 *parse, uint len, uint8 subtype, uint8 subtype_len)
{
	bcm_tlv_t *ie;

	while ((ie = bcm_parse_tlvs(parse, (int)len, DOT11_MNG_VS_ID))) {
		if (bcm_is_wme_ie((uint8 *)ie, &parse, &len)) {
			uint ie_len = TLV_HDR_LEN + ie->len;
			if (ie_len > TLV_HDR_LEN + WME_OUI_LEN &&
			    ((wme_ie_t *)ie->data)->subtype == subtype &&
			    ie_len == (uint)TLV_HDR_LEN + subtype_len)
				return ie;
			parse += ie_len;
			len -= ie_len;
		}
	}
	return NULL;
}

wps_ie_fixed_t *
bcm_find_wpsie(uint8 *parse, uint len)
{
	bcm_tlv_t *ie;

	while ((ie = bcm_parse_tlvs(parse, (int)len, DOT11_MNG_VS_ID))) {
		if (bcm_is_wps_ie((uint8 *)ie, &parse, &len)) {
			return (wps_ie_fixed_t *)ie;
		}
	}
	return NULL;
}

/* locate the Attribute in the WPS IE */
wps_at_fixed_t *
bcm_wps_find_at(wps_at_fixed_t *at, int len, uint16 id)
{
	while (len >= WPS_AT_FIXED_LEN) {
		int alen = WPS_AT_FIXED_LEN + ntoh16_ua(((wps_at_fixed_t *)at)->len);
		if (ntoh16_ua(((wps_at_fixed_t *)at)->at) == id && alen <= len)
			return at;
		at = (wps_at_fixed_t *)((uint8 *)at + alen);
		len -= alen;
	}
	return NULL;
}

#ifdef WLP2P
wifi_p2p_ie_t *
bcm_find_p2pie(uint8 *parse, uint len)
{
	bcm_tlv_t *ie;

	while ((ie = bcm_parse_tlvs(parse, (int)len, DOT11_MNG_VS_ID))) {
		if (bcm_is_p2p_ie((uint8 *)ie, &parse, &len)) {
			return (wifi_p2p_ie_t *)ie;
		}
	}
	return NULL;
}
#endif

bcm_tlv_t *
bcm_find_hs20ie(uint8 *parse, uint len)
{
	bcm_tlv_t *ie;

	while ((ie = bcm_parse_tlvs(parse, (int)len, DOT11_MNG_VS_ID))) {
		if (bcm_is_hs20_ie((uint8 *)ie, &parse, &len)) {
			return ie;
		}
	}
	return NULL;
}

bcm_tlv_t *
BCMROMFN(bcm_find_osenie)(uint8 *parse, uint len)
{
	bcm_tlv_t *ie;

	while ((ie = bcm_parse_tlvs(parse, (int)len, DOT11_MNG_VS_ID))) {
		if (bcm_is_osen_ie((uint8 *)ie, &parse, &len)) {
			return ie;
		}
	}
	return NULL;
}

#if defined(BCMSUP_PSK) || defined(BCMSUPPL) || defined(BCM_OL_DEV)
#define wpa_is_kde(ie, tlvs, len, type)	bcm_has_ie(ie, tlvs, len, \
	(const uint8 *)WPA2_OUI, WPA2_OUI_LEN, type)

eapol_wpa2_encap_data_t *
BCMROMFN(wpa_find_kde)(uint8 *parse, uint len, uint8 type)
{
	bcm_tlv_t *ie;

	while ((ie = bcm_parse_tlvs(parse, (int)len, DOT11_MNG_PROPR_ID))) {
		if (wpa_is_kde((uint8 *)ie, &parse, &len, type)) {
			return (eapol_wpa2_encap_data_t *)ie;
		}
	}
	return NULL;
}

bool
BCMROMFN(wpa_is_gtk_encap)(uint8 *ie, uint8 **tlvs, uint *tlvs_len)
{
	return wpa_is_kde(ie, tlvs, tlvs_len, WPA2_KEY_DATA_SUBTYPE_GTK);
}

eapol_wpa2_encap_data_t *
BCMROMFN(wpa_find_gtk_encap)(uint8 *parse, uint len)
{
	return wpa_find_kde(parse, len, WPA2_KEY_DATA_SUBTYPE_GTK);
}
#endif /* defined(BCMSUP_PSK) || defined(BCMSUPPL) */

uint8 *
BCMROMFN(wpa_array_cmp)(int max_array, uint8 *x, uint8 *y, uint len)
{
	uint i;
	uint8 *ret = x;

	for (i = 0; i < len; i++)
		if (x[i] != y[i])
			break;

	if (i == len) {
		/* returning null will cause crash, return value used for bcopy */
		/* return first param in this case to close security loophole */
		return x;
	}
	if (max_array && (y[i] > x[i]))
		ret = y;
	if (!max_array && (y[i] < x[i]))
		ret = y;

	return (ret);
}

void
BCMROMFN(wpa_incr_array)(uint8 *array, uint len)
{
	int i;

	for (i = (len-1); i >= 0; i--)
		if (array[i]++ != 0xff) {
			break;
		}
}

/* map akm suite to internal WPA_AUTH_XXXX */
/* akms points to 4 byte suite (oui + type) */
bool
BCMROMFN(bcmwpa_akm2WPAauth)(uint8 *akm, uint32 *auth, bool sta_iswpa)
{
	if (!bcmp(akm, WPA2_OUI, DOT11_OUI_LEN)) {
		switch (akm[DOT11_OUI_LEN]) {
		case RSN_AKM_NONE:
			*auth = WPA_AUTH_NONE;
			break;
		case RSN_AKM_UNSPECIFIED:
			*auth = WPA2_AUTH_UNSPECIFIED;
			break;
		case RSN_AKM_PSK:
			*auth = WPA2_AUTH_PSK;
			break;
		case RSN_AKM_FBT_1X:
			*auth = WPA2_AUTH_UNSPECIFIED | WPA2_AUTH_FT;
			break;
		case RSN_AKM_FBT_PSK:
			*auth = WPA2_AUTH_PSK | WPA2_AUTH_FT;
			break;
		case RSN_AKM_MFP_1X:
			*auth = WPA2_AUTH_UNSPECIFIED;
			break;
		case RSN_AKM_MFP_PSK:
			*auth = WPA2_AUTH_PSK;
			break;

		default:
			return FALSE;
		}
		return TRUE;
	}
	else
#ifdef WLOSEN
	if (!bcmp(akm, WFA_OUI, WFA_OUI_LEN)) {
		switch (akm[WFA_OUI_LEN]) {
		case OSEN_AKM_UNSPECIFIED:
			*auth = WPA2_AUTH_UNSPECIFIED;
			break;

		default:
			return FALSE;
		}
		return TRUE;
	}
	else
#endif	/* WLOSEN */
	if (!bcmp(akm, WPA_OUI, DOT11_OUI_LEN)) {
		switch (akm[DOT11_OUI_LEN]) {
		case RSN_AKM_NONE:
			*auth = WPA_AUTH_NONE;
			break;
		case RSN_AKM_UNSPECIFIED:
			*auth = WPA_AUTH_UNSPECIFIED;
			break;
		case RSN_AKM_PSK:
			*auth = WPA_AUTH_PSK;
			break;
		default:
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

/* map cipher suite to internal WSEC_XXXX */
/* cs points 4 byte cipher suite, and only the type is used for non CCX ciphers */
bool
BCMROMFN(bcmwpa_cipher2wsec)(uint8 *cipher, uint32 *wsec)
{
	switch (cipher[DOT11_OUI_LEN]) {
	case WPA_CIPHER_NONE:
		*wsec = 0;
		break;
	case WPA_CIPHER_WEP_40:
	case WPA_CIPHER_WEP_104:
		*wsec = WEP_ENABLED;
		break;
	case WPA_CIPHER_TKIP:
		*wsec = TKIP_ENABLED;
		break;
	case WPA_CIPHER_AES_CCM:
		*wsec = AES_ENABLED;
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

/* map WPA/RSN cipher to internal WSEC */
uint32
bcmwpa_wpaciphers2wsec(uint8 wpacipher)
{
	uint32 wsec = 0;

	switch (wpacipher) {
	case WPA_CIPHER_NONE:
		break;
	case WPA_CIPHER_WEP_40:
	case WPA_CIPHER_WEP_104:
		wsec = WEP_ENABLED;
		break;
	case WPA_CIPHER_TKIP:
		wsec = TKIP_ENABLED;
		break;
	case WPA_CIPHER_AES_OCB:
	case WPA_CIPHER_AES_CCM:
		wsec = AES_ENABLED;
		break;
	default:
		break;
	}

	return wsec;
}

bool
bcmwpa_is_wpa_auth(uint32 wpa_auth)
{
	uint16 auth = (uint16)wpa_auth;

	if ((auth == WPA_AUTH_NONE) ||
	   (auth == WPA_AUTH_UNSPECIFIED) ||
	   (auth == WPA_AUTH_PSK))
		return TRUE;
	else
		return FALSE;
}

bool
bcmwpa_includes_wpa_auth(uint32 wpa_auth)
{
	uint16 auth = (uint16)wpa_auth;

	if (auth & (WPA_AUTH_NONE |
		WPA_AUTH_UNSPECIFIED |
		WPA_AUTH_PSK))
		return TRUE;
	else
		return FALSE;
}

bool
bcmwpa_is_wpa2_auth(uint32 wpa_auth)
{
	uint16 auth = (uint16)wpa_auth;

	auth = auth & ~WPA2_AUTH_FT;

	if ((auth == WPA2_AUTH_UNSPECIFIED) ||
	   (auth == WPA2_AUTH_PSK) ||
	   (auth == BRCM_AUTH_PSK) ||
	   (auth == BRCM_AUTH_DPT))
		return TRUE;
	else
		return FALSE;
}

bool
bcmwpa_includes_wpa2_auth(uint32 wpa_auth)
{
	uint16 auth = (uint16)wpa_auth;

	if (auth & (WPA2_AUTH_UNSPECIFIED |
		WPA2_AUTH_PSK |
		BRCM_AUTH_PSK |
		BRCM_AUTH_DPT))
		return TRUE;
	else
		return FALSE;
}
