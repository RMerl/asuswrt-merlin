/* dnsmasq is Copyright (c) 2000-2012 Simon Kelley
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
     
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define COPYRIGHT "Copyright (c) 2000-2012 Simon Kelley" 

#ifndef NO_LARGEFILE
/* Ensure we can use files >2GB (log files may grow this big) */
#  define _LARGEFILE_SOURCE 1
#  define _FILE_OFFSET_BITS 64
#endif

/* Get linux C library versions and define _GNU_SOURCE for kFreeBSD. */
#if defined(__linux__) || defined(__GLIBC__)
#  ifndef __ANDROID__
#      define _GNU_SOURCE
#  endif
#  include <features.h> 
#endif

/* Need these defined early */
#if defined(__sun) || defined(__sun__)
#  define _XPG4_2
#  define __EXTENSIONS__
#endif

/* get these before config.h  for IPv6 stuff... */
#include <sys/types.h> 
#include <sys/socket.h>

#ifdef __APPLE__
/* Define before netinet/in.h to select API. OSX Lion onwards. */
#  define __APPLE_USE_RFC_3542
#endif
#include <netinet/in.h>

/* Also needed before config.h. */
#include <getopt.h>

#include "config.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

#include "dns-protocol.h"
#include "dhcp-protocol.h"
#ifdef HAVE_DHCP6
#include "dhcp6-protocol.h"
#include "radv-protocol.h"
#endif

#define gettext_noop(S) (S)
#ifndef LOCALEDIR
#  define _(S) (S)
#else
#  include <libintl.h>
#  include <locale.h>   
#  define _(S) gettext(S)
#endif

#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#if defined(HAVE_SOLARIS_NETWORK)
#  include <sys/sockio.h>
#endif
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/un.h>
#include <limits.h>
#include <net/if.h>
#if defined(HAVE_SOLARIS_NETWORK) && !defined(ifr_mtu)
/* Some solaris net/if./h omit this. */
#  define ifr_mtu  ifr_ifru.ifru_metric
#endif
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <stddef.h>
#include <time.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <stdarg.h>
#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__sun__) || defined (__sun) || defined (__ANDROID__)
#  include <netinet/if_ether.h>
#else
#  include <net/ethernet.h>
#endif
#include <net/if_arp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/uio.h>
#include <syslog.h>
#include <dirent.h>
#ifndef HAVE_LINUX_NETWORK
#  include <net/if_dl.h>
#endif

#if defined(HAVE_LINUX_NETWORK)
#include <linux/capability.h>
/* There doesn't seem to be a universally-available 
   userpace header for these. */
extern int capset(cap_user_header_t header, cap_user_data_t data);
extern int capget(cap_user_header_t header, cap_user_data_t data);
#define LINUX_CAPABILITY_VERSION_1  0x19980330
#define LINUX_CAPABILITY_VERSION_2  0x20071026
#define LINUX_CAPABILITY_VERSION_3  0x20080522

#include <sys/prctl.h>
#elif defined(HAVE_SOLARIS_NETWORK)
#include <priv.h>
#endif

/* daemon is function in the C library.... */
#define daemon dnsmasq_daemon

/* Async event queue */
struct event_desc {
  int event, data, msg_sz;
};

#define EVENT_RELOAD    1
#define EVENT_DUMP      2
#define EVENT_ALARM     3
#define EVENT_TERM      4
#define EVENT_CHILD     5
#define EVENT_REOPEN    6
#define EVENT_EXITED    7
#define EVENT_KILLED    8
#define EVENT_EXEC_ERR  9
#define EVENT_PIPE_ERR  10
#define EVENT_USER_ERR  11
#define EVENT_CAP_ERR   12
#define EVENT_PIDFILE   13
#define EVENT_HUSER_ERR 14
#define EVENT_GROUP_ERR 15
#define EVENT_DIE       16
#define EVENT_LOG_ERR   17
#define EVENT_FORK_ERR  18
#define EVENT_LUA_ERR   19
#define EVENT_TFTP_ERR  20

/* Exit codes. */
#define EC_GOOD        0
#define EC_BADCONF     1
#define EC_BADNET      2
#define EC_FILE        3
#define EC_NOMEM       4
#define EC_MISC        5
#define EC_INIT_OFFSET 10

/* Min buffer size: we check after adding each record, so there must be 
   memory for the largest packet, and the largest record so the
   min for DNS is PACKETSZ+MAXDNAME+RRFIXEDSZ which is < 1000.
   This might be increased is EDNS packet size if greater than the minimum.
*/
#define DNSMASQ_PACKETSZ PACKETSZ+MAXDNAME+RRFIXEDSZ

/* Trust the compiler dead-code eliminator.... */
#define option_bool(x) (((x) < 32) ? daemon->options & (1u << (x)) : daemon->options2 & (1u << ((x) - 32)))

