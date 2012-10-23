/*
 * Broadcom Home Gateway Reference Design
 * Web Page Configuration Support Routines
 *
 * Copyright 2004, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 * $Id: broadcom.c,v 1.1.1.1 2010/10/15 02:24:15 shinjung Exp $
 */

#ifdef WEBS
#include <webs.h>
#include <uemf.h>
#include <ej.h>
#else /* !WEBS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <httpd.h>
#endif /* WEBS */


#include <typedefs.h>
#include <proto/ethernet.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <shutils.h>
#include <wlutils.h>
#include <linux/types.h>
#include <shared.h>
#include <wlscan.h>
#include <sysinfo.h>

#define EZC_FLAGS_READ		0x0001
#define EZC_FLAGS_WRITE		0x0002
#define EZC_FLAGS_CRYPT		0x0004

#define EZC_CRYPT_KEY		"620A83A6960E48d1B05D49B0288A2C1F"

#define EZC_SUCCESS	 	0
#define EZC_ERR_NOT_ENABLED 	1
#define EZC_ERR_INVALID_STATE 	2
#define EZC_ERR_INVALID_DATA 	3
#ifndef NOUSB
static const char * const apply_header =
"<head>"
"<title>Broadcom Home Gateway Reference Design: Apply</title>"
"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">"
"<style type=\"text/css\">"
"body { background: white; color: black; font-family: arial, sans-serif; font-size: 9pt }"
".title	{ font-family: arial, sans-serif; font-size: 13pt; font-weight: bold }"
".subtitle { font-family: arial, sans-serif; font-size: 11pt }"
".label { color: #306498; font-family: arial, sans-serif; font-size: 7pt }"
"</style>"
"</head>"
"<body>"
"<p>"
"<span class=\"title\">APPLY</span><br>"
"<span class=\"subtitle\">This screen notifies you of any errors "
"that were detected while changing the router's settings.</span>"
"<form method=\"get\" action=\"apply.cgi\">"
"<p>"
;

static const char * const apply_footer =
"<p>"
"<input type=\"button\" name=\"action\" value=\"Continue\" OnClick=\"document.location.href='%s';\">"
"</form>"
"<p class=\"label\">&#169;2001-2004 Broadcom Corporation. All rights reserved.</p>"
"</body>"
;
#endif

#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/klog.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <net/if.h>

typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if_arp.h>

#define sys_restart() kill(1, SIGHUP)
#define sys_reboot() kill(1, SIGTERM)
#define sys_stats(url) eval("stats", (url))

int
ej_wl_sta_status(int eid, webs_t wp, char *name)
{
	// TODO
	return 0;
}

#include <bcmendian.h>
#include <bcmparams.h>		/* for DEV_NUMIFS */

#define SSID_FMT_BUF_LEN 4*32+1	/* Length for SSID format string */

/* The below macros handle endian mis-matches between wl utility and wl driver. */
static bool g_swap = FALSE;
#define htod32(i) (g_swap?bcmswap32(i):(uint32)(i))
#define dtoh32(i) (g_swap?bcmswap32(i):(uint32)(i))
#define dtoh16(i) (g_swap?bcmswap16(i):(uint16)(i))
#define dtohchanspec(i) (g_swap?dtoh16(i):i)

/* 802.11i/WPA RSN IE parsing utilities */
typedef struct {
	uint16 version;
	wpa_suite_mcast_t *mcast;
	wpa_suite_ucast_t *ucast;
	wpa_suite_auth_key_mgmt_t *akm;
	uint8 *capabilities;
} rsn_parse_info_t;

/* Helper routine to print the infrastructure mode while pretty printing the BSS list */
#if 0
static const char *
capmode2str(uint16 capability)
{
	capability &= (DOT11_CAP_ESS | DOT11_CAP_IBSS);

	if (capability == DOT11_CAP_ESS)
		return "Managed";
	else if (capability == DOT11_CAP_IBSS)
		return "Ad Hoc";
	else
		return "<unknown>";
}
#endif
int
dump_rateset(int eid, webs_t wp, int argc, char_t **argv, uint8 *rates, uint count)
{
	uint i;
	uint r;
	bool b;
	int retval = 0;

	retval += websWrite(wp, "[ ");
	for (i = 0; i < count; i++) {
		r = rates[i] & 0x7f;
		b = rates[i] & 0x80;
		if (r == 0)
			break;
		retval += websWrite(wp, "%d%s%s ", (r / 2), (r % 2)?".5":"", b?"(b)":"");
	}
	retval += websWrite(wp, "]");

	return retval;
}

/* Chanspec ASCII representation:
 * <channel><band><bandwidth><ctl-sideband>
 *   digit   [AB]     [N]        [UL]
 *
 * <channel>: channel number of the 10MHz or 20MHz channel,
 *	or control sideband channel of 40MHz channel.
 * <band>: A for 5GHz, B for 2.4GHz
 * <bandwidth>: N for 10MHz, nothing for 20MHz or 40MHz
 *	(ctl-sideband spec implies 40MHz)
 * <ctl-sideband>: U for upper, L for lower
 *
 * <band> may be omitted on input, and will be assumed to be
 * 2.4GHz if channel number <= 14.
 *
 * Examples:
 *	8  ->  2.4GHz channel 8, 20MHz
 *	8b ->  2.4GHz channel 8, 20MHz
 *	8l ->  2.4GHz channel 8, 40MHz, lower ctl sideband
 *	8a ->  5GHz channel 8 (low 5 GHz band), 20MHz
 *	36 ->  5GHz channel 36, 20MHz
 *	36l -> 5GHz channel 36, 40MHz, lower ctl sideband
 *	40u -> 5GHz channel 40, 40MHz, upper ctl sideband
 *	180n -> channel 180, 10MHz
 */


/* given a chanspec and a string buffer, format the chanspec as a
 * string, and return the original pointer a.
 * Min buffer length must be CHANSPEC_STR_LEN.
 * On error return NULL
 */
char *
wf_chspec_ntoa(chanspec_t chspec, char *buf)
{
	const char *band, *bw, *sb;
	uint channel;

	band = "";
	bw = "";
	sb = "";
	channel = CHSPEC_CHANNEL(chspec);
	/* check for non-default band spec */
	if ((CHSPEC_IS2G(chspec) && channel > CH_MAX_2G_CHANNEL) ||
	    (CHSPEC_IS5G(chspec) && channel <= CH_MAX_2G_CHANNEL))
		band = (CHSPEC_IS2G(chspec)) ? "b" : "a";
	if (CHSPEC_IS40(chspec)) {
		if (CHSPEC_SB_UPPER(chspec)) {
			sb = "u";
			channel += CH_10MHZ_APART;
		} else {
			sb = "l";
			channel -= CH_10MHZ_APART;
		}
	} else if (CHSPEC_IS10(chspec)) {
		bw = "n";
	}

	/* Outputs a max of 6 chars including '\0'  */
	snprintf(buf, 6, "%d%s%s%s", channel, band, bw, sb);
	return (buf);
}

char *
wl_ether_etoa(const struct ether_addr *n)
{
	static char etoa_buf[ETHER_ADDR_LEN * 3];
	char *c = etoa_buf;
	int i;

	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		if (i)
			*c++ = ':';
		c += sprintf(c, "%02X", n->octet[i] & 0xff);
	}
	return etoa_buf;
}

static int
wlu_bcmp(const void *b1, const void *b2, int len)
{
	return (memcmp(b1, b2, len));
}

/*
 * Traverse a string of 1-byte tag/1-byte length/variable-length value
 * triples, returning a pointer to the substring whose first element
 * matches tag
 */
static uint8 *
wlu_parse_tlvs(uint8 *tlv_buf, int buflen, uint key)
{
	uint8 *cp;
	int totlen;

	cp = tlv_buf;
	totlen = buflen;

	/* find tagged parameter */
	while (totlen >= 2) {
		uint tag;
		int len;

		tag = *cp;
		len = *(cp +1);

		/* validate remaining totlen */
		if ((tag == key) && (totlen >= (len + 2)))
			return (cp);

		cp += (len + 2);
		totlen -= (len + 2);
	}

	return NULL;
}

/* Is this body of this tlvs entry a WPA entry? If */
/* not update the tlvs buffer pointer/length */
static bool
wlu_is_wpa_ie(uint8 **wpaie, uint8 **tlvs, uint *tlvs_len)
{
	uint8 *ie = *wpaie;

	/* If the contents match the WPA_OUI and type=1 */
	if ((ie[1] >= 6) && !wlu_bcmp(&ie[2], WPA_OUI "\x01", 4)) {
		return TRUE;
	}

	/* point to the next ie */
	ie += ie[1] + 2;
	/* calculate the length of the rest of the buffer */
	*tlvs_len -= (int)(ie - *tlvs);
	/* update the pointer to the start of the buffer */
	*tlvs = ie;

	return FALSE;
}

/* Validates and parses the RSN or WPA IE contents into a rsn_parse_info_t structure
 * Returns 0 on success, or 1 if the information in the buffer is not consistant with
 * an RSN IE or WPA IE.
 * The buf pointer passed in should be pointing at the version field in either an RSN IE
 * or WPA IE.
 */
static int
wl_rsn_ie_parse_info(uint8* rsn_buf, uint len, rsn_parse_info_t *rsn)
{
	uint16 count;

	memset(rsn, 0, sizeof(rsn_parse_info_t));

	/* version */
	if (len < sizeof(uint16))
		return 1;

	rsn->version = ltoh16_ua(rsn_buf);
	len -= sizeof(uint16);
	rsn_buf += sizeof(uint16);

	/* Multicast Suite */
	if (len < sizeof(wpa_suite_mcast_t))
		return 0;

	rsn->mcast = (wpa_suite_mcast_t*)rsn_buf;
	len -= sizeof(wpa_suite_mcast_t);
	rsn_buf += sizeof(wpa_suite_mcast_t);

	/* Unicast Suite */
	if (len < sizeof(uint16))
		return 0;

	count = ltoh16_ua(rsn_buf);

	if (len < (sizeof(uint16) + count * sizeof(wpa_suite_t)))
		return 1;

	rsn->ucast = (wpa_suite_ucast_t*)rsn_buf;
	len -= (sizeof(uint16) + count * sizeof(wpa_suite_t));
	rsn_buf += (sizeof(uint16) + count * sizeof(wpa_suite_t));

	/* AKM Suite */
	if (len < sizeof(uint16))
		return 0;

	count = ltoh16_ua(rsn_buf);

	if (len < (sizeof(uint16) + count * sizeof(wpa_suite_t)))
		return 1;

	rsn->akm = (wpa_suite_auth_key_mgmt_t*)rsn_buf;
	len -= (sizeof(uint16) + count * sizeof(wpa_suite_t));
	rsn_buf += (sizeof(uint16) + count * sizeof(wpa_suite_t));

	/* Capabilites */
	if (len < sizeof(uint16))
		return 0;

	rsn->capabilities = rsn_buf;

	return 0;
}

static uint
wl_rsn_ie_decode_cntrs(uint cntr_field)
{
	uint cntrs;

	switch (cntr_field) {
	case RSN_CAP_1_REPLAY_CNTR:
		cntrs = 1;
		break;
	case RSN_CAP_2_REPLAY_CNTRS:
		cntrs = 2;
		break;
	case RSN_CAP_4_REPLAY_CNTRS:
		cntrs = 4;
		break;
	case RSN_CAP_16_REPLAY_CNTRS:
		cntrs = 16;
		break;
	default:
		cntrs = 0;
		break;
	}

	return cntrs;
}

