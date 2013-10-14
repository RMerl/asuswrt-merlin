// A broadcast packet repeater.  This packet repeater (currently designed for
// udp packets) will listen for broadcast packets.
// When it receives the packets, it will then re-broadcast the packet.
//
// Written by TheyCallMeLuc(at)yahoo.com.au
// I accept no responsiblity for the function of this program if you
// choose to use it.
// Modified for Poptop by Richard de Vroede <r.devroede@linvision.com>
// Ditto on the no responsibility.
//
// Rewritten by Norbert van Bolhuis <norbert@vanbolhuis.demon.nl> bcrelay (v1.0+)
// now supports/does:
// 1) Relaying from PPP (VPN tunnel) interfaces, hereby creating a virtual
//    LAN (w.r.t. UDP broadcasts) for VPN clients and ethernet PCs
//    belonging/matching the subnet portion of the VPN IP addresses.
//    So now broadcasting to/from all systems of the VPN has been implemented.
//    Note that bcrelay v0.5 only relayed from LAN to VPN clients.
//    It doesn't matter whether the systems on the VPN are on the LAN of the
//    VPN server or have a VPN/PPTP connection (over the internet) to the VPN
//    server. Broadcasts will always be relayed to/from all given interfaces. And
//    as long as the subnet portion of the IP addresses of the systems on the VPN
//    matches, the VPN server will be able to route properly. This means all
//    networking applications/games that rely on a UDP broadcast from one or
//    more PPP (VPN tunnel) interfaces will now see eachother and work over
//    the VPN.
//    Note that it depends on the networking application/game and whoever
//    acts as application/game server/host who is sending (UPD) broadcasts
//    and who is listening.
// 2) UDP broadcasts received on a PPP interface (VPN tunnel) sometimes
//    don't carry the VPN IP address which pptpd provisioned. I've seen
//    this happening on a WinXP SP1 box, especially when the application
//    responsible for the UDP broadcasts isn't aware of the PPP interface.
//    In this case it just uses the LAN IP src address for the IP src
//    address of the inner (GRE encapsulated) IP packet. This breaks
//    the "virtual LAN" and therefore bcrelay, as of this version, changes
//    the src IP address to the VPN IP address (which pptpd provisioned)
//    before relaying.
// 3) To avoid a UDP broadcast loop, bcrelay changes the IP TTL and the
//    UDP checksum (to 1 and 0 respectively) of the UDP broadcasts it relays.
//    No relaying will be done for UDP broadcasts with IP TTL=1 and UDP
//    checksum=0. Could also (mis)use IP identification for this, but IP TTL
//    and UDP chksum combination is expected to work fine.
// 4) bcrelay v0.5 forgot to update IP/UDP checksum when it changed the
//    dest. IP address (e.g. from 192.168.1.255 to 255.255.255.255).
//    Of course as of this version bcrelay always updates the IP/UDP
//    checksum since the IP TTL and src IP address will change also.
// 5) Enhanced the (syslog) debugging capabilities. By default bcrelay will
//    show what it is doing. Bcrelay will syslog the IP interfaces it tries
//    to read/relay UDP broadcasts from/to. These interfaces are called
//    the 'active interfaces', bcrelay will syslog the initial set of active
//    interfaces and whenever the set changes. Currently there is no difference
//    between an incoming interface (given with -i) and an outgoing interface
//    (given with -o), so bcrelay won't show this attribute. Also, bcrelay will
//    syslog a successfully relayed UDP broadcast, including the UDP port numbers,
//    the incoming interface and the interface(s) to which it was successfully
//    relayed. The (new) -n option allows to suppress syslog tracing.
//    If -n is given, bcrelay shows/syslogs nothing, except fatal error
//    messages.
//
// This software is completely free.  You can use and/or modify this
// software to your hearts content.  You are free to redistribute it as
// long as it is accompanied with the source and my credit is included.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __linux__
#define _GNU_SOURCE 1           /* strdup() prototype, broken arpa/inet.h */
#endif

#ifdef __svr4__
#define __EXTENSIONS__ 1        /* strdup() prototype */
#endif

#ifdef __sgi__
#define _XOPEN_SOURCE 500       /* strdup() prototype */
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <time.h>
#include <sys/time.h>
#include <regex.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <dirent.h>

#ifdef __linux__
#include <linux/filter.h>
#endif

#include "defaults.h"
#include "our_syslog.h"
#include "our_getopt.h"

//#define VERSION "1.0"

/* uncomment if you compile this without poptop's configure script */
//#define HAVE_FORK

/*
 * Value-return macros to fields in the IP PDU header
 */
#define IP_IPPDU_IHL(ippdu)                (*(unsigned char *)(ippdu) & 0x0F)
#define IP_IPPDU_PROTO(ippdu)              (*((unsigned char *)(ippdu) + 9) & 0xFF)

/*
 * Pointer macros to fields in the IP PDU header
 */
#define IP_IPPDU_CHECKSUM_MSB_PTR(ippdu)   ((unsigned char *)(ippdu) + 10 )
#define IP_IPPDU_CHECKSUM_LSB_PTR(ippdu)   ((unsigned char *)(ippdu) + 11 )

/*
 * Pointer macros to fields in the UDP PDU header
 */
#define IP_UDPPDU_CHECKSUM_MSB_PTR(udppdu) ((unsigned char *)(udppdu) + 6 )
#define IP_UDPPDU_CHECKSUM_LSB_PTR(udppdu) ((unsigned char *)(udppdu) + 7 )

#define MAXIF 255               // Maximum interfaces to use
#define MAX_SELECT_WAIT 3       // Maximum time (in secs) to wait for input on the socket/interfaces
                                // A time-out triggers the discovery of new interfaces.
