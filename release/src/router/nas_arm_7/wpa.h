/*
 * WPA definitions
 *
 * Copyright (C) 2002 Broadcom Corporation
 *
 * $Id: wpa.h 450927 2014-01-23 14:13:36Z $
 */

#ifndef _wpa_h_
#define _wpa_h_

#include <typedefs.h>
#include <bcmtimer.h>
#include <proto/ethernet.h>
#include <proto/eapol.h>
#include <proto/wpa.h>
#include <wlioctl.h>
#define REPLAY_LEN		8
#define NONCE_LEN		32
#define PMK_LEN			32
#define GMK_LEN			32
#define KEY_COUNTER_LEN		32

#define WEP1_PTK_LEN		48
#define WEP1_TK_LEN		5
#define WEP128_PTK_LEN		48
#define WEP128_TK_LEN		13

#define TKIP_PTK_LEN		64
#define TKIP_TK_LEN		32

#define AES_PTK_LEN		48
#define AES_TK_LEN		16

#define MIC_KEY_LEN		16
#define MAX_WPA_IE		256

#define WPA_RETRY		7

/* WPA2 timeout initial values */
#define WPA2_DEFAULT_RETRY_MSECS  990
#define WPA2_DEFAULT_RETRY_SECS   0

typedef uint8 wpaie_buf_t[MAX_WPA_IE];

#define KEYAUTH_SHA1 4
#define KEYAUTH_SHA256 6

#define MFP_1X_AKM 5
#define MFP_PSK_AKM 6

/* GTK/IGTK index values:  needed for changes in trunk to
 * not affect AARDVARK; these definitiona moved to bcmwpa.h
 */
#ifndef GTK_INDEX_1
#define GTK_INDEX_1    1
#define GTK_INDEX_2    2

#define IGTK_INDEX_1   4
#define IGTK_INDEX_2   5
#endif /* GTK_INDEX_1 */

/* WPA states */
typedef enum {
	/* authenticator states */
	/* 4 way pkt exchange state machine */
	WPA_DISCONNECT,
	WPA_DISCONNECTED,
	WPA_INITIALIZE,
	WPA_AUTHENTICATION2,
	WPA_INITPMK,
	WPA_INITPSK,
	WPA_PTKSTART,
	WPA_PTKINITNEGOTIATING,
	WPA_PTKINITDONE,
	WPA_UPDATEKEYS,
	WPA_INTEGRITYFAILURE,
	WPA_KEYUPDATE,
	/* group key state machine */
	WPA_REKEYNEGOTIATING,
	WPA_KEYERRROR,
	WPA_REKEYESTABLISHED,
	/* Authenticator, group key */
	WPA_SETKEYS,
	WPA_SETKEYSDONE,
#ifdef BCMSUPPL
	/* supplicant states */
	WPA_SUP_DISCONNECTED,
	WPA_SUP_INITIALIZE,
	WPA_SUP_AUTHENTICATION,
	WPA_SUP_STAKEYSTARTP,
	WPA_SUP_STAKEYSTARTG,
	WPA_SUP_KEYUPDATE
#endif
} wpa_suppl_state_t;

#ifdef BCMSUPPL
typedef enum {
	EAPOL_SUP_PK_ERROR,
	EAPOL_SUP_PK_UNKNOWN,
	EAPOL_SUP_PK_MICOK,
	EAPOL_SUP_PK_MICFAILED,
	EAPOL_SUP_PK_MSG1,
	EAPOL_SUP_PK_MSG3,
	EAPOL_SUP_PK_DONE
} eapol_sup_pk_state_t;
#endif

/* Declare incomplete types so references needn't be "void *".  */
struct wpa;
struct nas;

#ifdef MFP
/* Integrity group key info */
typedef struct wsec_igtk_info {
	uint16 id;			/* key id */
	uint32 ipn_lo;		/* key IPN low 32 bits */
	uint16 ipn_hi;		/* key IPN high 16 bits */
	ushort key_len;
	uint8  key[BIP_KEY_SIZE];
} igtk_info_t;
#endif

