/*
 * WPS AP eap state machine
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ap_eap_sm.c 383924 2013-02-08 04:14:39Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#if defined(__linux__) || defined(__ECOS)
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef __ECOS
#include <net/if.h>
#include <sys/param.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#endif /* __linux__ || __ECOS */

#include <typedefs.h>
#include <bcmutils.h>
#include <proto/ethernet.h>
#include <proto/eapol.h>
#include <proto/eap.h>

#ifdef WIN32
#include <windows.h>
#endif

#ifdef TARGETOS_symbian
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <tutrace.h>

#include <wps_wl.h>
#include <wps_apapi.h>
#include <ap_eap_sm.h>

#if defined(TARGETOS_nucleus)
uint16 ntohs(uint16 netshort);
#endif /* TARGETOS_nucleus */

static unsigned char* lookupSta(unsigned char *sta_mac, sta_lookup_mode_t mode);
static void cleanUpSta(int reason, int deauthFlag);
static void ap_eap_sm_sendEapol(char *eapol_data, char *eap_data, unsigned short eap_len);
static void ap_eap_sm_sendFailure(void);
static int ap_eap_sm_req_start(void);
static int ap_eap_sm_process_protocol(char *eap_msg);
static uint32 ap_eap_sm_sta_validate(char *eapol_msg);
static uint32 ap_eap_sm_create_pkt(char * dataBuffer, int dataLen, uint8 eapCode);
static int ap_eap_sm_sendFACK();


static EAP_WPS_AP_STATE s_apEapState;
static EAP_WPS_AP_STATE *apEapState = NULL;
static char zero_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/* Macro */
#define WPS_STA_ACTIVE()	memcmp(&apEapState->sta_mac, zero_mac, ETHER_ADDR_LEN)
#define WPS_DATA_OFFSET		(EAPOL_HEADER_LEN + sizeof(WpsEapHdr))

#define WPS_AP_M2D_READY()	(apEapState->flags & AP_EAP_SM_AP_M2D_READY)
#define WPS_M2D_DISCOVERY()	\
	((apEapState->flags & AP_EAP_SM_M1_RECVD) && \
	!(apEapState->flags & AP_EAP_SM_M2_SENT))

/* AP dependent */
#define IS_EAP_TYPE_WPS(eapHdr) \
	((eapHdr)->code == EAP_CODE_RESPONSE && (eapHdr)->type == EAP_TYPE_WPS)
#define FRAGMENTING(apEap)	\
	((apEap)->next_frag_to_send != (apEap)->msg_to_send)
#define REASSEMBLING(apEap, eapHdr)	\
	((apEap)->total_bytes_to_recv || (eapHdr)->flags & EAP_WPS_FLAGS_MF)

#define WPS_MSGTYPE_OFFSET		9
#define WPS_FIVE_SECONDS_TIMEOUT	5

/* WSC 2.0 */
static uint8 apEap_wps_version2 = 0;

struct apEap_wpsid_list {
	char identity_str[32];
	int identity_len;
	int wps_eap_id;
};

enum { AP_EAP_WPS_ID_ENROLLEE = 0, AP_EAP_WPS_ID_REGISTRAR, AP_EAP_WPS_ID_NONE };
static struct apEap_wpsid_list apEap_wpsid_list[2] = {
	{"WFA-SimpleConfig-Enrollee-1-0", 29, AP_EAP_WPS_ID_ENROLLEE},
	{"WFA-SimpleConfig-Registrar-1-0", 30, AP_EAP_WPS_ID_REGISTRAR}
};


/*
 * Search for or create a STA.
 * We can only handle only ONE station at a time.
 * If `enter' is not set, do not create it when one is not found.
 */
static unsigned char*
lookupSta(unsigned char *sta_mac, sta_lookup_mode_t mode)
{
	unsigned char *mac = NULL;
	int sta_active = WPS_STA_ACTIVE();

	/* Search if the entry in on the list */
	if (sta_active &&
		(!memcmp(&apEapState->sta_mac, sta_mac, ETHER_ADDR_LEN))) {
		mac = apEapState->sta_mac;
	}

	/* create a new entry if necessary */
	if (!mac && mode == SEARCH_ENTER) {
		if (!sta_active) {
			/* Initialize entry */
			memcpy(&apEapState->sta_mac, sta_mac, ETHER_ADDR_LEN);
			/* Initial STA state: */
			apEapState->state = INIT;
			apEapState->eap_id = 0;
			mac = apEapState->sta_mac;
		}
		else
			TUTRACE((TUTRACE_ERR,  "Sta in use\n"));
	}

	return mac;
}

static void
cleanUpSta(int reason, int deauthFlag)
{
	if (!WPS_STA_ACTIVE()) {
		TUTRACE((TUTRACE_ERR,  "cleanUpSta: Not a good sta\n"));
		return;
	}

	if (deauthFlag)
		wps_deauthenticate(apEapState->bssid, apEapState->sta_mac, reason);

	memset(&apEapState->sta_mac, 0, ETHER_ADDR_LEN);
	apEapState->sent_msg_id = 0;
	apEapState->recv_msg_id = 0;
	apEapState->flags = 0;

	TUTRACE((TUTRACE_ERR,  "cleanup STA Finish\n"));

	return;
}

static int
ap_eap_sm_get_wpsid(char *buf)
{
	eapol_header_t *eapol = (eapol_header_t *)buf;
	eap_header_t *eap = (eap_header_t *) eapol->body;
	int i;

	if (eap->type != EAP_IDENTITY)
		return AP_EAP_WPS_ID_NONE;

	for (i = 0; i < 2; i++) {
		if (memcmp((char *)eap->data, apEap_wpsid_list[i].identity_str,
			apEap_wpsid_list[i].identity_len) == 0) {
			return apEap_wpsid_list[i].wps_eap_id;
		}
	}

	return AP_EAP_WPS_ID_NONE;
}

