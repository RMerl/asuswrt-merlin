/* dnsmasq is Copyright (c) 2000-2014 Simon Kelley

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

#define FTABSIZ 150 /* max number of outstanding requests (default) */
#define MAX_PROCS 20 /* max no children for TCP requests */
#define CHILD_LIFETIME 150 /* secs 'till terminated (RFC1035 suggests > 120s) */
#define TCP_MAX_QUERIES 100 /* Maximum number of queries per incoming TCP connection */
#define EDNS_PKTSZ 4096 /* default max EDNS.0 UDP packet from RFC5625 */
#define KEYBLOCK_LEN 40 /* choose to mininise fragmentation when storing DNSSEC keys */
#define DNSSEC_WORK 50 /* Max number of queries to validate one question */
#define TIMEOUT 10 /* drop UDP queries after TIMEOUT seconds */
#define FORWARD_TEST 50 /* try all servers every 50 queries */
#define FORWARD_TIME 20 /* or 20 seconds */
#define RANDOM_SOCKS 64 /* max simultaneous random ports */
#define LEASE_RETRY 60 /* on error, retry writing leasefile after LEASE_RETRY seconds */
#define CACHESIZ 150 /* default cache size */
#define TTL_FLOOR_LIMIT 3600 /* don't allow --min-cache-ttl to raise TTL above this under any circumstances */
#define MAXLEASES 1000 /* maximum number of DHCP leases */
#define PING_WAIT 3 /* wait for ping address-in-use test */
#define PING_CACHE_TIME 30 /* Ping test assumed to be valid this long. */
#define DECLINE_BACKOFF 600 /* disable DECLINEd static addresses for this long */
#define DHCP_PACKET_MAX 16384 /* hard limit on DHCP packet size */
#define SMALLDNAME 50 /* most domain names are smaller than this */
#define CNAME_CHAIN 10 /* chains longer than this atr dropped for loop protection */
#define HOSTSFILE "/etc/hosts"
#define ETHERSFILE "/etc/ethers"
#define DEFLEASE 3600 /* default lease time, 1 hour */
#define CHUSER "nobody"
#define CHGRP "dip"
#define TFTP_MAX_CONNECTIONS 50 /* max simultaneous connections */
#define LOG_MAX 5 /* log-queue length */
#define RANDFILE "/dev/urandom"
#define DNSMASQ_SERVICE "uk.org.thekelleys.dnsmasq" /* Default - may be overridden by config */
#define DNSMASQ_PATH "/uk/org/thekelleys/dnsmasq"
#define AUTH_TTL 600 /* default TTL for auth DNS */
#define SOA_REFRESH 1200 /* SOA refresh default */
#define SOA_RETRY 180 /* SOA retry default */
#define SOA_EXPIRY 1209600 /* SOA expiry default */
#define LOOP_TEST_DOMAIN "test" /* domain for loop testing, "test" is reserved by RFC 2606 and won't therefore clash */
#define LOOP_TEST_TYPE T_TXT
 