#define MAX_NODISCOVER_IFS 12   // Maximum time (in secs) to elaps before a discovery of new
                                // interfaces is triggered. Only when a packet keeps coming in
                                // (this prevents a select time-out) a variable initialized with
                                // this #define becomes 0 and a rediscovery of the interfaces is
                                // triggered.
#define MAX_IFLOGTOSTR 16

/* Local function prototypes */
static void showusage(char *prog);
static void showversion();
#ifndef HAVE_DAEMON
static void my_daemon(int argc, char **argv);
#endif
static void mainloop(int argc, char **argv);

struct packet {
  struct iphdr ip;
  struct udphdr udp;
  char data[ETHERMTU];
};


/*
 * struct that keeps track of the interfaces of the system
 * selected upon usage by bcrelay (with -i and -o option).
 * An array of this struct is returned by discoverActiveInterfaces.
 * This array is reset (filled from scratch) each time
 * discoverActiveInterfaces is called.
 */
struct iflist {
//Fix 3mar2003
  //char index;
  int index;
  u_int32_t bcast;
  char ifname[16+1];
  unsigned long ifaddr;
  unsigned long ifdstaddr;
  unsigned long flags1;
};

#define IFLIST_FLAGS1_IF_IS_ETH            0x00000001
#define IFLIST_FLAGS1_IF_IS_PPP            0x00000002
#define IFLIST_FLAGS1_IF_IS_UNKNOWN        0x00000004
#define IFLIST_FLAGS1_CHANGED_INNER_SADDR  0x00010000


/*
 * struct that keeps track of the socket fd's for every interface
 * that is in use (and thus present in iflist).
 * Two permanent arrays of this struct are used, one for the
 * previous/old list and one for the current list.
 */
struct ifsnr {
  int sock_nr;
  int ifindex;
};

static void copy_ifsnr(struct ifsnr *from, struct ifsnr *to);
static int find_sock_nr(struct ifsnr *l, int ifidx);
struct iflist *discoverActiveInterfaces(void);
void ip_update_checksum(unsigned char *ippdu);
static char *IpProtToString( unsigned char prot );
static char *iflistToString( struct iflist *ifp );
static char *iflistLogIToString( struct iflist *ifp, int idx, struct ifsnr *ifnr );
static char *iflistLogRToString( struct iflist *ifp, int idx, struct ifsnr *ifnr );
static void bind_to_iface(int fd, int ifindex);

/*
 * This global variable determines whether NVBCR_PRINTF actually
 * displays something. While developping v1.0, NVBCR_PRINTF were
 * printf and a lot of tracing/logging/debugging was done with these.
 * Of course, by default these 'info' messages have been turned off
 * now. Enable by setting variable to 1. Note that output will only
 * appear in non-daemon mode (see also NVBCR_PRINTF).
 */
static int do_org_info_printfs = 0;

static int vnologging = 0;
static int vdaemon = 0;

#define NVBCR_PRINTF( args ) \
 if ((vdaemon == 0) && (do_org_info_printfs == 1)) printf args

static char interfaces[32];
static char log_interfaces[MAX_IFLOGTOSTR*MAXIF];
static char log_relayed[(MAX_IFLOGTOSTR-1)*MAXIF+81];
static char *ipsec = "";

static void showusage(char *prog)
{
        printf("\nBCrelay v%s\n\n", VERSION);
        printf("A broadcast packet repeater. This packet repeater (currently designed for udp packets) will listen\n");
        printf(" for broadcast packets. When it receives the packets, it will then re-broadcast the packet.\n\n");
        printf("Usage: %s [options], where options are:\n\n", prog);
        printf(" [-d] [--daemon]           Run as daemon.\n");
        printf(" [-h] [--help]             Displays this help message.\n");
        printf(" [-i] [--incoming <ifin>]  Defines from which interface broadcasts will be relayed.\n");
        printf(" [-n] [--nolog]            No logging/tracing to /var/log/messages.\n");
        printf(" [-o] [--outgoing <ifout>] Defines to which interface broadcasts will be relayed.\n");
        printf(" [-s] [--ipsec <arg>]      Defines an ipsec tunnel to be relayed to.\n");
        printf("                           Since ipsec tunnels terminate on the same interface, we need to define the broadcast\n");
        printf("                           address of the other end-point of the tunnel.  This is done as ipsec0:x.x.x.255\n");
        printf(" [-v] [--version]          Displays the BCrelay version number.\n");
        printf("\nLog messages and debugging go to syslog as DAEMON.\n\n");
        printf("\nInterfaces can be specified as regexpressions, ie. ppp[0-9]+\n\n");
}

static void showversion()
{
        printf("BCrelay v%s\n", VERSION);
}

