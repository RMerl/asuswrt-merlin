/*
 * NAS WorKSPace - NAS application common code
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: nas_wksp.c 450927 2014-01-23 14:13:36Z $
 */

#include <bcmcrypto/passhash.h>
#include <bcmcrypto/sha1.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <bcmtimer.h>

#include <nas.h>
#include <wpa.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

#include <typedefs.h>
#include <bcmutils.h>
#include <wlutils.h>
#include <wlioctl.h>
#include <shutils.h>
#include <proto/eap.h>

#include <nas.h>
#include <nas_wksp.h>
#include <nas_radius.h>
#include <nas_wksp_radius.h>
#include <netconf.h>
#include <nvparse.h>
#include <eapd.h>
#include <security_ipc.h>

/* debug stuff */
#ifdef BCMDBG
#ifndef NAS_WKSP_DEBUG
#define NAS_WKSP_DEBUG	0
#endif
int debug_nwksp = NAS_WKSP_DEBUG;
#endif	/* #if BCMDBG */

/*
* Locally used globals
*/
static char NAS_OPT_LIST[] = "B:g:h:i:I:k:r:K:l:m:N:p:s:t:v:w:d:ADS";
static int nas_wksp_inited = 0;

/*
* Build command line in argc/argv form for NAS configuration.
* This is a template provided for convenience. Program who
* needs the functionality can make a copy of this template
* into its own code space.
*/
#include <bcmutils.h>
#include <shutils.h>

#define NAS_DEBUG 0

/*
* Parse command line and populate nas_wksp_t structure which consists of
* nas_t wpa_t and all the auxiliary stuff that needed for all NAS instances.
*/
#if __ECOS
/* interface to option facilities */
extern int getopt(int argc, char **argv, char *ostr);
extern void initopt(void);
extern char *optarg;
#endif	

/* print NAS command line usage */
void
nas_wksp_display_usage(void)
{
#ifdef BCMDBG
	printf("\nUsage: nas [options]\n\n");
	printf("\t-i <interface>       Wireless interface name\n");
#if defined(NAS_WKSP_BUILD_NAS_AUTH) && defined(NAS_WKSP_BUILD_NAS_SUPPL)
	printf("\t-A                   Authenticator\n");
	printf("\t-S                   Supplicant\n");
#endif	/* #if defined(NAS_WKSP_BUILD_NAS_AUTH) && defined(NAS_WKSP_BUILD_NAS_SUPPL) */
#ifdef NAS_WKSP_BUILD_NAS_AUTH
	printf("\t-g <interval>        WPA GTK rotation interval (ms)\n");
	printf("\t-h <address>         RADIUS server IP address\n");
	printf("\t-p <port>            RADIUS server UDP port\n");
#endif	/* #ifdef NAS_WKSP_BUILD_NAS_AUTH */
	printf("\t-k <secret>          WPA pre-shared key\n");
	printf("\t-r <secret>          RADIUS server secret\n");
	printf("\t-m <authentication>  Authentication protocol - %d:WPA | %d:WPA-PSK | %d:802.1x\n",
		WPA, WPA_PSK, RADIUS);
	printf("\t                                               %d:WPA2 | %d:WPA2-PSK\n",
		WPA2, WPA2_PSK);
	printf("\t-s <SSID>            Service Set Identity\n");
	printf("\t-w <encryption>      Crypto algorithm - %d:WEP | %d:TKIP | %d:AES\n",
		WEP_ENABLED, TKIP_ENABLED, AES_ENABLED);
	printf("\t-I <index>           WEP key index - 2 | 3\n");
	printf("\t-K <key>             WEP key\n");
	printf("\t-t <duration>        Radius Session timeout/PMK Caching duration (ms)\n");
	printf("\t-N <NasID>           NAS id\n");
#ifdef BCMDBG
	printf("\t-v <debug>           Verbose mode - 0:off | 1:console\n");
#endif	/* #ifdef BCMDBG */
	printf("\nThe -i <interface> option must be present before any other per interface options "
	       "to specify the wireless interface\n");
	printf("\n");
#endif	/* #ifdef BCMDBG */
};

