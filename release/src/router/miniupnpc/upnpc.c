/* $Id: upnpc.c,v 1.105 2014/11/01 10:37:32 nanard Exp $ */
/* Project : miniupnp
 * Author : Thomas Bernard
 * Copyright (c) 2005-2014 Thomas Bernard
 * This software is subject to the conditions detailed in the
 * LICENCE file provided in this distribution. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#define snprintf _snprintf
#else
/* for IPPROTO_TCP / IPPROTO_UDP */
#include <netinet/in.h>
#endif
#include "miniwget.h"
#include "miniupnpc.h"
#include "upnpcommands.h"
#include "upnperrors.h"

#include <shared.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sysinfo.h>
#include <errno.h>

#ifdef RTCONFIG_JFFS2USERICON
#define JFFS_ICON_PATH	"/jffs/usericon"
#define UPNP_ICON_PATH	"/tmp/upnpicon"

typedef struct {
        unsigned char   ip_addr[255][4];
        unsigned char   mac_addr[255][6];
        unsigned char   user_define[255][16];
        unsigned char   device_name[255][32];
        unsigned char   apl_dev[255][16];
        int             type[255];
        int             http[255];
        int             printer[255];
        int             itune[255];
        int             exist[255];
#ifdef RTCONFIG_BWDPI
        char            bwdpi_hostname[255][32];
        char            bwdpi_devicetype[255][100];
#endif
        int             ip_mac_num;
        int             detail_info_num;
} CLIENT_DETAIL_INFO_TABLE, *P_CLIENT_DETAIL_INFO_TABLE;

//the crc32 of Windows related icon
unsigned long default_icon_crc[] = {
        0xeefa6488,
        0x0798893a,
        0xe06e1235,
        0xb54fd1af,
        0xc9c2b6c4,
        0xbaa4c50b,
	0xcec591e3,
        0
};

typedef struct IconFile {
    char                name[12];
    struct IconFile     *next;
}IconFile;

struct IconFile *IconList = NULL;

struct UPNPDevInfo IGDInfo;

static char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int
openvpn_base64_encode(const void *data, int size, char **str) {
    char *s, *p;
    int i;
    int c;
    const unsigned char *q;

    if (size < 0)
                return -1;
    p = s = (char *) malloc(size * 4 / 3 + 4);
    if (p == NULL)
                return -1;
    q = (const unsigned char *) data;
    i = 0;
    for (i = 0; i < size;) {
                c = q[i++];
                c *= 256;
                if (i < size)
                        c += q[i];
                i++;
                c *= 256;
                if (i < size)
                        c += q[i];
                i++;
                p[0] = base64_chars[(c & 0x00fc0000) >> 18];
                p[1] = base64_chars[(c & 0x0003f000) >> 12];
                p[2] = base64_chars[(c & 0x00000fc0) >> 6];
                p[3] = base64_chars[(c & 0x0000003f) >> 0];
                if (i > size)
                        p[3] = '=';
                if (i > size + 1)
                        p[2] = '=';
                p += 4;
    }
    *p = 0;
    *str = s;

    return strlen(s);
}

int
image_to_base64(char *file_path,  char *client_mac) {
        FILE * pFile;
        long lSize;
        char *buffer;
        char *base64;
        size_t result;
        int i;
        char client_mac_temp[15];
        memset(client_mac_temp, 0, sizeof(client_mac_temp));
        sprintf(client_mac_temp, client_mac);

printf("Store %s to jffs -> %s!!!\n", file_path, client_mac);
        int idex = 0;
        while ( client_mac_temp[idex] != 0 ) {
                if ( ( client_mac_temp[idex] >= 'a' ) && ( client_mac_temp[idex] <= 'z' ) ) {
                        client_mac_temp[idex] = client_mac_temp[idex] - 'a' + 'A';
                }
                idex++;
        }

        pFile = fopen (file_path , "rb");
        if (pFile == NULL) {
                printf("File error\n");
                i = 0;
                return i;
        }

        fseek (pFile , 0 , SEEK_END);
        lSize = ftell (pFile);
        rewind (pFile);

        buffer = (char*) malloc (sizeof(char)*lSize);
        base64 = (char*) malloc (sizeof(char)*lSize);

        if (buffer == NULL) {
                printf("Memory error\n");
                i = 0;
                return i;
        }

        result = fread (buffer,1,lSize,pFile);
        if (result != lSize) {
                printf("Reading error\n");
                i = 0;
                return i;
        }	

        if (openvpn_base64_encode (buffer, lSize, &base64) <= 0) { 
                printf("binary encode error \n");
                i = 0;
                return i;
        }

        char image_base64[(strlen(base64) + 25)];
        memset(image_base64, 0, sizeof(image_base64));

        sprintf(image_base64, "data:image/jpeg;base64,");
        strcat(image_base64, base64);

        fclose (pFile);
        free (buffer);
        free (base64);

        char write_file_path[35];
        memset(write_file_path, 0, sizeof(write_file_path)); 

        if(!check_if_dir_exist("/jffs/usericon"))
                system("mkdir /jffs/usericon");

        sprintf(write_file_path, "/jffs/usericon/%s.log", client_mac_temp);

        if(!check_if_file_exist(write_file_path)) {
                int str_len = strlen(image_base64);
                int i;
                FILE * pFile;
                pFile = fopen (write_file_path , "w");

                if (pFile == NULL) {
                        printf("File error\n");
                        i = 0;
                        return i;
                }

                for(i = 0; i < str_len; i++) {
                        fputc(image_base64[i], pFile);
                }
                fclose (pFile);
        }

        i = 1;
        return i;
}

