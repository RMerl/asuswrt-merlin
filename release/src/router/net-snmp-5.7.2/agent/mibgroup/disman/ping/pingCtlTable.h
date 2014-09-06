/*
 *Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
 *
 *All right reserved
 *
 *File Name:pingCtlTable.h
 *File Description:The head file of pingCtlTable.c
 *
 *Current Version:1.0
 *Author:ChenJing
 *Date:2004.8.20
 */


#ifndef PINGCTLTABLE_H
#define PINGCTLTABLE_H

#include	<sys/types.h>   /* basic system data types */
#include	<sys/socket.h>  /* basic socket definitions */
#include	<sys/time.h>    /* timeval{} for select() */
#include	<time.h>        /* timespec{} for pselect() */
#include	<netinet/in.h>  /* sockaddr_in{} and other Internet defns */
#include	<arpa/inet.h>   /* inet(3) functions */
#include	<errno.h>
#include	<fcntl.h>       /* for nonblocking */
#include	<netdb.h>
#include	<signal.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<sys/stat.h>    /* for S_xxx file mode constants */
#include	<sys/uio.h>     /* for iovec{} and readv/writev */
#include	<unistd.h>
#include	<sys/wait.h>
#include	<sys/un.h>      /* for Unix domain sockets */
#include <netdb.h>
#include <pthread.h>

#ifdef	HAVE_SYS_SELECT_H
# include	<sys/select.h>  /* for convenience */
#endif

#ifdef	HAVE_POLL_H
# include	<poll.h>        /* for convenience */
#endif

#ifdef	HAVE_STRINGS_H
# include	<strings.h>     /* for convenience */
#endif

/*
 * Three headers are normally needed for socket/file ioctl's:
 * * <sys/ioctl.h>, <sys/filio.h>, and <sys/sockio.h>.
 */
#ifdef	HAVE_SYS_IOCTL_H
# include	<sys/ioctl.h>
#endif
#ifdef	HAVE_SYS_FILIO_H
# include	<sys/filio.h>
#endif
#ifdef	HAVE_SYS_SOCKIO_H
# include	<sys/sockio.h>
#endif

#ifdef	HAVE_PTHREAD_H
# include	<pthread.h>
#endif

#ifdef	HAVE_SOCKADDR_DL_STRUCT
# include	<net/if_dl.h>
#endif

#include	<netinet/in_systm.h>
#include	<netinet/ip.h>
#include	<netinet/ip_icmp.h>

#define	BUFSIZE		1500

/*
 * ipv6 include   
 */
#include <sys/param.h>
#include <linux/sockios.h>
#include <sys/file.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/poll.h>
#include <linux/types.h>
#include <ctype.h>
#include <linux/errqueue.h>

#include <sched.h>

#include <netinet/ip6.h>
#include <netinet/icmp6.h>


/*
 * ipv4 include 
 */

#include	<netinet/in_systm.h>
#include	<netinet/ip.h>

#include	<netinet/ip_icmp.h>
#include	<netinet/in.h>  /* sockaddr_in{} and other Internet defns */


config_require(header_complex);

struct pingCtlTable_data {
    char           *pingCtlOwnerIndex;  /* string */
    size_t          pingCtlOwnerIndexLen;

    char           *pingCtlTestName;    /* string */
    size_t          pingCtlTestNameLen;

    long            pingCtlTargetAddressType;   /* integer32 */

    char           *pingCtlTargetAddress;       /* string */
    size_t          pingCtlTargetAddressLen;

    unsigned long   pingCtlDataSize;
    unsigned long   pingCtlTimeOut;
    unsigned long   pingCtlProbeCount;
    long            pingCtlAdminStatus;
    char           *pingCtlDataFill;
    size_t          pingCtlDataFillLen;

    unsigned long   pingCtlFrequency;
    unsigned long   pingCtlMaxRows;
    long            pingCtlStorageType;

    char           *pingCtlTrapGeneration;
    size_t          pingCtlTrapGenerationLen;
    unsigned long   pingCtlTrapProbeFailureFilter;
    unsigned long   pingCtlTrapTestFailureFilter;
    oid            *pingCtlType;
    size_t          pingCtlTypeLen;