#ifndef HAVE_DAEMON
static void my_daemon(int argc, char **argv)
{
        pid_t pid;
#ifndef BCRELAY_BIN
/* Need a smart way to locate the binary -rdv */
#define BCRELAY_BIN argv[0]
#endif
#ifndef HAVE_FORK
        /* need to use vfork - eg, uClinux */
        char **new_argv;
        extern char **environ;
        int minusd=0;
        int i;
        int fdr;

        /* Strip -d option */
        new_argv = malloc((argc) * sizeof(char **));
        fdr = open("/dev/null", O_RDONLY);
        new_argv[0] = BCRELAY_BIN;
        for (i = 1; argv[i] != NULL; i++) {
                if (fdr != 0) { dup2(fdr, 0); close(fdr); }
                if ( (strcmp(argv[i],"-d")) == 0 ) {
                        minusd=1;
                }
                if (minusd) {
                        new_argv[i] = argv[i+1];
                } else {
                        new_argv[i] = argv[i];
                }
        }
        syslog(LOG_DEBUG, "Option parse OK, re-execing as daemon");
        fflush(stderr);
        if ((pid = vfork()) == 0) {
                if (setsid() < 0) {                      /* shouldn't fail */
                        syslog(LOG_ERR, "Setsid failed!");
                        _exit(1);
                }
                chdir("/");
                umask(0);
                /* execve only returns on an error */
                execve(BCRELAY_BIN, new_argv, environ);
                exit(1);
        } else if (pid > 0) {
                syslog(LOG_DEBUG, "Success re-execing as daemon!");
                exit(0);
        } else {
                syslog(LOG_ERR, "Error vforking");
                exit(1);
        }
#else
    pid=fork();
    if (pid<0) { syslog(LOG_ERR, "Error forking"); _exit(1); }
    if (pid>0) { syslog(LOG_DEBUG, "Parent exits"); _exit(0); }
    if (pid==0) { syslog(LOG_DEBUG, "Running as child"); }
    /* child (daemon) continues */
    if (setsid() < 0) {                      /* shouldn't fail */
      syslog(LOG_ERR, "Setsid failed!");
      _exit(1);
    }
    chdir("/");
#endif
}
#endif

int main(int argc, char **argv) {
  regex_t preg;
  /* command line options */
  int c;
  char *ifout = "";
  char *ifin = "";

#ifndef BCRELAY
  fprintf(stderr,
	  "bcrelay: pptpd was compiled without support for bcrelay, exiting.\n"
	  "         run configure --with-bcrelay, make, and install.\n");
  exit(1);
#endif

        /* open a connection to the syslog daemon */
        openlog("bcrelay", LOG_PID, PPTP_FACILITY);

  while (1) {
                int option_index = 0;

                static struct option long_options[] =
                {
                        {"nolog", 0, 0, 0},
                        {"daemon", 0, 0, 0},
                        {"help", 0, 0, 0},
                        {"incoming", 1, 0, 0},
                        {"outgoing", 1, 0, 0},
                        {"ipsec", 1, 0, 0},
                        {"version", 0, 0, 0},
                        {0, 0, 0, 0}
                };

                c = getopt_long(argc, argv, "ndhi:o:s:v", long_options, &option_index);
                if (c == -1)
                        break;
                /* convert long options to short form */
                if (c == 0)
                        c = "ndhiosv"[option_index];
                switch (c) {
                case 'n':
                        vnologging = 1;
                        break;
                case 'd':
                        vdaemon = 1;
                        break;
                case 'h':
                        showusage(argv[0]);
                        return 0;
                case 'i':
                        ifin = strdup(optarg);
                        break;
                case 'o':
                        ifout = strdup(optarg);
                        break;
                case 's':
                        ipsec = strdup(optarg);
                        // Validate the ipsec parameters
                        regcomp(&preg, "ipsec[0-9]+:[0-9]+.[0-9]+.[0-9]+.255", REG_EXTENDED);
                        if (regexec(&preg, ipsec, 0, NULL, 0)) {
                                syslog(LOG_INFO,"Bad syntax: %s", ipsec);
                                fprintf(stderr, "\nBad syntax: %s\n", ipsec);
                                showusage(argv[0]);
                                return 0;
                        } else {
                                regfree(&preg);
                                break;
                        }
                case 'v':
                        showversion();
                        return 0;
                default:
                        showusage(argv[0]);
                        return 1;
                }
  }
  if (!(ifin && *ifin)) {
       syslog(LOG_INFO,"Incoming interface required!");
       showusage(argv[0]);
       _exit(1);
  }
  if (!(ifout && *ifout) && !(ipsec && *ipsec)) {
       syslog(LOG_INFO,"Listen-mode or outgoing or IPsec interface required!");
       showusage(argv[0]);
       _exit(1);
  } else {
        sprintf(interfaces,"%s|%s", ifin, ifout);
  }

  // If specified, become Daemon.
  if (vdaemon) {
#if HAVE_DAEMON
    closelog();
    freopen("/dev/null", "r", stdin);
    /* set noclose, we want stdout/stderr still attached if we can */
    daemon(0, 1);
    /* returns to child only */
    /* pid will have changed */
    openlog("bcrelay", LOG_PID, PPTP_FACILITY);
#else   /* !HAVE_DAEMON */
    my_daemon(argc, argv);
    /* returns to child if !HAVE_FORK
     * never returns if HAVE_FORK (re-execs without -d)
     */
#endif
  } else {
    syslog(LOG_INFO, "Running as child\n");
  }
  mainloop(argc,argv);
  _exit(0);
}