/*
* Parse command line and populate nas_wksp_t structure.
* It takes short form options only as described in function usage().
* Field 'nwcbs' in 'nwksp' indicates the number of elements populated
* upon return.
*/
int
nas_wksp_parse_cmd(int argc, char *argv[], nas_wksp_t *nwksp)
{
	nas_wpa_cb_t *nwcb = NULL;	/* pointer to current nas_wpa_cb_t */
	int i = -1;					/* index of current interface */
	int opt;
	int unit;

#if defined(__ECOS)
	initopt();
#endif

	/* parse command line parameters */
	while ((opt = getopt(argc, argv, NAS_OPT_LIST)) != EOF) {
		switch (opt) {
		case 'i':
			if (i + 1 >= NAS_WKSP_MAX_NUM_INTERFACES) {
				NASDBG("too many interfaces specified\n");
				return -1;
			}
			break;
#ifdef BCMDBG
		case 'v':
			break;
#endif
		default:
			if (i < 0) {
				NASDBG("no I/F specified\n");
				return -1;
			}
			break;
		}

		/* save parameters in user-provided structure */
		switch (opt) {
		/* i/f dependant parameters */
		case 'i':
			/* check if i/f exists and retrieve the i/f index */
			if (wl_probe(optarg) ||
			    wl_ioctl(optarg, WLC_GET_INSTANCE, &unit, sizeof(unit))) {
				NASDBG("%s: not a wireless interface\n", optarg);
				break;
			}
			/* ignore errorous interface and reuse nas_wpa_cb_t */
			if (i >= 0 && (nwcb->flags & NAS_WPA_CB_FLAG_ERROR))
				NASMSG("%s: error parsing options, ignored\n", nwcb->nas.interface);
			/* advance nas_wpa_cb_t index & alloc new nas_wpa_cb_t */
			else if ((++ i) < NAS_WKSP_MAX_NUM_INTERFACES) {
				nwcb = (nas_wpa_cb_t *)malloc(sizeof(nas_wpa_cb_t));
				assert(nwcb);
			}
			/* init nas_wpa_cb_t */
			memset(nwcb, 0, sizeof(nwcb[0]));
			nwcb->nwksp = nwksp;
			nwcb->unit = unit;
#if !defined(NAS_WKSP_BUILD_NAS_AUTH) && defined(NAS_WKSP_BUILD_NAS_SUPPL)
			nwcb->flags |= NAS_WPA_CB_FLAG_SUPPL;
			nwcb->nas.flags |= NAS_FLAG_SUPPLICANT;
#endif	/* #if !defined(NAS_WKSP_BUILD_NAS_AUTH) && defined(NAS_WKSP_BUILD_NAS_SUPPL) */
#if defined(NAS_WKSP_BUILD_NAS_AUTH) && !defined(NAS_WKSP_BUILD_NAS_SUPPL)
			nwcb->flags |= NAS_WPA_CB_FLAG_AUTH;
			nwcb->nas.flags |= NAS_FLAG_AUTHENTICATOR;
#endif	/* #if defined(NAS_WKSP_BUILD_NAS_AUTH) && !defined(NAS_WKSP_BUILD_NAS_SUPPL) */
			nwcb->nas.wan = NAS_WKSP_UNK_FILE_DESC;
			nwcb->nas.server.sin_port = htons(RADIUS_PORT);
			nwcb->nas.wsec = TKIP_ENABLED|AES_ENABLED;
			nwcb->nas.wpa = &nwcb->wpa;
			nwcb->nas.appl = nwcb;
#ifdef BCMDBG
			nwcb->nas.debug = 1;
#endif
			nwcb->nas.disable_preauth = 0;
			nwcb->nas.ssn_to = 36000;	/* 10hrs */
			nwcb->wpa.nas = &nwcb->nas;
			strncpy(nwcb->nas.interface, optarg, IFNAMSIZ);
			NASDBG("nas[%d].interface %s\n", i, optarg);
			/* Get interface address */
			if (wl_hwaddr(nwcb->nas.interface, nwcb->nas.ea.octet)) {
				NASDBG("%s: failed to get hwaddr\n", nwcb->nas.interface);
				nwcb->flags |= NAS_WPA_CB_FLAG_ERROR;
				break;
			}
			NASDBG("nas[%d].hwaddr %02x:%02x:%02x:%02x:%02x:%02x\n",
				i,
				nwcb->nas.ea.octet[0], nwcb->nas.ea.octet[1],
				nwcb->nas.ea.octet[2], nwcb->nas.ea.octet[3],
				nwcb->nas.ea.octet[4], nwcb->nas.ea.octet[5]);
			nwksp->nwcb[i] = nwcb;
			break;
		case 'k':
			/* Save key parameter.  What contraints to apply
			 * are unknown until mode is also known.
			 */
			strncpy((char *)nwcb->psk, optarg, NAS_WKSP_MAX_USER_KEY_LEN);
			nwcb->psk[NAS_WKSP_MAX_USER_KEY_LEN] = 0;
			NASDBG("nas[%d].psk %s\n", i, optarg);
			break;
		case 'r':
			/* Save key parameter.  What contraints to apply
			 * are unknown until mode is also known.
			 */
			strncpy((char *)nwcb->secret, optarg, NAS_WKSP_MAX_USER_KEY_LEN);
			nwcb->secret[NAS_WKSP_MAX_USER_KEY_LEN] = 0;
			NASDBG("nas[%d].secret %s\n", i, optarg);
			break;
		case 'm':
			/* update auth mode */
			nwcb->nas.mode = (int)strtoul(optarg, NULL, 0);
			NASDBG("nas[%d].mode %s\n", i, optarg);
			break;
#if defined(NAS_WKSP_BUILD_NAS_AUTH) && defined(NAS_WKSP_BUILD_NAS_SUPPL)
		case 'A':
			/* nas as authenticator */
			nwcb->flags |= NAS_WPA_CB_FLAG_AUTH;
			nwcb->nas.flags |= NAS_FLAG_AUTHENTICATOR;
			NASDBG("nas[%d].role authenticator\n", i);
			break;
		case 'S':
			/* nas as supplicant */
			nwcb->flags |= NAS_WPA_CB_FLAG_SUPPL;
			nwcb->nas.flags |= NAS_FLAG_SUPPLICANT;
			NASDBG("nas[%d].role supplicant\n", i);
			break;
#endif	/* #if defined(NAS_WKSP_BUILD_NAS_AUTH) && defined(NAS_WKSP_BUILD_NAS_SUPPL) */
#ifdef NAS_WKSP_BUILD_NAS_AUTH
		case 'g':
			/* update group key rekey interval */
			nwcb->wpa.gtk_rekey_secs = (int)strtoul(optarg, NULL, 0);
			NASDBG("nas[%d].gtk.rekey %s\n", i, optarg);
			break;
		case 'h':
#ifdef NAS_RADIUS
			/* update radius server address */
			nwcb->nas.server.sin_family = AF_INET;
			nwcb->nas.server.sin_addr.s_addr = inet_addr(optarg);
			NASDBG("nas[%d].server.address %s\n", i, optarg);
#endif /* #ifdef NAS_RADIUS */
			break;
		case 'p':
			/* update radius server port number */
			nwcb->nas.server.sin_port = htons((int)strtoul(optarg, NULL, 0));
			NASDBG("nas[%d].server.port %s\n", i, optarg);
			break;
#endif	/* #ifdef NAS_WKSP_BUILD_NAS_AUTH */
		case 's':
			/* update ssid info */
			strncpy(nwcb->nas.ssid, optarg, DOT11_MAX_SSID_LEN);
			NASDBG("nas[%d].ssid %s\n", i, optarg);
			break;
		case 'w':
			/* update wsec value */
			nwcb->nas.wsec = (int)strtoul(optarg, NULL, 0);
			NASDBG("nas[%d].wsec %s\n", i, optarg);
			break;
		case 'D':
			/* nas in WDS mode */
			nwcb->flags |= NAS_WPA_CB_FLAG_WDS;
			nwcb->nas.flags |= NAS_FLAG_WDS;
			NASDBG("nas[%d].flags %08x\n", i, nwcb->nas.flags);
			/* remote address */
			if (wl_ioctl(nwcb->nas.interface, WLC_WDS_GET_REMOTE_HWADDR,
			             nwcb->nas.remote, sizeof(nwcb->nas.remote))) {
				NASDBG("%s: failed to get remote hwaddr\n", nwcb->nas.interface);
				nwcb->flags |= NAS_WPA_CB_FLAG_ERROR;
				break;
			}
			NASDBG("nas[%d].remote %02x:%02x:%02x:%02x:%02x:%02x\n",
				i,
				nwcb->nas.remote[0], nwcb->nas.remote[1],
				nwcb->nas.remote[2], nwcb->nas.remote[3],
				nwcb->nas.remote[4], nwcb->nas.remote[5]);
			break;
#ifdef BCMDBG
		case 'v':
			/* verbose - 0:no | others:yes */
			/* for workspace */
			if (i < 0) {
				debug_nwksp = (int)strtoul(optarg, NULL, 0);
			}
			/* for nas */
			else
				nwcb->nas.debug = (bool)strtoul(optarg, NULL, 0);
			break;
#endif

		case 'I':
			/* WEP key index */
			nwcb->index = (int)strtoul(optarg, NULL, 0);
			NASDBG("nas[%d].wep.index %s\n", i, optarg);
			break;

		case 'K':
			/* WEP key */
			strncpy((char *)nwcb->wep, optarg, NAS_WKSP_MAX_USER_KEY_LEN);
			nwcb->wep[NAS_WKSP_MAX_USER_KEY_LEN] = 0;
			NASDBG("nas[%d].wep %s\n", i, optarg);
			break;
		case 't':
			nwcb->nas.ssn_to = (int)strtoul(optarg, NULL, 0);
			NASDBG("nas[%d].ssn.timeout %s\n", i, optarg);
			break;
		case 'd':
			nwcb->nas.disable_preauth = strtoul(optarg, NULL, 0) == 0;
			NASDBG("nas[%d].disable_preauth %d\n", i, nwcb->nas.disable_preauth);
			break;
		case 'N':
			strncpy(nwcb->nas.nas_id, optarg, MAX_NAS_ID_LEN);
			NASDBG("nas[%d].nas_id %s\n", i, optarg);
			break;
		default:
			/* display wrong option and quit */
			NASDBG("unknown option -%c, ignored\n", opt);
			break;
		}
	}

	/* return to caller # of i/f */
	nwksp->nwcbs = i + 1;

	return 0;
}

