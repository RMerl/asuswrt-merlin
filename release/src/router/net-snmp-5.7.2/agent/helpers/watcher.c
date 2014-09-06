#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/watcher.h>

#include <net-snmp/agent/instance.h>
#include <net-snmp/agent/scalar.h>

#include <string.h>

#ifdef HAVE_DMALLOC_H
static void free_wrapper(void * p)
{
    free(p);
}
#else
#define free_wrapper free
#endif

netsnmp_feature_provide(watcher_all)
netsnmp_feature_child_of(watcher_all, mib_helpers)
netsnmp_feature_child_of(watcher_create_info6, watcher_all)
netsnmp_feature_child_of(watcher_register_timestamp, watcher_all)
netsnmp_feature_child_of(watcher_ulong_scalar, watcher_all)
netsnmp_feature_child_of(watcher_read_only_ulong_scalar, watcher_all)
netsnmp_feature_child_of(watcher_read_only_int_scalar, watcher_all)
netsnmp_feature_child_of(watcher_long_scalar, watcher_all)
netsnmp_feature_child_of(watcher_read_only_long_scalar, watcher_all)
netsnmp_feature_child_of(watcher_int_scalar, watcher_all)
netsnmp_feature_child_of(read_only_counter32_scalar, watcher_all)
netsnmp_feature_child_of(watcher_spinlock, watcher_all)

/** @defgroup watcher watcher
 *  Watch a specified variable and process it as an instance or scalar object
 *  @ingroup leaf
 *  @{
 */
netsnmp_mib_handler *
netsnmp_get_watcher_handler(void)
{
    netsnmp_mib_handler *ret = NULL;
    
    ret = netsnmp_create_handler("watcher",
                                 netsnmp_watcher_helper_handler);
    if (ret) {
        ret->flags |= MIB_HANDLER_AUTO_NEXT;
    }
    return ret;
}

netsnmp_watcher_info *
netsnmp_init_watcher_info6(netsnmp_watcher_info *winfo,
                           void *data, size_t size, u_char type,
                           int flags, size_t max_size, size_t* size_p)
{
    winfo->data = data;
    winfo->data_size = size;
    winfo->max_size = max_size;
    winfo->type = type;
    winfo->flags = flags;
    winfo->data_size_p = size_p;
    return winfo;
}

#ifndef NETSNMP_FEATURE_REMOVE_WATCHER_CREATE_INFO6
netsnmp_watcher_info *
netsnmp_create_watcher_info6(void *data, size_t size, u_char type,
                             int flags, size_t max_size, size_t* size_p)
{
    netsnmp_watcher_info *winfo = SNMP_MALLOC_TYPEDEF(netsnmp_watcher_info);
    if (winfo)
        netsnmp_init_watcher_info6(winfo, data, size, type, flags, max_size,
                                   size_p);
    return winfo;
}
#endif /* NETSNMP_FEATURE_REMOVE_WATCHER_CREATE_INFO6 */

netsnmp_watcher_info *
netsnmp_init_watcher_info(netsnmp_watcher_info *winfo,
                          void *data, size_t size, u_char type, int flags)
{
  return netsnmp_init_watcher_info6(winfo, data, size,
				    type, (flags ? flags : WATCHER_FIXED_SIZE),
				    size,  /* Probably wrong for non-fixed
					    * size data */
				    NULL);
}

netsnmp_watcher_info *
netsnmp_create_watcher_info(void *data, size_t size, u_char type, int flags)
{
    netsnmp_watcher_info *winfo = SNMP_MALLOC_TYPEDEF(netsnmp_watcher_info);
    if (winfo)
        netsnmp_init_watcher_info(winfo, data, size, type, flags);
    return winfo;
}

/**
 * Register a watched scalar. The caller remains the owner of watchinfo.
 *
 * @see netsnmp_register_watched_instance2()
 */
int
netsnmp_register_watched_instance(netsnmp_handler_registration *reginfo,
                                  netsnmp_watcher_info         *watchinfo)
{
    netsnmp_mib_handler *whandler;

    whandler         = netsnmp_get_watcher_handler();
    whandler->myvoid = (void *)watchinfo;

    netsnmp_inject_handler(reginfo, whandler);
    return netsnmp_register_instance(reginfo);
}

/**
 * Register a watched scalar. Ownership of watchinfo is transferred to the handler.
 *
 * @see netsnmp_register_watched_instance()
 */
