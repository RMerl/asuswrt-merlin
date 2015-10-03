/* $Id: miniupnpc.c,v 1.123 2014/11/27 12:02:25 nanard Exp $ */
/* Project : miniupnp
 * Web : http://miniupnp.free.fr/
 * Author : Thomas BERNARD
 * copyright (c) 2005-2014 Thomas Bernard
 * This software is subjet to the conditions detailed in the
 * provided LICENSE file. */
#define __EXTENSIONS__ 1
#if !defined(__APPLE__) && !defined(__sun)
#if !defined(_XOPEN_SOURCE) && !defined(__OpenBSD__) && !defined(__NetBSD__)
#ifndef __cplusplus
#define _XOPEN_SOURCE 600
#endif
#endif
#ifndef __BSD_VISIBLE
#define __BSD_VISIBLE 1
#endif
#endif

#if !defined(__DragonFly__) && !defined(__OpenBSD__) && !defined(__NetBSD__) && !defined(__APPLE__) && !defined(_WIN32) && !defined(__CYGWIN__) && !defined(__sun) && !defined(__GNU__) && !defined(__FreeBSD_kernel__)
#define HAS_IP_MREQN
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
/* Win32 Specific includes and defines */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <iphlpapi.h>
#define snprintf _snprintf
#define strdup _strdup
#ifndef strncasecmp
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#define strncasecmp _memicmp
#else /* defined(_MSC_VER) && (_MSC_VER >= 1400) */
#define strncasecmp memicmp
#endif /* defined(_MSC_VER) && (_MSC_VER >= 1400) */
#endif /* #ifndef strncasecmp */
#define MAXHOSTNAMELEN 64
#else /* #ifdef _WIN32 */
/* Standard POSIX includes */
#include <unistd.h>
#if defined(__amigaos__) && !defined(__amigaos4__)
/* Amiga OS 3 specific stuff */
#define socklen_t int
#else
#include <sys/select.h>
#endif
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#if !defined(__amigaos__) && !defined(__amigaos4__)
#include <poll.h>
#endif
#include <strings.h>
#include <errno.h>
#define closesocket close
#endif /* #else _WIN32 */
#ifdef MINIUPNPC_SET_SOCKET_TIMEOUT
#include <sys/time.h>
#endif
#if defined(__amigaos__) || defined(__amigaos4__)
/* Amiga OS specific stuff */
#define TIMEVAL struct timeval
#endif
#ifdef __GNU__
#define MAXHOSTNAMELEN 64
#endif

#include <shared.h>

#if defined(HAS_IP_MREQN) && defined(NEED_STRUCT_IP_MREQN)
/* Several versions of glibc don't define this structure, define it here and compile with CFLAGS NEED_STRUCT_IP_MREQN */
struct ip_mreqn
{
	struct in_addr	imr_multiaddr;		/* IP multicast address of group */
	struct in_addr	imr_address;		/* local IP address of interface */
	int		imr_ifindex;		/* Interface index */
};
#endif

#include "miniupnpc.h"
#include "minissdpc.h"
#include "miniwget.h"
#include "minisoap.h"
#include "minixml.h"
#include "upnpcommands.h"
#include "connecthostport.h"
#include "receivedata.h"

/* compare the begining of a string with a constant string */
#define COMPARE(str, cstr) (0==memcmp(str, cstr, sizeof(cstr) - 1))

#ifdef _WIN32
#define PRINT_SOCKET_ERROR(x)    printf("Socket error: %s, %d\n", x, WSAGetLastError());
#else
#define PRINT_SOCKET_ERROR(x) perror(x)
#endif

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#define SOAPPREFIX "s"
#define SERVICEPREFIX "u"
#define SERVICEPREFIX2 'u'


/* root description parsing */
MINIUPNP_LIBSPEC void parserootdesc(const char * buffer, int bufsize, struct IGDdatas * data)
{
	struct xmlparser parser;
	/* xmlparser object */
	parser.xmlstart = buffer;
	parser.xmlsize = bufsize;
	parser.data = data;
	parser.starteltfunc = IGDstartelt;
	parser.endeltfunc = IGDendelt;
	parser.datafunc = IGDdata;
	parser.attfunc = 0;
	parsexml(&parser);
#ifdef DEBUG
/*
	printIGD(data);
*/
#endif
}

/* simpleUPnPcommand2 :
 * not so simple !
 * return values :
 *   pointer - OK
 *   NULL - error */
