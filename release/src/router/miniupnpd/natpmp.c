/* $Id: natpmp.c,v 1.32 2012/05/27 22:36:03 nanard Exp $ */
/* MiniUPnP project
 * (c) 2007-2012 Thomas Bernard
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "macros.h"
#include "config.h"
#include "natpmp.h"
#include "upnpglobalvars.h"
#include "getifaddr.h"
#include "upnpredirect.h"
#include "commonrdr.h"
#include "upnputils.h"

#ifdef ENABLE_NATPMP

int OpenAndConfNATPMPSocket(in_addr_t addr)
{
	int snatpmp;
	int i = 1;
	snatpmp = socket(PF_INET, SOCK_DGRAM, 0/*IPPROTO_UDP*/);
	if(snatpmp<0)
	{
		syslog(LOG_ERR, "%s: socket(natpmp): %m",
		       "OpenAndConfNATPMPSocket");
		return -1;
	}
	if(setsockopt(snatpmp, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) < 0)
	{
		syslog(LOG_WARNING, "%s: setsockopt(natpmp, SO_REUSEADDR): %m",
		       "OpenAndConfNATPMPSocket");
	}
	if(!set_non_blocking(snatpmp))
	{
		syslog(LOG_WARNING, "%s: set_non_blocking(): %m",
		       "OpenAndConfNATPMPSocket");
	}
	{
		struct sockaddr_in natpmp_addr;
		memset(&natpmp_addr, 0, sizeof(natpmp_addr));
		natpmp_addr.sin_family = AF_INET;
		natpmp_addr.sin_port = htons(NATPMP_PORT);
		/*natpmp_addr.sin_addr.s_addr = INADDR_ANY; */
		natpmp_addr.sin_addr.s_addr = addr;
		if(bind(snatpmp, (struct sockaddr *)&natpmp_addr, sizeof(natpmp_addr)) < 0)
		{
			syslog(LOG_ERR, "bind(natpmp): %m");
			close(snatpmp);
			return -1;
		}
	}
	return snatpmp;
}