static void mainloop(int argc, char **argv)
{
  socklen_t salen = sizeof(struct sockaddr_ll);
  int i, rcg, j, no_discifs_cntr, ifs_change;
  int logstr_cntr;
  struct iflist *iflist = NULL;         // Initialised after the 1st packet
  struct sockaddr_ll sa;
  struct packet *ipp_p;
  char *udppdu; // FIXME: warning: pointer targets in assignment differ in signedness
  fd_set sock_set;
  struct timeval time_2_wait;
  static struct ifsnr old_ifsnr[MAXIF+1]; // Old iflist to socket fd's mapping list
  static struct ifsnr cur_ifsnr[MAXIF+1]; // Current iflist to socket fd's mapping list
  unsigned char buf[1518];
  char *logstr = "";

  no_discifs_cntr = MAX_NODISCOVER_IFS;
  ifs_change = 0;

  /*
   * Discover interfaces (initial set) and create a dedicated socket bound to the interface
   */
  memset(old_ifsnr, -1, sizeof(old_ifsnr));
  memset(cur_ifsnr, -1, sizeof(cur_ifsnr));
  iflist = discoverActiveInterfaces();
  for (i=0; iflist[i].index; ++i) {
    if ((cur_ifsnr[i].sock_nr = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP))) < 0) {
      syslog(LOG_ERR, "mainloop: Error, socket error! (rv=%d, errno=%d)", cur_ifsnr[i].sock_nr, errno);
      exit(1);
    }
    bind_to_iface(cur_ifsnr[i].sock_nr, iflist[i].index);
    cur_ifsnr[i].ifindex = iflist[i].index;
  }
  NVBCR_PRINTF(("Displaying INITIAL active interfaces..\n"));
  if (vnologging == 0) {
    logstr = log_interfaces;
    logstr_cntr = sprintf(logstr, "Initial active interfaces: ");
    logstr += logstr_cntr;
  }
  for (i = 0; iflist[i].index; i++)
  {
    NVBCR_PRINTF(("\t\tactive interface number: %d, if=(%s), sock_nr=%d\n", i, iflistToString(&(iflist[i])), cur_ifsnr[i].sock_nr));
    if (vnologging == 0) {
      logstr_cntr = sprintf(logstr, "%s ", iflistLogIToString(&(iflist[i]), i, &(cur_ifsnr[i])));
      logstr += logstr_cntr;
    }
  }
  if (vnologging == 0) syslog(LOG_INFO, "%s", log_interfaces);

  // Main loop
  while (1)
  {

    /* 
     * Check all (interface) sockets for incoming packets
     */
    FD_ZERO(&sock_set);
    for (i=0; iflist[i].index; ++i)
    {
      if (cur_ifsnr[i].sock_nr >= 0) {
        FD_SET(cur_ifsnr[i].sock_nr, &sock_set);
      }
    }

    /*
     * Don't wait more than MAX_SELECT_WAIT seconds
     */
    time_2_wait.tv_sec = MAX_SELECT_WAIT;
    time_2_wait.tv_usec = 0L;

    /* select on sockets */
    rcg = select(MAXIF, &sock_set, (fd_set *) NULL,(fd_set *) NULL, &time_2_wait);

    if (rcg < 0)
    {
      syslog(LOG_ERR, "Error, select error! (rv=%d, errno=%d)", rcg, errno);
      exit(1);
    }

    if (rcg == 0)
    {
      /* TimeOut, rediscover interfaces */
      NVBCR_PRINTF(("Select timeout, rediscover interfaces\n"));
      copy_ifsnr(cur_ifsnr, old_ifsnr);
      memset(cur_ifsnr, -1, sizeof(cur_ifsnr));
      iflist = discoverActiveInterfaces();
      /*
       * Build new cur_ifsnr list.
       * Make iflist[i] correspond with cur_ifsnr[i], so iflist[i].index == cur_ifsnr[i].ifindex
       * The old list (old_ifsnr) is used to compare.
       */
      for (i=0; iflist[i].index; ++i) {
        /* check to see if it is a NEW interface */
        int fsnr = find_sock_nr(old_ifsnr, iflist[i].index);
        if (fsnr == -1) {
          /* found new interface, open dedicated socket and bind it to the interface */
          if ((cur_ifsnr[i].sock_nr = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP))) < 0) {
            syslog(LOG_ERR, "mainloop: Error, socket error! (rv=%d, errno=%d)", cur_ifsnr[i].sock_nr, errno);
            exit(1);
          }
          bind_to_iface(cur_ifsnr[i].sock_nr, iflist[i].index);
          ifs_change = 1;
        }
        else
        {
          /*
           * not a new interface, socket already openen, interface already
           * bound. Update cur_ifsnr.
           */
          cur_ifsnr[i].sock_nr = fsnr;
        }
        cur_ifsnr[i].ifindex = iflist[i].index;
      }
      /* Close disappeared interfaces */
      for (i=0; i<MAXIF; ++i)
      {
        if ((old_ifsnr[i].sock_nr != -1) && (old_ifsnr[i].ifindex != -1) &&
            (find_sock_nr(cur_ifsnr, old_ifsnr[i].ifindex) == -1)) {
          NVBCR_PRINTF(("Closing an interface (it disappeared), namely: (s_nr=%d, ifidx=%d)\n", old_ifsnr[i].sock_nr, old_ifsnr[i].ifindex));
          close(old_ifsnr[i].sock_nr);
          old_ifsnr[i].sock_nr = -1;
          old_ifsnr[i].ifindex = -1;
          ifs_change = 1;
        }
      }
      if (ifs_change == 1)
      {
        NVBCR_PRINTF(("Active interface set changed --> displaying current active interfaces..\n"));
        if (vnologging == 0) {
          logstr = log_interfaces;
          logstr_cntr = sprintf(logstr, "Active interface set changed to: ");
          logstr += logstr_cntr;
        }
        for (i = 0; iflist[i].index; i++)
        {
          NVBCR_PRINTF(("\t\tactive interface number: %d, if=(%s), sock_nr=%d\n", i, iflistToString(&(iflist[i])), cur_ifsnr[i].sock_nr));
          if (vnologging == 0) {
            logstr_cntr = sprintf(logstr, "%s ", iflistLogIToString(&(iflist[i]), i, &(cur_ifsnr[i])));
            logstr += logstr_cntr;
          }
        }
        if (vnologging == 0) syslog(LOG_INFO, "%s", log_interfaces);
        ifs_change = 0;
      }
      continue;
    }

    if (rcg > 0)
    {
      /* rcg interfaces have pending input */
      for (i=0; ((iflist[i].index != 0) && (rcg > 0)); ++i)
      {
        if ((cur_ifsnr[i].sock_nr != -1) && (FD_ISSET(cur_ifsnr[i].sock_nr,&sock_set)))
        {
          /* Valid socket number and pending input, let's read */
          int rlen = read(cur_ifsnr[i].sock_nr, buf, sizeof(buf));
          ipp_p = (struct packet *)&(buf[0]);
          NVBCR_PRINTF(("IP_Packet=(tot_len=%d, id=%02x%02x, ttl=%d, prot=%s, src_ip=%d.%d.%d.%d, dst_ip=%d.%d.%d.%d) (on if: %d/%d) ", ntohs(ipp_p->ip.tot_len), (ntohs(ipp_p->ip.id))>>8, (ntohs(ipp_p->ip.id))&0x00ff, ipp_p->ip.ttl, IpProtToString(ipp_p->ip.protocol), (ntohl(ipp_p->ip.saddr))>>24, ((ntohl(ipp_p->ip.saddr))&0x00ff0000)>>16, ((ntohl(ipp_p->ip.saddr))&0x0000ff00)>>8, (ntohl(ipp_p->ip.saddr))&0x000000ff, (ntohl(ipp_p->ip.daddr))>>24, ((ntohl(ipp_p->ip.daddr))&0x00ff0000)>>16, ((ntohl(ipp_p->ip.daddr))&0x0000ff00)>>8, (ntohl(ipp_p->ip.daddr))&0x000000ff, i, iflist[i].index));
          rcg -= 1;

          if ( (ipp_p->ip.protocol == IPPROTO_UDP) &&
               (((ntohl(ipp_p->ip.daddr)) & 0x000000ff) == 0x000000ff) &&
               (ipp_p->ip.ttl != 1) &&
               (!((*IP_UDPPDU_CHECKSUM_MSB_PTR((unsigned char *)ipp_p+(4*ipp_p->ip.ihl)) == 0) &&
               (*IP_UDPPDU_CHECKSUM_LSB_PTR((unsigned char *)ipp_p+(4*ipp_p->ip.ihl)) == 0))) )
          {
            int nrsent;
            int ifindex_to_exclude = iflist[i].index;

            NVBCR_PRINTF(("is an UDP BROADCAST (dstPort=%d, srcPort=%d) (with TTL!=1 and UDP_CHKSUM!=0)\n\n",
                           ntohs(ipp_p->udp.dest), ntohs(ipp_p->udp.source)));
            if (vnologging == 0) {
              logstr = log_relayed;
              logstr_cntr = sprintf(logstr, "UDP_BroadCast(sp=%d,dp=%d) from: %s relayed to: ", ntohs(ipp_p->udp.source),
                                    ntohs(ipp_p->udp.dest), iflistLogRToString(&(iflist[i]), i, &(cur_ifsnr[i])));
              logstr += logstr_cntr;
            }

            /* going to relay a broadcast packet on all the other interfaces */
            for (j=0; iflist[j].index; ++j)
            {
              int prepare_ipp = 0; // Changing the incoming UDP broadcast needs to be done once

              if (iflist[j].index != ifindex_to_exclude)
              {
                NVBCR_PRINTF(("Going to sent UDP Broadcast on interface: %s, sock_nr=%d\n", iflistToString(&(iflist[j])), cur_ifsnr[j].sock_nr));

                memset(&sa, 0, salen);

                sa.sll_family          = AF_PACKET;
                sa.sll_ifindex = iflist[j].index; /* Must be the SIOCGIFINDEX number */
                // Set the outgoing hardware address to 1's.  True Broadcast
                sa.sll_addr[0] = sa.sll_addr[1] = sa.sll_addr[2] = sa.sll_addr[3] = 0xff;
                sa.sll_addr[4] = sa.sll_addr[5] = sa.sll_addr[6] = sa.sll_addr[7] = 0xff;
                sa.sll_halen = 6;

                /*
                 * htons(ETH_P_IP) is necessary otherwise sendto will
                 * succeed but no packet is actually sent on the wire (this
                 * was the case for PPP interfaces, for ETH interfaces an unknown
                 * LAN frame is sent if htons(ETH_P_IP) is not set as protocol).
                 */
                sa.sll_protocol = htons(ETH_P_IP); /* ETH_P_PPP_MP */

                if (prepare_ipp == 0) {
                  // change TimeToLive to 1, This is to avoid loops, bcrelay will *NOT* relay
                  // anything with TTL==1.
                  ipp_p->ip.ttl = 1;
  
                  // The CRC gets broken here when sending over ipsec tunnels but that
                  // should not matter as we reassemble the packet at the other end.
                  ipp_p->ip.daddr = iflist[j].bcast;
  
                  // check IP srcAddr (on some winXP boxes it is found to be
                  // different from the configured ppp address).
                  // Only do this for PPP interfaces.
                  if ((iflist[i].flags1 & IFLIST_FLAGS1_IF_IS_PPP) &&
                      (ntohl(ipp_p->ip.saddr) != iflist[i].ifdstaddr))
                  {
                    ipp_p->ip.saddr = htonl(iflist[i].ifdstaddr);
                    iflist[i].flags1 |= IFLIST_FLAGS1_CHANGED_INNER_SADDR;
                  }
  
                  // Update IP checkSum (TTL and src/dest IP Address might have changed)
                  ip_update_checksum((unsigned char *)ipp_p);
                  /* Disable upper layer checksum */
                  udppdu = (unsigned char *)ipp_p + (4 * ipp_p->ip.ihl);
                  *IP_UDPPDU_CHECKSUM_MSB_PTR(udppdu) = (unsigned char)0;
                  *IP_UDPPDU_CHECKSUM_LSB_PTR(udppdu) = (unsigned char)0;

                  prepare_ipp = 1;
                }

                /*
                 * The beauty of sending IP packets on a PACKET socket of type SOCK_DGRAM is that
                 * there is no need to concern about the physical/link layer header because it is
                 * filled in automatically (based on the contents of sa).
                 */
                if ((nrsent = sendto(cur_ifsnr[j].sock_nr, ipp_p, rlen, MSG_DONTWAIT|MSG_TRYHARD, (struct sockaddr *)&sa, salen)) < 0)
                {
		  if (errno == ENETDOWN) {
		    syslog(LOG_NOTICE, "ignored ENETDOWN from sendto(), a network interface was going down?");
		  } else if (errno == ENXIO) {
		    syslog(LOG_NOTICE, "ignored ENXIO from sendto(), a network interface went down?");
		  } else if (errno == ENOBUFS) {
		    syslog(LOG_NOTICE, "ignored ENOBUFS from sendto(), temporary shortage of buffer memory");
		  } else {
		    syslog(LOG_ERR, "mainloop: Error, sendto failed! (rv=%d, errno=%d)", nrsent, errno);
		    exit(1);
		  }
                }
                NVBCR_PRINTF(("Successfully relayed %d bytes \n", nrsent));
                if (vnologging == 0) {
                  logstr_cntr = sprintf(logstr, "%s ", iflistLogRToString(&(iflist[j]), j, &(cur_ifsnr[j])));
                  logstr += logstr_cntr;
                }
              }
            }
            if (vnologging == 0) syslog(LOG_INFO, "%s", log_relayed);
          } else {
            NVBCR_PRINTF(("is NOT an UDP BROADCAST (with TTL!=1 and UDP_CHKSUM!=0)\n\n"));
          }
        }
      }
      /*
       * Don't forget to discover new interfaces if we keep getting
       * incoming packets (on an already discovered interface).
       */
      if (no_discifs_cntr == 0)
      {
        no_discifs_cntr = MAX_NODISCOVER_IFS;

        /* no_discifs_cntr became 0, rediscover interfaces */
        NVBCR_PRINTF(("no_discifs_cntr became 0, rediscover interfaces\n"));
        copy_ifsnr(cur_ifsnr, old_ifsnr);
        memset(cur_ifsnr, -1, sizeof(cur_ifsnr));
        iflist = discoverActiveInterfaces();
        /*
         * Build new cur_ifsnr list.
         * Make iflist[i] correspond with cur_ifsnr[i], so iflist[i].index == cur_ifsnr[i].ifindex
         * The old list (old_ifsnr) is used to compare.
         */
        for (i=0; iflist[i].index; ++i) {
          /* check to see if it is a NEW interface */
          int fsnr = find_sock_nr(old_ifsnr, iflist[i].index);
          if (fsnr == -1) {
            /* found new interface, open dedicated socket and bind it to the interface */
            if ((cur_ifsnr[i].sock_nr = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP))) < 0) {
              syslog(LOG_ERR, "mainloop: Error, socket error! (rv=%d, errno=%d)", cur_ifsnr[i].sock_nr, errno);
              exit(1);
            }
            bind_to_iface(cur_ifsnr[i].sock_nr, iflist[i].index);
            ifs_change = 1;
          }
          else
          {
            /*
             * not a new interface, socket already openen, interface already
             * bound. Update cur_ifsnr.
             */
            cur_ifsnr[i].sock_nr = fsnr;
          }
          cur_ifsnr[i].ifindex = iflist[i].index;
        }
        /* Close disappeared interfaces */
        for (i=0; i<MAXIF; ++i)
        {
          if ((old_ifsnr[i].sock_nr != -1) && (old_ifsnr[i].ifindex != -1) &&
              (find_sock_nr(cur_ifsnr, old_ifsnr[i].ifindex) == -1)) {
            NVBCR_PRINTF(("Closing an interface (it disappeared), namely: (s_nr=%d, ifidx=%d)\n", old_ifsnr[i].sock_nr, old_ifsnr[i].ifindex));
            close(old_ifsnr[i].sock_nr);
            old_ifsnr[i].sock_nr = -1;
            old_ifsnr[i].ifindex = -1;
            ifs_change = 1;
          }
        }
        if (ifs_change == 1)
        {
          NVBCR_PRINTF(("Active interface set changed --> displaying current active interfaces..\n"));
          if (vnologging == 0) {
            logstr = log_interfaces;
            logstr_cntr = sprintf(logstr, "Active interface set changed to: ");
            logstr += logstr_cntr;
          }
          for (i = 0; iflist[i].index; i++)
          {
            NVBCR_PRINTF(("\t\tactive interface number: %d, if=(%s), sock_nr=%d\n", i, iflistToString(&(iflist[i])), cur_ifsnr[i].sock_nr));
            if (vnologging == 0) {
              logstr_cntr = sprintf(logstr, "%s ", iflistLogIToString(&(iflist[i]), i, &(cur_ifsnr[i])));
              logstr += logstr_cntr;
            }
          }
          if (vnologging == 0) syslog(LOG_INFO, "%s", log_interfaces);
          ifs_change = 0;
        }
      }
      else
      {
        no_discifs_cntr -= MAX_SELECT_WAIT;
      }
    }
  }
}