char * simpleUPnPcommand2(int s, const char * url, const char * service,
		       const char * action, struct UPNParg * args,
		       int * bufsize, const char * httpversion)
{
	char hostname[MAXHOSTNAMELEN+1];
	unsigned short port = 0;
	char * path;
	char soapact[128];
	char soapbody[2048];
	char * buf;
    int n;

	*bufsize = 0;
	snprintf(soapact, sizeof(soapact), "%s#%s", service, action);
	if(args==NULL)
	{
		/*soapbodylen = */snprintf(soapbody, sizeof(soapbody),
						"<?xml version=\"1.0\"?>\r\n"
	    	              "<" SOAPPREFIX ":Envelope "
						  "xmlns:" SOAPPREFIX "=\"http://schemas.xmlsoap.org/soap/envelope/\" "
						  SOAPPREFIX ":encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
						  "<" SOAPPREFIX ":Body>"
						  "<" SERVICEPREFIX ":%s xmlns:" SERVICEPREFIX "=\"%s\">"
						  "</" SERVICEPREFIX ":%s>"
						  "</" SOAPPREFIX ":Body></" SOAPPREFIX ":Envelope>"
					 	  "\r\n", action, service, action);
	}
	else
	{
		char * p;
		const char * pe, * pv;
		int soapbodylen;
		soapbodylen = snprintf(soapbody, sizeof(soapbody),
						"<?xml version=\"1.0\"?>\r\n"
	    	            "<" SOAPPREFIX ":Envelope "
						"xmlns:" SOAPPREFIX "=\"http://schemas.xmlsoap.org/soap/envelope/\" "
						SOAPPREFIX ":encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
						"<" SOAPPREFIX ":Body>"
						"<" SERVICEPREFIX ":%s xmlns:" SERVICEPREFIX "=\"%s\">",
						action, service);
		p = soapbody + soapbodylen;
		while(args->elt)
		{
			/* check that we are never overflowing the string... */
			if(soapbody + sizeof(soapbody) <= p + 100)
			{
				/* we keep a margin of at least 100 bytes */
				return NULL;
			}
			*(p++) = '<';
			pe = args->elt;
			while(*pe)
				*(p++) = *(pe++);
			*(p++) = '>';
			if((pv = args->val))
			{
				while(*pv)
					*(p++) = *(pv++);
			}
			*(p++) = '<';
			*(p++) = '/';
			pe = args->elt;
			while(*pe)
				*(p++) = *(pe++);
			*(p++) = '>';
			args++;
		}
		*(p++) = '<';
		*(p++) = '/';
		*(p++) = SERVICEPREFIX2;
		*(p++) = ':';
		pe = action;
		while(*pe)
			*(p++) = *(pe++);
		strncpy(p, "></" SOAPPREFIX ":Body></" SOAPPREFIX ":Envelope>\r\n",
		        soapbody + sizeof(soapbody) - p);
	}
	if(!parseURL(url, hostname, &port, &path, NULL)) return NULL;
	if(s < 0) {
		s = connecthostport(hostname, port, 0);
		if(s < 0) {
			/* failed to connect */
			return NULL;
		}
	}

	n = soapPostSubmit(s, path, hostname, port, soapact, soapbody, httpversion);
	if(n<=0) {
#ifdef DEBUG
		printf("Error sending SOAP request\n");
#endif
		closesocket(s);
		return NULL;
	}

	buf = getHTTPResponse(s, bufsize, NULL);
#ifdef DEBUG
	if(*bufsize > 0 && buf)
	{
		printf("SOAP Response :\n%.*s\n", *bufsize, buf);
	}
#endif
	closesocket(s);
	return buf;
}

/* simpleUPnPcommand :
 * not so simple !
 * return values :
 *   pointer - OK
 *   NULL    - error */
char * simpleUPnPcommand(int s, const char * url, const char * service,
		       const char * action, struct UPNParg * args,
		       int * bufsize)
{
	char * buf;

#if 1
	buf = simpleUPnPcommand2(s, url, service, action, args, bufsize, "1.1");
#else
	buf = simpleUPnPcommand2(s, url, service, action, args, bufsize, "1.0");
	if (!buf || *bufsize == 0)
	{
#if DEBUG
	    printf("Error or no result from SOAP request; retrying with HTTP/1.1\n");
#endif
		buf = simpleUPnPcommand2(s, url, service, action, args, bufsize, "1.1");
	}
#endif
	return buf;
}

/* parseMSEARCHReply()
 * the last 4 arguments are filled during the parsing :
 *    - location/locationsize : "location:" field of the SSDP reply packet
 *    - st/stsize : "st:" field of the SSDP reply packet.
 * The strings are NOT null terminated */
static void
parseMSEARCHReply(const char * reply, int size,
                  const char * * location, int * locationsize,
			      const char * * st, int * stsize)
{
	int a, b, i;
	i = 0;
	a = i;	/* start of the line */
	b = 0;	/* end of the "header" (position of the colon) */
	while(i<size)
	{
		switch(reply[i])
		{
		case ':':
				if(b==0)
				{
					b = i; /* end of the "header" */
					/*for(j=a; j<b; j++)
					{
						putchar(reply[j]);
					}
					*/
				}
				break;
		case '\x0a':
		case '\x0d':
				if(b!=0)
				{
					/*for(j=b+1; j<i; j++)
					{
						putchar(reply[j]);
					}
					putchar('\n');*/
					/* skip the colon and white spaces */
					do { b++; } while(reply[b]==' ');
					if(0==strncasecmp(reply+a, "location", 8))
					{
						*location = reply+b;
						*locationsize = i-b;
					}
					else if(0==strncasecmp(reply+a, "st", 2))
					{
						*st = reply+b;
						*stsize = i-b;
					}
					b = 0;
				}
				a = i+1;
				break;
		default:
				break;
		}
		i++;
	}
}

/* port upnp discover : SSDP protocol */
#define PORT 1900
#define XSTR(s) STR(s)
#define STR(s) #s
#define UPNP_MCAST_ADDR "239.255.255.250"
/* for IPv6 */
#define UPNP_MCAST_LL_ADDR "FF02::C" /* link-local */
#define UPNP_MCAST_SL_ADDR "FF05::C" /* site-local */

/* upnpDiscoverDevices() :
 * return a chained list of all devices found or NULL if
 * no devices was found.
 * It is up to the caller to free the chained list
 * delay is in millisecond (poll) */