/* Send a EAPOL packet */
static void
ap_eap_sm_sendEapol(char *eapol_data, char *eap_data, unsigned short eap_len)
{
	eapol_header_t *eapolHdr = (eapol_header_t *)eapol_data;
	int sendLen;

	/* Construct ether header */
	memcpy(&eapolHdr->eth.ether_dhost, &apEapState->sta_mac, ETHER_ADDR_LEN);
	memcpy(&eapolHdr->eth.ether_shost, &apEapState->bssid, ETHER_ADDR_LEN);
	eapolHdr->eth.ether_type = WpsHtons(ETHER_TYPE_802_1X);

	/* Construct EAPOL header */
	eapolHdr->version = WPA_EAPOL_VERSION;
	eapolHdr->type = EAP_PACKET;
	eapolHdr->length = WpsHtons(eap_len);

	if ((char*)eapolHdr->body != eap_data)
		memcpy(eapolHdr->body, eap_data, eap_len);

	sendLen = EAPOL_HEADER_LEN + eap_len;

	/* increase send_count */
	if (!WPS_AP_M2D_READY())
		apEapState->send_count ++;

	if (apEapState->send_data)
		(*apEapState->send_data)(eapol_data, sendLen);

	apEapState->last_sent = eapol_data;
	apEapState->last_sent_len = sendLen;
	return;
}

/* Send one fragmented packet. */
static void
ap_eap_sm_sendNextFrag(WpsEapHdr *wpsEapHdr)
{
	eapol_header_t *feapolHdr = NULL;
	WpsEapHdr *fwpsEapHdr = NULL;
	uint8 flags = 0;
	int max_to_send = apEapState->eap_frag_threshold;


	/* Locate EAPOL and EAP header */
	feapolHdr = (eapol_header_t *)(apEapState->next_frag_to_send - WPS_DATA_OFFSET);
	fwpsEapHdr = (WpsEapHdr *)feapolHdr->body;

	/* How maximum length to send */
	if (apEapState->frag_size < apEapState->eap_frag_threshold)
		max_to_send = apEapState->frag_size;

	/* More fragmentations check */
	if (apEapState->frag_size > max_to_send)
		flags = EAP_WPS_FLAGS_MF;

	/* Create one fragmentation */
	*fwpsEapHdr = *wpsEapHdr;
	fwpsEapHdr->id = apEapState->eap_id;
	fwpsEapHdr->flags = flags;
	fwpsEapHdr->length = WpsHtons(sizeof(WpsEapHdr) + max_to_send);

	ap_eap_sm_sendEapol((char *)feapolHdr, (char *)fwpsEapHdr,
		WpsNtohs((uint8 *)&fwpsEapHdr->length));
}

static void
ap_eap_sm_sendEap(char *eap_data, unsigned short eap_len)
{
	WpsEapHdr *fwpsEapHdr = (WpsEapHdr *)eap_data;
	uint8 *fwpsEapHdrLF = (uint8 *)(fwpsEapHdr + 1);
	unsigned short wps_len = eap_len - sizeof(WpsEapHdr);

	/* No fragmentation need, shift EAP_WPS_LF_OFFSET */
	if (wps_len <= apEapState->eap_frag_threshold) {
		ap_eap_sm_sendEapol(&apEapState->msg_to_send[EAP_WPS_LF_OFFSET],
			eap_data, eap_len);
		return;
	}

	/* Modify FIRST fragmentation */
	fwpsEapHdr->flags = (EAP_WPS_FLAGS_MF | EAP_WPS_FLAGS_LF);
	fwpsEapHdr->length = WpsHtons(sizeof(WpsEapHdr) + apEapState->eap_frag_threshold);

	/* Set WPS data length field */
	WpsHtonsPtr((uint8*)&wps_len, (uint8*)fwpsEapHdrLF);

	/* Set next_frag_to_send to first WpsEapHdr to indecate first fragmentation has sent */
	apEapState->next_frag_to_send = &apEapState->msg_to_send[EAPOL_HEADER_LEN];

	/* frag_szie only save wps_len */
	apEapState->frag_size = wps_len;

	ap_eap_sm_sendEapol(apEapState->msg_to_send, (char *)fwpsEapHdr,
		WpsNtohs((uint8 *)&fwpsEapHdr->length));
}


static void
ap_eap_sm_sendFailure(void)
{
	eapol_header_t *eapolHdr = (eapol_header_t *) apEapState->msg_to_send;
	WpsEapHdr *wpsEapHdr = (WpsEapHdr *) eapolHdr->body;

	TUTRACE((TUTRACE_ERR,  "EAP: Building EAP-Failure (id=%d)\n", apEapState->eap_id));

	memset(wpsEapHdr, 0, sizeof(WpsEapHdr));
	wpsEapHdr->code = EAP_CODE_FAILURE;
	wpsEapHdr->id = apEapState->eap_id;
	/* Only send code, id and length, otherwise the DTM testing will have trouble */
	wpsEapHdr->length = WpsHtons(4);

	ap_eap_sm_sendEapol(apEapState->msg_to_send, (char *)wpsEapHdr, 4);
	apEapState->state = EAP_START_SEND;

	return;
}

static int
ap_eap_sm_req_start(void)
{
	/* reset msg_len, shift EAP_WPS_LF_OFFSET */
	apEapState->msg_len = sizeof(apEapState->msg_to_send) - WPS_DATA_OFFSET
		- EAP_WPS_LF_OFFSET;
	return wps_processMsg(apEapState->mc_dev, NULL, 0,
		&apEapState->msg_to_send[WPS_DATA_OFFSET + EAP_WPS_LF_OFFSET],
		(uint32 *)&apEapState->msg_len, TRANSPORT_TYPE_EAP);
}

