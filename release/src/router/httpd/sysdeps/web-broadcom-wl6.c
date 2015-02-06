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
#include <proto/wps.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <shutils.h>
#include <wlutils.h>
#include <linux/types.h>
#include <shared.h>
#include <wlscan.h>
#include <sysinfo.h>

#ifdef RTCONFIG_QTN
#include "web-qtn.h"
#endif

enum {
	NOTHING,
	REBOOT,
	RESTART,
};

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

//static int ioctl_version = WLC_IOCTL_VERSION;

extern uint8 wf_chspec_ctlchan(chanspec_t chspec);

int
ej_wl_sta_status(int eid, webs_t wp, char *name)
{
	// TODO
	return 0;
}

#include <bcmendian.h>
#include <bcmparams.h>		/* for DEV_NUMIFS */

#define SSID_FMT_BUF_LEN 4*32+1	/* Length for SSID format string */
#define	MAX_STA_COUNT	128

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

/* Definitions for D11AC capable Chanspec type */

/* Chanspec ASCII representation with 802.11ac capability:
 * [<band> 'g'] <channel> ['/'<bandwidth> [<ctl-sideband>]['/'<1st80channel>'-'<2nd80channel>]]
 *
 * <band>:
 *      (optional) 2, 3, 4, 5 for 2.4GHz, 3GHz, 4GHz, and 5GHz respectively.
 *      Default value is 2g if channel <= 14, otherwise 5g.
 * <channel>:
 *      channel number of the 5MHz, 10MHz, 20MHz channel,
 *      or primary channel of 40MHz, 80MHz, 160MHz, or 80+80MHz channel.
 * <bandwidth>:
 *      (optional) 5, 10, 20, 40, 80, 160, or 80+80. Default value is 20.
 * <primary-sideband>:
 *      (only for 2.4GHz band 40MHz) U for upper sideband primary, L for lower.
 *
 *      For 2.4GHz band 40MHz channels, the same primary channel may be the
 *      upper sideband for one 40MHz channel, and the lower sideband for an
 *      overlapping 40MHz channel.  The U/L disambiguates which 40MHz channel
 *      is being specified.
 *
 *      For 40MHz in the 5GHz band and all channel bandwidths greater than
 *      40MHz, the U/L specificaion is not allowed since the channels are
 *      non-overlapping and the primary sub-band is derived from its
 *      position in the wide bandwidth channel.
 *
 * <1st80Channel>:
 * <2nd80Channel>:
 *      Required for 80+80, otherwise not allowed.
 *      Specifies the center channel of the first and second 80MHz band.
 *
 * In its simplest form, it is a 20MHz channel number, with the implied band
 * of 2.4GHz if channel number <= 14, and 5GHz otherwise.
 *
 * To allow for backward compatibility with scripts, the old form for
 * 40MHz channels is also allowed: <channel><ctl-sideband>
 *
 * <channel>:
 *      primary channel of 40MHz, channel <= 14 is 2GHz, otherwise 5GHz
 * <ctl-sideband>:
 *      "U" for upper, "L" for lower (or lower case "u" "l")
 *
 * 5 GHz Examples:
 *      Chanspec        BW        Center Ch  Channel Range  Primary Ch
 *      5g8             20MHz     8          -              -
 *      52              20MHz     52         -              -
 *      52/40           40MHz     54         52-56          52
 *      56/40           40MHz     54         52-56          56
 *      52/80           80MHz     58         52-64          52
 *      56/80           80MHz     58         52-64          56
 *      60/80           80MHz     58         52-64          60
 *      64/80           80MHz     58         52-64          64
 *      52/160          160MHz    50         36-64          52
 *      36/160          160MGz    50         36-64          36
 *      36/80+80/42-106 80+80MHz  42,106     36-48,100-112  36
 *
 * 2 GHz Examples:
 *      Chanspec        BW        Center Ch  Channel Range  Primary Ch
 *      2g8             20MHz     8          -              -
 *      8               20MHz     8          -              -
 *      6               20MHz     6          -              -
 *      6/40l           40MHz     8          6-10           6
 *      6l              40MHz     8          6-10           6
 *      6/40u           40MHz     4          2-6            6
 *      6u              40MHz     4          2-6            6
 */

/* bandwidth ASCII string */
static const char *wf_chspec_bw_str[] =
{
	"5",
	"10",
	"20",
	"40",
	"80",
	"160",
	"80+80",
	"na"
};

static const uint8 wf_chspec_bw_mhz[] =
{5, 10, 20, 40, 80, 160, 160};

#define WF_NUM_BW \
	(sizeof(wf_chspec_bw_mhz)/sizeof(uint8))

/* 40MHz channels in 5GHz band */
static const uint8 wf_5g_40m_chans[] =
{38, 46, 54, 62, 102, 110, 118, 126, 134, 142, 151, 159};
#define WF_NUM_5G_40M_CHANS \
	(sizeof(wf_5g_40m_chans)/sizeof(uint8))

/* 80MHz channels in 5GHz band */
static const uint8 wf_5g_80m_chans[] =
{42, 58, 106, 122, 138, 155};
#define WF_NUM_5G_80M_CHANS \
	(sizeof(wf_5g_80m_chans)/sizeof(uint8))

/* 160MHz channels in 5GHz band */
static const uint8 wf_5g_160m_chans[] =
{50, 114};
#define WF_NUM_5G_160M_CHANS \
	(sizeof(wf_5g_160m_chans)/sizeof(uint8))

/* convert bandwidth from chanspec to MHz */
static uint
bw_chspec_to_mhz(chanspec_t chspec)
{
	uint bw;

	bw = (chspec & WL_CHANSPEC_BW_MASK) >> WL_CHANSPEC_BW_SHIFT;
	return (bw >= WF_NUM_BW ? 0 : wf_chspec_bw_mhz[bw]);
}

/* bw in MHz, return the channel count from the center channel to the
 * the channel at the edge of the band
 */
static uint8
center_chan_to_edge(uint bw)
{
	/* edge channels separated by BW - 10MHz on each side
	 * delta from cf to edge is half of that,
	 * MHz to channel num conversion is 5MHz/channel
	 */
	return (uint8)(((bw - 20) / 2) / 5);
}

/* return channel number of the low edge of the band
 * given the center channel and BW
 */
static uint8
channel_low_edge(uint center_ch, uint bw)
{
	return (uint8)(center_ch - center_chan_to_edge(bw));
}

/* return control channel given center channel and side band */
static uint8
channel_to_ctl_chan(uint center_ch, uint bw, uint sb)
{
	return (uint8)(channel_low_edge(center_ch, bw) + sb * 4);
}

/*
 * Verify the chanspec is using a legal set of parameters, i.e. that the
 * chanspec specified a band, bw, ctl_sb and channel and that the
 * combination could be legal given any set of circumstances.
 * RETURNS: TRUE is the chanspec is malformed, false if it looks good.
 */
bool
wf_chspec_malformed(chanspec_t chanspec)
{
	uint chspec_bw = CHSPEC_BW(chanspec);
	uint chspec_ch = CHSPEC_CHANNEL(chanspec);

	/* must be 2G or 5G band */
	if (CHSPEC_IS2G(chanspec)) {
		/* must be valid bandwidth */
		if (chspec_bw != WL_CHANSPEC_BW_20 &&
		    chspec_bw != WL_CHANSPEC_BW_40) {
			return TRUE;
		}
	} else if (CHSPEC_IS5G(chanspec)) {
		if (chspec_bw == WL_CHANSPEC_BW_8080) {
			uint ch1_id, ch2_id;

			/* channel number in 80+80 must be in range */
			ch1_id = CHSPEC_CHAN1(chanspec);
			ch2_id = CHSPEC_CHAN2(chanspec);
			if (ch1_id >= WF_NUM_5G_80M_CHANS || ch2_id >= WF_NUM_5G_80M_CHANS)
				return TRUE;

			/* ch2 must be above ch1 for the chanspec */
			if (ch2_id <= ch1_id)
				return TRUE;
		} else if (chspec_bw == WL_CHANSPEC_BW_20 || chspec_bw == WL_CHANSPEC_BW_40 ||
			   chspec_bw == WL_CHANSPEC_BW_80 || chspec_bw == WL_CHANSPEC_BW_160) {

			if (chspec_ch > MAXCHANNEL) {
				return TRUE;
			}
		} else {
			/* invalid bandwidth */
			return TRUE;
		}
	} else {
		/* must be 2G or 5G band */
		return TRUE;
	}
	/* side band needs to be consistent with bandwidth */
	if (chspec_bw == WL_CHANSPEC_BW_20) {
		if (CHSPEC_CTL_SB(chanspec) != WL_CHANSPEC_CTL_SB_LLL)
			return TRUE;
	} else if (chspec_bw == WL_CHANSPEC_BW_40) {
		if (CHSPEC_CTL_SB(chanspec) > WL_CHANSPEC_CTL_SB_LLU)
			return TRUE;
	} else if (chspec_bw == WL_CHANSPEC_BW_80) {
		if (CHSPEC_CTL_SB(chanspec) > WL_CHANSPEC_CTL_SB_LUU)
			return TRUE;
	}

	return FALSE;
}