MINIUPNP_LIBSPEC struct UPNPDev *
upnpDiscoverDevices(const char * const deviceTypes[],
                    int delay, const char * multicastif,
                    const char * minissdpdsock, int sameport,
                    int ipv6,
                    int * error)
{
	struct UPNPDev * tmp;
	struct UPNPDev * devlist = 0;
	unsigned int scope_id = 0;
	int opt = 1;
	static const char MSearchMsgFmt[] =
	"M-SEARCH * HTTP/1.1\r\n"
	"HOST: %s:" XSTR(PORT) "\r\n"
	"ST: %s\r\n"
	"MAN: \"ssdp:discover\"\r\n"
	"MX: %u\r\n"
	"\r\n";
	int deviceIndex;
	char bufr[1536];	/* reception and emission buffer */
	int sudp;
	int n;
	struct sockaddr_storage sockudp_r;
	unsigned int mx;
#ifdef NO_GETADDRINFO
	struct sockaddr_storage sockudp_w;
#else
	int rv;
	struct addrinfo hints, *servinfo, *p;
#endif
#ifdef _WIN32
	MIB_IPFORWARDROW ip_forward;
#endif
	int linklocal = 1;

	if(error)
		*error = UPNPDISCOVER_UNKNOWN_ERROR;
#if !defined(_WIN32) && !defined(__amigaos__) && !defined(__amigaos4__)
	/* first try to get infos from minissdpd ! */
	if(!minissdpdsock)
		minissdpdsock = "/var/run/minissdpd.sock";
	for(deviceIndex = 0; !devlist && deviceTypes[deviceIndex]; deviceIndex++) {
		devlist = getDevicesFromMiniSSDPD(deviceTypes[deviceIndex],
		                                  minissdpdsock);
		/* We return what we have found if it was not only a rootdevice */
		if(devlist && !strstr(deviceTypes[deviceIndex], "rootdevice")) {
			if(error)
				*error = UPNPDISCOVER_SUCCESS;
			return devlist;
		}
	}
#endif
	/* fallback to direct discovery */
#ifdef _WIN32
	sudp = socket(ipv6 ? PF_INET6 : PF_INET, SOCK_DGRAM, IPPROTO_UDP);
#else
	sudp = socket(ipv6 ? PF_INET6 : PF_INET, SOCK_DGRAM, 0);
#endif
	if(sudp < 0)
	{
		if(error)
			*error = UPNPDISCOVER_SOCKET_ERROR;
		PRINT_SOCKET_ERROR("socket");
		return NULL;
	}

	/* reception */
	memset(&sockudp_r, 0, sizeof(struct sockaddr_storage));
	if(ipv6) {
		struct sockaddr_in6 * p = (struct sockaddr_in6 *)&sockudp_r;
		p->sin6_family = AF_INET6;
		if(sameport)
			p->sin6_port = htons(PORT);
		p->sin6_addr = in6addr_any; /* in6addr_any is not available with MinGW32 3.4.2 */
	} else {
		struct sockaddr_in * p = (struct sockaddr_in *)&sockudp_r;
		p->sin_family = AF_INET;
		if(sameport)
			p->sin_port = htons(PORT);
		p->sin_addr.s_addr = INADDR_ANY;
	}
#ifdef _WIN32
/* This code could help us to use the right Network interface for
 * SSDP multicast traffic */
/* Get IP associated with the index given in the ip_forward struct
 * in order to give this ip to setsockopt(sudp, IPPROTO_IP, IP_MULTICAST_IF) */
	if(!ipv6
	   && (GetBestRoute(inet_addr("223.255.255.255"), 0, &ip_forward) == NO_ERROR)) {
		DWORD dwRetVal = 0;
		PMIB_IPADDRTABLE pIPAddrTable;
		DWORD dwSize = 0;
#ifdef DEBUG
		IN_ADDR IPAddr;
#endif
		int i;
#ifdef DEBUG
		printf("ifIndex=%lu nextHop=%lx \n", ip_forward.dwForwardIfIndex, ip_forward.dwForwardNextHop);
#endif
		pIPAddrTable = (MIB_IPADDRTABLE *) malloc(sizeof (MIB_IPADDRTABLE));
		if (GetIpAddrTable(pIPAddrTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER) {
			free(pIPAddrTable);
			pIPAddrTable = (MIB_IPADDRTABLE *) malloc(dwSize);
		}
		if(pIPAddrTable) {
			dwRetVal = GetIpAddrTable( pIPAddrTable, &dwSize, 0 );
#ifdef DEBUG
			printf("\tNum Entries: %ld\n", pIPAddrTable->dwNumEntries);
#endif
			for (i=0; i < (int) pIPAddrTable->dwNumEntries; i++) {
#ifdef DEBUG
				printf("\n\tInterface Index[%d]:\t%ld\n", i, pIPAddrTable->table[i].dwIndex);
				IPAddr.S_un.S_addr = (u_long) pIPAddrTable->table[i].dwAddr;
				printf("\tIP Address[%d]:     \t%s\n", i, inet_ntoa(IPAddr) );
				IPAddr.S_un.S_addr = (u_long) pIPAddrTable->table[i].dwMask;
				printf("\tSubnet Mask[%d]:    \t%s\n", i, inet_ntoa(IPAddr) );
				IPAddr.S_un.S_addr = (u_long) pIPAddrTable->table[i].dwBCastAddr;
				printf("\tBroadCast[%d]:      \t%s (%ld)\n", i, inet_ntoa(IPAddr), pIPAddrTable->table[i].dwBCastAddr);
				printf("\tReassembly size[%d]:\t%ld\n", i, pIPAddrTable->table[i].dwReasmSize);
				printf("\tType and State[%d]:", i);
				printf("\n");
#endif
				if (pIPAddrTable->table[i].dwIndex == ip_forward.dwForwardIfIndex) {
					/* Set the address of this interface to be used */
					struct in_addr mc_if;
					memset(&mc_if, 0, sizeof(mc_if));
					mc_if.s_addr = pIPAddrTable->table[i].dwAddr;
					if(setsockopt(sudp, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&mc_if, sizeof(mc_if)) < 0) {
						PRINT_SOCKET_ERROR("setsockopt");
					}
					((struct sockaddr_in *)&sockudp_r)->sin_addr.s_addr = pIPAddrTable->table[i].dwAddr;
#ifndef DEBUG
					break;
#endif
				}
			}
			free(pIPAddrTable);
			pIPAddrTable = NULL;
		}
	}
#endif

#ifdef _WIN32
	if (setsockopt(sudp, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof (opt)) < 0)
#else
	if (setsockopt(sudp, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt)) < 0)
#endif
	{
		if(error)
			*error = UPNPDISCOVER_SOCKET_ERROR;
		PRINT_SOCKET_ERROR("setsockopt");
		return NULL;
	}

	if(multicastif)
	{
		if(ipv6) {
#if !defined(_WIN32)
			/* according to MSDN, if_nametoindex() is supported since
			 * MS Windows Vista and MS Windows Server 2008.
			 * http://msdn.microsoft.com/en-us/library/bb408409%28v=vs.85%29.aspx */
			unsigned int ifindex = if_nametoindex(multicastif); /* eth0, etc. */
			if(setsockopt(sudp, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex, sizeof(&ifindex)) < 0)
			{
				PRINT_SOCKET_ERROR("setsockopt");
			}
#else
#ifdef DEBUG
			printf("Setting of multicast interface not supported in IPv6 under Windows.\n");
#endif
#endif
		} else {
			struct in_addr mc_if;
			mc_if.s_addr = inet_addr(multicastif); /* ex: 192.168.x.x */
			if(mc_if.s_addr != INADDR_NONE)
			{
				((struct sockaddr_in *)&sockudp_r)->sin_addr.s_addr = mc_if.s_addr;
				if(setsockopt(sudp, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&mc_if, sizeof(mc_if)) < 0)
				{
					PRINT_SOCKET_ERROR("setsockopt");
				}
			} else {
#ifdef HAS_IP_MREQN
				/* was not an ip address, try with an interface name */
				struct ip_mreqn reqn;	/* only defined with -D_BSD_SOURCE or -D_GNU_SOURCE */
				memset(&reqn, 0, sizeof(struct ip_mreqn));
				reqn.imr_ifindex = if_nametoindex(multicastif);
				if(setsockopt(sudp, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&reqn, sizeof(reqn)) < 0)
				{
					PRINT_SOCKET_ERROR("setsockopt");
				}
#else
#ifdef DEBUG
				printf("Setting of multicast interface not supported with interface name.\n");
#endif
#endif
			}
		}
	}

	/* Before sending the packed, we first "bind" in order to be able
	 * to receive the response */
	if (bind(sudp, (const struct sockaddr *)&sockudp_r,
	         ipv6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in)) != 0)
	{
		if(error)
			*error = UPNPDISCOVER_SOCKET_ERROR;
		PRINT_SOCKET_ERROR("bind");
		closesocket(sudp);
		return NULL;
	}

	if(error)
		*error = UPNPDISCOVER_SUCCESS;
	/* Calculating maximum response time in seconds */
	mx = ((unsigned int)delay) / 1000u;
	if(mx == 0) {
		mx = 1;
		delay = 1000;
	}
	/* receiving SSDP response packet */
	for(deviceIndex = 0; deviceTypes[deviceIndex]; deviceIndex++) {
		/* sending the SSDP M-SEARCH packet */
		n = snprintf(bufr, sizeof(bufr),
		             MSearchMsgFmt,
		             ipv6 ?
		             (linklocal ? "[" UPNP_MCAST_LL_ADDR "]" :  "[" UPNP_MCAST_SL_ADDR "]")
		             : UPNP_MCAST_ADDR,
		             deviceTypes[deviceIndex], mx);
#ifdef DEBUG
		/*printf("Sending %s", bufr);*/
		printf("Sending M-SEARCH request to %s with ST: %s\n",
		       ipv6 ?
		       (linklocal ? "[" UPNP_MCAST_LL_ADDR "]" :  "[" UPNP_MCAST_SL_ADDR "]")
		       : UPNP_MCAST_ADDR,
		       deviceTypes[deviceIndex]);
#endif
#ifdef NO_GETADDRINFO
		/* the following code is not using getaddrinfo */
		/* emission */
		memset(&sockudp_w, 0, sizeof(struct sockaddr_storage));
		if(ipv6) {
			struct sockaddr_in6 * p = (struct sockaddr_in6 *)&sockudp_w;
			p->sin6_family = AF_INET6;
			p->sin6_port = htons(PORT);
			inet_pton(AF_INET6,
			          linklocal ? UPNP_MCAST_LL_ADDR : UPNP_MCAST_SL_ADDR,
			          &(p->sin6_addr));
		} else {
			struct sockaddr_in * p = (struct sockaddr_in *)&sockudp_w;
			p->sin_family = AF_INET;
			p->sin_port = htons(PORT);
			p->sin_addr.s_addr = inet_addr(UPNP_MCAST_ADDR);
		}
		n = sendto(sudp, bufr, n, 0, &sockudp_w,
		           ipv6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in));
		if (n < 0) {
			if(error)
				*error = UPNPDISCOVER_SOCKET_ERROR;
			PRINT_SOCKET_ERROR("sendto");
			break;
		}