static uint32
ap_eap_sm_apm2d_alloc()
{
	char *data;
	int len;

	/* Delay send built-in registrar M2D */
	len = ap_eap_sm_get_msg_to_send(&data);
	if (len <= 0)
		return WPS_CONT;

	/* Allocate ap_m2d_data buffer */
	if (apEapState->ap_m2d_data == NULL || apEapState->ap_m2d_len < len) {
		/* Free old */
		if (apEapState->ap_m2d_data)
			free(apEapState->ap_m2d_data);

		/* Get new */
		apEapState->ap_m2d_data = (char *)malloc(len);
		if (apEapState->ap_m2d_data == NULL)
			return WPS_CONT;
	}

	memcpy(apEapState->ap_m2d_data, data, len);
	apEapState->ap_m2d_len = len;

	/* Set WPS_AP_M2D_READY */
	apEapState->flags |= AP_EAP_SM_AP_M2D_READY;
	return WPS_CONT;
}

static void
ap_eap_sm_apm2d_free()
{
	if (apEapState->ap_m2d_data) {
		free(apEapState->ap_m2d_data);
		apEapState->ap_m2d_data = NULL;
	}
}

int ap_eap_sm_process_timeout()
{
	/* If there is a message pending, re-send for a limited number of time. */
	if (apEapState->last_sent_len == 0) {
		TUTRACE((TUTRACE_ERR, "No data avaliable to retransmit.\n"));
		return EAP_TIMEOUT;
	}

	if (apEapState->resent_count <= EAP_MAX_RESEND_COUNT) {
		ap_eap_sm_resend_last_sent();
		TUTRACE((TUTRACE_ERR, "timeout receiving from STA. Retry previous message\n"));
		return WPS_CONT;
	}
	else {
		TUTRACE((TUTRACE_ERR, "Retry count exceeded. Bail out\n"));
		return EAP_TIMEOUT;
	}
}

int
ap_eap_sm_check_timer(uint32 time)
{
	int mode;

	if (!apEapState) {
		return WPS_CONT;
	}

	if (apEapState->send_count != 0) {
		/* update current time */
		apEapState->last_check_time = time;

		/* reset send_count */
		apEapState->send_count = 0;
		return WPS_CONT;
	}

	/* Check every 5 seconds if send count is 0 */
	if ((time - apEapState->last_check_time) < WPS_FIVE_SECONDS_TIMEOUT)
		return WPS_CONT;

	/* update current time */
	apEapState->last_check_time = time;

	/* Timed out for waiting NACK, send EAP-Fail, and close */
	if (apEapState->sent_msg_id == WPS_ID_MESSAGE_NACK) {
		/* send failure */
		/* WSC 2.0, send deauthentication followed by EAP-Fail */
		ap_eap_sm_Failure(apEap_wps_version2 ? 1 : 0);

		TUTRACE((TUTRACE_ERR, "Wait NACK time out!\n"));
		return EAP_TIMEOUT;
	}

	/* Process based on mode */
	mode = wps_get_mode(apEapState->mc_dev);
	if (mode == SCMODE_AP_ENROLLEE) {
		if (apEapState->sent_msg_id != 0 && ap_eap_sm_process_timeout() != WPS_CONT) {
			/* Timed-out, send eap failure */
			/* WSC 2.0, send deauthentication followed by EAP-Fail */
			ap_eap_sm_Failure(apEap_wps_version2 ? 1 : 0);

			return EAP_TIMEOUT;
		}
	}
	else if (mode == SCMODE_AP_REGISTRAR) {
		/* Check delay send built-in registrar M2D first */
		/* For DTM WCN Wireless ID 463. M1-M2D Proxy Functionality */
		if (WPS_AP_M2D_READY() && WPS_M2D_DISCOVERY()) {
			/* Send delay AP_M2D */
			ap_eap_sm_sendMsg(apEapState->ap_m2d_data, apEapState->ap_m2d_len);

			/* Adjust last_check_time to tigger M2D timeout in next second */
			apEapState->last_check_time -= (WPS_FIVE_SECONDS_TIMEOUT -1);

			/* Clear WPS_AP_M2D_READY */
			apEapState->flags &= ~AP_EAP_SM_AP_M2D_READY;
			return WPS_CONT;
		}

		/* Check M2/M2D timeout */
		if (WPS_M2D_DISCOVERY()) {
			/* WSC 2.0, send deauthentication followed by EAP-Fail */
			ap_eap_sm_Failure(apEap_wps_version2 ? 1 : 0);

			TUTRACE((TUTRACE_ERR, "SCMODE_AP_REGISTRAR M2/M2D timeout!\n"));
			/* Continue SCMODE_AP_REGISTRAR state machine */

			return WPS_ERR_M2D_TIMEOUT;
		}

		if (apEapState->sent_msg_id != 0 && ap_eap_sm_process_timeout() != WPS_CONT) {
			/* Timed-out, send eap failure */
			/* WSC 2.0, send deauthentication followed by EAP-Fail */
			ap_eap_sm_Failure(apEap_wps_version2 ? 1 : 0);

			return EAP_TIMEOUT;
		}
	}

	return WPS_CONT;
}


static int
ap_eap_sm_process_protocol(char *eap_msg)
{
	WpsEapHdr *wpsEapHdr = (WpsEapHdr *)eap_msg;
	char *in_data = (char *)(wpsEapHdr + 1);
	uint32 in_len = WpsNtohs((uint8*)&wpsEapHdr->length) - sizeof(WpsEapHdr);

	/* record the in message id, ingore WPS_Start */
	if (wpsEapHdr->opcode != WPS_Start) {
		apEapState->recv_msg_id = in_data[WPS_MSGTYPE_OFFSET];
		if (apEapState->recv_msg_id == WPS_ID_MESSAGE_M1)
			apEapState->flags |= AP_EAP_SM_M1_RECVD;
	}

	/* reset msg_len, shift EAP_WPS_LF_OFFSET */
	apEapState->msg_len = sizeof(apEapState->msg_to_send) - WPS_DATA_OFFSET
		- EAP_WPS_LF_OFFSET;
	return wps_processMsg(apEapState->mc_dev, in_data, in_len,
		&apEapState->msg_to_send[WPS_DATA_OFFSET + EAP_WPS_LF_OFFSET],
		(uint32 *)&apEapState->msg_len, TRANSPORT_TYPE_EAP);
}

