/*
 * WPS ENROLL thread (Linux platform dependent portion)
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_enr_linux_osl.c 383924 2013-02-08 04:14:39Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>

#include <bcmnvram.h>
#include <tutrace.h>
#include <wpserror.h>
#include <wlioctl.h>
#include <proto/ethernet.h>
#include <portability.h>
#include <wps_staeapsm.h>
#include <wps_enrapi.h>
#include <wps_enr.h>
#include <wps_enr_osl.h>
#include <wpscommon.h>
#include <wlutils.h>
#include <shutils.h>
#include <wlif_utils.h>
#include <wps.h>

extern int wps_set_wsec(int ess_id, char *ifname, void *credential, int mode);
extern uint32 wps_eapol_send_data(char *dataBuffer, uint32 dataLen);


/* For testing ... should be set to real peer value (bssid) */
static uint8 peer_mac[6] = {0x00, 0x90, 0xac, 0x6d, 0x09, 0x48};
static uint8 my_mac[6] = {0x00, 0x90, 0xac, 0x6d, 0x09, 0x48};
static int old_auth = 0, old_eap_restric = 0, old_wsec = 0, old_wsec_restric = 0;
static char old_ssid[SIZE_SSID_LENGTH];
static int old_ssidlen = 0;
static char if_name[20] = "";

/*
 * STA enrollee mode only stub functions
 */
#if !defined(BCMWPSAP) && defined(BCMWPSAPSTA)
int
wpsap_open_session(wps_app_t *wps_app, int sc_mode, unsigned char *mac, unsigned char *mac_sta,
	char *osifname, char *enr_nonce, char *priv_key, uint8 *authorizedMacs,
	uint32 authorizedMacs_len, bool b_reqToEnroll, bool b_nwKeyShareable)
{
	return 0;
}

void
wps_upnp_init()
{
	return;
}

void
wps_upnp_deinit()
{
	return;
}

int
wps_upnp_process_msg(char *upnpmsg, int upnpmsg_len)
{
	return WPS_CONT;
}

void
wps_upnp_clear_ssr()
{
	return;
}

void
wps_upnp_device_uuid(unsigned char *uuid)
{
	return;
}

int
wps_upnp_ssr_expire()
{
	return 0;
}

/* ##### Stubs function implement in ap_api.c ##### */
/* Stub functions called by wps_eap.c */
int
wps_getenrState(void *mc_dev)
{
	return 0;
}

int
wps_getregState(void  *mc_dev)
{
	return 0;
}
#endif /* !defined(BCMWPSAP) && defined(BCMWPSAPSTA)  */

static int
wps_wl_set_wep_key(char *prefix, int i)
{
	wl_wsec_key_t key;
	char wl_key[] = "wlXXXXXXXXXX_keyXXXXXXXXXX";
	char *keystr, hex[] = "XX";
	unsigned char *data = key.data;
	int ret = 0;

	memset(&key, 0, sizeof(key));
	key.index = i - 1;
	sprintf(wl_key, "%skey%d", prefix, i);
	keystr = nvram_safe_get(wl_key);

	switch (strlen(keystr)) {
	case WEP1_KEY_SIZE:
	case WEP128_KEY_SIZE:
		key.len = strlen(keystr);
		strcpy((char *)key.data, keystr);
		break;
	case WEP1_KEY_HEX_SIZE:
	case WEP128_KEY_HEX_SIZE:
		key.len = strlen(keystr) / 2;
		while (*keystr) {
			strncpy(hex, keystr, 2);
			*data++ = (unsigned char) strtoul(hex, NULL, 16);
			keystr += 2;
		}
		break;
	default:
		key.len = 0;
		break;
	}

	/* Set current WEP key */
	if (key.len && i == atoi(nvram_safe_get(strcat_r(prefix, "key", wl_key))))
		key.flags = WL_PRIMARY_KEY;

	wl_ioctl(if_name, WLC_SET_KEY, &key, sizeof(key));

	return ret;
}

/*
 * we need to set the ifname before anything else.
 */
int
wps_osl_set_ifname(char *ifname)
{
	/* this is a os name */
	strcpy(if_name, ifname);
	return 0;
}