/*
 * This function returns the channel number that control traffic is being sent on, for 20MHz
 * channels this is just the channel number, for 40MHZ, 80MHz, 160MHz channels it is the 20MHZ
 * sideband depending on the chanspec selected
 */
uint8
wf_chspec_ctlchan(chanspec_t chspec)
{
	uint center_chan;
	uint bw_mhz;
	uint sb;

	if(wf_chspec_malformed(chspec))
		return 0;

	/* Is there a sideband ? */
	if (CHSPEC_IS20(chspec)) {
		return CHSPEC_CHANNEL(chspec);
	} else {
		sb = CHSPEC_CTL_SB(chspec) >> WL_CHANSPEC_CTL_SB_SHIFT;

		if (CHSPEC_IS8080(chspec)) {
			bw_mhz = 80;

			if (sb < 4) {
				center_chan = CHSPEC_CHAN1(chspec);
			}
			else {
				center_chan = CHSPEC_CHAN2(chspec);
				sb -= 4;
			}

			/* convert from channel index to channel number */
			center_chan = wf_5g_80m_chans[center_chan];
		}
		else {
			bw_mhz = bw_chspec_to_mhz(chspec);
			center_chan = CHSPEC_CHANNEL(chspec) >> WL_CHANSPEC_CHAN_SHIFT;
		}

		return (channel_to_ctl_chan(center_chan, bw_mhz, sb));
	}
}

/* given a chanspec and a string buffer, format the chanspec as a
 * string, and return the original pointer a.
 * Min buffer length must be CHANSPEC_STR_LEN.
 * On error return ""
 */
