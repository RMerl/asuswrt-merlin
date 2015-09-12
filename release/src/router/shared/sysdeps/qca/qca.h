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
#ifndef _QCA_H_
#define _QCA_H_

#include <iwlib.h>


#define WIF	"ath0"
extern const char WIF_2G[];
extern const char WIF_5G[];
extern const char WDSIF_5G[];
extern const char STA_2G[];
extern const char STA_5G[];
extern const char VPHY_2G[];
extern const char VPHY_5G[];
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

typedef struct _RT_802_11_MAC_TABLE {
    unsigned long	Num;
    RT_802_11_MAC_ENTRY Entry[MAX_NUMBER_OF_MAC];
} RT_802_11_MAC_TABLE, *PRT_802_11_MAC_TABLE;

typedef struct _RT_802_11_MAC_TABLE_2G {
    unsigned long	Num;
    RT_802_11_MAC_ENTRY_2G Entry[MAX_NUMBER_OF_MAC];
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
	unsigned char ssid[33]; 
	char bssid[18];
	char encryption[9];
	char authmode[16];
	char signal[9];
	char wmode[8];
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

#define SPI_PARALLEL_NOR_FLASH_FACTORY_LENGTH	0x10000
/* Below offset addresses, actually, based on start address of Flash.
 * Thus, there are absolute addresses and only valid on products
 * associated with parallel NOR Flash and SPI Flash.
 */

#define ETH0_MAC_OFFSET			0x1002
#define ETH1_MAC_OFFSET			0x5006

#define MTD_FACTORY_BASE_ADDRESS	0x40000

#define OFFSET_RFCA_COPIED		(MTD_FACTORY_BASE_ADDRESS + 0x0D00A)	/* 4 bytes */
#define OFFSET_FORCE_USB3		(MTD_FACTORY_BASE_ADDRESS + 0x0D010)	/* 1 bytes */
#define OFFSET_MTD_FACTORY		(MTD_FACTORY_BASE_ADDRESS + 0x00000)
#define OFFSET_BOOT_VER			(MTD_FACTORY_BASE_ADDRESS + 0x0D18A)	/* 0x4018A -> 0x4D18A */
#define OFFSET_COUNTRY_CODE		(MTD_FACTORY_BASE_ADDRESS + 0x0D188)	/* 0x40188 -> 0x4D188 */
#define	FACTORY_COUNTRY_CODE_LEN	2

/* WAN: eth0
 * LAN: eth1
 * 2G: follow WAN
 * 5G: follow LAN
 */
#define OFFSET_MAC_ADDR_2G		(MTD_FACTORY_BASE_ADDRESS + ETH0_MAC_OFFSET)	/* FIXME: How to map 2G/5G to eth0/1? */
#define OFFSET_MAC_ADDR			(MTD_FACTORY_BASE_ADDRESS + ETH1_MAC_OFFSET)	/* FIXME: How to map 2G/5G to eth0/1? */
#define	QCA9557_EEPROM_SIZE		1088
#define	QCA9557_EEPROM_MAC_OFFSET	(OFFSET_MAC_ADDR_2G & 0xFFF) // 2
#define	QC98XX_EEPROM_SIZE_LARGEST	2116 // sync with driver
#define	QC98XX_EEPROM_MAC_OFFSET	(OFFSET_MAC_ADDR & 0xFFF) // 6

#define OFFSET_PIN_CODE			(MTD_FACTORY_BASE_ADDRESS + 0x0D180)	/* 0x40180 -> 0x4D180 */
#define OFFSET_PSK			(MTD_FACTORY_BASE_ADDRESS + 0x0ff60)    /* 15 bytes */  
#define OFFSET_TERRITORY_CODE		(MTD_FACTORY_BASE_ADDRESS + 0x0ff90)	/* 5 bytes, e.g., US/01, US/02, TW/01, etc. */
/*
 * PIB parameters of Powerline Communication (PLC)
 */
#ifdef RTCONFIG_QCA_PLC_UTILS
#define OFFSET_PLC_MAC			(MTD_FACTORY_BASE_ADDRESS + 0x0D18E)	// 6
#define OFFSET_PLC_NMK			(MTD_FACTORY_BASE_ADDRESS + 0x0D194)	// 16
#endif
/*
 * disable DHCP client and DHCP override during ATE
 */
#ifdef RTCONFIG_DEFAULT_AP_MODE
#define OFFSET_FORCE_DISABLE_DHCP	(MTD_FACTORY_BASE_ADDRESS + 0x0D1AA)	// 1
#endif

#define OFFSET_DEV_FLAGS		(MTD_FACTORY_BASE_ADDRESS + 0x0ffa0)	//device dependent flags
#define OFFSET_ODMPID			(MTD_FACTORY_BASE_ADDRESS + 0x0ffb0)	//the shown model name (for Bestbuy and others)
#define OFFSET_FAIL_RET			(MTD_FACTORY_BASE_ADDRESS + 0x0ffc0)
#define OFFSET_FAIL_BOOT_LOG		(MTD_FACTORY_BASE_ADDRESS + 0x0ffd0)	//bit operation for max 100
#define OFFSET_FAIL_DEV_LOG		(MTD_FACTORY_BASE_ADDRESS + 0x0ffe0)	//bit operation for max 100
#define OFFSET_SERIAL_NUMBER		(MTD_FACTORY_BASE_ADDRESS + 0x0fff0)

/*
 * LED/Button GPIO# definitions
 */
//#define QCA_LED_ON		0	// low active (all 5xx series)
//#define QCA_LED_OFF		1

/* RT-AC55U */
#define	QCA_BTN_RESET		17
#define	QCA_BTN_WPS		16
#define	QCA_LED_POWER		19
#define	QCA_LED_USB		 4	/* high active */
#define	QCA_LED_USB3		 0	/* FIXME: Needs xhci driver */
#define	QCA_LED_WAN		15	/* high active, blue */
#define	QCA_LED_WAN_RED		14	/* high active, red */
#define	QCA_LED_LAN		18

#define GPIO_DIR_OUT		1
#define GPIO_DIR_IN		0

/*
 * interface of CPU to LAN
 */
#if defined(PLN12)
#define MII_IFNAME	"eth1"
#else
#define MII_IFNAME	"eth0"
#endif

unsigned long task_mask;

extern int switch_init(void);
extern void switch_fini(void);
extern int wl_ioctl(const char *ifname, int cmd, struct iwreq *pwrq);
extern int qc98xx_verify_checksum(void *eeprom);
extern int calc_qca_eeprom_csum(void *ptr, unsigned int eeprom_size);
/* for ATE Get_WanLanStatus command */
typedef struct {
	unsigned int link[5];
	unsigned int speed[5];
} phyState;
#endif	/* _QCA_H_ */
