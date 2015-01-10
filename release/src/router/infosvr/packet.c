/************************************************************/
/*  Version 1.4     by Yuhsin_Lee 2005/1/19 16:31	   */
/************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <asm/byteorder.h>
#include "iboxcom.h"
#include "packet.h"

// AP:
// PackGetInfoCurrentAP		: Send command to get current setting of AP
// UnpackGetInfoCurrentAP	: Parse response to get current setting of AP
// PackSetInfoCurrentAP		: Send command to set current setting of AP	
// UnpackSetInfoCurrentAP	: Parse reponse to set current setting of AP	
// 
// WB:
// PackGetInfoCurrentSTA	: Send command to get current setting of station
// UnpackGetInfoCurrentSTA	: Parse response to get current setting of station
// PackGetInfoSites		: Send command to get site survey result
// UnpackGetInfoSites		: Parse reponse to get site survey result
// PackSetInfoCurrentSTA	: Send command to set current setting of station
// UnpackSetInfoCurrentSTA	: Parse response to set current setting of station
// PackGetInfoProfileSTA	: Send command to get profile
// UnpackGetInfoProfileSTA	: Parse reponse to get profile
// PackSetInfoProfiles	    : Send command to set profile	
// UnpackSetInfoProfileSTA	: Send command to set profile
 

DWORD transID=1;

DWORD GetTransactionID(void)
{
	transID++;
	
	if (transID==0) transID=1;
	
	return transID;
}


void PackKey(int keytype, char *keystr, char *key1, char *key2, char *key3, char *key4)
{			
	int i, j;
	char tmp[3];
	
	memset(keystr, 0, 64);
	
	if (keytype==ENCRYPTION_WEP64)
	{	
		j=5;
	}
	else
	{
		j=13;
	}
	
	for (i=0;i<j;i++)
	{
	   strncpy(tmp, key1+i*2, 2);
	   tmp[2] = 0;
	   keystr[i] = strtoul(tmp, 0, 16);
	}
	for (i=0;i<j;i++)
	{
	   strncpy(tmp, key2+i*2, 2);
	   tmp[2] = 0;
	   keystr[i+16] = strtoul(tmp, 0, 16);
	}
	for (i=0;i<j;i++)
	{
	   strncpy(tmp, key3+i*2, 2);
	   tmp[2] = 0;
	   keystr[i+32] = strtoul(tmp, 0, 16);
	}
	for (i=0;i<j;i++)
	{
	   strncpy(tmp, key4+i*2, 2);
	   tmp[2] = 0;
	   keystr[i+48] = strtoul(tmp, 0, 16);
	}
}

void UnpackKey(int keytype, char *keystr, char *key1, char *key2, char *key3, char *key4)
{	
	int i, j;
	
	if (keytype==ENCRYPTION_WEP64)	j=5;			
	else j=13;
	
	sprintf(key1,"");	
	sprintf(key2,"");		
	sprintf(key3,"");		
	sprintf(key4,"");			
	for (i=0;i<j;i++)
	{
	    sprintf(key1, "%s%02x", key1, (unsigned char)keystr[i]);
	    sprintf(key2, "%s%02x", key2, (unsigned char)keystr[i+16]);
	    sprintf(key3, "%s%02x", key3, (unsigned char)keystr[i+32]);
	    sprintf(key4, "%s%02x", key4, (unsigned char)keystr[i+48]);
	}	
}

DWORD PackCmdHdr(char *pdubuf, WORD cmd, char *mac, char *password)
{
	IBOX_COMM_PKT_HDR_EX *hdr;	      
						
	hdr=(IBOX_COMM_PKT_HDR_EX *)pdubuf;     						
	hdr->ServiceID = NET_SERVICE_ID_IBOX_INFO;
	hdr->PacketType = NET_PACKET_TYPE_CMD;  
	hdr->OpCode = __cpu_to_le16(cmd);
	hdr->Info = __cpu_to_le32(GetTransactionID());
	memcpy(hdr->MacAddress, mac, 6);
	memcpy(hdr->Password, password, 32);   
	return (hdr->Info);
}	

int UnpackResHdrNoCheck(char *pdubuf)
{
	IBOX_COMM_PKT_RES_EX *hdr;
		
	hdr = (IBOX_COMM_PKT_RES_EX *)pdubuf;
		
	if (hdr->ServiceID!=NET_SERVICE_ID_IBOX_INFO || 
	    hdr->PacketType!=NET_PACKET_TYPE_RES)
	{	
	    return (RESPONSE_HDR_IGNORE);
	}
	
	if ((hdr->OpCode&0xff00)==NET_RES_ERR_PASSWORD)
	{
	    return (RESPONSE_HDR_ERR_PASSWORD);
	}
	
	if ((hdr->OpCode&0xff00)==NET_RES_ERR_FIELD_UNDEF)
	{
	    return (RESPONSE_HDR_ERR_UNSUPPORT);
	}				
	return (RESPONSE_HDR_OK);
}

int UnpackResHdr(char *pdubuf, WORD opcode, DWORD tid, char *mac)
{
	IBOX_COMM_PKT_RES_EX *hdr;
		
	hdr = (IBOX_COMM_PKT_RES_EX *)pdubuf;

	if (hdr->ServiceID!=NET_SERVICE_ID_IBOX_INFO || 
	    hdr->PacketType!=NET_PACKET_TYPE_RES ||	    
	    hdr->Info != __cpu_to_le32(tid) ||
	    memcmp(hdr->MacAddress, mac, 6)!=0)
	    return (RESPONSE_HDR_IGNORE);
	
	if ((hdr->OpCode&0xff00)==NET_RES_ERR_PASSWORD)
	{
	    return (RESPONSE_HDR_ERR_PASSWORD);
	}
	
	if ((hdr->OpCode&0xff00)==NET_RES_ERR_FIELD_UNDEF)
	{
	    return (RESPONSE_HDR_ERR_UNSUPPORT);
	}
			
	return (RESPONSE_HDR_OK);
}

int PackGetInfo(char *pdubuf)
{
	IBOX_COMM_PKT_HDR_EX *hdr;	      
						
	hdr=(IBOX_COMM_PKT_HDR_EX *)pdubuf;     						
	hdr->ServiceID = NET_SERVICE_ID_IBOX_INFO;
	hdr->PacketType = NET_PACKET_TYPE_CMD;  
	hdr->OpCode = __cpu_to_le16(NET_CMD_ID_GETINFO);
	hdr->Info = 0;	 
			
	return (0);
}

int UnpackGetInfo(char *pdubuf, PKT_GET_INFO *Info)
{
	IBOX_COMM_PKT_RES_EX *hdr;
		
	hdr = (IBOX_COMM_PKT_RES_EX *)pdubuf;
		
	if (hdr->ServiceID!=NET_SERVICE_ID_IBOX_INFO || 
	    hdr->PacketType!=NET_PACKET_TYPE_RES ||
	    __le16_to_cpu(hdr->OpCode) != NET_CMD_ID_GETINFO)
	    return (RESPONSE_HDR_IGNORE);
	
	
	memcpy(Info, pdubuf+sizeof(IBOX_COMM_PKT_RES), sizeof(PKT_GET_INFO));
	return (RESPONSE_HDR_OK);	
}

int PackGetInfoCurrentAP(char *pdubuf, char *mac, char *password)
{
	PKT_GET_INFO_EX1 *body;		 
	DWORD tid;
	
	tid = PackCmdHdr(pdubuf, NET_CMD_ID_GETINFO_EX, mac, password);
	body = (PKT_GET_INFO_EX1 *)(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX));			
	body->FieldCount = 1;
	body->FieldID = __cpu_to_le16(FIELD_GENERAL_CURRENT_AP);
	
	return (tid);
}

int UnpackGetInfoCurrentAP(char *pdubuf, DWORD tid, char *mac, PKT_GET_INFO_AP *curAP)
{
	PKT_GET_INFO_EX1 *body;
	int err;
	
	err = UnpackResHdr(pdubuf, NET_CMD_ID_GETINFO_EX, tid, mac);
	
	if (err!=RESPONSE_HDR_OK) return (err);
	
	body = (PKT_GET_INFO_EX1 *)(pdubuf+sizeof(IBOX_COMM_PKT_RES_EX));
			
	if (body->FieldCount>0 &&
	    __le16_to_cpu(body->FieldID) == FIELD_GENERAL_CURRENT_AP)
	{		
		memcpy(curAP, pdubuf+sizeof(IBOX_COMM_PKT_RES_EX)+sizeof(PKT_GET_INFO_EX1), sizeof(PKT_GET_INFO_AP));
		return (RESPONSE_HDR_OK);
	}
	else return (RESPONSE_HDR_IGNORE);
}

int PackSetInfoCurrentAP(char *pdubuf, char *mac, char *password, PKT_GET_INFO_AP *curAP)
{
	PKT_GET_INFO_EX1 *body;
	DWORD tid;
	
	tid = PackCmdHdr(pdubuf, NET_CMD_ID_SETINFO, mac, password);
	body = (PKT_GET_INFO_EX1 *)(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX));
	body->FieldCount = 1;
	body->FieldID = __cpu_to_le16(FIELD_GENERAL_CURRENT_AP);
	memcpy(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX)+sizeof(PKT_GET_INFO_EX1), curAP, sizeof(PKT_GET_INFO_AP));
	return (tid);
}

int UnpackSetInfoCurrentAP(char *pdubuf, DWORD tid, char *mac)
{
	int err;
	
	err = UnpackResHdr(pdubuf, NET_CMD_ID_SETINFO, tid, mac);
	
	return (err);		
}

int PackGetInfoCurrentSTA(char *pdubuf, char *mac, char *password)
{
	PKT_GET_INFO_EX1 *body;		 
	DWORD tid;
	
	tid = PackCmdHdr(pdubuf, NET_CMD_ID_GETINFO_EX, mac, password);
	body = (PKT_GET_INFO_EX1 *)(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX));
	body->FieldCount = 1;
	body->FieldID = __cpu_to_le16(FIELD_GENERAL_CURRENT_STA);
	return (tid);
}

int UnpackGetInfoCurrentSTA(char *pdubuf, DWORD tid, char *mac, PKT_GET_INFO_STA *curSTA)
{
	PKT_GET_INFO_EX1 *body;
	int err;
	
	err = UnpackResHdr(pdubuf, NET_CMD_ID_GETINFO_EX, tid, mac);
	
	if (err!=RESPONSE_HDR_OK) return (err);
	
	body = (PKT_GET_INFO_EX1 *)(pdubuf+sizeof(IBOX_COMM_PKT_RES_EX));
			
	if (body->FieldCount>0 &&
	    __le16_to_cpu(body->FieldID) == FIELD_GENERAL_CURRENT_STA)
	{		
		memcpy(curSTA, pdubuf+sizeof(IBOX_COMM_PKT_RES_EX)+sizeof(PKT_GET_INFO_EX1), sizeof(PKT_GET_INFO_STA));
		return (RESPONSE_HDR_OK);
	}
	else return (RESPONSE_HDR_IGNORE);
}

int PackSetInfoCurrentSTA(char *pdubuf, char *mac, char *password, PKT_GET_INFO_STA *curSTA)
{
	PKT_GET_INFO_EX1 *body;
	DWORD tid;
	
	tid = PackCmdHdr(pdubuf, NET_CMD_ID_SETINFO, mac, password);
	body = (PKT_GET_INFO_EX1 *)(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX));
	body->FieldCount = 1;
	body->FieldID = __cpu_to_le16(FIELD_GENERAL_CURRENT_STA);
	memcpy(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX)+sizeof(PKT_GET_INFO_EX1), curSTA, sizeof(PKT_GET_INFO_STA));
	return (tid);
}

int UnpackSetInfoCurrentSTA(char *pdubuf, DWORD tid, char *mac)
{
//	PKT_GET_INFO_EX1 *body;
	int err;
	
	err = UnpackResHdr(pdubuf, NET_CMD_ID_SETINFO, tid, mac);
	return (err);		
}

int PackGetInfoSites(char *pdubuf, char *mac, char *password)
{
//	PKT_GET_INFO_EX1 *body;		 
	DWORD tid;
	
	tid = PackCmdHdr(pdubuf, NET_CMD_ID_GETINFO_SITES, mac, password);
	return (tid);
}

int UnpackGetInfoSites(char *pdubuf, DWORD tid, char *mac, PKT_GET_INFO_SITES *sites)
{
	PKT_GET_INFO_EX1 *body;
	int err;
	
	err = UnpackResHdr(pdubuf, NET_CMD_ID_GETINFO_SITES, tid, mac);
	
	if (err!=RESPONSE_HDR_OK) return (err);
	
	body = (PKT_GET_INFO_EX1 *)(pdubuf+sizeof(IBOX_COMM_PKT_RES_EX));
						
	memcpy(sites, pdubuf+sizeof(IBOX_COMM_PKT_RES_EX), sizeof(PKT_GET_INFO_SITES));
	return (RESPONSE_HDR_OK);
}

int PackGetInfoProfiles(char *pdubuf, char *mac, char *password, int start, int count)
{
	PKT_GET_INFO_PROFILE *profile;		 
	DWORD tid;
	
	profile=(PKT_GET_INFO_PROFILE *)(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX));
	tid = PackCmdHdr(pdubuf, NET_CMD_ID_GETINFO_PROF, mac, password);
	profile->StartIndex = start;
	profile->Count = count;	
	return (tid);
}

int UnpackGetInfoProfiles(char *pdubuf, DWORD tid, char *mac, PKT_GET_INFO_PROFILE *profile)
{	
	int err;
	
	err = UnpackResHdr(pdubuf, NET_CMD_ID_GETINFO_PROF, tid, mac);
	
	if (err!=RESPONSE_HDR_OK) return (err);
								
	memcpy(profile, pdubuf+sizeof(IBOX_COMM_PKT_RES_EX), sizeof(PKT_GET_INFO_PROFILE));
	return (RESPONSE_HDR_OK);
}

int PackSetInfoProfiles(char *pdubuf, char *mac, char *password, PKT_GET_INFO_PROFILE *profile)
{
//	PKT_GET_INFO_EX1 *body;
	DWORD tid;

	tid = PackCmdHdr(pdubuf, NET_CMD_ID_SETINFO_PROF, mac, password);
	memcpy(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX), profile, sizeof(PKT_GET_INFO_PROFILE));
	return (tid);
}

int UnpackSetInfoProfiles(char *pdubuf, DWORD tid, char *mac)
{
//	PKT_GET_INFO_EX1 *body;
	int err;

	err = UnpackResHdr(pdubuf, NET_CMD_ID_SETINFO_PROF, tid, mac);
	return (err);
}

int PackCheckPassword(char *pdubuf, char *mac, char *password)
{
//	PKT_GET_INFO_EX1 *body;
	DWORD tid;

	tid = PackCmdHdr(pdubuf, NET_CMD_ID_CHECK_PASS, mac, password);
	return (tid);
}

// only 2 return value
//RESPONSE_HDR_ERR_PASSWORD
//RESPONSE_HDR_OK
int UnpackCheckPassword(char *pdubuf, DWORD tid, char *mac)
{
//	PKT_GET_INFO_EX1 *body;
	int err;

	err = UnpackResHdr(pdubuf, NET_CMD_ID_CHECK_PASS, tid, mac);
	return (err);
}

// Added by Chen-I to Handle NET_CMD_ID_MANU_CMD
int PackSysCmd(char *pdubuf, char *mac, char *password, PKT_SYSCMD *cmd)
{
//	PKT_GET_INFO_EX1 *body;
	DWORD tid;
	
	tid = PackCmdHdr(pdubuf, NET_CMD_ID_MANU_CMD, mac, password);
	memcpy(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX), cmd, sizeof(PKT_SYSCMD));
	return (tid);
}

int UnpackSysCmdRes(char *pdubuf, DWORD tid, char *mac, PKT_SYSCMD_RES *sysres)
{
//	PKT_GET_INFO_EX1 *body;
	int err;
	
	err = UnpackResHdr(pdubuf, NET_CMD_ID_MANU_CMD, tid, mac);	
	if (err!=RESPONSE_HDR_OK) return (err);
								
	memcpy(sysres, pdubuf+sizeof(IBOX_COMM_PKT_RES_EX), sizeof(PKT_SYSCMD_RES));
	return (RESPONSE_HDR_OK);
}


#ifdef BTN_SETUP
int PackSetPubKey(char *pdubuf, char *mac, char *password, PKT_SET_INFO_GW_QUICK_KEY *key)
{
	DWORD tid;
	
	tid = PackCmdHdr(pdubuf, NET_CMD_ID_SETKEY_EX, mac, password);
	memcpy(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX), key, sizeof(PKT_SET_INFO_GW_QUICK_KEY));
	return (tid);
}

int UnpackSetPubKeyRes(char *pdubuf, DWORD tid, char *mac, PKT_SET_INFO_GW_QUICK_KEY *key)
{
	int err;
	
	err = UnpackResHdr(pdubuf, NET_CMD_ID_SETKEY_EX, tid, mac);	
	if (err!=RESPONSE_HDR_OK) return (err);
								
	memcpy(key, pdubuf+sizeof(IBOX_COMM_PKT_RES_EX), sizeof(PKT_SET_INFO_GW_QUICK_KEY));
	return (RESPONSE_HDR_OK);
}

#ifdef ENCRYPTION
#define BLOCKLEN 16

void Encrypt(int klen, unsigned char *key, unsigned char *ptext, int tlen, unsigned char *ctext)
{
	unsigned char *pptr, *cptr;
	int i;

	i = 0;
	pptr = ptext;
	cptr = ctext;

	while (1)
	{
		aes_encrypt(klen, key, pptr, cptr);
		i+=16;
		if (i>=tlen) break;		
		pptr+=16;
		cptr+=16;
	}
}

void Decrypt(int klen, unsigned char *key, unsigned char *ptext, int tlen, unsigned char *ctext)
{
	unsigned char *pptr, *cptr;
	int i;

	i = 0;
	pptr = ptext;
	cptr = ctext;

	while (1)
	{
		aes_decrypt(klen, key, pptr, cptr);
		i+=16;
		if (i>=tlen) break;		
		pptr+=16;
		cptr+=16;
	}
}
#endif


int PackEZProbe(char *pdubuf, char *mac, char *password, char *key, int klen)
{
	DWORD tid;	
	char tmpbuf[INFO_PDU_LENGTH];
	
	tid = PackCmdHdr(tmpbuf, NET_CMD_ID_EZPROBE, mac, password);

#ifdef ENCRYPTION
	if (klen) Encrypt(klen, key, tmpbuf, INFO_PDU_LENGTH, pdubuf);
	else memcpy(pdubuf, tmpbuf, INFO_PDU_LENGTH);
#else
	memcpy(pdubuf, tmpbuf, INFO_PDU_LENGTH);
#endif
	return (tid);
}

int UnpackEZProbeRes(char *pdubuf, DWORD tid, char *mac, PKT_EZPROBE_INFO *info, char *key, int klen)
{
	int err;
	char tmpbuf[INFO_PDU_LENGTH];	

#ifdef ENCRYPTION
	if (klen) Decrypt(klen, key, pdubuf, INFO_PDU_LENGTH, tmpbuf);
	else memcpy(tmpbuf, pdubuf, INFO_PDU_LENGTH);
#else
	memcpy(tmpbuf, pdubuf, INFO_PDU_LENGTH);
#endif	

	
	err = UnpackResHdr(pdubuf, NET_CMD_ID_EZPROBE, tid, mac);	
	if (err!=RESPONSE_HDR_OK) return (err);
								
	memcpy(info, pdubuf+sizeof(IBOX_COMM_PKT_RES_EX), sizeof(PKT_EZPROBE_INFO));
	return (RESPONSE_HDR_OK);
}

int PackSetInfoGWQuick(char *pdubuf, char *mac, char *password, PKT_SET_INFO_GW_QUICK *setting, char *key, int klen)
{
	DWORD tid;
	char tmpbuf[INFO_PDU_LENGTH];
	
	tid = PackCmdHdr(tmpbuf, NET_CMD_ID_QUICKGW_EX, mac, password);

	memcpy(tmpbuf+sizeof(IBOX_COMM_PKT_HDR_EX), setting, sizeof(PKT_SET_INFO_GW_QUICK));

#ifdef ENCRYPTION
	Encrypt(klen, key, tmpbuf, INFO_PDU_LENGTH, pdubuf);
#else
	memcpy(pdubuf, tmpbuf, INFO_PDU_LENGTH);
#endif
	return (tid);
}

int UnpackSetInfoGWQuickRes(char *pdubuf, DWORD tid, char *mac, PKT_SET_INFO_GW_QUICK *setting, char *key, int klen)
{
	int err;
	int i;
	char tmpbuf[INFO_PDU_LENGTH];	

#ifdef ENCRYPTION	
	Decrypt(klen, key, pdubuf, INFO_PDU_LENGTH, tmpbuf);
#else
	memcpy(tmpbuf, pdubuf, INFO_PDU_LENGTH);
#endif	
	err = UnpackResHdr(tmpbuf, NET_CMD_ID_QUICKGW_EX, tid, mac);
	if (err!=RESPONSE_HDR_OK) return (err);

	memcpy(setting, tmpbuf+sizeof(IBOX_COMM_PKT_RES_EX), sizeof(PKT_SET_INFO_GW_QUICK));
	return (RESPONSE_HDR_OK);
}
#endif
