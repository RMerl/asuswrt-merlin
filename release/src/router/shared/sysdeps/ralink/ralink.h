/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef RTXXXXH
#define RTXXXXH

#include <iwlib.h>


#define WIF	"ra0"
extern const char WIF_2G[];
extern const char WIF_5G[];
extern const char WDSIF_5G[];
extern const char APCLI_5G[];
extern const char APCLI_2G[];
#define URE	"apcli0"

#ifndef ETHER_ADDR_LEN
#define ETHER_ADDR_LEN		6
#endif
#define MAX_NUMBER_OF_MAC	64

#define MODE_CCK		0
#define MODE_OFDM		1
#define MODE_HTMIX		2
#define MODE_HTGREENFIELD	3
#define MODE_VHT		4

#define BW_20			0
#define BW_40			1
#define BW_BOTH			2
#define BW_80			2
#define BW_10			3

#define INIC_VLAN_ID_START	4 //first vlan id used for RT3352 iNIC MII
#define INIC_VLAN_IDX_START	2 //first available index to set vlan id and its group.

#if defined(RTCONFIG_WLMODULE_MT7610_AP)
#define RT_802_11_MAC_ENTRY_for_5G		RT_802_11_MAC_ENTRY_11AC
#define MACHTTRANSMIT_SETTING_for_5G		MACHTTRANSMIT_SETTING_11AC
#else
#define RT_802_11_MAC_ENTRY_for_5G		RT_802_11_MAC_ENTRY_RT3883
#define MACHTTRANSMIT_SETTING_for_5G		MACHTTRANSMIT_SETTING_2G
#endif

#if defined(RTN65U)
#define RT_802_11_MAC_ENTRY_for_2G		RT_802_11_MAC_ENTRY_RT3352_iNIC
#elif defined(RTN56UB1)
#define RT_802_11_MAC_ENTRY_for_2G		RT_802_11_MAC_ENTRY_7603E
#else
#define RT_802_11_MAC_ENTRY_for_2G		RT_802_11_MAC_ENTRY_2G
#endif
#define MACHTTRANSMIT_SETTING_for_2G		MACHTTRANSMIT_SETTING_2G

// MIMO Tx parameter, ShortGI, MCS, STBC, etc.  these are fields in TXWI. Don't change this definition!!!
typedef union  _MACHTTRANSMIT_SETTING {
	struct  {
	unsigned short	MCS:7;	// MCS
	unsigned short	BW:1;	//channel bandwidth 20MHz or 40 MHz
	unsigned short	ShortGI:1;
	unsigned short	STBC:2;	//SPACE
	unsigned short  eTxBF:1;
	unsigned short  rsv:1;
	unsigned short  iTxBF:1;
	unsigned short  MODE:2;	// Use definition MODE_xxx.
	} field;
	unsigned short	word;
 } MACHTTRANSMIT_SETTING, *PMACHTTRANSMIT_SETTING;

// MIMO Tx parameter, ShortGI, MCS, STBC, etc.  these are fields in TXWI. Don't change this definition!!!
typedef union  _MACHTTRANSMIT_SETTING_2G {
	struct  {
	unsigned short	MCS:7;	// MCS
	unsigned short	BW:1;	//channel bandwidth 20MHz or 40 MHz
	unsigned short	ShortGI:1;
	unsigned short	STBC:2;	//SPACE
	unsigned short	rsv:3;
	unsigned short	MODE:2;	// Use definition MODE_xxx.
	} field;
	unsigned short	word;
 } MACHTTRANSMIT_SETTING_2G, *PMACHTTRANSMIT_SETTING_2G;

typedef union  _MACHTTRANSMIT_SETTING_11AC {
	struct  {
	unsigned short	MCS:7;	// MCS
	unsigned short	BW:2;	//channel bandwidth 20MHz or 40 MHz
	unsigned short	ShortGI:1;
	unsigned short	STBC:1;	//SPACE
	unsigned short	eTxBF:1;
	unsigned short	iTxBF:1;
	unsigned short	MODE:3;	// Use definition MODE_xxx.
	} field;
	unsigned short	word;
 } MACHTTRANSMIT_SETTING_11AC, *PMACHTTRANSMIT_SETTING_11AC;

