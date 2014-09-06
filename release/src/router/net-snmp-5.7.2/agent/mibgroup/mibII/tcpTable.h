/*
 *  Template MIB group interface - tcp.h
 *
 */
#ifndef _MIBGROUP_TCPTABLE_H
#define _MIBGROUP_TCPTABLE_H

config_arch_require(solaris2, kernel_sunos5)
#if !defined(NETSNMP_ENABLE_MFD_REWRITES)
config_require(mibII/ip)
#endif

#ifdef linux
struct inpcb {
    struct inpcb   *inp_next;   /* pointers to other pcb's */
    struct in_addr  inp_faddr;  /* foreign host table entry */
    u_short         inp_fport;  /* foreign port */
    struct in_addr  inp_laddr;  /* local host table entry */
    u_short         inp_lport;  /* local port */
    int             inp_state;
    int             uid;        /* owner of the connection */
};
#endif

extern void                init_tcpTable(void);
extern Netsnmp_Node_Handler     tcpTable_handler;
extern NetsnmpCacheLoad         tcpTable_load;
extern NetsnmpCacheFree         tcpTable_free;
extern Netsnmp_First_Data_Point tcpTable_first_entry;
extern Netsnmp_Next_Data_Point  tcpTable_next_entry;

#define TCPCONNSTATE	     1
#define TCPCONNLOCALADDRESS  2
#define TCPCONNLOCALPORT     3
#define TCPCONNREMOTEADDRESS 4
#define TCPCONNREMOTEPORT    5

#endif                          /* _MIBGROUP_TCPTABLE_H */