/* WPA - supplicant */
typedef struct wpa_suppl {
	wpaie_buf_t assoc_wpaie;	/* WPA info element in assoc resp */
	uint16 assoc_wpaie_len;
	wpa_suppl_state_t state;	/* WPA state */
	wpa_suppl_state_t retry_state;	/* WPA state for retries */
	uint8 pmk[PMK_LEN];		/* pairwise master key */
	uint32 pmk_len;
	uint8 pmkid[WPA2_PMKID_LEN];
	uint16 ptk_len;			/* PTK len, used in PRF calculation */
	uint16 tk_len;			/* TK len, used when loading key into driver */
	uint16 desc;			/* key descriptor type */
	uint8 anonce[NONCE_LEN];
	uint8 snonce[NONCE_LEN];
	uint8 replay[REPLAY_LEN];       /* replay counter used by authenticator */
	uint8 replay_req[REPLAY_LEN];   /* replay counter from suppl req pkt */
	uint8 eapol_mic_key[16];  	/* Pair Wise transient Key */
	uint8 eapol_encr_key[16];
	uint8 temp_encr_key[16];
	uint8 temp_tx_key[8];
	uint8 temp_rx_key[8];
#ifdef BCMSUPPL
	/* need to differentiate message 1 and 3 in 4 way handshake */
	eapol_sup_pk_state_t pk_state;
#endif
} wpa_suppl_t;

/* This coalesces the WPA supplicant and RADIUS PAE structs.
 * Everything is needed in WPA mode, but the supplicant is not needed
 * in RADIUS mode and the pae is not needed in WPA_PSK mode.
 * Unneeded pieces could be malloc'ed in an initialization function.
 * Dynamic heap use is probably a bad idea.
 */
typedef struct nas_sta {
	ushort used;			/* flags use of item */
	ushort retries;			/* count retries for timeout */
	struct ether_addr ea;		/* STA's ethernet address */
	struct nas_sta *next;
	time_t last_use;		/* use timestamp */
	bcm_timer_id td;		/* timer modules cookie */
	struct nas *nas;		/* point back to nas */
	/* These two things might be allocated dynamicly... */
	pae_t pae;
	wpa_suppl_t suppl;
	/* WDS pairwise key initiator/requestor timeout timer */
	bcm_timer_id wds_td;
	uint32 mode;		/* Authentication mode */
	uint8 key_auth_type;  /* hash used for key auth (SHA256 or SHA1) */
	uint32 wsec;		/* Authenticator: supplicant requested mcast and unicast cryptos */
				/* Supplicant: supplicant user-cfg'd mcast and unicast cryptos */
	uint16 algo;		/* Supplicant: auth's mcast key algo when WEP as mcast crypto */
	uint16 flags;			/* runtime flags */
	uint32  wpa_msg_timeout_s;	/* WPA Messgae message timeout retry interval in seconds */
	uint32  wpa_msg_timeout_ms;	/* WPA Messgae message timeout retry interval in mseconds */
	uint32 listen_interval_ms;	/* Listen Interval from the Drivers point of view */
	/* deauth. timer for a delay before deauth. sta using wl ioctl */
	bcm_timer_id deauth_td;	/* deauthentication timer */
	uint16 rxauths;		/* reAuthCount */
	uint16 tx_when;		/* txWhen */
	uint16 auth_while;	/* authWhile */
	uint16 quiet_while;	/* quietWhile */
	uint8 eapol_version;	/* eapol version */
	/* handler for retransmission exceeding limit */
	void (*retx_exceed_hndlr)(struct nas_sta *sta);
	/* RC4 key replay counter */
	uint32 rc4keysec;	/* last key timestamp, initialized at state AUTHENTICATED */
	uint32 rc4keyusec;
	uint32 rc4keycntr;  /* use it with last key timestamp if gettimeofday doesn't return usec */

	uint32 MIC_failures;	/* supplicant countermeasures support */
	time_t prev_MIC_error;
#ifdef WLWNM
	uint8 sleeping;
	uint8 gtk_expire;
#endif /* WLWNM */
	/* WPA capabilities */
	uint8 cap[WPA_CAP_LEN];
} nas_sta_t;