static uint32
ap_eap_sm_sta_validate(char *eapol_msg)
{
	eapol_header_t *eapolHdr = (eapol_header_t *) eapol_msg;
	WpsEapHdr * wpsEapHdr = (WpsEapHdr *) eapolHdr->body;
	unsigned char *mac;

	/* Ignore non 802.1x EAP BRCM packets (dont error) */
	if (WpsNtohs((uint8*)&eapolHdr->eth.ether_type) != ETHER_TYPE_802_1X)
		return WPS_ERR_GENERIC;

	/* Ignore EAPOL_LOGOFF */
	if (eapolHdr->type == EAPOL_LOGOFF) {
		return WPS_CONT;
	}

	if ((WpsNtohs((uint8*)&eapolHdr->length) <= EAP_HEADER_LEN) ||
		(eapolHdr->type != EAP_PACKET) ||
		((wpsEapHdr->type != EAP_EXPANDED) && (wpsEapHdr->type != EAP_IDENTITY))) {
		return WPS_ERR_GENERIC;
	}

	mac = lookupSta(eapolHdr->eth.ether_shost, SEARCH_ONLY);
	if ((wpsEapHdr->type == EAP_IDENTITY) && mac) {
		if (memcmp(eapolHdr->eth.ether_shost, mac, ETHER_ADDR_LEN)) {
			TUTRACE((TUTRACE_ERR,  "Other sta's EAP_IDENTITY comes, ignore it...\n"));
			return WPS_CONT;
		}
		else {
			TUTRACE((TUTRACE_ERR,  "EAP_IDENTITY comes again...\n"));
		}
	}

	mac = lookupSta(eapolHdr->eth.ether_shost, SEARCH_ENTER);
	if (!mac) {
		TUTRACE((TUTRACE_ERR,  "no sta...\n"));
		return WPS_ERR_GENERIC;
	}

	return WPS_SUCCESS;
}

/*
 * Check if we already have requested and our peer didn't receive it.
 * Also check that that the id matches the response. AP dependent.
 */
static uint32
ap_eap_sm_sta_retrans(char *eapol_msg)
{
	char *wps_data;
	eapol_header_t *eapolHdr = (eapol_header_t *) eapol_msg;
	WpsEapHdr * wpsEapHdr = (WpsEapHdr *) eapolHdr->body;


	/* Re-ransmissions detection, fragmenting /Reassembling checking */
	if (IS_EAP_TYPE_WPS(wpsEapHdr) &&
	    (FRAGMENTING(apEapState) || REASSEMBLING(apEapState, wpsEapHdr))) {
		if (wpsEapHdr->id == apEapState->eap_id - 1) {
			TUTRACE((TUTRACE_INFO, "wpsEapHdr->id=%d, apEapState->eap_id=%d\n",
				wpsEapHdr->id, apEapState->eap_id));
			TUTRACE((TUTRACE_INFO, "Peer's retransmission of previous messages(FRAG)"
				", ignore it\n"));
			return WPS_IGNORE_MSG_CONT;
		}

		return WPS_CONT;
	}

	switch (apEapState->state) {
	case INIT :
		if (wpsEapHdr->type != EAP_TYPE_IDENTITY)
			return WPS_ERR_GENERIC;

		if (wpsEapHdr->id != apEapState->eap_id ||
			wpsEapHdr->code != EAP_CODE_RESPONSE) {
			TUTRACE((TUTRACE_ERR,  "bogus EAP packet id %d code %d, "
				"expected %d\n", wpsEapHdr->id, wpsEapHdr->code,
				apEapState->eap_id));
			cleanUpSta(DOT11_RC_8021X_AUTH_FAIL, 1);
			return WPS_ERR_GENERIC;
		}

		TUTRACE((TUTRACE_ERR,  "Response, Identity, code=%d, id=%d, length=%d, type=%d\n",
			wpsEapHdr->code, wpsEapHdr->id,
			WpsNtohs((uint8*)&wpsEapHdr->length), wpsEapHdr->type));

		/* Store whcih if sta come from */
		memcpy(&apEapState->bssid, &eapolHdr->eth.ether_dhost, ETHER_ADDR_LEN);
		break;

	case EAP_START_SEND:
	case PROCESSING_PROTOCOL:
		if (wpsEapHdr->code != EAP_CODE_RESPONSE) {
			TUTRACE((TUTRACE_ERR,  "bogus EAP packet not response code %d\n",
				wpsEapHdr->code));
			cleanUpSta(DOT11_RC_8021X_AUTH_FAIL, 1);
			return WPS_ERR_GENERIC;
		}

		if (wpsEapHdr->id != apEapState->eap_id) {
			/*
			 * Ignore eap_id check in ACK message,
			 * When there are more than 2 ER, AP may receive
			 * ACK for M2D-1 after AP send M2D-2, in this case
			 * the M2D-1 and M2D-2 use same eap_id because
			 * eap_id increased by receiving peer eap packet.
			 * One other case AP receive M2D-1 and M2-2.
			 * Note; see WSC 2.0 spec figure 9.
			 */
			if (apEapState->sent_msg_id == WPS_ID_MESSAGE_M2 ||
			    apEapState->sent_msg_id == WPS_ID_MESSAGE_M2D) {
				if (wpsEapHdr->opcode == WPS_ACK)
					break; /* Ignore retrans */
				else if (wpsEapHdr->opcode == WPS_MSG) {
					wps_data = (char *)(wpsEapHdr + 1);
					if (wps_data[WPS_MSGTYPE_OFFSET] == WPS_ID_MESSAGE_M3)
						break; /* Ignore retrans */
				}
			}

			/*
			 * Do retransmission when comes messages with id = eap_id -1.
			 * They are probably retransmissions of previous messages.
			 */
			if (wpsEapHdr->id == apEapState->eap_id - 1) {
				TUTRACE((TUTRACE_INFO, "wpsEapHdr->id=%d, apEapState->eap_id=%d\n",
					wpsEapHdr->id, apEapState->eap_id));
				TUTRACE((TUTRACE_INFO, "Peer's retransmission of previous messages"
					",ignore it\n"));
				return WPS_IGNORE_MSG_CONT;
			}

			TUTRACE((TUTRACE_ERR,  "bogus EAP packet id %d, expected %d\n",
				wpsEapHdr->id, apEapState->eap_id));
			cleanUpSta(DOT11_RC_8021X_AUTH_FAIL, 1);
			return WPS_ERR_GENERIC;
		}
		break;
	}

	return WPS_CONT;
}

