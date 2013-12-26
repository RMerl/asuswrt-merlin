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
/* vi: set sw=4 ts=4 sts=4: */
/*
 * stapriv.h -- definitions and structures used by station.c privately
 *
 * Copyright (c) Ralink Technology Corporation All Rights Reserved.
 *
 * $Id: stapriv.h,v 1.13 2007-07-26 11:22:24 yy Exp $
 */

#include "linux/autoconf.h"

#define NDIS_802_11_LENGTH_SSID         32
#define NDIS_802_11_LENGTH_RATES        8
#define NDIS_802_11_LENGTH_RATES_EX     16

// BW
#define BW_20       0
#define BW_40       1
// SHORTGI
#define GI_400      1 // only support in HT mode
#define GI_800      0

#define WPA_OUI_TYPE        0x01F25000
#define WPA_OUI             0x00F25000
#define WPA_OUI_1           0x00030000
#define WPA2_OUI            0x00AC0F00
#define CISCO_OUI           0x00964000


typedef unsigned long   NDIS_802_11_FRAGMENTATION_THRESHOLD;
typedef unsigned long   NDIS_802_11_RTS_THRESHOLD;
typedef unsigned long   NDIS_802_11_TX_POWER_LEVEL; // in milliwatts
typedef unsigned long long NDIS_802_11_KEY_RSC;

#define MAX_TX_POWER_LEVEL                100   /* mW */
#define MAX_RTS_THRESHOLD                 2347  /* byte count */

#define PACKED __attribute__ ((packed))

#define A_SHA_DIGEST_LEN 20
typedef struct
{
	unsigned int    H[5];
	unsigned int    W[80];
	int             lenW;
	unsigned int    sizeHi,sizeLo;
} A_SHA_CTX;

typedef struct PACKED _NDIS_802_11_SSID
{
	unsigned int    SsidLength;   // length of SSID field below, in bytes;
                                  // this can be zero.
	unsigned char   Ssid[NDIS_802_11_LENGTH_SSID]; // SSID information field
} NDIS_802_11_SSID, *PNDIS_802_11_SSID;

typedef struct _RT_802_11_STA_CONFIG {
	unsigned long   EnableTxBurst;      // 0-disable, 1-enable
	unsigned long   EnableTurboRate;    // 0-disable, 1-enable 72/100mbps turbo rate
	unsigned long   UseBGProtection;    // 0-AUTO, 1-always ON, 2-always OFF
	unsigned long   UseShortSlotTime;   // 0-no use, 1-use 9-us short slot time when applicable
	unsigned long   AdhocMode;          // 0-11b rates only (WIFI spec), 1 - b/g mixed, 2 - g only
	unsigned long   HwRadioStatus;      // 0-OFF, 1-ON, default is 1, Read-Only
	unsigned long   Rsv1;               // must be 0
	unsigned long   SystemErrorBitmap;  // ignore upon SET, return system error upon QUERY
} RT_802_11_STA_CONFIG, *PRT_802_11_STA_CONFIG;

typedef struct _NDIS_802_11_CONFIGURATION_FH
{
	unsigned long   Length;             // Length of structure
	unsigned long   HopPattern;         // As defined by 802.11, MSB set
	unsigned long   HopSet;             // to one if non-802.11
	unsigned long   DwellTime;          // units are Kusec
} NDIS_802_11_CONFIGURATION_FH, *PNDIS_802_11_CONFIGURATION_FH;

typedef struct _NDIS_802_11_CONFIGURATION
{
	unsigned long   Length;             // Length of structure
	unsigned long   BeaconPeriod;       // units are Kusec
	unsigned long   ATIMWindow;         // units are Kusec
	unsigned long   DSConfig;           // Frequency, units are kHz
	NDIS_802_11_CONFIGURATION_FH    FHConfig;
} NDIS_802_11_CONFIGURATION, *PNDIS_802_11_CONFIGURATION;

typedef struct _RT_802_11_LINK_STATUS {
	unsigned long   CurrTxRate;         // in units of 0.5Mbps
	unsigned long   ChannelQuality;     // 0..100 %
	unsigned long   TxByteCount;        // both ok and fail
	unsigned long   RxByteCount;        // both ok and fail
	unsigned long   CentralChannel;     // 40MHz central channel number
} RT_802_11_LINK_STATUS, *PRT_802_11_LINK_STATUS;

