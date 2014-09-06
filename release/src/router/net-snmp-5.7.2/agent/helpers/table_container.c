/*
 * table_container.c
 * $Id$
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/table_container.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/agent/table.h>
#include <net-snmp/library/container.h>
#include <net-snmp/library/snmp_assert.h>

netsnmp_feature_provide(table_container)
netsnmp_feature_child_of(table_container, table_container_all)
netsnmp_feature_child_of(table_container_replace_row, table_container_all)
netsnmp_feature_child_of(table_container_extract, table_container_all)
netsnmp_feature_child_of(table_container_management, table_container_all)
netsnmp_feature_child_of(table_container_row_remove, table_container_all)
netsnmp_feature_child_of(table_container_row_insert, table_container_all)
netsnmp_feature_child_of(table_container_all, mib_helpers)

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_CONTAINER

/*
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_BEGIN        -1 
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_RESERVE1     0 
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_RESERVE2     1 
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_ACTION       2 
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_COMMIT       3 
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_FREE         4 
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_UNDO         5 
 */

/*
 * PRIVATE structure for holding important info for each table.
 */
typedef struct container_table_data_s {

    /** Number of handlers whose myvoid pointer points to this structure. */
    int refcnt;

   /** registration info for the table */
    netsnmp_table_registration_info *tblreg_info;

   /** container for the table rows */
   netsnmp_container          *table;

    /*
     * mutex_type                lock;
     */

   /* what type of key do we want? */
   char            key_type;

} container_table_data;

/** @defgroup table_container table_container
 *  Helps you implement a table when data can be found via a netsnmp_container.
 *  @ingroup table
 *
 *  The table_container handler is used (automatically) in conjuntion
 *  with the @link table table@endlink handler.
 *
 *  This handler will use the index information provided by
 *  the @link table @endlink handler to find the row needed to process
 *  the request.
 *
 *  The container must use one of 3 key types. It is the sub-handler's
 *  responsibility to ensure that the container and key type match (unless
 *  neither is specified, in which case a default will be used.)
 *
 *  The current key types are:
 *
 *    TABLE_CONTAINER_KEY_NETSNMP_INDEX
 *        The container should do comparisons based on a key that may be cast
 *        to a netsnmp index (netsnmp_index *). This index contains only the
 *        index portion of the OID, not the entire OID.
 *
 *    TABLE_CONTAINER_KEY_VARBIND_INDEX
 *        The container should do comparisons based on a key that may be cast
 *        to a netsnmp variable list (netsnmp_variable_list *). This variable
 *        list will contain one varbind for each index component.
 *
 *    TABLE_CONTAINER_KEY_VARBIND_RAW    (NOTE: unimplemented)
 *        While not yet implemented, future plans include passing the request
 *        varbind with the full OID to a container.
 *
 *  If a key type is not specified at registration time, the default key type
 *  of TABLE_CONTAINER_KEY_NETSNMP_INDEX will be used. If a container is
 *  provided, or the handler name is aliased to a container type, the container
 *  must use a netsnmp index.
 *
 *  If no container is provided, a lookup will be made based on the
 *  sub-handler's name, or if that isn't found, "table_container". The 
 *  table_container key type will be netsnmp_index.
 *
 *  The container must, at a minimum, implement find and find_next. If a NULL
 *  key is passed to the container, it must return the first item, if any.
 *  All containers provided by net-snmp fulfil this requirement.
 *
 *  This handler will only register to process 'data lookup' modes. In
 *  traditional net-snmp modes, that is any GET-like mode (GET, GET-NEXT,
 *  GET-BULK) or the first phase of a SET (RESERVE1). In the new baby-steps
 *  mode, DATA_LOOKUP is it's own mode, and is a pre-cursor to other modes.
 *
 *  When called, the handler will call the appropriate container method
 *  with the appropriate key type. If a row was not found, the result depends
 *  on the mode.
 *
 *  GET Processing
 *    An exact match must be found. If one is not, the error NOSUCHINSTANCE
 *    is set.
 *
 *  GET-NEXT / GET-BULK
 *    If no row is found, the column number will be increased (using any
 *    valid_columns structure that may have been provided), and the first row
 *    will be retrieved. If no first row is found, the processed flag will be
 *    set, so that the sub-handler can skip any processing related to the
 *    request. The agent will notice this unsatisfied request, and attempt to
 *    pass it to the next appropriate handler.
 *
 *  SET
 *    If the hander did not register with the HANDLER_CAN_NOT_CREATE flag
 *    set in the registration modes, it is assumed that this is a row
 *    creation request and a NULL row is added to the request's data list.
 *    The sub-handler is responsbile for dealing with any row creation
 *    contraints and inserting any newly created rows into the container
 *    and the request's data list.
 *
 *  If a row is found, it will be inserted into
 *  the request's data list. The sub-handler may retrieve it by calling
 *      netsnmp_container_table_extract_context(request); *
 *  NOTE NOTE NOTE:
 *
 *  This helper and it's API are still being tested and are subject to change.
 *
 * @{
 */