unsigned long get_icon_crc32(char *icon_path)
{
	FILE *icon_fd;
        unsigned long calc_crc = 0;
        long iconlen;
        char buf[4096] = {0};

	//printf("get icon crc32 open: %s\n", icon_path);

	icon_fd = fopen(icon_path, "rb");

	if(icon_fd == NULL) {
		printf("Get Icon Failed!!!\n");
		return -1;
	}	

        fseek (icon_fd , 0 , SEEK_END);
        iconlen = ftell (icon_fd);
        rewind (icon_fd);

        while(iconlen>0)
        {
                if (iconlen > sizeof(buf))
                {
                        fread(buf, 1, sizeof(buf), icon_fd);
                        calc_crc = crc_calc(calc_crc, (unsigned char*)buf, sizeof(buf));
                        iconlen-=sizeof(buf);
                }
                else
                {
                        fread(buf, 1, iconlen, icon_fd);
                        calc_crc = crc_calc(calc_crc, (unsigned char*)buf, iconlen);
                        iconlen=0;
                }
        }

	return calc_crc;
}

void get_icon_files(void)
{
        FILE *fp;
        char *ico_ptr;
        char buf[128];
        struct IconFile *NextIcon = NULL, *ShowIcon;

        sprintf(buf, "ls -1 %s/*.log", JFFS_ICON_PATH);

        fp = popen(buf, "r");
        if (fp == NULL) {
                perror("popen");
                return;
        }

#ifdef DEBUG
printf("======= Get Icon Files =========\n");
#endif
        while(fgets(buf, sizeof(buf), fp))
        {
                IconFile *CurIcon = (IconFile *)malloc(sizeof(IconFile));

                ico_ptr = buf;
                ico_ptr+= strlen(JFFS_ICON_PATH)+1;

                memcpy(CurIcon->name, ico_ptr, 12);
                CurIcon->next = NULL;

                if( IconList == NULL )
                        IconList = CurIcon;
                else
                        NextIcon->next = CurIcon;
                NextIcon = CurIcon;
        }

        ShowIcon = IconList;
        while(ShowIcon != NULL)
        {
#ifdef DEBUG
                printf("%s->", ShowIcon->name);
#endif
                ShowIcon = ShowIcon->next;
        }
#ifdef DEBUG
        printf("\n===================================\n");
#endif
}

void TransferUpnpIcon(P_CLIENT_DETAIL_INFO_TABLE nmp_client)
{
        FILE *fp;
        char *ico_ptr, *ico_end;
	char ico_ip[16], ico_mac[13], nmp_client_ip[16];
        char buf[128];
        struct IconFile *icon = NULL;
	unsigned long icon_crc32;
	int i, default_icon=0;

        sprintf(buf, "ls -1 %s/*.ico", UPNP_ICON_PATH);

        fp = popen(buf, "r");
        if (fp == NULL) {
                perror("popen");
                return;
        }

#ifdef DEBUG
printf("======= Transfer UPnP Icon Files =========\n");
#endif
        while(fgets(buf, sizeof(buf), fp))
        {

                ico_ptr = buf;
                ico_ptr += strlen(UPNP_ICON_PATH)+1;
		ico_end = strstr(buf, ".ico");

		i = 0;
		while(ico_ptr < ico_end) 
			ico_ip[i++] = *ico_ptr++;
		ico_ip[i] = '\0';
		ico_end = ico_end+4;
		*ico_end = '\0';
		icon_crc32 = get_icon_crc32(buf);

		//default icon crc32 check
		i = 0;
		default_icon = 0;
		while((default_icon_crc[i] !=0 )) {
			if(default_icon_crc[i] == icon_crc32) {
				#ifdef DEBUG
				printf("GET DEFAULT ICON CRC!!! %s\n", buf);
				#endif
				default_icon = 1;
				break;
			}
			i++;
		}

		if(!default_icon) {
		    i = 0;
		    while( (nmp_client->ip_addr[i][0] != 0) && (i < 255) ) {
			sprintf(nmp_client_ip, "%d.%d.%d.%d", nmp_client->ip_addr[i][0],
				nmp_client->ip_addr[i][1],nmp_client->ip_addr[i][2],
				nmp_client->ip_addr[i][3]);
			if(!strcmp(ico_ip, nmp_client_ip)) {
				sprintf(ico_mac, "%02X%02X%02X%02X%02X%02X",
					nmp_client->mac_addr[i][0],
					nmp_client->mac_addr[i][1],
                                        nmp_client->mac_addr[i][2],
                                        nmp_client->mac_addr[i][3],
                                        nmp_client->mac_addr[i][4],
                                        nmp_client->mac_addr[i][5]);
				/* Compare icon in /jffs/usericon */
                		icon = IconList;
                		while( icon != NULL ) {
                        		if( memcmp(icon->name, ico_mac, 12) == 0) {
						break;
                		        }
                        		icon = icon->next;
                		}

            			if( icon == NULL ) { /* new icon store to /jffs/usericon */
					image_to_base64(buf, ico_mac);

            			}
				break;
			}
			i++;
		    }
		}
	}
}
#endif