#define OPT_BOGUSPRIV      0
#define OPT_FILTER         1
#define OPT_LOG            2
#define OPT_SELFMX         3
#define OPT_NO_HOSTS       4
#define OPT_NO_POLL        5
#define OPT_DEBUG          6
#define OPT_ORDER          7
#define OPT_NO_RESOLV      8
#define OPT_EXPAND         9
#define OPT_LOCALMX        10
#define OPT_NO_NEG         11
#define OPT_NODOTS_LOCAL   12
#define OPT_NOWILD         13
#define OPT_ETHERS         14
#define OPT_RESOLV_DOMAIN  15
#define OPT_NO_FORK        16
#define OPT_AUTHORITATIVE  17
#define OPT_LOCALISE       18
#define OPT_DBUS           19
#define OPT_DHCP_FQDN      20
#define OPT_NO_PING        21
#define OPT_LEASE_RO       22
#define OPT_ALL_SERVERS    23
#define OPT_RELOAD         24
#define OPT_LOCAL_REBIND   25  
#define OPT_TFTP_SECURE    26
#define OPT_TFTP_NOBLOCK   27
#define OPT_LOG_OPTS       28
#define OPT_TFTP_APREF     29
#define OPT_NO_OVERRIDE    30
#define OPT_NO_REBIND      31
#define OPT_ADD_MAC        32
#define OPT_DNSSEC         33
#define OPT_CONSEC_ADDR    34
#define OPT_CONNTRACK      35
#define OPT_FQDN_UPDATE    36
#define OPT_RA             37
#define OPT_TFTP_LC        38
#define OPT_CLEVERBIND     39
#define OPT_TFTP           40
#define OPT_LOG_QUIET      41
#define OPT_LAST           42

/* extra flags for my_syslog, we use a couple of facilities since they are known 
   not to occupy the same bits as priorities, no matter how syslog.h is set up. */
#define MS_TFTP LOG_USER
#define MS_DHCP LOG_DAEMON 

struct all_addr {
  union {
    struct in_addr addr4;
#ifdef HAVE_IPV6
    struct in6_addr addr6;
#endif
  } addr;
};

struct bogus_addr {
  struct in_addr addr;
  struct bogus_addr *next;
};

/* dns doctor param */
struct doctor {
  struct in_addr in, end, out, mask;
  struct doctor *next;
};

struct mx_srv_record {
  char *name, *target;
  int issrv, srvport, priority, weight;
  unsigned int offset;
  struct mx_srv_record *next;
};

struct naptr {
  char *name, *replace, *regexp, *services, *flags;
  unsigned int order, pref;
  struct naptr *next;
};

struct txt_record {
  char *name;
  unsigned char *txt;
  unsigned short class, len;
  struct txt_record *next;
};

struct ptr_record {
  char *name, *ptr;
  struct ptr_record *next;
};

struct cname {
  char *alias, *target;
  struct cname *next;
};

struct host_record {
  struct name_list {
    char *name;
    struct name_list *next;
  } *names;
  struct in_addr addr;
#ifdef HAVE_IPV6
  struct in6_addr addr6;
#endif
  struct host_record *next;
};

struct interface_name {
  char *name; /* domain name */
  char *intr; /* interface name */
  struct interface_name *next;
};

union bigname {
  char name[MAXDNAME];
  union bigname *next; /* freelist */
};

struct keydata {
  struct keydata *next;
  unsigned char key[KEYBLOCK_LEN];
};

struct crec { 
  struct crec *next, *prev, *hash_next;
  /* union is 16 bytes when doing IPv6, 8 bytes on 32 bit machines without IPv6 */
  union {
    struct all_addr addr;
    struct {
      struct crec *cache;
      int uid;
    } cname;
    struct {
      struct keydata *keydata;
      unsigned char algo;
      unsigned char digest; /* DS only */
      unsigned short flags_or_keyid; /* flags for DNSKEY, keyid for DS */
    } key;
  } addr;
  time_t ttd; /* time to die */
  /* used as keylen if F_DS or F_DNSKEY, index to source for F_HOSTS */
  int uid; 
  unsigned short flags;
  union {
    char sname[SMALLDNAME];
    union bigname *bname;
    char *namep;
  } name;
};

#define F_IMMORTAL  (1u<<0)
#define F_NAMEP     (1u<<1)
#define F_REVERSE   (1u<<2)
#define F_FORWARD   (1u<<3)
#define F_DHCP      (1u<<4)
#define F_NEG       (1u<<5)       
#define F_HOSTS     (1u<<6)
#define F_IPV4      (1u<<7)
#define F_IPV6      (1u<<8)
#define F_BIGNAME   (1u<<9)
#define F_NXDOMAIN  (1u<<10)
#define F_CNAME     (1u<<11)
#define F_DNSKEY    (1u<<12)
#define F_CONFIG    (1u<<13)
#define F_DS        (1u<<14)
#define F_DNSSECOK  (1u<<15)

/* below here are only valid as args to log_query: cache
   entries are limited to 16 bits */
#define F_UPSTREAM  (1u<<16)
#define F_RRNAME    (1u<<17)
#define F_SERVER    (1u<<18)
#define F_QUERY     (1u<<19)
#define F_NOERR     (1u<<20)
/* composites */
#define F_TYPE      (F_IPV4 | F_IPV6 | F_DNSKEY | F_DS) /* Only one may be set */