#else /* #ifdef NO_GETADDRINFO */
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC; /* AF_INET6 or AF_INET */
		hints.ai_socktype = SOCK_DGRAM;
		/*hints.ai_flags = */
		if ((rv = getaddrinfo(ipv6
		                      ? (linklocal ? UPNP_MCAST_LL_ADDR : UPNP_MCAST_SL_ADDR)
		                      : UPNP_MCAST_ADDR,
		                      XSTR(PORT), &hints, &servinfo)) != 0) {
			if(error)
				*error = UPNPDISCOVER_SOCKET_ERROR;
#ifdef _WIN32
			fprintf(stderr, "getaddrinfo() failed: %d\n", rv);
#else
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
#endif
			break;
		}
		for(p = servinfo; p; p = p->ai_next) {
			n = sendto(sudp, bufr, n, 0, p->ai_addr, p->ai_addrlen);
			if (n < 0) {
#ifdef DEBUG
				char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
				if (getnameinfo(p->ai_addr, p->ai_addrlen, hbuf, sizeof(hbuf), sbuf,
				                sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
					fprintf(stderr, "host:%s port:%s\n", hbuf, sbuf);
				}
#endif
				PRINT_SOCKET_ERROR("sendto");
				continue;
			}
		}
		freeaddrinfo(servinfo);
		if(n < 0) {
			if(error)
				*error = UPNPDISCOVER_SOCKET_ERROR;
			break;
		}
#endif /* #ifdef NO_GETADDRINFO */
		/* Waiting for SSDP REPLY packet to M-SEARCH */
		do {
			n = receivedata(sudp, bufr, sizeof(bufr), delay, &scope_id);
			if (n < 0) {
				/* error */
				if(error)
					*error = UPNPDISCOVER_SOCKET_ERROR;
				goto error;
			} else if (n == 0) {
				/* no data or Time Out */
				if (devlist) {
					/* found some devices, stop now*/
					if(error)
						*error = UPNPDISCOVER_SUCCESS;
					goto error;
				}
				if(ipv6) {
					/* switch linklocal flag */
					if(linklocal) {
						linklocal = 0;
						--deviceIndex;
					} else {
						linklocal = 1;
					}
				}
			} else {
				const char * descURL=NULL;
				int urlsize=0;
				const char * st=NULL;
				int stsize=0;
				parseMSEARCHReply(bufr, n, &descURL, &urlsize, &st, &stsize);
				if(st&&descURL) {
#ifdef DEBUG
					printf("M-SEARCH Reply:\n  ST: %.*s\n  Location: %.*s\n",
					       stsize, st, urlsize, descURL);
#endif
					for(tmp=devlist; tmp; tmp = tmp->pNext) {
						if(memcmp(tmp->descURL, descURL, urlsize) == 0 &&
						   tmp->descURL[urlsize] == '\0' &&
						   memcmp(tmp->st, st, stsize) == 0 &&
						   tmp->st[stsize] == '\0')
							break;
					}
					/* at the exit of the loop above, tmp is null if
					 * no duplicate device was found */
					if(tmp)
						continue;
					tmp = (struct UPNPDev *)malloc(sizeof(struct UPNPDev)+urlsize+stsize);
					if(!tmp) {
						/* memory allocation error */
						if(error)
							*error = UPNPDISCOVER_MEMORY_ERROR;
						goto error;
					}
					tmp->pNext = devlist;
					tmp->descURL = tmp->buffer;
					tmp->st = tmp->buffer + 1 + urlsize;
					memcpy(tmp->buffer, descURL, urlsize);
					tmp->buffer[urlsize] = '\0';
					memcpy(tmp->buffer + urlsize + 1, st, stsize);
					tmp->buffer[urlsize+1+stsize] = '\0';
					tmp->scope_id = scope_id;
					devlist = tmp;
				}
			}
		} while(n > 0);
	}