/* protofix() checks if protocol is "UDP" or "TCP"
 * returns NULL if not */
const char * protofix(const char * proto)
{
	static const char proto_tcp[4] = { 'T', 'C', 'P', 0};
	static const char proto_udp[4] = { 'U', 'D', 'P', 0};
	int i, b;
	for(i=0, b=1; i<4; i++)
		b = b && (   (proto[i] == proto_tcp[i])
		          || (proto[i] == (proto_tcp[i] | 32)) );
	if(b)
		return proto_tcp;
	for(i=0, b=1; i<4; i++)
		b = b && (   (proto[i] == proto_udp[i])
		          || (proto[i] == (proto_udp[i] | 32)) );
	if(b)
		return proto_udp;
	return 0;
}

static void DisplayInfos(struct UPNPUrls * urls,
                         struct IGDdatas * data)
{
	char externalIPAddress[40];
	char connectionType[64];
	char status[64];
	char lastconnerr[64];
	unsigned int uptime;
	unsigned int brUp, brDown;
	time_t timenow, timestarted;
	int r;
/*
printf("===== DisplayInfos ====\n");
printf("controlURL:   %s\n", urls->controlURL);
printf("ipcondescURL: %s\n", urls->ipcondescURL);
printf("rootdescURL:  %s\n", urls->rootdescURL);
printf("controlURL_CIF: %s\n", urls->controlURL_CIF);
printf("controlURL_6FC: %s\n", urls->controlURL_6FC);
printf("=======================\n");
*/
	if(UPNP_GetConnectionTypeInfo(urls->controlURL,
	                              data->first.servicetype,
	                              connectionType) != UPNPCOMMAND_SUCCESS)
		printf("GetConnectionTypeInfo failed.\n");
	else
		printf("Connection Type : %s\n", connectionType);
	if(UPNP_GetStatusInfo(urls->controlURL, data->first.servicetype,
	                      status, &uptime, lastconnerr) != UPNPCOMMAND_SUCCESS)
		printf("GetStatusInfo failed.\n");
	else
		printf("Status : %s, uptime=%us, LastConnectionError : %s\n",
		       status, uptime, lastconnerr);
	timenow = time(NULL);
	timestarted = timenow - uptime;
	printf("  Time started : %s", ctime(&timestarted));
	if(UPNP_GetLinkLayerMaxBitRates(urls->controlURL_CIF, data->CIF.servicetype,
	                                &brDown, &brUp) != UPNPCOMMAND_SUCCESS) {
		printf("GetLinkLayerMaxBitRates failed.\n");
	} else {
		printf("MaxBitRateDown : %u bps", brDown);
		if(brDown >= 1000000) {
			printf(" (%u.%u Mbps)", brDown / 1000000, (brDown / 100000) % 10);
		} else if(brDown >= 1000) {
			printf(" (%u Kbps)", brDown / 1000);
		}
		printf("   MaxBitRateUp %u bps", brUp);
		if(brUp >= 1000000) {
			printf(" (%u.%u Mbps)", brUp / 1000000, (brUp / 100000) % 10);
		} else if(brUp >= 1000) {
			printf(" (%u Kbps)", brUp / 1000);
		}
		printf("\n");
	}
	r = UPNP_GetExternalIPAddress(urls->controlURL,
	                          data->first.servicetype,
							  externalIPAddress);
	if(r != UPNPCOMMAND_SUCCESS) {
		printf("GetExternalIPAddress failed. (errorcode=%d)\n", r);
	} else {
		printf("ExternalIPAddress = %s\n", externalIPAddress);
	}
}

static void GetConnectionStatus(struct UPNPUrls * urls,
                               struct IGDdatas * data)
{
	unsigned int bytessent, bytesreceived, packetsreceived, packetssent;
	DisplayInfos(urls, data);
	bytessent = UPNP_GetTotalBytesSent(urls->controlURL_CIF, data->CIF.servicetype);
	bytesreceived = UPNP_GetTotalBytesReceived(urls->controlURL_CIF, data->CIF.servicetype);
	packetssent = UPNP_GetTotalPacketsSent(urls->controlURL_CIF, data->CIF.servicetype);
	packetsreceived = UPNP_GetTotalPacketsReceived(urls->controlURL_CIF, data->CIF.servicetype);
	printf("Bytes:   Sent: %8u\tRecv: %8u\n", bytessent, bytesreceived);
	printf("Packets: Sent: %8u\tRecv: %8u\n", packetssent, packetsreceived);
}

