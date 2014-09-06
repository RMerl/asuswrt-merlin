#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/table_tdata.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/agent/table.h>
#include <net-snmp/agent/table_container.h>
#include <net-snmp/agent/read_only.h>

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

netsnmp_feature_child_of(table_tdata_all, mib_helpers)
netsnmp_feature_child_of(table_tdata, table_tdata_all)
netsnmp_feature_child_of(table_tdata_delete_table, table_tdata_all)
netsnmp_feature_child_of(table_tdata_extract_table, table_tdata_all)
netsnmp_feature_child_of(table_tdata_remove_row, table_tdata_all)
netsnmp_feature_child_of(table_tdata_insert_row, table_tdata_all)

#ifdef NETSNMP_FEATURE_REQUIRE_TABLE_TDATA
netsnmp_feature_require(table_container_row_insert)
#ifdef NETSNMP_FEATURE_REQUIRE_TABLE_TDATA_REMOVE_ROW
netsnmp_feature_require(table_container_row_remove)
#endif /* NETSNMP_FEATURE_REQUIRE_TABLE_TDATA_REMOVE_ROW */
#endif /* NETSNMP_FEATURE_REQUIRE_TABLE_TDATA */

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_TDATA

/** @defgroup tdata tdata
 *  Implement a table with datamatted storage.
 *  @ingroup table
 *
 *  This helper helps you implement a table where all the rows are
 *  expected to be stored within the agent itself and not in some
 *  external storage location.  It can be used to store a list of
 *  rows, where a row consists of the indexes to the table and a
 *  generic data pointer.  You can then implement a subhandler which
 *  is passed the exact row definition and data it must return data
 *  for or accept data for.  Complex GETNEXT handling is greatly
 *  simplified in this case.
 *
 *  @{
 */

/* ==================================
 *
 * TData API: Table maintenance
 *
 * ================================== */

/*
 * generates the index portion of an table oid from a varlist.
 */
void
_netsnmp_tdata_generate_index_oid(netsnmp_tdata_row *row)
{
    build_oid(&row->oid_index.oids, &row->oid_index.len, NULL, 0, row->indexes);
}

