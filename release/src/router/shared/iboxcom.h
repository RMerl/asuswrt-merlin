/************************************************************/
/*  Version 2.4     by Edison      2016/5/16		    */
/*  Using unsigned integer for 64 bits to 32 bits           */
/************************************************************/
/************************************************************/
/*  Version 2.4     by Cheni      2015/5/27		    */
/*  HW Control Info                                         */
/************************************************************/
/************************************************************/
/*  Version 2.3     by Cheni      2014/01/15	            */
/*  AAE and API Level Info                                  */
/************************************************************/
/************************************************************/
/*  Version 2.2     by Cheni	  2012/11/1		    */
/*  AppHttpPort Info                                        */
/************************************************************/
/************************************************************/
/*  Version 2.1     by Cheni      2012/5/31		    */
/*  Webdav Info						    */
/************************************************************/

/************************************************************/
/*  Version 2.0     by Cheni      2012/2/24                 */
/*  Webdav Info						    */
/************************************************************/

/************************************************************/
/*  Version 1.9     by Roly      2009/7/24                  */
/*  Add LAN IP and Static Route List Info		    */
/************************************************************/

/************************************************************/
/*  Version 1.8     by Jiahao      2007/2/13                */
/*  Add AUTHENTICATION_WPA_PSK2 for WL520gu		    */
/************************************************************/

/************************************************************/
/*  Version 1.7     by ChenI      2005/6/14                 */
/*  Add DISK_STATUS_INSTALLING for WL700g		    */
/************************************************************/

/************************************************************/
/*  Version 1.6     by ChenI      2005/5/27                 */
/*  Add STORAGE_INFO_T for WL700g			    */
/************************************************************/

/************************************************************/
/*  Version 1.5     by Yuhsin_Lee 2005/3/24 15:02           */
/************************************************************/

/* Defintion of Communcation Protocol of ASUS's network devices */
/* $Id: iboxcom.h,v 1.2 2007/04/24 11:15:16 shinjung Exp $								*/

#ifndef __IBOX_COMMON__
#define __IBOX_COMMON__
#include "shared.h"

#pragma pack(1)

/****************************************/
/*              FOR LINUX               */
/****************************************/
#ifndef  WIN32
#define ULONG   unsigned int	//Using unsigned integer for 64 bits to 32 bits.
#define DWORD   unsigned int	//Using unsigned integer for 64 bits to 32 bits.
#define BYTE    char
#define PBYTE   char *
#define WORD    unsigned short
#define INT     int
#endif //#ifndef  WIN32

#define SVRPORT 9999
#define OTSPORT 9998
#define MAX_NO_OF_IBOX  255
#define OTS_IPADD "192.168.1.1"
#define WLHDD_SUPPORT 1
//Define Error Code

/************ Use Internally in Program************/
#define	ERR_SUCCESS				0

/************ Use Internally in Program************/

#define	ERR_BASE				100


//Use For Network Communication Protocol

//Packet Type Section
#define NET_SERVICE_ID_BASE	        (10)
#define NET_SERVICE_ID_LPT_EMU	    (NET_SERVICE_ID_BASE + 1)
#define NET_SERVICE_ID_IBOX_INFO	(NET_SERVICE_ID_BASE + 2)


//Packet Type Section
#define NET_PACKET_TYPE_BASE	    (20)
#define NET_PACKET_TYPE_CMD	        (NET_PACKET_TYPE_BASE + 1)
#define NET_PACKET_TYPE_RES	        (NET_PACKET_TYPE_BASE + 2)

//Command ID Section
//#define NET_CMD_ID_BASE		30
//#define NET_CMD_ID_GETINFO	NET_CMD_ID_BASE + 1