static uint32
ap_eap_sm_create_pkt(char * dataBuffer, int dataLen, uint8 eapCode)
{
	eapol_header_t *eapolHdr = (eapol_header_t *)apEapState->msg_to_send;
	WpsEapHdr *wpsEapHdr;
	int LF_bytes = EAP_WPS_LF_OFFSET;

	/* Shift EAP_WPS_LF_OFFSET */
	if (dataLen < apEapState->eap_frag_threshold) {
		eapolHdr = (eapol_header_t *)&apEapState->msg_to_send[EAP_WPS_LF_OFFSET];
		LF_bytes = 0;
	}

	/* Construct EAP header */
	wpsEapHdr = (WpsEapHdr *)eapolHdr->body;

	wpsEapHdr->code = eapCode;
	wpsEapHdr->id = apEapState->eap_id;
	wpsEapHdr->length = WpsHtons(sizeof(WpsEapHdr) + dataLen);
	wpsEapHdr->type = EAP_TYPE_WPS;
	wpsEapHdr->vendorId[0] = WPS_VENDORID1;
	wpsEapHdr->vendorId[1] = WPS_VENDORID2;
	wpsEapHdr->vendorId[2] = WPS_VENDORID3;
	wpsEapHdr->vendorType = WpsHtonl(WPS_VENDORTYPE);

	if (dataBuffer) {
		if (dataBuffer[WPS_MSGTYPE_OFFSET] >= WPS_ID_MESSAGE_M1 &&
			dataBuffer[WPS_MSGTYPE_OFFSET] <= WPS_ID_MESSAGE_M8) {
			wpsEapHdr->opcode = WPS_MSG;
		}
		else if (dataBuffer[WPS_MSGTYPE_OFFSET] == WPS_ID_MESSAGE_ACK) {
			wpsEapHdr->opcode = WPS_ACK;
		}
		else if (dataBuffer[WPS_MSGTYPE_OFFSET] == WPS_ID_MESSAGE_NACK) {
			wpsEapHdr->opcode = WPS_NACK;
		}
		else if (dataBuffer[WPS_MSGTYPE_OFFSET] == WPS_ID_MESSAGE_DONE) {
			wpsEapHdr->opcode = WPS_Done;
		}
		else {
			TUTRACE((TUTRACE_ERR, "Unknown Message Type code %d; "
				"Not sending msg\n", dataBuffer[WPS_MSGTYPE_OFFSET]));
			return TREAP_ERR_SENDRECV;
		}

		/* record the out message id, we don't save fragmented sent_msg_id */
		apEapState->sent_msg_id = dataBuffer[WPS_MSGTYPE_OFFSET];
		if (apEapState->sent_msg_id == WPS_ID_MESSAGE_M2)
			apEapState->flags |= AP_EAP_SM_M2_SENT;

		/* TBD: Flags are always set to zero for now, if message is too big */
		/*
		 * fragmentation must be done here and flags will have some bits set
		 * and message length field is added
		 */
		/* Default flags are set to zero, let ap_eap_sm_sendEap to handle it */
		wpsEapHdr->flags = 0;

		/* in case we passed this buffer to the protocol, do not copy :-) */
		if (dataBuffer - LF_bytes != (char *)(wpsEapHdr + 1))
			memcpy((char *)wpsEapHdr + sizeof(WpsEapHdr) + LF_bytes,
				dataBuffer, dataLen);
	}

	return WPS_SUCCESS;
}

/*
 * Receive a fragmentation then send EAP_FRAG_ACK.
 */
static int
ap_eap_sm_process_frag(char *eap_msg)
{
	int ret;
	int LF_bytes = 0;
	WpsEapHdr *wpsEapHdr = (WpsEapHdr *)eap_msg;
	uint32 wps_len = WpsNtohs((uint8*)&wpsEapHdr->length) - sizeof(WpsEapHdr);
	int wps_data_received = apEapState->total_received - sizeof(WpsEapHdr);


	TUTRACE((TUTRACE_INFO, "Receive a EAP WPS fragment packet\n"));

	/* Total length checking */
	if (apEapState->total_bytes_to_recv < wps_data_received + wps_len) {
		TUTRACE((TUTRACE_ERR, "Received WPS data len %d excess total bytes to "
			"receive %d\n", wps_data_received + wps_len,
			apEapState->total_bytes_to_recv));
		return EAP_FAILURE;
	}

	/* Copy to frags_received */
	/* First fragmentation include WpsEapHdr without length field */
	if (apEapState->total_received == 0) {
		memcpy(apEapState->frags_received, eap_msg, sizeof(WpsEapHdr));
		apEapState->total_received += sizeof(WpsEapHdr);

		/* Ignore length field 2 bytes copy */
		LF_bytes = EAP_WPS_LF_OFFSET;
	}
	/* WPS data */
	memcpy(&apEapState->frags_received[apEapState->total_received],
		eap_msg + sizeof(WpsEapHdr) + LF_bytes,	wps_len - LF_bytes);
	apEapState->total_received += wps_len - LF_bytes;

	/* Is last framentation? */
	if (wpsEapHdr->flags & EAP_WPS_FLAGS_MF) {
		/* Increase eap_id */
		apEapState->eap_id++;

		/* Send WPS_FRAG_ACK */
		ret = ap_eap_sm_sendFACK();
		if (ret == WPS_SUCCESS)
			return WPS_CONT;

		return ret;
	}

	/* Got all fragmentations, adjust WpsEapHdr */
	wpsEapHdr = (WpsEapHdr *)apEapState->frags_received;
	wpsEapHdr->length = WpsHtons((uint16)apEapState->total_received);

	TUTRACE((TUTRACE_ERR, "Received all WPS fragmentations, total bytes of WPS data "
		"to receive %d, received %d\n", apEapState->total_bytes_to_recv,
		apEapState->total_received - sizeof(WpsEapHdr)));

	return WPS_SUCCESS;
}