    char           *pingCtlDescr;
    size_t          pingCtlDescrLen;
    long            pingCtlSourceAddressType;
    char           *pingCtlSourceAddress;
    size_t          pingCtlSourceAddressLen;
    long            pingCtlIfIndex;
    long            pingCtlByPassRouteTable;
    unsigned long   pingCtlDSField;
    long            pingCtlRowStatus;

    int             storageType;
    u_long          timer_id;
    unsigned long   pingProbeHistoryMaxIndex;

    struct pingResultsTable_data *pingResults;
    struct pingProbeHistoryTable_data *pingProbeHis;

};

struct pingResultsTable_data {
    char           *pingCtlOwnerIndex;  /* string */
    size_t          pingCtlOwnerIndexLen;
    char           *pingCtlTestName;    /* string */
    size_t          pingCtlTestNameLen;

    long            pingResultsOperStatus;
    long            pingResultsIpTargetAddressType;
    char           *pingResultsIpTargetAddress;
    size_t          pingResultsIpTargetAddressLen;
    unsigned long   pingResultsMinRtt;
    unsigned long   pingResultsMaxRtt;
    unsigned long   pingResultsAverageRtt;
    unsigned long   pingResultsProbeResponses;
    unsigned long   pingResultsSendProbes;
    unsigned long   pingResultsRttSumOfSquares;
    u_char         *pingResultsLastGoodProbe;
    size_t          pingResultsLastGoodProbeLen;
    time_t          pingResultsLastGoodProbe_time;

    int             storageType;

};

struct pingProbeHistoryTable_data {
    struct pingProbeHistoryTable_data *next;
    char           *pingCtlOwnerIndex;  /* string */
    size_t          pingCtlOwnerIndexLen;
    char           *pingCtlTestName;    /* string */
    size_t          pingCtlTestNameLen;

    unsigned long   pingProbeHistoryIndex;
    unsigned long   pingProbeHistoryResponse;
    long            pingProbeHistoryStatus;
    long            pingProbeHistoryLastRC;
    u_char         *pingProbeHistoryTime;
    size_t          pingProbeHistoryTimeLen;
    time_t          pingProbeHistoryTime_time;

    int             storageType;

};


/*
 * function declarations 
 */
void            init_pingCtlTable(void);
void            shutdown_pingCtlTable(void);
FindVarMethod   var_pingCtlTable;
void            parse_pingCtlTable(const char *, char *);
SNMPCallback    store_pingCtlTable;


WriteMethod     write_pingCtlTargetAddressType;
WriteMethod     write_pingCtlTargetAddress;
WriteMethod     write_pingCtlDataSize;
WriteMethod     write_pingCtlTimeOut;
WriteMethod     write_pingCtlProbeCount;
WriteMethod     write_pingCtlAdminStatus;
WriteMethod     write_pingCtlDataFill;
WriteMethod     write_pingCtlFrequency;
WriteMethod     write_pingCtlMaxRows;
WriteMethod     write_pingCtlStorageType;
WriteMethod     write_pingCtlTrapGeneration;
WriteMethod     write_pingCtlTrapProbeFailureFilter;
WriteMethod     write_pingCtlTrapTestFailureFilter;
WriteMethod     write_pingCtlType;
WriteMethod     write_pingCtlDescr;
WriteMethod     write_pingCtlSourceAddressType;
WriteMethod     write_pingCtlSourceAddress;
WriteMethod     write_pingCtlIfIndex;
WriteMethod     write_pingCtlByPassRouteTable;
WriteMethod     write_pingCtlDSField;

WriteMethod     write_pingCtlRowStatus;



#define PINGTRAPGENERATION_PROBEFAILED                 	0x80
#define PINGTRAPGENERATION_TESTFAILED                  	0x40
#define PINGTRAPGENERATION_TESTCOMPLETED          	0x20
#define PINGTRAPGENERATION_NULL					0x00

/*
 * column number definitions for table pingCtlTable 
 */
