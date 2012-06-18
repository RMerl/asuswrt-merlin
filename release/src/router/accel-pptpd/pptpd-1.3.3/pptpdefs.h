/*
 * pptpdefs.h
 *
 * PPTP structs and defines
 *
 * $Id: pptpdefs.h,v 1.3 2005/08/02 09:51:18 quozl Exp $
 */

#ifndef _PPTPD_PPTPDEFS_H
#define _PPTPD_PPTPDEFS_H

/* define "portable" htons, etc, copied to make Ananian's gre stuff work. */
#define hton8(x)  (x)
#define ntoh8(x)  (x)
#define hton16(x) htons(x)
#define ntoh16(x) ntohs(x)
#define hton32(x) htonl(x)
#define ntoh32(x) ntohl(x)

#include <sys/types.h>

/* PPTP ctrl message port */
#define PPTP_PORT			1723

/* PPTP gre prototype */
#define PPTP_PROTO			47

/* PPTP version */
#define PPTP_VERSION			0x0100
#define	PPTP_FIRMWARE_VERSION		0x0001

/* Hostname and Vendor */
#define PPTP_HOSTNAME			"local"
#define PPTP_VENDOR			"linux"

#define MAX_HOSTNAME_SIZE		64
#define MAX_VENDOR_SIZE			64

/* Magic Cookie */
#define PPTP_MAGIC_COOKIE		0x1a2b3c4d

/* Message types */
#define PPTP_CTRL_MESSAGE		1

/* Maximum size of any PPTP control packet we will get */
#define PPTP_MAX_CTRL_PCKT_SIZE		220

/* Control Connection Management */
#define START_CTRL_CONN_RQST		1
#define START_CTRL_CONN_RPLY		2
#define STOP_CTRL_CONN_RQST		3
#define STOP_CTRL_CONN_RPLY		4
#define ECHO_RQST			5
#define ECHO_RPLY			6

/* Call Management */
#define OUT_CALL_RQST			7
#define OUT_CALL_RPLY			8
#define IN_CALL_RQST			9
#define IN_CALL_RPLY			10
#define IN_CALL_CONN			11
#define CALL_CLR_RQST			12
#define CALL_DISCONN_NTFY		13

/* Error Reporting */
#define WAN_ERR_NTFY			14

/* PPP Session Control */
#define SET_LINK_INFO			15

/* how long before a link is idle? (seconds) */
#define IDLE_WAIT			60

/* how long should we wait for an echo reply? (seconds) */
#define MAX_ECHO_WAIT			60

/* how long to wait for ppp to begin negotiation (seconds) */
#define PPP_WAIT			30

#define RESERVED			0x0000

/* Start Control Connection Reply */
#define ASYNCHRONOUS_FRAMING		0x00000001
#define SYNCHRONOUS_FRAMING		0x00000002
#define ANALOG_ACCESS			0x00000001
#define DIGITAL_ACCESS			0x00000002

/* Our properties - we don't actually have any physical serial i/f's and only want
 * one call per client!
 */
#define OUR_FRAMING			0x00000000
#define OUR_BEARER			0x00000000
#define MAX_CHANNELS			0x0001

/* Out Call Reply Defines */
#define PCKT_RECV_WINDOW_SIZE		0x0001
#define PCKT_PROCESS_DELAY		0x0000
#define CHANNEL_ID			0x00000000

/* ERROR CODES */
#define NO_ERROR			0x00

/* CALL_CLEAR RESULT CODES */
#define LOST_CARRIER			0x01
#define ADMIN_SHUTDOWN			0x03
#define CALL_CLEAR_REQUEST		0x04

/* RESULT CODES */
#define CONNECTED			0x01
#define DISCONNECTED			0x01
#define GENERAL_ERROR			0x02	/* also for ERROR CODES, CALL CLEAR */
#define NO_CARRIER			0x03
#define BUSY				0x04
#define NO_DIAL_TONE			0x05
#define TIME_OUT			0x06
#define DO_NOT_ACCEPT			0x07

/* CTRL CLOSE CODES */
#define GENERAL_STOP_CTRL		0x01
#define STOP_PROTOCOL			0x02
#define STOP_LOCAL_SHUTDOWN		0x03

/* PPTP CTRL structs */

struct pptp_header {
	u_int16_t length;		/* pptp message length incl header */
	u_int16_t pptp_type;		/* pptp message type */
	u_int32_t magic;		/* magic cookie */
	u_int16_t ctrl_type;		/* control message type */
	u_int16_t reserved0;		/* reserved */
};

