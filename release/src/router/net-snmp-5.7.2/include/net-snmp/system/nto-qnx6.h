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

#undef IFADDR_SYMBOL
#define IFADDR_SYMBOL "in_ifaddrhead"

#define UTMP_FILE _PATH_UTMP

#define UDP_ADDRESSES_IN_HOST_ORDER 1

/* define the extra mib modules that are supported */
/* #define NETSNMP_INCLUDE_HOST_RESOURCES */
