/*
 *  Host Resources
 *      Device index manipulation data
 */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <sys/types.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/snmp_agent.h>
#include <net-snmp/agent/agent_handler.h>
#include <net-snmp/agent/snmp_vars.h>
#include <net-snmp/agent/var_struct.h>

#if defined(NETSNMP_IFNET_NEEDS_KERNEL) && !defined(_KERNEL)
#define _KERNEL 1
#define _I_DEFINED_KERNEL
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NET_IF_H
#include <net/if.h>
#endif
#ifdef _I_DEFINED_KERNEL
#undef _KERNEL
#endif
#if HAVE_NET_IF_VAR_H
#include <net/if_var.h>
#endif
#if HAVE_NETINET_IN_VAR_H
#ifdef HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#endif
#include <netinet/in_var.h>
#endif

/*
 * #include "snmp_vars.linux.h" 
 */

                /*
                 * Deliberately set to the same values as hrDeviceTypes 
                 */
#define	HRDEV_OTHER	1
#define	HRDEV_UNKNOWN	2
#define	HRDEV_PROC	3
#define	HRDEV_NETWORK	4
#define	HRDEV_PRINTER	5
#define	HRDEV_DISK	6
#define	HRDEV_VIDEO	10
#define	HRDEV_AUDIO	11
#define	HRDEV_COPROC	12
#define	HRDEV_KEYBOARD	13
#define	HRDEV_MODEM	14
#define	HRDEV_PARALLEL	15
#define	HRDEV_POINTER	16
#define	HRDEV_SERIAL	17
#define	HRDEV_TAPE	18
#define	HRDEV_CLOCK	19
#define	HRDEV_VMEM	20
#define	HRDEV_NVMEM	21

#define	HRDEV_TYPE_MAX	22      /* one greater than largest device type */
#define	HRDEV_TYPE_SHIFT  16
#define	HRDEV_TYPE_MASK 0xffff

typedef void    (*PFV) (void);
typedef int     (*PFI) (int);
typedef int     (*PFIV) (void);
typedef const char *(*PFS) (int);
typedef oid    *(*PFO) (int, size_t *);

extern PFV      init_device[];  /* Routines for stepping through devices */
extern PFIV     next_device[];
extern PFV      save_device[];
extern int      dev_idx_inc[];  /* Flag - are indices returned in strictly
                                 * increasing order */

extern PFS      device_descr[]; /* Return data for a particular device */
extern PFO      device_prodid[];
extern PFI      device_status[];
extern PFI      device_errors[];