typedef struct _PAIR_CHANNEL_FREQ_ENTRY
{
	unsigned long   lChannel;
	unsigned long   lFreq;
} PAIR_CHANNEL_FREQ_ENTRY, *PPAIR_CHANNEL_FREQ_ENTRY;

typedef union  _HTTRANSMIT_SETTING {
	struct {
		unsigned short  MCS:7;          // MCS
		unsigned short  BW:1;           //channel bandwidth 20MHz or 40 MHz
		unsigned short  ShortGI:1;
		unsigned short  STBC:2;         //SPACE
		unsigned short  rsv:3;
		unsigned short  MODE:2;         // 0: CCK, 1:OFDM, 2:Mixedmode, 3:GreenField
	} field;
	unsigned short  word;
} HTTRANSMIT_SETTING, *PHTTRANSMIT_SETTING;

typedef union _LARGE_INTEGER {
	struct {
		unsigned long LowPart;
		long HighPart;
	};
	struct {
		unsigned long LowPart;
		long HighPart;
	} u;
	signed long long QuadPart;
} LARGE_INTEGER;

typedef struct _NDIS_802_11_STATISTICS
{
	unsigned long   Length;             // Length of structure
	LARGE_INTEGER   TransmittedFragmentCount;
	LARGE_INTEGER   MulticastTransmittedFrameCount;
	LARGE_INTEGER   FailedCount;
	LARGE_INTEGER   RetryCount;
	LARGE_INTEGER   MultipleRetryCount;
	LARGE_INTEGER   RTSSuccessCount;
	LARGE_INTEGER   RTSFailureCount;
	LARGE_INTEGER   ACKFailureCount;
	LARGE_INTEGER   FrameDuplicateCount;
	LARGE_INTEGER   ReceivedFragmentCount;
	LARGE_INTEGER   MulticastReceivedFrameCount;
	LARGE_INTEGER   FCSErrorCount;
	LARGE_INTEGER   TKIPLocalMICFailures;
	LARGE_INTEGER   TKIPRemoteMICErrors;
	LARGE_INTEGER   TKIPICVErrors;
	LARGE_INTEGER   TKIPCounterMeasuresInvoked;
	LARGE_INTEGER   TKIPReplays;
	LARGE_INTEGER   CCMPFormatErrors;
	LARGE_INTEGER   CCMPReplays;
	LARGE_INTEGER   CCMPDecryptErrors;
	LARGE_INTEGER   FourWayHandshakeFailures;
} NDIS_802_11_STATISTICS, *PNDIS_802_11_STATISTICS;

typedef struct _RT_VERSION_INFO {
	unsigned char   DriverVersionW;
	unsigned char   DriverVersionX;
	unsigned char   DriverVersionY;
	unsigned char   DriverVersionZ;
	unsigned int    DriverBuildYear;
	unsigned int    DriverBuildMonth;
	unsigned int    DriverBuildDay;
} RT_VERSION_INFO, *PRT_VERSION_INFO;

//Block ACK
#define TID_SIZE 8

typedef enum _REC_BLOCKACK_STATUS
{
	Recipient_NONE=0,
	Recipient_USED,
	Recipient_HandleRes,
	Recipient_Accept
} REC_BLOCKACK_STATUS, *PREC_BLOCKACK_STATUS;

typedef enum _ORI_BLOCKACK_STATUS
{
	Originator_NONE=0,
	Originator_USED,
	Originator_WaitRes,
	Originator_Done
} ORI_BLOCKACK_STATUS, *PORI_BLOCKACK_STATUS;

//For QureyBATableOID use
typedef struct  _OID_BA_REC_ENTRY
{
	unsigned char   MACAddr[6];
	unsigned char   BaBitmap;   // if (BaBitmap&(1<<TID)), this session with{MACAddr, TID}exists, so read BufSize[TID] for BufferSize
	unsigned char   rsv;
	unsigned char   BufSize[8];
	REC_BLOCKACK_STATUS REC_BA_Status[8];
} OID_BA_REC_ENTRY, *POID_BA_REC_ENTRY;

//For QureyBATableOID use
typedef struct  _OID_BA_ORI_ENTRY
{
	unsigned char   MACAddr[6];
	unsigned char   BaBitmap;  // if (BaBitmap&(1<<TID)), this session with{MACAddr, TID}exists, so read BufSize[TID] for BufferSize, read ORI_BA_Status[TID] for status
	unsigned char   rsv;
	unsigned char   BufSize[8];
	ORI_BLOCKACK_STATUS  ORI_BA_Status[8];
} OID_BA_ORI_ENTRY, *POID_BA_ORI_ENTRY;

