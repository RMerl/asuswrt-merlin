/*
 * LICENSE NOTICE.
 *
 * Use of the Microsoft Windows Rally Development Kit is covered under
 * the Microsoft Windows Rally Development Kit License Agreement,
 * which is provided within the Microsoft Windows Rally Development
 * Kit or at http://www.microsoft.com/whdc/rally/rallykit.mspx. If you
 * want a license from Microsoft to use the software in the Microsoft
 * Windows Rally Development Kit, you must (1) complete the designated
 * "licensee" information in the Windows Rally Development Kit License
 * Agreement, and (2) sign and return the Agreement AS IS to Microsoft
 * at the address provided in the Agreement.
 */

/*
 * Copyright (c) Microsoft Corporation 2005.  All rights reserved.
 * This software is provided with NO WARRANTY.
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/socket.h>
#include <features.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>

/* We use the POSIX.1e capability subsystem to drop all but
 * CAP_NET_ADMIN rights */
//#define HAVE_CAPABILITIES
#ifdef HAVE_CAPABILITIES
# include <sys/capability.h>
#endif

/* Do you have wireless extensions available? (most modern kernels do) */
#define HAVE_WIRELESS

#ifdef HAVE_WIRELESS
  /* for get access-point address (BSSID) and infrastructure mode */
# include <linux/wireless.h>
#else /* ! HAVE_WIRELESS */
  /* still want struct ifreq and friends */
# include <net/if.h>
#endif /* ! HAVE_WIRELESS */


/* for uname: */
#include <sys/utsname.h>

#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <errno.h>

#include "globals.h"

#include "packetio.h"

/* helper functions */

/* Convert from name "interface" to its index, or die on error. */
static int
if_get_index(int sock, char *interface)
{
    struct ifreq req;
    int ret;

    memset(&req, 0, sizeof(req));
    strncpy(req.ifr_name, interface, sizeof(req.ifr_name));
    ret = ioctl(sock, SIOCGIFINDEX, &req);
    if (ret != 0)
	die("if_get_index: for interface %s: %s\n",
	    interface, strerror(errno));

    return req.ifr_ifindex;
}


osl_t *
osl_init(void)
{
    osl_t *osl;

    osl = xmalloc(sizeof(*osl));
    osl->sock = -1;
    osl->arpsock = -1;
    osl->arp_enabled = FALSE;
    return osl;
}



/* pidfile maintenance: this is not locking (there's plenty of scope
 * for races here!) but it should stop most accidental runs of two
 * lld2d instances on the same interface.  We open the pidfile for
 * read: if it exists and the named pid is running we abort ourselves.
 * Otherwise we reopen the pidfile for write and log our pid to it. */
void
osl_write_pidfile(osl_t *osl)
{
    char pidfile[80];
    char pidbuf[16];
    int fd;
    int ret;

    snprintf(pidfile, sizeof(pidfile), "/var/run/lld2d-ct-%.10s.pid", osl->interface);
    fd = open(pidfile, O_RDONLY);
    if (fd < 0)
    {
	if (errno != ENOENT)
	    die("osl_write_pidfile: opening pidfile %s for read: %s\n",
		pidfile, strerror(errno));
	/* ENOENT is good: the pidfile doesn't exist */
    }
    else
    {
	/* the pidfile exists: read it and check whether the named pid
	   is still around */
	int pid;
	char *end;

	ret = read(fd, pidbuf, sizeof(pidbuf));
	if (ret < 0)
	    die("osl_write_pidfile: read of pre-existing %s failed: %s\n",
		pidfile, strerror(errno));
	pid = strtol(pidbuf, &end, 10);
	if (*end != '\0' && *end != '\n')
	    die("osl_write_pidfile: couldn't parse \"%s\" as a pid (from file %s); "
		"aborting\n", pidbuf, pidfile);
	ret = kill(pid, 0); /* try sending signal 0 to the pid to check it exists */
	if (ret == 0)
	    die("osl_write_pidfile: %s contains pid %d which is still running; aborting\n",
		pidfile, pid);
	/* pid doesn't exist, looks like we can proceed */
	close(fd);
    }

    /* re-open pidfile for write, possibly creating it */
    fd = open(pidfile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd < 0)
	die("osl_write_pidfile: open %s for write/create: %s\n", pidfile, strerror(errno));
    snprintf(pidbuf, sizeof(pidbuf), "%d\n", getpid());
    ret = write(fd, pidbuf, strlen(pidbuf));
    if (ret < 0)
	die("osl_write_pidfile: writing my PID to lockfile %s: %s\n",
	    pidfile, strerror(errno));
    close(fd);
}