error:
	closesocket(sudp);
	return devlist;
}

/* upnpDiscover() Discover IGD device */
MINIUPNP_LIBSPEC struct UPNPDev *
upnpDiscover(int delay, const char * multicastif,
             const char * minissdpdsock, int sameport,
             int ipv6,
             int * error)
{
	static const char * const deviceList[] = {
#if 0
		"urn:schemas-upnp-org:device:InternetGatewayDevice:2",
		"urn:schemas-upnp-org:service:WANIPConnection:2",
#endif
		"urn:schemas-upnp-org:device:InternetGatewayDevice:1",
		//"urn:schemas-upnp-org:service:WANIPConnection:1",
		//"urn:schemas-upnp-org:service:WANPPPConnection:1",
		"upnp:rootdevice",
		/*"ssdp:all",*/
		0
	};
	return upnpDiscoverDevices(deviceList,
	                           delay, multicastif, minissdpdsock, sameport,
	                           ipv6, error);
}

/* upnpDiscoverAll() Discover all UPnP devices */
MINIUPNP_LIBSPEC struct UPNPDev *
upnpDiscoverAll(int delay, const char * multicastif,
                const char * minissdpdsock, int sameport,
                int ipv6,
                int * error)
{
	static const char * const deviceList[] = {
		/*"upnp:rootdevice",*/
		"ssdp:all",
		0
	};
	return upnpDiscoverDevices(deviceList,
	                           delay, multicastif, minissdpdsock, sameport,
	                           ipv6, error);
}

/* upnpDiscoverDevice() Discover a specific device */
MINIUPNP_LIBSPEC struct UPNPDev *
upnpDiscoverDevice(const char * device, int delay, const char * multicastif,
                const char * minissdpdsock, int sameport,
                int ipv6,
                int * error)
{
	const char * const deviceList[] = {
		device,
		0
	};
	return upnpDiscoverDevices(deviceList,
	                           delay, multicastif, minissdpdsock, sameport,
	                           ipv6, error);
}

/* freeUPNPDevlist() should be used to
 * free the chained list returned by upnpDiscover() */
MINIUPNP_LIBSPEC void freeUPNPDevlist(struct UPNPDev * devlist)
{
	struct UPNPDev * next;
	while(devlist)
	{
		next = devlist->pNext;
		free(devlist);
		devlist = next;
	}
}

static char *
build_absolute_url(const char * baseurl, const char * descURL,
                   const char * url, unsigned int scope_id)
{
	int l, n;
	char * s;
	const char * base;
	char * p;
#if defined(IF_NAMESIZE) && !defined(_WIN32)
	char ifname[IF_NAMESIZE];
#else /* defined(IF_NAMESIZE) && !defined(_WIN32) */
	char scope_str[8];
#endif	/* defined(IF_NAMESIZE) && !defined(_WIN32) */

	if(  (url[0] == 'h')
	   &&(url[1] == 't')
	   &&(url[2] == 't')
	   &&(url[3] == 'p')
	   &&(url[4] == ':')
	   &&(url[5] == '/')
	   &&(url[6] == '/'))
		return strdup(url);
	base = (baseurl[0] == '\0') ? descURL : baseurl;
	n = strlen(base);
	if(n > 7) {
		p = strchr(base + 7, '/');
		if(p)
			n = p - base;
	}
	l = n + strlen(url) + 1;
	if(url[0] != '/')
		l++;
	if(scope_id != 0) {
#if defined(IF_NAMESIZE) && !defined(_WIN32)
		if(if_indextoname(scope_id, ifname)) {
			l += 3 + strlen(ifname);	/* 3 == strlen(%25) */
		}
#else /* defined(IF_NAMESIZE) && !defined(_WIN32) */
		/* under windows, scope is numerical */
		l += 3 + snprintf(scope_str, sizeof(scope_str), "%u", scope_id);
#endif /* defined(IF_NAMESIZE) && !defined(_WIN32) */
	}
	s = malloc(l);
	if(s == NULL) return NULL;
	memcpy(s, base, n);
	if(scope_id != 0) {
		s[n] = '\0';
		if(0 == memcmp(s, "http://[fe80:", 13)) {
			/* this is a linklocal IPv6 address */
			p = strchr(s, ']');
			if(p) {
				/* insert %25<scope> into URL */
#ifdef IF_NAMESIZE
				memmove(p + 3 + strlen(ifname), p, strlen(p) + 1);
				memcpy(p, "%25", 3);
				memcpy(p + 3, ifname, strlen(ifname));
				n += 3 + strlen(ifname);
#else
				memmove(p + 3 + strlen(scope_str), p, strlen(p) + 1);
				memcpy(p, "%25", 3);
				memcpy(p + 3, scope_str, strlen(scope_str));
				n += 3 + strlen(scope_str);
#endif
			}
		}
	}
	if(url[0] != '/')
		s[n++] = '/';
	memcpy(s + n, url, l - n);
	return s;
}

