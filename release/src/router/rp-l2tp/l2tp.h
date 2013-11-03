/***********************************************************************
*
* lt2p.h
*
* Header file for L2TP definitions.
*
* Copyright (C) 2002 Roaring Penguin Software Inc.
*
* LIC: GPL
*
***********************************************************************/

#ifndef L2TP_H
#define L2TP_H

#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/route.h>

#include "hash.h"
#include "event.h"

#define ENABLE_DEBUG

#ifdef ENABLE_DEBUG
#define DBG(x) x
#else
#define DBG(x) (void) 0
#endif

#define MD5LEN 16		/* Length of MD5 hash */

/* Debug bitmasks */
#define DBG_TUNNEL             1 /* Tunnel-related events                 */
#define DBG_XMIT_RCV           2 /* Datagram transmission/reception       */
#define DBG_AUTH               4 /* Authentication                        */
#define DBG_SESSION            8 /* Session-related events                */
#define DBG_FLOW              16 /* Flow control code                     */
#define DBG_AVP               32 /* Hiding/showing of AVP's               */
#define DBG_SNOOP             64 /* Snooping in on LCP                    */

/* Maximum size of L2TP datagram we accept... kludge... */
#define MAX_PACKET_LEN   4096

#define MAX_SECRET_LEN   96
#define MAX_HOSTNAME     128
#define MAX_OPTS         64

#define MAX_RETRANSMISSIONS 5

#define EXTRA_HEADER_ROOM 32

/* Forward declarations */

/* an L2TP datagram */
typedef struct l2tp_dgram_t {
    uint16_t msg_type;		/* Message type */
    uint8_t bits;		/* Options bits */
    uint8_t version;		/* Version */
    uint16_t length;		/* Length (opt) */
    uint16_t tid;		/* Tunnel ID */
    uint16_t sid;		/* Session ID */
    uint16_t Ns;		/* Ns (opt) */
    uint16_t Nr;		/* Nr (opt) */
    uint16_t off_size;		/* Offset size (opt) */
    unsigned char data[MAX_PACKET_LEN];	/* Data */
    size_t last_random;	        /* Offset of last random vector AVP */
    size_t payload_len;		/* Payload len (not including L2TP header) */
    size_t cursor;		/* Cursor for adding/stripping AVP's */
    size_t alloc_len;		/* Length allocated for data */
    struct l2tp_dgram_t *next;	/* Link to next packet in xmit queue */
} l2tp_dgram;

/* An L2TP peer */
typedef struct l2tp_peer_t {
    hash_bucket hash;		/* all_peers hash (hashed by address) */
    struct sockaddr_in addr;	/* Peer's address */
    int mask_bits;		/* Peer's netmask in number of bits */
    char hostname[MAX_HOSTNAME]; /* My hostname as presented to this peer. */
    size_t hostname_len;	/* Length of my hostname */
    char peername[MAX_HOSTNAME]; /* Peer's hostname. */
    size_t peername_len;	/* Length of hostname */
    char secret[MAX_SECRET_LEN]; /* Secret for this peer */
    size_t secret_len;		/* Length of secret */
    struct l2tp_call_ops_t *lac_ops;	/* Call ops if we act as LAC */
    char *lac_options[MAX_OPTS+1]; /* Handler options if we act as LAC */
    int num_lac_options;        /* Number of above */
    struct l2tp_call_ops_t *lns_ops;	/* Call ops if we act as LNS */
    char *lns_options[MAX_OPTS+1]; /* Handler options if we act as LNS */
    int num_lns_options;        /* Number of above */
    int hide_avps;		/* If true, hide AVPs to this peer */
    int retain_tunnel;		/* If true, keep tunnel after last session is
				   deleted.  Otherwise, delete tunnel too. */
    int validate_peer_ip;	/* If true, do not accept datagrams except
				   from initial peer IP address */
    int persist;                /* If true, keep session established */
    int holdoff;                /* If persist is true, delay after which the
                                   session is re-established. */
    int maxfail;                /* If persist is true, try to establish a
                                   broken session at most on maxfail times. */
    int fail;                   /* Number of failed attempts. */
} l2tp_peer;

