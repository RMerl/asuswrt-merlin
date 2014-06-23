/*
 * 802.1x EAPOL to RADIUS proxy (Network Access Server)
 *
 * See
 *
 * IEEE Std 802.1X-2001
 * RFC 2284: PPP Extensible Authentication Protocol (EAP)
 * RFC 2548: Microsoft Vendor-specific RADIUS Attributes
 * RFC 2716: PPP EAP TLS Authentication Protocol
 * RFC 2865: Remote Authentication Dial In User Service (RADIUS)
 * RFC 2869: RADIUS Extensions
 * RFC 3079: Deriving Keys for use with Microsoft Point-to-Point Encryption (MPPE)
 * IEEE 802.1X RADIUS Usage Guidelines
 * RADIUS Master Session Key Attribute
 * Making IEEE 802.11 Networks Enterprise-Ready
 * Recommendations for IEEE 802.11 Access Points
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: nas.c 462635 2014-03-18 09:09:23Z $
 */

#include <typedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>

#if defined(linux)
#include <sys/time.h>
#elif defined(__ECOS)
extern int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

#include <bcmutils.h>
#include <bcmtimer.h>
#include <bcmendian.h>
#include <shutils.h>
#include <proto/ethernet.h>
#include <proto/eapol.h>
#include <proto/eap.h>

#include "nas.h"
#include "wpa.h"

#include "radius.h"
#include "nas_radius.h"

#include "global.h"
#include <bcmcrypto/md5.h>
#include <bcmcrypto/rc4.h>

#define CHECK_EAPOL_KEY(key) (((key) == EAPOL_WPA_KEY) || ((key) == EAPOL_WPA2_KEY))

static void eapol_canned(nas_t *nas, nas_sta_t *sta, unsigned char code,
                         unsigned char type);
static void eapol_dispatch_ex(nas_t *nas, eapol_header_t *eapol, int preauth, int bytes);

static void toss_sta(nas_t *nas, nas_sta_t *sta, int reason, int driver_signal);

void
send_identity_req(nas_t *nas, nas_sta_t *sta)
{
	eapol_header_t eapol;

	dbg(nas, "send Identity Request");
	memcpy(eapol.eth.ether_shost, &sta->ea, ETHER_ADDR_LEN);
	memcpy(eapol.eth.ether_dhost, &nas->ea, ETHER_ADDR_LEN);
	eapol.eth.ether_type = HTON16(ETHER_TYPE_802_1X);
	eapol.version = sta->eapol_version;
	eapol.type = EAPOL_START;
	eapol.length = 0;
	eapol_dispatch(nas, &eapol, EAPOL_HEADER_LEN);
}

void
cleanup_sta(nas_t *nas, nas_sta_t *sta, int reason, int driver_signal)
{
	unsigned char toss = 1;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif

	if (sta == NULL) {
		dbg(nas, "called with NULL STA ponter");
		return;
	}

	/* De-Authenticate supplicant only once, and ignore further
	 * response from supplicant. We put supplicant in DISCONNECTED
	 * state, till we either receive handshake from supplicant or the
	 * wds_td initiator timer expires, either of the events put
	 * supplicant sta back in START state.
	 * Cleanup of supplicant sta is not required in case of WDS,
	 * Because, Removing WDS config leades to rc restart anyway, which
	 * restarts the whole NAS story.
	 */
	if ((nas->flags & NAS_FLAG_WDS) && (nas->flags & NAS_FLAG_AUTHENTICATOR)) {
		if (!driver_signal && (sta->suppl.state != WPA_DISCONNECTED))
			sta->suppl.state = WPA_DISCONNECTED;
		if (!driver_signal && (sta->suppl.state == WPA_DISCONNECTED))
			driver_signal = 1;
		/* Dont toss supplicant sta in case of WDS, the initiator
		 * timer will trigger fake assoc.
		 */
		toss = 0;
	}

	/* If this is because of a driver message, telling the driver again
	 * is unnecessary (and could loop).
	 */
	if (!driver_signal) {
		dbg(nas, "deauthenticating %s", ether_etoa((uchar *)&sta->ea, eabuf));
		nas_deauthenticate(nas, &sta->ea, reason);
	}
	/* Be careful not to leak timer descriptors. */
	wpa_stop_retx(sta);

	/* Respect the PMK caching if the STA is associate and Authenticated with WPA2 AKM */
	if ((sta->mode & WPA2) && (sta->pae.state == AUTHENTICATED) && sta->pae.ssnto) {
		dbg(nas, "keep STA %s", ether_etoa((uchar *)&sta->ea, eabuf));
		sta->suppl.state = WPA_PTKSTART;
		return;
	}
	if (toss) toss_sta(nas, sta, 0, 1);
	return;
}

/* Just clean up the STA */