/* nas_sta_t flags */

#define STA_FLAG_PRE_AUTH	0x0001	/* STA is doing pre-auth */
#define STA_FLAG_OSEN_AUTH	0x0002	/* STA is doing OSEN-auth */

/* WPA - Authenticator struct */
typedef struct wpa {
	uint8 global_key_counter[KEY_COUNTER_LEN];	/* global key counter */
	uint8 initial_gkc[KEY_COUNTER_LEN];		/* initial GKC value */
	uint8 pmk[PMK_LEN];				/* pairwise master key */
	uint  pmk_len;
	uint8 gmk[GMK_LEN];				/* group master key */
	uint8 gtk[TKIP_TK_LEN];				/* groupwise tmp key */
	uint8 gtk_encr[TKIP_TK_LEN];			/* groupwise tmp key, RC4 encrypted */
	uint8 gtk_rsc[8];
	uint  gtk_len;
	int   gtk_index;				/* where or whether gtk was plumbed */
	int   gtk_rekey_secs;				/* rotational period */
#ifdef MFP
	igtk_info_t igtk;
#endif
	int   ptk_rekey_secs;				/* rotational period */
	uint8 gnonce[NONCE_LEN];
	struct nas *nas;				/* back pointer to the nas struct */
	/* Interval timer descriptor for GTK updates.
	 * Non-zero means the timer is in use.  Check the analogous field
	 * of the wpa_t to see whether it should ever be set.
	 */
	bcm_timer_id gtk_rekey_timer;
	bcm_timer_id ptk_rekey_timer;
	bcm_timer_id countermeasures_timer;
	/* WPA capabilities */
	uint8 cap[WPA_CAP_LEN];
	/* WDS pairwise key initiator/requestor timeout interval */
	uint32 wds_to;
#ifdef NAS_GTK_PER_STA
	bool gtk_per_sta; /* unique GTK per STA */
#endif
} wpa_t;

extern int process_wpa(wpa_t *wpa, eapol_header_t *eapol, nas_sta_t *sta);
#ifdef BCMSUPPL
extern int process_sup_wpa(wpa_t *wpa, eapol_header_t *eapol, nas_sta_t *sta);
#endif
extern void initialize_global_key_counter(wpa_t *wpa);
extern void initialize_gmk(wpa_t *wpa);
extern int wpa_driver_assoc_msg(wpa_t *wpa, bcm_event_t *dpkt, nas_sta_t *sta);
extern int wpa_driver_disassoc_msg(wpa_t *wpa, bcm_event_t  *dpkt, nas_sta_t *sta);
extern void wpa_mic_error(wpa_t *wpa, nas_sta_t *sta, bool from_driver);
extern int wpa_set_suppl(wpa_t *wpa, nas_sta_t *sta, uint32 mode, uint32 wsec, uint32 algo);
#ifdef BCMSUPPL
extern void wpa_request(wpa_t *wpa, nas_sta_t *sta);
#endif
extern void wpa_start(wpa_t *wpa, nas_sta_t *sta);

extern int wpa_mode2auth(int mode);
extern int wpa_auth2mode(int auth);
extern void nas_wpa_calc_pmkid(wpa_t *wpa, nas_sta_t *sta);

typedef enum { ITIMER_OK = 0, ITIMER_CREATE_ERROR, ITIMER_CONNECT_ERROR,
	ITIMER_SET_ERROR } itimer_status_t;

/* Set an iterval timer. */
extern itimer_status_t wpa_set_itimer(bcm_timer_module_id module, bcm_timer_id *td,
                                      bcm_timer_cb handler, int handler_param,
                                      int secs, int msecs);

extern void wpa_stop_retx(nas_sta_t *sta);
extern void wpa_reset_countermeasures(wpa_t *wpa);

#ifdef NAS_GTK_PER_STA
extern void wpa_set_gtk_per_sta(wpa_t *wpa, bool gtk_per_sta);
#endif

extern void wpa_send_rekey_frame(wpa_t *wpa, nas_sta_t *sta);
extern void wpa_new_gtk(wpa_t *wpa);

#endif /* _wpa_h_ */