char *
wf_chspec_ntoa(chanspec_t chspec, char *buf)
{
	const char *band;
	uint ctl_chan;

	if (wf_chspec_malformed(chspec))
		return "";

	band = "";

	/* check for non-default band spec */
	if ((CHSPEC_IS2G(chspec) && CHSPEC_CHANNEL(chspec) > CH_MAX_2G_CHANNEL) ||
	    (CHSPEC_IS5G(chspec) && CHSPEC_CHANNEL(chspec) <= CH_MAX_2G_CHANNEL))
		band = (CHSPEC_IS2G(chspec)) ? "2g" : "5g";

	/* ctl channel */
	if(!(ctl_chan = wf_chspec_ctlchan(chspec)))
		return "";

	/* bandwidth and ctl sideband */
	if (CHSPEC_IS20(chspec)) {
		snprintf(buf, CHANSPEC_STR_LEN, "%s%d", band, ctl_chan);
	} else if (!CHSPEC_IS8080(chspec)) {
		const char *bw;
		const char *sb = "";

		bw = wf_chspec_bw_str[(chspec & WL_CHANSPEC_BW_MASK) >> WL_CHANSPEC_BW_SHIFT];

#ifdef CHANSPEC_NEW_40MHZ_FORMAT
		/* ctl sideband string if needed for 2g 40MHz */
		if (CHSPEC_IS40(chspec) && CHSPEC_IS2G(chspec)) {
			sb = CHSPEC_SB_UPPER(chspec) ? "u" : "l";
		}

		snprintf(buf, CHANSPEC_STR_LEN, "%s%d/%s%s", band, ctl_chan, bw, sb);
#else
		/* ctl sideband string instead of BW for 40MHz */
		if (CHSPEC_IS40(chspec)) {
			sb = CHSPEC_SB_UPPER(chspec) ? "u" : "l";
			snprintf(buf, CHANSPEC_STR_LEN, "%s%d%s", band, ctl_chan, sb);
		} else {
			snprintf(buf, CHANSPEC_STR_LEN, "%s%d/%s", band, ctl_chan, bw);
		}
#endif /* CHANSPEC_NEW_40MHZ_FORMAT */
	} else {
		/* 80+80 */
		uint chan1 = (chspec & WL_CHANSPEC_CHAN1_MASK) >> WL_CHANSPEC_CHAN1_SHIFT;
		uint chan2 = (chspec & WL_CHANSPEC_CHAN2_MASK) >> WL_CHANSPEC_CHAN2_SHIFT;

		/* convert to channel number */
		chan1 = (chan1 < WF_NUM_5G_80M_CHANS) ? wf_5g_80m_chans[chan1] : 0;
		chan2 = (chan2 < WF_NUM_5G_80M_CHANS) ? wf_5g_80m_chans[chan2] : 0;

		/* Outputs a max of CHANSPEC_STR_LEN chars including '\0'  */
		snprintf(buf, CHANSPEC_STR_LEN, "%d/80+80/%d-%d", ctl_chan, chan1, chan2);
	}

	return (buf);
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
bcm_wps_version(webs_t wp, uint8 *wps_ie)
{
	uint16 wps_len;
	uint16 wps_off, wps_suboff;
	uint16 wps_key;
	uint8 wps_field_len;
	int retval = 0;

	wps_len = (uint16)*(wps_ie+TLV_LEN_OFF);/* Get the length of the WPS OUI header */
	wps_off = WPS_OUI_FIXED_HEADER_OFF;	/* Skip the fixed headers */
	wps_field_len = 1;

	/* Parsing the OUI header looking for version number */
	while ((wps_len >= wps_off + 2) && (wps_field_len))
	{
		wps_key = (((uint8)wps_ie[wps_off]*256) + (uint8)wps_ie[wps_off+1]);
		if (wps_key == WPS_ID_VENDOR_EXT) {
			/* Key found */
			wps_suboff = wps_off + WPS_OUI_HEADER_SIZE;

			/* Looking for the Vendor extension code 0x00 0x37 0x2A
			 * and the Version 2 sudId 0x00
			 * if found then the next byte is the len of field which is always 1
			 * for version field the byte after is the version number
			 */
			if (!wlu_bcmp(&wps_ie[wps_suboff],  WFA_VENDOR_EXT_ID, WPS_OUI_LEN)&&
				(wps_ie[wps_suboff+WPS_WFA_SUBID_V2_OFF] == WPS_WFA_SUBID_VERSION2))
			{
				retval += websWrite(wp, "V%d.%d ", (wps_ie[wps_suboff+WPS_WFA_V2_OFF]>>4),
				(wps_ie[wps_suboff+WPS_WFA_V2_OFF] & 0x0f));
				return retval;
			}
		}
		/* Jump to next field */
		wps_field_len = wps_ie[wps_off+WPS_OUI_HEADER_LEN+1];
		wps_off += WPS_OUI_HEADER_SIZE + wps_field_len;
	}

	/* If nothing found from the parser then this is the WPS version 1.0 */
	retval += websWrite(wp, "V1.0 ");

	return retval;
}

static int
bcm_is_wps_configured(webs_t wp, uint8 *wps_ie)
{
	uint16 wps_key;
	int retval = 0;

	wps_key = (wps_ie[WPS_SCSTATE_OFF]*256) + wps_ie[WPS_SCSTATE_OFF+1];
	if ((wps_ie[TLV_LEN_OFF] > (WPS_SCSTATE_OFF+5))&&
		(wps_key == WPS_ID_SC_STATE))
	{
		switch (wps_ie[WPS_SCSTATE_OFF+WPS_OUI_HEADER_SIZE])
		{
			case WPS_SCSTATE_UNCONFIGURED:
				retval += websWrite(wp, "Unconfigured\n");
				break;
			case WPS_SCSTATE_CONFIGURED:
				retval += websWrite(wp, "Configured\n");
				break;
			default:
				retval += websWrite(wp, "Unknown State\n");
		}
	}
	return retval;
}

/* Looking for WPS OUI in the propriatary_ie */
static bool
bcm_is_wps_ie(uint8 *ie, uint8 **tlvs, uint32 *tlvs_len)
{
	bool retval = FALSE;
	/* If the contents match the WPS_OUI and type=4 */
	if ((ie[TLV_LEN_OFF] > (WPS_OUI_LEN+1)) &&
		!wlu_bcmp(&ie[TLV_BODY_OFF], WPS_OUI "\x04", WPS_OUI_LEN + 1)) {
		retval = TRUE;
	}

	/* point to the next ie */
	ie += ie[TLV_LEN_OFF] + TLV_HDR_LEN;
	/* calculate the length of the rest of the buffer */
	*tlvs_len -= (int)(ie - *tlvs);
	/* update the pointer to the start of the buffer */
	*tlvs = ie;

	return retval;
}

static int
wl_dump_wps(webs_t wp, uint8* cp, uint len)
{
	uint8 *parse = cp;
	uint32 parse_len = len;
	uint8 *proprietary_ie;
	int retval = 0;

	while ((proprietary_ie = wlu_parse_tlvs(parse, parse_len, DOT11_MNG_WPA_ID))) {
		if (bcm_is_wps_ie(proprietary_ie, &parse, &parse_len)) {
			/* Print WPS status */
			retval += websWrite(wp, "WPS: ");
			/* Print the version get from vendor extension field */
			retval += bcm_wps_version(wp, proprietary_ie);
			/* Print the WPS configure or Unconfigure option */
			retval += bcm_is_wps_configured(wp, proprietary_ie);
			break;
		}
	}
	return retval;
}

static chanspec_t
wl_chspec_from_driver(chanspec_t chanspec)
{
	chanspec = dtohchanspec(chanspec);
	/*
	if (ioctl_version == 1) {
		chanspec = wl_chspec_from_legacy(chanspec);
	}
	*/
	return chanspec;
}

static int
dump_bss_info(int eid, webs_t wp, int argc, char_t **argv, wl_bss_info_t *bi)
{
	char ssidbuf[SSID_FMT_BUF_LEN];
	char chspec_str[CHANSPEC_STR_LEN];
	wl_bss_info_107_t *old_bi;
#ifndef RTCONFIG_QTN
	int mcs_idx = 0;
#endif
	int retval = 0;

	/* Convert version 107 to 109 */
	if (dtoh32(bi->version) == LEGACY_WL_BSS_INFO_VERSION) {
		old_bi = (wl_bss_info_107_t *)bi;
		bi->chanspec = CH20MHZ_CHSPEC(old_bi->channel);
		bi->ie_length = old_bi->ie_length;
		bi->ie_offset = sizeof(wl_bss_info_107_t);
	} else {
		/* do endian swap and format conversion for chanspec if we have
		* not created it from legacy bi above
		*/
		bi->chanspec = wl_chspec_from_driver(bi->chanspec);
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

#ifndef RTCONFIG_QTN
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
#endif
	retval += websWrite(wp, "\n");

	retval += websWrite(wp, "Supported Rates: ");
	retval += dump_rateset(eid, wp, argc, argv, bi->rateset.rates, dtoh32(bi->rateset.count));
	retval += websWrite(wp, "\n");
	if (dtoh32(bi->ie_length))
		retval += wl_dump_wpa_rsn_ies(eid, wp, argc, argv, (uint8 *)(((uint8 *)bi) + dtoh16(bi->ie_offset)),
				    dtoh32(bi->ie_length));
#ifndef RTCONFIG_QTN
	if (dtoh32(bi->version) != LEGACY_WL_BSS_INFO_VERSION && bi->n_cap) {
		if (bi->vht_cap)
			retval += websWrite(wp, "VHT Capable:\n");
		else
			retval += websWrite(wp, "HT Capable:\n");
		retval += websWrite(wp, "\tChanspec: %sGHz channel %d %dMHz (0x%x)\n",
			CHSPEC_IS2G(bi->chanspec)?"2.4":"5", CHSPEC_CHANNEL(bi->chanspec),
		       (CHSPEC_IS80(bi->chanspec) ?
			80 : (CHSPEC_IS40(bi->chanspec) ?
			      40 : (CHSPEC_IS20(bi->chanspec) ? 20 : 10))),
			bi->chanspec);
		retval += websWrite(wp, "\tPrimary channel: %d\n", bi->ctl_ch);
		retval += websWrite(wp, "\tHT Capabilities: ");
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

		if (bi->vht_cap) {
			int i;
			uint mcs;
			retval += websWrite(wp, "\tVHT Capabilities: \n");
			retval += websWrite(wp, "\tSupported VHT (tx) Rates:\n");
			for (i = 1; i <= VHT_CAP_MCS_MAP_NSS_MAX; i++) {
				mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i, dtoh16(bi->vht_txmcsmap));
				if (mcs != VHT_CAP_MCS_MAP_NONE)
					retval += websWrite(wp, "\t\tNSS: %d MCS: %s\n", i,
						(mcs == VHT_CAP_MCS_MAP_0_9 ? "0-9" :
						(mcs == VHT_CAP_MCS_MAP_0_8 ? "0-8" : "0-7")));
			}
			retval += websWrite(wp, "\tSupported VHT (rx) Rates:\n");
			for (i = 1; i <= VHT_CAP_MCS_MAP_NSS_MAX; i++) {
				mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i, dtoh16(bi->vht_rxmcsmap));
				if (mcs != VHT_CAP_MCS_MAP_NONE)
					retval += websWrite(wp, "\t\tNSS: %d MCS: %s\n", i,
						(mcs == VHT_CAP_MCS_MAP_0_9 ? "0-9" :
						(mcs == VHT_CAP_MCS_MAP_0_8 ? "0-8" : "0-7")));
			}
		}
	}
#endif
	if (dtoh32(bi->ie_length))
	{
		retval += wl_dump_wps(wp, (uint8 *)(((uint8 *)bi) + dtoh16(bi->ie_offset)),
			dtoh32(bi->ie_length));
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

sta_info_t *
wl_sta_info(char *ifname, struct ether_addr *ea)
{
	static char buf[sizeof(sta_info_t)];
	sta_info_t *sta = NULL;

	strcpy(buf, "sta_info");
	memcpy(buf + strlen(buf) + 1, (void *)ea, ETHER_ADDR_LEN);

	if (!wl_ioctl(ifname, WLC_GET_VAR, buf, sizeof(buf))) {
		sta = (sta_info_t *)buf;
		sta->ver = dtoh16(sta->ver);

		/* Report unrecognized version */
		if (sta->ver > WL_STA_VER) {
			dbg(" ERROR: unknown driver station info version %d\n", sta->ver);
			return NULL;
		}

		sta->len = dtoh16(sta->len);
		sta->cap = dtoh16(sta->cap);
#ifdef RTCONFIG_BCMARM
		sta->aid = dtoh16(sta->aid);
#endif
		sta->flags = dtoh32(sta->flags);
		sta->idle = dtoh32(sta->idle);
		sta->rateset.count = dtoh32(sta->rateset.count);
		sta->in = dtoh32(sta->in);
		sta->listen_interval_inms = dtoh32(sta->listen_interval_inms);
#ifdef RTCONFIG_BCMARM
		sta->ht_capabilities = dtoh16(sta->ht_capabilities);
		sta->vht_flags = dtoh16(sta->vht_flags);
#endif
	}

	return sta;
}

#if 0
char *
print_rate_buf(int raw_rate, char *buf)
{
	if (!buf) return NULL;

	if (raw_rate == -1) sprintf(buf, "        ");
	else if ((raw_rate % 1000) == 0)
		sprintf(buf, "%6dM ", raw_rate / 1000);
	else
		sprintf(buf, "%6.1fM ", (double) raw_rate / 1000);

	return buf;
}
#endif

int wl_control_channel(int unit);

#ifdef RTCONFIG_QTN
extern int wl_status_5g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_status_5g(int eid, webs_t wp, int argc, char_t **argv);
#endif

int
ej_wl_status(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char *name;
	char name_vif[] = "wlX.Y_XXXXXXXXXX";
	struct maclist *auth;
	int mac_list_size;
	int i, ii, val = 0, ret = 0;
	char *arplist = NULL, *arplistptr;
	char *leaselist = NULL, *leaselistptr;
	char hostnameentry[16];
	char ipentry[40], macentry[18];
	int found;
	char rxrate[12], txrate[12];
	char ea[ETHER_ADDR_STR_LEN];
	scb_val_t scb_val;
	int hr, min, sec;
	sta_info_t *sta;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
#ifdef RTCONFIG_PROXYSTA
	if (psta_exist_except(unit))
	{
		ret += websWrite(wp, "%s radio is disabled\n",
			(wl_control_channel(unit) > 0) ?
			((wl_control_channel(unit) > CH_MAX_2G_CHANNEL) ? "5 GHz" : "2.4 GHz") :
			(nvram_match(strcat_r(prefix, "nband", tmp), "1") ? "5 GHz" : "2.4 GHz"));
		return ret;
	}
#endif
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
#ifdef RTCONFIG_QTN
	if (unit && rpc_qtn_ready())
	{
		ret = qcsapi_wifi_rfstatus(WIFINAME, (qcsapi_unsigned_int *) &val);
		if (ret < 0)
			dbG("qcsapi_wifi_rfstatus error, return: %d\n", ret);
		else
			val = !val;
	}
	else
#endif
	{
		wl_ioctl(name, WLC_GET_RADIO, &val, sizeof(val));
		val &= WL_RADIO_SW_DISABLE | WL_RADIO_HW_DISABLE;
	}

#ifdef RTCONFIG_QTN
	if (unit && !rpc_qtn_ready())
	{
		ret += websWrite(wp, "5 GHz radio is not ready\n");
		return ret;
	}
	else
#endif
	if (val)
	{
		ret += websWrite(wp, "%s radio is disabled\n",
			(wl_control_channel(unit) > 0) ?
			((wl_control_channel(unit) > CH_MAX_2G_CHANNEL) ? "5 GHz" : "2.4 GHz") :
			(nvram_match(strcat_r(prefix, "nband", tmp), "1") ? "5 GHz" : "2.4 GHz"));
		return ret;
	}

	if (nvram_match(strcat_r(prefix, "mode", tmp), "wds")) {
		// dump static info only for wds mode:
		// ret += websWrite(wp, "SSID: %s\n", nvram_safe_get(strcat_r(prefix, "ssid", tmp)));
		ret += websWrite(wp, "Channel: %d\n", wl_control_channel(unit));
	}
	else {
#ifdef RTCONFIG_QTN
		if (unit)
			ret += wl_status_5g(eid, wp, argc, argv);
		else
#endif
		ret += wl_status(eid, wp, argc, argv, unit);
	}

	if (nvram_match(strcat_r(prefix, "mode", tmp), "ap"))
	{
		if (nvram_match(strcat_r(prefix, "lazywds", tmp), "1") ||
			nvram_invmatch(strcat_r(prefix, "wds", tmp), ""))
			ret += websWrite(wp, "Mode	: Hybrid\n");
		else	ret += websWrite(wp, "Mode	: AP Only\n");
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
#ifdef RTCONFIG_PROXYSTA
	else if (nvram_match(strcat_r(prefix, "mode", tmp), "psta"))
	{
		if ((nvram_get_int("sw_mode") == SW_MODE_AP) &&
			(nvram_get_int("wlc_psta") == 1) &&
			(nvram_get_int("wlc_band") == unit))
		ret += websWrite(wp, "Mode	: Media Bridge\n");
	}
#endif

#ifdef RTCONFIG_QTN
	if (unit && rpc_qtn_ready())
	{
		ret += ej_wl_status_5g(eid, wp, argc, argv);
		return ret;
	}
#endif

#ifdef RTCONFIG_WIRELESSREPEATER
	if ((nvram_get_int("sw_mode") == SW_MODE_REPEATER)
		&& (nvram_get_int("wlc_band") == unit))
	{
		sprintf(name_vif, "wl%d.%d", unit, 1);
		name = name_vif;
	}
#endif

	/* buffers and length */
	mac_list_size = sizeof(auth->count) + MAX_STA_COUNT * sizeof(struct ether_addr);
	auth = malloc(mac_list_size);

	if (!auth)
		goto exit;

	memset(auth, 0, mac_list_size);

	/* query wl for authenticated sta list */
	strcpy((char*)auth, "authe_sta_list");
	if (wl_ioctl(name, WLC_GET_VAR, auth, mac_list_size))
		goto exit;

	/* Obtain mac + IP list */
	arplist = read_whole_file("/proc/net/arp");
	/* Obtain lease list - we still need the arp list for
	   cases where a device uses a static IP rather than DHCP */
	leaselist = read_whole_file("/var/lib/misc/dnsmasq.leases");

	ret += websWrite(wp, "\n");
#ifdef RTCONFIG_QTN
 	ret += websWrite(wp, "Stations  (flags: S=Short GI, T=STBC, A=Associated, U=Authenticated)\n");
#else
	ret += websWrite(wp, "Stations  (flags: P=Powersave Mode, S=Short GI, T=STBC, A=Associated, U=Authenticated)\n");
#endif

 	ret += websWrite(wp, "----------------------------------------\n");

	if (leaselist) {
		ret += websWrite(wp, "%-18s%-16s%-16s%-8s%-15s%-10s%-5s\n",
				"MAC", "IP Address", "Name", "  RSSI", "  Rx/Tx Rate", "Connected", "Flags");
	} else {
		ret += websWrite(wp, "%-18s%-16s%-8s%-15s%-10s%-5s\n",
				"MAC", "IP Address", "  RSSI", "  Rx/Tx Rate", "Connected", "Flags");
	}

	/* build authenticated sta list */
	for (i = 0; i < auth->count; i ++) {
		sta = wl_sta_info(name, &auth->ea[i]);
		if (!sta) continue;

		ret += websWrite(wp, "%-18s", ether_etoa((void *)&auth->ea[i], ea));

		found = 0;
		if (arplist) {
			arplistptr = arplist;

			while ((arplistptr < arplist+strlen(arplist)-2) && (sscanf(arplistptr,"%15s %*s %*s %17s",ipentry,macentry) == 2)) {
				if (upper_strcmp(macentry, ether_etoa((void *)&auth->ea[i], ea)) == 0) {
					found = 1;
					break;
				} else {
					arplistptr = strstr(arplistptr,"\n")+1;
				}
			}

			if (found || !leaselist) {
				ret += websWrite(wp, "%-16s", (found ? ipentry : ""));
			}
		}

		// Retrieve hostname from dnsmasq leases
		if (leaselist) {
			leaselistptr = leaselist;

			while ((leaselistptr < leaselist+strlen(leaselist)-2) && (sscanf(leaselistptr,"%*s %17s %15s %15s %*s", macentry, ipentry, hostnameentry) == 3)) {
				if (upper_strcmp(macentry, ether_etoa((void *)&auth->ea[i], ea)) == 0) {
					found += 2;
					break;
				} else {
					leaselistptr = strstr(leaselistptr,"\n")+1;
				}
			}
			if (found == 0) {
				// Not in arplist nor in leaselist
				ret += websWrite(wp, "%-16s%-15s ", "", "");
			} else if (found == 1) {
				// Only in arplist (static IP)
				ret += websWrite(wp, "%-15s ", "");
			} else if (found == 2) {
				// Only in leaselist (dynamic IP that has not communicated with router for a while)
				ret += websWrite(wp, "%-16s%-15s ", ipentry, hostnameentry);
			} else if (found == 3) {
				// In both arplist and leaselist (dynamic IP)
				ret += websWrite(wp, "%-15s ", hostnameentry);
			}
		}

// RSSI
		memcpy(&scb_val.ea, &auth->ea[i], ETHER_ADDR_LEN);
		if (wl_ioctl(name, WLC_GET_RSSI, &scb_val, sizeof(scb_val_t)))
			ret += websWrite(wp, "%-8s", "");
		else
			ret += websWrite(wp, " %3ddBm ", scb_val.val);

		if (sta->flags & WL_STA_SCBSTATS)
		{
// Rate
			if ((int)sta->rx_rate > 0)
				sprintf(rxrate,"%d", sta->rx_rate / 1000);
			else
				strcpy(rxrate,"??");

			if ((int)sta->tx_rate > 0)
				sprintf(txrate,"%d Mbps", sta->tx_rate / 1000);
			else
				sprintf(rxrate,"??");

			ret += websWrite(wp, "%4s/%-10s", rxrate, txrate);

// Connect time
			hr = sta->in / 3600;
			min = (sta->in % 3600) / 60;
			sec = sta->in - hr * 3600 - min * 60;
			ret += websWrite(wp, "%3d:%02d:%02d ", hr, min, sec);

// Flags
#ifdef RTCONFIG_BCMARM
			ret += websWrite(wp, "%s%s%s",
				(sta->flags & WL_STA_PS) ? "P" : " ",
				((sta->ht_capabilities & WL_STA_CAP_SHORT_GI_20) || (sta->ht_capabilities & WL_STA_CAP_SHORT_GI_40)) ? "S" : " ",
				((sta->ht_capabilities & WL_STA_CAP_TX_STBC) || (sta->ht_capabilities & WL_STA_CAP_RX_STBC_MASK)) ? "T" : " ");
#else
			ret += websWrite(wp, "%s",
				(sta->flags & WL_STA_PS) ? "P" : " ");
#endif
		}
		ret += websWrite(wp, "%s%s\n",
			(sta->flags & WL_STA_ASSOC) ? "A" : " ",
			(sta->flags & WL_STA_AUTHO) ? "U" : " ");
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
			memset(auth, 0, mac_list_size);

			/* query wl for authenticated sta list */
			strcpy((char*)auth, "authe_sta_list");
			if (wl_ioctl(name_vif, WLC_GET_VAR, auth, mac_list_size))
				goto exit;

			for (ii = 0; ii < auth->count; ii++) {
				sta = wl_sta_info(name_vif, &auth->ea[ii]);
				if (!sta) continue;

				ret += websWrite(wp, "%-18s", ether_etoa((void *)&auth->ea[ii], ea));

				found = 0;
				if (arplist) {
					arplistptr = arplist;

					while ((arplistptr < arplist+strlen(arplist)-2) && (sscanf(arplistptr,"%15s %*s %*s %17s",ipentry,macentry) == 2)) {
						if (upper_strcmp(macentry, ether_etoa((void *)&auth->ea[ii], ea)) == 0) {
							found = 1;
							break;
						} else {
							arplistptr = strstr(arplistptr,"\n")+1;
						}
					}

					if (found || !leaselist) {
						ret += websWrite(wp, "%-16s", (found ? ipentry : ""));
					}
				}

				// Retrieve hostname from dnsmasq leases
				if (leaselist) {
					leaselistptr = leaselist;

					while ((leaselistptr < leaselist+strlen(leaselist)-2) && (sscanf(leaselistptr,"%*s %17s %15s %15s %*s", macentry, ipentry, hostnameentry) == 3)) {
						if (upper_strcmp(macentry, ether_etoa((void *)&auth->ea[ii], ea)) == 0) {
							found += 2;
							break;
						} else {
							leaselistptr = strstr(leaselistptr,"\n")+1;
						}
					}
					if (found == 0) {
						// Not in arplist nor in leaselist
						ret += websWrite(wp, "%-16s%-15s ", "", "");
					} else if (found == 1) {
						// Only in arplist (static IP)
						ret += websWrite(wp, "%-15s ", "");
					} else if (found == 2) {
						// Only in leaselist (dynamic IP that has not communicated with router for a while)
						ret += websWrite(wp, "%-16s%-15s ", ipentry, hostnameentry);
					} else if (found == 3) {
						// In both arplist and leaselist (dynamic IP)
						ret += websWrite(wp, "%-15s ", hostnameentry);
					}
				}

// RSSI
				memcpy(&scb_val.ea, &auth->ea[ii], ETHER_ADDR_LEN);
				if (wl_ioctl(name, WLC_GET_RSSI, &scb_val, sizeof(scb_val_t)))
					ret += websWrite(wp, "%-8s", "");
				else
					ret += websWrite(wp, " %3ddBm ", scb_val.val);

				if (sta->flags & WL_STA_SCBSTATS)
				{
// Rate
					if ((int)sta->rx_rate > 0)
						sprintf(rxrate,"%d", sta->rx_rate / 1000);
					else
						strcpy(rxrate,"??");

					if ((int)sta->tx_rate > 0)
						sprintf(txrate,"%d Mbps", sta->tx_rate / 1000);
					else
						sprintf(rxrate,"??");

					ret += websWrite(wp, "%4s/%-10s", rxrate, txrate);

// Connect time
					hr = sta->in / 3600;
					min = (sta->in % 3600) / 60;
					sec = sta->in - hr * 3600 - min * 60;
					ret += websWrite(wp, "%3d:%02d:%02d ", hr, min, sec);

// Flags
#ifdef RTCONFIG_BCMARM
					ret += websWrite(wp, "%s%s%s",
						(sta->flags & WL_STA_PS) ? "P" : " ",
						((sta->ht_capabilities & WL_STA_CAP_SHORT_GI_20) || (sta->ht_capabilities & WL_STA_CAP_SHORT_GI_40)) ? "S" : " ",
						((sta->ht_capabilities & WL_STA_CAP_TX_STBC) || (sta->ht_capabilities & WL_STA_CAP_RX_STBC_MASK)) ? "T" : " ");
#else
					ret += websWrite(wp, "%s",
						(sta->flags & WL_STA_PS) ? "P" : " ");
#endif
				}

// Auth/Ass flags

		                ret += websWrite(wp, "%s%s\n",
                		        (sta->flags & WL_STA_ASSOC) ? "A" : " ",
                		        (sta->flags & WL_STA_AUTHO) ? "U" : " ");
			}
		}
	}

	ret += websWrite(wp, "\n");
	/* error/exit */
exit:
	if (auth) free(auth);
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
wl_extent_channel(int unit)
{
	int ret;
	struct ether_addr bssid;
	wl_bss_info_t *bi;
	wl_bss_info_107_t *old_bi;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char *name;
#ifdef RTCONFIG_QTN
	qcsapi_unsigned_int bw;
#endif

        snprintf(prefix, sizeof(prefix), "wl%d_", unit);
        name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	if (unit == 1) {
#ifdef RTCONFIG_QTN
		if (rpc_qcsapi_get_bw(&bw) >= 0) {
			return bw;
		} else {
			return 0;
		}
#else
		if ((ret = wl_ioctl(name, WLC_GET_BSSID, &bssid, ETHER_ADDR_LEN)) == 0) {
			/* The adapter is associated. */
			*(uint32*)buf = htod32(WLC_IOCTL_MAXLEN);
			if ((ret = wl_ioctl(name, WLC_GET_BSS_INFO, buf, WLC_IOCTL_MAXLEN)) < 0)
				return 0;
			bi = (wl_bss_info_t*)(buf + 4);

			return bw_chspec_to_mhz(bi->chanspec);
		} else {
			return 0;
		}
#endif

	}

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
				return  CHSPEC_CHANNEL(bi->chanspec);
		}
	}

	return 0;
}

int
ej_wl_extent_channel(int eid, webs_t wp, int argc, char_t **argv)
{
	return websWrite(wp, "[\"%d\", \"%d\"]", wl_extent_channel(0), wl_extent_channel(1));
}

int
wl_control_channel(int unit)
{
	int ret;
	struct ether_addr bssid;
	wl_bss_info_t *bi;
	wl_bss_info_107_t *old_bi;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char *name;
#ifdef RTCONFIG_QTN
	qcsapi_unsigned_int p_channel;
#endif

#ifdef RTCONFIG_QTN
	if (unit == 1) {
		if (rpc_qcsapi_get_channel(&p_channel) >= 0) {
			return p_channel;
		} else {
			return 0;
		}
	}
#endif

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
			else
				return CHSPEC_CHANNEL(bi->chanspec);
		}
	}

	return 0;
}