//For SetBATable use
typedef struct {
	unsigned char   IsRecipient;
	unsigned char   MACAddr[6];
	unsigned char   TID;
	unsigned char   BufSize;
	unsigned short  TimeOut;
	unsigned char   AllTid;  // If True, delete all TID for BA sessions with this MACaddr.
} OID_ADD_BA_ENTRY, *POID_ADD_BA_ENTRY;

typedef struct _QUERYBA_TABLE
{
	OID_BA_ORI_ENTRY       BAOriEntry[32];
	OID_BA_REC_ENTRY       BARecEntry[32];
	unsigned char   OriNum;// Number of below BAOriEntry
	unsigned char   RecNum;// Number of below BARecEntry
} QUERYBA_TABLE, *PQUERYBA_TABLE;

// Received Signal Strength Indication
typedef long NDIS_802_11_RSSI;           // in dBm

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
	Ndis802_11OFDM5_VHT,
	Ndis802_11NetworkTypeMax    // not a real type, defined as an upper bound
} NDIS_802_11_NETWORK_TYPE, *PNDIS_802_11_NETWORK_TYPE;

typedef enum _NDIS_802_11_NETWORK_INFRASTRUCTURE
{
	Ndis802_11IBSS,
	Ndis802_11Infrastructure,
	Ndis802_11AutoUnknown,
	Ndis802_11InfrastructureMax         // Not a real value, defined as upper bound
} NDIS_802_11_NETWORK_INFRASTRUCTURE, *PNDIS_802_11_NETWORK_INFRASTRUCTURE;

typedef unsigned char  NDIS_802_11_RATES[NDIS_802_11_LENGTH_RATES];        // Set of 8 data rates
typedef unsigned char  NDIS_802_11_RATES_EX[NDIS_802_11_LENGTH_RATES_EX];  // Set of 16 data rates

typedef struct PACKED _NDIS_WLAN_BSSID
{
	unsigned long                       Length;             // Length of this structure
	unsigned char                       MacAddress[6];      // BSSID
	unsigned char                       Reserved[2];
	NDIS_802_11_SSID                    Ssid;               // SSID
	unsigned long                       Privacy;            // WEP encryption requirement
	NDIS_802_11_RSSI                    Rssi;               // receive signal
                                                            // strength in dBm
	NDIS_802_11_NETWORK_TYPE            NetworkTypeInUse;
	NDIS_802_11_CONFIGURATION           Configuration;
	NDIS_802_11_NETWORK_INFRASTRUCTURE  InfrastructureMode;
	NDIS_802_11_RATES                   SupportedRates;
} NDIS_WLAN_BSSID, *PNDIS_WLAN_BSSID;

// Added Capabilities, IELength and IEs for each BSSID
typedef struct PACKED _NDIS_WLAN_BSSID_EX
{
	unsigned long                       Length;             // Length of this structure
	unsigned char                       MacAddress[6];      // BSSID
	unsigned char                       Reserved[2];
	NDIS_802_11_SSID                    Ssid;               // SSID
	unsigned int                        Privacy;            // WEP encryption requirement
	NDIS_802_11_RSSI                    Rssi;               // receive signal
                                                            // strength in dBm
	NDIS_802_11_NETWORK_TYPE            NetworkTypeInUse;
	NDIS_802_11_CONFIGURATION           Configuration;
	NDIS_802_11_NETWORK_INFRASTRUCTURE  InfrastructureMode;
	NDIS_802_11_RATES_EX                SupportedRates;
	unsigned long                       IELength;
	unsigned char                       IEs[1];
} NDIS_WLAN_BSSID_EX, *PNDIS_WLAN_BSSID_EX;

typedef struct PACKED _NDIS_802_11_BSSID_LIST_EX
{
	unsigned int            NumberOfItems;      // in list below, at least 1
	NDIS_WLAN_BSSID_EX      Bssid[1];
} NDIS_802_11_BSSID_LIST_EX, *PNDIS_802_11_BSSID_LIST_EX;

typedef struct PACKED _NDIS_802_11_FIXED_IEs
{
	unsigned char   Timestamp[8];
	unsigned short  BeaconInterval;
	unsigned short  Capabilities;
} NDIS_802_11_FIXED_IEs, *PNDIS_802_11_FIXED_IEs;

