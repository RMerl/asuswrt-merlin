/* $Id: minissdp.c,v 1.77 2015/08/26 07:36:52 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2015 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <syslog.h>

#include "config.h"
#include "upnpdescstrings.h"
#include "miniupnpdpath.h"
#include "upnphttp.h"
#include "upnpglobalvars.h"
#include "minissdp.h"
#include "upnputils.h"
#include "getroute.h"
#include "asyncsendto.h"
#include "codelength.h"
#include "macros.h"

/* SSDP ip/port */
#define SSDP_PORT (1900)
#define SSDP_MCAST_ADDR ("239.255.255.250")
#define LL_SSDP_MCAST_ADDR "FF02::C"
#define SL_SSDP_MCAST_ADDR "FF05::C"
#define GL_SSDP_MCAST_ADDR "FF0E::C"

/* AddMulticastMembership()
 * param s			socket
 * param lan_addr	lan address
 */
static int
AddMulticastMembership(int s, struct lan_addr_s * lan_addr)
{
#ifndef HAVE_IP_MREQN
	/* The ip_mreqn structure appeared in Linux 2.4. */
	struct ip_mreq imr;	/* Ip multicast membership */
#else	/* HAVE_IP_MREQN */
	struct ip_mreqn imr;	/* Ip multicast membership */
#endif	/* HAVE_IP_MREQN */

    /* setting up imr structure */
    imr.imr_multiaddr.s_addr = inet_addr(SSDP_MCAST_ADDR);
    /*imr.imr_interface.s_addr = htonl(INADDR_ANY);*/
#ifndef HAVE_IP_MREQN
	imr.imr_interface.s_addr = lan_addr->addr.s_addr;
#else	/* HAVE_IP_MREQN */
    imr.imr_address.s_addr = lan_addr->addr.s_addr;
#ifndef MULTIPLE_EXTERNAL_IP
#ifdef ENABLE_IPV6
	imr.imr_ifindex = lan_addr->index;
#else	/* ENABLE_IPV6 */
    imr.imr_ifindex = if_nametoindex(lan_addr->ifname);
#endif	/* ENABLE_IPV6 */
#else	/* MULTIPLE_EXTERNAL_IP */
    imr.imr_ifindex = 0;
#endif	/* MULTIPLE_EXTERNAL_IP */
#endif	/* HAVE_IP_MREQN */

#ifndef HAVE_IP_MREQN
	if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&imr, sizeof(struct ip_mreq)) < 0)
#else	/* HAVE_IP_MREQN */
	if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&imr, sizeof(struct ip_mreqn)) < 0)
#endif	/* HAVE_IP_MREQN */
	{
        syslog(LOG_ERR, "setsockopt(udp, IP_ADD_MEMBERSHIP): %m");
		return -1;
    }

	return 0;
}

/* AddMulticastMembershipIPv6()
 * param s	socket (IPv6)
 * param ifindex : interface index (0 : All interfaces) */
#ifdef ENABLE_IPV6
static int
AddMulticastMembershipIPv6(int s, unsigned int ifindex)
{
	struct ipv6_mreq mr;

	memset(&mr, 0, sizeof(mr));
	mr.ipv6mr_interface = ifindex;	/* 0 : all interfaces */
#ifndef IPV6_ADD_MEMBERSHIP
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#endif
	inet_pton(AF_INET6, LL_SSDP_MCAST_ADDR, &mr.ipv6mr_multiaddr);
	if(setsockopt(s, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mr, sizeof(struct ipv6_mreq)) < 0)
	{
		syslog(LOG_ERR, "setsockopt(udp, IPV6_ADD_MEMBERSHIP): %m");
		return -1;
	}
	inet_pton(AF_INET6, SL_SSDP_MCAST_ADDR, &mr.ipv6mr_multiaddr);
	if(setsockopt(s, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mr, sizeof(struct ipv6_mreq)) < 0)
	{
		syslog(LOG_ERR, "setsockopt(udp, IPV6_ADD_MEMBERSHIP): %m");
		return -1;
	}
	inet_pton(AF_INET6, GL_SSDP_MCAST_ADDR, &mr.ipv6mr_multiaddr);
	if(setsockopt(s, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mr, sizeof(struct ipv6_mreq)) < 0)
	{
		syslog(LOG_ERR, "setsockopt(udp, IPV6_ADD_MEMBERSHIP): %m");
		return -1;
	}
	return 0;
}
#endif

/* Open and configure the socket listening for
 * SSDP udp packets sent on 239.255.255.250 port 1900
 * SSDP v6 udp packets sent on FF02::C, or FF05::C, port 1900 */
int
OpenAndConfSSDPReceiveSocket(int ipv6)
{
	int s;
	struct sockaddr_storage sockname;
	socklen_t sockname_len;
	struct lan_addr_s * lan_addr;
	int j = 1;

	if( (s = socket(ipv6 ? PF_INET6 : PF_INET, SOCK_DGRAM, 0)) < 0)
	{
		syslog(LOG_ERR, "%s: socket(udp): %m",
		       "OpenAndConfSSDPReceiveSocket");
		return -1;
	}

	memset(&sockname, 0, sizeof(struct sockaddr_storage));
#ifdef ENABLE_IPV6
	if(ipv6)
	{
		struct sockaddr_in6 * saddr = (struct sockaddr_in6 *)&sockname;
		saddr->sin6_family = AF_INET6;
		saddr->sin6_port = htons(SSDP_PORT);
		saddr->sin6_addr = ipv6_bind_addr;
		sockname_len = sizeof(struct sockaddr_in6);
	}
	else
#endif /* ENABLE_IPV6 */
	{
		struct sockaddr_in * saddr = (struct sockaddr_in *)&sockname;
		saddr->sin_family = AF_INET;
		saddr->sin_port = htons(SSDP_PORT);
		/* NOTE : it seems it doesnt work when binding on the specific address */
		/*saddr->sin_addr.s_addr = inet_addr(UPNP_MCAST_ADDR);*/
		saddr->sin_addr.s_addr = htonl(INADDR_ANY);
		/*saddr->sin_addr.s_addr = inet_addr(ifaddr);*/
		sockname_len = sizeof(struct sockaddr_in);
	}

	if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &j, sizeof(j)) < 0)
	{
		syslog(LOG_WARNING, "setsockopt(udp, SO_REUSEADDR): %m");
	}

	if(!set_non_blocking(s))
	{
		syslog(LOG_WARNING, "%s: set_non_blocking(): %m",
		       "OpenAndConfSSDPReceiveSocket");
	}