int
ej_wl_control_channel(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret = 0;
	int channel_24 = 0, channel_50 = 0;
#ifdef RTAC3200
	int channel_50_2 = 0;
#endif

	channel_24 = wl_control_channel(0);

	if (!(channel_50 = wl_control_channel(1)))
		ret = websWrite(wp, "[\"%d\", \"%d\"]", channel_24, 0);
	else
#ifndef RTAC3200
		ret = websWrite(wp, "[\"%d\", \"%d\"]", channel_24, channel_50);
#else
	{
		if (!(channel_50_2 = wl_control_channel(2)))
			ret = websWrite(wp, "[\"%d\", \"%d\", \"%d\"]", channel_24, channel_50, 0);
		else
			ret = websWrite(wp, "[\"%d\", \"%d\", \"%d\"]", channel_24, channel_50, channel_50_2);
	}
#endif

	return ret;
}

#define	IW_MAX_FREQUENCIES	32

static int ej_wl_channel_list(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
	int i, retval = 0;
	int channels[MAXCHANNEL+1];
	wl_uint32_list_t *list = (wl_uint32_list_t *) channels;
	char tmp[256], tmp1[256], tmp2[256], prefix[] = "wlXXXXXXXXXX_";
	char *name;
	uint ch;
	char word[256], *next;
	int unit_max = 0, count = 0;

	sprintf(tmp1, "[\"%d\"]", 0);

	foreach (word, nvram_safe_get("wl_ifnames"), next)
		unit_max++;

	if (unit > (unit_max - 1))
		goto ERROR;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	if (is_wlif_up(name) != 1)
	{
		foreach (word, nvram_safe_get(strcat_r(prefix, "chlist", tmp)), next)
			count++;

		if (count < 2)
			goto ERROR;

		i = 0;
		foreach (word, nvram_safe_get(strcat_r(prefix, "chlist", tmp)), next) {
			if (i == 0)
			{
				sprintf(tmp1, "[\"%s\",", word);
			}
			else if (i == (count - 1))
			{
				sprintf(tmp1,  "%s \"%s\"]", tmp1, word);
			}
			else
			{
				sprintf(tmp1,  "%s \"%s\",", tmp1, word);
			}

			i++;
		}
		goto ERROR;
	}

	memset(channels, 0, sizeof(channels));
	list->count = htod32(MAXCHANNEL);
	if (wl_ioctl(name, WLC_GET_VALID_CHANNELS , channels, sizeof(channels)) < 0)
	{
		dbg("error doing WLC_GET_VALID_CHANNELS\n");
		sprintf(tmp1, "[\"%d\"]", 0);
		goto ERROR;
	}

	if (dtoh32(list->count) == 0)
	{
		sprintf(tmp1, "[\"%d\"]", 0);
		goto ERROR;
	}

	for (i = 0; i < dtoh32(list->count) && i < IW_MAX_FREQUENCIES; i++) {
		ch = dtoh32(list->element[i]);

		if (i == 0)
		{
			sprintf(tmp1, "[\"%d\",", ch);
			sprintf(tmp2, "%d", ch);
		}
		else if (i == (dtoh32(list->count) - 1))
		{
			sprintf(tmp1,  "%s \"%d\"]", tmp1, ch);
			sprintf(tmp2,  "%s %d", tmp2, ch);
		}
		else
		{
			sprintf(tmp1,  "%s \"%d\",", tmp1, ch);
			sprintf(tmp2,  "%s %d", tmp2, ch);
		}

		if (strlen(tmp2))
			nvram_set(strcat_r(prefix, "chlist", tmp), tmp2);
	}
ERROR:
	retval += websWrite(wp, "%s", tmp1);
	return retval;
}