typedef struct _RT_802_11_MAC_ENTRY_RT3352_iNIC {
    unsigned char	ApIdx;
    unsigned char	Addr[ETHER_ADDR_LEN];
    unsigned char	Aid;
    unsigned char	Psm;	// 0:PWR_ACTIVE, 1:PWR_SAVE
    unsigned char	MimoPs;	// 0:MMPS_STATIC, 1:MMPS_DYNAMIC, 3:MMPS_Enabled
    char		AvgRssi0;
    char		AvgRssi1;
    char		AvgRssi2;
    unsigned int	ConnectedTime;
    MACHTTRANSMIT_SETTING_2G	TxRate;
    MACHTTRANSMIT_SETTING_2G	MaxTxRate;
} RT_802_11_MAC_ENTRY_RT3352_iNIC, *PRT_802_11_MAC_ENTRY_RT3352_iNIC;

typedef struct _RT_802_11_MAC_ENTRY_RT3883 {
    unsigned char	ApIdx;
    unsigned char	Addr[ETHER_ADDR_LEN];
    unsigned char	Aid;
    unsigned char	Psm;     // 0:PWR_ACTIVE, 1:PWR_SAVE
    unsigned char	MimoPs;  // 0:MMPS_STATIC, 1:MMPS_DYNAMIC, 3:MMPS_Enabled
    char		AvgRssi0;
    char		AvgRssi1;
    char		AvgRssi2;
    unsigned int	ConnectedTime;
    MACHTTRANSMIT_SETTING_2G       TxRate;
    unsigned int	LastRxRate;
    short		StreamSnr[3];
    short		SoundingRespSnr[3];
} RT_802_11_MAC_ENTRY_RT3883, *PRT_802_11_MAC_ENTRY_RT3883;

typedef struct _RT_802_11_MAC_ENTRY {
    unsigned char	ApIdx;
    unsigned char	Addr[ETHER_ADDR_LEN];
    unsigned char	Aid;
    unsigned char	Psm;     // 0:PWR_ACTIVE, 1:PWR_SAVE
    unsigned char	MimoPs;  // 0:MMPS_STATIC, 1:MMPS_DYNAMIC, 3:MMPS_Enabled
    char		AvgRssi0;
    char		AvgRssi1;
    char		AvgRssi2;
    unsigned int	ConnectedTime;
    MACHTTRANSMIT_SETTING       TxRate;
    unsigned int	LastRxRate;
    int			StreamSnr[3];
    int			SoundingRespSnr[3];
} RT_802_11_MAC_ENTRY, *PRT_802_11_MAC_ENTRY;

typedef struct _RT_802_11_MAC_ENTRY_2G {
    unsigned char	ApIdx;
    unsigned char	Addr[ETHER_ADDR_LEN];
    unsigned char	Aid;
    unsigned char	Psm;	/* 0:PWR_ACTIVE, 1:PWR_SAVE */
    unsigned char	MimoPs;	/* 0:MMPS_STATIC, 1:MMPS_DYNAMIC, 3:MMPS_Enabled */
    char		AvgRssi0;
    char		AvgRssi1;
    char		AvgRssi2;
    unsigned int	ConnectedTime;
    MACHTTRANSMIT_SETTING_2G	TxRate;
    unsigned int	LastRxRate;
    short		StreamSnr[3];	/* BF SNR from RXWI. Units=0.25 dB. 22 dB offset removed */
    short		SoundingRespSnr[3];
} RT_802_11_MAC_ENTRY_2G, *PRT_802_11_MAC_ENTRY_2G;