static void
toss_sta(nas_t *nas, nas_sta_t *sta, int reason, int driver_signal)
{
	nas_sta_t *sta_list;
	uint hash;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif
	if (sta == NULL) {
		dbg(nas, "called with NULL STA ponter");
		return;
	}
	/* If this is because of a driver message, telling the driver again
	 * is unnecessary (and could loop).
	 */
	if (!driver_signal) {
		dbg(nas, "deauthenticating %s", ether_etoa((uchar *)&sta->ea, eabuf));
		nas_deauthenticate(nas, &sta->ea, reason);
	}
	/* Be careful not to leak timer descriptors. */
	wpa_stop_retx(sta);
	dbg(nas, "cleanup STA %s", ether_etoa((uchar *)&sta->ea, eabuf));
	/* Remove this one from its hashed list. */
	hash = pae_hash(&sta->ea);
	sta_list = nas->sta_hashed[hash];

	if (sta_list == sta) {
		/* It was the head, so its next is the new head. */
		nas->sta_hashed[hash] = sta->next;

	} else {
		/* Find the one that points to it and change the pointer. */
		while ((sta_list != NULL) && (sta_list->next != sta))
			sta_list = sta_list->next;
		if (sta_list == NULL) {
			dbg(nas, "sta %s not in hash list",
			    ether_etoa((uchar *)&sta->ea, eabuf));
		} else {
			sta_list->next = sta->next;
		}
	}

	sta->used = FALSE;
	return;
}

/*
 * Search for or create a STA struct.
 * If `enter' is not set, do not create it when one is not found.
 */
nas_sta_t *
lookup_sta(nas_t *nas, struct ether_addr *ea, sta_lookup_mode_t mode)
{
	unsigned int hash;
	nas_sta_t *sta;
	time_t now, oldest;

	hash = pae_hash(ea);

	/* Search for entry in the hash table */
	for (sta = nas->sta_hashed[hash];
	     sta && memcmp(&sta->ea, ea, ETHER_ADDR_LEN);
	     sta = sta->next);

	/* One second resolution is probably good enough. */
	(void) time(&now);

	/* Allocate a new entry */
	if (!sta) {
		int i, old_idx = -1;

		/* Don't make an unwanted entry. */
		if (mode == SEARCH_ONLY)
			return NULL;

		oldest = now;
		for (i = 0; i < MAX_SUPPLICANTS; i++) {
			if (!nas->sta[i].used)
				break;
			else if (nas->sta[i].last_use < oldest) {
				oldest = nas->sta[i].last_use;
				old_idx = i;
			}
		}

		if (i < MAX_SUPPLICANTS) {
			sta = &nas->sta[i];
		} else if (old_idx == -1) {
			/* Full up with all the same timestamp?!  Can
			 * this really happen?
			 */
			return NULL;
		} else {
			/* Didn't find one unused, so age out LRU entry. */
			sta = &nas->sta[old_idx];
			toss_sta(nas, sta, DOT11_RC_BUSY, 0);
		}

		if ((sta != NULL) && (sta->pae.radius.request != NULL)) {
			free(sta->pae.radius.request);
		}
		/* Initialize entry */
		memset(sta, 0, (sizeof(nas_sta_t)));
		memcpy(&sta->ea, ea, ETHER_ADDR_LEN);
		sta->used = TRUE;
		sta->nas = nas;
		/* initialize EAPOL version */
		sta->eapol_version = WPA_EAPOL_VERSION;

		/* Initial STA state:
		 * Cheaper and harmless not to distiguish NAS mode.
		 */
		if (nas->flags & NAS_FLAG_AUTHENTICATOR)
			sta->suppl.state = sta->suppl.retry_state = WPA_AUTHENTICATION2;
#ifdef BCMSUPPL
		if (nas->flags & NAS_FLAG_SUPPLICANT)
			sta->suppl.state = sta->suppl.retry_state = WPA_SUP_AUTHENTICATION;
		sta->suppl.pk_state = EAPOL_SUP_PK_UNKNOWN;
#endif
		sta->pae.state = INITIALIZE;

		/* initial mode/wsec/algom. assoc proc will override them */
		if ((nas->mode & RADIUS) && (nas->wsec & WEP_ENABLED)) {
			sta->mode = RADIUS;
			sta->wsec = WEP_ENABLED;
			sta->algo = CRYPTO_ALGO_WEP128;
		}
		else {
			sta->mode = 0;
			sta->wsec = 0;
			sta->algo = 0;
		}
		dbg(nas, "mode %d wsec %d algo %d", sta->mode, sta->wsec, sta->algo);

		/* Add entry to the cache */
		sta->next = nas->sta_hashed[hash];
		nas->sta_hashed[hash] = sta;
		sta->flags = 0;
	}
	sta->last_use = now;
	return sta;
}