static int
wl_rsn_ie_dump(int eid, webs_t wp, int argc, char_t **argv, bcm_tlv_t *ie)
{
	int i;
	int rsn;
	wpa_ie_fixed_t *wpa = NULL;
	rsn_parse_info_t rsn_info;
	wpa_suite_t *suite;
	uint8 std_oui[3];
	int unicast_count = 0;
	int akm_count = 0;
	uint16 capabilities;
	uint cntrs;
	int err;
	int retval = 0;

	if (ie->id == DOT11_MNG_RSN_ID) {
		rsn = TRUE;
		memcpy(std_oui, WPA2_OUI, WPA_OUI_LEN);
		err = wl_rsn_ie_parse_info(ie->data, ie->len, &rsn_info);
	} else {
		rsn = FALSE;
		memcpy(std_oui, WPA_OUI, WPA_OUI_LEN);
		wpa = (wpa_ie_fixed_t*)ie;
		err = wl_rsn_ie_parse_info((uint8*)&wpa->version, wpa->length - WPA_IE_OUITYPE_LEN,
					   &rsn_info);
	}
	if (err || rsn_info.version != WPA_VERSION)
		return retval;

	if (rsn)
		retval += websWrite(wp, "RSN:\n");
	else
		retval += websWrite(wp, "WPA:\n");

	/* Check for multicast suite */
	if (rsn_info.mcast) {
		retval += websWrite(wp, "\tmulticast cipher: ");
		if (!wlu_bcmp(rsn_info.mcast->oui, std_oui, 3)) {
			switch (rsn_info.mcast->type) {
			case WPA_CIPHER_NONE:
				retval += websWrite(wp, "NONE\n");
				break;
			case WPA_CIPHER_WEP_40:
				retval += websWrite(wp, "WEP64\n");
				break;
			case WPA_CIPHER_WEP_104:
				retval += websWrite(wp, "WEP128\n");
				break;
			case WPA_CIPHER_TKIP:
				retval += websWrite(wp, "TKIP\n");
				break;
			case WPA_CIPHER_AES_OCB:
				retval += websWrite(wp, "AES-OCB\n");
				break;
			case WPA_CIPHER_AES_CCM:
				retval += websWrite(wp, "AES-CCMP\n");
				break;
			default:
				retval += websWrite(wp, "Unknown-%s(#%d)\n", rsn ? "RSN" : "WPA",
				       rsn_info.mcast->type);
				break;
			}
		}
		else {
			retval += websWrite(wp, "Unknown-%02X:%02X:%02X(#%d) ",
			       rsn_info.mcast->oui[0], rsn_info.mcast->oui[1],
			       rsn_info.mcast->oui[2], rsn_info.mcast->type);
		}
	}

	/* Check for unicast suite(s) */
	if (rsn_info.ucast) {
		unicast_count = ltoh16_ua(&rsn_info.ucast->count);
		retval += websWrite(wp, "\tunicast ciphers(%d): ", unicast_count);
		for (i = 0; i < unicast_count; i++) {
			suite = &rsn_info.ucast->list[i];
			if (!wlu_bcmp(suite->oui, std_oui, 3)) {
				switch (suite->type) {
				case WPA_CIPHER_NONE:
					retval += websWrite(wp, "NONE ");
					break;
				case WPA_CIPHER_WEP_40:
					retval += websWrite(wp, "WEP64 ");
					break;
				case WPA_CIPHER_WEP_104:
					retval += websWrite(wp, "WEP128 ");
					break;
				case WPA_CIPHER_TKIP:
					retval += websWrite(wp, "TKIP ");
					break;
				case WPA_CIPHER_AES_OCB:
					retval += websWrite(wp, "AES-OCB ");
					break;
				case WPA_CIPHER_AES_CCM:
					retval += websWrite(wp, "AES-CCMP ");
					break;
				default:
					retval += websWrite(wp, "WPA-Unknown-%s(#%d) ", rsn ? "RSN" : "WPA",
					       suite->type);
					break;
				}
			}
			else {
				retval += websWrite(wp, "Unknown-%02X:%02X:%02X(#%d) ",
					suite->oui[0], suite->oui[1], suite->oui[2],
					suite->type);
			}
		}
		retval += websWrite(wp, "\n");
	}
	/* Authentication Key Management */
	if (rsn_info.akm) {
		akm_count = ltoh16_ua(&rsn_info.akm->count);
		retval += websWrite(wp, "\tAKM Suites(%d): ", akm_count);
		for (i = 0; i < akm_count; i++) {
			suite = &rsn_info.akm->list[i];
			if (!wlu_bcmp(suite->oui, std_oui, 3)) {
				switch (suite->type) {
				case RSN_AKM_NONE:
					retval += websWrite(wp, "None ");
					break;
				case RSN_AKM_UNSPECIFIED:
					retval += websWrite(wp, "WPA ");
					break;
				case RSN_AKM_PSK:
					retval += websWrite(wp, "WPA-PSK ");
					break;
				default:
					retval += websWrite(wp, "Unknown-%s(#%d)  ",
					       rsn ? "RSN" : "WPA", suite->type);
					break;
				}
			}
			else {
				retval += websWrite(wp, "Unknown-%02X:%02X:%02X(#%d)  ",
					suite->oui[0], suite->oui[1], suite->oui[2],
					suite->type);
			}
		}
		retval += websWrite(wp, "\n");
	}

	/* Capabilities */
	if (rsn_info.capabilities) {
		capabilities = ltoh16_ua(rsn_info.capabilities);
		retval += websWrite(wp, "\tCapabilities(0x%04x): ", capabilities);
		if (rsn)
			retval += websWrite(wp, "%sPre-Auth, ", (capabilities & RSN_CAP_PREAUTH) ? "" : "No ");

		retval += websWrite(wp, "%sPairwise, ", (capabilities & RSN_CAP_NOPAIRWISE) ? "No " : "");

		cntrs = wl_rsn_ie_decode_cntrs((capabilities & RSN_CAP_PTK_REPLAY_CNTR_MASK) >>
					       RSN_CAP_PTK_REPLAY_CNTR_SHIFT);

		retval += websWrite(wp, "%d PTK Replay Ctr%s", cntrs, (cntrs > 1)?"s":"");

		if (rsn) {
			cntrs = wl_rsn_ie_decode_cntrs(
				(capabilities & RSN_CAP_GTK_REPLAY_CNTR_MASK) >>
				RSN_CAP_GTK_REPLAY_CNTR_SHIFT);

			retval += websWrite(wp, "%d GTK Replay Ctr%s\n", cntrs, (cntrs > 1)?"s":"");
		} else {
			retval += websWrite(wp, "\n");
		}
	} else {
		retval += websWrite(wp, "\tNo %s Capabilities advertised\n", rsn ? "RSN" : "WPA");
	}

	return retval;
}

static int
wl_dump_wpa_rsn_ies(int eid, webs_t wp, int argc, char_t **argv, uint8* cp, uint len)
{
	uint8 *parse = cp;
	uint parse_len = len;
	uint8 *wpaie;
	uint8 *rsnie;
	int retval = 0;

	while ((wpaie = wlu_parse_tlvs(parse, parse_len, DOT11_MNG_WPA_ID)))
		if (wlu_is_wpa_ie(&wpaie, &parse, &parse_len))
			break;
	if (wpaie)
		retval += wl_rsn_ie_dump(eid, wp, argc, argv, (bcm_tlv_t*)wpaie);

	rsnie = wlu_parse_tlvs(cp, len, DOT11_MNG_RSN_ID);
	if (rsnie)
		retval += wl_rsn_ie_dump(eid, wp, argc, argv, (bcm_tlv_t*)rsnie);

	return retval;
}

int
wl_format_ssid(char* ssid_buf, uint8* ssid, int ssid_len)
{
	int i, c;
	char *p = ssid_buf;

	if (ssid_len > 32) ssid_len = 32;

	for (i = 0; i < ssid_len; i++) {
		c = (int)ssid[i];
		if (c == '\\') {
			*p++ = '\\';
//			*p++ = '\\';
		} else if (isprint((uchar)c)) {
			*p++ = (char)c;
		} else {
			p += sprintf(p, "\\x%02X", c);
		}
	}
	*p = '\0';

	return p - ssid_buf;
}

static int
dump_bss_info(int eid, webs_t wp, int argc, char_t **argv, wl_bss_info_t *bi)
{
	char ssidbuf[SSID_FMT_BUF_LEN];
	char chspec_str[CHANSPEC_STR_LEN];
	wl_bss_info_107_t *old_bi;
	int mcs_idx = 0;
	int retval = 0;

	/* Convert version 107 to 109 */
	if (dtoh32(bi->version) == LEGACY_WL_BSS_INFO_VERSION) {
		old_bi = (wl_bss_info_107_t *)bi;
		bi->chanspec = CH20MHZ_CHSPEC(old_bi->channel);
		bi->ie_length = old_bi->ie_length;
		bi->ie_offset = sizeof(wl_bss_info_107_t);
	}

	wl_format_ssid(ssidbuf, bi->SSID, bi->SSID_len);

	retval += websWrite(wp, "SSID: \"%s\"\n", ssidbuf);

//	retval += websWrite(wp, "Mode: %s\t", capmode2str(dtoh16(bi->capability)));
	retval += websWrite(wp, "RSSI: %d dBm\t", (int16)(dtoh16(bi->RSSI)));

	/*
	 * SNR has valid value in only 109 version.
	 * So print SNR for 109 version only.
	 */
	if (dtoh32(bi->version) == WL_BSS_INFO_VERSION) {
		retval += websWrite(wp, "SNR: %d dB\t", (int16)(dtoh16(bi->SNR)));
	}

	retval += websWrite(wp, "noise: %d dBm\t", bi->phy_noise);
	if (bi->flags) {
		bi->flags = dtoh16(bi->flags);
		retval += websWrite(wp, "Flags: ");
		if (bi->flags & WL_BSS_FLAGS_FROM_BEACON) retval += websWrite(wp, "FromBcn ");
		if (bi->flags & WL_BSS_FLAGS_FROM_CACHE) retval += websWrite(wp, "Cached ");
		if (bi->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL) retval += websWrite(wp, "RSSI on-channel ");
		retval += websWrite(wp, "\t");
	}
	retval += websWrite(wp, "Channel: %s\n", wf_chspec_ntoa(dtohchanspec(bi->chanspec), chspec_str));

	retval += websWrite(wp, "BSSID: %s\t", wl_ether_etoa(&bi->BSSID));

	retval += websWrite(wp, "Capability: ");
	bi->capability = dtoh16(bi->capability);
	if (bi->capability & DOT11_CAP_ESS) retval += websWrite(wp, "ESS ");
	if (bi->capability & DOT11_CAP_IBSS) retval += websWrite(wp, "IBSS ");
	if (bi->capability & DOT11_CAP_POLLABLE) retval += websWrite(wp, "Pollable ");
	if (bi->capability & DOT11_CAP_POLL_RQ) retval += websWrite(wp, "PollReq ");
	if (bi->capability & DOT11_CAP_PRIVACY) retval += websWrite(wp, "WEP ");
	if (bi->capability & DOT11_CAP_SHORT) retval += websWrite(wp, "ShortPre ");
	if (bi->capability & DOT11_CAP_PBCC) retval += websWrite(wp, "PBCC ");
	if (bi->capability & DOT11_CAP_AGILITY) retval += websWrite(wp, "Agility ");
	if (bi->capability & DOT11_CAP_SHORTSLOT) retval += websWrite(wp, "ShortSlot ");
	if (bi->capability & DOT11_CAP_CCK_OFDM) retval += websWrite(wp, "CCK-OFDM ");
	retval += websWrite(wp, "\n");

	retval += websWrite(wp, "Supported Rates: ");
	retval += dump_rateset(eid, wp, argc, argv, bi->rateset.rates, dtoh32(bi->rateset.count));
	retval += websWrite(wp, "\n");
	if (dtoh32(bi->ie_length))
		retval += wl_dump_wpa_rsn_ies(eid, wp, argc, argv, (uint8 *)(((uint8 *)bi) + dtoh16(bi->ie_offset)),
				    dtoh32(bi->ie_length));

	if (dtoh32(bi->version) != LEGACY_WL_BSS_INFO_VERSION && bi->n_cap) {
		retval += websWrite(wp, "802.11N Capable:\n");
		bi->chanspec = dtohchanspec(bi->chanspec);
		retval += websWrite(wp, "\tChanspec: %sGHz channel %d %dMHz (0x%x)\n",
			CHSPEC_IS2G(bi->chanspec)?"2.4":"5", CHSPEC_CHANNEL(bi->chanspec),
			CHSPEC_IS40(bi->chanspec) ? 40 : (CHSPEC_IS20(bi->chanspec) ? 20 : 10),
			bi->chanspec);
		retval += websWrite(wp, "\tControl channel: %d\n", bi->ctl_ch);
		retval += websWrite(wp, "\t802.11N Capabilities: ");
		if (dtoh32(bi->nbss_cap) & HT_CAP_40MHZ)
			retval += websWrite(wp, "40Mhz ");
		if (dtoh32(bi->nbss_cap) & HT_CAP_SHORT_GI_20)
			retval += websWrite(wp, "SGI20 ");
		if (dtoh32(bi->nbss_cap) & HT_CAP_SHORT_GI_40)
			retval += websWrite(wp, "SGI40 ");
		retval += websWrite(wp, "\n\tSupported MCS : [ ");
		for (mcs_idx = 0; mcs_idx < (MCSSET_LEN * 8); mcs_idx++)
			if (isset(bi->basic_mcs, mcs_idx))
				retval += websWrite(wp, "%d ", mcs_idx);
		retval += websWrite(wp, "]\n");
	}

	retval += websWrite(wp, "\n");

	return retval;
}