/* struct sockaddr is not large enough to hold any address,
   and specifically not big enough to hold an IPv6 address.
   Blech. Roll our own. */
union mysockaddr {
  struct sockaddr sa;
  struct sockaddr_in in;
#if defined(HAVE_IPV6)
  struct sockaddr_in6 in6;
#endif
};

#define SERV_FROM_RESOLV       1  /* 1 for servers from resolv, 0 for command line. */
#define SERV_NO_ADDR           2  /* no server, this domain is local only */
#define SERV_LITERAL_ADDRESS   4  /* addr is the answer, not the server */ 
#define SERV_HAS_DOMAIN        8  /* server for one domain only */
#define SERV_HAS_SOURCE       16  /* source address defined */
#define SERV_FOR_NODOTS       32  /* server for names with no domain part only */
#define SERV_WARNED_RECURSIVE 64  /* avoid warning spam */
#define SERV_FROM_DBUS       128  /* 1 if source is DBus */
#define SERV_MARK            256  /* for mark-and-delete */
#define SERV_TYPE    (SERV_HAS_DOMAIN | SERV_FOR_NODOTS)
#define SERV_COUNTED         512  /* workspace for log code */
#define SERV_USE_RESOLV     1024  /* forward this domain in the normal way */
#define SERV_NO_REBIND      2048  /* inhibit dns-rebind protection */

struct serverfd {
  int fd;
  union mysockaddr source_addr;
  char interface[IF_NAMESIZE+1];
  struct serverfd *next;
};

struct randfd {
  int fd;
  unsigned short refcount, family;
};
  
struct server {
  union mysockaddr addr, source_addr;
  char interface[IF_NAMESIZE+1];
  struct serverfd *sfd; 
  char *domain; /* set if this server only handles a domain. */ 
  int flags, tcpfd;
  unsigned int queries, failed_queries;
  struct server *next; 
};

struct irec {
  union mysockaddr addr;
  struct in_addr netmask; /* only valid for IPv4 */
  int tftp_ok, dhcp_ok, mtu, done, dad;
  char *name;
  struct irec *next;
};

struct listener {
  int fd, tcpfd, tftpfd, family;
  struct irec *iface; /* only sometimes valid for non-wildcard */
  struct listener *next;
};

/* interface and address parms from command line. */
struct iname {
  char *name;
  union mysockaddr addr;
  int used;
  struct iname *next;
};

/* resolv-file parms from command-line */
struct resolvc {
  struct resolvc *next;
  int is_default, logged;
  time_t mtime;
  char *name;
};

/* adn-hosts parms from command-line (also dhcp-hostsfile and dhcp-optsfile */
#define AH_DIR      1
#define AH_INACTIVE 2
struct hostsfile {
  struct hostsfile *next;
  int flags;
  char *fname;
  int index; /* matches to cache entries for logging */
};

#define FREC_NOREBIND           1
#define FREC_CHECKING_DISABLED  2

struct frec {
  union mysockaddr source;
  struct all_addr dest;
  struct server *sentto; /* NULL means free */
  struct randfd *rfd4;
#ifdef HAVE_IPV6
  struct randfd *rfd6;
#endif
  unsigned int iface;
  unsigned short orig_id, new_id;
  int fd, forwardall, flags;
  unsigned int crc;
  time_t time;
  struct frec *next;
};

/* flags in top of length field for DHCP-option tables */
#define OT_ADDR_LIST    0x8000
#define OT_RFC1035_NAME 0x4000
#define OT_INTERNAL     0x2000
#define OT_NAME         0x1000
#define OT_CSTRING      0x0800
#define OT_DEC          0x0400 
#define OT_TIME         0x0200

/* actions in the daemon->helper RPC */
#define ACTION_DEL           1
#define ACTION_OLD_HOSTNAME  2
#define ACTION_OLD           3
#define ACTION_ADD           4
#define ACTION_TFTP          5

#define LEASE_NEW            1  /* newly created */
#define LEASE_CHANGED        2  /* modified */
#define LEASE_AUX_CHANGED    4  /* CLID or expiry changed */
#define LEASE_AUTH_NAME      8  /* hostname came from config, not from client */
#define LEASE_USED          16  /* used this DHCPv6 transaction */
#define LEASE_NA            32  /* IPv6 no-temporary lease */
#define LEASE_TA            64  /* IPv6 temporary lease */
#define LEASE_HAVE_HWADDR  128  /* Have set hwaddress */

struct dhcp_lease {
  int clid_len;          /* length of client identifier */
  unsigned char *clid;   /* clientid */
  char *hostname, *fqdn; /* name from client-hostname option or config */
  char *old_hostname;    /* hostname before it moved to another lease */
  int flags;
  time_t expires;        /* lease expiry */
#ifdef HAVE_BROKEN_RTC
  unsigned int length;
#endif
  int hwaddr_len, hwaddr_type; /* hw_type used for iaid in v6 */
  unsigned char hwaddr[DHCP_CHADDR_MAX]; /* also IPv6 address */
  struct in_addr addr, override, giaddr;
  unsigned char *extradata;
  unsigned int extradata_len, extradata_size;
  int last_interface;
#ifdef HAVE_DHCP6
  struct slaac_address {
    struct in6_addr addr, local;
    time_t ping_time;
    int backoff; /* zero -> confirmed */
    struct slaac_address *next;
  } *slaac_address;
#endif
  struct dhcp_lease *next;
};