/* Transition to new PAE state */
void
pae_state(nas_t *nas, nas_sta_t *sta, int state)
{
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif
	sta->pae.state = state;

	/* New PAE state */
	switch (sta->pae.state) {

	case AUTHENTICATED:
		dbg(nas, "Authenticated %s", ether_etoa((uchar *)&sta->ea, eabuf));
		eapol_canned(nas, sta, EAP_SUCCESS, 0);
		sta->pae.id++;
		sta->rxauths = 0;
		break;

	case HELD:
		dbg(nas, "Held %s", ether_etoa((uchar *)&sta->ea, eabuf));
		eapol_canned(nas, sta, EAP_FAILURE, 0);
		sta->pae.id++;
		/* hold off for the quiet period */
		sta->quiet_while = STA_QUIETWHILE_MAX;

		break;

	case ABORTING:
		dbg(nas, "Aborting %s", ether_etoa((uchar *)&sta->ea, eabuf));
		if (sta->pae.radius.username.data)
			free(sta->pae.radius.username.data);
		memset(&sta->pae.radius.username, 0,
		       sizeof(sta->pae.radius.username));
		if (sta->pae.radius.state.data)
			free(sta->pae.radius.state.data);
		memset(&sta->pae.radius.state, 0,
		       sizeof(sta->pae.radius.state));
		if (sta->pae.radius.request)
			free(sta->pae.radius.request);
		sta->pae.radius.request = NULL;
		sta->pae.id++;

		break;

	case DISCONNECTED:
		dbg(nas, "Disconnected %s", ether_etoa((uchar *)&sta->ea, eabuf));
		eapol_canned(nas, sta, EAP_FAILURE, 0);
		sta->pae.id++;
		sta->pae.state = CONNECTING;
		sta->rxauths = 0;
		/* Fall through */

	case CONNECTING:
		dbg(nas, "Connecting %s", ether_etoa((uchar *)&sta->ea, eabuf));
		sta->pae.flags = 0;	/* reset flags */
		sta->pae.ssnto = 0;	/* session timeout */
		eapol_canned(nas, sta, EAP_REQUEST, EAP_IDENTITY);
		sta->rxauths++;
		sta->tx_when = STA_TXPERIOD_MAX;
		break;

	case AUTHENTICATING:
		dbg(nas, "Authenticating %s", ether_etoa((uchar *)&sta->ea, eabuf));
		sta->auth_while = STA_AUTHWHILE_MAX;
		break;

	default:
		dbg(nas, "Unexpected state %d %s", sta->pae.state,
		    ether_etoa((uchar *)&sta->ea, eabuf));
		break;
	}
}

void
fix_wpa(nas_t *nas, nas_sta_t *sta, char *key, int len)
{
	eapol_header_t eapol;
	eapol_wpa_key_header_t wpa_key;
	unsigned char buffer[sizeof(eapol) + sizeof(wpa_key)];
	wpa_t *wpa = (wpa_t *)nas->wpa;

	dbg(nas, "use MPPE recv key as PMK");
	/* Use the RADIUS key for a PMK. */
	if (len > PMK_LEN)
		len = PMK_LEN;
	memcpy(sta->suppl.pmk, key, len);
	sta->suppl.pmk_len = len;
	/* generate pmkid */
	if (sta->mode & WPA2) {
		nas_wpa_calc_pmkid(wpa, sta);
		if (sta->flags & STA_FLAG_PRE_AUTH) {
			/* Once Pre authed don't worry about this variable */
			sta->flags &= ~STA_FLAG_PRE_AUTH;
			return;
		}
	}

	/* Fake enough of an EAPOL key message to make WPA happy. */
	memcpy(&eapol.eth.ether_shost, &sta->ea, ETHER_ADDR_LEN);
	memcpy(&eapol.eth.ether_dhost, &nas->ea, ETHER_ADDR_LEN);
	eapol.eth.ether_type = htons(ETHER_TYPE_802_1X);
	eapol.version = sta->eapol_version;
	eapol.type = EAPOL_KEY;
	eapol.length = EAPOL_WPA_KEY_LEN;
	memset(&wpa_key, 0, sizeof(wpa_key));
	if (sta->mode & (WPA2_PSK | WPA2))
		wpa_key.type = EAPOL_WPA2_KEY;
	else
		wpa_key.type = EAPOL_WPA_KEY;
	wpa_key.key_info = htons(WPA_KEY_PAIRWISE | WPA_KEY_REQ);
	/* Put the message pieces together.  (They probably were together,
	 *  but it's not valid to assume so.
	 */
	memcpy(buffer, &eapol, sizeof(eapol));
	memcpy(&buffer[offsetof(eapol_header_t, body)], &wpa_key,
	       sizeof(wpa_key));

	/* WPA state should be okay now to start 4-way handshake.  */
	if (process_wpa(wpa, (eapol_header_t *)buffer, sta)) {
		cleanup_sta(nas, sta, DOT11_RC_8021X_AUTH_FAIL, 0);
	}
}


