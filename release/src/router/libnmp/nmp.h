#include <netinet/if_ether.h>

/*+++++ typedefs.h +++++*/
typedef unsigned int	uint32;
typedef unsigned short	uint16;
typedef unsigned char	uint8;
typedef unsigned char	uchar;
typedef signed short	int16;
typedef signed char	int8;
typedef signed int	int32;
typedef	unsigned char	bool;			/* consistent w/BOOL */

#define FALSE	0
#define TRUE	1  /* TRUE */



/*+++++ shutils.h +++++*/
char *ether_etoa(const unsigned char *e, char *a);
extern int _eval(char *const argv[], const char *path, int timeout, pid_t *ppid);

static inline char * strcat_r(const char *s1, const char *s2, char *buf)
{
	strcpy(buf, s1);
	strcat(buf, s2);
	return buf;
}

#define vstrsep(buf, sep, args...) _vstrsep(buf, sep, args, NULL)

#define eval(cmd, args...) ({ \
	char * const argv[] = { cmd, ## args, NULL }; \
	_eval(argv, NULL, 0, NULL); \
})



/*+++++ shared/semaphore_mfp.h +++++*/ 
#define SPINLOCK_SiteSurvey	1
#define SPINLOCK_Networkmap     3
int spinlock_lock(int idx);
int spinlock_unlock(int idx);



/*+++++ shared/shared.h +++++*/
extern int nvram_get_int(const char *key);
extern int killall(const char *name, int sig);
extern void notify_rc(const char *event_name);

#define GIF_LINKLOCAL  0x0001  /* return link-local addr */
#define GIF_PREFIXLEN  0x0002  /* return addr & prefix */

#define RTF_UP		0x0001  /* route usable			*/
#define RTF_GATEWAY     0x0002  /* destination is a gateway	*/
#define RTF_HOST	0x0004  /* host entry (net otherwise)	*/
#define RTF_REINSTATE   0x0008  /* reinstate route after tmout	*/
#define RTF_DYNAMIC     0x0010  /* created dyn. (by redirect)	*/
#define RTF_MODIFIED    0x0020  /* modified dyn. (by redirect)	*/
#define	RTF_DEFAULT	0x00010000	/* default - learned via ND	*/
#define	RTF_ADDRCONF	0x00040000	/* addrconf route - RA		*/
#define	RTF_CACHE	0x01000000	/* cache entry			*/
#define IPV6_MASK (RTF_GATEWAY|RTF_HOST|RTF_DEFAULT|RTF_ADDRCONF|RTF_CACHE)



/*+++++ From bcmnvram.h +++++*/ 
extern char * nvram_get(const char *name);
extern int nvram_set(const char *name, const char *value);
extern int nvram_commit(void);
#define nvram_safe_get(name) (nvram_get(name) ? : "")



/*+++++ router/shared/wlutils.h +++++*/
extern int wl_ioctl(char *name, int cmd, void *buf, int len);



/*+++++ bcmendian.h +++++*/
/* Reverse the bytes in a 32-bit value */
#define BCMSWAP32(val) \
	((uint32)((((uint32)(val) & (uint32)0x000000ffU) << 24) | \
		  (((uint32)(val) & (uint32)0x0000ff00U) <<  8) | \
		  (((uint32)(val) & (uint32)0x00ff0000U) >>  8) | \
		  (((uint32)(val) & (uint32)0xff000000U) >> 24)))

#define bcmswap32(val) ({ \
	uint32 _val = (val); \
	BCMSWAP32(_val); \
})

/* Reverse the bytes in a 16-bit value */
#define BCMSWAP16(val) \
	((uint16)((((uint16)(val) & (uint16)0x00ffU) << 8) | \
		  (((uint16)(val) & (uint16)0xff00U) >> 8)))

#define bcmswap16(val) ({ \
	uint16 _val = (val); \
	BCMSWAP16(_val); \
})



/*+++++ bcmutils.h +++++*/
#define ETHER_ADDR_STR_LEN	18	/* 18-bytes of Ethernet address*/
#define	NBBY	8	/* 8 bits per byte */
#define	isset(a, i)	(((const uint8 *)a)[(i)/NBBY] & (1<<((i)%NBBY)))


/*++++++ shared/misc.c +++++*/
char *wl_nvname(const char *nv, int unit, int subunit);