int
netsnmp_register_watched_instance2(netsnmp_handler_registration *reginfo,
				   netsnmp_watcher_info         *watchinfo)
{
    netsnmp_mib_handler *whandler;

    whandler         = netsnmp_get_watcher_handler();
    whandler->myvoid = (void *)watchinfo;
    netsnmp_owns_watcher_info(whandler);

    netsnmp_inject_handler(reginfo, whandler);
    return netsnmp_register_instance(reginfo);
}

/**
 * Register a watched scalar. The caller remains the owner of watchinfo.
 *
 * @see netsnmp_register_watched_scalar2()
 */
int
netsnmp_register_watched_scalar(netsnmp_handler_registration *reginfo,
                                  netsnmp_watcher_info         *watchinfo)
{
    netsnmp_mib_handler *whandler;

    whandler         = netsnmp_get_watcher_handler();
    whandler->myvoid = (void *)watchinfo;

    netsnmp_inject_handler(reginfo, whandler);
    return netsnmp_register_scalar(reginfo);
}

/**
 * Register a watched scalar. Ownership of watchinfo is transferred to the handler.
 *
 * @see netsnmp_register_watched_scalar()
 */
int
netsnmp_register_watched_scalar2(netsnmp_handler_registration *reginfo,
                                  netsnmp_watcher_info         *watchinfo)
{
    netsnmp_mib_handler *whandler;

    whandler         = netsnmp_get_watcher_handler();
    whandler->myvoid = (void *)watchinfo;
    netsnmp_owns_watcher_info(whandler);

    netsnmp_inject_handler(reginfo, whandler);
    return netsnmp_register_scalar(reginfo);
}

void
netsnmp_owns_watcher_info(netsnmp_mib_handler *handler)
{
    netsnmp_assert(handler);
    netsnmp_assert(handler->myvoid);
    handler->data_clone = (void *(*)(void *))netsnmp_clone_watcher_info;
    handler->data_free = free;
}

/** @cond */

NETSNMP_STATIC_INLINE size_t
get_data_size(const netsnmp_watcher_info* winfo)
{
    if (winfo->flags & WATCHER_SIZE_STRLEN)
        return strlen((const char*)winfo->data);
    else {
        size_t res;
        if (winfo->flags & WATCHER_SIZE_IS_PTR)
            res = *winfo->data_size_p;
        else
            res = winfo->data_size;
        if (winfo->flags & WATCHER_SIZE_UNIT_OIDS)
          res *= sizeof(oid);
        return res;
    }
}

NETSNMP_STATIC_INLINE void
set_data(netsnmp_watcher_info* winfo, void* data, size_t size)
{
    memcpy(winfo->data, data, size);
    if (winfo->flags & WATCHER_SIZE_STRLEN)
        ((char*)winfo->data)[size] = '\0';
    else {
        if (winfo->flags & WATCHER_SIZE_UNIT_OIDS)
          size /= sizeof(oid);
        if (winfo->flags & WATCHER_SIZE_IS_PTR)
            *winfo->data_size_p = size;
        else
            winfo->data_size = size;
    }
}

typedef struct {
    size_t size;
    char data[1];
} netsnmp_watcher_cache;

NETSNMP_STATIC_INLINE netsnmp_watcher_cache*
netsnmp_watcher_cache_create(const void* data, size_t size)
{
    netsnmp_watcher_cache *res = (netsnmp_watcher_cache*)
        malloc(sizeof(netsnmp_watcher_cache) + size - 1);
    if (res) {
        res->size = size;
        memcpy(res->data, data, size);
    }
    return res;
}

/** @endcond */