/* Send a canned EAPOL packet */
static void
eapol_canned(nas_t *nas, nas_sta_t *sta, unsigned char code, unsigned char type)
{
	eapol_header_t eapol;
	eap_header_t eap;
	struct iovec frags[2];

	memcpy(&eapol.eth.ether_dhost, &sta->ea, ETHER_ADDR_LEN);
	memcpy(&eapol.eth.ether_shost, &nas->ea, ETHER_ADDR_LEN);
	if (sta->flags & STA_FLAG_PRE_AUTH)
		eapol.eth.ether_type = htons(ETHER_TYPE_802_1X_PREAUTH);
	else
		eapol.eth.ether_type = htons(ETHER_TYPE_802_1X);
	eapol.version = sta->eapol_version;
	eapol.type = EAP_PACKET;
	eapol.length = htons(type ? (EAP_HEADER_LEN + 1) : EAP_HEADER_LEN);

	eap.code = code;
	eap.id = sta->pae.id;
	eap.length = eapol.length;
	eap.type = type;

	frags[0].iov_base = (caddr_t) &eapol;
	frags[0].iov_len = EAPOL_HEADER_LEN;
	frags[1].iov_base = (caddr_t) &eap;
	frags[1].iov_len = ntohs(eapol.length);

	if (sta->flags & STA_FLAG_PRE_AUTH)
		nas_preauth_send_packet(nas, frags, 2);
	else
		nas_eapol_send_packet(nas, frags, 2);
}

#define JAN_1970        0x83aa7e80      /* 2208988800 1970 - 1900 in seconds */

/* How to multiply by 4294.967296 quickly (and not quite exactly)
 * without using floating point or greater than 32-bit integers.
 * If you want to fix the last 12 microseconds of error, add in
 * (2911*(x))>>28)
 */
#define NTPFRAC(x) (4294*(x) + ((1981*(x))>>11))

/* Send a EAPOL-Key packet */
void
eapol_key(nas_t *nas, nas_sta_t *sta,
          unsigned char *send_key, int send_key_len,
          unsigned char *recv_key, int recv_key_len,
          unsigned char *key, int key_len, int index, int unicast)
{
	struct iovec packet;
	eapol_header_t *eapol;
	eapol_key_header_t *body;

	struct timeval tv;
	struct timezone tz;
	unsigned short length;
	unsigned int replay[2];
	unsigned char rc4_seed[48] = { 0 };
	rc4_ks_t rc4_key;

	/* Allocate packet */
	packet.iov_len = EAPOL_HEADER_LEN + EAPOL_KEY_HEADER_LEN;
	if (key)
		packet.iov_len += key_len;
	if (!(packet.iov_base = (caddr_t) malloc(packet.iov_len))) {
		perror("malloc");
		return;
	}

	/* Fill EAPOL header */
	eapol = (eapol_header_t *) packet.iov_base;
	memcpy(&eapol->eth.ether_dhost, &sta->ea, ETHER_ADDR_LEN);
	memcpy(&eapol->eth.ether_shost, &nas->ea, ETHER_ADDR_LEN);
	if (sta->flags & STA_FLAG_PRE_AUTH)
		eapol->eth.ether_type = htons(ETHER_TYPE_802_1X_PREAUTH);
	else
		eapol->eth.ether_type = htons(ETHER_TYPE_802_1X);
	eapol->version = sta->eapol_version;
	eapol->type = EAPOL_KEY;
	eapol->length = htons(packet.iov_len - EAPOL_HEADER_LEN);

	/* Fill EAPOL-Key header */
	body = (eapol_key_header_t *) eapol->body;
	body->type = EAPOL_RC4_KEY;

	/* Length field is unaligned */
	length = htons(key_len);
	memcpy(&body->length, &length, sizeof(body->length));

	/* Replay Counter field is unaligned */
	gettimeofday(&tv, &tz);
	/*
	* keep track timestamp locally in case gettimeofday() does not return
	* correct usec value, for example, on vx. It is ok to not adjust
	* tv.tv_sec since tv.tv_usec is not going to overflow anyway.
	*/
	if (tv.tv_usec == sta->rc4keyusec && tv.tv_sec == sta->rc4keysec) {
		tv.tv_usec += sta->rc4keycntr;
		sta->rc4keycntr ++;
	}
	else {
		sta->rc4keysec = tv.tv_sec;
		sta->rc4keyusec = tv.tv_usec;
		sta->rc4keycntr = 1;
	}
	replay[0] = htonl(tv.tv_sec + JAN_1970);
	replay[1] = htonl(NTPFRAC(tv.tv_usec));
	memcpy(body->replay, replay, sizeof(body->replay));

	/* Fill Key IV */
	nas_rand128(body->iv);

	/* Fill Key Index */
	body->index = index;
	if (unicast)
		body->index |= EAPOL_KEY_UNICAST;

	/* Encrypt Key */
	if (key) {
		memcpy(rc4_seed, body->iv, 16);
		memcpy(&rc4_seed[16], recv_key, recv_key_len);
		prepare_key(rc4_seed, 16 + recv_key_len, &rc4_key);
		memcpy(body->key, key, key_len);
		rc4(body->key, key_len, &rc4_key);
	}

	/* Calculate HMAC-MD5 checksum with null signature */
	if (send_key) {
		memset(body->signature, 0, 16);
		hmac_md5(&eapol->version, packet.iov_len - OFFSETOF(eapol_header_t, version),
		         send_key, send_key_len, body->signature);
	}

	if (sta->flags & STA_FLAG_PRE_AUTH)
		nas_preauth_send_packet(nas, &packet, 1);
	else
		nas_eapol_send_packet(nas, &packet, 1);
	free(packet.iov_base);
}

