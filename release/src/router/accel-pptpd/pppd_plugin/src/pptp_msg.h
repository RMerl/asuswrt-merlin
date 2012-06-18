/*  pptp.h:  packet structures and magic constants for the PPTP protocol 
 *           C. Scott Ananian <cananian@alumni.princeton.edu>            
 *
 * $Id: pptp_msg.h,v 1.3 2003/02/15 10:37:21 quozl Exp $
 */

#ifndef INC_PPTP_H
#define INC_PPTP_H

/* Grab definitions of int16, int32, etc. */
#include <sys/types.h>
/* define "portable" htons, etc. */
#define hton8(x)  (x)
#define ntoh8(x)  (x)
#define hton16(x) htons(x)
#define ntoh16(x) ntohs(x)
#define hton32(x) htonl(x)
#define ntoh32(x) ntohl(x)

/* PPTP magic numbers: ----------------------------------------- */

#define PPTP_MAGIC 0x1A2B3C4D /* Magic cookie for PPTP datagrams */
#define PPTP_PORT  1723       /* PPTP TCP port number            */
#define PPTP_PROTO 47         /* PPTP IP protocol number         */

/* Control Connection Message Types: --------------------------- */

#define PPTP_MESSAGE_CONTROL		1
#define PPTP_MESSAGE_MANAGE		2

/* Control Message Types: -------------------------------------- */

/* (Control Connection Management) */
#define PPTP_START_CTRL_CONN_RQST	1
#define PPTP_START_CTRL_CONN_RPLY	2
#define PPTP_STOP_CTRL_CONN_RQST	3
#define PPTP_STOP_CTRL_CONN_RPLY	4
#define PPTP_ECHO_RQST			5
#define PPTP_ECHO_RPLY			6

/* (Call Management) */
#define PPTP_OUT_CALL_RQST		7
#define PPTP_OUT_CALL_RPLY		8
#define PPTP_IN_CALL_RQST		9
#define PPTP_IN_CALL_RPLY		10
#define PPTP_IN_CALL_CONNECT		11
#define PPTP_CALL_CLEAR_RQST		12
#define PPTP_CALL_CLEAR_NTFY		13

/* (Error Reporting) */
#define PPTP_WAN_ERR_NTFY		14

/* (PPP Session Control) */
#define PPTP_SET_LINK_INFO		15

/* PPTP version information: --------------------------------------*/
#define PPTP_VERSION_STRING	"1.00"
#define PPTP_VERSION		0x100
#define PPTP_FIRMWARE_STRING	"0.01"
#define PPTP_FIRMWARE_VERSION	0x001

/* PPTP capabilities: ---------------------------------------------*/

/* (Framing capabilities for msg sender) */
#define PPTP_FRAME_ASYNC	1
#define PPTP_FRAME_SYNC		2
#define PPTP_FRAME_ANY          3

/* (Bearer capabilities for msg sender) */
#define PPTP_BEARER_ANALOG	1
#define PPTP_BEARER_DIGITAL 	2
#define PPTP_BEARER_ANY		3

#define PPTP_RESULT_GENERAL_ERROR 2

/* (Reasons to close a connection) */
#define PPTP_STOP_NONE		  1 /* no good reason                        */
#define PPTP_STOP_PROTOCOL	  2 /* can't support peer's protocol version */
#define PPTP_STOP_LOCAL_SHUTDOWN  3 /* requester is being shut down          */

/* PPTP datagram structures (all data in network byte order): ----------*/

struct pptp_header {
  u_int16_t length;	  /* message length in octets, including header */
  u_int16_t pptp_type;	  /* PPTP message type. 1 for control message.  */
  u_int32_t magic;	  /* this should be PPTP_MAGIC.                 */
  u_int16_t ctrl_type;	  /* Control message type (0-15)                */
  u_int16_t reserved0;	  /* reserved.  MUST BE ZERO.                   */
};

struct pptp_start_ctrl_conn { /* for control message types 1 and 2 */
  struct pptp_header header;

