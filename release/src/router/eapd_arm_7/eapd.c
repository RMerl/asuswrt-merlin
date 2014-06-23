/*
 * Broadcom EAP dispatcher (EAPD) module main loop
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: eapd.c 472702 2014-04-25 02:14:12Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <proto/ethernet.h>
#include <proto/eapol.h>
#include <proto/eap.h>
#include <wlutils.h>
#include <time.h>
#include <eapd.h>
#include <shutils.h>
#include <wlif_utils.h>
#include <bcmconfig.h>
#include <security_ipc.h>

uint eapd_msg_level =
#ifdef BCMDBG
	EAPD_ERROR_VAL;
#else
	0;
#endif /* BCMDBG */

#define EAPD_WKSP_MAX_EAP_USER_IDENT	32
#define EAPD_WKSP_EAP_USER_NUM			2
#define EAP_TYPE_WSC						EAP_EXPANDED

struct eapd_user_list {
	unsigned char identity[EAPD_WKSP_MAX_EAP_USER_IDENT];
	unsigned int	identity_len;
	unsigned char method;
};

static struct eapd_user_list  eapdispuserlist[EAPD_WKSP_EAP_USER_NUM] = {
	{"WFA-SimpleConfig-Enrollee-1-0", 29, EAP_TYPE_WSC},
	{"WFA-SimpleConfig-Registrar-1-0", 30, EAP_TYPE_WSC}
};


static int eapd_wksp_inited = 0;

/* Static function protype define */
static int sta_init(eapd_wksp_t *nwksp);
static int sta_deinit(eapd_wksp_t *nwksp);
static unsigned char eapd_get_method(unsigned char *user);
static int event_init(eapd_wksp_t *nwksp);
static int event_deinit(eapd_wksp_t *nwksp);
static bool eapd_add_interface(eapd_wksp_t *nwksp, char *ifname, eapd_app_mode_t mode,
                               eapd_cb_t **cbp);
static bool eapd_valid_eapol_start(eapd_wksp_t *nwksp, eapd_brcm_socket_t *from, char *ifname);

#ifdef BCMDBG
/* #define HEXDUMP */
#ifdef  HEXDUMP
extern int isprint(char i);
static void eapd_hexdump_ascii(const char *title, const unsigned char *buf,
	unsigned int len)
{
	int i, llen;
	const unsigned char *pos = buf;
	const int line_len = 16;

	EAPD_PRINT("%s - (data len=%lu):\n", title, (unsigned long) len);
	while (len) {
		llen = len > line_len ? line_len : len;
		EAPD_PRINT("    ");
		for (i = 0; i < llen; i++)
			EAPD_PRINT(" %02x", pos[i]);
		for (i = llen; i < line_len; i++)
			EAPD_PRINT("   ");
		EAPD_PRINT("   ");
		for (i = 0; i < llen; i++) {
			if (isprint(pos[i]))
				EAPD_PRINT("%c", pos[i]);
			else
				EAPD_PRINT("*");
		}
		for (i = llen; i < line_len; i++)
			EAPD_PRINT(" ");
		EAPD_PRINT("\n");
		pos += llen;
		len -= llen;
	}
}

#define HEXDUMP_ASCII(title, buf, len)		eapd_hexdump_ascii(title, buf, len)
#else
#define HEXDUMP_ASCII(title, buf, len)
#endif /* HEXDUMP */
#endif /* BCMDBG */

#ifdef EAPDDUMP
/* dump brcm and preauth socket information */
static void
eapd_dump(eapd_wksp_t *nwksp)
{
	int i, j, flag;
	eapd_brcm_socket_t	*brcmSocket;
	eapd_preauth_socket_t	*preauthSocket;
	eapd_wps_t		*wps;
	eapd_ses_t		*ses;
	eapd_nas_t		*nas;
#ifdef BCM_DCS
	eapd_dcs_t		*dcs;
#endif
#ifdef BCM_BSD
	eapd_bsd_t		*bsd;
#endif
	eapd_cb_t		*cb;
	eapd_sta_t		*sta;
	char eabuf[ETHER_ADDR_STR_LEN], bssidbuf[ETHER_ADDR_STR_LEN];


	if (nwksp == NULL) {
		EAPD_PRINT("Wrong argument...\n");
		return;
	}

	EAPD_PRINT("\n***************************\n");

	EAPD_PRINT("WPS:\n");
	wps = &nwksp->wps;
	cb = wps->cb;
	if (cb) {
		EAPD_PRINT("     wps-monitor appSocket %d for %s", wps->appSocket, cb->ifname);
		/* print each cb ifname */
		cb = cb->next;
		while (cb) {
			EAPD_PRINT(" %s", cb->ifname);
			cb = cb->next;
		}
		EAPD_PRINT("\n");

		EAPD_PRINT("       interested wireless interfaces [%s]\n", wps->ifnames);

		/* bitvec for brcmevent */
		flag = 1;
		for (i = 0; i < WL_EVENTING_MASK_LEN; i++) {
		for (j = 0; j < 8; j++) {
			if (isset(&wps->bitvec[i], j)) {
				if (flag) {
					EAPD_PRINT("       interested event message id [%d",
						(i*8+j));
					flag = 0;
				}
				else {
					EAPD_PRINT(" %d", (i*8+j));
				}
			}
		}
		}
		EAPD_PRINT("]\n");
	}
	cb = wps->cb;
	while (cb) {
		if (cb->brcmSocket) {
			brcmSocket = cb->brcmSocket;
			EAPD_PRINT("         [rfp=0x%x] drvSocket %d on %s for brcm event packet\n",
				(uint) brcmSocket, brcmSocket->drvSocket, brcmSocket->ifname);
		}
		cb = cb->next;
		if (cb)
			EAPD_PRINT("         ----\n");
	}
	EAPD_PRINT("\n");

	EAPD_PRINT("SES:\n");
	ses = &nwksp->ses;
	cb = ses->cb;
	if (cb) {
		EAPD_PRINT("     ses appSocket %d for %s", ses->appSocket, cb->ifname);
		/* print each cb ifname */
		cb = cb->next;
		while (cb) {
			EAPD_PRINT(" %s", cb->ifname);
			cb = cb->next;
		}
		EAPD_PRINT("\n");

		EAPD_PRINT("       interested wireless interfaces [%s]\n", ses->ifnames);

		/* bitvec for brcmevent */
		flag = 1;
		for (i = 0; i < WL_EVENTING_MASK_LEN; i++) {
		for (j = 0; j < 8; j++) {
			if (isset(&ses->bitvec[i], j)) {
				if (flag) {
					EAPD_PRINT("       interested event message id [%d",
						(i*8+j));
					flag = 0;
				}
				else {
					EAPD_PRINT(" %d", (i*8+j));
				}
			}
		}
		}
		EAPD_PRINT("]\n");
	}
	cb = ses->cb;
	while (cb) {
		if (cb->brcmSocket) {
			brcmSocket = cb->brcmSocket;
			EAPD_PRINT("         [rfp=0x%x] drvSocket %d on %s for brcm event packet\n",
				(uint) brcmSocket, brcmSocket->drvSocket, brcmSocket->ifname);
		}
		cb = cb->next;
		if (cb)
			EAPD_PRINT("         ----\n");
	}
	EAPD_PRINT("\n");

	EAPD_PRINT("NAS:\n");
	nas = &nwksp->nas;
	cb = nas->cb;
	if (cb) {
		EAPD_PRINT("     nas appSocket %d for %s", nas->appSocket, cb->ifname);
		/* print each cb ifname */
		cb = cb->next;
		while (cb) {
			EAPD_PRINT(" %s", cb->ifname);
			cb = cb->next;
		}
		EAPD_PRINT("\n");

		EAPD_PRINT("       interested wireless interfaces [%s]\n", nas->ifnames);

		/* bitvec for brcmevent */
		flag = 1;
		for (i = 0; i < WL_EVENTING_MASK_LEN; i++) {
		for (j = 0; j < 8; j++) {
			if (isset(&nas->bitvec[i], j)) {
				if (flag) {
					EAPD_PRINT("       interested event message id [%d",
						(i*8+j));
					flag = 0;
				}
				else {
					EAPD_PRINT(" %d", (i*8+j));
				}
			}
		}
		}
		EAPD_PRINT("]\n");
	}
	cb = nas->cb;
	while (cb) {
		preauthSocket = &cb->preauthSocket;
		if (preauthSocket->drvSocket >= 0) {
			EAPD_PRINT("         [0x%x] drvSocket %d on %s for preauth packet\n",
			        (uint) preauthSocket, preauthSocket->drvSocket,
			        preauthSocket->ifname);
		}
		if (cb->brcmSocket) {
			brcmSocket = cb->brcmSocket;
			EAPD_PRINT("         [rfp=0x%x] drvSocket %d on %s for brcm event packet\n",
				(uint) brcmSocket, brcmSocket->drvSocket, brcmSocket->ifname);
		}
		cb = cb->next;
		if (cb)
			EAPD_PRINT("         ----\n");
	}
	EAPD_PRINT("\n");


#ifdef BCM_DCS
	EAPD_PRINT("DCS:\n");
	dcs = &nwksp->dcs;
	cb = dcs->cb;
	if (cb) {
		EAPD_PRINT("	 dcs appSocket %d for %s", dcs->appSocket, cb->ifname);
		/* print each cb ifname */
		cb = cb->next;
		while (cb) {
			EAPD_PRINT(" %s", cb->ifname);
			cb = cb->next;
		}
		EAPD_PRINT("\n");

		EAPD_PRINT("	   interested wireless interfaces [%s]\n", dcs->ifnames);

		/* bitvec for brcmevent */
		flag = 1;
		for (i = 0; i < WL_EVENTING_MASK_LEN; i++) {
		for (j = 0; j < 8; j++) {
			if (isset(&dcs->bitvec[i], j)) {
				if (flag) {
					EAPD_PRINT("	   interested event message id [%d",
						(i*8+j));
					flag = 0;
				}
				else {
					EAPD_PRINT(" %d", (i*8+j));
				}
			}
		}
		}
		EAPD_PRINT("]\n");
	}
	cb = dcs->cb;
	while (cb) {
		if (cb->brcmSocket) {
			brcmSocket = cb->brcmSocket;
			EAPD_PRINT("	     [rfp=0x%x] drvSocket %d on %s for brcm event packet\n",
				(uint) brcmSocket, brcmSocket->drvSocket, brcmSocket->ifname);
		}
		cb = cb->next;
		if (cb)
			EAPD_PRINT("	     ----\n");
	}
	EAPD_PRINT("\n");
#endif /* BCM_DCS */

#ifdef BCM_BSD
	EAPD_PRINT("BSD:\n");
	bsd = &nwksp->bsd;
	cb = bsd->cb;
	if (cb) {
		EAPD_PRINT("	 bsd appSocket %d for %s", bsd->appSocket, cb->ifname);
		/* print each cb ifname */
		cb = cb->next;
		while (cb) {
			EAPD_PRINT(" %s", cb->ifname);
			cb = cb->next;
		}
		EAPD_PRINT("\n");

		EAPD_PRINT("	   interested wireless interfaces [%s]\n", bsd->ifnames);

		/* bitvec for brcmevent */
		flag = 1;
		for (i = 0; i < WL_EVENTING_MASK_LEN; i++) {
		for (j = 0; j < 8; j++) {
			if (isset(&bsd->bitvec[i], j)) {
				if (flag) {
					EAPD_PRINT("	   interested event message id [%d",
						(i*8+j));
					flag = 0;
				}
				else {
					EAPD_PRINT(" %d", (i*8+j));
				}
			}
		}
		}
		EAPD_PRINT("]\n");
	}
	cb = bsd->cb;
	while (cb) {
		if (cb->brcmSocket) {
			brcmSocket = cb->brcmSocket;
			EAPD_PRINT("	     [rfp=0x%x] drvSocket %d on %s for brcm event packet\n",
				(uint) brcmSocket, brcmSocket->drvSocket, brcmSocket->ifname);
		}
		cb = cb->next;
		if (cb)
			EAPD_PRINT("	     ----\n");
	}
	EAPD_PRINT("\n");
#endif /* BCM_BSD */

	EAPD_PRINT("BRCM (brcm event):\n");
	for (i = 0; i < nwksp->brcmSocketCount; i++) {
		brcmSocket = &nwksp->brcmSocket[i];
		EAPD_PRINT("     [0x%x] [inuseCount=%d] drvSocket %d on %s for "
		        "brcm event packet\n",	(uint) brcmSocket,
		        brcmSocket->inuseCount, brcmSocket->drvSocket,
		        brcmSocket->ifname);
	}

	EAPD_PRINT("\n");

	EAPD_PRINT("Stations Info:\n");
	j = 0;
	for (i = 0; i < EAPD_WKSP_MAX_SUPPLICANTS; i++) {
		for (sta = nwksp->sta_hashed[i]; sta; sta = sta->next) {
			EAPD_PRINT("     [%d] %s from %s[%s]\n", j++,
				ether_etoa((uchar *)&sta->ea, eabuf), sta->ifname,
				ether_etoa((uchar *)&sta->bssid, bssidbuf));
		}
	}
	EAPD_PRINT("***************************\n");
}
#endif /* EAPDDUMP */