struct dhcp_netid {
  char *net;
  struct dhcp_netid *next;
};

struct dhcp_netid_list {
  struct dhcp_netid *list;
  struct dhcp_netid_list *next;
};

struct tag_if {
  struct dhcp_netid_list *set;
  struct dhcp_netid *tag;
  struct tag_if *next;
};

struct hwaddr_config {
  int hwaddr_len, hwaddr_type;
  unsigned char hwaddr[DHCP_CHADDR_MAX];
  unsigned int wildcard_mask;
  struct hwaddr_config *next;
};

struct dhcp_config {
  unsigned int flags;
  int clid_len;          /* length of client identifier */
  unsigned char *clid;   /* clientid */
  char *hostname, *domain;
  struct dhcp_netid_list *netid;
#ifdef HAVE_DHCP6
  struct in6_addr addr6;
#endif
  struct in_addr addr;
  time_t decline_time;
  unsigned int lease_time;
  struct hwaddr_config *hwaddr;
  struct dhcp_config *next;
};

#define have_config(config, mask) ((config) && ((config)->flags & (mask))) 

#define CONFIG_DISABLE           1
#define CONFIG_CLID              2
#define CONFIG_TIME              8
#define CONFIG_NAME             16
#define CONFIG_ADDR             32
#define CONFIG_NOCLID          128
#define CONFIG_FROM_ETHERS     256    /* entry created by /etc/ethers */
#define CONFIG_ADDR_HOSTS      512    /* address added by from /etc/hosts */
#define CONFIG_DECLINED       1024    /* address declined by client */
#define CONFIG_BANK           2048    /* from dhcp hosts file */
#define CONFIG_ADDR6          4096

struct dhcp_opt {
  int opt, len, flags;
  union {
    int encap;
    unsigned int wildcard_mask;
    unsigned char *vendor_class;
  } u;
  unsigned char *val;
  struct dhcp_netid *netid;
  struct dhcp_opt *next;
};

#define DHOPT_ADDR               1
#define DHOPT_STRING             2
#define DHOPT_ENCAPSULATE        4
#define DHOPT_ENCAP_MATCH        8
#define DHOPT_FORCE             16
#define DHOPT_BANK              32
#define DHOPT_ENCAP_DONE        64
#define DHOPT_MATCH            128
#define DHOPT_VENDOR           256
#define DHOPT_HEX              512
#define DHOPT_VENDOR_MATCH    1024
#define DHOPT_RFC3925         2048
#define DHOPT_TAGOK           4096
#define DHOPT_ADDR6           8192

struct dhcp_boot {
  char *file, *sname, *tftp_sname;
  struct in_addr next_server;
  struct dhcp_netid *netid;
  struct dhcp_boot *next;
};

struct pxe_service {
  unsigned short CSA, type; 
  char *menu, *basename, *sname;
  struct in_addr server;
  struct dhcp_netid *netid;
  struct pxe_service *next;
};

#define MATCH_VENDOR     1
#define MATCH_USER       2
#define MATCH_CIRCUIT    3
#define MATCH_REMOTE     4
#define MATCH_SUBSCRIBER 5

/* vendorclass, userclass, remote-id or cicuit-id */
struct dhcp_vendor {
  int len, match_type;
  unsigned int enterprise;
  char *data;
  struct dhcp_netid netid;
  struct dhcp_vendor *next;
};

struct dhcp_mac {
  unsigned int mask;
  int hwaddr_len, hwaddr_type;
  unsigned char hwaddr[DHCP_CHADDR_MAX];
  struct dhcp_netid netid;
  struct dhcp_mac *next;
};

struct dhcp_bridge {
  char iface[IF_NAMESIZE];
  struct dhcp_bridge *alias, *next;
};

struct cond_domain {
  char *domain;
  struct in_addr start, end;
#ifdef HAVE_IPV6
  struct in6_addr start6, end6;
#endif
  int is6;
  struct cond_domain *next;
};

struct dhcp_context {
  unsigned int lease_time, addr_epoch;
  struct in_addr netmask, broadcast;
  struct in_addr local, router;
  struct in_addr start, end; /* range of available addresses */
#ifdef HAVE_DHCP6
  struct in6_addr start6, end6; /* range of available addresses */
  struct in6_addr local6;
  int prefix, if_index;
  time_t ra_time;
#endif
  int flags;
  struct dhcp_netid netid, *filter;
  struct dhcp_context *next, *current;
};

#define CONTEXT_STATIC         1
#define CONTEXT_NETMASK        2
#define CONTEXT_BRDCAST        4
#define CONTEXT_PROXY          8
#define CONTEXT_RA_ONLY       16
#define CONTEXT_RA_DONE       32
#define CONTEXT_RA_NAME       64
#define CONTEXT_RA_STATELESS 128
#define CONTEXT_DHCP         256
#define CONTEXT_DEPRECATE    512