/* Open "interface", and add packetio_recv_handler(state) as the IO
 * event handler for its packets (or die on failure).  If possible,
 * the OSL should try to set the OS to filter packets so just frames
 * with ethertype == topology protocol are received, but if not the
 * packetio_recv_handler will filter appropriately, so providing more
 * frames than necessary is safe. */
void
osl_interface_open(osl_t *osl, char *interface, void *state)
{
    struct sockaddr_ll addr;
    int ret;

    osl->sock = socket(PF_PACKET, SOCK_RAW, TOPO_ETHERTYPE);
    if (osl->sock < 0)
	die("osl_interface_open: open %s failed: %s\n",
	    interface, strerror(errno));

    osl->interface = interface;

    /* perhaps check interface flags indicate it is up? */

    /* set filter to only topology frames on this one interface */
    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = TOPO_ETHERTYPE;
    addr.sll_ifindex = if_get_index(osl->sock, interface);
    DEBUG({printf("binding raw socket (index= %d, fd=%d) on %s to TOPO_ETHERTYPE\n", addr.sll_ifindex, osl->sock, osl->interface);})
    ret = bind(osl->sock, (struct sockaddr*)&addr, sizeof(addr));
    if (ret != 0)
	die("osl_interface_open: binding to interface %s (index %d): %s\n",
	    osl->interface, addr.sll_ifindex, strerror(errno));

    event_add_io(osl->sock, packetio_recv_handler, state);

    /* create the ARP socket here too, while we still have enough rights */
    osl->arpsock = socket(PF_PACKET, SOCK_RAW, TOPO_ARP_ETHERTYPE);
    if (osl->arpsock < 0)
	die("osl_interface_open: open ARP socket on %s failed: %s\n",
	    osl->interface, strerror(errno));
}


/* Permanently drop elevated privilleges. */
/* Actually, drop all but CAP_NET_ADMIN rights, so we can still enable
 * and disable promiscuous mode, and listen to ARPs. */
void
osl_drop_privs(osl_t *osl)
{
#ifdef HAVE_CAPABILITIES
    cap_t caps = cap_init();
    cap_value_t netadmin[] = {CAP_NET_ADMIN};

    if (!caps)
	die("osl_drop_privs: cap_init failed: %s\n", strerror(errno));
    if (cap_set_flag(caps, CAP_PERMITTED, 1, netadmin, CAP_SET) < 0)
	die("osl_drop_privs: cap_set_flag (permitted) %s\n", strerror(errno));
    if (cap_set_flag(caps, CAP_EFFECTIVE, 1, netadmin, CAP_SET) < 0)
	die("osl_drop_privs: cap_set_flag (effective): %s\n", strerror(errno));
    if (cap_set_proc(caps) < 0)
	die("osl_drop_privs: cap_set_proc: %s\n", strerror(errno));
    cap_free(caps);
#endif
}


/* Turn promiscuous mode on or off */
void
osl_set_promisc(osl_t *osl, bool_t promisc)
{
    struct ifreq req;
    int ret=0;

    /* we query to get the previous state of the flags so as not to
     * change any others by accident */
    memset(&req, 0, sizeof(req));
    strncpy(req.ifr_name, osl->interface, sizeof(req.ifr_name)-1);
    ret = ioctl(osl->sock, SIOCGIFFLAGS, &req);
    if (ret != 0)
	die("osl_set_promisc: couldn't get interface flags for %s: %s\n",
	    osl->interface, strerror(errno));

    /* now clear (and optionally set) the IFF_PROMISC bit */
    req.ifr_flags &= ~IFF_PROMISC;
    if (promisc)
	req.ifr_flags |= IFF_PROMISC;

    ret = ioctl(osl->sock, SIOCSIFFLAGS, &req);
    if (ret != 0)
	die("osl_set_promisc: couldn't set interface flags for %s: %s\n",
	    osl->interface, strerror(errno));
}