enum  NET_CMD_ID
{                               // Decimal      Hexadecimal
	NET_CMD_ID_BASE = 30,       //  30              0x1E
	NET_CMD_ID_GETINFO,         //  31              0x1F
	NET_CMD_ID_GETINFO_EX,      //  32              0x20
	NET_CMD_ID_GETINFO_SITES,   //  33              0x21
	NET_CMD_ID_SETINFO,         //  34              0x22
	NET_CMD_ID_SETSYSTEM,       //  35              0x23
	NET_CMD_ID_GETINFO_PROF,    //  36              0x24
	NET_CMD_ID_SETINFO_PROF,    //  37              0x25
    	NET_CMD_ID_CHECK_PASS,      //  38              0x26
#ifdef BTN_SETUP
	NET_CMD_ID_SETKEY_EX,	    //  39		0x27
	NET_CMD_ID_QUICKGW_EX,	    //  40 		0x28
	NET_CMD_ID_EZPROBE,	    //  41		0x29
#endif
	NET_CMD_ID_MANU_BASE=50,    //  50		0x32
	NET_CMD_ID_MANU_CMD,	    //  51		0x33
	NET_CMD_ID_GETINFO_MANU,    //  52              0x34
	NET_CMD_ID_GETINFO_EX2,     //  53              0x35
	NET_CMD_ID_MAXIMUM
};

enum  NET_RES_OP
{
	NET_RES_OK = 0x0000,
	NET_RES_ERR_PASSWORD = 0x0100,
	NET_RES_ERR_FIELD_UNDEF = 0x0200
};

//Packet Header Structure
typedef struct iboxPKT
{
	BYTE		ServiceID;
	BYTE		PacketType;
	WORD		OpCode;
	DWORD 		Info; // Or Transaction ID
} IBOX_COMM_PKT_HDR;

typedef struct iboxPKTRes
{
	BYTE		ServiceID;
	BYTE		PacketType;
	WORD		OpCode;
	DWORD 		Info; // Or Transaction ID
} IBOX_COMM_PKT_RES;

typedef struct iboxPKTEx
{
	BYTE		ServiceID;
	BYTE		PacketType;
	WORD		OpCode;
	DWORD 		Info; // Or Transaction ID
	BYTE		MacAddress[6];
	BYTE		Password[32];   //NULL terminated string, string length:1~31, cannot be NULL string
} IBOX_COMM_PKT_HDR_EX;

typedef struct iboxPKTExRes
{
	BYTE		ServiceID;
	BYTE		PacketType;
	WORD		OpCode;
	DWORD 		Info; // Or Transaction ID
	BYTE		MacAddress[6];
} IBOX_COMM_PKT_RES_EX;

//structure in IBOX_COMM_PKT_HDR.Info
//--------------------------------------------
//|Product Info| Not used						|
//--------------------------------------------
//   1 bytes       3 bytes

//Define the constants used in IBOX_COMM_PKT_HDR.Info
#define INFO_WL500 0x00010000
#define INFO_WL510 0x00020000
#define INFO_PDU_LENGTH	512

#define WAVESERVER			// eric++

//Extended Fields Definition
typedef struct PktGetInfo
{
  	BYTE PrinterInfo[128];
	BYTE SSID[32];
  	BYTE NetMask[32];
  	BYTE ProductID[32];
  	BYTE FirmwareVersion[16];
  	BYTE OperationMode; 
	BYTE MacAddress[6];
#ifdef WCLIENT
	BYTE Regulation;
#endif
	BYTE sw_mode;
} PKT_GET_INFO;

#ifdef WAVESERVER			// eric++
#define WS_INFO_FILENAME		"/tmp/waveserver.info"