#ifdef BCMDBG
void
eapd_wksp_display_usage(void)
{
	EAPD_PRINT("\nUsage: eapd [options]\n\n");
	EAPD_PRINT("\n-wps ifname(s)\n");
	EAPD_PRINT("\n-nas ifname(s)\n");
	EAPD_PRINT("\n-ses ifname\n");
#ifdef BCM_DCS
	EAPD_PRINT("\n-dcs ifname(s)\n");
#endif /* BCM_DCS */
#ifdef BCM_BSD
	EAPD_PRINT("\n-bsd ifname(s)\n");
#endif /* BCM_BSD */
	EAPD_PRINT("\n\n");
};
#endif	/* BCMDBG */

#ifdef EAPD_WKSP_AUTO_CONFIG
int
eapd_wksp_auto_config(eapd_wksp_t *nwksp)
{
	int i;
	char ifnames[256], tmp_ifname[128], tmp_ifnames[128], name[IFNAMSIZ];
	char *next;
	bool needStart = FALSE;
	eapd_cb_t *cb = NULL;

	/* lan */
	for (i = 0; i < EAPD_WKSP_MAX_NO_BRIDGE; i++) {
		memset(tmp_ifname, 0, sizeof(tmp_ifname));
		memset(tmp_ifnames, 0, sizeof(tmp_ifnames));
		if (i == 0) {
			sprintf(tmp_ifname, "lan_ifname");
			sprintf(tmp_ifnames, "lan_ifnames");
		}
		else {
			sprintf(tmp_ifname, "lan%d_ifname", i);
			sprintf(tmp_ifnames, "lan%d_ifnames", i);
		}

		memset(ifnames, 0, sizeof(ifnames));
		memset(name, 0, sizeof(name));
		eapd_safe_get_conf(ifnames, sizeof(ifnames), tmp_ifnames);
		if (!strcmp(ifnames, "")) {
			eapd_safe_get_conf(ifnames, sizeof(ifnames), tmp_ifname);
			if (!strcmp(ifnames, ""))
				continue;
		}
		foreach(name, ifnames, next) {
			if (wps_app_enabled(name))
				needStart |= eapd_add_interface(nwksp, name, EAPD_APP_WPS, &cb);
			if (nas_app_enabled(name))
				needStart |= eapd_add_interface(nwksp, name, EAPD_APP_NAS, &cb);
			if (i == 0 && ses_app_enabled(name)) /* SES only running on lan_ifname */
				needStart |= eapd_add_interface(nwksp, name, EAPD_APP_SES, &cb);

#ifdef BCM_DCS
			if (dcs_app_enabled(name))
				needStart |= eapd_add_interface(nwksp, name, EAPD_APP_DCS, &cb);
#endif /* BCM_DCS */

#ifdef BCM_MEVENT
			if (mevent_app_enabled(name))
				needStart |= eapd_add_interface(nwksp, name, EAPD_APP_MEVENT, &cb);
#endif /* BCM_MEVENT */

#ifdef BCM_BSD
			if (bsd_app_enabled(name))
				needStart |= eapd_add_interface(nwksp, name, EAPD_APP_BSD, &cb);
#endif /* BCM_BSD */
		}
	}

	/* wan */
	memset(ifnames, 0, sizeof(ifnames));
	memset(name, 0, sizeof(name));
	eapd_safe_get_conf(ifnames, sizeof(ifnames), "wan_ifnames");
	foreach(name, ifnames, next) {
		if (wps_app_enabled(name))
			needStart |= eapd_add_interface(nwksp, name, EAPD_APP_WPS, &cb);
		if (nas_app_enabled(name))
			needStart |= eapd_add_interface(nwksp, name, EAPD_APP_NAS, &cb);
#ifdef BCM_DCS
		if (dcs_app_enabled(name))
			needStart |= eapd_add_interface(nwksp, name, EAPD_APP_DCS, &cb);
#endif /* BCM_DCS */
#ifdef BCM_MEVENT
		if (mevent_app_enabled(name))
			needStart |= eapd_add_interface(nwksp, name, EAPD_APP_MEVENT, &cb);
#endif /* BCM_MEVENT */

#ifdef BCM_BSD
		if (bsd_app_enabled(name))
			needStart |= eapd_add_interface(nwksp, name, EAPD_APP_BSD, &cb);
#endif /* BCM_BSD */
	}

	return ((needStart == TRUE) ? 0 : -1);
}
#endif	/* EAPD_WKSP_AUTO_CONFIG */