int
netsnmp_watcher_helper_handler(netsnmp_mib_handler *handler,
                               netsnmp_handler_registration *reginfo,
                               netsnmp_agent_request_info *reqinfo,
                               netsnmp_request_info *requests)
{
    netsnmp_watcher_info  *winfo = (netsnmp_watcher_info *) handler->myvoid;
    netsnmp_watcher_cache *old_data;

    DEBUGMSGTL(("helper:watcher", "Got request:  %d\n", reqinfo->mode));
    DEBUGMSGTL(( "helper:watcher", "  oid:"));
    DEBUGMSGOID(("helper:watcher", requests->requestvb->name,
                                   requests->requestvb->name_length));
    DEBUGMSG((   "helper:watcher", "\n"));

    switch (reqinfo->mode) {
        /*
         * data requests 
         */
    case MODE_GET:
        snmp_set_var_typed_value(requests->requestvb,
                                 winfo->type,
                                 winfo->data,
                                 get_data_size(winfo));
        break;

        /*
         * SET requests.  Should only get here if registered RWRITE 
         */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
        if (requests->requestvb->type != winfo->type) {
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_WRONGTYPE);
            handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
        } else if (((winfo->flags & WATCHER_MAX_SIZE) &&
                     requests->requestvb->val_len > winfo->max_size) ||
            ((winfo->flags & WATCHER_FIXED_SIZE) &&
                requests->requestvb->val_len != get_data_size(winfo))) {
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_WRONGLENGTH);
            handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
        } else if ((winfo->flags & WATCHER_SIZE_STRLEN) &&
            (memchr(requests->requestvb->val.string, '\0',
                requests->requestvb->val_len) != NULL)) {
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_WRONGVALUE);
            handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
        }
        break;

    case MODE_SET_RESERVE2:
        /*
         * store old info for undo later 
         */
        old_data =
            netsnmp_watcher_cache_create(winfo->data, get_data_size(winfo));
        if (old_data == NULL) {
            netsnmp_set_request_error(reqinfo, requests,
                                      SNMP_ERR_RESOURCEUNAVAILABLE);
            handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
        } else
            netsnmp_request_add_list_data(requests,
                                          netsnmp_create_data_list
                                          ("watcher", old_data, &free_wrapper));
        break;

    case MODE_SET_FREE:
        /*
         * nothing to do 
         */
        break;

    case MODE_SET_ACTION:
        /*
         * update current 
         */
        set_data(winfo, (void *)requests->requestvb->val.string,
                                requests->requestvb->val_len);
        break;

    case MODE_SET_UNDO:
        old_data = (netsnmp_watcher_cache*)netsnmp_request_get_list_data(requests, "watcher");
        set_data(winfo, old_data->data, old_data->size);
        break;

    case MODE_SET_COMMIT:
        break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */

    default:
        snmp_log(LOG_ERR, "watcher handler called with an unknown mode: %d\n",
                 reqinfo->mode);
        return SNMP_ERR_GENERR;

    }

    /* next handler called automatically - 'AUTO_NEXT' */
    return SNMP_ERR_NOERROR;
}


    /***************************
     *
     * A specialised form of the above, reporting
     *   the sysUpTime indicated by a given timestamp
     *
     ***************************/

netsnmp_mib_handler *
netsnmp_get_watched_timestamp_handler(void)
{
    netsnmp_mib_handler *ret = NULL;
    
    ret = netsnmp_create_handler("watcher-timestamp",
                                 netsnmp_watched_timestamp_handler);
    if (ret) {
        ret->flags |= MIB_HANDLER_AUTO_NEXT;
    }
    return ret;
}

int
netsnmp_watched_timestamp_register(netsnmp_mib_handler *whandler,
                                   netsnmp_handler_registration *reginfo,
                                   marker_t timestamp)
{
    whandler->myvoid = (void *)timestamp;
    netsnmp_inject_handler(reginfo, whandler);
    return netsnmp_register_scalar(reginfo);   /* XXX - or instance? */
}

#ifndef NETSNMP_FEATURE_REMOVE_WATCHER_REGISTER_TIMESTAMP
int
netsnmp_register_watched_timestamp(netsnmp_handler_registration *reginfo,
                                   marker_t timestamp)
{
    netsnmp_mib_handler *whandler;

    whandler         = netsnmp_get_watched_timestamp_handler();

    return netsnmp_watched_timestamp_register(whandler, reginfo, timestamp);
}
#endif /* NETSNMP_FEATURE_REMOVE_WATCHER_REGISTER_TIMESTAMP */


int
netsnmp_watched_timestamp_handler(netsnmp_mib_handler *handler,
                               netsnmp_handler_registration *reginfo,
                               netsnmp_agent_request_info *reqinfo,
                               netsnmp_request_info *requests)
{
    marker_t timestamp = (marker_t) handler->myvoid;
    long     uptime;

    DEBUGMSGTL(("helper:watcher:timestamp",
                               "Got request:  %d\n", reqinfo->mode));
    DEBUGMSGTL(( "helper:watcher:timestamp", "  oid:"));
    DEBUGMSGOID(("helper:watcher:timestamp", requests->requestvb->name,
                                   requests->requestvb->name_length));
    DEBUGMSG((   "helper:watcher:timestamp", "\n"));

    switch (reqinfo->mode) {
        /*
         * data requests 
         */
    case MODE_GET:
        if (handler->flags & NETSNMP_WATCHER_DIRECT)
            uptime = * (long*)timestamp;
        else
            uptime = netsnmp_marker_uptime( timestamp );
        snmp_set_var_typed_value(requests->requestvb,
                                 ASN_TIMETICKS,
                                 (u_char *) &uptime,
                                 sizeof(uptime));
        break;

        /*
         * Timestamps are inherently Read-Only,
         *  so don't need to support SET requests.
         */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
        netsnmp_set_request_error(reqinfo, requests,
                                  SNMP_ERR_NOTWRITABLE);
        handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
        return SNMP_ERR_NOTWRITABLE;
#endif /* NETSNMP_NO_WRITE_SUPPORT */
    }

    /* next handler called automatically - 'AUTO_NEXT' */
    return SNMP_ERR_NOERROR;
}

    /***************************
     *
     * Another specialised form of the above,
     *   implementing a 'TestAndIncr' spinlock
     *
     ***************************/

