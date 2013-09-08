/*
 * EAP WPS state machine specific to STA
 *
 * Can apply both to enrollee sta and wireless registrar.
 * This code is intended to be used on embedded platform on a mono-thread system,
 * using as litle resources as possible.
 * It is also trying to be as little system dependant as possible.
 * For that reason we use statically allocated memory.
 * This code IS NOT RE-ENTRANT.
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: sta_eap_sm.c 295985 2011-11-13 02:57:31Z $
 */

#include <bn.h>
#include <wps_dh.h>
#include <tutrace.h>

#include <wpscommon.h>
#include <wpserror.h>
#include <portability.h>
#include <reg_prototlv.h>
#include <sminfo.h>
#include <reg_protomsg.h>
#include <reg_proto.h>
#include <statemachine.h>
#include <wps_staeapsm.h>
#include <wps_enrapi.h>
#include <eap_defs.h>
#include <wps_enr.h>

/* useful macros */
#define ARRAYSIZE(a)  (sizeof(a)/sizeof(a[0]))

#include <packed_section_start.h>
typedef BWL_PRE_PACKED_STRUCT struct eap_header {
	uint8 code;
	uint8 id;
	uint16 length;
	uint8 type;
} BWL_POST_PACKED_STRUCT EAP_HDR;

typedef BWL_PRE_PACKED_STRUCT struct eapol_hdr {
	uint8 version;
	uint8 type;
	uint16 len;
} BWL_POST_PACKED_STRUCT EAPOL_HDR;
#include <packed_section_end.h>

typedef struct  {
	char state; /* current state. */
	char sent_msg_id; /* out wps message ID */
	char recv_msg_id; /* in wps message ID */
	unsigned char eap_id; /* identifier of the last request  */
	char peer_mac[6];
	uint8 sta_type; /* enrollee or registrar */
	char pad1;
	char msg_to_send[WPS_EAP_DATA_MAX_LENGTH]; /* message to be sent */
	int msg_len;

	/* For fragmented packets */
	char frags_received[WPS_EAP_DATA_MAX_LENGTH]; /* buffer to store all fragmentations  */
	/*
	 * From the Length Field of the first frag.  To be set to 0 when packet is not fragmented
	 * or set to the new value when receiving the begining of the next fragmented packet.
	 */
	int total_bytes_to_recv; /* total bytes of WPS data need to receive */
	int total_received; /* total EAP data received so far (include EAP header and WPS data) */
	/*
	 * After a fragment is sent and acked, the next_frag_to_send pointer
	 * is moved along the msg_to_send buffer. It is reset to the
	 * begining of the buffer after all fragments have been sent.
	 * When this pointer is not at the begining, the receive ap_eap_sm_process_sta_msg
	 * must only accept WPS_FRAG_ACK messages.
	 */
	char *next_frag_to_send; /* pointer to the next fragment to send */
	int frag_size;

	char *last_sent; /* remember last sent EAPOL packet location */
	int last_sent_len;
	uint32 time_sent; /* Time of the first call to wps_get_msg_to_send  (per msg) */
	int send_count; /* number of times the current message has been sent */
	void *sm_hdle; /* handle to state machine  */
	int eap_frag_threshold;

	char prev_frag[EAP_WPS_FRAG_MAX];
	int prev_frag_len;

	int retcode;
} eap_wps_sta_state;

#define EAP_MSG_OFFSET 4
#define WPS_DATA_OFFSET	(EAP_MSG_OFFSET + sizeof(WpsEapHdr))

static eap_wps_sta_state s_staEapState;
static eap_wps_sta_state *staEapState = NULL;
static int sta_e_mode = SCMODE_STA_ENROLLEE;
static int staEapState_eap_frag_threshold = EAP_WPS_FRAG_MAX;

static char *WpsEapEnrIdentity =  "WFA-SimpleConfig-Enrollee-1-0";
static char *WpsEapRegIdentity =  "WFA-SimpleConfig-Registrar-1-0";
static char* WPS_MSG_STRS[] = {
	" NONE",
	"(BEACON)",
	"(PROBE_REQ)",
	"(PROBE_RESP)",
	"(M1)",
	"(M2)",
	"(M2D)",
	"(M3)",
	"(M4)",
	"(M5)",
	"(M6)",
	"(M7)",
	"(M8)",
	"(ACK)",
	"(NACK)",
	"(DONE)",
	" / Identity",
	"(Start)",
	"(FAILURE)",
	"(FRAG)",
	"(FRAG_ACK)"
};

extern int WpsEnrolleeSM_Step(void *handle, uint32 msgLen, uint8 *p_msg);

static int wps_eap_eapol_start(char *pkt);
static int wps_eap_ap_retrans(char *eapol_msg, int eapol_len);
static int wps_eap_enr_process_protocol(char *eap_msg, int eap_len);
static int wps_eap_identity_resp(char *pkt);
static int wps_eap_frag_ack();

/* Macro */
/* Sta dependent */
#define IS_EAP_TYPE_WPS(eapHdr) \
	((eapHdr)->code == EAP_CODE_REQUEST && (eapHdr)->type == EAP_TYPE_WPS)