/* compile-time options: uncomment below to enable or do eg.
   make COPTS=-DHAVE_BROKEN_RTC

HAVE_BROKEN_RTC
   define this on embedded systems which don't have an RTC
   which keeps time over reboots. Causes dnsmasq to use uptime
   for timing, and keep lease lengths rather than expiry times
   in its leases file. This also make dnsmasq "flash disk friendly".
   Normally, dnsmasq tries very hard to keep the on-disk leases file
   up-to-date: rewriting it after every renewal.  When HAVE_BROKEN_RTC 
   is in effect, the lease file is only written when a new lease is 
   created, or an old one destroyed. (Because those are the only times 
   it changes.) This vastly reduces the number of file writes, and makes
   it viable to keep the lease file on a flash filesystem.
   NOTE: when enabling or disabling this, be sure to delete any old
   leases file, otherwise dnsmasq may get very confused.

HAVE_LEASEFILE_EXPIRE
   define this if you want to enable lease file update with expire
   timeouts instead of expiry times or lease lengths, if HAVE_BROKEN_RTC
   is also enabled. Lease file will be rewritten upon SIGUSR2 signal
   reception and/or dnsmasq termination.

HAVE_TFTP
   define this to get dnsmasq's built-in TFTP server.

HAVE_DHCP
   define this to get dnsmasq's DHCPv4 server.

HAVE_DHCP6
   define this to get dnsmasq's DHCPv6 server. (implies HAVE_DHCP).

HAVE_SCRIPT
   define this to get the ability to call scripts on lease-change.

HAVE_LUASCRIPT
   define this to get the ability to call Lua script on lease-change. (implies HAVE_SCRIPT) 

HAVE_DBUS
   define this if you want to link against libdbus, and have dnsmasq
   support some methods to allow (re)configuration of the upstream DNS 
   servers via DBus.

HAVE_IDN
   define this if you want international domain name support.
   NOTE: for backwards compatibility, IDN support is automatically 
         included when internationalisation support is built, using the 
	 *-i18n makefile targets, even if HAVE_IDN is not explicitly set.

HAVE_CONNTRACK
   define this to include code which propogates conntrack marks from
   incoming DNS queries to the corresponding upstream queries. This adds
   a build-dependency on libnetfilter_conntrack, but the resulting binary will
   still run happily on a kernel without conntrack support.

HAVE_IPSET
    define this to include the ability to selectively add resolved ip addresses
    to given ipsets.

HAVE_AUTH
   define this to include the facility to act as an authoritative DNS
   server for one or more zones.

HAVE_DNSSEC
   include DNSSEC validator.

HAVE_LOOP
   include functionality to probe for and remove DNS forwarding loops.

HAVE_INOTIFY
   use the Linux inotify facility to efficiently re-read configuration files.

NO_IPV6
NO_TFTP
NO_DHCP
NO_DHCP6
NO_SCRIPT
NO_LARGEFILE
NO_AUTH
NO_INOTIFY
   these are avilable to explictly disable compile time options which would 
   otherwise be enabled automatically (HAVE_IPV6, >2Gb file sizes) or 
   which are enabled  by default in the distributed source tree. Building dnsmasq
   with something like "make COPTS=-DNO_SCRIPT" will do the trick.

NO_NETTLE_ECC
   Don't include the ECDSA cypher in DNSSEC validation. Needed for older Nettle versions.
NO_GMP
   Don't use and link against libgmp, Useful if nettle is built with --enable-mini-gmp.

LEASEFILE
CONFFILE
RESOLVFILE
   the default locations of these files are determined below, but may be overridden 
   in a build command line using COPTS.

*/

/* Defining this builds a binary which handles time differently and works better on a system without a 
   stable RTC (it uses uptime, not epoch time) and writes the DHCP leases file less often to avoid flash wear. 
*/

/* #define HAVE_BROKEN_RTC */
/* #define HAVE_LEASEFILE_EXPIRE */

/* The default set of options to build. Built with these options, dnsmasq
   has no library dependencies other than libc */

#define HAVE_DHCP
#define HAVE_DHCP6 
#define HAVE_TFTP
#define HAVE_SCRIPT
#define HAVE_AUTH
#define HAVE_IPSET 
#define HAVE_LOOP

/* Build options which require external libraries.
   
   Defining HAVE_<opt>_STATIC as _well_ as HAVE_<opt> will link the library statically.

   You can use "make COPTS=-DHAVE_<opt>" instead of editing these.
*/

/* #define HAVE_LUASCRIPT */
/* #define HAVE_DBUS */
/* #define HAVE_IDN */
/* #define HAVE_CONNTRACK */
/* #define HAVE_DNSSEC */


/* Default locations for important system files. */

#ifndef LEASEFILE
#   if defined(__FreeBSD__) || defined (__OpenBSD__) || defined(__DragonFly__) || defined(__NetBSD__)
#      define LEASEFILE "/var/db/dnsmasq.leases"
#   elif defined(__sun__) || defined (__sun)
#      define LEASEFILE "/var/cache/dnsmasq.leases"
#   elif defined(__ANDROID__)
#      define LEASEFILE "/data/misc/dhcp/dnsmasq.leases"
#   else
#      define LEASEFILE "/var/lib/misc/dnsmasq.leases"
#   endif
#endif

