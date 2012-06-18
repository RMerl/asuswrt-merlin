/*
**  igmpproxy - IGMP proxy based multicast router 
**  Copyright (C) 2005 Johnny Egeland <johnny@rlo.org>
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**
**----------------------------------------------------------------------------
**
**  This software is derived work from the following software. The original
**  source code has been modified from it's original state by the author
**  of igmpproxy.
**
**  smcroute 0.92 - Copyright (C) 2001 Carsten Schill <carsten@cschill.de>
**  - Licensed under the GNU General Public License, version 2
**  
**  mrouted 3.9-beta3 - COPYRIGHT 1989 by The Board of Trustees of 
**  Leland Stanford Junior University.
**  - Original license can be found in the Stanford.txt file.
**
*/
/**
*   igmpproxy.h - Header file for common includes.
*/

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/param.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "os.h"
#include "config.h"

/*
 * Limit on length of route data
 */
#define MAX_IP_PACKET_LEN	576
#define MIN_IP_HEADER_LEN	20
#define MAX_IP_HEADER_LEN	60
#define IP_HEADER_RAOPT_LEN	24

#define MAX_MC_VIFS    32     // !!! check this const in the specific includes

// Useful macros..          
#define VCMC( Vc )  (sizeof( Vc ) / sizeof( (Vc)[ 0 ] ))
#define VCEP( Vc )  (&(Vc)[ VCMC( Vc ) ])

// Bit manipulation macros...
#define BIT_ZERO(X)      ((X) = 0)
#define BIT_SET(X,n)     ((X) |= 1 << (n))
#define BIT_CLR(X,n)     ((X) &= ~(1 << (n)))
#define BIT_TST(X,n)     ((X) & 1 << (n))


//#################################################################################
//  Globals
//#################################################################################

/*
 * External declarations for global variables and functions.
 */
#define RECV_BUF_SIZE 8192
extern char     *recv_buf;
extern char     *send_buf;

extern char     s1[];
extern char     s2[];
extern char     s3[];
extern char     s4[];



//#################################################################################
//  Lib function prototypes.
//#################################################################################

/* syslog.c
 */
extern bool Log2Stderr;           // Log to stderr instead of to syslog
extern int  LogLevel;             // Log threshold, LOG_WARNING .... LOG_DEBUG 

void my_log( int Serverity, int Errno, const char *FmtSt, ... );

/* ifvc.c
 */
#define MAX_IF         40     // max. number of interfaces recognized 

// Interface states
#define IF_STATE_DISABLED      0   // Interface should be ignored.
#define IF_STATE_UPSTREAM      1   // Interface is the upstream interface
#define IF_STATE_DOWNSTREAM    2   // Interface is a downstream interface

// Multicast default values...
#define DEFAULT_ROBUSTNESS     2
#define DEFAULT_THRESHOLD      1
#define DEFAULT_RATELIMIT      0

// Define timer constants (in seconds...)
#define INTERVAL_QUERY          125
#define INTERVAL_QUERY_RESPONSE  10
//#define INTERVAL_QUERY_RESPONSE  10

#define ROUTESTATE_NOTJOINED            0   // The group corresponding to route is not joined
#define ROUTESTATE_JOINED               1   // The group corresponding to route is joined
#define ROUTESTATE_CHECK_LAST_MEMBER    2   // The router is checking for hosts



// Linked list of networks... 
struct SubnetList {
    uint32_t              subnet_addr;
    uint32_t              subnet_mask;
    struct SubnetList*  next;
};

struct IfDesc {
    char                Name[IF_NAMESIZE];
    struct in_addr      InAdr;          /* == 0 for non IP interfaces */
    short               Flags;
    short               state;
    short               index;
    uint8_t             robustness;
    uint8_t             threshold;   /* ttl limit */
    unsigned int        ratelimit;
    struct SubnetList*  allowednets;
    struct SubnetList*  allowedgroups;
};