/* Prepare the Urls for usage...
 */
MINIUPNP_LIBSPEC void
GetUPNPUrls(struct UPNPUrls * urls, struct IGDdatas * data,
            const char * descURL, unsigned int scope_id)
{
	/* strdup descURL */
	urls->rootdescURL = strdup(descURL);

	/* get description of WANIPConnection */
	urls->ipcondescURL = build_absolute_url(data->urlbase, descURL,
	                                        data->first.scpdurl, scope_id);
	urls->controlURL = build_absolute_url(data->urlbase, descURL,
	                                      data->first.controlurl, scope_id);
	urls->controlURL_CIF = build_absolute_url(data->urlbase, descURL,
	                                          data->CIF.controlurl, scope_id);
	urls->controlURL_6FC = build_absolute_url(data->urlbase, descURL,
	                                          data->IPv6FC.controlurl, scope_id);

#ifdef DEBUG
	printf("urls->ipcondescURL='%s'\n", urls->ipcondescURL);
	printf("urls->controlURL='%s'\n", urls->controlURL);
	printf("urls->controlURL_CIF='%s'\n", urls->controlURL_CIF);
	printf("urls->controlURL_6FC='%s'\n", urls->controlURL_6FC);
#endif
}

MINIUPNP_LIBSPEC void
FreeUPNPUrls(struct UPNPUrls * urls)
{
	if(!urls)
		return;
	free(urls->controlURL);
	urls->controlURL = 0;
	free(urls->ipcondescURL);
	urls->ipcondescURL = 0;
	free(urls->controlURL_CIF);
	urls->controlURL_CIF = 0;
	free(urls->controlURL_6FC);
	urls->controlURL_6FC = 0;
	free(urls->rootdescURL);
	urls->rootdescURL = 0;
}

int
UPNPIGD_IsConnected(struct UPNPUrls * urls, struct IGDdatas * data)
{
	char status[64];
	unsigned int uptime;
	status[0] = '\0';
	UPNP_GetStatusInfo(urls->controlURL, data->first.servicetype,
	                   status, &uptime, NULL);
	if(0 == strcmp("Connected", status))
	{
		return 1;
	}
	else
		return 0;
}

void parsedescxml(char *msg, char *friendly_name, char *icon_url)
{
        char line[200], *body, *p, *mxend;
        char tmp[200];
        int i, j;
        int s_num = 0;
        int type = 0;
	char *head, *end;
        char *opts[] = {"<friendlyName>","<manufacturer>", "<presentationURL>",
                        "<modelDescription>", "<modelName>", "<modelNumber>",
                        "<serviceType>", "<SCPDURL>", "<icon>"
                        };

	char *icon_desc_end, *url_ptr;
	int get_icon_url = 0;

        /* pointer to the head of msg */
        p = strstr(msg, "<?xml");
        /* pointer to the end of msg */
        body = strstr(msg, "</root>");

        while( p!= NULL && p < body)
        {
                /* get rid of 'TAB' or 'Space' in the start of a line. */
                while((*p == '\r' || *p == '\n' || *p == '\t' || *p == ' ') && p < body)
                        p ++;

                /* get a line. */
                i = 0;
                while(*p != '>' && p < body)
                {
                    if(i<199)
                        line[i++] = *p++;
                    else
                        *p++;
                }
                if(p == body)
                {
                        break;
                }

                line[i++] = *p++;
                line[i] ='\0';

                if(p == body)
                        break;

                /* judge whether this line is useful. */
                for(type = 0; type< sizeof(opts)/sizeof(*opts); type++)
                        if(strncmp(line, opts[type], strlen(opts[type])) == 0)
                                break;

                if(type == sizeof(opts)/sizeof(*opts))
                        continue;


                /* get the information.
                 * eg. <manufacturer> information </manufacturer>
		 */

	if((type == 8) && (!get_icon_url)) {
		icon_desc_end = strstr(p, "</icon>");

		i = 0;
		while(p < icon_desc_end) {
	        	if(i<199)
        	                line[i++] = *p++;
                    	else
                        	*p++;
		}
                line[i++] = *p++;
                line[i] ='\0';
		p = icon_desc_end + 7;
	}
	else {

                i = 0;
                while(*p != '>' && p < body)
                {
                    if(i<199)
                        line[i++] = *p++;
                    else
                        *p++;
                }
                line[i++] = *p++;
                line[i] ='\0';
                for(j =0; line[j] != '<'; j++)
                        tmp[j] = line[j];
                tmp[j] = '\0';
 } 

                switch(type)
                {
                case 0:
#ifdef DEBUG
			printf( "friendlyname = %s\n", tmp);
#endif
			if(strstr(tmp,"WPS Router") || strstr(tmp,"Device"))
				break;

			if( (end = strchr(tmp, '(')) != NULL )
                        {
                                *end = '\0';
                                strcpy(friendly_name, tmp);
                        }
                        else if( (end = strchr(tmp, ':')) != NULL )
			{
				*end = '\0';
				strcpy(friendly_name, tmp);
			}
			else if( (head = strchr(tmp,'['))!=NULL && (end = strchr(tmp,']'))!=NULL )
			{
				if( head < end )
				{
					*end = '\0';
					strcpy(friendly_name, head+1);
				}
			}
			else
			{
				if(tmp!=NULL)
					strcpy(friendly_name, tmp);
			}
                        break;
#ifdef DEBUG
                case 1:
                        printf("manufacturer = %s\n", tmp);
                        break;
                case 2:
                        printf("presentation = %s\n", tmp);
                        break;
                case 3:
                        printf("description = %s\n", tmp);

                        break;
                case 4:
                        printf("modelname = %s\n", tmp);
                        break;
                case 5:
                        printf("modelnumber = %s\n", tmp);
                        break;
#endif
                case 6: /* tmp="urn:schemas-upnp-org:service:serviceType:v" */
                        mxend = tmp;
                        i = 0; j = 0;
                        while(i != 4)
                        {
                                if(i == 3)
                                        tmp[j++] = *mxend;
                                if(*mxend == ':')
                                        i++;
                                mxend++;
                        }
                        tmp[j-1] = '\0';
#ifdef DEBUG
                        printf("service name = %s\n", tmp);
#endif
                        break;
                case 7:
#ifdef DEBUG
                        printf("service url = %s\n", tmp);
#endif
                        break;
                case 8:
	                if(strstr(line, "jpeg")||strstr(line, "png")) {
        	                if( url_ptr = strstr(line,"<url>")) {
                	                url_ptr = url_ptr + 5;
                        	        i = 0;
                                	while(*url_ptr != '<')
                                        	tmp[i++] = *url_ptr++;
                                	tmp[i] = '\0';
					strcpy(icon_url, tmp);
#ifdef DEBUG
					printf("icon url =%s\n", tmp);
#endif
					get_icon_url = 1;
				}
                        }
                        break;
                }
        }

}

