#ifndef __PACKET_H__
#define __PACKET_H__

#ifdef __cplusplus
extern "C" {
#endif

// Internal error message
enum  RESPOSNE_HDR 
{
	RESPONSE_HDR_OK = 0,
	RESPONSE_HDR_IGNORE,
	RESPONSE_HDR_ERR_PASSWORD,
	RESPONSE_HDR_ERR_TID,
	RESPONSE_HDR_ERR_UNSUPPORT
};

#ifdef REMOVE
// discovery:
int PackGetInfo(char *pdubuf);
int UnpackGetInfo(char *pdubuf, PKT_GET_INFO *Info);

// AP:
int PackGetInfoCurrentAP(char *pdubuf, char *mac, char *password);
int UnpackGetInfoCurrentAP(char *pdubuf, WORD opcode, DWORD tid, char *mac, PKT_GET_INFO_AP *curAP);
int PackSetInfoCurrentAP(char *pdubuf, char *mac, char *password, PKT_GET_INFO_AP *curAP);
int UnpackSetInfoCurrentAP(char *pdubuf, WORD opcode, DWORD tid, char *mac);

// WB:
int PackGetInfoCurrentSTA(char *pdubuf, char *mac, char *password);
int UnpackGetInfoCurrentSTA(char *pdubuf, WORD opcode, DWORD tid, char *mac, PKT_GET_INFO_STA *curSTA);
int PackGetInfoSites(char *pdubuf, char *mac, char *password);
int UnpackGetInfoSites(char *pdubuf, WORD opcode, DWORD tid, char *mac, PKT_GET_INFO_SITES *sites);
int PackSetInfoCurrentSTA(char *pdubuf, char *mac, char *password, PKT_GET_INFO_STA *curSTA);
int UnpackSetInfoCurrentSTA(char *pdubuf, WORD opcode, DWORD tid, char *mac);
#endif

#ifdef __cplusplus
}
#endif

#endif // #ifndef __PACKET_H__
 