#ifndef NETSNMP_FEATURE_REMOVE_WATCHER_SPINLOCK

netsnmp_mib_handler *
netsnmp_get_watched_spinlock_handler(void)
{
    netsnmp_mib_handler *ret = NULL;
    
    ret = netsnmp_create_handler("watcher-spinlock",
                                 netsnmp_watched_spinlock_handler);
    if (ret) {
        ret->flags |= MIB_HANDLER_AUTO_NEXT;
    }
    return ret;
}

int
netsnmp_register_watched_spinlock(netsnmp_handler_registration *reginfo,
                                   int *spinlock)
{
    netsnmp_mib_handler  *whandler;
    netsnmp_watcher_info *winfo;

    whandler         = netsnmp_get_watched_spinlock_handler();
    whandler->myvoid = (void *)spinlock;
    winfo            = netsnmp_create_watcher_info((void *)spinlock,
		           sizeof(int), ASN_INTEGER, WATCHER_FIXED_SIZE);
    netsnmp_inject_handler(reginfo, whandler);
    return netsnmp_register_watched_scalar2(reginfo, winfo);
}


int
netsnmp_watched_spinlock_handler(netsnmp_mib_handler *handler,
                               netsnmp_handler_registration *reginfo,
                               netsnmp_agent_request_info *reqinfo,
                               netsnmp_request_info *requests)
{
    int     *spinlock = (int *) handler->myvoid;
    netsnmp_request_info *request;

    DEBUGMSGTL(("helper:watcher:spinlock",
                               "Got request:  %d\n", reqinfo->mode));
    DEBUGMSGTL(( "helper:watcher:spinlock", "  oid:"));
    DEBUGMSGOID(("helper:watcher:spinlock", requests->requestvb->name,
                                   requests->requestvb->name_length));
    DEBUGMSG((   "helper:watcher:spinlock", "\n"));

    switch (reqinfo->mode) {
        /*
         * Ensure the assigned value matches the current one
         */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
        for (request=requests; request; request=request->next) {
            if (request->processed)
                continue;

            if (*request->requestvb->val.integer != *spinlock) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_WRONGVALUE);
                handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
                return SNMP_ERR_WRONGVALUE;

            }
        }
        break;

        /*
         * Everything else worked, so increment the spinlock
         */
    case MODE_SET_COMMIT:
	(*spinlock)++;
	break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */
    }

    /* next handler called automatically - 'AUTO_NEXT' */
    return SNMP_ERR_NOERROR;
}
#endif /* NETSNMP_FEATURE_REMOVE_WATCHER_SPINLOCK */

    /***************************
     *
     *   Convenience registration routines - modelled on
     *   the equivalent netsnmp_register_*_instance() calls
     *
     ***************************/

netsnmp_watcher_info *
netsnmp_clone_watcher_info(netsnmp_watcher_info *winfo)
{
    netsnmp_watcher_info *copy = malloc(sizeof(*copy));
    if (copy)
	*copy = *winfo;
    return copy;
}

static int
register_scalar_watcher(const char* name,
                        const oid* reg_oid, size_t reg_oid_len,
                        void *data, size_t size, u_char type,
                        Netsnmp_Node_Handler * subhandler, int mode)
{
    netsnmp_handler_registration *reginfo = NULL;
    netsnmp_mib_handler *whandler = NULL;
    netsnmp_watcher_info* watchinfo =
        netsnmp_create_watcher_info(data, size, type, WATCHER_FIXED_SIZE);
    if (watchinfo)
        whandler = netsnmp_get_watcher_handler();
    if (watchinfo && whandler) {
        whandler->myvoid = watchinfo;
	netsnmp_owns_watcher_info(whandler);
        reginfo =
            netsnmp_create_handler_registration(
                name, subhandler, reg_oid, reg_oid_len, mode);
    }
    if (watchinfo && whandler && reginfo) {
        netsnmp_inject_handler(reginfo, whandler);
        return netsnmp_register_scalar(reginfo);
    }
    if (whandler)
        netsnmp_handler_free(whandler);
    else if (watchinfo)
        free(watchinfo);
    return SNMP_ERR_RESOURCEUNAVAILABLE;
}

