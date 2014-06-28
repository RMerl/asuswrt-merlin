//
//  ASUS_Discovery.h
//  asus
//
//  Created by Junda Txia on 11/22/10.
//  Copyright ASUSTek COMPUTER INC. 2011. All rights reserved.
//

#include "netinet/in.h"

#include "iboxcom.h"

typedef struct _SearchRouterInfoStruct
{
    char PrinterInfo[128];
    char SSID[64];
    char SubMask[32];
    char ProductID[32];
    char FirmwareVersion[16];
    char OperationMode;
    unsigned char RealMacAddress[18];
    char Regulation;
    char IPAddress[32];
    unsigned char MacAddress[6];
    char RouterOperateMode;
    char SupportWebdavDDNSInfo;
    char EnableWebDav;
	char HttpType;
	unsigned short HttpPort;
    unsigned short HttpsPort;
	char EnableDDNS;
	char HostName[64];
  	unsigned int WANIPAddr;
    char WANIPAddress[32];
    char WANState;
    char IsNotDefault;
} SearchRouterInfoStruct;

typedef struct _WebdavRouterInfoStruct
{
    char ProductID[32];
    char DDNS[128];
    char IPAddress[32];
    unsigned char Mac[17];
} WebdavRouterInfoStruct;

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
//void PROCESS_UNPACK_GET_INFO_NEW(char *pbuf, struct sockaddr_in from_addr);