static void ListRedirections(struct UPNPUrls * urls,
                             struct IGDdatas * data)
{
	int r;
	int i = 0;
	char index[6];
	char intClient[40];
	char intPort[6];
	char extPort[6];
	char protocol[4];
	char desc[80];
	char enabled[6];
	char rHost[64];
	char duration[16];
	/*unsigned int num=0;
	UPNP_GetPortMappingNumberOfEntries(urls->controlURL, data->servicetype, &num);
	printf("PortMappingNumberOfEntries : %u\n", num);*/
	printf(" i protocol exPort->inAddr:inPort description remoteHost leaseTime\n");
	do {
		snprintf(index, 6, "%d", i);
		rHost[0] = '\0'; enabled[0] = '\0';
		duration[0] = '\0'; desc[0] = '\0';
		extPort[0] = '\0'; intPort[0] = '\0'; intClient[0] = '\0';
		r = UPNP_GetGenericPortMappingEntry(urls->controlURL,
		                               data->first.servicetype,
		                               index,
		                               extPort, intClient, intPort,
									   protocol, desc, enabled,
									   rHost, duration);
		if(r==0)
		/*
			printf("%02d - %s %s->%s:%s\tenabled=%s leaseDuration=%s\n"
			       "     desc='%s' rHost='%s'\n",
			       i, protocol, extPort, intClient, intPort,
				   enabled, duration,
				   desc, rHost);
				   */
			printf("%2d %s %5s->%s:%-5s '%s' '%s' %s\n",
			       i, protocol, extPort, intClient, intPort,
			       desc, rHost, duration);
		else
			printf("GetGenericPortMappingEntry() returned %d (%s)\n",
			       r, strupnperror(r));
		i++;
	} while(r==0);
}

static void NewListRedirections(struct UPNPUrls * urls,
                                struct IGDdatas * data)
{
	int r;
	int i = 0;
	struct PortMappingParserData pdata;
	struct PortMapping * pm;

	memset(&pdata, 0, sizeof(struct PortMappingParserData));
	r = UPNP_GetListOfPortMappings(urls->controlURL,
                                   data->first.servicetype,
	                               "0",
	                               "65535",
	                               "TCP",
	                               "1000",
	                               &pdata);
	if(r == UPNPCOMMAND_SUCCESS)
	{
		printf(" i protocol exPort->inAddr:inPort description remoteHost leaseTime\n");
		for(pm = pdata.l_head; pm != NULL; pm = pm->l_next)
		{
			printf("%2d %s %5hu->%s:%-5hu '%s' '%s' %u\n",
			       i, pm->protocol, pm->externalPort, pm->internalClient,
			       pm->internalPort,
			       pm->description, pm->remoteHost,
			       (unsigned)pm->leaseTime);
			i++;
		}
		FreePortListing(&pdata);
	}
	else
	{
		printf("GetListOfPortMappings() returned %d (%s)\n",
		       r, strupnperror(r));
	}
	r = UPNP_GetListOfPortMappings(urls->controlURL,
                                   data->first.servicetype,
	                               "0",
	                               "65535",
	                               "UDP",
	                               "1000",
	                               &pdata);
	if(r == UPNPCOMMAND_SUCCESS)
	{
		for(pm = pdata.l_head; pm != NULL; pm = pm->l_next)
		{
			printf("%2d %s %5hu->%s:%-5hu '%s' '%s' %u\n",
			       i, pm->protocol, pm->externalPort, pm->internalClient,
			       pm->internalPort,
			       pm->description, pm->remoteHost,
			       (unsigned)pm->leaseTime);
			i++;
		}
		FreePortListing(&pdata);
	}
	else
	{
		printf("GetListOfPortMappings() returned %d (%s)\n",
		       r, strupnperror(r));
	}
}

/* Test function
 * 1 - get connection type
 * 2 - get extenal ip address
 * 3 - Add port mapping
 * 4 - get this port mapping from the IGD */
static void SetRedirectAndTest(struct UPNPUrls * urls,
			       struct IGDdatas * data,
			       const char * iaddr,
			       const char * iport,
			       const char * eport,
			       const char * proto,
			       const char * leaseDuration,
			       const char * description,
			       int addAny)
{
	char externalIPAddress[40];
	char intClient[40];
	char intPort[6];
	char reservedPort[6];
	char duration[16];
	int r;

	if(!iaddr || !iport || !eport || !proto)
	{
		fprintf(stderr, "Wrong arguments\n");
		return;
	}
	proto = protofix(proto);
	if(!proto)
	{
		fprintf(stderr, "invalid protocol\n");
		return;
	}

	r = UPNP_GetExternalIPAddress(urls->controlURL,
				      data->first.servicetype,
				      externalIPAddress);
	if(r!=UPNPCOMMAND_SUCCESS)
		printf("GetExternalIPAddress failed.\n");
	else
		printf("ExternalIPAddress = %s\n", externalIPAddress);

	if (addAny) {
		r = UPNP_AddAnyPortMapping(urls->controlURL, data->first.servicetype,
					   eport, iport, iaddr, description,
					   proto, 0, leaseDuration, reservedPort);
		if(r==UPNPCOMMAND_SUCCESS)
			eport = reservedPort;
		else
			printf("AddAnyPortMapping(%s, %s, %s) failed with code %d (%s)\n",
			       eport, iport, iaddr, r, strupnperror(r));
	} else {
		r = UPNP_AddPortMapping(urls->controlURL, data->first.servicetype,
					eport, iport, iaddr, description,
					proto, 0, leaseDuration);
		if(r!=UPNPCOMMAND_SUCCESS)
			printf("AddPortMapping(%s, %s, %s) failed with code %d (%s)\n",
			       eport, iport, iaddr, r, strupnperror(r));
	}

	r = UPNP_GetSpecificPortMappingEntry(urls->controlURL,
					     data->first.servicetype,
					     eport, proto, NULL/*remoteHost*/,
					     intClient, intPort, NULL/*desc*/,
					     NULL/*enabled*/, duration);
	if(r!=UPNPCOMMAND_SUCCESS)
		printf("GetSpecificPortMappingEntry() failed with code %d (%s)\n",
		       r, strupnperror(r));
	else {
		printf("InternalIP:Port = %s:%s\n", intClient, intPort);
		printf("external %s:%s %s is redirected to internal %s:%s (duration=%s)\n",
		       externalIPAddress, eport, proto, intClient, intPort, duration);
	}
}

