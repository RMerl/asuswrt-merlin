/*
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: acsd_utils.c 428596 2013-10-09 11:37:33Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <assert.h>
#include <typedefs.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <bcmtimer.h>
#include <bcmendian.h>
#include <shutils.h>
#include <bcmendian.h>
#include <bcmwifi_channels.h>
#include <wlioctl.h>
#include <wlutils.h>
#include <ctype.h> /* isprint() */
#include "acsd.h"

bool acsd_swap = FALSE;
int acsd_debug_level = ACSD_DEBUG_ERROR;

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
			*p++ = '\\';
		} else if (isprint((uchar)c)) {
			*p++ = (char)c;
		} else {
			p += sprintf(p, "\\x%02X", c);
		}
	}
	*p = '\0';

	return p - ssid_buf;
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

void
dump_rateset(uint8 *rates, uint count)
{
	uint i;
	uint r;
	bool b;

	printf("[ ");
	for (i = 0; i < count; i++) {
		r = rates[i] & 0x7f;
		b = rates[i] & 0x80;
		if (r == 0)
			break;
		printf("%d%s%s ", (r / 2), (r % 2)?".5":"", b?"(b)":"");
	}
	printf("]");
}

static void
dump_bss_info(wl_bss_info_t *bi)
{
	char ssidbuf[SSID_FMT_BUF_LEN];
	wl_bss_info_107_t *old_bi;
	int mcs_idx = 0;

	/* Convert version 107 to 109 */
	if (dtoh32(bi->version) == LEGACY_WL_BSS_INFO_VERSION) {
		old_bi = (wl_bss_info_107_t *)bi;
		bi->chanspec = CH20MHZ_CHSPEC(old_bi->channel);
		bi->ie_length = old_bi->ie_length;
		bi->ie_offset = sizeof(wl_bss_info_107_t);
	}

	wl_format_ssid(ssidbuf, bi->SSID, bi->SSID_len);

	printf("SSID: \"%s\"\n", ssidbuf);

	printf("Mode: %s\t", capmode2str(dtoh16(bi->capability)));
	printf("RSSI: %d dBm\t", (int16)(dtoh16(bi->RSSI)));

	/*
	 * SNR has valid value in only 109 version.
	 * So print SNR for 109 version only.
	 */
	if (dtoh32(bi->version) == WL_BSS_INFO_VERSION) {
		printf("SNR: %d dB\t", (int16)(dtoh16(bi->SNR)));
	}

	printf("noise: %d dBm\t", bi->phy_noise);
	if (bi->flags) {
		bi->flags = dtoh16(bi->flags);
		printf("Flags: ");
		if (bi->flags & WL_BSS_FLAGS_FROM_BEACON) printf("FromBcn ");
		if (bi->flags & WL_BSS_FLAGS_FROM_CACHE) printf("Cached ");
		if (bi->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL) printf("RSSI on-channel ");
		printf("\t");
	}
	printf("chanspec: %x\n", dtohchanspec(bi->chanspec));

	printf("BSSID: %s\t", wl_ether_etoa(&bi->BSSID));

	printf("Capability: ");
	bi->capability = dtoh16(bi->capability);
	if (bi->capability & DOT11_CAP_ESS) printf("ESS ");
	if (bi->capability & DOT11_CAP_IBSS) printf("IBSS ");
	if (bi->capability & DOT11_CAP_POLLABLE) printf("Pollable ");
	if (bi->capability & DOT11_CAP_POLL_RQ) printf("PollReq ");
	if (bi->capability & DOT11_CAP_PRIVACY) printf("WEP ");
	if (bi->capability & DOT11_CAP_SHORT) printf("ShortPre ");
	if (bi->capability & DOT11_CAP_PBCC) printf("PBCC ");
	if (bi->capability & DOT11_CAP_AGILITY) printf("Agility ");
	if (bi->capability & DOT11_CAP_SHORTSLOT) printf("ShortSlot ");
	if (bi->capability & DOT11_CAP_CCK_OFDM) printf("CCK-OFDM ");
	printf("\n");

	printf("Supported Rates: ");
	dump_rateset(bi->rateset.rates, dtoh32(bi->rateset.count));
	printf("\n");

	if (dtoh32(bi->version) != LEGACY_WL_BSS_INFO_VERSION && bi->n_cap) {
		printf("802.11N Capable:\n");
		bi->chanspec = dtohchanspec(bi->chanspec);
		printf("\tChanspec: %sGHz channel %d %dMHz (0x%x)\n",
			CHSPEC_IS2G(bi->chanspec)?"2.4":"5", CHSPEC_CHANNEL(bi->chanspec),
			CHSPEC_IS40(bi->chanspec) ? 40 : (CHSPEC_IS20(bi->chanspec) ? 20 : 10),
			bi->chanspec);
		printf("\tControl channel: %d\n", bi->ctl_ch);
		printf("\t802.11N Capabilities: ");
		if (dtoh32(bi->nbss_cap) & HT_CAP_40MHZ)
			printf("40Mhz ");
		if (dtoh32(bi->nbss_cap) & HT_CAP_SHORT_GI_20)
			printf("SGI20 ");
		if (dtoh32(bi->nbss_cap) & HT_CAP_SHORT_GI_40)
			printf("SGI40 ");
		if (dtoh32(bi->nbss_cap) & VHT_BI_SGI_80MHZ)
			printf("SGI80 ");
		printf("\n\tSupported MCS : [ ");
		for (mcs_idx = 0; mcs_idx < (MCSSET_LEN * 8); mcs_idx++)
			if (isset(bi->basic_mcs, mcs_idx))
				printf("%d ", mcs_idx);
		printf("]\n");
	}

	printf("\n");
}