/*+++++ sysdeps/web-broadcom.c +++++*/
#define WLC_DUMP_BUF_LEN		(32 * 1024)
#define WLC_MAX_AP_SCAN_LIST_LEN	50
#define WLC_SCAN_RETRY_TIMES		5

/* WPS ENR mode APIs */
typedef struct wlc_ap_list_info
{
#if 0
	bool        used;
#endif
	uint8       ssid[33];
	uint8       ssidLen; 
	uint8       BSSID[6];
#if 0
	uint8       *ie_buf;
	uint32      ie_buflen;
#endif
	uint8       channel;
#if 0
	uint8       wep;
	bool	    scstate;
#endif
} wlc_ap_list_info_t;



/*+++++ shared/wlscan.h +++++*/
#define BIT(n) (1 << (n))
#define WPA_GET_LE16(a) ((unsigned int) (((a)[1] << 8) | (a)[0]))

#define PMKID_LEN 16

#define NUMCHANS 64

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

struct wpa_ie_data {
	int proto;
	int pairwise_cipher;
	int group_cipher;
	int key_mgmt;
	int capabilities;
	int num_pmkid;
	const unsigned char *pmkid;
};

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
};



/*+++++ shared/ethutils.h +++++*/
static const struct ether_addr ether_bcast = {{255, 255, 255, 255, 255, 255}};



/*+++++ bcmwifi.h +++++*/
/* A chanspec holds the channel number, band, bandwidth and control sideband */
typedef uint16 chanspec_t;

/* channel defines */
#define CH_10MHZ_APART			2
#define CH_MAX_2G_CHANNEL		14	/* Max channel in 2G band */

#define WL_CHANSPEC_BW_10		0x0400
#define WL_CHANSPEC_BW_20		0x0800
#define WL_CHANSPEC_BW_40		0x0C00
#define WL_CHANSPEC_BAND_5G		0x1000
#define WL_CHANSPEC_BAND_2G		0x2000
#define WL_CHANSPEC_BW_MASK		0x0C00
#define WL_CHANSPEC_CTL_SB_NONE		0x0300
#define CH20MHZ_CHSPEC(channel)	(chanspec_t)((chanspec_t)(channel) | WL_CHANSPEC_BW_20 | \
				WL_CHANSPEC_CTL_SB_NONE | (((channel) <= CH_MAX_2G_CHANNEL) ? \
				WL_CHANSPEC_BAND_2G : WL_CHANSPEC_BAND_5G))
#define WL_CHANSPEC_CHAN_MASK		0x00ff
#define WL_CHANSPEC_BAND_MASK		0xf000

/* simple MACROs to get different fields of chanspec */
#define CHSPEC_CHANNEL(chspec)	((uint8)((chspec) & WL_CHANSPEC_CHAN_MASK))
#define CHSPEC_IS10(chspec)	(((chspec) & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_10)
#define CHSPEC_IS20(chspec)	(((chspec) & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_20)
#define CHSPEC_IS40(chspec)	(((chspec) & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_40)
#define CHSPEC_IS5G(chspec)	(((chspec) & WL_CHANSPEC_BAND_MASK) == WL_CHANSPEC_BAND_5G)
#define CHSPEC_IS2G(chspec)	(((chspec) & WL_CHANSPEC_BAND_MASK) == WL_CHANSPEC_BAND_2G)

#define WL_CHANSPEC_CTL_SB_MASK		0x0300
#define WL_CHANSPEC_CTL_SB_UPPER	0x0200
#define CHSPEC_SB_UPPER(chspec)	(((chspec) & WL_CHANSPEC_CTL_SB_MASK) == WL_CHANSPEC_CTL_SB_UPPER)

#define CHANSPEC_STR_LEN    8



//From 802.11.h
#define DOT11_MNG_RSN_ID			48	/* d11 management RSN id */
#define DOT11_MNG_WPA_ID			221	/* d11 management WPA id */

