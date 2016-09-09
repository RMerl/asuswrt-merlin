//
//  ASUS_Discovery.c
//  ASUS
//
//  Created by Junda Txia on 11/22/10.
//  Copyright ASUSTek COMPUTER INC. 2011. All rights reserved.
//

#include <string.h>          //memset function
#include <stdio.h>           //sprintf function
#include <sys/socket.h>      //socket function
//#include <sys/_endian.h>     //htons function
#include <sys/errno.h>       //errorno function
#include <sys/poll.h>        //struct pollfd
#include <netinet/in.h>      //const IPPROTO_UDP
#include <unistd.h>          //close function
#include "../shared/rtstate.h"
#include <bcmnvram.h>

#include "ASUS_Discovery_Debug.h"    //myAsusDiscoveryDebugPrint function
#include "iboxcom.h"         //const INFO_PDU_LENGTH
#include "packet.h"          //const RESPONSE_HDR_OK
#include "ASUS_Discovery.h"

//char txMac[6] = {0};
int a_bEndApp = 0;
int a_socket = 0;
int a_GetRouterCount = 0;

//SearchRouterInfoStruct searchRouterInfo[MAX_SEARCH_ROUTER] = {0};
SearchRouterInfoStruct searchRouterInfo[MAX_SEARCH_ROUTER];

int ASUS_Discovery()
{	
    myAsusDiscoveryDebugPrint("----------ASUS_Discovery Start----------");
    
    int iRet = 0;
    
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
    if (a_socket < 0)
    {
        myAsusDiscoveryDebugPrint("Create socket failed");
        return 0;
    }

    // set reuseaddr option
    int reuseaddr = 1;
    if (setsockopt(a_socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) < 0)
    {
        myAsusDiscoveryDebugPrint("setsockopt: SO_REUSEADDR failed\n");
	close(a_socket);
	a_socket = 0;
        return 0;
    }

    char *lan_ifname;
    lan_ifname = nvram_safe_get("lan_ifname");
    setsockopt(a_socket, SOL_SOCKET, SO_BINDTODEVICE, lan_ifname, strlen(lan_ifname));

    // set broadcast flag
    int broadcast = 1;
    int iRes = setsockopt(a_socket, SOL_SOCKET, SO_BROADCAST, (char *)&broadcast, sizeof(broadcast));
    if (iRes != 0)
    {
        myAsusDiscoveryDebugPrint("setsockopt: SO_BROADCAST failed");
        close(a_socket);
        a_socket = 0;
        return 0;
    }

    struct sockaddr_in clit;
    memset(&clit, 0, sizeof(clit));
    clit.sin_family = AF_INET;
    clit.sin_port = htons(INFO_SERVER_PORT);
    clit.sin_addr.s_addr = htonl(INADDR_ANY);
    
    int bind_result = bind(a_socket, (struct sockaddr *)&clit, sizeof(clit));
    if (bind_result < 0)
    {
        myAsusDiscoveryDebugPrint("could not bind to address");
        close(a_socket);
        a_socket = 0;
        return 0;
    }

	struct sockaddr_in serv;
	memset(&serv, 0, sizeof(serv));
    	serv.sin_family = AF_INET;
	serv.sin_port = htons(INFO_SERVER_PORT);
	inet_aton("255.255.255.255", &serv.sin_addr);
    
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
	
	int retry = 3;
	while (retry > 0)
	{
		ssize_t iRet2 = sendto(a_socket, pdubuf, INFO_PDU_LENGTH, 0, (struct sockaddr *) &serv, sizeof(serv));
		if (iRet2 < 0)
		{
            		char error[128] = {0};
            		sprintf(error, "sendto failed : %s", strerror(errno));
			myAsusDiscoveryDebugPrint(error);
            
            		close(a_socket);
            		a_socket = 0;
			return 0;
		}
        
        	//receive
	        while (1)
	        {
	            result = poll(pollfd, 1, 500); // Wait for 1 seconds
            
        	    // Error during poll()
	            if (result < 0) 
        	    {
	                myAsusDiscoveryDebugPrint("Error during poll()");
        	        break;
	            }
	            // Timeout...
	            else if (result == 0) 
        	    {
	                myAsusDiscoveryDebugPrint("Timeout during poll()");
        	        break;
	            }
        	    // Success
	            else
        	    {
	                if (!(pollfd->revents & pollfd->events))
        	            continue;
                
	                if (!ParseASUSDiscoveryPackage(a_socket))
        	        {
	                    myAsusDiscoveryDebugPrint("Failed to ParseASUSDiscoveryPackage");
        	        }
                	else
	                {
        	            iRet = 1;
                	    //break;
	                }
        	    }
	        }
        
        	sleep(1);
        
	        retry --;
	}
    
    close(a_socket);
    a_socket = 0;

        unsigned int lan_netmask, lan_gateway, ip_addr_t;
        lan_netmask = inet_addr(nvram_safe_get("lan_netmask"));
        lan_gateway = inet_addr(nvram_safe_get("lan_gateway"));

        char asus_device_buf[128] = {0};
        char asus_device_list_buf[2048] = {0};
        int getRouterIndex;

        for (getRouterIndex = 0; getRouterIndex < a_GetRouterCount; getRouterIndex++)
        {
                ip_addr_t = inet_addr(searchRouterInfo[getRouterIndex].routerIPAddress);
                sprintf(asus_device_buf, "<%d>%s>%s>%s>%d>%s>%s>%d",
                3,
                searchRouterInfo[getRouterIndex].routerProductID,
                searchRouterInfo[getRouterIndex].routerIPAddress,
                searchRouterInfo[getRouterIndex].routerRealMacAddress,
                ((ip_addr_t&lan_netmask)==(lan_gateway&lan_netmask))?1:0,
                searchRouterInfo[getRouterIndex].routerSSID,
                searchRouterInfo[getRouterIndex].routerSubMask,
                searchRouterInfo[getRouterIndex].routerOperationMode
                );
                strcat(asus_device_list_buf, asus_device_buf);
        }
   	nvram_set("asus_device_list", asus_device_list_buf);
 
    return iRet;
}

