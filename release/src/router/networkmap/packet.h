/************************************************************/
/*  Version 1.4     by Yuhsin_Lee 2005/1/19 16:31           */
/************************************************************/
	
#ifndef packetH
#define packetH

#ifdef __cplusplus
extern "C" {
#endif
	
#include "iboxcom.h"
	
// constants for regulatory domain
#define FCC				(0x10)  //channel 1-11
//#define CANADA			(0x20)  //channel 1-11 => just use FCC
#define ETSI			(0x30)  //channel 1-13
//#define SPAIN				(0x31)  //channel 10-11
//#define FRANCE			(0x32)  //channel 10-13
//#define MKK				(0x40)  //channel 14
#define MKK1			(0xff)  //channel 1-14
	
#define MIN_FCC         (1)
#define MAX_FCC         (11)
	
#define MIN_ETSI        (1)
#define MAX_ETSI        (13)

/*
#define MIN_SPAIN       (10)
#define MAX_SPAIN       (11)
	 
#define MIN_FRANCE      (10)
#define MAX_FRANCE      (13)
	 
#define MIN_MKK         (14)
#define MAX_MKK         (14)
*/
	
#define MIN_MKK1        (1)
#define MAX_MKK1        (14)
	
// Internal error message
enum  RESPOSNE_HDR
{
	RESPONSE_HDR_OK = 0,
	RESPONSE_HDR_IGNORE,
	RESPONSE_HDR_ERR_PASSWORD,
	RESPONSE_HDR_ERR_TID,
	RESPONSE_HDR_ERR_UNSUPPORT,
    RESPONSE_HDR_OK_SUPPORT_WEBDAV,
    RESPONSE_HDR_OK_SUPPORT_WEBDAV_TUNNEL
};
	
enum  WAITING_FUNCTION
{
	UNPACK_GET_INFO = 0,
	UNPACK_GET_INFO_CURRENT_AP,
	UNPACK_SET_INFO_CURRENT_AP,
	UNPACK_GET_INFO_CURRENT_STA,
	UNPACK_GET_INFO_SITES,
	UNPACK_SET_INFO_CURRENT_STA,
	UNPACK_GET_INFO_PROFILES,
	UNPACK_SET_INFO_PROFILES,
	UNPACK_CHECK_PASSWORD,
	UNPACK_SysCmdRes,
	NONE
};

enum COMM_STATE_ENUM
{
	COMM_STATE_IDLE = 0x00,
	COMM_STATE_WAIT
};

enum WEP_STATE
{
	WEP_DISABLED = 0x00,
	WEP_ENABLED
};

//---------------------------------------------------------------------------

//Header
int UnpackResHdr(char *pdubuf, WORD opcode, DWORD tid, char *mac);

// discovery:
int PackGetInfo(char *pdubuf);
int UnpackGetInfo(char *pdubuf, PKT_GET_INFO *Info);
int PackGetInfo_NEW(char *pdubuf);
int UnpackGetInfo_NEW(char *pdubuf, PKT_GET_INFO *discoveryInfo, STORAGE_INFO_T *storageInfo);

// AP:
int PackGetInfoCurrentAP(char *pdubuf, char *mac, char *password);
int UnpackGetInfoCurrentAP(char *pdubuf, DWORD tid, char *mac, PKT_GET_INFO_AP *curAP);
int PackSetInfoCurrentAP(char *pdubuf, char *mac, char *password, PKT_GET_INFO_AP *curAP);
int UnpackSetInfoCurrentAP(char *pdubuf, DWORD tid, char *mac);

// WB:
int PackGetInfoCurrentSTA(char *pdubuf, char *mac, char *password);
int UnpackGetInfoCurrentSTA(char *pdubuf, DWORD tid, char *mac, PKT_GET_INFO_STA *curSTA);
int PackGetInfoSites(char *pdubuf, char *mac, char *password);
int UnpackGetInfoSites(char *pdubuf, DWORD tid, char *mac, PKT_GET_INFO_SITES *sites);
int PackSetInfoCurrentSTA(char *pdubuf, char *mac, char *password, PKT_GET_INFO_STA *curSTA);
int UnpackSetInfoCurrentSTA(char *pdubuf, DWORD tid, char *mac);
int PackGetInfoProfiles(char *pdubuf, char *mac, char *password, int start, int count);
int UnpackGetInfoProfiles(char *pdubuf, DWORD tid, char *mac, PKT_GET_INFO_PROFILE *profile);
int PackSetInfoProfiles(char *pdubuf, char *mac, char *password, PKT_GET_INFO_PROFILE *profile);
int UnpackSetInfoProfiles(char *pdubuf, DWORD tid, char *mac);

// WEP key
void PackKey(int keytype, char *keystr, char *key1, char *key2, char *key3, char *key4);
void UnpackKey(int keytype, char *keystr, char *key1, char *key2, char *key3, char *key4);

// check password
int PackCheckPassword(char *pdubuf, char *mac, char *password);
int UnpackCheckPassword(char *pdubuf, DWORD tid, char *mac);
	
//others
int PackSysCmd(char *pdubuf, char *mac, char *password, PKT_SYSCMD *cmd);
int UnpackSysCmdRes(char *pdubuf, DWORD tid, char *mac, PKT_SYSCMD_RES *sysres);
	
// EZSetup
int UnpackEZProbeRes(char *pdubuf, DWORD tid, char *mac, PKT_EZPROBE_INFO *info, char *key, int klen);
int PackSetPubKey(char *pdubuf, char *mac, char *password, PKT_SET_INFO_GW_QUICK_KEY *key);
int UnpackSetPubKeyRes(char *pdubuf, DWORD tid, char *mac, PKT_SET_INFO_GW_QUICK_KEY *key);
int PackEZProbe(char *pdubuf, char *mac, char *password, char *key, int klen);
int UnpackEZProbeRes(char *pdubuf, DWORD tid, char *mac, PKT_EZPROBE_INFO *info, char *key, int klen);
int PackSetInfoGWQuick(char *pdubuf, char *mac, char *password, PKT_SET_INFO_GW_QUICK *setting, char *key, int klen);
int UnpackSetInfoGWQuickRes(char *pdubuf, DWORD tid, char *mac, PKT_SET_INFO_GW_QUICK *setting, char *key, int klen);

// ENCRYPTION
void Encrypt(int klen, unsigned char *key, unsigned char *ptext, int tlen, unsigned char *ctext);
void Decrypt(int klen, unsigned char *key, unsigned char *ptext, int tlen, unsigned char *ctext);
//---------------------------------------------------------------------------
	
#ifdef __cplusplus
}
#endif

#endif//#ifndef packetH