uint32
ap_eap_sm_process_sta_msg(char *msg, int msg_len)
{
	eapol_header_t *eapolHdr = NULL;
	WpsEapHdr *wpsEapHdr = NULL;
	int len;
	int wps_data_sent_bytes = apEapState->eap_frag_threshold;
	int advance_bytes = apEapState->eap_frag_threshold;
	int wps_eap_id;
	char *total_bytes_to_recv;
	WPS_SCMODE e_mode;
	uint32 ret;


	/* Check initialization */
	if (apEapState == 0)
		return WPS_MESSAGE_PROCESSING_ERROR;

	if (apEapState->parse_msg)
		eapolHdr = (eapol_header_t *) (*apEapState->parse_msg)(msg, msg_len, &len);

	if (!eapolHdr) {
		TUTRACE((TUTRACE_ERR,  "Missing EAPOL header\n"));
		return WPS_ERR_GENERIC;
	}

	/* Validate eapol message */
	if ((ret = ap_eap_sm_sta_validate((char*)eapolHdr)) != WPS_SUCCESS) {
		TUTRACE((TUTRACE_ERR,  "Vaildation failed...\n"));
		return ret;
	}

	/* Reset resend count   */
	apEapState->resent_count = 0;

	/*
	 * Handle same station eap identity response comes again:
	 * 1. When AP is a enrollee, we have to take care of same station eap identity response
	 *     comes again change to "WFA-SimpleConfig-Enrollee-1-0"
	 * 2. When AP is a registrar, we have to take care of same station eap identity response
	 *     comes again change to "WFA-SimpleConfig-Registrar-1-0"
	 */
	wpsEapHdr = (WpsEapHdr *)eapolHdr->body;
	if (wpsEapHdr->type == EAP_TYPE_IDENTITY) {

		wps_eap_id = ap_eap_sm_get_wpsid(msg);
		/* Ignore unrecognized WPS eap id */
		if (wps_eap_id == AP_EAP_WPS_ID_NONE)
			return WPS_CONT;

		e_mode = wps_get_mode(apEapState->mc_dev);
		/* Case 1, AP is running as Enrollee send EAP-Request(M1) */
		if (e_mode == SCMODE_AP_ENROLLEE) {
			/* Peer switch to Enrollee comes again */
			if (wps_eap_id == AP_EAP_WPS_ID_ENROLLEE) {
				TUTRACE((TUTRACE_INFO, "We are running as Enrollee but comes "
					"again EAP Identity Response changes to WFA-SimpleConfig-"
					"Enrollee-1-0. Return ERROR and close session!\n"));
				return WPS_MESSAGE_PROCESSING_ERROR;

			}
			/* Registrar comes again */
			else if (apEapState->sent_msg_id == WPS_ID_MESSAGE_M1) {
				/* Re-send last sent message */
				ap_eap_sm_resend_last_sent();
				return WPS_CONT;
			}
		}
		/* Case 2, AP is running as Registrar send EAP-Request(Start) */
		else if (e_mode == SCMODE_AP_REGISTRAR) {
			/* Peer switch to Registrar comes again */
			if (wps_eap_id == AP_EAP_WPS_ID_REGISTRAR) {
				TUTRACE((TUTRACE_INFO, "We are running as Registrar but comes "
					"again EAP Identity Response changes to WFA-SimpleConfig-"
					"Registrar-1-0. Return ERROR and close session!\n"));
				return WPS_MESSAGE_PROCESSING_ERROR;
			}
			/* Enrollee comes again */
			else if (apEapState->state == EAP_START_SEND) {
				/* Re-send last sent message */
				ap_eap_sm_resend_last_sent();
				return WPS_CONT;
			}
		}
	}

	/* STA retransmission checking */
	ret = ap_eap_sm_sta_retrans((char*)eapolHdr);
	if (ret != WPS_SEND_RET_MSG_CONT && ret != WPS_CONT) {
		if (ret == WPS_IGNORE_MSG_CONT)
			ret = WPS_CONT;
		return ret;
	}
	else if (ret == WPS_SEND_RET_MSG_CONT) {
		/* Re-send last sent message */
		ap_eap_sm_resend_last_sent();
		return WPS_CONT;
	}

	/*
	 * For Fragmentation: Test if we are waiting for a WPS_FRAG_ACK
	 * If so, only accept that code and send the next fragment directly
	 * with ap_eap_sm_sendNextFrag().  Return with no error.
	 */
	if (IS_EAP_TYPE_WPS(wpsEapHdr) && FRAGMENTING(apEapState)) {
		/* Only accept WPS_FRAG_ACK and send next fragmentation */
		if (wpsEapHdr->opcode == WPS_FRAG_ACK) {
			TUTRACE((TUTRACE_INFO, "Receive a WPS fragment ACK packet, send next\n"));

			/* Is first WPS_FRAG_ACK? */
			if (apEapState->next_frag_to_send ==
				&apEapState->msg_to_send[EAPOL_HEADER_LEN]) {
				wps_data_sent_bytes -= EAP_WPS_LF_OFFSET;
				advance_bytes += sizeof(WpsEapHdr);
			}

			/* Advance to next fragment point */
			apEapState->next_frag_to_send += advance_bytes;
			apEapState->frag_size -= wps_data_sent_bytes;
			apEapState->eap_id++;

			/* Get msg_to_send WpsEapHdr */
			eapolHdr = (eapol_header_t *)apEapState->msg_to_send;
			wpsEapHdr = (WpsEapHdr *)eapolHdr->body;

			/* Send next fragmented packet */
			ap_eap_sm_sendNextFrag(wpsEapHdr);

			return WPS_CONT;
		}
		/* Bypass non WPS_MSG */
		else if (wpsEapHdr->opcode == WPS_MSG) {
			if (apEapState->frag_size < apEapState->eap_frag_threshold) {
				/* All fragmentations sent completed, reset next_frag_to_send */
				apEapState->next_frag_to_send = apEapState->msg_to_send;
				apEapState->frag_size = 0;
				/* Fall through, process this message. */
			} else {
				TUTRACE((TUTRACE_INFO, "Not a WPS_FRAG_ACK packet, ingore it\n"));
				return WPS_CONT;
			}
		}
	}

	/* For reassembly: Test if this is a fragmented packet.
	 * If it is the begining, set the expected total.
	 * Copy new data at position (frags_received  + total_received) and increase total_received.
	 * Make sure to detect re-transmissions (same ID)?.
	 * When receiving a packet without the "More Fragment" flags,
	 * continue, otherwise send a WPS_FRAG_ACK with ap_eap_sm_sendFACK() and return;
	 */
	total_bytes_to_recv = (char *)(wpsEapHdr + 1);
	if (IS_EAP_TYPE_WPS(wpsEapHdr) && REASSEMBLING(apEapState, wpsEapHdr)) {
		if (apEapState->total_bytes_to_recv == 0) {
			/* First fragmentation must have length information */
			if (!(wpsEapHdr->flags & EAP_WPS_FLAGS_LF)) {
				return EAP_FAILURE;
			}

			/* Save total EAP message length */
			apEapState->total_bytes_to_recv = WpsNtohs((uint8*)total_bytes_to_recv);
			TUTRACE((TUTRACE_INFO, "Got EAP WPS total bytes to receive info %d\n",
				apEapState->total_bytes_to_recv));
		}

		ret = ap_eap_sm_process_frag((char *)wpsEapHdr);
		if (ret != WPS_SUCCESS)
			return ret;

		/* fall through when all fragmentations are received. */
		apEapState->total_received = 0; /* Reset total_received */
		apEapState->total_bytes_to_recv = 0;

		/* Use reassembled buffer */
		wpsEapHdr = (WpsEapHdr *)apEapState->frags_received;
	}

	switch (apEapState->state) {
		case INIT :
			/* Reset counter  */
			apEapState->eap_id = 1;

			/* Request Start message */
			return ap_eap_sm_req_start();

		case EAP_START_SEND:
		case PROCESSING_PROTOCOL:
			apEapState->eap_id++;

			ret = ap_eap_sm_process_protocol((char *)wpsEapHdr);

			/* For DTM WCN Wireless ID 463. M1-M2D Proxy Functionality */
			if (ret == WPS_AP_M2D_READY_CONT)
				ret = ap_eap_sm_apm2d_alloc();
			return ret;

		case REG_SUCCESSFUL:
			return REG_SUCCESSFUL;
	}

	return WPS_CONT;
}