#if defined(SO_BINDTODEVICE) && !defined(MULTIPLE_EXTERNAL_IP)
	/* One and only one LAN interface */
	if(lan_addrs.lh_first != NULL && lan_addrs.lh_first->list.le_next == NULL
	   && strlen(lan_addrs.lh_first->ifname) > 0)
	{
		if(setsockopt(s, SOL_SOCKET, SO_BINDTODEVICE,
		              lan_addrs.lh_first->ifname,
		              strlen(lan_addrs.lh_first->ifname)) < 0)
		    syslog(LOG_WARNING, "%s: setsockopt(udp%s, SO_BINDTODEVICE, %s): %m",
			       "OpenAndConfSSDPReceiveSocket", ipv6 ? "6" : "",
			       lan_addrs.lh_first->ifname);
	}
#endif /* defined(SO_BINDTODEVICE) && !defined(MULTIPLE_EXTERNAL_IP) */

	if(bind(s, (struct sockaddr *)&sockname, sockname_len) < 0)
	{
		syslog(LOG_ERR, "%s: bind(udp%s): %m",
		       "OpenAndConfSSDPReceiveSocket", ipv6 ? "6" : "");
		close(s);
		return -1;
	}

#ifdef ENABLE_IPV6
	if(ipv6)
	{
		for(lan_addr = lan_addrs.lh_first; lan_addr != NULL; lan_addr = lan_addr->list.le_next)
		{
			if(AddMulticastMembershipIPv6(s, lan_addr->index) < 0)
			{
				syslog(LOG_WARNING,
				       "Failed to add IPv6 multicast membership for interface %s",
				       lan_addr->str ? lan_addr->str : "NULL");
			}
		}
	}
	else
#endif
	{
		for(lan_addr = lan_addrs.lh_first; lan_addr != NULL; lan_addr = lan_addr->list.le_next)
		{
			if(AddMulticastMembership(s, lan_addr) < 0)
			{
				syslog(LOG_WARNING,
				       "Failed to add multicast membership for interface %s",
				       strlen(lan_addr->str) ? lan_addr->str : "NULL");
			}
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
	unsigned char ttl = 2; /* UDA v1.1 says :
		The TTL for the IP packet SHOULD default to 2 and
		SHOULD be configurable. */
	/* TODO: Make TTL be configurable */
	struct in_addr mc_if;
	struct sockaddr_in sockname;

	if( (s = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
	{
		syslog(LOG_ERR, "socket(udp_notify): %m");
		return -1;
	}

	mc_if.s_addr = addr;	/*inet_addr(addr);*/

	if(setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopchar, sizeof(loopchar)) < 0)
	{
		syslog(LOG_ERR, "setsockopt(udp_notify, IP_MULTICAST_LOOP): %m");
		close(s);
		return -1;
	}

	if(setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, (char *)&mc_if, sizeof(mc_if)) < 0)
	{
		syslog(LOG_ERR, "setsockopt(udp_notify, IP_MULTICAST_IF): %m");
		close(s);
		return -1;
	}

	if(setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
	{
		syslog(LOG_WARNING, "setsockopt(udp_notify, IP_MULTICAST_TTL,): %m");
	}

	if(setsockopt(s, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast)) < 0)
	{
		syslog(LOG_ERR, "setsockopt(udp_notify, SO_BROADCAST): %m");
		close(s);
		return -1;
	}

	/* bind() socket before using sendto() is not mandatory
	 * (sendto() will implicitly bind the socket when called on
	 * an unbound socket)
	 * here it is used to se a specific sending address */
	memset(&sockname, 0, sizeof(struct sockaddr_in));
    sockname.sin_family = AF_INET;
    sockname.sin_addr.s_addr = addr;	/*inet_addr(addr);*/

    if (bind(s, (struct sockaddr *)&sockname, sizeof(struct sockaddr_in)) < 0)
	{
		syslog(LOG_ERR, "bind(udp_notify): %m");
		close(s);
		return -1;
    }

	return s;
}

#ifdef ENABLE_IPV6
/* open the UDP socket used to send SSDP notifications to
 * the multicast group reserved for them. IPv6 */
static int
OpenAndConfSSDPNotifySocketIPv6(unsigned int if_index)
{
	int s;
	unsigned int loop = 0;
	struct sockaddr_in6 sockname;

	s = socket(PF_INET6, SOCK_DGRAM, 0);
	if(s < 0)
	{
		syslog(LOG_ERR, "socket(udp_notify IPv6): %m");
		return -1;
	}
	if(setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_IF, &if_index, sizeof(if_index)) < 0)
	{
		syslog(LOG_ERR, "setsockopt(udp_notify IPv6, IPV6_MULTICAST_IF, %u): %m", if_index);
		close(s);
		return -1;
	}
	if(setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, sizeof(loop)) < 0)
	{
		syslog(LOG_ERR, "setsockopt(udp_notify, IPV6_MULTICAST_LOOP): %m");
		close(s);
		return -1;
	}

	/* bind() socket before using sendto() is not mandatory
	 * (sendto() will implicitly bind the socket when called on
	 * an unbound socket)
	 * but explicit bind permits to set port/scope_id/etc. */
	memset(&sockname, 0, sizeof(sockname));
	sockname.sin6_family = AF_INET6;
	sockname.sin6_addr = in6addr_any;
	/*sockname.sin6_port = htons(port);*/
	/*sockname.sin6_scope_id = if_index;*/
	if(bind(s, (struct sockaddr *)&sockname, sizeof(sockname)) < 0)
	{
		syslog(LOG_ERR, "bind(udp_notify IPv6): %m");
		close(s);
		return -1;
	}

	return s;
}
#endif

int
OpenAndConfSSDPNotifySockets(int * sockets)
/*OpenAndConfSSDPNotifySockets(int * sockets,
                             struct lan_addr_s * lan_addr, int n_lan_addr)*/
{
	int i;
	struct lan_addr_s * lan_addr;