static int
wl_status(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
	int ret;
	struct ether_addr bssid;
	wlc_ssid_t ssid;
	char ssidbuf[SSID_FMT_BUF_LEN];
	wl_bss_info_t *bi;
	int retval = 0;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char *name;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	if ((ret = wl_ioctl(name, WLC_GET_BSSID, &bssid, ETHER_ADDR_LEN)) == 0) {
		/* The adapter is associated. */
		*(uint32*)buf = htod32(WLC_IOCTL_MAXLEN);
		if ((ret = wl_ioctl(name, WLC_GET_BSS_INFO, buf, WLC_IOCTL_MAXLEN)) < 0)
			return 0;

		bi = (wl_bss_info_t*)(buf + 4);
		if (dtoh32(bi->version) == WL_BSS_INFO_VERSION ||
		    dtoh32(bi->version) == LEGACY2_WL_BSS_INFO_VERSION ||
		    dtoh32(bi->version) == LEGACY_WL_BSS_INFO_VERSION)
			retval += dump_bss_info(eid, wp, argc, argv, bi);
		else
			retval += websWrite(wp, "Sorry, your driver has bss_info_version %d "
				"but this program supports only version %d.\n",
				bi->version, WL_BSS_INFO_VERSION);
	} else {
		retval += websWrite(wp, "Not associated. Last associated with ");

		if ((ret = wl_ioctl(name, WLC_GET_SSID, &ssid, sizeof(wlc_ssid_t))) < 0) {
			retval += websWrite(wp, "\n");
			return 0;
		}

		wl_format_ssid(ssidbuf, ssid.SSID, dtoh32(ssid.SSID_len));
		retval += websWrite(wp, "SSID: \"%s\"\n", ssidbuf);
	}

	return retval;
}

int
ej_wl_status(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char *name;
	char name_vif[] = "wlX.Y_XXXXXXXXXX";
	struct maclist *auth, *assoc, *authorized;
	int max_sta_count, maclist_size;
	int i, j, val = 0, ret = 0;
	int ii, jj;
	char *arplist = NULL, *arplistptr;
	char *leaselist = NULL, *leaselistptr;
#ifdef RTCONFIG_DNSMASQ
	char hostnameentry[16], hostname[16];
#endif
	char ip[40], ipentry[40], macentry[18];
	int found = 0;
        char rxrate[5], txrate[5];
        scb_val_t rssi;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
#ifdef RTCONFIG_WIRELESSREPEATER
	if ((nvram_get_int("sw_mode") == SW_MODE_REPEATER)
		&& (nvram_get_int("wlc_band") == unit))
	{
		sprintf(name_vif, "wl%d.%d", unit, 1);
		name = name_vif;
	}
	else
#endif
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
	wl_ioctl(name, WLC_GET_RADIO, &val, sizeof(val));
	val &= WL_RADIO_SW_DISABLE | WL_RADIO_HW_DISABLE;

	
	if (nvram_match(strcat_r(prefix, "mode", tmp), "wds")) {
		// dump static info only for wds mode:
		// ret += websWrite(wp, "SSID: %s\n", nvram_safe_get(strcat_r(prefix, "ssid", tmp)));
		ret += websWrite(wp, "Channel: %s\n", nvram_safe_get(strcat_r(prefix, "channel", tmp)));
	}
	else {
		ret += wl_status(eid, wp, argc, argv, unit);
	}

	if (val) 
	{
		ret += websWrite(wp, "Radio is disabled\n");
		return ret;
	}

	if (nvram_match(strcat_r(prefix, "mode", tmp), "ap"))
	{
		if (nvram_match(strcat_r(prefix, "lazywds", tmp), "1") ||
			nvram_invmatch(strcat_r(prefix, "wds", tmp), ""))
			ret += websWrite(wp, "Mode	: Hybrid\n");
		else    ret += websWrite(wp, "Mode	: AP Only\n");
	}
	else if (nvram_match(strcat_r(prefix, "mode", tmp), "wds"))
	{
		ret += websWrite(wp, "Mode	: WDS Only\n");
		return ret;
	}
	else if (nvram_match(strcat_r(prefix, "mode", tmp), "sta"))
	{
		ret += websWrite(wp, "Mode	: Stations\n");
		ret += ej_wl_sta_status(eid, wp, name);
		return ret;
	}
	else if (nvram_match(strcat_r(prefix, "mode", tmp), "wet"))
	{
//		ret += websWrite(wp, "Mode	: Ethernet Bridge\n");
#ifdef RTCONFIG_WIRELESSREPEATER
		if ((nvram_get_int("sw_mode") == SW_MODE_REPEATER)
			&& (nvram_get_int("wlc_band") == unit))
			sprintf(prefix, "wl%d.%d_", unit, 1);
#endif		
		ret += websWrite(wp, "Mode	: Repeater [ SSID local: \"%s\" ]\n", nvram_safe_get(strcat_r(prefix, "ssid", tmp)));
//		ret += ej_wl_sta_status(eid, wp, name);
//		return ret;
	}

	/* buffers and length */
	max_sta_count = 128;
	maclist_size = sizeof(auth->count) + max_sta_count * sizeof(struct ether_addr);
	auth = malloc(maclist_size);
	assoc = malloc(maclist_size);
	authorized = malloc(maclist_size);

	char buf[sizeof(sta_info_t)];

	if (!auth || !assoc || !authorized)
		goto exit;

	/* query wl for authenticated sta list */
	strcpy((char*)auth, "authe_sta_list");
	if (wl_ioctl(name, WLC_GET_VAR, auth, maclist_size))
		goto exit;

	/* query wl for associated sta list */
	assoc->count = max_sta_count;
	if (wl_ioctl(name, WLC_GET_ASSOCLIST, assoc, maclist_size))
		goto exit;

	/* query wl for authorized sta list */
	strcpy((char*)authorized, "autho_sta_list");
	if (wl_ioctl(name, WLC_GET_VAR, authorized, maclist_size))
		goto exit;

	/* Obtain mac + IP list */
	arplist = read_whole_file("/proc/net/arp");
	/* Obtain lease list - we still need the arp list for
          cases where a device uses a static IP rather than DHCP */
#ifdef RTCONFIG_DNSMASQ
	leaselist = read_whole_file("/var/lib/misc/dnsmasq.leases");
#endif

	ret += websWrite(wp, "\n");
	if (leaselist) {
	        ret += websWrite(wp, "Stations List                                        Rx/Tx  speed  rssi     state\n");
	        ret += websWrite(wp, "--------------------------------------------------------------------------------------\n");
	} else {
	        ret += websWrite(wp, "Stations List                        Rx/Tx  speed   rssi    state\n");
	        ret += websWrite(wp, "----------------------------------------------------------------------\n");
	}
//                            00:00:00:00:00:00 111.222.333.444 hostnamexxxxxxx  xxxx/xxxx Mbps  -xx dBm  assoc auth

	/* build authenticated/associated/authorized sta list */
	for (i = 0; i < auth->count; i ++) {
		char ea[ETHER_ADDR_STR_LEN];

		ret += websWrite(wp, "%s ", ether_etoa((void *)&auth->ea[i], ea));

		/* Retrieve IP from arp cache */
		if (arplist) {
			arplistptr = arplist;

			while ((found == 0) && (arplistptr < arplist+strlen(arplist)-2) && (sscanf(arplistptr,"%s %*s %*s %s",ipentry,macentry))) {
				if (strcmp(macentry, ether_etoa((void *)&auth->ea[i], ea)) == 0)
					found = 1;
				else
					arplistptr = strstr(arplistptr,"\n")+1;
			}
			if (found == 1) {
				sprintf(ip,"%-15s",ipentry);
				found = 0;
			} else {
				strcpy(ip,"               ");
			}
			ret += websWrite(wp, "%s ",ip);
		}

#ifdef RTCONFIG_DNSMASQ
		// Retrieve hostname from dnsmasq leases
		if (leaselist) {
			leaselistptr = strstr(leaselist,ipentry); // ip entry more efficient than MAC
			if (leaselistptr) {
				sscanf(leaselistptr, "%*s %15s", hostnameentry);
				sprintf(hostname,"%-15s ",hostnameentry);
			} else {
				sprintf(hostname,"%-15s ","");
			}
			ret += websWrite(wp, hostname);
		}
#endif

		// Get additional info: rate, rssi
		memset(buf, 0, sizeof(buf));
		strcpy(buf, "sta_info");
		memcpy(buf + strlen(buf) + 1, (unsigned char *)&auth->ea[i], ETHER_ADDR_LEN);

		if (!wl_ioctl(name, WLC_GET_VAR, buf, sizeof(buf))) {
			sta_info_t *sta = (sta_info_t *)buf;

			if ((int)sta->rx_rate > 0)
				sprintf(rxrate,"%d", sta->rx_rate / 1000);
			else
				strcpy(rxrate,"??");

			if ((int)sta->tx_rate > 0)
				sprintf(txrate,"%d", sta->tx_rate / 1000);
			else
				sprintf(rxrate,"??");

			sprintf(tmp," %4s/%-4s Mbps ", rxrate, txrate);
			ret += websWrite(wp,tmp);
		}

		rssi.ea = auth->ea[i];
		rssi.val = 0;

		if ((wl_ioctl(name, WLC_GET_RSSI, &rssi, sizeof(rssi)) != 0) || (rssi.val <= 0))
			strcpy(tmp,"  ?? dBm ");
		else
			sprintf(tmp," %3d dBm ", rssi.val);

		ret += websWrite(wp, tmp);


		for (j = 0; j < assoc->count; j ++) {
			if (!bcmp((void *)&auth->ea[i], (void *)&assoc->ea[j], ETHER_ADDR_LEN)) {
				ret += websWrite(wp, " assoc");
				break;
			}
		}

		for (j = 0; j < authorized->count; j ++) {
			if (!bcmp((void *)&auth->ea[i], (void *)&authorized->ea[j], ETHER_ADDR_LEN)) {
				ret += websWrite(wp, " auth");
				break;
			}
		}
		ret += websWrite(wp, "\n");
	}

	for (i = 1; i < 4; i++) {
#ifdef RTCONFIG_WIRELESSREPEATER
		if ((nvram_get_int("sw_mode") == SW_MODE_REPEATER)
			&& (unit == nvram_get_int("wlc_band")) && (i == 1))
			break;
#endif
		sprintf(prefix, "wl%d.%d_", unit, i);
		if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
		{
			sprintf(name_vif, "wl%d.%d", unit, i);

			/* query wl for authenticated sta list */
			strcpy((char*)auth, "authe_sta_list");
			if (wl_ioctl(name_vif, WLC_GET_VAR, auth, maclist_size))
				goto exit;

			/* query wl for associated sta list */
			assoc->count = max_sta_count;
			if (wl_ioctl(name_vif, WLC_GET_ASSOCLIST, assoc, maclist_size))
				goto exit;

			/* query wl for authorized sta list */
			strcpy((char*)authorized, "autho_sta_list");
			if (wl_ioctl(name_vif, WLC_GET_VAR, authorized, maclist_size))
				goto exit;

			for (ii = 0; ii < auth->count; ii++) {
				char ea[ETHER_ADDR_STR_LEN];

				ret += websWrite(wp, "%s ", ether_etoa((void *)&auth->ea[ii], ea));

				/* Retrieve IP from arp cache */
				if (arplist) {
					arplistptr = arplist;

					while ((found == 0) && (arplistptr < arplist+strlen(arplist)-2) && (sscanf(arplistptr,"%s %*s %*s %s",ipentry,macentry))) {
						if (strcmp(macentry, ether_etoa((void *)&auth->ea[ii], ea)) == 0)
							found = 1;
						else
							arplistptr = strstr(arplistptr,"\n")+1;
					}
					if (found == 1) {
						sprintf(ip,"%-15s",ipentry);
						found = 0;
					} else {
						strcpy(ip,"               ");
					}
					ret += websWrite(wp, "%s ",ip);
				}

#ifdef RTCONFIG_DNSMASQ
				// Retrieve hostname from dnsmasq leases
				if (leaselist) {
					leaselistptr = strstr(leaselist,ipentry); // ip entry more efficient than MAC
					if (leaselistptr) {
						sscanf(leaselistptr, "%*s %15s", hostnameentry);
						sprintf(hostname,"%-15s ",hostnameentry);
					} else {
						sprintf(hostname,"%-15s ","");
					}
					ret += websWrite(wp, hostname);
				}
#endif

				// Get additional info: Rate, rssi
				memset(buf, 0, sizeof(buf));
				strcpy(buf, "sta_info");
				memcpy(buf + strlen(buf) + 1, (unsigned char *)&auth->ea[ii], ETHER_ADDR_LEN);

				if (!wl_ioctl(name, WLC_GET_VAR, buf, sizeof(buf))) {
					sta_info_t *sta = (sta_info_t *)buf;

					if ((int)sta->rx_rate > 0)
						sprintf(rxrate,"%d", sta->rx_rate / 1000);
					else
						strcpy(rxrate,"??");

					if ((int)sta->tx_rate > 0)
						sprintf(txrate,"%d", sta->tx_rate / 1000);
					else
						sprintf(rxrate,"??");

					sprintf(tmp," %4s/%-4s Mbps ", rxrate, txrate);
					ret += websWrite(wp,tmp);
				}

				rssi.ea = auth->ea[ii];
				rssi.val = 0;

				if ((wl_ioctl(name, WLC_GET_RSSI, &rssi, sizeof(rssi)) != 0) || (rssi.val <= 0))
					strcpy(tmp,"  ?? dBm ");
				else
					sprintf(tmp," %3d dBm ", rssi.val);

				ret += websWrite(wp, tmp);


				for (jj = 0; jj < assoc->count; jj++) {
					if (!bcmp((void *)&auth->ea[ii], (void *)&assoc->ea[jj], ETHER_ADDR_LEN)) {
						ret += websWrite(wp, " assoc");
						break;
					}
				}

				for (jj = 0; jj < authorized->count; jj++) {
					if (!bcmp((void *)&auth->ea[ii], (void *)&authorized->ea[jj], ETHER_ADDR_LEN)) {
						ret += websWrite(wp, " auth");
						break;
					}
				}
				ret += websWrite(wp, "\n");
			}
		}
	}

	/* error/exit */
exit:
	if (auth) free(auth);
	if (assoc) free(assoc);
	if (authorized) free(authorized);
	if (arplist) free(arplist);
	if (leaselist) free(leaselist);

	return ret;
}