typedef struct _RT_802_11_MAC_ENTRY_7603E {
    unsigned char  ApIdx;
    unsigned char Addr[ETHER_ADDR_LEN];
    unsigned char Aid;
    unsigned char Psm;              /* 0:PWR_ACTIVE, 1:PWR_SAVE */
    unsigned char MimoPs;           /* 0:MMPS_STATIC, 1:MMPS_DYNAMIC, 3:MMPS_Enabled */
    char AvgRssi0;
    char AvgRssi1;
    char AvgRssi2;
    unsigned int ConnectedTime;
    MACHTTRANSMIT_SETTING_2G    TxRate;
    unsigned int LastRxRate;
} RT_802_11_MAC_ENTRY_7603E, *PRT_802_11_MAC_ENTRY_7603E;


typedef struct _RT_802_11_MAC_ENTRY_11AC {
    unsigned char	ApIdx;
    unsigned char	Addr[ETHER_ADDR_LEN];
    unsigned char	Aid;
    unsigned char	Psm;	/* 0:PWR_ACTIVE, 1:PWR_SAVE */
    unsigned char	MimoPs;	/* 0:MMPS_STATIC, 1:MMPS_DYNAMIC, 3:MMPS_Enabled */
    char		AvgRssi0;
    char		AvgRssi1;
    char		AvgRssi2;
    unsigned int	ConnectedTime;
    MACHTTRANSMIT_SETTING_11AC	TxRate;
    unsigned int	LastRxRate;
    short		StreamSnr[3];	/* BF SNR from RXWI. Units=0.25 dB. 22 dB offset removed */
    short		SoundingRespSnr[3];
} RT_802_11_MAC_ENTRY_11AC, *PRT_802_11_MAC_ENTRY_11AC;

typedef struct _RT_802_11_MAC_TABLE_5G {
    unsigned long	Num;
    RT_802_11_MAC_ENTRY_for_5G	Entry[MAX_NUMBER_OF_MAC];
} RT_802_11_MAC_TABLE_5G, *PRT_802_11_MAC_TABLE_5G;

typedef struct _RT_802_11_MAC_TABLE_2G {
    unsigned long	Num;
    RT_802_11_MAC_ENTRY_for_2G	Entry[MAX_NUMBER_OF_MAC];
} RT_802_11_MAC_TABLE_2G, *PRT_802_11_MAC_TABLE_2G;

typedef struct _SITE_SURVEY_RT3352_iNIC
{
	char channel[4];
	unsigned char ssid[33];
	char bssid[20];
	char authmode[15];	//security part1
	char encryption[8];	//security part2 and need to shift data
	char signal[9];
	char wmode[8];
	char extch[7];
	char nt[3];
	char wps[4];
	char dpid[4];
	char newline;
} SITE_SURVEY_RT3352_iNIC;

typedef struct _SITE_SURVEY 
{ 
	char channel[4];
//	unsigned char channel;
//	unsigned char centralchannel;
//	unsigned char unused;
	unsigned char ssid[33]; 
	char bssid[18];
	char encryption[9];
	char authmode[16];
	char signal[9];
	char wmode[8];
#if 0//defined(RTN14U)
	char wps[4];
	char dpid[5];
#endif
//	char bsstype[3];
//	char centralchannel[3];
} SITE_SURVEY;

typedef struct _SITE_SURVEY_ARRAY
{ 
	SITE_SURVEY SiteSurvey[64];
} SSA;

#define SITE_SURVEY_APS_MAX	(16*1024)