static bool
eapd_add_interface(eapd_wksp_t *nwksp, char *ifname, eapd_app_mode_t mode, eapd_cb_t **cbp)
{
	int unit;
	char ifnames[256], gmac3_enable[8];
	char os_name[IFNAMSIZ], prefix[8];
	uchar mac[ETHER_ADDR_LEN];
	char *lanifname = NULL;
	eapd_app_t *app;
	eapd_cb_t *cb;
	void (*app_set_eventmask)(eapd_app_t *app) = NULL;

	if ((ifname == NULL) || (cbp == NULL))
		return FALSE;

	*cbp = NULL;

	switch (mode) {
		case EAPD_APP_NAS:
			app = &nwksp->nas;
			app_set_eventmask = (void*) nas_app_set_eventmask;
			break;
		case EAPD_APP_WPS:
			app = &nwksp->wps;
			app_set_eventmask = (void*) wps_app_set_eventmask;
			break;
		case EAPD_APP_SES:
			app = &nwksp->ses;
			if (app->cb)
				return FALSE; /* ses already set */
			app_set_eventmask = (void*) ses_app_set_eventmask;
			break;
#ifdef BCM_DCS
		case EAPD_APP_DCS:
			app = &nwksp->dcs;
			app_set_eventmask = (void*) dcs_app_set_eventmask;
			break;
#endif /* BCM_DCS */
#ifdef BCM_MEVENT
		case EAPD_APP_MEVENT:
			app = &nwksp->mevent;
			app_set_eventmask = (void*) mevent_app_set_eventmask;
			break;
#endif /* BCM_MEVENT */
#ifdef BCM_BSD
		case EAPD_APP_BSD:
			app = &nwksp->bsd;
			app_set_eventmask = (void*) bsd_app_set_eventmask;
			break;
#endif /* BCM_BSD */
		default:
			return FALSE;
	}

	/* verify ifname */
	if (nvifname_to_osifname(ifname, os_name, sizeof(os_name)) < 0)
		return FALSE;
	if (wl_probe(os_name) ||
		wl_ioctl(os_name, WLC_GET_INSTANCE, &unit, sizeof(unit)))
		return FALSE;
	/* convert eth name to wl name */
	if (osifname_to_nvifname(ifname, prefix, sizeof(prefix)) != 0)
		return FALSE;

	/* check ifname in ifnames list */
	if (find_in_list(app->ifnames, prefix))
		return FALSE; /* duplicate */
	/* find ifname in which lan?_ifnames or wan_ifnames */
	(void) wl_hwaddr(os_name, mac);

	/* for psta i/f set lanifname same as ifname, because we know it is
	 * not attached to the bridge.
	 */
	memset(ifnames, 0, sizeof(ifnames));
	eapd_safe_get_conf(ifnames, sizeof(ifnames), "dpsta_ifnames");
	if (wl_wlif_is_psta(os_name) || find_in_list(ifnames, ifname))
		lanifname = ifname;

	/* In 3GMAC mode, each wl interfaces in "fwd_wlandevs" don't attach to the bridge. */
	eapd_safe_get_conf(gmac3_enable, sizeof(gmac3_enable), "gmac3_enable");
	if (!lanifname && atoi(gmac3_enable) == 1) {
		memset(ifnames, 0, sizeof(ifnames));
		eapd_safe_get_conf(ifnames, sizeof(ifnames), "fwd_wlandevs");
		if (find_in_list(ifnames, ifname))
			lanifname = ifname;
	}

	if (!lanifname && (lanifname = get_ifname_by_wlmac(mac, ifname)) == NULL)
		return FALSE;

	EAPD_INFO("lanifname %s\n", lanifname);

	/* find ifname in which lan?_ifnames or wan_ifnames */
	/* check lanifname in cb list */
	cb = app->cb;
	while (cb) {
		if (!strcmp(cb->ifname, lanifname))
			break;
		cb = cb->next;
	}

	if (!cb) {
		/* prepare application structure */
		cb = (eapd_cb_t *)malloc(sizeof(eapd_cb_t));
		if (cb == NULL) {
			EAPD_ERROR("app cb allocate fail...\n");
			return FALSE;
		}
		memset(cb, 0, sizeof(eapd_cb_t));

		/* add cb to the head */
		cb->next = app->cb;
		app->cb = cb;

		/* save ifname */
		strncpy(cb->ifname, lanifname, IFNAMSIZ - 1);

		/* save bcm event bitvec */
		if (app_set_eventmask)
			app_set_eventmask(app);

		EAPD_INFO("add one %s interface cb for %s (%s)\n", prefix, cb->ifname, ifname);
	}

	/* save prefix name to ifnames */
	add_to_list(prefix, app->ifnames, sizeof(app->ifnames));

	/* return the new/existing cb struct pointer */
	*cbp = cb;

	return TRUE;
}

int
eapd_wksp_parse_cmd(int argc, char *argv[], eapd_wksp_t *nwksp)
{
	int i = 1;
	bool needStart = FALSE;
	eapd_app_mode_t current_mode = EAPD_APP_UNKNOW;
	eapd_cb_t *cb = NULL;

	if (nwksp == NULL)
		return -1;

	/* dispatch parse command */
	while (i < argc) {
		if (!strncmp(argv[i], "-wps", 4)) {
			current_mode = EAPD_APP_WPS;
		}
		else if (!strncmp(argv[i], "-nas", 4)) {
			current_mode = EAPD_APP_NAS;
		}
		else if (!strncmp(argv[i], "-ses", 4)) {
			current_mode = EAPD_APP_SES;
		}
#ifdef BCM_DCS
		else if (!strncmp(argv[i], "-dcs", 4)) {
			current_mode = EAPD_APP_DCS;
		}
#endif /* BCM_DCS */
#ifdef BCM_MEVENT
		else if (!strncmp(argv[i], "-mev", 4)) {
			current_mode = EAPD_APP_MEVENT;
		}
#endif /* BCM_MEVENT */
#ifdef BCM_BSD
		else if (!strncmp(argv[i], "-bsd", 4)) {
			current_mode = EAPD_APP_BSD;
		}
#endif /* BCM_BSD */
		else {
			needStart |= eapd_add_interface(nwksp, argv[i], current_mode, &cb);
		}
		i++;
	}
	return ((needStart == TRUE) ? 0 : -1);
}

int
eapd_add_dif(eapd_wksp_t *nwksp, char *ifname)
{
	char os_name[16], name[16];
	eapd_cb_t *cb = NULL;
	int unit;

	EAPD_INFO("Adding dif %s\n", ifname);

	/* Get the os interface name */
	if (nvifname_to_osifname(ifname, os_name, sizeof(os_name)))
		return -1;

	EAPD_INFO("os_name: %s\n", os_name);

	if (wl_probe(os_name) || wl_ioctl(os_name, WLC_GET_INSTANCE, &unit, sizeof(unit)))
		return -1;

	EAPD_INFO("unit: %d\n", unit);

	snprintf(name, sizeof(name), "wl%d", unit);
	if (!nas_app_enabled(name)) {
		/* We get here if the auth is open or shared wep. For wep we want
		 * to configure the encr keys.
		 */
#ifdef linux
		eval("wlconf", ifname, "security");
#endif

		EAPD_INFO("Allow joins from now\n");

		/* If security is not configured then indicate to driver that joins
		 * are allowed for psta.
		 */
		if (wl_iovar_setint(ifname, "psta_allow_join", ON))
			EAPD_ERROR("iovar psta_allow_join failed for %s\n", ifname);

		return -1;
	}

	/* Configure security options for the new interface */
#ifdef linux
	eval("wlconf", ifname, "security");
#endif

	/* Add new nas interface cb */
	if (!eapd_add_interface(nwksp, ifname, EAPD_APP_NAS, &cb) || (cb == NULL)) {
		EAPD_ERROR("New NAS interface add for %s failed, cb %p\n", ifname, cb);
		return -1;
	}

	EAPD_INFO("new nas interface for %s added\n", ifname);

	/* Add bcmevent bitvec for nas */
	if (event_init(nwksp) < 0) {
		EAPD_ERROR("event_init failed\n");
		return -1;
	}

	EAPD_INFO("open new sockets for this interface\n");

	/* Add brcm and preauth sockets to talk to the nas daemon
	 * over this new interface.
	 */
	if (nas_open_dif_sockets(nwksp, cb) < 0) {
		EAPD_ERROR("sockets open failed\n");
		return -1;
	}

	/* After security is configured and sockets are created indicate to
	 * driver that joins can be allowed for psta.
	 */
	if (wl_iovar_setint(ifname, "psta_allow_join", ON)) {
		EAPD_ERROR("iovar psta_allow_join failed for %s\n", ifname);
		return -1;
	}

	EAPD_INFO("Allow joins from now on\n");

	return 0;
}

void
eapd_delete_dif(eapd_wksp_t *nwksp, char *ifname)
{
	eapd_app_t *app;
	eapd_cb_t *prev = NULL, *tmp_cb, *cb;
	char os_name[IFNAMSIZ];

	app = &nwksp->nas;

	EAPD_INFO("Deleting dif %s\n", ifname);

	/* look for ifname in cb list */
	cb = app->cb;
	while (cb) {
		if (!strcmp(cb->ifname, ifname))
			break;
		prev = cb;
		cb = cb->next;
	}

	if (!cb)
		return;

	/* Found the cb struct for this interface */
	nas_close_dif_sockets(nwksp, cb);
	tmp_cb = cb;
	if (prev != NULL)
		prev->next = cb->next;
	else
		app->cb = cb->next;
	free(tmp_cb);

	if (nvifname_to_osifname(ifname, os_name, sizeof(os_name)) < 0)
		return;

	EAPD_INFO("Removing %s from app ifnames\n", os_name);

	remove_from_list(os_name, app->ifnames, sizeof(app->ifnames));

	return;
}

eapd_wksp_t *
eapd_wksp_alloc_workspace(void)
{
	eapd_wksp_t *nwksp = (eapd_wksp_t *)malloc(sizeof(eapd_wksp_t));

	if (!nwksp)
		return NULL;
	memset(nwksp, 0, sizeof(eapd_wksp_t));
	FD_ZERO(&nwksp->fdset);
	nwksp->fdmax = -1;
	nwksp->wps.appSocket = -1;
	nwksp->nas.appSocket = -1;
	nwksp->ses.appSocket = -1;
#ifdef BCM_DCS
	nwksp->dcs.appSocket = -1;
#endif /* BCM_DCS */
#ifdef BCM_MEVENT
	nwksp->mevent.appSocket = -1;
#endif /* BCM_MEVENT */
#ifdef BCM_BSD
	nwksp->bsd.appSocket = -1;
#endif /* BCM_BSD */
	nwksp->difSocket = -1;

	EAPD_INFO("allocated EAPD workspace\n");

	return nwksp;
}

void
eapd_wksp_free_workspace(eapd_wksp_t * nwksp)
{
	if (!nwksp)
		return;
	free(nwksp);
	EAPD_INFO("free EAPD workspace\n");
	return;
}

