/*
 * bcmwpa.h - interface definitions of shared WPA-related functions
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: bcmwpa.h 508617 2014-10-16 09:30:52Z $
 */

#ifndef _BCMWPA_H_
#define _BCMWPA_H_

#include <proto/wpa.h>
#if defined(BCMSUP_PSK) || defined(BCMSUPPL) || defined(MFP) || defined(BCMAUTH_PSK) || \
	defined(WLFBT) || defined(BCM_OL_DEV) || defined(WL_OKC)
#include <proto/eapol.h>
#endif
#include <proto/802.11.h>
#ifdef WLP2P
#include <proto/p2p.h>
#endif
#include <bcmcrypto/rc4.h>
#include <bcmutils.h>
#include <wlioctl.h>

/* Field sizes for WPA key hierarchy */
#define WPA_MIC_KEY_LEN		16
#define WPA_ENCR_KEY_LEN	16
#define WPA_TEMP_ENCR_KEY_LEN	16
#define WPA_TEMP_TX_KEY_LEN	8
#define WPA_TEMP_RX_KEY_LEN	8

#define PMK_LEN			32
#define TKIP_PTK_LEN		64
#define TKIP_TK_LEN		32
#define AES_PTK_LEN		48
#define AES_TK_LEN		16

/* limits for pre-shared key lengths */
#define WPA_MIN_PSK_LEN		8
#define WPA_MAX_PSK_LEN		64

#define WPA_KEY_DATA_LEN_256	256	/* allocation size of 256 for temp data pointer. */
#define WPA_KEY_DATA_LEN_128	128	/* allocation size of 128 for temp data pointer. */

#define WLC_SW_KEYS(wlc, bsscfg) ((((wlc)->wsec_swkeys) || \
	((bsscfg)->wsec & WSEC_SWFLAG)))




#define IS_WPA_AKM(akm)	((akm) == RSN_AKM_NONE || \
				 (akm) == RSN_AKM_UNSPECIFIED || \
				 (akm) == RSN_AKM_PSK)
#define IS_WPA2_AKM(akm)	((akm) == RSN_AKM_UNSPECIFIED || \
				 (akm) == RSN_AKM_PSK)
#define IS_FBT_AKM(akm)	((akm) == RSN_AKM_FBT_1X || \
				 (akm) == RSN_AKM_FBT_PSK)
#define IS_MFP_AKM(akm)	((akm) == RSN_AKM_MFP_1X || \
				 (akm) == RSN_AKM_MFP_PSK)
#define IS_TDLS_AKM(akm)        ((akm) == RSN_AKM_TPK)

/* Broadcom(OUI) authenticated key managment suite */
#define BRCM_AKM_NONE           0
#define BRCM_AKM_PSK            1       /* Proprietary PSK AKM */
#define BRCM_AKM_DPT            2       /* Proprietary DPT PSK AKM */

#define IS_BRCM_AKM(akm)        ((akm) == BRCM_AKM_PSK)

#define MAX_ARRAY 1
#define MIN_ARRAY 0

/* convert wsec to WPA mcast cipher. algo is needed only when WEP is enabled. */
#define WPA_MCAST_CIPHER(wsec, algo)	(WSEC_WEP_ENABLED(wsec) ? \
		((algo) == CRYPTO_ALGO_WEP128 ? WPA_CIPHER_WEP_104 : WPA_CIPHER_WEP_40) : \
			WSEC_TKIP_ENABLED(wsec) ? WPA_CIPHER_TKIP : \
			WSEC_AES_ENABLED(wsec) ? WPA_CIPHER_AES_CCM : \
			WPA_CIPHER_NONE)

#define WPS_ATID_SEL_REGISTRAR		0x1041

#define WPS_IE_FIXED_LEN	6

/* GTK indices we use - 0-3 valid per IEEE/802.11 2012 */
#define GTK_INDEX_1       1
#define GTK_INDEX_2       2

/* IGTK indices we use - 4-5 are valid per IEEE 802.11 2012 */
#define IGTK_INDEX_1      4
#define IGTK_INDEX_2      5

#define IS_IGTK_INDEX(x) ((x) == IGTK_INDEX_1 || (x) == IGTK_INDEX_2)

/* special reserved wsec index wsec key table for IGTK */
#define IGTK_ID_TO_WSEC_INDEX(x) (-(int)(x))
#define IS_IGTK_WSEC_INDEX_1(x) (x == IGTK_ID_TO_WSEC_INDEX(IGTK_INDEX_1))
#define IS_IGTK_WSEC_INDEX_2(x) (x == IGTK_ID_TO_WSEC_INDEX(IGTK_INDEX_2))
#define IS_IGTK_WSEC_INDEX(x) (IS_IGTK_WSEC_INDEX_1(x) || \
	IS_IGTK_WSEC_INDEX_2(x))

