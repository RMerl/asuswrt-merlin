/* Defintion of Communcation Protocol of ASUS's network devices */
/* $Id: iboxcom.h.wirelesshd,v 1.1.1.1 2008/07/21 09:17:45 james26_jang Exp $								*/

#ifndef __IBOX_COMMON__
#define __IBOX_COMMON__

#pragma pack(1)

/****************************************/
/*              FOR LINUX               */
/****************************************/
#ifndef  WIN32
#define ULONG   unsigned long
#define DWORD   unsigned long
#define BYTE    char
#define PBYTE   char *
#define WORD    unsigned short
#define INT     int
#endif //#ifndef  WIN32

#define WIRELESS_HD 1
#define SVRPORT 9999

//Define Error Code

/************ Use Internally in Program************/
#define	ERR_SUCCESS				0

/************ Use Internally in Program************/

#define	ERR_BASE				100


//Use For Network Communication Protocol

//Packet Type Section
#define NET_SERVICE_ID_BASE	10
#define NET_SERVICE_ID_LPT_EMU		NET_SERVICE_ID_BASE + 1
#define NET_SERVICE_ID_IBOX_INFO	NET_SERVICE_ID_BASE + 2


//Packet Type Section
#define NET_PACKET_TYPE_BASE	20
#define NET_PACKET_TYPE_CMD	NET_PACKET_TYPE_BASE + 1
#define NET_PACKET_TYPE_RES	NET_PACKET_TYPE_BASE + 2

//Command ID Section
//#define NET_CMD_ID_BASE		30
//#define NET_CMD_ID_GETINFO	NET_CMD_ID_BASE + 1

enum  NET_CMD_ID
{
	NET_CMD_ID_BASE=30,
	NET_CMD_ID_GETINFO,
	NET_CMD_ID_GETINFO_EX,
	NET_CMD_ID_GETINFO_SITES,
	NET_CMD_ID_SETINFO,
	NET_CMD_ID_SETSYSTEM,
	NET_CMD_ID_GETINFO_PROF,
	NET_CMD_ID_SETINFO_PROF,
	NET_CMD_ID_MANU_BASE=50,
	NET_CMD_ID_MANU_CMD,
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
	BYTE		Password[32];
} IBOX_COMM_PKT_HDR_EX;


typedef struct iboxPKTExRes
{
	BYTE		ServiceID;
	BYTE		PacketType;
	WORD		OpCode;
	DWORD 		Info; // Or Transaction ID
	BYTE		MacAddress[6];
} IBOX_COMM_PKT_RES_EX;


typedef struct iboxPKTCmd
{
	BYTE		len;
	BYTE		cmd[480];
} IBOX_COMM_PKT_SYSCMD;

typedef struct iboxPKTCmdRes
{
	BYTE		len;
	BYTE		res[480];
} IBOX_COMM_PKT_SYSCMD_RES;

//structure in IBOX_COMM_PKT_HDR.Info
//--------------------------------------------
//|Product Info| Not used						|
//--------------------------------------------
//   1 bytes       3 bytes

//Define the constants used in IBOX_COMM_PKT_HDR.Info
#define INFO_WL500 0x00010000
#define INFO_WL510 0x00020000


#define INFO_PDU_LENGTH	512

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
  	BYTE Regulation;
} PKT_GET_INFO;

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
#ifdef WIRELESS_HD
	BYTE name[32];
  	DWORD ipaddr;		// Wireless HD, IP for this profile
  	DWORD netmask;		// Wireless HD, Mask for this profile
  	DWORD gateway;		// Wireless HD, Gateway for this profile
  	BYTE dhcp;		// Wireless HD, DHCP Client for this profile
	BYTE Reserve[55-13-32];
#else
	BYTE Reserve[55];
#endif
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
  	BYTE Reserve[51];
} PKT_GET_INFO_STA;

typedef struct InfoSite
{
	BYTE SSID[32];
	BYTE Channel;
	BYTE Type;
	BYTE Wep;
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
  	BYTE WdsNo;
  	BYTE WdsMAC[6*6];
  	BYTE Reserve[9];
} PKT_GET_INFO_AP;


// Definition for field ID and type
enum  OPERATION_MODE
{
	OPERATION_MODE_WB=0x00,
	OPERATION_MODE_AP,
	OPERATION_MODE_GATEWAY,
	OPERATION_MODE_ROUTER
};


enum AP_AUTHENTICATION
{
	AUTHENTICATION_SHARED=0x00,	
	AUTHENTICATION_OPEN	
};

enum  AP_ENCRYPTION
{
	ENCRYPTION_DISABLE=0x00,
	ENCRYPTION_WEP64,
	ENCRYPTION_WEP128
};

enum  STA_STATION_MODE
{
	STATION_MODE_INFRA=0x00,
	STATION_MODE_ADHOC,
	STATION_MODE_AP
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


enum  STA_CONNECTION_STATUS
{
	STATUS_DISCONNECT=0x00,
	STATUS_CONNECT
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
};

#pragma pack()
#endif //#ifndef __IBOX_COMMON__
