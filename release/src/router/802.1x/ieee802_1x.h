#ifndef IEEE802_1X_H
#define IEEE802_1X_H

#include "rtmp_type.h"

/* IEEE Std 802.1X-2001, 7.2 */
struct ieee802_1x_hdr {
	u8 version;
	u8 type;
	u16 length;
	/* followed by length octets of data */
} __attribute__ ((packed));

#define LENGTH_8021X_HDR        4

#define EAPOL_VERSION		1
#define EAPOL_VERSION_2		2    //for WPA2(pre-auth)

#define LENGTH_802_1_H			8

enum { IEEE802_1X_TYPE_EAP_PACKET = 0,
       IEEE802_1X_TYPE_EAPOL_START = 1,
       IEEE802_1X_TYPE_EAPOL_LOGOFF = 2,
       IEEE802_1X_TYPE_EAPOL_KEY = 3,
       IEEE802_1X_TYPE_EAPOL_ENCAPSULATED_ASF_ALERT = 4
};

struct ieee802_1x_eapol_key {
	u8 type;
	u16 key_length;
	u8 replay_counter[8]; /* does not repeat within the life of the keying
			       * material used to encrypt the Key field;
			       * 64-bit NTP timestamp MAY be used here */
	u8 key_iv[16]; /* cryptographically random number */
	u8 key_index; /* key flag in the most significant bit:
		       * 0 = broadcast (default key),
		       * 1 = unicast (key mapping key); key index is in the
		       * 7 least significant bits */
	u8 key_signature[16]; /* HMAC-MD5 message integrity check computed with
			       * MS-MPPE-Send-Key as the key */

	/* followed by key: if packet body length = 44 + key length, then the
	 * key field (of key_length bytes) contains the key in encrypted form;
	 * if packet body length = 44, key field is absent and key_length
	 * represents the number of least significant octets from
	 * MS-MPPE-Send-Key attribute to be used as the keying material;
	 * RC4 key used in encryption = Key-IV + MS-MPPE-Recv-Key */
} __attribute__ ((packed));

enum { EAPOL_KEY_TYPE_RC4 = 1 };


/* RFC 2284 - PPP Extensible Authentication Protocol (EAP) */

struct eap_hdr {
	u8 code;
	u8 identifier;
	u16 length; /* including code and identifier */
	/* followed by length-2 octets of data */
} __attribute__ ((packed));

#define LENGTH_EAP_HDR        4

enum { EAP_CODE_REQUEST = 1, EAP_CODE_RESPONSE = 2, EAP_CODE_SUCCESS = 3,
       EAP_CODE_FAILURE = 4 };

/* EAP Request and Response data begins with one octet Type. Success and
 * Failure do not have additional data. */

/* RFC 2284, 3.0 */
enum { EAP_TYPE_IDENTITY = 1,
       EAP_TYPE_NOTIFICATION = 2,
       EAP_TYPE_NAK = 3 /* Response only */,
       EAP_TYPE_MD5_CHALLENGE = 4,
       EAP_TYPE_ONE_TIME_PASSWORD = 5 /* RFC 1938 */,
       EAP_TYPE_GENERIC_TOKEN_CARD = 6,
       EAP_TYPE_TLS = 13 /* RFC 2716 */,
       EAP_TYPE_TTLS = 21 /* draft-ietf-pppext-eap-ttls-02.txt */,
       EAP_TYPE_PEAP = 25 /* draft-josefsson-pppext-eap-tls-eap-06.txt */,
};

typedef	enum	_Dot1xInternalCmd
{
	DOT1X_DISCONNECT_ENTRY, 			
	DOT1X_RELOAD_CONFIG,
}	DOT1X_INTERNAL_CMD;

// Key mapping keys require a BSSID
typedef struct _NDIS_802_11_KEY
{
    u32           Length;             // Length of this structure
    u8            addr[6];
    u32           KeyIndex;           
    u32           KeyLength;          // length of key in bytes
    u8            KeyMaterial[64];     // variable length depending on above field
} NDIS_802_11_KEY, *PNDIS_802_11_KEY;

// The definition MUST synchronize with driver(in oid.h)
#define MAX_RADIUS_SRV_NUM			2	  // 802.1x failover number

typedef struct PACKED _RADIUS_SRV_INFO {
	unsigned int		radius_ip;
	unsigned int		radius_port;
	unsigned char		radius_key[64];
	unsigned char		radius_key_len;
} RADIUS_SRV_INFO, *PRADIUS_SRV_INFO;

typedef struct PACKED _DOT1X_BSS_INFO
{
	unsigned char		radius_srv_num;			
	RADIUS_SRV_INFO		radius_srv_info[MAX_RADIUS_SRV_NUM];	
	unsigned char		ieee8021xWEP;		 // dynamic WEP
    unsigned char       key_index;           
    unsigned char       key_length;          // length of key in bytes
    unsigned char       key_material[13];    
	unsigned char		nasId[IFNAMSIZ];
	unsigned char		nasId_len;
} DOT1X_BSS_INFO, *PDOT1X_BSS_INFO;

// It's used by 802.1x daemon to require relative configuration
typedef struct PACKED _DOT1X_CMM_CONF
{
    unsigned int       	Length;             // Length of this structure    
    unsigned char		mbss_num;			// indicate multiple BSS number 
	unsigned int		own_ip_addr;	
	unsigned int		retry_interval;
	unsigned int		session_timeout_interval;
	unsigned int		quiet_interval;
	unsigned char		EAPifname[MAX_MBSSID_NUM][IFNAMSIZ];
	unsigned char		EAPifname_len[MAX_MBSSID_NUM];
	unsigned char 		PreAuthifname[MAX_MBSSID_NUM][IFNAMSIZ];
	unsigned char		PreAuthifname_len[MAX_MBSSID_NUM];
	DOT1X_BSS_INFO		Dot1xBssInfo[MAX_MBSSID_NUM];
} DOT1X_CMM_CONF, *PDOT1X_CMM_CONF;

typedef struct PACKED _DOT1X_IDLE_TIMEOUT
{
	unsigned char			StaAddr[MAC_ADDR_LEN];			
	unsigned int			idle_timeout;
} DOT1X_IDLE_TIMEOUT, *PDOT1X_IDLE_TIMEOUT;

void ieee802_1x_new_station(rtapd *apd, struct sta_info *sta);
void ieee802_1x_free_station(struct sta_info *sta);

void ieee802_1x_request_identity(rtapd *apd, struct sta_info *sta, u8 id);
void ieee802_1x_tx_canned_eap(rtapd *apd, struct sta_info *sta, u8 id, int success);
void ieee802_1x_tx_req(rtapd *apd, struct sta_info *sta, u8 id);
void ieee802_1x_tx_key(rtapd *hapd, struct sta_info *sta, u8 id);
void ieee802_1x_send_resp_to_server(rtapd *apd, struct sta_info *sta);
void ieee802_1x_set_sta_authorized(rtapd *rtapd, struct sta_info *sta, int authorized);
int ieee802_1x_init(rtapd *apd);
void ieee802_1x_new_auth_session(rtapd *apd, struct sta_info *sta);

#endif /* IEEE802_1X_H */