// Discover active interfaces
struct iflist *
discoverActiveInterfaces(void) {
  int s;
  static struct iflist iflist[MAXIF+1];         // Allow for MAXIF interfaces
  static struct ifconf ifs;
  int i, j, cntr = 0;
  regex_t preg;
  struct ifreq ifrflags, ifr;
  struct sockaddr_in *sin;

  /* Reset iflist */
  memset(iflist, 0, sizeof(iflist));
  /* Reset ifs */
  memset(&ifs, 0, sizeof(ifs));

  /*
   * Open general ethernet socket, only used to discover interfaces.
   */
  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    syslog(LOG_INFO,"bcrelay: Error creating socket");
    return iflist;
  }

  //regcomp(&preg, argv[1], REG_ICASE|REG_EXTENDED);
  regcomp(&preg, interfaces, REG_ICASE|REG_EXTENDED);
  ifs.ifc_len = MAXIF*sizeof(struct ifreq);
  ifs.ifc_req = malloc(ifs.ifc_len);
  ioctl(s, SIOCGIFCONF, &ifs);                  // Discover active interfaces
  for (i = 0; i * sizeof(struct ifreq) < ifs.ifc_len && cntr < MAXIF; i++)
  {
    if (regexec(&preg, ifs.ifc_req[i].ifr_name, 0, NULL, 0) == 0) {

      /*
       * Get interface flags and check status and type.
       * Only if interface is up it will be used.
       */
      memset(&ifrflags, 0, sizeof(ifrflags));
      strncpy(ifrflags.ifr_name, ifs.ifc_req[i].ifr_name, strlen(ifs.ifc_req[i].ifr_name));
      if (ioctl(s, SIOCGIFFLAGS, &ifrflags) < 0) {
        syslog(LOG_ERR, "discoverActiveInterfaces: Error, SIOCGIFFLAGS Failed! (errno=%d)", errno);
        exit(1);
      }

      if (ifrflags.ifr_flags & IFF_UP)
      {
        /*
         * Get interface index
         */
        ioctl(s, SIOCGIFINDEX, &ifs.ifc_req[i]);
//Fix 3mar2003
        //iflist[cntr].index = (char)ifs.ifc_req[i].ifr_ifindex;
        iflist[cntr].index = ifs.ifc_req[i].ifr_ifindex;

        /*
         * Get interface name
         */
        for (j=0; (j<sizeof(iflist[cntr].ifname) && j<strlen(ifs.ifc_req[i].ifr_ifrn.ifrn_name)); ++j)
                iflist[cntr].ifname[j] = ifs.ifc_req[i].ifr_ifrn.ifrn_name[j];
        iflist[cntr].ifname[j+1] = '\0';

        /*
         * Get local IP address 
         */
        memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_addr.sa_family = AF_INET;
        (void)strncpy(ifr.ifr_name, iflist[cntr].ifname, strlen(iflist[cntr].ifname)+1);
        if (ioctl(s, SIOCGIFADDR, (char *)&ifr) < 0) {
          syslog(LOG_ERR, "discoverActiveInterfaces: Error, SIOCGIFADDR Failed! (errno=%d)", errno);
          exit(1);
        }
        sin = (struct sockaddr_in *)&ifr.ifr_addr;
        iflist[cntr].ifaddr = ntohl(sin->sin_addr.s_addr);

        iflist[cntr].flags1 = 0;

        if (ifrflags.ifr_flags & IFF_POINTOPOINT) {
          /*
           * Get remote IP address (only for PPP interfaces)
           */
          memset(&ifr, 0, sizeof(ifr));
          ifr.ifr_addr.sa_family = AF_INET;
          (void)strncpy(ifr.ifr_name, iflist[cntr].ifname, strlen(iflist[cntr].ifname)+1);
          if (ioctl(s, SIOCGIFDSTADDR, (char *)&ifr) < 0) {
            syslog(LOG_ERR, "discoverActiveInterfaces: Error, SIOCGIFDSTADDR Failed! (errno=%d)", errno);
            exit(1);
          }
          sin = (struct sockaddr_in *)&ifr.ifr_addr;
          iflist[cntr].ifdstaddr = ntohl(sin->sin_addr.s_addr);

          iflist[cntr].flags1 |= IFLIST_FLAGS1_IF_IS_PPP;
          iflist[cntr].bcast = INADDR_BROADCAST;
        }
        else if (ifrflags.ifr_flags & IFF_BROADCAST)
        {
          iflist[cntr].ifdstaddr = 0;
          iflist[cntr].flags1 |= IFLIST_FLAGS1_IF_IS_ETH;
          iflist[cntr].bcast = INADDR_BROADCAST;
        }
        else
        {
          iflist[cntr].ifdstaddr = 0;
          iflist[cntr].flags1 |= IFLIST_FLAGS1_IF_IS_UNKNOWN;
          iflist[cntr].bcast = INADDR_BROADCAST;
        }

        cntr++;
      }
    // IPSEC tunnels are a fun one.  We must change the destination address
    // so that it will be routed to the correct tunnel end point.
    // We can define several tunnel end points for the same ipsec interface.
    } else if (ipsec && *ipsec && strncmp(ifs.ifc_req[i].ifr_name, "ipsec", 5) == 0) {
      if (strncmp(ifs.ifc_req[i].ifr_name, ipsec, 6) == 0) {
        struct hostent *hp = gethostbyname(ipsec+7);
        ioctl(s, SIOCGIFINDEX, &ifs.ifc_req[i]);
        iflist[cntr].index = ifs.ifc_req[i].ifr_ifindex; /* Store the SIOCGIFINDEX number */
        memcpy(&(iflist[cntr++].bcast), hp->h_addr, sizeof(u_int32_t));
      }
    }
  }

  iflist[cntr].index = 0;                       // Terminate list
  free(ifs.ifc_req);                            // Stop that leak.
  regfree(&preg);
  if (s >= 0)
    close(s);

  return iflist;
}