static void
RemoveRedirect(struct UPNPUrls * urls,
               struct IGDdatas * data,
               const char * eport,
               const char * proto,
               const char * remoteHost)
{
	int r;
	if(!proto || !eport)
	{
		fprintf(stderr, "invalid arguments\n");
		return;
	}
	proto = protofix(proto);
	if(!proto)
	{
		fprintf(stderr, "protocol invalid\n");
		return;
	}
	r = UPNP_DeletePortMapping(urls->controlURL, data->first.servicetype, eport, proto, remoteHost);
	printf("UPNP_DeletePortMapping() returned : %d\n", r);
}

static void
RemoveRedirectRange(struct UPNPUrls * urls,
		    struct IGDdatas * data,
		    const char * ePortStart, char const * ePortEnd,
		    const char * proto, const char * manage)
{
	int r;

	if (!manage)
		manage = "0";

	if(!proto || !ePortStart || !ePortEnd)
	{
		fprintf(stderr, "invalid arguments\n");
		return;
	}
	proto = protofix(proto);
	if(!proto)
	{
		fprintf(stderr, "protocol invalid\n");
		return;
	}
	r = UPNP_DeletePortMappingRange(urls->controlURL, data->first.servicetype, ePortStart, ePortEnd, proto, manage);
	printf("UPNP_DeletePortMappingRange() returned : %d\n", r);
}

/* IGD:2, functions for service WANIPv6FirewallControl:1 */
static void GetFirewallStatus(struct UPNPUrls * urls, struct IGDdatas * data)
{
	unsigned int bytessent, bytesreceived, packetsreceived, packetssent;
	int firewallEnabled = 0, inboundPinholeAllowed = 0;

	UPNP_GetFirewallStatus(urls->controlURL_6FC, data->IPv6FC.servicetype, &firewallEnabled, &inboundPinholeAllowed);
	printf("FirewallEnabled: %d & Inbound Pinhole Allowed: %d\n", firewallEnabled, inboundPinholeAllowed);
	printf("GetFirewallStatus:\n   Firewall Enabled: %s\n   Inbound Pinhole Allowed: %s\n", (firewallEnabled)? "Yes":"No", (inboundPinholeAllowed)? "Yes":"No");

	bytessent = UPNP_GetTotalBytesSent(urls->controlURL_CIF, data->CIF.servicetype);
	bytesreceived = UPNP_GetTotalBytesReceived(urls->controlURL_CIF, data->CIF.servicetype);
	packetssent = UPNP_GetTotalPacketsSent(urls->controlURL_CIF, data->CIF.servicetype);
	packetsreceived = UPNP_GetTotalPacketsReceived(urls->controlURL_CIF, data->CIF.servicetype);
	printf("Bytes:   Sent: %8u\tRecv: %8u\n", bytessent, bytesreceived);
	printf("Packets: Sent: %8u\tRecv: %8u\n", packetssent, packetsreceived);
}

/* Test function
 * 1 - Add pinhole
 * 2 - Check if pinhole is working from the IGD side */
static void SetPinholeAndTest(struct UPNPUrls * urls, struct IGDdatas * data,
					const char * remoteaddr, const char * eport,
					const char * intaddr, const char * iport,
					const char * proto, const char * lease_time)
{
	char uniqueID[8];
	/*int isWorking = 0;*/
	int r;
	char proto_tmp[8];

	if(!intaddr || !remoteaddr || !iport || !eport || !proto || !lease_time)
	{
		fprintf(stderr, "Wrong arguments\n");
		return;
	}
	if(atoi(proto) == 0)
	{
		const char * protocol;
		protocol = protofix(proto);
		if(protocol && (strcmp("TCP", protocol) == 0))
		{
			snprintf(proto_tmp, sizeof(proto_tmp), "%d", IPPROTO_TCP);
			proto = proto_tmp;
		}
		else if(protocol && (strcmp("UDP", protocol) == 0))
		{
			snprintf(proto_tmp, sizeof(proto_tmp), "%d", IPPROTO_UDP);
			proto = proto_tmp;
		}
		else
		{
			fprintf(stderr, "invalid protocol\n");
			return;
		}
	}
	r = UPNP_AddPinhole(urls->controlURL_6FC, data->IPv6FC.servicetype, remoteaddr, eport, intaddr, iport, proto, lease_time, uniqueID);
	if(r!=UPNPCOMMAND_SUCCESS)
		printf("AddPinhole([%s]:%s -> [%s]:%s) failed with code %d (%s)\n",
		       remoteaddr, eport, intaddr, iport, r, strupnperror(r));
	else
	{
		printf("AddPinhole: ([%s]:%s -> [%s]:%s) / Pinhole ID = %s\n",
		       remoteaddr, eport, intaddr, iport, uniqueID);
		/*r = UPNP_CheckPinholeWorking(urls->controlURL_6FC, data->servicetype_6FC, uniqueID, &isWorking);
		if(r!=UPNPCOMMAND_SUCCESS)
			printf("CheckPinholeWorking() failed with code %d (%s)\n", r, strupnperror(r));
		printf("CheckPinholeWorking: Pinhole ID = %s / IsWorking = %s\n", uniqueID, (isWorking)? "Yes":"No");*/
	}
}