/* listen to sockets and call handlers to process packets */
int
nas_wksp_main_loop(nas_wksp_t *nwksp)
{
	int ret;

	/* init nas */
	ret = nas_wksp_init(nwksp);

	/* nas wksp initialization finished */
	nas_wksp_inited = 1;
	if (ret) {
		NASMSG("Unable to initialize NAS. Quitting...\n");
		nas_wksp_free_workspace(nwksp);
		return -1;
	}

#if !defined(DEBUG) && !defined(__ECOS)
	/* Daemonize */
	if (daemon(1, 1) == -1) {
		/* clean up nas workspace */
		nas_wksp_cleanup(nwksp);
		/* free workspace context */
		nas_wksp_free_workspace(nwksp);
		perror("nas_wksp_main_loop: daemon\n");
		exit(errno);
	}
#endif

	while (1) {
		/* check user command for shutdown */
		if (nwksp->flags & NAS_WKSP_FLAG_SHUTDOWN) {
			/* clean up nas workspace */
			nas_wksp_cleanup(nwksp);

			/* free workspace context */
			nas_wksp_free_workspace(nwksp);

			NASDBG("NAS shutdown...\n");
			return 0;
		}

		/* check user command for rekey */
		if (nwksp->flags & NAS_WKSP_FLAG_REKEY) {
#ifdef NAS_WKSP_BUILD_NAS_AUTH
			nas_wpa_cb_t *nwcb = NULL;
			int i;

			NASDBG("NAS rekey...\n");
			/* do rekey operation */
			for (i = 0; i < nwksp->nwcbs; i ++) {
				nwcb = nwksp->nwcb[i];
				if (nwcb)
					nas_force_rekey(&nwcb->nas);
			}
#endif	/* #ifdef NAS_WKSP_BUILD_NAS_AUTH */
			nwksp->flags &= ~NAS_WKSP_FLAG_REKEY;
		}

		/* do packets dispatch */
		nas_wksp_dispatch_packet(nwksp);
	}
}

/* listen to sockets and call handlers to process packets */
void
nas_wksp_dispatch_packet(nas_wksp_t *nwksp)
{
	fd_set fdset;
	int i, len, width, status, bytes;
	nas_wpa_cb_t *nwcb = NULL;
	uint8 *pkt;
	bcm_event_t *pvt_data;
	struct timeval tv;

	/* init file descriptor set */
	FD_ZERO(&nwksp->fdset);
	nwksp->fdmax = -1;

	/* build file descriptor set now to save time later */
	if (nwksp->eapd != NAS_WKSP_UNK_FILE_DESC) {
		FD_SET(nwksp->eapd, &nwksp->fdset);
		if (nwksp->eapd > nwksp->fdmax)
			nwksp->fdmax = nwksp->eapd;
	}
	for (i = 0; i < nwksp->nwcbs; i ++) {
		nwcb = nwksp->nwcb[i];

		/* ignore the interface if there was any error */
		if (nwcb->flags & NAS_WPA_CB_FLAG_ERROR) {
			/* NASMSG("%s: ignore i/f due to error(s)\n", nwcb->nas.interface); */
			continue;
		}
#ifdef NAS_WKSP_BUILD_NAS_AUTH
		if (nwcb->nas.wan != NAS_WKSP_UNK_FILE_DESC) {
			FD_SET(nwcb->nas.wan, &nwksp->fdset);
			if (nwcb->nas.wan > nwksp->fdmax)
				nwksp->fdmax = nwcb->nas.wan;
		}
#endif	/* #ifdef NAS_WKSP_BUILD_NAS_AUTH */
	}
	/* check if there is any sockets in the fd set */
	if (nwksp->fdmax == -1) {
		/* do shutdown procedure */
		nwksp->flags  = NAS_WKSP_FLAG_SHUTDOWN;
		NASMSG("Threr is no any sockets in the fd set, shutdown...\n");
		return;
	}

	pkt = nwksp->packet;
	len = sizeof(nwksp->packet);
	width = nwksp->fdmax + 1;
	fdset = nwksp->fdset;
	/* set timeout value */
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	/* enable timer handling */
	bcm_timer_module_enable(nwksp->timer, 1);

	/* listen to data availible on all sockets */
	status = select(width, &fdset, NULL, NULL, &tv);

	/* disable timer handling */
	bcm_timer_module_enable(nwksp->timer, 0);

	/* got something */
	if (status > 0) {
		NASDBG("recv brcmevent packet from eapd Socket %d fd_isset %d\n",
			nwksp->eapd, (int)FD_ISSET(nwksp->eapd, &fdset));
		/* the data is driver indication message or encapsulated 802.1x frame */
		if (nwksp->eapd !=  NAS_WKSP_UNK_FILE_DESC && FD_ISSET(nwksp->eapd, &fdset)) {
			char *ifname = (char *)pkt;
			eapol_header_t *eapol = (eapol_header_t *)(ifname + IFNAMSIZ);

			if ((bytes = recv(nwksp->eapd, pkt, len, 0)) > 0) {
				/* strip prepend ifname */
				bytes -= IFNAMSIZ;

				/* dispatch message to eapol, preauth, brcmevent */
				switch (ntohs(eapol->eth.ether_type)) {
				case ETHER_TYPE_802_1X: /* eapol */
					NASDBG("recv eapol packet from socket %d ifname %s\n",
					       nwksp->eapd, ifname);

					nwcb = nas_wksp_find_nwcb(nwksp, eapol->eth.ether_dhost,
					                          ifname, NAS_WKSP_NWCB_AUTO);
					if (!nwcb)
						return;

					/* dispatch eapol for auth or suppl */
					nas_eapol_message_dispatch(nwcb, (void *)eapol, bytes);
					break;
				case ETHER_TYPE_802_1X_PREAUTH: /* preauth */
					NASDBG("recv preauth packet from socket %d\n",
					       nwksp->eapd);

					nwcb = nas_wksp_find_nwcb(nwksp, eapol->eth.ether_dhost,
					                          NULL, NAS_WKSP_NWCB_AUTO);
					if (!nwcb)
						return;

					preauth_dispatch(&nwcb->nas, eapol, bytes);
					break;
				case ETHER_TYPE_BRCM: /* brcmevent */
					NASDBG("recv brcmevent packet from eapd Socket %d "
					       "ifname %s\n", nwksp->eapd, ifname);

					/* make sure to interpret only messages destined to NAS */
					if (nas_validate_wlpvt_message(bytes, (uint8 *)eapol))
						return;

					pvt_data = (bcm_event_t *)(ifname + IFNAMSIZ);

					NASDBG("%s: %02x:%02x:%02x:%02x:%02x:%02x\n",
						pvt_data->event.ifname,
						pvt_data->eth.ether_dhost[0],
						pvt_data->eth.ether_dhost[1],
						pvt_data->eth.ether_dhost[2],
						pvt_data->eth.ether_dhost[3],
						pvt_data->eth.ether_dhost[4],
						pvt_data->eth.ether_dhost[5]);

					nwcb = nas_wksp_find_nwcb(nwksp,
					                          pvt_data->eth.ether_dhost,
					                          pvt_data->event.ifname,
					                          NAS_WKSP_NWCB_AUTO);

					if (!nwcb) {
						NASDBG("unable to find nwcb\n");
						return;
					}

					/* do not process eapol message in brcmevent */
					nas_handle_wlpvt_messages(nwcb, (void *)pvt_data, bytes);
					break;
				} /* switch(ntohs(eapol->eth.ether_type)) */
			} /* if ((bytes = recv(nwksp->eapd, pkt, len, 0)) > 0) */
		} /* FD_ISSET(nwksp->eapd, &fdset) */

#ifdef NAS_WKSP_BUILD_NAS_AUTH
		/* process radius data from individual interfaces */
		for (i = 0; i < nwksp->nwcbs; i ++) {
			nwcb = nwksp->nwcb[i];
			/* the data is radius message */
			if ((nwcb->nas.wan !=  NAS_WKSP_UNK_FILE_DESC) &&
				FD_ISSET(nwcb->nas.wan, &fdset)) {
				if (recv(nwcb->nas.wan, pkt, len, 0) <= 0) {
					NASMSG("%s: recv radius error %d from socket %d\n",
					       nwcb->nas.interface, errno, nwcb->nas.wan);
					/* Reopen the socket to the radius server if possible */
					if (NAS_RADIUS_OPEN(nwksp, nwcb) != 0) {
						NASMSG("%s: open radius connection failed\n",
						       nwcb->nas.interface);
					}
					continue; /* Skip this attempt */
				}

				memset(&nwcb->nas.m_deauth_params, 0, sizeof(deauth_params_t));
				memset(&nwcb->nas.m_subrem_params, 0, sizeof(subrem_params_t));
				memset(&nwcb->nas.m_bsstran_params, 0, sizeof(bsstran_params_t));

				NASDBG("%s: recv radius packet from socket %d\n",
				       nwcb->nas.interface, nwcb->nas.wan);
				RADIUS_DISPATCH(&nwcb->nas, (void *)pkt);
			}
		}
#endif	/* #ifdef NAS_WKSP_BUILD_NAS_AUTH */

	}

	return;
}