int
eapd_wksp_init(eapd_wksp_t *nwksp)
{
	int reuse = 1;
	struct sockaddr_in addr;

	if (nwksp == NULL)
		return -1;

	/* initial sta list */
	if (sta_init(nwksp)) {
		EAPD_ERROR("sta_init fail...\n");
		return -1;
	}

	/* initial wps */
	if (wps_app_init(nwksp)) {
		EAPD_ERROR("wps_app_init fail...\n");
		return -1;
	}

	/* initial nas */
	if (nas_app_init(nwksp)) {
		EAPD_ERROR("nas_app_init fail...\n");
		return -1;
	}

	/* initial ses */
	if (ses_app_init(nwksp)) {
		EAPD_ERROR("ses_app_init fail...\n");
		return -1;
	}


#ifdef BCM_DCS
	/* initial dcs */
	if (dcs_app_init(nwksp)) {
		EAPD_ERROR("dcs_app_init fail...\n");
		return -1;
	}
#endif /* BCM_DCS */

#ifdef BCM_MEVENT
	/* initial mevent */
	if (mevent_app_init(nwksp)) {
		EAPD_ERROR("mevent_app_init fail...\n");
		return -1;
	}
#endif /* BCM_MEVENT */

#ifdef BCM_BSD
	/* initial bsd */
	if (bsd_app_init(nwksp)) {
		EAPD_ERROR("bsd_app_init fail...\n");
		return -1;
	}
#endif /* BCM_BSD */

	/* apply bcmevent bitvec */
	if (event_init(nwksp)) {
		EAPD_ERROR("event_init fail...\n");
		return -1;
	}

	/* create a socket to receive dynamic i/f events */
	nwksp->difSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (nwksp->difSocket < 0) {
		EAPD_ERROR("UDP Open failed.\n");
		return -1;
	}
#if defined(__ECOS)
	if (setsockopt(nwksp->difSocket, SOL_SOCKET, SO_REUSEPORT, (char*)&reuse,
		sizeof(reuse)) < 0) {
		EAPD_ERROR("UDP setsockopt failed.\n");
		close(nwksp->difSocket);
		nwksp->difSocket = -1;
		return -1;
	}
#else
	if (setsockopt(nwksp->difSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse,
		sizeof(reuse)) < 0) {
		EAPD_ERROR("UDP setsockopt failed.\n");
		close(nwksp->difSocket);
		nwksp->difSocket = -1;
		return -1;
	}
#endif 

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(EAPD_WKSP_DIF_UDP_PORT);
	if (bind(nwksp->difSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		EAPD_ERROR("UDP Bind failed, close nas appSocket %d\n", nwksp->difSocket);
		close(nwksp->difSocket);
		nwksp->difSocket = -1;
		return -1;
	}
	EAPD_INFO("NAS difSocket %d opened\n", nwksp->difSocket);

	return 0;
}

int
eapd_wksp_deinit(eapd_wksp_t *nwksp)
{
	if (nwksp == NULL) {
		EAPD_ERROR("Wrong argument...\n");
		return -1;
	}

	sta_deinit(nwksp);
	wps_app_deinit(nwksp);
	ses_app_deinit(nwksp);
	nas_app_deinit(nwksp);
#ifdef BCM_DCS
	dcs_app_deinit(nwksp);
#endif /* BCM_DCS */
#ifdef BCM_MEVENT
	mevent_app_deinit(nwksp);
#endif /* BCM_MEVENT */
#ifdef BCM_BSD
	bsd_app_deinit(nwksp);
#endif /* BCM_BSD */
	event_deinit(nwksp);

	/* close  difSocket */
	if (nwksp->difSocket >= 0) {
		EAPD_INFO("close difSocket %d\n", nwksp->difSocket);
		close(nwksp->difSocket);
		nwksp->difSocket = -1;
	}

	return 0;
}

void
eapd_wksp_cleanup(eapd_wksp_t *nwksp)
{
	eapd_wksp_deinit(nwksp);
	eapd_wksp_free_workspace(nwksp);
}

static eapd_cb_t *
eapd_wksp_find_cb(eapd_app_t *app, char *wlifname, uint8 *mac)
{
	eapd_cb_t *cb = NULL;
	char *ifname;
	char value[8], name[IFNAMSIZ], fwd_wlandevs[256];

	if (!app)
		return NULL;

	/* In 3GMAC mode, each wl interfaces in "fwd_wlandevs" don't attach to the bridge. */
	memset(fwd_wlandevs, 0, sizeof(fwd_wlandevs));
	eapd_safe_get_conf(value, sizeof(value), "gmac3_enable");
	if (atoi(value) == 1)
		eapd_safe_get_conf(fwd_wlandevs, sizeof(fwd_wlandevs), "fwd_wlandevs");

	if (wl_wlif_is_psta(wlifname) || find_in_list(fwd_wlandevs, wlifname))
		ifname = wlifname;
	else
		ifname = get_ifname_by_wlmac(mac, wlifname);

	if (ifname) {
		cb = app->cb;
		while (cb) {
			if (!strcmp(ifname, cb->ifname))
				break;
			cb = cb->next;
		}
	}

	if (!cb)
		EAPD_ERROR("No cb found\n");

	return cb;
}

void
eapd_wksp_dispatch(eapd_wksp_t *nwksp)
{
	fd_set fdset;
	struct timeval tv = {1, 0};    /* timed out every second */
	int status, len, i, bytes;
	uint8 *pkt;
	eapd_brcm_socket_t *brcmSocket;
	eapd_preauth_socket_t *preauthSocket;
	eapd_cb_t *cb;
	eapd_wps_t *wps;
	eapd_ses_t *ses;
	eapd_nas_t *nas;
#ifdef BCM_DCS
	eapd_dcs_t *dcs;
#endif /* BCM_DCS */
#ifdef BCM_MEVENT
	eapd_mevent_t *mevent;
#endif /* BCM_MEVENT */
#ifdef BCM_BSD
	eapd_bsd_t *bsd;
#endif /* BCM_BSD */

	FD_ZERO(&nwksp->fdset);
	nwksp->fdmax = -1;

	pkt = &nwksp->packet[IFNAMSIZ];
	len = sizeof(nwksp->packet) - IFNAMSIZ;

	/* add brcm drvSocket */
	for (i = 0; i < nwksp->brcmSocketCount; i++) {
		brcmSocket = &nwksp->brcmSocket[i];
		if (brcmSocket->inuseCount > 0) {
			FD_SET(brcmSocket->drvSocket, &nwksp->fdset);
			if (brcmSocket->drvSocket > nwksp->fdmax)
				nwksp->fdmax = brcmSocket->drvSocket;
		}
	}

	/* add wps appSocket */
	wps = &nwksp->wps;
	if (wps->appSocket >= 0) {
		FD_SET(wps->appSocket, &nwksp->fdset);
		if (wps->appSocket > nwksp->fdmax)
			nwksp->fdmax = wps->appSocket;
	}

	/* add ses appSocket */
	ses = &nwksp->ses;
	if (ses->appSocket >= 0) {
		FD_SET(ses->appSocket, &nwksp->fdset);
		if (ses->appSocket > nwksp->fdmax)
			nwksp->fdmax = ses->appSocket;
	}

	/* add nas appSocket */
	nas = &nwksp->nas;
	if (nas->appSocket >= 0) {
		FD_SET(nas->appSocket, &nwksp->fdset);
		if (nas->appSocket > nwksp->fdmax)
			nwksp->fdmax = nas->appSocket;
	}

	/* add nas preauth drvSocket */
	cb = nas->cb;
	while (cb) {
		preauthSocket = &cb->preauthSocket;
		if (preauthSocket->drvSocket >= 0) {
			FD_SET(preauthSocket->drvSocket, &nwksp->fdset);
			if (preauthSocket->drvSocket > nwksp->fdmax)
				nwksp->fdmax = preauthSocket->drvSocket;
		}
		cb = cb->next;
	}


#ifdef BCM_DCS
	/* add dcs appSocket */
	dcs = &nwksp->dcs;
	if (dcs->appSocket >= 0) {
		FD_SET(dcs->appSocket, &nwksp->fdset);
		if (dcs->appSocket > nwksp->fdmax)
			nwksp->fdmax = dcs->appSocket;
	}
#endif /* BCM_DCS */

#ifdef BCM_MEVENT
	/* add mevent appSocket */
	mevent = &nwksp->mevent;
	if (mevent->appSocket >= 0) {
		FD_SET(mevent->appSocket, &nwksp->fdset);
		if (mevent->appSocket > nwksp->fdmax)
			nwksp->fdmax = mevent->appSocket;
	}
#endif /* BCM_MEVENT */

#ifdef BCM_BSD
	/* add bsd appSocket */
	bsd = &nwksp->bsd;
	if (bsd->appSocket >= 0) {
		FD_SET(bsd->appSocket, &nwksp->fdset);
		if (bsd->appSocket > nwksp->fdmax)
			nwksp->fdmax = bsd->appSocket;
	}
#endif /* BCM_BSD */

	/* add difSocket */
	if (nwksp->difSocket >= 0) {
		FD_SET(nwksp->difSocket, &nwksp->fdset);
		if (nwksp->difSocket > nwksp->fdmax)
			nwksp->fdmax = nwksp->difSocket;
	}

	if (nwksp->fdmax == -1) {
		/* do shutdown procedure */
		nwksp->flags  = EAPD_WKSP_FLAG_SHUTDOWN;
		EAPD_ERROR("There is no any sockets in the fd set, shutdown...\n");
		return;
	}

	fdset = nwksp->fdset;
	status = select(nwksp->fdmax+1, &fdset, NULL, NULL, &tv);
	if (status > 0) {
		/* check brcm drvSocket */
		for (i = 0; i < nwksp->brcmSocketCount; i++) {
			brcmSocket = &nwksp->brcmSocket[i];
			if (brcmSocket->inuseCount > 0 &&
			     FD_ISSET(brcmSocket->drvSocket, &fdset)) {
				/*
				 * Use eapd_message_read read to receive BRCM packets,
				 * this change allows the drvSocket is a file descriptor, and
				 * handle any other local io differences
				 */
				bytes = eapd_message_read(brcmSocket->drvSocket, pkt, len);
				if (bytes > 0)  {
					/* call brcm recv handler */
					eapd_brcm_recv_handler(nwksp, brcmSocket, pkt, &bytes);
				}
			}
		}

		/* check wps appSocket */
		if (wps->appSocket >= 0 &&
		     FD_ISSET(wps->appSocket, &fdset)) {
			bytes = recv(wps->appSocket, pkt, len, 0);
			if (bytes > ETHER_HDR_LEN) {
				char *ifname = (char *) pkt;
				struct ether_header *eth;
#ifdef BCMDBG
				HEXDUMP_ASCII("EAPD:: data from WPS app", pkt, bytes);
#endif
				/* ether header */
				eth = (struct ether_header*)(ifname + IFNAMSIZ);
				bytes -= IFNAMSIZ;

				cb = eapd_wksp_find_cb(wps, ifname, eth->ether_shost);
				/* send message data out. */
				if (cb)
					wps_app_recv_handler(nwksp, ifname, cb, (uint8 *)eth,
						&bytes, (struct ether_addr *)&eth->ether_shost);
			}
		}

		/* check ses appSocket */
		if (ses->appSocket >= 0 &&
		     FD_ISSET(ses->appSocket, &fdset)) {
			bytes = recv(ses->appSocket, pkt, len, 0);
			if (bytes > ETHER_HDR_LEN) {
				char *ifname = (char *) pkt;
				struct ether_header *eth;
#ifdef BCMDBG
				HEXDUMP_ASCII("EAPD:: data from SES app", pkt, bytes);
#endif

				/* ether header */
				eth = (struct ether_header*)(ifname + IFNAMSIZ);
				bytes -= IFNAMSIZ;

				cb = eapd_wksp_find_cb(ses, ifname, eth->ether_shost);
				/* send message data out. */
				if (cb)
					ses_app_recv_handler(nwksp, ifname, cb, (uint8 *)eth,
						&bytes);
			}
		}

		/* check difSocket */
		if (nwksp->difSocket >= 0 &&
		     FD_ISSET(nwksp->difSocket, &fdset)) {
			bytes = recv(nwksp->difSocket, pkt, len, 0);
			if (bytes > ETHER_HDR_LEN) {
				char *ifname = (char *) pkt;
				uint32 event;

#ifdef BCMDBG
				HEXDUMP_ASCII("EAPD:: data from hotplug", pkt, bytes);
#endif
				event = *(uint32 *)(ifname + IFNAMSIZ);
				if (event == 0)
					eapd_add_dif(nwksp, ifname);
				else if (event == 1)
					eapd_delete_dif(nwksp, ifname);
			}
		}

		/* check nas appSocket */
		if (nas->appSocket >= 0 &&
		     FD_ISSET(nas->appSocket, &fdset)) {
			bytes = recv(nas->appSocket, pkt, len, 0);
			if (bytes > ETHER_HDR_LEN) {
				char *ifname = (char *) pkt;
				struct ether_header *eth;

#ifdef BCMDBG
				HEXDUMP_ASCII("EAPD:: data from NAS app", pkt, bytes);
#endif
				/* ether header */
				eth = (struct ether_header*)(ifname + IFNAMSIZ);
				bytes -= IFNAMSIZ;

				cb = eapd_wksp_find_cb(nas, ifname, eth->ether_shost);
				/* send message data out. */
				if (cb)
					nas_app_recv_handler(nwksp, ifname, cb, (uint8 *)eth,
						&bytes);
			}
		}

		/* check preauth drvSocket */
		cb = nas->cb;
		while (cb) {
			preauthSocket = &cb->preauthSocket;
			if (preauthSocket->drvSocket >= 0 &&
			     FD_ISSET(preauthSocket->drvSocket, &fdset)) {
				/*
				 * Use eapd_message_read instead to receive PREAUTH packets,
				 * this change allows the drvSocket is a file descriptor
				 */
				bytes = eapd_message_read(preauthSocket->drvSocket, pkt, len);
				if (bytes > 0) {
#ifdef BCMDBG
					HEXDUMP_ASCII("EAPD:: data from PREAUTH Driver",
						pkt, bytes);
#endif
					/* call preauth recv handler */
					eapd_preauth_recv_handler(nwksp, cb->ifname,
						pkt, &bytes);
				}
			}

			cb = cb->next;
		}


#ifdef BCM_DCS
		/* check dcs appSocket */
		if (dcs->appSocket >= 0 &&
		     FD_ISSET(dcs->appSocket, &fdset)) {
			bytes = recv(dcs->appSocket, pkt, len, 0);
			if (bytes > ETHER_HDR_LEN) {
				char *ifname = (char *) pkt;
				struct ether_header *eth;
#ifdef BCMDBG
				HEXDUMP_ASCII("EAPD:: data from DCS app", pkt, bytes);
#endif
				/* ether header */
				eth = (struct ether_header*)(ifname + IFNAMSIZ);
				bytes -= IFNAMSIZ;

				cb = eapd_wksp_find_cb(dcs, ifname, eth->ether_shost);
				/* send message data out. */
				if (cb)
					dcs_app_recv_handler(nwksp, cb, (uint8 *)eth, &bytes);
			}
		}
#endif /* BCM_DCS */

#ifdef BCM_MEVENT
		/* check mevent appSocket */
		if (mevent->appSocket >= 0 &&
		     FD_ISSET(mevent->appSocket, &fdset)) {
			bytes = recv(mevent->appSocket, pkt, len, 0);
			if (bytes > ETHER_HDR_LEN) {
				char *ifname = (char *) pkt;
				struct ether_header *eth;
#ifdef BCMDBG
				HEXDUMP_ASCII("EAPD:: data from MEVENT app", pkt, bytes);
#endif
				/* ether header */
				eth = (struct ether_header*)(ifname + IFNAMSIZ);
				bytes -= IFNAMSIZ;

				cb = eapd_wksp_find_cb(mevent, ifname, eth->ether_shost);
				/* send message data out. */
				if (cb)
					mevent_app_recv_handler(nwksp, cb, (uint8 *)eth, &bytes);
			}
		}
#endif /* BCM_MEVENT */

#ifdef BCM_BSD
		/* check bsd appSocket */
		if (bsd->appSocket >= 0 &&
		     FD_ISSET(bsd->appSocket, &fdset)) {
			bytes = recv(bsd->appSocket, pkt, len, 0);
			if (bytes > ETHER_HDR_LEN) {
				char *ifname = (char *) pkt;
				struct ether_header *eth;
#ifdef BCMDBG
				HEXDUMP_ASCII("EAPD:: data from DCS app", pkt, bytes);
#endif
				/* ether header */
				eth = (struct ether_header*)(ifname + IFNAMSIZ);
				bytes -= IFNAMSIZ;

				cb = eapd_wksp_find_cb(bsd, ifname, eth->ether_shost);
				/* send message data out. */
				if (cb)
					bsd_app_recv_handler(nwksp, cb, (uint8 *)eth, &bytes);
			}
		}
#endif /* BCM_BSD */

	}

	return;
}

int
eapd_wksp_main_loop(eapd_wksp_t *nwksp)
{
	int ret;

	/* init eapd */
	ret = eapd_wksp_init(nwksp);

	/* eapd wksp initialization finished */
	eapd_wksp_inited = 1;
	if (ret) {
		EAPD_ERROR("Unable to initialize EAPD. Quitting...\n");
		eapd_wksp_cleanup(nwksp);
		return -1;
	}

#if !defined(DEBUG) && !defined(__ECOS)
	/* Daemonize */
	if (daemon(1, 1) == -1) {
		eapd_wksp_cleanup(nwksp);
		perror("eapd_wksp_main_loop: daemon\n");
		exit(errno);
	}
#endif

	while (1) {
		/* check user command for shutdown */
		if (nwksp->flags & EAPD_WKSP_FLAG_SHUTDOWN) {
			eapd_wksp_cleanup(nwksp);
			EAPD_INFO("NAS shutdown...\n");
			return 0;
		}

#ifdef EAPDDUMP
		/* check dump */
		if (nwksp->flags & EAPD_WKSP_FLAG_DUMP) {
			eapd_dump(nwksp);
			nwksp->flags &= ~EAPD_WKSP_FLAG_DUMP;
		}
#endif

		/* do packets dispatch */
		eapd_wksp_dispatch(nwksp);
	}
}

static int
eapd_brcm_dispatch(eapd_wksp_t *nwksp, eapd_brcm_socket_t *from, uint8 *pData, int Len)
{
	if (nwksp == NULL || pData == NULL) {
		EAPD_ERROR("Wrong arguments...\n");
		return -1;
	}


	/* check nas application */
	nas_app_handle_event(nwksp, pData, Len, from->ifname);

	/* check wps application */
	wps_app_handle_event(nwksp, pData, Len, from->ifname);

	/* check ses application */
	ses_app_handle_event(nwksp, pData, Len, from->ifname);


#ifdef BCM_DCS
	/* check dcs application */
	dcs_app_handle_event(nwksp, pData, Len, from->ifname);
#endif /* BCM_DCS */

#ifdef BCM_MEVENT
	/* check mevent application */
	mevent_app_handle_event(nwksp, pData, Len, from->ifname);
#endif /* BCM_MEVENT */

#ifdef BCM_BSD
	/* check bsd application */
	bsd_app_handle_event(nwksp, pData, Len, from->ifname);
#endif /* BCM_BSD */

	return 0;
}


/*
 * look for a raw brcm event socket connected to
 * interface "ifname".
 */
eapd_brcm_socket_t*
eapd_find_brcm(eapd_wksp_t *nwksp, char *ifname)
{
	int i, brcmSocketCount;
	eapd_brcm_socket_t *brcmSocket;

	if (nwksp == NULL || ifname == NULL) {
		EAPD_ERROR("Wrong arguments...\n");
		return NULL;
	}

	brcmSocketCount = nwksp->brcmSocketCount;
	for (i = 0; i < brcmSocketCount; i++) {
		brcmSocket = &nwksp->brcmSocket[i];
		if (brcmSocket->inuseCount > 0) {
			if (!strcmp(brcmSocket->ifname, ifname)) {
				EAPD_INFO("Find brcm interface %s\n", ifname);
				return brcmSocket;
			}
		}
	}


	EAPD_INFO("Not found brcm interface %s\n", ifname);
	return NULL;
}


eapd_brcm_socket_t*
eapd_add_brcm(eapd_wksp_t *nwksp, char *ifname)
{
	int i;
	eapd_brcm_socket_t *brcmSocket;

	if (nwksp == NULL || ifname == NULL) {
		EAPD_ERROR("Wrong arguments...\n");
		return NULL;
	}

	brcmSocket = eapd_find_brcm(nwksp, ifname);
	if (brcmSocket) {
		brcmSocket->inuseCount++;
		return brcmSocket;
	}


	/* not found, find inuseCount is zero first */
	for (i = 0; i < nwksp->brcmSocketCount; i++) {
		brcmSocket = &nwksp->brcmSocket[i];
		if (brcmSocket->inuseCount == 0)
			break;
	}


	 /* not found inuseCount is zero */
	if (i == nwksp->brcmSocketCount) {
		if (nwksp->brcmSocketCount >= EAPD_WKSP_MAX_NO_BRCM) {
			EAPD_ERROR("brcmSocket number is not enough, max %d\n",
				EAPD_WKSP_MAX_NO_BRCM);
			return NULL;
		}
		nwksp->brcmSocketCount++;
	}

	brcmSocket = &nwksp->brcmSocket[i];

	memset(brcmSocket, 0, sizeof(eapd_brcm_socket_t));
	strncpy(brcmSocket->ifname, ifname, IFNAMSIZ -1);
	if (eapd_brcm_open(nwksp, brcmSocket) < 0) {
		EAPD_ERROR("open brcm socket on %s error!!\n", ifname);
		return NULL;
	}

	return brcmSocket;
}

int
eapd_del_brcm(eapd_wksp_t *nwksp, eapd_brcm_socket_t *sock)
{
	int i;
	eapd_brcm_socket_t *brcmSocket;

	if (nwksp == NULL || sock == NULL) {
		EAPD_ERROR("Wrong arguments...\n");
		return -1;
	}

	/* find it first. */
	for (i = 0; i < nwksp->brcmSocketCount; i++) {
		brcmSocket = &nwksp->brcmSocket[i];
		if (brcmSocket->inuseCount > 0 &&
		     !strcmp(brcmSocket->ifname, sock->ifname)) {
			EAPD_INFO("Find brcm interface %s, brcmSocket->inuseCount=%d\n",
				sock->ifname, brcmSocket->inuseCount);
			brcmSocket->inuseCount--;
			if (brcmSocket->inuseCount == 0) {
				EAPD_INFO("close brcm drvSocket %d\n", brcmSocket->drvSocket);
				eapd_brcm_close(brcmSocket->drvSocket);
				brcmSocket->drvSocket = -1;
			}
			return 0;
		}
	}

	/* not found */
	EAPD_INFO("Not found brcm interface to del %s\n", sock->ifname);
	return -1;
}

/* dispatch EAPOL packet from brcmevent. */
static void
eapd_eapol_dispatch(eapd_wksp_t *nwksp, eapd_brcm_socket_t *from, uint8 *pData, int *pLen)
{
	eapd_sta_t *sta;
	eapol_header_t *eapol;
	eap_header_t *eap;
	char *ifname;

	if (!nwksp || !from || !pData) {
		EAPD_ERROR("Wrong argument...\n");
		return;
	}

#ifdef BCMDBG
	HEXDUMP_ASCII("EAPD:: eapol data from BRCM driver", pData, *pLen);
#endif

	/* incoming ifname */
	ifname = (char *) pData;

	/* eapol header */
	eapol = (eapol_header_t *)(ifname + IFNAMSIZ);
	eap = (eap_header_t *) eapol->body;

#ifdef NAS_GTK_PER_STA
	if (ETHER_ISNULLADDR(eapol->eth.ether_shost) && (eapol->type == 0xFF)) {
		nas_app_sendup(nwksp, pData, *pLen, from->ifname);
		return;
	}
#endif /* NAS_GTK_PER_STA */

	sta = sta_lookup(nwksp, (struct ether_addr *) eapol->eth.ether_shost,
		(struct ether_addr *) eapol->eth.ether_dhost, ifname, EAPD_SEARCH_ENTER);
	if (!sta) {
		EAPD_ERROR("no STA struct available\n");
		return;
	}

	if (eapol->version < sta->eapol_version) {
		EAPD_ERROR("EAPOL version %d packet received, current version is %d\n",
		            eapol->version, sta->eapol_version);
	}

	EAPD_INFO("sta->pae_state=%d sta->mode=%d\n", sta->pae_state, sta->mode);

	/* Pass EAP_RESPONSE to WPS module. */
	if (sta->pae_state >= EAPD_IDENTITY &&
	    (sta->mode == EAPD_STA_MODE_WPS || sta->mode == EAPD_STA_MODE_WPS_ENR) &&
	    eapol->type == EAP_PACKET &&
	    (eap->type == EAP_EXPANDED || eap->code == EAP_FAILURE)) {
		switch (eap->code) {
		case EAP_REQUEST:
		case EAP_FAILURE:
			/* in case of router running enr */
			EAPD_INFO("EAP %s Packet received...\n",
				eap->code == EAP_REQUEST ? "Request" : "Failure");

			/* Send to wps-monitor */
			if (sta->mode == EAPD_STA_MODE_WPS_ENR) {
				/* monitor eapol packet */
				if (eap->code == EAP_FAILURE || eap->code == EAP_SUCCESS) {
					sta_remove(nwksp, sta);
				}
#ifdef BCMDBG
				HEXDUMP_ASCII("Receive, EAP Request", pData, *pLen);
#endif
				wps_app_monitor_sendup(nwksp, pData, *pLen, from->ifname);
			}
			else
				return;
			break;

		case EAP_RESPONSE:
			EAPD_INFO("EAP Response Packet received...\n");

			/* Send to wps-monitor */
			if (sta->mode == EAPD_STA_MODE_WPS)
				wps_app_monitor_sendup(nwksp, pData, *pLen, from->ifname);
			else
				return;
			break;

		default:
				/* Do nothing */
			break;
		}

		return;
	}

	/* Check SES IDENTITY */
	if ((eapol->type == EAP_PACKET) &&
	    (eap->type == EAP_EXPANDED) &&
	    (eap->data[2] == (BCM_SMI_ID & 0xff)) &&
	    (eap->data[1] == ((BCM_SMI_ID & 0xff00) >> 8)) &&
	    (eap->data[0] == ((BCM_SMI_ID & 0xff000) >> 16))) {
		/* Send to SES module. */
		ses_app_sendup(nwksp, pData, *pLen, from->ifname);

		sta->mode = EAPD_STA_MODE_SES;
		sta->pae_state = EAPD_IDENTITY;

		return;
	}

	/* We handle
	  * 1. Receive EAPOL-Start and send EAP_REQUEST for EAP_IDENTITY
	  * 2. Receive EAP_IDENTITY and pass it to NAS if not a WPS IDENTITY,
	  *     NAS need to record identity.
	  * 3. Pass NAS other EAPOL type
	  */
	switch (eapol->type) {
	case EAP_PACKET:
		if (ntohs(eapol->length) >= (EAP_HEADER_LEN + 1)) {
			EAPD_INFO("STA State=%d EAP Packet Type=%d Id=%d code=%d\n",
				sta->pae_state, eap->type, eap->id, eap->code);

			switch (eap->type) {
			case EAP_IDENTITY:
				EAPD_INFO("Receive , eap code=%d, id = %d, length=%d, type=%d\n",
					eap->code, eap->id, ntohs(eap->length), eap->type);
#ifdef BCMDBG
				HEXDUMP_ASCII("Receive, EAP Identity", eap->data,
					ntohs(eap->length) - EAP_HEADER_LEN - 1);
#endif
				/* Store which interface sta come from */
				memcpy(&sta->bssid, &eapol->eth.ether_dhost, ETHER_ADDR_LEN);
				memcpy(&sta->ifname, ifname, IFNAMSIZ);

				if (eapd_get_method(eap->data) == EAP_TYPE_WSC) {
					EAPD_INFO("This is a wps eap identity response!\n");

					/* Send to WPS-Monitor module. */
					wps_app_monitor_sendup(nwksp, pData, *pLen, from->ifname);

					sta->mode = EAPD_STA_MODE_WPS;
				} else if (sta->mode == EAPD_STA_MODE_WPS_ENR) {
					EAPD_INFO("This is a wps eap identity request!\n");

					/* Send to WPS-Monitor module. */
					wps_app_monitor_sendup(nwksp, pData, *pLen, from->ifname);

					/*
					  * sta mode EAPD_STA_MODE_WPS_ENR set in
					  * wps_app_recv_handler, when enr initial send
					  * EAPOL-START.
					  */
				}
				else {
					/* Send to NAS module. */
					nas_app_sendup(nwksp, pData, *pLen, from->ifname);
					sta->mode = EAPD_STA_MODE_NAS;
				}
				sta->pae_state = EAPD_IDENTITY;
				break;
/*
#if 1
*/
#ifdef __CONFIG_WFI__
			case EAP_NAK:
				if (sta->mode == EAPD_STA_MODE_UNKNOW &&
				    sta->pae_state == EAPD_INITIALIZE &&
				    eap->code == EAP_RESPONSE) {
					/*
					 * EAP-RESPONSE-NAK for WFI reject.
					 * Send to WPS-Monitor module.
					 */
					wps_app_monitor_sendup(nwksp, pData, *pLen, from->ifname);
					break;
				}
				/* Fall through */
#endif /* __CONFIG_WFI__ */
/* #endif */

			default:
				/* Send to NAS module. */
				nas_app_sendup(nwksp, pData, *pLen, from->ifname);
				sta->mode = EAPD_STA_MODE_UNKNOW;
				break;
			}
		}
		break;

	case EAPOL_START:
		EAPD_INFO("EAPOL Start\n");

		sta->pae_id = 0;
		memcpy(&sta->bssid, &eapol->eth.ether_dhost, ETHER_ADDR_LEN);
		memcpy(&sta->ifname, ifname, IFNAMSIZ);

		/* check EAPD interface application capability first */
		if (!eapd_valid_eapol_start(nwksp, from, ifname))
			break;

		/* break out if STA is only PSK */
		if (sta->mode == EAPD_STA_MODE_NAS_PSK)
			break;

		eapd_eapol_canned_send(nwksp, from, sta, EAP_REQUEST, EAP_IDENTITY);
		sta->mode = EAPD_STA_MODE_UNKNOW;
		break;

	case EAPOL_LOGOFF:
		EAPD_INFO("EAPOL Logoff sta mode %d\n", sta->mode);

		/* Send EAPOL_LOGOFF to application. */
		switch (sta->mode) {
		case EAPD_STA_MODE_WPS:
		case EAPD_STA_MODE_WPS_ENR:
			wps_app_monitor_sendup(nwksp, pData, *pLen, from->ifname);
			break;

		case EAPD_STA_MODE_NAS:
		case EAPD_STA_MODE_NAS_PSK:
			nas_app_sendup(nwksp, pData, *pLen, from->ifname);
			break;

		default:
			EAPD_INFO("Ignore EAPOL Logoff\n");
			break;
		}
		break;

	default:
		EAPD_INFO("unknown EAPOL type %d\n", eapol->type);

		/* Send to NAS module. */
		nas_app_sendup(nwksp, pData, *pLen, from->ifname);
		break;
	}

	return;
}


/* Handle brcmevent type packet from any interface */
void
eapd_brcm_recv_handler(eapd_wksp_t *nwksp, eapd_brcm_socket_t *from, uint8 *pData, int *pLen)
{
	eapol_header_t *eapol;
	bcm_event_t *dpkt = (bcm_event_t *)pData;
	unsigned int len;
	char *ifname, ifname_tmp[BCM_MSG_IFNAME_MAX];

	if (nwksp == NULL || from == NULL || dpkt == NULL) {
		EAPD_ERROR("Wrong argument...\n");
		return;
	}

	switch (ntohs(dpkt->bcm_hdr.usr_subtype)) {
	case BCMILCP_BCM_SUBTYPE_EVENT:
		switch (ntohl(dpkt->event.event_type)) {
		case WLC_E_EAPOL_MSG:
			EAPD_INFO("%s: recved wl eapol packet in brcmevent bytes: %d\n",
			       dpkt->event.ifname, *pLen);

			len = ntohl(dpkt->event.datalen);

			/* Reconstructs a EAPOL packet from the received packet	*/
			/* Point the EAPOL packet to the start of the data portion of the
			  * received packet minus some space to add the ethernet header to the
			  * EAPOL packet
			  */
			eapol = (eapol_header_t *)((char *)(dpkt + 1) - ETHER_HDR_LEN);
			ifname = (char *)eapol - BCM_MSG_IFNAME_MAX;

			/* Save incoming interface name to temp_ifname */
			bcopy(dpkt->event.ifname, ifname_tmp, BCM_MSG_IFNAME_MAX);

			/* Now move the received packet's ethernet header to the head of the
			  * EAPOL packet
			  */
			memmove((char *)eapol, (char *)pData, ETHER_HDR_LEN);

			/* Set the EAPOL packet type correctly */
			eapol->eth.ether_type = htons(ETHER_TYPE_802_1X);

			/* The correct shost address was encapsulated to the event struct by the
			  * driver, copy it to the EAPOL packet's ethernet header
			  */
			bcopy(dpkt->event.addr.octet, eapol->eth.ether_shost, ETHER_ADDR_LEN);

			/* Save incoming interface name */
			bcopy(ifname_tmp, ifname, BCM_MSG_IFNAME_MAX);

			len = len + ETHER_HDR_LEN + BCM_MSG_IFNAME_MAX;
			eapd_eapol_dispatch(nwksp, from, (void *)ifname, (int *)&len);
			return;

		default:
			/* dispatch brcnevent to wps, ses(?), nas, wapid */
			EAPD_INFO("%s: dispatching brcmevent %d to nas/wps/ses...\n",
			          dpkt->event.ifname, ntohl(dpkt->event.event_type));
			eapd_brcm_dispatch(nwksp, from, pData, *pLen);
			return;
		}
		break;

	default: /* not a NAS supported message so return an error */
		EAPD_ERROR("%s: ERROR: recved unknown packet interface subtype "
		        "0x%x bytes: %d\n", dpkt->event.ifname,
		        ntohs(dpkt->bcm_hdr.usr_subtype), *pLen);
		return;
	}

	return;
}

/* Handle PreAuth packet from any interface for nas
 * Receive it and just pass to nas
 */
void
eapd_preauth_recv_handler(eapd_wksp_t *nwksp, char *from, uint8 *pData, int *pLen)
{
	if (!nwksp || !from || !pData) {
		EAPD_ERROR("Wrong argument...\n");
		return;
	}

	/* prepend ifname,  we reserved IFNAMSIZ length already */
	pData -= IFNAMSIZ;
	*pLen += IFNAMSIZ;
	memcpy(pData, from, IFNAMSIZ);

	/* Do not parse it right now, just pass to NAS */
	nas_app_sendup(nwksp, pData, *pLen, from);
	return;
}

static unsigned char
eapd_get_method(unsigned char *user)
{
	unsigned char ret = 0;
	int i;

	for (i = 0; i < EAPD_WKSP_EAP_USER_NUM; i++) {
		if (memcmp(user, eapdispuserlist[i].identity,
			eapdispuserlist[i].identity_len) == 0) {
			ret = eapdispuserlist[i].method;
			break;
		}
	}

	return ret;
}


static int
sta_init(eapd_wksp_t *nwksp)
{
	return 0;
}

static int
sta_deinit(eapd_wksp_t *nwksp)
{
	return 0;
}

void
sta_remove(eapd_wksp_t *nwksp, eapd_sta_t *sta)
{
	eapd_sta_t *sta_list;
	uint hash;

	EAPD_INFO("sta %s remove\n", ether_etoa((uchar *)&sta->ea, eabuf));

	if (sta == NULL) {
		EAPD_ERROR("called with NULL STA ponter\n");
		return;
	}

	/* Remove this one from its hashed list. */
	hash = EAPD_PAE_HASH(&sta->ea);
	sta_list = nwksp->sta_hashed[hash];

	if (sta_list == sta) {
		/* It was the head, so its next is the new head. */
		nwksp->sta_hashed[hash] = sta->next;

	}
	else {
		/* Find the one that points to it and change the pointer. */
		while ((sta_list != NULL) && (sta_list->next != sta))
			sta_list = sta_list->next;
		if (sta_list == NULL) {
			EAPD_INFO("sta %s not in hash list\n",
				ether_etoa((uchar *)&sta->ea, eabuf));
		}
		else {
			sta_list->next = sta->next;
		}
	}
	sta->used = FALSE;
	return;
}

/*
 * Search for or create a STA struct.
 * If `mode' is not EAPD_SEARCH_ENTER, do not create it when one is not found.
 * NOTE: bssid_ea is a spoof mac.
 */
eapd_sta_t*
sta_lookup(eapd_wksp_t *nwksp, struct ether_addr *sta_ea, struct ether_addr *bssid_ea,
               char *ifname, eapd_lookup_mode_t mode)
{
	unsigned int hash;
	eapd_sta_t *sta;
	time_t now, oldest;

	EAPD_INFO("lookup for sta %s\n", ether_etoa((uchar *)sta_ea, eabuf));

	hash = EAPD_PAE_HASH(sta_ea);

	/* Search for entry in the hash table */
	for (sta = nwksp->sta_hashed[hash];
	     sta && memcmp(&sta->ea, sta_ea, ETHER_ADDR_LEN);
	     sta = sta->next);

	/* One second resolution is probably good enough. */
	(void) time(&now);

	/* Not found in sta_hashed, allocate a new entry */
	if (!sta) {
		int i, old_idx = -1;

		/* Don't make an unwanted entry. */
		if (mode == EAPD_SEARCH_ONLY)
			return NULL;

		oldest = now;
		for (i = 0; i < EAPD_WKSP_MAX_SUPPLICANTS; i++) {
			if (!nwksp->sta[i].used)
				break;
			else if (nwksp->sta[i].last_use < oldest) {
				oldest = nwksp->sta[i].last_use;
				old_idx = i;
			}
		}

		if (i < EAPD_WKSP_MAX_SUPPLICANTS) {
			sta = &nwksp->sta[i];
		}
		else if (old_idx == -1) {
			/* Full up with all the same timestamp? Can
			 * this really happen?
			 */
			return NULL;
		}
		else {
			/* Didn't find one unused, so age out LRU not wps entry. */
			sta = &nwksp->sta[old_idx];
			sta_remove(nwksp, sta);
		}

		/* Initialize entry */
		memset(sta, 0, (sizeof(eapd_sta_t)));
		memcpy(&sta->ea, sta_ea, ETHER_ADDR_LEN);
		memcpy(&sta->bssid, bssid_ea, ETHER_ADDR_LEN);
		memcpy(&sta->ifname, ifname, IFNAMSIZ);
		sta->used = TRUE;
		sta->eapol_version = WPA_EAPOL_VERSION;

		/* Initial STA state */
		sta->pae_state = EAPD_INITIALIZE;

		/* initial mode */
		sta->mode = EAPD_STA_MODE_UNKNOW;
		EAPD_INFO("Create eapd sta %s\n", ether_etoa((uchar *)&sta->ea, eabuf));

		/* Add entry to the cache */
		sta->next = nwksp->sta_hashed[hash];
		nwksp->sta_hashed[hash] = sta;
	}
	else if (bssid_ea && !wl_wlif_is_psta(sta->ifname) && !wl_wlif_is_psta(ifname) &&
		(memcmp(&sta->bssid, bssid_ea, ETHER_ADDR_LEN) || strcmp(sta->ifname, ifname))) {
		/* from different wl */
		memcpy(&sta->bssid, bssid_ea, ETHER_ADDR_LEN);
		memcpy(&sta->ifname, ifname, IFNAMSIZ);

		/* Initial STA state */
		sta->pae_state = EAPD_INITIALIZE;

		/* initial mode */
		sta->mode = EAPD_STA_MODE_UNKNOW;
		EAPD_INFO("sta %s come from changed.\n", ether_etoa((uchar *)&sta->ea, eabuf));
	}

	sta->last_use = now;
	return sta;
}

static int
event_init(eapd_wksp_t *nwksp)
{
	int i, ret, unit, wlunit[16];
	eapd_wps_t *wps;
	eapd_nas_t *nas;
	eapd_ses_t *ses;
#ifdef BCM_DCS
	eapd_dcs_t *dcs;
#endif /* BCM_DCS */
#ifdef BCM_MEVENT
	eapd_mevent_t *mevent;
#endif /* BCM_MEVENT */
#ifdef BCM_BSD
	eapd_bsd_t *bsd;
#endif /* BCM_BSD */

	char name[IFNAMSIZ], os_name[IFNAMSIZ], all_ifnames[IFNAMSIZ * EAPD_WKSP_MAX_NO_IFNAMES];
	char *next;
	uchar bitvec[WL_EVENTING_MASK_LEN];

	memset(wlunit, -1, sizeof(wlunit));
	memset(name, 0, sizeof(name));
	memset(all_ifnames, 0, sizeof(all_ifnames));
	wps = &nwksp->wps;
	nas = &nwksp->nas;
	ses = &nwksp->ses;
#ifdef BCM_DCS
	dcs = &nwksp->dcs;
#endif /* BCM_DCS */
#ifdef BCM_MEVENT
	mevent = &nwksp->mevent;
#endif /* BCM_MEVENT */
#ifdef BCM_BSD
	bsd = &nwksp->bsd;
#endif /* BCM_BSD */

	/* add all application ifnames to all_ifnames */
	strcpy(all_ifnames, wps->ifnames);
	foreach(name, nas->ifnames, next) {
		add_to_list(name, all_ifnames, sizeof(all_ifnames));
	}
	memset(name, 0, sizeof(name));
	foreach(name, ses->ifnames, next) {
		add_to_list(name, all_ifnames, sizeof(all_ifnames));
	}
#ifdef BCM_DCS
	memset(name, 0, sizeof(name));
	foreach(name, dcs->ifnames, next) {
		add_to_list(name, all_ifnames, sizeof(all_ifnames));
	}
#endif /* BCM_DCS */
#ifdef BCM_MEVENT
	memset(name, 0, sizeof(name));
	foreach(name, mevent->ifnames, next) {
		add_to_list(name, all_ifnames, sizeof(all_ifnames));
	}
#endif /* BCM_MEVENT */
#ifdef BCM_BSD
	memset(name, 0, sizeof(name));
	foreach(name, bsd->ifnames, next) {
		add_to_list(name, all_ifnames, sizeof(all_ifnames));
	}
#endif /* BCM_BSD */

	/* check each name in all_ifnames */
	memset(name, 0, sizeof(name));
	foreach(name, all_ifnames, next) {
		/* apply bitvec to driver */
		memset(bitvec, 0, WL_EVENTING_MASK_LEN);
		memset(os_name, 0, sizeof(os_name));
		if (nvifname_to_osifname(name, os_name, sizeof(os_name)) < 0)
			continue;
		if (wl_probe(os_name) ||
			wl_ioctl(os_name, WLC_GET_INSTANCE, &unit, sizeof(unit)))
			continue;

		/* get current bitvec value */
		ret = wl_iovar_get(os_name, "event_msgs", bitvec, sizeof(bitvec));
		if (ret) {
			EAPD_ERROR("Get event_msg error %d on %s[%s]\n",
				ret, name, os_name);
			continue;
		}

		/* is wps have this name */
		if (find_in_list(wps->ifnames, name)) {
			for (i = 0; i < WL_EVENTING_MASK_LEN; i++) {
				bitvec[i] |= wps->bitvec[i];
			}
		}
		/* is nas have this name */
		if (find_in_list(nas->ifnames, name)) {
			for (i = 0; i < WL_EVENTING_MASK_LEN; i++) {
				bitvec[i] |= nas->bitvec[i];
			}
		}
		/* is ses have this name */
		if (find_in_list(ses->ifnames, name)) {
			for (i = 0; i < WL_EVENTING_MASK_LEN; i++) {
				bitvec[i] |= ses->bitvec[i];
			}
		}

#ifdef BCM_DCS
		/* is dcs have this name */
		if (find_in_list(dcs->ifnames, name)) {
			for (i = 0; i < WL_EVENTING_MASK_LEN; i++) {
				bitvec[i] |= dcs->bitvec[i];
			}
		}
#endif /* BCM_DCS */

#ifdef BCM_MEVENT
		/* is mevent have this name */
		if (find_in_list(mevent->ifnames, name)) {
			for (i = 0; i < WL_EVENTING_MASK_LEN; i++) {
				bitvec[i] |= mevent->bitvec[i];
			}
		}
#endif /* BCM_MEVENT */

#ifdef BCM_BSD
		/* is bsd have this name */
		if (find_in_list(bsd->ifnames, name)) {
			for (i = 0; i < WL_EVENTING_MASK_LEN; i++) {
				bitvec[i] |= bsd->bitvec[i];
			}
		}
#endif /* BCM_BSD */

		ret = wl_iovar_set(os_name, "event_msgs", bitvec, sizeof(bitvec));
		wlunit[unit] = unit;
		if (ret) {
			EAPD_ERROR("Set event_msg error %d on %s[%s]\n", ret, name, os_name);
		}
		else {
#ifdef BCMDBG
			int j, flag;
			if (eapd_msg_level) {
				flag = 1;
				for (i = 0; i < WL_EVENTING_MASK_LEN; i++) {
				for (j = 0; j < 8; j++) {
					if (isset(&bitvec[i], j)) {
						if (flag) {
							EAPD_PRINT("Set event_msg bitvec [%d",
								(i*8+j));
							flag = 0;
						}
						else {
							EAPD_PRINT(" %d", (i*8+j));
						}
					}
				}
				}
				EAPD_PRINT("] on %s[%s]\n", name, os_name);
			}
#endif /* BCMDBG */
		}
	}

	return 0;
}

static int
event_deinit(eapd_wksp_t *nwksp)
{
	return 0;
}

/* Validate handling EAPOL START packet on this receive interface */
static bool
eapd_valid_eapol_start(eapd_wksp_t *nwksp, eapd_brcm_socket_t *from, char *ifname)
{
	eapd_nas_t *nas;
	eapd_wps_t *wps;
	char nv_name[IFNAMSIZ];
	bool needHandle = FALSE;

	if (!nwksp || !from || !ifname)
		return FALSE;

	/* convert eth ifname to wl name (nv name) */
	if (osifname_to_nvifname(ifname, nv_name, sizeof(nv_name)) != 0)
		return FALSE;

	nas = &nwksp->nas;
	wps = &nwksp->wps;

	/* Is this brcmSocket have to handle EAPOL START */
	if (from->flag & (EAPD_CAP_NAS | EAPD_CAP_WPS)) {
		/* Is this receive interface have to handle EAPOL START */
		/* check nas */
		if (find_in_list(nas->ifnames, nv_name))
			needHandle = TRUE;

		/* check wps */
		if (find_in_list(wps->ifnames, nv_name))
			needHandle = TRUE;
	}

	return needHandle;
}

void
eapd_wksp_clear_inited()
{
	eapd_wksp_inited = 0;
}

int eapd_wksp_is_inited()
{
	return eapd_wksp_inited;
}
