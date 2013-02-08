/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * author: Ryan Wagoner and Thomas Bernard
 * (c) 2006 Ryan Wagoner
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <net/if_var.h>
#endif
#if defined(__DragonFly__)
#include <net/pf/pfvar.h>
#else
#include <net/pfvar.h>
#endif
#include <kvm.h>
#include <fcntl.h>
#include <nlist.h>
#include <sys/queue.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "../getifstats.h"
#include "../config.h"

static struct nlist list[] = {
	{"_ifnet", 0, 0, 0, 0},
	{NULL,0, 0, 0, 0}
};

int
getifstats(const char * ifname, struct ifdata * data)
{
#if defined(__FreeBSD__) || defined(__DragonFly__)
	struct ifnethead ifh;
#elif defined(__OpenBSD__) || defined(__NetBSD__)
	struct ifnet_head ifh;
#else
	#error "Dont know if I should use struct ifnethead or struct ifnet_head"
#endif
	struct ifnet ifc;
	struct ifnet *ifp;
	kvm_t *kd;
	ssize_t n;
	char errstr[_POSIX2_LINE_MAX];
#ifdef ENABLE_GETIFSTATS_CACHING
	static time_t cache_timestamp = 0;
	static struct ifdata cache_data;
	time_t current_time;
#endif
	if(!data)
		return -1;
	data->baudrate = 4200000;
	data->opackets = 0;
	data->ipackets = 0;
	data->obytes = 0;
	data->ibytes = 0;
	if(!ifname || ifname[0]=='\0')
		return -1;
#ifdef ENABLE_GETIFSTATS_CACHING
	current_time = time(NULL);
	if(current_time == ((time_t)-1)) {
		syslog(LOG_ERR, "getifstats() : time() error : %m");
	} else {
		if(current_time < cache_timestamp + GETIFSTATS_CACHING_DURATION) {
			memcpy(data, &cache_data, sizeof(struct ifdata));
			return 0;
		}
	}
#endif

	/*kd = kvm_open(NULL, NULL, NULL, O_RDONLY, NULL);*/
	kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errstr);
	if(!kd)
	{
		syslog (LOG_ERR, "getifstats() : kvm_open(): %s", errstr);
		return -1;
	}
	if(kvm_nlist(kd, list) < 0)
	{
		syslog(LOG_ERR, "getifstats() : kvm_nlist(): FAILED");
		goto error;
	}
	if(!list[0].n_value)
	{
		syslog(LOG_ERR, "getifstats() : n_value(): FAILED");
		goto error;
	}
	n = kvm_read(kd, list[0].n_value, &ifh, sizeof(ifh));
	if(n<0)
	{
		syslog(LOG_ERR, "getifstats() : kvm_read(head): %s", kvm_geterr(kd));
		goto error;
	}
	for(ifp = TAILQ_FIRST(&ifh); ifp; ifp = TAILQ_NEXT(&ifc, if_list))
	{
		n = kvm_read(kd, (u_long)ifp, &ifc, sizeof(ifc));
		if(n<0)
		{
			syslog(LOG_ERR, "getifstats() : kvm_read(element): %s", kvm_geterr(kd));
			goto error;
		}
		if(strcmp(ifname, ifc.if_xname) == 0)
		{
			/* found the right interface */
			data->opackets = ifc.if_data.ifi_opackets;
			data->ipackets = ifc.if_data.ifi_ipackets;
			data->obytes = ifc.if_data.ifi_obytes;
			data->ibytes = ifc.if_data.ifi_ibytes;
			data->baudrate = ifc.if_data.ifi_baudrate;
			kvm_close(kd);

#ifdef ENABLE_GETIFSTATS_CACHING
			if(current_time!=((time_t)-1)) {
				cache_timestamp = current_time;
				memcpy(&cache_data, data, sizeof(struct ifdata));
			}
#endif
			return 0;	/* ok */
		}
	}
error:
	kvm_close(kd);
	return -1;	/* not found or error */
}

