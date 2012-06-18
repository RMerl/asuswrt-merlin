#ifndef RADIUS_H
#define RADIUS_H

/* RFC 2865 - RADIUS */

struct radius_hdr {
	u8 code;
	u8 identifier;
	u16 length; /* including this header */
	u8 authenticator[16];
	/* followed by length-20 octets of attributes */
} __attribute__ ((packed));

enum { RADIUS_CODE_ACCESS_REQUEST = 1,
       RADIUS_CODE_ACCESS_ACCEPT = 2,
       RADIUS_CODE_ACCESS_REJECT = 3,
       RADIUS_CODE_ACCOUNTING_REQUEST = 4,
       RADIUS_CODE_ACCOUNTING_RESPONSE = 5,
       RADIUS_CODE_ACCESS_CHALLENGE = 11,
       RADIUS_CODE_STATUS_SERVER = 12,
       RADIUS_CODE_STATUS_CLIENT = 13,
       RADIUS_CODE_RESERVED = 255
};

struct radius_attr_hdr {
	u8 type;
	u8 length; /* including this header */
	/* followed by length-2 octets of attribute value */
} __attribute__ ((packed));

#define RADIUS_MAX_ATTR_LEN (255 - sizeof(struct radius_attr_hdr))

enum { RADIUS_ATTR_USER_NAME = 1,
       RADIUS_ATTR_USER_PASSWORD = 2,
       RADIUS_ATTR_NAS_IP_ADDRESS = 4,
       RADIUS_ATTR_NAS_PORT = 5,
       RADIUS_ATTR_FRAMED_MTU = 12,
       RADIUS_ATTR_STATE = 24,
       RADIUS_ATTR_VENDOR_SPECIFIC = 26,
       RADIUS_ATTR_SESSION_TIMEOUT = 27,
       RADIUS_ATTR_IDLE_TIMEOUT = 28,
       RADIUS_ATTR_TERMINATION_ACTION = 29,
       RADIUS_ATTR_CALLED_STATION_ID = 30,
       RADIUS_ATTR_CALLING_STATION_ID = 31,
       RADIUS_ATTR_NAS_IDENTIFIER = 32,
       RADIUS_ATTR_ACCT_STATUS_TYPE = 40,
       RADIUS_ATTR_ACCT_DELAY_TIME = 41,
       RADIUS_ATTR_ACCT_INPUT_OCTETS = 42,
       RADIUS_ATTR_ACCT_OUTPUT_OCTETS = 43,
       RADIUS_ATTR_ACCT_SESSION_ID = 44,
       RADIUS_ATTR_ACCT_AUTHENTIC = 45,
       RADIUS_ATTR_ACCT_SESSION_TIME = 46,
       RADIUS_ATTR_ACCT_INPUT_PACKETS = 47,
       RADIUS_ATTR_ACCT_OUTPUT_PACKETS = 48,
       RADIUS_ATTR_ACCT_TERMINATE_CAUSE = 49,
       RADIUS_ATTR_ACCT_MULTI_SESSION_ID = 50,
       RADIUS_ATTR_ACCT_LINK_COUNT = 51,
       RADIUS_ATTR_NAS_PORT_TYPE = 61,
       RADIUS_ATTR_CONNECT_INFO = 77,
       RADIUS_ATTR_EAP_MESSAGE = 79,
       RADIUS_ATTR_MESSAGE_AUTHENTICATOR = 80
};


/* Termination-Action */
#define RADIUS_TERMINATION_ACTION_DEFAULT 0
#define RADIUS_TERMINATION_ACTION_RADIUS_REQUEST 1

/* NAS-Port-Type */
#define RADIUS_NAS_PORT_TYPE_IEEE_802_11 19


/* RFC 2548 - Microsoft Vendor-specific RADIUS Attributes */
#define RADIUS_VENDOR_ID_MICROSOFT 311

struct radius_attr_vendor_microsoft {
	u8 vendor_type;
	u8 vendor_length;
} __attribute__ ((packed));

enum { RADIUS_VENDOR_ATTR_MS_MPPE_SEND_KEY = 16,
       RADIUS_VENDOR_ATTR_MS_MPPE_RECV_KEY = 17
};

struct radius_ms_mppe_keys {
	u8 *send;
	size_t send_len;
	u8 *recv;
	size_t recv_len;
};


/* RADIUS message structure for new and parsed messages */
struct radius_msg {
	unsigned char *buf;
	size_t buf_size; /* total size allocated for buf */
	size_t buf_used; /* bytes used in buf */

	struct radius_hdr *hdr;

	struct radius_attr_hdr **attrs; /* array of pointers to attributes */
	size_t attr_size; /* total size of the attribute pointer array */
	size_t attr_used; /* total number of attributes in the array */
};


/* Default size to be allocated for new RADIUS messages */
#define RADIUS_DEFAULT_MSG_SIZE 1024

/* Default size to be allocated for attribute array */
#define RADIUS_DEFAULT_ATTR_COUNT 16


/* MAC address ASCII format for IEEE 802.1X use
 * (draft-congdon-radius-8021x-20.txt) */
#define RADIUS_802_1X_ADDR_FORMAT "%02X-%02X-%02X-%02X-%02X-%02X"
/* MAC address ASCII format for non-802.1X use */
#define RADIUS_ADDR_FORMAT "%02x%02x%02x%02x%02x%02x"

struct radius_msg *Radius_msg_new(u8 code, u8 identifier);
int Radius_msg_initialize(struct radius_msg *msg, size_t init_len);
void Radius_msg_set_hdr(struct radius_msg *msg, u8 code, u8 identifier);
void Radius_msg_free(struct radius_msg *msg);
void Radius_msg_finish(struct radius_msg *msg, u8 *secret, size_t secret_len);
struct radius_attr_hdr *Radius_msg_add_attr(struct radius_msg *msg, u8 type,
					    u8 *data, size_t data_len);
struct radius_msg *Radius_msg_parse(const u8 *data, size_t len);
int Radius_msg_add_eap(struct radius_msg *msg, u8 *data, size_t data_len);
u8 *Radius_msg_get_eap(struct radius_msg *msg, size_t *len);
int Radius_msg_verify(struct radius_msg *msg, u8 *secret, size_t secret_len,
		      struct radius_msg *sent_msg);
int Radius_msg_verify_acct(struct radius_msg *msg, u8 *secret,
			   size_t secret_len, struct radius_msg *sent_msg);
int Radius_msg_copy_attr(struct radius_msg *dst, struct radius_msg *src, u8 type);
void Radius_msg_make_authenticator(struct radius_msg *msg, u8 *data, size_t len);
struct radius_ms_mppe_keys *
Radius_msg_get_ms_keys(struct radius_msg *msg, struct radius_msg *sent_msg,
		       u8 *secret, size_t secret_len);
struct radius_attr_hdr *
Radius_msg_add_attr_user_password(struct radius_msg *msg,
				  u8 *data, size_t data_len, u8 *secret, size_t secret_len);
int Radius_msg_get_attr(struct radius_msg *msg, u8 type, u8 *buf, size_t len);

static inline int Radius_msg_add_attr_int32(struct radius_msg *msg, u8 type, u32 value)
{
	u32 val = htonl(value);
	return Radius_msg_add_attr(msg, type, (u8 *) &val, 4) != NULL;
}

static inline int Radius_msg_get_attr_int32(struct radius_msg *msg, u8 type, u32 *value)
{
	u32 val;
	int res;
	res = Radius_msg_get_attr(msg, type, (u8 *) &val, 4);
	if (res != 4)
		return -1;

	*value = ntohl(val);
	return 0;
}

#endif /* RADIUS_H */