struct ping_result {
  struct in_addr addr;
  time_t time;
  unsigned int hash;
  struct ping_result *next;
};

struct tftp_file {
  int refcount, fd;
  off_t size;
  dev_t dev;
  ino_t inode;
  char filename[];
};

struct tftp_transfer {
  int sockfd;
  time_t timeout;
  int backoff;
  unsigned int block, blocksize, expansion;
  off_t offset;
  union mysockaddr peer;
  char opt_blocksize, opt_transize, netascii, carrylf;
  struct tftp_file *file;
  struct tftp_transfer *next;
};

struct addr_list {
  struct in_addr addr;
  struct addr_list *next;
};

struct tftp_prefix {
  char *interface;
  char *prefix;
  struct tftp_prefix *next;
};


extern struct daemon {
  /* datastuctures representing the command-line and 
     config file arguments. All set (including defaults)
     in option.c */

  unsigned int options, options2;
  struct resolvc default_resolv, *resolv_files;
  time_t last_resolv;
  struct mx_srv_record *mxnames;
  struct naptr *naptr;
  struct txt_record *txt, *rr;
  struct ptr_record *ptr;
  struct host_record *host_records, *host_records_tail;
  struct cname *cnames;
  struct interface_name *int_names;
  char *mxtarget;
  char *lease_file; 
  char *username, *groupname, *scriptuser;
  char *luascript;
  int group_set, osport;
  char *domain_suffix;
  struct cond_domain *cond_domain;
  char *runfile; 
  char *lease_change_command;
  struct iname *if_names, *if_addrs, *if_except, *dhcp_except;
  struct bogus_addr *bogus_addr;
  struct server *servers;
  int log_fac; /* log facility */
  char *log_file; /* optional log file */
  int max_logs;  /* queue limit */
  int cachesize, ftabsize;
  int port, query_port, min_port;
  unsigned long local_ttl, neg_ttl, max_ttl;
  struct hostsfile *addn_hosts;
  struct dhcp_context *dhcp, *dhcp6, *ra_contexts;
  struct dhcp_config *dhcp_conf;
  struct dhcp_opt *dhcp_opts, *dhcp_match, *dhcp_opts6, *dhcp_match6;
  struct dhcp_vendor *dhcp_vendors;
  struct dhcp_mac *dhcp_macs;
  struct dhcp_boot *boot_config;
  struct pxe_service *pxe_services;
  struct tag_if *tag_if; 
  struct addr_list *override_relays;
  int override;
  int enable_pxe;
  struct dhcp_netid_list *dhcp_ignore, *dhcp_ignore_names, *dhcp_gen_names; 
  struct dhcp_netid_list *force_broadcast, *bootp_dynamic;
  struct hostsfile *dhcp_hosts_file, *dhcp_opts_file;
  int dhcp_max, tftp_max;
  int dhcp_server_port, dhcp_client_port;
  int start_tftp_port, end_tftp_port; 
  unsigned int min_leasetime;
  struct doctor *doctors;
  unsigned short edns_pktsz;
  char *tftp_prefix; 
  struct tftp_prefix *if_prefix; /* per-interface TFTP prefixes */
  unsigned int duid_enterprise, duid_config_len;
  unsigned char *duid_config;
  char *dbus_name;

  /* globally used stuff for DNS */
  char *packet; /* packet buffer */
  int packet_buff_sz; /* size of above */
  char *namebuff; /* MAXDNAME size buffer */
  unsigned int local_answer, queries_forwarded;
  struct frec *frec_list;
  struct serverfd *sfds;
  struct irec *interfaces;
  struct listener *listeners;
  struct server *last_server;
  time_t forwardtime;
  int forwardcount;
  struct server *srv_save; /* Used for resend on DoD */
  size_t packet_len;       /*      "        "        */
  struct randfd *rfd_save; /*      "        "        */
  pid_t tcp_pids[MAX_PROCS];
  struct randfd randomsocks[RANDOM_SOCKS];
  int v6pktinfo; 

  /* DHCP state */
  int dhcpfd, helperfd, pxefd; 
#if defined(HAVE_LINUX_NETWORK)
  int netlinkfd;
#elif defined(HAVE_BSD_NETWORK)
  int dhcp_raw_fd, dhcp_icmp_fd;
#endif
  struct iovec dhcp_packet;
  char *dhcp_buff, *dhcp_buff2, *dhcp_buff3;
  struct ping_result *ping_results;
  FILE *lease_stream;
  struct dhcp_bridge *bridges;
#ifdef HAVE_DHCP6
  int duid_len;
  unsigned char *duid;
  struct iovec outpacket;
  int dhcp6fd, icmp6fd;
#endif
  /* DBus stuff */
  /* void * here to avoid depending on dbus headers outside dbus.c */
  void *dbus;
#ifdef HAVE_DBUS
  struct watch *watches;
#endif

  /* TFTP stuff */
  struct tftp_transfer *tftp_trans, *tftp_done_trans;

  /* utility string buffer, hold max sized IP address as string */
  char *addrbuff;

} *daemon;