typedef struct PACKED _NDIS_802_11_VARIABLE_IEs
{
	unsigned char   ElementID;
	unsigned char   Length;        // Number of bytes in data field
	unsigned char   data[1];
} NDIS_802_11_VARIABLE_IEs, *PNDIS_802_11_VARIABLE_IEs;

typedef struct _NDIS_802_11_NETWORK_TYPE_LIST
{
	unsigned int    NumberOfItems;  // in list below, at least 1
	NDIS_802_11_NETWORK_TYPE    NetworkType [1];
} NDIS_802_11_NETWORK_TYPE_LIST, *PNDIS_802_11_NETWORK_TYPE_LIST;

//For OID Query or Set about BA structure
typedef struct  _OID_BACAP_STRUC {
	unsigned char   RxBAWinLimit;
	unsigned char   TxBAWinLimit;
	unsigned char   Policy;     // 0: DELAY_BA 1:IMMED_BA  (//BA Policy subfiled value in ADDBA frame)   2:BA-not use. other value invalid
	unsigned char   MpduDensity;// 0: DELAY_BA 1:IMMED_BA  (//BA Policy subfiled value in ADDBA frame)   2:BA-not use. other value invalid
	unsigned char   AmsduEnable;//Enable AMSDU transmisstion
	unsigned char   AmsduSize;  // 0:3839, 1:7935 bytes. unsigned int  MSDUSizeToBytes[]    = { 3839, 7935};
	unsigned char   MMPSmode;   // MIMO power save more, 0:static, 1:dynamic, 2:rsv, 3:mimo enable
	unsigned char   AutoBA;     // Auto BA will automatically , BOOLEAN
} OID_BACAP_STRUC, *POID_BACAP_STRUC;

typedef enum _RT_802_11_PHY_MODE {
	PHY_11BG_MIXED = 0,
	PHY_11B,
	PHY_11A,
	PHY_11ABG_MIXED,
	PHY_11G,
	PHY_11ABGN_MIXED,   // both band   5
	PHY_11N,            //    6
	PHY_11GN_MIXED,     // 2.4G band      7
	PHY_11AN_MIXED,     // 5G  band       8
	PHY_11BGN_MIXED,    // if check 802.11b.      9
	PHY_11AGN_MIXED,    // if check 802.11b.      10
	PHY_11N_5G,         // 11
	PHY_11VHT_N_ABG_MIXED,
	PHY_11VHT_N_AG_MIXED,
	PHY_11VHT_N_A_MIXED,// 14
	PHY_11VHT_N_MIXED,
	PHY_MODE_MAX
} RT_802_11_PHY_MODE;

enum WIFI_MODE{
	WMODE_INVALID = 0,
	WMODE_A  = 1 << 0,
	WMODE_B  = 1 << 1,
	WMODE_G  = 1 << 2,
	WMODE_GN = 1 << 3,
	WMODE_AN = 1 << 4,
	WMODE_AC = 1 << 5,
	WMODE_COMP = 6, /* total types of supported wireless mode, add this value once yow add new type */
};


typedef struct {
	RT_802_11_PHY_MODE  PhyMode;
	unsigned char       TransmitNo;
	unsigned char       HtMode;     //HTMODE_GF or HTMODE_MM
	unsigned char       ExtOffset;  //extension channel above or below
	unsigned char       MCS;
	unsigned char       BW;
	unsigned char       STBC;
	unsigned char       SHORTGI;
	unsigned char       rsv;
} OID_SET_HT_PHYMODE, *POID_SET_HT_PHYMODE;

#define MAX_NUM_OF_DLS_ENTRY        4
// structure for DLS
typedef struct _RT_802_11_DLS {
	unsigned short      TimeOut;        // unit: second , set by UI
	unsigned short      CountDownTimer; // unit: second , used by driver only
	unsigned char       MacAddr[6];     // set by UI
	unsigned char       Status;         // 0: none, 1: wait STAkey, 2: finish DLS setup, set by driver only
	unsigned char       Valid;          // 1: valid, 0: invalid, set by UI, use to setup or tear down DLS link
} RT_802_11_DLS, *PRT_802_11_DLS;

typedef enum _RT_802_11_DLS_MODE {
	DLS_NONE,
	DLS_WAIT_KEY,
	DLS_FINISH                                                                                                                                               } RT_802_11_DLS_MODE;