#ifndef NETSNMP_FEATURE_REMOVE_WATCHER_ULONG_SCALAR
int
netsnmp_register_ulong_scalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              u_long * it,
                              Netsnmp_Node_Handler * subhandler)
{
    return register_scalar_watcher(
        name, reg_oid, reg_oid_len,
        (void *)it, sizeof( u_long ),
        ASN_UNSIGNED, subhandler, HANDLER_CAN_RWRITE);
}
#endif /* NETSNMP_FEATURE_REMOVE_WATCHER_ULONG_SCALAR */

#ifndef NETSNMP_FEATURE_REMOVE_WATCHER_READ_ONLY_ULONG_SCALAR
int
netsnmp_register_read_only_ulong_scalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              u_long * it,
                              Netsnmp_Node_Handler * subhandler)
{
    return register_scalar_watcher(
        name, reg_oid, reg_oid_len,
        (void *)it, sizeof( u_long ),
        ASN_UNSIGNED, subhandler, HANDLER_CAN_RONLY);
}
#endif /* NETSNMP_FEATURE_REMOVE_WATCHER_READ_ONLY_ULONG_SCALAR */

#ifndef NETSNMP_FEATURE_REMOVE_WATCHER_LONG_SCALAR
int
netsnmp_register_long_scalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              long * it,
                              Netsnmp_Node_Handler * subhandler)
{
    return register_scalar_watcher(
        name, reg_oid, reg_oid_len,
        (void *)it, sizeof( long ),
        ASN_INTEGER, subhandler, HANDLER_CAN_RWRITE);
}
#endif /* NETSNMP_FEATURE_REMOVE_WATCHER_LONG_SCALAR */

#ifndef NETSNMP_FEATURE_REMOVE_WATCHER_READ_ONLY_LONG_SCALAR
int
netsnmp_register_read_only_long_scalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              long * it,
                              Netsnmp_Node_Handler * subhandler)
{
    return register_scalar_watcher(
        name, reg_oid, reg_oid_len,
        (void *)it, sizeof( long ),
        ASN_INTEGER, subhandler, HANDLER_CAN_RONLY);
}
#endif /* NETSNMP_FEATURE_REMOVE_WATCHER_READ_ONLY_LONG_SCALAR */


#ifndef NETSNMP_FEATURE_REMOVE_WATCHER_INT_SCALAR
int
netsnmp_register_int_scalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              int * it,
                              Netsnmp_Node_Handler * subhandler)
{
    return register_scalar_watcher(
        name, reg_oid, reg_oid_len,
        (void *)it, sizeof( int ),
        ASN_INTEGER, subhandler, HANDLER_CAN_RWRITE);
}
#endif /* NETSNMP_FEATURE_REMOVE_WATCHER_INT_SCALAR */

#ifndef NETSNMP_FEATURE_REMOVE_WATCHER_READ_ONLY_INT_SCALAR
int
netsnmp_register_read_only_int_scalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              int * it,
                              Netsnmp_Node_Handler * subhandler)
{
    return register_scalar_watcher(
        name, reg_oid, reg_oid_len,
        (void *)it, sizeof( int ),
        ASN_INTEGER, subhandler, HANDLER_CAN_RONLY);
}
#endif /* NETSNMP_FEATURE_REMOVE_WATCHER_READ_ONLY_INT_SCALAR */

#ifndef NETSNMP_FEATURE_REMOVE_READ_ONLY_COUNTER32_SCALAR
int
netsnmp_register_read_only_counter32_scalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              u_long * it,
                              Netsnmp_Node_Handler * subhandler)
{
    return register_scalar_watcher(
        name, reg_oid, reg_oid_len,
        (void *)it, sizeof( u_long ),
        ASN_COUNTER, subhandler, HANDLER_CAN_RONLY);
}
#endif /* NETSNMP_FEATURE_REMOVE_READ_ONLY_COUNTER32_SCALAR */
/**  @} */