/* cache.c */
void cache_init(void);
void log_query(unsigned int flags, char *name, struct all_addr *addr, char *arg); 
char *record_source(int index);
void querystr(char *str, unsigned short type);
struct crec *cache_find_by_addr(struct crec *crecp,
				struct all_addr *addr, time_t now, 
				unsigned short prot);
struct crec *cache_find_by_name(struct crec *crecp, 
				char *name, time_t now, unsigned short  prot);
void cache_end_insert(void);
void cache_start_insert(void);
struct crec *cache_insert(char *name, struct all_addr *addr,
			  time_t now, unsigned long ttl, unsigned short flags);
void cache_reload(void);
void cache_add_dhcp_entry(char *host_name, int prot, struct all_addr *host_address, time_t ttd);
struct in_addr a_record_from_hosts(char *name, time_t now);
void cache_unhash_dhcp(void);
void dump_cache(time_t now);
char *cache_get_name(struct crec *crecp);
char *get_domain(struct in_addr addr);
#ifdef HAVE_IPV6
char *get_domain6(struct in6_addr *addr);
#endif
#ifdef HAVE_DNSSEC
struct keydata *keydata_alloc(char *data, size_t len);
void keydata_free(struct keydata *blocks);
#endif

/* rfc1035.c */
unsigned int extract_request(struct dns_header *header, size_t qlen, 
			       char *name, unsigned short *typep);
size_t setup_reply(struct dns_header *header, size_t  qlen,
		   struct all_addr *addrp, unsigned int flags,
		   unsigned long local_ttl);
int extract_addresses(struct dns_header *header, size_t qlen, char *namebuff, 
		      time_t now, int is_sign, int checkrebind, int checking_disabled);
size_t answer_request(struct dns_header *header, char *limit, size_t qlen,  
		   struct in_addr local_addr, struct in_addr local_netmask, time_t now);
int check_for_bogus_wildcard(struct dns_header *header, size_t qlen, char *name, 
			     struct bogus_addr *addr, time_t now);
unsigned char *find_pseudoheader(struct dns_header *header, size_t plen,
				 size_t *len, unsigned char **p, int *is_sign);
int check_for_local_domain(char *name, time_t now);
unsigned int questions_crc(struct dns_header *header, size_t plen, char *buff);
size_t resize_packet(struct dns_header *header, size_t plen, 
		  unsigned char *pheader, size_t hlen);
size_t add_mac(struct dns_header *header, size_t plen, char *limit, union mysockaddr *l3);

/* util.c */
void rand_init(void);
unsigned short rand16(void);
int legal_hostname(char *c);
char *canonicalise(char *s, int *nomem);
unsigned char *do_rfc1035_name(unsigned char *p, char *sval);
void *safe_malloc(size_t size);
void safe_pipe(int *fd, int read_noblock);
void *whine_malloc(size_t size);
int sa_len(union mysockaddr *addr);
int sockaddr_isequal(union mysockaddr *s1, union mysockaddr *s2);
int hostname_isequal(char *a, char *b);
time_t dnsmasq_time(void);
int is_same_net(struct in_addr a, struct in_addr b, struct in_addr mask);
#ifdef HAVE_IPV6
int is_same_net6(struct in6_addr *a, struct in6_addr *b, int prefixlen);
u64 addr6part(struct in6_addr *addr);
void setaddr6part(struct in6_addr *addr, u64 host);
#endif
int retry_send(void);
void prettyprint_time(char *buf, unsigned int t);
int prettyprint_addr(union mysockaddr *addr, char *buf);
int parse_hex(char *in, unsigned char *out, int maxlen, 
	      unsigned int *wildcard_mask, int *mac_type);
int memcmp_masked(unsigned char *a, unsigned char *b, int len, 
		  unsigned int mask);
int expand_buf(struct iovec *iov, size_t size);
char *print_mac(char *buff, unsigned char *mac, int len);
void bump_maxfd(int fd, int *max);
int read_write(int fd, unsigned char *packet, int size, int rw);

/* log.c */
void die(char *message, char *arg1, int exit_code);
int log_start(struct passwd *ent_pw, int errfd);
int log_reopen(char *log_file);
void my_syslog(int priority, const char *format, ...);
void set_log_writer(fd_set *set, int *maxfdp);
void check_log_writer(fd_set *set);
void flush_log(void);

/* option.c */
void read_opts (int argc, char **argv, char *compile_opts);
char *option_string(int prot, unsigned int opt, unsigned char *val, 
		    int opt_len, char *buf, int buf_len);
void reread_dhcp(void);
void set_option_bool(unsigned int opt);
void reset_option_bool(unsigned int opt);
struct hostsfile *expand_filelist(struct hostsfile *list);
char *parse_server(char *arg, union mysockaddr *addr, 
		   union mysockaddr *source_addr, char *interface, int *flags);

/* forward.c */
void reply_query(int fd, int family, time_t now);
void receive_query(struct listener *listen, time_t now);
unsigned char *tcp_request(int confd, time_t now,
			   union mysockaddr *local_addr, struct in_addr netmask);