#ifdef BCMDBG
/* Make a debug message lucid for those who don't know types by number */
static char *
eapol_msg_type_name(int type_no)
{
	char *name;

	switch (type_no) {
	case EAP_PACKET:
		name = "EAP";
		break;
	case EAPOL_START:
		name = "EAPOL start";
		break;
	case EAPOL_LOGOFF:
		name = "EAPOL logoff";
		break;
	case EAPOL_KEY:
		name = "EAPOL key";
		break;
	case EAPOL_ASF:
		name = "EAPOL ASF";
		break;
	default:
		name = "unexpected type";
		break;
	}
	return name;
}
#endif /* BCMDBG */

void
eapol_dispatch(nas_t *nas, eapol_header_t *eapol, int bytes)
{
	eapol_dispatch_ex(nas, eapol, 0, bytes);
}

void
preauth_dispatch(nas_t *nas, eapol_header_t *eapol, int bytes)
{
	if (!nas->disable_preauth)
		eapol_dispatch_ex(nas, eapol, 1, bytes);
}

static void
eapol_dispatch_ex(nas_t *nas, eapol_header_t *eapol, int preauth, int bytes)
{
	nas_sta_t *sta;
	eap_header_t *eap;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif

	/* Validate EAPOL version */
	if (!eapol) {
		dbg(nas, "Missing EAPOL header");
		return;
	}

#ifdef NAS_GTK_PER_STA
	if (ETHER_ISNULLADDR(eapol->eth.ether_shost) && (eapol->type == 0xFF)) {
		wpa_set_gtk_per_sta(nas->wpa, eapol->body[0]);
		return;
	}
#endif

	sta = lookup_sta(nas, (struct ether_addr *) eapol->eth.ether_shost,
	                 SEARCH_ENTER);
	if (!sta) {
		dbg(nas, "no STA struct available");
		return;
	}
	if (sta->pae.state == HELD) {
		dbg(nas, "nothing done since in HELD state");
		return;
	}
	dbg(nas, "%s message from %s", eapol_msg_type_name(eapol->type),
	    ether_etoa((uchar *)&sta->ea, eabuf));
	if (eapol->version < sta->eapol_version) {
		dbg(nas, "EAPOL version %d packet received, current version is %d", eapol->version,
		    sta->eapol_version);
	}

	if (!preauth) {
		/* If this is a WPA key pkt then process it accordingly */
		if ((eapol->type == EAPOL_KEY) &&
		    (CHECK_EAPOL_KEY(eapol->body[0]))) {
			/*
			 * Expect to do this only for WPA_PSK or for WPA either very
			 * early or after RADIUS acceptance.
			 */
			if ((CHECK_NAS(sta->mode)) &&
			    ((sta->pae.state == AUTHENTICATED) ||
			          (sta->pae.state == INITIALIZE))) {
				/* process WPA key pkt */
				if (process_wpa(nas->wpa, eapol, sta)) {
					/* Something wrong in WPA.  Lose this sta. */
					cleanup_sta(nas, sta, DOT11_RC_8021X_AUTH_FAIL, 0);
				}
				return;
			}
			/* return; */
		}

		/* break out if STA is only PSK */
		if (CHECK_PSK(sta->mode))
			return;
		sta->flags &= ~STA_FLAG_PRE_AUTH;
	}
	/*
	* instead of passing the state variable saying whether this data is from a pre auth
	* sta or from a normal  store the variable in the sta struct and
	* would be reset once the message is handled ...  state variable makes sense
	* only for this one message.
	*/
	else {
		sta->flags |= STA_FLAG_PRE_AUTH;
		sta->mode = WPA2;
		sta->eapol_version = WPA2_EAPOL_VERSION;
	}

	/* EAPOL event : Radius support */
	switch (eapol->type) {

	case EAP_PACKET:
		dbg(nas, "EAP Packet.Preauth=%d", preauth);
		if (ntohs(eapol->length) >= (EAP_HEADER_LEN + 1)) {
			eap = (eap_header_t *) eapol->body;
			dbg(nas, "STA State=%d EAP Packet Type=%d Id=%d code=%d",
			    sta->pae.state, eap->type, eap->id, eap->code);

			switch (eap->type) {

			case EAP_IDENTITY:
				/* Bogus packet */
				if (eap->id != sta->pae.id ||
					eap->code != EAP_RESPONSE) {
					dbg(nas, "bogus EAP packet id %d code %d, expected %d",
					    eap->id, eap->code, sta->pae.id);
					break;
				}

				if (sta->pae.state == AUTHENTICATING) {
					dbg(nas, "NAS currently authenticating STA. "
					    "Tossing packet id=%d code=%d.",
					    eap->id, eap->code);

					break;
				}

				/*
				 * Do eap length checking to prevent attacker responds to
				 * an EAP Identity Request with an EAP Identity Response,
				 * whose Message Length value is larger than the actual
				 * EAP packet, nas sends out a Radius message with more
				 * data than contained in the EAP Identity Response.  That
				 * cause nas over-reads the heap memory when forming
				 * the Radius message. An attack may result in exposure
				 * of sensitive data from the heap or may allow to cause
				 * a denial-of-service.
				 */
				if (ntohs(eap->length) > bytes - EAPOL_HEADER_LEN) {
					dbg(nas, "Invalid EAP length %d, received %d.",
					    ntohs(eap->length), bytes - EAPOL_HEADER_LEN);
					break;
				}

				/* Record identity */
				if (ntohs(eap->length) > (EAP_HEADER_LEN + 1)) {
					if (sta->pae.radius.username.data)
						free(sta->pae.radius.username.data);
					sta->pae.radius.username.length = ntohs(eap->length) -
					        EAP_HEADER_LEN - 1;
					if (!(sta->pae.radius.username.data =
					      malloc(sta->pae.radius.username.length))) {
						perror("malloc");
						return;
					} else
						memcpy(sta->pae.radius.username.data, eap->data,
						       sta->pae.radius.username.length);
				}
				pae_state(nas, sta, AUTHENTICATING);
				/* Fall through */

			default:
				/* Bogus packet */

				/* Forward to authentication server */
				RADIUS_FORWARD(nas, sta, eap);
				break;
			}
		}
		break;

	case EAPOL_START:
		dbg(nas, "Start");
		sta->pae.id = 0;	/* reset counter  */
		switch (sta->pae.state) {
		case AUTHENTICATING:
			pae_state(nas, sta, ABORTING);
			/* Fall through */

		default:
			pae_state(nas, sta, CONNECTING);
			break;
		}
		sta->suppl.state = sta->suppl.retry_state = WPA_PTKSTART;
		break;

	case EAPOL_LOGOFF:
		dbg(nas, "Logoff");
		switch (sta->pae.state) {
		default:
			dbg(nas, "Unexpected pae state %d", sta->pae.state);
			/* Fall through */
		case AUTHENTICATING:
			pae_state(nas, sta, ABORTING);
			/* Fall through */

		case CONNECTING:
		case AUTHENTICATED:
			pae_state(nas, sta, DISCONNECTED);
			dbg(nas, "deauthenticating %s", ether_etoa((uchar *)&sta->ea, eabuf));
			nas_deauthorize(nas, &sta->ea);
			break;
		}
		break;

	case EAPOL_KEY:
		dbg(nas, "Key");
		break;

	case EAPOL_ASF:
		dbg(nas, "Encapsulated ASF Alert");
		break;

	default:
		dbg(nas, "unknown EAPOL type %d", eapol->type);
		break;
	}
}

