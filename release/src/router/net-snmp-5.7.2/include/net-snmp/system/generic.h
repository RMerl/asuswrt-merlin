#ifndef __NET_SNMP_SYSTEM_GENERIC_H__
#define __NET_SNMP_SYSTEM_GENERIC_H__

#define bsdlike bsdlike

/*
 * nlist symbols in ip.c 
 */
#define IPSTAT_SYMBOL "ipstat"
#define IP_FORWARDING_SYMBOL "ipforwarding"
#define TCP_TTL_SYMBOL "tcpDefaultTTL"

/*
 * nlist symbols in interfaces.c 
 */
#define IFNET_SYMBOL "ifnet"
#define IFADDR_SYMBOL "in_ifaddr"

/*
 * nlist symbols in at.c 
 */
#define ARPTAB_SYMBOL "arptab"
#define ARPTAB_SIZE_SYMBOL "arptab_size"

/*
 * load average lookup symbol 
 */
#define LOADAVE_SYMBOL "avenrun"

/*
 * nlist symbols in hr_proc.c and memory.c 
 */
#define PHYSMEM_SYMBOL "physmem"
#define TOTAL_MEMORY_SYMBOL "total"
#define MBSTAT_SYMBOL "mbstat"
#define SWDEVT_SYMBOL "swdevt"
#define FSWDEVT_SYMBOL "fswdevt"
#define NSWAPFS_SYMBOL "nswapfs"
#define NSWAPDEV_SYMBOL "nswapdev"

/*
 * process nlist symbols. 
 */
#define NPROC_SYMBOL "nproc"
#define PROC_SYMBOL "proc"

/*
 * icmp.c nlist symbols 
 */
#define ICMPSTAT_SYMBOL "icmpstat"

/*
 * tcp.c nlist symbols 
 */
#define TCPSTAT_SYMBOL "tcpstat"
#define TCP_SYMBOL "tcb"

/*
 * upd.c nlist symbols 
 */
#define UDPSTAT_SYMBOL "udpstat"
#define UDB_SYMBOL "udb"

/*
 * var_route.c nlist symbols 
 */
#define RTTABLES_SYMBOL "rt_table"
#define RTHASHSIZE_SYMBOL "rthashsize"
#define RTHOST_SYMBOL "rthost"
#define RTNET_SYMBOL "rtnet"

/*
 * udp_inpcb list symbol 
 */
#define INP_NEXT_SYMBOL inp_next
#define INP_PREV_SYMBOL inp_prev

#endif /* !__NET_SNMP_SYSTEM_GENERIC_H__ */