/** creates and returns a 'tdata' table data structure */
netsnmp_tdata *
netsnmp_tdata_create_table(const char *name, long flags)
{
    netsnmp_tdata *table = SNMP_MALLOC_TYPEDEF(netsnmp_tdata);
    if ( !table )
        return NULL;

    table->flags = flags;
    if (name)
        table->name = strdup(name);

    if (!(table->flags & TDATA_FLAG_NO_CONTAINER)) {
        table->container = netsnmp_container_find( name );
        if (!table->container)
            table->container = netsnmp_container_find( "table_container" );
        if (table->container)
            table->container->container_name = strdup(name);
    }
    return table;
}

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_TDATA_DELETE_TABLE
/** creates and returns a 'tdata' table data structure */
void
netsnmp_tdata_delete_table(netsnmp_tdata *table)
{
    if (!table)
       return;

    if (table->name)
       free(table->name);
    if (table->container)
       CONTAINER_FREE(table->container);
    
    SNMP_FREE(table);
    return;
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_TDATA_DELETE_TABLE */

/** creates and returns a pointer to new row data structure */
netsnmp_tdata_row *
netsnmp_tdata_create_row(void)
{
    netsnmp_tdata_row *row = SNMP_MALLOC_TYPEDEF(netsnmp_tdata_row);
    return row;
}

/** clones a 'tdata' row. DOES NOT CLONE THE TABLE-SPECIFIC ENTRY DATA. */
netsnmp_feature_child_of(tdata_clone_row, table_tdata_all)
#ifndef NETSNMP_FEATURE_REMOVE_TDATA_CLONE_ROW
netsnmp_tdata_row *
netsnmp_tdata_clone_row(netsnmp_tdata_row *row)
{
    netsnmp_tdata_row *newrow = NULL;
    if (!row)
        return NULL;

    memdup((u_char **) & newrow, (u_char *) row,
           sizeof(netsnmp_tdata_row));
    if (!newrow)
        return NULL;

    if (row->indexes) {
        newrow->indexes = snmp_clone_varbind(newrow->indexes);
        if (!newrow->indexes) {
            SNMP_FREE(newrow);
            return NULL;
        }
    }

    if (row->oid_index.oids) {
        newrow->oid_index.oids =
            snmp_duplicate_objid(row->oid_index.oids, row->oid_index.len);
        if (!newrow->oid_index.oids) {
            if (newrow->indexes)
                snmp_free_varbind(newrow->indexes);
            SNMP_FREE(newrow);
            return NULL;
        }
    }

    return newrow;
}
#endif /* NETSNMP_FEATURE_REMOVE_TDATA_CLONE_ROW */

/** copy the contents of a 'tdata' row.
    DOES NOT COPY THE TABLE-SPECIFIC ENTRY DATA. */
netsnmp_feature_child_of(tdata_copy_row, table_tdata_all)
#ifndef NETSNMP_FEATURE_REMOVE_TDATA_COPY_ROW
int
netsnmp_tdata_copy_row(netsnmp_tdata_row *dst_row, netsnmp_tdata_row *src_row)
{
     if ( !src_row || !dst_row )
         return -1;

    memcpy((u_char *) dst_row, (u_char *) src_row,
           sizeof(netsnmp_tdata_row));
    if (src_row->indexes) {
        dst_row->indexes = snmp_clone_varbind(src_row->indexes);
        if (!dst_row->indexes)
            return -1;
    }

    if (src_row->oid_index.oids) {
        dst_row->oid_index.oids = snmp_duplicate_objid(src_row->oid_index.oids,
                                                       src_row->oid_index.len);
        if (!dst_row->oid_index.oids)
            return -1;
    }

    return 0;
}
#endif /* NETSNMP_FEATURE_REMOVE_TDATA_COPY_ROW */

/** deletes the memory used by the specified row
 *  returns the table-specific entry data
 *  (that it doesn't know how to delete) */
void           *
netsnmp_tdata_delete_row(netsnmp_tdata_row *row)
{
    void           *data;

    if (!row)
        return NULL;

    /*
     * free the memory we can 
     */
    if (row->indexes)
        snmp_free_varbind(row->indexes);
    SNMP_FREE(row->oid_index.oids);
    data = row->data;
    free(row);

    /*
     * return the void * pointer 
     */
    return data;
}

/**
 * Adds a row to the given table (stored in proper lexographical order).
 *
 * returns SNMPERR_SUCCESS on successful addition.
 *      or SNMPERR_GENERR  on failure (E.G., indexes already existed)
 */
int
netsnmp_tdata_add_row(netsnmp_tdata     *table,
                      netsnmp_tdata_row *row)
{
    if (!row || !table)
        return SNMPERR_GENERR;

    if (row->indexes)
        _netsnmp_tdata_generate_index_oid(row);

    if (!row->oid_index.oids) {
        snmp_log(LOG_ERR,
                 "illegal data attempted to be added to table %s (no index)\n",
                 table->name);
        return SNMPERR_GENERR;
    }

    /*
     * The individual index values probably won't be needed,
     *    so this memory can be released.
     * Note that this is purely internal to the helper.
     * The calling application can set this flag as
     *    a hint to the helper that these values aren't
     *    required, but it's up to the helper as to
     *    whether it takes any notice or not!
     */
    if (table->flags & TDATA_FLAG_NO_STORE_INDEXES) {
        snmp_free_varbind(row->indexes);
        row->indexes = NULL;
    }

    /*
     * add this row to the stored table
     */
    if (CONTAINER_INSERT( table->container, row ) != 0)
        return SNMPERR_GENERR;

    DEBUGMSGTL(("tdata_add_row", "added row (%p)\n", row));

    return SNMPERR_SUCCESS;
}

/** swaps out origrow with newrow.  This does *not* delete/free anything! */
netsnmp_feature_child_of(tdata_replace_row, table_tdata_all)
#ifndef NETSNMP_FEATURE_REMOVE_TDATA_REPLACE_ROW
void
netsnmp_tdata_replace_row(netsnmp_tdata *table,
                               netsnmp_tdata_row *origrow,
                               netsnmp_tdata_row *newrow)
{
    netsnmp_tdata_remove_row(table, origrow);
    netsnmp_tdata_add_row(table, newrow);
}
#endif /* NETSNMP_FEATURE_REMOVE_TDATA_REPLACE_ROW */

/**
 * removes a row from the given table and returns it (no free's called)
 *
 * returns the row pointer itself on successful removing.
 *      or NULL on failure (bad arguments)
 */
netsnmp_tdata_row *
netsnmp_tdata_remove_row(netsnmp_tdata *table,
                              netsnmp_tdata_row *row)
{
    if (!row || !table)
        return NULL;

    CONTAINER_REMOVE( table->container, row );
    return row;
}

/**
 * removes and frees a row of the given table and
 *  returns the table-specific entry data
 *
 * returns the void * pointer on successful deletion.
 *      or NULL on failure (bad arguments)
 */
void           *
netsnmp_tdata_remove_and_delete_row(netsnmp_tdata     *table,
                                    netsnmp_tdata_row *row)
{
    if (!row || !table)
        return NULL;

    /*
     * remove it from the list 
     */
    netsnmp_tdata_remove_row(table, row);
    return netsnmp_tdata_delete_row(row);
}


/* ==================================
 *
 * TData API: MIB maintenance
 *
 * ================================== */

Netsnmp_Node_Handler _netsnmp_tdata_helper_handler;

/** Creates a tdata handler and returns it */
netsnmp_mib_handler *
netsnmp_get_tdata_handler(netsnmp_tdata *table)
{
    netsnmp_mib_handler *ret = NULL;

    if (!table) {
        snmp_log(LOG_INFO,
                 "netsnmp_get_tdata_handler(NULL) called\n");
        return NULL;
    }

    ret = netsnmp_create_handler(TABLE_TDATA_NAME,
                               _netsnmp_tdata_helper_handler);
    if (ret) {
        ret->flags |= MIB_HANDLER_AUTO_NEXT;
        ret->myvoid = (void *) table;
    }
    return ret;
}

/*
 * The helper handler that takes care of passing a specific row of
 * data down to the lower handler(s).  The table_container helper
 * has already taken care of identifying the appropriate row of the
 * table (and converting GETNEXT requests into an equivalent GET request)
 * So all we need to do here is make sure that the row is accessible
 * using tdata-style retrieval techniques as well.
 */
int
_netsnmp_tdata_helper_handler(netsnmp_mib_handler *handler,
                                  netsnmp_handler_registration *reginfo,
                                  netsnmp_agent_request_info *reqinfo,
                                  netsnmp_request_info *requests)
{
    netsnmp_tdata *table = (netsnmp_tdata *) handler->myvoid;
    netsnmp_request_info       *request;
    netsnmp_table_request_info *table_info;
    netsnmp_tdata_row          *row;
    int                         need_processing = 1;

    switch ( reqinfo->mode ) {
    case MODE_GET:
        need_processing = 0; /* only need processing if some vars found */
        /** Fall through */

#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
#endif /* NETSNMP_NO_WRITE_SUPPORT */

        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;
    
            table_info = netsnmp_extract_table_info(request);
            if (!table_info) {
                netsnmp_assert(table_info); /* yes, this will always hit */
                netsnmp_set_request_error(reqinfo, request, SNMP_ERR_GENERR);
                continue;           /* eek */
            }
            row = (netsnmp_tdata_row*)netsnmp_container_table_row_extract( request );
            if (!row && (reqinfo->mode == MODE_GET)) {
                netsnmp_assert(row); /* yes, this will always hit */
                netsnmp_set_request_error(reqinfo, request, SNMP_ERR_GENERR);
                continue;           /* eek */
            }
            ++need_processing;
            netsnmp_request_add_list_data(request,
                                      netsnmp_create_data_list(
                                          TABLE_TDATA_TABLE, table, NULL));
            netsnmp_request_add_list_data(request,
                                      netsnmp_create_data_list(
                                          TABLE_TDATA_ROW,   row,   NULL));
        }
        /** skip next handler if processing not needed */
        if (!need_processing)
            handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
    }

    /* next handler called automatically - 'AUTO_NEXT' */
    return SNMP_ERR_NOERROR;
}