/* Capability Information Field */
#define DOT11_CAP_ESS				0x0001	/* d11 cap. ESS */
#define DOT11_CAP_IBSS				0x0002	/* d11 cap. IBSS */
#define DOT11_CAP_POLLABLE			0x0004	/* d11 cap. pollable */
#define DOT11_CAP_POLL_RQ			0x0008	/* d11 cap. poll request */
#define DOT11_CAP_PRIVACY			0x0010	/* d11 cap. privacy */
#define DOT11_CAP_SHORT				0x0020	/* d11 cap. short */
#define DOT11_CAP_PBCC				0x0040	/* d11 cap. PBCC */
#define DOT11_CAP_AGILITY			0x0080	/* d11 cap. agility */
#define DOT11_CAP_SPECTRUM			0x0100	/* d11 cap. spectrum */
#define DOT11_CAP_SHORTSLOT			0x0400	/* d11 cap. shortslot */
#define DOT11_CAP_RRM			0x1000	/* d11 cap. 11k radio measurement */
#define DOT11_CAP_CCK_OFDM			0x2000	/* d11 cap. CCK/OFDM */

#define HT_CAP_40MHZ		0x0002  /* FALSE:20Mhz, TRUE:20/40MHZ supported */
#define HT_CAP_SHORT_GI_20	0x0020	/* 20MHZ short guard interval support */
#define HT_CAP_SHORT_GI_40	0x0040	/* 40Mhz short guard interval support */

/* MLME Enumerations */
#define DOT11_BSSTYPE_INFRASTRUCTURE		0	/* d11 infrastructure */
#define DOT11_BSSTYPE_INDEPENDENT		1	/* d11 independent */
#define DOT11_BSSTYPE_ANY			2	/* d11 any BSS type */
#define DOT11_SCANTYPE_ACTIVE			0	/* d11 scan active */
#define DOT11_SCANTYPE_PASSIVE			1	/* d11 scan passive */

/* ************* HT definitions. ************* */
#define MCSSET_LEN	16	/* 16-bits per 8-bit set to give 128-bits bitmap of MCS Index */
#define MAX_MCS_NUM	(128)	/* max mcs number = 128 */



/*+++++ wlioctl.h +++++*/
/* size of wl_scan_params not including variable length array */
#define WL_SCAN_PARAMS_FIXED_SIZE 64

#define	WLC_IOCTL_MAXLEN		8192	/* max length ioctl buffer required */

/* band types */
#define	WLC_BAND_AUTO		0	/* auto-select */
#define	WLC_BAND_5G		1	/* 5 Ghz */
#define	WLC_BAND_2G		2	/* 2.4 Ghz */
#define	WLC_BAND_ALL		3	/* all bands */

#define WLC_CNTRY_BUF_SZ	4		/* Country string is 3 bytes + NUL */

typedef struct wl_channels_in_country {
	uint32 buflen;
	uint32 band;
	char country_abbrev[WLC_CNTRY_BUF_SZ];
	uint32 count;
	uint32 channel[1];
} wl_channels_in_country_t;

#define WLC_GET_BSSID				23
#define WLC_GET_SSID				25
#define WLC_GET_RADIO				37
#define WLC_SCAN				50
#define WLC_SCAN_RESULTS			51
#define WLC_GET_RSSI				127
#define WLC_GET_BSS_INFO			136
#define WLC_GET_ASSOCLIST			159
#define WLC_GET_SCAN_CHANNEL_TIME		184
#define WLC_SET_SCAN_CHANNEL_TIME		185
#define WLC_GET_CHANNELS_IN_COUNTRY		260
#define WLC_GET_VAR				262	/* get value of named variable */

/* Bit masks for radio disabled status - returned by WL_GET_RADIO */
#define WL_RADIO_SW_DISABLE		(1<<0)
#define WL_RADIO_HW_DISABLE		(1<<1)

#define WL_BSS_FLAGS_FROM_BEACON	0x01	/* bss_info derived from beacon */
#define WL_BSS_FLAGS_FROM_CACHE		0x02	/* bss_info collected from cache */
#define WL_BSS_FLAGS_RSSI_ONCHANNEL     0x04 /* rssi info was received on channel (vs offchannel) */

/* For ioctls that take a list of MAC addresses */
struct maclist {
	uint count;			/* number of MAC addresses */
	struct ether_addr ea[1];	/* variable length array of MAC addresses */
};

#define	LEGACY_WL_BSS_INFO_VERSION	107	/* older version of wl_bss_info struct */