void server_gone(struct server *server);
struct frec *get_new_frec(time_t now, int *wait);
int send_from(int fd, int nowild, char *packet, size_t len, 
	       union mysockaddr *to, struct all_addr *source,
	       unsigned int iface);

/* network.c */
int indextoname(int fd, int index, char *name);
int local_bind(int fd, union mysockaddr *addr, char *intname, int is_tcp);
int random_sock(int family);
void pre_allocate_sfds(void);
int reload_servers(char *fname);
void check_servers(void);
int enumerate_interfaces();
void create_wildcard_listeners(void);
void create_bound_listeners(int die);
int is_dad_listeners(void);
int iface_check(int family, struct all_addr *addr, char *name);
int fix_fd(int fd);
struct in_addr get_ifaddr(char *intr);
#ifdef HAVE_IPV6
int set_ipv6pktinfo(int fd);
#endif

/* dhcp.c */
#ifdef HAVE_DHCP
void dhcp_init(void);
void dhcp_packet(time_t now, int pxe_fd);
struct dhcp_context *address_available(struct dhcp_context *context, 
				       struct in_addr addr,
				       struct dhcp_netid *netids);
struct dhcp_context *narrow_context(struct dhcp_context *context, 
				    struct in_addr taddr,
				    struct dhcp_netid *netids);
int address_allocate(struct dhcp_context *context,
		     struct in_addr *addrp, unsigned char *hwaddr, int hw_len,
		     struct dhcp_netid *netids, time_t now);
int config_has_mac(struct dhcp_config *config, unsigned char *hwaddr, int len, int type);
struct dhcp_config *find_config(struct dhcp_config *configs,
				struct dhcp_context *context,
				unsigned char *clid, int clid_len,
				unsigned char *hwaddr, int hw_len, 
				int hw_type, char *hostname);
void dhcp_read_ethers(void);
struct dhcp_config *config_find_by_address(struct dhcp_config *configs, struct in_addr addr);
char *host_from_dns(struct in_addr addr);
#endif

/* lease.c */
#ifdef HAVE_DHCP
#ifdef HAVE_LEASEFILE_EXPIRE
void lease_flush_file(time_t now);
#endif
void lease_update_file(time_t now);
void lease_update_dns(int force);
void lease_init(time_t now);
struct dhcp_lease *lease4_allocate(struct in_addr addr);
#ifdef HAVE_DHCP6
struct dhcp_lease *lease6_allocate(struct in6_addr *addrp, int lease_type);
struct dhcp_lease *lease6_find(unsigned char *clid, int clid_len, 
			       int lease_type, int iaid, struct in6_addr *addr);
void lease6_filter(int lease_type, int iaid, struct dhcp_context *context);
struct dhcp_lease *lease6_find_by_addr(struct in6_addr *net, int prefix, u64 addr);
u64 lease_find_max_addr6(struct dhcp_context *context);
void lease_ping_reply(struct in6_addr *sender, unsigned char *packet, char *interface);
#endif
void lease_set_hwaddr(struct dhcp_lease *lease, unsigned char *hwaddr,
		      unsigned char *clid, int hw_len, int hw_type, int clid_len, time_t now, int force);
void lease_set_hostname(struct dhcp_lease *lease, char *name, int auth, char *domain, char *config_domain);
void lease_set_expires(struct dhcp_lease *lease, unsigned int len, time_t now);
void lease_set_interface(struct dhcp_lease *lease, int interface, time_t now);
struct dhcp_lease *lease_find_by_client(unsigned char *hwaddr, int hw_len, int hw_type,  
					unsigned char *clid, int clid_len);
struct dhcp_lease *lease_find_by_addr(struct in_addr addr);
struct in_addr lease_find_max_addr(struct dhcp_context *context);
void lease_prune(struct dhcp_lease *target, time_t now);
void lease_update_from_configs(void);
int do_script_run(time_t now);
void rerun_scripts(void);
void lease_find_interfaces(time_t now);
#ifdef HAVE_SCRIPT
void lease_add_extradata(struct dhcp_lease *lease, unsigned char *data, 
			 unsigned int len, int delim);
#endif
#endif

/* rfc2131.c */
#ifdef HAVE_DHCP
size_t dhcp_reply(struct dhcp_context *context, char *iface_name, int int_index,
		  size_t sz, time_t now, int unicast_dest, int *is_inform, int pxe_fd, struct in_addr fallback);
unsigned char *extended_hwaddr(int hwtype, int hwlen, unsigned char *hwaddr, 
			       int clid_len, unsigned char *clid, int *len_out);
#endif

/* dnsmasq.c */
#ifdef HAVE_DHCP
int make_icmp_sock(void);
int icmp_ping(struct in_addr addr);
#endif
void send_alarm(time_t event, time_t now);
void send_event(int fd, int event, int data, char *msg);
void clear_cache_and_reload(time_t now);
void poll_resolv(int force, int do_reload, time_t now);

/* netlink.c */
#ifdef HAVE_LINUX_NETWORK
void netlink_init(void);
void netlink_multicast(void);
#endif

/* bpf.c */
#ifdef HAVE_BSD_NETWORK
void init_bpf(void);
void send_via_bpf(struct dhcp_packet *mess, size_t len,
		  struct in_addr iface_addr, struct ifreq *ifr);
