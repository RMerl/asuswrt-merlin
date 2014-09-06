/*
 *  Template MIB group interface - at.h
 *
 */

#ifndef _MIBGROUP_AT_H
#define _MIBGROUP_AT_H

config_arch_require(solaris2, kernel_sunos5)

     extern void     init_at(void);
     extern FindVarMethod var_atEntry;


#define ATIFINDEX	0
#define ATPHYSADDRESS	1
#define ATNETADDRESS	2

#define IPMEDIAIFINDEX          0
#define IPMEDIAPHYSADDRESS      1
#define IPMEDIANETADDRESS       2
#define IPMEDIATYPE             3

/*
 * in case its missing: 
 */
#ifndef ATF_PERM
# define ATF_PERM	0x04
#endif                          /*  ATF_PERM */
#ifndef ATF_COM
# define ATF_COM	0x02
#endif                          /*  ATF_COM */

/* InfiniBand uses HW addr > 6 */
#define MAX_MAC_ADDR_LEN 32 

#if defined(linux) || defined(irix6)
/*
 * arp struct to pass flags, hw-addr and ip-addr in bsd manner:
 */
     struct arptab {
         int             at_flags;
         char            at_enaddr[MAX_MAC_ADDR_LEN];
         int             at_enaddr_len;
         struct in_addr  at_iaddr;
         int             if_index;
     };
#endif
#endif                          /* _MIBGROUP_AT_H */