typedef struct wl_bss_info_107 {
	uint32		version;		/* version field */
	uint32		length;			/* byte length of data in this record,
						 * starting at version and including IEs
						 */
	struct ether_addr BSSID;
	uint16		beacon_period;		/* units are Kusec */
	uint16		capability;		/* Capability information */
	uint8		SSID_len;
	uint8		SSID[32];
	struct {
		uint	count;			/* # rates in this set */
		uint8	rates[16];		/* rates in 500kbps units w/hi bit set if basic */
	} rateset;				/* supported rates */
	uint8		channel;		/* Channel no. */
	uint16		atim_window;		/* units are Kusec */
	uint8		dtim_period;		/* DTIM period */
	int16		RSSI;			/* receive signal strength (in dBm) */
	int8		phy_noise;		/* noise (in dBm) */
	uint32		ie_length;		/* byte length of Information Elements */
	/* variable length Information Elements */
} wl_bss_info_107_t;

#define	LEGACY2_WL_BSS_INFO_VERSION	108		/* old version of wl_bss_info struct */

#define	WL_BSS_INFO_VERSION	109		/* current version of wl_bss_info struct */

/* BSS info structure
 * Applications MUST CHECK ie_offset field and length field to access IEs and
 * next bss_info structure in a vector (in wl_scan_results_t)
 */
typedef struct wl_bss_info {
	uint32		version;		/* version field */
	uint32		length;			/* byte length of data in this record,
						 * starting at version and including IEs
						 */
	struct ether_addr BSSID;
	uint16		beacon_period;		/* units are Kusec */
	uint16		capability;		/* Capability information */
	uint8		SSID_len;
	uint8		SSID[32];
	struct {
		uint	count;			/* # rates in this set */
		uint8	rates[16];		/* rates in 500kbps units w/hi bit set if basic */
	} rateset;				/* supported rates */
	chanspec_t	chanspec;		/* chanspec for bss */
	uint16		atim_window;		/* units are Kusec */
	uint8		dtim_period;		/* DTIM period */
	int16		RSSI;			/* receive signal strength (in dBm) */
	int8		phy_noise;		/* noise (in dBm) */

	uint8		n_cap;			/* BSS is 802.11N Capable */
	uint32		nbss_cap;		/* 802.11N BSS Capabilities (based on HT_CAP_*) */
	uint8		ctl_ch;			/* 802.11N BSS control channel number */
	uint32		reserved32[1];		/* Reserved for expansion of BSS properties */
	uint8		flags;			/* flags */
	uint8		reserved[3];		/* Reserved for expansion of BSS properties */
	uint8		basic_mcs[MCSSET_LEN];	/* 802.11N BSS required MCS set */

	uint16		ie_offset;		/* offset at which IEs start, from beginning */
	uint32		ie_length;		/* byte length of Information Elements */
	int16		SNR;			/* average SNR of during frame reception */
	/* Add new fields here */
	/* variable length Information Elements */
} wl_bss_info_t;

#define WL_CHANSPEC_CHAN_MASK		0x00ff

typedef struct wl_scan_results {
	uint32 buflen;
	uint32 version;
	uint32 count;
	wl_bss_info_t bss_info[1];
} wl_scan_results_t;

typedef struct wlc_ssid {
	uint32		SSID_len;
	uchar		SSID[32];
} wlc_ssid_t;

typedef struct wl_scan_params {
	wlc_ssid_t ssid;		/* default: {0, ""} */
	struct ether_addr bssid;	/* default: bcast */
	int8 bss_type;			/* default: any,
					 * DOT11_BSSTYPE_ANY/INFRASTRUCTURE/INDEPENDENT
					 */
	uint8 scan_type;		/* flags, 0 use default */
	int32 nprobes;			/* -1 use default, number of probes per channel */
	int32 active_time;		/* -1 use default, dwell time per channel for
					 * active scanning
					 */
	int32 passive_time;		/* -1 use default, dwell time per channel
					 * for passive scanning
					 */
	int32 home_time;		/* -1 use default, dwell time for the home channel
					 * between channel scans
					 */
	int32 channel_num;		/* count of channels and ssids that follow
					 *
					 * low half is count of channels in channel_list, 0
					 * means default (use all available channels)
					 *
					 * high half is entries in wlc_ssid_t array that
					 * follows channel_list, aligned for int32 (4 bytes)
					 * meaning an odd channel count implies a 2-byte pad
					 * between end of channel_list and first ssid
					 *
					 * if ssid count is zero, single ssid in the fixed
					 * parameter portion is assumed, otherwise ssid in
					 * the fixed portion is ignored
					 */
	uint16 channel_list[1];		/* list of chanspecs */
} wl_scan_params_t;