/* An L2TP tunnel */
typedef struct l2tp_tunnel_t {
    hash_bucket hash_by_my_id;  /* Hash bucket for tunnel hash table */
    hash_bucket hash_by_peer;   /* Hash bucket for tunnel-by-peer table */
    hash_table sessions_by_my_id;	/* Sessions in this tunnel */
    uint16_t my_id;		/* My tunnel ID */
    uint16_t assigned_id;	/* ID assigned by peer */
    l2tp_peer *peer;		/* The L2TP peer */
    struct sockaddr_in peer_addr; /* Peer's address */
    uint16_t Ns;		/* Sequence of next packet to queue */
    uint16_t Ns_on_wire;	/* Sequence of next packet to be sent on wire */
    uint16_t Nr;		/* Expected sequence of next received packet */
    uint16_t peer_Nr;		/* Last packet ack'd by peer */
    int ssthresh;		/* Slow-start threshold */
    int cwnd;                   /* Congestion window */
    int cwnd_counter;		/* Counter for incrementing cwnd in congestion-avoidance phase */
    int timeout;		/* Retransmission timeout (seconds) */
    int retransmissions;	/* Number of retransmissions */
    int rws;			/* Our receive window size */
    int peer_rws;		/* Peer receive window size */
    EventSelector *es;		/* The event selector */
    EventHandler *hello_handler; /* Timer for sending HELLO */
    EventHandler *timeout_handler; /* Handler for timeout */
    EventHandler *ack_handler;	/* Handler for sending Ack */
    l2tp_dgram *xmit_queue_head; /* Head of control transmit queue */
    l2tp_dgram *xmit_queue_tail; /* Tail of control transmit queue */
    l2tp_dgram *xmit_new_dgrams; /* dgrams which have not been transmitted */
    char peer_hostname[MAX_HOSTNAME]; /* Peer's host name */
    unsigned char response[MD5LEN]; /* Our response to challenge */
    unsigned char expected_response[MD5LEN]; /* Expected resp. to challenge */
    int state;			/* Tunnel state */
    struct rtentry rt;		/* Route added to destination */
    struct l2tp_call_ops_t *call_ops; /* Call ops                      */
    void *private;		/* Private data for call-op's use */
} l2tp_tunnel;

/* A session within a tunnel */
typedef struct l2tp_session_t {
    hash_bucket hash_by_my_id;	/* Hash bucket for session table  */
    l2tp_tunnel *tunnel;	/* Tunnel we belong to            */
    uint16_t my_id;		/* My ID                          */
    uint16_t assigned_id;	/* Assigned ID                    */
    int state;			/* Session state                  */

    /* Some flags */
    unsigned int snooping:1;    /* Are we snooping in on LCP?     */
    unsigned int got_send_accm:1; /* Do we have send_accm?        */
    unsigned int got_recv_accm:1; /* Do we have recv_accm?        */
    unsigned int we_are_lac:1;    /* Are we a LAC?                */
    unsigned int sequencing_required:1; /* Sequencing required?   */
    unsigned int sent_sli:1;    /* Did we send SLI yet?           */

    uint32_t send_accm;		/* Negotiated send accm           */
    uint32_t recv_accm;		/* Negotiated receive accm        */
    uint16_t Nr;		/* Data sequence number */
    uint16_t Ns;		/* Data sequence number */
    struct l2tp_call_ops_t *call_ops; /* Call ops                      */
    char calling_number[MAX_HOSTNAME]; /* Calling number          */
    void *private;		/* Private data for call-op's use */
} l2tp_session;

/* Call operations */
typedef struct l2tp_call_ops_t {
    /* Called once session has been established (LAC) or when we want
       to establish session (LNS) */
    int (*establish)(l2tp_session *ses);

    /* Called when session must be closed.  May be called without
       established() being called if session could not be established.*/
    void (*close)(l2tp_session *ses, char const *reason, int may_reestablish);

    /* Called when a PPP frame arrives over tunnel */
    void (*handle_ppp_frame)(l2tp_session *ses, unsigned char *buf,
			     size_t len);

    /* Called once tunnel has been established (LAC) or when we want
       to establish tunnel (LNS) */
    int (*tunnel_establish)(l2tp_tunnel *tun);

    /* Called when tunnel must be closed.  May be called without
       established() being called if tunnel could not be established.*/
    void (*tunnel_close)(l2tp_tunnel *tun);
} l2tp_call_ops;

/* an LNS handler */
typedef struct l2tp_lns_handler_t {
    struct l2tp_lns_handler_t *next;
    char const *handler_name;
    l2tp_call_ops *call_ops;
} l2tp_lns_handler;

