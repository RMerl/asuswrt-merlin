//
//  ASUS_Discovery.c
//  asus
//
//  Created by Junda Txia on 11/22/10.
//  Copyright ASUSTek COMPUTER INC. 2011. All rights reserved.
//

#include <string.h>          //memset function
#include <stdio.h>           //sprintf function
#include <sys/socket.h>      //socket function
#include <sys/errno.h>       //errorno function
#include <sys/poll.h>        //struct pollfd
#include <netinet/in.h>      //const IPPROTO_UDP
#include <arpa/inet.h>       //inet_addr function
#include <unistd.h>          //close function
#include <bcmnvram.h>
#include <asm/byteorder.h>
#include "iboxcom.h"         //const INFO_PDU_LENGTH
#include "packet.h"          //const RESPONSE_HDR_OK
#include "asusdiscovery.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(ary) (sizeof(ary) / sizeof((ary)[0]))
#endif

//char txMac[6] = {0};
int a_bEndApp = 0;
int a_socket = 0;
int a_GetRouterCount = 0;

SearchRouterInfoStruct searchRouterInfo[MAX_SEARCH_ROUTER];

int asusdiscovery()
{	
	//printf("----------ASUS_Discovery Start----------\n");
    
	int iRet = 0;
	char router_ipaddr[17];
	unsigned char bcast_ipaddr[4];
    
	//initial search router count
	a_GetRouterCount = 0;
	// clean structure
	memset(&searchRouterInfo[0], 0, MAX_SEARCH_ROUTER * sizeof(SearchRouterInfoStruct));
    
	if (a_socket != 0)
	{
		close(a_socket);
		a_socket = 0;
	}

	a_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (a_socket == 0)
	{
		printf("Create socket failed\n");
        	return 0;
    	}

       	int reuseaddr = 1;
	if (setsockopt(a_socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) < 0) 
	{
		printf("LINE : SO_REUSEADDR setsockopt() error!!!\n");
		return 0;
	}		

	char *lan_ifname;
	lan_ifname = nvram_safe_get("lan_ifname");
	setsockopt(a_socket, SOL_SOCKET, SO_BINDTODEVICE, lan_ifname, strlen(lan_ifname));

	// set broadcast flag
	int broadcast = 1;
	int iRes = setsockopt(a_socket, SOL_SOCKET , SO_BROADCAST ,(char *)&broadcast, sizeof(broadcast));
	if (iRes != 0)
	{
		printf("setsockopt failed\n");
		close(a_socket);
		a_socket = 0;
		return 0;
	}

	// set the port and address we want to listen on
	struct sockaddr_in clit;
	memset(&clit, 0, sizeof(clit));
	//clit.sin_len = sizeof(clit);
	clit.sin_family = AF_INET;
	clit.sin_port = htons(INFO_SERVER_PORT);
	clit.sin_addr.s_addr = htonl(INADDR_ANY);
    
	if (bind(a_socket, (struct sockaddr *)&clit, sizeof(clit)) < 0)
	{
		printf("could not bind to address\n");
		close(a_socket);
		a_socket = 0;
		return 0;
	}

	struct sockaddr_in serv;

	//Get Router's IP
	strcpy(router_ipaddr, nvram_safe_get("lan_ipaddr"));

        inet_aton(router_ipaddr, &serv.sin_addr);
        memset(bcast_ipaddr, 0xff, 4);
        //memcpy(bcast_ipaddr, &serv.sin_addr, 3);

	memset(&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_port = htons(INFO_SERVER_PORT);
	//serv.sin_addr.s_addr = inet_addr("255.255.255.255");
	memcpy(&serv.sin_addr.s_addr, bcast_ipaddr, 4);
    
	char pdubuf[INFO_PDU_LENGTH] = {0};
	pdubuf[0] = 0x0C; //12
	pdubuf[1] = 0x15; //21
	pdubuf[2] = 0x1f; //31
	pdubuf[3] = 0x00;
	pdubuf[4] = 0x00;
	pdubuf[5] = 0x00;
	pdubuf[6] = 0x00;
	pdubuf[7] = 0x00;
    
	// POLLIN      any readable data available
	// POLLRDNORM  non-OOB/URG data available
	struct pollfd pollfd[1];    
	pollfd->fd = a_socket;
	pollfd->events = POLLIN; 
	pollfd->revents = 0;
    
	int result;
	
	int send_count = 3;
	int retry = 3;
	while (retry > 0)
	{
		while(send_count > 0)
		{
			ssize_t iRet2 = sendto(a_socket, pdubuf, INFO_PDU_LENGTH, 0, (struct sockaddr *) &serv, sizeof(serv));
			if (iRet2 < 0)
			{
            			printf("sendto failed\n");
            			close(a_socket);
				a_socket = 0;
				return 0;
			}
		send_count--;
		}
        
        //receive
        while (1)
        {
            result = poll(pollfd, 1, 500); // Wait for 1 seconds
            //printf("result = %d, retry = %d\n",result, retry);
            // Error during poll()
            if (result < 0) 
            {
                printf("Error during poll()\n");
                break;
            }
            // Timeout...
            else if (result == 0) 
            {
                //printf("Timeout during poll()\n");
                break;
            }
            // Success
            else
            {
                if (!(pollfd->revents & pollfd->events))
                    continue;
                
                if (!ParseASUSDiscoveryPackage(a_socket))
                {
                    printf("Failed to ParseASUSDiscoveryPackage\n");
                }
                else
                {
                    iRet = 1;
                }
            }
        }

		retry --;
	}
    
	close(a_socket);
	a_socket = 0;

	unsigned int lan_netmask, lan_gateway, ip_addr_t;
	lan_netmask = inet_addr(nvram_safe_get("lan_netmask"));
	lan_gateway = inet_addr(nvram_safe_get("lan_gateway"));

	char asus_device_buf[128];
	char asus_device_list_buf[1024] = {0};
	int getRouterIndex;
	//printf("a_GetRouterCount = %d\n",a_GetRouterCount);
	for (getRouterIndex = 0; getRouterIndex < a_GetRouterCount; getRouterIndex++)
	{
		ip_addr_t = inet_addr(searchRouterInfo[getRouterIndex].IPAddress);
		sprintf(asus_device_buf, "<%d>%s>%s>%s>%d>>>%s>%s",
		3,
		searchRouterInfo[getRouterIndex].ProductID,
		searchRouterInfo[getRouterIndex].IPAddress,
		searchRouterInfo[getRouterIndex].RealMacAddress,
		((ip_addr_t&lan_netmask)==(lan_gateway&lan_netmask))?1:0,
		searchRouterInfo[getRouterIndex].SSID,
		searchRouterInfo[getRouterIndex].SubMask
		);

		strcat(asus_device_list_buf, asus_device_buf); 
	}
	//printf("asus_device_list_buf = %s\n",asus_device_list_buf);
	nvram_set("asus_device_list", asus_device_list_buf);

	return iRet;
}

int ParseASUSDiscoveryPackage(int socket)
{
	//printf("----------ParseASUSDiscoveryPackage Start----------\n");
	
	if (a_bEndApp)
	{
		printf("a_bEndApp = true\n");
		return 0;
	}
	
	struct sockaddr_in from_addr;
	socklen_t ifromlen = sizeof(from_addr);
	char buf[INFO_PDU_LENGTH] = {0};
	ssize_t iRet = recvfrom(socket , buf, INFO_PDU_LENGTH, 0, (struct sockaddr *)&from_addr, &ifromlen);
	if (iRet <= 0)
	{
		printf("recvfrom function failed\n");
		return 0;
	}
	
	//PROCESS_UNPACK_GET_INFO_NEW(buf, from_addr);
	PROCESS_UNPACK_GET_INFO(buf, from_addr);
	
	return 1;
}

int UnpackasusGetInfo(char *pdubuf, PKT_GET_INFO *Info)
{
	unsigned int realOPCode;
        IBOX_COMM_PKT_RES_EX *hdr;

        hdr = (IBOX_COMM_PKT_RES_EX *)pdubuf;

	realOPCode = __cpu_to_le16(hdr->OpCode);

        if (hdr->ServiceID!=NET_SERVICE_ID_IBOX_INFO ||
            hdr->PacketType!=NET_PACKET_TYPE_RES ||
            realOPCode!=NET_CMD_ID_GETINFO){
            return 0;
	}

        memcpy(Info, pdubuf+sizeof(IBOX_COMM_PKT_RES), sizeof(PKT_GET_INFO));
        return 1;
}

void PROCESS_UNPACK_GET_INFO(char *pbuf, struct sockaddr_in from_addr)
{
	//printf("start PROCESS_UNPACK_GET_INFO\n");
	int inc = 1;
	PKT_GET_INFO get_discovery_info;
	SearchRouterInfoStruct *p;

	if (UnpackasusGetInfo(pbuf, &get_discovery_info) == 0) {
		//printf("error data\n");
		return;		// error data
	}

	if (a_GetRouterCount >= ARRAY_SIZE(searchRouterInfo)) {
		//printf("a_GetRouterCount >= 128\n");
		return;
	}
	//memcpy(txMac, get_discovery_info.MacAddress, 6);

	//check MAC address for duplicate response
	int getRouterIndex = 0;
	for (p = &searchRouterInfo[0], getRouterIndex = 0; getRouterIndex < a_GetRouterCount; getRouterIndex++, p++) {
		if (!memcmp(p->MacAddress, get_discovery_info.MacAddress, 6)) {
			//printf("check MAC address for duplicate response\n");
			inc = 0;
			break;
		}
	}

	// check for correct operation mode
	switch (get_discovery_info.OperationMode) {
	case OPERATION_MODE_WB:
	case OPERATION_MODE_AP:
		break;
	default:		// incorrect operation mode
		return;
	}

	char cTemp1[128] = { 0 };
	if ((get_discovery_info.PrinterInfo[0] == ' ' && get_discovery_info.PrinterInfo[1] == ' ' && get_discovery_info.PrinterInfo[2] == '\0') ||
	    (get_discovery_info.PrinterInfo[0] == '\0' && get_discovery_info.PrinterInfo[1] == '\0' && get_discovery_info.PrinterInfo[2] == '\0'))
	{
		//[sPrinterInfo stringWithFormat: @"%s", ""];
	} else {
		int i = 0;
		for (i = 0; i < sizeof(get_discovery_info.PrinterInfo); i++) {
			cTemp1[i] = get_discovery_info.PrinterInfo[i];
		}
	}
	memcpy(p->PrinterInfo, cTemp1, 128);

	char *pTemp = inet_ntoa(from_addr.sin_addr);
	memcpy(p->IPAddress, pTemp, 32);

	char cTemp2[32] = { 0 };
	sprintf(cTemp2, "%02X:%02X:%02X:%02X:%02X:%02X",
		(unsigned char)get_discovery_info.MacAddress[0],
		(unsigned char)get_discovery_info.MacAddress[1],
		(unsigned char)get_discovery_info.MacAddress[2],
		(unsigned char)get_discovery_info.MacAddress[3],
		(unsigned char)get_discovery_info.MacAddress[4],
		(unsigned char)get_discovery_info.MacAddress[5]);
	memcpy(p->RealMacAddress, cTemp2, 17);
	memset(p->SSID, '\0', 64);
	memcpy(p->SSID, get_discovery_info.SSID, 32);
	memcpy(p->SubMask, get_discovery_info.NetMask, 32);
	memcpy(p->ProductID, get_discovery_info.ProductID, 32);
	memcpy(p->FirmwareVersion, get_discovery_info.FirmwareVersion, 16);
	memcpy(p->MacAddress, get_discovery_info.MacAddress, 6);
	p->OperationMode = get_discovery_info.OperationMode;
	p->Regulation = get_discovery_info.Regulation;
	p->RouterOperateMode = pbuf[16];

#if 0
	char cTemp[512];
	printf("********************* Search a Router ********************\n");
	sprintf(cTemp, "ProductID : %s\n", p->ProductID);
	printf(cTemp);
	sprintf(cTemp, "SSID : %s\n", p->SSID);
	printf(cTemp);
	sprintf(cTemp, "IPAddress : %s\n", p->IPAddress);
	printf(cTemp);
	sprintf(cTemp, "MacAddress : %s\n", p->RealMacAddress);
	printf(cTemp);
#endif

	if (inc)
		a_GetRouterCount++;
}