unsigned int ip_compute_checksum(unsigned char *ippdu, int hlen)
{
  unsigned int sum = 0, temp;
  unsigned char *p;
  unsigned char cs_msb;
  unsigned char cs_lsb;

  /* Save original checksum */
  cs_msb = *IP_IPPDU_CHECKSUM_MSB_PTR(ippdu);
  cs_lsb = *IP_IPPDU_CHECKSUM_LSB_PTR(ippdu);

  *IP_IPPDU_CHECKSUM_MSB_PTR(ippdu) = 0;
  *IP_IPPDU_CHECKSUM_LSB_PTR(ippdu) = 0;

  p = ippdu;
  hlen /= 2; /* We'll compute taking two bytes a a time */
  while(hlen--) { sum += ((*p * 256) + *(p + 1)); p += 2; }
  while ((temp = (sum >> 16))) { sum = (temp + (sum & 0xFFFF)); }

  /* Restore original checksum */
  *IP_IPPDU_CHECKSUM_MSB_PTR(ippdu) = cs_msb;
  *IP_IPPDU_CHECKSUM_LSB_PTR(ippdu) = cs_lsb;

  return(~sum & 0xFFFF);
}

void ip_update_checksum(unsigned char *ippdu)
{
  unsigned int cs;

  cs = ip_compute_checksum(ippdu, 4 * IP_IPPDU_IHL(ippdu));

  *IP_IPPDU_CHECKSUM_MSB_PTR(ippdu) = (unsigned char)((cs >> 8) & 0xFF);
  *IP_IPPDU_CHECKSUM_LSB_PTR(ippdu) = (unsigned char)(cs & 0xFF);
}