#define FRAGMENTING(staEap)	\
	((staEap)->next_frag_to_send != (staEap)->msg_to_send)
#define REASSEMBLING(staEap, eapHdr)	\
	((staEap)->total_bytes_to_recv || (eapHdr)->flags & EAP_WPS_FLAGS_MF)
#define IS_PREV_FRAG(staEap, eapHdr, len)	\
	(((staEap)->prev_frag_len != 0) && (staEap)->prev_frag_len == len && \
	memcmp((staEap)->prev_frag, eapHdr, len) == 0)

int
wps_eap_sta_init(char *bssid, void *handle, int e_mode)
{
	/*
	 * I like working on pointer better, and that way, we can make sure
	 * we are initialized...
	 */
	memset(&s_staEapState, 0, sizeof(s_staEapState));
	staEapState = &s_staEapState;
	if (bssid)
		memcpy(staEapState->peer_mac, bssid, 6);

	/* prepare the EAPOL START */
	wps_eap_eapol_start(staEapState->msg_to_send);
	staEapState->sm_hdle = handle;
	sta_e_mode = e_mode;

	/* For fragmentation */
	staEapState->next_frag_to_send = staEapState->msg_to_send;
	staEapState->eap_frag_threshold = staEapState_eap_frag_threshold;

	TUTRACE((TUTRACE_INFO, "sta eap sm Eap fragment threshold %d\n",
		staEapState->eap_frag_threshold));

	return 0;
}

/*
 * Here we only update for Failure / Identity Req / WPS Start.
 * There is other update point at wps_eap_enr_process_protocol( )
*/
static int
wps_recv_msg_id_upd(char *eap_msg)
{
	WpsEapHdr *wpsEapHdr = (WpsEapHdr *)eap_msg;

	if (wpsEapHdr->code == EAP_CODE_FAILURE) {
		/* Failure */
		staEapState->recv_msg_id = WPS_PRIVATE_ID_FAILURE;
	}
	else if (wpsEapHdr->type == EAP_TYPE_IDENTITY) {
		/* Identity */
		staEapState->recv_msg_id = WPS_PRIVATE_ID_IDENTITY;
	}
	else if (wpsEapHdr->opcode == WPS_Start) {
		/* WPS Start */
		staEapState->recv_msg_id = WPS_PRIVATE_ID_WPS_START;
	}

	/* There is other update point at wps_eap_enr_process_protocol( ) */

	return 0;
}

/* Prepare one fragmented packet and store in last_sent. */
static int
wps_eap_next_frag(WpsEapHdr *wpsEapHdr)
{
	EAPOL_HDR *feapol = NULL;
	WpsEapHdr *fwpsEapHdr = NULL;
	uint8 flags = 0;
	unsigned short eap_len;
	int max_to_send = staEapState->eap_frag_threshold;


	/* Locate EAPOL and EAP header */
	feapol = (EAPOL_HDR *)(staEapState->next_frag_to_send - WPS_DATA_OFFSET);
	fwpsEapHdr = (WpsEapHdr *)(feapol + 1);

	/* How maximum length to send */
	if (staEapState->frag_size < staEapState->eap_frag_threshold)
		max_to_send = staEapState->frag_size;

	/* More fragmentations check */
	if (staEapState->frag_size - max_to_send)
		flags = EAP_WPS_FLAGS_MF;

	/* Create one fragmentation */
	eap_len = (unsigned short)(sizeof(WpsEapHdr) + max_to_send);

	*fwpsEapHdr = *wpsEapHdr;
	fwpsEapHdr->id = staEapState->eap_id;
	fwpsEapHdr->flags = flags;
	fwpsEapHdr->length = WpsHtons(eap_len);

	/* Construct EAPOL header */
	feapol->version = 1;
	feapol->type = 0;
	feapol->len = fwpsEapHdr->length;

	staEapState->last_sent = (char *)feapol;
	staEapState->last_sent_len = EAP_MSG_OFFSET + eap_len;

	return WPS_SEND_FRAG_CONT;
}

