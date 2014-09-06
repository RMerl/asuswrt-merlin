/*
 * defaultrouter data access header
 *
 * $Id:$
 */
#ifndef NETSNMP_ACCESS_DEFAULTROUTER_H
#define NETSNMP_ACCESS_DEFAULTROUTER_H

# ifdef __cplusplus
extern          "C" {
#endif

/**---------------------------------------------------------------------*/
/*
 * structure definitions
 */
#if defined( NETSNMP_ENABLE_IPV6 )
#   define NETSNMP_ACCESS_DEFAULTROUTER_BUF_SIZE 16   /* XXX: 20, for ip6z? */
#else
#   define NETSNMP_ACCESS_DEFAULTROUTER_BUF_SIZE 4
#endif


/*
 * netsnmp_default_route_entry
 */
typedef struct netsnmp_defaultrouter_s {

    netsnmp_index oid_index;   /* MUST BE FIRST!! for container use */
    oid           ns_dr_index; /* arbitrary index */

    int      flags; /* for net-snmp use */

    /*
     * mib related data
     */
    u_char   dr_addresstype;    /* ipDefaultRouterAddressType */
    char     dr_address[NETSNMP_ACCESS_DEFAULTROUTER_BUF_SIZE]; /* ipDefaultRouterAddress */
    size_t   dr_address_len;    /* length of ipDefaultRouterAddress */
    oid      dr_if_index;       /* ipDefaultRouterIfIndex */
    uint32_t dr_lifetime;       /* ipDefaultRouterLifetime (0-65535) */
    char     dr_preference;     /* ipDefaultRouterPreference (-2,-1,0,1) */

} netsnmp_defaultrouter_entry;


/**---------------------------------------------------------------------*/
/*
 * ACCESS function prototypes
 */
/*
 * container init
 */
netsnmp_container *
netsnmp_access_defaultrouter_container_init(u_int flags);
#define NETSNMP_ACCESS_DEFAULTROUTER_INIT_NOFLAGS               0x0000
#define NETSNMP_ACCESS_DEFAULTROUTER_INIT_ADDL_IDX_BY_ADDR      0x0001

/*
 * container load
 */
netsnmp_container*
netsnmp_access_defaultrouter_container_load(netsnmp_container* container,
                                            u_int load_flags);
#define NETSNMP_ACCESS_DEFAULTROUTER_LOAD_NOFLAGS               0x0000
#define NETSNMP_ACCESS_DEFAULTROUTER_LOAD_IPV4_ONLY             0x0001
#define NETSNMP_ACCESS_DEFAULTROUTER_LOAD_IPV6_ONLY             0x0002
#define NETSNMP_ACCESS_DEFAULTROUTER_LOAD_ADDL_IDX_BY_ADDR      0x0004

/*
 * container free
 */
void
netsnmp_access_defaultrouter_container_free(netsnmp_container *container,
                                            u_int free_flags);
#define NETSNMP_ACCESS_DEFAULTROUTER_FREE_NOFLAGS               0x0000
#define NETSNMP_ACCESS_DEFAULTROUTER_FREE_DONT_CLEAR            0x0001
#define NETSNMP_ACCESS_DEFAULTROUTER_FREE_KEEP_CONTAINER        0x0002

/*
 * entry create
 */
netsnmp_defaultrouter_entry *
netsnmp_access_defaultrouter_entry_create(void);

/*
 * entry load
 */
int
netsnmp_access_defaultrouter_entry_load(size_t *num_entries,
                                        netsnmp_defaultrouter_entry **entries);

/*
 * entry update
 */
int
netsnmp_access_defaultrouter_entry_update(netsnmp_defaultrouter_entry *lhs,
                                          netsnmp_defaultrouter_entry *rhs);

/*
 * entry free
 */
void
netsnmp_access_defaultrouter_entry_free(netsnmp_defaultrouter_entry *entry);



/**---------------------------------------------------------------------*/

# ifdef __cplusplus
}
#endif

#endif /* NETSNMP_ACCESS_DEFAULTROUTER_H */
