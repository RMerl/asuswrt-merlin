/*
 * tcpConn data access header
 *
 * $Id$
 */
#ifndef NETSNMP_ACCESS_TCPCONN_H
#define NETSNMP_ACCESS_TCPCONN_H

/** need def of NETSNMP_ACCESS_IPADDRESS_BUF_SIZE */
#include <net-snmp/data_access/ipaddress.h>

# ifdef __cplusplus
extern          "C" {
#endif

/**---------------------------------------------------------------------*/
/*
 * structure definitions
 */

/*
 * netsnmp_tcpconn_entry
 *   - primary tcpconn structure for both ipv4 & ipv6
 */
    typedef struct netsnmp_tcpconn_s {

        netsnmp_index oid_index;   /* MUST BE FIRST!! for container use */
        oid           arbitrary_index; /* arbitrary index */

        int       flags; /* for net-snmp use */

        u_char    loc_addr[NETSNMP_ACCESS_IPADDRESS_BUF_SIZE];
        u_char    rmt_addr[NETSNMP_ACCESS_IPADDRESS_BUF_SIZE];

        u_char    loc_addr_len;/* address len, 4 | 16 */
        u_char    rmt_addr_len;/* address len, 4 | 16 */

        u_short   loc_port;
        u_short   rmt_port;
        
        /*
         * mib related data (considered for
         *  netsnmp_access_tcpconn_entry_update)
         */
   
        /*
         * tcpconnState(1)/INTEGER/ASN_INTEGER/long(u_long)//l/A/W/E/r/d/h
         */
        u_int           tcpConnState; /* 1-12 */

        u_int           pid;
   
        netsnmp_data_list *arch_data;      /* arch specific data */
   
    } netsnmp_tcpconn_entry;


/**---------------------------------------------------------------------*/
/*
 * ACCESS function prototypes
 */
/*
 * ifcontainer init
 */
    netsnmp_container * netsnmp_access_tcpconn_container_init(u_int init_flags);
#define NETSNMP_ACCESS_TCPCONN_INIT_NOFLAGS               0x0000

/*
 * ifcontainer load and free
 */
    netsnmp_container*
    netsnmp_access_tcpconn_container_load(netsnmp_container* container,
                                          u_int load_flags);
#define NETSNMP_ACCESS_TCPCONN_LOAD_NOFLAGS               0x0000
#define NETSNMP_ACCESS_TCPCONN_LOAD_NOLISTEN              0x0001
#define NETSNMP_ACCESS_TCPCONN_LOAD_ONLYLISTEN            0x0002
#define NETSNMP_ACCESS_TCPCONN_LOAD_IPV4_ONLY             0x0004

    void netsnmp_access_tcpconn_container_free(netsnmp_container *container,
                                               u_int free_flags);
#define NETSNMP_ACCESS_TCPCONN_FREE_NOFLAGS               0x0000
#define NETSNMP_ACCESS_TCPCONN_FREE_DONT_CLEAR            0x0001
#define NETSNMP_ACCESS_TCPCONN_FREE_KEEP_CONTAINER        0x0002


/*
 * create/free a tcpconn entry
 */
    netsnmp_tcpconn_entry *
    netsnmp_access_tcpconn_entry_create(void);

    void netsnmp_access_tcpconn_entry_free(netsnmp_tcpconn_entry * entry);

/*
 * update/compare
 */
    int
    netsnmp_access_tcpconn_entry_update(netsnmp_tcpconn_entry *old, 
                                        netsnmp_tcpconn_entry *new_val);

/*
 * find entry in container
 */
/** not yet */

/*
 * create/change/delete
 */
    int
    netsnmp_access_tcpconn_entry_set(netsnmp_tcpconn_entry * entry);


/*
 * tcpconn flags
 *   upper bits for internal use
 *   lower bits indicate changed fields. see FLAG_TCPCONN* definitions in
 *         tcpConnTable_constants.h
 */
#define NETSNMP_ACCESS_TCPCONN_CREATE     0x80000000
#define NETSNMP_ACCESS_TCPCONN_DELETE     0x40000000
#define NETSNMP_ACCESS_TCPCONN_CHANGE     0x20000000


/**---------------------------------------------------------------------*/

# ifdef __cplusplus
}
#endif

#endif /* NETSNMP_ACCESS_TCPCONN_H */