/* establish connection to EAPD to
 * receive wpa, eapol and preauth.
 */
int
nas_wksp_open_eapd(nas_wksp_t *nwksp)
{
	int reuse = 1;
	struct sockaddr_in sockaddr;

	/* open loopback socket to communicate with EAPD */
	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sockaddr.sin_port = htons(EAPD_WKSP_NAS_UDP_SPORT);
	if ((nwksp->eapd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		NASDBG("eapd: Unable to create loopback socket\n");
		goto exit0;
	}
#if defined(__ECOS)
	if (setsockopt(nwksp->eapd, SOL_SOCKET, SO_REUSEPORT, (char*)&reuse, sizeof(reuse)) < 0) {
		NASDBG("eapd: Unable to setsockopt to loopback socket %d.\n", nwksp->eapd);
		goto exit1;
	}
#else
	if (setsockopt(nwksp->eapd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
		NASDBG("eapd: Unable to setsockopt to loopback socket %d.\n", nwksp->eapd);
		goto exit1;
	}
#endif
	if (bind(nwksp->eapd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
		NASDBG("eapd: Unable to bind to loopback socket %d\n", nwksp->eapd);
		goto exit1;
	}
	NASDBG("eapd: opened loopback socket %d\n", nwksp->eapd);
	return 0;

	/* error handling */
exit1:
	close(nwksp->eapd);

exit0:
	nwksp->eapd = NAS_WKSP_UNK_FILE_DESC;
	NASDBG("eapd: failed to open loopback socket\n");
	return errno;
}

void
nas_wksp_close_eapd(nas_wksp_t *nwksp)
{
	NASDBG("eapd: try closing loopback socket %d\n", nwksp->eapd);

	/* clsoe eapd socket */
	if (nwksp->eapd != NAS_WKSP_UNK_FILE_DESC) {
		NASDBG("eapd: close loopback socket %d\n", nwksp->eapd);
		close(nwksp->eapd);
		nwksp->eapd = NAS_WKSP_UNK_FILE_DESC;
	}
	return;
}

/* transmit eapol message thru the socket */
static int
nas_wksp_eapd_send_packet(nas_t *nas, struct iovec *frags, int nfrags)
{
	nas_wpa_cb_t *nwcb = (nas_wpa_cb_t *)nas->appl;
	nas_wksp_t *nwksp = nwcb->nwksp;
	struct ether_header *eth;
	struct msghdr mh;
	struct sockaddr_in to;
	struct iovec *iov;
	int i, rc = 0;

	if (!nfrags)
		return -1;
	assert(frags != NULL);

	if (frags->iov_len < sizeof(struct ether_header))
		return -1;

	/* allocate iov buffer */
	iov = malloc(sizeof(struct iovec) * (nfrags + 1));
	if (iov == NULL)
		return -1;

	eth = (struct ether_header*) frags->iov_base;

	to.sin_addr.s_addr = inet_addr(EAPD_WKSP_UDP_ADDR);
	to.sin_family = AF_INET;
	to.sin_port = htons(EAPD_WKSP_NAS_UDP_RPORT);

	/* Save incoming interface name */
	iov[0].iov_base = (void *)&nas->interface;
	iov[0].iov_len = IFNAMSIZ;

	for (i = 1; i <= nfrags; i++) {
		iov[i].iov_base = frags[i-1].iov_base;
		iov[i].iov_len = frags[i-1].iov_len;
	}

	memset(&mh, 0, sizeof(mh));
	mh.msg_name = (void *)&to;
	mh.msg_namelen = sizeof(to);
	mh.msg_iov = iov;
	mh.msg_iovlen = nfrags + 1;

	if (sendmsg(nwksp->eapd, &mh, 0) < 0)
		rc = errno;

	free(iov);
	return rc;
}

/* transmit eapol message thru the socket */
int
nas_eapol_send_packet(nas_t *nas, struct iovec *frags, int nfrags)
{
	return (nas_wksp_eapd_send_packet(nas, frags, nfrags));
}

/* transmit preauth message thru the socket */
int nas_preauth_send_packet(nas_t *nas, struct iovec *frags, int nfrags)
{
	return (nas_wksp_eapd_send_packet(nas, frags, nfrags));
}

/* allocate NAS workspace for <nifs> interfaces/instances */
nas_wksp_t *
nas_wksp_alloc_workspace(void)
{
	nas_wksp_t *nwksp = (nas_wksp_t *)malloc(sizeof(nas_wksp_t));
	if (!nwksp)
		return NULL;

	memset(nwksp, 0, sizeof(nas_wksp_t));


	NASDBG("allocated NAS workspace %d bytes\n", sizeof(nas_wksp_t));
	return nwksp;
}

/* free memory taken by NAS workspace */
void
nas_wksp_free_workspace(nas_wksp_t *nwksp)
{
	int i;
	NASDBG("free NAS workspace %08x\n", (int)nwksp);
	for (i = 0; i < nwksp->nwcbs; i ++)
		if (nwksp->nwcb[i])
			free(nwksp->nwcb[i]);
	free(nwksp);
}

#define NAS_WKSP_MODULE_TIMER	0
/*
* Init the timer event queue (used only by WPA stuff,
*  up to one per supplicant plus:
*    the group rekey timer
*    MIC failure throttling timer
*    4-way handshake initiator for wds
*    pmk timer for WPA2
*/
#define NAS_WKSP_MAX_NUM_TIMER	(MAX_SUPPLICANTS + 4)

/* init one nas instance */
static int
nas_init_nas(nas_wksp_t *nwksp, nas_wpa_cb_t *nwcb)
{
	uint8 *data, *key;
	int len;
#ifdef BCMDBG
	if (nwcb->nas.debug == 0)
		nwcb->nas.debug = NAS_DEBUG;
#endif
#if !NAS_WKSP_MODULE_TIMER
	nwcb->nas.timer = nwksp->timer;
#else
	bcm_timer_module_init(NAS_WKSP_MAX_NUM_TIMER, &nwcb->nas.timer);
#endif	/* #if NAS_WKSP_MODULE_TIMER */

	/* check if mode is supported */
	if (!(CHECK_AUTH(nwcb->nas.mode))) {
		NASDBG("%s: auth mode %d is not supported\n", nwcb->nas.interface, nwcb->nas.mode);
		nwcb->flags |= NAS_WPA_CB_FLAG_ERROR;
		return 0;
	}

	/* check if wsec is supported */
	if (!nwcb->nas.wsec ||
	    (nwcb->nas.wsec & (AES_ENABLED | TKIP_ENABLED | WEP_ENABLED)) != nwcb->nas.wsec) {
		NASDBG("%s: wsec 0x%x is not supported\n", nwcb->nas.interface, nwcb->nas.wsec);
		nwcb->flags |= NAS_WPA_CB_FLAG_ERROR;
		return 0;
	}

	/* validate gtk rotation and gtk index */
	if (nwcb->flags & NAS_WPA_CB_FLAG_AUTH) {
		if ((CHECK_NAS(nwcb->nas.mode)) &&
		    WSEC_WEP_ENABLED(nwcb->nas.wsec)) {
			if (nwcb->wpa.gtk_rekey_secs) {
				NASDBG("%s: GTK rotation is not allowed when WEP is enabled,"
				       " disabled\n",
				       nwcb->nas.interface);
				nwcb->wpa.gtk_rekey_secs = 0;
			}
			if (nwcb->index != (GTK_INDEX_1 + 1) && nwcb->index != (GTK_INDEX_2 + 1)) {
				NASDBG("%s: GTK index %d is invalid when WEP is enabled, using %d"
				       " instead\n",
				       nwcb->nas.interface, nwcb->index, GTK_INDEX_1 + 1);
				nwcb->index = GTK_INDEX_1 + 1;
			}
		}
		else if (nwcb->index &&
			nwcb->index != (GTK_INDEX_1 + 1) && nwcb->index != (GTK_INDEX_2 + 1)) {
			NASDBG("%s: GTK index %d is invalid when used with mode %d, using %d"
			       " instead\n",
			       nwcb->nas.interface, nwcb->index, nwcb->nas.mode, GTK_INDEX_1 + 1);
			nwcb->index = GTK_INDEX_1 + 1;
		}
	}

	/* nas type is Wireless IEEE 802.11 */
	nwcb->nas.type = NAS_PORT_TYPE_WIRELESS_IEEE80211;

	/* default key index and size */
	nwcb->wpa.gtk_index = GTK_INDEX_1;
#ifdef MFP
	nwcb->wpa.igtk.id = IGTK_INDEX_1;
#endif

	if (nwcb->nas.mode & RADIUS)
		nwcb->wpa.gtk_len = WEP128_KEY_SIZE;

	/* apply key constraints according to the mode */
	/* PSK pre-shared key */
	if (CHECK_PSK(nwcb->nas.mode)) {
		uint8 *num = NULL;
		key = nwcb->psk;
		len = strlen((char *)key);
		nwcb->nas.key.data = data = nwcb->wpa.pmk;
		/* numeric key must be 256-bit. */
		if (len == NAS_WKSP_PSK_LEN)
			num = key;
		/* allow leading hex radix for a proper size number */
		else if ((len == NAS_WKSP_PSK_LEN + 2) &&
		         (!strncmp((char *)key, "0x", 2) || !strncmp((char *)key, "0X", 2)))
			num = key + 2;
		if (num) {
			int j = 0;
			char hex[] = "XX";
			do {
				hex[0] = *num++;
				hex[1] = *num++;
				if (!isxdigit((int)hex[0]) ||
				    !isxdigit((int)hex[1])) {
					NASDBG("%s: numeric PSK %s not 256-bit hex number\n",
					       nwcb->nas.interface, key);
					nwcb->flags |= NAS_WPA_CB_FLAG_ERROR;
					break;
				}
				*data++ = (uint8)strtoul(hex, NULL, 16);
			} while (++j < NAS_WKSP_PSK_LEN/2);
			nwcb->nas.key.length = NAS_WKSP_PSK_LEN/2;
		} else {
			unsigned char output[2*SHA1HashSize];
			if ((len < NAS_WKSP_PASSPHRASE_MIN) ||
			    (len > NAS_WKSP_PASSPHRASE_MAX)) {
				NASDBG("%s: %s length illegal\n", nwcb->nas.interface, key);
				nwcb->flags |= NAS_WPA_CB_FLAG_ERROR;
				return 0;
			}

			/* perform password to hash conversion */
			if (passhash((char *)key, len, (uchar *)nwcb->nas.ssid,
			             strlen(nwcb->nas.ssid), output)) {
				NASDBG("%s: PSK password hash failed\n", nwcb->nas.interface);
				nwcb->flags |= NAS_WPA_CB_FLAG_ERROR;
				return 0;
			}
			memcpy(data, output, PMK_LEN);
			nwcb->nas.key.length = PMK_LEN;
		}
		nwcb->wpa.pmk_len = nwcb->nas.key.length;
	}

	/* RADIUS secret */
	if (CHECK_RADIUS(nwcb->nas.mode)) {
		key = nwcb->secret;
		len = strlen((char *)key);
		if (len > NAS_WKSP_MAX_USER_KEY_LEN) {
			NASDBG("%s: %s too long, truncated\n", nwcb->nas.interface, key);
			len = NAS_WKSP_MAX_USER_KEY_LEN;
		}
		nwcb->nas.secret.data = key;
		nwcb->nas.secret.length = len;
	}

	/* init mode-specific keys */
	if (CHECK_NAS(nwcb->nas.mode)) {
		/* generate the initial global_key_counter and gmk */
		initialize_global_key_counter(&nwcb->wpa);
		initialize_gmk(&nwcb->wpa);
	}

	/* get default key size, key index */
	if (!nwcb->wpa.gtk_rekey_secs) {
		/*
		* Honor statically configured WEP key. Key index
		* should be either 2 or 3.
		*/
		if (WSEC_WEP_ENABLED(nwcb->nas.wsec) &&
		    nwcb->index && strlen((char *)nwcb->wep)) {

			uint8 wep[NAS_WKSP_MAX_USER_KEY_LEN + 1];
			char hex[] = "XX";
			key = nwcb->wep;
			data = wep;

			switch (strlen((char *)key)) {
			case WEP1_KEY_SIZE:
			case WEP128_KEY_SIZE:
				len = strlen((char *)key);
				strcpy((char *)wep, (char *) key);
				break;
			case WEP1_KEY_HEX_SIZE:
			case WEP128_KEY_HEX_SIZE:
				len = strlen((char *)key) / 2;
				while (*key) {
					strncpy(hex, (char *) key, 2);
					*data++ = (uint8)strtoul(hex, NULL, 16);
					key += 2;
				}
				break;
			default:
				len = 0;
				break;
			}

			/* wlconf will apply wep first */
			if (len) {
				bcopy(wep, nwcb->wpa.gtk, len);
				nwcb->wpa.gtk_index = nwcb->index - 1;
				nwcb->wpa.gtk_len = len;
				nwcb->nas.flags |= NAS_FLAG_GTK_PLUMBED;
			}
			else {
				NASDBG("%s: unable to plumb WEP key!\n",
				       nwcb->nas.interface);
				nwcb->flags |= NAS_WPA_CB_FLAG_ERROR;
				return 0;
			}
		}
		/*
		 * When doing WPA (WEP is not enabled) or doing
		 * 802.1x without static WEP key, honor the key index.
		 */
		else if (nwcb->index)
			nwcb->wpa.gtk_index = nwcb->index - 1;
	}

	/* AP specific setup */
	if (nwcb->flags & NAS_WPA_CB_FLAG_AUTH) {
		/* grab WPA capabilities, used in IE contruction */
		nas_get_wpacap(&nwcb->nas, nwcb->wpa.cap);
	}


	/* tell nas to start */
	if (nwcb->flags & NAS_WPA_CB_FLAG_ERROR) {
		NASDBG("%s: unable to start NAS due to early error",
			nwcb->nas.interface);
		return 0;
	}

	nas_start(&nwcb->nas);

	return 0;
}

/* init all NAS instances */
static int
nas_wksp_init_nas(nas_wksp_t *nwksp)
{
	nas_wpa_cb_t *nwcb;
	int i;

	for (i = 0; i < nwksp->nwcbs; i ++) {
		nwcb = nwksp->nwcb[i];
		assert(nwcb);

		nas_init_nas(nwksp, nwcb);
	}

	return 0;
}

/* cleanup one nas instance */
static void
nas_cleanup_nas(nas_wksp_t *nwksp, nas_wpa_cb_t *nwcb)
{
	wpa_reset_countermeasures(nwcb->nas.wpa);

#if !NAS_WKSP_MODULE_TIMER
	nwcb->nas.timer = 0;
#else
	bcm_timer_module_cleanup(nwcb->nas.timer);
#endif

	return;
}

/* cleanup all nas instances */
static void
nas_wksp_cleanup_nas(nas_wksp_t *nwksp)
{
	nas_wpa_cb_t *nwcb;
	int i;

	/* init each instance */
	for (i = 0; i < nwksp->nwcbs; i ++) {
		nwcb = nwksp->nwcb[i];
		assert(nwcb);

		nas_cleanup_nas(nwksp, nwcb);
	}
}

/*
* Common NAS application level routines that can be used under different
* OSs. These functions need to be re-implemented only when the application
* needs a different low level control over how NAS behaves than this
* implementation.
*/
/* init NAS workspace */
int
nas_wksp_init(nas_wksp_t *nwksp)
{
#if !NAS_WKSP_MODULE_TIMER
	if (bcm_timer_module_init(NAS_WKSP_MAX_NUM_TIMER*NAS_WKSP_MAX_NUM_INTERFACES,
	                          &nwksp->timer)) {
		NASMSG("bcm_timer_module_init failed\n");
	}
#endif	/* #if !NAS_WKSP_MODULE_TIMER */

	/* open connection to receive eapd messages */
	nas_wksp_open_eapd(nwksp);

#ifdef NAS_WKSP_BUILD_NAS_AUTH
	/* open connection to receive radius messages */
	(void)NAS_WKSP_OPEN_RADIUS(nwksp);
#endif	/* #ifdef NAS_WKSP_BUILD_NAS_AUTH */

	/* init each instance */
	nas_wksp_init_nas(nwksp);

	return 0;
}

/* cleanup NAS workspace */
void
nas_wksp_cleanup(nas_wksp_t *nwksp)
{
	/* stop running nas */
	nas_wksp_cleanup_nas(nwksp);

#ifdef NAS_WKSP_BUILD_NAS_AUTH
	/* disconnect from radius server */
	NAS_WKSP_CLOSE_RADIUS(nwksp);
#endif	/* #ifdef NAS_WKSP_BUILD_NAS_AUTH */

#if !NAS_WKSP_MODULE_TIMER
	bcm_timer_module_cleanup(nwksp->timer);
#endif	/* #if !NAS_WKSP_MODULE_TIMER */

	nas_wksp_close_eapd(nwksp);
}

#ifdef NAS_WKSP_ON_DEMAND
static nas_wpa_cb_t*
nas_get_wsec(nas_wksp_t *nwksp, uint8 *mac, char *osifname)
{
	int ret;
	wsec_info_t info;
	nas_wpa_cb_t *nwcb = NULL;

	if ((ret = get_wsec(&info, mac, osifname)) != WLIFU_WSEC_SUCCESS) {
		if (ret != WLIFU_ERR_NOT_SUPPORT_MODE)
			NASDBG("Get wireless security settings failed,"
			       "mac=0x%02x:%02x:%02x:%02x:%02x:%02x osifname = %s err %d\n",
			       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
			       osifname ? osifname : "NULL", ret);
		return NULL;
	}

	NASDBG("Wireless settings for %s: 0x%08x\n", osifname ? osifname : "NULL",
		info.flags);

	nwcb = (nas_wpa_cb_t *)malloc(sizeof(nas_wpa_cb_t));
	if (!nwcb) {
		NASMSG("Memory allocate failed for adding new nwcb\n");
		return NULL;
	}
	memset(nwcb, 0, sizeof(nas_wpa_cb_t));

	/* Set default values */
	nwcb->nwksp = nwksp;
#if !defined(NAS_WKSP_BUILD_NAS_AUTH) && defined(NAS_WKSP_BUILD_NAS_SUPPL)
	nwcb->flags |= NAS_WPA_CB_FLAG_SUPPL;
	nwcb->nas.flags |= NAS_FLAG_SUPPLICANT;
#endif	/* #if !defined(NAS_WKSP_BUILD_NAS_AUTH) && defined(NAS_WKSP_BUILD_NAS_SUPPL) */
#if defined(NAS_WKSP_BUILD_NAS_AUTH) && !defined(NAS_WKSP_BUILD_NAS_SUPPL)
	nwcb->flags |= NAS_WPA_CB_FLAG_AUTH;
	nwcb->nas.flags |= NAS_FLAG_AUTHENTICATOR;
#endif	/* #if defined(NAS_WKSP_BUILD_NAS_AUTH) && !defined(NAS_WKSP_BUILD_NAS_SUPPL) */
	nwcb->nas.wan = NAS_WKSP_UNK_FILE_DESC;
	nwcb->nas.server.sin_port = htons(RADIUS_PORT);
	nwcb->nas.wsec = TKIP_ENABLED|AES_ENABLED;
	nwcb->nas.wpa = &nwcb->wpa;
	nwcb->nas.appl = nwcb;
#ifdef BCMDBG
	nwcb->nas.debug = 1;
#endif
	nwcb->nas.disable_preauth = 0;
	nwcb->nas.ssn_to = 36000;	/* 10hrs */
	nwcb->wpa.nas = &nwcb->nas;

	/* interface unit */
	nwcb->unit = info.unit;
	NASDBG("new nwcb's unit %d\n", nwcb->unit);
	/* interface name */
	strncpy(nwcb->nas.interface, info.osifname, IFNAMSIZ);
	NASDBG("new nwcb's nas interface %s\n", nwcb->nas.interface);
	/* interface address */
	memcpy(nwcb->nas.ea.octet, info.ea, ETHER_ADDR_LEN);
	NASDBG("new nwcb's nas hwaddr %02x:%02x:%02x:%02x:%02x:%02x\n",
		nwcb->nas.ea.octet[0], nwcb->nas.ea.octet[1],
		nwcb->nas.ea.octet[2], nwcb->nas.ea.octet[3],
		nwcb->nas.ea.octet[4], nwcb->nas.ea.octet[5]);
	/* ssid info */
	strncpy(nwcb->nas.ssid, info.ssid, DOT11_MAX_SSID_LEN);
	NASDBG("new nwcb's nas ssid %s\n", nwcb->nas.ssid);
	/* nas auth mode */
	nwcb->nas.mode = info.akm;
	NASDBG("new nwcb's nas auth mode %d\n", nwcb->nas.mode);
	if (!nwcb->nas.mode) {
		NASDBG("%s: Ignored interface. Invalid NAS mode\n", info.osifname);
		free(nwcb);
		return NULL;
	}
	/* wsec encryption */
	nwcb->nas.wsec = info.wsec;
	NASDBG("new nwcb's nas wsec encryption mode %d\n", nwcb->nas.wsec);
	if (!nwcb->nas.wsec) {
		NASDBG("%s: Ignored interface. Invalid WSEC\n", info.osifname);
		free(nwcb);
		return NULL;
	}

	/* nas role setting */
#if defined(NAS_WKSP_BUILD_NAS_AUTH) && defined(NAS_WKSP_BUILD_NAS_SUPPL)
	nwcb->flags = info.flags;
	nwcb->nas.flags = info.flags;
#endif /* #if defined(NAS_WKSP_BUILD_NAS_AUTH) && defined(NAS_WKSP_BUILD_NAS_SUPPL) */
	if (info.flags & NAS_WPA_CB_FLAG_WDS) {
		nwcb->flags = info.flags;
		nwcb->nas.flags = info.flags;
	}
	NASDBG("new nwcb's nas flags %d\n", nwcb->nas.flags);

#ifdef NAS_WKSP_BUILD_NAS_SUPPL
	if (info.flags & NAS_WPA_CB_FLAG_SUPPL) {
		nas_get_wpaauth(&nwcb->nas, &info.akm);
		nwcb->nas.mode = info.akm;
		NASDBG("updating new nwcb's nas auth mode %d\n", nwcb->nas.mode);
		nas_get_wpawsec(&nwcb->nas, &info.wsec);
		/* SWWLAN-28198, filter out SES_OW_ENABLED, it is not a real crypto mode */
		nwcb->nas.wsec = (info.wsec & ~SES_OW_ENABLED);
		NASDBG("updating new nwcb's nas wsec %x\n", nwcb->nas.wsec);
	}
#endif /* NAS_WKSP_BUILD_NAS_SUPPL */

	/* remote address */
	memcpy(nwcb->nas.remote, info.remote, ETHER_ADDR_LEN);
	NASDBG("new nwcb's nas remote %02x:%02x:%02x:%02x:%02x:%02x\n",
		nwcb->nas.remote[0], nwcb->nas.remote[1],
		nwcb->nas.remote[2], nwcb->nas.remote[3],
		nwcb->nas.remote[4], nwcb->nas.remote[5]);
	/* user-supplied psk passphrase */
	strncpy((char *)nwcb->psk, info.psk, NAS_WKSP_MAX_USER_KEY_LEN);
	nwcb->psk[NAS_WKSP_MAX_USER_KEY_LEN] = 0;
	NASDBG("new nwcb's psk %s\n", nwcb->psk);

	/* user-supplied radius server secret */
	if (info.secret) {
		strncpy((char *)nwcb->secret, info.secret, NAS_WKSP_MAX_USER_KEY_LEN);
		nwcb->secret[NAS_WKSP_MAX_USER_KEY_LEN] = 0;
		NASDBG("new nwcb's secret %s\n", nwcb->secret);
	}
#ifdef NAS_WKSP_BUILD_NAS_AUTH
	nwcb->wpa.gtk_rekey_secs = info.gtk_rekey_secs;
	NASDBG("new nwcb's wpa gtk_rekey_sec %d\n", nwcb->wpa.gtk_rekey_secs);
	/* key index */
	nwcb->index = info.wep_index;
	NASDBG("new nwcb's wep index %d\n", nwcb->index);
	/* wep key */
	if (info.wep_key) {
		strncpy((char *)nwcb->wep, info.wep_key, NAS_WKSP_MAX_USER_KEY_LEN);
		nwcb->wep[NAS_WKSP_MAX_USER_KEY_LEN] = 0;
		NASDBG("new nwcb's wep %s\n", nwcb->wep);
	}
	/* radius server host/port */
#ifdef NAS_RADIUS
	/* update radius server address */
	if (info.radius_addr) {
		nwcb->nas.server.sin_family = AF_INET;
		nwcb->nas.server.sin_addr.s_addr = inet_addr(info.radius_addr);
		/* update radius server port number */
		nwcb->nas.server.sin_port = info.radius_port;
		NASDBG("new nwcb's nas radius server address %s, port %d\n",
			info.radius_addr, nwcb->nas.server.sin_port);
	}
#endif /* NAS_RADIUS */
	/* 802.1x session timeout/pmk cache duration */
	nwcb->nas.ssn_to = info.ssn_to;
	NASDBG("new nwcb's nas ssn timeout %d\n", nwcb->nas.ssn_to);
#endif	/* #ifdef NAS_WKSP_BUILD_NAS_AUTH */

#ifdef BCMDBG
	/* verbose - 0:no | others:yes */
	nwcb->nas.debug = info.debug;
	NASDBG("new nwcb's nas debug %d\n", nwcb->nas.debug);
#endif

	nwcb->nas.disable_preauth = (info.preauth == 0) ? 1 : 0;
	NASDBG("new nwcb's nas disable_preauth %d\n", nwcb->nas.disable_preauth);


	/* nas id */
	if (info.nas_id) {
		strncpy(nwcb->nas.nas_id, info.nas_id, MAX_NAS_ID_LEN);
		NASDBG("new nwcb's nas nas_id %s\n", nwcb->nas.nas_id);
	}


	/* save to list */
	nwksp->nwcb[nwksp->nwcbs++] = nwcb;

	return nwcb;
}

/* add new NAS instance */
nas_wpa_cb_t *
nas_wksp_add_nwcb(nas_wksp_t *nwksp, uint8 *mac, char *osifname)
{
	nas_wpa_cb_t *nwcb = NULL;

	/* retrieve wireless security infomation for this interface */
	 if (nwksp->nwcbs < NAS_WKSP_MAX_NUM_INTERFACES) {
		if (!(nwcb = nas_get_wsec(nwksp, mac, osifname))) {
			return NULL;
		}

#ifdef NAS_WKSP_BUILD_NAS_AUTH
		/* open connection to receive radius messages */
		if (CHECK_RADIUS(nwcb->nas.mode)) {
			/* open connection to radius server */
			if (NAS_RADIUS_OPEN(nwksp, nwcb) != 0)
				NASMSG("%s: open radius connection failed\n", nwcb->nas.interface);
		}
#endif	/* #ifdef NAS_WKSP_BUILD_NAS_AUTH */

		/* init new instance */
		nas_init_nas(nwksp, nwcb);
	}

	return nwcb;
}
#endif /* #ifdef NAS_WKSP_ON_DEMAND */

/* find NAS instance based on MAC address and i/f name */
nas_wpa_cb_t *
nas_wksp_find_nwcb(nas_wksp_t *nwksp, uint8 *mac, char *osifname, int mode)
{
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif
	nas_wpa_cb_t *nwcb = NULL;
	int i;

	for (i = 0; i < nwksp->nwcbs; i ++) {
		nwcb = nwksp->nwcb[i];
		assert(nwcb);

		if (!bcmp(mac, nwcb->nas.ea.octet, ETHER_ADDR_LEN)) {
			NASDBG("%s: mac %s osifname %s: \n",
			       __FUNCTION__, ether_etoa((uchar *)mac, eabuf),
			       osifname ? : "*");
			if (!osifname) {
				NASDBG("%s *: found %08x\n",
					ether_etoa((uchar *)mac, eabuf), (uint)nwcb);
				return nwcb;
			}
			else if (!strncmp(nwcb->nas.interface, osifname, BCM_MSG_IFNAME_MAX)) {
				NASDBG("%s %s: found %08x\n",
					ether_etoa((uchar *)mac, eabuf), osifname, (uint)nwcb);
				return nwcb;
			}
		}
	}

	if (mode == NAS_WKSP_NWCB_SEARCH_ENTER)
		nwcb = NAS_WKSP_ADD_NWCB(nwksp, mac, osifname);

	if (!nwcb)
		NASDBG("%s %s: find error\n", ether_etoa((uchar *)mac, eabuf), osifname ? : "*");

	return nwcb;
}

/*
** Return values are really improtant here please make sure you look
** thr'u the code before changing it. vx code depends on the return values.
** returnvalues
** 0,our packet otherwise not our packet
*/
int
nas_validate_wlpvt_message(int bytes, uint8 *dpkt)
{
	bcm_event_t *pvt_data;

	/* the message should be at least the header to even look at it */
	if (bytes < sizeof(bcm_event_t) + 2) {
		NASDBG("nas_validate_wlpvt_message: invalid length of message\n");
		goto error_exit;
	}
	pvt_data  = (bcm_event_t *)dpkt;
	if (ntohs(pvt_data->bcm_hdr.subtype) != BCMILCP_SUBTYPE_VENDOR_LONG) {
		NASDBG("%s: nas_validate_wlpvt_message: not vendor specifictype\n",
		       pvt_data->event.ifname);
		goto error_exit;
	}
	if (pvt_data->bcm_hdr.version != BCMILCP_BCM_SUBTYPEHDR_VERSION) {
		NASDBG("%s: nas_validate_wlpvt_message: subtype header version mismatch\n",
		pvt_data->event.ifname);
		goto error_exit;
	}
	if (ntohs(pvt_data->bcm_hdr.length) < BCMILCP_BCM_SUBTYPEHDR_MINLENGTH) {
		NASDBG("%s: nas_validate_wlpvt_message: subtype hdr length not even minimum\n",
		pvt_data->event.ifname);
		goto error_exit;
	}
	if (bcmp(&pvt_data->bcm_hdr.oui[0], BRCM_OUI, DOT11_OUI_LEN) != 0) {
		NASDBG("%s: nas_validate_wlpvt_message: not BRCM OUI\n", pvt_data->event.ifname);
		goto error_exit;
	}
	/* check for wl nas message types */
	switch (ntohs(pvt_data->bcm_hdr.usr_subtype)) {
		case BCMILCP_BCM_SUBTYPE_EVENT:
			/* wl nas message */
			/* if (pvt_data->version != BCM_MSG_VERSION) {
			 * atleast a debug message
			 * }
			 */
			break;
		default:
			goto error_exit;
			break;
	}
	return 0; /* good packet may be this is destined to us */
error_exit:
	return -1;
}

void
nas_eapol_message_dispatch(nas_wpa_cb_t *nwcb, void *eapol, int bytes)
{
	nas_t *nas = &nwcb->nas;

#if defined(NAS_WKSP_BUILD_NAS_AUTH) && defined(NAS_WKSP_BUILD_NAS_SUPPL)
	if (nwcb->flags & NAS_WPA_CB_FLAG_AUTH)
		eapol_dispatch(nas, eapol, bytes);
	else
		eapol_sup_dispatch(nas, eapol);
#elif defined(NAS_WKSP_BUILD_NAS_AUTH) /* Only support authenticator */
	eapol_dispatch(nas, eapol, bytes);
#else /* Only support supplicant */
	eapol_sup_dispatch(nas, eapol);
#endif  /* #if defined(NAS_WKSP_BUILD_NAS_AUTH) && defined(NAS_WKSP_BUILD_NAS_SUPPL) */

	return;
}

int
nas_handle_wlpvt_messages(nas_wpa_cb_t *nwcb, void *pkt, int bytes)
{
	bcm_event_t *dpkt;

	dpkt = (bcm_event_t *)pkt;
	NASDBG("received event of type : %d\n", ntohs(dpkt->event.event_type));
	switch (ntohs(dpkt->bcm_hdr.usr_subtype)) {
		case BCMILCP_BCM_SUBTYPE_EVENT:
			/* ingore EAPOL message encapsulated ini bcmevent packet */
			if ((ntohl(dpkt->event.event_type) != WLC_E_EAPOL_MSG)) {
				NASDBG("%s: recved wl wpa packet interface bytes: %d\n",
				       dpkt->event.ifname, bytes);
				driver_message_dispatch(&nwcb->nas, dpkt);
			}
		break;

		default: /* not a NAS supported message so return an error */
			NASDBG("%s: ERROR: recved unknown packet interface subtype "
						 "0x%x bytes: %d\n", dpkt->event.ifname,
						 ntohs(dpkt->bcm_hdr.usr_subtype), bytes);
			return -1;
	}

	return 0;
}

void
nas_wksp_clear_inited()
{
	nas_wksp_inited = 0;
}

int nas_wksp_is_inited()
{
	return nas_wksp_inited;
}