static uint32
wps_eapol_create_pkt(char *eap_data, unsigned short eap_len, char **eapol_pkt)
{
	EAPOL_HDR *eapol;
	WpsEapHdr *wpsEapHdr;
	char *data;
	unsigned short data_len;
	uint32 sendLen;

	WpsEapHdr *fwpsEapHdr = (WpsEapHdr *)eap_data;
	uint8 *fwpsEapHdrLF = (uint8 *)(fwpsEapHdr + 1);
	unsigned short wps_len = eap_len - sizeof(WpsEapHdr);


	/* No fragmentation need, shift EAP_WPS_LF_OFFSET */
	if (wps_len <= staEapState->eap_frag_threshold) {
		eapol = (EAPOL_HDR *)&staEapState->msg_to_send[EAP_WPS_LF_OFFSET];
		data = eap_data;
		data_len = eap_len;
	}
	else {
		/* Modify FIRST fragmentation */
		fwpsEapHdr->flags = (EAP_WPS_FLAGS_MF | EAP_WPS_FLAGS_LF);
		fwpsEapHdr->length =
			WpsHtons((uint16)(sizeof(WpsEapHdr) + staEapState->eap_frag_threshold));

		/* Set WPS data length field */
		WpsHtonsPtr((uint8*)&wps_len, (uint8*)fwpsEapHdrLF);

		/*
		 * Set next_frag_to_send to first WpsEapHdr to indecate first
		 * fragmentation has sent
		 */
		staEapState->next_frag_to_send = &staEapState->msg_to_send[EAP_MSG_OFFSET];

		/* frag_szie only save wps_len */
		staEapState->frag_size = wps_len;

		eapol = (EAPOL_HDR *)staEapState->msg_to_send;
		data = (char *)fwpsEapHdr;
		data_len = WpsNtohs((uint8 *)&fwpsEapHdr->length);
	}

	wpsEapHdr = (WpsEapHdr *)(eapol + 1);

	/* Construct EAPOL header */
	eapol->version = 1;
	eapol->type = 0;
	eapol->len = WpsHtons(data_len);

	if ((char *)wpsEapHdr != data)
		memcpy((char *)wpsEapHdr, data, data_len);

	sendLen = EAP_MSG_OFFSET + data_len;

	staEapState->last_sent = (char *)eapol;
	staEapState->last_sent_len = sendLen;

	*eapol_pkt = (char *)eapol;
	return sendLen;
}

/* If receive a fragmentation prepare EAP_FRAG_ACK packet */
static int
wps_eap_process_frag(char *eap_msg)
{
	int LF_bytes = 0;
	WpsEapHdr *wpsEapHdr = (WpsEapHdr *)eap_msg;
	uint32 wps_len = WpsNtohs((uint8*)&wpsEapHdr->length) - sizeof(WpsEapHdr);
	int wps_data_received = staEapState->total_received - sizeof(WpsEapHdr);


	TUTRACE((TUTRACE_INFO, "Receive a EAP WPS fragment packet\n"));

	/* Total length checking */
	if (staEapState->total_bytes_to_recv < wps_data_received + (int)wps_len) {
		TUTRACE((TUTRACE_ERR, "Received WPS data len %d excess total bytes to "
			"receive %d\n", wps_data_received + wps_len,
			staEapState->total_bytes_to_recv));
		return EAP_FAILURE;
	}

	/* Copy to frags_received */
	/* In first fragmentation, copy WpsEapHdr without length field */
	if (staEapState->total_received == 0) {
		/* include WpsEapHdr without length info field */
		memcpy(staEapState->frags_received, eap_msg, sizeof(WpsEapHdr));
		staEapState->total_received += sizeof(WpsEapHdr);

		/* Ignore EAP_WPS_LF_OFFSET copy */
		LF_bytes = EAP_WPS_LF_OFFSET;
	}
	/* WPS data */
	memcpy(&staEapState->frags_received[staEapState->total_received],
		eap_msg + sizeof(WpsEapHdr) + LF_bytes, wps_len - LF_bytes);
	staEapState->total_received += wps_len - LF_bytes;

	/* Is last framentation? */
	if (wpsEapHdr->flags & EAP_WPS_FLAGS_MF)
		return wps_eap_frag_ack();

	/* Got all fragmentations, adjust WpsEapHdr */
	wpsEapHdr = (WpsEapHdr *)staEapState->frags_received;
	wpsEapHdr->length = WpsHtons((uint16)staEapState->total_received);

	TUTRACE((TUTRACE_ERR, "Received all WPS fragmentations, total bytes of WPS data "
		"to receive %d, received %d\n", staEapState->total_bytes_to_recv,
		staEapState->total_received - sizeof(WpsEapHdr)));

	return WPS_SUCCESS;
}