//#if WIRELESS_EXT <= 11 
//#ifndef SIOCDEVPRIVATE 
//#define SIOCDEVPRIVATE 0x8BE0 
//#endif 
//#define SIOCIWFIRSTPRIV SIOCDEVPRIVATE 
//#endif 
//
//SET/GET CONVENTION :
// * ------------------
// * Simplistic summary :
// * o even numbered ioctls are SET, restricted to root, and should not
// * return arguments (get_args = 0).
// * o odd numbered ioctls are GET, authorised to anybody, and should
// * not expect any arguments (set_args = 0).
//
#define RT_PRIV_IOCTL			(SIOCIWFIRSTPRIV + 0x01)
#define RTPRIV_IOCTL_SET		(SIOCIWFIRSTPRIV + 0x02)
#define RTPRIV_IOCTL_GSITESURVEY	(SIOCIWFIRSTPRIV + 0x0D)
#define RTPRIV_IOCTL_GET_MAC_TABLE	(SIOCIWFIRSTPRIV + 0x0F) //used by rt2860v2
#define	RTPRIV_IOCTL_SHOW		(SIOCIWFIRSTPRIV + 0x11)
#define RTPRIV_IOCTL_WSC_PROFILE	(SIOCIWFIRSTPRIV + 0x12)
#define RTPRIV_IOCTL_SWITCH		(SIOCIWFIRSTPRIV + 0x1D) //used by iNIC_RT3352 on RTN65U
#define RTPRIV_IOCTL_ASUSCMD		(SIOCIWFIRSTPRIV + 0x1E)
#define RTPRIV_IOCTL_GET_MAC_TABLE_STRUCT	(SIOCIWFIRSTPRIV + 0x1F)	/* RT3352 iNIC driver doesn't support this ioctl. */
#define OID_802_11_DISASSOCIATE		0x0114
#define OID_802_11_BSSID_LIST_SCAN	0x0508
#define OID_802_11_SSID			0x0509
#define OID_802_11_BSSID		0x050A
#define RT_OID_802_11_RADIO		0x050B
#define OID_802_11_BSSID_LIST		0x0609
#define OID_802_3_CURRENT_ADDRESS	0x060A
#define OID_GEN_MEDIA_CONNECT_STATUS	0x060B
#define RT_OID_GET_PHY_MODE		0x0761
#define OID_GET_SET_TOGGLE		0x8000
#define RT_OID_SYNC_RT61		0x0D010750
#define RT_OID_WSC_QUERY_STATUS		((RT_OID_SYNC_RT61 + 0x01) & 0xffff)
#define RT_OID_WSC_PIN_CODE		((RT_OID_SYNC_RT61 + 0x02) & 0xffff)

enum ASUS_IOCTL_SUBCMD {
	ASUS_SUBCMD_UNKNOWN = 0,
	ASUS_SUBCMD_GSTAINFO,
	ASUS_SUBCMD_GSTAT,
	ASUS_SUBCMD_GRSSI,
	ASUS_SUBCMD_RADIO_STATUS,
	ASUS_SUBCMD_CHLIST,
	ASUS_SUBCMD_GROAM,
	ASUS_SUBCMD_CONN_STATUS,
	ASUS_SUBCMD_MAX
};


#if 0
typedef enum _RT_802_11_PHY_MODE {
	PHY_11BG_MIXED = 0,
	PHY_11B,
	PHY_11A,
	PHY_11ABG_MIXED,
	PHY_11G,
	PHY_11ABGN_MIXED,	// both band		5
	PHY_11N,		//			6
	PHY_11GN_MIXED,		// 2.4G band		7
	PHY_11AN_MIXED,		// 5G  band		8
	PHY_11BGN_MIXED,	// if check 802.11b.	9
	PHY_11AGN_MIXED,	// if check 802.11b.	10
} RT_802_11_PHY_MODE;
#endif

#define SPI_PARALLEL_NOR_FLASH_FACTORY_LENGTH	0x10000
/* Below offset addresses, actually, based on start address of Flash.
 * Thus, there are absolute addresses and only valid on products
 * associated with parallel NOR Flash and SPI Flash.
 */