/* Return the Ethernet address for "interface", or FALSE on error. */
static bool_t
get_hwaddr(const char *interface, /*OUT*/ etheraddr_t *addr,
	   bool_t expect_ethernet)
{
    int rqfd;
    struct ifreq req;
    int ret;

    memset(&req, 0, sizeof(req));
    strncpy(req.ifr_name, interface, sizeof(req.ifr_name));
    rqfd = socket(AF_INET,SOCK_DGRAM,0);
    if (rqfd<0)
    {
        warn("get_hwaddr: FAILED creating request socket for \'%s\' : %s\n",interface,strerror(errno));
        return FALSE;
    }
    ret = ioctl(rqfd, SIOCGIFHWADDR, &req);
    if (ret < 0)
    {
	warn("get_hwaddr(%d,%s): FAILED : %s\n", rqfd, interface, strerror(errno));
	return FALSE;
    }
    close(rqfd);
    if (req.ifr_hwaddr.sa_family != ARPHRD_ETHER)
    {
	if (expect_ethernet)
	    warn("get_hwaddr: was expecting addr type to be Ether, not type %d\n",
		 req.ifr_hwaddr.sa_family);
	return FALSE;
    }

    memcpy(addr, req.ifr_hwaddr.sa_data, sizeof(*addr));
    return TRUE;
}

/* Return the Ethernet address for socket "sock", or die. */
void
osl_get_hwaddr(osl_t *osl, /*OUT*/ etheraddr_t *addr)
{
    if (!get_hwaddr(osl->interface, addr, TRUE))
	die("osl_get_hw_addr: expected an ethernet address on our interface\n");
}


ssize_t
osl_read(int fd, void *buf, size_t count)
{
    return read(fd, buf, count);
}

ssize_t
osl_write(osl_t *osl, const void *buf, size_t count)
{
    return write(osl->sock, buf, count);
}



/* TRUE if x is less than y (lexographically) */
static bool_t
etheraddr_lt(const etheraddr_t *x, const etheraddr_t *y)
{
    int i;

    for (i=0; i<6; i++)
    {
	if (x->a[i] > y->a[i])
	    return FALSE;
	else if (x->a[i] < y->a[i])
	    return TRUE;
    }

    return FALSE; /* they're equal */
}

/* Find hardware address of "interface" and if it's lower than
 * "lowest", update lowest. */
static void
pick_lowest_address(const char *interface, void *state)
{
    etheraddr_t *lowest = state;
    etheraddr_t candidate;

    /* query the interface's hardware address */
    if (!get_hwaddr(interface, &candidate, FALSE))
	return; /* failure; just ignore (maybe it's not an Ethernet NIC, eg loopback) */

    if (etheraddr_lt(&candidate, lowest))
	*lowest = candidate;

    uint8_t *bytes = (uint8_t*)state;
}

typedef void (*foreach_interface_fn)(const char *interface, void *state);