#define COLUMN_PINGCTLOWNERINDEX		1
#define COLUMN_PINGCTLTESTNAME		2
#define COLUMN_PINGCTLTARGETADDRESSTYPE		3
#define COLUMN_PINGCTLTARGETADDRESS		4
#define COLUMN_PINGCTLDATASIZE		5
#define COLUMN_PINGCTLTIMEOUT		6
#define COLUMN_PINGCTLPROBECOUNT		7
#define COLUMN_PINGCTLADMINSTATUS		8
#define COLUMN_PINGCTLDATAFILL		9
#define COLUMN_PINGCTLFREQUENCY		10
#define COLUMN_PINGCTLMAXROWS		11
#define COLUMN_PINGCTLSTORAGETYPE		12
#define COLUMN_PINGCTLTRAPGENERATION		13
#define COLUMN_PINGCTLTRAPPROBEFAILUREFILTER		14
#define COLUMN_PINGCTLTRAPTESTFAILUREFILTER		15
#define COLUMN_PINGCTLTYPE		16
#define COLUMN_PINGCTLDESCR		17
#define COLUMN_PINGCTLSOURCEADDRESSTYPE		18
#define COLUMN_PINGCTLSOURCEADDRESS		19
#define COLUMN_PINGCTLIFINDEX		20
#define COLUMN_PINGCTLBYPASSROUTETABLE		21
#define COLUMN_PINGCTLDSFIELD		22
#define COLUMN_PINGCTLROWSTATUS		23


/*
 * ipv4 function  
 */
int             proc_v4(char *, ssize_t, struct timeval *, time_t,
                        struct pingCtlTable_data *, struct addrinfo *, int,
                        unsigned long *, unsigned long *, unsigned long *,
                        unsigned long *, unsigned long, int, int, int,
                        struct pingProbeHistoryTable_data *, pid_t);
void            send_v4(int, pid_t, int, int, char *);
void            readloop(struct pingCtlTable_data *, struct addrinfo *,
                         int, unsigned long *, unsigned long *,
                         unsigned long *, pid_t);
void            sig_alrm(int);
void            tv_sub(struct timeval *, struct timeval *);
unsigned long   round_double(double);
struct proto {
    int             (*fproc) (char *, ssize_t, struct timeval *, time_t,
                              struct pingCtlTable_data *,
                              struct addrinfo *, int, unsigned long *,
                              unsigned long *, unsigned long *,
                              unsigned long *, unsigned long, int, int,
                              int, struct pingProbeHistoryTable_data *,
                              pid_t);
    void            (*fsend) (int, pid_t, int, int, char *);
    struct sockaddr *sasend;    /* sockaddr{} for send, from getaddrinfo */
    struct sockaddr *sarecv;    /* sockaddr{} for receiving */
    socklen_t       salen;      /* length of sockaddr{}s */
    int             icmpproto;  /* IPPROTO_xxx value for ICMP */
}              *pr;


/*
 * ipv6 function 
 */

#define BIT_CLEAR(nr, addr) do { ((__u32 *)(addr))[(nr) >> 5] &= ~(1U << ((nr) & 31)); } while(0)
#define BIT_SET(nr, addr) do { ((__u32 *)(addr))[(nr) >> 5] |= (1U << ((nr) & 31)); } while(0)
#define BIT_TEST(nr, addr) do { (__u32 *)(addr))[(nr) >> 5] & (1U << ((nr) & 31)); } while(0)

#define ICMPV6_FILTER_WILLPASS(type, filterp) \
	(BIT_TEST((type), filterp) == 0)

#define ICMPV6_FILTER_WILLBLOCK(type, filterp) \
	BIT_TEST((type), filterp)

#define ICMPV6_FILTER_SETPASS(type, filterp) \
	BIT_CLEAR((type), filterp)

#define ICMPV6_FILTER_SETBLOCK(type, filterp) \
	BIT_SET((type), filterp)

#define ICMPV6_FILTER_SETPASSALL(filterp) \
	memset(filterp, 0, sizeof(struct icmp6_filter));

#define ICMPV6_FILTER_SETBLOCKALL(filterp) \
	memset(filterp, 0xFF, sizeof(struct icmp6_filter));