// appended to the PKT_GET_INFO; 
// at offset 0x100 (sizeof (IBOX_COMM_PKT_RES) + sizeof (PKT_GET_INFO))
typedef struct ws_info_t	{
	BYTE Name[4];		// 'W' + 'S' + Major version number + Minor version number
	WORD Channel;
	WORD SampleRate;
	union   {
		unsigned char SampleSize;
		struct {
			unsigned char s8:1,     /* bit0, Sample Size  8-bits */
			s16:1,		  	/* bit1, Sample Size 16-bits */
			s24:1,		  	/* bit2, Sample Size 24-bits */
			s32:1,		  	/* bit3, Sample Size 32-bits */
			rsvd:3,		 	/* bit4~6, Reserved */
			cflag:1;		/* bit7, Connection Flag */
		} b;
	} u;
	BYTE Compress;
	BYTE Reserved[6];
} WS_INFO_T;
#endif

#define EXTEND_MAGIC			0x8082
#define EXTEND_CAP_WEBDAV 		0x0001
#define EXTEND_CAP_SYNC			0x0002
#define EXTEND_CAP_MEDIA		0x0004

#define EXTEND_CAP_AAE_BASIC		0x0010
#define EXTEND_CAP_HWCTRL		0x0020
#define EXTEND_CAP_SWCTRL		0x0040

#define EXTEND_WEBDAV_TYPE_HTTP		0x00
#define EXTEND_WEBDAV_TYPE_HTTPS	0x01
#define EXTEND_WEBDAV_TYPE_BOTH		0x02

#ifdef RTCONFIG_ROG
#define EXTEND_API_LEVEL		2
#else
#define EXTEND_API_LEVEL                1
#endif

typedef struct webdav_info_t {
	BYTE EnableWebDav;
	BYTE HttpType;
	WORD HttpPort;
	BYTE EnableDDNS;
	BYTE HostName[64];
  	DWORD WANIPAddr;
	BYTE WANState;
	BYTE isNotDefault;
	WORD HttpsPort;
} WEBDAV_INFO_T;

typedef struct hwctrl_info_t {
        BYTE EnableLed;
        BYTE EnableBuzz;
	BYTE Reserved[14];
} HWCTRL_INFO_T;

typedef struct swctrl_info_t {
	BYTE ROGAPILevel;
	BYTE AiHOMEAPILevel;
	BYTE AiProtectionAPILevel;
	BYTE Reserved[13];
} SWCTRL_INFO_T;

typedef struct device_info_t {
	WEBDAV_INFO_T wt;
	HWCTRL_INFO_T hw;
	SWCTRL_INFO_T sw;
} DEVICE_INFO_T;

typedef struct storage_info_t {
	WORD MagicWord;
	WORD ExtendCap;
	union {
		WEBDAV_INFO_T wt;
		DEVICE_INFO_T dev;
		BYTE Reserved[128];
	} u;
	WORD AppHttpPort;      	/* Port for accessing app UI */
	BYTE AppAPILevel;	/* API level for app */
	BYTE EnableAAE;	
	BYTE AAEDeviceID[64];
} STORAGE_INFO_T;

typedef struct PktGetInfoEx1
{
	BYTE FieldCount;
  	WORD FieldID;  	
} PKT_GET_INFO_EX1;

typedef struct Profiles
{
	BYTE mode;
  	BYTE chan;
   	BYTE ssid[32];
  	BYTE rate;
  	BYTE wep;
  	BYTE wepkeylen;  /* 0: 40 bit, 1: 128 bit */
   	BYTE wepkey1[16];  /* only up to 13 bits used */
  	BYTE wepkey2[16] ;  /* only up to 13 bits used */
  	BYTE wepkey3[16] ;  /* only up to 13 bits used */
  	BYTE wepkey4[16];  /* only up to 13 bits used */
  	BYTE wepkeyactive;
  	BYTE sharedkeyauth;
   	BYTE brgmacclone;
  	BYTE preamble;
	BYTE Reserve[55];
} PROFILES;

typedef struct PktGetInfoProf
{
	BYTE StartIndex;
  	BYTE Count;
  	union
  	{
  	   PROFILES Profiles[2];
  	   struct
  	   {
  	   	BYTE ButtonType;
  		BYTE ButtonMode;
  		BYTE ProfileCount;
  	   } ProfileControl;
  	} p;
} PKT_GET_INFO_PROFILE;