/* enumerate all interfaces on this host via /proc/net/dev */
char buf[160];
static bool_t
foreach_interface(foreach_interface_fn fn, void *state)
{
    char *p, *colon, *name;
    FILE *netdev;

/* Note: When I dropped this on the WRT54GS version 4, the following fopen() hung the box... who knows why? */
/* the workaround involves keeping the descriptor open from before the first select in the event loop.      */

#if CAN_FOPEN_IN_SELECT_LOOP
    netdev = fopen("/proc/net/dev", "r");
#else
    netdev = g_procnetdev;
#endif
    if (!netdev)
    {
	warn("foreach_interface: open(\"/proc/net/dev\") for read failed: %s; "
	     "not able to determine hostId, so no support for multiple NICs.\n",
	     strerror(errno));
	return FALSE;
    }

    /* skip first 2 lines (header) */
    fgets(buf, sizeof(buf), netdev);
    fgets(buf, sizeof(buf), netdev);

    /* interface name is non-whitespace up to the colon (but a colon
     * might be part of a virtual interface eg eth0:0 */
    while (fgets(buf, sizeof(buf), netdev))
    {
	name = buf;
	while (isspace(*name))
	    name++;
	colon = strchr(name, ':');
	if (!colon)
	{
	    warn("foreach_interface: parse error reading /proc/net/dev: missing colon\n");
	    fclose(netdev);
	    return FALSE;
	}

	/* see if there's an optional ":DIGITS" virtual interface here */
	p = colon+1;
	while (isdigit(*p))
	    p++;
	if (*p == ':')
	    *p = '\0'; /* virtual interface name */
	else
	    *colon = '\0'; /* normal interface name */

	fn(name, state);
    }

    if (ferror(netdev))
    {
	warn("foreach_interface: error during reading of /proc/net/dev\n");
	fclose(netdev);
	return FALSE;
    }

#if CAN_FOPEN_IN_SELECT_LOOP
    fclose(netdev);
#endif
    return TRUE;
}


/* Recipe from section 1.7 of the Unix Programming FAQ */
/* http://www.erlenstar.demon.co.uk/unix/faq_toc.html */
void
osl_become_daemon(osl_t *osl)
{
    /* 1) fork */
    pid_t pid = fork();
    if (pid == -1)
	die("osl_become_daemon: fork to drop process group leadership failed: %s\n",
	    strerror(errno));
    if (pid != 0)
	exit(0); /* parent exits; child continues */

    /* we are now guaranteed not to be a process group leader */

    /* 2) setsid() */
    pid = setsid();
    if (pid == -1)
	die("osl_become_daemon: setsid failed?!: %s\n", strerror(errno));

    /* we are now the new group leader, and have successfully
     * jettisonned our controlling terminal - hooray! */
    
    /* 3) fork again */
    pid = fork();
    if (pid == -1)
	die("osl_become_daemon: fork to lose group leadership (again) failed: %s\n",
	    strerror(errno));
    if (pid != 0)
	exit(0); /* parent exits; child continues */

    /* we now have no controlling terminal, and are not a group leader
       (and thus have no way of acquiring a controlling terminal). */

    /* 4) chdir("/") */
    if (chdir("/") != 0)
	die("osl_become_daemon: chdir(\"/\") failed: %s\n", strerror(errno));
    /* this allows admins to umount filesystems etc since we're not
     * keeping them busy.*/

    /* 5) umask(0) */
    umask(0); /* if we create any files, we specify full mode bits */

    /* 6) close fds */

    /* 7) re-establish stdin/out/err to logfiles or /dev/console as needed */
}


/********************** T L V _ G E T _ F N s ******************************/

/* Return a 6-byte identifier which will be unique and consistent for
 * this host, across all of its interfaces.  Here we enumerate all the
 * interfaces that are up and have assigned Ethernet addresses, and
 * pick the lowest of them to represent this host.  */
int
get_hostid(void *data)
{
/*    TLVDEF( etheraddr_t, hostid,              ,   1,  Access_unset ) */

    etheraddr_t *hid = (etheraddr_t*) data;

    *hid = Etheraddr_broadcast;

    if (!foreach_interface(pick_lowest_address, hid))
    {
	if (TRACE(TRC_TLVINFO))
	    dbgprintf("get_hostid(): FAILED picking lowest address.\n");
	return TLV_GET_FAILED;
    }
    if (ETHERADDR_EQUALS(hid, &Etheraddr_broadcast))
    {
	if (TRACE(TRC_TLVINFO))
	    dbgprintf("get_hostid(): FAILED, because chosen hostid = broadcast address\n");
	return TLV_GET_FAILED;
    } else {
	if (TRACE(TRC_TLVINFO))
	    dbgprintf("get_hostid: hostid=" ETHERADDR_FMT "\n", ETHERADDR_PRINT(hid));
	return TLV_GET_SUCCEEDED;
    }
}