int
ap_eap_sm_startWPSReg(unsigned char *sta_mac, unsigned char *ap_mac)
{
	unsigned char *mac;
	int retVal;

	mac = lookupSta(sta_mac, SEARCH_ENTER);

	if (!mac) {
		TUTRACE((TUTRACE_ERR,  "no sta...\n"));
		return -1;
	}

	TUTRACE((TUTRACE_ERR,  "Build WPS Start!\n"));

	/* reset counter  */
	apEapState->eap_id = 1;

	/* store whcih if sta come from */
	memcpy(&apEapState->bssid, ap_mac, ETHER_ADDR_LEN);

	/* Request Start message */
	retVal = ap_eap_sm_req_start();

	return retVal;
}

uint32
ap_eap_sm_init(void *mc_dev, char *mac_sta, char * (*parse_msg)(char *, int, int *),
	unsigned int (*send_data)(char *, uint32), int eap_frag_threshold)
{
	TUTRACE((TUTRACE_INFO,  "Initial...\n"));

	memset(&s_apEapState, 0, sizeof(EAP_WPS_AP_STATE));
	apEapState = &s_apEapState;
	apEapState->mc_dev = mc_dev;
	memcpy(apEapState->sta_mac, mac_sta, ETHER_ADDR_LEN);
	if (parse_msg)
		apEapState->parse_msg = parse_msg;
	if (send_data)
		apEapState->send_data = send_data;

	/* For fragmentation */
	apEapState->next_frag_to_send = apEapState->msg_to_send;

	/* WSC 2.0 */
	apEap_wps_version2 = wps_get_version2(mc_dev);
	apEapState->eap_frag_threshold = EAP_WPS_FRAG_MAX;

	if (eap_frag_threshold >= 100 && eap_frag_threshold < EAP_WPS_FRAG_MAX)
		apEapState->eap_frag_threshold = eap_frag_threshold;


	TUTRACE((TUTRACE_INFO,  "ap_eap_sm_init: EAP Frag Threshold %d\n",
		apEapState->eap_frag_threshold));

	return WPS_SUCCESS;
}

uint32
ap_eap_sm_deinit()
{
	if (!apEapState) {
		TUTRACE((TUTRACE_ERR, "Not initialized; Returning\n"));
		return WPS_ERR_NOT_INITIALIZED;
	}

	if (WPS_STA_ACTIVE()) {
		cleanUpSta(DOT11_RC_8021X_AUTH_FAIL, 1);
	}

	ap_eap_sm_apm2d_free();

	return WPS_SUCCESS;
}