  u_int16_t version;      /* PPTP protocol version.  = PPTP_VERSION     */
  u_int8_t  result_code;  /* these two fields should be zero on rqst msg*/
  u_int8_t  error_code;   /* 0 unless result_code==2 (General Error)    */
  u_int32_t framing_cap;  /* Framing capabilities                       */
  u_int32_t bearer_cap;   /* Bearer Capabilities                        */
  u_int16_t max_channels; /* Maximum Channels (=0 for PNS, PAC ignores) */
  u_int16_t firmware_rev; /* Firmware or Software Revision              */
  u_int8_t  hostname[64]; /* Host Name (64 octets, zero terminated)     */
  u_int8_t  vendor[64];   /* Vendor string (64 octets, zero term.)      */
  /* MS says that end of hostname/vendor fields should be filled with   */
  /* octets of value 0, but Win95 PPTP driver doesn't do this.          */
};

struct pptp_stop_ctrl_conn { /* for control message types 3 and 4 */
  struct pptp_header header;

  u_int8_t reason_result; /* reason for rqst, result for rply          */
  u_int8_t error_code;	  /* MUST be 0, unless rply result==2 (general err)*/
  u_int16_t reserved1;    /* MUST be 0                                */
};

struct pptp_echo_rqst { /* for control message type 5 */
  struct pptp_header header;
  u_int32_t identifier;   /* arbitrary value set by sender which is used */
                          /* to match up reply and request               */
};

struct pptp_echo_rply { /* for control message type 6 */
  struct pptp_header header;
  u_int32_t identifier;	  /* should correspond to id of rqst             */
  u_int8_t result_code;
  u_int8_t error_code;    /* =0, unless result_code==2 (general error)   */
  u_int16_t reserved1;    /* MUST BE ZERO                                */
};

struct pptp_out_call_rqst { /* for control message type 7 */
  struct pptp_header header;
  u_int16_t call_id;	  /* Call ID (unique id used to multiplex data)  */
  u_int16_t call_sernum;  /* Call Serial Number (used for logging)       */
  u_int32_t bps_min;      /* Minimum BPS (lowest acceptable line speed)  */
  u_int32_t bps_max;	  /* Maximum BPS (highest acceptable line speed) */
  u_int32_t bearer;	  /* Bearer type                                 */
  u_int32_t framing;      /* Framing type                                */
  u_int16_t recv_size;	  /* Recv. Window Size (no. of buffered packets) */
  u_int16_t delay;	  /* Packet Processing Delay (in 1/10 sec)       */
  u_int16_t phone_len;	  /* Phone Number Length (num. of valid digits)  */
  u_int16_t reserved1;    /* MUST BE ZERO				 */
  u_int8_t  phone_num[64]; /* Phone Number (64 octets, null term.)       */
  u_int8_t subaddress[64]; /* Subaddress (64 octets, null term.)         */
};

struct pptp_out_call_rply { /* for control message type 8 */
  struct pptp_header header;
  u_int16_t call_id;      /* Call ID (used to multiplex data over tunnel)*/
  u_int16_t call_id_peer; /* Peer's Call ID (call_id of pptp_out_call_rqst)*/
  u_int8_t  result_code;  /* Result Code (1 is no errors)                */
  u_int8_t  error_code;   /* Error Code (=0 unless result_code==2)       */
  u_int16_t cause_code;   /* Cause Code (addt'l failure information)     */
  u_int32_t speed;        /* Connect Speed (in BPS)                      */
  u_int16_t recv_size;    /* Recv. Window Size (no. of buffered packets) */
  u_int16_t delay;	  /* Packet Processing Delay (in 1/10 sec)       */
  u_int32_t channel;      /* Physical Channel ID (for logging)           */
};

struct pptp_in_call_rqst { /* for control message type 9 */
  struct pptp_header header;
  u_int16_t call_id;	  /* Call ID (unique id used to multiplex data)  */
  u_int16_t call_sernum;  /* Call Serial Number (used for logging)       */
  u_int32_t bearer;	  /* Bearer type                                 */
  u_int32_t channel;      /* Physical Channel ID (for logging)           */
  u_int16_t dialed_len;   /* Dialed Number Length (# of valid digits)    */
  u_int16_t dialing_len;  /* Dialing Number Length (# of valid digits)   */
  u_int8_t dialed_num[64]; /* Dialed Number (64 octets, zero term.)      */
  u_int8_t dialing_num[64]; /* Dialing Number (64 octets, zero term.)    */
  u_int8_t subaddress[64];  /* Subaddress (64 octets, zero term.)        */
};