#ifdef BCMSUPPL
void
eapol_sup_dispatch(nas_t *nas, eapol_header_t *eapol)
{
	nas_sta_t *sta;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif

	if (!eapol) {
		dbg(nas, "Missing EAPOL header");
		return;
	}

	sta = lookup_sta(nas, (struct ether_addr *) eapol->eth.ether_shost,
	                 SEARCH_ENTER);
	if (!sta) {
		dbg(nas, "No STA struct available");
		return;
	}

	dbg(nas, "%s message from %s", eapol_msg_type_name(eapol->type),
	    ether_etoa((uchar *)&sta->ea, eabuf));
	if (eapol->version < sta->eapol_version) {
		dbg(nas, "EAPOL version %d packet received, current version is %d", eapol->version,
		    sta->eapol_version);
	}

	/* If this is a WPA key pkt then process it accordingly */
	if ((eapol->type == EAPOL_KEY) &&
	    (CHECK_EAPOL_KEY(eapol->body[0]))) {
		/* Expect to do this only for WPA_PSK or for WPA either very
		 * early or after RADIUS acceptance.
		 */
		if ((CHECK_NAS(sta->mode)) &&
		    ((sta->pae.state == AUTHENTICATED) || (sta->pae.state == INITIALIZE))) {
			/* process WPA key pkt */
			if (process_sup_wpa(nas->wpa, eapol, sta)) {
				/* Something wrong in WPA.  Lose this pae. */
				cleanup_sta(nas, sta, DOT11_RC_8021X_AUTH_FAIL, 0);
			}
			return;
		}
	}

	err(nas, "unknown EAPOL type %d", eapol->type);
}
#endif /* BCMSUPPL */