void
dump_networks(char *network_buf)
{
	wl_scan_results_t *list = (wl_scan_results_t*)network_buf;
	wl_bss_info_t *bi;
	uint i;

	if (list->count == 0)
		return;
	else if (list->version != WL_BSS_INFO_VERSION &&
	         list->version != LEGACY2_WL_BSS_INFO_VERSION &&
	         list->version != LEGACY_WL_BSS_INFO_VERSION) {
		fprintf(stderr, "Sorry, your driver has bss_info_version %d "
			"but this program supports only version %d.\n",
			list->version, WL_BSS_INFO_VERSION);
		return;
	}

	bi = list->bss_info;
	for (i = 0; i < list->count; i++, bi = (wl_bss_info_t*)((int8*)bi + dtoh32(bi->length))) {
		dump_bss_info(bi);
	}
}

char *
acsd_malloc(int bufsize)
{
	char *buf = NULL;

	buf = malloc(bufsize);

	if (!buf) {
		ACSD_ERROR("failed to allocate %d bytes\n", bufsize);
		perror("acs_malloc failed");
		exit(-1);
	}

	memset(buf, 0, bufsize);
	ACSD_DEBUG("address: 0x%x, size %d\n", (uint32)buf, bufsize);
	return buf;
}

int
acs_safe_get_conf(char *outval, int outval_size, char *name)
{
	char *val;

	if (name == NULL || outval == NULL) {
		if (outval)
			memset(outval, 0, outval_size);
		return -1;
	}

	val = nvram_safe_get(name);
	if (!strcmp(val, ""))
		memset(outval, 0, outval_size);
	else
		snprintf(outval, outval_size, "%s", val);
	return 0;
}

int
swrite(int fd, char *buf, unsigned int size)
{
	int ret = 0, len = 0;

	do {
		errno = 0;
		ret = write(fd, &buf[len], size - len);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			else
				break;
		}
		if (ret > 0)
			len += ret;
	} while (len < size);

	return ((len > 0) ? len:ret);
}

/*
 * Reads size bytes into from a given device.
 * Handles the read failures because of signals.
 */

int
sread(int fd, char *buf, unsigned int size)
{
	int ret = 0, len = 0;

	do {
		errno = 0;
		ret = read(fd, &buf[len], size - len);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			else
				break;
		}
		if (ret > 0)
			len += ret;
	} while ((len < size) && ret);

	return ((len > 0) ? len:ret);
}


void
sleep_ms(const unsigned int ms)
{
	usleep(1000*ms);
}

const char *
acs_ch_score_name(const int ch_score_index)
{
	static const char *score_names[CH_SCORE_MAX] = {
		"BSS", "busy", "interf.", "itf_adj",
		"fcs", "txpower", "bgnoise", "TOTAL",
		"CNS", "ADJ"
	};

	return (ch_score_index < CH_SCORE_MAX) ? score_names[ch_score_index] : "?range";
}

void
acs_dump_score(ch_score_t * score_p)
{
	int i;
	int score, weight, subtotal;

	printf("Channel Score Breakdown:\n");
	printf("Fact\t Score   \t Weight  \t SubTotal\t\n");

	for (i = 0; i < CH_SCORE_MAX; i++) {
		if (i != CH_SCORE_BSS && i != CH_SCORE_CNS)
			continue;
		score = score_p[i].score;
		weight = score_p[i].weight;
		subtotal = score * weight;
		printf("%s\t %8d\t %8d\t %8d\t\n", acs_ch_score_name(i), score, weight, subtotal);
	}
}