static int
_container_table_handler(netsnmp_mib_handler *handler,
                         netsnmp_handler_registration *reginfo,
                         netsnmp_agent_request_info *agtreq_info,
                         netsnmp_request_info *requests);

static void *
_find_next_row(netsnmp_container *c,
               netsnmp_table_request_info *tblreq,
               void * key);

/**********************************************************************
 **********************************************************************
 *                                                                    *
 *                                                                    *
 * PUBLIC Registration functions                                      *
 *                                                                    *
 *                                                                    *
 **********************************************************************
 **********************************************************************/

/* ==================================
 *
 * Container Table API: Table maintenance
 *
 * ================================== */

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_CONTAINER_MANAGEMENT
container_table_data *
netsnmp_tcontainer_create_table( const char *name,
                                 netsnmp_container *container, long flags )
{
    container_table_data *table;

    table = SNMP_MALLOC_TYPEDEF(container_table_data);
    if (!table)
        return NULL;
    if (container)
        table->table = container;
    else {
        table->table = netsnmp_container_find("table_container");
        if (!table->table) {
            SNMP_FREE(table);
            return NULL;
        }
    }

    if (flags)
        table->key_type = (char)(flags & 0x03);  /* Use lowest two bits */
    else
        table->key_type = TABLE_CONTAINER_KEY_NETSNMP_INDEX;

    if (!table->table->compare)
         table->table->compare  = netsnmp_compare_netsnmp_index;
    if (!table->table->ncompare)
         table->table->ncompare = netsnmp_ncompare_netsnmp_index;

    return table;
}

