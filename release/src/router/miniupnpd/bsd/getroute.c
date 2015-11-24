/* $Id: getroute.c,v 1.12 2015/11/19 11:46:30 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2015 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#ifdef AF_LINK
#include <net/if_dl.h>
#endif

#include "../config.h"
#include "../upnputils.h"

#ifndef SA_SIZE
#define SA_SIZE(sa) (SA_LEN(sa))
#endif /* SA_SIZE */

int
get_src_for_route_to(const struct sockaddr * dst,
                     void * src, size_t * src_len,
                     int * index)
{
	int found = 0;
	int s;
	int l, i;
	char * p;
	struct sockaddr * sa;
	struct {
	  struct rt_msghdr m_rtm;
	  char       m_space[512];
	} m_rtmsg;
#define rtm m_rtmsg.m_rtm

	if(dst == NULL)
		return -1;
	if(dst->sa_len > 0) {
		l = dst->sa_len;
	} else {
		if(dst->sa_family == AF_INET)
			l = sizeof(struct sockaddr_in);
		else if(dst->sa_family == AF_INET6)
			l = sizeof(struct sockaddr_in6);
		else {
			syslog(LOG_ERR, "unknown address family %d", dst->sa_family);
			return -1;
		}
	}
	s = socket(PF_ROUTE, SOCK_RAW, dst->sa_family);
	if(s < 0) {
		syslog(LOG_ERR, "socket(PF_ROUTE) failed : %m");
		return -1;
	}
	memset(&rtm, 0, sizeof(rtm));
	rtm.rtm_type = RTM_GET;
	rtm.rtm_flags = RTF_UP;
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_seq = 1;
	rtm.rtm_addrs = RTA_DST | RTA_IFA | RTA_IFP;	/* pass destination address, request source address & interface */
	memcpy(m_rtmsg.m_space, dst, l);
	((struct sockaddr *)m_rtmsg.m_space)->sa_len = l;
	rtm.rtm_msglen = sizeof(struct rt_msghdr) + l;
	if(write(s, &m_rtmsg, rtm.rtm_msglen) < 0) {
		syslog(LOG_ERR, "write: %m");
		close(s);
		return -1;
	}

	do {
		l = read(s, &m_rtmsg, sizeof(m_rtmsg));
		if(l<0) {
			syslog(LOG_ERR, "read: %m");
			close(s);
			return -1;
		}
		syslog(LOG_DEBUG, "read l=%d seq=%d pid=%d   sizeof(struct rt_msghdr)=%d",
		       l, rtm.rtm_seq, rtm.rtm_pid, (int)sizeof(struct rt_msghdr));
	} while(l > 0 && (rtm.rtm_pid != getpid() || rtm.rtm_seq != 1));
	close(s);
	if(l <= 0) {
		syslog(LOG_WARNING, "no matching ROUTE response message");
		return -1;
	}
	p = m_rtmsg.m_space;
	if(rtm.rtm_addrs) {
		for(i=1; i<0x8000; i <<= 1) {
			if(i & rtm.rtm_addrs) {
				char tmp[256] = { 0 };
				if(p >= (char *)&m_rtmsg + l) {
					syslog(LOG_ERR, "error parsing ROUTE response message");
					break;
				}
				sa = (struct sockaddr *)p;
				sockaddr_to_string(sa, tmp, sizeof(tmp));
				syslog(LOG_DEBUG, "type=%d sa_len=%d sa_family=%d %s",
				       i, sa->sa_len, sa->sa_family, tmp);
				if(i == RTA_IFA) {
					size_t len = 0;
					void * paddr = NULL;
					if(sa->sa_family == AF_INET) {
						paddr = &((struct sockaddr_in *)sa)->sin_addr;
						len = sizeof(struct in_addr);
					} else if(sa->sa_family == AF_INET6) {
						paddr = &((struct sockaddr_in6 *)sa)->sin6_addr;
						len = sizeof(struct in6_addr);
					}
					if(paddr) {
						if(src && src_len) {
							if(*src_len < len) {
								syslog(LOG_WARNING, "cannot copy src. %u<%u",
								       (unsigned)*src_len, (unsigned)len);
								return -1;
							}
							memcpy(src, paddr, len);
							*src_len = len;
						}
						found = 1;
					}
				}
#ifdef AF_LINK
				else if((i == RTA_IFP) && (sa->sa_family == AF_LINK)) {
					struct sockaddr_dl * sdl = (struct sockaddr_dl *)sa;
					if(index)
						*index = sdl->sdl_index;
				}
#endif
				/* at least 4 bytes per address are reserved,
				 * that is true with OpenBSD 4.3.
				 * The test is only useful when SA_SIZE() is not properly
				 * defined, as it should be always >= sizeof(long) */
				if(SA_SIZE(sa) > 0)
					p += SA_SIZE(sa);
				else
					p += sizeof(long);
			}
		}
	}
	return found ? 0 : -1;
}