#ifndef CONFFILE
#   if defined(__FreeBSD__)
#      define CONFFILE "/usr/local/etc/dnsmasq.conf"
#   else
#      define CONFFILE "/etc/dnsmasq.conf"
#   endif
#endif

#ifndef RESOLVFILE
#   if defined(__uClinux__)
#      define RESOLVFILE "/etc/config/resolv.conf"
#   else
#      define RESOLVFILE "/etc/resolv.conf"
#   endif
#endif

#ifndef RUNFILE
#   if defined(__ANDROID__)
#      define RUNFILE "/data/dnsmasq.pid"
#    else
#      define RUNFILE "/var/run/dnsmasq.pid"
#    endif
#endif

/* platform dependent options: these are determined automatically below

HAVE_LINUX_NETWORK
HAVE_BSD_NETWORK
HAVE_SOLARIS_NETWORK
   define exactly one of these to alter interaction with kernel networking.

HAVE_GETOPT_LONG
   defined when GNU-style getopt_long available. 

HAVE_SOCKADDR_SA_LEN
   defined if struct sockaddr has sa_len field (*BSD) 
*/

/* Must preceed __linux__ since uClinux defines __linux__ too. */
#if defined(__uClinux__)
#define HAVE_LINUX_NETWORK
#define HAVE_GETOPT_LONG
#undef HAVE_SOCKADDR_SA_LEN
/* Never use fork() on uClinux. Note that this is subtly different from the
   --keep-in-foreground option, since it also  suppresses forking new 
   processes for TCP connections and disables the call-a-script on leasechange
   system. It's intended for use on MMU-less kernels. */
#define NO_FORK

#elif defined(__UCLIBC__)
#define HAVE_LINUX_NETWORK
#if defined(__UCLIBC_HAS_GNU_GETOPT__) || \
   ((__UCLIBC_MAJOR__==0) && (__UCLIBC_MINOR__==9) && (__UCLIBC_SUBLEVEL__<21))
#    define HAVE_GETOPT_LONG
#endif
#undef HAVE_SOCKADDR_SA_LEN
#if !defined(__ARCH_HAS_MMU__) && !defined(__UCLIBC_HAS_MMU__)
#  define NO_FORK
#endif
#if defined(__UCLIBC_HAS_IPV6__) && defined(USE_IPV6)
#  ifndef IPV6_V6ONLY
#    define IPV6_V6ONLY 26
#  endif
#elif !defined(NO_IPV6)
#  define NO_IPV6
#endif

/* This is for glibc 2.x */
#elif defined(__linux__)
#define HAVE_LINUX_NETWORK
#define HAVE_GETOPT_LONG
#undef HAVE_SOCKADDR_SA_LEN

#elif defined(__FreeBSD__) || \
      defined(__OpenBSD__) || \
      defined(__DragonFly__) || \
      defined(__FreeBSD_kernel__)
#define HAVE_BSD_NETWORK
/* Later verions of FreeBSD have getopt_long() */
#if defined(optional_argument) && defined(required_argument)
#   define HAVE_GETOPT_LONG
#endif
#define HAVE_SOCKADDR_SA_LEN

#elif defined(__APPLE__)
#define HAVE_BSD_NETWORK
#define HAVE_GETOPT_LONG
#define HAVE_SOCKADDR_SA_LEN
/* Define before sys/socket.h is included so we get socklen_t */
#define _BSD_SOCKLEN_T_
/* Select the RFC_3542 version of the IPv6 socket API. 
   Define before netinet6/in6.h is included. */
#define __APPLE_USE_RFC_3542 
#define NO_IPSET

#elif defined(__NetBSD__)
#define HAVE_BSD_NETWORK
#define HAVE_GETOPT_LONG
#define HAVE_SOCKADDR_SA_LEN