/** registers a tdata-based MIB table */
int
netsnmp_tdata_register(netsnmp_handler_registration    *reginfo,
                       netsnmp_tdata                   *table,
                       netsnmp_table_registration_info *table_info)
{
    netsnmp_inject_handler(reginfo, netsnmp_get_tdata_handler(table));
    return netsnmp_container_table_register(reginfo, table_info,
                  table->container, TABLE_CONTAINER_KEY_NETSNMP_INDEX);
}

netsnmp_feature_child_of(tdata_unregister, table_tdata_all)
#ifndef NETSNMP_FEATURE_REMOVE_TDATA_UNREGISTER
int
netsnmp_tdata_unregister(netsnmp_handler_registration    *reginfo)
{
    /* free table; */
    return netsnmp_container_table_unregister(reginfo);
}
#endif /* NETSNMP_FEATURE_REMOVE_TDATA_UNREGISTER */

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_TDATA_EXTRACT_TABLE
/** extracts the tdata table from the request structure */
netsnmp_tdata *
netsnmp_tdata_extract_table(netsnmp_request_info *request)
{
    return (netsnmp_tdata *) netsnmp_request_get_list_data(request,
                                                           TABLE_TDATA_TABLE);
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_TDATA_EXTRACT_TABLE */

/** extracts the tdata container from the request structure */
netsnmp_feature_child_of(tdata_extract_container, table_tdata_all)
#ifndef NETSNMP_FEATURE_REMOVE_TDATA_EXTRACT_CONTAINER
netsnmp_container *
netsnmp_tdata_extract_container(netsnmp_request_info *request)
{
    netsnmp_tdata *tdata = (netsnmp_tdata*)
        netsnmp_request_get_list_data(request, TABLE_TDATA_TABLE);
    return ( tdata ? tdata->container : NULL );
}
#endif /* NETSNMP_FEATURE_REMOVE_TDATA_EXTRACT_CONTAINER */

/** extracts the tdata row being accessed from the request structure */
netsnmp_tdata_row *
netsnmp_tdata_extract_row(netsnmp_request_info *request)
{
    return (netsnmp_tdata_row *) netsnmp_container_table_row_extract(request);
}

/** extracts the (table-specific) entry being accessed from the
 *  request structure */
void           *
netsnmp_tdata_extract_entry(netsnmp_request_info *request)
{
    netsnmp_tdata_row *row =
        (netsnmp_tdata_row *) netsnmp_tdata_extract_row(request);
    if (row)
        return row->data;
    else
        return NULL;
}

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_TDATA_INSERT_ROW
/** inserts a newly created tdata row into a request */
NETSNMP_INLINE void
netsnmp_insert_tdata_row(netsnmp_request_info *request,
                         netsnmp_tdata_row *row)
{
    netsnmp_container_table_row_insert(request, (netsnmp_index *)row);
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_TDATA_INSERT_ROW */

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_TDATA_REMOVE_ROW
/** inserts a newly created tdata row into a request */
NETSNMP_INLINE void
netsnmp_remove_tdata_row(netsnmp_request_info *request,
                         netsnmp_tdata_row *row)
{
    netsnmp_container_table_row_remove(request, (netsnmp_index *)row);
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_TDATA_REMOVE_ROW */


/* ==================================
 *
 * Generic API: Row operations
 *
 * ================================== */

/** returns the (table-specific) entry data for a given row */
void *
netsnmp_tdata_row_entry( netsnmp_tdata_row *row )
{
    if (row)
        return row->data;
    else
        return NULL;
}

/** returns the first row in the table */
netsnmp_tdata_row *
netsnmp_tdata_row_first(netsnmp_tdata *table)
{
    return (netsnmp_tdata_row *)CONTAINER_FIRST( table->container );
}

/** finds a row in the 'tdata' table given another row */
netsnmp_tdata_row *
netsnmp_tdata_row_get(  netsnmp_tdata     *table,
                        netsnmp_tdata_row *row)
{
    return (netsnmp_tdata_row*)CONTAINER_FIND( table->container, row );
}

/** returns the next row in the table */
netsnmp_tdata_row *
netsnmp_tdata_row_next( netsnmp_tdata      *table,
                        netsnmp_tdata_row  *row)
{
    return (netsnmp_tdata_row *)CONTAINER_NEXT( table->container, row  );
}

/** finds a row in the 'tdata' table given the index values */
netsnmp_tdata_row *
netsnmp_tdata_row_get_byidx(netsnmp_tdata         *table,
                            netsnmp_variable_list *indexes)
{
    oid             searchfor[      MAX_OID_LEN];
    size_t          searchfor_len = MAX_OID_LEN;

    build_oid_noalloc(searchfor, MAX_OID_LEN, &searchfor_len, NULL, 0,
                      indexes);
    return netsnmp_tdata_row_get_byoid(table, searchfor, searchfor_len);
}

/** finds a row in the 'tdata' table given the index OID */
netsnmp_tdata_row *
netsnmp_tdata_row_get_byoid(netsnmp_tdata *table,
                            oid * searchfor, size_t searchfor_len)
{
    netsnmp_index index;
    if (!table)
        return NULL;

    index.oids = searchfor;
    index.len  = searchfor_len;
    return (netsnmp_tdata_row*)CONTAINER_FIND( table->container, &index );
}

/** finds the lexically next row in the 'tdata' table
    given the index values */
netsnmp_tdata_row *
netsnmp_tdata_row_next_byidx(netsnmp_tdata         *table,
                             netsnmp_variable_list *indexes)
{
    oid             searchfor[      MAX_OID_LEN];
    size_t          searchfor_len = MAX_OID_LEN;

    build_oid_noalloc(searchfor, MAX_OID_LEN, &searchfor_len, NULL, 0,
                      indexes);
    return netsnmp_tdata_row_next_byoid(table, searchfor, searchfor_len);
}

/** finds the lexically next row in the 'tdata' table
    given the index OID */
netsnmp_tdata_row *
netsnmp_tdata_row_next_byoid(netsnmp_tdata *table,
                             oid * searchfor, size_t searchfor_len)
{
    netsnmp_index index;
    if (!table)
        return NULL;

    index.oids = searchfor;
    index.len  = searchfor_len;
    return (netsnmp_tdata_row*)CONTAINER_NEXT( table->container, &index );
}

netsnmp_feature_child_of(tdata_row_count, table_tdata_all)
#ifndef NETSNMP_FEATURE_REMOVE_TDATA_ROW_COUNT
int
netsnmp_tdata_row_count(netsnmp_tdata *table)
{
    if (!table)
        return 0;
    return CONTAINER_SIZE( table->container );
}
#endif /* NETSNMP_FEATURE_REMOVE_TDATA_ROW_COUNT */

/* ==================================
 *
 * Generic API: Index operations on a 'tdata' table
 *
 * ================================== */


/** compare a row with the given index values */
netsnmp_feature_child_of(tdata_compare_idx, table_tdata_all)
#ifndef NETSNMP_FEATURE_REMOVE_TDATA_COMPARE_IDX
int
netsnmp_tdata_compare_idx(netsnmp_tdata_row     *row,
                          netsnmp_variable_list *indexes)
{
    oid             searchfor[      MAX_OID_LEN];
    size_t          searchfor_len = MAX_OID_LEN;

    build_oid_noalloc(searchfor, MAX_OID_LEN, &searchfor_len, NULL, 0,
                      indexes);
    return netsnmp_tdata_compare_oid(row, searchfor, searchfor_len);
}
#endif /* NETSNMP_FEATURE_REMOVE_TDATA_COMPARE_IDX */

/** compare a row with the given index OID */
int
netsnmp_tdata_compare_oid(netsnmp_tdata_row     *row,
                          oid * compareto, size_t compareto_len)
{
    netsnmp_index *index = (netsnmp_index *)row;
    return snmp_oid_compare( index->oids, index->len,
                             compareto,   compareto_len);
}

int
netsnmp_tdata_compare_subtree_idx(netsnmp_tdata_row     *row,
                                  netsnmp_variable_list *indexes)
{
    oid             searchfor[      MAX_OID_LEN];
    size_t          searchfor_len = MAX_OID_LEN;

    build_oid_noalloc(searchfor, MAX_OID_LEN, &searchfor_len, NULL, 0,
                      indexes);
    return netsnmp_tdata_compare_subtree_oid(row, searchfor, searchfor_len);
}

int
netsnmp_tdata_compare_subtree_oid(netsnmp_tdata_row     *row,
                                  oid * compareto, size_t compareto_len)
{
    netsnmp_index *index = (netsnmp_index *)row;
    return snmp_oidtree_compare( index->oids, index->len,
                                 compareto,   compareto_len);
}
#else /* NETSNMP_FEATURE_REMOVE_TABLE_TDATA */
netsnmp_feature_unused(table_tdata);
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_TDATA */


/** @} 
 */