struct pptp_start_ctrl_conn_rqst {
	struct pptp_header header;	/* pptp header */
	u_int16_t version;		/* pptp protocol version */
	u_int16_t reserved1;		/* reserved */
	u_int32_t framing_cap;		/* framing capabilities */
	u_int32_t bearer_cap;		/* bearer capabilities */
	u_int16_t max_channels;		/* maximum channels */
	u_int16_t firmware_rev;		/* firmware revision */
	u_int8_t hostname[MAX_HOSTNAME_SIZE];	/* hostname */
	u_int8_t vendor[MAX_VENDOR_SIZE];	/* vendor */
};

struct pptp_start_ctrl_conn_rply {
	struct pptp_header header;	/* pptp header */
	u_int16_t version;		/* pptp protocol version */
	u_int8_t result_code;		/* result code */
	u_int8_t error_code;		/* error code */
	u_int32_t framing_cap;		/* framing capabilities */
	u_int32_t bearer_cap;		/* bearer capabilities */
	u_int16_t max_channels;		/* maximum channels */
	u_int16_t firmware_rev;		/* firmware revision */
	u_int8_t hostname[MAX_HOSTNAME_SIZE];	/* hostname */
	u_int8_t vendor[MAX_VENDOR_SIZE];	/* vendor */
};

struct pptp_stop_ctrl_conn_rqst {
	struct pptp_header header;	/* header */
	u_int8_t reason;		/* reason for closing */
	u_int8_t reserved1;		/* reserved */
	u_int16_t reserved2;		/* reserved */
};

struct pptp_stop_ctrl_conn_rply {
	struct pptp_header header;	/* header */
	u_int8_t result_code;		/* result code */
	u_int8_t error_code;		/* error code */
	u_int16_t reserved1;		/* reserved */
};

struct pptp_echo_rqst {
	struct pptp_header header;	/* header */
	u_int32_t identifier;		/* value to match rply with rqst */
};

struct pptp_echo_rply {
	struct pptp_header header;	/* header */
	u_int32_t identifier;		/* identifier of echo rqst */
	u_int8_t result_code;		/* result code */
	u_int8_t error_code;		/* error code */
	u_int16_t reserved1;		/* reserved */
};

struct pptp_out_call_rqst {
	struct pptp_header header;	/* header */
	u_int16_t call_id;		/* unique identifier to PAC-PNS pair */
	u_int16_t call_serial;		/* session identifier */
	u_int32_t min_bps;		/* minimum line speed */
	u_int32_t max_bps;		/* maximum line speed */
	u_int32_t bearer_type;		/* bearer type */
	u_int32_t framing_type;		/* framing type */
	u_int16_t pckt_recv_size;	/* packet recv window size */
	u_int16_t pckt_delay;		/* packet processing delay */
	u_int16_t phone_len;		/* phone number length */
	u_int16_t reserved1;		/* reserved */
	u_int8_t phone_num[64];		/* phone number */
	u_int8_t subaddress[64];	/* additional dialing info */
};

struct pptp_out_call_rply {
	struct pptp_header header;	/* header */
	u_int16_t call_id;		/* unique identifier to PAC-PNS pair */
	u_int16_t call_id_peer;		/* set to call_id of the call rqst */
	u_int8_t result_code;		/* result code */
	u_int8_t error_code;		/* error code */
	u_int16_t cause_code;		/* additional failure information */
	u_int32_t speed;		/* actual connection speed */
	u_int16_t pckt_recv_size;	/* packet recv window size */
	u_int16_t pckt_delay;		/* packet processing delay */
	u_int32_t channel_id;		/* physical channel ID */
};

struct pptp_in_call_rqst {
	struct pptp_header header;	/* header */
	u_int16_t call_id;		/* unique identifier for tunnel */
	u_int16_t call_serial;		/* session identifier */
	u_int32_t bearer_type;		/* bearer capability */
	u_int32_t channel_id;		/* channel ID */
	u_int16_t dialed_len;		/* dialed length */
	u_int16_t dialing_len;		/* dialing length */
	u_int8_t dialed_num[64];	/* number that was dialed by the caller */
	u_int8_t dialing_num[64];	/* the number from which the call was placed */
	u_int8_t subaddress[64];	/* additional dialing information */
};