struct pptp_in_call_rply { /* for control message type 10 */
  struct pptp_header header;
  u_int16_t call_id;      /* Call ID (used to multiplex data over tunnel)*/
  u_int16_t call_id_peer; /* Peer's Call ID (call_id of pptp_out_call_rqst)*/
  u_int8_t  result_code;  /* Result Code (1 is no errors)                */
  u_int8_t  error_code;   /* Error Code (=0 unless result_code==2)       */
  u_int16_t recv_size;    /* Recv. Window Size (no. of buffered packets) */
  u_int16_t delay;	  /* Packet Processing Delay (in 1/10 sec)       */
  u_int16_t reserved1;    /* MUST BE ZERO                                */
};

struct pptp_in_call_connect { /* for control message type 11 */
  struct pptp_header header;
  u_int16_t call_id_peer; /* Peer's Call ID (call_id of pptp_out_call_rqst)*/
  u_int16_t reserved1;    /* MUST BE ZERO                                */
  u_int32_t speed;        /* Connect Speed (in BPS)                      */
  u_int16_t recv_size;    /* Recv. Window Size (no. of buffered packets) */
  u_int16_t delay;	  /* Packet Processing Delay (in 1/10 sec)       */
  u_int32_t framing;      /* Framing type                                */
};

struct pptp_call_clear_rqst { /* for control message type 12 */
  struct pptp_header header;
  u_int16_t call_id;      /* Call ID (used to multiplex data over tunnel)*/
  u_int16_t reserved1;    /* MUST BE ZERO                                */
};

struct pptp_call_clear_ntfy { /* for control message type 13 */
  struct pptp_header header;
  u_int16_t call_id;      /* Call ID (used to multiplex data over tunnel)*/
  u_int8_t  result_code;  /* Result Code                                 */
  u_int8_t  error_code;   /* Error Code (=0 unless result_code==2)       */
  u_int16_t cause_code;   /* Cause Code (for ISDN, is Q.931 cause code)  */
  u_int16_t reserved1;    /* MUST BE ZERO                                */
  u_int8_t call_stats[128]; /* Call Statistics: 128 octets, ascii, 0-term */
};

struct pptp_wan_err_ntfy {    /* for control message type 14 */
  struct pptp_header header;
  u_int16_t call_id_peer; /* Peer's Call ID (call_id of pptp_out_call_rqst)*/
  u_int16_t reserved1;    /* MUST BE ZERO                                */
  u_int32_t crc_errors;   /* CRC errors 				 */
  u_int32_t frame_errors; /* Framing errors 				 */
  u_int32_t hard_errors;  /* Hardware overruns 				 */
  u_int32_t buff_errors;  /* Buffer overruns				 */
  u_int32_t time_errors;  /* Time-out errors				 */
  u_int32_t align_errors; /* Alignment errors				 */
};

struct pptp_set_link_info {   /* for control message type 15 */
  struct pptp_header header;
  u_int16_t call_id_peer; /* Peer's Call ID (call_id of pptp_out_call_rqst) */
  u_int16_t reserved1;    /* MUST BE ZERO                                   */
  u_int32_t send_accm;    /* Send ACCM (for PPP packets; default 0xFFFFFFFF)*/
  u_int32_t recv_accm;    /* Receive ACCM (for PPP pack.;default 0xFFFFFFFF)*/
};

/* helpful #defines: -------------------------------------------- */
#define pptp_isvalid_ctrl(header, type, length) \
 (!( ( ntoh16(((struct pptp_header *)header)->length)    < (length)  ) ||   \
     ( ntoh16(((struct pptp_header *)header)->pptp_type) !=(type)    ) ||   \
     ( ntoh32(((struct pptp_header *)header)->magic)     !=PPTP_MAGIC) ||   \
     ( ntoh16(((struct pptp_header *)header)->ctrl_type) > PPTP_SET_LINK_INFO) || \
     ( ntoh16(((struct pptp_header *)header)->reserved0) !=0         ) ))

#define PPTP_HEADER_CTRL(type)  \
{ hton16(PPTP_CTRL_SIZE(type)), \
  hton16(PPTP_MESSAGE_CONTROL), \
  hton32(PPTP_MAGIC),           \
  hton16(type), 0 }             