int
ej_wl_channel_list_2g(int eid, webs_t wp, int argc, char_t **argv)
{
#ifndef RTAC3200_INTF_ORDER
	return ej_wl_channel_list(eid, wp, argc, argv, 0);
#else
	return ej_wl_channel_list(eid, wp, argc, argv, 1);
#endif
}

#ifndef RTCONFIG_QTN
int
ej_wl_channel_list_5g(int eid, webs_t wp, int argc, char_t **argv)
{
#ifndef RTAC3200_INTF_ORDER
	return ej_wl_channel_list(eid, wp, argc, argv, 1);
#else
	return ej_wl_channel_list(eid, wp, argc, argv, 0);
#endif
}
#endif

#ifdef RTAC3200
int
ej_wl_channel_list_5g_2(int eid, webs_t wp, int argc, char_t **argv)
{
	return ej_wl_channel_list(eid, wp, argc, argv, 2);
}
#endif

static int ej_wl_rate(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
	int retval = 0;
	char tmp[256], prefix[] = "wlXXXXXXXXXX_";
	char *name;
	char word[256], *next;
	int unit_max = 0, unit_cur = -1;
	int rate = 0;
	char rate_buf[32];

#ifdef RTCONFIG_QTN
	uint32_t count = 0, speed;
#endif

	sprintf(rate_buf, "0 Mbps");

#ifdef RTCONFIG_QTN
	if (unit != 0){
		if (!rpc_qtn_ready()){
			goto ERROR;
		}
		// if ssid associated, check associations
		if(qcsapi_wifi_get_link_quality(WIFINAME, count, &speed) < 0){
			// dbg("fail to get link status index %d\n", (int)count);
		}else{
			speed = speed ;  /* 4 antenna? */
			if((int)speed < 1){
				sprintf(rate_buf, "auto");
			}else{
				sprintf(rate_buf, "%d Mbps", (int)speed);
			}
		}

		retval += websWrite(wp, "%s", rate_buf);
		return retval;
	}
#endif

	foreach (word, nvram_safe_get("wl_ifnames"), next)
		unit_max++;

	if (unit > (unit_max - 1))
		goto ERROR;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	wl_ioctl(name, WLC_GET_INSTANCE, &unit_cur, sizeof(unit_cur));
	if (unit != unit_cur)
		goto ERROR;
	else if (wl_ioctl(name, WLC_GET_RATE, &rate, sizeof(int)))
	{
		dbG("can not get rate info of %s\n", name);
		goto ERROR;
	}
	else
	{
		rate = dtoh32(rate);
		if ((rate == -1) || (rate == 0))
			sprintf(rate_buf, "auto");
		else
			sprintf(rate_buf, "%d%s Mbps", (rate / 2), (rate & 1) ? ".5" : "");
	}

#ifdef RTCONFIG_BCM7
	/* workaround for SDK 7.x */
	if ((wl_control_channel(unit) > 0) ?
		(wl_control_channel(unit) <= CH_MAX_2G_CHANNEL) :
		nvram_match(strcat_r(prefix, "nband", tmp), "2")) {

		if (!nvram_match(strcat_r(prefix, "mode", tmp), "psta"))
			goto ERROR;

		struct ether_addr bssid;
		unsigned char bssid_null[6] = {0x0,0x0,0x0,0x0,0x0,0x0};
		if (wl_ioctl(name, WLC_GET_BSSID, &bssid, ETHER_ADDR_LEN) != 0)
			goto ERROR;
		else if (!memcmp(&bssid, bssid_null, 6))
			goto ERROR;

		sta_info_t *sta = wl_sta_info(name, &bssid);
		if (sta && (sta->flags & WL_STA_SCBSTATS)) {
			if ((sta->rx_rate % 1000) == 0)
				sprintf(rate_buf, "%6d Mbps ", sta->rx_rate / 1000);
			else
				sprintf(rate_buf, "%6.1fM Mbps", (double) sta->rx_rate / 1000);
		}
	}
#endif

ERROR:
	retval += websWrite(wp, "%s", rate_buf);
	return retval;
}