int
ej_wl_status_2g(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;
	int ii = 0;
	char nv_param[NVRAM_MAX_PARAM_LEN];
	char *temp;

	for (ii = 0; ii < DEV_NUMIFS; ii++) {
		sprintf(nv_param, "wl%d_unit", ii);
		temp = nvram_get(nv_param);

		if (temp && strlen(temp) > 0)
		{
			retval += ej_wl_status(eid, wp, argc, argv, ii);
			retval += websWrite(wp, "\n");
		}
	}

	return retval;
}

static int
wl_control_channel(int unit)
{
	int ret;
	struct ether_addr bssid;
	wl_bss_info_t *bi;
	wl_bss_info_107_t *old_bi;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char *name;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	if ((ret = wl_ioctl(name, WLC_GET_BSSID, &bssid, ETHER_ADDR_LEN)) == 0) {
		/* The adapter is associated. */
		*(uint32*)buf = htod32(WLC_IOCTL_MAXLEN);
		if ((ret = wl_ioctl(name, WLC_GET_BSS_INFO, buf, WLC_IOCTL_MAXLEN)) < 0)
			return 0;

		bi = (wl_bss_info_t*)(buf + 4);
		if (dtoh32(bi->version) == WL_BSS_INFO_VERSION ||
		    dtoh32(bi->version) == LEGACY2_WL_BSS_INFO_VERSION ||
		    dtoh32(bi->version) == LEGACY_WL_BSS_INFO_VERSION)
		{
			/* Convert version 107 to 109 */
			if (dtoh32(bi->version) == LEGACY_WL_BSS_INFO_VERSION) {
				old_bi = (wl_bss_info_107_t *)bi;
				bi->chanspec = CH20MHZ_CHSPEC(old_bi->channel);
				bi->ie_length = old_bi->ie_length;
				bi->ie_offset = sizeof(wl_bss_info_107_t);
			}

        		if (dtoh32(bi->version) != LEGACY_WL_BSS_INFO_VERSION && bi->n_cap)
                		return bi->ctl_ch;

		}
	}

	return 0;
}

int
ej_wl_control_channel(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret = 0;
	int channel_24 = 0, channel_50 = 0;

	if (!(channel_24 = wl_control_channel(0)))
	{
		ret = websWrite(wp, "[\"0\"]");
		return ret;
	}

	if (!(channel_50 = wl_control_channel(1)))
		ret = websWrite(wp, "[\"%d\", \"%d\"]", channel_24, 0);
	else
		ret = websWrite(wp, "[\"%d\", \"%d\"]", channel_24, channel_50);

	return ret;
}

static int ej_wl_channel_list(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
	int i, retval = 0;
	char buf[4096];
	wl_channels_in_country_t *cic = (wl_channels_in_country_t *)buf;
	char tmp[256], prefix[] = "wlXXXXXXXXXX_";
	char *country_code;
	char *name;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	country_code = nvram_safe_get(strcat_r(prefix, "country_code", tmp));
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	cic->buflen = sizeof(buf);
	strcpy(cic->country_abbrev, country_code);
	if (!unit)
		cic->band = WLC_BAND_2G;
	else
		cic->band = WLC_BAND_5G;
	cic->count = 0;

	if (wl_ioctl(name, WLC_GET_CHANNELS_IN_COUNTRY, cic, cic->buflen) != 0)
		return retval;

	if (cic->count == 0)
		return retval;
	else
	{
		memset(tmp, 0x0, sizeof(tmp));

		for (i = 0; i < cic->count; i++)
		{
#if 0
			if (!strlen(tmp))
				sprintf(tmp, "%d", cic->channel[i]);
			else
				sprintf(tmp, "%s %d", tmp, cic->channel[i]);
#else
			if (i == 0)
				sprintf(tmp, "[\"%d\",", cic->channel[i]);
			else if (i == (cic->count - 1))
				sprintf(tmp,  "%s \"%d\"]", tmp, cic->channel[i]);
			else
				sprintf(tmp,  "%s \"%d\",", tmp, cic->channel[i]);
#endif
		}

		retval += websWrite(wp, "%s", tmp);
	}

	return retval;
}

int
ej_wl_channel_list_2g(int eid, webs_t wp, int argc, char_t **argv)
{
	return ej_wl_channel_list(eid, wp, argc, argv, 0);
}

int
ej_wl_channel_list_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	return ej_wl_channel_list(eid, wp, argc, argv, 1);
}

static int wps_error_count = 0;

char *
getWscStatusStr()
{
	char *status;

	status = nvram_safe_get("wps_proc_status");

	switch (atoi(status)) {
#if 1	/* AP mode */
	case 1: /* WPS_ASSOCIATED */
		wps_error_count = 0;
		return "Start WPS Process";
		break;
	case 2: /* WPS_OK */
	case 7: /* WPS_MSGDONE */
		wps_error_count = 0;
		return "Success";
		break;
	case 3: /* WPS_MSG_ERR */
		if (++wps_error_count > 60)
		{
			wps_error_count = 0;
			nvram_set("wps_proc_status", "0");
		}
		return "Fail due to WPS message exchange error!";
		break;
	case 4: /* WPS_TIMEOUT */
		if (++wps_error_count > 60)
		{
			wps_error_count = 0;
			nvram_set("wps_proc_status", "0");
		}
		return "Fail due to WPS time out!";
		break;
        case 8: /* WPS_PBCOVERLAP */
		if (++wps_error_count > 60)
		{
			wps_error_count = 0;
			nvram_set("wps_proc_status", "0");
		}
		return "Fail due to WPS time out!";
                return "Fail due to WPS session overlap!";
                break;
	default:
		wps_error_count = 0;
		if (nvram_match("wps_enable", "1"))
			return "Idle";
		else
			return "Not used";
		break;
#else	/* STA mode */
	case 0:
		return "Idle";
		break;
	case 1: /* WPS_ASSOCIATED */
		return "Start enrolling...";
		break;
	case 2: /* WPS_OK */
		return "Succeeded...";
		break;
	case 3: /* WPS_MSG_ERR */
		return "Failed...";
		break;
	case 4: /* WPS_TIMEOUT */
		return "Failed (timeout)...";
		break;
	case 7: /* WPS_MSGDONE */
		return "Success";
		break;
	case 8: /* WPS_PBCOVERLAP */
		return "Failed (pbc overlap)...";
		break;
	case 9: /* WPS_FIND_PBC_AP */
		return "Finding a pbc access point...";
		break;
	case 10: /* WPS_ASSOCIATING */
		return "Assciating with access point...";
		break;
	default:
//		return "Init...";
		return "Start WPS Process";
		break;
#endif
	}
}