int
wps_osl_get_mac(uint8 *mac)
{
	struct ifreq ifr;
	int ret = 0;
	int s;

	if (!if_name[0]) {
		printf("Wireless Interface not specified.\n");
		return WPS_ERR_SYSTEM;
	}

	/* Open a raw socket */
	if ((s = socket (AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("socket open failed\n");
		return WPS_ERR_SYSTEM;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);
	if ((ret = ioctl(s, SIOCGIFHWADDR, &ifr)) < 0) {
		printf("ioctl  to get hwaddr failed.\n");
		close(s);
		return WPS_ERR_SYSTEM;
	}

	/* Copy the result back */
	memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);

	close(s);
	return ret;
}

int
wps_osl_init(char *bssid)
{
	int eap_fd = wps_eap_get_handle();
	int ret;
	struct ifreq  ifr;

	if (!if_name[0]) {
		TUTRACE((TUTRACE_ERR, "Wireless Interface not specified.\n"));
		return WPS_ERR_SYSTEM;
	}

	/* Check interface address */
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));
	if ((ret = ioctl(eap_fd, SIOCGIFHWADDR, &ifr)) < 0) {
		TUTRACE((TUTRACE_ERR, "Get interface mac failed\n"));
		return WPS_ERR_SYSTEM;
	}
	/* Copy the result back */
	memcpy(my_mac, ifr.ifr_hwaddr.sa_data, 6);
	TUTRACE((TUTRACE_INFO, "set my_mac %02x:%02x:%02x:%02x:%02x:%02x\n", my_mac[0], my_mac[1],
		my_mac[2], my_mac[3], my_mac[4], my_mac[5]));

	/* record destination bssid */
	if (bssid) {
		memcpy(peer_mac, bssid, 6);
		TUTRACE((TUTRACE_INFO, "set peer_mac %02x:%02x:%02x:%02x:%02x:%02x\n", peer_mac[0],
		peer_mac[1], peer_mac[2], peer_mac[3], peer_mac[4], peer_mac[5]));
	}

	return WPS_SUCCESS;
}

uint32
wpsenr_eapol_validate(char* buf, uint32* len)
{
	struct ether_header *eth;
	uint32 ret, copylen;

	if (*len < ETHER_HDR_LEN)
		return WPS_CONT;

	ret = WPS_SUCCESS;

	/* check is it EAPOL packets */
	eth = (struct ether_header *) buf;
	if (ntohs(eth->ether_type) == ETHER_TYPE_802_1X) {
		uint8 *mac = eth->ether_shost;
		/* make sure we received from our bssid */
		if (memcmp(peer_mac, eth->ether_shost, 6) || memcmp(my_mac, eth->ether_dhost, 6)) {
			TUTRACE((TUTRACE_ERR, "received frame from wrong AP"));

			TUTRACE((TUTRACE_ERR, "ether_shost=%02x:%02x:%02x:%02x:%02x:%02x\n",
				mac[0], mac[1], mac[2], mac[3],	mac[4], mac[5]));

			mac = eth->ether_dhost;
			TUTRACE((TUTRACE_ERR, "ether_dhost=%02x:%02x:%02x:%02x:%02x:%02x\n",
				mac[0], mac[1], mac[2], mac[3],	mac[4], mac[5]));

			TUTRACE((TUTRACE_ERR, "peer_mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
				peer_mac[0], peer_mac[1], peer_mac[2], peer_mac[3], peer_mac[4],
				peer_mac[5]));

			TUTRACE((TUTRACE_ERR, "my_mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
				my_mac[0], my_mac[1], my_mac[2], my_mac[3], my_mac[4], my_mac[5]));

			ret = WPS_CONT;
		}
		else {
			/* remove ether header */
			copylen = *len - ETHER_HDR_LEN;
			memcpy(buf, buf + ETHER_HDR_LEN, copylen);
			*len = copylen;
			TUTRACE((TUTRACE_ERR, "received frame from %02x:%02x:%02x:%02x:%02x:%02x, "
				"len = %d\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
				copylen));
		}
	}
	else {
		ret = WPS_CONT;
	}

	return ret;
}

/* add ether header before send to eapd */
uint32
send_eapol_packet(char *packet, uint32 len)
{
	char sendBuf[WPS_ENR_EAP_DATA_MAX_LENGTH];
	struct ether_header  *eth = (struct ether_header *) sendBuf;
	int sendLen;

	/* add ether header */
	memcpy(&eth->ether_dhost, &peer_mac, ETHER_ADDR_LEN);
	memcpy(&eth->ether_shost, &my_mac, ETHER_ADDR_LEN);
	eth->ether_type = htons(ETHER_TYPE_802_1X);

	/* copy eapol data */
	memcpy(sendBuf + ETHER_HDR_LEN, packet, len);
	sendLen = ETHER_HDR_LEN + len;

	return wps_eapol_send_data(sendBuf, sendLen);
}

