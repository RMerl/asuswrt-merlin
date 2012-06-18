/* MiniDLNA media server
 * This file is part of MiniDLNA.
 *
 * The code herein is based on the MiniUPnP Project.
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 *
 * Copyright (c) 2006, Thomas Bernard
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "config.h"
#include "upnpdescstrings.h"
#include "minidlnapath.h"
#include "upnphttp.h"
#include "upnpglobalvars.h"
#include "upnpreplyparse.h"
#include "getifaddr.h"
#include "minissdp.h"
#include "codelength.h"
#include "utils.h"
#include "log.h"

/* SSDP ip/port */
#define SSDP_PORT (1900)
#define SSDP_MCAST_ADDR ("239.255.255.250")

static int
AddMulticastMembership(int s, in_addr_t ifaddr)
{
	struct ip_mreq imr;	/* Ip multicast membership */

	/* setting up imr structure */
	imr.imr_multiaddr.s_addr = inet_addr(SSDP_MCAST_ADDR);
	/*imr.imr_interface.s_addr = htonl(INADDR_ANY);*/
	imr.imr_interface.s_addr = ifaddr;	/*inet_addr(ifaddr);*/
	
	if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&imr, sizeof(struct ip_mreq)) < 0)
	{
		DPRINTF(E_ERROR, L_SSDP, "setsockopt(udp, IP_ADD_MEMBERSHIP): %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

/* Open and configure the socket listening for 
 * SSDP udp packets sent on 239.255.255.250 port 1900 */
int
OpenAndConfSSDPReceiveSocket()
{
	int s;
	int i = 1;
	struct sockaddr_in sockname;
	
	if( (s = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
	{
		DPRINTF(E_ERROR, L_SSDP, "socket(udp): %s\n", strerror(errno));
		return -1;
	}	

	if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) < 0)
	{
		DPRINTF(E_WARN, L_SSDP, "setsockopt(udp, SO_REUSEADDR): %s\n", strerror(errno));
	}
	
	memset(&sockname, 0, sizeof(struct sockaddr_in));
	sockname.sin_family = AF_INET;
	sockname.sin_port = htons(SSDP_PORT);
	/* NOTE : it seems it doesnt work when binding on the specific address */
	/*sockname.sin_addr.s_addr = inet_addr(UPNP_MCAST_ADDR);*/
	sockname.sin_addr.s_addr = htonl(INADDR_ANY);
	/*sockname.sin_addr.s_addr = inet_addr(ifaddr);*/

	if(bind(s, (struct sockaddr *)&sockname, sizeof(struct sockaddr_in)) < 0)
	{
		DPRINTF(E_ERROR, L_SSDP, "bind(udp): %s\n", strerror(errno));
		close(s);
		return -1;
	}

	i = n_lan_addr;
	while(i>0)
	{
		i--;
		if(AddMulticastMembership(s, lan_addr[i].addr.s_addr) < 0)
		{
			DPRINTF(E_WARN, L_SSDP,
			       "Failed to add multicast membership for address %s\n", 
			       lan_addr[i].str );
		}
	}

	return s;
}

/* open the UDP socket used to send SSDP notifications to
 * the multicast group reserved for them */
static int
OpenAndConfSSDPNotifySocket(in_addr_t addr)
{
	int s;
	unsigned char loopchar = 0;
	int bcast = 1;
	uint8_t ttl = 4;
	struct in_addr mc_if;
	struct sockaddr_in sockname;
	
	if( (s = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
	{
		DPRINTF(E_ERROR, L_SSDP, "socket(udp_notify): %s\n", strerror(errno));
		return -1;
	}

	mc_if.s_addr = addr;	/*inet_addr(addr);*/

	if(setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopchar, sizeof(loopchar)) < 0)
	{
		DPRINTF(E_ERROR, L_SSDP, "setsockopt(udp_notify, IP_MULTICAST_LOOP): %s\n", strerror(errno));
		close(s);
		return -1;
	}

	if(setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, (char *)&mc_if, sizeof(mc_if)) < 0)
	{
		DPRINTF(E_ERROR, L_SSDP, "setsockopt(udp_notify, IP_MULTICAST_IF): %s\n", strerror(errno));
		close(s);
		return -1;
	}

	setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
	
	if(setsockopt(s, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast)) < 0)
	{
		DPRINTF(E_ERROR, L_SSDP, "setsockopt(udp_notify, SO_BROADCAST): %s\n", strerror(errno));
		close(s);
		return -1;
	}

	memset(&sockname, 0, sizeof(struct sockaddr_in));
	sockname.sin_family = AF_INET;
	sockname.sin_addr.s_addr = addr;	/*inet_addr(addr);*/

	if (bind(s, (struct sockaddr *)&sockname, sizeof(struct sockaddr_in)) < 0)
	{
		DPRINTF(E_ERROR, L_SSDP, "bind(udp_notify): %s\n", strerror(errno));
		close(s);
		return -1;
	}

	return s;
}

int
OpenAndConfSSDPNotifySockets(int * sockets)
{
	int i, j;
	for(i=0; i<n_lan_addr; i++)
	{
		sockets[i] = OpenAndConfSSDPNotifySocket(lan_addr[i].addr.s_addr);
		if(sockets[i] < 0)
		{
			for(j=0; j<i; j++)
			{
				close(sockets[j]);
				sockets[j] = -1;
			}
			return -1;
		}
	}
	return 0;
}

/*
 * response from a LiveBox (Wanadoo)
HTTP/1.1 200 OK
CACHE-CONTROL: max-age=1800
DATE: Thu, 01 Jan 1970 04:03:23 GMT
EXT:
LOCATION: http://192.168.0.1:49152/gatedesc.xml
SERVER: Linux/2.4.17, UPnP/1.0, Intel SDK for UPnP devices /1.2
ST: upnp:rootdevice
USN: uuid:75802409-bccb-40e7-8e6c-fa095ecce13e::upnp:rootdevice

 * response from a Linksys 802.11b :
HTTP/1.1 200 OK
Cache-Control:max-age=120
Location:http://192.168.5.1:5678/rootDesc.xml
Server:NT/5.0 UPnP/1.0
ST:upnp:rootdevice
USN:uuid:upnp-InternetGatewayDevice-1_0-0090a2777777::upnp:rootdevice
EXT:
 */

static const char * const known_service_types[] =
{
	uuidvalue,
	"upnp:rootdevice",
	"urn:schemas-upnp-org:device:MediaServer:",
	"urn:schemas-upnp-org:service:ContentDirectory:",
	"urn:schemas-upnp-org:service:ConnectionManager:",
	"urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:",
	0
};

static void
_usleep(long usecs)
{
	struct timespec sleep_time;

	sleep_time.tv_sec = 0;
	sleep_time.tv_nsec = usecs * 1000;
	nanosleep(&sleep_time, NULL);
}

/* not really an SSDP "announce" as it is the response
 * to a SSDP "M-SEARCH" */
static void
SendSSDPAnnounce2(int s, struct sockaddr_in sockname, int st_no,
                  const char * host, unsigned short port)
{
	int l, n;
	char buf[512];
	/*
	 * follow guideline from document "UPnP Device Architecture 1.0"
	 * uppercase is recommended.
	 * DATE: is recommended
	 * SERVER: OS/ver UPnP/1.0 minidlna/1.0
	 * - check what to put in the 'Cache-Control' header 
	 * */
	char   szTime[30];
	time_t tTime = time(NULL);
	strftime(szTime, 30,"%a, %d %b %Y %H:%M:%S GMT" , gmtime(&tTime));

	l = snprintf(buf, sizeof(buf), "HTTP/1.1 200 OK\r\n"
		"CACHE-CONTROL: max-age=%u\r\n"
		"DATE: %s\r\n"
		"ST: %s%s\r\n"
		"USN: %s%s%s%s\r\n"
		"EXT:\r\n"
		"SERVER: " MINIDLNA_SERVER_STRING "\r\n"
		"LOCATION: http://%s:%u" ROOTDESC_PATH "\r\n"
		"Content-Length: 0\r\n"
		"\r\n",
		(runtime_vars.notify_interval<<1)+10,
		szTime,
		known_service_types[st_no], (st_no>1?"1":""),
		uuidvalue, (st_no>0?"::":""), (st_no>0?known_service_types[st_no]:""), (st_no>1?"1":""),
		host, (unsigned int)port);
	//DEBUG DPRINTF(E_DEBUG, L_SSDP, "Sending M-SEARCH response:\n%s", buf);
	n = sendto(s, buf, l, 0,
	           (struct sockaddr *)&sockname, sizeof(struct sockaddr_in) );
	if(n < 0)
	{
		DPRINTF(E_ERROR, L_SSDP, "sendto(udp): %s\n", strerror(errno));
	}
}

static void
SendSSDPNotifies(int s, const char * host, unsigned short port,
                 unsigned int lifetime)
{
	struct sockaddr_in sockname;
	int l, n, dup, i=0;
	char bufr[512];

	memset(&sockname, 0, sizeof(struct sockaddr_in));
	sockname.sin_family = AF_INET;
	sockname.sin_port = htons(SSDP_PORT);
	sockname.sin_addr.s_addr = inet_addr(SSDP_MCAST_ADDR);

	for( dup=0; dup<2; dup++ )
	{
		if( dup )
			_usleep(200000);
		i=0;
		while(known_service_types[i])
		{
			l = snprintf(bufr, sizeof(bufr), 
					"NOTIFY * HTTP/1.1\r\n"
					"HOST:%s:%d\r\n"
					"CACHE-CONTROL:max-age=%u\r\n"
					"LOCATION:http://%s:%d" ROOTDESC_PATH"\r\n"
					"SERVER: " MINIDLNA_SERVER_STRING "\r\n"
					"NT:%s%s\r\n"
					"USN:%s%s%s%s\r\n"
					"NTS:ssdp:alive\r\n"
					"\r\n",
					SSDP_MCAST_ADDR, SSDP_PORT,
					lifetime,
					host, port,
					known_service_types[i], (i>1?"1":""),
					uuidvalue, (i>0?"::":""), (i>0?known_service_types[i]:""), (i>1?"1":"") );
			if(l>=sizeof(bufr))
			{
				DPRINTF(E_WARN, L_SSDP, "SendSSDPNotifies(): truncated output\n");
				l = sizeof(bufr);
			}
			//DEBUG DPRINTF(E_DEBUG, L_SSDP, "Sending NOTIFY:\n%s", bufr);
			n = sendto(s, bufr, l, 0,
				(struct sockaddr *)&sockname, sizeof(struct sockaddr_in) );
			if(n < 0)
			{
				DPRINTF(E_ERROR, L_SSDP, "sendto(udp_notify=%d, %s): %s\n", s, host, strerror(errno));
			}
			i++;
		}
	}
}

void
SendSSDPNotifies2(int * sockets,
                  unsigned short port,
                  unsigned int lifetime)
/*SendSSDPNotifies2(int * sockets, struct lan_addr_s * lan_addr, int n_lan_addr,
                  unsigned short port,
                  unsigned int lifetime)*/
{
	int i;
	DPRINTF(E_DEBUG, L_SSDP, "Sending SSDP notifies\n");
	for(i=0; i<n_lan_addr; i++)
	{
		SendSSDPNotifies(sockets[i], lan_addr[i].str, port, lifetime);
	}
}

void
ParseUPnPClient(char *location)
{
	char buf[8192];
	struct sockaddr_in dest;
	int s, n, do_headers = 0, nread = 0;
	struct timeval tv;
	char *addr, *path, *port_str;
	long port = 80;
	char *off = NULL, *p;
	int content_len = sizeof(buf);
	struct NameValueParserData xml;
	int client;
	enum client_types type = 0;
	uint32_t flags = 0;
	char *model, *serial, *name;

	if (strncmp(location, "http://", 7) != 0)
		return;
	path = location + 7;
	port_str = strsep(&path, "/");
	if (!path)
		return;
	addr = strsep(&port_str, ":");
	if (port_str)
	{
		port = strtol(port_str, NULL, 10);
		if (!port)
			port = 80;
	}

	memset(&dest, '\0', sizeof(dest));
	if (!inet_aton(addr, &dest.sin_addr))
		return;
	/* Check if the client is already in cache */
	dest.sin_family = AF_INET;
	dest.sin_port = htons(port);

	s = socket(PF_INET, SOCK_STREAM, 0);
	if( s < 0 )
		return;

	tv.tv_sec = 0;
	tv.tv_usec = 500000;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	if( connect(s, (struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0 )
		goto close;

	n = snprintf(buf, sizeof(buf), "GET /%s HTTP/1.0\r\n"
	                               "HOST: %s:%ld\r\n\r\n",
	                               path, addr, port);
	if( write(s, buf, n) < 1 )
		goto close;

	while( (n = read(s, buf+nread, sizeof(buf)-nread-1)) > 0 )
	{
		nread += n;
		buf[nread] = '\0';
		n = nread;
		p = buf;

		while( !off && n-- > 0 )
		{
			if(p[0]=='\r' && p[1]=='\n' && p[2]=='\r' && p[3]=='\n')
			{
				off = p + 4;
				do_headers = 1;
			}
			p++;
		}
		if( !off )
			continue;

		if( do_headers )
		{
			p = buf;
			if( strncmp(p, "HTTP/", 5) != 0 )
				goto close;
			while(*p != ' ' && *p != '\t') p++;
			/* If we don't get a 200 status, ignore it */
			if( strtol(p, NULL, 10) != 200 )
				goto close;
			if( (p = strcasestr(p, "Content-Length:")) )
				content_len = strtol(p+15, NULL, 10);
			do_headers = 0;
		}
		if( buf + nread - off >= content_len )
			break;
	}
close:
	close(s);
	if( !off )
		return;
	nread -= off - buf;
	ParseNameValue(off, nread, &xml);
	model = GetValueFromNameValueList(&xml, "modelName");
	serial = GetValueFromNameValueList(&xml, "serialNumber");
	name = GetValueFromNameValueList(&xml, "friendlyName");
	if( model )
	{
		DPRINTF(E_DEBUG, L_SSDP, "Model: %s\n", model);
		if( strstr(model, "Roku SoundBridge") )
		{
			type = ERokuSoundBridge;
			flags |= FLAG_MS_PFS;
			flags |= FLAG_AUDIO_ONLY;
			flags |= FLAG_MIME_WAV_WAV;
		}
		else if( strcmp(model, "Samsung DTV DMR") == 0 && serial )
		{
			DPRINTF(E_DEBUG, L_SSDP, "Serial: %s\n", serial);
			/* The Series B I saw was 20081224DMR.  Series A should be older than that. */
			if( atoi(serial) > 20081200 )
			{
				type = ESamsungSeriesB;
				flags |= FLAG_SAMSUNG;
				flags |= FLAG_DLNA;
				flags |= FLAG_NO_RESIZE;
			}
		}
		else
		{
			if( name && (strcmp(name, "marantz DMP") == 0) )
			{
				type = EMarantzDMP;
				flags |= FLAG_DLNA;
				flags |= FLAG_MIME_WAV_WAV;
			}
		}
	}
	ClearNameValueList(&xml);
	if( !type )
		return;
	/* Add this client to the cache if it's not there already. */
	client = SearchClientCache(dest.sin_addr, 1);
	if( client < 0 )
	{
		for( client=0; client<CLIENT_CACHE_SLOTS; client++ )
		{
			if( clients[client].addr.s_addr )
				continue;
			get_remote_mac(dest.sin_addr, clients[client].mac);
			clients[client].addr = dest.sin_addr;
			DPRINTF(E_DEBUG, L_SSDP, "Added client [%d/%s/%02X:%02X:%02X:%02X:%02X:%02X] to cache slot %d.\n",
			                         type, inet_ntoa(clients[client].addr),
			                         clients[client].mac[0], clients[client].mac[1], clients[client].mac[2],
			                         clients[client].mac[3], clients[client].mac[4], clients[client].mac[5], client);
			break;
		}
	}
	clients[client].type = type;
	clients[client].flags = flags;
	clients[client].age = time(NULL);
}

/* ProcessSSDPRequest()
 * process SSDP M-SEARCH requests and responds to them */
void
ProcessSSDPRequest(int s, unsigned short port)
{
	int n;
	char bufr[1500];
	socklen_t len_r;
	struct sockaddr_in sendername;
	int i;
	char *st = NULL, *mx = NULL, *man = NULL, *mx_end = NULL;
	int man_len = 0;
	len_r = sizeof(struct sockaddr_in);

	n = recvfrom(s, bufr, sizeof(bufr)-1, 0,
	             (struct sockaddr *)&sendername, &len_r);
	if(n < 0)
	{
		DPRINTF(E_ERROR, L_SSDP, "recvfrom(udp): %s\n", strerror(errno));
		return;
	}
	bufr[n] = '\0';

	if(memcmp(bufr, "NOTIFY", 6) == 0)
	{
		char *loc = NULL, *srv = NULL, *nts = NULL, *nt = NULL;
		int loc_len = 0;
		//DEBUG DPRINTF(E_DEBUG, L_SSDP, "Received SSDP notify:\n%.*s", n, bufr);
		for(i=0; i < n; i++)
		{
			if( bufr[i] == '*' )
				break;
		}
		if( !strcasestrc(bufr+i, "HTTP/1.1", '\r') )
			return;
		while(i < n)
		{
			while((i < n - 2) && (bufr[i] != '\r' || bufr[i+1] != '\n'))
				i++;
			i += 2;
			if(strncasecmp(bufr+i, "SERVER:", 7) == 0)
			{
				srv = bufr+i+7;
				while(*srv == ' ' || *srv == '\t') srv++;
			}
			else if(strncasecmp(bufr+i, "LOCATION:", 9) == 0)
			{
				loc = bufr+i+9;
				while(*loc == ' ' || *loc == '\t') loc++;
				while(loc[loc_len]!='\r' && loc[loc_len]!='\n') loc_len++;
			}
			else if(strncasecmp(bufr+i, "NTS:", 4) == 0)
			{
				nts = bufr+i+4;
				while(*nts == ' ' || *nts == '\t') nts++;
			}
			else if(strncasecmp(bufr+i, "NT:", 3) == 0)
			{
				nt = bufr+i+3;
				while(*nt == ' ' || *nt == '\t') nt++;
			}
		}
		if( !loc || !srv || !nt || !nts || (strncmp(nts, "ssdp:alive", 10) != 0) ||
		    (strncmp(nt, "urn:schemas-upnp-org:device:MediaRenderer", 41) != 0) )
		{
			return;
		}
		loc[loc_len] = '\0';
		if( (strncmp(srv, "Allegro-Software-RomPlug", 24) == 0) || /* Roku */
		    (strstr(loc, "SamsungMRDesc.xml") != NULL) || /* Samsung TV */
		    (strstrc(srv, "DigiOn DiXiM", '\r') != NULL) ) /* Marantz Receiver */
		{
			/* Check if the client is already in cache */
			i = SearchClientCache(sendername.sin_addr, 1);
			if( i >= 0 )
			{
				if( clients[i].type < EStandardDLNA150 &&
				    clients[i].type != ESamsungSeriesA )
				{
					clients[i].age = time(NULL);
					return;
				}
			}
			ParseUPnPClient(loc);
		}
	}
	else if(memcmp(bufr, "M-SEARCH", 8) == 0)
	{
		int st_len = 0, mx_len = 0, mx_val = 0;
		//DPRINTF(E_DEBUG, L_SSDP, "Received SSDP request:\n%.*s\n", n, bufr);
		for(i=0; i < n; i++)
		{
			if( bufr[i] == '*' )
				break;
		}
		if( !strcasestrc(bufr+i, "HTTP/1.1", '\r') )
			return;
		while(i < n)
		{
			while((i < n - 2) && (bufr[i] != '\r' || bufr[i+1] != '\n'))
				i++;
			i += 2;
			if(strncasecmp(bufr+i, "ST:", 3) == 0)
			{
				st = bufr+i+3;
				st_len = 0;
				while(*st == ' ' || *st == '\t') st++;
				while(st[st_len]!='\r' && st[st_len]!='\n') st_len++;
			}
			else if(strncasecmp(bufr+i, "MX:", 3) == 0)
			{
				mx = bufr+i+3;
				mx_len = 0;
				while(*mx == ' ' || *mx == '\t') mx++;
				while(mx[mx_len]!='\r' && mx[mx_len]!='\n') mx_len++;
        			mx_val = strtol(mx, &mx_end, 10);
			}
			else if(strncasecmp(bufr+i, "MAN:", 4) == 0)
			{
				man = bufr+i+4;
				man_len = 0;
				while(*man == ' ' || *man == '\t') man++;
				while(man[man_len]!='\r' && man[man_len]!='\n') man_len++;
			}
		}
		/*DPRINTF(E_INFO, L_SSDP, "SSDP M-SEARCH packet received from %s:%d\n",
	           inet_ntoa(sendername.sin_addr),
	           ntohs(sendername.sin_port) );*/
		if( GETFLAG(DLNA_STRICT_MASK) && (ntohs(sendername.sin_port) <= 1024 || ntohs(sendername.sin_port) == 1900) )
		{
			DPRINTF(E_INFO, L_SSDP, "WARNING: Ignoring invalid SSDP M-SEARCH from %s [bad source port %d]\n",
			   inet_ntoa(sendername.sin_addr), ntohs(sendername.sin_port));
		}
		else if( !man || (strncmp(man, "\"ssdp:discover\"", 15) != 0) )
		{
			DPRINTF(E_INFO, L_SSDP, "WARNING: Ignoring invalid SSDP M-SEARCH from %s [bad %s header '%.*s']\n",
			   inet_ntoa(sendername.sin_addr), "MAN", man_len, man);
		}
		else if( !mx || mx == mx_end || mx_val < 0 ) {
			DPRINTF(E_INFO, L_SSDP, "WARNING: Ignoring invalid SSDP M-SEARCH from %s [bad %s header '%.*s']\n",
			   inet_ntoa(sendername.sin_addr), "MX", mx_len, mx);
		}
		else if( st && (st_len > 0) )
		{
			int l;
			int lan_addr_index = 0;
			/* find in which sub network the client is */
			for(i = 0; i<n_lan_addr; i++)
			{
				if( (sendername.sin_addr.s_addr & lan_addr[i].mask.s_addr)
				   == (lan_addr[i].addr.s_addr & lan_addr[i].mask.s_addr))
				{
					lan_addr_index = i;
					break;
				}
			}
			if( i == n_lan_addr )
			{
				DPRINTF(E_DEBUG, L_SSDP, "Ignoring SSDP M-SEARCH on other interface [%s]\n",
					inet_ntoa(sendername.sin_addr));
				return;
			}
			DPRINTF(E_INFO, L_SSDP, "SSDP M-SEARCH from %s:%d ST: %.*s, MX: %.*s, MAN: %.*s\n",
	        	   inet_ntoa(sendername.sin_addr),
	           	   ntohs(sendername.sin_port),
			   st_len, st, mx_len, mx, man_len, man);
			/* Responds to request with a device as ST header */
			for(i = 0; known_service_types[i]; i++)
			{
				l = strlen(known_service_types[i]);
				if( l <= st_len && (memcmp(st, known_service_types[i], l) == 0))
				{
					if( st_len != l )
					{
						/* Check version number - must always be 1 currently. */
						if( (st[l-1] == ':') && (st[l] == '1') )
							l++;
						while( l < st_len )
						{
							if( !isspace(st[l]) )
							{
								DPRINTF(E_DEBUG, L_SSDP, "Ignoring SSDP M-SEARCH with bad extra data [%s]\n",
									inet_ntoa(sendername.sin_addr));
								break;
							}
							l++;
						}
						if( l != st_len )
							break;
					}
					_usleep(random()>>20);
					SendSSDPAnnounce2(s, sendername,
					                  i,
					                  lan_addr[lan_addr_index].str, port);
					break;
				}
			}
			/* Responds to request with ST: ssdp:all */
			/* strlen("ssdp:all") == 8 */
			if(st_len==8 && (0 == memcmp(st, "ssdp:all", 8)))
			{
				for(i=0; known_service_types[i]; i++)
				{
					l = (int)strlen(known_service_types[i]);
					SendSSDPAnnounce2(s, sendername,
					                  i,
					                  lan_addr[lan_addr_index].str, port);
				}
			}
		}
		else
		{
			DPRINTF(E_INFO, L_SSDP, "Invalid SSDP M-SEARCH from %s:%d\n",
	        	   inet_ntoa(sendername.sin_addr), ntohs(sendername.sin_port));
		}
	}
	else
	{
		DPRINTF(E_WARN, L_SSDP, "Unknown udp packet received from %s:%d\n",
		       inet_ntoa(sendername.sin_addr), ntohs(sendername.sin_port));
	}
}

/* This will broadcast ssdp:byebye notifications to inform 
 * the network that UPnP is going down. */
int
SendSSDPGoodbye(int * sockets, int n_sockets)
{
	struct sockaddr_in sockname;
	int n, l;
	int i, j;
	int dup, ret = 0;
	char bufr[512];

	memset(&sockname, 0, sizeof(struct sockaddr_in));
	sockname.sin_family = AF_INET;
	sockname.sin_port = htons(SSDP_PORT);
	sockname.sin_addr.s_addr = inet_addr(SSDP_MCAST_ADDR);

	for (dup = 0; dup < 2; dup++)
	{
		for(j=0; j<n_sockets; j++)
		{
			for(i=0; known_service_types[i]; i++)
			{
				l = snprintf(bufr, sizeof(bufr),
				             "NOTIFY * HTTP/1.1\r\n"
				             "HOST:%s:%d\r\n"
				             "NT:%s%s\r\n"
				             "USN:%s%s%s%s\r\n"
			        	     "NTS:ssdp:byebye\r\n"
				             "\r\n",
				             SSDP_MCAST_ADDR, SSDP_PORT,
				             known_service_types[i], (i>1?"1":""),
				             uuidvalue, (i>0?"::":""), (i>0?known_service_types[i]:""), (i>1?"1":"") );
				//DEBUG DPRINTF(E_DEBUG, L_SSDP, "Sending NOTIFY:\n%s", bufr);
				n = sendto(sockets[j], bufr, l, 0,
				           (struct sockaddr *)&sockname, sizeof(struct sockaddr_in) );
				if(n < 0)
				{
					DPRINTF(E_ERROR, L_SSDP, "sendto(udp_shutdown=%d): %s\n", sockets[j], strerror(errno));
					ret = -1;
					break;
				}
			}
		}
	}
	return ret;
}

/* SubmitServicesToMiniSSDPD() :
 * register services offered by MiniUPnPd to a running instance of
 * MiniSSDPd */
int
SubmitServicesToMiniSSDPD(const char * host, unsigned short port) {
	struct sockaddr_un addr;
	int s;
	unsigned char buffer[2048];
	char strbuf[256];
	unsigned char * p;
	int i, l;

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if(s < 0) {
		DPRINTF(E_ERROR, L_SSDP, "socket(unix): %s", strerror(errno));
		return -1;
	}
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, minissdpdsocketpath, sizeof(addr.sun_path));
	if(connect(s, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
		DPRINTF(E_ERROR, L_SSDP, "connect(\"%s\"): %s",
		        minissdpdsocketpath, strerror(errno));
		return -1;
	}
	for(i = 0; known_service_types[i]; i++) {
		buffer[0] = 4;
		p = buffer + 1;
		l = (int)strlen(known_service_types[i]);
		if(i > 0)
			l++;
		CODELENGTH(l, p);
		memcpy(p, known_service_types[i], l);
		if(i > 0)
			p[l-1] = '1';
		p += l;
		l = snprintf(strbuf, sizeof(strbuf), "%s::%s%s", 
		             uuidvalue, known_service_types[i], (i==0)?"":"1");
		CODELENGTH(l, p);
		memcpy(p, strbuf, l);
		p += l;
		l = (int)strlen(MINIDLNA_SERVER_STRING);
		CODELENGTH(l, p);
		memcpy(p, MINIDLNA_SERVER_STRING, l);
		p += l;
		l = snprintf(strbuf, sizeof(strbuf), "http://%s:%u" ROOTDESC_PATH,
		             host, (unsigned int)port);
		CODELENGTH(l, p);
		memcpy(p, strbuf, l);
		p += l;
		if(write(s, buffer, p - buffer) < 0) {
			DPRINTF(E_ERROR, L_SSDP, "write(): %s", strerror(errno));
			return -1;
		}
	}
 	close(s);
	return 0;
}