char *
get_lan_ipaddr()
{
        int s;
        struct ifreq ifr;
        struct sockaddr_in *inaddr;
        struct in_addr ip_addr;

        /* Retrieve IP info */
        if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
#if 0
                return strdup("0.0.0.0");
#else
        {
                memset(&ip_addr, 0x0, sizeof(ip_addr));
                return inet_ntoa(ip_addr);
        }
#endif

        strncpy(ifr.ifr_name, "br0", IFNAMSIZ);
        inaddr = (struct sockaddr_in *)&ifr.ifr_addr;
        inet_aton("0.0.0.0", &inaddr->sin_addr);

        /* Get IP address */
        ioctl(s, SIOCGIFADDR, &ifr);
        close(s);

        ip_addr = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr;
        return inet_ntoa(ip_addr);
}

/* UPNP_GetValidIGDe) :
 * return values :
 *    -1 = Internal error
 *     0 = NO IGD found
 *     1 = A valid connected IGD has been found
 *     2 = A valid IGD has been found but it reported as
 *         not connected
 *     3 = an UPnP device has been found but was not recognized as an IGD
 *
 * In any positive non zero return case, the urls and data structures
 * passed as parameters are set. Donc forget to call FreeUPNPUrls(urls) to
 * free allocated memory.
 */