int
get_net_flags(void *data)
{
/*    TLVDEF( uint32_t,         net_flags,           ,   2,  Access_unset ) */

    uint32_t* nf = (uint32_t*) data;

#define fLP  0x08000000         // Looping back outbound packets, if set
#define fMW  0x10000000         // Has management web page accessible via HTTP, if set
#define fFD  0x20000000         // Full Duplex if set
#define fNX  0x40000000         // NAT private-side if set
#define fNP  0x80000000         // NAT public-side if set

    /* If your device has a management page at the url
            http://<device-ip-address>/
       then use the fMW flag, otherwise, remove it */
    *nf = htonl(fFD | fNX | fMW);

    return TLV_GET_SUCCEEDED;
}


int
get_physical_medium(void *data)
{
/*    TLVDEF( uint32_t,         physical_medium,     ,   3,  Access_unset ) */

    uint32_t* pm = (uint32_t*) data;

    /* Since this interface is bridged, we should actually return two responses,
     * one for the wireless, and one for the ethernet switch. It's probably OK to
     * just say "ethernet" though... */
//!!    *pm = htonl(71);	// "wireless"
    *pm = htonl(6);	// "ethernet"


    return TLV_GET_SUCCEEDED;
}

/* Note that most of the code in this block is not pertinent to the WRT, and has not been tested. In particular,
 * there may be many cases of network byte order not being observed.... */

#ifdef HAVE_WIRELESS

/* Return TRUE on successful query, setting "wireless_mode" to:
 *  0 = ad-hoc mode
 *  1 = infrastructure mode ie ESS (Extended Service Set) mode
 *  2 = auto/unknown (device may auto-switch between ad-hoc and infrastructure modes)
 */
int
get_wireless_mode(void *data)
{
/*    TLVDEF( uint32_t,         wireless_mode,       ,   4,  Access_unset ) */

    uint32_t* wl_mode = (uint32_t*) data;

    int rqfd;
    struct iwreq req;
    int ret;

    memset(&req, 0, sizeof(req));
    strncpy(req.ifr_ifrn.ifrn_name, g_osl->interface, sizeof(req.ifr_ifrn.ifrn_name));
//*/printf("get_wireless_mode: requesting addr for interface \'%s\'\n",g_osl->interface);
    rqfd = socket(AF_INET,SOCK_DGRAM,0);
    if (rqfd<0)
    {
        warn("get_wireless_mode: FAILED creating request socket for \'%s\' : %s\n",g_osl->interface,strerror(errno));
        return FALSE;
    }
    ret = ioctl(rqfd, SIOCGIWMODE, &req);
    close(rqfd);
    if (ret != 0)
    {
	/* We don't whine about "Operation Not Supported" since that
	 * just means the interface does not have wireless extensions. */
	if (errno != EOPNOTSUPP)
	    warn("get_wireless_mode: get associated Access Point MAC failed: %s\n",
		 strerror(errno));
	return TLV_GET_FAILED;
    }

    switch (req.u.mode) {
    case IW_MODE_AUTO:
	*wl_mode = 2;
        break;

    case IW_MODE_ADHOC:
	*wl_mode = 0;
        break;

    case IW_MODE_INFRA:
	*wl_mode = 1;
        break;

    default:
	/* this is a pretty complex wireless device (probably acting
	 * as an AP); we can't express its features just like this, so
	 * we'll not return a wireless mode TLV. */
        IF_TRACED(TRC_TLVINFO)
            printf("get_wireless_mode(): mode (%d) unrecognizable - FAILING the get...\n", req.u.mode);
        END_TRACE
	return TLV_GET_FAILED;
    }
    IF_TRACED(TRC_TLVINFO)
        printf("get_wireless_mode(): wireless_mode=%d\n", *wl_mode);
    END_TRACE

    return TLV_GET_SUCCEEDED;
}


/* If the interface is 802.11 wireless in infrastructure mode, and it
 * has successfully associated to an AP (Access Point) then it will
 * have a BSSID (Basic Service Set Identifier), which is usually the
 * AP's MAC address. 
 * Of course, this code resides in an AP, so this is actually quite
 * meaningless. */