typedef struct PktGetInfoSta
{
	BYTE mode;
  	BYTE chan;
   	BYTE ssid[32];
  	BYTE rate;
  	BYTE wep;
  	BYTE wepkeylen;  /* 0: 40 bit, 1: 128 bit */
   	BYTE wepkey1[16];  /* only up to 13 bits used */
  	BYTE wepkey2[16] ;  /* only up to 13 bits used */
  	BYTE wepkey3[16] ;  /* only up to 13 bits used */
  	BYTE wepkey4[16];  /* only up to 13 bits used */
  	BYTE wepkeyactive;
  	BYTE sharedkeyauth;
   	BYTE brgmacclone;
  	BYTE preamble;
	BYTE buttonMode;
  	BYTE buttonType;
  	BYTE profileCount;
  	BYTE connectionStatus;
  	DWORD IPAddr;
  	DWORD Mask;
  	DWORD Gateway;
  	BYTE Dhcp;
  	BYTE Reserve[38]; //51-13=38
} PKT_GET_INFO_STA;	// total 173

/*
=======================================================
definition in these 2 data structure (EA mode)
typedef struct Profiles
typedef struct PktGetInfoSta

                wep     wepkeylen   sharedkeyauth
open-none       0       0           0
open-wep64      1       0           0
open-wep128     1       1           0
shared-wep64    1       0           1
shared-wep128   1       1           1
wpa-tkip        0       1           not used

//enum STA_VIRTUAL_WEP_KEYLEN_TYPE
=======================================================
*/

typedef struct InfoSite
{
	BYTE SSID[32];
	BYTE Channel;
	BYTE Type;
	BYTE Wep; //enum  SITE_ENCRYPTION
	BYTE Status;
	BYTE BSSID[6];
	BYTE RSSI;
	BYTE Flag;
	BYTE Reserve[16];
} SITES;

typedef struct PktGetInfoSite
{
	BYTE Count;
	BYTE Index;
  	SITES Sites[8];  	
} PKT_GET_INFO_SITES;

typedef struct PktGetInfoAP
{
	BYTE SSID[32];
  	BYTE Channel;
  	BYTE Auth;
  	BYTE Encrypt;
  	BYTE Key[64];
  	BYTE DefaultKey;
  	DWORD IPAddr;
  	DWORD Mask;
  	DWORD Gateway;
  	BYTE Dhcp;
  	BYTE WdsMode;
  	BYTE WdsMAC[6*6];
  	BYTE WdsNo;
  	BYTE Reserve[9];
} PKT_GET_INFO_AP;	// total 224 bytes

typedef struct iboxPKTCmd
{
	WORD		len;
	BYTE		cmd[420];
} PKT_SYSCMD;		// total 422 bytes

typedef struct iboxPKTCmdRes
{
	WORD		len;
	BYTE		res[420];
} PKT_SYSCMD_RES;	// total 422 bytes

#ifdef BTN_SETUP
// what is included in PKT_SET_INFO_GW_QUICK, OR these 3 flags
#define QFCAP_WIRELESS 		0x01
#define QFCAP_ISP		0x02
#define QFCAP_WIRELESS2		0x04
#define QFCAP_GET		0x40
#define QFCAP_FINISH		0x80
#define QFCAP_REBOOT		0x20
#define MAX_DHKEY_LEN		192

enum ACK_TYPE
{
	QUICK_ACK=0x00,
	QUICK_NACK
};

enum ISP_TYPE
{
	ISP_TYPE_DHCPCLIENT=0x00,
	ISP_TYPE_PPPOE,
	ISP_TYPE_PPTP,
	ISP_TYPE_STATICIP,
	ISP_TYPE_BIGPOND,
	ISP_TYPE_L2TP               //added by Minyen
};

