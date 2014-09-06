/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#ifndef NETSNMP_CACHE_HANDLER_H
#define NETSNMP_CACHE_HANDLER_H

/*
 * This caching helper provides a generalised (SNMP-manageable) caching
 * mechanism.  Individual SNMP table and scalar/scalar group MIB
 * implementations can use data caching in a consistent manner, without
 * needing to handle the generic caching details themselves.
 */

#include <net-snmp/library/tools.h>

#ifdef __cplusplus
extern          "C" {
#endif

#define CACHE_NAME "cache_info"

    typedef struct netsnmp_cache_s netsnmp_cache;

    typedef int  (NetsnmpCacheLoad)(netsnmp_cache *, void*);
    typedef void (NetsnmpCacheFree)(netsnmp_cache *, void*);

    struct netsnmp_cache_s {
	/** Number of handlers whose myvoid member points at this structure. */
	int      refcnt;
        /*
	 * For operation of the data caches
	 */
        int      flags;
        int      enabled;
        int      valid;
        char     expired;
        int      timeout;	/* Length of time the cache is valid (in s) */
        marker_t timestampM;	/* When the cache was last loaded */
        u_long   timer_id;      /* periodic timer id */

        NetsnmpCacheLoad *load_cache;
        NetsnmpCacheFree *free_cache;

       /*
        * void pointer for the user that created the cache.
        * You never know when it might not come in useful ....
        */
        void             *magic;

       /*
        * hint from the cache helper. contains the standard
        * handler arguments.
        */
       netsnmp_handler_args          *cache_hint;

        /*
	 * For SNMP-management of the data caches
	 */
	netsnmp_cache *next, *prev;
        oid *rootoid;
        int  rootoid_len;

    };


    void netsnmp_cache_reqinfo_insert(netsnmp_cache* cache,
                                      netsnmp_agent_request_info * reqinfo,
                                      const char *name);
    netsnmp_cache  *
       netsnmp_cache_reqinfo_extract(netsnmp_agent_request_info * reqinfo,
                                     const char *name);
    netsnmp_cache* netsnmp_extract_cache_info(netsnmp_agent_request_info *);

    int            netsnmp_cache_check_and_reload(netsnmp_cache * cache);
    int            netsnmp_cache_check_expired(netsnmp_cache *cache);
    int            netsnmp_cache_is_valid(    netsnmp_agent_request_info *,
                                              const char *name);
    /** for backwards compat */
    int            netsnmp_is_cache_valid(    netsnmp_agent_request_info *);
    netsnmp_mib_handler *netsnmp_get_cache_handler(int, NetsnmpCacheLoad *,
                                                        NetsnmpCacheFree *,
                                                        const oid*, int);
    int   netsnmp_register_cache_handler(netsnmp_handler_registration *reginfo,
                                         int, NetsnmpCacheLoad *,
                                              NetsnmpCacheFree *);

    Netsnmp_Node_Handler netsnmp_cache_helper_handler;

    netsnmp_cache *
    netsnmp_cache_create(int timeout, NetsnmpCacheLoad * load_hook,
                         NetsnmpCacheFree * free_hook,
                         const oid * rootoid, int rootoid_len);
    int netsnmp_cache_remove(netsnmp_cache *cache);
    int netsnmp_cache_free(netsnmp_cache *cache);

    netsnmp_mib_handler *
    netsnmp_cache_handler_get(netsnmp_cache* cache);
    void netsnmp_cache_handler_owns_cache(netsnmp_mib_handler *handler);

    netsnmp_cache * netsnmp_cache_find_by_oid(const oid * rootoid,
                                              int rootoid_len);

    unsigned int netsnmp_cache_timer_start(netsnmp_cache *cache);
    void netsnmp_cache_timer_stop(netsnmp_cache *cache);

/*
 * Flags affecting cache handler operation
 */
#define NETSNMP_CACHE_DONT_INVALIDATE_ON_SET                0x0001
#define NETSNMP_CACHE_DONT_FREE_BEFORE_LOAD                 0x0002
#define NETSNMP_CACHE_DONT_FREE_EXPIRED                     0x0004
#define NETSNMP_CACHE_DONT_AUTO_RELEASE                     0x0008
#define NETSNMP_CACHE_PRELOAD                               0x0010
#define NETSNMP_CACHE_AUTO_RELOAD                           0x0020
#define NETSNMP_CACHE_RESET_TIMER_ON_USE                    0x0040

#define NETSNMP_CACHE_HINT_HANDLER_ARGS                     0x1000


#ifdef __cplusplus
}
#endif
#endif /* NETSNMP_CACHE_HANDLER_H */