int
get_bssid(void *data)
{
/*    TLVDEF( etheraddr_t, bssid,               ,   5,  Access_unset ) */

#ifdef NOT_ACCESSPOINT
    etheraddr_t* bssid = (etheraddr_t*) data;

    struct iwreq req;
    int ret;

    memset(&req, 0, sizeof(req));
    strncpy(req.ifr_ifrn.ifrn_name, g_osl->interface, sizeof(req.ifr_ifrn.ifrn_name));
    ret = ioctl(g_osl->sock, SIOCGIWAP, &req);
    if (ret != 0)
    {
	/* We don't whine about "Operation Not Supported" since that
	 * just means the interface does not have wireless extensions. */
	if (errno != EOPNOTSUPP)
	    warn("get_bssid: get associated Access Point MAC failed: %s\n",
		 strerror(errno));
        return TLV_GET_FAILED;
    }

    if (req.u.ap_addr.sa_family != ARPHRD_ETHER)
    {
	warn("get_bssid: expecting Ethernet address back, not sa_family=%d\n",
	     req.u.ap_addr.sa_family);
        return TLV_GET_FAILED;
    }

    memcpy(bssid, req.u.ap_addr.sa_data, sizeof(etheraddr_t));

    IF_TRACED(TRC_TLVINFO)
        printf("get_bssid(): bssid=" ETHERADDR_FMT "\n", ETHERADDR_PRINT(bssid));
    END_TRACE

    return TLV_GET_SUCCEEDED;
#else
    return TLV_GET_FAILED;	// This is an AP - no associated BSSID //
#endif
}


int
get_ssid(void *data)
{
/*    TLVDEF( ssid_t,           ssid,                ,   6,  Access_unset ) */

//    ssid_t* ssid = (ssid_t*) data;

    return TLV_GET_FAILED;
}

#endif /* HAVE_WIRELESS */


int
get_ipv4addr(void *data)
{
/*    TLVDEF( ipv4addr_t,       ipv4addr,            ,   7,  Access_unset ) */

    int          rqfd, ret;
    struct ifreq req;
    ipv4addr_t*  ipv4addr = (ipv4addr_t*) data;
    char         dflt_if[] = {"br0"};
    char        *interface = g_interface;

    if (interface == NULL) interface = dflt_if;
    memset(&req, 0, sizeof(req));
    strncpy(req.ifr_name, interface, sizeof(req.ifr_name));
    rqfd = socket(AF_INET,SOCK_DGRAM,0);
    if (rqfd<0)
    {
        warn("get_ipv4addr: FAILED creating request socket for \'%s\' : %s\n",interface,strerror(errno));
        return TLV_GET_FAILED;
    }
    ret = ioctl(rqfd, SIOCGIFADDR, &req);
    if (ret < 0)
    {
	warn("get_ipv4addr(%d,%s): FAILED : %s\n", rqfd, interface, strerror(errno));
	return TLV_GET_FAILED;
    }
    close(rqfd);
    IF_DEBUG
        /* Interestingly, the ioctl to get the ipv4 address returns that
           address offset by 2 bytes into the buf. Leaving this in case
           one of the ports results in a different offset... */
        unsigned char *d;
        d = (unsigned char*)req.ifr_addr.sa_data;
        /* Dump out the whole 14-byte buffer */
        printf("get_ipv4addr: sa_data = 0x%02x-0x%02x-0x%02x-0x%02x-0x%02x-0x%02x-0x%02x-0x%02x-0x%02x-0x%02x-0x%02x-0x%02x-0x%02x-0x%02x\n",d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7],d[8],d[9],d[10],d[11],d[12],d[13]);
    END_DEBUG

    memcpy(ipv4addr,&req.ifr_addr.sa_data[2],sizeof(uint32_t));
    IF_DEBUG
        printf("get_ipv4addr: returning %d.%d.%d.%d as ipv4addr\n", \
                *((uint8_t*)ipv4addr),*((uint8_t*)ipv4addr +1), \
                *((uint8_t*)ipv4addr +2),*((uint8_t*)ipv4addr +3));
    END_DEBUG

    return TLV_GET_SUCCEEDED;
}


