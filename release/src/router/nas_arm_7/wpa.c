/*
 * WPA, WiFi Protected Access
 *
 * Copyright (C) 2002, 2003 Broadcom Corporation
 *
 * $Id: wpa.c 462635 2014-03-18 09:09:23Z $
 */

#include <typedefs.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <bcmendian.h>
#include <bcmutils.h>
#include <wlioctl.h>

#include <proto/ethernet.h>
#include <proto/802.11.h>
#include <proto/eapol.h>
#include <proto/eap.h>
#include <bcmcrypto/aeskeywrap.h>
#include <bcmcrypto/rc4.h>
#include <bcmcrypto/sha1.h>
#include <bcmwpa.h>
#include <bcmcrypto/aes.h>
#include <bcmcrypto/sha256.h>
#include <bcmcrypto/hmac_sha256.h>


#include <nas.h>
#include <wpa.h>

#ifdef __ECOS
#define timer_t		int
#endif

#define CHECK_WPA2(mode) ((mode) & (WPA2 | WPA2_PSK))

#define MAX_ARRAY 1
#define MIN_ARRAY 0

/* mapping from WEP key length to CRYPTO_ALGO_WEPXXX */
#define WEP_KEY2ALGO(len)	((len) == WEP1_KEY_SIZE ? CRYPTO_ALGO_WEP1 : \
				(len) == WEP128_KEY_SIZE ? CRYPTO_ALGO_WEP128 : \
				CRYPTO_ALGO_OFF)
/* mapping from WPA_CIPHER_XXXX to CRYPTO_ALGO_XXXX */
#define WPA_CIPHER2ALGO(cipher)	((cipher) == WPA_CIPHER_WEP_40 ? CRYPTO_ALGO_WEP1 : \
				(cipher) == WPA_CIPHER_WEP_104 ? CRYPTO_ALGO_WEP128 : \
				(cipher) == WPA_CIPHER_TKIP ? CRYPTO_ALGO_TKIP : \
				(cipher) == WPA_CIPHER_AES_CCM ? CRYPTO_ALGO_AES_CCM : \
				CRYPTO_ALGO_OFF)
#define CRYPTO_ALGO2CIPHER(algo)	((algo) == CRYPTO_ALGO_WEP1 ? WPA_CIPHER_WEP_40 : \
				(algo) == CRYPTO_ALGO_WEP128 ? WPA_CIPHER_WEP_104 : \
				(algo) == CRYPTO_ALGO_TKIP ? WPA_CIPHER_TKIP : \
				(algo) == CRYPTO_ALGO_AES_CCM ? WPA_CIPHER_AES_CCM : \
				WPA_CIPHER_NONE)
/* mapping from WPA_CIPHER_XXXX to wsec */
#define WPA_CIPHER2WSEC(cipher)	((cipher) == WPA_CIPHER_WEP_40 ? WEP_ENABLED : \
				(cipher) == WPA_CIPHER_WEP_104 ? WEP_ENABLED : \
				(cipher) == WPA_CIPHER_TKIP ? TKIP_ENABLED : \
				(cipher) == WPA_CIPHER_AES_CCM ? AES_ENABLED : \
				0)
/* PRF() expects to write its result sloppily. */
#define PRF_RESULT_LEN	80
#include <bcmcrypto/prf.h>

#ifndef WPA_AES_CMAC_CALC
/* needed for compatibility because router code is automerged. non-TOT ignores
 * mic length and assumes to be AES_BLOCK_SZ
 */
#define WPA_AES_CMAC_CALC(buf, buf_len, key, key_len, mic, mic_len) \
	aes_cmac_calc(buf, buf_len, key, key_len, mic)
#endif /* WPA_AES_CMAC_CALC */

static void wpa_calc_ptk(wpa_t *wpa, nas_sta_t *sta);
static void wpa_gen_gtk(wpa_t *wpa, nas_sta_t *sta);
static void wpa_plumb_gtk(wpa_t *wpa, int primary);
static bool wpa_encr_gtk(wpa_t *wpa, nas_sta_t *ssta, uint8 *encrypted, uint16 *encrypted_len);
#ifdef BCMSUPPL
static bool wpa_decr_gtk(wpa_t *wpa, nas_sta_t *sta, eapol_wpa_key_header_t *body);
#endif /* ifdef BCMSUPPL */
static bool wpa_insert_pmkid(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol, uint16 *data_len);
static bool wpa_insert_gtk(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol, uint16 *data_len);
static bool wpa_encr_key_data(wpa_t *wpa, nas_sta_t *sta, uint8 *buffer, uint16 *data_len);
#ifdef BCMSUPPL
static bool wpa_extract_gtk(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol);
static bool wpa_decr_key_data(wpa_t *wpa, nas_sta_t *sta, eapol_wpa_key_header_t *body);
#endif /* ifdef BCMSUPPL */
static itimer_status_t wpa_set_timeout(bcm_timer_id td, int secs, int msecs);
static bool wpa_check_mic(nas_sta_t *sta, eapol_header_t *eapol);
static bool wpa_send_eapol(wpa_t *wpa, nas_sta_t *sta);
static void wpa_incr_gkc(wpa_t *wpa);
static int wpa_verifystart(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol);
static int wpa_ptkstart(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol);
static int wpa_ptkinitnegotiating(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol);
static int wpa_ptkinitdone(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol);
static int wpa_ptkinitdone2(wpa_t *wpa, nas_sta_t *sta);
static void wpa_rekeyneg(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol);
static void wpa_setkeysdone(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol);
static void wpa_retransmission(bcm_timer_id td, nas_sta_t *sta);
static void wpa_new_gtk_callback(bcm_timer_id td, wpa_t *wpa);
extern int nas_get_stainfo(nas_t *nas, char *macaddr, int len, char *ret_buf, int ret_buf_len);

#ifdef MFP
#define MFP2 1
#define MFP_IGTK_REQUIRED(wpa, sta)	((wpa)->cap[0] & RSN_CAP_MFPC)
static void wpa_gen_igtk(wpa_t *wpa);
static void wpa_initialize_ipn(wpa_t *wpa);
#endif

/* Definitions for interval timer periods */
#define RETRY_SECS	1
#define RETRY_MSECS	0
#define NSECS_PER_MSEC	1000*1000

/* local prototypes */
static int wpa_find_mckey_algo(wpa_t *wpa, uint8 *ie, int ie_len);
static int wpa_new_ptk_callback(bcm_timer_id td, wpa_t *wpa);

/* Toggle GTK index.  Indices 1 - 3 are usable; spec recommends 1 and 2. */
#define GTK_NEXT_INDEX(wpa)	((wpa)->gtk_index == GTK_INDEX_1 ? GTK_INDEX_2 : GTK_INDEX_1)
/* Toggle IGTK index.  Indices 4 - 5 are usable per spec */
#define IGTK_NEXT_INDEX(wpa)	((wpa)->igtk.id == IGTK_INDEX_1 ? IGTK_INDEX_2 : IGTK_INDEX_1)

/* Set an iterval timer. */
itimer_status_t
wpa_set_itimer(bcm_timer_module_id module, bcm_timer_id *td,
               bcm_timer_cb handler, int handler_param, int secs, int msecs)
{
	struct itimerspec  timer;
	itimer_status_t ret = ITIMER_OK;

	timer.it_interval.tv_sec = secs;
	timer.it_interval.tv_nsec = msecs * NSECS_PER_MSEC;
	timer.it_value.tv_sec = secs;
	timer.it_value.tv_nsec = msecs * NSECS_PER_MSEC;

	if (bcm_timer_create(module, td))
		ret = ITIMER_CREATE_ERROR;
	else if (bcm_timer_connect(*td, handler, handler_param))
		ret = ITIMER_CONNECT_ERROR;
	else if (bcm_timer_settime(*td, &timer))
		ret = ITIMER_SET_ERROR;
	return ret;
}

static itimer_status_t
wpa_set_timeout(bcm_timer_id td, int secs, int msecs)
{
	struct itimerspec  timer;
	itimer_status_t ret = ITIMER_OK;

	if (msecs >= 1000) {
		secs = msecs / 1000;
		msecs -= (secs * 1000);
	}
	timer.it_interval.tv_sec = secs;
	timer.it_interval.tv_nsec = msecs * NSECS_PER_MSEC;
	timer.it_value.tv_sec = secs;
	timer.it_value.tv_nsec = msecs * NSECS_PER_MSEC;

	bcm_timer_change_expirytime(td, &timer);

	return ret;

}

/* remove the retransmission timer if any and do some clean up */
void
wpa_stop_retx(nas_sta_t *sta)
{
	if (sta->td)
		TIMER_DELETE(sta->td);
	sta->retx_exceed_hndlr = NULL;
}

/* ARGSUSED */
static void
wpa_retransmission(bcm_timer_id td, nas_sta_t *sta)
{
	nas_t *nas;
	wpa_suppl_state_t save_state;
	int reason;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif
	int retry_timer;

	if (sta) {
		nas = sta->nas;
		dbg(nas, "start for %s (%d)",
			ether_etoa((uchar *)&sta->ea, eabuf), sta->retries + 1);
	} else {
		dbg(NULL, "called with NULL sta");
		return;
	}

	if (nas->MIC_countermeasures)
		/* This can happen very early during countermeasures
		 * when nothing should go out.  Clean-up is elsewhere.
		 */
		return;

	else if (sta->suppl.state == WPA_SETKEYSDONE ||
		((nas->flags & NAS_FLAG_WDS) && sta->suppl.state == WPA_PTKINITDONE)) {
		/* Check STA state for race between interval timer firing and
		 *  receipt of response.
		 */
		wpa_stop_retx(sta);
		return;
	}

	/* error handling when retry limit is exceeded. */
	if (++sta->retries > WPA_RETRY) {
		/* don't toss the sta if it is part of the initiator/requestor */
		if (sta->wds_td) {
			/*
			* deauth the sta after the first batch retries and
			* give it a chance for the second batch retries.
			*/
			if (sta->retries == WPA_RETRY + 1) {
				dbg(nas, "deauthenticating %s",
				    ether_etoa((uchar *)&sta->ea, eabuf));
				nas_deauthenticate(nas, &sta->ea, DOT11_RC_4WH_TIMEOUT);
			}
			/* done the second batch retries as well */
			else if (sta->retries == (WPA_RETRY + 1) * 2) {
				/*
				* done for this time, it is up to the
				* initiator/requestor to reschedule the
				* next batch of retries.
				*/
				wpa_stop_retx(sta);
				return;
			}
			/* send eapol message */
			goto xmit;
		}
		/* Disassoc the supplicant unless there is a handler */
		if (sta->retx_exceed_hndlr) {
			sta->retx_exceed_hndlr(sta);
			wpa_stop_retx(sta);
			return;
		}
		/* force timeout so that cleanup_sta() can toss the sta for WPA2 */
		sta->pae.ssnto = 0;
		/* deauth the sta - also removes any unicast key we have
		 * for it and deallocates its suppl struct.
		 */
		reason = DOT11_RC_4WH_TIMEOUT;
		if (sta->suppl.retry_state == WPA_REKEYNEGOTIATING)
			reason = DOT11_RC_GTK_UPDATE_TIMEOUT;
		cleanup_sta(nas, sta, reason, 0);
		return;
	}

xmit:	/* Send eapol on to the air */
	/* increment the replay counter */
	wpa_incr_array(sta->suppl.replay, REPLAY_LEN);

	save_state = sta->suppl.state;
	sta->suppl.state = sta->suppl.retry_state;
	if (wpa_send_eapol(nas->wpa, sta) == FALSE) {
		wpa_stop_retx(sta);
	}
	if (((sta->mode & WPA2) || (sta->mode & WPA2_PSK))) {
		if (sta->td) {
			if (sta->retries == 1)
				retry_timer =  sta->listen_interval_ms / 2;
			else
				retry_timer =  sta->listen_interval_ms;
			if (retry_timer > WPA2_DEFAULT_RETRY_MSECS) {
				dbg(nas, "setting timeout to %d msec", retry_timer);
				wpa_set_timeout(sta->td, 0, retry_timer);
			}
		}
	}
	sta->suppl.state = save_state;

	dbg(nas, "done");
}