typedef enum _NDIS_802_11_AUTHENTICATION_MODE
{
	Ndis802_11AuthModeOpen,
	Ndis802_11AuthModeShared,
	Ndis802_11AuthModeAutoSwitch,
	Ndis802_11AuthModeWPA,
	Ndis802_11AuthModeWPAPSK,
	Ndis802_11AuthModeWPANone,
	Ndis802_11AuthModeWPA2,
	Ndis802_11AuthModeWPA2PSK,
	Ndis802_11AuthModeMax               // Not a real mode, defined as upper bound
} NDIS_802_11_AUTHENTICATION_MODE, *PNDIS_802_11_AUTHENTICATION_MODE;

typedef enum _NDIS_802_11_WEP_STATUS
{
	Ndis802_11WEPEnabled,
	Ndis802_11Encryption1Enabled = Ndis802_11WEPEnabled,
	Ndis802_11WEPDisabled,
	Ndis802_11EncryptionDisabled = Ndis802_11WEPDisabled,
	Ndis802_11WEPKeyAbsent,
	Ndis802_11Encryption1KeyAbsent = Ndis802_11WEPKeyAbsent,
	Ndis802_11WEPNotSupported,
	Ndis802_11EncryptionNotSupported = Ndis802_11WEPNotSupported,
	Ndis802_11Encryption2Enabled,
	Ndis802_11Encryption2KeyAbsent,
	Ndis802_11Encryption3Enabled,
	Ndis802_11Encryption3KeyAbsent
} NDIS_802_11_WEP_STATUS, *PNDIS_802_11_WEP_STATUS,
  NDIS_802_11_ENCRYPTION_STATUS, *PNDIS_802_11_ENCRYPTION_STATUS;

typedef enum _NDIS_802_11_POWER_MODE
{
	Ndis802_11PowerModeCAM,
	Ndis802_11PowerModeMAX_PSP,
	Ndis802_11PowerModeFast_PSP,
	Ndis802_11PowerModeMax      // not a real mode, defined as an upper bound
} NDIS_802_11_POWER_MODE, *PNDIS_802_11_POWER_MODE;

typedef enum _RT_802_11_PREAMBLE {
	Rt802_11PreambleLong,
	Rt802_11PreambleShort,
	Rt802_11PreambleAuto
} RT_802_11_PREAMBLE, *PRT_802_11_PREAMBLE;

#ifdef WPA_SUPPLICANT_SUPPORT
#define IDENTITY_LENGTH 32
#define CERT_PATH_LENGTH    64
#define PRIVATE_KEY_PATH_LENGTH 64

typedef enum _RT_WPA_SUPPLICANT_KEY_MGMT {
	Rtwpa_supplicantKeyMgmtWPAPSK,
	Rtwpa_supplicantKeyMgmtWPAEAP,
	Rtwpa_supplicantKeyMgmtIEEE8021X,
	Rtwpa_supplicantKeyMgmtNONE
} RT_WPA_SUPPLICANT_KEY_MGMT, *PRT_WPA_SUPPLICANT_KEY_MGMT;

typedef enum _RT_WPA_SUPPLICANT_EAP {
	Rtwpa_supplicantEAPMD5,
	Rtwpa_supplicantEAPMSCHAPV2,
	Rtwpa_supplicantEAPOTP,
	Rtwpa_supplicantEAPGTC,
	Rtwpa_supplicantEAPTLS,
	Rtwpa_supplicantEAPPEAP,
	Rtwpa_supplicantEAPTTLS,
	Rtwpa_supplicantEAPNONE
} RT_WPA_SUPPLICANT_EAP, *PRT_WPA_SUPPLICANT_EAP;

typedef enum _RT_WPA_SUPPLICANT_TUNNEL {
	Rtwpa_supplicantTUNNELMSCHAP,
	Rtwpa_supplicantTUNNELMSCHAPV2,
	Rtwpa_supplicantTUNNELPAP,
	Rtwpa_supplicantTUNNENONE
} RT_WPA_SUPPLICANT_TUNNEL, *PRT_WPA_SUPPLICANT_TUNNEL;
#endif