int
get_ipv6addr(void *data)
{
/*    TLVDEF( ipv6addr_t,       ipv6addr,            ,   8,  Access_unset ) */

    ipv6addr_t* ipv6addr = (ipv6addr_t*) data;

    return TLV_GET_FAILED;
}


int
get_max_op_rate(void *data)
{
/*    TLVDEF( uint16_t,         max_op_rate,         ,   9,  Access_unset ) units of 0.5 Mbs */

    uint16_t* morate = (uint16_t*) data;

    *morate = htons(108);	/* 54 Mbs / 0.5 Mbs = 108 */

    return TLV_GET_SUCCEEDED;
}


int
get_tsc_ticks_per_sec(void *data)
{
/*    TLVDEF( uint64_t,         tsc_ticks_per_sec,   , 0xA,  Access_unset ) */

    uint64_t   ticks;
    uint8_t   *srcticks = (uint8_t*)(&ticks) + sizeof(uint64_t) - 1;
    uint8_t   *dstticks = (uint8_t*) data;

    int        i;

    ticks = (uint64_t) 0x999e36LL;	// 10M (100ns) ticks - YMMV

    /* do our own htonll() here.... */
    for (i=0; i<sizeof(uint64_t); i++)   *dstticks++ = *srcticks--;

    return TLV_GET_SUCCEEDED;
}


int
get_link_speed(void *data)
{
/*    TLVDEF( uint32_t,         link_speed,          , 0xC,  Access_unset ) // 100bps increments */

    uint32_t* speed = (uint32_t*) data;

    /* Since this is a bridged pair of interfaces (br0 = eth1 + wl0), I am returning the
     * wireless speed, which is the lowest of the upper limits on the two interfaces... */

    *speed = htonl(540000); // 54Mbit wireless... (540k x 100 = 54Mbs)

    return TLV_GET_SUCCEEDED;
}


/* Unless this box is used as a wireless client, RSSI is meaningless - which client should be measured? */
int
get_rssi(void *data)
{
/*    TLVDEF( uint32_t,         rssi,                , 0xD,  Access_unset ) */

    uint32_t* rssi = (uint32_t*) data;

    return TLV_GET_FAILED;
}


int
get_icon_image(void *data)
{
/*    TLVDEF( icon_file_t,      icon_image,   , 0xE )	// Usually LTLV style (Windows .ico file > 1k long) */

    icon_file_t* icon = (icon_file_t*) data;
    int          fd;

    /* The configuration file (lld2d.conf, in /etc) may have specified an icon file, which will be sent as
     * a TLV if it is smaller than the lesser of 1k and whatever space is left in the send-buffer... Such a
     * determination will be made at the time of the write, tho....
     *
     * In main.c, we read the config file, and established a value for the iconfile_path in "g_icon_path".
     * This path-value ( a char* ) MAY be NULL, if no path was configured correctly.
     * Here, we test that path, opening a file descriptor if it exists, and save its size for the writer-routine. */

    /* Close any still-open fd to a previously "gotten" iconfile */
    if (icon->fd_icon != -1 && icon->sz_iconfile != 0)
        close(icon->fd_icon);

    /* set defaults */
    icon->sz_iconfile = 0;
    icon->fd_icon = -1;

    if (g_icon_path == NULL || g_icon_path[0] == 0)
    {
        return TLV_GET_FAILED;	// No iconfile was correctly declared. This may NOT be an error!
    }

    fd = open(g_icon_path, O_RDONLY);
    if (fd < 0)
    {
        warn("get_icon_image: reading iconfile %s FAILED : %s\n",
		g_icon_path, strerror(errno));
        return TLV_GET_FAILED;
    } else {
        /* size the file, for the writer to determine if it needs to force an LTLV */
        struct stat  filestats;
        int          statstat;

        statstat = fstat( fd, &filestats );
        if (statstat < 0)
        {
            warn("get_icon_image: stat-ing iconfile %s FAILED : %s\n",
                  g_icon_path, strerror(errno));
            return TLV_GET_FAILED;
        } else {
            /* Finally! SUCCESS! */
            icon->sz_iconfile = filestats.st_size;
            icon->fd_icon = fd;
            IF_TRACED(TRC_TLVINFO)
                printf("get_icon_image: stat\'ing iconfile %s returned length=%d\n",g_icon_path, icon->sz_iconfile);
            END_TRACE
        }
    }
    return TLV_GET_SUCCEEDED;
}