int
wps_is_oob()
{
	char tmp[16];

	snprintf(tmp, sizeof(tmp), "lan_wps_oob");

	/*
	 * OOB: enabled
	 * Configured: disabled
	 */
	if (nvram_match(tmp, "disabled"))
		return 0;

	return 1;
}

void getWPSAuthMode(int unit, char *ret_str)
{
	char tmp[128], prefix[]="wlXXXXXXX_";

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "shared"))
		strcpy(ret_str, "Shared Key");
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "psk"))
		strcpy(ret_str, "WPA-Personal");
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "psk2"))
		strcpy(ret_str, "WPA2-Personal");
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "pskpsk2"))
		strcpy(ret_str, "WPA-Auto-Personal");
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpa"))
		strcpy(ret_str, "WPA-Enterprise");
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpa2"))
		strcpy(ret_str, "WPA2-Enterprise");
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpawpa2"))
		strcpy(ret_str, "WPA-Auto-Enterprise");
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "radius"))
		strcpy(ret_str, "802.1X");
	else
		strcpy(ret_str, "Open System");
}

void getWPSEncrypType(int unit, char *ret_str)
{
	char tmp[128], prefix[]="wlXXXXXXX_";

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "open") &&
		nvram_match(strcat_r(prefix, "wep_x", tmp), "0"))
		strcpy(ret_str, "None");
	else if ((nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "open") && !nvram_match(strcat_r(prefix, "wep_x", tmp), "0")) ||
		nvram_match("wl_auth_mode", "shared") ||
		nvram_match("wl_auth_mode", "radius"))
		strcpy(ret_str, "WEP");

	if (nvram_match(strcat_r(prefix, "crypto", tmp), "tkip"))
		strcpy(ret_str, "TKIP");
	else if (nvram_match(strcat_r(prefix, "crypto", tmp), "aes"))
		strcpy(ret_str, "AES");
	else if (nvram_match(strcat_r(prefix, "crypto", tmp), "tkip+aes"))
		strcpy(ret_str, "TKIP+AES");
}

int wl_wps_info(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
	char tmpstr[256];
	int retval = 0;
	char tmp[128], prefix[]="wlXXXXXXX_";
	char *wps_sta_pin;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	retval += websWrite(wp, "<wps>\n");

	//0. WSC Status
	if (!strcmp(nvram_safe_get(strcat_r(prefix, "wps_mode", tmp)), "enabled"))
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", getWscStatusStr());
	else
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "Not used");

	//1. WPSConfigured
	if (!wps_is_oob())
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "Yes");
	else
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "No");
	
	//2. WPSSSID
	memset(tmpstr, 0, sizeof(tmpstr));
	char_to_ascii(tmpstr, nvram_safe_get(strcat_r(prefix, "ssid", tmp)));
	retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);

	//3. WPSAuthMode
	memset(tmpstr, 0, sizeof(tmpstr));
	getWPSAuthMode(unit, tmpstr);
	retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);

	//4. EncrypType
	memset(tmpstr, 0, sizeof(tmpstr));
	getWPSEncrypType(unit, tmpstr);
	retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);

	//5. DefaultKeyIdx
	memset(tmpstr, 0, sizeof(tmpstr));
	sprintf(tmpstr, "%s", nvram_safe_get(strcat_r(prefix, "key", tmp)));
	retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);

	//6. WPAKey
	if (!strlen(nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp))))
		retval += websWrite(wp, "<wps_info>None</wps_info>\n");
	else
	{
		memset(tmpstr, 0, sizeof(tmpstr));
		char_to_ascii(tmpstr, nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp)));
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);
	}

	//7. AP PIN Code
	memset(tmpstr, 0, sizeof(tmpstr));
	sprintf(tmpstr, "%s", nvram_safe_get("wps_device_pin"));
	retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);

	//8. Saved WPAKey
	if (!strlen(nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp))))
	{
		retval += websWrite(wp, "<wps_info>None</wps_info>\n");
	}
	else
	{
		memset(tmpstr, 0, sizeof(tmpstr));
		char_to_ascii(tmpstr, nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp)));
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);
	}

	//9. WPS enable?
	if (!strcmp(nvram_safe_get(strcat_r(prefix, "wps_mode", tmp)), "enabled"))
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "1");
	else
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "1");

	//A. WPS mode
	wps_sta_pin = nvram_safe_get("wps_sta_pin");
	if (strlen(wps_sta_pin) && strcmp(wps_sta_pin, "00000000"))
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "1");
	else
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "2");
	
	//B. current auth mode
	retval += websWrite(wp, "<wps_info>%s</wps_info>\n", nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp)));

	//C. WPS band
	retval += websWrite(wp, "<wps_info>%d</wps_info>\n", nvram_get_int("wps_band"));

	retval += websWrite(wp, "</wps>");

	return retval;
}

int
ej_wps_info(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_wps_info(eid, wp, argc, argv, 1);
}

int
ej_wps_info_2g(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_wps_info(eid, wp, argc, argv, 0);
}

/* Dump NAT table <tr><td>destination</td><td>MAC</td><td>IP</td><td>expires</td></tr> format */
int
ej_nat_table(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret = 0;
#ifdef REMOVE
	int needlen = 0, listlen, i
	netconf_nat_t *nat_list = 0;
	netconf_nat_t **plist, *cur;
	char line[256], tstr[32];
#endif
	ret += websWrite(wp, "Destination     Proto.  Port Range  Redirect to\n");

	// find another way to show iptable
#ifdef REMOVE
    	netconf_get_nat(NULL, &needlen);

    	if (needlen > 0) 
	{

		nat_list = (netconf_nat_t *) malloc(needlen);
		if (nat_list) {
	    		memset(nat_list, 0, needlen);
	    		listlen = needlen;
	    		if (netconf_get_nat(nat_list, &listlen) == 0 && needlen == listlen) {
				listlen = needlen/sizeof(netconf_nat_t);

				for(i=0;i<listlen;i++)
				{				
				//printf("%d %d %d\n", nat_list[i].target,
				//		nat_list[i].match.ipproto,
				//		nat_list[i].match.dst.ipaddr.s_addr);	
				if (nat_list[i].target==NETCONF_DNAT)
				{
					if (nat_list[i].match.dst.ipaddr.s_addr==0)
					{
						sprintf(line, "%-15s", "all");
					}
					else
					{
						sprintf(line, "%-15s", inet_ntoa(nat_list[i].match.dst.ipaddr));
					}


					if (ntohs(nat_list[i].match.dst.ports[0])==0)	
						sprintf(line, "%s %-7s", line, "ALL");
					else if (nat_list[i].match.ipproto==IPPROTO_TCP)
						sprintf(line, "%s %-7s", line, "TCP");
					else sprintf(line, "%s %-7s", line, "UDP");

					if (nat_list[i].match.dst.ports[0] == nat_list[i].match.dst.ports[1])
					{
						if (ntohs(nat_list[i].match.dst.ports[0])==0)	
						sprintf(line, "%s %-11s", line, "ALL");
						else
						sprintf(line, "%s %-11d", line, ntohs(nat_list[i].match.dst.ports[0]));
					}
					else 
					{
						sprintf(tstr, "%d:%d", ntohs(nat_list[i].match.dst.ports[0]),
ntohs(nat_list[i].match.dst.ports[1]));
						sprintf(line, "%s %-11s", line, tstr);					
					}	
					sprintf(line, "%s %s\n", line, inet_ntoa(nat_list[i].ipaddr));
					ret += websWrite(wp, line);
				
				}
				}
	    		}
	    		free(nat_list);
		}
    	}
#endif
	return ret;
}