#define PPTP_CTRL_SIZE(type) ( \
(type==PPTP_START_CTRL_CONN_RQST)?sizeof(struct pptp_start_ctrl_conn):	\
(type==PPTP_START_CTRL_CONN_RPLY)?sizeof(struct pptp_start_ctrl_conn):	\
(type==PPTP_STOP_CTRL_CONN_RQST )?sizeof(struct pptp_stop_ctrl_conn):	\
(type==PPTP_STOP_CTRL_CONN_RPLY )?sizeof(struct pptp_stop_ctrl_conn):	\
(type==PPTP_ECHO_RQST           )?sizeof(struct pptp_echo_rqst):	\
(type==PPTP_ECHO_RPLY           )?sizeof(struct pptp_echo_rply):	\
(type==PPTP_OUT_CALL_RQST       )?sizeof(struct pptp_out_call_rqst):	\
(type==PPTP_OUT_CALL_RPLY       )?sizeof(struct pptp_out_call_rply):	\
(type==PPTP_IN_CALL_RQST        )?sizeof(struct pptp_in_call_rqst):	\
(type==PPTP_IN_CALL_RPLY        )?sizeof(struct pptp_in_call_rply):	\
(type==PPTP_IN_CALL_CONNECT     )?sizeof(struct pptp_in_call_connect):	\
(type==PPTP_CALL_CLEAR_RQST     )?sizeof(struct pptp_call_clear_rqst):	\
(type==PPTP_CALL_CLEAR_NTFY     )?sizeof(struct pptp_call_clear_ntfy):	\
(type==PPTP_WAN_ERR_NTFY        )?sizeof(struct pptp_wan_err_ntfy):	\
(type==PPTP_SET_LINK_INFO       )?sizeof(struct pptp_set_link_info):	\
0)
#define max(a,b) (((a)>(b))?(a):(b))
#define PPTP_CTRL_SIZE_MAX (			\
max(sizeof(struct pptp_start_ctrl_conn),	\
max(sizeof(struct pptp_echo_rqst),		\
max(sizeof(struct pptp_echo_rply),		\
max(sizeof(struct pptp_out_call_rqst),		\
max(sizeof(struct pptp_out_call_rply),		\
max(sizeof(struct pptp_in_call_rqst),		\
max(sizeof(struct pptp_in_call_rply),		\
max(sizeof(struct pptp_in_call_connect),	\
max(sizeof(struct pptp_call_clear_rqst),	\
max(sizeof(struct pptp_call_clear_ntfy),	\
max(sizeof(struct pptp_wan_err_ntfy),		\
max(sizeof(struct pptp_set_link_info), 0)))))))))))))


/* gre header structure: -------------------------------------------- */

#define PPTP_GRE_PROTO  0x880B
#define PPTP_GRE_VER    0x1

#define PPTP_GRE_FLAG_C	0x80
#define PPTP_GRE_FLAG_R	0x40
#define PPTP_GRE_FLAG_K	0x20
#define PPTP_GRE_FLAG_S	0x10
#define PPTP_GRE_FLAG_A	0x80

#define PPTP_GRE_IS_C(f) ((f)&PPTP_GRE_FLAG_C)
#define PPTP_GRE_IS_R(f) ((f)&PPTP_GRE_FLAG_R)
#define PPTP_GRE_IS_K(f) ((f)&PPTP_GRE_FLAG_K)
#define PPTP_GRE_IS_S(f) ((f)&PPTP_GRE_FLAG_S)
#define PPTP_GRE_IS_A(f) ((f)&PPTP_GRE_FLAG_A)

struct pptp_gre_header {
  u_int8_t flags;		/* bitfield */
  u_int8_t ver;			/* should be PPTP_GRE_VER (enhanced GRE) */
  u_int16_t protocol;		/* should be PPTP_GRE_PROTO (ppp-encaps) */
  u_int16_t payload_len;	/* size of ppp payload, not inc. gre header */
  u_int16_t call_id;		/* peer's call_id for this session */
  u_int32_t seq;		/* sequence number.  Present if S==1 */
  u_int32_t ack;		/* seq number of highest packet recieved by */
  				/*  sender in this session */
};

#endif /* INC_PPTP_H */