/* WiFi WPS Attribute fixed portion */
typedef struct wps_at_fixed {
	uint8 at[2];
	uint8 len[2];
	uint8 data[1];
} wps_at_fixed_t;

#define WPS_AT_FIXED_LEN	4

#define wps_ie_fixed_t wpa_ie_fixed_t

/* Return address of max or min array depending first argument.
 * Return NULL in case of a draw.
 */
extern uint8 *BCMROMFN(wpa_array_cmp)(int max_array, uint8 *x, uint8 *y, uint len);

/* Increment the array argument */
extern void BCMROMFN(wpa_incr_array)(uint8 *array, uint len);

/* Convert WPA IE cipher suite to locally used value */
extern bool BCMROMFN(wpa_cipher)(wpa_suite_t *suite, ushort *cipher, bool wep_ok);

/* Look for a WPA IE; return it's address if found, NULL otherwise */
extern wpa_ie_fixed_t *BCMROMFN(bcm_find_wpaie)(uint8 *parse, uint len);
extern bcm_tlv_t *bcm_find_wmeie(uint8 *parse, uint len, uint8 subtype, uint8 subtype_len);
/* Look for a WPS IE; return it's address if found, NULL otherwise */
extern wps_ie_fixed_t *bcm_find_wpsie(uint8 *parse, uint len);
extern wps_at_fixed_t *bcm_wps_find_at(wps_at_fixed_t *at, int len, uint16 id);
#ifdef WLP2P
/* Look for a WiFi P2P IE; return it's address if found, NULL otherwise */
extern wifi_p2p_ie_t *bcm_find_p2pie(uint8 *parse, uint len);
#endif
/* Look for a hotspot2.0 IE; return it's address if found, NULL otherwise */
bcm_tlv_t *bcm_find_hs20ie(uint8 *parse, uint len);
/* Look for a OSEN IE; return it's address if found, NULL otherwise */
bcm_tlv_t *bcm_find_osenie(uint8 *parse, uint len);

/* Check whether the given IE has the specific OUI and the specific type. */
extern bool bcm_has_ie(uint8 *ie, uint8 **tlvs, uint *tlvs_len,
                       const uint8 *oui, int oui_len, uint8 type);

/* Check whether pointed-to IE looks like WPA. */
#define bcm_is_wpa_ie(ie, tlvs, len)	bcm_has_ie(ie, tlvs, len, \
	(const uint8 *)WPA_OUI, WPA_OUI_LEN, WPA_OUI_TYPE)
/* Check whether pointed-to IE looks like WME. */
#define bcm_is_wme_ie(ie, tlvs, len)	bcm_has_ie(ie, tlvs, len, \
	(const uint8 *)WME_OUI, WME_OUI_LEN, WME_OUI_TYPE)
/* Check whether pointed-to IE looks like WPS. */
#define bcm_is_wps_ie(ie, tlvs, len)	bcm_has_ie(ie, tlvs, len, \
	(const uint8 *)WPS_OUI, WPS_OUI_LEN, WPS_OUI_TYPE)
#ifdef WLP2P
/* Check whether the given IE looks like WFA P2P IE. */
#define bcm_is_p2p_ie(ie, tlvs, len)	bcm_has_ie(ie, tlvs, len, \
	(const uint8 *)WFA_OUI, WFA_OUI_LEN, WFA_OUI_TYPE_P2P)
#endif
/* Check whether the given IE looks like WFA hotspot2.0 IE. */
#define bcm_is_hs20_ie(ie, tlvs, len)	bcm_has_ie(ie, tlvs, len, \
	(const uint8 *)WFA_OUI, WFA_OUI_LEN, WFA_OUI_TYPE_HS20)
/* Check whether the given IE looks like WFA OSEN IE. */
#define bcm_is_osen_ie(ie, tlvs, len)	bcm_has_ie(ie, tlvs, len, \
	(const uint8 *)WFA_OUI, WFA_OUI_LEN, WFA_OUI_TYPE_OSEN)

/* Convert WPA2 IE cipher suite to locally used value */
extern bool BCMROMFN(wpa2_cipher)(wpa_suite_t *suite, ushort *cipher, bool wep_ok);

#if defined(BCMSUP_PSK) || defined(BCMSUPPL) || defined(BCM_OL_DEV)
/* Look for an encapsulated GTK; return it's address if found, NULL otherwise */
extern eapol_wpa2_encap_data_t *BCMROMFN(wpa_find_gtk_encap)(uint8 *parse, uint len);

/* Check whether pointed-to IE looks like an encapsulated GTK. */
extern bool BCMROMFN(wpa_is_gtk_encap)(uint8 *ie, uint8 **tlvs, uint *tlvs_len);