int
ej_wl_rate_2g(int eid, webs_t wp, int argc, char_t **argv)
{
#ifndef RTAC3200_INTF_ORDER
	return ej_wl_rate(eid, wp, argc, argv, 0);
#else
	return ej_wl_rate(eid, wp, argc, argv, 1);
#endif
}

int
ej_wl_rate_5g(int eid, webs_t wp, int argc, char_t **argv)
{
#ifndef RTAC3200_INTF_ORDER
	return ej_wl_rate(eid, wp, argc, argv, 1);
#else
	return ej_wl_rate(eid, wp, argc, argv, 0);
#endif
}

#ifdef RTAC3200
int
ej_wl_rate_5g_2(int eid, webs_t wp, int argc, char_t **argv)
{
	return ej_wl_rate(eid, wp, argc, argv, 2);
}
#endif

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
#ifdef RTCONFIG_QTN
	int ret;
	qcsapi_SSID ssid;
	string_64 key_passphrase;
//	char wps_pin[16];
#endif


	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	retval += websWrite(wp, "<wps>\n");

	//0. WSC Status
	if (!strcmp(nvram_safe_get(strcat_r(prefix, "wps_mode", tmp)), "enabled"))
	{
#ifdef RTCONFIG_QTN
		if (unit)
		{
			if (!rpc_qtn_ready())
				retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "5 GHz radio is not ready");
			else
				retval += websWrite(wp, "<wps_info>%s</wps_info>\n", getWscStatusStr_qtn());
		}
	else
#endif
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", getWscStatusStr());
	}
	else
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "Not used");

	//1. WPSConfigured
#ifdef RTCONFIG_QTN
	if (unit)
	{
		if (!rpc_qtn_ready())
			retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "");
		else
			retval += websWrite(wp, "<wps_info>%s</wps_info>\n", get_WPSConfiguredStr_qtn());
	}
	else
#endif
	if (!wps_is_oob())
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "Yes");
	else
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "No");

	//2. WPSSSID
#ifdef RTCONFIG_QTN
	if (unit)
	{
		if (!rpc_qtn_ready())
			retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "");
		else
		{
			memset(&ssid, 0, sizeof(qcsapi_SSID));
			ret = rpc_qcsapi_get_SSID(WIFINAME, &ssid);
			if (ret < 0)
				dbG("rpc_qcsapi_get_SSID %s error, return: %d\n", WIFINAME, ret);

			memset(tmpstr, 0, sizeof(tmpstr));
			char_to_ascii(tmpstr, ssid);
			retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);
		}
	}
	else
#endif
	{
		memset(tmpstr, 0, sizeof(tmpstr));
		char_to_ascii(tmpstr, nvram_safe_get(strcat_r(prefix, "ssid", tmp)));
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);
	}

	//3. WPSAuthMode
#ifdef RTCONFIG_QTN
	if (unit)
	{
		if (!rpc_qtn_ready())
			retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "");
		else
			retval += websWrite(wp, "<wps_info>%s</wps_info>\n", getWPSAuthMode_qtn());
	}
	else
#endif
	{
		memset(tmpstr, 0, sizeof(tmpstr));
		getWPSAuthMode(unit, tmpstr);
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);
	}

	//4. EncrypType
#ifdef RTCONFIG_QTN
	if (unit)
	{
		if (!rpc_qtn_ready())
			retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "");
		else
			retval += websWrite(wp, "<wps_info>%s</wps_info>\n", getWPSEncrypType_qtn());
	}
	else
#endif
	{
		memset(tmpstr, 0, sizeof(tmpstr));
		getWPSEncrypType(unit, tmpstr);
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);
	}

	//5. DefaultKeyIdx
#ifdef RTCONFIG_QTN
	if (unit)
	{
		if (!rpc_qtn_ready())
			retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "");
		else
			retval += websWrite(wp, "<wps_info>%d</wps_info>\n", 1);
	}
	else
#endif
	{
		memset(tmpstr, 0, sizeof(tmpstr));
		sprintf(tmpstr, "%s", nvram_safe_get(strcat_r(prefix, "key", tmp)));
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);
	}

	//6. WPAKey
#ifdef RTCONFIG_QTN
	if (unit)
	{
		if (!rpc_qtn_ready())
			retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "");
		else
		{
			memset(&key_passphrase, 0, sizeof(key_passphrase));
			ret = rpc_qcsapi_get_key_passphrase(WIFINAME, (char *) &key_passphrase);
			if (ret < 0)
				dbG("rpc_qcsapi_get_key_passphrase %s error, return: %d\n", WIFINAME, ret);

			if (!strlen(key_passphrase))
				retval += websWrite(wp, "<wps_info>None</wps_info>\n");
			else
			{
				memset(tmpstr, 0, sizeof(tmpstr));
				char_to_ascii(tmpstr, key_passphrase);
				retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);
			}
		}
	}