void
netsnmp_tcontainer_delete_table( container_table_data *table )
{
    if (!table)
       return;

    if (table->table)
       CONTAINER_FREE(table->table);
    
    SNMP_FREE(table);
    return;
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_CONTAINER_MANAGEMENT */

    /*
     * The various standalone row operation routines
     *    (create/clone/copy/delete)
     * will be specific to a particular table,
     *    so can't be implemented here.
     */

int
netsnmp_tcontainer_add_row( container_table_data *table, netsnmp_index *row )
{
    if (!table || !table->table || !row)
        return -1;
    CONTAINER_INSERT( table->table, row );
    return 0;
}

netsnmp_index *
netsnmp_tcontainer_remove_row( container_table_data *table, netsnmp_index *row )
{
    if (!table || !table->table || !row)
        return NULL;
    CONTAINER_REMOVE( table->table, row );
    return NULL;
}

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_CONTAINER_REPLACE_ROW
int
netsnmp_tcontainer_replace_row( container_table_data *table,
                                netsnmp_index *old_row, netsnmp_index *new_row )
{
    if (!table || !table->table || !old_row || !new_row)
        return -1;
    netsnmp_tcontainer_remove_row( table, old_row );
    netsnmp_tcontainer_add_row(    table, new_row );
    return 0;
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_CONTAINER_REPLACE_ROW */

    /* netsnmp_tcontainer_remove_delete_row() will be table-specific too */


/* ==================================
 *
 * Container Table API: MIB maintenance
 *
 * ================================== */

static container_table_data *
netsnmp_container_table_data_clone(container_table_data *tad)
{
    ++tad->refcnt;
    return tad;
}

static void
netsnmp_container_table_data_free(container_table_data *tad)
{
    if (--tad->refcnt == 0)
	free(tad);
}

/** returns a netsnmp_mib_handler object for the table_container helper */
netsnmp_mib_handler *
netsnmp_container_table_handler_get(netsnmp_table_registration_info *tabreg,
                                    netsnmp_container *container, char key_type)
{
    container_table_data *tad;
    netsnmp_mib_handler *handler;

    if (NULL == tabreg) {
        snmp_log(LOG_ERR, "bad param in netsnmp_container_table_register\n");
        return NULL;
    }

    tad = SNMP_MALLOC_TYPEDEF(container_table_data);
    handler = netsnmp_create_handler("table_container",
                                     _container_table_handler);
    if((NULL == tad) || (NULL == handler)) {
        if(tad) free(tad); /* SNMP_FREE wasted on locals */
        if(handler) free(handler); /* SNMP_FREE wasted on locals */
        snmp_log(LOG_ERR,
                 "malloc failure in netsnmp_container_table_register\n");
        return NULL;
    }

    tad->refcnt = 1;
    tad->tblreg_info = tabreg;  /* we need it too, but it really is not ours */
    if(key_type)
        tad->key_type = key_type;
    else
        tad->key_type = TABLE_CONTAINER_KEY_NETSNMP_INDEX;

    if(NULL == container)
        container = netsnmp_container_find("table_container");
    tad->table = container;

    if (NULL==container->compare)
        container->compare = netsnmp_compare_netsnmp_index;
    if (NULL==container->ncompare)
        container->ncompare = netsnmp_ncompare_netsnmp_index;
    
    handler->myvoid = (void*)tad;
    handler->data_clone = (void *(*)(void *))netsnmp_container_table_data_clone;
    handler->data_free = (void (*)(void *))netsnmp_container_table_data_free;
    handler->flags |= MIB_HANDLER_AUTO_NEXT;
    
    return handler;
}

int
netsnmp_container_table_register(netsnmp_handler_registration *reginfo,
                                 netsnmp_table_registration_info *tabreg,
                                 netsnmp_container *container, char key_type )
{
    netsnmp_mib_handler *handler;

    if ((NULL == reginfo) || (NULL == reginfo->handler) || (NULL == tabreg)) {
        snmp_log(LOG_ERR, "bad param in netsnmp_container_table_register\n");
        return SNMPERR_GENERR;
    }

    if (NULL==container)
        container = netsnmp_container_find(reginfo->handlerName);

    handler = netsnmp_container_table_handler_get(tabreg, container, key_type);
    netsnmp_inject_handler(reginfo, handler );

    return netsnmp_register_table(reginfo, tabreg);
}

int
netsnmp_container_table_unregister(netsnmp_handler_registration *reginfo)
{
    container_table_data *tad;

    if (!reginfo)
        return MIB_UNREGISTRATION_FAILED;
    tad = (container_table_data *)
        netsnmp_find_handler_data_by_name(reginfo, "table_container");
    if (tad) {
        CONTAINER_FREE( tad->table );
        tad->table = NULL;
	/*
	 * Note: don't free the memory tad points at here - that is done
	 * by netsnmp_container_table_data_free().
	 */
    }
    return netsnmp_unregister_table( reginfo );
}

/** retrieve the container used by the table_container helper */
#ifndef NETSNMP_FEATURE_REMOVE_TABLE_CONTAINER_EXTRACT
netsnmp_container*
netsnmp_container_table_container_extract(netsnmp_request_info *request)
{
    return (netsnmp_container *)
         netsnmp_request_get_list_data(request, TABLE_CONTAINER_CONTAINER);
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_CONTAINER_EXTRACT */

#ifndef NETSNMP_USE_INLINE
/** find the context data used by the table_container helper */
void *
netsnmp_container_table_row_extract(netsnmp_request_info *request)
{
    /*
     * NOTE: this function must match in table_container.c and table_container.h.
     *       if you change one, change them both!
     */
    return netsnmp_request_get_list_data(request, TABLE_CONTAINER_ROW);
}
/** find the context data used by the table_container helper */
void *
netsnmp_container_table_extract_context(netsnmp_request_info *request)
{
    /*
     * NOTE: this function must match in table_container.c and table_container.h.
     *       if you change one, change them both!
     */
    return netsnmp_request_get_list_data(request, TABLE_CONTAINER_ROW);
}
#endif /* inline */

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_CONTAINER_ROW_INSERT
/** inserts a newly created table_container entry into a request list */
void
netsnmp_container_table_row_insert(netsnmp_request_info *request,
                                   netsnmp_index        *row)
{
    netsnmp_request_info       *req;
    netsnmp_table_request_info *table_info = NULL;
    netsnmp_variable_list      *this_index = NULL;
    netsnmp_variable_list      *that_index = NULL;
    oid      base_oid[] = {0, 0};	/* Make sure index OIDs are legal! */
    oid      this_oid[MAX_OID_LEN];
    oid      that_oid[MAX_OID_LEN];
    size_t   this_oid_len, that_oid_len;

    if (!request)
        return;

    /*
     * We'll add the new row information to any request
     * structure with the same index values as the request
     * passed in (which includes that one!).
     *
     * So construct an OID based on these index values.
     */

    table_info = netsnmp_extract_table_info(request);
    this_index = table_info->indexes;
    build_oid_noalloc(this_oid, MAX_OID_LEN, &this_oid_len,
                      base_oid, 2, this_index);

    /*
     * We need to look through the whole of the request list
     * (as received by the current handler), as there's no
     * guarantee that this routine will be called by the first
     * varbind that refers to this row.
     *   In particular, a RowStatus controlled row creation
     * may easily occur later in the variable list.
     *
     * So first, we rewind to the head of the list....
     */
    for (req=request; req->prev; req=req->prev)
        ;

    /*
     * ... and then start looking for matching indexes
     * (by constructing OIDs from these index values)
     */
    for (; req; req=req->next) {
        if (req->processed) 
            continue;
        
        table_info = netsnmp_extract_table_info(req);
        that_index = table_info->indexes;
        build_oid_noalloc(that_oid, MAX_OID_LEN, &that_oid_len,
                          base_oid, 2, that_index);
      
        /*
         * This request has the same index values,
         * so add the newly-created row information.
         */
        if (snmp_oid_compare(this_oid, this_oid_len,
                             that_oid, that_oid_len) == 0) {
            netsnmp_request_add_list_data(req,
                netsnmp_create_data_list(TABLE_CONTAINER_ROW, row, NULL));
        }
    }
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_CONTAINER_ROW_INSERT */

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_CONTAINER_ROW_REMOVE
/** removes a table_container entry from a request list */
void
netsnmp_container_table_row_remove(netsnmp_request_info *request,
                                   netsnmp_index        *row)
{
    netsnmp_request_info       *req;
    netsnmp_table_request_info *table_info = NULL;
    netsnmp_variable_list      *this_index = NULL;
    netsnmp_variable_list      *that_index = NULL;
    oid      base_oid[] = {0, 0};	/* Make sure index OIDs are legal! */
    oid      this_oid[MAX_OID_LEN];
    oid      that_oid[MAX_OID_LEN];
    size_t   this_oid_len, that_oid_len;

    if (!request)
        return;

    /*
     * We'll add the new row information to any request
     * structure with the same index values as the request
     * passed in (which includes that one!).
     *
     * So construct an OID based on these index values.
     */

    table_info = netsnmp_extract_table_info(request);
    this_index = table_info->indexes;
    build_oid_noalloc(this_oid, MAX_OID_LEN, &this_oid_len,
                      base_oid, 2, this_index);

    /*
     * We need to look through the whole of the request list
     * (as received by the current handler), as there's no
     * guarantee that this routine will be called by the first
     * varbind that refers to this row.
     *   In particular, a RowStatus controlled row creation
     * may easily occur later in the variable list.
     *
     * So first, we rewind to the head of the list....
     */
    for (req=request; req->prev; req=req->prev)
        ;

    /*
     * ... and then start looking for matching indexes
     * (by constructing OIDs from these index values)
     */
    for (; req; req=req->next) {
        if (req->processed) 
            continue;
        
        table_info = netsnmp_extract_table_info(req);
        that_index = table_info->indexes;
        build_oid_noalloc(that_oid, MAX_OID_LEN, &that_oid_len,
                          base_oid, 2, that_index);
      
        /*
         * This request has the same index values,
         * so add the newly-created row information.
         */
        if (snmp_oid_compare(this_oid, this_oid_len,
                             that_oid, that_oid_len) == 0) {
            netsnmp_request_remove_list_data(req, TABLE_CONTAINER_ROW);
        }
    }
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_CONTAINER_ROW_REMOVE */

/** @cond */
/**********************************************************************
 **********************************************************************
 *                                                                    *
 *                                                                    *
 * DATA LOOKUP functions                                              *
 *                                                                    *
 *                                                                    *
 **********************************************************************
 **********************************************************************/
NETSNMP_STATIC_INLINE void
_set_key( container_table_data * tad, netsnmp_request_info *request,
          netsnmp_table_request_info *tblreq_info,
          void **key, netsnmp_index *index )
{
    if (TABLE_CONTAINER_KEY_NETSNMP_INDEX == tad->key_type) {
        index->oids = tblreq_info->index_oid;
        index->len = tblreq_info->index_oid_len;
        *key = index;
    }
    else if (TABLE_CONTAINER_KEY_VARBIND_INDEX == tad->key_type) {
        *key = tblreq_info->indexes;
    }
#if 0
    else if (TABLE_CONTAINER_KEY_VARBIND_RAW == tad->key_type) {
        *key = request->requestvb;
    }
#endif
    else
        *key = NULL;
}


NETSNMP_STATIC_INLINE void
_data_lookup(netsnmp_handler_registration *reginfo,
            netsnmp_agent_request_info *agtreq_info,
            netsnmp_request_info *request, container_table_data * tad)
{
    netsnmp_index *row = NULL;
    netsnmp_table_request_info *tblreq_info;
    netsnmp_variable_list *var;
    netsnmp_index index;
    void *key;

    var = request->requestvb;

    DEBUGIF("table_container") {
        DEBUGMSGTL(("table_container", "  data_lookup oid:"));
        DEBUGMSGOID(("table_container", var->name, var->name_length));
        DEBUGMSG(("table_container", "\n"));
    }

    /*
     * Get pointer to the table information for this request. This
     * information was saved by table_helper_handler.
     */
    tblreq_info = netsnmp_extract_table_info(request);
    /** the table_helper_handler should enforce column boundaries. */
    netsnmp_assert((NULL != tblreq_info) &&
                   (tblreq_info->colnum <= tad->tblreg_info->max_column));
    
    if ((agtreq_info->mode == MODE_GETNEXT) ||
        (agtreq_info->mode == MODE_GETBULK)) {
        /*
         * find the row. This will automatically move to the next
         * column, if necessary.
         */
        _set_key( tad, request, tblreq_info, &key, &index );
        row = (netsnmp_index*)_find_next_row(tad->table, tblreq_info, key);
        if (row) {
            /*
             * update indexes in tblreq_info (index & varbind),
             * then update request varbind oid
             */
            if(TABLE_CONTAINER_KEY_NETSNMP_INDEX == tad->key_type) {
                tblreq_info->index_oid_len = row->len;
                memcpy(tblreq_info->index_oid, row->oids,
                       row->len * sizeof(oid));
                netsnmp_update_variable_list_from_index(tblreq_info);
            }
            else if (TABLE_CONTAINER_KEY_VARBIND_INDEX == tad->key_type) {
                /** xxx-rks: shouldn't tblreq_info->indexes be updated
                    before we call this?? */
                netsnmp_update_indexes_from_variable_list(tblreq_info);
            }

            if (TABLE_CONTAINER_KEY_VARBIND_RAW != tad->key_type) {
                netsnmp_table_build_oid_from_index(reginfo, request,
                                                   tblreq_info);
            }
        }
        else {
            /*
             * no results found. Flag the request so lower handlers will
             * ignore it, but it is not an error - getnext will move
             * on to another handler to process this request.
             */
            netsnmp_set_request_error(agtreq_info, request, SNMP_ENDOFMIBVIEW);
            DEBUGMSGTL(("table_container", "no row found\n"));
        }
    } /** GETNEXT/GETBULK */
    else {

        _set_key( tad, request, tblreq_info, &key, &index );
        row = (netsnmp_index*)CONTAINER_FIND(tad->table, key);
        if (NULL == row) {
            /*
             * not results found. For a get, that is an error
             */
            DEBUGMSGTL(("table_container", "no row found\n"));
#ifndef NETSNMP_NO_WRITE_SUPPORT
            if((agtreq_info->mode != MODE_SET_RESERVE1) || /* get */
               (reginfo->modes & HANDLER_CAN_NOT_CREATE)) { /* no create */
#endif /* NETSNMP_NO_WRITE_SUPPORT */
                netsnmp_set_request_error(agtreq_info, request,
                                          SNMP_NOSUCHINSTANCE);
#ifndef NETSNMP_NO_WRITE_SUPPORT
            }
#endif /* NETSNMP_NO_WRITE_SUPPORT */
        }
    } /** GET/SET */
    
    /*
     * save the data and table in the request.
     */
    if (SNMP_ENDOFMIBVIEW != request->requestvb->type) {
        if (NULL != row)
            netsnmp_request_add_list_data(request,
                                          netsnmp_create_data_list
                                          (TABLE_CONTAINER_ROW,
                                           row, NULL));
        netsnmp_request_add_list_data(request,
                                      netsnmp_create_data_list
                                      (TABLE_CONTAINER_CONTAINER,
                                       tad->table, NULL));
    }
}

/**********************************************************************
 **********************************************************************
 *                                                                    *
 *                                                                    *
 * netsnmp_table_container_helper_handler()                           *
 *                                                                    *
 *                                                                    *
 **********************************************************************
 **********************************************************************/
static int
_container_table_handler(netsnmp_mib_handler *handler,
                         netsnmp_handler_registration *reginfo,
                         netsnmp_agent_request_info *agtreq_info,
                         netsnmp_request_info *requests)
{
    int             rc = SNMP_ERR_NOERROR;
    int             oldmode, need_processing = 0;
    container_table_data *tad;

    /** sanity checks */
    netsnmp_assert((NULL != handler) && (NULL != handler->myvoid));
    netsnmp_assert((NULL != reginfo) && (NULL != agtreq_info));

    DEBUGMSGTL(("table_container", "Mode %s, Got request:\n",
                se_find_label_in_slist("agent_mode",agtreq_info->mode)));

    /*
     * First off, get our pointer from the handler. This
     * lets us get to the table registration information we
     * saved in get_table_container_handler(), as well as the
     * container where the actual table data is stored.
     */
    tad = (container_table_data *)handler->myvoid;

    /*
     * only do data lookup for first pass
     *
     * xxx-rks: this should really be handled up one level. we should
     * be able to say what modes we want to be called for during table
     * registration.
     */
    oldmode = agtreq_info->mode;
    if(MODE_IS_GET(oldmode)
#ifndef NETSNMP_NO_WRITE_SUPPORT
       || (MODE_SET_RESERVE1 == oldmode)
#endif /* NETSNMP_NO_WRITE_SUPPORT */
        ) {
        netsnmp_request_info *curr_request;
        /*
         * Loop through each of the requests, and
         * try to find the appropriate row from the container.
         */
        for (curr_request = requests; curr_request; curr_request = curr_request->next) {
            /*
             * skip anything that doesn't need processing.
             */
            if (curr_request->processed != 0) {
                DEBUGMSGTL(("table_container", "already processed\n"));
                continue;
            }
            
            /*
             * find data for this request
             */
            _data_lookup(reginfo, agtreq_info, curr_request, tad);

            if(curr_request->processed)
                continue;

            ++need_processing;
        } /** for ( ... requests ... ) */
    }
    
    /*
     * send GET instead of GETNEXT to sub-handlers
     * xxx-rks: again, this should be handled further up.
     */
    if ((oldmode == MODE_GETNEXT) && (handler->next)) {
        /*
         * tell agent handlder not to auto call next handler
         */
        handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;

        /*
         * if we found rows to process, pretend to be a get request
         * and call handler below us.
         */
        if(need_processing > 0) {
            agtreq_info->mode = MODE_GET;
            rc = netsnmp_call_next_handler(handler, reginfo, agtreq_info,
                                           requests);
            if (rc != SNMP_ERR_NOERROR) {
                DEBUGMSGTL(("table_container",
                            "next handler returned %d\n", rc));
            }

            agtreq_info->mode = oldmode; /* restore saved mode */
        }
    }

    return rc;
}
/** @endcond */


/* ==================================
 *
 * Container Table API: Row operations
 *
 * ================================== */

static void *
_find_next_row(netsnmp_container *c,
               netsnmp_table_request_info *tblreq,
               void * key)
{
    void *row = NULL;

    if (!c || !tblreq || !tblreq->reg_info ) {
        snmp_log(LOG_ERR,"_find_next_row param error\n");
        return NULL;
    }

    /*
     * table helper should have made sure we aren't below our minimum column
     */
    netsnmp_assert(tblreq->colnum >= tblreq->reg_info->min_column);

    /*
     * if no indexes then use first row.
     */
    if(tblreq->number_indexes == 0) {
        row = CONTAINER_FIRST(c);
    } else {

        if(NULL == key) {
            netsnmp_index index;
            index.oids = tblreq->index_oid;
            index.len = tblreq->index_oid_len;
            row = CONTAINER_NEXT(c, &index);
        }
        else
            row = CONTAINER_NEXT(c, key);

        /*
         * we don't have a row, but we might be at the end of a
         * column, so try the next column.
         */
        if (NULL == row) {
            /*
             * don't set tblreq next_col unless we know there is one,
             * so we don't mess up table handler sparse table processing.
             */
            oid next_col = netsnmp_table_next_column(tblreq);
            if (0 != next_col) {
                tblreq->colnum = next_col;
                row = CONTAINER_FIRST(c);
            }
        }
    }
    
    return row;
}

/**
 * deprecated, backwards compatability only
 *
 * expected impact to remove: none
 *  - used between helpers, shouldn't have been used by end users
 *
 * replacement: none
 *  - never should have been a public method in the first place
 */
netsnmp_index *
netsnmp_table_index_find_next_row(netsnmp_container *c,
                                  netsnmp_table_request_info *tblreq)
{
    return (netsnmp_index*)_find_next_row(c, tblreq, NULL );
}

/* ==================================
 *
 * Container Table API: Index operations
 *
 * ================================== */

#else /* NETSNMP_FEATURE_REMOVE_TABLE_CONTAINER */
netsnmp_feature_unused(table_container);
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_CONTAINER */
/** @} */