#define OFFSET_MTD_FACTORY	0x40000
#define OFFSET_EEPROM_VER	0x40002
#define OFFSET_BOOT_VER		0x4018A
#define OFFSET_COUNTRY_CODE	0x40188
#if defined(RTN14U) || defined(RTN11P) || defined(RTN300)
#define OFFSET_MAC_ADDR		0x40004
#define OFFSET_MAC_ADDR_2G	0x40004 //only one MAC
#define OFFSET_MAC_GMAC2	0x4018E
#define OFFSET_MAC_GMAC0	0x40194
#elif defined(RTAC52U) || defined(RTAC51U) || defined(RTN54U) || defined(RTAC54U) || defined(RTAC1200HP) || defined(RTN56UB1)
#define OFFSET_MAC_ADDR_2G	0x40004
#define OFFSET_MAC_ADDR		0x48004
#define OFFSET_MAC_GMAC0	0x40022
#define OFFSET_MAC_GMAC2	0x40028
#else
#define OFFSET_MAC_ADDR		0x40004
#define OFFSET_MAC_ADDR_2G	0x48004
#define OFFSET_MAC_GMAC2	0x40022
#define OFFSET_MAC_GMAC0	0x40028
#endif
#if defined(RTAC1200HP) || defined(RTN56UB1)
#define OFFSET_FIX_CHANNEL      0x40170
#endif
#define OFFSET_PIN_CODE		0x40180
#define OFFSET_TXBF_PARA	0x401A0

#if defined(RTCONFIG_NEW_REGULATION_DOMAIN)
#define	MAX_REGDOMAIN_LEN	10
#define	MAX_REGSPEC_LEN		4
#define REG2G_EEPROM_ADDR	0x40234 //10 bytes
#define REG5G_EEPROM_ADDR	0x4023E //10 bytes
#define REGSPEC_ADDR		0x40248 // 4 bytes
#endif /* RTCONFIG_NEW_REGULATION_DOMAIN */

#define OFFSET_PSK		0x4ff80 //15bytes
#define OFFSET_TERRITORY_CODE	0x4ff90	/* 5 bytes, e.g., US/01, US/02, TW/01, etc. */
#define OFFSET_DEV_FLAGS	0x4ffa0 //device dependent flags
#define OFFSET_ODMPID		0x4ffb0 //the shown model name (for Bestbuy and others)
#define OFFSET_FAIL_RET		0x4ffc0
#define OFFSET_FAIL_BOOT_LOG	0x4ffd0	//bit operation for max 100
#define OFFSET_FAIL_DEV_LOG	0x4ffe0	//bit operation for max 100
#define OFFSET_SERIAL_NUMBER	0x4fff0

#define OFFSET_POWER_5G_TX0_36_x6	0x40096
#define OFFSET_POWER_5G_TX1_36_x6	0x400CA
#define OFFSET_POWER_5G_TX2_36_x6	0x400FE
#define OFFSET_POWER_2G		0x480DE


#define RA_LED_ON		0	// low active (all 5xx series)
#define RA_LED_OFF		1

#define	RA_LED_POWER		0
#define	RA_LED_USB		24
#ifdef	RTCONFIG_N56U_SR2
#define	RA_BTN_RESET		25
#else
#define	RA_BTN_RESET		13
#endif
#define	RA_BTN_WPS		26

#ifdef	RTCONFIG_DSL
#define	RA_LED_WAN		25	//javi
#else
#define	RA_LED_WAN		27
#endif

#ifdef	RTCONFIG_N56U_SR2
#define	RA_LED_LAN		31
#else
#define	RA_LED_LAN		19
#endif
/*
#define RTN13U_SW1	9
#define RTN13U_SW2	13
#define RTN13U_SW3	11
*/

#define GPIO_DIR_OUT	1
#define GPIO_DIR_IN	0

#define GPIO0		0x0001
#define GPIO1		0x0002
#define GPIO2		0x0004
#define GPIO3		0x0008
#define GPIO4		0x0010
#define GPIO5		0x0020
#define GPIO6		0x0040
#define GPIO7		0x0080
#define GPIO15		0x8000

unsigned long task_mask;

int switch_init(void);

void switch_fini(void);

int ra3052_reg_read(int offset, int *value);

int ra3052_reg_write(int offset, int value);

int config_3052(int type);

int restore_3052();

void ra_gpio_write_spec(int bit_idx, int flag);

int check_all_tasks();

int ra_gpio_set_dir(int dir);

int ra_gpio_write_int(int value);

int ra_gpio_read_int(int *value);

int ra_gpio_write_bit(int idx, int value);

extern int wl_ioctl(const char *ifname, int cmd, struct iwreq *pwrq);

#endif
