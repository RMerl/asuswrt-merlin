/*
 * scopezone data access header
 *
 * $Id: scopezone.h 14170 2007-04-29 02:22:12Z varun_c $
 */
#ifndef NETSNMP_ACCESS_SCOPEZONE_H
#define NETSNMP_ACCESS_SCOPEZONE_H

#ifdef __cplusplus
extern          "C" {
#endif


/**---------------------------------------------------------------------*/
/*
 * structure definitions
 *
 *
 * NOTE: if you add fields, update code dealing with
 *       them in ipv6scopezone_common.c
 */
typedef struct netsnmp_v6scopezone_entry_s {
    netsnmp_index oid_index;
    oid           ns_scopezone_index;
    u_int   ns_flags; /* net-snmp flags */
    oid     index;
    int     scopezone_linklocal;    

} netsnmp_v6scopezone_entry;

/**---------------------------------------------------------------------*/
/*
 * ACCESS function prototypes
 */

/*
 * scopezone container init
 */
netsnmp_container * netsnmp_access_scopezone_container_init(u_int init_flags);

/*
 * scopezone container load and free
 */
netsnmp_container*
netsnmp_access_scopezone_container_load(netsnmp_container* container,
                                        u_int load_flags);

void netsnmp_access_scopezone_container_free(netsnmp_container *container,
                                             u_int free_flags);
#define NETSNMP_ACCESS_SCOPEZONE_FREE_NOFLAGS               0x0000
#define NETSNMP_ACCESS_SCOPEZONE_FREE_DONT_CLEAR            0x0001
#define NETSNMP_ACCESS_SCOPEZONE_FREE_KEEP_CONTAINER        0x0002


/*
 * create/free an scopezone entry
 */
netsnmp_v6scopezone_entry *
netsnmp_access_scopezone_entry_create(void);

void netsnmp_access_scopezone_entry_free(netsnmp_v6scopezone_entry * entry);


#ifdef __cplusplus
}
#endif

#endif /* NETSNMP_ACCESS_SCOPEZONE_H */
