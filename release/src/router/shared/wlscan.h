#define BIT(n) (1 << (n))
#define WPA_GET_LE16(a) ((unsigned int) (((a)[1] << 8) | (a)[0]))

//#define DOT11_MNG_WPA_ID 0xdd
//#define DOT11_MNG_RSN_ID 0x30

#define WPA_CIPHER_NONE_ BIT(0)
#define WPA_CIPHER_WEP40_ BIT(1)
#define WPA_CIPHER_WEP104_ BIT(2)
#define WPA_CIPHER_TKIP_ BIT(3)
#define WPA_CIPHER_CCMP_ BIT(4)

#define WPA_KEY_MGMT_IEEE8021X_ BIT(0)
#define WPA_KEY_MGMT_IEEE8021X2_ BIT(6)
#define WPA_KEY_MGMT_PSK_ BIT(1)
#define WPA_KEY_MGMT_PSK2_ BIT(5)
#define WPA_KEY_MGMT_NONE_ BIT(2)
#define WPA_KEY_MGMT_IEEE8021X_NO_WPA_ BIT(3)
#define WPA_KEY_MGMT_WPA_NONE_ BIT(4)

#define WPA_PROTO_WPA_ BIT(0)
#define WPA_PROTO_RSN_ BIT(1)

#define PMKID_LEN 16

#define NUMCHANS 64

#define WSC_ID_VERSION				0x104A
#define WSC_ID_VERSION_LEN			1
#define WSC_ID_VERSION_BEACON			0x00000001

#define WSC_ID_SC_STATE				0x1044
#define WSC_ID_SC_STATE_LEN			1
#define WSC_ID_SC_STATE_BEACON			0x00000002

#define WSC_ID_AP_SETUP_LOCKED			0x1057
#define WSC_ID_AP_SETUP_LOCKED_LEN		1
#define WSC_ID_AP_SETUP_LOCKED_BEACON		0x00000004

#define WSC_ID_SEL_REGISTRAR			0x1041
#define WSC_ID_SEL_REGISTRAR_LEN		1
#define WSC_ID_SEL_REGISTRAR_BEACON		0x00000008

#define WSC_ID_DEVICE_PWD_ID			0x1012
#define WSC_ID_DEVICE_PWD_ID_LEN		2
#define WSC_ID_DEVICE_PWD_ID_BEACON		0x00000010

#define WSC_ID_SEL_REG_CFG_METHODS		0x1053
#define WSC_ID_SEL_REG_CFG_METHODS_LEN		2
#define WSC_ID_SEL_REG_CFG_METHODS_BEACON	0x00000020

#define WSC_ID_UUID_E				0x1047
#define WSC_ID_UUID_E_LEN			16
#define WSC_ID_UUID_E_BEACON			0x00000040

#define WSC_ID_RF_BAND				0x103C
#define WSC_ID_RF_BAND_LEN			1
#define WSC_ID_RF_BAND_BEACON			0x00000080

#define WSC_ID_PRIMARY_DEVICE_TYPE		0x1054
#define WSC_ID_PRIMARY_DEVICE_TYPE_LE		8
#define WSC_ID_PRIMARY_DEVICE_TYPE_BEACON	0x00000100

static const int WPA_SELECTOR_LEN = 4;
static const unsigned char WPA_OUI_TYPE_ARR[] = { 0x00, 0x50, 0xf2, 1 };
static const unsigned int WPA_VERSION_ = 1;
static const unsigned char WPA_AUTH_KEY_MGMT_NONE[] = { 0x00, 0x50, 0xf2, 0 };
static const unsigned char WPA_AUTH_KEY_MGMT_UNSPEC_802_1X[] = { 0x00, 0x50, 0xf2, 1 };
static const unsigned char WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X[] = { 0x00, 0x50, 0xf2, 2 };
static const unsigned char WPA_CIPHER_SUITE_NONE[] = { 0x00, 0x50, 0xf2, 0 };
static const unsigned char WPA_CIPHER_SUITE_WEP40[] = { 0x00, 0x50, 0xf2, 1 };
static const unsigned char WPA_CIPHER_SUITE_TKIP[] = { 0x00, 0x50, 0xf2, 2 };
static const unsigned char WPA_CIPHER_SUITE_WRAP[] = { 0x00, 0x50, 0xf2, 3 };
static const unsigned char WPA_CIPHER_SUITE_CCMP[] = { 0x00, 0x50, 0xf2, 4 };
static const unsigned char WPA_CIPHER_SUITE_WEP104[] = { 0x00, 0x50, 0xf2, 5 };

static const int RSN_SELECTOR_LEN = 4;
static const unsigned int RSN_VERSION_ = 1;
static const unsigned char RSN_AUTH_KEY_MGMT_UNSPEC_802_1X[] = { 0x00, 0x0f, 0xac, 1 };
static const unsigned char RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X[] = { 0x00, 0x0f, 0xac, 2 };
static const unsigned char RSN_CIPHER_SUITE_NONE[] = { 0x00, 0x0f, 0xac, 0 };
static const unsigned char RSN_CIPHER_SUITE_WEP40[] = { 0x00, 0x0f, 0xac, 1 };
static const unsigned char RSN_CIPHER_SUITE_TKIP[] = { 0x00, 0x0f, 0xac, 2 };
static const unsigned char RSN_CIPHER_SUITE_WRAP[] = { 0x00, 0x0f, 0xac, 3 };
static const unsigned char RSN_CIPHER_SUITE_CCMP[] = { 0x00, 0x0f, 0xac, 4 };
static const unsigned char RSN_CIPHER_SUITE_WEP104[] = { 0x00, 0x0f, 0xac, 5 };

struct wpa_ie_data {
	int proto;
	int pairwise_cipher;
	int group_cipher;
	int key_mgmt;
	int capabilities;
	int num_pmkid;
	const unsigned char *pmkid;
};

struct wpa_ie_hdr {
	unsigned char elem_id;
	unsigned char len;
	unsigned char oui[3];
	unsigned char oui_type;
	unsigned char version[2];
} __attribute__ ((packed));

struct rsn_ie_hdr {
	unsigned char elem_id; /* WLAN_EID_RSN */
	unsigned char len;
	unsigned char version[2];
} __attribute__ ((packed));

// Added new types for OFDM 5G and 2.4G
typedef enum _NDIS_802_11_NETWORK_TYPE
{
	Ndis802_11FH,
	Ndis802_11DS,
	Ndis802_11OFDM5,
	Ndis802_11OFDM24,
	Ndis802_11Automode,
	Ndis802_11OFDM5_N,
	Ndis802_11OFDM24_N,
	Ndis802_11NetworkTypeMax    // not a real type, defined as an upper bound
} NDIS_802_11_NETWORK_TYPE;

struct bss_ie_hdr {
	unsigned char elem_id;
	unsigned char len;
	unsigned char oui[3];
} bss_ie;

struct apinfo
{
	char BSSID[18];
	char SSID[33];
	int RSSI_Quality;
	unsigned char channel;
	unsigned char ctl_ch;
	unsigned int capability;
	int wep;
	int wpa;
	struct wpa_ie_data wid;
	int status;
	int NetworkType;
} apinfos[32];

#define WIF "eth1"
char buf[WLC_IOCTL_MAXLEN];
