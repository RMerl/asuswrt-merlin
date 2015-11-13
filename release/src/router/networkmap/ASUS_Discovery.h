//
//  ASUS_Discovery.h
//  asus
//
//  Created by Junda Txia on 11/22/10.
//  Copyright ASUSTek COMPUTER INC. 2011. All rights reserved.
//

#include <arpa/inet.h>       //inet_addr function

#include "netinet/in.h"

#include "iboxcom.h"

typedef struct _SearchRouterInfoStruct
{
    char routerProductID[32];
    char routerIPAddress[32];
    char routerSubMask[32];
    unsigned char routerMacAddress[6];
    unsigned char routerRealMacAddress[18];
    char routerSSID[33];			/* maximum length of SSID is 32 characters. */
    char routerFirmwareVersion[16];
    char routerOperationMode;
    char routerRegulation;
    char routerPrinterInfo[128];
    
    // webdav
    char webdavSupport;
    char webdavEnableWebDav;
    char webdavHttpType;
    unsigned short webdavHttpPort;
    unsigned short webdavHttpsPort;
    char webdavEnableDDNS;
    char webdavHostName[64];
    unsigned int webdavWANIPAddr;
    char webdavWANIPAddress[32];
    char webdavWANState;
    char webdavIsNotDefault;
    
    // tunnel
    char tunnelSupport;
    unsigned short tunnelAppHttpPort;
    char tunnelAppAPILevel;
    char tunnelEnableAAE;
    char tunnelAAEDeviceID[64];
    
} SearchRouterInfoStruct;

#define LISTEN_PORT 9990
#define INFO_SERVER_PORT 9999
#define SERV_IP	"255.255.255.255"
#define MAX_SEARCH_ROUTER  127

extern int a_bEndApp;
extern int a_GetRouterCount;
extern SearchRouterInfoStruct searchRouterInfo[MAX_SEARCH_ROUTER];

int ASUS_Discovery();
int ParseASUSDiscoveryPackage(int socket);

void PROCESS_UNPACK_GET_INFO(char *pbuf, struct sockaddr_in from_addr);