int
get_machine_name(void *data)
{
/*    TLVDEF( ucs2char_t,       machine_name,    [16], 0xF,  Access_unset ) */

    ucs2char_t* name = (ucs2char_t*) data;

    int ret;
    struct utsname unamebuf;

    /* use uname() to get the system's hostname */
    ret = uname(&unamebuf);
    if (ret != -1)
    {
	/* nuke any '.' in what should be a nodename, not a FQDN */
	char *p = strchr(unamebuf.nodename, '.');
	if (p)
	    *p = '\0';

        size_t namelen = strlen(unamebuf.nodename);

        /* because I am a fool, and can't remember how to refer to the sizeof a structure
         * element directly, there is an unnecessary declaration here... */
        tlv_info_t* fool;
	util_copy_ascii_to_ucs2(name, sizeof(fool->machine_name), unamebuf.nodename);
        IF_TRACED(TRC_TLVINFO)
            dbgprintf("get_machine_name(): space= %d, len=%d, machine name = %s\n",
                       sizeof(fool->machine_name), namelen, unamebuf.nodename);
        END_TRACE
        return TLV_GET_SUCCEEDED;
    }

    IF_TRACED(TRC_TLVINFO)
        dbgprintf("get_machine_name(): uname() FAILED, returning %d\n", ret);
    END_TRACE

    return TLV_GET_FAILED;
}


int
get_support_info(void *data)
{
/*    TLVDEF( ucs2char_t,       support_info,    [32], 0x10, Access_unset ) // RLS: was "contact_info" */

    ucs2char_t * support = (ucs2char_t*) data;

    return TLV_GET_FAILED;
}


int
get_friendly_name(void *data)
{
/*    TLVDEF( ucs2char_t,       friendly_name,   [32], 0x11, Access_unset ) */

    ucs2char_t * fname = (ucs2char_t*) data;

    return TLV_GET_FAILED;
}


int
get_upnp_uuid(void *data)
{
/*    TLVDEF( uuid_t,           upnp_uuid,           , 0x12, Access_unset ) // 16 bytes long */

//    uuid_t* uuid = (uuid_t*) data;

    return TLV_GET_FAILED;
}


int
get_hw_id(void *data)
{
/*    TLVDEF( ucs2char_t,       hw_id,          [200], 0x13, Access_unset ) // 400 bytes long, max */

    ucs2char_t * hwid = (ucs2char_t*) data;

    return TLV_GET_FAILED;
}


int
get_qos_flags(void *data)
{
/*    TLVDEF( uint32_t,         qos_flags,           , 0x14, Access_unset ) */

    int32_t* qosf = (int32_t*) data;

#define  TAG8P	0x20000000      // supports 802.1p priority tagging
#define  TAG8Q  0x40000000      // supports 802.1q VLAN tagging
#define  QW     0x80000000      // has Microsoft qWave service implemented; this is not LLTD QoS extensions

    *qosf = htonl(TAG8P);

    return TLV_GET_SUCCEEDED;
}


int
get_wl_physical_medium(void *data)
{
/*    TLVDEF( uint8_t,          wl_physical_medium,  , 0x15, Access_unset ) */

    uint8_t* wpm = (uint8_t*) data;

    return TLV_GET_FAILED;
}


int
get_accesspt_assns(void *data)
{
/*    TLVDEF( ltlv_t,           accesspt_assns,      , 0x16, Access_unset ) // RLS: Large_TLV */

//    ltlv_t* assns = (ltlv_t*) data;

    return TLV_GET_FAILED;
}