/* an LAC handler */
typedef struct l2tp_lac_handler_t {
    struct l2tp_lac_handler_t *next;
    char const *handler_name;
    l2tp_call_ops *call_ops;
} l2tp_lac_handler;

/* Settings */
typedef struct l2tp_settings_t {
    int listen_port;		/* Port we listen on */
    struct in_addr listen_addr;	/* IP to bind to */
} l2tp_settings;

extern l2tp_settings Settings;

/* Bit definitions */
#define TYPE_BIT         0x80
#define LENGTH_BIT       0x40
#define SEQUENCE_BIT     0x08
#define OFFSET_BIT       0x02
#define PRIORITY_BIT     0x01
#define RESERVED_BITS    0x34
#define VERSION_MASK     0x0F
#define VERSION_RESERVED 0xF0

#define AVP_MANDATORY_BIT 0x80
#define AVP_HIDDEN_BIT    0x40
#define AVP_RESERVED_BITS 0x3C

#define MANDATORY     1
#define NOT_MANDATORY 0
#define HIDDEN        1
#define NOT_HIDDEN    0
#define VENDOR_IETF   0

#define AVP_MESSAGE_TYPE               0
#define AVP_RESULT_CODE                1
#define AVP_PROTOCOL_VERSION           2
#define AVP_FRAMING_CAPABILITIES       3
#define AVP_BEARER_CAPABILITIES        4
#define AVP_TIE_BREAKER                5
#define AVP_FIRMWARE_REVISION          6
#define AVP_HOST_NAME                  7
#define AVP_VENDOR_NAME                8
#define AVP_ASSIGNED_TUNNEL_ID         9
#define AVP_RECEIVE_WINDOW_SIZE       10
#define AVP_CHALLENGE                 11
#define AVP_Q931_CAUSE_CODE           12
#define AVP_CHALLENGE_RESPONSE        13
#define AVP_ASSIGNED_SESSION_ID       14
#define AVP_CALL_SERIAL_NUMBER        15
#define AVP_MINIMUM_BPS               16
#define AVP_MAXIMUM_BPS               17
#define AVP_BEARER_TYPE               18
#define AVP_FRAMING_TYPE              19
#define AVP_CALLED_NUMBER             21
#define AVP_CALLING_NUMBER            22
#define AVP_SUB_ADDRESS               23
#define AVP_TX_CONNECT_SPEED          24
#define AVP_PHYSICAL_CHANNEL_ID       25
#define AVP_INITIAL_RECEIVED_CONFREQ  26
#define AVP_LAST_SENT_CONFREQ         27
#define AVP_LAST_RECEIVED_CONFREQ     28
#define AVP_PROXY_AUTHEN_TYPE         29
#define AVP_PROXY_AUTHEN_NAME         30
#define AVP_PROXY_AUTHEN_CHALLENGE    31
#define AVP_PROXY_AUTHEN_ID           32
#define AVP_PROXY_AUTHEN_RESPONSE     33
#define AVP_CALL_ERRORS               34
#define AVP_ACCM                      35
#define AVP_RANDOM_VECTOR             36
#define AVP_PRIVATE_GROUP_ID          37
#define AVP_RX_CONNECT_SPEED          38
#define AVP_SEQUENCING_REQUIRED       39

#define HIGHEST_AVP                   39

#define MESSAGE_SCCRQ                  1
#define MESSAGE_SCCRP                  2
#define MESSAGE_SCCCN                  3
#define MESSAGE_StopCCN                4
#define MESSAGE_HELLO                  6

#define MESSAGE_OCRQ                   7
#define MESSAGE_OCRP                   8
#define MESSAGE_OCCN                   9

#define MESSAGE_ICRQ                  10
#define MESSAGE_ICRP                  11
#define MESSAGE_ICCN                  12

#define MESSAGE_CDN                   14
#define MESSAGE_WEN                   15
#define MESSAGE_SLI                   16

/* A fake type for our own consumption */
#define MESSAGE_ZLB                32767

/* Result and error codes */
#define RESULT_GENERAL_REQUEST          1
#define RESULT_GENERAL_ERROR            2
#define RESULT_CHANNEL_EXISTS           3
#define RESULT_NOAUTH                   4
#define RESULT_UNSUPPORTED_VERSION      5
#define RESULT_SHUTTING_DOWN            6
#define RESULT_FSM_ERROR                7