/*+++++ networkmap.h +++++*/
//Device service info data structure
typedef struct {
        unsigned char   ip_addr[255][4];
        unsigned char   mac_addr[255][6];
	unsigned char   device_name[255][16];
        int             type[255];
        int             http[255];
        int             printer[255];
        int             itune[255];
        int             ip_mac_num;
	int 		detail_info_num;
} CLIENT_DETAIL_INFO_TABLE, *P_CLIENT_DETAIL_INFO_TABLE;



/*+++++ rtstate.h +++++*/
enum {
	SW_MODE_NONE=0,
	SW_MODE_ROUTER,
	SW_MODE_REPEATER,
	SW_MODE_AP,
	SW_MODE_HOTSPOT
};

enum {
	WAN_UNIT_FIRST=0,
	WAN_UNIT_SECOND,
	WAN_UNIT_MAX
};



/*+++++ Other +++++*/
extern char *nmp_get(const char *name);
extern int nmp_get_int(const char *key);
extern int nmp_set(const char *name, const char *value);
extern int nmp_set_int(const char *key, int value);
#define nmp_safe_get(name) (nmp_get(name) ? : NULL)
extern int channels_in_country(int unit, int channels[]);
extern void generateWepKey(const char *unit, unsigned char *passphrase);

typedef struct client_list_info
{
        unsigned char ip_addr[4];
        unsigned char mac_addr[6];
	unsigned char device_name[16];
} client_list_info_t;

typedef struct dhcp_lease_info
{
    char expire[10];
    char mac_addr[18];
	char ip_addr[20];
	char host_name[16];
} dhcp_lease_info_t;

typedef struct port_forwarding_info
{
        char dest[20];
        char protocol[8];
	char port_range[12];
	char redirect_to[20];
	char local_port[12]; 
} port_forwarding_info_t;

typedef struct routing_info
{
	char dest[20];
 	char gateway[20];
	char genmask[20];
	char flags[8];
	int metric;
	int ref;
	int use;
	char iface[8]; 
} routing_info_t;

/* Cherry Cho added and modified in 2014/5/6. */

#define wan_prefix(unit, prefix)	snprintf(prefix, sizeof(prefix), "wan%d_", unit)

typedef struct active_connection_info
{
    char protocol[8];
    char nated_addr[22];
    char dest[22];
	char state[16]; 
} active_connection_info_t;

#define WL_MAXRATES_IN_SET		16	/* max # of rates in a rateset */
typedef struct wl_rateset {
	uint32	count;			/* # rates in this set */
	uint8	rates[WL_MAXRATES_IN_SET];	/* rates in 500kbps units w/hi bit set if basic */
} wl_rateset_t;

#define WL_STA_VER			3
#define WL_STA_PS			0x100		/* STA is in power save mode from AP's viewpoint */
#define WL_STA_SCBSTATS		0x4000		/* Per STA debug stats */

typedef struct {
	uint16			ver;		/* version of this struct */
	uint16			len;		/* length in bytes of this structure */
	uint16			cap;		/* sta's advertised capabilities */
	uint32			flags;		/* flags defined below */
	uint32			idle;		/* time since data pkt rx'd from sta */
	struct ether_addr	ea;		/* Station address */
	wl_rateset_t		rateset;	/* rateset in use */
	uint32			in;		/* seconds elapsed since associated */
	uint32			listen_interval_inms; /* Min Listen interval in ms for this STA */
	uint32			tx_pkts;	/* # of packets transmitted */
	uint32			tx_failures;	/* # of packets failed */
	uint32			rx_ucast_pkts;	/* # of unicast packets received */
	uint32			rx_mcast_pkts;	/* # of multicast packets received */
	uint32			tx_rate;	/* Rate of last successful tx frame */
	uint32			rx_rate;	/* Rate of last successful rx frame */
	uint32			rx_decrypt_succeeds;	/* # of packet decrypted successfully */
	uint32			rx_decrypt_failures;	/* # of packet decrypted unsuccessfully */
} sta_info_t;

/* Used to get specific STA parameters */
typedef struct {
	uint32	val;
	struct ether_addr ea;
} scb_val_t;



typedef struct wireless_info
{
	char mac_addr[18];
	int status;
	char rssi[16];
	int psm; /* 1: yes 0: no*/
	int tx_rate;
	int rx_rate;
	int connect_time; /* seconds */
} wireless_info_t;