#endif

/* bpf.c or netlink.c */
int iface_enumerate(int family, void *parm, int (callback)());

/* dbus.c */
#ifdef HAVE_DBUS
char *dbus_init(void);
void check_dbus_listeners(fd_set *rset, fd_set *wset, fd_set *eset);
void set_dbus_listeners(int *maxfdp, fd_set *rset, fd_set *wset, fd_set *eset);
#  ifdef HAVE_DHCP
void emit_dbus_signal(int action, struct dhcp_lease *lease, char *hostname);
#  endif
#endif

/* helper.c */
#if defined(HAVE_SCRIPT)
int create_helper(int event_fd, int err_fd, uid_t uid, gid_t gid, long max_fd);
void helper_write(void);
void queue_script(int action, struct dhcp_lease *lease, 
		  char *hostname, time_t now);
#ifdef HAVE_TFTP
void queue_tftp(off_t file_len, char *filename, union mysockaddr *peer);
#endif
int helper_buf_empty(void);
#endif

/* tftp.c */
#ifdef HAVE_TFTP
void tftp_request(struct listener *listen, time_t now);
void check_tftp_listeners(fd_set *rset, time_t now);
int do_tftp_script_run(void);
#endif

/* conntrack.c */
#ifdef HAVE_CONNTRACK
int get_incoming_mark(union mysockaddr *peer_addr, struct all_addr *local_addr,
		      int istcp, unsigned int *markp);
#endif

/* dhcp6.c */
#ifdef HAVE_DHCP6
void dhcp6_init(void);
void dhcp6_packet(time_t now);
int address6_allocate(struct dhcp_context *context,  unsigned char *clid, int clid_len, 
		      int serial, struct dhcp_netid *netids, struct in6_addr *ans);
int is_addr_in_context6(struct dhcp_context *context, struct in6_addr *addr);
struct dhcp_context *address6_available(struct dhcp_context *context, 
					struct in6_addr *taddr,
					struct dhcp_netid *netids);
struct dhcp_context *narrow_context6(struct dhcp_context *context, 
				     struct in6_addr *taddr,
				     struct dhcp_netid *netids);
struct dhcp_config *find_config6(struct dhcp_config *configs,
				 struct dhcp_context *context,
				 unsigned char *duid, int duid_len,
				 char *hostname);
struct dhcp_config *config_find_by_address6(struct dhcp_config *configs, struct in6_addr *net, 
					    int prefix, u64 addr);
void make_duid(time_t now);
#endif

/* rfc3315.c */
#ifdef HAVE_DHCP6
unsigned short dhcp6_reply(struct dhcp_context *context, int interface, char *iface_name,  
			   struct in6_addr *fallback, size_t sz, int is_multicast, time_t now);
#endif

/* dhcp-common.c */
#ifdef HAVE_DHCP
void dhcp_common_init(void);
ssize_t recv_dhcp_packet(int fd, struct msghdr *msg);
struct dhcp_netid *run_tag_if(struct dhcp_netid *input);
struct dhcp_netid *option_filter(struct dhcp_netid *tags, struct dhcp_netid *context_tags,
				 struct dhcp_opt *opts);
int match_netid(struct dhcp_netid *check, struct dhcp_netid *pool, int negonly);
char *strip_hostname(char *hostname);
void log_tags(struct dhcp_netid *netid, u32 xid);
int match_bytes(struct dhcp_opt *o, unsigned char *p, int len);
void dhcp_update_configs(struct dhcp_config *configs);
void display_opts(void);
u16 lookup_dhcp_opt(int prot, char *name);
u16 lookup_dhcp_len(int prot, u16 val);
char *option_string(int prot, unsigned int opt, unsigned char *val, 
		    int opt_len, char *buf, int buf_len);
#ifdef HAVE_LINUX_NETWORK
void bindtodevice(int fd);
#endif
#  ifdef HAVE_DHCP6
void display_opts6(void);
void join_multicast(void);
#  endif
#endif

/* outpacket.c */
#ifdef HAVE_DHCP6
void end_opt6(int container);
int save_counter(int newval);
void *expand(size_t headroom);
int new_opt6(int opt);
void *put_opt6(void *data, size_t len);
void put_opt6_long(unsigned int val);
void put_opt6_short(unsigned int val);
void put_opt6_char(unsigned int val);
void put_opt6_string(char *s);
#endif

/* radv.c */
#ifdef HAVE_DHCP6
void ra_init(time_t now);
void icmp6_packet(void);
time_t periodic_ra(time_t now);
void ra_start_unsolicted(time_t now, struct dhcp_context *context);
#endif

/* slaac.c */ 
#ifdef HAVE_DHCP6
void build_subnet_map(void);
void slaac_add_addrs(struct dhcp_lease *lease, time_t now, int force);
time_t periodic_slaac(time_t now, struct dhcp_lease *leases);
void slaac_ping_reply(struct in6_addr *sender, unsigned char *packet, char *interface, struct dhcp_lease *leases);
void schedule_subnet_map(void);
#endif