/* Test function
 * 1 - Check if pinhole is working from the IGD side
 * 2 - Update pinhole */
static void GetPinholeAndUpdate(struct UPNPUrls * urls, struct IGDdatas * data,
					const char * uniqueID, const char * lease_time)
{
	int isWorking = 0;
	int r;

	if(!uniqueID || !lease_time)
	{
		fprintf(stderr, "Wrong arguments\n");
		return;
	}
	r = UPNP_CheckPinholeWorking(urls->controlURL_6FC, data->IPv6FC.servicetype, uniqueID, &isWorking);
	printf("CheckPinholeWorking: Pinhole ID = %s / IsWorking = %s\n", uniqueID, (isWorking)? "Yes":"No");
	if(r!=UPNPCOMMAND_SUCCESS)
		printf("CheckPinholeWorking() failed with code %d (%s)\n", r, strupnperror(r));
	if(isWorking || r==709)
	{
		r = UPNP_UpdatePinhole(urls->controlURL_6FC, data->IPv6FC.servicetype, uniqueID, lease_time);
		printf("UpdatePinhole: Pinhole ID = %s with Lease Time: %s\n", uniqueID, lease_time);
		if(r!=UPNPCOMMAND_SUCCESS)
			printf("UpdatePinhole: ID (%s) failed with code %d (%s)\n", uniqueID, r, strupnperror(r));
	}
}

/* Test function
 * Get pinhole timeout
 */
static void GetPinholeOutboundTimeout(struct UPNPUrls * urls, struct IGDdatas * data,
					const char * remoteaddr, const char * eport,
					const char * intaddr, const char * iport,
					const char * proto)
{
	int timeout = 0;
	int r;

	if(!intaddr || !remoteaddr || !iport || !eport || !proto)
	{
		fprintf(stderr, "Wrong arguments\n");
		return;
	}

	r = UPNP_GetOutboundPinholeTimeout(urls->controlURL_6FC, data->IPv6FC.servicetype, remoteaddr, eport, intaddr, iport, proto, &timeout);
	if(r!=UPNPCOMMAND_SUCCESS)
		printf("GetOutboundPinholeTimeout([%s]:%s -> [%s]:%s) failed with code %d (%s)\n",
		       intaddr, iport, remoteaddr, eport, r, strupnperror(r));
	else
		printf("GetOutboundPinholeTimeout: ([%s]:%s -> [%s]:%s) / Timeout = %d\n", intaddr, iport, remoteaddr, eport, timeout);
}

static void
GetPinholePackets(struct UPNPUrls * urls,
               struct IGDdatas * data, const char * uniqueID)
{
	int r, pinholePackets = 0;
	if(!uniqueID)
	{
		fprintf(stderr, "invalid arguments\n");
		return;
	}
	r = UPNP_GetPinholePackets(urls->controlURL_6FC, data->IPv6FC.servicetype, uniqueID, &pinholePackets);
	if(r!=UPNPCOMMAND_SUCCESS)
		printf("GetPinholePackets() failed with code %d (%s)\n", r, strupnperror(r));
	else
		printf("GetPinholePackets: Pinhole ID = %s / PinholePackets = %d\n", uniqueID, pinholePackets);
}

static void
CheckPinhole(struct UPNPUrls * urls,
               struct IGDdatas * data, const char * uniqueID)
{
	int r, isWorking = 0;
	if(!uniqueID)
	{
		fprintf(stderr, "invalid arguments\n");
		return;
	}
	r = UPNP_CheckPinholeWorking(urls->controlURL_6FC, data->IPv6FC.servicetype, uniqueID, &isWorking);
	if(r!=UPNPCOMMAND_SUCCESS)
		printf("CheckPinholeWorking() failed with code %d (%s)\n", r, strupnperror(r));
	else
		printf("CheckPinholeWorking: Pinhole ID = %s / IsWorking = %s\n", uniqueID, (isWorking)? "Yes":"No");
}

static void
RemovePinhole(struct UPNPUrls * urls,
               struct IGDdatas * data, const char * uniqueID)
{
	int r;
	if(!uniqueID)
	{
		fprintf(stderr, "invalid arguments\n");
		return;
	}
	r = UPNP_DeletePinhole(urls->controlURL_6FC, data->IPv6FC.servicetype, uniqueID);
	printf("UPNP_DeletePinhole() returned : %d\n", r);
}


