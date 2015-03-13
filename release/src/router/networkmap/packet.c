/************************************************************/
/*  Version 1.4     by Yuhsin_Lee 2005/1/19 16:31           */
/************************************************************/

#include <stdio.h>    //sprintf function
#include <memory.h>   //memset  function
#include <string.h>   //strncpy function
#include <stdlib.h>   //strtoul function

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
	
	if (transID==0)	transID=1;
	
	return transID;
}

void PackKey(int keytype, char *keystr, char *key1, char *key2, char *key3, char *key4)
{			
	int i, j;
	char tmp[3];
	
	memset(keystr, 0, 64);
	
	if (keytype == ENCRYPTION_WEP64)
	{	
		j=5;
	}
	else
	{
		j=13;
	}
	
	for(i=0;i<j;i++)
	{
		strncpy(tmp, key1+i*2, 2);
		tmp[2] = 0;
		keystr[i] = strtoul(tmp, 0, 16);
	}
	for(i=0;i<j;i++)
	{
		strncpy(tmp, key2+i*2, 2);
		tmp[2] = 0;
		keystr[i+16] = strtoul(tmp, 0, 16);
	}
	for(i=0;i<j;i++)
	{
		strncpy(tmp, key3+i*2, 2);
		tmp[2] = 0;
		keystr[i+32] = strtoul(tmp, 0, 16);
	}
	for(i=0;i<j;i++)
	{
		strncpy(tmp, key4+i*2, 2);
		tmp[2] = 0;
		keystr[i+48] = strtoul(tmp, 0, 16);
	}
}

int UnpackGetInfo(char *pdubuf, PKT_GET_INFO *Info)
{
	IBOX_COMM_PKT_RES *hdr;
	
	hdr = (IBOX_COMM_PKT_RES *)pdubuf;
	
	if (hdr->ServiceID!=NET_SERVICE_ID_IBOX_INFO ||   //0x0C 12
	    hdr->PacketType!=NET_PACKET_TYPE_RES ||       //0x16 22
	    hdr->OpCode!=NET_CMD_ID_GETINFO)	    	  //0x1F 31
	    return (RESPONSE_HDR_IGNORE);
	
	// save after the header
	memcpy(Info, pdubuf+sizeof(IBOX_COMM_PKT_RES), sizeof(PKT_GET_INFO));
	
	return(RESPONSE_HDR_OK);
}

int UnpackGetInfo_NEW(char *pdubuf, PKT_GET_INFO *discoveryInfo, STORAGE_INFO_T *storageInfo)
{
	IBOX_COMM_PKT_RES *hdr;
	
	hdr = (IBOX_COMM_PKT_RES *)pdubuf;
	
	if (hdr->ServiceID != NET_SERVICE_ID_IBOX_INFO ||       //0x0C 12
	    hdr->PacketType != NET_PACKET_TYPE_RES ||           //0x16 22
	    hdr->OpCode != NET_CMD_ID_GETINFO)                  //0x1F 31
	    return (RESPONSE_HDR_IGNORE);
	
	// get discovery info 
	memcpy(discoveryInfo, pdubuf+sizeof(IBOX_COMM_PKT_RES), sizeof(PKT_GET_INFO));
    
    STORAGE_INFO_T *tempStorageInfo = (STORAGE_INFO_T *)(pdubuf + sizeof(IBOX_COMM_PKT_RES) + sizeof(PKT_GET_INFO) + sizeof(WS_INFO_T));

    if (tempStorageInfo->MagicWord != EXTEND_MAGIC) // 0x8082 32898
        return (RESPONSE_HDR_OK);
    
    if ((tempStorageInfo->ExtendCap & EXTEND_CAP_WEBDAV) != EXTEND_CAP_WEBDAV)    // 0x0001 1
        return (RESPONSE_HDR_OK);

    memcpy(storageInfo, tempStorageInfo, sizeof(STORAGE_INFO_T));
    
    if ((tempStorageInfo->ExtendCap & EXTEND_CAP_AAE_BASIC) != EXTEND_CAP_AAE_BASIC)    // 0x0010 16
        return (RESPONSE_HDR_OK_SUPPORT_WEBDAV);

	return (RESPONSE_HDR_OK_SUPPORT_WEBDAV_TUNNEL);
}