#endif
	if (!strlen(nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp))))
		retval += websWrite(wp, "<wps_info>None</wps_info>\n");
	else
	{
		memset(tmpstr, 0, sizeof(tmpstr));
		char_to_ascii(tmpstr, nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp)));
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);
	}

	//7. AP PIN Code
#ifdef RTCONFIG_QTNBAK
	/* QTN get wps_device_pin from BRCM */
	if (unit)
	{
		if (!rpc_qtn_ready())
			retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "");
		else
		{
			wps_pin[0] = 0;
			ret = rpc_qcsapi_wps_get_ap_pin(WIFINAME, wps_pin, 0);
			if (ret < 0)
				dbG("rpc_qcsapi_wps_get_ap_pin %s error, return: %d\n", WIFINAME, ret);

			retval += websWrite(wp, "<wps_info>%s</wps_info>\n", wps_pin);
		}
	}
	else
#endif
	{
		memset(tmpstr, 0, sizeof(tmpstr));
		sprintf(tmpstr, "%s", nvram_safe_get("wps_device_pin"));
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);
	}

	//8. Saved WPAKey
#ifdef RTCONFIG_QTN
	if (unit)
	{
		if (!rpc_qtn_ready())
			retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "");
		else
		{
			if (!strlen(key_passphrase))
				retval += websWrite(wp, "<wps_info>None</wps_info>\n");
			else
			{
				memset(tmpstr, 0, sizeof(tmpstr));
				char_to_ascii(tmpstr, key_passphrase);
				retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);
			}
		}
	}
	else
#endif
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
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "0");

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
	unsigned char rate;
	unsigned char bssid[6];
	char macstr[18];
	char ure_mac[18];
	char ssid_str[256];
	wl_scan_results_t *result;
	wl_bss_info_t *info;
	wl_bss_info_107_t *old_info;
	struct bss_ie_hdr *ie;
	NDIS_802_11_NETWORK_TYPE NetWorkType;
	struct maclist *authorized;
	int mac_list_size;
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

			/* Convert version 107 to 109 */
			if (dtoh32(info->version) == LEGACY_WL_BSS_INFO_VERSION) {
				old_info = (wl_bss_info_107_t *)info;
				info->chanspec = CH20MHZ_CHSPEC(old_info->channel);
				info->ie_length = old_info->ie_length;
				info->ie_offset = sizeof(wl_bss_info_107_t);
			}

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
					if(strcmp(apinfos[k].BSSID, macstr) == 0 && strcmp(apinfos[k].SSID, (char *)info->SSID) == 0)
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
//					apinfos[ap_count].channel = info->chanspec;
					apinfos[ap_count].channel = (uint8)(info->chanspec & WL_CHANSPEC_CHAN_MASK);
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
					if ((uint8)(info->chanspec & WL_CHANSPEC_CHAN_MASK) <= 14)
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
						{
							if (info->vht_cap)
								NetWorkType = Ndis802_11OFDM5_VHT;
							else
								NetWorkType = Ndis802_11OFDM5_N;
						}
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
			fprintf(stderr, "%2d ", apinfos[k].ctl_ch);
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
			else if (apinfos[k].NetworkType == Ndis802_11OFDM5_VHT)
				fprintf(stderr, "%-7s", "11ac");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM24)
				fprintf(stderr, "%-7s", "11b/g");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM24_N)
				fprintf(stderr, "%-7s", "11b/g/n");
			else
				fprintf(stderr, "%-7s", "unknown");

			fprintf(stderr, "%3d", apinfos[k].ctl_ch);

			if (	((apinfos[k].NetworkType == Ndis802_11OFDM5_VHT) || (apinfos[k].NetworkType == Ndis802_11OFDM5_N) || (apinfos[k].NetworkType == Ndis802_11OFDM24_N)) &&
				(apinfos[k].channel != apinfos[k].ctl_ch))
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
		mac_list_size = sizeof(authorized->count) + MAX_STA_COUNT * sizeof(struct ether_addr);
		authorized = malloc(mac_list_size);

		if (!authorized) goto ap_list;

		memset(authorized, 0, mac_list_size);

		// query wl for authorized sta list
		strcpy((char*)authorized, "autho_sta_list");
		if (!wl_ioctl(WIF, WLC_GET_VAR, authorized, mac_list_size))
		{
			if (authorized->count > 0) wl_authorized = 1;
		}

		if (authorized) free(authorized);
	}
ap_list:
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

		retval += websWrite(wp, "\"%d\", ", apinfos[i].ctl_ch);

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
			else if (apinfos[i].wid.pairwise_cipher == (WPA_CIPHER_TKIP_|WPA_CIPHER_CCMP_))
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
		else if (apinfos[i].NetworkType == Ndis802_11OFDM5_VHT)
			retval += websWrite(wp, "\"%s\", ", "ac");
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

#if 0
static bool find_ethaddr_in_list(void *ethaddr, struct maclist *list){
	int i;

	for(i = 0; i < list->count; ++i)
		if(!bcmp(ethaddr, (void *)&list->ea[i], ETHER_ADDR_LEN))
			return TRUE;

	return FALSE;
}
#endif

static int wl_sta_list(int eid, webs_t wp, int argc, char_t **argv, int unit) {
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char *name;
	struct maclist *auth;
	int mac_list_size;
	int i, firstRow = 1;
	char ea[ETHER_ADDR_STR_LEN];
	scb_val_t scb_val;
	char *value;
	char name_vif[] = "wlX.Y_XXXXXXXXXX";
	int ii;
	int ret = 0;
	sta_info_t *sta;

	/* buffers and length */
	mac_list_size = sizeof(auth->count) + MAX_STA_COUNT * sizeof(struct ether_addr);
	auth = malloc(mac_list_size);

	if(!auth)
		goto exit;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	memset(auth, 0, mac_list_size);

	/* query wl for authenticated sta list */
	strcpy((char*)auth, "authe_sta_list");
	if (wl_ioctl(name, WLC_GET_VAR, auth, mac_list_size))
		goto exit;

	/* build authenticated sta list */
	for(i = 0; i < auth->count; ++i) {
		sta = wl_sta_info(name, &auth->ea[i]);
		if (!sta) continue;

		if (firstRow == 1)
			firstRow = 0;
		else
			ret += websWrite(wp, ", ");

		ret += websWrite(wp, "[");

		ret += websWrite(wp, "\"%s\"", ether_etoa((void *)&auth->ea[i], ea));

		value = (sta->flags & WL_STA_ASSOC) ? "Yes" : "No";
		ret += websWrite(wp, ", \"%s\"", value);

		value = (sta->flags & WL_STA_AUTHO) ? "Yes" : "No";
		ret += websWrite(wp, ", \"%s\"", value);

		memcpy(&scb_val.ea, &auth->ea[i], ETHER_ADDR_LEN);
		if (wl_ioctl(name, WLC_GET_RSSI, &scb_val, sizeof(scb_val_t)))
			ret += websWrite(wp, ", \"%d\"", 0);
		else
			ret += websWrite(wp, ", \"%d\"", scb_val.val);

		ret += websWrite(wp, "]");
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

			memset(auth, 0, mac_list_size);

			/* query wl for authenticated sta list */
			strcpy((char*)auth, "authe_sta_list");
			if (wl_ioctl(name_vif, WLC_GET_VAR, auth, mac_list_size))
				goto exit;

			for(ii = 0; ii < auth->count; ii++) {
				sta = wl_sta_info(name_vif, &auth->ea[ii]);
				if (!sta) continue;

				if (firstRow == 1)
					firstRow = 0;
				else
					ret += websWrite(wp, ", ");

				ret += websWrite(wp, "[");

				ret += websWrite(wp, "\"%s\"", ether_etoa((void *)&auth->ea[ii], ea));

				value = (sta->flags & WL_STA_ASSOC) ? "Yes" : "No";
				ret += websWrite(wp, ", \"%s\"", value);

				value = (sta->flags & WL_STA_AUTHO) ? "Yes" : "No";
				ret += websWrite(wp, ", \"%s\"", value);

				memcpy(&scb_val.ea, &auth->ea[ii], ETHER_ADDR_LEN);
				if (wl_ioctl(name, WLC_GET_RSSI, &scb_val, sizeof(scb_val_t)))
					ret += websWrite(wp, ", \"%d\"", 0);
				else
					ret += websWrite(wp, ", \"%d\"", scb_val.val);

				ret += websWrite(wp, "]");
			}
		}
	}

	/* error/exit */
exit:
	if(auth) free(auth);

	return ret;
}

int
ej_wl_sta_list_2g(int eid, webs_t wp, int argc, char_t **argv)
{
#ifndef RTAC3200_INTF_ORDER
	return wl_sta_list(eid, wp, argc, argv, 0);
#else
	return wl_sta_list(eid, wp, argc, argv, 1);
#endif
}

#ifndef RTCONFIG_QTN
int
ej_wl_sta_list_5g(int eid, webs_t wp, int argc, char_t **argv)
{
#ifndef RTAC3200_INTF_ORDER
	return wl_sta_list(eid, wp, argc, argv, 1);
#else
	return wl_sta_list(eid, wp, argc, argv, 0);
#endif
}