enum PPTP_OPTION
{
    PPTP_OPTION_NO=0x00,
    PPTP_OPTION_NOENCRYPT,
    PPTP_OPTION_MPPE40,
    PPTP_OPTION_MPPE56,
    PPTP_OPTION_MPPE128
};
                                                                                                                  
enum  MULTICAST_RATE
{
    MULTICAST_RATE_AUTO=0x00,
    MULTICAST_RATE_1,
    MULTICAST_RATE_2,
    MULTICAST_RATE_5,
    MULTICAST_RATE_6,
    MULTICAST_RATE_9,
    MULTICAST_RATE_11,
    MULTICAST_RATE_12,
    MULTICAST_RATE_18,
    MULTICAST_RATE_24,
    MULTICAST_RATE_36,
    MULTICAST_RATE_48,
    MULTICAST_RATE_54
};
                                                                                                                  
enum WAN_BRIDGE_PORT
{
    WAN_BRIDGE_NONE=0x00,
    WAN_BRIDGE_LAN1,
    WAN_BRIDGE_LAN2,
    WAN_BRIDGE_LAN3,
    WAN_BRIDGE_LAN4,
    WAN_BRIDGE_LAN3LAN4
};

enum PACKET_NUM
{
	SR_PACKET_1=0x01,
	SR_PACKET_2,
	SR_PACKET_3
};

typedef struct PktEZProbeInfo
{
	BYTE isNotDefault;  // 0/1, is in default state ?
	BYTE isSetByOts;    // 0/1, is in EZSetup default state ?
	BYTE isWAN;         // 0/1, is WAN connected ?
	BYTE isDHCP;        // Not used now
	BYTE isPPPOE;	    // Not used now
	BYTE Auth;	    // Authentication Method 
	BYTE Wds;	    // WDS Mode, in WDS_MODE
	BYTE Acl;	    // ACL Mode, in ACL_MODE
  	BYTE ProductID[32]; // Not used now
  	BYTE FirmwareVersion[16]; // Not used now
	STORAGE_INFO_T StorageInfo;
} PKT_EZPROBE_INFO;

typedef struct PktSetInfoGWQuickKey
{
	BYTE Auth;
	BYTE Encrypt;
	BYTE KeyLen;	//default: 0
	BYTE Key[MAX_DHKEY_LEN];
} PKT_SET_INFO_GW_QUICK_KEY;

typedef struct TempWirelessStruct {
	BYTE TempSSID[32];
	BYTE TempKey[32];      // AES Key, 256 bits = 32 bytes
	BYTE SuggestSSID[32];  // If SSID = NULL String, use this one
	BYTE SuggestKey[64];   // If Key = NULL String, use this one
} TEMP_WIRELESS_STRUCT;

typedef struct TempWireless
{
  	union
  	{
  	   BYTE SharedKey[MAX_DHKEY_LEN*2];
	   TEMP_WIRELESS_STRUCT WirelessStruct;
	} u;
} TEMP_WIRELESS;

typedef struct GWQuickWireless
{
	BYTE SSID[32];
	BYTE Auth;		// as enum in AP_AUTHENTICATION
  	BYTE Encrypt;	// as enum is AP_ENCRYPTION
  	BYTE Key[64];
	BYTE DefaultKey;	
} GW_QUICK_WIRELESS;

