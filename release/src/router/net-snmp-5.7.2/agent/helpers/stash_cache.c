#include <net-snmp/net-snmp-config.h>

#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

netsnmp_feature_provide(stash_cache)
netsnmp_feature_child_of(stash_cache, mib_helpers)
#ifdef NETSNMP_FEATURE_REQUIRE_STASH_CACHE
netsnmp_feature_require(oid_stash)
netsnmp_feature_require(oid_stash_iterate)
netsnmp_feature_require(oid_stash_get_data)
#endif

#ifndef NETSNMP_FEATURE_REMOVE_STASH_CACHE
#include <net-snmp/agent/stash_to_next.h>

#include <net-snmp/agent/stash_cache.h>

extern NetsnmpCacheLoad _netsnmp_stash_cache_load;
extern NetsnmpCacheFree _netsnmp_stash_cache_free;
 
/** @defgroup stash_cache stash_cache
 *  Automatically caches data for certain handlers.
 *  This handler caches data in an optimized way which may alleviate
 *  the need for the lower level handlers to perform as much
 *  optimization.  Specifically, somewhere in the lower level handlers
 *  must be a handler that supports the MODE_GET_STASH operation.
 *  Note that the table_iterator helper supports this.
 *  @ingroup handler
 *  @{
 */

netsnmp_stash_cache_info *
netsnmp_get_new_stash_cache(void)
{
    netsnmp_stash_cache_info *cinfo;

    cinfo = SNMP_MALLOC_TYPEDEF(netsnmp_stash_cache_info);
    if (cinfo != NULL)
        cinfo->cache_length = 30;
    return cinfo;
}

/** returns a stash_cache handler that can be injected into a given
 *  handler chain (with the specified timeout and root OID values),
 *  but *only* if that handler chain explicitly supports stash cache processing.
 */
netsnmp_mib_handler *
netsnmp_get_timed_bare_stash_cache_handler(int timeout, oid *rootoid, size_t rootoid_len)
{
    netsnmp_mib_handler *handler;
    netsnmp_cache       *cinfo;

    cinfo = netsnmp_cache_create( timeout, _netsnmp_stash_cache_load,
                                 _netsnmp_stash_cache_free, rootoid, rootoid_len );

    if (!cinfo)
        return NULL;

    handler = netsnmp_cache_handler_get( cinfo );
    if (!handler) {
        free(cinfo);
        return NULL;
    }

    handler->next = netsnmp_create_handler("stash_cache", netsnmp_stash_cache_helper);
    if (!handler->next) {
        netsnmp_handler_free(handler);
        free(cinfo);
        return NULL;
    }

    handler->myvoid = cinfo;
    netsnmp_cache_handler_owns_cache(handler);

    return handler;
}

/** returns a single stash_cache handler that can be injected into a given
 *  handler chain (with a fixed timeout), but *only* if that handler chain
 *  explicitly supports stash cache processing.
 */
netsnmp_mib_handler *
netsnmp_get_bare_stash_cache_handler(void)
{
    return netsnmp_get_timed_bare_stash_cache_handler( 30, NULL, 0 );
}

/** returns a stash_cache handler sub-chain that can be injected into a given
 *  (arbitrary) handler chain, using a fixed cache timeout.
 */
netsnmp_mib_handler *
netsnmp_get_stash_cache_handler(void)
{
    netsnmp_mib_handler *handler = netsnmp_get_bare_stash_cache_handler();
    if (handler && handler->next) {
        handler->next->next = netsnmp_get_stash_to_next_handler();
    }
    return handler;
}

/** returns a stash_cache handler sub-chain that can be injected into a given
 *  (arbitrary) handler chain, using a configurable cache timeout.
 */
netsnmp_mib_handler *
netsnmp_get_timed_stash_cache_handler(int timeout, oid *rootoid, size_t rootoid_len)
{
    netsnmp_mib_handler *handler = 
       netsnmp_get_timed_bare_stash_cache_handler(timeout, rootoid, rootoid_len);
    if (handler && handler->next) {
        handler->next->next = netsnmp_get_stash_to_next_handler();
    }
    return handler;
}

/** extracts a pointer to the stash_cache info from the reqinfo structure. */
netsnmp_oid_stash_node  **
netsnmp_extract_stash_cache(netsnmp_agent_request_info *reqinfo)
{
    return (netsnmp_oid_stash_node**)netsnmp_agent_get_list_data(reqinfo, STASH_CACHE_NAME);
}


