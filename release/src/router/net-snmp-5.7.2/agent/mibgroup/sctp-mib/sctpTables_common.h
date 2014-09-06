#ifndef SCTP_TABLES_COMMON_H
#define SCTP_TABLES_COMMON_H

#if defined(freebsd8) || defined(freebsd7)
#define netsnmp_table_registration_info_free SNMP_FREE
#endif

#define SCTP_IPADDRESS_SIZE 16
#define SCTP_HOSTNAME_SIZE 255

#define INETADDRESSTYPE_IPV4 1
#define INETADDRESSTYPE_IPV6 2

#define TRUTHVALUE_TRUE 1
#define TRUTHVALUE_FALSE 2

#define SCTP_TABLES_CACHE_TIMEOUT 30

/*
 * Let the platform independent part delete all entries with valid==0.
 */
#define SCTP_TABLES_LOAD_FLAG_DELETE_INVALID 1
/*
 * Let the platform independent part automatically calculate the lookup tables.
 */
#define SCTP_TABLES_LOAD_FLAG_AUTO_LOOKUP 1

typedef struct sctpTables_containers_s {
    netsnmp_container *sctpAssocTable;
    netsnmp_container *sctpAssocRemAddrTable;
    netsnmp_container *sctpAssocLocalAddrTable;
    netsnmp_container *sctpLookupLocalPortTable;
    netsnmp_container *sctpLookupRemPortTable;
    netsnmp_container *sctpLookupRemHostNameTable;
    netsnmp_container *sctpLookupRemPrimIPAddrTable;
    netsnmp_container *sctpLookupRemIPAddrTable;
} sctpTables_containers;

/*
 * Forward declaration of some types. 
 */
typedef struct sctpAssocTable_entry_s sctpAssocTable_entry;
typedef struct sctpAssocRemAddrTable_entry_s sctpAssocRemAddrTable_entry;
typedef struct sctpAssocLocalAddrTable_entry_s
                sctpAssocLocalAddrTable_entry;
typedef struct sctpLookupLocalPortTable_entry_s
                sctpLookupLocalPortTable_entry;
typedef struct sctpLookupRemPortTable_entry_s sctpLookupRemPortTable_entry;
typedef struct sctpLookupRemHostNameTable_entry_s
                sctpLookupRemHostNameTable_entry;
typedef struct sctpLookupRemPrimIPAddrTable_entry_s
                sctpLookupRemPrimIPAddrTable_entry;
typedef struct sctpLookupRemIPAddrTable_entry_s
                sctpLookupRemIPAddrTable_entry;

extern netsnmp_container *sctpAssocTable_get_container(void);
extern netsnmp_container *sctpAssocRemAddrTable_get_container(void);
extern netsnmp_container *sctpAssocLocalAddrTable_get_container(void);
extern netsnmp_container *sctpLookupLocalPortTable_get_container(void);
extern netsnmp_container *sctpLookupRemPortTable_get_container(void);
extern netsnmp_container *sctpLookupRemHostNameTable_get_container(void);
extern netsnmp_container *sctpLookupRemPrimIPAddrTable_get_container(void);
extern netsnmp_container *sctpLookupRemIPAddrTable_get_container(void);

/*
 * Platform independent helper. Reloads all sctp table containers, uses sctpTables_arch_load internally.
 */
extern int      sctpTables_load(void);


/*
 * Platform dependent loader of sctp tables. It gets the containers with 'old' values and it must update these containers to reflect actual state. It does not matter if it decides to clear the containers and fill them from scratch or just update the entries.
 * If the function wants to use automatic removal of entries with valid==0, it must set the SCTP_TABLES_LOAD_FLAG_DELETE_INVALID flag and mark all valid entries (all 'old' entries are invalid by default).
 */
extern int      sctpTables_arch_load(sctpTables_containers * containers,
                                     u_long * flags);

/*
 * Add the entry to the table or overwrite it if it already exists.
 * Don't overwrite timestamp. If the timestamp is not set and it should be,
 * then set it.
 * 
 * Caller must not use the entry after calling the function, it may be freed.
 */
extern int      sctpAssocTable_add_or_update(netsnmp_container *assocTable,
                                             sctpAssocTable_entry * entry);

/*
 * Add the entry to the table or overwrite it if it already exists.
 * Don't overwrite timestamp. If the timestamp is not set and it should be,
 * then set it.
 * 
 * Caller must not use the entry after calling the function, it may be freed.
 */
extern int      sctpAssocRemAddrTable_add_or_update(netsnmp_container
                                                    *remAddrTable,
                                                    sctpAssocRemAddrTable_entry
                                                    * entry);

/*
 * Add the entry to the table or overwrite it if it already exists.
 * Don't overwrite timestamp. If the timestamp is not set and it should be,
 * then set it.
 * 
 * Caller must not use the entry after calling the function, it may be freed.
 */
extern int      sctpAssocLocalAddrTable_add_or_update(netsnmp_container
                                                      *localAddrTable,
                                                      sctpAssocLocalAddrTable_entry
                                                      * entry);

#endif                          /* SCTP_TABLES_COMMON_H */