void
nas_wpa_calc_pmkid(wpa_t *wpa, nas_sta_t *sta)
{
	/* PMKID = HMAC-SHA1-128(PMK, "PMK Name" | AA | SPA) */
	uint8 data[128], digest[SHA256_DIGEST_LENGTH];
	uint8 prefix[] = "PMK Name";
	int data_len = 0;

	/* create the the data portion */
	bcopy(prefix, data, strlen((char *)prefix));
	data_len += strlen((char *)prefix);
	bcopy((uint8*)&wpa->nas->ea, &data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	bcopy((uint8*)&sta->ea, &data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;

	/* generate the pmkid */
#if defined(MFP) || defined(HSPOT_OSEN)
	if (sta->key_auth_type == KEYAUTH_SHA256) {
	  unsigned int digest_len = 0;
	  dbg(wpa->nas, "PMKID using SHA256");
	  hmac_sha256(sta->suppl.pmk, sta->suppl.pmk_len, data, data_len, digest, &digest_len);
	  dbg(wpa->nas, "PMKID using SHA256, len %u", digest_len);
	}
	else
#endif
	  hmac_sha1(data, data_len, sta->suppl.pmk, sta->suppl.pmk_len, digest);
	bcopy(digest, sta->suppl.pmkid, WPA2_PMKID_LEN);

	dbg(wpa->nas, "PMKID");
	dump(wpa->nas, sta->suppl.pmkid, WPA2_PMKID_LEN);

	dbg(wpa->nas, "done");
}

static void
wpa_calc_ptk(wpa_t *wpa, nas_sta_t *sta)
{
	unsigned char data[128], prf_buff[PRF_RESULT_LEN];
	unsigned char prefix[] = "Pairwise key expansion";
	int data_len = 0;
	uint8 *p;

	/* create the the data portion */
	p = wpa_array_cmp(MIN_ARRAY, (uint8*)&wpa->nas->ea, (uint8*)&sta->ea, ETHER_ADDR_LEN);
	bcopy(p ? (char*)p : (char*)&wpa->nas->ea, (char*)&data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;

	p = wpa_array_cmp(MAX_ARRAY, (uint8*)&wpa->nas->ea, (uint8*)&sta->ea, ETHER_ADDR_LEN);
	bcopy(p ? (char*)p : (char*)&wpa->nas->ea, (char*)&data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;

	p = wpa_array_cmp(MIN_ARRAY, (uint8*)&sta->suppl.snonce,
	                  (uint8*)&sta->suppl.anonce, NONCE_LEN);
	bcopy(p ? (char*)p : (char*)&sta->suppl.snonce, (char*)&data[data_len], NONCE_LEN);
	data_len += NONCE_LEN;

	p = wpa_array_cmp(MAX_ARRAY, (uint8*)&sta->suppl.snonce,
	                  (uint8*)&sta->suppl.anonce, NONCE_LEN);
	bcopy(p ? (char*)p : (char*)&sta->suppl.snonce, (char*)&data[data_len], NONCE_LEN);
	data_len += NONCE_LEN;

	/* generate the ptk */
	if (sta->suppl.ptk_len > TKIP_PTK_LEN) {
		err(wpa->nas, "ptk_len = %d", sta->suppl.ptk_len);
		nas_handle_error(wpa->nas, 1);
	}

	dbg(wpa->nas, "STA : SUPPL PMK \n");
	dump(wpa->nas, sta->suppl.pmk, sta->suppl.pmk_len);
#if defined(MFP) || defined(HSPOT_OSEN)
	if (sta->key_auth_type == KEYAUTH_SHA256) {
	  KDF(sta->suppl.pmk, sta->suppl.pmk_len, prefix, strlen((char *)prefix),
	      data, data_len, prf_buff, sta->suppl.ptk_len);
	  dbg(wpa->nas, "ptk with kdf SH256 \n");
	}
	else
#endif
	{
		fPRF(sta->suppl.pmk, sta->suppl.pmk_len, prefix, strlen((char *)prefix),
		     data, data_len, prf_buff, sta->suppl.ptk_len);
		dbg(wpa->nas, "ptk with kdf SHA1 \n");
	}

	memcpy(sta->suppl.eapol_mic_key, prf_buff, sta->suppl.ptk_len);
	dbg(wpa->nas, "done");
}

static void
wpa_gen_gtk(wpa_t *wpa, nas_sta_t *sta)
{
	unsigned char data[256], prf_buff[PRF_RESULT_LEN];
	unsigned char prefix[] = "Group key expansion";
	int data_len = 0;

	/* Select a mcast cipher */
	dbg(wpa->nas, "wpa->nas->wsec 0x%x", wpa->nas->wsec);
	switch (WPA_MCAST_CIPHER(wpa->nas->wsec, WEP_KEY2ALGO(wpa->gtk_len))) {
	case WPA_CIPHER_TKIP:
		dbg(wpa->nas, "TKIP");
		wpa->gtk_len = TKIP_TK_LEN;
		break;
	case WPA_CIPHER_AES_CCM:
		dbg(wpa->nas, "AES");
		wpa->gtk_len = AES_TK_LEN;
		break;
	default:
		dbg(wpa->nas, "not supported multicast cipher");
		return;
	}
	dbg(wpa->nas, "gtk_len %d", wpa->gtk_len);

	/* create the the data portion */
	bcopy((char*)&wpa->nas->ea, (char*)&data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	bcopy(wpa->global_key_counter, wpa->gnonce, NONCE_LEN);
	wpa_incr_gkc(wpa);
	bcopy((char*)&wpa->gnonce, (char*)&data[data_len], NONCE_LEN);
	data_len += NONCE_LEN;

	/* generate the GTK */
	fPRF(wpa->gmk, sizeof(wpa->gmk), prefix, strlen((char *)prefix),
	    data, data_len, prf_buff, wpa->gtk_len);
	memcpy(wpa->gtk, prf_buff, wpa->gtk_len);

	/* The driver clears the IV when it gets a new key, so
	 * clearing RSC should be consistent with that, right?
	 */
	memset(wpa->gtk_rsc, 0, sizeof(wpa->gtk_rsc));

	dbg(wpa->nas, "done");
}

static void
wpa_plumb_gtk(wpa_t *wpa, int primary)
{
	/* install the key */
	if (nas_set_key(wpa->nas, NULL, wpa->gtk, wpa->gtk_len,
	                wpa->gtk_index, primary, 0, 0) < 0) {
		err(wpa->nas, "invalid multicast key");
		nas_handle_error(wpa->nas, 1);
	}
	wpa->nas->flags |= NAS_FLAG_GTK_PLUMBED;
	dbg(wpa->nas, "done");
}

static void
wpa_set_gtk_timer(wpa_t *wpa)
{
	itimer_status_t ts;

	dbg(wpa->nas, "start, rekey timer = %d", wpa->gtk_rekey_secs);
	if (wpa->gtk_rekey_secs) {
		/* Set up an interval timer to invoke the rekey function. */
		ts = wpa_set_itimer(wpa->nas->timer, &wpa->gtk_rekey_timer,
		                    (bcm_timer_cb)wpa_new_gtk_callback,
		                    (int)wpa, wpa->gtk_rekey_secs, 0);
		if (ts != ITIMER_OK)
			dbg(wpa->nas, "GTK interval timer set failed, code %d", ts);
	}
	dbg(wpa->nas, "done");
}


/* The timer call-back expects to pass a timer descriptor, but there's
 * no use for it here.
 */
void
wpa_send_rekey_frame(wpa_t *wpa, nas_sta_t *sta)
{
	nas_t *nas = wpa->nas;
	itimer_status_t ts;

	/* Set state wpa_send_eapol uses for the group key message. */
	sta->suppl.retry_state = sta->suppl.state = WPA_REKEYNEGOTIATING;
	sta->retries = 0;
	wpa_stop_retx(sta);
	wpa_incr_array(sta->suppl.replay, REPLAY_LEN);

	if (wpa_send_eapol(wpa, sta) == FALSE) {
		cleanup_sta(nas, sta, DOT11_RC_UNSPECIFIED, 0);

	/* There is an OS on which nas has run where we got back from
	 * the send *after* the STA replied.  If that happens, don't
	 * set the interval timer.
	 */
	} else if (sta->suppl.state == WPA_REKEYNEGOTIATING) {
		/* Set a timeout for retransmission */
		ts = wpa_set_itimer(nas->timer, &sta->td, (bcm_timer_cb)wpa_retransmission,
			(int)sta, sta->wpa_msg_timeout_s, sta->wpa_msg_timeout_ms);
		if (ts != ITIMER_OK)
			dbg(nas, "set of GTK update retry interval timer failed, code %d",
			    ts);
	}
}

/* ARGSUSED */
static void
wpa_new_gtk_callback(bcm_timer_id td, wpa_t *wpa)
{
	int did_one = 0;
	nas_t *nas = wpa->nas;
	nas_sta_t *sta = nas->sta;

	dbg(nas, "start");

	do {
		/* Only a supplicant in the state of having keys
		 * already should be of interest for this purpose.
		 */
		if ((sta->used == 0) || (sta->suppl.state != WPA_SETKEYSDONE))
			continue;

		/* Found one, so cobble a new key. */
		if (!did_one) {
			wpa_gen_gtk(wpa, sta);
			wpa->gtk_index = GTK_NEXT_INDEX(wpa);
			wpa_plumb_gtk(wpa, 0);
#ifdef MFP2
			if (MFP_IGTK_REQUIRED(wpa, sta))
				wpa_gen_igtk(wpa);
#endif
			did_one = 1;
		}

#ifdef WLWNM
		/* don't update key if sta is sleeping */
		if (sta->sleeping) {
#ifdef BCMDBG
			char eabuf[ETHER_ADDR_STR_LEN];
			dbg(nas, "skip update key in sta %s due to its on sleeping",
			    ether_etoa((uchar *)&sta->ea, eabuf));
#endif
			/* mark sta gtk expired */
			sta->gtk_expire = TRUE;

			continue;
		}
#endif /* WLWNM */

		wpa_send_rekey_frame(wpa, sta);

	} while (++sta < (nas->sta + MAX_SUPPLICANTS));

	/* Make it primary after it's been sent to all known supplicants. */
	if (did_one)
		wpa_plumb_gtk(wpa, 1);
	else {
		/* If a GTK was neither sent nor plumbed, maybe we
		 * should revert to the state where there isn't one.
		 */
	}
	dbg(nas, "done");
}

/* When we want a new group key at a time other than when the rekey
 * interval timer goes off, we want to turn off the interval timer.
 *   Turning off that timer during a timer alarm callback can make a
 * mess of what the timer module knows of its event queue, so this is
 * a front-end for the cases when the work is not induced by the GTK
 * rekey interval timer.
 */
void
wpa_new_gtk(wpa_t *wpa)
{
	if (wpa->gtk_rekey_timer)
		TIMER_DELETE(wpa->gtk_rekey_timer);

	wpa_new_gtk_callback((bcm_timer_id) 0, wpa);
	/* Begin a new rekeying interval. */
	wpa_set_gtk_timer(wpa);
	dbg(wpa->nas, "done");
}

static void
wpa_init_gtk(wpa_t *wpa, nas_sta_t *sta)
{
	/* Cobble the key and plumb it. */
	wpa_gen_gtk(wpa, sta);
	wpa_plumb_gtk(wpa, 1);
	wpa_set_gtk_timer(wpa);

	dbg(wpa->nas, "done");
}

static bool
wpa_encr_gtk(wpa_t *wpa, nas_sta_t *sta, uint8 *encrypted, uint16 *encrypted_len)
{
	unsigned char data[256], encrkey[32];
	rc4_ks_t rc4key;
	int len = wpa->gtk_len;
#ifdef NAS_GTK_PER_STA
	uint8 gtk[TKIP_TK_LEN];
	if (wpa->gtk_per_sta) {
		bcopy(&sta->ea, gtk, ETHER_ADDR_LEN);
		bcopy(wpa->gtk+ETHER_ADDR_LEN, gtk+ETHER_ADDR_LEN, TKIP_TK_LEN-ETHER_ADDR_LEN);
	} else {
		bcopy(wpa->gtk, gtk, TKIP_TK_LEN);
	}
#endif

	/* encrypt the gtk using RC4 */
	switch (sta->suppl.desc) {
	case WPA_KEY_DESC_V1:
		dbg(wpa->nas, "v1");
		/* create the iv/ptk key */
		bcopy(&wpa->global_key_counter[KEY_COUNTER_LEN-16], encrkey, 16);
		bcopy(sta->suppl.eapol_encr_key, &encrkey[16], 16);

		/* copy the gtk into the encryption buffer */
#ifdef NAS_GTK_PER_STA
		bcopy(gtk, encrypted, len);
#else
		bcopy(wpa->gtk, encrypted, len);
#endif
		/* encrypt the gtk using RC4 */
		prepare_key(encrkey, 32, &rc4key);
		rc4(data, 256, &rc4key); /* dump 256 bytes */
		rc4(encrypted, len, &rc4key);
		break;
	case WPA_KEY_DESC_V2:
#ifdef MFP
	case WPA_KEY_DESC_V3:
#endif
#ifdef HSPOT_OSEN
	case WPA_KEY_DESC_OSEN:
#endif
		dbg(wpa->nas, "v2");
		/* pad gtk if needed - min. 16 bytes, 8 byte aligned */
		if (len < 16) {
#ifdef NAS_GTK_PER_STA
			bzero(&gtk[len], 16 - len);
#else
			bzero(&wpa->gtk[len], 16 - len);
#endif
			len = 16;
		}
		else if (len % AKW_BLOCK_LEN) {
#ifdef NAS_GTK_PER_STA
			bzero(&gtk[len], AKW_BLOCK_LEN - (len % AKW_BLOCK_LEN));
#else
			bzero(&wpa->gtk[len], AKW_BLOCK_LEN - (len % AKW_BLOCK_LEN));
#endif
			len += AKW_BLOCK_LEN - (len % AKW_BLOCK_LEN);
		}
#ifdef NAS_GTK_PER_STA
		if (aes_wrap(sizeof(sta->suppl.eapol_encr_key), sta->suppl.eapol_encr_key,
			len, gtk, encrypted)) {
#else
		if (aes_wrap(sizeof(sta->suppl.eapol_encr_key), sta->suppl.eapol_encr_key,
			len, wpa->gtk, encrypted)) {
#endif
			dbg(wpa->nas, "encrypt failed");
			return FALSE;
		}
		break;
	default:
		dbg(wpa->nas, "sta->suppl.desc = %d", sta->suppl.desc);
		return FALSE;
	}

	/* tell the calling func how long the encrypted data is */
	*encrypted_len = len;
	return TRUE;
}

#ifdef BCMSUPPL
static bool
wpa_decr_gtk(wpa_t *wpa, nas_sta_t *sta, eapol_wpa_key_header_t *body)
{
	unsigned char data[256], encrkey[32];
	rc4_ks_t rc4key;
	ushort key_info = ntohs(body->key_info), len;

	dbg(wpa->nas, "start");

	wpa->gtk_len = ntohs(body->key_len);
	len = ntohs(body->data_len);

	wpa->gtk_index = (key_info & WPA_KEY_INDEX_MASK) >> WPA_KEY_INDEX_SHIFT;
	dbg(wpa->nas, "gtk_index %d", wpa->gtk_index);

	switch (sta->suppl.desc) {
	case WPA_KEY_DESC_V1:
		/* decrypt the gtk using RC4 */
		dbg(wpa->nas, "v1");
		bcopy(body->iv, encrkey, 16);
		bcopy(sta->suppl.eapol_encr_key, &encrkey[16], 16);

		/* decrypt the gtk using RC4 */
		prepare_key(encrkey, 32, &rc4key);
		rc4(data, 256, &rc4key); /* dump 256 bytes */
		rc4(body->data, len, &rc4key);
		bcopy(body->data, wpa->gtk, wpa->gtk_len);
		break;
	case WPA_KEY_DESC_V2:
#ifdef MFP
	case WPA_KEY_DESC_V3:
#endif
#ifdef HSPOT_OSEN
	case WPA_KEY_DESC_OSEN:
#endif
		/* decrypt the gtk using AES */
		dbg(wpa->nas, "v2");
		if (aes_unwrap(sizeof(sta->suppl.eapol_encr_key), sta->suppl.eapol_encr_key,
			len, body->data, wpa->gtk)) {
			dbg(wpa->nas, "unencrypt failed");
			return FALSE;
		}
		break;
	default:
		return FALSE;
	}

	return TRUE;
}
#endif /* BCMSUPPL */
#ifdef MFP2
static int
wpa_insert_igtk(wpa_t *wpa, eapol_header_t *eapol, uint16 *data_len)
{
	uint16 len = *data_len;
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	eapol_wpa2_encap_data_t *data_encap;
	eapol_wpa2_key_igtk_encap_t *igtk_encap;

	if (!(wpa->nas->flags & NAS_FLAG_IGTK_PLUMBED)) {
		wpa_gen_igtk(wpa);
		wpa->nas->flags |= NAS_FLAG_IGTK_PLUMBED;
	} else {
		uint8 rsc[DOT11_WPA_KEY_RSC_LEN];
		if (nas_get_group_rsc(wpa->nas, rsc,  IGTK_ID_TO_WSEC_INDEX(wpa->igtk.id))) {
			memset(rsc, 0, sizeof(rsc));
			dbg(wpa->nas, "failed to find IGTK RSC");
		}
		wpa->igtk.ipn_lo = rsc[0] | rsc[1] << 8 |
			rsc[2] << 16 | rsc[3] << 24;
		wpa->igtk.ipn_hi = rsc[4] | rsc[5] << 8;
	}

	data_encap = (eapol_wpa2_encap_data_t *) (body->data + len);
	data_encap->type = DOT11_MNG_PROPR_ID;
	data_encap->length = (EAPOL_WPA2_ENCAP_DATA_HDR_LEN - TLV_HDR_LEN) +
	        EAPOL_WPA2_KEY_IGTK_ENCAP_HDR_LEN + wpa->igtk.key_len;
	bcopy(WPA2_OUI, data_encap->oui, DOT11_OUI_LEN);
	data_encap->subtype = WPA2_KEY_DATA_SUBTYPE_IGTK;
	len += EAPOL_WPA2_ENCAP_DATA_HDR_LEN;
	igtk_encap = (eapol_wpa2_key_igtk_encap_t *) (body->data + len);

	igtk_encap->key_id = wpa->igtk.id;
	*(uint32 *)igtk_encap->ipn = htol32(wpa->igtk.ipn_lo);
	*(uint16 *)(igtk_encap->ipn + 4) = htol16(wpa->igtk.ipn_hi);

	bcopy(wpa->igtk.key, igtk_encap->key, wpa->igtk.key_len);
	len += wpa->igtk.key_len + EAPOL_WPA2_KEY_IGTK_ENCAP_HDR_LEN;

	/* return the adjusted data len */
	*data_len = len;
	return TRUE;
}
#endif /* #ifdef MFP */

static bool
wpa_insert_gtk(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol, uint16 *data_len)
{
	int len = *data_len;
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	eapol_wpa2_encap_data_t *data_encap;
	eapol_wpa2_key_gtk_encap_t *gtk_encap;
#ifdef NAS_GTK_PER_STA
	uint8 gtk[TKIP_TK_LEN];
#endif

	dbg(wpa->nas, "data len before gtk %d", len);
	/* make sure we have a GTK plumbed */
	if (!(wpa->nas->flags & NAS_FLAG_GTK_PLUMBED))
		wpa_init_gtk(wpa, sta);
	else {
		if (nas_get_group_rsc(wpa->nas, &wpa->gtk_rsc[0], wpa->gtk_index)) {
			/* Don't use what we don't have. */
			memset(wpa->gtk_rsc, 0, sizeof(wpa->gtk_rsc));
			dbg(wpa->nas, "failed to find group key RSC");
		}
	}

#ifdef NAS_GTK_PER_STA
	if (wpa->gtk_per_sta) {
		bcopy(&sta->ea, gtk, ETHER_ADDR_LEN);
		bcopy(wpa->gtk+ETHER_ADDR_LEN, gtk+ETHER_ADDR_LEN, TKIP_TK_LEN-ETHER_ADDR_LEN);
	} else {
		bcopy(wpa->gtk, gtk, TKIP_TK_LEN);
	}
#endif

	/* insert GTK into eapol message */
	/*	body->key_len = htons(wpa->gtk_len); */
	/* key_len is PTK len, gtk len is implicit in encapsulation */
	data_encap = (eapol_wpa2_encap_data_t *) (body->data + len);
	data_encap->type = DOT11_MNG_PROPR_ID;
	data_encap->length = (EAPOL_WPA2_ENCAP_DATA_HDR_LEN - TLV_HDR_LEN) +
	        EAPOL_WPA2_KEY_GTK_ENCAP_HDR_LEN + wpa->gtk_len;
	bcopy(WPA2_OUI, data_encap->oui, DOT11_OUI_LEN);
	data_encap->subtype = WPA2_KEY_DATA_SUBTYPE_GTK;
	len += EAPOL_WPA2_ENCAP_DATA_HDR_LEN;

	gtk_encap = (eapol_wpa2_key_gtk_encap_t *) (body->data + len);
	gtk_encap->flags = (wpa->gtk_index << WPA2_GTK_INDEX_SHIFT) & WPA2_GTK_INDEX_MASK;
#ifdef NAS_GTK_PER_STA
	bcopy(gtk, gtk_encap->gtk, wpa->gtk_len);
#else
	bcopy(wpa->gtk, gtk_encap->gtk, wpa->gtk_len);
#endif
	len += wpa->gtk_len + EAPOL_WPA2_KEY_GTK_ENCAP_HDR_LEN;
	dbg(wpa->nas, "data len including gtk %d", len);

	/* copy in the gtk rsc */
	bcopy(wpa->gtk_rsc, body->rsc, sizeof(body->rsc));

	/* return the adjusted data len */
	*data_len = len;

	return (TRUE);
}

#ifdef BCMSUPPL
static bool
wpa_extract_gtk(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol)
{
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	eapol_wpa2_encap_data_t *data_encap;
	eapol_wpa2_key_gtk_encap_t *gtk_encap;

	int len = ntohs(body->data_len);
	ushort key_info = ntohs(body->key_info);

	/* look for a GTK */
	data_encap = wpa_find_gtk_encap(body->data, len);
	if (data_encap) {
		dbg(wpa->nas, "found gtk encap");
		gtk_encap = (eapol_wpa2_key_gtk_encap_t *)data_encap->data;
		/*		wpa->gtk_len = ntohs(body->key_len); */
		/* key_len is PTK len, gtk len is implicit in encap len */
		wpa->gtk_len = data_encap->length - ((EAPOL_WPA2_ENCAP_DATA_HDR_LEN - TLV_HDR_LEN) +
		                                     EAPOL_WPA2_KEY_GTK_ENCAP_HDR_LEN);
		dbg(wpa->nas, "gtk len %d", wpa->gtk_len);
		wpa->gtk_index = (gtk_encap->flags & WPA2_GTK_INDEX_MASK) >> WPA2_GTK_INDEX_SHIFT;
		dbg(wpa->nas, "gtk index %d", wpa->gtk_index);
		bcopy(gtk_encap->gtk, wpa->gtk, wpa->gtk_len);
		dbg(wpa->nas, "gtk:");
		dump(wpa->nas, wpa->gtk, wpa->gtk_len);
		if (!(key_info & WPA_KEY_ENCRYPTED_DATA)) {
			dbg(wpa->nas, "gtk wasn't encrypted!");
			return (FALSE);
		}
	} else {
		dbg(wpa->nas, "didn't find gtk encap");
		return (FALSE);
	}

	return (TRUE);
}
#endif /* BCMSUPPL */

static bool
wpa_insert_pmkid(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol, uint16 *data_len)
{
	int len = *data_len;
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	eapol_wpa2_encap_data_t *data_encap;
	uint8 *pmkid;

	dbg(wpa->nas, "data len before pmkid %d", len);

	/* insert PMKID into eapol message */
	data_encap = (eapol_wpa2_encap_data_t *) (body->data + len);
	data_encap->type = DOT11_MNG_PROPR_ID;
	data_encap->length = (EAPOL_WPA2_ENCAP_DATA_HDR_LEN - TLV_HDR_LEN) + WPA2_PMKID_LEN;
	bcopy(WPA2_OUI, data_encap->oui, DOT11_OUI_LEN);
	data_encap->subtype = WPA2_KEY_DATA_SUBTYPE_PMKID;
	len += EAPOL_WPA2_ENCAP_DATA_HDR_LEN;

	pmkid = (uint8 *) (body->data + len);
	bcopy(sta->suppl.pmkid, pmkid, WPA2_PMKID_LEN);
	len += WPA2_PMKID_LEN;
	dbg(wpa->nas, "data len including pmkid %d", len);

	/* return the adjusted data len */
	*data_len = len;

	return (TRUE);
}


static bool
wpa_encr_key_data(wpa_t *wpa, nas_sta_t *sta, uint8 *buffer, uint16 *data_len)
{
	unsigned char data[256], encrkey[32];
	rc4_ks_t rc4key;
	int len = *data_len;

	dbg(wpa->nas, "before encrypt (len=%d):", len);
	dump(wpa->nas, buffer, len);

	switch (sta->suppl.desc) {
	case WPA_KEY_DESC_V1:
		dbg(wpa->nas, "v1");
		/* create the iv/ptk key */
		bcopy(&wpa->global_key_counter[KEY_COUNTER_LEN-16], encrkey, 16);
		bcopy(sta->suppl.eapol_encr_key, &encrkey[16], 16);
		/* encrypt the key data */
		prepare_key(encrkey, 32, &rc4key);
		rc4(data, 256, &rc4key); /* dump 256 bytes */
		rc4(buffer, len, &rc4key);
		break;
	case WPA_KEY_DESC_V2:
#ifdef MFP
	case WPA_KEY_DESC_V3:
#endif
#ifdef HSPOT_OSEN
	case WPA_KEY_DESC_OSEN:
#endif
		dbg(wpa->nas, "v2");
		/* pad if needed - min. 16 bytes, 8 byte aligned */
		/* padding is 0xdd followed by 0's */
		if (len < 2*AKW_BLOCK_LEN) {
			buffer[len] = WPA2_KEY_DATA_PAD;
			bzero(&buffer[len+1], 2*AKW_BLOCK_LEN - (len+1));
			len = 2*AKW_BLOCK_LEN;
		} else if (len % AKW_BLOCK_LEN) {
			buffer[len] = WPA2_KEY_DATA_PAD;
			bzero(&buffer[len+1], AKW_BLOCK_LEN - ((len+1) % AKW_BLOCK_LEN));
			len += AKW_BLOCK_LEN - (len % AKW_BLOCK_LEN);
		}
		if (aes_wrap(sizeof(sta->suppl.eapol_encr_key), sta->suppl.eapol_encr_key,
			len, buffer, buffer)) {
			dbg(wpa->nas, "encrypt failed");
			return FALSE;
		}
		len += 8;
		break;
	default:
		dbg(wpa->nas, "unknown descriptor type sta->suppl.desc = %d", sta->suppl.desc);
		return FALSE;
	}

	dbg(wpa->nas, "after encrypt (len=%d):", len);
	dump(wpa->nas, buffer, len);

	/* tell the calling func how long the encrypted data is */
	*data_len = len;
	return TRUE;
}

#ifdef BCMSUPPL
static bool
wpa_decr_key_data(wpa_t *wpa, nas_sta_t *sta, eapol_wpa_key_header_t *body)
{
	unsigned char data[256], encrkey[32];
	rc4_ks_t rc4key;
	ushort len;

	len = ntohs(body->data_len);

	dbg(wpa->nas, "before decrypt:");
	dump(wpa->nas, body->data, len);

	switch (sta->suppl.desc) {
	case WPA_KEY_DESC_V1:
		dbg(wpa->nas, "v1");
		/* create the iv/ptk key */
		bcopy(body->iv, encrkey, 16);
		bcopy(sta->suppl.eapol_encr_key, &encrkey[16], 16);
		/* decrypt the key data */
		prepare_key(encrkey, 32, &rc4key);
		rc4(data, 256, &rc4key); /* dump 256 bytes */
		rc4(body->data, len, &rc4key);
		break;
	case WPA_KEY_DESC_V2:
#ifdef MFP
	case WPA_KEY_DESC_V3:
#endif
#ifdef HSPOT_OSEN
	case WPA_KEY_DESC_OSEN:
#endif
		dbg(wpa->nas, "v2");
		if (aes_unwrap(sizeof(sta->suppl.eapol_encr_key), sta->suppl.eapol_encr_key,
			len, body->data, body->data)) {
			dbg(wpa->nas, "unencrypt failed");
			return FALSE;
		}
		break;
	default:
		dbg(wpa->nas, "unknown descriptor type %d", sta->suppl.desc);
		return FALSE;
	}

	dbg(wpa->nas, "after decrypt:");
	dump(wpa->nas, body->data, len);

	return TRUE;
}
#endif /* BCMSUPPL */

static void
wpa_countermeasures_cb(timer_t td, nas_t *nas)
{
	nas->MIC_countermeasures = 0;
	(void) nas_wl_tkip_countermeasures(nas, FALSE);
	TIMER_DELETE(nas->wpa->countermeasures_timer);
}

void wpa_reset_countermeasures(wpa_t *wpa)
{
	if (wpa && wpa->countermeasures_timer)
		wpa_countermeasures_cb((timer_t)wpa->countermeasures_timer, wpa->nas);
}

/* compute mic for the eapol about to be sent - buffer must point to
 * the beginning of EAPOL header after the ethernet header (if any)
 * returns the length of mic copied in. note: out_mic may over lap
 * with buffer
 */
static size_t wpa_do_mic(nas_sta_t *sta, int kdver,
	uint8 *buf, size_t buf_len, uint8* out_mic, size_t mic_len)
{
	uint8 mic[SHA1HashSize]; /* 20 bytes, largest MIC */
	uint8 *key = sta->suppl.eapol_mic_key;

	assert(sizeof(mic) >= EAPOL_WPA_KEY_MIC_LEN);

	switch (kdver) {
	case WPA_KEY_DESC_V1:
		hmac_md5(buf, buf_len, key, MIC_KEY_LEN, mic);
		break;
	case WPA_KEY_DESC_V2:
		hmac_sha1(buf, buf_len, key, MIC_KEY_LEN, mic);
		break;
#ifdef MFP
	case WPA_KEY_DESC_V3:
		WPA_AES_CMAC_CALC(buf, buf_len, key, MIC_KEY_LEN, mic, mic_len);
		break;
#endif
#ifdef HSPOT_OSEN
	case WPA_KEY_DESC_OSEN:
		WPA_AES_CMAC_CALC(buf, buf_len, key, MIC_KEY_LEN, mic, mic_len);
		break;
#endif
	default:
		mic_len = 0;
		err(sta->nas, "unknown descriptor type %d", sta->suppl.desc);
		break;
	}

	if (mic_len > EAPOL_WPA_KEY_MIC_LEN)
		mic_len = EAPOL_WPA_KEY_MIC_LEN;

	if (mic_len)
		bcopy(mic, out_mic, mic_len);

	return mic_len;
}

/* send mic failure report - supplicant only */
static void wpa_send_mic_failure(wpa_t *wpa, nas_sta_t *sta)
{
	struct iovec frags;
	unsigned char buffer[EAPOL_HEADER_LEN + EAPOL_WPA_KEY_LEN];
	eapol_header_t *eapol = (eapol_header_t *)buffer;
	eapol_wpa_key_header_t *wpa_key = (eapol_wpa_key_header_t *)eapol->body;
	nas_t *nas = wpa->nas;
	ushort ki;
#ifdef BCMDBG
	char sta_eabuf[ETHER_ADDR_STR_LEN]; /* peer */
	char nas_eabuf[ETHER_ADDR_STR_LEN]; /* local */
#endif

	assert(nas->flags & NAS_FLAG_SUPPLICANT);
	dbg(nas, "sending mic failure report to %s from %s",
		ether_etoa((uchar *)&sta->ea, sta_eabuf),
		ether_etoa((uchar *)&nas->ea, nas_eabuf));

	/* increment replay counter */
	wpa_incr_array(sta->suppl.replay, REPLAY_LEN);

	bzero(buffer, sizeof(buffer));
	memcpy(&eapol->eth.ether_dhost, &sta->ea, ETHER_ADDR_LEN);
	memcpy(&eapol->eth.ether_shost, &nas->ea, ETHER_ADDR_LEN);
	eapol->eth.ether_type = htons(ETHER_TYPE_802_1X);
	eapol->version = sta->eapol_version;
	eapol->type = EAPOL_KEY;
	eapol->length = htons(EAPOL_WPA_KEY_LEN);

	wpa_key->type = (sta->mode & (WPA2_PSK | WPA2)) ? EAPOL_WPA2_KEY :
		EAPOL_WPA_KEY;
	bcopy(sta->suppl.replay, wpa_key->replay, REPLAY_LEN);
	ki = sta->suppl.desc;
	ki |= WPA_KEY_ERROR | WPA_KEY_REQ | WPA_KEY_MIC;
	wpa_key->key_info = hton16(ki);

	wpa_do_mic(sta, sta->suppl.desc, &buffer[sizeof(struct ether_header)],
		EAPOL_WPA_KEY_LEN + 4, wpa_key->mic, EAPOL_WPA_KEY_MIC_LEN);

	frags.iov_base = (caddr_t) buffer;
	frags.iov_len = EAPOL_HEADER_LEN + EAPOL_WPA_KEY_LEN;

	nas_eapol_send_packet(nas, &frags, 1);
	dbg(nas, "mic failure report sent");
}

void
wpa_mic_error(wpa_t *wpa, nas_sta_t *msta, bool from_driver)
{
	time_t now, since;
	nas_t *nas = wpa->nas;
	itimer_status_t ts;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif

	now = time(NULL);

	/* update nas counters etc */
	nas->MIC_failures++;
	since = now - nas->prev_MIC_error;
	nas->prev_MIC_error = now;

	dbg(nas, "%s MIC error %d at time %d, STA %s, %s",
		(nas->flags & NAS_FLAG_SUPPLICANT) ? "Supplicant" :
		((nas->flags & NAS_FLAG_AUTHENTICATOR) ? "Authenticator" :
		"Non-Suppl-Auth"), nas->MIC_failures,
	    (int) now, ether_etoa((uchar *)&msta->ea, eabuf),
	    (from_driver) ? "detected by driver" : "reported by STA");

	/* For supplicant, send mic failure report if TKIP is enabled
	 * The assumption here is that we are invoked for TKIP only.
	 * sta->wsec could be checked, but when only AES is configured
	 * in wsec without TKIP we currently accept TKIP for multicast
	 */
	if (nas->flags & NAS_FLAG_SUPPLICANT) {
		if (!from_driver)
			return;

		/* update sta counters */
		msta->MIC_failures++;
		since = now - msta->prev_MIC_error;
		msta->prev_MIC_error = now;

		wpa_send_mic_failure(wpa, msta);
		if (msta->MIC_failures != 0 && since < WPA_TKIP_CM_DETECT) {
			nas_sleep_ms(STA_DEAUTH_DELAY_MS);
			cleanup_sta(nas, msta, DOT11_RC_MIC_FAILURE, 0);
		}
		return;
	}

	/* If within the time limit we have to toss all the STAs. */
	if ((nas->MIC_failures != 0) && (since < WPA_TKIP_CM_DETECT)) {
		nas_sta_t *sta;

		dbg(nas, "Second MIC failure in %d seconds, taking countermeasures",
		    (int) since);
		nas->MIC_countermeasures = 1;
		nas_wl_tkip_countermeasures(nas, TRUE);
		ts = wpa_set_itimer(wpa->nas->timer, &wpa->countermeasures_timer,
		                    (bcm_timer_cb) wpa_countermeasures_cb,
		                    (int) nas, WPA_TKIP_CM_BLOCK, 0);
		if (ts != ITIMER_OK)
			dbg(nas, "Setting TKIP countermeasures interval timer failed, code %d", ts);

		/* Everybody out of the pool! */
		sta = nas->sta;
		do {
			if (sta->used)
				cleanup_sta(nas, sta, DOT11_RC_MIC_FAILURE, 0);
		} while (sta++ < nas->sta + MAX_SUPPLICANTS);

		/* Try getting them all with a broadcast. */
		/* nas_deauthenticate(nas, &ether_bcast); */

		/* Clobber any vestige of group keys. */
		if (wpa->gtk_rekey_timer)
			TIMER_DELETE(wpa->gtk_rekey_timer);
		nas_set_key(nas, NULL, NULL, 0, wpa->gtk_index, 0, 0, 0);
		nas_set_key(nas, NULL, NULL, 0, GTK_NEXT_INDEX(wpa), 0, 0, 0);
		nas->flags &= ~NAS_FLAG_GTK_PLUMBED;
	}
	dbg(nas, "done");
}

static bool
wpa_check_mic(nas_sta_t *sta, eapol_header_t *eapol)
{
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	unsigned char digest[MIC_KEY_LEN], mic[MIC_KEY_LEN];
	int mic_length;
	ushort key_info = ntohs(body->key_info);
	bool ret = FALSE;

	/* eapol pkt from the version field on */
	mic_length =  4 + EAPOL_WPA_KEY_LEN + ntohs(body->data_len);

	bcopy((char*)&body->mic, mic, MIC_KEY_LEN);
	bzero((char*)&body->mic, MIC_KEY_LEN);

	/* Create the MIC for the pkt */
	if (wpa_do_mic(sta, (key_info & (WPA_KEY_DESC_V1|WPA_KEY_DESC_V2)),
		&eapol->version, mic_length, digest, MIC_KEY_LEN) != MIC_KEY_LEN)
			goto done;

	/* compare received mic with generated */
	ret = (bcmp(digest, mic, MIC_KEY_LEN) == 0) ? TRUE : FALSE;

done:
	dbg(sta->nas, "%s", (ret ? "match" : "fail"));
	return ret;
}

static uint8
wpa_auth2akm(wpa_t *wpa, uint32 auth)
{
	switch (auth) {
	case WPA_AUTH_PSK:
		return RSN_AKM_PSK;
	case WPA_AUTH_UNSPECIFIED:
		return RSN_AKM_UNSPECIFIED;
	case WPA2_AUTH_PSK:
		return RSN_AKM_PSK;
	case WPA2_AUTH_UNSPECIFIED:
		return RSN_AKM_UNSPECIFIED;
	case WPA_AUTH_DISABLED:
	default:
		return RSN_AKM_NONE;
	}
}

static int
wpa_build_ie(wpa_t *wpa, uint32 wsec, uint32 algo, uint32 sta_mode,
	uint8 *buf, uint16 *len)
{
	wpa_suite_mcast_t *mcast = NULL;
	wpa_suite_ucast_t *ucast = NULL;
	wpa_suite_auth_key_mgmt_t *auth = NULL;
	uint16 count;
	uint16 tag_len;	/* tag length */
	uint16 buf_len;	/* buf length */
	int sup = wpa->nas->flags & NAS_FLAG_SUPPLICANT;
	int wds = wpa->nas->flags & NAS_FLAG_WDS;
	uint8 *cap;
	uint32 mode;

	buf_len = *len;
	*len = 0;
	mode = wpa->nas->mode;

	/* for now, only !wds and !sup */
	if (!wds && !sup) {
		uint8 *ptr = buf;
		uint16 bytes;

		if (sta_mode & (WPA2 | WPA2_PSK | WPA | WPA_PSK)) {
			/* get wpa ie */
			bytes = nas_get_wpa_ie(wpa->nas, (char*)ptr, buf_len, sta_mode);

			dbg(wpa->nas, "WPA IE");
			dump(wpa->nas, ptr, bytes);

			buf_len -= bytes;
			ptr += bytes;
		}

		/* update total wsec IE  length */
		*len = ptr - buf;
		return 0;
	}

	if ((mode & (WPA2 | WPA2_PSK)) && (sta_mode & (WPA2 | WPA2_PSK))) {
		wpa_rsn_ie_fixed_t *wpaie = (wpa_rsn_ie_fixed_t *)buf;

		/* fixed portion */
		if (buf_len < WPA_RSN_IE_FIXED_LEN)
			return -1;
		buf_len -= TLV_HDR_LEN;
		wpaie->tag = DOT11_MNG_RSN_ID;
		wpaie->version.low = (uint8)WPA2_VERSION;
		wpaie->version.high = (uint8)(WPA2_VERSION>>8);
		tag_len = WPA_RSN_IE_TAG_FIXED_LEN;

		/* multicast suite */
		if (tag_len + WPA_SUITE_LEN > buf_len)
			return -1;
		mcast = (wpa_suite_mcast_t *)&wpaie[1];
		if (wds) {
			bcopy(BRCM_OUI, mcast->oui, WPA_OUI_LEN);
			mcast->type = 0; /* no mcast cipher */
		}
		else {
			bcopy(WPA2_OUI, mcast->oui, WPA_OUI_LEN);
			if (sup) {
				if (WSEC_WEP_ENABLED(wsec)) {
					mcast->type = WPA_MCAST_CIPHER(wsec, algo);
				}
				else {
					/* supplicant has to follow authenticator's mcast algo */
					mcast->type = CRYPTO_ALGO2CIPHER(algo);
				}
			}
			else {
				mcast->type = WPA_MCAST_CIPHER(wsec, algo);
			}
		}
		tag_len += WPA_SUITE_LEN;

		/* unicast suite list */
		if (tag_len + WPA_IE_SUITE_COUNT_LEN > buf_len)
			return -1;
		ucast = (wpa_suite_ucast_t *)&mcast[1];
		count = 0;
		tag_len += WPA_IE_SUITE_COUNT_LEN;
		if (WSEC_AES_ENABLED(wsec)) {
			if (tag_len + WPA_SUITE_LEN > buf_len)
				return -1;
			bcopy(WPA2_OUI, ucast->list[count].oui, WPA_OUI_LEN);
			ucast->list[count].type = WPA_CIPHER_AES_CCM;
			tag_len += WPA_SUITE_LEN;
			count ++;
			if (sup || wds)
				goto uni2_done;
		}
		if (WSEC_TKIP_ENABLED(wsec)) {
			if (tag_len + WPA_SUITE_LEN > buf_len)
				return -1;
			bcopy(WPA2_OUI, ucast->list[count].oui, WPA_OUI_LEN);
			ucast->list[count].type = WPA_CIPHER_TKIP;
			tag_len += WPA_SUITE_LEN;
			count ++;
			if (sup || wds)
				goto uni2_done;
		}
		if (!count) {
			if (tag_len + WPA_SUITE_LEN > buf_len)
				return -1;
			bcopy(WPA2_OUI, ucast->list[count].oui, WPA_OUI_LEN);
			ucast->list[count].type = WPA_CIPHER_NONE;
			tag_len += WPA_SUITE_LEN;
			count ++;
		}
uni2_done:
		ucast->count.low = (uint8)count;
		ucast->count.high = (uint8)(count>>8);

		/* authenticated key management suite list */
		if (tag_len + WPA_IE_SUITE_COUNT_LEN > buf_len)
			return -1;
		auth = (wpa_suite_auth_key_mgmt_t *)&ucast->list[count];
		count = 0;
		tag_len += WPA_IE_SUITE_COUNT_LEN;
		if (mode & WPA2) {
			if (tag_len + WPA_SUITE_LEN > buf_len)
				return -1;
			bcopy(WPA2_OUI, auth->list[count].oui, WPA_OUI_LEN);
			auth->list[count].type = wpa_auth2akm(wpa, wpa_mode2auth(WPA2));
			tag_len += WPA_SUITE_LEN;
			count ++;
			if (sup || wds)
				goto akm2_done;
		}
		if (mode & WPA2_PSK) {
			if (tag_len + WPA_SUITE_LEN > buf_len)
				return -1;
			bcopy(WPA2_OUI, auth->list[count].oui, WPA_OUI_LEN);
			auth->list[count].type = wpa_auth2akm(wpa, wpa_mode2auth(WPA2_PSK));
			tag_len += WPA_SUITE_LEN;
			count ++;
			if (sup || wds)
				goto akm2_done;
		}
		if (!count) {
			if (tag_len + WPA_SUITE_LEN > buf_len)
				return -1;
			bcopy(WPA2_OUI, auth->list[count].oui, WPA_OUI_LEN);
			auth->list[count].type = wpa_auth2akm(wpa, WPA_AUTH_NONE);
			tag_len += WPA_SUITE_LEN;
			count ++;
		}
akm2_done:
		auth->count.low = (uint8)count;
		auth->count.high = (uint8)(count>>8);

		/* WPA capabilities */
		if (!wds) {
			cap = (uint8 *)&auth->list[count];
			cap[0] = wpa->cap[0];
			cap[1] = wpa->cap[1];
			tag_len += WPA_CAP_LEN;
		}

		/* update tag length */
		wpaie->length = (uint8)tag_len;
		dbg(wpa->nas, "WPA2 IE");
		dump(wpa->nas, buf, tag_len + TLV_HDR_LEN);
		buf += tag_len + TLV_HDR_LEN;

		/* update used buffer length */
		*len += TLV_HDR_LEN + tag_len;

		/* for wds use first found AKM no mix-mode anymore */
		if (wds)
			return 0;
	}
	if ((mode & (WPA | WPA_PSK)) && (sta_mode & (WPA | WPA_PSK))) {
		wpa_ie_fixed_t *wpaie = (wpa_ie_fixed_t *)buf;

		/* fixed portion */
		if (buf_len < WPA_IE_FIXED_LEN)
			return -1;
		buf_len -= TLV_HDR_LEN;
		wpaie->tag = DOT11_MNG_WPA_ID;
		bcopy(WPA_OUI, wpaie->oui, sizeof(wpaie->oui));
		wpaie->oui_type = 0x01;
		wpaie->version.low = (uint8)WPA_VERSION;
		wpaie->version.high = (uint8)(WPA_VERSION>>8);
		tag_len = WPA_IE_TAG_FIXED_LEN;

		/* multicast suite */
		if (tag_len + WPA_SUITE_LEN > buf_len)
			return -1;
		mcast = (wpa_suite_mcast_t *)&wpaie[1];
		if (wds) {
			bcopy(BRCM_OUI, mcast->oui, WPA_OUI_LEN);
			mcast->type = 0; /* no mcast cipher */
		}
		else {
			bcopy(WPA_OUI, mcast->oui, WPA_OUI_LEN);
			if (sup) {
				if (WSEC_WEP_ENABLED(wsec)) {
					mcast->type = WPA_MCAST_CIPHER(wsec, algo);
				}
				else {
					/* supplicant has to follow authenticator's mcast algo */
					mcast->type = CRYPTO_ALGO2CIPHER(algo);
				}
			}
			else {
				mcast->type = WPA_MCAST_CIPHER(wsec, algo);
			}
		}
		tag_len += WPA_SUITE_LEN;

		/* unicast suite list */
		if (tag_len + WPA_IE_SUITE_COUNT_LEN > buf_len)
			return -1;
		ucast = (wpa_suite_ucast_t *)&mcast[1];
		count = 0;
		tag_len += WPA_IE_SUITE_COUNT_LEN;
		if (WSEC_AES_ENABLED(wsec)) {
			if (tag_len + WPA_SUITE_LEN > buf_len)
				return -1;
			bcopy(WPA_OUI, ucast->list[count].oui, WPA_OUI_LEN);
			ucast->list[count].type = WPA_CIPHER_AES_CCM;
			tag_len += WPA_SUITE_LEN;
			count ++;
			if (sup || wds)
				goto uni_done;
		}
		if (WSEC_TKIP_ENABLED(wsec)) {
			if (tag_len + WPA_SUITE_LEN > buf_len)
				return -1;
			bcopy(WPA_OUI, ucast->list[count].oui, WPA_OUI_LEN);
			ucast->list[count].type = WPA_CIPHER_TKIP;
			tag_len += WPA_SUITE_LEN;
			count ++;
			if (sup || wds)
				goto uni_done;
		}
		if (!count) {
			if (tag_len + WPA_SUITE_LEN > buf_len)
				return -1;
			bcopy(WPA_OUI, ucast->list[count].oui, WPA_OUI_LEN);
			ucast->list[count].type = WPA_CIPHER_NONE;
			tag_len += WPA_SUITE_LEN;
			count ++;
		}
uni_done:
		ucast->count.low = (uint8)count;
		ucast->count.high = (uint8)(count>>8);

		/* authenticated key management suite list */
		if (tag_len + WPA_IE_SUITE_COUNT_LEN > buf_len)
			return -1;
		auth = (wpa_suite_auth_key_mgmt_t *)&ucast->list[count];
		count = 0;
		tag_len += WPA_IE_SUITE_COUNT_LEN;
		if (mode & WPA) {
			if (tag_len + WPA_SUITE_LEN > buf_len)
				return -1;
			bcopy(WPA_OUI, auth->list[count].oui, WPA_OUI_LEN);
			auth->list[count].type = wpa_auth2akm(wpa, wpa_mode2auth(WPA));
			tag_len += WPA_SUITE_LEN;
			count ++;
			if (sup || wds)
				goto akm_done;
		}
		if (mode & WPA_PSK) {
			if (tag_len + WPA_SUITE_LEN > buf_len)
				return -1;
			bcopy(WPA_OUI, auth->list[count].oui, WPA_OUI_LEN);
			auth->list[count].type = wpa_auth2akm(wpa, wpa_mode2auth(WPA_PSK));
			tag_len += WPA_SUITE_LEN;
			count ++;
			if (sup || wds)
				goto akm_done;
		}
		if (!count) {
			if (tag_len + WPA_SUITE_LEN > buf_len)
				return -1;
			bcopy(WPA_OUI, auth->list[count].oui, WPA_OUI_LEN);
			auth->list[count].type = wpa_auth2akm(wpa, WPA_AUTH_NONE);
			tag_len += WPA_SUITE_LEN;
			count ++;
		}
akm_done:
		auth->count.low = (uint8)count;
		auth->count.high = (uint8)(count>>8);

		/* WPA capabilities */
		if (!wds) {
			cap = (uint8 *)&auth->list[count];
			cap[0] = wpa->cap[0];
			cap[1] = wpa->cap[1];
			tag_len += WPA_CAP_LEN;
		}

		/* update tag length */
		wpaie->length = (uint8)tag_len;
		dbg(wpa->nas, "WPA IE");
		dump(wpa->nas, buf, tag_len + TLV_HDR_LEN);
		buf += tag_len + TLV_HDR_LEN;

		/* update used buffer length */
		*len += TLV_HDR_LEN + tag_len;
	}

	return 0;
}

static bool
wpa_send_eapol(wpa_t *wpa, nas_sta_t *sta)
{
	struct iovec frags;
	unsigned char buffer[1024];
	eapol_header_t *eapol = (eapol_header_t *)buffer;
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	int key_index, mic_length = 0;
	nas_t *nas = wpa->nas;
	uint16 data_len = 0;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif

	dbg(nas, "message for %s", ether_etoa((uchar *)&sta->ea, eabuf));

	bzero(buffer, 1024);

	memcpy(&eapol->eth.ether_dhost, &sta->ea, ETHER_ADDR_LEN);
	memcpy(&eapol->eth.ether_shost, &nas->ea, ETHER_ADDR_LEN);
	eapol->eth.ether_type = htons(ETHER_TYPE_802_1X);
	eapol->version = sta->eapol_version;
	eapol->type = EAPOL_KEY;
	eapol->length = EAPOL_WPA_KEY_LEN;

	frags.iov_base = (caddr_t) buffer;
	frags.iov_len = EAPOL_HEADER_LEN + EAPOL_WPA_KEY_LEN;

	/* Set the key type to WPA */
	if ((sta->mode & (WPA2_PSK | WPA2)))
		body->type = EAPOL_WPA2_KEY;
	else
		body->type = EAPOL_WPA_KEY;
	/* set the replay count */
	bcopy(sta->suppl.replay, body->replay, REPLAY_LEN);

	/* Fill in the WPA key descriptor */
	switch (sta->suppl.state) {
	case WPA_PTKSTART:
		dbg(nas, "WPA_PTKSTART");
		/* Set Key length */
		body->key_len = htons(sta->suppl.tk_len);
		/* Set the Key info flags */
		body->key_info = htons(sta->suppl.desc | WPA_KEY_ACK | WPA_KEY_PAIRWISE);
		/* set the nonce */
		bcopy(sta->suppl.anonce, body->nonce, NONCE_LEN);

		/* No PMKID for OSEN-AUTH */
		if ((sta->mode & (WPA2_PSK | WPA2)) &&
#ifdef HSPOT_OSEN
		    !(sta->flags & STA_FLAG_OSEN_AUTH) &&
#endif
		    TRUE) {
			wpa_insert_pmkid(wpa, sta, eapol, &data_len);
			body->data_len = htons(data_len);
			/* add the data field length (WPA IE) */
			frags.iov_len += data_len;
			eapol->length += data_len;
		}

		eapol->length = htons(eapol->length);
		break;

	case WPA_PTKINITNEGOTIATING:
		dbg(nas, "WPA_PTKINITNEGOTIATING");
		/* Set Key length */
		body->key_len = htons(sta->suppl.tk_len);
		/* Set the Key info flags */
		body->key_info = htons(sta->suppl.desc | WPA_KEY_INDEX_0 | WPA_KEY_ACK |
		                       WPA_KEY_PAIRWISE | WPA_KEY_INSTALL | WPA_KEY_MIC);
		if ((sta->mode & (WPA2 | WPA2_PSK)))
			body->key_info |= htons(WPA_KEY_SECURE | WPA_KEY_ENCRYPTED_DATA);
		if (nas->flags & NAS_FLAG_WDS)
			body->key_info |= htons(WPA_KEY_SECURE);

		/* set the nonce */
		bcopy(sta->suppl.anonce, body->nonce, NONCE_LEN);

		/* Copy the WPA IE sent in the beacon
		 * since the body eapol_wpa_key_header_t is a pointer masked over the buffer
		 * we need to retreive the actual size left
		 */
		data_len = sizeof(buffer) - (sizeof(eapol_wpa_key_header_t)  - sizeof(body->data));

		if (wpa_build_ie(wpa, nas->wsec, WEP_KEY2ALGO(wpa->gtk_len),
		                 sta->mode, body->data, &data_len)) {
			dbg(nas, "failed to build WPA IE");
			return FALSE;
		}

		if ((sta->mode & (WPA2 | WPA2_PSK))) {
			if (!wpa_insert_gtk(wpa, sta, eapol, &data_len)) {
				err(nas, "insertion of gtk failed");
				nas_handle_error(nas, 1);
			}
#ifdef MFP2
			if (MFP_IGTK_REQUIRED(wpa, sta)) {
				wpa_insert_igtk(wpa, eapol, &data_len);
				/* wpa_key->data_len = hton16(data_len); */
			}
#endif
			/* copy in the iv and encrypt the key data field */
			bcopy(&wpa->global_key_counter[KEY_COUNTER_LEN-16], body->iv, 16);
			if (!wpa_encr_key_data(wpa, sta, body->data, &data_len)) {
				err(nas, "encryption of key data failed");
				nas_handle_error(nas, 1);
			}
		}
		body->data_len = htons(data_len);
		/* add the data field length (WPA IE) */
		frags.iov_len += data_len;
		eapol->length += data_len;
		eapol->length = htons(eapol->length);

		/* How much data to MIC */
		mic_length = 4 + EAPOL_WPA_KEY_LEN + data_len;
		break;

	case WPA_REKEYNEGOTIATING:
		dbg(nas, "WPA_REKEYNEGOTIATING");

		body->key_info = htons(sta->suppl.desc | WPA_KEY_SECURE |
		                       WPA_KEY_ACK | WPA_KEY_MIC);

	  if ((sta->mode & (WPA2 | WPA2_PSK))) {
		if (!wpa_insert_gtk(wpa, sta, eapol, &data_len)) {
			err(nas, "insertion of gtk failed");
			nas_handle_error(nas, 1);
		}
#ifdef MFP2
		if (MFP_IGTK_REQUIRED(wpa, sta)) {
			wpa_insert_igtk(wpa, eapol, &data_len);
			/* wpa_key->data_len = hton16(data_len); */
		}
#endif
		/* copy in the iv and encrypt the key data field */
		bcopy(&wpa->global_key_counter[KEY_COUNTER_LEN-16], body->iv, 16);
		body->key_info |= htons(WPA_KEY_SECURE | WPA_KEY_ENCRYPTED_DATA);
		if (!wpa_encr_key_data(wpa, sta, body->data, &data_len)) {
			err(nas, "encryption of key data failed");
			nas_handle_error(nas, 1);
		}
	  } else {
		/* Set Key length */
		body->key_len = htons(wpa->gtk_len);
		/* Set the Key info flags */
		key_index = (wpa->gtk_index << WPA_KEY_INDEX_SHIFT) & WPA_KEY_INDEX_MASK;
		body->key_info |= htons(key_index);

		/* set the nonce */
		bcopy(wpa->gnonce, body->nonce, NONCE_LEN);

		/* copy in the key IV */
		bcopy(&wpa->global_key_counter[KEY_COUNTER_LEN-16], body->iv, 16);
		bcopy(wpa->gtk_rsc, body->rsc, sizeof(body->rsc));

		/* encrypt the gtk and put it in the data field */
		if (!wpa_encr_gtk(wpa, sta, body->data, &data_len)) {
			err(nas, "encryption of GTK failed");
			nas_handle_error(nas, 1);
		}
		switch (sta->suppl.desc) {
		case WPA_KEY_DESC_V1:
			break;
		case WPA_KEY_DESC_V2:
#ifdef MFP
		case WPA_KEY_DESC_V3:
#endif
#ifdef HSPOT_OSEN
		case WPA_KEY_DESC_OSEN:
#endif
			data_len += 8;
			break;
		default:
			err(nas, "unknown descriptor type %d", sta->suppl.desc);
			nas_handle_error(nas, 1);
		}
	  }
		body->data_len = htons(data_len);

		/* fix up the length */
		frags.iov_len += data_len;
		eapol->length += data_len;
		eapol->length = htons(eapol->length);
		/* How much data to MIC */
		mic_length = 4 + EAPOL_WPA_KEY_LEN + data_len;
		break;

#ifdef BCMSUPPL
	case WPA_SUP_INITIALIZE:
		dbg(nas, "WPA_SUP_INITIALIZE");
		/* Set Key length */
		body->key_len = htons(sta->suppl.tk_len);
		/* Set the Key info flags */
		body->key_info = htons(sta->suppl.desc | WPA_KEY_INDEX_0 |
		                       WPA_KEY_REQ | WPA_KEY_PAIRWISE);
		/* set the nonce */
		bcopy(sta->suppl.snonce, body->nonce, NONCE_LEN);
		eapol->length = htons(eapol->length);
		break;

	case WPA_SUP_STAKEYSTARTG:
	case WPA_SUP_STAKEYSTARTP:
		dbg(nas, "WPA_SUP_STAKEYSTARTP/G");
		/* Set Key length */
		body->key_len = htons(sta->suppl.tk_len);
		/* Set the Key info flags */
		body->key_info = htons(sta->suppl.desc | WPA_KEY_INDEX_0 | WPA_KEY_MIC);
		switch (sta->suppl.state) {
		case WPA_SUP_STAKEYSTARTP:
			body->key_info |= htons(WPA_KEY_PAIRWISE);
			switch (sta->suppl.pk_state) {
			case EAPOL_SUP_PK_MSG1:
				data_len = sizeof(body->data);
				if (wpa_build_ie(wpa, nas->wsec, sta->algo,
				                 sta->mode, body->data, &data_len)) {
					dbg(nas, "failed to build WPA IE");
					return FALSE;
				}
				body->data_len = htons(data_len);

				/* add the data field length (WPA IE) */
				frags.iov_len += data_len;
				eapol->length += data_len;

				/* expect message #3 after sending message #2 */
				sta->suppl.pk_state = EAPOL_SUP_PK_MSG3;
				break;
			case EAPOL_SUP_PK_MSG3:
				/* send message #4 and we are done with pairwise */
				if (nas->flags & NAS_FLAG_WDS)
					body->key_info |= htons(WPA_KEY_SECURE);
				if ((sta->mode & (WPA2 | WPA2_PSK)))
					body->key_info |= htons(WPA_KEY_SECURE);
				sta->suppl.pk_state = EAPOL_SUP_PK_DONE;
				break;
			default:
				dbg(nas, "Unexpected supplicant pk state %d", sta->suppl.pk_state);
				break;
			}
			break;
		case WPA_SUP_STAKEYSTARTG:
			body->key_info |= htons(WPA_KEY_SECURE);
			break;
		default:
			/* just to satisfy the compiler, no way to get here */
			break;
		}

		/* set the nonce */
		bcopy(sta->suppl.snonce, body->nonce, NONCE_LEN);

		eapol->length = htons(eapol->length);

		/* How much data to MIC */
		mic_length = 4 + EAPOL_WPA_KEY_LEN + data_len;

		switch (sta->suppl.desc) {
		case WPA_KEY_DESC_V1:
		case WPA_KEY_DESC_V2:
#ifdef MFP
		case WPA_KEY_DESC_V3:
#endif
#ifdef HSPOT_OSEN
		case WPA_KEY_DESC_OSEN:
#endif
			break;
		default:
			dbg(nas, "unknown descriptor type %d", sta->suppl.desc);
			if (!(nas->flags & NAS_FLAG_WDS)) {
				nas_disassoc(nas);
				nas_sleep_ms(500);
				nas_set_ssid(nas, nas->ssid);
			}
			return FALSE;
		}
		break;
#endif /* BCMSUPPL */

	default:
		/* error state; callers will bail */
		dbg(nas, "Unexpected supplicant state %d", sta->suppl.state);
		return FALSE;
	}

	if (sta->retries)
		dbg(nas, "eapol retry");

	if (mic_length)
		wpa_do_mic(sta, sta->suppl.desc,
			&buffer[sizeof(struct ether_header)], mic_length,
			body->mic, EAPOL_WPA_KEY_MIC_LEN);

	/* send the pkt */
	nas_eapol_send_packet(nas, &frags, 1);

	dbg(nas, "done");
	return (TRUE);
}

/* only handle single mode! */
int
wpa_mode2auth(int mode)
{
	switch (mode) {
	case WPA_PSK:
		return WPA_AUTH_PSK;
	case WPA:
		return WPA_AUTH_UNSPECIFIED;
	case WPA2_PSK:
		return WPA2_AUTH_PSK;
	case WPA2:
		return WPA2_AUTH_UNSPECIFIED;
	case RADIUS:
	default:
		return WPA_AUTH_DISABLED;
	}
}

/* only handle single mode! */
int
wpa_auth2mode(int auth)
{
	switch (auth) {
	case WPA_AUTH_PSK:
		return WPA_PSK;
	case WPA_AUTH_UNSPECIFIED:
		return WPA;
	case WPA2_AUTH_PSK:
		return WPA2_PSK;
	case WPA2_AUTH_UNSPECIFIED:
		return WPA2;
	case WPA_AUTH_DISABLED:
	default:
		return RADIUS;
	}
}

static uint32
wpa_akm2auth(uint32 akm)
{
	switch (akm) {
	case RSN_AKM_PSK:
		return WPA_AUTH_PSK;
	case RSN_AKM_UNSPECIFIED:
		return WPA_AUTH_UNSPECIFIED;
	case RSN_AKM_NONE:
	default:
		return WPA_AUTH_NONE;
	}
}

static uint32
wpa2_akm2auth(uint32 akm)
{
#ifdef MFP
	/* for PMF : clear the bit which is not relevant here  */
	akm &= ~0x4;
#endif

	switch (akm) {
	case RSN_AKM_PSK:
		return WPA2_AUTH_PSK;
	case RSN_AKM_UNSPECIFIED:
		return WPA2_AUTH_UNSPECIFIED;
	case RSN_AKM_NONE:
	default:
		return WPA_AUTH_NONE;
	}
}

/* decode WPA IE to retrieve supplicant wsec, auth mode, and pmk cached */
/* pmkc - 0:no pmkid in ie, -1:pmkid not found, 1:pmkid found */
static int
wpa_parse_ie(wpa_t *wpa, uint8 *ie, int ie_len, uint32 *wsec, uint32 *mode,
	uint32 *pmkc, nas_sta_t *sta)
{
	int type, len;
	wpa_suite_mcast_t *mcast = NULL;
	wpa_suite_ucast_t *ucast = NULL;
	wpa_suite_auth_key_mgmt_t *mgmt = NULL;
	uint8 *cap = NULL;
	uint8 *oui;
	wpa_pmkid_list_t *pmkid = NULL;
	uint16 count;
	uint32 m = 0;
	uint32 (*akm2auth)(uint32 akm) = NULL;
#ifdef HSPOT_OSEN
	bool osen_auth = FALSE;
#endif

	/* validate ie length */
	if (!bcm_valid_tlv((bcm_tlv_t *)ie, ie_len)) {
		dbg(wpa->nas, "IE is too short");
		return -1;
	}

	/* type specific header processing */
	switch ((type = *ie)) {
	case DOT11_MNG_RSN_ID: {
		wpa_rsn_ie_fixed_t *rsnie = (wpa_rsn_ie_fixed_t *)ie;
		if (rsnie->length < WPA_RSN_IE_TAG_FIXED_LEN) {
			dbg(wpa->nas, "invalid RSN IE header");
			return -1;
		}
		if (ltoh16_ua((uint8 *)&rsnie->version) != WPA2_VERSION) {
			dbg(wpa->nas, "unsupported RSN IE version");
			return -1;
		}
		mcast = (wpa_suite_mcast_t *)(rsnie + 1);
		len = ie_len - WPA_RSN_IE_FIXED_LEN;
		oui = (uint8*)WPA2_OUI;
		akm2auth = wpa2_akm2auth;
		break;
	}
	case DOT11_MNG_WPA_ID: {
		wpa_ie_fixed_t *wpaie = (wpa_ie_fixed_t *)ie;
		if (wpaie->length < WPA_IE_TAG_FIXED_LEN) {
			dbg(wpa->nas, "invalid WPA IE header");
			return -1;
		}

		/* WPA */
		if (!bcmp(wpaie->oui, WPA_OUI "\x01", WPA_IE_OUITYPE_LEN)) {
			if (ltoh16_ua((uint8 *)&wpaie->version) != WPA_VERSION) {
				dbg(wpa->nas, "unsupported WPA IE version");
				return -1;
			}
			mcast = (wpa_suite_mcast_t *)(wpaie + 1);
			len = ie_len - WPA_IE_FIXED_LEN;
			oui = (uint8*)WPA_OUI;
			akm2auth = wpa_akm2auth;
		}
#ifdef HSPOT_OSEN
		/* OSEN */
		else if (!bcmp(wpaie->oui, WFA_OUI, WFA_OUI_LEN) &&
			wpaie->oui_type == WFA_OUI_TYPE_OSEN) {
			uint8 *osen_ie = (uint8 *)wpaie;
			mcast = (wpa_suite_mcast_t *)(osen_ie + WFA_OSEN_IE_FIXED_LEN);
			len = ie_len - WFA_OSEN_IE_FIXED_LEN;
			oui = (uint8*)WFA_OUI;
			akm2auth = wpa2_akm2auth;
			osen_auth = TRUE;
		}
#endif
		else {
			dbg(wpa->nas, "invalid WPA IE header");
			return -1;
		}
		break;
	}
	default:
		dbg(wpa->nas, "unsupported IE type");
		return -1;
	}

	/* init return values - no mcast cipher and no ucast cipher */
	if (wsec)
		*wsec = 0;
	if (mode)
		*mode = 0;
	if (pmkc)
		*pmkc = 0;
	/* Check for multicast suite */
	if (len >= WPA_SUITE_LEN) {
		if (!bcmp(mcast->oui, oui, DOT11_OUI_LEN)) {
			if (wsec)
				*wsec |= WPA_CIPHER2WSEC(mcast->type);
		}
		len -= WPA_SUITE_LEN;
	}
	/* Check for unicast suite(s) */
#ifdef HSPOT_OSEN
	if (osen_auth)
		oui = (uint8*)WPA2_OUI;
#endif
	if (len >= WPA_IE_SUITE_COUNT_LEN) {
		ucast = (wpa_suite_ucast_t *)&mcast[1];
		count = ltoh16_ua((uint8 *)&ucast->count);
		len -= WPA_IE_SUITE_COUNT_LEN;
		if (count != 1) {
			dbg(wpa->nas, "# of unicast cipher suites %d", count);
			return -1;
		}
		if (!bcmp(ucast->list[0].oui, oui, DOT11_OUI_LEN)) {
			if (wsec)
				*wsec |= WPA_CIPHER2WSEC(ucast->list[0].type);
		}
		len -= WPA_SUITE_LEN;
	}
	/* Check for auth key management suite(s) */
#ifdef HSPOT_OSEN
	if (osen_auth)
		oui = (uint8*)WFA_OUI;
#endif
	if (len >= WPA_IE_SUITE_COUNT_LEN) {
		mgmt = (wpa_suite_auth_key_mgmt_t *)&ucast->list[1];
		count = ltoh16_ua((uint8 *)&mgmt->count);
		len -= WPA_IE_SUITE_COUNT_LEN;
		if (count != 1) {
			dbg(wpa->nas, "# of AKM suites %d", count);
			return -1;
		}
		if (!bcmp(mgmt->list[0].oui, oui, DOT11_OUI_LEN)) {
			uint16 keyauth_type = mgmt->list[0].type;
			uint8 sha_type = KEYAUTH_SHA1;
#ifdef MFP
			if (keyauth_type == MFP_1X_AKM || keyauth_type == MFP_PSK_AKM) {
				dbg(wpa->nas, "key auth using SHA256");
				sha_type = KEYAUTH_SHA256;
			}
#endif
#ifdef HSPOT_OSEN
			if (osen_auth && keyauth_type == OSEN_AKM_UNSPECIFIED) {
				dbg(wpa->nas, "key auth using SHA256");
				sha_type = KEYAUTH_SHA256;
			}
#endif
			m = wpa_auth2mode(akm2auth(keyauth_type));
			if (mode)
				*mode = m;
			if (sta)
				sta->key_auth_type = sha_type;
		}
		len -= WPA_SUITE_LEN;
	}

	if (wsec) {
		dbg(wpa->nas, "wsec 0x%x", *wsec);
	}
	if (mode) {
		dbg(wpa->nas, "mode 0x%x", *mode);
	}

	if (sta) {
#ifdef HSPOT_OSEN
		if (osen_auth)
			sta->flags |= STA_FLAG_OSEN_AUTH;
#endif
		memset(sta->cap, 0, sizeof(sta->cap));
	}

	/* Check for capabilities */
	if (len >= WPA_CAP_LEN) {
		cap = (uint8 *)&mgmt->list[1];
		if (sta) {
			sta->cap[0] = cap[0];
			sta->cap[1] = cap[1];
		}
		len -= WPA_CAP_LEN;
	}

	/* Check for PMKID(s) */
	dbg(wpa->nas, "checking for  PMK id");
	switch (type) {
	case DOT11_MNG_RSN_ID: {
		uint16 i;
		if (len < WPA_IE_SUITE_COUNT_LEN || m != WPA2 || !sta) {
			dbg(wpa->nas, "no PMK id");
			break;
		}
		if (pmkc)
			*pmkc = -1;
		pmkid = (wpa_pmkid_list_t *)(cap + WPA_CAP_LEN);
		count = ltoh16_ua((uint8 *)&pmkid->count);
		for (i = 0, len -= WPA_IE_SUITE_COUNT_LEN;
		     i < count && len >= WPA2_PMKID_LEN;
		     i ++, len -= WPA2_PMKID_LEN) {
			if (bcmp(pmkid->list[i], sta->suppl.pmkid, WPA2_PMKID_LEN))
				continue;
			dbg(wpa->nas, "found cached PMK id");
			dump(wpa->nas, sta->suppl.pmkid, WPA2_PMKID_LEN);
			if (pmkc)
				*pmkc = 1;
			break;
		}
		if (pmkc)
			if (*pmkc != 1)
				dbg(wpa->nas, "NO PMK cache hit");

		break;
	}
	default:
		dbg(wpa->nas, "no RSN id");
		break;
	}

	return 0;
}

#ifdef BCMSUPPL
/* decode WPA IE to verify authenticator wsec and auth mode */
/* return 0 if authenticator supports what we are configured for */
static int
wpa_check_ie(wpa_t *wpa, uint8 *ie, int ie_len, uint32 wsec, uint32 mode)
{
	int type, len;
	wpa_suite_mcast_t *mcast = NULL;
	wpa_suite_ucast_t *ucast = NULL;
	wpa_suite_auth_key_mgmt_t *mgmt = NULL;
	uint8 *oui;
	uint16 count = 0, i;
	uint32 (*akm2auth)(uint32 akm) = NULL;

	/* validate ie length */
	if (!bcm_valid_tlv((bcm_tlv_t *)ie, ie_len)) {
		dbg(wpa->nas, "IE is too short");
		return -1;
	}

	/* type specific header processing */
	switch ((type = *ie)) {
	case DOT11_MNG_RSN_ID: {
		wpa_rsn_ie_fixed_t *rsnie = (wpa_rsn_ie_fixed_t *)ie;
		if (rsnie->length < WPA_RSN_IE_TAG_FIXED_LEN) {
			dbg(wpa->nas, "invalid RSN IE header");
			return -1;
		}
		if (ltoh16_ua((uint8 *)&rsnie->version) != WPA2_VERSION) {
			dbg(wpa->nas, "unsupported RSN IE version");
			return -1;
		}
		mcast = (wpa_suite_mcast_t *)&rsnie[1];
		len = ie_len - WPA_RSN_IE_FIXED_LEN;
		oui = (uint8 *)WPA2_OUI;
		akm2auth = wpa2_akm2auth;
		break;
	}
	case DOT11_MNG_WPA_ID: {
		wpa_ie_fixed_t *wpaie = (wpa_ie_fixed_t *)ie;
		if (wpaie->length < WPA_IE_TAG_FIXED_LEN ||
		    bcmp(wpaie->oui, WPA_OUI, sizeof(wpaie->oui)) || wpaie->oui_type != 0x01) {
			dbg(wpa->nas, "invalid WPA IE header");
			return -1;
		}
		if (ltoh16_ua((uint8 *)&wpaie->version) != WPA_VERSION) {
			dbg(wpa->nas, "unsupported WPA IE version");
			return -1;
		}
		mcast = (wpa_suite_mcast_t *)&wpaie[1];
		len = ie_len - WPA_IE_FIXED_LEN;
		oui = (uint8 *)WPA_OUI;
		akm2auth = wpa_akm2auth;
		break;
	}
	default:
		dbg(wpa->nas, "unsupported IE type");
		return -1;
	}

	/* Check for multicast suite */
	if (len >= WPA_SUITE_LEN) {
		if (!bcmp(mcast->oui, oui, DOT11_OUI_LEN))
			wsec &= ~WPA_CIPHER2WSEC(mcast->type);
		len -= WPA_SUITE_LEN;
	}
	/* Check for unicast suite(s) */
	if (len >= WPA_IE_SUITE_COUNT_LEN) {
		ucast = (wpa_suite_ucast_t *)&mcast[1];
		count = ltoh16_ua((uint8 *)&ucast->count);
		for (i = 0, len -= WPA_IE_SUITE_COUNT_LEN;
		     i < count && len >= WPA_SUITE_LEN;
		     i ++, len -= WPA_SUITE_LEN) {
			if (!bcmp(ucast->list[i].oui, oui, DOT11_OUI_LEN))
				wsec &= ~WPA_CIPHER2WSEC(ucast->list[i].type);
		}
	}
	/* Check for auth key management suite(s) */
	if (len >= WPA_IE_SUITE_COUNT_LEN) {
		mgmt = (wpa_suite_auth_key_mgmt_t *)&ucast->list[count];
		count = ltoh16_ua((uint8 *)&mgmt->count);
		for (i = 0, len -= WPA_IE_SUITE_COUNT_LEN;
		     i < count && len >= WPA_SUITE_LEN;
		     i ++, len -= WPA_SUITE_LEN) {
			if (!bcmp(mgmt->list[i].oui, oui, DOT11_OUI_LEN))
				mode &= ~wpa_auth2mode(akm2auth(mgmt->list[i].type));
		}
	}

	return wsec + mode;
}
#endif /* BCMSUPPL */

/* parse WPA IE to retrieve key algo (converted from WPA cipher) */
static int
wpa_find_mckey_algo(wpa_t *wpa, uint8 *ie, int ie_len)
{
	int len;
	wpa_suite_mcast_t *mcast;
	uint8 *oui;

	/* validate ie length */
	if (!bcm_valid_tlv((bcm_tlv_t *)ie, ie_len)) {
		dbg(wpa->nas, "IE is too short");
		return -1;
	}

	/* type specific header processing */
	switch (*ie) {
	case DOT11_MNG_RSN_ID: {
		wpa_rsn_ie_fixed_t *rsnie = (wpa_rsn_ie_fixed_t *)ie;
		if (rsnie->length < WPA_RSN_IE_TAG_FIXED_LEN) {
			dbg(wpa->nas, "invalid RSN IE header");
			return -1;
		}
		if (ltoh16_ua((uint8 *)&rsnie->version) != WPA2_VERSION) {
			dbg(wpa->nas, "unsupported RSN IE version");
			return -1;
		}
		mcast = (wpa_suite_mcast_t *)(rsnie + 1);
		len = ie_len - WPA_RSN_IE_FIXED_LEN;
		oui = (uint8*)WPA2_OUI;
		break;
	}
	case DOT11_MNG_WPA_ID: {
		wpa_ie_fixed_t *wpaie = (wpa_ie_fixed_t *)ie;
		if (wpaie->length < WPA_IE_TAG_FIXED_LEN) {
			dbg(wpa->nas, "invalid WPA IE header");
			return -1;
		}

		/* WPA case */
		if (!bcmp(wpaie->oui, WPA_OUI "\x01", WPA_IE_OUITYPE_LEN)) {
			if (ltoh16_ua((uint8 *)&wpaie->version) != WPA_VERSION) {
				dbg(wpa->nas, "unsupported WPA IE version");
				return -1;
			}
			mcast = (wpa_suite_mcast_t *)(wpaie + 1);
			len = ie_len - WPA_IE_FIXED_LEN;
			oui = (uint8*)WPA_OUI;
		}
#ifdef HSPOT_OSEN
		/* OSEN */
		else if (!bcmp(wpaie->oui, WFA_OUI, WFA_OUI_LEN) &&
			wpaie->oui_type == WFA_OUI_TYPE_OSEN) {
			uint8 *osen_ie = (uint8 *)wpaie;
			mcast = (wpa_suite_mcast_t *)(osen_ie + WFA_OSEN_IE_FIXED_LEN);
			len = ie_len - WFA_OSEN_IE_FIXED_LEN;
			oui = (uint8*)WFA_OUI;
		}
#endif
		else {
			dbg(wpa->nas, "invalid WPA IE header");
			return -1;
		}
		break;
	}
	default:
		dbg(wpa->nas, "unsupported IE type");
		return -1;
	}

	/* Check for multicast suite */
	if (len >= WPA_SUITE_LEN) {
		if (!bcmp(mcast->oui, oui, DOT11_OUI_LEN))
			return WPA_CIPHER2ALGO(mcast->type);
	}

	return CRYPTO_ALGO_OFF;
}

/* setup supplicant info in sta stucture */
/*
 * wsec - crypto bitvec, WEP_ENABLED/TKIP_ENABLED/AES_ENABLED
 * algo - key algo, CRYPRO_ALGO_WEPXXXX, only when WEP_ENABLED
 */
int
wpa_set_suppl(wpa_t *wpa, nas_sta_t *sta, uint32 mode, uint32 wsec, uint32 algo)
{
	nas_t *nas = wpa->nas;
	/* check if we support the requested mode */
	if (mode != (mode & nas->mode)) {
		dbg(wpa->nas, "not supported mode 0x%x", mode);
		return 1;
	}
	/* check if we support the requested cipher */
	if (wsec != (wsec & nas->wsec)) {
		dbg(wpa->nas, "not supported wsec 0x%x", wsec);
		return 1;
	}
	/* update sta supplicant info */
#if defined(MFP) || defined(HSPOT_OSEN)
	if (sta->key_auth_type == KEYAUTH_SHA256) {
		dbg(wpa->nas, "PMF AES");
		sta->suppl.ptk_len = AES_PTK_LEN;
		sta->suppl.tk_len = AES_TK_LEN;
#ifdef HSPOT_OSEN
		if (sta->flags & STA_FLAG_OSEN_AUTH)
			sta->suppl.desc = WPA_KEY_DESC_OSEN;
		else
#endif
			sta->suppl.desc = WPA_KEY_DESC_V3;
	}
	else
#endif /* MFP || HSPOT_OSEN */
	if (WSEC_AES_ENABLED(wsec)) {
		dbg(wpa->nas, "AES");
		sta->suppl.ptk_len = AES_PTK_LEN;
		sta->suppl.tk_len = AES_TK_LEN;
		sta->suppl.desc = WPA_KEY_DESC_V2;
	}
	else if (WSEC_TKIP_ENABLED(wsec)) {
		dbg(wpa->nas, "TKIP");
		sta->suppl.ptk_len = TKIP_PTK_LEN;
		sta->suppl.tk_len = TKIP_TK_LEN;
		sta->suppl.desc = WPA_KEY_DESC_V1;
	}
	else {
		dbg(wpa->nas, "unsupported WPA unicast ciphers");
		return 1;
	}
	dbg(wpa->nas, "ptk_len %d tk_len %d", sta->suppl.ptk_len, sta->suppl.tk_len);
	sta->mode = mode;
	sta->eapol_version = WPA_EAPOL_VERSION;

	if (mode & (WPA2 | WPA2_PSK))
		sta->eapol_version = WPA2_EAPOL_VERSION;

	sta->wsec = wsec;
	sta->algo = algo;

	dbg(wpa->nas, "mode %d wsec %d algo %d", mode, wsec, algo);
	return 0;
}

/* This function does what we used to do with the EAPOL key message
 *   synthesized by the driver, except that now there's no EAPOL key
 * message.
 */
int
wpa_driver_assoc_msg(wpa_t *wpa, bcm_event_t *dpkt, nas_sta_t *sta)
{
	nas_t *nas = wpa->nas;
	wl_event_msg_t *event = &(dpkt->event);
	uint8 *data = (uint8 *)(event + 1);
	uint16 len = ntoh32(event->datalen);
	uint32 mode, wsec;
	itimer_status_t ts;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif
	uint32 algo, pmkc;
	wpa_ie_fixed_t *wpaie;
	uint8 *parse = data;
	int parse_len = len;
	int wpaie_len = 0;
	sta_info_t   *sta_info;
	char sta_info_buf[300] __attribute__ ((aligned(4)));

	if (!sta) {
		dbg(nas, "null passed to wpa_driver_assoc_msg as sta");
		return 1;
	}
	dbg(nas, "nas->wsec 0x%x nas->mode 0x%x", nas->wsec, nas->mode);

	/* reset WPA IE length */
	sta->suppl.assoc_wpaie_len = 0;

	/* search WPA IE  */
	wpaie = bcm_find_wpaie(parse, parse_len);
	/* search RSN IE */
	if (!wpaie)
		wpaie = (wpa_ie_fixed_t *)bcm_parse_tlvs(data, len, DOT11_MNG_RSN_ID);
#ifdef HSPOT_OSEN
	/* search OSEN IE */
	if (!wpaie)
		wpaie = (wpa_ie_fixed_t *)bcm_find_osenie(parse, parse_len);
#endif
	if (wpaie)
		wpaie_len = wpaie->length + TLV_HDR_LEN;

	/* non-WPA sta comes in without WPA IE */
	if (!wpaie_len && WSEC_WEP_ENABLED(nas->wsec)) {
		/* Authorize sta right away if we are configured to do WEP. */
		if (!(nas->mode & RADIUS)) {
			nas_set_key(nas, &sta->ea, nas->wpa->gtk, nas->wpa->gtk_len,
			            nas->wpa->gtk_index, 0, 0, 0);
			dbg(nas, "authorize %s (WEP)", ether_etoa((uchar *)&sta->ea, eabuf));
			nas_authorize(nas, &sta->ea);
			return 0;
		}
		/* 802.1x request */
		else {
			sta->mode = RADIUS;
			sta->wsec = WEP_ENABLED;
			sta->algo = CRYPTO_ALGO_WEP128;
			dbg(wpa->nas, "mode %d wsec %d algo %d", sta->mode,
				sta->wsec, sta->algo);
			return 0;
		}
	}

	/* WPA capabilities, used in IE contruction */
	if ((nas->flags & NAS_FLAG_SUPPLICANT) &&
	    nas_get_wpacap(nas, wpa->cap)) {
		dbg(nas, "WPA capabilities retrival failed");
		return 1;
	}

	/* If there was a stale timer descriptor, get rid of it. */
	wpa_stop_retx(sta);
	sta->retries = 0;

	/* Check supplicant WPA IE against NAS config */
	if (nas->flags & NAS_FLAG_AUTHENTICATOR) {
		if (wpa_parse_ie(wpa, (uint8*)wpaie, wpaie_len, &wsec, &mode, &pmkc, sta)) {
			dbg(nas, "parse supplicant WPA IE failed");
			dump(nas, data, len);
			return 1;
		}

	}
#ifdef BCMSUPPL
	/* check authenticator WPA IE to see if it supports our config */
	else {
		if (wpa_check_ie(wpa, (uint8*)wpaie, wpaie_len, nas->wsec, nas->mode)) {
			dbg(nas, "check authenticator WPA IE failed");
			dump(nas, data, len);
			return 1;
		}
		wsec = nas->wsec;
		mode = nas->mode;
	}
#endif /* BCMSUPPL */

	/* find mcast key algo. only needed when WEP as mcast key */
	algo = wpa_find_mckey_algo(wpa, (uint8*)wpaie, wpaie_len);

	/* update supplicant info */
	if (wpa_set_suppl(wpa, sta, mode, wsec, algo)) {
		dbg(nas, "update STA suppl struct failed");
		return 1;
	}

	sta->wpa_msg_timeout_s = RETRY_SECS;
	sta->wpa_msg_timeout_ms = RETRY_MSECS;

	if ((sta->mode & WPA2) || (sta->mode & WPA2_PSK)) {
		/* Query the Driver for the STA capabilities */
		bzero(sta_info_buf, 300);
		nas_get_stainfo(nas, (char *)&sta->ea, sizeof(struct ether_addr), sta_info_buf,
		                300);
		sta_info = (sta_info_t *)sta_info_buf;
		dbg(nas, "sta_info ver: %d listen_interval_ms: %dms",
			sta_info->ver, sta_info->listen_interval_inms);
		sta->wpa_msg_timeout_s = WPA2_DEFAULT_RETRY_SECS;
		sta->wpa_msg_timeout_ms = WPA2_DEFAULT_RETRY_MSECS;
		sta->listen_interval_ms = sta_info->listen_interval_inms;
	}
	/* Save the WPA IE */
	bcopy((uint8*)wpaie, sta->suppl.assoc_wpaie, wpaie_len);
	sta->suppl.assoc_wpaie_len = wpaie_len;

	if (CHECK_PSK(sta->mode)) {
		sta->suppl.pmk_len = wpa->pmk_len;
		bcopy(wpa->pmk, sta->suppl.pmk, wpa->pmk_len);
	}

#ifdef BCMSUPPL
	/* No need to start handshake for supplicant */
	if (nas->flags & NAS_FLAG_SUPPLICANT) {
		/* fill the snonce */
		bcopy(wpa->global_key_counter, sta->suppl.snonce, NONCE_LEN);
		wpa_incr_gkc(wpa);
		dbg(nas, "done");
		return 0;
	}
#endif /* BCMSUPPL */

	/* If STA wants TKIP and we're in countermeasures, toss
	 * it.  Driver will block these, but there's a race early
	 * in the period making this check is necessary.
	 */
	if (nas->MIC_countermeasures && WSEC_TKIP_ENABLED(wsec)) {
		cleanup_sta(nas, sta, DOT11_RC_MIC_FAILURE, 0);
		return 0;
	}

	/* Get the anonce */
	bcopy(wpa->global_key_counter, sta->suppl.anonce, NONCE_LEN);
	wpa_incr_gkc(wpa);

	/* The driver could have a stale pairwise key which would cause the
	 * exchange to be encrypted.  Be sure it's cleared.
	 */
	nas_set_key(nas, &sta->ea, NULL, 0, 0, 0, 0, 0);
	sta->suppl.retry_state = sta->suppl.state;
	sta->suppl.state = WPA_PTKSTART;

	/* WPA mode needs RADIUS server acceptance before beginning
	 * the 4-way handshake.
	 */
	if (sta->mode & WPA) {
		/* send Identity Request */
		send_identity_req(nas, sta);
		return 0;
	}
	/* WPA2 supports PMK caching. */
	else if (sta->mode & WPA2) {
		dbg(nas, "checking for a PMK Cache");
		/* pmk found and valid, start 4-way */
		if (pmkc == 1 && sta->pae.ssnto)
			;
		else {
			/* send Identity Request */
			send_identity_req(nas, sta);
			return 0;
		}
	}
	else if (sta->mode & WPA2_PSK) {
		/*
		 * Calculate the PMK ID for the PSK case.
		 * In the WPA2 case it gets done  when the Radius Accept is received
		*/
		nas_wpa_calc_pmkid(wpa, sta);
	}

	/* send the initial pkt in the 4 way exchange */
	sta->retries = 0;
	if (wpa_send_eapol(wpa, sta) == FALSE)
		return 1;

	wpa->ptk_rekey_secs = 0;
	wpa->ptk_rekey_timer = 0;

	/* move to next state */
	sta->suppl.retry_state = sta->suppl.state;
	sta->suppl.state = WPA_PTKINITNEGOTIATING;

	/* set a timeout for retransmission */
	ts = wpa_set_itimer(nas->timer, &sta->td, (bcm_timer_cb)wpa_retransmission,
	                    (int) sta, sta->wpa_msg_timeout_s, sta->wpa_msg_timeout_ms);
	if (ts != ITIMER_OK)
		dbg(nas, "Setting PTKINITNEGOTIATING interval timer failed, code %d", ts);

	dbg(nas, "done");
	return 0;
}

static int
wpa_new_ptk_callback(bcm_timer_id td, wpa_t *wpa)
{
	nas_t *nas = wpa->nas;
	nas_sta_t *sta = nas->sta;
	itimer_status_t ts;

	dbg(nas, "nas->wsec %d", nas->wsec);

	/* If there was a stale timer descriptor, get rid of it. */
	wpa_stop_retx(sta);
	sta->retries = 0;

	/* No need to start handshake for supplicant */
	if (nas->flags & NAS_FLAG_SUPPLICANT) {
		dbg(nas, "done");
		return 0;
	}

	/* Get the anonce */
	bcopy(wpa->global_key_counter, sta->suppl.anonce, NONCE_LEN);
	wpa_incr_gkc(wpa);

	sta->suppl.retry_state = sta->suppl.state;
	sta->suppl.state = WPA_PTKSTART;

	/* send the initial pkt in the 4 way exchange */
	sta->retries = 0;
	if (wpa_send_eapol(wpa, sta) == FALSE)
		return 1;

	/* move to next state */
	sta->suppl.retry_state = sta->suppl.state;
	sta->suppl.state = WPA_PTKINITNEGOTIATING;

	/* set a timeout for retransmission */
	ts = wpa_set_itimer(nas->timer, &sta->td, (bcm_timer_cb)wpa_retransmission,
		(int) sta, sta->wpa_msg_timeout_s, sta->wpa_msg_timeout_ms);
	if (ts != ITIMER_OK)
		dbg(nas, "Setting PTKINITNEGOTIATING interval timer failed, code %d", ts);

	dbg(nas, "done");
	return 0;
}

/*
* WDS notes:
*
* 1. We need to send out EAPOL key message with WPA_KEY_REQ bit set to
*    the originating STA if we are supplicant so that the authenticator
*    on that STA can initiate the key exchange.
* 2. We need to initiate key exchange with the originating STA if we are
*    authenticator and no key exchange is currently in progress.
*/
int wpa_driver_disassoc_msg(wpa_t *wpa,  bcm_event_t *dpkt, nas_sta_t *sta)
{
	wl_event_msg_t *event = &(dpkt->event);
	uint8 *data = (uint8 *)(event + 1);
	uint16 len = ntoh32(event->datalen);

	nas_t *nas = wpa->nas;
	uint16 reason;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif

	/* non-WDS processing */
	if (!(nas->flags & NAS_FLAG_WDS)) {
		/* free sta */
		return 1;
	}

	/* we must check reason code and then decide what to do */
	if (len != 2) {
		dbg(nas, "reason code length error in driver disassoc message");
		return 1;
	}
	/* TODO:reason = *(uint16 *)(dpkt+1); */
	reason = *(uint16 *) data;
	reason = ntoh16(reason);
	if (reason != DOT11_RC_AUTH_INVAL) {
		dbg(nas, "reason code is not DOT11_RC_AUTH_INVAL");
		return 1;
	}

	/* use existing sta or create a new one */
	if (!sta) {
		sta = lookup_sta(nas, (struct ether_addr *)&dpkt->eth.ether_shost,
		                 SEARCH_ENTER);
		if (!sta) {
			dbg(nas, "sta %s not available", ether_etoa((uchar *)&dpkt->eth.ether_shost,
			                                            eabuf));
			return 1;
		}
	}

	/* don't interrupt on-going negotiation */
	if (sta->td) {
		dbg(nas, "sta %s is busy", ether_etoa((uchar *)&dpkt->eth.ether_shost, eabuf));
		return 0;
	}


#ifdef BCMSUPPL
	/* we are supplicant */
	if (nas->flags & NAS_FLAG_SUPPLICANT)
		wpa_request(wpa, sta);
#endif
	/* we are authenticator and no key exchange in progress */
	if (nas->flags & NAS_FLAG_AUTHENTICATOR)
		wpa_start(wpa, sta);

	dbg(nas, "done");
	return 0;
}

/*  */
static int
wpa_verifystart(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol)
{
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	nas_t *nas = wpa->nas;
	uint32 mode;

	/* Is this a eapol-wpa-key request pkt */
	if ((ntohs(body->key_info) & WPA_KEY_REQ) == 0) {
		dbg(nas, "This was not a request pkt");
		return 1;
	}

	/* ensure the timer was cleared */
	wpa_stop_retx(sta);

	/* check supplicant auth mode. */
	if (wpa_parse_ie(wpa, body->data, ntohs(body->data_len), NULL, &mode, NULL, NULL)) {
		dbg(nas, "parse supplicant WPA IE failed");
		return 1;
	}
	if ((mode & nas->mode) != mode) {
		dbg(nas, "unsupported mode 0x%x", mode);
		return 1;
	}

	/* Save the WPA IE */
	bcopy(body->data, sta->suppl.assoc_wpaie, ntohs(body->data_len));
	sta->suppl.assoc_wpaie_len = ntohs(body->data_len);

	/* Get the anonce */
	bcopy(wpa->global_key_counter, sta->suppl.anonce, NONCE_LEN);
	wpa_incr_gkc(wpa);

	/* Move to next state */
	sta->suppl.retry_state = sta->suppl.state;
	sta->suppl.state = WPA_PTKSTART;
	sta->retries = 0;
	return 0;
}


static int
wpa_ptkstart(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol)
{
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	unsigned short key_info;
	itimer_status_t ts;

	/* Is this a key request? */
	if (!((key_info = ntohs(body->key_info)) & WPA_KEY_REQ)) {
		dbg(wpa->nas, "invalid EAPOL WPA key request");
		return 0;
	}
	if ((key_info & WPA_KEY_ERROR) != 0) {
		dbg(wpa->nas, "KEY_ERROR flag set during PTKINIT");
		return 1;
	}
	/* ensure the timer was cleared */
	wpa_stop_retx(sta);

	/* send the initial pkt in the 4 way exchange */
	sta->retries = 0;
	if (wpa_send_eapol(wpa, sta) == FALSE)
		return 1;

	/* move to next state */
	sta->suppl.retry_state = sta->suppl.state;
	sta->suppl.state = WPA_PTKINITNEGOTIATING;

	/* set a timeout for retransmission */
	ts = wpa_set_itimer(wpa->nas->timer, &sta->td, (bcm_timer_cb)wpa_retransmission,
	                    (int) sta, sta->wpa_msg_timeout_s, sta->wpa_msg_timeout_ms);
	if (ts != ITIMER_OK)
		dbg(wpa->nas, "Setting PTKINITNEGOTIATING interval timer failed, code %d", ts);

	dbg(wpa->nas, "done");
	return 0;
}

static void
wpa_goto_rekeyneg(nas_sta_t *sta)
{
	dbg(sta->nas, "Falling thru to group key negotiation");

	wpa_ptkinitdone2(sta->nas->wpa, sta);
}

static int
wpa_ptkinitnegotiating(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol)
{
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	ushort key_info, wpaie_len;
	unsigned int required_flags, prohibited_flags;
	itimer_status_t ts;

	/* Check that the packet looks like the correct response */
	required_flags = WPA_KEY_PAIRWISE | WPA_KEY_MIC;
	prohibited_flags = WPA_KEY_ACK | WPA_KEY_INDEX_MASK;
	key_info = ntohs(body->key_info);

	if (((key_info & required_flags) != required_flags) ||
	    ((key_info & prohibited_flags) != 0)) {
		dbg(wpa->nas, "Ignoring key response with incorrect key_info 0x%04x",
		    key_info);
		return 0;
	}

	/* check the replay counter */
	if (bcmp(body->replay, sta->suppl.replay, REPLAY_LEN) != 0) {
		/* Ignore this message */
		dbg(wpa->nas, "failed replay check; ignoring");
		return 0;
	}

	/* kill timeouts from previous state */
	wpa_stop_retx(sta);

	/* Check the WPA IE */
	wpaie_len = ntohs(body->data_len);
	if (!wpaie_len || wpaie_len != sta->suppl.assoc_wpaie_len ||
	    bcmp(body->data, sta->suppl.assoc_wpaie, wpaie_len) != 0) {
		dbg(wpa->nas, "WPA IE mismatch");
		dbg(wpa->nas, "WPA IE in 4-way handshake message #2");
		dump(wpa->nas, body->data, wpaie_len);
		dbg(wpa->nas, "WPA IE in driver assoc indication");
		dump(wpa->nas, sta->suppl.assoc_wpaie, wpaie_len);
		return 1;
	}

	/* Get the snonce */
	bcopy(body->nonce, sta->suppl.snonce, NONCE_LEN);

	/* generate the PTK */
	wpa_calc_ptk(wpa, sta);

	/* check the MIC */
	if (wpa_check_mic(sta, eapol) == FALSE) {
		/* "Silently" discard (8.5.3.2) */
		dbg(wpa->nas, "MIC check failed; ignoring message");
		/* for wrong key case, report one more event to driver */
		nas_send_brcm_event(wpa->nas, (uint8 *)&sta->ea, DOT11_RC_MIC_FAILURE);

		ts = wpa_set_itimer(wpa->nas->timer, &sta->td, (bcm_timer_cb)wpa_retransmission,
		                    (int) sta, sta->wpa_msg_timeout_s, sta->wpa_msg_timeout_ms);
		if (ts != ITIMER_OK)
			dbg(wpa->nas, "Setting PTKSTART retry interval timer failed, code %d", ts);
		return 0;
	}

	/* increment the replay counter */
	wpa_incr_array(sta->suppl.replay, REPLAY_LEN);

	/* send pkt 3 */
	sta->retries = 0;
	if (wpa_send_eapol(wpa, sta) == FALSE)
		return 1;

	/* install the special handler to fall thru to group key exchange if
	 * sending m3 of 4-way handshake exceeds max. retries, which basically
	 * means m3 or m4 is lost
	 */
	if (sta->mode & (WPA | WPA_PSK))
		sta->retx_exceed_hndlr = wpa_goto_rekeyneg;

	/* set a timeout for retransmission */
	ts = wpa_set_itimer(wpa->nas->timer, &sta->td, (bcm_timer_cb)wpa_retransmission,
	                    (int) sta, sta->wpa_msg_timeout_s, sta->wpa_msg_timeout_ms);
	if (ts != ITIMER_OK)
		dbg(wpa->nas, "Setting PTKINITDONE retry interval timer failed, code %d", ts);

	/* move to next state */
	sta->suppl.retry_state = sta->suppl.state;
	sta->suppl.state = WPA_PTKINITDONE;
	dbg(wpa->nas, "done");
	return 0;
}

static int
wpa_ptkinitdone(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol)
{
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	nas_t *nas = wpa->nas;
	ushort key_info;
	unsigned int required_flags, prohibited_flags;

	/* Check that the packet looks like the correct response */
	required_flags = WPA_KEY_PAIRWISE | WPA_KEY_MIC;
	if ((sta->mode & (WPA2 | WPA2_PSK)))
		required_flags |= WPA_KEY_SECURE;
	if (nas->flags & NAS_FLAG_WDS)
		required_flags |= WPA_KEY_SECURE;
	prohibited_flags = WPA_KEY_ACK | WPA_KEY_INDEX_MASK;
	key_info = ntohs(body->key_info);

	if (((key_info & required_flags) != required_flags) ||
	    ((key_info & prohibited_flags) != 0)) {
		dbg(nas, "Ignoring key response with incorrect key_info 0x%04x",
		    key_info);
		return 0;
	}

	/* check the replay counter */
	if (bcmp(body->replay, sta->suppl.replay, REPLAY_LEN) != 0) {
		/* Ignore this message. */
		dbg(nas, "failed replay check; ignoring");
		return 0;
	}

	/* check the MIC */
	if (wpa_check_mic(sta, eapol) == FALSE) {
		/* "silently" discard (8.5.3.4) */
		dbg(nas, "MIC check failed; ignoring message");
		return 0;
	}

	/* Update key and start group key exchange if necessary */
	return wpa_ptkinitdone2(wpa, sta);
}

/* plumb the pairwise key and start group key exchange */
static int
wpa_ptkinitdone2(wpa_t *wpa, nas_sta_t *sta)
{
	nas_t *nas = wpa->nas;
	itimer_status_t ts;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif

	/* kill timeouts from previous state */
	wpa_stop_retx(sta);

	/* plumb pairwise key */
	if (nas_set_key(nas, &sta->ea, sta->suppl.temp_encr_key,
	                sta->suppl.tk_len, 0, 1, 0, 0) < 0) {
		dbg(nas, "unicast key rejected by driver, assuming too many associated STAs");
		cleanup_sta(nas, sta, DOT11_RC_BUSY, 0);
		return 0;
	}

	/* not perform group key exchange if not required so */
	if ((nas->flags & NAS_FLAG_WDS) ||
	    CHECK_WPA2(sta->mode)) {
		if (sta->wds_td)
			TIMER_DELETE(sta->wds_td);

		/* It will give some to complete 4 Key Exchange */
		nas_sleep_ms(500);

		/* Authorize supplicant */
		dbg(nas, "authorize %s (WPA)", ether_etoa((uchar *)&sta->ea, eabuf));
		nas_authorize(nas, &sta->ea);

		sta->suppl.state = sta->suppl.retry_state = WPA_SETKEYSDONE;

		return 0;
	}

	/* increment the replay counter */
	wpa_incr_array(sta->suppl.replay, REPLAY_LEN);

	/* Initiate the group key exchange */
	sta->suppl.state = WPA_REKEYNEGOTIATING;

	/* set the GTK if not already done */
	if (!(nas->flags & NAS_FLAG_GTK_PLUMBED)) {
		wpa_init_gtk(wpa, sta);
#ifdef MFP2
		if (MFP_IGTK_REQUIRED(wpa, sta)) {
			wpa_gen_igtk(wpa);
		}
#endif
	}
	else {

		if (nas_get_group_rsc(nas, &wpa->gtk_rsc[0], wpa->gtk_index)) {
			/* Don't use what we don't have. */
			memset(wpa->gtk_rsc, 0, sizeof(wpa->gtk_rsc));
			dbg(nas, "failed to find group key RSC");
		}
	}

	sta->retries = 0;
	if (wpa_send_eapol(wpa, sta) == FALSE) {
		return 1;
	}
	sta->suppl.retry_state = WPA_REKEYNEGOTIATING;

	/* set a timeout for retransmission */
	ts = wpa_set_itimer(nas->timer, &sta->td, (bcm_timer_cb)wpa_retransmission,
	                    (int) sta, sta->wpa_msg_timeout_s, sta->wpa_msg_timeout_ms);
	if (ts != ITIMER_OK)
		dbg(nas, "Setting REKEYNEGOTIATING retry interval timer failed, code %d", ts);
	return 0;
}

static void
wpa_rekeyneg(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol)
{
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	ushort key_info;
	unsigned int required_flags, prohibited_flags;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif

	/* Check that the packet looks like the correct response.
	 * WPA_KEY_ERROR is not really "prohibited".  It's set as
	 * though it is to simplify its special case.
	 */
	required_flags = WPA_KEY_SECURE | WPA_KEY_MIC;
	prohibited_flags = WPA_KEY_ACK | WPA_KEY_PAIRWISE | WPA_KEY_ERROR;
	key_info = ntohs(body->key_info);

	if (((key_info & required_flags) != required_flags) ||
	    ((key_info & prohibited_flags) != 0)) {
		/* Pick off MIC failure reports and the like that
		 * arrive while we're in a group key exchange.
		 */
		if ((key_info & WPA_KEY_ERROR) != 0) {
			wpa_setkeysdone(wpa, sta, eapol);
		} else {
			dbg(wpa->nas, "Ignoring key response with incorrect key_info 0x%04x",
			    key_info);
		}
		return;
	}

	/* check the replay counter */
	if (bcmp(body->replay, sta->suppl.replay, REPLAY_LEN) != 0) {
		/* Ignore the message. */
		dbg(wpa->nas, "failed replay check; ignoring");
		return;
	}

	/* check the MIC */
	if (wpa_check_mic(sta, eapol) == FALSE) {
		dbg(wpa->nas, "MIC check failed; ignoring message");
		return;
	}

	/* kill timeouts from previous state */
	wpa_stop_retx(sta);

	/* Authorize supplicant */
	dbg(wpa->nas, "authorize %s (WPA)", ether_etoa((uchar *)&sta->ea, eabuf));
	nas_authorize(wpa->nas, &sta->ea);

	/* These states need more thought. */
	sta->suppl.state = sta->suppl.retry_state = WPA_SETKEYSDONE;
	dbg(wpa->nas, "Group key exchange with %s completed",
	    ether_etoa((uchar *)&sta->ea, eabuf));

	/* set PTK rekey timer */
	if (wpa->ptk_rekey_secs) {
		itimer_status_t ts = wpa_set_itimer(wpa->nas->timer, &wpa->ptk_rekey_timer,
		                                    (bcm_timer_cb)wpa_new_ptk_callback,
		                                    (int)wpa, wpa->ptk_rekey_secs, 0);
		if (ts != ITIMER_OK)
			dbg(wpa->nas, "PTK interval timer set failed, code %d", ts);
	}

	return;
}

static void
wpa_setkeysdone(wpa_t *wpa, nas_sta_t *sta, eapol_header_t *eapol)
{
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	unsigned int required_flags, prohibited_flags, MIC_failure_flags;
	ushort key_info;

	dbg(wpa->nas, "start");

	/* Is this a proper WPA key request? */
	required_flags = WPA_KEY_REQ;
	prohibited_flags = WPA_KEY_ACK;
	key_info = ntohs(body->key_info);
	if (((key_info & required_flags) != required_flags) ||
	    ((key_info & prohibited_flags) != 0)) {

		if (((key_info & WPA_KEY_REQ) == 0) &&
		    !bcmp(body->replay, sta->suppl.replay, REPLAY_LEN)) {
			/* Silently ignore redundant tardy reply */
		} else {
			dbg(wpa->nas, "Ignoring invalid EAPOL WPA key request, key_info 0x%04x",
			    key_info);
		}
		return;
	}
	/* The combination of REQ, ERROR, and MIC is the STA's way
	 * of informing the authenticator of a MIC failure.
	 */
	MIC_failure_flags = WPA_KEY_ERROR | WPA_KEY_REQ | WPA_KEY_MIC;
	if ((key_info & MIC_failure_flags) == MIC_failure_flags) {
		wpa_mic_error(wpa, sta, FALSE);
		return;
	}

	if (key_info & WPA_KEY_PAIRWISE) {
		/* A STA wants its pairwise key updated. */
		sta->suppl.state = WPA_PTKSTART;
		/* This will fall into the 4-way handshake on return. */

	} else {
		/* It's a request to update group keys for all. */
		/* Some places in the spec say there should be a 4-way
		 * handshake for the requester, too.
		 */
		wpa_new_gtk(wpa);
	}
	dbg(wpa->nas, "done");
	return;
}

#ifdef BCMDBG
static char *sta_state[] = {
	/* authenticator states */
	/* 4 way pkt exchange state machine */
	"WPA_DISCONNECT",
	"WPA_DISCONNECTED",
	"WPA_INITIALIZE",
	"WPA_AUTHENTICATION2",
	"WPA_INITPMK",
	"WPA_INITPSK",
	"WPA_PTKSTART",
	"WPA_PTKINITNEGOTIATING",
	"WPA_PTKINITDONE",
	"WPA_UPDATEKEYS",
	"WPA_INTEGRITYFAILURE",
	"WPA_KEYUPDATE",
	/* group key state machine */
	"WPA_REKEYNEGOTIATING",
	"WPA_KEYERRROR",
	"WPA_REKEYESTABLISHED",
	/* Authenticator, group key */
	"WPA_SETKEYS",
	"WPA_SETKEYSDONE",
#ifdef BCMSUPPL
	/* supplicant states */
	"WPA_SUP_DISCONNECTED",
	"WPA_SUP_INITIALIZE",
	"WPA_SUP_AUTHENTICATION",
	"WPA_SUP_STAKEYSTARTP",
	"WPA_SUP_STAKEYSTARTG",
	"WPA_SUP_KEYUPDATE"
#endif /* BCMSUPPL */
};
static char*
sta_state_name(int state)
{
	return state < sizeof(sta_state)/sizeof(sta_state[0]) ?
		sta_state[state] : "unknown";
}
#endif /* BCMDBG */

#ifdef BCMSUPPL
/*
 * Traverse a string of 1-byte tag/1-byte length/variable-length value
 * triples, returning a pointer to the substring whose first element
 * matches tag
 */
int
eapol_sup_process_key(wpa_t *wpa, eapol_header_t *eapol, nas_sta_t *sta)
{
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	ushort key_info = ntohs(body->key_info);
	ushort data_len = ntohs(body->data_len);
	/*	uint8 TSnonce[NONCE_LEN]; */
	bool UpdatePTK = FALSE;
	bool UpdateGTK = FALSE;
	eapol_sup_pk_state_t state = EAPOL_SUP_PK_UNKNOWN;

	/* get replay counter from recieved frame */
	bcopy(body->replay, sta->suppl.replay, REPLAY_LEN);

	dbg(wpa->nas, "start");
	if (key_info & WPA_KEY_MIC) {
		dbg(wpa->nas, "MIC");
		if (wpa_check_mic(sta, eapol) == FALSE) {
			dbg(wpa->nas, "MIC Failed");
			state = EAPOL_SUP_PK_MICFAILED;
		} else {
			dbg(wpa->nas, "MIC OK");
			state = EAPOL_SUP_PK_MICOK;
		}
	}

	/* decrypt key data field */
	if ((sta->mode & (WPA2 | WPA2_PSK)) &&
	    (key_info & WPA_KEY_ENCRYPTED_DATA))
		if (!wpa_decr_key_data(wpa, sta, body)) {
			err(wpa->nas, "decryption of key data failed");
			return EAPOL_SUP_PK_UNKNOWN;
		}


	if (key_info & WPA_KEY_PAIRWISE) {
		sta->suppl.state = WPA_SUP_STAKEYSTARTP;
		/* 4-way handshke message 1 - reset state to EAPOL_SUP_PK_MSG1 */
		if (!(key_info & WPA_KEY_MIC)) {
			nas_set_key(wpa->nas, &sta->ea, NULL, 0, 0, 0, 0, 0);
			sta->suppl.pk_state = EAPOL_SUP_PK_MSG1;
		}
		/* 4-way handshake message 3 - validate current state */
		else if (sta->suppl.pk_state != EAPOL_SUP_PK_MSG3)
			return state;
	}
	else
		sta->suppl.state = WPA_SUP_STAKEYSTARTG;

	/* Per 802.11i spec, silently drop MIC failures */
	if (key_info & WPA_KEY_PAIRWISE) {
		dbg(wpa->nas, "Pairwise");
		/* Pairwise Key */
		if (state != EAPOL_SUP_PK_MICFAILED)  {
			dbg(wpa->nas, "!Failed");
			/* bcopy(suppl->snonce, TSNonce, NONCE_LEN); */
			if (key_info & WPA_KEY_INSTALL) {
				if ((sta->mode & (WPA2 | WPA2_PSK))) {
					bcm_tlv_t *rsnie;
					int len = ntohs(body->data_len);

					dbg(wpa->nas, "data_len %d", len);

					rsnie = bcm_parse_tlvs(body->data, len, DOT11_MNG_RSN_ID);
					if (rsnie)
						dbg(wpa->nas, "found rsnie");
					else
						dbg(wpa->nas, "didn't find rsnie");

					if (!(UpdateGTK = wpa_extract_gtk(wpa, sta, eapol))) {
						err(wpa->nas, "extraction of gtk from eapol message"
						    " failed");
						return EAPOL_SUP_PK_UNKNOWN;
					}

				} else
				/* Check message 3 WPA IE against probe response IE. */
				if (!data_len || data_len != sta->suppl.assoc_wpaie_len ||
				    bcmp(body->data, sta->suppl.assoc_wpaie, data_len)) {
					dbg(wpa->nas, "WPA IE mismatch");
					dbg(wpa->nas, "WPA IE in 4-way handdshake message #3");
					dump(wpa->nas, body->data, data_len);
					dbg(wpa->nas, "WPA IE in driver assoc indication");
					dump(wpa->nas, sta->suppl.assoc_wpaie,
					     sta->suppl.assoc_wpaie_len);
					return EAPOL_SUP_PK_ERROR;
				}
			}
			bcopy(body->nonce, sta->suppl.anonce, NONCE_LEN);
			wpa_calc_ptk(wpa, sta);
		}

		if (state == EAPOL_SUP_PK_MICOK)  {
			dbg(wpa->nas, "MICOK");
			if (key_info & WPA_KEY_INSTALL)
				UpdatePTK = TRUE;
			else {
				dbg(wpa->nas, "INSTALL flag not set in msg 3 key_info; no PTK"
				    " installed");
			}
		}
	} else if (state == EAPOL_SUP_PK_MICOK) {
		dbg(wpa->nas, "Group, MICOK");
	  if ((sta->mode & (WPA2 | WPA2_PSK))) {
			if (!(UpdateGTK = wpa_extract_gtk(wpa, sta, eapol))) {
				err(wpa->nas, "extraction of gtk from eapol message failed");
				return EAPOL_SUP_PK_UNKNOWN;
			}
	  } else {
		/* Group Key */
		if (!wpa_decr_gtk(wpa, sta, body)) {
			dbg(wpa->nas, "unencrypt failed");
			state = EAPOL_SUP_PK_MICFAILED;
		} else {
			dbg(wpa->nas, "unencrypt ok, plumb gtk");
			wpa_plumb_gtk(wpa, key_info & WPA_KEY_INSTALL);
			/* nas_deauthenticate(wpa->nas, suppl->ea) */
		}
	  }
	} else {
		dbg(wpa->nas, "Group, !MICOK");
		state = EAPOL_SUP_PK_MICFAILED;
	}

	if ((key_info & WPA_KEY_ACK) && (state != EAPOL_SUP_PK_MICFAILED)) {
		dbg(wpa->nas, "ACK, !Failed");
/*		Send EAPOL(0, 1, 0, 0, 0, K, 0, TSNonce, 0, MIC(TPTK), 0) */
		if (wpa_send_eapol(wpa, sta) == FALSE) {
			dbg(wpa->nas, "error: can't send eapol msg to AP");
			return EAPOL_SUP_PK_ERROR;
		}
	}
	if (UpdatePTK == TRUE) {
		dbg(wpa->nas, "UpdatePTK");
		nas_sleep_ms(100);

		if (nas_set_key(wpa->nas, &sta->ea, sta->suppl.temp_encr_key, sta->suppl.tk_len, 0,
		                1, 0, 0) < 0) {
			dbg(wpa->nas, "nas_set_key() failed");
			nas_deauthenticate(wpa->nas, &sta->ea, DOT11_RC_BUSY);
		}
	}
	if (UpdateGTK == TRUE) {
		dbg(wpa->nas, "UpdateGTK");
		wpa_plumb_gtk(wpa, key_info & WPA_KEY_INSTALL);
	}
	if ((state == EAPOL_SUP_PK_MICOK) && (key_info & WPA_KEY_SECURE)) {
#ifdef BCMDBG
		char eabuf[ETHER_ADDR_STR_LEN];
#endif

		/* Authorize authenticator */
		dbg(wpa->nas, "authorize %s (WPA)", ether_etoa((uchar *)&sta->ea, eabuf));
		nas_authorize(wpa->nas, &sta->ea);

		/* We are done! no more retries */
		wpa_stop_retx(sta);
		if (sta->wds_td)
			TIMER_DELETE(sta->wds_td);
	}

	dbg(wpa->nas, "done");
	return state;
}

/* Process a WPA-EAPOL-Key packet at the supplicant */
int
process_sup_wpa(wpa_t *wpa, eapol_header_t *eapol, nas_sta_t *sta)
{
#ifdef BCMDBG
	nas_t *nas = wpa->nas;
	char eabuf[ETHER_ADDR_STR_LEN];
	eapol_wpa_key_header_t *wpa_key = (eapol_wpa_key_header_t *)eapol->body;
	uint16 key_info = ntohs(wpa_key->key_info);
#endif

	dbg(nas, "start for %s", ether_etoa((uchar *)&sta->ea, eabuf));
	dbg(nas, "state %s key info %04x", sta_state_name(sta->suppl.state), key_info);

	if (sta->suppl.state == WPA_SUP_AUTHENTICATION)
		sta->suppl.state = WPA_SUP_STAKEYSTARTP;
	if (sta->suppl.retry_state == WPA_SUP_AUTHENTICATION)
		sta->suppl.retry_state = WPA_SUP_STAKEYSTARTP;

	/* Proceed with the key exchange... */

/* !!! most of this belongs somewhere else.  only STAKEYSTAT is handled here */
	/* New WPA state */
	switch (sta->suppl.state) {
	case WPA_SUP_DISCONNECTED:
		dbg(nas, "WPA_SUP_DISCONNECTED");
		/* StaDisconnect() */
		/* fall through */
	case WPA_SUP_INITIALIZE:
		dbg(nas, "WPA_SUP_INITIALIZE: %s", ether_etoa((uchar *)&sta->ea, eabuf));
/*
		MSK = 0
		802.1X:; portEnabled = FALSE
		Remove PTK
		Remove GTK(0..N)
		802.1X:portValid = FALSE
*/
		break;
	case WPA_SUP_AUTHENTICATION:
		dbg(nas, "WPA_SUP_AUTHENTICATION: %s", ether_etoa((uchar *)&sta->ea, eabuf));
/*
		SNonce = Counter++
		PTK=GTK(0..N) = 0
		CANonce = 0
		802.1X::portValid = FALSE
		802.1X::portControl = Auto
		802.1X::portEnabled = FALSE
*/
		break;
	case WPA_SUP_STAKEYSTARTP:
	case WPA_SUP_STAKEYSTARTG:
		dbg(nas, "WPA_SUP_STAKEYSTARTP/G: %s", ether_etoa((uchar *)&sta->ea, eabuf));
		/* ensure the timer was cleared */
		wpa_stop_retx(sta);
		if (eapol_sup_process_key(wpa, eapol, sta) == EAPOL_SUP_PK_ERROR)
			return 1;
		break;
	case WPA_SUP_KEYUPDATE:
		dbg(nas, "WPA_SUP_KEYUPDATE: %s", ether_etoa((uchar *)&sta->ea, eabuf));
/*
		SNonce = Counter++
		Remove PTK
		Remove GTK(0..N)
		Send EAPOL()
		IntegrityFailed = False
		Updatekeys = False
*/
		break;
	default:
		dbg(nas, "error: WPA state not recognized %d for sta %s",
		    sta->suppl.state, ether_etoa((uchar *)&sta->ea, eabuf));
		return 1;
	}
	dbg(nas, "done");
	return 0;
}
#endif /* BCMSUPPL */

/* Process a WPA-EAPOL-Key packet */
int
process_wpa(wpa_t *wpa, eapol_header_t *eapol, nas_sta_t *sta)
{
	int ret = 0;
	nas_t *nas = wpa->nas;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif
	eapol_wpa_key_header_t *wpa_key = (eapol_wpa_key_header_t *)eapol->body;
	uint16 key_info = ntohs(wpa_key->key_info);

	dbg(nas, "start for %s", ether_etoa((uchar *)&sta->ea, eabuf));
	dbg(nas, "state %s key info %04x", sta_state_name(sta->suppl.state), key_info);

	/*
	* Supplicant requested a 4-way handshake. May or may not honor
	* the request depending on our own state.
	*/
	if ((nas->flags & NAS_FLAG_WDS) &&
	    (key_info & WPA_KEY_REQ) && (key_info & WPA_KEY_PAIRWISE)) {
		if (sta->suppl.state == WPA_AUTHENTICATION2 ||
		    sta->suppl.state == WPA_PTKINITNEGOTIATING ||
		    sta->suppl.state == WPA_SETKEYSDONE ||
		    sta->suppl.state == WPA_DISCONNECT) {
			/*
			* Start handshake if when there is no initiator in progress.
			*/
			if (!sta->wds_td) {
				wpa_start(wpa, sta);
			}
		}
		return 0;
	}

	/* If STA wants TKIP and we're in countermeasures, toss
	 * it.  Driver will block these, but there's a race early
	 * in the period making this check is necessary.
	 */
	if (nas->MIC_countermeasures && WSEC_TKIP_ENABLED(sta->wsec)) {
		cleanup_sta(nas, sta, DOT11_RC_MIC_FAILURE, 0);
		return 0;
	}

	/* New WPA state */
	switch (sta->suppl.state) {
	case WPA_SETKEYSDONE:
		/* A supplicant in this state is probably here
		 * asking to be rekeyed.
		 */
		dbg(nas, "SETKEYSDONE: %s", ether_etoa((uchar *)&sta->ea, eabuf));
		wpa_setkeysdone(wpa, sta, eapol);
		if (sta->suppl.state != WPA_PTKSTART)
			return 0;
		/* fall through */
	case WPA_AUTHENTICATION2:
		dbg(nas, "AUTHENTICATION2: %s", ether_etoa((uchar *)&sta->ea, eabuf));
		if (wpa_verifystart(wpa, sta, eapol)) {
			return 1;
		}
		else if ((sta->mode & WPA) &&
			(sta->suppl.retry_state != WPA_PTKSTART))
			/* WPA-mode will be back if RADIUS auth works. */
			return 0;
		/* fall through */

	case WPA_PTKSTART:
		dbg(nas, "PTKSTART: %s", ether_etoa((uchar *)&sta->ea, eabuf));
		ret = wpa_ptkstart(wpa, sta, eapol);
		break;

	case WPA_PTKINITNEGOTIATING:
		dbg(nas, "PTKINITNEGOTIATING: %s", ether_etoa((uchar *)&sta->ea, eabuf));
		ret = wpa_ptkinitnegotiating(wpa, sta, eapol);
		break;

	case WPA_PTKINITDONE:
		dbg(nas, "PTKINITDONE: %s", ether_etoa((uchar *)&sta->ea, eabuf));
		ret = wpa_ptkinitdone(wpa, sta, eapol);
		break;

	case WPA_REKEYNEGOTIATING:
		dbg(nas, "WPA_REKEYNEGOTIATING: %s", ether_etoa((uchar *)&sta->ea, eabuf));
		wpa_rekeyneg(wpa, sta, eapol);
		break;

	default:
		dbg(nas, "error: WPA state not recognized");
		break;
	}
	dbg(nas, "done");
	return ret;
}


/* generate the initial global_key_counter */
void
initialize_global_key_counter(wpa_t *wpa)
{
	unsigned char buff[32], prf_buff[PRF_RESULT_LEN];
	unsigned char prefix[] = "Init Counter";

	nas_rand128(&buff[0]);
	nas_rand128(&buff[16]);
	/* Still not exactly right, but better. */
	fPRF(buff, sizeof(buff), prefix, strlen((char *)prefix),
	    (unsigned char *) &wpa->nas->ea, ETHER_ADDR_LEN,
	    prf_buff, KEY_COUNTER_LEN);
	memcpy(wpa->global_key_counter, prf_buff, KEY_COUNTER_LEN);
	memcpy(wpa->initial_gkc, wpa->global_key_counter, KEY_COUNTER_LEN);
}

#ifdef MFP2
/* initialize IPN (IGTK Packet Number) */
static void
wpa_initialize_ipn(wpa_t *wpa)
{
	unsigned char buff[32], prf_buff[PRF_RESULT_LEN];
	unsigned char prefix[] = "Init Counter";

	nas_rand128(&buff[0]);
	nas_rand128(&buff[16]);
	/* Still not exactly right, but better. */
	fPRF(buff, sizeof(buff), prefix, strlen((char *)prefix),
	    (unsigned char *) &wpa->nas->ea, ETHER_ADDR_LEN,
	    prf_buff, KEY_COUNTER_LEN);
	memcpy(&wpa->igtk.ipn_lo, prf_buff, sizeof(uint32));
	memcpy(&wpa->igtk.ipn_hi, prf_buff+sizeof(uint32), sizeof(uint16));
}

static void
wpa_gen_igtk(wpa_t *wpa)
{
	unsigned char data[256], prf_buff[PRF_RESULT_LEN];
	unsigned char prefix[] = "Group key expansion";
	int data_len = 0;

	wpa->igtk.key_len = AES_TK_LEN;

	wpa->igtk.id = IGTK_NEXT_INDEX(wpa);
	wpa_initialize_ipn(wpa);

	/* create the the data portion */
	bcopy((char*)&wpa->nas->ea, (char*)&data[data_len], ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	*(uint32 *)&data[data_len] = wpa->igtk.ipn_lo;
	*(uint16 *)&data[data_len+sizeof(uint32)] = wpa->igtk.ipn_hi;
	data_len += 6;
	wpa->igtk.ipn_lo++;
	if (wpa->igtk.ipn_lo == 0)
		wpa->igtk.ipn_hi++;

	/* generate the GTK */
	fPRF(wpa->gmk, sizeof(wpa->gmk), prefix, strlen((char *)prefix),
	    data, data_len, prf_buff, AES_TK_LEN);
	memcpy(wpa->igtk.key, prf_buff, AES_TK_LEN);
	nas_set_key(wpa->nas, &wpa->nas->ea, wpa->igtk.key, wpa->igtk.key_len, wpa->igtk.id, 0,
	            wpa->igtk.ipn_lo, wpa->igtk.ipn_hi);
}
#endif /* MFP */

static void
wpa_incr_gkc(wpa_t *wpa)
{
	wpa_incr_array(wpa->global_key_counter, KEY_COUNTER_LEN);

	/* if key counter is now equal to the original one, reset it */
	if (!bcmp(wpa->global_key_counter, wpa->initial_gkc, KEY_COUNTER_LEN))
		initialize_global_key_counter(wpa);
}

void
initialize_gmk(wpa_t *wpa)
{
	unsigned char *gmk = (unsigned char *)wpa->gmk;

	nas_rand128(&gmk[0]);
	nas_rand128(&gmk[16]);
}

#ifdef BCMSUPPL
/* build and send EAPOL key request message */
static void
request_pkinit(bcm_timer_id timer, int data)
{
	nas_sta_t *sta = (nas_sta_t *)data;
	nas_t *nas = sta->nas;
	wpa_t *wpa = nas->wpa;

	/* If there was a stale timer descriptor, get rid of it. */
	wpa_stop_retx(sta);

	/*
	 * build expected authenticator wpaie so that we can compare
	 * what's carried in message #3 of the 4-way handshake.
	 * No WEP supported when doing WPA over WDS.
	 */
	sta->suppl.assoc_wpaie_len = sizeof(sta->suppl.assoc_wpaie);
	wpa_build_ie(wpa, nas->wsec, CRYPTO_ALGO_OFF, nas->mode,
	             sta->suppl.assoc_wpaie, &sta->suppl.assoc_wpaie_len);

	/* we should have no key installed in the MAC by now */
	sta->suppl.retry_state = sta->suppl.state;
	sta->suppl.state = WPA_SUP_INITIALIZE;

	/* WPA mode needs RADIUS server acceptance before beginning
	 * the 4-way handshake.
	 */
	if (nas->mode & WPA)
		return;

	/* send the key request pkt to start the 4 way exchange */
	sta->retries = 0;
	if (wpa_send_eapol(wpa, sta) == FALSE)
		return;

	/* move to next state */
	sta->suppl.retry_state = sta->suppl.state;
	sta->suppl.state = WPA_SUP_AUTHENTICATION;

	/* set a timeout for retransmission */
	if (wpa_set_itimer(nas->timer, &sta->td, (bcm_timer_cb)wpa_retransmission,
	    (int)sta, sta->wpa_msg_timeout_s, sta->wpa_msg_timeout_ms) != ITIMER_OK)
		dbg(nas, "Setting WPA_SUP_AUTHENTICATION interval timer failed");
}

/* send EAPOL key request message */
void
wpa_request(wpa_t *wpa, nas_sta_t *sta)
{
	struct itimerspec  its;
	itimer_status_t ret = ITIMER_OK;
	nas_t *nas = wpa->nas;

	/* remove previous timer if any */
	if (sta->wds_td)
		TIMER_DELETE(sta->wds_td);

	/*
	* start the reqeust right away only when the
	* supplicant is not in key exchange process.
	*/
	sta->wpa_msg_timeout_s = RETRY_SECS;
	sta->wpa_msg_timeout_ms = RETRY_MSECS;

	if (sta->suppl.pk_state == EAPOL_SUP_PK_UNKNOWN ||
	    sta->suppl.pk_state == EAPOL_SUP_PK_DONE) {
		/* kick off the negotiation */
		request_pkinit(0, (int)sta);
	}
	/* delay the next request(s) */
	else {
		dbg(nas, "delay request, handshake in progress");
		wpa_stop_retx(sta);
	}

	/* set up retry timer */
	its.it_interval.tv_sec = wpa->wds_to ? : 60;
	its.it_interval.tv_nsec = 0 * NSECS_PER_MSEC;
	its.it_value.tv_sec = wpa->wds_to ? : 60;
	its.it_value.tv_nsec = 0 * NSECS_PER_MSEC;

	if (bcm_timer_create(nas->timer, &sta->wds_td))
		ret = ITIMER_CREATE_ERROR;
	else if (bcm_timer_connect(sta->wds_td, request_pkinit, (int)sta))
		ret = ITIMER_CONNECT_ERROR;
	else if (bcm_timer_settime(sta->wds_td, &its))
		ret = ITIMER_SET_ERROR;
	if (ret)
		dbg(nas, "failed to set up pairwise key requestor timer");
}
#endif /* BCMSUPPL */

/* pretend to have received a assoc message from the driver. */
static void
start_pkinit(bcm_timer_id timer, int data)
{


	char msg[sizeof(bcm_event_t) + 128 + 2];
	bcm_event_t *assoc = (bcm_event_t *)msg;

	wl_event_msg_t *event = &(assoc->event);
	uint8 *databuf = (uint8 *)(event + 1);

	nas_sta_t *sta = (nas_sta_t *)data;
	nas_t *nas = sta->nas;
	wpa_t *wpa = nas->wpa;
	uint16 len = sizeof(msg); /* - sizeof(bcm_event_t); */

	/* wpa message header */
	bcopy(nas->ea.octet, assoc->eth.ether_dhost, ETHER_ADDR_LEN);
	bcopy(sta->ea.octet, assoc->eth.ether_shost, ETHER_ADDR_LEN);
	assoc->eth.ether_type = htons(ETHER_TYPE_BRCM);

	/*	assoc->version = BCM_MSG_VERSION; */
	event->event_type = hton32(WLC_E_ASSOC_IND);

	strncpy(event->ifname, nas->interface, sizeof(event->ifname) - 1);
	event->ifname[sizeof(event->ifname) - 1] = '\0';
	/* append wpa ie */
	if (wpa_build_ie(wpa, sta->wsec, sta->algo,
	                 sta->mode, databuf, &len)) {
		dbg(nas, "wpa_build_ie failed");
		return;
	}

	if (len > 128)	{
		dbg(nas, "wpa_build_ie failed: length greater than 128 bytes");
		return;
	}

	event->datalen = hton32(len);
	event->version = hton16(BCM_EVENT_MSG_VERSION);
	event->status = 0; /* TODO: Is value OK ??? */
	event->reason = 0; /* TODO: Is value OK ??? */
	event->auth_type = 0; /* TODO: Is value OK ??? */
	event->flags =  0; /* TODO: Is value OK ??? */
	bcopy(sta->ea.octet, event->addr.octet, ETHER_ADDR_LEN);

	/* BCM Vendor specifc header... */
	assoc->bcm_hdr.subtype = htons(BCMILCP_SUBTYPE_VENDOR_LONG);
	assoc->bcm_hdr.version = BCMILCP_BCM_SUBTYPEHDR_VERSION;
	bcopy(BRCM_OUI, &assoc->bcm_hdr.oui[0], DOT11_OUI_LEN);
	/* vendor spec header length + pvt data length (private indication hdr + actual message
	 * itself)
	 */
	assoc->bcm_hdr.length = htons(BCMILCP_BCM_SUBTYPEHDR_MINLENGTH +  BCM_MSG_LEN + len);
	assoc->bcm_hdr.usr_subtype = htons(BCMILCP_BCM_SUBTYPE_EVENT);

	/* Last 2 bytes of the message should be 0x00 0x00 to signal that there are no ethertypes
	 * which are following this
	 */
	msg[sizeof(bcm_event_t) + len ]		= 0x00;
	msg[sizeof(bcm_event_t) + len + 1 ]	= 0x00;

	dbg(nas, "start_pkinit: sending fake msg to driver_message_dispatch");
	driver_message_dispatch(nas, assoc);

}

#ifdef NAS_GTK_PER_STA
void wpa_set_gtk_per_sta(wpa_t *wpa, bool gtk_per_sta)
{
	wpa->gtk_per_sta = gtk_per_sta;
	wpa_new_gtk(wpa);
}
#endif

void
wpa_start(wpa_t *wpa, nas_sta_t *sta)
{
	struct itimerspec  its;
	itimer_status_t ret = ITIMER_OK;
	nas_t *nas = wpa->nas;

	/* remove previous timer if any */
	if (sta->wds_td)
		TIMER_DELETE(sta->wds_td);

	/* kick off the negotiation */
	start_pkinit(0, (int)sta);

	/* set up retry timer */
	its.it_interval.tv_sec = wpa->wds_to ? : 60;
	its.it_interval.tv_nsec = 0 * NSECS_PER_MSEC;
	its.it_value.tv_sec = wpa->wds_to ? : 60;
	its.it_value.tv_nsec = 0 * NSECS_PER_MSEC;

	if (bcm_timer_create(nas->timer, &sta->wds_td))
		ret = ITIMER_CREATE_ERROR;
	else if (bcm_timer_connect(sta->wds_td, start_pkinit, (int)sta))
		ret = ITIMER_CONNECT_ERROR;
	else if (bcm_timer_settime(sta->wds_td, &its))
		ret = ITIMER_SET_ERROR;
	if (ret)
		dbg(nas, "failed to set up pairwise key initiator timer");
}
