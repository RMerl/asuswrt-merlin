/*
 * systemstats data access header
 *
 * $Id$
 */
#ifndef NETSNMP_ACCESS_SYSTEMSTATS_H
#define NETSNMP_ACCESS_SYSTEMSTATS_H

# ifdef __cplusplus
extern          "C" {
#endif

/**---------------------------------------------------------------------*/
/*
 * structure definitions
 */


/*
 * netsnmp_systemstats_entry
 */
typedef struct netsnmp_systemstats_s {

   netsnmp_index oid_index;   /* MUST BE FIRST!! for container use */
   /* 
    * Index of the table
    * First entry = ip version
    * Second entry = interface index (0 for ipSystemStatsTable */
   oid           index[2];           

   int       flags; /* for net-snmp use */

   /*
    * mib related data (considered for
    *  netsnmp_access_systemstats_entry_update)
    */
   netsnmp_ipstats stats;

   /*
    * for logging
    */
   const char* tableName;
   
   /** old_stats is used in netsnmp_access_interface_entry_update_stats */
   netsnmp_ipstats *old_stats;

} netsnmp_systemstats_entry;


/**---------------------------------------------------------------------*/
/*
 * ACCESS function prototypes
 */
/*
 * init
 */
void netsnmp_access_systemstats_init(void);

/*
 * init
 */
netsnmp_container * netsnmp_access_systemstats_container_init(u_int init_flags);
#define NETSNMP_ACCESS_SYSTEMSTATS_INIT_NOFLAGS               0x0000
#define NETSNMP_ACCESS_SYSTEMSTATS_INIT_ADDL_IDX_BY_ADDR      0x0001

/**
 * Load container. If the NETSNMP_ACCESS_SYSTEMSTATS_LOAD_IFTABLE is set
 * the ipIfSystemStats table is loaded, else ipSystemStatsTable is loaded.
 */
netsnmp_container*
netsnmp_access_systemstats_container_load(netsnmp_container* container,
                                    u_int load_flags);
#define NETSNMP_ACCESS_SYSTEMSTATS_LOAD_NOFLAGS               0x0000
#define NETSNMP_ACCESS_SYSTEMSTATS_LOAD_IFTABLE               0x0001 

void netsnmp_access_systemstats_container_free(netsnmp_container *container,
                                         u_int free_flags);
#define NETSNMP_ACCESS_SYSTEMSTATS_FREE_NOFLAGS               0x0000
#define NETSNMP_ACCESS_SYSTEMSTATS_FREE_DONT_CLEAR            0x0001
#define NETSNMP_ACCESS_SYSTEMSTATS_FREE_KEEP_CONTAINER        0x0002


/*
 * create/free an entry
 */
netsnmp_systemstats_entry *
netsnmp_access_systemstats_entry_create(int version, int if_index,
            const char* tableName);

void netsnmp_access_systemstats_entry_free(netsnmp_systemstats_entry * entry);

/*
 * update/compare
 */
int
netsnmp_access_systemstats_entry_update(netsnmp_systemstats_entry *old, 
                                        netsnmp_systemstats_entry *new_val);


/**---------------------------------------------------------------------*/

# ifdef __cplusplus
}
#endif

#endif /* NETSNMP_ACCESS_SYSTEMSTATS_H */