typedef struct GWQuickISP
{
  	BYTE ISPType;		// as enum in ISP_TYPE
  	BYTE UserName[64];  // NOTE: max length: 64! 0~63: null-terminated, 64: NOT null-terminated
  	BYTE Password[64];  // NOTE: max length: 64! 0~63: null-terminated, 64: NOT null-terminated
  	BYTE MAC[12];		// MAC in xxxxxxxxxxxx
	BYTE HostName[32];	// Name                 // NOTE: max length: 32! 0~31: null-terminated, 32: NOT null-terminated
	BYTE BPServer[32];	// IP address or Name   // NOTE: max length: 32! 0~31: null-terminated, 32: NOT null-terminated
  	DWORD IPAddr;
  	DWORD Mask;
  	DWORD Gateway;
	DWORD DNSServer1;
	DWORD DNSServer2;
	BYTE TimeZone[16];	// as string in Linux setting
	BYTE PPTPOption;        // PPTP options
	BYTE DHCPRoute;         // 0/1
	BYTE MulticastRoute;    // 0/1
	BYTE StaticRoute;       // 0/1
	BYTE MulticastRate;     // multi-cast rate
	BYTE WANBridgePort;     // WAN Bridge Port
	BYTE PacketNum;		// check next Packet
	BYTE SRNum;		// Static Route List number
	DWORD LAN_IPAddr;	// LAN IP address
	DWORD SR_IPAddr[12];	// Static Route IP address
	DWORD SR_Mask[12];	// Static Route Mask address

} GW_QUICK_ISP;

typedef struct PktSetInfoGWQuick
{
	BYTE QuickFlag;			// 1 byte
	GW_QUICK_WIRELESS WSetting;	// 99 byte
	GW_QUICK_ISP ISPSetting;	// 348 byte,total = 46(IBOX_COMM_PKT_HDR_EX) + 1 + 99 + 348 = 494 (max = 512)
} PKT_SET_INFO_GW_QUICK;

#endif
// Definition for field ID and type
enum  ACL_MODE
{
	ACL_MODE_DISABLE=0x00,
	ACL_MODE_ACCEPT,
	ACL_MODE_REJECT
};

enum  WDS_MODE
{
	WDS_MODE_AP_ONLY=0x00,
	WDS_MODE_HYBRID,
	WDS_MODE_WDS_ONLY
};

enum  OPERATION_MODE
{
	OPERATION_MODE_WB=0x00,
	OPERATION_MODE_AP,
	OPERATION_MODE_GATEWAY,
	OPERATION_MODE_ROUTER,
	OPERATION_MODE_PAIRING
};

enum AP_AUTHENTICATION
{
	AUTHENTICATION_SHARED=0x00,
	AUTHENTICATION_OPEN,
   	AUTHENTICATION_WPA_PSK=3, 	//add by Axin
   	AUTHENTICATION_WPA=4, 		//add by Chen-I
   	AUTHENTICATION_8021X=5, 	//add by Chen-I
   	AUTHENTICATION_WPA_PSK2=2, 	//added by Jiahao
   	AUTHENTICATION_WPA_AUTO=6 	//added by Axin
};

enum  AP_ENCRYPTION
{
	ENCRYPTION_DISABLE=0x00,
	ENCRYPTION_WEP64,
	ENCRYPTION_WEP128,
   	ENCRYPTION_TKIP, 	//add by Axin
	ENCRYPTION_AES,
	ENCRYPTION_TKIP_AES
};

enum DEFAULT_KEY_INDEX
{
    DEFAULT_KEY_1=0x1,
    DEFAULT_KEY_2,
    DEFAULT_KEY_3,
    DEFAULT_KEY_4
};

enum  SITE_ENCRYPTION
{
	SITE_ENCRYPTION_DISABLE=0x00,
	SITE_ENCRYPTION_WEP,
    SITE_ENCRYPTION_TKIP
};

enum  STA_STATION_MODE
{
	STATION_MODE_INFRA=0x00,
	STATION_MODE_ADHOC
#ifdef WLHDD_SUPPORT
	,
	STATION_MODE_AP
#endif
};

enum STA_AUTHENTICATION
{
	STA_AUTHENTICATION_OPEN=0x00,
	STA_AUTHENTICATION_SHARED
};

enum  STA_ENCRYPTION
{
	STA_ENCRYPTION_DISABLE=0x00,
	STA_ENCRYPTION_ENABLE
};

enum  STA_ENCRYPTION_TYPE
{
	STA_ENCRYPTION_TYPE_WEP64=0x00,
	STA_ENCRYPTION_TYPE_WEP128
};

