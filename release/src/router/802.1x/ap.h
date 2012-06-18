#ifndef AP_H
#define AP_H

/* STA flags */
#define WLAN_STA_AUTH           BIT(0)
#define WLAN_STA_ASSOC          BIT(1)
#define WLAN_STA_PS             BIT(2)
#define WLAN_STA_TIM            BIT(3)
#define WLAN_STA_PERM           BIT(4)
#define WLAN_STA_PENDING_POLL   BIT(6) /* pending activity poll not ACKed */

#define WLAN_RATE_1M            BIT(0)
#define WLAN_RATE_2M            BIT(1)
#define WLAN_RATE_5M5           BIT(2)
#define WLAN_RATE_11M           BIT(3)
#define WLAN_RATE_COUNT         4

/* Maximum size of Supported Rates info element. IEEE 802.11 has a limit of 8,
 * but some pre-standard IEEE 802.11g products use longer elements. */
#define WLAN_SUPP_RATES_MAX     32


struct sta_info {
	struct sta_info         *next; /* next entry in sta list */
	struct sta_info         *hnext; /* next entry in hash table list */
	u8                      addr[6];
	u16                     aid; /* STA's unique AID (1 .. 2007) or 0 if not yet assigned */
	u32                     flags;
	u16                     capability;
	u16                     listen_interval; /* or beacon_int for APs */
	u8                      supported_rates[WLAN_SUPP_RATES_MAX];
	u8                      tx_supp_rates;

	enum { STA_NULLFUNC = 0, STA_DISASSOC, STA_DEAUTH } timeout_next;

	/* IEEE 802.1X related data */
	struct                  eapol_state_machine *eapol_sm;
	int                     radius_identifier;
	/* TODO: check when the last messages can be released */
	struct radius_msg       *last_recv_radius;
	u8                      *last_eap_supp; /* last received EAP Response from Supplicant */
	size_t                  last_eap_supp_len;
	u8                      *last_eap_radius; /* last received EAP Response from Authentication Server */
	size_t                  last_eap_radius_len;
	u8                      *identity;
	size_t                  identity_len;

	/* Keys for encrypting and signing EAPOL-Key frames */
	u8                      *eapol_key_sign;
	size_t                  eapol_key_sign_len;
	u8                      *eapol_key_crypt;
	size_t                  eapol_key_crypt_len;

	/* IEEE 802.11f (IAPP) related data */
	struct ieee80211_mgmt   *last_assoc_req;

	// Multiple SSID interface
	u8						ApIdx;
	u16						ethertype;

	// From which raw socket
	int						SockNum;
};

#define MAX_STA_COUNT           1024

/* Maximum number of AIDs to use for STAs; must be 2007 or lower
 * (8802.11 limitation) */
#define MAX_AID_TABLE_SIZE      256

#define STA_HASH_SIZE           256
#define STA_HASH(sta)           (sta[5])

/* Default value for maximum station inactivity. After AP_MAX_INACTIVITY has
 * passed since last received frame from the station, a nullfunc data frame is
 * sent to the station. If this frame is not acknowledged and no other frames
 * have been received, the station will be disassociated after
 * AP_DISASSOC_DELAY. Similarily, a the station will be deauthenticated after
 * AP_DEAUTH_DELAY. AP_TIMEOUT_RESOLUTION is the resolution that is used with
 * max inactivity timer. All these times are in seconds. */
#define AP_MAX_INACTIVITY       (5* 60)
#define AP_DISASSOC_DELAY       (1)
#define AP_DEAUTH_DELAY         (1)

#endif /* AP_H */