/* sample upnp client program */
int main(int argc, char ** argv)
{
	char command = 0;
	char ** commandargv = 0;
	int commandargc = 0;
	struct UPNPDev * devlist = 0;
	char lanaddr[64];	/* my ip address on the LAN */
	int i;
	const char * rootdescurl = 0;
	const char * multicastif = 0;
	const char * minissdpdpath = 0;
	int retcode = 0;
	int error = 0;
	int ipv6 = 0;
	const char * description = 0;

#ifdef _WIN32
	WSADATA wsaData;
	int nResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if(nResult != NO_ERROR)
	{
		fprintf(stderr, "WSAStartup() failed.\n");
		return -1;
	}
#endif
/*
    printf("upnpc : miniupnpc library test client. (c) 2005-2014 Thomas Bernard\n");
    printf("Go to http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/\n"
           "for more information.\n");
*/

	/* command line processing */
	for(i=1; i<argc; i++)
	{
		if(0 == strcmp(argv[i], "--help") || 0 == strcmp(argv[i], "-h"))
		{
			command = 0;
			break;
		}
		if(argv[i][0] == '-')
		{
			if(argv[i][1] == 'u')
				rootdescurl = argv[++i];
			else if(argv[i][1] == 'm')
				multicastif = argv[++i];
			else if(argv[i][1] == 'p')
				minissdpdpath = argv[++i];
			else if(argv[i][1] == '6')
				ipv6 = 1;
			else if(argv[i][1] == 'e')
				description = argv[++i];
			else
			{
				command = argv[i][1];
				i++;
				commandargv = argv + i;
				commandargc = argc - i;
				break;
			}
		}
		else
		{
			fprintf(stderr, "option '%s' invalid\n", argv[i]);
		}
	}

	if(!command || (command == 'a' && commandargc<4)
	   || (command == 'd' && argc<2)
	   || (command == 'r' && argc<2)
	   || (command == 'A' && commandargc<6)
	   || (command == 'U' && commandargc<2)
	   || (command == 'D' && commandargc<1))
	{
		fprintf(stderr, "Usage :\t%s [options] -a ip port external_port protocol [duration]\n\t\tAdd port redirection\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -d external_port protocol <remote host>\n\t\tDelete port redirection\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -s\n\t\tGet Connection status\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -l\n\t\tList redirections\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -L\n\t\tList redirections (using GetListOfPortMappings (for IGD:2 only)\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -n ip port external_port protocol [duration]\n\t\tAdd (any) port redirection allowing IGD to use alternative external_port (for IGD:2 only)\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -N external_port_start external_port_end protocol [manage]\n\t\tDelete range of port redirections (for IGD:2 only)\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -r port1 protocol1 [port2 protocol2] [...]\n\t\tAdd all redirections to the current host\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -A remote_ip remote_port internal_ip internal_port protocol lease_time\n\t\tAdd Pinhole (for IGD:2 only)\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -U uniqueID new_lease_time\n\t\tUpdate Pinhole (for IGD:2 only)\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -C uniqueID\n\t\tCheck if Pinhole is Working (for IGD:2 only)\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -K uniqueID\n\t\tGet Number of packets going through the rule (for IGD:2 only)\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -D uniqueID\n\t\tDelete Pinhole (for IGD:2 only)\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -S\n\t\tGet Firewall status (for IGD:2 only)\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -G remote_ip remote_port internal_ip internal_port protocol\n\t\tGet Outbound Pinhole Timeout (for IGD:2 only)\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -P\n\t\tGet Presentation url\n", argv[0]);
		fprintf(stderr, "\nprotocol is UDP or TCP\n");
		fprintf(stderr, "Options:\n");
		fprintf(stderr, "  -e description : set description for port mapping.\n");
		fprintf(stderr, "  -6 : use ip v6 instead of ip v4.\n");
		fprintf(stderr, "  -u url : bypass discovery process by providing the XML root description url.\n");
		fprintf(stderr, "  -m address/interface : provide ip address (ip v4) or interface name (ip v4 or v6) to use for sending SSDP multicast packets.\n");
		fprintf(stderr, "  -p path : use this path for MiniSSDPd socket.\n");
		return 1;
	}

#if 0
	if( rootdescurl
	  || (devlist = upnpDiscover(2000, multicastif, minissdpdpath,
	                             0/*sameport*/, ipv6, &error)))
#endif

#ifdef RTCONFIG_JFFS2USERICON
        if(!check_if_dir_exist("/tmp/upnpicon")) 
                system("mkdir /tmp/upnpicon");
#endif

	int x = 0;
	while(x<3)
	{
		devlist = upnpDiscoverAll(2000, multicastif, minissdpdpath, 1, ipv6, &error);
		if(devlist)
			break;
		x++;
		sleep(6);
	}

	if(devlist)
	{
		struct UPNPDev * device;
		struct UPNPUrls urls;
		struct IGDdatas data;
		if(devlist)
		{
#ifdef DEBUG
			printf("List of UPNP devices found on the network :\n");
			for(device = devlist; device; device = device->pNext)
			{
				printf(" desc: %s\n st: %s\n\n",
					   device->descURL, device->st);
			}
#endif
		}
		else
		{
			printf("upnpDiscover() error code=%d\n", error);
		}

		i = 1;

		if(i = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr)))
		{
			FILE *fp;
		        int i, shm_client_info_id;
		        void *shared_client_info=(void *) 0;
		        int lock;

			if(fp = fopen("/tmp/miniupnpc.log", "w"))
			{
				for(device = devlist; device; device = device->pNext)
			        {
					if(strcmp(device->DevInfo.hostname, ""))
					{
				                fprintf(fp,"%s>", device->DevInfo.hostname);
				                fprintf(fp,"%s>", device->DevInfo.type);
				                fprintf(fp,"%s\n", device->DevInfo.friendlyName);
					}
				}
				fclose(fp);
			}

#ifdef RTCONFIG_JFFS2USERICON
			get_icon_files();
		        lock = file_lock("networkmap");
		        shm_client_info_id = shmget((key_t)1001, sizeof(CLIENT_DETAIL_INFO_TABLE), 0666| IPC_CREAT);
		        if (shm_client_info_id == -1){
		            	perror("shmget failed:\n");
		            	file_unlock(lock);
		            	return 0;
		        }

		        shared_client_info = shmat(shm_client_info_id,(void *) 0,SHM_RDONLY);
		        if (shared_client_info == (void *)-1){
		                printf("shmat failed\n");
                		file_unlock(lock);
		                return 0;
		        }

			TransferUpnpIcon(shared_client_info);
#endif
			
#ifdef DEBUG
			switch(i) {
			case 1:
				printf("Found valid IGD : %s\n", urls.controlURL);
				break;
			case 2:
				printf("Found a (not connected?) IGD : %s\n", urls.controlURL);
				printf("Trying to continue anyway\n");
				break;
			case 3:
				printf("UPnP device found. Is it an IGD ? : %s\n", urls.controlURL);
				printf("Trying to continue anyway\n");
				break;
			default:
				printf("Found device (igd ?) : %s\n", urls.controlURL);
				printf("Trying to continue anyway\n");
			}
			printf("Local LAN ip address : %s\n", lanaddr);
#endif
			#if 0
			printf("getting \"%s\"\n", urls.ipcondescURL);
			descXML = miniwget(urls.ipcondescURL, &descXMLsize);
			if(descXML)
			{
				/*fwrite(descXML, 1, descXMLsize, stdout);*/
				free(descXML); descXML = NULL;
			}
			#endif

			switch(command)
			{
			case 'l':
				DisplayInfos(&urls, &data);
				ListRedirections(&urls, &data);
				break;
			case 'L':
				NewListRedirections(&urls, &data);
				break;
			case 'a':
				SetRedirectAndTest(&urls, &data,
						   commandargv[0], commandargv[1],
						   commandargv[2], commandargv[3],
						   (commandargc > 4)?commandargv[4]:"0",
						   description, 0);
				break;
			case 'd':
				RemoveRedirect(&urls, &data, commandargv[0], commandargv[1],
				               commandargc > 2 ? commandargv[2] : NULL);
				break;
			case 'n':	/* aNy */
				SetRedirectAndTest(&urls, &data,
						   commandargv[0], commandargv[1],
						   commandargv[2], commandargv[3],
						   (commandargc > 4)?commandargv[4]:"0",
						   description, 1);
				break;
			case 'N':
				if (commandargc < 3)
					fprintf(stderr, "too few arguments\n");

				RemoveRedirectRange(&urls, &data, commandargv[0], commandargv[1], commandargv[2],
						    commandargc > 3 ? commandargv[3] : NULL);
				break;
			case 's':
				GetConnectionStatus(&urls, &data);
				break;
			case 'r':
				for(i=0; i<commandargc; i+=2)
				{
					/*printf("port %s protocol %s\n", argv[i], argv[i+1]);*/
					SetRedirectAndTest(&urls, &data,
							   lanaddr, commandargv[i],
							   commandargv[i], commandargv[i+1], "0",
							   description, 0);
				}
				break;
			case 'A':
				SetPinholeAndTest(&urls, &data,
				                  commandargv[0], commandargv[1],
				                  commandargv[2], commandargv[3],
				                  commandargv[4], commandargv[5]);
				break;
			case 'U':
				GetPinholeAndUpdate(&urls, &data,
				                   commandargv[0], commandargv[1]);
				break;
			case 'C':
				for(i=0; i<commandargc; i++)
				{
					CheckPinhole(&urls, &data, commandargv[i]);
				}
				break;
			case 'K':
				for(i=0; i<commandargc; i++)
				{
					GetPinholePackets(&urls, &data, commandargv[i]);
				}
				break;
			case 'D':
				for(i=0; i<commandargc; i++)
				{
					RemovePinhole(&urls, &data, commandargv[i]);
				}
				break;
			case 'S':
				GetFirewallStatus(&urls, &data);
				break;
			case 'G':
				GetPinholeOutboundTimeout(&urls, &data,
							commandargv[0], commandargv[1],
							commandargv[2], commandargv[3],
							commandargv[4]);
				break;
			case 'P':
				printf("Presentation URL found:\n");
				printf("            %s\n", data.presentationurl);
				break;
			case 't':
				break;			
			default:
				fprintf(stderr, "Unknown switch -%c\n", command);
				retcode = 1;
			}

			FreeUPNPUrls(&urls);
		}
		else
		{
			fprintf(stderr, "No valid UPNP Internet Gateway Device found.\n");
			retcode = 1;
		}

		freeUPNPDevlist(devlist); devlist = 0;
	}
	else
	{
		fprintf(stderr, "No IGD UPnP Device found on the network !\n");
		retcode = 1;
	}
	return retcode;
}