/* sta dependent  */
int
wps_process_ap_msg(char *eapol_msg, int eapol_len)
{
	EAPOL_HDR *eapol = (EAPOL_HDR *)eapol_msg;
	WpsEapHdr *wpsEapHdr = (WpsEapHdr *)(eapol + 1);
	int eap_len = WpsNtohs((uint8 *)&wpsEapHdr->length);
	int status = 0;
	int ret;
	int wps_data_sent_bytes = staEapState->eap_frag_threshold;
	int advance_bytes = staEapState->eap_frag_threshold;
	char *total_bytes_to_recv;


	/* check the code  */
	if (wpsEapHdr->code != EAP_CODE_REQUEST) {
		/* oops, probably a failure  */
		if (wpsEapHdr->code == EAP_CODE_FAILURE) {
			if (staEapState->retcode == WPS_SUCCESS) {
				TUTRACE((TUTRACE_ERR, "Reveing EAP-Failure Success\n"));
				return WPS_SUCCESS;
			}
			else
				return EAP_FAILURE;
		}
		else {
			/*
			 * CSP 308396, Buffalo router WRZ-AMPG300NH
			 * interoperability problem.  Ignore unknow packets.
			 */
			return WPS_CONT;
		}
	}

	if (!staEapState) {
		return INTERNAL_ERROR;
	}

	/* We should check the consistency between eapol_len and eap_len */
	if (eapol_len < eap_len + EAP_MSG_OFFSET) {
		TUTRACE((TUTRACE_INFO, "Not a valid packet (EAPOL len %d but EAP len %d), "
			"ingore it\n", eapol_len, eap_len));
		return WPS_CONT;
	}

	/* now check that this is not a re-transmission from ap */
	status = wps_eap_ap_retrans(eapol_msg, eapol_len);
	if (status == WPS_SEND_RET_MSG_CONT || status == REG_FAILURE)
		return status;

	/* update receive message id, only for Failure / Identity Req / WPS Start */
	wps_recv_msg_id_upd((char *)wpsEapHdr);

	/*
	 * For Fragmentation: Test if we are waiting for a WPS_FRAG_ACK
	 * If so, only accept that code and send the next fragment directly
	 * with wps_eap_send_next_frag().  Return with no error.
	 */
	if (IS_EAP_TYPE_WPS(wpsEapHdr) && FRAGMENTING(staEapState)) {
		/* Only accept WPS_FRAG_ACK and send next fragmentation */
		if (wpsEapHdr->opcode == WPS_FRAG_ACK) {
			TUTRACE((TUTRACE_INFO, "Receive a WPS fragment ACK packet, send next\n"));

			/* Is first WPS_FRAG_ACK? */
			if (staEapState->next_frag_to_send ==
			    &staEapState->msg_to_send[EAP_MSG_OFFSET]) {
				wps_data_sent_bytes -= EAP_WPS_LF_OFFSET;
				advance_bytes += sizeof(WpsEapHdr);
			}

			/* Advance to next fragment point */
			staEapState->next_frag_to_send += advance_bytes;
			staEapState->frag_size -= wps_data_sent_bytes;

			/* Get msg_to_send WpsEapHdr */
			eapol = (EAPOL_HDR *)staEapState->msg_to_send;
			wpsEapHdr = (WpsEapHdr *)(eapol + 1);

			/* Send next fragmented packet */
			return wps_eap_next_frag(wpsEapHdr);
		}
		/* Bypass non WPS_MSG */
		else if (wpsEapHdr->opcode == WPS_MSG) {
			if (staEapState->frag_size < staEapState->eap_frag_threshold) {
				/* All fragmentations sent completed, reset next_frag_to_send */
				staEapState->next_frag_to_send = staEapState->msg_to_send;
				staEapState->frag_size = 0;
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
	 * Make sure to detect re-transmissions (same ID).
	 * When receiving a packet without the "More Fragment" flags,
	 * continue, otherwise send a WPS_FRAG_ACK with wps_eap_send_frag_ack() and return;
	 */
	total_bytes_to_recv = (char *)(wpsEapHdr + 1);
	if (IS_EAP_TYPE_WPS(wpsEapHdr) && REASSEMBLING(staEapState, wpsEapHdr)) {
		if (staEapState->total_bytes_to_recv == 0) {
			/* First fragmentation must have length information */
			if (!(wpsEapHdr->flags & EAP_WPS_FLAGS_LF))
				return EAP_FAILURE;

			/* Save total EAP message length */
			staEapState->total_bytes_to_recv = WpsNtohs((uint8*)total_bytes_to_recv);
			TUTRACE((TUTRACE_INFO, "Got EAP WPS total bytes to receive info %d\n",
				staEapState->total_bytes_to_recv));
		}

		ret = wps_eap_process_frag((char *)wpsEapHdr);
		if (ret != WPS_SUCCESS)
			return ret;

		/* fall through when all fragmentations are received. */
		staEapState->total_received = 0; /* Reset total_received */
		staEapState->total_bytes_to_recv = 0;

		/* Use reassembled buffer */
		wpsEapHdr = (WpsEapHdr *)staEapState->frags_received;
		eap_len = WpsNtohs((uint8 *)&wpsEapHdr->length);
	}

	/*
	 * invalidate the previous message to send.
	 * This allows for checking if we need to re-transmit and if the message
	 * to send is still valid.
	 */
	switch (staEapState->state) {
	case EAPOL_START_SENT :
	{
		if (wpsEapHdr->type != EAP_TYPE_IDENTITY)
			return WPS_CONT;
		return  wps_eap_identity_resp(staEapState->msg_to_send);
	}

	case EAP_IDENTITY_SENT :
	case PROCESSING_PROTOCOL:
		ret = wps_eap_enr_process_protocol((char *)wpsEapHdr, eap_len);
		if (ret == WPS_SEND_MSG_SUCCESS || ret == WPS_SUCCESS) {
			staEapState->retcode = WPS_SUCCESS;
			if (ret == WPS_SEND_MSG_SUCCESS)
				ret = WPS_SEND_MSG_CONT;
			else
				ret = WPS_CONT;
		}
		return ret;

	case REG_SUCCESSFUL:
		return REG_SUCCESS;
	}

	return WPS_CONT;
}

char
wps_get_msg_type(char *eapol_msg, int eapol_len)
{
	char *eap_msg;
	WpsEapHdr * wpsEapHdr = NULL;
	unsigned char *wps_msg;

	eap_msg = eapol_msg + EAP_MSG_OFFSET;
	wpsEapHdr = (WpsEapHdr *)eap_msg;
	wps_msg = (unsigned char *)(wpsEapHdr + 1);

	if (!staEapState)
		return 0;

	/* Failure */
	if (wpsEapHdr->code == EAP_CODE_FAILURE)
		return WPS_PRIVATE_ID_FAILURE;

	/* Identity */
	if (wpsEapHdr->type == EAP_TYPE_IDENTITY)
		return WPS_PRIVATE_ID_IDENTITY;

	/* WPS Start */
	if (wpsEapHdr->opcode == WPS_Start)
		return WPS_PRIVATE_ID_WPS_START;

	/* WPS_FRAG_ACK */
	if (wpsEapHdr->opcode == WPS_FRAG_ACK)
		return WPS_PRIVATE_ID_FRAG_ACK;

	/* Advance LF_offset */
	if (wpsEapHdr->flags & EAP_WPS_FLAGS_LF)
		wps_msg += EAP_WPS_LF_OFFSET;

	/* Fragmented packet */
	if ((REASSEMBLING(staEapState, wpsEapHdr) ||
	    IS_PREV_FRAG(staEapState, wpsEapHdr, (eapol_len - EAP_MSG_OFFSET))) &&
	    !(wpsEapHdr->flags & EAP_WPS_FLAGS_LF))
		return WPS_PRIVATE_ID_FRAG;

	return wps_msg[WPS_MSGTYPE_OFFSET];
}

/*
 * Check if we already have answered this request and our peer didn't receive it.
 * Also check that that the id matches the request. Sta dependent.
 */
static int
wps_eap_ap_retrans(char *eapol_msg, int eapol_len)
{
	char *eap_msg;
	WpsEapHdr *wpsEapHdr;
	/* normal status : we are waiting from a new message from reg */
	int status = WPS_CONT;
	char wps_msg_type;

	eap_msg = eapol_msg + EAP_MSG_OFFSET;
	wpsEapHdr = (WpsEapHdr *)eap_msg;

	/* Fragmenting /Reassembling checking */
	if (IS_EAP_TYPE_WPS(wpsEapHdr) &&
	    (FRAGMENTING(staEapState) || REASSEMBLING(staEapState, wpsEapHdr))) {
		if (wpsEapHdr->id == staEapState->eap_id)
			return WPS_SEND_RET_MSG_CONT;

		/* Update eap_id from server */
		staEapState->eap_id = wpsEapHdr->id;

		/* Save last received frag */
		if (REASSEMBLING(staEapState, wpsEapHdr)) {
			staEapState->prev_frag_len = eapol_len - EAP_MSG_OFFSET;
			if (staEapState->prev_frag_len > sizeof(staEapState->prev_frag)) {
				TUTRACE((TUTRACE_ERR, "Invalid fragment packet len %d\n",
					staEapState->prev_frag_len));
				staEapState->prev_frag_len = 0;
			}
			else
				memcpy(staEapState->prev_frag, eap_msg,
					staEapState->prev_frag_len);
		}
		return WPS_CONT;
	}

	/* get wps messge type id */
	wps_msg_type = wps_get_msg_type(eapol_msg, eapol_len);

	/*
	 * Some AP use same identifier for different eap message.
	 * thus, we should check not only identifier, but also message type.
	 * if both identifier and message type is same, we can handle it as
	 * re-sent message from AP.
	 */
	/* The IS_PREV_FRAG(staEapState, wpsEapHdr, (eapol_len - EAP_MSG_OFFSET))  is
	 * used for detecting the latest fragment frame re-transmit from AP.  At this moment,
	 * STA is not in REASSEMBLING anymore because STA has received latest fragment frame
	 * from AP.
	*/
	if (wpsEapHdr->id == staEapState->eap_id &&
	    (IS_PREV_FRAG(staEapState, wpsEapHdr, (eapol_len - EAP_MSG_OFFSET)) ||
	    wpsEapHdr->opcode == WPS_FRAG_ACK ||
	    wps_msg_type == staEapState->recv_msg_id)) {
		switch (staEapState->state) {
		case EAPOL_START_SENT :
			/* we haven't received the first valid id yet, process normally */
			return status;

		case EAP_IDENTITY_SENT :
			/*
			 * some AP reused request id in request identity and M1,
			 * don't check it, let up layer to check it.
			 */
			return status;

		case PROCESSING_PROTOCOL:
			/* wps type ? */
			if (wpsEapHdr->type != EAP_TYPE_WPS)
				return REG_FAILURE;

			/*
			 * Reduce sizeof(EAPOL_HDR) + sizeof(WpsEapHdr), because caller
			 * use wps_eap_send_msg() to send ths re-transmission packet
			 */
			staEapState->msg_len -= WPS_DATA_OFFSET;
			break;
		}
		/*
		 * valid re-transmission, do not process, just indicate that
		 * the old response is waiting to be sent.
		 */
		status = WPS_SEND_RET_MSG_CONT;
	}
	else if ((wpsEapHdr->id != staEapState->eap_id) &&
		(wps_msg_type == staEapState->recv_msg_id) &&
		(wps_msg_type == WPS_PRIVATE_ID_IDENTITY)) {
		/*
		 * Some AP change identifier when it retransmit Identity request message.
		 * In that case, we change staEapState->state to EAPOL_START_SENT
		 * from EAP_IDENTITY_SENT because we should re-create Identity response
		 * message with new identifier
		 */
		staEapState->state = EAPOL_START_SENT;
		staEapState->eap_id = wpsEapHdr->id;
	}
	else {
		staEapState->eap_id = wpsEapHdr->id;
	}

	return status;
}

/*
* returns the length of current message to be sent or -1 if none.
* move the counter.
* Independent from enr/reg or ap/sta
*/
int
wps_get_msg_to_send(char **data, uint32 time)
{
	if (!staEapState) {
		*data = NULL;
		return -1;
	}

	if (staEapState->send_count == 0)
		staEapState->time_sent = time;

	/* we should check that counter when checking timers. */
	staEapState->send_count ++;

	if (staEapState->state >= PROCESSING_PROTOCOL)
		/* shift EAP_WPS_LF_OFFSET */
		*data = &staEapState->msg_to_send[WPS_DATA_OFFSET + EAP_WPS_LF_OFFSET];
	else
		*data = staEapState->msg_to_send;

	/* remember last sent data and len */
	staEapState->last_sent = *data;
	staEapState->last_sent_len = staEapState->msg_len;

	return staEapState->msg_len;
}

int
wps_get_retrans_msg_to_send(char **data, uint32 time, char *mtype)
{
	int type_offset = (WPS_DATA_OFFSET + WPS_MSGTYPE_OFFSET);
	WpsEapHdr *wpsEapHdr;
	int len;
	if (!staEapState) {
		*data = NULL;
		return -1;
	}

	if (staEapState->send_count == 0)
		staEapState->time_sent = time;

	/* we should check that counter when checking timers. */
	staEapState->send_count ++;

	/* PHIL :
	 * this is broken when the application is implementing its own eapol_send_packet.
	 * we should treat this the same as any other case and return SEND_MSG_CONT
	 * The application will call "get_msg_to_send" which will return frag ack.
	 * be sent.
	 */
	if (staEapState->last_sent) {
		*data = staEapState->last_sent;
		len = staEapState->last_sent_len;
	}
	else {
		*data = staEapState->msg_to_send;
		len = staEapState->msg_len;

		/* remember last sent data and len */
		staEapState->last_sent = *data;
		staEapState->last_sent_len = len;
	}

	*mtype = 0;

	if (staEapState->state >= PROCESSING_PROTOCOL) {
		wpsEapHdr = (WpsEapHdr *)(staEapState->last_sent + EAP_MSG_OFFSET);
		if (wpsEapHdr->flags & EAP_WPS_FLAGS_LF)
			type_offset += EAP_WPS_LF_OFFSET;

		*mtype = staEapState->last_sent[type_offset];
	}

	return len;
}

int
wps_get_frag_msg_to_send(char **data, uint32 time)
{
	if (!staEapState) {
		*data = NULL;
		return -1;
	}

	if (staEapState->send_count == 0)
		staEapState->time_sent = time;

	/* we should check that counter when checking timers. */
	staEapState->send_count ++;

	*data = staEapState->last_sent;
	return staEapState->last_sent_len;
}

char *
wps_get_msg_string(int mtype)
{
	if (mtype < WPS_ID_BEACON || mtype > (ARRAYSIZE(WPS_MSG_STRS)-1))
		return WPS_MSG_STRS[0];
	else
		return WPS_MSG_STRS[mtype];
}

int
wps_eap_check_timer(uint32 time)
{
	if (!staEapState) {
		return WPS_CONT;
	}

	if (5 < (time - staEapState->time_sent)) {
		/* We have WPS_SUCCESS and wait for EAP-Failure */
		if (staEapState->retcode == WPS_SUCCESS) {
			if (staEapState->send_count >= 1)
				return WPS_SUCCESS;

			staEapState->send_count ++;
			staEapState->time_sent = time;
			return WPS_CONT;
		}

		if (staEapState->send_count >= 3) {
			return EAP_TIMEOUT;
		}

		/* update current time */
		staEapState->time_sent = time;

		/* In M2D time wait */
		if (staEapState->recv_msg_id == WPS_ID_MESSAGE_M2D) {
			staEapState->send_count ++;
			return WPS_CONT;
		}

		/* We need WPS_SEND_RET_MSG_CONT to re-send M1 */
		return WPS_SEND_RET_MSG_CONT; /* re-transmit  */
	}

	return WPS_CONT;
}

/*
* this is enrollee/registrar dependent.
* Could be made independent by using function pointer for "Step" instead
* of the state machine.
*/
static int
wps_eap_enr_process_protocol(char *eap_msg, int eap_len)
{
	/* ideally, it would be nice to pass the buffer to output to the process function ... */
	/* WpsEapHdr * wpsEapHdr_out = (WpsEapHdr *)staEapState->msg_to_send; */
	WpsEapHdr * wpsEapHdr_in = (WpsEapHdr *)eap_msg;
	int retVal;
	/* point after the eap header */
	unsigned char *msg_in = (unsigned char *)(wpsEapHdr_in + 1);

	/* record the in message id, ingore WPS_Start */
	if (wpsEapHdr_in->opcode != WPS_Start)
		staEapState->recv_msg_id = msg_in[WPS_MSGTYPE_OFFSET];

	/* change to PROCESSING_PROTOCOL state */
	staEapState->state = PROCESSING_PROTOCOL;

	/* reset msg_len */
	staEapState->msg_len = sizeof(staEapState->msg_to_send) - WPS_DATA_OFFSET
		- EAP_WPS_LF_OFFSET;
	if (eap_len >= 14) {
		if (sta_e_mode == SCMODE_STA_REGISTRAR)
			retVal = reg_sm_step(staEapState->sm_hdle, eap_len - sizeof(WpsEapHdr),
				msg_in, (uint8*)&staEapState->msg_to_send[WPS_DATA_OFFSET + 2],
				(uint32*)&staEapState->msg_len);
		else
			retVal = enr_sm_step(staEapState->sm_hdle, eap_len - sizeof(WpsEapHdr),
				msg_in, (uint8*)&staEapState->msg_to_send[WPS_DATA_OFFSET + 2],
				(uint32*)&staEapState->msg_len);

		return retVal;
	}

	return WPS_CONT;
}

/* Construct an EAPOL Start. Sta dependent */
static int
wps_eap_eapol_start(char *pkt)
{
	EAPOL_HDR *eapol = (EAPOL_HDR *)pkt;
	eapol->version = 1;
	eapol->type = 1;
	eapol->len = 0;
	staEapState->state = EAPOL_START_SENT;
	staEapState->send_count = 0;
	staEapState->msg_len = sizeof(EAPOL_HDR);

	return WPS_SEND_MSG_CONT;
}

/* EAP Response Identity. Sta dependent  */
static int
wps_eap_identity_resp(char *pkt)
{
	int size;
	EAPOL_HDR *eapol;
	EAP_HDR *eapHdr;
	char *identity = WpsEapEnrIdentity;

	if (sta_e_mode == SCMODE_STA_REGISTRAR)
		identity = WpsEapRegIdentity;

	size = (int)(strlen(identity) + sizeof(EAP_HDR));
	eapol = (EAPOL_HDR *)pkt;
	eapHdr = (EAP_HDR *)(eapol +1);
	eapol->version = 1;
	eapol->type = 0;
	eapol->len = WpsHtons(size);
	eapHdr->code = EAP_CODE_RESPONSE;
	eapHdr->id = staEapState->eap_id;
	eapHdr->length = eapol->len;
	eapHdr->type = EAP_TYPE_IDENTITY;
	memcpy((char *)eapHdr + 5, identity, strlen(identity));
	staEapState->send_count = 0;
	staEapState->msg_len = sizeof(EAPOL_HDR) + size;
	staEapState->state = EAP_IDENTITY_SENT;

	return WPS_SEND_MSG_IDRESP;
}

static int
wps_eap_frag_ack()
{
	EAPOL_HDR *eapol = (EAPOL_HDR *)staEapState->msg_to_send;
	WpsEapHdr *wpsEapHdr = (WpsEapHdr *)(eapol +1);

	wpsEapHdr = (WpsEapHdr *)(eapol +1);
	memset(wpsEapHdr, 0, sizeof(WpsEapHdr));

	wpsEapHdr->code = EAP_CODE_RESPONSE;
	wpsEapHdr->type = EAP_TYPE_WPS;
	wpsEapHdr->id = staEapState->eap_id;
	wpsEapHdr->length = WpsHtons(sizeof(WpsEapHdr));
	wpsEapHdr->vendorId[0] = WPS_VENDORID1;
	wpsEapHdr->vendorId[1] = WPS_VENDORID2;
	wpsEapHdr->vendorId[2] = WPS_VENDORID3;
	wpsEapHdr->vendorType = WpsHtonl(WPS_VENDORTYPE);
	wpsEapHdr->opcode = WPS_FRAG_ACK;
	wpsEapHdr->flags = 0;

	/* Construct EAPOL header */
	eapol->version = 1;
	eapol->type = 0;
	eapol->len = wpsEapHdr->length;

	/* mark as not been retreived yet */
	staEapState->send_count = 0;
	staEapState->msg_len = sizeof(EAPOL_HDR) + WpsHtons(wpsEapHdr->length);

	/* store in last_sent */
	staEapState->last_sent = (char *)eapol;
	staEapState->last_sent_len = staEapState->msg_len;

	return WPS_SEND_FRAG_ACK_CONT;
}

/*
 * called after enrollee processing
 * The message will then be available using
 * get_message_from_enrollee().
 * Independent (must be called from sta/ap dependent code).
 */
int
wps_eap_create_pkt(char *dataBuffer, int dataLen, uint8 eapCode)
{
	int ret = WPS_SUCCESS;
	int LF_bytes = EAP_WPS_LF_OFFSET;
	EAPOL_HDR *eapol = (EAPOL_HDR *)staEapState->msg_to_send;
	WpsEapHdr * wpsEapHdr = NULL;

	/* Shift EAP_WPS_LF_OFFSET */
	if (dataLen < staEapState->eap_frag_threshold) {
		eapol = (EAPOL_HDR *)&staEapState->msg_to_send[EAP_WPS_LF_OFFSET];
		LF_bytes = 0;
	}

	/* Construct EAP header */
	wpsEapHdr = (WpsEapHdr *)(eapol +1);

	wpsEapHdr->code = eapCode;
	wpsEapHdr->type = EAP_TYPE_WPS;
	wpsEapHdr->id = staEapState->eap_id;
	wpsEapHdr->vendorId[0] = WPS_VENDORID1;
	wpsEapHdr->vendorId[1] = WPS_VENDORID2;
	wpsEapHdr->vendorId[2] = WPS_VENDORID3;
	wpsEapHdr->vendorType = WpsHtonl(WPS_VENDORTYPE);
	wpsEapHdr->length = WpsHtons((uint16)(dataLen + sizeof(WpsEapHdr)));

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
			staEapState->state = REG_FAILED;
		}
		else if (dataBuffer[WPS_MSGTYPE_OFFSET] == WPS_ID_MESSAGE_DONE) {
			wpsEapHdr->opcode = WPS_Done;
			staEapState->state = REG_SUCCESSFUL;
		}
		else {
			TUTRACE((TUTRACE_ERR, "Unknown Message Type code %d; "
				"Not a valid msg\n", dataBuffer[WPS_MSGTYPE_OFFSET]));
			return TREAP_ERR_SENDRECV;
		}

		/* record the out message id, we don't save fragmented sent_msg_id */
		staEapState->sent_msg_id = dataBuffer[WPS_MSGTYPE_OFFSET];

		/* Default flags are set to zero, let wps_eap_send_eap to handle it */
		wpsEapHdr->flags = 0;

		/*
		 * in case we passed this buffer to the protocol,
		 * do not copy :-)
		 */
		if (dataBuffer - LF_bytes != (char *)(wpsEapHdr + 1))
			memcpy((char *)wpsEapHdr + sizeof(WpsEapHdr) + LF_bytes,
				dataBuffer, dataLen);

		staEapState->msg_len = dataLen + WPS_DATA_OFFSET;

		/* mark as not been retreived yet */
		staEapState->send_count = 0;
	}
	else {
		/* don't know if this makes sens ... */
		ret = WPS_CONT;
	}

	return ret;
}

int
wps_get_eapol_msg_to_send(char **data, uint32 time)
{
	int wps_len = 0;
	char *wps_data = NULL;

	uint32 eapol_len;
	EAPOL_HDR *eapol = (EAPOL_HDR *)staEapState->msg_to_send;
	WpsEapHdr * wpsEapHdr;

	/* Get wps data first */
	wps_len = wps_get_msg_to_send(&wps_data, time);
	if (wps_data == NULL) {
		*data = NULL;
		return -1;
	}

	/* Create eap packet */
	if (wps_eap_create_pkt(wps_data, wps_len, EAP_CODE_RESPONSE) != WPS_SUCCESS) {
		*data = NULL;
		return -1;
	}

	/* Shift EAP_WPS_LF_OFFSET */
	if (wps_len < staEapState->eap_frag_threshold)
		eapol = (EAPOL_HDR *)&staEapState->msg_to_send[EAP_WPS_LF_OFFSET];

	wpsEapHdr = (WpsEapHdr *)(eapol + 1);

	/* Create eapol packet */
	eapol_len = wps_eapol_create_pkt((char *)wpsEapHdr,
		WpsNtohs((uint8*)&wpsEapHdr->length), data);

	return eapol_len;
}

int
wps_get_sent_msg_id()
{
	return ((int)staEapState->sent_msg_id);
}

int
wps_get_recv_msg_id()
{
	/*
	 * Because we don't want to modify the caller source code,
	 * return 0 (same as before) in case of Identity/WPS Start/Failure.
	 */
	if (staEapState->recv_msg_id == WPS_PRIVATE_ID_IDENTITY ||
	    staEapState->recv_msg_id == WPS_PRIVATE_ID_WPS_START ||
	    staEapState->recv_msg_id == WPS_PRIVATE_ID_FAILURE)
	    return 0;

	return ((int)staEapState->recv_msg_id);
}

int
wps_get_eap_state()
{
	return ((int)staEapState->state);
}

#ifdef WFA_WPS_20_TESTBED
int
sta_eap_sm_set_eap_frag_threshold(int eap_frag_threshold)
{
	if (eap_frag_threshold >= 100 && eap_frag_threshold <= EAP_WPS_FRAG_MAX)
		staEapState_eap_frag_threshold = eap_frag_threshold;
	else
		return -1;

	return 0;
}
#endif /* WFA_WPS_20_TESTBED */