typedef struct ipv6_landev_info
{
	char hostname[65];
	char mac_addr[33];
	char ipv6_addrs[8193];
} ipv6_landev_info_t;

typedef struct ipv6_routing_info
{
	char dest[44];
 	char next_hop[40];
	char flags[8];
	int metric;
	int ref;
	int use;
	char iface[8]; 
} ipv6_routing_info_t;

typedef struct pc_list_info
{
	int enable;
	char device_name[33];
	char mac[18];
	char sunday[128];
	char monday[128];
	char tuesday[128];
	char wednesday[128];	
	char thursday[128];
	char friday[128];
	char saturday[128];
} pc_list_info_t;
/*---*/

#if 0
typedef struct 11n_capable
{
        char ssid[33];
	int rssi;
	int snr;
	int noise;
	char flags[40];
	char channel[8];
	char bssid[16];
	char cap[100];
	char sup_rates[128];
	
} 11n_capable_t;
#endif

/*typedef struct bss_info
{
        char ssid[33];
	int rssi;
	int snr;
	int noise;
	char flags[40];
	char channel[8];
	char bssid[16];
	char bss_cap[100];
	char sup_rates[128];
	//struct 11n_capable_t cap;
} bss_info_t;
*/

typedef struct ap_info
{
	char band[10];
	char ssid[33];
	int channel;
	char auth[12];
	char enc[9];
	int rssi;
	char bssid[18];	
	char mode[4];
	int status;
} ap_info_t;

/* For OpenVPN Settings. */
#include <stdbool.h>

#define	VPN_UPLOAD_DONE			0
#define	VPN_UPLOAD_NEED_CA_CERT		1
#define	VPN_UPLOAD_NEED_CERT		2
#define	VPN_UPLOAD_NEED_KEY		4
#define	VPN_UPLOAD_NEED_STATIC		8
#define MAX_PARMS 16

#define OPTION_PARM_SIZE 256
#define OPTION_LINE_SIZE 256

#define BUF_SIZE_MAX 1000000

#define BOOL_CAST(x) ((x) ? (true) : (false))

#define INLINE_FILE_TAG "[[INLINE]]"

/* size of an array */
#define SIZE(x) (sizeof(x)/sizeof(x[0]))

/* clear an object */
#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define streq(x, y) (!strcmp((x), (y)))
#define likely(x)       __builtin_expect((x),1)

struct buffer
{
	int capacity;	//Size in bytes of memory allocated by malloc().
	int offset;	//Offset in bytes of the actual content within the allocated memory.
	int len;	//Length in bytes of the actual content within the allocated memory.
	uint8_t *data;	//Pointer to the allocated memory.
};


struct in_src {
# define IS_TYPE_FP 1
# define IS_TYPE_BUF 2
	int type;
	union {
		FILE *fp;
		struct buffer *multiline;
	} u;
};


static inline bool
buf_defined (const struct buffer *buf)
{
	return buf->data != NULL;
}

static inline bool
buf_valid (const struct buffer *buf)
{
	return likely (buf->data != NULL) && likely (buf->len >= 0);
}

static inline uint8_t *
buf_bptr (const struct buffer *buf)
{
	if (buf_valid (buf))
		return buf->data + buf->offset;
	else
		return NULL;
}

static int
buf_len (const struct buffer *buf)
{
	if (buf_valid (buf))
		return buf->len;
	else
		return 0;
}

static inline uint8_t *
buf_bend (const struct buffer *buf)
{
	return buf_bptr (buf) + buf_len (buf);
}

static inline bool
buf_size_valid (const size_t size)
{
	return likely (size < BUF_SIZE_MAX);
}

static inline char *
buf_str (const struct buffer *buf)
{
	return (char *)buf_bptr(buf);
}

static inline int
buf_forward_capacity (const struct buffer *buf)
{
	if (buf_valid (buf))
	{
		int ret = buf->capacity - (buf->offset + buf->len);
		if (ret < 0)
			ret = 0;
		return ret;
	}
	else
		return 0;
}

static inline bool
buf_advance (struct buffer *buf, int size)
{
	if (!buf_valid (buf) || size < 0 || buf->len < size)
		return false;
	buf->offset += size;
	buf->len -= size;
	return true;
}

static inline int
buf_read_u8 (struct buffer *buf)
{
	int ret;
	if (buf_len (buf) < 1)
		return -1;
	ret = *buf_bptr(buf);
		buf_advance (buf, 1);
	return ret;
}