int OpenAndConfNATPMPSockets(int * sockets)
{
	int i, j;
	struct lan_addr_s * lan_addr;
	for(i = 0, lan_addr = lan_addrs.lh_first; lan_addr != NULL; lan_addr = lan_addr->list.le_next, i++)
	{
		sockets[i] = OpenAndConfNATPMPSocket(lan_addr->addr.s_addr);
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

static void FillPublicAddressResponse(unsigned char * resp, in_addr_t senderaddr)
{
#ifndef MULTIPLE_EXTERNAL_IP
	char tmp[16];
	UNUSED(senderaddr);

	if(use_ext_ip_addr) {
        inet_pton(AF_INET, use_ext_ip_addr, resp+8);
	} else {
		if(!ext_if_name || ext_if_name[0]=='\0') {
			resp[3] = 3;	/* Network Failure (e.g. NAT box itself
			                 * has not obtained a DHCP lease) */
		} else if(getifaddr(ext_if_name, tmp, INET_ADDRSTRLEN) < 0) {
			syslog(LOG_ERR, "Failed to get IP for interface %s", ext_if_name);
			resp[3] = 3;	/* Network Failure (e.g. NAT box itself
			                 * has not obtained a DHCP lease) */
		} else {
			inet_pton(AF_INET, tmp, resp+8); /* ok */
		}
	}
#else
	struct lan_addr_s * lan_addr;

	for(lan_addr = lan_addrs.lh_first; lan_addr != NULL; lan_addr = lan_addr->list.le_next) {
		if( (senderaddr & lan_addr->mask.s_addr)
		   == (lan_addr->addr.s_addr & lan_addr->mask.s_addr)) {
			memcpy(resp+8, &lan_addr->ext_ip_addr,
			       sizeof(lan_addr->ext_ip_addr));
			break;
		}
	}
#endif
}

/** read the request from the socket, process it and then send the
 * response back.
 */
void ProcessIncomingNATPMPPacket(int s)
{
	unsigned char req[32];	/* request udp packet */
	unsigned char resp[32];	/* response udp packet */
	int resplen;
	struct sockaddr_in senderaddr;
	socklen_t senderaddrlen = sizeof(senderaddr);
	int n;
	char senderaddrstr[16];
	n = recvfrom(s, req, sizeof(req), 0,
	             (struct sockaddr *)&senderaddr, &senderaddrlen);
	if(n<0) {
		/* EAGAIN, EWOULDBLOCK and EINTR : silently ignore (retry next time)
		 * other errors : log to LOG_ERR */
		if(errno != EAGAIN &&
		   errno != EWOULDBLOCK &&
		   errno != EINTR) {
			syslog(LOG_ERR, "recvfrom(natpmp): %m");
		}
		return;
	}
	if(!inet_ntop(AF_INET, &senderaddr.sin_addr,
	              senderaddrstr, sizeof(senderaddrstr))) {
		syslog(LOG_ERR, "inet_ntop(natpmp): %m");
	}
	syslog(LOG_INFO, "NAT-PMP request received from %s:%hu %dbytes",
           senderaddrstr, ntohs(senderaddr.sin_port), n);
	if(n<2 || ((((req[1]-1)&~1)==0) && n<12)) {
		syslog(LOG_WARNING, "discarding NAT-PMP request (too short) %dBytes",
		       n);
		return;
	}
	if(req[1] & 128) {
		/* discarding NAT-PMP responses silently */
		return;
	}
	memset(resp, 0, sizeof(resp));
	resplen = 8;
	resp[1] = 128 + req[1];	/* response OPCODE is request OPCODE + 128 */
	/* setting response TIME STAMP :
	 * time elapsed since its port mapping table was initialized on
	 * startup or reset for any other reason */
	*((uint32_t *)(resp+4)) = htonl(time(NULL) - startup_time);
	if(req[0] > 0) {
		/* invalid version */
		syslog(LOG_WARNING, "unsupported NAT-PMP version : %u",
		       (unsigned)req[0]);
		resp[3] = 1;	/* unsupported version */
	} else switch(req[1]) {
	case 0:	/* Public address request */
		syslog(LOG_INFO, "NAT-PMP public address request");
		FillPublicAddressResponse(resp, senderaddr.sin_addr.s_addr);
		resplen = 12;
		break;
	case 1:	/* UDP port mapping request */
	case 2:	/* TCP port mapping request */
		{
			unsigned short iport;	/* private port */
			unsigned short eport;	/* public port */
			uint32_t lifetime; 		/* lifetime=0 => remove port mapping */
			int r;
			int proto;
			char iaddr_old[16];
			unsigned short iport_old;
			unsigned int timestamp;

			iport = ntohs(*((uint16_t *)(req+4)));
			eport = ntohs(*((uint16_t *)(req+6)));
			lifetime = ntohl(*((uint32_t *)(req+8)));
			proto = (req[1]==1)?IPPROTO_UDP:IPPROTO_TCP;
			syslog(LOG_INFO, "NAT-PMP port mapping request : "
			                 "%hu->%s:%hu %s lifetime=%us",
			                 eport, senderaddrstr, iport,
			                 (req[1]==1)?"udp":"tcp", lifetime);
			if(eport==0)
				eport = iport;
			/* TODO: accept port mapping if iport ok but eport not ok
			 * (and set eport correctly) */
			if(lifetime == 0) {
				/* remove the mapping */
				if(iport == 0) {
					/* remove all the mappings for this client */
					int index = 0;
					unsigned short eport2, iport2;
					char iaddr2[16];
					int proto2;
					char desc[64];
					while(get_redirect_rule_by_index(index, 0,
					          &eport2, iaddr2, sizeof(iaddr2),
							  &iport2, &proto2,
							  desc, sizeof(desc),
					          0, 0, &timestamp, 0, 0) >= 0) {
						syslog(LOG_DEBUG, "%d %d %hu->'%s':%hu '%s'",
						       index, proto2, eport2, iaddr2, iport2, desc);
						if(0 == strcmp(iaddr2, senderaddrstr)
						  && 0 == memcmp(desc, "NAT-PMP", 7)) {
							r = _upnp_delete_redir(eport2, proto2);
							/* TODO : check return value */
							if(r<0) {
								syslog(LOG_ERR, "failed to remove port mapping");
								index++;
							} else {
								syslog(LOG_INFO, "NAT-PMP %s port %hu mapping removed",
								       proto2==IPPROTO_TCP?"TCP":"UDP", eport2);
							}
						} else {
							index++;
						}
					}
				} else {
					/* To improve the interworking between nat-pmp and
					 * UPnP, we should check that we remove only NAT-PMP
					 * mappings */
					r = _upnp_delete_redir(eport, proto);
					/*syslog(LOG_DEBUG, "%hu %d r=%d", eport, proto, r);*/
					if(r<0) {
						//syslog(LOG_ERR, "Failed to remove NAT-PMP mapping eport %hu, protocol %s", eport, (proto==IPPROTO_TCP)?"TCP":"UDP");
						resp[3] = 2;	/* Not Authorized/Refused */
					}
				}
				eport = 0; /* to indicate correct removing of port mapping */
			} else if(iport==0
			   || !check_upnp_rule_against_permissions(upnppermlist, num_upnpperm, eport, senderaddr.sin_addr, iport)) {
				resp[3] = 2;	/* Not Authorized/Refused */
			} else do {
				r = get_redirect_rule(ext_if_name, eport, proto,
				                      iaddr_old, sizeof(iaddr_old),
				                      &iport_old, 0, 0, 0, 0,
				                      &timestamp, 0, 0);
				if(r==0) {
					if(strcmp(senderaddrstr, iaddr_old)==0
				       && iport==iport_old) {
						/* redirection allready existing */
						syslog(LOG_INFO, "port %hu %s already redirected to %s:%hu, replacing",
						       eport, (proto==IPPROTO_TCP)?"tcp":"udp", iaddr_old, iport_old);
						/* remove and then add again */
						if(_upnp_delete_redir(eport, proto) < 0) {
							syslog(LOG_ERR, "failed to remove port mapping");
							break;
						}
					} else {
						eport++;
						continue;
					}
				}
				{ /* do the redirection */
					char desc[64];
#if 0
					timestamp = (unsigned)(time(NULL) - startup_time)
					                      + lifetime;
					snprintf(desc, sizeof(desc), "NAT-PMP %u", timestamp);
#else
					timestamp = time(NULL) + lifetime;
					snprintf(desc, sizeof(desc), "NAT-PMP %hu %s",
					         eport, (proto==IPPROTO_TCP)?"tcp":"udp");
#endif
					/* TODO : check return code */
					if(upnp_redirect_internal(NULL, eport, senderaddrstr,
					                          iport, proto, desc,
					                          timestamp) < 0) {
						syslog(LOG_ERR, "Failed to add NAT-PMP %hu %s->%s:%hu '%s'",
						       eport, (proto==IPPROTO_TCP)?"tcp":"udp", senderaddrstr, iport, desc);
						resp[3] = 3;  /* Failure */
#if 0
					} else if( !nextnatpmptoclean_eport
					         || timestamp < nextnatpmptoclean_timestamp) {
						nextnatpmptoclean_timestamp = timestamp;
						nextnatpmptoclean_eport = eport;
						nextnatpmptoclean_proto = proto;
#endif
					}
					break;
				}
			} while(r==0);
			*((uint16_t *)(resp+8)) = htons(iport);	/* private port */
			*((uint16_t *)(resp+10)) = htons(eport);	/* public port */
			*((uint32_t *)(resp+12)) = htonl(lifetime);	/* Port Mapping lifetime */
		}
		resplen = 16;
		break;
	default:
		resp[3] = 5;	/* Unsupported OPCODE */
	}
	n = sendto(s, resp, resplen, 0,
	           (struct sockaddr *)&senderaddr, sizeof(senderaddr));
	if(n<0) {
		syslog(LOG_ERR, "sendto(natpmp): %m");
	} else if(n<resplen) {
		syslog(LOG_ERR, "sendto(natpmp): sent only %d bytes out of %d",
		       n, resplen);
	}
}

#if 0
/* iterate through the redirection list to find those who came
 * from NAT-PMP and select the first to expire */
int ScanNATPMPforExpiration()
{
	char desc[64];
	unsigned short iport, eport;
	int proto;
	int r, i;
	unsigned timestamp;
	nextnatpmptoclean_eport = 0;
	nextnatpmptoclean_timestamp = 0;
	for(i = 0; ; i++) {
		r = get_redirect_rule_by_index(i, 0, &eport, 0, 0,
		                               &iport, &proto, desc, sizeof(desc),
		                               &timestamp, 0, 0);
		if(r<0)
			break;
		if(sscanf(desc, "NAT-PMP %u", &timestamp) == 1) {
			if( !nextnatpmptoclean_eport
			  || timestamp < nextnatpmptoclean_timestamp) {
				nextnatpmptoclean_eport = eport;
				nextnatpmptoclean_proto = proto;
				nextnatpmptoclean_timestamp = timestamp;
				syslog(LOG_DEBUG, "set nextnatpmptoclean_timestamp to %u", timestamp);
			}
		}
	}
	return 0;
}

/* remove the next redirection that is expired
 */
int CleanExpiredNATPMP()
{
	char desc[64];
	unsigned timestamp;
	unsigned short iport;
	if(get_redirect_rule(ext_if_name, nextnatpmptoclean_eport,
	                     nextnatpmptoclean_proto,
	                     0, 0,
	                     &iport, desc, sizeof(desc), &timestamp, 0, 0) < 0)
		return ScanNATPMPforExpiration();
	/* check desc - this is important since we keep expiration time as part
	 * of the desc.
	 * If the rule is renewed, timestamp and nextnatpmptoclean_timestamp
	 * can be different. In that case, the rule must not be removed ! */
	if(sscanf(desc, "NAT-PMP %u", &timestamp) == 1) {
		if(timestamp > nextnatpmptoclean_timestamp)
			return ScanNATPMPforExpiration();
	}
	/* remove redirection then search for next one:) */
	if(_upnp_delete_redir(nextnatpmptoclean_eport, nextnatpmptoclean_proto)<0)
		return -1;
	syslog(LOG_NOTICE, "Expired NAT-PMP mapping port %hu %s removed",
	       nextnatpmptoclean_eport,
	       nextnatpmptoclean_proto==IPPROTO_TCP?"TCP":"UDP");
	return ScanNATPMPforExpiration();
}
#endif

/* SendNATPMPPublicAddressChangeNotification()
 * should be called when the public IP address changed */
void SendNATPMPPublicAddressChangeNotification(int * sockets, int n_sockets)
{
	struct sockaddr_in sockname;
	unsigned char notif[12];
	int j, n;

	notif[0] = 0;	/* vers */
	notif[1] = 128;	/* OP code */
	notif[2] = 0;	/* result code */
	notif[3] = 0;	/* result code */
	/* seconds since "start of epoch" :
	 * time elapsed since the port mapping table was initialized on
	 * startup or reset for any other reason */
	*((uint32_t *)(notif+4)) = htonl(time(NULL) - startup_time);
#ifndef MULTIPLE_EXTERNAL_IP
	FillPublicAddressResponse(notif, 0);
	if(notif[3])
	{
		syslog(LOG_WARNING, "%s: cannot get public IP address, stopping",
		       "SendNATPMPPublicAddressChangeNotification");
		return;
	}
#endif
	memset(&sockname, 0, sizeof(struct sockaddr_in));
    sockname.sin_family = AF_INET;
    sockname.sin_addr.s_addr = inet_addr(NATPMP_NOTIF_ADDR);

	for(j=0; j<n_sockets; j++)
	{
		if(sockets[j] < 0)
			continue;
#ifdef MULTIPLE_EXTERNAL_IP
		{
			struct lan_addr_s * lan_addr = lan_addrs.lh_first;
			int i;
			for(i=0; i<j; i++)
				lan_addr = lan_addr->list.le_next;
			FillPublicAddressResponse(notif, lan_addr->addr.s_addr);
		}
#endif
		/* Port to use in 2006 version of the NAT-PMP specification */
    	sockname.sin_port = htons(NATPMP_PORT);
		n = sendto(sockets[j], notif, 12, 0,
		           (struct sockaddr *)&sockname, sizeof(struct sockaddr_in));
		if(n < 0)
		{
			syslog(LOG_ERR, "%s: sendto(s_udp=%d): %m",
			       "SendNATPMPPublicAddressChangeNotification", sockets[j]);
			return;
		}
		/* Port to use in 2008 version of the NAT-PMP specification */
    	sockname.sin_port = htons(NATPMP_NOTIF_PORT);
		n = sendto(sockets[j], notif, 12, 0,
		           (struct sockaddr *)&sockname, sizeof(struct sockaddr_in));
		if(n < 0)
		{
			syslog(LOG_ERR, "%s: sendto(s_udp=%d): %m",
			       "SendNATPMPPublicAddressChangeNotification", sockets[j]);
			return;
		}
	}
}

#endif

