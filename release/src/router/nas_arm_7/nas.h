/*
 * NAS definitions
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: nas.h 450927 2014-01-23 14:13:36Z $
 */

#ifndef _nas_h_
#define _nas_h_

#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>

#include <typedefs.h>
#include <proto/ethernet.h>
#include <proto/802.11.h>
#include <proto/eapol.h>
#include <proto/wpa.h>
#include <bcmtimer.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <shutils.h>
#include <wlif_utils.h>

#include <radius.h>



/* Debug macros */
#ifdef BCMDBG
#undef dbg
#define dbg(nas, fmt, args...) (\
{ \
	if (nas) { \
		nas_t *tmp = nas; \
		if (tmp->debug) { \
			fprintf(stderr, "%s: %s: " fmt "\n", __FUNCTION__, \
				tmp->interface , ## args); \
		} \
	} \
	else {\
		fprintf(stderr, "%s: " fmt "\n", __FUNCTION__ , ## args); \
	}\
} \
)
#define dump(nas, mem, size)	(\
{ \
	if (nas) { \
		nas_t *tmp = nas; \
		if (tmp->debug) \
			prhex("", mem, size); \
	} \
} \
)
#else
#undef dbg
#define dbg(nas, fmt, args...)
#define dump(nas, mem, size)
#endif /* BCMDBG */
#define err(nas, fmt, args...) (\
{ \
	if (nas) { \
		nas_t *tmp = nas; \
		printf("%s: %s: " fmt "\n", __FUNCTION__, tmp->interface , ## args); \
	} \
	else { \
		printf("%s: " fmt "\n", __FUNCTION__ , ## args); \
	} \
} \
)

/* Maximum number of supplicants */
#define MAX_SUPPLICANTS 128

typedef struct deauth_params
{
	uint8 	reason_code;
	uint16 	reauth_delay;
	char* 	reason_url;
	uint8	deauth_req;
} deauth_params_t;

typedef struct subrem_params
{
	char* 	subrem_url;
	uint8	subrem_req;
	uint8 serverMethod;
} subrem_params_t;

typedef struct bsstran_params
{
	uint8 	bsstran_reqmode;
	uint16	bsstran_swt;
	uint16	bsstran_dur;
	char* 	bsstran_url;
	uint8	bsstran_req;
} bsstran_params_t;

typedef struct binstring {
	unsigned int length;
	unsigned char *data;
} binstring_t;

/* PAE states */
typedef enum {
	INITIALIZE,
	DISCONNECTED,
	CONNECTING,
	AUTHENTICATING,
	ABORTING,
	HELD,
	AUTHENTICATED
} pae_state_t;

/* 802.1x Port Access Entity */
typedef struct pae {
	pae_state_t state;			/* PAE state */
	int id;					/* EAP current request ID */
	struct {
		binstring_t username;		/* RADIUS User Name */
		binstring_t state;		/* RADIUS State */
		radius_header_t *request;	/* Last request */
	} radius;
	struct pae *next;			/* Linked list */
	uint32 flags;
	uint32 ssnto;				/* session timeout */
} pae_t;

#define PAE_FLAG_EAP_SUCCESS		0x00000001
#define PAE_FLAG_RADIUS_ACCESS_REJECT	0x00000002

#define MAX_NAS_ID_LEN  32

#include "wpa.h"

/* Modes */
typedef enum
{
	WPA = WPA_AUTH_UNSPECIFIED,
	WPA_PSK = WPA_AUTH_PSK,
	WPA2 = WPA2_AUTH_UNSPECIFIED,
	WPA2_PSK = WPA2_AUTH_PSK,
	RADIUS = 0x20
} nas_mode_t;

/* RADIUS Network Access Server (NAS) */
typedef struct nas {
	char interface[IFNAMSIZ+1];		/* LAN interface name */
	char ssid[DOT11_MAX_SSID_LEN+1];	/* SSID */
	nas_mode_t mode;			/* 0:Radius, 1:WPA, 2:WPA-PSK */
	uint32 wsec;				/* crypto algorithm config, same as wl driver */
	struct ether_addr ea;			/* LAN Ethernet address */
	wpa_t *wpa;				/* WPA struct (NULL if wpa not in use) */
	int wan;				/* RADIUS interface handle */
	struct sockaddr_in client;		/* RADIUS interface IP address */
	struct sockaddr_in server;		/* RADIUS server IP address */
	binstring_t key;			/* PSK shared secret */
	unsigned int type;			/* RADIUS NAS Port Type */
	nas_sta_t sta[MAX_SUPPLICANTS];		/* STAs */
	nas_sta_t *sta_hashed[MAX_SUPPLICANTS];	/* STA cache */
	bcm_timer_module_id timer;		/* timer module ID */
	/* MIC error stuff needs to be per-interface */
	uint32 MIC_failures;		/* how many detected */
	bool MIC_countermeasures;		/* flags lock-out period */
	time_t prev_MIC_error;			/* seconds since last one */
	/* various flags see below */
	uint32 flags;
	/* wds remote address */
	uint8 remote[ETHER_ADDR_LEN];
	/* application data */
	void *appl;
#ifdef BCMDBG
	/* debug flag */
	bool debug;
#endif

	/* session timeout - global */
	uint32 ssn_to;
	bcm_timer_id watchdog_td;
	/* RADIUS shared secret */
	binstring_t secret;
	uint32  disable_preauth;	/* Internal Flags to disable the WPA2 preauth */
	uint32 auth_blockout_time;	/* seconds to block out client after auth. fail */
	char nas_id[MAX_NAS_ID_LEN+1];	/* nas mac address */

	deauth_params_t  m_deauth_params;
	subrem_params_t  m_subrem_params;
	bsstran_params_t m_bsstran_params;

} nas_t;

#define NAS_FLAG_SUPPLICANT	WLIFU_WSEC_SUPPL	/* nas is supplicant, exclusive */
#define NAS_FLAG_AUTHENTICATOR	WLIFU_WSEC_AUTH		/* nas is authenticator, exclusive */
#define NAS_FLAG_WDS		WLIFU_WSEC_WDS		/* nas in WDS mode */
#define NAS_FLAG_GTK_PLUMBED	0x40000000		/* GTK has been plumbed */
#define NAS_FLAG_IGTK_PLUMBED	0x80000000		/* IGTK has been plumbed */


/* Supplicant cache */
#define pae_hash(ea) \
((((unsigned char *) ea)[3] ^ ((unsigned char *) ea)[4] ^ ((unsigned char *) ea)[5]) & \
(MAX_SUPPLICANTS - 1))


/* Always clear the descriptor when deleting a timer! */
#define TIMER_DELETE(td)	{(void) bcm_timer_delete(td); td = 0;}

/* Driver specific */
extern int nas_send_wnm_on_radius_access_accept(nas_t *nas, char* url,
	struct ether_addr *ea, uint8 serverMethod);
extern int nas_send_wnm_deauthentication_imminent(nas_t *nas, uint8 reason,
	int16 reauthDelay, char* url, struct ether_addr *ea);
extern int nas_send_bss_transition_mgmt_req(nas_t *nas, char* url,
	uint8 reqmode, uint16 tbtt, uint16 dur, uint8 unicast);
extern int nas_authorize(nas_t *nas, struct ether_addr *ea);
extern int nas_deauthorize(nas_t *nas, struct ether_addr *ea);
extern int nas_deauthenticate(nas_t *nas, struct ether_addr *ea, int reason);
extern int nas_disassoc(nas_t *nas);
extern int nas_set_key(nas_t *nas, struct ether_addr *ea, unsigned char *key, int len, int index,
                       int tx_flag, uint32 hi, uint16 lo);
extern int nas_set_mode(nas_t *nas, int mode);
extern int nas_get_group_rsc(nas_t *nas, uint8 *buf, int index);
extern void nas_wl_init(nas_t *nas);
extern int nas_wl_tkip_countermeasures(nas_t *nas, int enable);
extern void nas_wl_cleanup(nas_t *nas);
extern int nas_set_ssid(nas_t *nas, char *ssid);
extern int nas_join_bss(nas_t *nas, char *ssid);
extern int nas_get_wpawsec(nas_t *nas, uint32 *wsec);
extern int nas_get_wpaauth(nas_t *nas, uint32 *wpa_auth);
extern int nas_get_wpacap(nas_t *nas, uint8 *cap);
extern int nas_get_wpa_ie(nas_t *nas, char *ret_buf, int ret_buf_len, uint32 sta_mode);

extern int nas_set_eventmsgs(nas_t *nas, uchar *msgs, int size);
extern int nas_get_eventmsgs(nas_t *nas, uchar *msgs, int size);
/* OS specific */
extern void nas_rand128(uint8 *rand128);
extern int nas_eapol_send_packet(nas_t *nas, struct iovec *frags, int nfrags);

/* Main dispatch functions */
extern void eapol_dispatch(nas_t *nas, eapol_header_t *eapol, int bytes);
#ifdef BCMSUPPL
extern void eapol_sup_dispatch(nas_t *nas, eapol_header_t *eapol);
#endif

extern int nas_preauth_send_packet(nas_t *nas, struct iovec *frags, int nfrags);
extern void preauth_dispatch(nas_t *nas, eapol_header_t *eapol, int bytes);

extern void driver_message_dispatch(nas_t *nas, bcm_event_t *dpkt);
extern void driver_message_sup_dispatch(nas_t *nas, bcm_event_t *dpkt);
extern void cleanup_sta(nas_t *nas, nas_sta_t *sta, int reason, int driver_signal);

typedef enum { SEARCH_ONLY, SEARCH_ENTER } sta_lookup_mode_t;

extern nas_sta_t *lookup_sta(nas_t *nas, struct ether_addr *sta,
	sta_lookup_mode_t mode);

extern void nas_start(nas_t *nas);
extern void nas_sleep_ms(uint ms);

extern void send_identity_req(nas_t *nas, nas_sta_t *sta);

extern void nas_reset_board(void);
extern int nas_handle_error(nas_t *nas, int error);
extern void nas_force_rekey(nas_t *nas);

extern void pae_state(nas_t *nas, nas_sta_t *sta, int state);
extern void eapol_key(nas_t *nas, nas_sta_t *sta,
	unsigned char *send_key, int send_key_len,
	unsigned char *recv_key, int recv_key_len,
	unsigned char *key, int key_len, int index, int unicast);
extern void fix_wpa(nas_t *nas, nas_sta_t *sta, char *key, int len);

/* bcmcrypto/hmac.c */
extern void hmac_md5(unsigned char* text, int text_len, unsigned char *key,
                     int key_len, unsigned char*digest);
extern void hmac_sha1(unsigned char *text, int text_len, unsigned char *key,
                      int key_len, unsigned char *digest);
extern int nas_send_brcm_event(nas_t *nas, uint8* mac, int reason);

#define MIC_RATE_LIMIT	60		/* seconds */

#define STA_DEAUTH_DELAY_MS 50	/* delay before call wl ioctl deauth */
#define STA_REAUTH_MAX		2	/* reAuthMax */
#define STA_TXPERIOD_MAX	30	/* max txPeriod in second */
#define STA_AUTHWHILE_MAX	60	/* max authWhile in second */
#define STA_QUIETWHILE_MAX	60	/* max quietWhile in second */

#define CHECK_NAS(mode) ((mode) & (WPA | WPA_PSK | WPA2 | WPA2_PSK))
#define CHECK_PSK(mode) ((mode) & (WPA_PSK | WPA2_PSK))
#define CHECK_WPA(mode) ((mode) & (WPA | WPA2))
#define CHECK_RADIUS(mode) ((mode) & (WPA | RADIUS | WPA2))
#define CHECK_AUTH(mode) ((mode) & (RADIUS | WPA | WPA_PSK | WPA2 | WPA2_PSK))

#endif /* _nas_h_ */