/** @internal Implements the stash_cache handler */
int
netsnmp_stash_cache_helper(netsnmp_mib_handler *handler,
                           netsnmp_handler_registration *reginfo,
                           netsnmp_agent_request_info *reqinfo,
                           netsnmp_request_info *requests)
{
    netsnmp_cache            *cache;
    netsnmp_stash_cache_info *cinfo;
    netsnmp_oid_stash_node   *cnode;
    netsnmp_variable_list    *cdata;
    netsnmp_request_info     *request;

    DEBUGMSGTL(("helper:stash_cache", "Got request\n"));

    cache = netsnmp_cache_reqinfo_extract( reqinfo, reginfo->handlerName );
    if (!cache) {
        DEBUGMSGTL(("helper:stash_cache", "No cache structure\n"));
        return SNMP_ERR_GENERR;
    }
    cinfo = (netsnmp_stash_cache_info *) cache->magic;

    switch (reqinfo->mode) {

    case MODE_GET:
        DEBUGMSGTL(("helper:stash_cache", "Processing GET request\n"));
        for(request = requests; request; request = request->next) {
            cdata = (netsnmp_variable_list*)
                netsnmp_oid_stash_get_data(cinfo->cache,
                                           requests->requestvb->name,
                                           requests->requestvb->name_length);
            if (cdata && cdata->val.string && cdata->val_len) {
                DEBUGMSGTL(("helper:stash_cache", "Found cached GET varbind\n"));
                DEBUGMSGOID(("helper:stash_cache", cdata->name, cdata->name_length));
                DEBUGMSG(("helper:stash_cache", "\n"));
                snmp_set_var_typed_value(request->requestvb, cdata->type,
                                         cdata->val.string, cdata->val_len);
            }
        }
        break;

    case MODE_GETNEXT:
        DEBUGMSGTL(("helper:stash_cache", "Processing GETNEXT request\n"));
        for(request = requests; request; request = request->next) {
            cnode =
                netsnmp_oid_stash_getnext_node(cinfo->cache,
                                               requests->requestvb->name,
                                               requests->requestvb->name_length);
            if (cnode && cnode->thedata) {
                cdata =  (netsnmp_variable_list*)cnode->thedata;
                if (cdata->val.string && cdata->name && cdata->name_length) {
                    DEBUGMSGTL(("helper:stash_cache", "Found cached GETNEXT varbind\n"));
                    DEBUGMSGOID(("helper:stash_cache", cdata->name, cdata->name_length));
                    DEBUGMSG(("helper:stash_cache", "\n"));
                    snmp_set_var_typed_value(request->requestvb, cdata->type,
                                             cdata->val.string, cdata->val_len);
                    snmp_set_var_objid(request->requestvb, cdata->name,
                                       cdata->name_length);
                }
            }
        }
        break;

    default:
        cinfo->cache_valid = 0;
        return netsnmp_call_next_handler(handler, reginfo, reqinfo,
                                         requests);
    }

    return SNMP_ERR_NOERROR;
}

/** updates a given cache depending on whether it needs to or not.
 */
int
_netsnmp_stash_cache_load( netsnmp_cache *cache, void *magic )
{
    netsnmp_mib_handler          *handler  = cache->cache_hint->handler;
    netsnmp_handler_registration *reginfo  = cache->cache_hint->reginfo;
    netsnmp_agent_request_info   *reqinfo  = cache->cache_hint->reqinfo;
    netsnmp_request_info         *requests = cache->cache_hint->requests;
    netsnmp_stash_cache_info     *cinfo    = (netsnmp_stash_cache_info*) magic;
    int old_mode;
    int ret;

    if (!cinfo) {
        cinfo = netsnmp_get_new_stash_cache();
        cache->magic = cinfo;
    }

    /* change modes to the GET_STASH mode */
    old_mode = reqinfo->mode;
    reqinfo->mode = MODE_GET_STASH;
    netsnmp_agent_add_list_data(reqinfo,
                                netsnmp_create_data_list(STASH_CACHE_NAME,
                                                         &cinfo->cache, NULL));

    /* have the next handler fill stuff in and switch modes back */
    ret = netsnmp_call_next_handler(handler->next, reginfo, reqinfo, requests);
    reqinfo->mode = old_mode;

    return ret;
}

void
_netsnmp_stash_cache_free( netsnmp_cache *cache, void *magic )
{
    netsnmp_stash_cache_info *cinfo = (netsnmp_stash_cache_info*) magic;
    netsnmp_oid_stash_free(&cinfo->cache,
                          (NetSNMPStashFreeNode *) snmp_free_var);
    return;
}

/** initializes the stash_cache helper which then registers a stash_cache
 *  handler as a run-time injectable handler for configuration file
 *  use.
 */
void
netsnmp_init_stash_cache_helper(void)
{
    netsnmp_register_handler_by_name("stash_cache",
                                     netsnmp_get_stash_cache_handler());
}
/**  @} */

#else /* NETSNMP_FEATURE_REMOVE_STASH_CACHE */
netsnmp_feature_unused(stash_cache);
#endif /* NETSNMP_FEATURE_REMOVE_STASH_CACHE */