#define ERROR_OK                        0
#define ERROR_NO_CONTROL_CONNECTION     1
#define ERROR_BAD_LENGTH                2
#define ERROR_BAD_VALUE                 3
#define ERROR_OUT_OF_RESOURCES          4
#define ERROR_INVALID_SESSION_ID        5
#define ERROR_VENDOR_SPECIFIC           6
#define ERROR_TRY_ANOTHER               7
#define ERROR_UNKNOWN_AVP_WITH_M_BIT    8

/* Tunnel states */
enum {
    TUNNEL_IDLE,
    TUNNEL_WAIT_CTL_REPLY,
    TUNNEL_WAIT_CTL_CONN,
    TUNNEL_ESTABLISHED,
    TUNNEL_RECEIVED_STOP_CCN,
    TUNNEL_SENT_STOP_CCN
};

/* Session states */
enum {
    SESSION_IDLE,
    SESSION_WAIT_TUNNEL,
    SESSION_WAIT_REPLY,
    SESSION_WAIT_CONNECT,
    SESSION_ESTABLISHED
};

/* Constants and structures for parsing config file */
typedef struct l2tp_opt_descriptor_t {
    char const *name;
    int type;
    void *addr;
} l2tp_opt_descriptor;

/* Structures for option-handlers for different sections */
typedef struct option_handler_t {
    struct option_handler_t *next;
    char const *section;
    int (*process_option)(EventSelector *, char const *, char const *);
} option_handler;

#define OPT_TYPE_BOOL       0
#define OPT_TYPE_INT        1
#define OPT_TYPE_IPADDR     2
#define OPT_TYPE_STRING     3
#define OPT_TYPE_CALLFUNC   4
#define OPT_TYPE_PORT       5   /* 1-65535 */

/* tunnel.c */
l2tp_session *l2tp_tunnel_find_session(l2tp_tunnel *tunnel, uint16_t sid);
l2tp_tunnel *l2tp_tunnel_find_by_my_id(uint16_t id);
l2tp_tunnel *l2tp_tunnel_find_for_peer(l2tp_peer *peer, EventSelector *es);
void l2tp_tunnel_add_session(l2tp_session *ses);
void l2tp_tunnel_reestablish(EventSelector *es, int fd, unsigned int flags, void *data);
void l2tp_tunnel_delete_session(l2tp_session *ses, char const *reason, int may_reestablish);
void l2tp_tunnel_handle_received_control_datagram(l2tp_dgram *dgram,
						  EventSelector *es,
						  struct sockaddr_in *from);
void l2tp_tunnel_init(EventSelector *es);
void l2tp_tunnel_xmit_control_message(l2tp_tunnel *tunnel, l2tp_dgram  *dgram);
void l2tp_tunnel_stop_tunnel(l2tp_tunnel *tunnel, char const *reason);
void l2tp_tunnel_stop_all(char const *reason);

l2tp_session *l2tp_tunnel_first_session(l2tp_tunnel *tunnel, void **cursor);
l2tp_session *l2tp_tunnel_next_session(l2tp_tunnel *tunnel, void **cursor);
void tunnel_send_ZLB(l2tp_tunnel *tunnel);

/* Access functions */
int l2tp_num_tunnels(void);
l2tp_tunnel *l2tp_first_tunnel(void **cursor);
l2tp_tunnel *l2tp_next_tunnel(void **cursor);
char const *l2tp_tunnel_state_name(l2tp_tunnel *tunnel);

/* session.c */
void l2tp_session_lcp_snoop(l2tp_session *ses,
			    unsigned char const *buf,
			    int len,
			    int incoming);
int l2tp_session_register_lns_handler(l2tp_lns_handler *handler);
int l2tp_session_register_lac_handler(l2tp_lac_handler *handler);
l2tp_lns_handler *l2tp_session_find_lns_handler(char const *name);
l2tp_lac_handler *l2tp_session_find_lac_handler(char const *name);

void l2tp_session_send_CDN(l2tp_session *ses, int result_code, int error_code,
			   char const *fmt, ...);
void l2tp_session_hash_init(hash_table *tab);
void l2tp_session_free(l2tp_session *ses, char const *reason, int may_reestablish);
void l2tp_session_notify_tunnel_open(l2tp_session *ses);
void l2tp_session_lns_handle_incoming_call(l2tp_tunnel *tunnel,
				      uint16_t assigned_id,
				      l2tp_dgram *dgram,
				      char const *calling_number);
