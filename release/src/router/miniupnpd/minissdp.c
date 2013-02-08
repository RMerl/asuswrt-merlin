/* $Id: minissdp.c,v 1.48 2013/02/07 12:22:25 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2013 Thomas Bernard
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
#include "codelength.h"

/* SSDP ip/port */
#define SSDP_PORT (1900)
#define SSDP_MCAST_ADDR ("239.255.255.250")
#define LL_SSDP_MCAST_ADDR "FF02::C"
#define SL_SSDP_MCAST_ADDR "FF05::C"

/* AddMulticastMembership()
 * param s		socket
 * param ifaddr	ip v4 address
 */
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
        syslog(LOG_ERR, "setsockopt(udp, IP_ADD_MEMBERSHIP): %m");
		return -1;
    }

	return 0;
}

/* AddMulticastMembershipIPv6()
 * param s	socket (IPv6)
 * To be improved to target specific network interfaces */
#ifdef ENABLE_IPV6
static int
AddMulticastMembershipIPv6(int s)
{
	struct ipv6_mreq mr;
	/*unsigned int ifindex;*/

	memset(&mr, 0, sizeof(mr));
	inet_pton(AF_INET6, LL_SSDP_MCAST_ADDR, &mr.ipv6mr_multiaddr);
	/*mr.ipv6mr_interface = ifindex;*/
	mr.ipv6mr_interface = 0; /* 0 : all interfaces */
#ifndef IPV6_ADD_MEMBERSHIP
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#endif
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
	if(ipv6) {
		struct sockaddr_in6 * saddr = (struct sockaddr_in6 *)&sockname;
		saddr->sin6_family = AF_INET6;
		saddr->sin6_port = htons(SSDP_PORT);
		saddr->sin6_addr = in6addr_any;
		sockname_len = sizeof(struct sockaddr_in6);
	} else {
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
		AddMulticastMembershipIPv6(s);
	}
	else
#endif
	{
		for(lan_addr = lan_addrs.lh_first; lan_addr != NULL; lan_addr = lan_addr->list.le_next)
		{
			if(AddMulticastMembership(s, lan_addr->addr.s_addr) < 0)
			{
				syslog(LOG_WARNING,
				       "Failed to add multicast membership for interface %s",
				       lan_addr->str ? lan_addr->str : "NULL");
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
		sockets[i] = OpenAndConfSSDPNotifySocketIPv6(lan_addr->index);
		if(sockets[i] < 0)
			goto error;
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

/* not really an SSDP "announce" as it is the response
 * to a SSDP "M-SEARCH" */
static void
SendSSDPAnnounce2(int s, const struct sockaddr * addr,
                  const char * st, int st_len, const char * suffix,
                  const char * host, unsigned short port)
{
	int l, n;
	char buf[512];
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
		host, (unsigned int)port,
		upnp_bootid, upnp_bootid, upnp_configid);
	addrlen = (addr->sa_family == AF_INET6)
	          ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
	n = sendto(s, buf, l, 0,
	           addr, addrlen);
	sockaddr_to_string(addr, addr_str, sizeof(addr_str));
	syslog(LOG_INFO, "SSDP Announce %d bytes to %s ST: %.*s",n,
       		addr_str,
		l, buf);
	if(n < 0)
	{
		/* XXX handle EINTR, EAGAIN, EWOULDBLOCK */
		syslog(LOG_ERR, "sendto(udp): %m");
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
} const known_service_types[] =
{
	{"upnp:rootdevice", 0},
	{"urn:schemas-upnp-org:device:InternetGatewayDevice:", IGD_VER},
	{"urn:schemas-upnp-org:device:WANConnectionDevice:", 1},
	{"urn:schemas-upnp-org:device:WANDevice:", 1},
	{"urn:schemas-upnp-org:service:WANCommonInterfaceConfig:", 1},
	{"urn:schemas-upnp-org:service:WANIPConnection:", WANIPC_VER},
	{"urn:schemas-upnp-org:service:WANPPPConnection:", 1},
#ifdef ENABLE_L3F_SERVICE
	{"urn:schemas-upnp-org:service:Layer3Forwarding:", 1},
#endif
#ifdef ENABLE_6FC_SERVICE
	{"url:schemas-upnp-org:service:WANIPv6FirewallControl:", 1},
#endif
	{0, 0}
};

static void
SendSSDPNotify(int s, const struct sockaddr * dest,
               const char * host, unsigned short port,
               const char * nt, const char * suffix,
               const char * usn1, const char * usn2, const char * usn3,
               unsigned int lifetime, int ipv6)
{
	char bufr[512];
	int n, l;

	l = snprintf(bufr, sizeof(bufr),
		"NOTIFY * HTTP/1.1\r\n"
		"HOST: %s:%d\r\n"
		"CACHE-CONTROL: max-age=%u\r\n"
		"LOCATION: http://%s:%d" ROOTDESC_PATH"\r\n"
		"SERVER: " MINIUPNPD_SERVER_STRING "\r\n"
		"NT: %s%s\r\n"
		"USN: %s%s%s%s\r\n"
		"NTS: ssdp:alive\r\n"
		"OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n" /* UDA v1.1 */
		"01-NLS: %u\r\n" /* same as BOOTID field. UDA v1.1 */
		"BOOTID.UPNP.ORG: %u\r\n" /* UDA v1.1 */
		"CONFIGID.UPNP.ORG: %u\r\n" /* UDA v1.1 */
		"\r\n",
		ipv6 ? "[" LL_SSDP_MCAST_ADDR "]" : SSDP_MCAST_ADDR,
		SSDP_PORT,
		lifetime,
		host, port,
		nt, suffix, /* NT: */
		usn1, usn2, usn3, suffix, /* USN: */
		upnp_bootid, upnp_bootid, upnp_configid );
	if(l<0)
	{
		syslog(LOG_ERR, "SendSSDPNotifies() snprintf error");
		return;
	}
	else if((unsigned int)l >= sizeof(bufr))
	{
		syslog(LOG_WARNING, "SendSSDPNotifies(): truncated output");
		l = sizeof(bufr);
	}
	n = sendto(s, bufr, l, 0, dest,
#ifdef ENABLE_IPV6
		ipv6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in)
#else
		sizeof(struct sockaddr_in)
#endif
		);
	if(n < 0)
	{
		/* XXX handle EINTR, EAGAIN, EWOULDBLOCK */
		syslog(LOG_ERR, "sendto(udp_notify=%d, %s): %m", s,
		       host ? host : "NULL");
	}
	else if(n != l)
	{
		syslog(LOG_NOTICE, "sendto() sent %d out of %d bytes", n, l);
	}
}

static void
SendSSDPNotifies(int s, const char * host, unsigned short port,
                 unsigned int lifetime, int ipv6)
{
#ifdef ENABLE_IPV6
	struct sockaddr_storage sockname;
#else
	struct sockaddr_in sockname;
#endif
	int i=0;
	char ver_str[4];

	memset(&sockname, 0, sizeof(sockname));
#ifdef ENABLE_IPV6
	if(ipv6)
	{
		struct sockaddr_in6 * p = (struct sockaddr_in6 *)&sockname;
		p->sin6_family = AF_INET6;
		p->sin6_port = htons(SSDP_PORT);
		inet_pton(AF_INET6, LL_SSDP_MCAST_ADDR, &(p->sin6_addr));
	}
	else
#endif
	{
		struct sockaddr_in *p = (struct sockaddr_in *)&sockname;
		p->sin_family = AF_INET;
		p->sin_port = htons(SSDP_PORT);
		p->sin_addr.s_addr = inet_addr(SSDP_MCAST_ADDR);
	}

	while(known_service_types[i].s)
	{
		if(i==0)
			ver_str[0] = '\0';
		else
			snprintf(ver_str, sizeof(ver_str), "%d", known_service_types[i].version);
		SendSSDPNotify(s, (struct sockaddr *)&sockname, host, port,
		               known_service_types[i].s, ver_str,	/* NT: */
		               uuidvalue, "::", known_service_types[i].s, /* ver_str,	USN: */
		               lifetime, ipv6);
		if(i==0) /* rootdevice */
			SendSSDPNotify(s, (struct sockaddr *)&sockname, host, port,
			               uuidvalue, "",	/* NT: */
			               uuidvalue, "", "", /* ver_str,	USN: */
			               lifetime, ipv6);
		i++;
	}
}

void
SendSSDPNotifies2(int * sockets,
                  unsigned short port,
                  unsigned int lifetime)
{
	int i;
	struct lan_addr_s * lan_addr;
	for(i=0, lan_addr = lan_addrs.lh_first;
	    lan_addr != NULL;
	    lan_addr = lan_addr->list.le_next)
	{
		SendSSDPNotifies(sockets[i], lan_addr->str, port,
		                 lifetime, 0);
		i++;
#ifdef ENABLE_IPV6
		SendSSDPNotifies(sockets[i], ipv6_addr_for_http_with_brackets, port,
		                 lifetime, 1);
		i++;
#endif
	}
}

/* ProcessSSDPRequest()
 * process SSDP M-SEARCH requests and responds to them */
void
ProcessSSDPRequest(int s, unsigned short port)
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
	ProcessSSDPData(s, bufr, n, (struct sockaddr *)&sendername, port);

}

void
ProcessSSDPData(int s, const char *bufr, int n,
                const struct sockaddr * sender, unsigned short port) {
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
	int mx_value = -1;
#endif

	/* get the string representation of the sender address */
	sockaddr_to_string(sender, sender_str, sizeof(sender_str));
	lan_addr = get_lan_for_peer(sender);
	if(lan_addr == NULL)
	{
		syslog(LOG_WARNING, "SSDP packet sender %s not from a LAN, ignoring",
		       sender_str);
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
#ifdef UPNP_STRICT
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
		if(mx_value < 0) {
			syslog(LOG_INFO, "ignoring SSDP packet missing MX: header");
			return;
		}
#endif
		/*syslog(LOG_INFO, "SSDP M-SEARCH packet received from %s",
	           sender_str );*/
		if(st && (st_len > 0))
		{
			/* TODO : doesnt answer at once but wait for a random time */
			syslog(LOG_INFO, "SSDP M-SEARCH from %s ST: %.*s",
			       sender_str, st_len, st);
			/* find in which sub network the client is */
			if(sender->sa_family == AF_INET)
			{
				if (lan_addr == NULL)
				{
					syslog(LOG_ERR, "Can't find in which sub network the client %s is",
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
					syslog(LOG_INFO, "Single search found");
					SendSSDPAnnounce2(s, sender,
					                  st, st_len, "",
					                  announced_host, port);
					break;
				}
			}
			/* Responds to request with ST: ssdp:all */
			/* strlen("ssdp:all") == 8 */
			if(st_len==8 && (0 == memcmp(st, "ssdp:all", 8)))
			{
				syslog(LOG_INFO, "ssdp:all found");
				for(i=0; known_service_types[i].s; i++)
				{
					if(i==0)
						ver_str[0] = '\0';
					else
						snprintf(ver_str, sizeof(ver_str), "%d", known_service_types[i].version);
					l = (int)strlen(known_service_types[i].s);
					SendSSDPAnnounce2(s, sender,
					                  known_service_types[i].s, l, ver_str,
					                  announced_host, port);
				}
				/* also answer for uuid */
				SendSSDPAnnounce2(s, sender, uuidvalue, strlen(uuidvalue), "",
				                  announced_host, port);
			}
			/* responds to request by UUID value */
			l = (int)strlen(uuidvalue);
			if(l==st_len && (0 == memcmp(st, uuidvalue, l)))
			{
				syslog(LOG_INFO, "ssdp:uuid found");
				SendSSDPAnnounce2(s, sender, st, st_len, "",
				                  announced_host, port);
			}
		}
		else
		{
			syslog(LOG_INFO, "Invalid SSDP M-SEARCH from %s", sender_str);
		}
	}
	else
	{
		syslog(LOG_NOTICE, "Unknown udp packet received from %s", sender_str);
	}
}

static int
SendSSDPbyebye(int s, const struct sockaddr * dest,
               const char * nt, const char * suffix,
               const char * usn1, const char * usn2, const char * usn3,
               int ipv6)
{
	int n, l;
	char bufr[512];

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
	             ipv6 ? "[" LL_SSDP_MCAST_ADDR "]" : SSDP_MCAST_ADDR,
	             SSDP_PORT,
	             nt, suffix,	/* NT: */
	             usn1, usn2, usn3, suffix,	/* USN: */
	             upnp_bootid, upnp_bootid, upnp_configid);
	if(l<0)
	{
		syslog(LOG_ERR, "SendSSDPbyebye() snprintf error");
		return -1;
	}
	else if((unsigned int)l >= sizeof(bufr))
	{
		syslog(LOG_WARNING, "SendSSDPbyebye(): truncated output");
		l = sizeof(bufr);
	}
	n = sendto(s, bufr, l, 0, dest,
#ifdef ENABLE_IPV6
	           ipv6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in)
#else
	           sizeof(struct sockaddr_in)
#endif
	          );
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
	struct sockaddr_in sockname;
#ifdef ENABLE_IPV6
	struct sockaddr_in6 sockname6;
#endif
	int i, j;
	char ver_str[4];
	int ret = 0;
	int ipv6 = 0;

    memset(&sockname, 0, sizeof(struct sockaddr_in));
    sockname.sin_family = AF_INET;
    sockname.sin_port = htons(SSDP_PORT);
    sockname.sin_addr.s_addr = inet_addr(SSDP_MCAST_ADDR);
#ifdef ENABLE_IPV6
	memset(&sockname6, 0, sizeof(struct sockaddr_in6));
	sockname6.sin6_family = AF_INET6;
	sockname6.sin6_port = htons(SSDP_PORT);
	inet_pton(AF_INET6, LL_SSDP_MCAST_ADDR, &(sockname6.sin6_addr));
#endif

	for(j=0; j<n_sockets; j++)
	{
#ifdef ENABLE_IPV6
		ipv6 = j & 1;
#endif
	    for(i=0; known_service_types[i].s; i++)
	    {
			if(i==0)
				ver_str[0] = '\0';
			else
				snprintf(ver_str, sizeof(ver_str), "%d", known_service_types[i].version);
			ret += SendSSDPbyebye(sockets[j],
#ifdef ENABLE_IPV6
			                      ipv6 ? (struct sockaddr *)&sockname6 : (struct sockaddr *)&sockname,
#else
			                      (struct sockaddr *)&sockname,
#endif
			                      known_service_types[i].s, ver_str,	/* NT: */
			                      uuidvalue, "::", known_service_types[i].s, /* ver_str, USN: */
			                      ipv6);
			if(i==0)	/* root device */
			{
				ret += SendSSDPbyebye(sockets[j],
#ifdef ENABLE_IPV6
				                      ipv6 ? (struct sockaddr *)&sockname6 : (struct sockaddr *)&sockname,
#else
				                      (struct sockaddr *)&sockname,
#endif
				                      uuidvalue, "",	/* NT: */
				                      uuidvalue, "", "", /* ver_str, USN: */
				                      ipv6);
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
		             uuidvalue, known_service_types[i].s, ver_str);
		CODELENGTH(l, p);
		memcpy(p, strbuf, l);
		p += l;
		l = (int)strlen(MINIUPNPD_SERVER_STRING);
		CODELENGTH(l, p);
		memcpy(p, MINIUPNPD_SERVER_STRING, l);
		p += l;
		l = snprintf(strbuf, sizeof(strbuf), "http://%s:%u" ROOTDESC_PATH,
		             host, (unsigned int)port);
		CODELENGTH(l, p);
		memcpy(p, strbuf, l);
		p += l;
		/* now write the encoded data */
		n = p - buffer;	/* bytes to send */
		p = buffer;	/* start */
		while(n > 0) {
			l = write(s, p, n);
			if (l < 0) {
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