enum STA_VIRTUAL_WEP_KEYLEN_TYPE
{
	STA_VIRTUAL_WEP_KEYLEN_NONE=0x00,
	STA_VIRTUAL_WEP_KEYLEN_TKIP
};

enum  STA_CONNECTION_STATUS
{
	STATUS_DISCONNECT=0x00,
	STATUS_CONNECT
};

enum DHCP_MODE 
{
	DHCP_DISABLED = 0x00,
	DHCP_ENABLED
};

enum  STA_BUTTON_MODE
{
	BUTTON_MODE_DISABLE=0x00,
	BUTTON_MODE_ENABLE
};

enum  STA_BUTTON_TYPE
{
	BUTTON_TYPE_MAX_POWER=0x00,
	BUTTON_TYPE_MAX_POWER_PROFILE,
	BUTTON_TYPE_MAX_POWER_ALL,
	BUTTON_TYPE_PROFILE,
	BUTTON_TYPE_PROFILE_ONE
};

enum  STA_PROFILE_INDEX
{
	PROFILE_INDEX_START=0x00,
	PROFILE_INDEX_ERASE
};


enum FIELD_DEFINTION
{
	FIELD_GENERAL = 0x0001,
	FIELD_GENERAL_CONTROL, 		// 1
	FIELD_GENERAL_OPERATION_MODE, 	// 1
	FIELD_GENERAL_CURRENT_STA, 	// 160
	FIELD_GENERAL_CURRENT_AP, 	// 160
	FIELD_STA = 0x1000,
	FIELD_STA_SSID,			// 32
	FIELD_STA_MODE,			// 1
	FIELD_STA_CHANNEL,		// 1
	FIELD_STA_AUTH,			// 1
	FIELD_STA_ENCRYPT,		// 1
	FIELD_STA_TXRATE,		// 1
	FIELD_STA_KEY1,			// 32
	FIELD_STA_KEY2,			// 32
	FIELD_STA_KEY3,			// 32
	FIELD_STA_KEY4,			// 32
	FIELD_STA_DEFAULTKEY,		// 1
	FIELD_STA_PASSPHRASE,		// 64
	FIELD_STA_SCANMODE,		// 1
	FIELD_STA_SCANTYPE,		// 1
	FIELD_STA_PROFILE,		// 1
	FIELD_STA_CONNECTION_STATUS = 0x1050, // 1
	FIELD_WIRELESS = 0x2000,
	FIELD_WIRELESS_SSID,		// 32
	FIELD_WIRELESS_CHANNEL,		// 1
	FIELD_WIRELESS_AUTH,		// 1
	FIELD_WIRELESS_ENCRYPT,		// 1
	FIELD_WIRELESS_TXRATE,		// 1
	FIELD_WIRELESS_KEY1,		// 1
	FIELD_WIRELESS_KEY2,		// 1
	FIELD_WIRELESS_KEY3,		// 1
	FIELD_WIRELESS_KEY4,		// 1
	FIELD_WIRELESS_DEFAULTKEY,	// 1
	FIELD_WIRELESS_PASSPHRASE,	// 64
	FIELD_WIRELESS_BLOCKSSID,	// 1
	FIELD_WIRELESS_BASICRATE,	// 2
	FIELD_WIRELESS_PREAMBLE,	// 1
	FIELD_LAN = 0x2100,
	FIELD_LAN_CONNECTION_TYPE,	// 1
	FIELD_LAN_IPADDRESS,		// 32
	FIELD_LAN_NETMASK,		// 32
	FIELD_LAN_GATEWAY,		// 32
	FIELD_WAN = 0x2200,
	FIELD_WAN_CONNECTION_TYPE	// 1
#ifdef BTN_SETUP
	, FIELD_GW_QUICK = 0x2300,
	FIELD_GW_QUICK_SETUP		// 1
#endif
};

#pragma pack()
#endif //#ifndef __IBOX_COMMON__