unsigned char *
ap_eap_sm_getMac(int mode)
{
	if ((mode != SCMODE_AP_ENROLLEE) && WPS_STA_ACTIVE())
		return (apEapState->sta_mac);

	return (wps_get_mac_income(apEapState->mc_dev));
}

uint32
ap_eap_sm_sendMsg(char *dataBuffer, uint32 dataLen)
{
	uint32 retVal;
	eapol_header_t *eapolHdr = (eapol_header_t *)apEapState->msg_to_send;
	WpsEapHdr *wpsEapHdr;

	TUTRACE((TUTRACE_INFO, "In ap_eap_sm_sendMsg buffer Length = %d\n",
		dataLen));

	retVal = ap_eap_sm_create_pkt(dataBuffer, dataLen, EAP_CODE_REQUEST);
	if (retVal == WPS_SUCCESS) {
		/* Shift EAP_WPS_LF_OFFSET */
		if (dataLen < apEapState->eap_frag_threshold)
			eapolHdr = (eapol_header_t *)&apEapState->msg_to_send[EAP_WPS_LF_OFFSET];

		wpsEapHdr = (WpsEapHdr *)eapolHdr->body;
		ap_eap_sm_sendEap((char *)wpsEapHdr, WpsNtohs((uint8*)&wpsEapHdr->length));
		apEapState->state = PROCESSING_PROTOCOL;
	}
	else {
		TUTRACE((TUTRACE_ERR,  "Send EAP FAILURE to station!\n"));
		ap_eap_sm_Failure(apEap_wps_version2 ? 1 : 0);

		retVal = TREAP_ERR_SENDRECV;
	}

	return retVal;
}

uint32
ap_eap_sm_sendWPSStart()
{
	WpsEapHdr wpsEapHdr;

	/* check sta status */
	if (!WPS_STA_ACTIVE()) {
		TUTRACE((TUTRACE_ERR,  "sta not in use!\n"));
		return WPS_ERROR_MSG_TIMEOUT;
	}

	wpsEapHdr.code = EAP_CODE_REQUEST;
	wpsEapHdr.id = apEapState->eap_id;
	wpsEapHdr.length = WpsHtons(sizeof(WpsEapHdr));
	wpsEapHdr.type = EAP_TYPE_WPS;
	wpsEapHdr.vendorId[0] = WPS_VENDORID1;
	wpsEapHdr.vendorId[1] = WPS_VENDORID2;
	wpsEapHdr.vendorId[2] = WPS_VENDORID3;
	wpsEapHdr.vendorType = WpsHtonl(WPS_VENDORTYPE);
	wpsEapHdr.opcode = WPS_Start;
	wpsEapHdr.flags = 0;

	ap_eap_sm_sendEapol(apEapState->msg_to_send, (char *)&wpsEapHdr, sizeof(WpsEapHdr));
	apEapState->state = EAP_START_SEND;
	apEapState->sent_msg_id = WPS_PRIVATE_ID_WPS_START;

	return WPS_SUCCESS;
}

/* Send EAP fragment ACK */
static int
ap_eap_sm_sendFACK()
{
	WpsEapHdr wpsEapHdr;

	/* check sta status */
	if (!WPS_STA_ACTIVE()) {
		TUTRACE((TUTRACE_ERR,  "sta not in use!\n"));
		return WPS_ERROR_MSG_TIMEOUT;
	}

	TUTRACE((TUTRACE_INFO, "Build EAP fragment ACK\n"));

	wpsEapHdr.code = EAP_CODE_REQUEST;
	wpsEapHdr.id = apEapState->eap_id;
	wpsEapHdr.length = WpsHtons(sizeof(WpsEapHdr));
	wpsEapHdr.type = EAP_TYPE_WPS;
	wpsEapHdr.vendorId[0] = WPS_VENDORID1;
	wpsEapHdr.vendorId[1] = WPS_VENDORID2;
	wpsEapHdr.vendorId[2] = WPS_VENDORID3;
	wpsEapHdr.vendorType = WpsHtonl(WPS_VENDORTYPE);
	wpsEapHdr.opcode = WPS_FRAG_ACK;
	wpsEapHdr.flags = 0;

	ap_eap_sm_sendEapol(apEapState->msg_to_send, (char *)&wpsEapHdr, sizeof(WpsEapHdr));

	return WPS_SUCCESS;
}

int
ap_eap_sm_get_msg_to_send(char **data)
{
	if (!apEapState) {
		*data = NULL;
		return -1;
	}

	*data = &apEapState->msg_to_send[WPS_DATA_OFFSET + EAP_WPS_LF_OFFSET];
	return apEapState->msg_len;
}

int
ap_eap_sm_resend_last_sent()
{
	int ret = 0;

	if (apEapState->send_data)
		(*apEapState->send_data)(apEapState->last_sent, apEapState->last_sent_len);
	else
		ret = -1;

	apEapState->resent_count++;
	return ret;
}

void
ap_eap_sm_Failure(int deauthFlag)
{
	apEapState->state = EAP_FAILURED;
	ap_eap_sm_sendFailure();

	/* Sleep a short time to avoid de-auth send before EAP-Failure */
	if (deauthFlag)
		WpsSleepMs(10);

	cleanUpSta(0, deauthFlag);
	return;
}

int
ap_eap_sm_get_sent_msg_id()
{
	if (!apEapState)
		return -1;

	return ((int)apEapState->sent_msg_id);
}

int
ap_eap_sm_get_recv_msg_id()
{
	if (!apEapState)
		return -1;

	return ((int)apEapState->recv_msg_id);
}

int
ap_eap_sm_get_eap_state()
{
	return ((int)apEapState->state);
}