int ParseASUSDiscoveryPackage(int socket)
{
	myAsusDiscoveryDebugPrint("----------ParseASUSDiscoveryPackage Start----------");
	
	if (a_bEndApp)
	{
		myAsusDiscoveryDebugPrint("a_bEndApp = true");
		return 0;
	}
	
	struct sockaddr_in from_addr;
	socklen_t ifromlen = sizeof(from_addr);
	char buf[INFO_PDU_LENGTH] = {0};
	ssize_t iRet = recvfrom(socket , buf, INFO_PDU_LENGTH, 0, (struct sockaddr *)&from_addr, &ifromlen);
	if (iRet <= 0)
	{
		myAsusDiscoveryDebugPrint("recvfrom function failed");
		return 0;
	}
	
	PROCESS_UNPACK_GET_INFO(buf, from_addr);
	
	return 1;
}

void PROCESS_UNPACK_GET_INFO(char *pbuf, struct sockaddr_in from_addr)
{
    
    PKT_GET_INFO get_discovery_info = {0};
    STORAGE_INFO_T get_storage_info = {0};
	
    int responseUnpackGetInfo = UnpackGetInfo_NEW(pbuf, &get_discovery_info, &get_storage_info);
    if (responseUnpackGetInfo == RESPONSE_HDR_IGNORE ||
        responseUnpackGetInfo == RESPONSE_HDR_ERR_UNSUPPORT)
    {
        return; // error data
    }
	
	// copy info to local buffer
    // check whether buffer overflow!
    if (a_GetRouterCount >= MAX_NO_OF_IBOX)
        return;
	
    //memcpy(txMac, get_discovery_info.MacAddress, 6);
    
	//check MAC address for duplicate response
    int getRouterIndex;
    for (getRouterIndex = 0; getRouterIndex < a_GetRouterCount; getRouterIndex++)
    {
        if (memcmp(searchRouterInfo[getRouterIndex].routerMacAddress, get_discovery_info.MacAddress, 6) == 0)
            return;
    }
	
	char cTemp1[128] = {0}; 
    if ((get_discovery_info.PrinterInfo[0] == ' ' && get_discovery_info.PrinterInfo[1] == ' ' && get_discovery_info.PrinterInfo[2] == '\0') ||
        (get_discovery_info.PrinterInfo[0] == '\0' && get_discovery_info.PrinterInfo[1] == '\0' && get_discovery_info.PrinterInfo[2] == '\0'))
	{
        //[sPrinterInfo stringWithFormat: @"%s", ""];
	}
    else
    {
	int i;
        for (i=0; i<sizeof(get_discovery_info.PrinterInfo); i++)
        {
            cTemp1[i] = get_discovery_info.PrinterInfo[i];
        }
    }
	memcpy(searchRouterInfo[a_GetRouterCount].routerPrinterInfo, cTemp1, 128);
    
	char *pTemp = inet_ntoa(from_addr.sin_addr);
	memcpy(searchRouterInfo[a_GetRouterCount].routerIPAddress, pTemp, 32);
	
	char cTemp2[32] = {0};
    sprintf(cTemp2, "%02X:%02X:%02X:%02X:%02X:%02X",(unsigned char)get_discovery_info.MacAddress[0], (unsigned char)get_discovery_info.MacAddress[1], (unsigned char)get_discovery_info.MacAddress[2], (unsigned char)get_discovery_info.MacAddress[3], (unsigned char)get_discovery_info.MacAddress[4], (unsigned char)get_discovery_info.MacAddress[5]);
    memcpy(searchRouterInfo[a_GetRouterCount].routerRealMacAddress, cTemp2, 17);
    
    memcpy(searchRouterInfo[a_GetRouterCount].routerSSID,            get_discovery_info.SSID,              32);
    searchRouterInfo[a_GetRouterCount].routerSSID[32] = '\0';	/* get_discovery_info.SSID is not ASCIIZ string. */
    memcpy(searchRouterInfo[a_GetRouterCount].routerSubMask,         get_discovery_info.NetMask,           32);
    memcpy(searchRouterInfo[a_GetRouterCount].routerProductID,       get_discovery_info.ProductID,         32);
    memcpy(searchRouterInfo[a_GetRouterCount].routerFirmwareVersion, get_discovery_info.FirmwareVersion,   16);
	memcpy(searchRouterInfo[a_GetRouterCount].routerMacAddress,      get_discovery_info.MacAddress,        6);
    searchRouterInfo[a_GetRouterCount].routerOperationMode =  get_discovery_info.sw_mode;
#ifdef WCLIENT
    searchRouterInfo[a_GetRouterCount].routerRegulation =     get_discovery_info.Regulation;
#endif
    
    char cTemp[512] = {0};
    myAsusDiscoveryDebugPrint("********************* Search a Router ********************");
    sprintf(cTemp, "Router ProductID : %s", searchRouterInfo[a_GetRouterCount].routerProductID);
    myAsusDiscoveryDebugPrint(cTemp);
    sprintf(cTemp, "Router SSID : %s", searchRouterInfo[a_GetRouterCount].routerSSID);
    myAsusDiscoveryDebugPrint(cTemp);
    sprintf(cTemp, "Router IPAddress : %s", searchRouterInfo[a_GetRouterCount].routerIPAddress);
    myAsusDiscoveryDebugPrint(cTemp);
    sprintf(cTemp, "Router MacAddress : %s", searchRouterInfo[a_GetRouterCount].routerRealMacAddress);
    myAsusDiscoveryDebugPrint(cTemp);
    sprintf(cTemp, "Router Firmware version : %s", searchRouterInfo[a_GetRouterCount].routerFirmwareVersion);
    myAsusDiscoveryDebugPrint(cTemp);
    
    if (searchRouterInfo[a_GetRouterCount].routerOperationMode == SW_MODE_ROUTER)
    {
        sprintf(cTemp, "Router Operation Mode : %d, ROUTER mode", searchRouterInfo[a_GetRouterCount].routerOperationMode);
        myAsusDiscoveryDebugPrint(cTemp);
    }
    else if (searchRouterInfo[a_GetRouterCount].routerOperationMode == SW_MODE_REPEATER)	
    {
        sprintf(cTemp, "Router Operation Mode : %d, REPEATER mode", searchRouterInfo[a_GetRouterCount].routerOperationMode);
        myAsusDiscoveryDebugPrint(cTemp);
    }
    else if (searchRouterInfo[a_GetRouterCount].routerOperationMode == SW_MODE_AP)
    {
        sprintf(cTemp, "Router Operation Mode : %d, AP mode", searchRouterInfo[a_GetRouterCount].routerOperationMode);
        myAsusDiscoveryDebugPrint(cTemp);
    }
    else if (searchRouterInfo[a_GetRouterCount].routerOperationMode == SW_MODE_HOTSPOT)
    {
        sprintf(cTemp, "Router Operation Mode : %d, HOTSPOT mode", searchRouterInfo[a_GetRouterCount].routerOperationMode);
        myAsusDiscoveryDebugPrint(cTemp);
    }
    else
    {
        sprintf(cTemp, "Router Operation Mode : %d, Not support this flag!", searchRouterInfo[a_GetRouterCount].routerOperationMode);
        myAsusDiscoveryDebugPrint(cTemp);
	searchRouterInfo[a_GetRouterCount].routerOperationMode = 0;
    }

    if (responseUnpackGetInfo == RESPONSE_HDR_OK)
    {
        searchRouterInfo[a_GetRouterCount].webdavSupport = 0;
        
        a_GetRouterCount++;
        
        return;
    }

    // ap mode WAN IP address will be zero
    unsigned int getWANIPAddr = get_storage_info.u.wt.WANIPAddr;
    if (getWANIPAddr == 0)
    {
        searchRouterInfo[a_GetRouterCount].webdavSupport = 0;
        
        a_GetRouterCount++;
        
        return;
    }
    
    // save webdav info
    searchRouterInfo[a_GetRouterCount].webdavSupport = 1;
    searchRouterInfo[a_GetRouterCount].webdavEnableWebDav = get_storage_info.u.wt.EnableWebDav;
    searchRouterInfo[a_GetRouterCount].webdavHttpType = get_storage_info.u.wt.HttpType;
    searchRouterInfo[a_GetRouterCount].webdavHttpPort = htons(get_storage_info.u.wt.HttpPort);
    searchRouterInfo[a_GetRouterCount].webdavHttpsPort = htons(get_storage_info.u.wt.HttpsPort);
    searchRouterInfo[a_GetRouterCount].webdavEnableDDNS = get_storage_info.u.wt.EnableDDNS;
    memcpy(searchRouterInfo[a_GetRouterCount].webdavHostName, get_storage_info.u.wt.HostName, 64);
    searchRouterInfo[a_GetRouterCount].webdavWANIPAddr = getWANIPAddr;
    searchRouterInfo[a_GetRouterCount].webdavWANState = htons(get_storage_info.u.wt.WANState);
    searchRouterInfo[a_GetRouterCount].webdavIsNotDefault = htons(get_storage_info.u.wt.isNotDefault);
    
    struct in_addr tempAddrWANIP = {0};
    tempAddrWANIP.s_addr = searchRouterInfo[a_GetRouterCount].webdavWANIPAddr;
    char *pTempWANIP = inet_ntoa(tempAddrWANIP);
    
    // covert 1.1.168.192 to 192.168.1.1
    char cTempWANIP[4][32];// = {0};
    int indexCol = 0;
    int indexRow = 0;
    memset(cTempWANIP, 0, sizeof(char)*4*32);
    while (*pTempWANIP != '\0')
    {
        if (indexCol == 4)
            break;
        
        if (*pTempWANIP != '.')
        {
            cTempWANIP[indexCol][indexRow] = *pTempWANIP; 
            
            pTempWANIP += 1;
            indexRow += 1;
        }
        else 
        {
            indexRow = 0;
            indexCol += 1; 
            pTempWANIP += 1;
        }
    }
    
    sprintf(searchRouterInfo[a_GetRouterCount].webdavWANIPAddress, "%s.%s.%s.%s", cTempWANIP[3], cTempWANIP[2], cTempWANIP[1], cTempWANIP[0]);
    
    sprintf(cTemp, "Webdav Enable WebDav : %d", searchRouterInfo[a_GetRouterCount].webdavEnableWebDav);
    myAsusDiscoveryDebugPrint(cTemp);
    sprintf(cTemp, "Webdav Http Type : %d", searchRouterInfo[a_GetRouterCount].webdavHttpType);
    myAsusDiscoveryDebugPrint(cTemp);
    sprintf(cTemp, "Webdav Http Port : %d", searchRouterInfo[a_GetRouterCount].webdavHttpPort);
    myAsusDiscoveryDebugPrint(cTemp);
    sprintf(cTemp, "Webdav Https Port : %d", searchRouterInfo[a_GetRouterCount].webdavHttpsPort);
    myAsusDiscoveryDebugPrint(cTemp);
    sprintf(cTemp, "Webdav Enable DDNS : %d", searchRouterInfo[a_GetRouterCount].webdavEnableDDNS);
    myAsusDiscoveryDebugPrint(cTemp);
    sprintf(cTemp, "Webdav Host Name (DDNS Name) : %s", searchRouterInfo[a_GetRouterCount].webdavHostName);
    myAsusDiscoveryDebugPrint(cTemp);
    sprintf(cTemp, "Webdav WANIPAddr : %u", searchRouterInfo[a_GetRouterCount].webdavWANIPAddr);
    myAsusDiscoveryDebugPrint(cTemp);
    sprintf(cTemp, "Webdav WAN IP Address : %s", searchRouterInfo[a_GetRouterCount].webdavWANIPAddress);
    myAsusDiscoveryDebugPrint(cTemp);
    
    if (responseUnpackGetInfo == RESPONSE_HDR_OK_SUPPORT_WEBDAV)
    {
        searchRouterInfo[a_GetRouterCount].tunnelSupport = 0;
        
        a_GetRouterCount++;
        
        return;
    }
    
    // save tunnel info
    searchRouterInfo[a_GetRouterCount].tunnelAppHttpPort =  get_storage_info.AppHttpPort;
    searchRouterInfo[a_GetRouterCount].tunnelAppAPILevel = get_storage_info.AppAPILevel;
    searchRouterInfo[a_GetRouterCount].tunnelEnableAAE = get_storage_info.EnableAAE;
    memcpy(searchRouterInfo[a_GetRouterCount].tunnelAAEDeviceID, get_storage_info.AAEDeviceID, 64);
    
    sprintf(cTemp, "Tunnel App Http Port : %d", searchRouterInfo[a_GetRouterCount].tunnelAppHttpPort);
    myAsusDiscoveryDebugPrint(cTemp);
    sprintf(cTemp, "Tunnel App API Level : %d", searchRouterInfo[a_GetRouterCount].tunnelAppAPILevel);
    myAsusDiscoveryDebugPrint(cTemp);
    sprintf(cTemp, "Tunnel Enable AAE : %d", searchRouterInfo[a_GetRouterCount].tunnelEnableAAE);
    myAsusDiscoveryDebugPrint(cTemp);
    sprintf(cTemp, "Tunnel AAE Device ID : %s", searchRouterInfo[a_GetRouterCount].tunnelAAEDeviceID);
    myAsusDiscoveryDebugPrint(cTemp);

    a_GetRouterCount++;
}

int main(void)
{
	return ASUS_Discovery();
}