void l2tp_session_handle_CDN(l2tp_session *ses, l2tp_dgram *dgram);
void l2tp_session_handle_ICRP(l2tp_session *ses, l2tp_dgram *dgram);
void l2tp_session_handle_ICCN(l2tp_session *ses, l2tp_dgram *dgram);
char const *l2tp_session_state_name(l2tp_session *ses);

/* Call this when a LAC wants to send an incoming-call-request to an LNS */
l2tp_session *l2tp_session_call_lns(l2tp_peer *peer,
				    char const *calling_number,
				    EventSelector *es,
				    void *private);

/* dgram.c */
l2tp_dgram *l2tp_dgram_new(size_t len);
l2tp_dgram *l2tp_dgram_new_control(uint16_t msg_type, uint16_t tid, uint16_t sid);
void l2tp_dgram_free(l2tp_dgram *dgram);
l2tp_dgram *l2tp_dgram_take_from_wire(int fd, struct sockaddr_in *from);
int l2tp_dgram_send_to_wire(l2tp_dgram const *dgram,
		       struct sockaddr_in const *to);
int l2tp_dgram_send_ppp_frame(l2tp_session *ses, unsigned char const *buf,
			 int len);

unsigned char *l2tp_dgram_search_avp(l2tp_dgram *dgram,
				     l2tp_tunnel *tunnel,
				     int *mandatory,
				     int *hidden,
				     uint16_t *len,
				     uint16_t vendor,
				     uint16_t type);

unsigned char *l2tp_dgram_pull_avp(l2tp_dgram *dgram,
				   l2tp_tunnel *tunnel,
				   int *mandatory,
				   int *hidden,
				   uint16_t *len,
				   uint16_t *vendor,
				   uint16_t *type,
				   int *err);

int l2tp_dgram_add_avp(l2tp_dgram *dgram,
		       l2tp_tunnel *tunnel,
		       int mandatory,
		       uint16_t len,
		       uint16_t vendor,
		       uint16_t type,
		       void *val);

int l2tp_dgram_validate_avp(uint16_t vendor, uint16_t type,
			    uint16_t len, int mandatory);

/* utils.c */
typedef void (*l2tp_shutdown_func)(void *);

void l2tp_random_init(void);
void l2tp_random_fill(void *ptr, size_t size);
void l2tp_set_errmsg(char const *fmt, ...);
char const *l2tp_get_errmsg(void);
void l2tp_cleanup(void);
int l2tp_register_shutdown_handler(l2tp_shutdown_func f, void *data);
void l2tp_die(void);
int l2tp_load_handler(EventSelector *es, char const *fname);

#define L2TP_RANDOM_FILL(x) l2tp_random_fill(&(x), sizeof(x))

/* network.c */
extern int Sock;
extern char Hostname[MAX_HOSTNAME];

int l2tp_network_init(EventSelector *es);
void network_readable(EventSelector *es, int fd, unsigned int flags, void *data);

/* peer.c */
void l2tp_peer_init(void);
l2tp_peer *l2tp_peer_find(struct sockaddr_in *addr, char const *hostname);
l2tp_peer *l2tp_peer_insert(struct sockaddr_in *addr);

/* debug.c */
char const *l2tp_debug_avp_type_to_str(uint16_t type);
char const *l2tp_debug_message_type_to_str(uint16_t type);
char const *l2tp_debug_tunnel_to_str(l2tp_tunnel *tunnel);
char const *l2tp_debug_session_to_str(l2tp_session *session);
char const *l2tp_debug_describe_dgram(l2tp_dgram const *dgram);
void l2tp_db(int what, char const *fmt, ...);
void l2tp_debug_set_bitmask(unsigned long mask);

/* auth.c */
void l2tp_auth_gen_response(uint16_t msg_type, char const *secret,
			    unsigned char const *challenge, size_t chal_len,
			    unsigned char buf[16]);

/* options.c */
int l2tp_parse_config_file(EventSelector *es,
			   char const *fname);
int l2tp_option_set(EventSelector *es,
		    char const *name,
		    char const *value,
		    l2tp_opt_descriptor descriptors[]);

void l2tp_option_register_section(option_handler *h);
char const *l2tp_chomp_word(char const *line, char *word);

/* tunnel.c */
#ifdef RTCONFIG_VPNC
extern int vpnc;
#endif

#endif
