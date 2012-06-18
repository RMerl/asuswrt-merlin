/*
 *
 *   Authors:
 *    Lars Fenneberg		<lf@elemental.net>
 *
 *   This software is Copyright 1996,1997 by the above mentioned author(s),
 *   All Rights Reserved.
 *
 *   The license which is distributed with this software in the file COPYRIGHT
 *   applies to this software. If your distribution is missing this file, you
 *   may request it from <pekkas@netcore.fi>.
 *
 */

#include "config.h"
#include "includes.h"
#include "radvd.h"
#include "defaults.h"

int
check_device(struct Interface *iface)
{
	struct ifreq	ifr;

	strncpy(ifr.ifr_name, iface->Name, IFNAMSIZ-1);
	ifr.ifr_name[IFNAMSIZ-1] = '\0';

	if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0)
	{
		if (!iface->IgnoreIfMissing)
			flog(LOG_ERR, "ioctl(SIOCGIFFLAGS) failed for %s: %s",
				iface->Name, strerror(errno));
		return (-1);
	}

	if (!(ifr.ifr_flags & IFF_UP))
	{
		if (!iface->IgnoreIfMissing)
                	flog(LOG_ERR, "interface %s is not UP", iface->Name);
		return (-1);
	}
	if (!(ifr.ifr_flags & IFF_RUNNING))
	{
		if (!iface->IgnoreIfMissing)
                	flog(LOG_ERR, "interface %s is not RUNNING", iface->Name);
		return (-1);
	}

	if (! iface->UnicastOnly && !(ifr.ifr_flags & IFF_MULTICAST))
	{
		flog(LOG_WARNING, "interface %s does not support multicast",
			iface->Name);
		flog(LOG_WARNING, "   do you need to add the UnicastOnly flag?");
	}

	if (! iface->UnicastOnly && !(ifr.ifr_flags & IFF_BROADCAST))
	{
		flog(LOG_WARNING, "interface %s does not support broadcast",
			iface->Name);
		flog(LOG_WARNING, "   do you need to add the UnicastOnly flag?");
	}

	return 0;
}

int
get_v4addr(const char *ifn, unsigned int *dst)
{
        struct ifreq    ifr;
        struct sockaddr_in *addr;
        int fd;

        if( ( fd = socket(AF_INET,SOCK_DGRAM,0) ) < 0 )
        {
                flog(LOG_ERR, "create socket for IPv4 ioctl failed for %s: %s",
                        ifn, strerror(errno));
                return (-1);
        }

        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, ifn, IFNAMSIZ-1);
        ifr.ifr_name[IFNAMSIZ-1] = '\0';
        ifr.ifr_addr.sa_family = AF_INET;

        if (ioctl(fd, SIOCGIFADDR, &ifr) < 0)
        {
                flog(LOG_ERR, "ioctl(SIOCGIFADDR) failed for %s: %s",
                        ifn, strerror(errno));
                close( fd );
                return (-1);
        }

        addr = (struct sockaddr_in *)(&ifr.ifr_addr);

        dlog(LOG_DEBUG, 3, "IPv4 address for %s is %s", ifn,
                inet_ntoa( addr->sin_addr ) );

        *dst = addr->sin_addr.s_addr;

        close( fd );

        return 0;
}
