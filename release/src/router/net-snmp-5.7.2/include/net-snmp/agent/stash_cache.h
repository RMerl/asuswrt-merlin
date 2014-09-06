#ifndef STASH_CACHE_H
#define STASH_CACHE_H

#include <net-snmp/library/oid_stash.h>
#include <net-snmp/library/tools.h>

#define STASH_CACHE_NAME "stash_cache"

typedef struct netsnmp_stash_cache_info_s {
   int                      cache_valid;
   marker_t                 cache_time;
   netsnmp_oid_stash_node  *cache;
   int                      cache_length;
} netsnmp_stash_cache_info;

typedef struct netsnmp_stash_cache_data_s {
   void              *data;
   size_t             data_len;
   u_char             data_type;
} netsnmp_stash_cache_data;

/* function prototypes */
void netsnmp_init_stash_cache_helper(void);
netsnmp_mib_handler *netsnmp_get_bare_stash_cache_handler(void);
netsnmp_mib_handler *netsnmp_get_stash_cache_handler(void);
netsnmp_mib_handler *netsnmp_get_timed_bare_stash_cache_handler(int timeout,
                                           oid *rootoid, size_t rootoid_len);
netsnmp_mib_handler *netsnmp_get_timed_stash_cache_handler(int timeout,
                                           oid *rootoid, size_t rootoid_len);
Netsnmp_Node_Handler netsnmp_stash_cache_helper;
netsnmp_oid_stash_node  **netsnmp_extract_stash_cache(netsnmp_agent_request_info *reqinfo);
int netsnmp_stash_cache_update(netsnmp_mib_handler *handler, netsnmp_handler_registration *reginfo, netsnmp_agent_request_info *reqinfo, netsnmp_request_info *requests, netsnmp_stash_cache_info *cinfo);


#endif /* STASH_CACHE_H */