MINIUPNP_LIBSPEC int
UPNP_GetValidIGD(struct UPNPDev * devlist,
                 struct UPNPUrls * urls,
				 struct IGDdatas * data,
				 char * lanaddr, int lanaddrlen)
{
	struct xml_desc {
		char * xml;
		int size;
		int is_igd;
	} * desc = NULL;
	struct UPNPDev * dev;
	int ndev = 0;
	int i;
	int state = -1; /* state 1 : IGD connected. State 2 : IGD. State 3 : anything */
	int n_igd = 0;
	char extIpAddr[16];
	struct UPNPDevInfo IGDInfo;
	char *dut_lanaddr;
	char IGDDescURL[256];

	if(!devlist)
	{
#ifdef DEBUG
		printf("Empty devlist\n");
#endif
		return 0;
	}
	/* counting total number of devices in the list */
	for(dev = devlist; dev; dev = dev->pNext)
		ndev++;
	if(ndev > 0)
	{
		desc = calloc(ndev, sizeof(struct xml_desc));
		if(!desc)
			return -1; /* memory allocation error */
	}

	dut_lanaddr = get_lan_ipaddr();
	
	/* Step 1 : downloading descriptions and testing type */
	for(dev = devlist, i = 0; dev; dev = dev->pNext, i++)
	{
		memset(&IGDInfo, 0, sizeof(struct UPNPDevInfo));
		/* we should choose an internet gateway device.
		 * with st == urn:schemas-upnp-org:device:InternetGatewayDevice:1 */
		strcpy(IGDDescURL, dev->descURL);
		desc[i].xml = miniwget_getaddr(dev->descURL, &(desc[i].size),
		                               lanaddr, lanaddrlen,
		                               dev->scope_id, &IGDInfo, dut_lanaddr);

#ifdef DEBUG
		if(!desc[i].xml)
		{
			printf("error getting XML description %s\n", dev->descURL);
		}
#endif
		if(desc[i].xml)
		{
			memset(data, 0, sizeof(struct IGDdatas));
			memset(urls, 0, sizeof(struct UPNPUrls));
			/* parserootdesc(desc[i].xml, desc[i].size, data); */

			FILE *xml_fd;
			xml_fd = fopen("/tmp/upnpc_xml.log", "a");
			fprintf(xml_fd, "============= XML ==============\n");
			fprintf(xml_fd, "%s\n", desc[i].xml);
			parsedescxml(desc[i].xml, &IGDInfo.friendlyName, &IGDInfo.iconUrl);
	                fprintf(xml_fd, "    HostName: %s\n", IGDInfo.hostname);
        	        fprintf(xml_fd, "    type:     %s\n", IGDInfo.type);
                	fprintf(xml_fd, "    F Name:   %s\n", IGDInfo.friendlyName);
			fprintf(xml_fd, "    Icon URL: %s\n", IGDInfo.iconUrl);
			fprintf(xml_fd, "================================\n\n");
			//syslog(LOG_NOTICE, "parse icon url: %s", IGDInfo.iconUrl);

			strcpy(dev->DevInfo.hostname, IGDInfo.hostname);
			strcpy(dev->DevInfo.type, IGDInfo.type);
			strcpy(dev->DevInfo.friendlyName, IGDInfo.friendlyName);
			strcpy(dev->DevInfo.iconUrl, IGDInfo.iconUrl);

#ifdef RTCONFIG_JFFS2USERICON
			if(strcmp(IGDInfo.iconUrl, "") != NULL) {
				char realIconUrl[158], iconFile[32], iconBuf[512];
				char *ico_head, *ico_end;
				char *icon, *p, *q;
				int  iconSize, i = 0, iconCheck = 0;
				FILE *ico_fd;

				memset(realIconUrl, 0, 158);
				if(strstr(IGDInfo.iconUrl, "http://")) {
					strcpy(realIconUrl, IGDInfo.iconUrl);
				}
				else {
				    q = IGDDescURL;
				    while(q = strchr(q, '/')) {
					i++;
					if(i == 3) {
						p = IGDDescURL;
						i = 0;
						while ( p < q ) {
							realIconUrl[i++] = *p++;
						}
						strcat(realIconUrl, IGDInfo.iconUrl);
					#ifdef DEBUG
						printf("\n*** Real URL=%s=\n\n", realIconUrl);
					#endif
						break;
					}
					q++;
				    }
				}
			get_icon:
                                //syslog(LOG_NOTICE, "Real icon url: %s", realIconUrl);
                                fprintf(xml_fd, "Real icon url: %s\n", realIconUrl);
                                sprintf(iconFile, "/tmp/upnpicon/%s.ico", IGDInfo.hostname);

				icon = miniwget_getaddr(realIconUrl, &iconSize,
                                               lanaddr, lanaddrlen,
                                               0, &IGDInfo, dut_lanaddr);

				if( iconSize > 512 ) {
					ico_fd = fopen(iconFile, "w");
					if(ico_fd) {
					fwrite(icon, sizeof(char), iconSize, ico_fd);
					fclose(ico_fd);
					}
				}
				else if(iconSize > 0) {
					ico_head = strstr(icon, "<a href=");
					if(ico_head) {
						ico_head = ico_head+9;
						ico_end = strstr(icon, "\">");
						if(ico_end) {
							*ico_end = '\0';
							strcpy(realIconUrl, ico_head);
							goto get_icon;
						}
					}
                                }
			}
#endif

			if(COMPARE(data->CIF.servicetype,
			           "urn:schemas-upnp-org:service:WANCommonInterfaceConfig:"))
			{
				desc[i].is_igd = 1;
				n_igd++;
			}
			fclose(xml_fd);
		}
		else
			memset(dev->DevInfo.hostname, 0, 65);
	}

	/* iterate the list to find a device depending on state */
	for(state = 1; state <= 3; state++)
	/*if(0)*/
	{
		for(dev = devlist, i = 0; dev; dev = dev->pNext, i++)
		{
			if(desc[i].xml)
			{
				memset(data, 0, sizeof(struct IGDdatas));
				memset(urls, 0, sizeof(struct UPNPUrls));
				parserootdesc(desc[i].xml, desc[i].size, data);
				if(desc[i].is_igd || state >= 3 )
				{
				  GetUPNPUrls(urls, data, dev->descURL, dev->scope_id);

				  /* in state 2 and 3 we dont test if device is connected ! */
				  if(state >= 2)
				    goto free_and_return;
#ifdef DEBUG
				  printf("UPNPIGD_IsConnected(%s) = %d\n",
				     urls->controlURL,
			         UPNPIGD_IsConnected(urls, data));
#endif
				  /* checks that status is connected AND there is a external IP address assigned */
				  if(UPNPIGD_IsConnected(urls, data)
				     && (UPNP_GetExternalIPAddress(urls->controlURL,  data->first.servicetype, extIpAddr) == 0))
					goto free_and_return;
				  FreeUPNPUrls(urls);
				  if(data->second.servicetype[0] != '\0') {
#ifdef DEBUG
				    printf("We tried %s, now we try %s !\n",
				           data->first.servicetype, data->second.servicetype);
#endif
				    /* swaping WANPPPConnection and WANIPConnection ! */
				    memcpy(&data->tmp, &data->first, sizeof(struct IGDdatas_service));
				    memcpy(&data->first, &data->second, sizeof(struct IGDdatas_service));
				    memcpy(&data->second, &data->tmp, sizeof(struct IGDdatas_service));
				    GetUPNPUrls(urls, data, dev->descURL, dev->scope_id);
#ifdef DEBUG
				    printf("UPNPIGD_IsConnected(%s) = %d\n",
				       urls->controlURL,
			           UPNPIGD_IsConnected(urls, data));
#endif
				    if(UPNPIGD_IsConnected(urls, data)
				       && (UPNP_GetExternalIPAddress(urls->controlURL,  data->first.servicetype, extIpAddr) == 0))
					  goto free_and_return;
				    FreeUPNPUrls(urls);
				  }
				}
				memset(data, 0, sizeof(struct IGDdatas));
			}
		}
	}
	state = 0;
free_and_return:
	if(desc) {
		for(i = 0; i < ndev; i++) {
			if(desc[i].xml) {
				free(desc[i].xml);
			}
		}
		free(desc);
	}
	return state;
}

/* UPNP_GetIGDFromUrl()
 * Used when skipping the discovery process.
 * return value :
 *   0 - Not ok
 *   1 - OK */
int
UPNP_GetIGDFromUrl(const char * rootdescurl,
                   struct UPNPUrls * urls,
                   struct IGDdatas * data,
                   char * lanaddr, int lanaddrlen)
{
	char * descXML;
	int descXMLsize = 0;

	descXML = miniwget_getaddr(rootdescurl, &descXMLsize,
	   	                       lanaddr, lanaddrlen, 0, NULL, NULL);
	if(descXML) {
		memset(data, 0, sizeof(struct IGDdatas));
		memset(urls, 0, sizeof(struct UPNPUrls));
		parserootdesc(descXML, descXMLsize, data);
		free(descXML);
		descXML = NULL;
		GetUPNPUrls(urls, data, rootdescurl, 0);
		return 1;
	} else {
		return 0;
	}
}