#define	MAX_PACKET	128000  /* max packet size */

#ifdef SO_TIMESTAMP
#define HAVE_SIN6_SCOPEID 1
#endif



#define	MAX_DUP_CHK	0x10000
char            rcvd_tbl[MAX_DUP_CHK / 8];

volatile int    exiting;
volatile int    status_snapshot;

#ifndef MSG_CONFIRM
#define MSG_CONFIRM 0
#endif

#define	DEFDATALEN	(64 - 8)        /* default data length */

#define	MAXWAIT		10      /* max seconds to wait for response */
#define MININTERVAL	10      /* Minimal interpacket gap */
#define MINUSERINTERVAL	200     /* Minimal allowed interval for non-root */

#define SCHINT(a)	(((a) <= MININTERVAL) ? MININTERVAL : (a))

#define	A(bit)		rcvd_tbl[(bit)>>3]      /* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))   /* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

/*
 * various options 
 */
#define	F_FLOOD		0x001
#define	F_INTERVAL	0x002
#define	F_NUMERIC	0x004
#define	F_PINGFILLED	0x008
#define	F_QUIET		0x010
#define	F_RROUTE	0x020
#define	F_SO_DEBUG	0x040
#define	F_SO_DONTROUTE	0x080
#define	F_VERBOSE	0x100
#define	F_TIMESTAMP	0x200
#define	F_FLOWINFO	0x200
#define	F_SOURCEROUTE	0x400
#define	F_TCLASS	0x400
#define	F_FLOOD_POLL	0x800
#define	F_LATENCY	0x1000
#define	F_AUDIBLE	0x2000
#define	F_ADAPTIVE	0x4000

/*
 * multicast options 
 */
#define MULTICAST_NOLOOP	0x001
#define MULTICAST_TTL		0x002
#define MULTICAST_IF		0x004

int             __schedule_exit(int, long *, long *);
int             pinger(int, int, int, char *, struct sockaddr_in6 *, int *,
                       int, int, int, int, int, char *, int *, int *,
                       int *, int *, __u16 *, long *, long *, long *,
                       long *, int *, int *, int *, struct timeval *);
void            sock_setbufs(int, int, int);
void            setup(int, int, int, int, int, int, int, char *, int *,
                      struct timeval *, int *, int *);
void            main_loop(struct pingCtlTable_data *, int, int, __u8 *,
                          int, int, char *, struct sockaddr_in6 *, int,
                          int, char *, int, int, int, int, char *, int *,
                          struct timeval *, int *, int *);
int             gather_statistics(int *, struct pingCtlTable_data *,
                                  __u8 *, int, __u16, int, int,
                                  struct timeval *, time_t, int *, int,
                                  int, char *, int, int, int, char *,
                                  int *, __u16 *, long *, long *, long *,
                                  long *, long *, long *, long long *,
                                  long long *, int *, int *, int *,
                                  struct pingProbeHistoryTable_data *);
void            finish(int, char *, int, int, int *, struct timeval *,
                       int *, long *, long *, long *, long *, long *,
                       long *, long *, long *, long long *, long long *,
                       int *, struct timeval *);
void            status(int, int *, long *, long *, long *, long *, long *,
                       long long *, long long *);
size_t          inet6_srcrt_space(int, int);
struct cmsghdr *inet6_srcrt_init(void *, int);
int             inet6_srcrt_add(struct cmsghdr *, const struct in6_addr *);
int             receive_error_msg(int, struct sockaddr_in6 *, int, int *,
                                  long *);
int             send_v6(int, int, char *, struct sockaddr_in6 *, int, int,
                        char *, int *, long *, int *);
int             parse_reply(int *, struct pingCtlTable_data *,
                            struct msghdr *, int, void *, struct timeval *,
                            time_t, int, struct sockaddr_in6 *, int *, int,
                            int, int, int, int, char *, int *, int *,
                            __u16 *, long *, long *, long *, long *,
                            long *, long *, long *, long long *,
                            long long *, int *, int *, int *,
                            struct pingProbeHistoryTable_data *);
void            install_filter(int, int *);

#endif
/*
 * PINGCTLTABLE_H 
 */