// Keeps common configuration settings 
struct Config {
    unsigned int        robustnessValue;
    unsigned int        queryInterval;
    unsigned int        queryResponseInterval;
    // Used on startup..
    unsigned int        startupQueryInterval;
    unsigned int        startupQueryCount;
    // Last member probe...
    unsigned int        lastMemberQueryInterval;
    unsigned int        lastMemberQueryCount;
    // Set if upstream leave messages should be sent instantly..
    unsigned int        fastUpstreamLeave;
};

// Defines the Index of the upstream VIF...
extern int upStreamVif;

/* ifvc.c
 */
void buildIfVc( void );
struct IfDesc *getIfByName( const char *IfName );
struct IfDesc *getIfByIx( unsigned Ix );
struct IfDesc *getIfByAddress( uint32_t ipaddr );
int isAdressValidForIf(struct IfDesc* intrface, uint32_t ipaddr);

/* mroute-api.c
 */
struct MRouteDesc {
    vifi_t          InVif;
    uint8_t         TtlVc[ MAX_MC_VIFS ];
    struct in_addr  OriginAdr, McAdr;
};

// IGMP socket as interface for the mrouted API
// - receives the IGMP messages
extern int MRouterFD;

int enableMRouter( void );
void disableMRouter( void );
void addVIF( struct IfDesc *Dp );
int addMRoute( struct MRouteDesc * Dp );
int delMRoute( struct MRouteDesc * Dp );
int getVifIx( struct IfDesc *IfDp );

/* config.c
 */
int loadConfig(char *configFile);
void configureVifs();
struct Config *getCommonConfig();

/* igmp.c
*/
extern uint32_t allhosts_group;
extern uint32_t allrouters_group;
extern uint32_t alligmp3_group;
void initIgmp(void);
void acceptIgmp(int);
void sendIgmp (uint32_t, uint32_t, int, int, uint32_t,int);

/* lib.c
 */
char   *fmtInAdr( char *St, struct in_addr InAdr );
char   *inetFmt(uint32_t addr, char *s);
char   *inetFmts(uint32_t addr, uint32_t mask, char *s);
uint16_t inetChksum(uint16_t *addr, int len);

/* kern.c
 */
void k_set_rcvbuf(int bufsize, int minsize);
void k_hdr_include(int hdrincl);
void k_set_ttl(int t);
void k_set_loop(int l);
void k_set_if(uint32_t ifa);
/*
void k_join(uint32_t grp, uint32_t ifa);
void k_leave(uint32_t grp, uint32_t ifa);
*/

/* udpsock.c
 */
int openUdpSocket( uint32_t PeerInAdr, uint16_t PeerPort );

/* mcgroup.c
 */
int joinMcGroup( int UdpSock, struct IfDesc *IfDp, uint32_t mcastaddr );
int leaveMcGroup( int UdpSock, struct IfDesc *IfDp, uint32_t mcastaddr );


/* rttable.c
 */
void initRouteTable();
void clearAllRoutes();
int insertRoute(uint32_t group, int ifx);
int activateRoute(uint32_t group, uint32_t originAddr);
void ageActiveRoutes();
void setRouteLastMemberMode(uint32_t group);
int lastMemberGroupAge(uint32_t group);

/* request.c
 */
void acceptGroupReport(uint32_t src, uint32_t group, uint8_t type);
void acceptLeaveMessage(uint32_t src, uint32_t group);
void sendGeneralMembershipQuery();

/* callout.c 
*/
typedef void (*timer_f)(void *);

void callout_init();
void free_all_callouts();
void age_callout_queue(int);
int timer_nextTimer();
int timer_setTimer(int, timer_f, void *);
int timer_clearTimer(int);
int timer_leftTimer(int);

/* confread.c
 */
#define MAX_TOKEN_LENGTH    30

int openConfigFile(char *filename);
void closeConfigFile();
char* nextConfigToken();
char* getCurrentConfigToken();