static char *IpProtToString( unsigned char prot )
{
  switch( prot )
  {
  case 0x11:
    return "UDP";
  case 0x6:
    return "TCP";
  case 0x2f:
    return "GRE";
  case 0x1:
    return "ICMP";
  default:
    return "???";
  }
}

static char *iflistToString( struct iflist *ifp )
{
  static char str_tr[80+1];

  sprintf(str_tr, "index=%d, ifname=%s, ifaddr=%ld.%ld.%ld.%ld, ifdstaddr=%ld.%ld.%ld.%ld, flags1=0x%04lx",
          ifp->index, ifp->ifname,
          (ifp->ifaddr)>>24, ((ifp->ifaddr)&0x00ff0000)>>16, ((ifp->ifaddr)&0x0000ff00)>>8, (ifp->ifaddr)&0x000000ff,
          (ifp->ifdstaddr)>>24, ((ifp->ifdstaddr)&0x00ff0000)>>16, ((ifp->ifdstaddr)&0x0000ff00)>>8, 
          (ifp->ifdstaddr)&0x000000ff, ifp->flags1);

  return str_tr;
}

static char *iflistLogRToString( struct iflist *ifp, int idx, struct ifsnr *ifnr )
{
  static char str_tr[MAX_IFLOGTOSTR]; /*
                                       * This makes function: 1) non-reentrant (doesn't matter).
                                       *                      2) not useable multiple times by (s)printf.
                                       */
  sprintf(str_tr, "%s", ifp->ifname);
  return str_tr;
}