#ifdef RTAC3200
int
ej_wl_sta_list_5g_2(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_sta_list(eid, wp, argc, argv, 2);
}
#endif

#else
extern int ej_wl_sta_list_5g(int eid, webs_t wp, int argc, char_t **argv);
#endif

// no WME in WL500gP V2
// MAC/associated/authorized
int ej_wl_auth_list(int eid, webs_t wp, int argc, char_t **argv) {
	int unit = 0;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char *name;
	struct maclist *auth;
	int mac_list_size;
	int i, firstRow = 1;
	char ea[ETHER_ADDR_STR_LEN];
	scb_val_t scb_val;
	char *value;
	char word[256], *next;
	char name_vif[] = "wlX.Y_XXXXXXXXXX";
	int ii;
	int ret = 0;
	sta_info_t *sta;

	/* buffers and length */
	mac_list_size = sizeof(auth->count) + MAX_STA_COUNT * sizeof(struct ether_addr);
	auth = malloc(mac_list_size);
	//wme = malloc(mac_list_size);

	//if(!auth || !wme)
	if(!auth)
		goto exit;

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
#ifdef RTCONFIG_QTN
		if (unit) {
			if (rpc_qtn_ready()){
				if (firstRow == 1)
					firstRow = 0;
				else
					ret += websWrite(wp, ", ");
				ret += ej_wl_sta_list_5g(eid, wp, argc, argv);
			}
			return ret;
		}
#endif
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

		memset(auth, 0, mac_list_size);
		//memset(wme, 0, mac_list_size);

		/* query wl for authenticated sta list */
		strcpy((char*)auth, "authe_sta_list");
		if (wl_ioctl(name, WLC_GET_VAR, auth, mac_list_size))
			goto exit;

		/* query wl for WME sta list */
		/*strcpy((char*)wme, "wme_sta_list");
		if (wl_ioctl(name, WLC_GET_VAR, wme, mac_list_size))
			goto exit;*/

		/* build authenticated/associated sta list */
		for(i = 0; i < auth->count; ++i) {
			sta = wl_sta_info(name, &auth->ea[i]);
			if (!sta) continue;

			if (firstRow == 1)
				firstRow = 0;
			else
				ret += websWrite(wp, ", ");

			ret += websWrite(wp, "[");

			ret += websWrite(wp, "\"%s\"", ether_etoa((void *)&auth->ea[i], ea));

			value = (sta->flags & WL_STA_ASSOC) ? "Yes" : "No";
			ret += websWrite(wp, ", \"%s\"", value);

			value = (sta->flags & WL_STA_AUTHO) ? "Yes" : "No";
			ret += websWrite(wp, ", \"%s\"", value);

			/*value = (find_ethaddr_in_list((void *)&auth->ea[i], wme))?"Yes":"No";
			ret += websWrite(wp, ", \"%s\"", value);*/

			ret += websWrite(wp, "]");
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

				memset(auth, 0, mac_list_size);

				/* query wl for authenticated sta list */
				strcpy((char*)auth, "authe_sta_list");
				if (wl_ioctl(name_vif, WLC_GET_VAR, auth, mac_list_size))
					goto exit;

				for(ii = 0; ii < auth->count; ii++) {
					sta = wl_sta_info(name_vif, &auth->ea[ii]);
					if (!sta) continue;

					if (firstRow == 1)
						firstRow = 0;
					else
						ret += websWrite(wp, ", ");

					ret += websWrite(wp, "[");

					ret += websWrite(wp, "\"%s\"", ether_etoa((void *)&auth->ea[ii], ea));

					value = (sta->flags & WL_STA_ASSOC) ? "Yes" : "No";
					ret += websWrite(wp, ", \"%s\"", value);

					value = (sta->flags & WL_STA_AUTHO) ? "Yes" : "No";
					ret += websWrite(wp, ", \"%s\"", value);

					ret += websWrite(wp, "]");
				}
			}
		}

		unit++;
	}

	/* error/exit */
exit:
	if(auth) free(auth);
	//if(wme) free(wme);

	return ret;
}

/* WPS ENR mode APIs */
typedef struct wlc_ap_list_info
{
#if 0
	bool	used;
#endif
	uint8	ssid[33];
	uint8	ssidLen;
	uint8	BSSID[6];
#if 0
	uint8	*ie_buf;
	uint32	ie_buflen;
#endif
	uint8	channel;
#if 0
	uint8	wep;
	bool	scstate;
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
		return 0;

	memset(ap_list, 0, sizeof(ap_list));
	if (list->count == 0)
		return 0;
	else if (list->version != WL_BSS_INFO_VERSION &&
			list->version != LEGACY_WL_BSS_INFO_VERSION &&
			list->version != LEGACY2_WL_BSS_INFO_VERSION) {
		/* fprintf(stderr, "Sorry, your driver has bss_info_version %d "
		    "but this program supports only version %d.\n",
		    list->version, WL_BSS_INFO_VERSION); */
		return 0;
	}

	bi = list->bss_info;
	for (i = 0; i < list->count; i++) {
		/* Convert version 107 to 109 */
		if (dtoh32(bi->version) == LEGACY_WL_BSS_INFO_VERSION) {
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
			char_to_ascii(ssid_str, trim_r((char *)ap_list[i].ssid));

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

#ifndef RTCONFIG_QTN
int
ej_wl_scan_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_scan(eid, wp, argc, argv, 1);
}
#endif

#ifdef RTAC3200
int
ej_wl_scan_5g_2(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_scan(eid, wp, argc, argv, 2);
}
#endif

#ifdef RTCONFIG_PROXYSTA
#define	NVRAM_BUFSIZE	100

static int
wl_autho(char *name, struct ether_addr *ea)
{
	char buf[sizeof(sta_info_t)];

	strcpy(buf, "sta_info");
	memcpy(buf + strlen(buf) + 1, (void *)ea, ETHER_ADDR_LEN);

	if (!wl_ioctl(name, WLC_GET_VAR, buf, sizeof(buf))) {
		sta_info_t *sta = (sta_info_t *)buf;
		uint32 f = sta->flags;

		if (f & WL_STA_AUTHO)
			return 1;
	}

	return 0;
}

static int psta_auth = 0;

int
ej_wl_auth_psta(int eid, webs_t wp, int argc, char_t **argv)
{
	char tmp[NVRAM_BUFSIZE], prefix[] = "wlXXXXXXXXXX_";
	char *name;
	struct maclist *mac_list;
	int mac_list_size, i, unit;
	int retval = 0, psta = 0;
	struct ether_addr bssid;
	unsigned char bssid_null[6] = {0x0,0x0,0x0,0x0,0x0,0x0};
	int psta_debug = 0;

	if (nvram_match("psta_debug", "1"))
		psta_debug = 1;

	unit = nvram_get_int("wlc_band");
#ifdef RTCONFIG_QTN
	if(unit == 1){
		return ej_wl_auth_psta_qtn(eid, wp, argc, argv);
	}
#endif

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	if (!nvram_match(strcat_r(prefix, "mode", tmp), "psta"))
		goto PSTA_ERR;

	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	if (wl_ioctl(name, WLC_GET_BSSID, &bssid, ETHER_ADDR_LEN) != 0)
		goto PSTA_ERR;
	else if (!memcmp(&bssid, bssid_null, 6))
		goto PSTA_ERR;

	/* buffers and length */
	mac_list_size = sizeof(mac_list->count) + MAX_STA_COUNT * sizeof(struct ether_addr);
	mac_list = malloc(mac_list_size);

	if (!mac_list)
		goto PSTA_ERR;

	memset(mac_list, 0, mac_list_size);

	/* query wl for authenticated sta list */
	strcpy((char*)mac_list, "authe_sta_list");
	if (wl_ioctl(name, WLC_GET_VAR, mac_list, mac_list_size)) {
		free(mac_list);
		goto PSTA_ERR;
	}

	/* query sta_info for each STA */
	if (mac_list->count)
	{
		if (nvram_match(strcat_r(prefix, "akm", tmp), ""))
			psta = 1;
		else
		for (i = 0, psta = 2; i < mac_list->count; i++) {
			if (wl_autho(name, &mac_list->ea[i]))
			{
				psta = 1;
				break;
			}
		}
	}

	free(mac_list);
PSTA_ERR:
	if (psta == 1)
	{
		if (psta_debug) dbg("connected\n");
		psta_auth = 0;
	}
	else if (psta == 2)
	{
		if (psta_debug) dbg("authorization failed\n");
		psta_auth = 1;
	}
	else
	{
		if (psta_debug) dbg("disconnected\n");
	}

	retval += websWrite(wp, "wlc_state=%d;", psta);
	retval += websWrite(wp, "wlc_state_auth=%d;", psta_auth);

	return retval;
}
#endif
