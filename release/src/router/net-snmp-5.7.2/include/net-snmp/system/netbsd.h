#include "bsd.h"

#include <sys/param.h>

#define PCB_TABLE 1
#undef TCP_SYMBOL
#define TCP_SYMBOL "tcbtable"
#undef TCP_TTL_SYMBOL
#define TCP_TTL_SYMBOL "ip_defttl"
#undef UDB_SYMBOL
#define UDB_SYMBOL "udbtable"
#undef NPROC_SYMBOL
#undef PROC_SYMBOL

#define MBPOOL_SYMBOL	"mbpool"
#define MCLPOOL_SYMBOL	"mclpool"

/*
 * inp_next symbol 
 */
#undef INP_NEXT_SYMBOL
#define INP_NEXT_SYMBOL inp_queue.cqe_next
#undef INP_PREV_SYMBOL
#define INP_PREV_SYMBOL inp_queue.cqe_prev
#define HAVE_INPCBTABLE 1

#if __NetBSD_Version__ >= 106300000       /* NetBSD 1.6ZD */            
#undef IFADDR_SYMBOL
#define IFADDR_SYMBOL "in_ifaddrhead"
#undef TOTAL_MEMORY_SYMBOL
#endif

#define UTMP_FILE _PATH_UTMP

#define UDP_ADDRESSES_IN_HOST_ORDER 1

#ifdef netbsdelf6
#define netbsd6
#define netbsdelf5
#endif
#ifdef netbsdelf5
#define netbsd5
#define netbsdelf4
#endif
#ifdef netbsdelf4
#define netbsd4
#define netbsdelf3
#endif
#ifdef netbsdelf3
#define netbsd3
#endif

#if defined(netbsd6) && !defined(netbsd5)
#define netbsd5 netbsd5
#endif
#if defined(netbsd5) && !defined(netbsd4)
#define netbsd4 netbsd4
#endif
#if defined(netbsd4) && !defined(netbsd3)
#define netbsd3 netbsd3
#endif
#if defined(netbsd3) && !defined(netbsd2)
#define netbsd2 netbsd2
#endif
#ifndef netbsd1
#define netbsd1 netbsd1
#endif

#if __NetBSD_Version__ >= 499005800
#define NETBSD_STATS_VIA_SYSCTL
#endif /* __NetBSD_Version__ >= 499005800 */

/* define the extra mib modules that are supported */
#define NETSNMP_INCLUDE_HOST_RESOURCES
#define NETSNMP_INCLUDE_IFTABLE_REWRITES