static char *iflistLogIToString( struct iflist *ifp, int idx, struct ifsnr *ifnr )
{
  static char str_tr[MAX_IFLOGTOSTR]; /*
                                       * This makes function: 1) non-reentrant (doesn't matter).
                                       *                      2) not useable multiple times by (s)printf.
                                       */
  sprintf(str_tr, "%s(%d/%d/%d)", ifp->ifname, idx, ifp->index, ifnr->sock_nr);
  return str_tr;
}

static void bind_to_iface(int fd, int ifindex)
{
#ifdef __linux__
  static const struct sock_filter filter_instr[] = {
    /* ip.protocol == IPPROTO_UDP */
    BPF_STMT(BPF_LD|BPF_B|BPF_ABS, 9),
    BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, IPPROTO_UDP, 0, 9),
    /* ip.daddr & 0x000000ff == 0x000000ff */
    BPF_STMT(BPF_LD|BPF_W|BPF_ABS, 16),
    BPF_STMT(BPF_ALU|BPF_AND|BPF_K, 0x000000ff),
    BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, 0x000000ff, 0, 6),
    /* ip.ttl != 1 */
    BPF_STMT(BPF_LD|BPF_B|BPF_ABS, 8),
    BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, 1, 4, 0),
    /* udp.checksum != 0 */
    BPF_STMT(BPF_LDX|BPF_B|BPF_MSH, 0),
    BPF_STMT(BPF_LD|BPF_H|BPF_IND, 6),
    BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, 0, 1, 0),
    /* accept packet */
    BPF_STMT(BPF_RET|BPF_K, 0xffffffff),
    /* discard packet */
    BPF_STMT(BPF_RET|BPF_K, 0),
  };
  static const struct sock_fprog filter = {
    .len = sizeof(filter_instr) / sizeof(filter_instr[0]),
    .filter = (struct sock_filter *) filter_instr,
  };
#endif
  struct sockaddr_ll      sll;

  memset(&sll, 0, sizeof(sll));
  sll.sll_family          = AF_PACKET;
  sll.sll_ifindex         = ifindex;
  sll.sll_protocol        = htons(ETH_P_IP);

  if (bind(fd, (struct sockaddr *) &sll, sizeof(sll)) == -1) {
    syslog(LOG_ERR, "bind_to_iface: Error, bind failed! (rv=-1, errno=%d)", errno);
    exit(1);
  }

#ifdef __linux__
  /* Ignoring error (kernel may lack support for this) */
  setsockopt(fd, SOL_SOCKET, SO_ATTACH_FILTER, &filter, sizeof(filter));
#endif
}

static void copy_ifsnr(struct ifsnr *from, struct ifsnr *to)
{
  int i;

  for (i=0; i<MAXIF; ++i)
  {
    to[i].sock_nr = from[i].sock_nr;
    to[i].ifindex = from[i].ifindex;
  }
}

static int find_sock_nr(struct ifsnr *l, int ifidx)
{
  int i;

  for (i=0; i<MAXIF; ++i)
    if (l[i].ifindex == ifidx) return l[i].sock_nr;
  /* not found */
  return -1;
}