struct pptp_in_call_rply {
	struct pptp_header header;	/* header */
	u_int16_t call_id;		/* unique identifier for the tunnel */
	u_int16_t peers_call_id;	/* set to rcvd call ID */
	u_int8_t result_code;		/* result code */
	u_int8_t error_code;		/* error code */
	u_int16_t pckt_recv_size;	/* packet recv window size */
	u_int16_t pckt_delay;		/* packet transmit delay */
	u_int16_t reserved1;		/* reserved */
};

struct pptp_in_call_connect {
	struct pptp_header header;	/* header */
	u_int16_t peers_call_id;	/* set to rcvd call ID */
	u_int16_t reserved1;		/* reserved */
	u_int32_t speed;		/* connect speed */
	u_int16_t pckt_recv_size;	/* packet rcvd window size */
	u_int16_t pckt_delay;		/* packet transmit delay */
	u_int32_t framing_type;		/* framing type */
};

struct pptp_call_clr_rqst {
	struct pptp_header header;	/* header */
	u_int16_t call_id;		/* call ID assigned by the PNS */
	u_int16_t reserved1;		/* reserved */
};

struct pptp_call_disconn_ntfy {
	struct pptp_header header;	/* header */
	u_int16_t call_id;		/* call ID assigned by the PAC */
	u_int8_t result_code;		/* result code */
	u_int8_t error_code;		/* error code */
	u_int16_t cause_code;		/* additional disconnect info */
	u_int16_t reserved1;		/* reserved */
	u_int8_t call_stats[128];	/* vendor specific call stats */
};

struct pptp_wan_err_ntfy {
	struct pptp_header header;	/* header */
	u_int16_t peers_call_id;	/* call ID assigned by PNS */
	u_int16_t reserved1;		/* reserved */
	u_int32_t crc_errors;		/* # of PPP frames rcvd with CRC errors */
	u_int32_t framing_errors;	/* # of improperly framed PPP pckts */
	u_int32_t hardware_overruns;	/* # of receive buffer overruns */
	u_int32_t buff_overruns;	/* # of buffer overruns */
	u_int32_t timeout_errors;	/* # of timeouts */
	u_int32_t align_errors;		/* # of alignment errors */
};

struct pptp_set_link_info {
	struct pptp_header header;
	u_int16_t peers_call_id;	/* call ID assigned by PAC */
	u_int16_t reserved1;		/* reserved */
	u_int32_t send_accm;		/* send ACCM value the client should use */
	u_int32_t recv_accm;		/* recv ACCM value the client should use */
};

/* GRE and PPP structs */

/* Copied from C. S. Ananian */

#define HDLC_FLAG		0x7E
#define HDLC_ESCAPE		0x7D

#define PPTP_GRE_PROTO		0x880B
#define PPTP_GRE_VER		0x1

#define PPTP_GRE_FLAG_C		0x80
#define PPTP_GRE_FLAG_R		0x40
#define PPTP_GRE_FLAG_K		0x20
#define PPTP_GRE_FLAG_S		0x10
#define PPTP_GRE_FLAG_A		0x80

#define PPTP_GRE_IS_C(f)	((f)&PPTP_GRE_FLAG_C)
#define PPTP_GRE_IS_R(f)	((f)&PPTP_GRE_FLAG_R)
#define PPTP_GRE_IS_K(f)	((f)&PPTP_GRE_FLAG_K)
#define PPTP_GRE_IS_S(f)	((f)&PPTP_GRE_FLAG_S)
#define PPTP_GRE_IS_A(f)	((f)&PPTP_GRE_FLAG_A)

struct pptp_gre_header {
	u_int8_t flags;		/* bitfield */
	u_int8_t ver;		/* should be PPTP_GRE_VER (enhanced GRE) */
	u_int16_t protocol;	/* should be PPTP_GRE_PROTO (ppp-encaps) */
	u_int16_t payload_len;	/* size of ppp payload, not inc. gre header */
	u_int16_t call_id;	/* peer's call_id for this session */
	u_int32_t seq;		/* sequence number.  Present if S==1 */
	u_int32_t ack;		/* seq number of highest packet recieved by */
	/* sender in this session */
};

/* For our call ID pairs */
#define PNS_VALUE 0
#define PAC_VALUE 1

#define GET_VALUE(which, where) ((which ## _VALUE) ? ((where) & 0xffff) : ((where) >> 16))

#define NOTE_VALUE(which, where, what) ((which ## _VALUE) \
					  ? ((where) = ((where) & 0xffff0000) | (what)) \
					  : ((where) = ((where) & 0xffff) | ((what) << 16)))

#endif	/* !_PPTPD_PPTPDEFS_H */
