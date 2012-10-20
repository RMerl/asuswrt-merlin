/**************************************
 ecmh - Easy Cast du Multi Hub
 by Jeroen Massar <jeroen@unfix.org>
***************************************
 $Author: fuzzel $
 $Id: ecmh.h,v 1.10 2005/02/09 17:58:06 fuzzel Exp $
 $Date: 2005/02/09 17:58:06 $
**************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <pwd.h>
#include <getopt.h>

#include <net/if.h>
#include <netinet/if_ether.h>
#include <sched.h>
#ifdef linux
#include <netpacket/packet.h>
#endif
#ifdef __FreeBSD__
#include <fcntl.h>
#include <sys/uio.h>
#include <netinet/in_systm.h>
#include <net/ethernet.h>
#include <net/bpf.h>
#define ETH_P_IPV6 ETHERTYPE_IPV6
#define ETH_P_IP ETHERTYPE_IP
#endif
#include <ifaddrs.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#define	ICMP6_MEMBERSHIP_QUERY		MLD_LISTENER_QUERY
#define	ICMP6_MEMBERSHIP_REPORT		MLD_LISTENER_REPORT
#define	ICMP6_MEMBERSHIP_REDUCTION	MLD_LISTENER_REDUCTION
#define	ICMP6_DST_UNREACH_NOTNEIGHBOR	ICMP6_DST_UNREACH_BEYONDSCOPE

#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <sys/ioctl.h>
#include "mld.h"

#define PIDFILE "/var/run/ecmh.pid"
#define ECMH_DUMPFILE "/var/run/ecmh.dump"

#define ECMH_VERSION_STRING "Easy Cast du Multi Hub (ecmh) %s by Jeroen Massar <jeroen@unfix.org>\n"

#ifdef DEBUG
#define D(x) x
#else
#define D(x) {}
#endif

/* The timeout for queries */
/* as per RFC3810 MLDv2 "9.2.  Query Interval" */
#define ECMH_SUBSCRIPTION_TIMEOUT	125

/* Robustness Factor, per RFC3810 MLDv2 "9.1.  Robustness Variable" */
#define ECMH_ROBUSTNESS_FACTOR		2

#include "linklist.h"
#include "common.h"

/* Booleans */
#define false	0
#define true	(!false)
#define bool	int

#include "interfaces.h"
#include "groups.h"
#include "grpint.h"
#include "subscr.h"

/* Our configuration structure */
struct conf
{
	unsigned int		maxgroups;
	unsigned int		maxinterfaces;			/* The max number of interfaces the array can hold */
	struct intnode		*ints;				/* The interfaces we are watching */
	struct list		*groups;			/* The groups we are joined to */

	char			*upstream;			/* Upstream interface */
	unsigned int		upstream_id;			/* Interface ID of upstream interface */

	bool			daemonize;			/* To Daemonize or to not to Daemonize */
	bool			verbose;			/* Verbose Operation ? */
	bool			quit;				/* Global Quit signal */
#ifdef ECMH_SUPPORT_MLD2
	bool			mld1only;			/* Only MLDv1 ? */
	bool			mld2only;			/* Only MLDv2 ? */
#endif
	bool			promisc;			/* Make interfaces promisc? (To be sure to receive all MLD's) */
	
	uint8_t			*buffer;			/* Our buffer */
	unsigned int		bufferlen;			/* Length of the buffer */

#ifndef ECMH_BPF
	int			rawsocket;			/* Single RAW socket for sending and receiving everything */
#else
	bool			tunnelmode;			/* Intercept&handle proto-41 packets? */
	struct list		*locals;			/* Local devices that could have tunnels */
	fd_set			selectset;			/* Selectset */
	int			hifd;				/* Highest File Descriptor */
#endif

	FILE			*stat_file;			/* The file handle of ourdump file */
	time_t			stat_starttime;			/* When did we start */
	uint64_t		stat_packets_received;		/* Number of packets received */
	uint64_t		stat_packets_sent;		/* Number of packets forwarded */
	uint64_t		stat_bytes_received;		/* Number of bytes received */
	uint64_t		stat_bytes_sent;		/* Number of bytes forwarded */
	uint64_t		stat_icmp_received;		/* Number of ICMP's received */
	uint64_t		stat_icmp_sent;			/* Number of ICMP's sent */
	uint64_t		stat_hlim_exceeded;		/* Packets that where dropped due to hlim == 0 */
};

/* Global Stuff */
extern struct conf *g_conf;