#ifdef BCMDBG
/* Make a debug message lucid for those who don't know types by number */
static char *
driver_msg_name(int type)
{
	switch (type) {
	case WLC_E_LINK:
		return "LINK";
	case WLC_E_ASSOC_IND:
		return "ASSOC";
	case WLC_E_REASSOC_IND:
		return "REASSOC";
	case WLC_E_DISASSOC_IND:
		return "DISASSOC";
	case WLC_E_DEAUTH_IND:
		return "DEAUTH";
	case WLC_E_MIC_ERROR:
		return "MIC error";
	default:
		return "unknown";
	}
}
#endif /* BCMDBG */

void
driver_message_dispatch(nas_t *nas, bcm_event_t *dpkt)
{
	wl_event_msg_t *event = &(dpkt->event);
	int type = ntohl(event->event_type);
	uint8 *addr = (uint8 *)&(event->addr);
	nas_sta_t *sta;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif

	/* !!!THESE ARE THE MESSAGES WE CARE!!! */
	dbg(nas, "received event of type : %d\n", type);
	switch (type) {
	case WLC_E_LINK:
		/* WLC_E_LINK evnet on WDS is use for trigger nas_start */
		if (nas->flags & NAS_FLAG_WDS)
			return;
		/* authenticator is not interested in LINK event */
		if (nas->flags & NAS_FLAG_AUTHENTICATOR) {
			nas_get_wpacap(nas, nas->wpa->cap);
			if (!WSEC_WEP_ENABLED(nas->wsec)) {
				nas->flags &= ~NAS_FLAG_GTK_PLUMBED;
			}
			return;
		}
		/* and the supplicant is only interested in link up */
		if (!(ntohs(event->flags) & WLC_EVENT_MSG_LINK)) {
			return;
		}
	case WLC_E_ASSOC_IND:
	case WLC_E_REASSOC_IND:
	case WLC_E_DISASSOC_IND:
	case WLC_E_DEAUTH_IND:
	case WLC_E_MIC_ERROR:
#ifdef WLWNM
	case WLC_E_WNM_STA_SLEEP:
#endif /* WLWNM */
		break;
	default:
		/* quietly discard unwanted events */
		return;
	}

	dbg(nas, "start");

	dbg(nas, "driver %s message received for %s", driver_msg_name(type),
	    ether_etoa(addr, eabuf));

	/* Look for the STA struct, but don't create one if the goal is
	 *  to remove it.
	 */
	sta = lookup_sta(nas, (struct ether_addr *)addr,
		(type == WLC_E_DISASSOC_IND || type == WLC_E_DEAUTH_IND ||
		type == WLC_E_MIC_ERROR) ? SEARCH_ONLY : SEARCH_ENTER);

	switch (type) {

	case WLC_E_LINK:
	case WLC_E_ASSOC_IND:
	case WLC_E_REASSOC_IND:
		if (!(CHECK_NAS(nas->mode))) {
			dbg(nas, "Unexpected driver %s message in mode %d", driver_msg_name(type),
			    nas->mode);
			return;
		}
		if (wpa_driver_assoc_msg(nas->wpa, dpkt, sta) == 0)
			break;

		/* clean-up stuff if there was a problem. */
		if (sta)
			cleanup_sta(nas, sta, 0, 1);

		break;

	case WLC_E_DISASSOC_IND:
	case WLC_E_DEAUTH_IND:
		if (wpa_driver_disassoc_msg(nas->wpa, dpkt, sta) == 0)
			break;

		/* clean-up stuff if there was a problem. */
		if (sta)
			cleanup_sta(nas, sta, 0, 1);

		break;

	case WLC_E_MIC_ERROR:
		if ((ntohs(event->flags) & WLC_EVENT_MSG_GROUP) &&
		    (nas->flags & NAS_FLAG_AUTHENTICATOR)) {
			/* flags - authenticator and supplicant are mutually exclusive. */
			assert(!(nas->flags & NAS_FLAG_SUPPLICANT));
			dbg(nas, "Group addressed MIC error notification received by AP");
			return;
		}

		dbg(nas, "%scast MIC error notification received - %sknown STA",
			((ntohs(event->flags) & WLC_EVENT_MSG_GROUP) ? "multi" : "uni"),
			(!sta ? "un" : ""));

		/* ignore errors from STAs we don't know about. */
		if (sta)
			wpa_mic_error(nas->wpa, sta, TRUE);
		break;

#ifdef WLWNM
	case WLC_E_WNM_STA_SLEEP:
		if (sta != NULL) {
			int sleeping = ntohl(event->status);
			int mfp = ntohl(event->reason);
#ifdef BCMDBG
			char eabuf[ETHER_ADDR_STR_LEN];
			dbg(nas, "Sleep event: sta %s: %x->%x (MFP:%x)",
				ether_etoa((uchar *)&sta->ea, eabuf), sta->sleeping, sleeping, mfp);
#endif
			/* If STA is getting out of sleep and does not support MFP, send the key */
			if (!sleeping && sta->sleeping) {
				if (!mfp && sta->gtk_expire)
					wpa_send_rekey_frame(nas->wpa, sta);
				sta->gtk_expire = FALSE;
			}

			sta->sleeping = sleeping;
		}
		break;
#endif /* WLWNM */

	default:
		dbg(nas, "Tossing unexpected event #%u", type);
	}

	dbg(nas, "done");
}