static int wpa_key_mgmt_to_bitfield(const unsigned char *s)
{
	if (memcmp(s, WPA_AUTH_KEY_MGMT_UNSPEC_802_1X, WPA_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_IEEE8021X_;
	if (memcmp(s, WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X, WPA_SELECTOR_LEN) ==
	    0)
		return WPA_KEY_MGMT_PSK_;
	if (memcmp(s, WPA_AUTH_KEY_MGMT_NONE, WPA_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_WPA_NONE_;
	return 0;
}

static int rsn_key_mgmt_to_bitfield(const unsigned char *s)
{
	if (memcmp(s, RSN_AUTH_KEY_MGMT_UNSPEC_802_1X, RSN_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_IEEE8021X2_;
	if (memcmp(s, RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X, RSN_SELECTOR_LEN) ==
	    0)
		return WPA_KEY_MGMT_PSK2_;
	return 0;
}

static int wpa_selector_to_bitfield(const unsigned char *s)
{
	if (memcmp(s, WPA_CIPHER_SUITE_NONE, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_NONE_;
	if (memcmp(s, WPA_CIPHER_SUITE_WEP40, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP40_;
	if (memcmp(s, WPA_CIPHER_SUITE_TKIP, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_TKIP_;
	if (memcmp(s, WPA_CIPHER_SUITE_CCMP, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_CCMP_;
	if (memcmp(s, WPA_CIPHER_SUITE_WEP104, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP104_;
	return 0;
}

static int rsn_selector_to_bitfield(const unsigned char *s)
{
	if (memcmp(s, RSN_CIPHER_SUITE_NONE, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_NONE_;
	if (memcmp(s, RSN_CIPHER_SUITE_WEP40, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP40_;
	if (memcmp(s, RSN_CIPHER_SUITE_TKIP, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_TKIP_;
	if (memcmp(s, RSN_CIPHER_SUITE_CCMP, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_CCMP_;
	if (memcmp(s, RSN_CIPHER_SUITE_WEP104, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP104_;
	return 0;
}

static int wpa_parse_wpa_ie_wpa(const unsigned char *wpa_ie, size_t wpa_ie_len,
				struct wpa_ie_data *data)
{
	const struct wpa_ie_hdr *hdr;
	const unsigned char *pos;
	int left;
	int i, count;

	data->proto = WPA_PROTO_WPA_;
	data->pairwise_cipher = WPA_CIPHER_TKIP_;
	data->group_cipher = WPA_CIPHER_TKIP_;
	data->key_mgmt = WPA_KEY_MGMT_IEEE8021X_;
	data->capabilities = 0;
	data->pmkid = NULL;
	data->num_pmkid = 0;

	if (wpa_ie_len == 0) {
		/* No WPA IE - fail silently */
		return -1;
	}

	if (wpa_ie_len < sizeof(struct wpa_ie_hdr)) {
//		fprintf(stderr, "ie len too short %lu", (unsigned long) wpa_ie_len);
		return -1;
	}

	hdr = (const struct wpa_ie_hdr *) wpa_ie;

	if (hdr->elem_id != DOT11_MNG_WPA_ID ||
	    hdr->len != wpa_ie_len - 2 ||
	    memcmp(&hdr->oui, WPA_OUI_TYPE_ARR, WPA_SELECTOR_LEN) != 0 ||
	    WPA_GET_LE16(hdr->version) != WPA_VERSION_) {
//		fprintf(stderr, "malformed ie or unknown version");
		return -1;
	}

	pos = (const unsigned char *) (hdr + 1);
	left = wpa_ie_len - sizeof(*hdr);

	if (left >= WPA_SELECTOR_LEN) {
		data->group_cipher = wpa_selector_to_bitfield(pos);
		pos += WPA_SELECTOR_LEN;
		left -= WPA_SELECTOR_LEN;
	} else if (left > 0) {
//		fprintf(stderr, "ie length mismatch, %u too much", left);
		return -1;
	}

	if (left >= 2) {
		data->pairwise_cipher = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * WPA_SELECTOR_LEN) {
//			fprintf(stderr, "ie count botch (pairwise), "
//				   "count %u left %u", count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->pairwise_cipher |= wpa_selector_to_bitfield(pos);
			pos += WPA_SELECTOR_LEN;
			left -= WPA_SELECTOR_LEN;
		}
	} else if (left == 1) {
//		fprintf(stderr, "ie too short (for key mgmt)");
		return -1;
	}

	if (left >= 2) {
		data->key_mgmt = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * WPA_SELECTOR_LEN) {
//			fprintf(stderr, "ie count botch (key mgmt), "
//				   "count %u left %u", count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->key_mgmt |= wpa_key_mgmt_to_bitfield(pos);
			pos += WPA_SELECTOR_LEN;
			left -= WPA_SELECTOR_LEN;
		}
	} else if (left == 1) {
//		fprintf(stderr, "ie too short (for capabilities)");
		return -1;
	}

	if (left >= 2) {
		data->capabilities = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
	}

	if (left > 0) {
//		fprintf(stderr, "ie has %u trailing bytes", left);
		return -1;
	}

	return 0;
}

static int wpa_parse_wpa_ie_rsn(const unsigned char *rsn_ie, size_t rsn_ie_len,
				struct wpa_ie_data *data)
{
	const struct rsn_ie_hdr *hdr;
	const unsigned char *pos;
	int left;
	int i, count;

	data->proto = WPA_PROTO_RSN_;
	data->pairwise_cipher = WPA_CIPHER_CCMP_;
	data->group_cipher = WPA_CIPHER_CCMP_;
	data->key_mgmt = WPA_KEY_MGMT_IEEE8021X2_;
	data->capabilities = 0;
	data->pmkid = NULL;
	data->num_pmkid = 0;

	if (rsn_ie_len == 0) {
		/* No RSN IE - fail silently */
		return -1;
	}

	if (rsn_ie_len < sizeof(struct rsn_ie_hdr)) {
//		fprintf(stderr, "ie len too short %lu", (unsigned long) rsn_ie_len);
		return -1;
	}

	hdr = (const struct rsn_ie_hdr *) rsn_ie;

	if (hdr->elem_id != DOT11_MNG_RSN_ID ||
	    hdr->len != rsn_ie_len - 2 ||
	    WPA_GET_LE16(hdr->version) != RSN_VERSION_) {
//		fprintf(stderr, "malformed ie or unknown version");
		return -1;
	}

	pos = (const unsigned char *) (hdr + 1);
	left = rsn_ie_len - sizeof(*hdr);

	if (left >= RSN_SELECTOR_LEN) {
		data->group_cipher = rsn_selector_to_bitfield(pos);
		pos += RSN_SELECTOR_LEN;
		left -= RSN_SELECTOR_LEN;
	} else if (left > 0) {
//		fprintf(stderr, "ie length mismatch, %u too much", left);
		return -1;
	}

	if (left >= 2) {
		data->pairwise_cipher = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * RSN_SELECTOR_LEN) {
//			fprintf(stderr, "ie count botch (pairwise), "
//				   "count %u left %u", count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->pairwise_cipher |= rsn_selector_to_bitfield(pos);
			pos += RSN_SELECTOR_LEN;
			left -= RSN_SELECTOR_LEN;
		}
	} else if (left == 1) {
//		fprintf(stderr, "ie too short (for key mgmt)");
		return -1;
	}

	if (left >= 2) {
		data->key_mgmt = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * RSN_SELECTOR_LEN) {
//			fprintf(stderr, "ie count botch (key mgmt), "
//				   "count %u left %u", count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->key_mgmt |= rsn_key_mgmt_to_bitfield(pos);
			pos += RSN_SELECTOR_LEN;
			left -= RSN_SELECTOR_LEN;
		}
	} else if (left == 1) {
//		fprintf(stderr, "ie too short (for capabilities)");
		return -1;
	}

	if (left >= 2) {
		data->capabilities = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
	}

	if (left >= 2) {
		data->num_pmkid = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (left < data->num_pmkid * PMKID_LEN) {
//			fprintf(stderr, "PMKID underflow "
//				   "(num_pmkid=%d left=%d)", data->num_pmkid, left);
			data->num_pmkid = 0;
		} else {
			data->pmkid = pos;
			pos += data->num_pmkid * PMKID_LEN;
			left -= data->num_pmkid * PMKID_LEN;
		}
	}

	if (left > 0) {
//		fprintf(stderr, "ie has %u trailing bytes - ignored", left);
	}

	return 0;
}

int wpa_parse_wpa_ie(const unsigned char *wpa_ie, size_t wpa_ie_len,
		     struct wpa_ie_data *data)
{
	if (wpa_ie_len >= 1 && wpa_ie[0] == DOT11_MNG_RSN_ID)
		return wpa_parse_wpa_ie_rsn(wpa_ie, wpa_ie_len, data);
	else
		return wpa_parse_wpa_ie_wpa(wpa_ie, wpa_ie_len, data);
}

static const char * wpa_cipher_txt(int cipher)
{
	switch (cipher) {
	case WPA_CIPHER_NONE_:
		return "NONE";
	case WPA_CIPHER_WEP40_:
		return "WEP-40";
	case WPA_CIPHER_WEP104_:
		return "WEP-104";
	case WPA_CIPHER_TKIP_:
		return "TKIP";
	case WPA_CIPHER_CCMP_:
//		return "CCMP";
		return "AES";
	case (WPA_CIPHER_TKIP_|WPA_CIPHER_CCMP_):
		return "TKIP+AES";
	default:
		return "Unknown";
	}
}

static const char * wpa_key_mgmt_txt(int key_mgmt, int proto)
{
	switch (key_mgmt) {
	case WPA_KEY_MGMT_IEEE8021X_:
/*
		return proto == WPA_PROTO_RSN_ ?
			"WPA2/IEEE 802.1X/EAP" : "WPA/IEEE 802.1X/EAP";
*/
		return "WPA-Enterprise";
	case WPA_KEY_MGMT_IEEE8021X2_:
		return "WPA2-Enterprise";
	case WPA_KEY_MGMT_PSK_:
/*
		return proto == WPA_PROTO_RSN_ ?
			"WPA2-PSK" : "WPA-PSK";
*/
		return "WPA-Personal";
	case WPA_KEY_MGMT_PSK2_:
		return "WPA2-Personal";
	case WPA_KEY_MGMT_NONE_:
		return "NONE";
	case WPA_KEY_MGMT_IEEE8021X_NO_WPA_:
//		return "IEEE 802.1X (no WPA)";
		return "IEEE 802.1X";
	default:
		return "Unknown";
	}
}

int
ej_SiteSurvey(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret, i, k, left, ht_extcha;
	int retval = 0, ap_count = 0, idx_same = -1, count = 0;
	unsigned char *bssidp;
	char *info_b;
	unsigned char rate;
	unsigned char bssid[6];
	char macstr[18];
	char ure_mac[18];
	char ssid_str[256];
	wl_scan_results_t *result;
	wl_bss_info_t *info;
	struct bss_ie_hdr *ie;
	NDIS_802_11_NETWORK_TYPE NetWorkType;
	struct maclist *authorized;
	int maclist_size;
	int max_sta_count = 128;
	int wl_authorized = 0;
	wl_scan_params_t *params;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + NUMCHANS * sizeof(uint16);

#ifdef RTN12
	if (nvram_invmatch("sw_mode_ex", "2"))
	{
		retval += websWrite(wp, "[");
		retval += websWrite(wp, "];");
		return retval;
	}
#endif

	params = (wl_scan_params_t*)malloc(params_size);
	if (params == NULL)
		return retval;

	memset(params, 0, params_size);
	params->bss_type = DOT11_BSSTYPE_INFRASTRUCTURE;
	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
//	params->scan_type = -1;
	params->scan_type = DOT11_SCANTYPE_ACTIVE;
//	params->scan_type = DOT11_SCANTYPE_PASSIVE;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = -1;
	params->home_time = -1;
	params->channel_num = 0;

	while ((ret = wl_ioctl(WIF, WLC_SCAN, params, params_size)) < 0 && count++ < 2)
	{
		fprintf(stderr, "set scan command failed, retry %d\n", count);
		sleep(1);
	}

	free(params);

	nvram_set("ap_selecting", "1");
	fprintf(stderr, "Please wait (web hook) ");
	fprintf(stderr, ".");
	sleep(1);
	fprintf(stderr, ".");
	sleep(1);
	fprintf(stderr, ".");
	sleep(1);
	fprintf(stderr, ".\n\n");
	sleep(1);
	nvram_set("ap_selecting", "0");

	if (ret == 0)
	{
		count = 0;

		result = (wl_scan_results_t *)buf;
		result->buflen = WLC_IOCTL_MAXLEN - sizeof(result);

		while ((ret = wl_ioctl(WIF, WLC_SCAN_RESULTS, result, WLC_IOCTL_MAXLEN)) < 0 && count++ < 2)
		{
			fprintf(stderr, "set scan results command failed, retry %d\n", count);
			sleep(1);
		}

		if (ret == 0)
		{
			info = &(result->bss_info[0]);
			info_b = (unsigned char *)info;
			
			for(i = 0; i < result->count; i++)
			{
				if (info->SSID_len > 32/* || info->SSID_len == 0*/)
					goto next_info;
#if 0
				SSID_valid = 1;
				for(j = 0; j < info->SSID_len; j++)
				{
					if(info->SSID[j] < 32 || info->SSID[j] > 126)
					{
						SSID_valid = 0;
						break;
					}
				}
				if(!SSID_valid)
					goto next_info;
#endif
				bssidp = (unsigned char *)&info->BSSID;
				sprintf(macstr, "%02X:%02X:%02X:%02X:%02X:%02X", (unsigned char)bssidp[0],
										 (unsigned char)bssidp[1],
										 (unsigned char)bssidp[2],
										 (unsigned char)bssidp[3],
										 (unsigned char)bssidp[4],
										 (unsigned char)bssidp[5]);

				idx_same = -1;
				for (k = 0; k < ap_count; k++)	// deal with old version of Broadcom Multiple SSID (share the same BSSID)
				{
					if(strcmp(apinfos[k].BSSID, macstr) == 0 && strcmp(apinfos[k].SSID, info->SSID) == 0)
					{
						idx_same = k;
						break;
					}
				}

				if (idx_same != -1)
				{
					if (info->RSSI >= -50)
						apinfos[idx_same].RSSI_Quality = 100;
					else if (info->RSSI >= -80)	// between -50 ~ -80dbm
						apinfos[idx_same].RSSI_Quality = (int)(24 + ((info->RSSI + 80) * 26)/10);
					else if (info->RSSI >= -90)	// between -80 ~ -90dbm
						apinfos[idx_same].RSSI_Quality = (int)(((info->RSSI + 90) * 26)/10);
					else					// < -84 dbm
						apinfos[idx_same].RSSI_Quality = 0;
				}
				else
				{
					strcpy(apinfos[ap_count].BSSID, macstr);
//					strcpy(apinfos[ap_count].SSID, info->SSID);
					memset(apinfos[ap_count].SSID, 0x0, 33);
					memcpy(apinfos[ap_count].SSID, info->SSID, info->SSID_len);
//					apinfos[ap_count].channel = ((wl_bss_info_107_t *) info)->channel;
					apinfos[ap_count].channel = info->chanspec;
					apinfos[ap_count].ctl_ch = info->ctl_ch;

					if (info->RSSI >= -50)
						apinfos[ap_count].RSSI_Quality = 100;
					else if (info->RSSI >= -80)	// between -50 ~ -80dbm
						apinfos[ap_count].RSSI_Quality = (int)(24 + ((info->RSSI + 80) * 26)/10);
					else if (info->RSSI >= -90)	// between -80 ~ -90dbm
						apinfos[ap_count].RSSI_Quality = (int)(((info->RSSI + 90) * 26)/10);
					else					// < -84 dbm
						apinfos[ap_count].RSSI_Quality = 0;

					if ((info->capability & 0x10) == 0x10)
						apinfos[ap_count].wep = 1;
					else
						apinfos[ap_count].wep = 0;
					apinfos[ap_count].wpa = 0;

/*
					unsigned char *RATESET = &info->rateset;
					for (k = 0; k < 18; k++)
						fprintf(stderr, "%02x ", (unsigned char)RATESET[k]);
					fprintf(stderr, "\n");
*/

					NetWorkType = Ndis802_11DS;
//					if (((wl_bss_info_107_t *) info)->channel <= 14)
					if ((uint8)info->chanspec <= 14)
					{
						for (k = 0; k < info->rateset.count; k++)
						{
							rate = info->rateset.rates[k] & 0x7f;	// Mask out basic rate set bit
							if ((rate == 2) || (rate == 4) || (rate == 11) || (rate == 22))
								continue;
							else
							{
								NetWorkType = Ndis802_11OFDM24;
								break;
							}	
						}
					}
					else
						NetWorkType = Ndis802_11OFDM5;

					if (info->n_cap)
					{
						if (NetWorkType == Ndis802_11OFDM5)
							NetWorkType = Ndis802_11OFDM5_N;
						else
							NetWorkType = Ndis802_11OFDM24_N;
					}

					apinfos[ap_count].NetworkType = NetWorkType;

					ap_count++;
				}

				ie = (struct bss_ie_hdr *) ((unsigned char *) info + sizeof(*info));
				for (left = info->ie_length; left > 0; // look for RSN IE first
					left -= (ie->len + 2), ie = (struct bss_ie_hdr *) ((unsigned char *) ie + 2 + ie->len)) 
				{
					if (ie->elem_id != DOT11_MNG_RSN_ID)
						continue;

					if (wpa_parse_wpa_ie(&ie->elem_id, ie->len + 2, &apinfos[ap_count - 1].wid) == 0)
					{
						apinfos[ap_count-1].wpa = 1;
						goto next_info;
					}
				}

				ie = (struct bss_ie_hdr *) ((unsigned char *) info + sizeof(*info));
				for (left = info->ie_length; left > 0; // then look for WPA IE
					left -= (ie->len + 2), ie = (struct bss_ie_hdr *) ((unsigned char *) ie + 2 + ie->len)) 
				{
					if (ie->elem_id != DOT11_MNG_WPA_ID)
						continue;

					if (wpa_parse_wpa_ie(&ie->elem_id, ie->len + 2, &apinfos[ap_count-1].wid) == 0)
					{
						apinfos[ap_count-1].wpa = 1;
						break;
					}
				}

next_info:
				info = (wl_bss_info_t *) ((unsigned char *) info + info->length);
			}
		}
	}

	if (ap_count == 0)
	{
		fprintf(stderr, "No AP found!\n");
	}
	else
	{
		fprintf(stderr, "%-4s%-3s%-33s%-18s%-9s%-16s%-9s%8s%3s%3s\n",
				"idx", "CH", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode", "CC", "EC");
		for (k = 0; k < ap_count; k++)
		{
			fprintf(stderr, "%2d. ", k + 1);
			fprintf(stderr, "%2d ", apinfos[k].channel);
			fprintf(stderr, "%-33s", apinfos[k].SSID);
			fprintf(stderr, "%-18s", apinfos[k].BSSID);

			if (apinfos[k].wpa == 1)
				fprintf(stderr, "%-9s%-16s", wpa_cipher_txt(apinfos[k].wid.pairwise_cipher), wpa_key_mgmt_txt(apinfos[k].wid.key_mgmt, apinfos[k].wid.proto));
			else if (apinfos[k].wep == 1)
				fprintf(stderr, "WEP      Unknown         ");
			else
				fprintf(stderr, "NONE     Open System     ");
			fprintf(stderr, "%9d ", apinfos[k].RSSI_Quality);

			if (apinfos[k].NetworkType == Ndis802_11FH || apinfos[k].NetworkType == Ndis802_11DS)
				fprintf(stderr, "%-7s", "11b");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM5)
				fprintf(stderr, "%-7s", "11a");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM5_N)
				fprintf(stderr, "%-7s", "11a/n");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM24)
				fprintf(stderr, "%-7s", "11b/g");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM24_N)
				fprintf(stderr, "%-7s", "11b/g/n");
			else
				fprintf(stderr, "%-7s", "unknown");

			fprintf(stderr, "%3d", apinfos[k].ctl_ch);

			if (	((apinfos[k].NetworkType == Ndis802_11OFDM5_N) || (apinfos[k].NetworkType == Ndis802_11OFDM24_N)) &&
				(apinfos[k].channel != apinfos[k].ctl_ch)	)
			{
				if (apinfos[k].ctl_ch < apinfos[k].channel)
					ht_extcha = 1;
				else
					ht_extcha = 0;

				fprintf(stderr, "%3d", ht_extcha);
			}

			fprintf(stderr, "\n");
		}
	}

	ret = wl_ioctl(WIF, WLC_GET_BSSID, bssid, sizeof(bssid));
	memset(ure_mac, 0x0, 18);
	if (!ret)
	{
		if ( !(!bssid[0] && !bssid[1] && !bssid[2] && !bssid[3] && !bssid[4] && !bssid[5]) )
		{
			sprintf(ure_mac, "%02X:%02X:%02X:%02X:%02X:%02X", (unsigned char)bssid[0],
									    (unsigned char)bssid[1],
									    (unsigned char)bssid[2],
									    (unsigned char)bssid[3],
									    (unsigned char)bssid[4],
									    (unsigned char)bssid[5]);
		}
	}

	if (strstr(nvram_safe_get("wl0_akm"), "psk"))
	{
		maclist_size = sizeof(authorized->count) + max_sta_count * sizeof(struct ether_addr);
		authorized = malloc(maclist_size);

		// query wl for authorized sta list
		strcpy((char*)authorized, "autho_sta_list");
		if (!wl_ioctl(WIF, WLC_GET_VAR, authorized, maclist_size))
		{
			if (authorized->count > 0) wl_authorized = 1;
		}

		if (authorized) free(authorized);
	}

	retval += websWrite(wp, "[");
	if (ap_count > 0)
	for (i = 0; i < ap_count; i++)
	{
		retval += websWrite(wp, "[");

		if (strlen(apinfos[i].SSID) == 0)
			retval += websWrite(wp, "\"\", ");
		else
		{
			memset(ssid_str, 0, sizeof(ssid_str));
			char_to_ascii(ssid_str, apinfos[i].SSID);
			retval += websWrite(wp, "\"%s\", ", ssid_str);
		}

		retval += websWrite(wp, "\"%d\", ", apinfos[i].channel);

		if (apinfos[i].wpa == 1)
		{
			if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_IEEE8021X_)
				retval += websWrite(wp, "\"%s\", ", "WPA");
			else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_IEEE8021X2_)
				retval += websWrite(wp, "\"%s\", ", "WPA2");
			else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_PSK_)
				retval += websWrite(wp, "\"%s\", ", "WPA-PSK");
			else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_PSK2_)
				retval += websWrite(wp, "\"%s\", ", "WPA2-PSK");
			else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_NONE_)
				retval += websWrite(wp, "\"%s\", ", "NONE");
			else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA_)
				retval += websWrite(wp, "\"%s\", ", "IEEE 802.1X");
			else
				retval += websWrite(wp, "\"%s\", ", "Unknown");
		}
		else if (apinfos[i].wep == 1)
			retval += websWrite(wp, "\"%s\", ", "Unknown");
		else
			retval += websWrite(wp, "\"%s\", ", "Open System");

		if (apinfos[i].wpa == 1)
		{
			if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_NONE_)
				retval += websWrite(wp, "\"%s\", ", "NONE");
			else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_WEP40_)
				retval += websWrite(wp, "\"%s\", ", "WEP");
			else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_WEP104_)
				retval += websWrite(wp, "\"%s\", ", "WEP");
			else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_TKIP_)
				retval += websWrite(wp, "\"%s\", ", "TKIP");
			else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_CCMP_)
				retval += websWrite(wp, "\"%s\", ", "AES");
			else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_TKIP_|WPA_CIPHER_CCMP_)
				retval += websWrite(wp, "\"%s\", ", "TKIP+AES");
			else
				retval += websWrite(wp, "\"%s\", ", "Unknown");
		}
		else if (apinfos[i].wep == 1)
			retval += websWrite(wp, "\"%s\", ", "WEP");
		else
			retval += websWrite(wp, "\"%s\", ", "NONE");

		retval += websWrite(wp, "\"%d\", ", apinfos[i].RSSI_Quality);
		retval += websWrite(wp, "\"%s\", ", apinfos[i].BSSID);
		retval += websWrite(wp, "\"%s\", ", "In");

		if (apinfos[i].NetworkType == Ndis802_11FH || apinfos[i].NetworkType == Ndis802_11DS)
			retval += websWrite(wp, "\"%s\", ", "b");
		else if (apinfos[i].NetworkType == Ndis802_11OFDM5)
			retval += websWrite(wp, "\"%s\", ", "a");
		else if (apinfos[i].NetworkType == Ndis802_11OFDM5_N)
			retval += websWrite(wp, "\"%s\", ", "an");
		else if (apinfos[i].NetworkType == Ndis802_11OFDM24)
			retval += websWrite(wp, "\"%s\", ", "bg");
		else if (apinfos[i].NetworkType == Ndis802_11OFDM24_N)
			retval += websWrite(wp, "\"%s\", ", "bgn");
		else
			retval += websWrite(wp, "\"%s\", ", "");

		if (nvram_invmatch("wl0_ssid", "") && strcmp(nvram_safe_get("wl0_ssid"), apinfos[i].SSID))
		{
			if (strcmp(apinfos[i].SSID, ""))
				retval += websWrite(wp, "\"%s\"", "0");				// none
			else if (!strcmp(ure_mac, apinfos[i].BSSID))
			{									// hidden AP (null SSID)
				if (strstr(nvram_safe_get("wl0_akm"), "psk"))
				{
					if (wl_authorized)
						retval += websWrite(wp, "\"%s\"", "4");		// in profile, connected
					else
						retval += websWrite(wp, "\"%s\"", "5");		// in profile, connecting
				}
				else
					retval += websWrite(wp, "\"%s\"", "4");			// in profile, connected
			}
			else									// hidden AP (null SSID)
				retval += websWrite(wp, "\"%s\"", "0");				// none
		}
		else if (nvram_invmatch("wl0_ssid", "") && !strcmp(nvram_safe_get("wl0_ssid"), apinfos[i].SSID))
		{
			if (!strlen(ure_mac))
				retval += websWrite(wp, "\"%s\", ", "1");			// in profile, disconnected
			else if (!strcmp(ure_mac, apinfos[i].BSSID))
			{
				if (strstr(nvram_safe_get("wl0_akm"), "psk"))
				{
					if (wl_authorized)
						retval += websWrite(wp, "\"%s\"", "2");		// in profile, connected
					else
						retval += websWrite(wp, "\"%s\"", "3");		// in profile, connecting
				}
				else
					retval += websWrite(wp, "\"%s\"", "2");			// in profile, connected
			}
			else
				retval += websWrite(wp, "\"%s\"", "0");				// impossible...
		}
		else
			retval += websWrite(wp, "\"%s\"", "0");					// wl0_ssid == ""

		if (i == ap_count - 1)
			retval += websWrite(wp, "]\n");
		else
			retval += websWrite(wp, "],\n");
	}
	retval += websWrite(wp, "];");

	return retval;
}

int
ej_urelease(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;

	if (    nvram_match("sw_mode_ex", "2") &&
		nvram_invmatch("lan_ipaddr_new", "") &&
		nvram_invmatch("lan_netmask_new", "") &&
		nvram_invmatch("lan_gateway_new", ""))
	{
		retval += websWrite(wp, "[");
		retval += websWrite(wp, "\"%s\", ", nvram_safe_get("lan_ipaddr_new"));
		retval += websWrite(wp, "\"%s\", ", nvram_safe_get("lan_netmask_new"));
		retval += websWrite(wp, "\"%s\"", nvram_safe_get("lan_gateway_new"));
		retval += websWrite(wp, "];");

		kill_pidfile_s("/var/run/ure_monitor.pid", SIGUSR1);
	}
	else
	{
		retval += websWrite(wp, "[");
		retval += websWrite(wp, "\"\", ");
		retval += websWrite(wp, "\"\", ");
		retval += websWrite(wp, "\"\"");
		retval += websWrite(wp, "];");
	}

	return retval;
}


static bool find_ethaddr_in_list(void *ethaddr, struct maclist *list){
	int i;
	
	for(i = 0; i < list->count; ++i)
		if(!bcmp(ethaddr, (void *)&list->ea[i], ETHER_ADDR_LEN))
			return TRUE;
	
	return FALSE;
}


// no WME in WL500gP V2
// MAC/associated/authorized
int ej_wl_auth_list(int eid, webs_t wp, int argc, char_t **argv){
	int unit;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char *name;
	struct maclist *auth, *assoc, *authorized;
	int max_sta_count, maclist_size;
	int i, firstRow;
	
	if((unit = atoi(nvram_safe_get("wl_unit"))) < 0)
		return -1;
	
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
	
	/* buffers and length */
	max_sta_count = 256;
	maclist_size = sizeof(auth->count)+max_sta_count*sizeof(struct ether_addr);
	
	auth = malloc(maclist_size);
	assoc = malloc(maclist_size);
	authorized = malloc(maclist_size);
	//wme = malloc(maclist_size);
	
	//if(!auth || !assoc || !authorized || !wme)
	if(!auth || !assoc || !authorized)
		goto exit;
	
	/* query wl for authenticated sta list */
	strcpy((char*)auth, "authe_sta_list");
	if (wl_ioctl(name, WLC_GET_VAR, auth, maclist_size))
		goto exit;
	
	/* query wl for associated sta list */
	assoc->count = max_sta_count;
	if (wl_ioctl(name, WLC_GET_ASSOCLIST, assoc, maclist_size))
		goto exit;
	
	/* query wl for authorized sta list */
	strcpy((char*)authorized, "autho_sta_list");
	if (wl_ioctl(name, WLC_GET_VAR, authorized, maclist_size))
		goto exit;
	
	/* query wl for WME sta list */
	/*strcpy((char*)wme, "wme_sta_list");
	if (wl_ioctl(name, WLC_GET_VAR, wme, maclist_size))
		goto exit;*/
	
	/* build authenticated/associated/authorized sta list */
	firstRow = 1;
	for(i = 0; i < auth->count; ++i){
		char ea[ETHER_ADDR_STR_LEN];
		char *value;
		
		if(firstRow == 1)
			firstRow = 0;
		else
			websWrite(wp, ", ");
		websWrite(wp, "[");
		
		websWrite(wp, "\"%s\"", ether_etoa((void *)&auth->ea[i], ea));
		
		value = (find_ethaddr_in_list((void *)&auth->ea[i], assoc))?"Yes":"No";
		websWrite(wp, ", \"%s\"", value);
		
		value = (find_ethaddr_in_list((void *)&auth->ea[i], authorized))?"Yes":"No";
		websWrite(wp, ", \"%s\"", value);
		
		/*value = (find_ethaddr_in_list((void *)&auth->ea[i], wme))?"Yes":"No";
		websWrite(wp, ", \"%s\"", value);*/
		
		websWrite(wp, "]");
	}
	
	/* error/exit */
exit:
	if(auth) free(auth);
	if(assoc) free(assoc);
	if(authorized) free(authorized);
	//if(wme) free(wme);
	
	return 0;
}

/* WPS ENR mode APIs */
typedef struct wlc_ap_list_info
{
#if 0
	bool        used;
#endif
	uint8       ssid[33];
	uint8       ssidLen; 
	uint8       BSSID[6];
#if 0
	uint8       *ie_buf;
	uint32      ie_buflen;
#endif
	uint8       channel;
#if 0
	uint8       wep;
	bool	    scstate;
#endif
} wlc_ap_list_info_t;

#define WLC_DUMP_BUF_LEN		(32 * 1024)
#define WLC_MAX_AP_SCAN_LIST_LEN	50
#define WLC_SCAN_RETRY_TIMES		5

static wlc_ap_list_info_t ap_list[WLC_MAX_AP_SCAN_LIST_LEN];
static char scan_result[WLC_DUMP_BUF_LEN]; 

static char *
wl_get_scan_results(char *ifname)
{
	int ret, retry_times = 0;
	wl_scan_params_t *params;
	wl_scan_results_t *list = (wl_scan_results_t*)scan_result;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + NUMCHANS * sizeof(uint16);
	int org_scan_time = 20, scan_time = 40;

	params = (wl_scan_params_t*)malloc(params_size);
	if (params == NULL) {
		return NULL;
	}

	memset(params, 0, params_size);
	params->bss_type = DOT11_BSSTYPE_ANY;
	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->scan_type = -1;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = -1;
	params->home_time = -1;
	params->channel_num = 0;

	/* extend scan channel time to get more AP probe resp */
	wl_ioctl(ifname, WLC_GET_SCAN_CHANNEL_TIME, &org_scan_time, sizeof(org_scan_time));
	if (org_scan_time < scan_time)
		wl_ioctl(ifname, WLC_SET_SCAN_CHANNEL_TIME, &scan_time,	sizeof(scan_time));

retry:
	ret = wl_ioctl(ifname, WLC_SCAN, params, params_size);
	if (ret < 0) {
		if (retry_times++ < WLC_SCAN_RETRY_TIMES) {
			printf("set scan command failed, retry %d\n", retry_times);
			sleep(1);
			goto retry;
		}
	}

	sleep(2);

	list->buflen = WLC_DUMP_BUF_LEN;
	ret = wl_ioctl(ifname, WLC_SCAN_RESULTS, scan_result, WLC_DUMP_BUF_LEN);
	if (ret < 0 && retry_times++ < WLC_SCAN_RETRY_TIMES) {
		printf("get scan result failed, retry %d\n", retry_times);
		sleep(1);
		goto retry;
	}

	free(params);

	/* restore original scan channel time */
	wl_ioctl(ifname, WLC_SET_SCAN_CHANNEL_TIME, &org_scan_time, sizeof(org_scan_time));

	if (ret < 0)
		return NULL;

	return scan_result;
}

//static wlc_ap_list_info_t *
static int
wl_scan(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
	char *name = NULL;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	wl_scan_results_t *list = (wl_scan_results_t*)scan_result;
	wl_bss_info_t *bi;
	wl_bss_info_107_t *old_bi;
	uint i, ap_count = 0;
	unsigned char *bssidp;
	char ssid_str[128];
	char macstr[18];
	int retval = 0;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	if (wl_get_scan_results(name) == NULL)
		return NULL;

	memset(ap_list, 0, sizeof(ap_list));
	if (list->count == 0)
		return 0;
	else if (list->version != WL_BSS_INFO_VERSION &&
			list->version != LEGACY_WL_BSS_INFO_VERSION) {
		/* fprintf(stderr, "Sorry, your driver has bss_info_version %d "
		    "but this program supports only version %d.\n",
		    list->version, WL_BSS_INFO_VERSION); */
		return NULL;
	}

	bi = list->bss_info;
	for (i = 0; i < list->count; i++) {
        /* Convert version 107 to 108 */
		if (bi->version == LEGACY_WL_BSS_INFO_VERSION) {
			old_bi = (wl_bss_info_107_t *)bi;
			bi->chanspec = CH20MHZ_CHSPEC(old_bi->channel);
			bi->ie_length = old_bi->ie_length;
			bi->ie_offset = sizeof(wl_bss_info_107_t);
		}    
		if (bi->ie_length) {
			if (ap_count < WLC_MAX_AP_SCAN_LIST_LEN){
#if 0
				ap_list[ap_count].used = TRUE;
#endif
				memcpy(ap_list[ap_count].BSSID, (uint8 *)&bi->BSSID, 6);
				strncpy((char *)ap_list[ap_count].ssid, (char *)bi->SSID, bi->SSID_len);
				ap_list[ap_count].ssid[bi->SSID_len] = '\0';
#if 0
				ap_list[ap_count].ssidLen= bi->SSID_len;
				ap_list[ap_count].ie_buf = (uint8 *)(((uint8 *)bi) + bi->ie_offset);
				ap_list[ap_count].ie_buflen = bi->ie_length;
#endif
				ap_list[ap_count].channel = (uint8)(bi->chanspec & WL_CHANSPEC_CHAN_MASK);
#if 0
				ap_list[ap_count].wep = bi->capability & DOT11_CAP_PRIVACY;
#endif
				ap_count++;
			}
		}
		bi = (wl_bss_info_t*)((int8*)bi + bi->length);
	}

	sprintf(tmp, "%-4s%-33s%-18s\n", "Ch", "SSID", "BSSID");
	dbg("\n%s", tmp);
	if (ap_count)
	{
		retval += websWrite(wp, "[");
		for (i = 0; i < ap_count; i++)
		{
			memset(ssid_str, 0, sizeof(ssid_str));
			char_to_ascii(ssid_str, trim_r(ap_list[i].ssid));

			bssidp = (unsigned char *)&ap_list[i].BSSID;
			sprintf(macstr, "%02X:%02X:%02X:%02X:%02X:%02X",
				(unsigned char)bssidp[0],
				(unsigned char)bssidp[1],
				(unsigned char)bssidp[2],
				(unsigned char)bssidp[3],
				(unsigned char)bssidp[4],
				(unsigned char)bssidp[5]);

			dbg("%-4d%-33s%-18s\n",
				ap_list[i].channel,
				ap_list[i].ssid,
				macstr
			);

			if (!i)
				retval += websWrite(wp, "[\"%s\", \"%s\"]", ssid_str, macstr);
			else
				retval += websWrite(wp, ", [\"%s\", \"%s\"]", ssid_str, macstr);
		}
		retval += websWrite(wp, "]");
		dbg("\n");
	}
	else
		retval += websWrite(wp, "[]");

//	return ap_list;
	return retval;
}

int
ej_wl_scan(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_scan(eid, wp, argc, argv, 0);
}

int
ej_wl_scan_2g(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_scan(eid, wp, argc, argv, 0);
}

int
ej_wl_scan_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_scan(eid, wp, argc, argv, 1);
}