/* Look for encapsulated key data; return it's address if found, NULL otherwise */
extern eapol_wpa2_encap_data_t *BCMROMFN(wpa_find_kde)(uint8 *parse, uint len, uint8 type);
#endif /* defined(BCMSUP_PSK) || defined(BCMSUPPL) */

#if defined(BCMSUP_PSK) || defined(WLFBT) || defined(BCMAUTH_PSK)|| defined(BCM_OL_DEV) \
	|| defined(WL_OKC)
/* Calculate a pair-wise transient key */
extern void BCMROMFN(wpa_calc_ptk)(struct ether_addr *auth_ea, struct ether_addr *sta_ea,
                                   uint8 *anonce, uint8* snonce, uint8 *pmk, uint pmk_len,
                                   uint8 *ptk, uint ptk_len);

/* Compute Message Integrity Code (MIC) over EAPOL message */
extern bool BCMROMFN(wpa_make_mic)(eapol_header_t *eapol, uint key_desc, uint8 *mic_key,
                                   uchar *mic);

/* Check MIC of EAPOL message */
extern bool BCMROMFN(wpa_check_mic)(eapol_header_t *eapol, uint key_desc, uint8 *mic_key);

/* Calculate PMKID */
extern void BCMROMFN(wpa_calc_pmkid)(struct ether_addr *auth_ea, struct ether_addr *sta_ea,
	uint8 *pmk, uint pmk_len, uint8 *pmkid, uint8 *data, uint8 *digest);

/* Calculate PMKR0 for FT association */
extern void wpa_calc_pmkR0(uchar *ssid, int ssid_len, uint16 mdid, uint8 *r0kh,
	uint r0kh_len, struct ether_addr *sta_ea,
	uint8 *pmk, uint pmk_len, uint8 *pmkid, uint8 *pmkr0name);

/* Calculate PMKR1 for FT association */
extern void wpa_calc_pmkR1(struct ether_addr *r1kh, struct ether_addr *sta_ea,
	uint8 *pmk, uint pmk_len, uint8 *pmkr0name, uint8 *pmkid, uint8 *pmkr1name);

/* Calculate PTK for FT association */
extern void wpa_calc_ft_ptk(struct ether_addr *bssid, struct ether_addr *sta_ea,
	uint8 *anonce, uint8* snonce, uint8 *pmk, uint pmk_len,
	uint8 *ptk, uint ptk_len);

/* Encrypt key data for a WPA key message */
extern bool wpa_encr_key_data(eapol_wpa_key_header_t *body, uint16 key_info,
	uint8 *ekey, uint8 *gtk, uint8 *data, uint8 *encrkey, rc4_ks_t *rc4key);

/* Decrypt key data from a WPA key message */
extern bool BCMROMFN(wpa_decr_key_data)(eapol_wpa_key_header_t *body, uint16 key_info,
	uint8 *ekey, uint8 *gtk, uint8 *data, uint8 *encrkey, rc4_ks_t *rc4key);

/* Decrypt a group transient key from a WPA key message */
extern bool BCMROMFN(wpa_decr_gtk)(eapol_wpa_key_header_t *body, uint16 key_info,
	uint8 *ekey, uint8 *gtk, uint8 *data, uint8 *encrkey, rc4_ks_t *rc4key);
#endif	/* BCMSUP_PSK || WLFBT || BCMAUTH_PSK || BCM_OL_DEV */

extern bool BCMROMFN(bcmwpa_akm2WPAauth)(uint8 *akm, uint32 *auth, bool sta_iswpa);

extern bool BCMROMFN(bcmwpa_cipher2wsec)(uint8 *cipher, uint32 *wsec);
extern uint32 bcmwpa_wpaciphers2wsec(uint8 unicast);

#ifdef MFP
/* Calculate PMKID */
extern void kdf_calc_pmkid(struct ether_addr *auth_ea, struct ether_addr *sta_ea,
	uint8 *pmk, uint pmk_len, uint8 *pmkid, uint8 *data, uint8 *digest);
extern void kdf_calc_ptk(struct ether_addr *auth_ea, struct ether_addr *sta_ea,
                                   uint8 *anonce, uint8* snonce, uint8 *pmk, uint pmk_len,
                                   uint8 *ptk, uint ptk_len);
#endif

#ifdef WLTDLS
/* Calculate TPK for TDLS association */
extern void wpa_calc_tpk(struct ether_addr *init_ea, struct ether_addr *resp_ea,
struct ether_addr *bssid, uint8 *anonce, uint8* snonce, uint8 *tpk, uint tpk_len);
#endif

extern bool bcmwpa_is_wpa_auth(uint32 wpa_auth);
extern bool bcmwpa_includes_wpa_auth(uint32 wpa_auth);
extern bool bcmwpa_is_wpa2_auth(uint32 wpa_auth);
extern bool bcmwpa_includes_wpa2_auth(uint32 wpa_auth);
#endif	/* _BCMWPA_H_ */