static void
nas_watchdog(bcm_timer_id td, nas_t *nas)
{
	nas_sta_t *sta;
	int i;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif
	for (i = 0; i < MAX_SUPPLICANTS; i ++) {
		for (sta = nas->sta_hashed[i]; sta; sta = sta->next) {
#ifdef BCMDBG
			if (nas->auth_blockout_time) {
				if (sta->pae.flags & PAE_FLAG_RADIUS_ACCESS_REJECT)
					dbg(nas, "blocking %s, time remaining = %d",
					    ether_etoa((uchar *)&sta->ea, eabuf), sta->quiet_while);
			}
#endif
			/* check for time out */
			switch (sta->pae.state) {
			case AUTHENTICATED:
				if (!sta->pae.ssnto)
					break;
				sta->pae.ssnto--;
				/* timed out */
				if (sta->pae.ssnto)
					continue;
				/* send Identity Request */
				dbg(nas, "ID req to %s\n", ether_etoa((uchar *)&sta->ea, eabuf));
				send_identity_req(nas, sta);
				break;
			case HELD:
				if (sta->quiet_while && --sta->quiet_while == 0)
					pae_state(nas, sta, CONNECTING);
				break;
			case CONNECTING:
				if (sta->tx_when && --sta->tx_when == 0) {
					if (sta->rxauths > STA_REAUTH_MAX)
						pae_state(nas, sta, DISCONNECTED);
					else
						pae_state(nas, sta, CONNECTING);
				}
				break;
			case AUTHENTICATING:
				if (sta->auth_while && --sta->auth_while == 0) {
					pae_state(nas, sta, ABORTING);
					pae_state(nas, sta, CONNECTING);
				}
				break;
			default:
				break;
			}
		}
	}
}

static void
nas_start_watchdog(nas_t *nas)
{
	itimer_status_t ts;

	if (nas->watchdog_td)
		TIMER_DELETE(nas->watchdog_td);

	ts = wpa_set_itimer(nas->timer, &nas->watchdog_td,
	                    (bcm_timer_cb)nas_watchdog,
	                    (int)nas, 1, 0);
	if (ts != ITIMER_OK)
		dbg(nas, "Session timeout timer set failed, code %d", ts);
}

void
nas_start(nas_t *nas)
{
	nas_sta_t *sta;

	/* reset all STAs */
	bzero((void *)nas->sta, sizeof(nas->sta));
	bzero((void *)nas->sta_hashed, sizeof(nas->sta_hashed));
	nas->MIC_failures = 0;
	nas->MIC_countermeasures = 0;
	nas->prev_MIC_error = 0;

	/* start session count down timer */
	if (CHECK_RADIUS(nas->mode))
		nas_start_watchdog(nas);

	/* initiate/request pairwise key exchange */
	if (!(nas->flags & NAS_FLAG_WDS))
		return;

	sta = lookup_sta(nas, (struct ether_addr *)nas->remote,
	                 SEARCH_ENTER);
	if (!sta) {
#ifdef BCMDBG
		char eabuf[ETHER_ADDR_STR_LEN];
#endif
		dbg(nas, "sta %s not available", ether_etoa(nas->remote, eabuf));
		return;
	}

	/* Assume the peer use the same cipher and akm */
	if (CHECK_PSK(nas->mode)) {
		sta->suppl.pmk_len = nas->wpa->pmk_len;
		bcopy(nas->wpa->pmk, sta->suppl.pmk, nas->wpa->pmk_len);
	}
	/*
	 * There is no beacon/proberesp/assocreq across WDS, therefore
	 * we have no way to know what cipher and akm the peer is configured
	 * for. But assuming the peer uses the same configuration seems
	 * reasonable. No WEP when doing WPA over WDS.
	 */
	wpa_set_suppl(nas->wpa, sta, nas->mode, nas->wsec, CRYPTO_ALGO_OFF);

#ifdef BCMSUPPL
	if (nas->flags & NAS_FLAG_SUPPLICANT)
		wpa_request(nas->wpa, sta);
#endif
	if (nas->flags & NAS_FLAG_AUTHENTICATOR)
		wpa_start(nas->wpa, sta);
}

int
nas_handle_error(nas_t *nas, int error)
{
	/* Handle the Error cases one by one */
	err(nas, "NAS encountered a non recoverable Error, so resetting the board\n");
	nas_reset_board();
	while (1);
	return 1;
}

void
nas_force_rekey(nas_t *nas)
{
	if (nas->flags & NAS_FLAG_AUTHENTICATOR)
		wpa_new_gtk(nas->wpa);
}