typedef struct _RT_PROFILE_SETTING {
	unsigned int                        ProfileDataType; //0x18140201
	unsigned char                       Profile[32+1];
	unsigned char                       SSID[NDIS_802_11_LENGTH_SSID+1];
	unsigned int                        SsidLen;
	unsigned int                        Channel;
	NDIS_802_11_AUTHENTICATION_MODE     Authentication; //Ndis802_11AuthModeOpen, Ndis802_11AuthModeShared, Ndis802_11AuthModeWPAPSK
	NDIS_802_11_WEP_STATUS              Encryption; //Ndis802_11WEPEnabled, Ndis802_11WEPDisabled, Ndis802_11Encryption2Enabled, Ndis802_11Encryption3Enabled
	NDIS_802_11_NETWORK_INFRASTRUCTURE  NetworkType;
	unsigned int                        KeyDefaultId;
	unsigned int                        Key1Type;
	unsigned int                        Key2Type;
	unsigned int                        Key3Type;
	unsigned int                        Key4Type;
	unsigned int                        Key1Length;
	unsigned int                        Key2Length;
	unsigned int                        Key3Length;
	unsigned int                        Key4Length;
	char                                Key1[26+1];
	char                                Key2[26+1];
	char                                Key3[26+1];
	char                                Key4[26+1];
	char                                WpaPsk[64+1]; //[32+1];
	unsigned int                        TransRate;
	unsigned int                        TransPower;
	char                                RTSCheck;  //boolean
	unsigned int                        RTS;
	char                                FragmentCheck; //boolean
	unsigned int                        Fragment;
	NDIS_802_11_POWER_MODE              PSmode;
	RT_802_11_PREAMBLE                  PreamType;
	unsigned int                        AntennaRx;
	unsigned int                        AntennaTx;
	unsigned int                        CountryRegion;
	//Advance
	//RT_802_11_TX_RATES                  ConfigSta;
	unsigned int                        AdhocMode;
	//unsigned char                       reserved[64];
	unsigned int                        Active; // 0 is the profile is set as connection profile, 1 is not.
#ifdef WPA_SUPPLICANT_SUPPORT
	RT_WPA_SUPPLICANT_KEY_MGMT          KeyMgmt;
	RT_WPA_SUPPLICANT_EAP               EAP;
	unsigned char                       Identity[IDENTITY_LENGTH];
	unsigned char                       Password[32];
	unsigned char                       CACert[CERT_PATH_LENGTH];
	unsigned char                       ClientCert[CERT_PATH_LENGTH];
	unsigned char                       PrivateKey[PRIVATE_KEY_PATH_LENGTH];
	unsigned char                       PrivateKeyPassword[32];
	//unsigned int                        EapolFlag;
	//RT_WPA_SUPPLICANT_PROTO             Proto;
	//RT_WPA_SUPPLICANT_PAIRWISE          Pairwise;
	//RT_WPA_SUPPLICANT_GROUP             group;
	//unsigned char                       Phase1[32];
	RT_WPA_SUPPLICANT_TUNNEL            Tunnel;
#endif
	struct  _RT_PROFILE_SETTING         *Next;
} RT_PROFILE_SETTING, *PRT_PROFILE_SETTING;

// move to station.c
//PRT_PROFILE_SETTING selectedProfileSetting = NULL, headerProfileSetting = NULL, currentProfileSetting = NULL;

typedef struct _NDIS_802_11_REMOVE_KEY
{
	unsigned int        Length;             // Length of this structure
	unsigned int        KeyIndex;
	unsigned char       BSSID[6];
} NDIS_802_11_REMOVE_KEY, *PNDIS_802_11_REMOVE_KEY;

// Key mapping keys require a BSSID
typedef struct _NDIS_802_11_KEY
{
    unsigned int        Length;             // Length of this structure
	unsigned int        KeyIndex;
	unsigned int        KeyLength;          // length of key in bytes
	unsigned char       BSSID[6];
	NDIS_802_11_KEY_RSC KeyRSC;
	unsigned char       KeyMaterial[1];     // variable length depending on above field
} NDIS_802_11_KEY, *PNDIS_802_11_KEY;

typedef struct _NDIS_802_11_WEP                                                                                                                              {
	unsigned int        Length;        // Length of this structure
	unsigned int        KeyIndex;      // 0 is the per-client key, 1-N are the global keys
	unsigned int        KeyLength;     // length of key in bytes
	unsigned char       KeyMaterial[1];// variable length depending on above field
} NDIS_802_11_WEP, *PNDIS_802_11_WEP;