	for(i=0, lan_addr = lan_addrs.lh_first;
	    lan_addr != NULL;
	    lan_addr = lan_addr->list.le_next)
	{
		sockets[i] = OpenAndConfSSDPNotifySocket(lan_addr->addr.s_addr);
		if(sockets[i] < 0)
			goto error;
		i++;
#ifdef ENABLE_IPV6
		if(GETFLAG(IPV6DISABLEDMASK))
		{
			sockets[i] = -1;
		}
		else
		{
			sockets[i] = OpenAndConfSSDPNotifySocketIPv6(lan_addr->index);
			if(sockets[i] < 0)
				goto error;
		}
		i++;
#endif
	}
	return 0;
error:
	while(--i >= 0)
	{
		close(sockets[i]);
		sockets[i] = -1;
	}
	return -1;
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

/* Responds to a SSDP "M-SEARCH"
 * s :          socket to use
 * addr :       peer
 * st, st_len : ST: header
 * suffix :     suffix for USN: header
 * host, port : our HTTP host, port
 * delay :      in milli-seconds
 */
static void
SendSSDPResponse(int s, const struct sockaddr * addr,
                 const char * st, int st_len, const char * suffix,
                 const char * host, unsigned short http_port,
#ifdef ENABLE_HTTPS
                 unsigned short https_port,
#endif
                 const char * uuidvalue, unsigned int delay)
{
	int l, n;
	char buf[SSDP_PACKET_MAX_LEN];
	char addr_str[64];
	socklen_t addrlen;
	int st_is_uuid;
#ifdef ENABLE_HTTP_DATE
	char http_date[64];
	time_t t;
	struct tm tm;

	time(&t);
	gmtime_r(&t, &tm);
	strftime(http_date, sizeof(http_date),
		    "%a, %d %b %Y %H:%M:%S GMT", &tm);
#endif

	st_is_uuid = (st_len == (int)strlen(uuidvalue)) &&
	              (memcmp(uuidvalue, st, st_len) == 0);
	/*
	 * follow guideline from document "UPnP Device Architecture 1.0"
	 * uppercase is recommended.
	 * DATE: is recommended
	 * SERVER: OS/ver UPnP/1.0 miniupnpd/1.0
	 * - check what to put in the 'Cache-Control' header
	 *
	 * have a look at the document "UPnP Device Architecture v1.1 */
	l = snprintf(buf, sizeof(buf), "HTTP/1.1 200 OK\r\n"
		"CACHE-CONTROL: max-age=120\r\n"
#ifdef ENABLE_HTTP_DATE
		"DATE: %s\r\n"
#endif
		"ST: %.*s%s\r\n"
		"USN: %s%s%.*s%s\r\n"
		"EXT:\r\n"
		"SERVER: " MINIUPNPD_SERVER_STRING "\r\n"
		"LOCATION: http://%s:%u" ROOTDESC_PATH "\r\n"
#ifdef ENABLE_HTTPS
		"SECURELOCATION.UPNP.ORG: https://%s:%u" ROOTDESC_PATH "\r\n"
#endif
		"OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n" /* UDA v1.1 */
		"01-NLS: %u\r\n" /* same as BOOTID. UDA v1.1 */
		"BOOTID.UPNP.ORG: %u\r\n" /* UDA v1.1 */
		"CONFIGID.UPNP.ORG: %u\r\n" /* UDA v1.1 */
		"\r\n",
#ifdef ENABLE_HTTP_DATE
		http_date,
#endif
		st_len, st, suffix,
		uuidvalue, st_is_uuid ? "" : "::",
		st_is_uuid ? 0 : st_len, st, suffix,
		host, (unsigned int)http_port,
#ifdef ENABLE_HTTPS
		host, (unsigned int)https_port,
#endif
		upnp_bootid, upnp_bootid, upnp_configid);
	if(l<0)
	{
		syslog(LOG_ERR, "%s: snprintf failed %m",
		       "SendSSDPResponse()");
		return;
	}
	else if((unsigned)l>=sizeof(buf))
	{
		syslog(LOG_WARNING, "%s: truncated output (%u>=%u)",
		       "SendSSDPResponse()", (unsigned)l, (unsigned)sizeof(buf));
		l = sizeof(buf) - 1;
	}
	addrlen = (addr->sa_family == AF_INET6)
	          ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
	n = sendto_schedule(s, buf, l, 0,
	                    addr, addrlen, delay);
	sockaddr_to_string(addr, addr_str, sizeof(addr_str));
	syslog(LOG_DEBUG, "%s: %d bytes to %s ST: %.*s",
	       "SendSSDPResponse()",
	       n, addr_str, l, buf);
	if(n < 0)
	{
		syslog(LOG_ERR, "%s: sendto(udp): %m",
		       "SendSSDPResponse()");
	}
}

#ifndef IGD_V2
#define IGD_VER 1
#define WANIPC_VER 1
#else
#define IGD_VER 2
#define WANIPC_VER 2
#endif

static struct {
	const char * s;
	const int version;
	const char * uuid;
} const known_service_types[] =
{
	{"upnp:rootdevice", 0, uuidvalue_igd},
	{"urn:schemas-upnp-org:device:InternetGatewayDevice:", IGD_VER, uuidvalue_igd},
	{"urn:schemas-upnp-org:device:WANConnectionDevice:", 1, uuidvalue_wcd},
	{"urn:schemas-upnp-org:device:WANDevice:", 1, uuidvalue_wan},
	{"urn:schemas-upnp-org:service:WANCommonInterfaceConfig:", 1, uuidvalue_wan},
	{"urn:schemas-upnp-org:service:WANIPConnection:", WANIPC_VER, uuidvalue_wcd},
#ifndef UPNP_STRICT
	/* We use WAN IP Connection, not PPP connection,
	 * but buggy control points may try to use WanPPPConnection
	 * anyway */
	{"urn:schemas-upnp-org:service:WANPPPConnection:", 1, uuidvalue_wcd},
#endif
#ifdef ENABLE_L3F_SERVICE
	{"urn:schemas-upnp-org:service:Layer3Forwarding:", 1, uuidvalue_igd},
#endif
#ifdef ENABLE_6FC_SERVICE
	{"urn:schemas-upnp-org:service:WANIPv6FirewallControl:", 1, uuidvalue_wcd},
#endif
/* we might want to support urn:schemas-wifialliance-org:device:WFADevice:1
 * urn:schemas-wifialliance-org:device:WFADevice:1
 * in the future */
	{0, 0, 0}
};

/* SendSSDPNotify() sends the SSDP NOTIFY to a specific
 * destination, for a specific UPnP service or device */
static void
SendSSDPNotify(int s, const struct sockaddr * dest, socklen_t dest_len,
               const char * dest_str,
               const char * host, unsigned short http_port,
#ifdef ENABLE_HTTPS
               unsigned short https_port,
#endif
               const char * nt, const char * suffix,
               const char * usn1, const char * usn2, const char * usn3,
               unsigned int lifetime)
{
	char bufr[SSDP_PACKET_MAX_LEN];
	int n, l;

	l = snprintf(bufr, sizeof(bufr),
		"NOTIFY * HTTP/1.1\r\n"
		"HOST: %s:%d\r\n"
		"CACHE-CONTROL: max-age=%u\r\n"
		"LOCATION: http://%s:%u" ROOTDESC_PATH "\r\n"
#ifdef ENABLE_HTTPS
		"SECURELOCATION.UPNP.ORG: https://%s:%u" ROOTDESC_PATH "\r\n"
#endif
		"SERVER: " MINIUPNPD_SERVER_STRING "\r\n"
		"NT: %s%s\r\n"
		"USN: %s%s%s%s\r\n"
		"NTS: ssdp:alive\r\n"
		"OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n" /* UDA v1.1 */
		"01-NLS: %u\r\n" /* same as BOOTID field. UDA v1.1 */
		"BOOTID.UPNP.ORG: %u\r\n" /* UDA v1.1 */
		"CONFIGID.UPNP.ORG: %u\r\n" /* UDA v1.1 */
		"\r\n",
		dest_str, SSDP_PORT,			/* HOST: */
		lifetime,						/* CACHE-CONTROL: */
		host, (unsigned int)http_port,	/* LOCATION: */
#ifdef ENABLE_HTTPS
		host, (unsigned int)https_port,	/* SECURE-LOCATION: */
#endif
		nt, suffix,						/* NT: */
		usn1, usn2, usn3, suffix,		/* USN: */
		upnp_bootid,					/* 01-NLS: */
		upnp_bootid,					/* BOOTID.UPNP.ORG: */
		upnp_configid );				/* CONFIGID.UPNP.ORG: */
	if(l<0) {
		syslog(LOG_ERR, "%s: snprintf error", "SendSSDPNotify()");
		return;
	} else if((unsigned int)l >= sizeof(bufr)) {
		syslog(LOG_WARNING, "%s: truncated output (%u>=%u)",
		       "SendSSDPNotify()", (unsigned)l, (unsigned)sizeof(bufr));
		l = sizeof(bufr) - 1;
	}
	n = sendto_or_schedule(s, bufr, l, 0, dest, dest_len);
	if(n < 0) {
		syslog(LOG_ERR, "sendto(udp_notify=%d, %s): %m", s,
		       host ? host : "NULL");
	} else if(n != l) {
		syslog(LOG_NOTICE, "sendto() sent %d out of %d bytes", n, l);
	}
	/* Due to the unreliable nature of UDP, devices SHOULD send the entire
	 * set of discovery messages more than once with some delay between
	 * sets e.g. a few hundred milliseconds. To avoid network congestion
	 * discovery messages SHOULD NOT be sent more than three times. */
	n = sendto_schedule(s, bufr, l, 0, dest, dest_len, 250);
	if(n < 0) {
		syslog(LOG_ERR, "sendto(udp_notify=%d, %s): %m", s,
		       host ? host : "NULL");
	}
}

/* SendSSDPNotifies() send SSPD NOTIFY for a specific
 * LAN (network interface) for all devices / services */
#ifdef ENABLE_HTTPS
static void
SendSSDPNotifies(int s, const char * host, unsigned short http_port,
                 unsigned short https_port,
                 unsigned int lifetime, int ipv6)
#else
static void
SendSSDPNotifies(int s, const char * host, unsigned short http_port,
                 unsigned int lifetime, int ipv6)
#endif
{
#ifdef ENABLE_IPV6
	struct sockaddr_storage sockname;
	static struct { const char * p1, * p2; } const mcast_addrs[] =
		{ { LL_SSDP_MCAST_ADDR, "[" LL_SSDP_MCAST_ADDR "]" },	/* Link Local */
		  { SL_SSDP_MCAST_ADDR, "[" SL_SSDP_MCAST_ADDR "]" },	/* Site Local */
		  { GL_SSDP_MCAST_ADDR, "[" GL_SSDP_MCAST_ADDR "]" },	/* Global */
		  { NULL, NULL } };
	int j;
#else /* ENABLE_IPV6 */
	struct sockaddr_in sockname;
#endif /* ENABLE_IPV6 */
	socklen_t sockname_len;
	const char * dest_str;
	int i;
	char ver_str[4];
#ifndef ENABLE_IPV6
	UNUSED(ipv6);
#endif /* ENABLE_IPV6 */

	memset(&sockname, 0, sizeof(sockname));
#ifdef ENABLE_IPV6
	/* first iterate destinations for this LAN interface (only 1 for IPv4) */
	for(j = 0; (mcast_addrs[j].p1 != 0 && ipv6) || j < 1; j++) {
		if(ipv6) {
			struct sockaddr_in6 * p = (struct sockaddr_in6 *)&sockname;
			sockname_len = sizeof(struct sockaddr_in6);
			p->sin6_family = AF_INET6;
			p->sin6_port = htons(SSDP_PORT);
			inet_pton(AF_INET6, mcast_addrs[j].p1, &(p->sin6_addr));
			dest_str = mcast_addrs[j].p2;
			/* UPnP Device Architecture 1.1 :
			 * Devices MUST multicast SSDP messages for each of the UPnP-enabled
			 * interfaces. The scope of multicast SSDP messages MUST be
			 * link local FF02::C if the message is sent from a link local address.
			 * If the message is sent from a global address it MUST be multicast
			 * using either global scope FF0E::C or site local scope FF05::C.
			 * In networks with complex topologies and overlapping sites, use of
			 * global scope is RECOMMENDED. */
		} else {
#else /* ENABLE_IPV6 */
		{
#endif /* ENABLE_IPV6 */
			/* IPv4 */
			struct sockaddr_in *p = (struct sockaddr_in *)&sockname;
			sockname_len = sizeof(struct sockaddr_in);
			p->sin_family = AF_INET;
			p->sin_port = htons(SSDP_PORT);
			p->sin_addr.s_addr = inet_addr(SSDP_MCAST_ADDR);
			dest_str = SSDP_MCAST_ADDR;
		}

		/* iterate all services / devices */
		for(i = 0; known_service_types[i].s; i++) {
			if(i==0)
				ver_str[0] = '\0';
			else
				snprintf(ver_str, sizeof(ver_str), "%d", known_service_types[i].version);
			SendSSDPNotify(s, (struct sockaddr *)&sockname, sockname_len, dest_str,
			               host, http_port,
#ifdef ENABLE_HTTPS
			               https_port,
#endif
			               known_service_types[i].s, ver_str,	/* NT: */
			               known_service_types[i].uuid, "::",
			               known_service_types[i].s, /* ver_str,	USN: */
			               lifetime);
			/* for devices, also send NOTIFY on the uuid */
			if(0==memcmp(known_service_types[i].s,
			             "urn:schemas-upnp-org:device", sizeof("urn:schemas-upnp-org:device")-1)) {
				SendSSDPNotify(s, (struct sockaddr *)&sockname, sockname_len, dest_str,
				               host, http_port,
#ifdef ENABLE_HTTPS
				               https_port,
#endif
				               known_service_types[i].uuid, "",	/* NT: */
				               known_service_types[i].uuid, "", "", /* ver_str,	USN: */
				               lifetime);
			}
		} /* for(i = 0; known_service_types[i].s; i++) */
#ifdef ENABLE_IPV6
	} /* for(j = 0; (mcast_addrs[j].p1 != 0 && ipv6) || j < 1; j++) */
#endif /* ENABLE_IPV6 */
}

/* SendSSDPNotifies2() sends SSDP NOTIFY packets on all interfaces
 * for all destinations, all devices / services */
void
SendSSDPNotifies2(int * sockets,
                  unsigned short http_port,
#ifdef ENABLE_HTTPS
                  unsigned short https_port,
#endif
                  unsigned int lifetime)
{
	int i;
	struct lan_addr_s * lan_addr;
	for(i = 0, lan_addr = lan_addrs.lh_first;
	    lan_addr != NULL;
	    lan_addr = lan_addr->list.le_next) {
		SendSSDPNotifies(sockets[i], lan_addr->str, http_port,
#ifdef ENABLE_HTTPS
		                 https_port,
#endif
		                 lifetime, 0);
		i++;
#ifdef ENABLE_IPV6
		if(sockets[i] >= 0) {
			SendSSDPNotifies(sockets[i], ipv6_addr_for_http_with_brackets, http_port,
#ifdef ENABLE_HTTPS
			                 https_port,
#endif
			                 lifetime, 1);
		}
		i++;
#endif	/* ENABLE_IPV6 */
	}
}

/* ProcessSSDPRequest()
 * process SSDP M-SEARCH requests and responds to them */
void
#ifdef ENABLE_HTTPS
ProcessSSDPRequest(int s, unsigned short http_port, unsigned short https_port)
#else
ProcessSSDPRequest(int s, unsigned short http_port)
#endif
{
	int n;
	char bufr[1500];
	socklen_t len_r;
#ifdef ENABLE_IPV6
	struct sockaddr_storage sendername;
	len_r = sizeof(struct sockaddr_storage);
#else
	struct sockaddr_in sendername;
	len_r = sizeof(struct sockaddr_in);
#endif

	n = recvfrom(s, bufr, sizeof(bufr), 0,
	             (struct sockaddr *)&sendername, &len_r);
	if(n < 0)
	{
		/* EAGAIN, EWOULDBLOCK, EINTR : silently ignore (try again next time)
		 * other errors : log to LOG_ERR */
		if(errno != EAGAIN &&
		   errno != EWOULDBLOCK &&
		   errno != EINTR)
		{
			syslog(LOG_ERR, "recvfrom(udp): %m");
		}
		return;
	}
#ifdef ENABLE_HTTPS
	ProcessSSDPData(s, bufr, n, (struct sockaddr *)&sendername,
	                http_port, https_port);
#else
	ProcessSSDPData(s, bufr, n, (struct sockaddr *)&sendername,
	                http_port);
#endif

}

#ifdef ENABLE_HTTPS
void
ProcessSSDPData(int s, const char *bufr, int n,
                const struct sockaddr * sender,
                unsigned short http_port, unsigned short https_port)
#else
void
ProcessSSDPData(int s, const char *bufr, int n,
                const struct sockaddr * sender,
                unsigned short http_port)
#endif
{
	int i, l;
	struct lan_addr_s * lan_addr = NULL;
	const char * st = NULL;
	int st_len = 0;
	int st_ver = 0;
	char sender_str[64];
	char ver_str[4];
	const char * announced_host = NULL;
#ifdef UPNP_STRICT
#ifdef ENABLE_IPV6
	char announced_host_buf[64];
#endif
#endif
#if defined(UPNP_STRICT) || defined(DELAY_MSEARCH_RESPONSE)
	int mx_value = -1;
#endif
	unsigned int delay = 50; /* Non-zero default delay to prevent flooding */
	/* UPnP Device Architecture v1.1.  1.3.3 Search response :
	 * Devices responding to a multicast M-SEARCH SHOULD wait a random period
	 * of time between 0 seconds and the number of seconds specified in the
	 * MX field value of the search request before responding, in order to
	 * avoid flooding the requesting control point with search responses
	 * from multiple devices. If the search request results in the need for
	 * a multiple part response from the device, those multiple part
	 * responses SHOULD be spread at random intervals through the time period
	 * from 0 to the number of seconds specified in the MX header field. */

	/* get the string representation of the sender address */
	sockaddr_to_string(sender, sender_str, sizeof(sender_str));
	lan_addr = get_lan_for_peer(sender);
	if(lan_addr == NULL)
	{
//		syslog(LOG_WARNING, "SSDP packet sender %s not from a LAN, ignoring",
//		       sender_str);
		return;
	}

	if(memcmp(bufr, "NOTIFY", 6) == 0)
	{
		/* ignore NOTIFY packets. We could log the sender and device type */
		return;
	}
	else if(memcmp(bufr, "M-SEARCH", 8) == 0)
	{
		i = 0;
		while(i < n)
		{
			while((i < n - 1) && (bufr[i] != '\r' || bufr[i+1] != '\n'))
				i++;
			i += 2;
			if((i < n - 3) && (strncasecmp(bufr+i, "st:", 3) == 0))
			{
				st = bufr+i+3;
				st_len = 0;
				while((*st == ' ' || *st == '\t') && (st < bufr + n))
					st++;
				while(st[st_len]!='\r' && st[st_len]!='\n'
				     && (st + st_len < bufr + n))
					st_len++;
				l = st_len;
				while(l > 0 && st[l-1] != ':')
					l--;
				st_ver = atoi(st+l);
				syslog(LOG_DEBUG, "ST: %.*s (ver=%d)", st_len, st, st_ver);
				/*j = 0;*/
				/*while(bufr[i+j]!='\r') j++;*/
				/*syslog(LOG_INFO, "%.*s", j, bufr+i);*/
			}
#if defined(UPNP_STRICT) || defined(DELAY_MSEARCH_RESPONSE)
			else if((i < n - 3) && (strncasecmp(bufr+i, "mx:", 3) == 0))
			{
				const char * mx;
				int mx_len;
				mx = bufr+i+3;
				mx_len = 0;
				while((*mx == ' ' || *mx == '\t') && (mx < bufr + n))
					mx++;
				while(mx[mx_len]!='\r' && mx[mx_len]!='\n'
				     && (mx + mx_len < bufr + n))
					mx_len++;
				mx_value = atoi(mx);
				syslog(LOG_DEBUG, "MX: %.*s (value=%d)", mx_len, mx, mx_value);
			}
#endif
		}
#ifdef UPNP_STRICT
		/* For multicast M-SEARCH requests, if the search request does
		 * not contain an MX header field, the device MUST silently
		 * discard and ignore the search request. */
		if(mx_value < 0) {
			syslog(LOG_INFO, "ignoring SSDP packet missing MX: header");
			return;
		} else if(mx_value > 5) {
			/* If the MX header field specifies a field value greater
			 * than 5, the device SHOULD assume that it contained the
			 * value 5 or less. */
			mx_value = 5;
		}
#elif defined(DELAY_MSEARCH_RESPONSE)
		if(mx_value < 0) {
			mx_value = 1;
		} else if(mx_value > 5) {
			/* If the MX header field specifies a field value greater
			 * than 5, the device SHOULD assume that it contained the
			 * value 5 or less. */
			mx_value = 5;
		}
#endif
		/*syslog(LOG_INFO, "SSDP M-SEARCH packet received from %s",
	           sender_str );*/
		if(st && (st_len > 0))
		{
			syslog(LOG_INFO, "SSDP M-SEARCH from %s ST: %.*s",
			       sender_str, st_len, st);
			/* find in which sub network the client is */
			if(sender->sa_family == AF_INET)
			{
				if (lan_addr == NULL)
				{
					syslog(LOG_ERR,
					       "Can't find in which sub network the client %s is",
					       sender_str);
					return;
				}
				announced_host = lan_addr->str;
			}
#ifdef ENABLE_IPV6
			else
			{
				/* IPv6 address with brackets */
#ifdef UPNP_STRICT
				int index;
				struct in6_addr addr6;
				size_t addr6_len = sizeof(addr6);
				/* retrieve the IPv6 address which
				 * will be used locally to reach sender */
				memset(&addr6, 0, sizeof(addr6));
				if(get_src_for_route_to (sender, &addr6, &addr6_len, &index) < 0) {
					syslog(LOG_WARNING, "get_src_for_route_to() failed, using %s", ipv6_addr_for_http_with_brackets);
					announced_host = ipv6_addr_for_http_with_brackets;
				} else {
					if(inet_ntop(AF_INET6, &addr6,
					             announced_host_buf+1,
					             sizeof(announced_host_buf) - 2)) {
						announced_host_buf[0] = '[';
						i = strlen(announced_host_buf);
						if(i < (int)sizeof(announced_host_buf) - 1) {
							announced_host_buf[i] = ']';
							announced_host_buf[i+1] = '\0';
						} else {
							syslog(LOG_NOTICE, "cannot suffix %s with ']'",
							       announced_host_buf);
						}
						announced_host = announced_host_buf;
					} else {
						syslog(LOG_NOTICE, "inet_ntop() failed %m");
						announced_host = ipv6_addr_for_http_with_brackets;
					}
				}
#else
				announced_host = ipv6_addr_for_http_with_brackets;
#endif
			}
#endif
			/* Responds to request with a device as ST header */
			for(i = 0; known_service_types[i].s; i++)
			{
				l = (int)strlen(known_service_types[i].s);
				if(l<=st_len && (0 == memcmp(st, known_service_types[i].s, l))
#ifdef UPNP_STRICT
				   && (st_ver <= known_service_types[i].version)
		/* only answer for service version lower or equal of supported one */
#endif
				   )
				{
					/* SSDP_RESPOND_SAME_VERSION :
					 * response is urn:schemas-upnp-org:service:WANIPConnection:1 when
					 * M-SEARCH included urn:schemas-upnp-org:service:WANIPConnection:1
					 * else the implemented versions is included in the response
					 *
					 * From UPnP Device Architecture v1.1 :
					 * 1.3.2 [...] Updated versions of device and service types
					 * are REQUIRED to be fully backward compatible with
					 * previous versions. Devices MUST respond to M-SEARCH
					 * requests for any supported version. For example, if a
					 * device implements “urn:schemas-upnporg:service:xyz:2”,
					 * it MUST respond to search requests for both that type
					 * and “urn:schemas-upnp-org:service:xyz:1”. The response
					 * MUST specify the same version as was contained in the
					 * search request. [...] */
#ifndef SSDP_RESPOND_SAME_VERSION
					if(i==0)
						ver_str[0] = '\0';
					else
						snprintf(ver_str, sizeof(ver_str), "%d", known_service_types[i].version);
#endif
					syslog(LOG_INFO, "Single search found");
#ifdef DELAY_MSEARCH_RESPONSE
					delay = random() / (1 + RAND_MAX / (1000 * mx_value));
#ifdef DEBUG
					syslog(LOG_DEBUG, "mx=%dsec delay=%ums", mx_value, delay);
#endif
#endif
					SendSSDPResponse(s, sender,
#ifdef SSDP_RESPOND_SAME_VERSION
					                 st, st_len, "",
#else
					                 known_service_types[i].s, l, ver_str,
#endif
					                 announced_host, http_port,
#ifdef ENABLE_HTTPS
					                 https_port,
#endif
					                 known_service_types[i].uuid,
					                 delay);
					break;
				}
			}
			/* Responds to request with ST: ssdp:all */
			/* strlen("ssdp:all") == 8 */
			if(st_len==8 && (0 == memcmp(st, "ssdp:all", 8)))
			{
#ifdef DELAY_MSEARCH_RESPONSE
				unsigned int delay_increment = (mx_value * 1000) / 15;
#endif
				syslog(LOG_INFO, "ssdp:all found");
				for(i=0; known_service_types[i].s; i++)
				{
#ifdef DELAY_MSEARCH_RESPONSE
					delay += delay_increment;
#endif
					if(i==0)
						ver_str[0] = '\0';
					else
						snprintf(ver_str, sizeof(ver_str), "%d", known_service_types[i].version);
					l = (int)strlen(known_service_types[i].s);
					SendSSDPResponse(s, sender,
					                 known_service_types[i].s, l, ver_str,
					                 announced_host, http_port,
#ifdef ENABLE_HTTPS
					                 https_port,
#endif
					                 known_service_types[i].uuid,
					                 delay);
				}
				/* also answer for uuid */
#ifdef DELAY_MSEARCH_RESPONSE
					delay += delay_increment;
#endif
				SendSSDPResponse(s, sender, uuidvalue_igd, strlen(uuidvalue_igd), "",
				                 announced_host, http_port,
#ifdef ENABLE_HTTPS
				                 https_port,
#endif
				                 uuidvalue_igd, delay);
#ifdef DELAY_MSEARCH_RESPONSE
					delay += delay_increment;
#endif
				SendSSDPResponse(s, sender, uuidvalue_wan, strlen(uuidvalue_wan), "",
				                 announced_host, http_port,
#ifdef ENABLE_HTTPS
				                 https_port,
#endif
				                 uuidvalue_wan, delay);
#ifdef DELAY_MSEARCH_RESPONSE
					delay += delay_increment;
#endif
				SendSSDPResponse(s, sender, uuidvalue_wcd, strlen(uuidvalue_wcd), "",
				                 announced_host, http_port,
#ifdef ENABLE_HTTPS
				                 https_port,
#endif
				                 uuidvalue_wcd, delay);
			}
			/* responds to request by UUID value */
			l = (int)strlen(uuidvalue_igd);
			if(l==st_len)
			{
#ifdef DELAY_MSEARCH_RESPONSE
				delay = random() / (1 + RAND_MAX / (1000 * mx_value));
#endif
				if(0 == memcmp(st, uuidvalue_igd, l))
				{
					syslog(LOG_INFO, "ssdp:uuid (IGD) found");
					SendSSDPResponse(s, sender, st, st_len, "",
					                 announced_host, http_port,
#ifdef ENABLE_HTTPS
					                 https_port,
#endif
					                 uuidvalue_igd, delay);
				}
				else if(0 == memcmp(st, uuidvalue_wan, l))
				{
					syslog(LOG_INFO, "ssdp:uuid (WAN) found");
					SendSSDPResponse(s, sender, st, st_len, "",
					                 announced_host, http_port,
#ifdef ENABLE_HTTPS
					                 https_port,
#endif
					                 uuidvalue_wan, delay);
				}
				else if(0 == memcmp(st, uuidvalue_wcd, l))
				{
					syslog(LOG_INFO, "ssdp:uuid (WCD) found");
					SendSSDPResponse(s, sender, st, st_len, "",
					                 announced_host, http_port,
#ifdef ENABLE_HTTPS
					                 https_port,
#endif
					                 uuidvalue_wcd, delay);
				}
			}
		}
		else
		{
			syslog(LOG_INFO, "Invalid SSDP M-SEARCH from %s", sender_str);
		}
	}
	else
	{
		syslog(LOG_INFO, "Unknown udp packet received from %s", sender_str);
	}
}

static int
SendSSDPbyebye(int s, const struct sockaddr * dest, socklen_t destlen,
               const char * dest_str,
               const char * nt, const char * suffix,
               const char * usn1, const char * usn2, const char * usn3)
{
	int n, l;
	char bufr[SSDP_PACKET_MAX_LEN];

	l = snprintf(bufr, sizeof(bufr),
	             "NOTIFY * HTTP/1.1\r\n"
	             "HOST: %s:%d\r\n"
	             "NT: %s%s\r\n"
	             "USN: %s%s%s%s\r\n"
	             "NTS: ssdp:byebye\r\n"
	             "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n" /* UDA v1.1 */
	             "01-NLS: %u\r\n" /* same as BOOTID field. UDA v1.1 */
	             "BOOTID.UPNP.ORG: %u\r\n" /* UDA v1.1 */
	             "CONFIGID.UPNP.ORG: %u\r\n" /* UDA v1.1 */
	             "\r\n",
	             dest_str, SSDP_PORT,		/* HOST : */
	             nt, suffix,				/* NT: */
	             usn1, usn2, usn3, suffix,	/* USN: */
	             upnp_bootid, upnp_bootid, upnp_configid);
	if(l<0)
	{
		syslog(LOG_ERR, "%s: snprintf error", "SendSSDPbyebye()");
		return -1;
	}
	else if((unsigned int)l >= sizeof(bufr))
	{
		syslog(LOG_WARNING, "%s: truncated output (%u>=%u)",
		       "SendSSDPbyebye()", (unsigned)l, (unsigned)sizeof(bufr));
		l = sizeof(bufr) - 1;
	}
	n = sendto_or_schedule(s, bufr, l, 0, dest, destlen);
	if(n < 0)
	{
		syslog(LOG_ERR, "sendto(udp_shutdown=%d): %m", s);
		return -1;
	}
	else if(n != l)
	{
		syslog(LOG_NOTICE, "sendto() sent %d out of %d bytes", n, l);
		return -1;
	}
	return 0;
}

/* This will broadcast ssdp:byebye notifications to inform
 * the network that UPnP is going down. */
int
SendSSDPGoodbye(int * sockets, int n_sockets)
{
	struct sockaddr_in sockname4;
#ifdef ENABLE_IPV6
	struct sockaddr_in6 sockname6;
	struct sockaddr * sockname;
	socklen_t socknamelen;
	int ipv6 = 0;
#endif
	int i, j;
	char ver_str[4];
	int ret = 0;
	const char * dest_str;

    memset(&sockname4, 0, sizeof(struct sockaddr_in));
    sockname4.sin_family = AF_INET;
    sockname4.sin_port = htons(SSDP_PORT);
    sockname4.sin_addr.s_addr = inet_addr(SSDP_MCAST_ADDR);
#ifdef ENABLE_IPV6
	memset(&sockname6, 0, sizeof(struct sockaddr_in6));
	sockname6.sin6_family = AF_INET6;
	sockname6.sin6_port = htons(SSDP_PORT);
	inet_pton(AF_INET6, LL_SSDP_MCAST_ADDR, &(sockname6.sin6_addr));
#else
	dest_str = SSDP_MCAST_ADDR;
#endif

	for(j=0; j<n_sockets; j++)
	{
		if(sockets[j] < 0)
			continue;
#ifdef ENABLE_IPV6
		ipv6 = j & 1;
		if(ipv6) {
			dest_str = "[" LL_SSDP_MCAST_ADDR "]";
			sockname = (struct sockaddr *)&sockname6;
			socknamelen = sizeof(struct sockaddr_in6);
		} else {
			dest_str = SSDP_MCAST_ADDR;
			sockname = (struct sockaddr *)&sockname4;
			socknamelen = sizeof(struct sockaddr_in);
		}
#endif
	    for(i=0; known_service_types[i].s; i++)
	    {
			if(i==0)
				ver_str[0] = '\0';
			else
				snprintf(ver_str, sizeof(ver_str), "%d", known_service_types[i].version);
			ret += SendSSDPbyebye(sockets[j],
#ifdef ENABLE_IPV6
			                      sockname, socknamelen,
#else
			                      (struct sockaddr *)&sockname4, sizeof(struct sockaddr_in),
#endif
			                      dest_str,
			                      known_service_types[i].s, ver_str,	/* NT: */
			                      known_service_types[i].uuid, "::",
			                      known_service_types[i].s); /* ver_str, USN: */
			if(0==memcmp(known_service_types[i].s,
			             "urn:schemas-upnp-org:device", sizeof("urn:schemas-upnp-org:device")-1))
			{
				ret += SendSSDPbyebye(sockets[j],
#ifdef ENABLE_IPV6
				                      sockname, socknamelen,
#else
				                      (struct sockaddr *)&sockname4, sizeof(struct sockaddr_in),
#endif
				                      dest_str,
				                      known_service_types[i].uuid, "",	/* NT: */
				                      known_service_types[i].uuid, "", ""); /* ver_str, USN: */
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
	int i, l, n;
	char ver_str[4];

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if(s < 0) {
		syslog(LOG_ERR, "socket(unix): %m");
		return -1;
	}
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, minissdpdsocketpath, sizeof(addr.sun_path));
	addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
	if(connect(s, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
		syslog(LOG_ERR, "connect(\"%s\"): %m", minissdpdsocketpath);
		close(s);
		return -1;
	}
	for(i = 0; known_service_types[i].s; i++) {
		buffer[0] = 4;	/* request type 4 : submit service */
		/* 4 strings following : ST (service type), USN, Server, Location */
		p = buffer + 1;
		l = (int)strlen(known_service_types[i].s);
		if(i > 0)
			l++;
		CODELENGTH(l, p);
		memcpy(p, known_service_types[i].s, l);
		if(i > 0)
			p[l-1] = '1';
		p += l;
		if(i==0)
			ver_str[0] = '\0';
		else
			snprintf(ver_str, sizeof(ver_str), "%d", known_service_types[i].version);
		l = snprintf(strbuf, sizeof(strbuf), "%s::%s%s",
		             known_service_types[i].uuid, known_service_types[i].s, ver_str);
		if(l<0) {
			syslog(LOG_WARNING, "SubmitServicesToMiniSSDPD: snprintf %m");
			continue;
		} else if((unsigned)l>=sizeof(strbuf)) {
			l = sizeof(strbuf) - 1;
		}
		CODELENGTH(l, p);
		memcpy(p, strbuf, l);
		p += l;
		l = (int)strlen(MINIUPNPD_SERVER_STRING);
		CODELENGTH(l, p);
		memcpy(p, MINIUPNPD_SERVER_STRING, l);
		p += l;
		l = snprintf(strbuf, sizeof(strbuf), "http://%s:%u" ROOTDESC_PATH,
		             host, (unsigned int)port);
		if(l<0) {
			syslog(LOG_WARNING, "SubmitServicesToMiniSSDPD: snprintf %m");
			continue;
		} else if((unsigned)l>=sizeof(strbuf)) {
			l = sizeof(strbuf) - 1;
		}
		CODELENGTH(l, p);
		memcpy(p, strbuf, l);
		p += l;
		/* now write the encoded data */
		n = p - buffer;	/* bytes to send */
		p = buffer;	/* start */
		while(n > 0) {
			l = write(s, p, n);
			if (l < 0) {
				if(errno == EINTR)
					continue;
				syslog(LOG_ERR, "write(): %m");
				close(s);
				return -1;
			} else if (l == 0) {
				syslog(LOG_ERR, "write() returned 0");
				close(s);
				return -1;
			}
			p += l;
			n -= l;
		}
	}
 	close(s);
	return 0;
}