#elif defined(__sun) || defined(__sun__)
#define HAVE_SOLARIS_NETWORK
#define HAVE_GETOPT_LONG
#undef HAVE_SOCKADDR_SA_LEN
#define ETHER_ADDR_LEN 6 
 
#endif

/* Decide if we're going to support IPv6 */
/* We assume that systems which don't have IPv6
   headers don't have ntop and pton either */

#if defined(INET6_ADDRSTRLEN) && defined(IPV6_V6ONLY)
#  define HAVE_IPV6
#  define ADDRSTRLEN INET6_ADDRSTRLEN
#else
#  if !defined(INET_ADDRSTRLEN)
#      define INET_ADDRSTRLEN 16 /* 4*3 + 3 dots + NULL */
#  endif
#  undef HAVE_IPV6
#  define ADDRSTRLEN INET_ADDRSTRLEN
#endif


/* rules to implement compile-time option dependencies and 
   the NO_XXX flags */

#ifdef NO_IPV6
#undef HAVE_IPV6
#endif

#ifdef NO_TFTP
#undef HAVE_TFTP
#endif

#ifdef NO_DHCP
#undef HAVE_DHCP
#undef HAVE_DHCP6
#endif

#if defined(NO_DHCP6) || !defined(HAVE_IPV6)
#undef HAVE_DHCP6
#endif

/* DHCP6 needs DHCP too */
#ifdef HAVE_DHCP6
#define HAVE_DHCP
#endif

#if defined(NO_SCRIPT) || !defined(HAVE_DHCP) || defined(NO_FORK)
#undef HAVE_SCRIPT
#undef HAVE_LUASCRIPT
#endif

/* Must HAVE_SCRIPT to HAVE_LUASCRIPT */
#ifdef HAVE_LUASCRIPT
#define HAVE_SCRIPT
#endif

#ifdef NO_AUTH
#undef HAVE_AUTH
#endif

#if defined(NO_IPSET)
#undef HAVE_IPSET
#endif

#ifdef NO_LOOP
#undef HAVE_LOOP
#endif

#if defined (HAVE_LINUX_NETWORK) && !defined(NO_INOTIFY)
#define HAVE_INOTIFY
#endif

/* Define a string indicating which options are in use.
   DNSMASQP_COMPILE_OPTS is only defined in dnsmasq.c */

#ifdef DNSMASQ_COMPILE_OPTS

static char *compile_opts = 
#ifndef HAVE_IPV6
"no-"
#endif
"IPv6 "
#ifndef HAVE_GETOPT_LONG
"no-"
#endif
"GNU-getopt "
#ifdef HAVE_BROKEN_RTC
"no-RTC "
#endif
#ifdef NO_FORK
"no-MMU "
#endif
#ifndef HAVE_DBUS
"no-"
#endif
"DBus "
#ifndef LOCALEDIR
"no-"
#endif
"i18n "
#if !defined(LOCALEDIR) && !defined(HAVE_IDN)
"no-"
#endif 
"IDN "
#ifndef HAVE_DHCP
"no-"
#endif
"DHCP "
#if defined(HAVE_DHCP)
#  if !defined (HAVE_DHCP6)
     "no-"
#  endif  
     "DHCPv6 "
#  if !defined(HAVE_SCRIPT)
     "no-scripts "
#  else
#    if !defined(HAVE_LUASCRIPT)
       "no-"
#    endif
     "Lua "
#  endif
#endif
#ifndef HAVE_TFTP
"no-"
#endif
"TFTP "
#ifndef HAVE_CONNTRACK
"no-"
#endif
"conntrack "
#ifndef HAVE_IPSET
"no-"
#endif
"ipset "
#ifndef HAVE_AUTH
"no-"
#endif
"auth "
#ifndef HAVE_DNSSEC
"no-"
#endif
"DNSSEC "
#ifndef HAVE_LOOP
"no-"
#endif
"loop-detect "
#ifndef HAVE_INOTIFY
"no-"
#endif
"inotify";


#endif