unsigned long
get_current_time()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return now.tv_sec;
}

/*
 * Link to wl driver.
 */
int
wps_wl_ioctl(int cmd, void *buf, int len, bool set)
{
	return wl_ioctl(if_name, cmd, buf, len);
}

/* record Enroll states */
void
wpsenr_osl_proc_states(int state)
{
	/*
	  * use wps_proc_status to save process state,
	  * same as wps_ap app
	  */
	wps_setProcessStates(state);

	return;
}

int
wpsenr_osl_set_wsec(int ess_id, void *credential, int mode)
{
	return wps_set_wsec(ess_id, if_name, credential, mode);
}

int
wpsenr_osl_clear_wsec()
{
	int i, val;
	wl_wsec_key_t wep;
	wlc_ssid_t ssid;

	/* backup current security settings */
	wl_ioctl(if_name, WLC_GET_SSID, &ssid, sizeof(ssid));
	old_ssidlen = ssid.SSID_len;
	strncpy(old_ssid, (char*)ssid.SSID, old_ssidlen);
	wl_ioctl(if_name, WLC_GET_WPA_AUTH, &old_auth, sizeof(int));
	wl_ioctl(if_name, WLC_GET_EAP_RESTRICT, &old_eap_restric, sizeof(int));
	wl_ioctl(if_name, WLC_GET_WSEC, &old_wsec, sizeof(int));
	wl_ioctl(if_name, WLC_GET_WEP_RESTRICT, &old_wsec_restric, sizeof(int));

	/* clear current security settings */
	val = 0;
	wl_ioctl(if_name, WLC_SET_WPA_AUTH, &val, sizeof(int));
	val = 0;
	wl_ioctl(if_name, WLC_SET_EAP_RESTRICT, &val, sizeof(int));
	val = 0;
	wl_ioctl(if_name, WLC_SET_WSEC, &val, sizeof(int));
	val = 0;
	wl_ioctl(if_name, WLC_SET_WEP_RESTRICT, &val, sizeof(int));
	/* clobber the default keys */
	for (i = 0; i < DOT11_MAX_DEFAULT_KEYS; i++) {
		memset(&wep, 0, sizeof(wep));
		wep.index = i;
		wl_ioctl(if_name, WLC_SET_KEY, &wep, sizeof(wep));
	}

	return 0;
}

int
wpsenr_osl_restore_wsec()
{
	int i, val;
	wlc_ssid_t ssid;
	char prefix[] = "wlXXXXXXXXXX_";

	/* restore backup settings */
	wl_ioctl(if_name, WLC_SET_WPA_AUTH, &old_auth, sizeof(int));
	wl_ioctl(if_name, WLC_SET_EAP_RESTRICT, &old_eap_restric, sizeof(int));
	wl_ioctl(if_name, WLC_SET_WSEC, &old_wsec, sizeof(int));
	wl_ioctl(if_name, WLC_SET_WEP_RESTRICT, &old_wsec_restric, sizeof(int));

	/* set the default keys from nvram */
	if (old_wsec & WEP_ENABLED) {
		if (osifname_to_nvifname(if_name, prefix, sizeof(prefix)) != 0) {
			printf("wpsenr_osl_restore_wsec:: convert to nvname failed\n");
			return -1;
		}
		strcat(prefix, "_");

		for (i = 1; i <= DOT11_MAX_DEFAULT_KEYS; i++) {
			wps_wl_set_wep_key(prefix, i);
		}
	}

	/* need a down/up for change to take effect */
	wl_ioctl(if_name, WLC_GET_UP, &val, sizeof(val));
	if (val) {
		/* Nuke SSID  */
		ssid.SSID_len = 0;
		ssid.SSID[0] = '\0';
		wl_ioctl(if_name, WLC_SET_SSID, &ssid, sizeof(ssid));
	}
	/* set ssid to bring up */
	ssid.SSID_len = old_ssidlen;
	strncpy((char *)ssid.SSID, old_ssid, old_ssidlen);
	wl_ioctl(if_name, WLC_SET_SSID, &ssid, sizeof(ssid));

	return 0;
}
