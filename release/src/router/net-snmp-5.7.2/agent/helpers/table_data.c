#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/table_data.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/agent/table.h>
#include <net-snmp/agent/read_only.h>

netsnmp_feature_child_of(table_data_all, mib_helpers)

netsnmp_feature_child_of(table_data, table_data_all)
netsnmp_feature_child_of(register_read_only_table_data, table_data_all)
netsnmp_feature_child_of(extract_table_row_data, table_data_all)
netsnmp_feature_child_of(insert_table_row, table_data_all)
netsnmp_feature_child_of(table_data_delete_table, table_data_all)

netsnmp_feature_child_of(table_data_extras, table_data_all)

netsnmp_feature_child_of(table_data_create_table, table_data_extras)
netsnmp_feature_child_of(table_data_create_row, table_data_extras)
netsnmp_feature_child_of(table_data_copy_row, table_data_extras)
netsnmp_feature_child_of(table_data_remove_delete_row, table_data_extras)
netsnmp_feature_child_of(table_data_unregister, table_data_extras)
netsnmp_feature_child_of(table_data_row_count, table_data_extras)
netsnmp_feature_child_of(table_data_row_operations, table_data_extras)
netsnmp_feature_child_of(table_data_row_first, table_data_extras)

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_DATA

/** @defgroup table_data table_data
 *  Helps you implement a table with datamatted storage.
 *  @ingroup table
 *
 *  This helper is obsolete.  If you are writing a new module, please
 *  consider using the table_tdata helper instead.
 *
 *  This helper helps you implement a table where all the indexes are
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
 * Table Data API: Table maintenance
 *
 * ================================== */

/*
 * generates the index portion of an table oid from a varlist.
 */
void
netsnmp_table_data_generate_index_oid(netsnmp_table_row *row)
{
    build_oid(&row->index_oid, &row->index_oid_len, NULL, 0, row->indexes);
}

/** creates and returns a pointer to table data set */
netsnmp_table_data *
netsnmp_create_table_data(const char *name)
{
    netsnmp_table_data *table = SNMP_MALLOC_TYPEDEF(netsnmp_table_data);
    if (name && table)
        table->name = strdup(name);
    return table;
}

/** creates and returns a pointer to table data set */
netsnmp_table_row *
netsnmp_create_table_data_row(void)
{
    netsnmp_table_row *row = SNMP_MALLOC_TYPEDEF(netsnmp_table_row);
    return row;
}

/** clones a data row. DOES NOT CLONE THE CONTAINED DATA. */
netsnmp_table_row *
netsnmp_table_data_clone_row(netsnmp_table_row *row)
{
    netsnmp_table_row *newrow = NULL;
    if (!row)
        return NULL;

    memdup((u_char **) & newrow, (u_char *) row,
           sizeof(netsnmp_table_row));
    if (!newrow)
        return NULL;

    if (row->indexes) {
        newrow->indexes = snmp_clone_varbind(newrow->indexes);
        if (!newrow->indexes) {
            free(newrow);
            return NULL;
        }
    }

    if (row->index_oid) {
        newrow->index_oid =
            snmp_duplicate_objid(row->index_oid, row->index_oid_len);
        if (!newrow->index_oid) {
            free(newrow->indexes);
            free(newrow);
            return NULL;
        }
    }

    return newrow;
}

/** deletes a row's memory.
 *  returns the void data that it doesn't know how to delete. */
void           *
netsnmp_table_data_delete_row(netsnmp_table_row *row)
{
    void           *data;

    if (!row)
        return NULL;

    /*
     * free the memory we can 
     */
    if (row->indexes)
        snmp_free_varbind(row->indexes);
    SNMP_FREE(row->index_oid);
    data = row->data;
    free(row);

    /*
     * return the void * pointer 
     */
    return data;
}

/**
 * Adds a row of data to a given table (stored in proper lexographical order).
 *
 * returns SNMPERR_SUCCESS on successful addition.
 *      or SNMPERR_GENERR  on failure (E.G., indexes already existed)
 */
int
netsnmp_table_data_add_row(netsnmp_table_data *table,
                           netsnmp_table_row *row)
{
    int rc, dup = 0;
    netsnmp_table_row *nextrow = NULL, *prevrow;

    if (!row || !table)
        return SNMPERR_GENERR;

    if (row->indexes)
        netsnmp_table_data_generate_index_oid(row);

    /*
     * we don't store the index info as it
     * takes up memory. 
     */
    if (!table->store_indexes) {
        snmp_free_varbind(row->indexes);
        row->indexes = NULL;
    }

    if (!row->index_oid) {
        snmp_log(LOG_ERR,
                 "illegal data attempted to be added to table %s (no index)\n",
                 table->name);
        return SNMPERR_GENERR;
    }

    /*
     * check for simple append
     */
    if ((prevrow = table->last_row) != NULL) {
        rc = snmp_oid_compare(prevrow->index_oid, prevrow->index_oid_len,
                              row->index_oid, row->index_oid_len);
        if (0 == rc)
            dup = 1;
    }
    else
        rc = 1;
    
    /*
     * if no last row, or newrow < last row, search the table and
     * insert it into the table in the proper oid-lexographical order 
     */
    if (rc > 0) {
        for (nextrow = table->first_row, prevrow = NULL;
             nextrow != NULL; prevrow = nextrow, nextrow = nextrow->next) {
            if (NULL == nextrow->index_oid) {
                DEBUGMSGT(("table_data_add_data", "row doesn't have index!\n"));
                /** xxx-rks: remove invalid row? */
                continue;
            }
            rc = snmp_oid_compare(nextrow->index_oid, nextrow->index_oid_len,
                                  row->index_oid, row->index_oid_len);
            if(rc > 0)
                break;
            if (0 == rc) {
                dup = 1;
                break;
            }
        }
    }

    if (dup) {
        /*
         * exact match.  Duplicate entries illegal 
         */
        snmp_log(LOG_WARNING,
                 "duplicate table data attempted to be entered. row exists\n");
        return SNMPERR_GENERR;
    }

    /*
     * ok, we have the location of where it should go 
     */
    /*
     * (after prevrow, and before nextrow) 
     */
    row->next = nextrow;
    row->prev = prevrow;

    if (row->next)
        row->next->prev = row;

    if (row->prev)
        row->prev->next = row;

    if (NULL == row->prev)      /* it's the (new) first row */
        table->first_row = row;
    if (NULL == row->next)      /* it's the last row */
        table->last_row = row;

    DEBUGMSGTL(("table_data_add_data", "added something...\n"));

    return SNMPERR_SUCCESS;
}

/** swaps out origrow with newrow.  This does *not* delete/free anything! */
void
netsnmp_table_data_replace_row(netsnmp_table_data *table,
                               netsnmp_table_row *origrow,
                               netsnmp_table_row *newrow)
{
    netsnmp_table_data_remove_row(table, origrow);
    netsnmp_table_data_add_row(table, newrow);
}

/**
 * removes a row of data to a given table and returns it (no free's called)
 *
 * returns the row pointer itself on successful removing.
 *      or NULL on failure (bad arguments)
 */
netsnmp_table_row *
netsnmp_table_data_remove_row(netsnmp_table_data *table,
                              netsnmp_table_row *row)
{
    if (!row || !table)
        return NULL;

    if (row->prev)
        row->prev->next = row->next;
    else
        table->first_row = row->next;

    if (row->next)
        row->next->prev = row->prev;
    else
        table->last_row = row->prev;

    return row;
}

/**
 * removes and frees a row of data to a given table and returns the void *
 *
 * returns the void * data on successful deletion.
 *      or NULL on failure (bad arguments)
 */
void           *
netsnmp_table_data_remove_and_delete_row(netsnmp_table_data *table,
                                         netsnmp_table_row *row)
{
    if (!row || !table)
        return NULL;

    /*
     * remove it from the list 
     */
    netsnmp_table_data_remove_row(table, row);
    return netsnmp_table_data_delete_row(row);
}

    /* =====================================
     * Generic API - mostly renamed wrappers
     * ===================================== */

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_DATA_CREATE_TABLE
netsnmp_table_data *
netsnmp_table_data_create_table(const char *name, long flags)
{
    return netsnmp_create_table_data( name );
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_DATA_CREATE_TABLE */

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_DATA_DELETE_TABLE
void
netsnmp_table_data_delete_table( netsnmp_table_data *table )
{
    netsnmp_table_row *row, *nextrow;

    if (!table)
        return;

    snmp_free_varbind(table->indexes_template);
    table->indexes_template = NULL;

    for (row = table->first_row; row; row=nextrow) {
        nextrow   = row->next;
        row->next = NULL;
        netsnmp_table_data_delete_row(row);
        /* Can't delete table-specific entry memory */
    }
    table->first_row = NULL;

    SNMP_FREE(table->name);
    SNMP_FREE(table);
    return;
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_DATA_DELETE_TABLE */

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_DATA_CREATE_ROW
netsnmp_table_row *
netsnmp_table_data_create_row( void* entry )
{
    netsnmp_table_row *row = SNMP_MALLOC_TYPEDEF(netsnmp_table_row);
    if (row)
        row->data = entry;
    return row;
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_DATA_CREATE_ROW */

    /* netsnmp_table_data_clone_row() defined above */

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_DATA_COPY_ROW
int
netsnmp_table_data_copy_row( netsnmp_table_row  *old_row,
                             netsnmp_table_row  *new_row )
{
    if (!old_row || !new_row)
        return -1;

    memcpy(new_row, old_row, sizeof(netsnmp_table_row));

    if (old_row->indexes)
        new_row->indexes = snmp_clone_varbind(old_row->indexes);
    if (old_row->index_oid)
        new_row->index_oid =
            snmp_duplicate_objid(old_row->index_oid, old_row->index_oid_len);
    /* XXX - Doesn't copy table-specific row structure */
    return 0;
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_DATA_COPY_ROW */

    /*
     * netsnmp_table_data_delete_row()
     * netsnmp_table_data_add_row()
     * netsnmp_table_data_replace_row()
     * netsnmp_table_data_remove_row()
     *     all defined above
     */

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_DATA_REMOVE_DELETE_ROW
void *
netsnmp_table_data_remove_delete_row(netsnmp_table_data *table,
                                     netsnmp_table_row *row)
{
    return netsnmp_table_data_remove_and_delete_row(table, row);
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_DATA_REMOVE_DELETE_ROW */


/* ==================================
 *
 * Table Data API: MIB maintenance
 *
 * ================================== */

/** Creates a table_data handler and returns it */
netsnmp_mib_handler *
netsnmp_get_table_data_handler(netsnmp_table_data *table)
{
    netsnmp_mib_handler *ret = NULL;

    if (!table) {
        snmp_log(LOG_INFO,
                 "netsnmp_get_table_data_handler(NULL) called\n");
        return NULL;
    }

    ret =
        netsnmp_create_handler(TABLE_DATA_NAME,
                               netsnmp_table_data_helper_handler);
    if (ret) {
        ret->flags |= MIB_HANDLER_AUTO_NEXT;
        ret->myvoid = (void *) table;
    }
    return ret;
}

/** registers a handler as a data table.
 *  If table_info != NULL, it registers it as a normal table too. */
int
netsnmp_register_table_data(netsnmp_handler_registration *reginfo,
                            netsnmp_table_data *table,
                            netsnmp_table_registration_info *table_info)
{
    netsnmp_inject_handler(reginfo, netsnmp_get_table_data_handler(table));
    return netsnmp_register_table(reginfo, table_info);
}


#ifndef NETSNMP_FEATURE_REMOVE_REGISTER_READ_ONLY_TABLE_DATA
/** registers a handler as a read-only data table
 *  If table_info != NULL, it registers it as a normal table too. */
int
netsnmp_register_read_only_table_data(netsnmp_handler_registration *reginfo,
                                      netsnmp_table_data *table,
                                      netsnmp_table_registration_info *table_info)
{
    netsnmp_inject_handler(reginfo, netsnmp_get_read_only_handler());
    return netsnmp_register_table_data(reginfo, table, table_info);
}
#endif /* NETSNMP_FEATURE_REMOVE_REGISTER_READ_ONLY_TABLE_DATA */

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_DATA_UNREGISTER
int
netsnmp_unregister_table_data(netsnmp_handler_registration *reginfo)
{
    /* free table; */
    return netsnmp_unregister_table(reginfo);
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_DATA_UNREGISTER */

/*
 * The helper handler that takes care of passing a specific row of
 * data down to the lower handler(s).  It sets request->processed if
 * the request should not be handled.
 */
int
netsnmp_table_data_helper_handler(netsnmp_mib_handler *handler,
                                  netsnmp_handler_registration *reginfo,
                                  netsnmp_agent_request_info *reqinfo,
                                  netsnmp_request_info *requests)
{
    netsnmp_table_data *table = (netsnmp_table_data *) handler->myvoid;
    netsnmp_request_info *request;
    int             valid_request = 0;
    netsnmp_table_row *row;
    netsnmp_table_request_info *table_info;
    netsnmp_table_registration_info *table_reg_info =
        netsnmp_find_table_registration_info(reginfo);
    int             result, regresult;
    int             oldmode;

    for (request = requests; request; request = request->next) {
        if (request->processed)
            continue;

        table_info = netsnmp_extract_table_info(request);
        if (!table_info)
            continue;           /* ack */
        switch (reqinfo->mode) {
        case MODE_GET:
        case MODE_GETNEXT:
#ifndef NETSNMP_NO_WRITE_SUPPORT
        case MODE_SET_RESERVE1:
#endif /* NETSNMP_NO_WRITE_SUPPORT */
            netsnmp_request_add_list_data(request,
                                      netsnmp_create_data_list(
                                          TABLE_DATA_TABLE, table, NULL));
        }

        /*
         * find the row in question 
         */
        switch (reqinfo->mode) {
        case MODE_GETNEXT:
        case MODE_GETBULK:     /* XXXWWW */
            if (request->requestvb->type != ASN_NULL)
                continue;
            /*
             * loop through data till we find the next row 
             */
            result = snmp_oid_compare(request->requestvb->name,
                                      request->requestvb->name_length,
                                      reginfo->rootoid,
                                      reginfo->rootoid_len);
            regresult = snmp_oid_compare(request->requestvb->name,
                                         SNMP_MIN(request->requestvb->
                                                  name_length,
                                                  reginfo->rootoid_len),
                                         reginfo->rootoid,
                                         reginfo->rootoid_len);
            if (regresult == 0
                && request->requestvb->name_length < reginfo->rootoid_len)
                regresult = -1;

            if (result < 0 || 0 == result) {
                /*
                 * before us entirely, return the first 
                 */
                row = table->first_row;
                table_info->colnum = table_reg_info->min_column;
            } else if (regresult == 0 && request->requestvb->name_length ==
                       reginfo->rootoid_len + 1 &&
                       /* entry node must be 1, but any column is ok */
                       request->requestvb->name[reginfo->rootoid_len] == 1) {
                /*
                 * exactly to the entry 
                 */
                row = table->first_row;
                table_info->colnum = table_reg_info->min_column;
            } else if (regresult == 0 && request->requestvb->name_length ==
                       reginfo->rootoid_len + 2 &&
                       /* entry node must be 1, but any column is ok */
                       request->requestvb->name[reginfo->rootoid_len] == 1) {
                /*
                 * exactly to the column 
                 */
                row = table->first_row;
            } else {
                /*
                 * loop through all rows looking for the first one
                 * that is equal to the request or greater than it 
                 */
                for (row = table->first_row; row; row = row->next) {
                    /*
                     * compare the index of the request to the row 
                     */
                    result =
                        snmp_oid_compare(row->index_oid,
                                         row->index_oid_len,
                                         request->requestvb->name + 2 +
                                         reginfo->rootoid_len,
                                         request->requestvb->name_length -
                                         2 - reginfo->rootoid_len);
                    if (result == 0) {
                        /*
                         * equal match, return the next row 
                         */
                        row = row->next;
                        break;
                    } else if (result > 0) {
                        /*
                         * the current row is greater than the
                         * request, use it 
                         */
                        break;
                    }
                }
            }
            if (!row) {
                table_info->colnum++;
                if (table_info->colnum <= table_reg_info->max_column) {
                    row = table->first_row;
                }
            }
            if (row) {
                valid_request = 1;
                netsnmp_request_add_list_data(request,
                                              netsnmp_create_data_list
                                              (TABLE_DATA_ROW, row,
                                               NULL));
                /*
                 * Set the name appropriately, so we can pass this
                 *  request on as a simple GET request
                 */
                netsnmp_table_data_build_result(reginfo, reqinfo, request,
                                                row,
                                                table_info->colnum,
                                                ASN_NULL, NULL, 0);
            } else {            /* no decent result found.  Give up. It's beyond us. */
                request->processed = 1;
            }
            break;

        case MODE_GET:
            if (request->requestvb->type != ASN_NULL)
                continue;
            /*
             * find the row in question 
             */
            if (request->requestvb->name_length < (reginfo->rootoid_len + 3)) { /* table.entry.column... */
                /*
                 * request too short 
                 */
                netsnmp_set_request_error(reqinfo, request,
                                          SNMP_NOSUCHINSTANCE);
                break;
            } else if (NULL ==
                       (row =
                        netsnmp_table_data_get_from_oid(table,
                                                        request->
                                                        requestvb->name +
                                                        reginfo->
                                                        rootoid_len + 2,
                                                        request->
                                                        requestvb->
                                                        name_length -
                                                        reginfo->
                                                        rootoid_len -
                                                        2))) {
                /*
                 * no such row 
                 */
                netsnmp_set_request_error(reqinfo, request,
                                          SNMP_NOSUCHINSTANCE);
                break;
            } else {
                valid_request = 1;
                netsnmp_request_add_list_data(request,
                                              netsnmp_create_data_list
                                              (TABLE_DATA_ROW, row,
                                               NULL));
            }
            break;

#ifndef NETSNMP_NO_WRITE_SUPPORT
        case MODE_SET_RESERVE1:
            valid_request = 1;
            if (NULL !=
                (row =
                 netsnmp_table_data_get_from_oid(table,
                                                 request->requestvb->name +
                                                 reginfo->rootoid_len + 2,
                                                 request->requestvb->
                                                 name_length -
                                                 reginfo->rootoid_len -
                                                 2))) {
                netsnmp_request_add_list_data(request,
                                              netsnmp_create_data_list
                                              (TABLE_DATA_ROW, row,
                                               NULL));
            }
            break;

        case MODE_SET_RESERVE2:
        case MODE_SET_ACTION:
        case MODE_SET_COMMIT:
        case MODE_SET_FREE:
        case MODE_SET_UNDO:
            valid_request = 1;
#endif /* NETSNMP_NO_WRITE_SUPPORT */

        }
    }

    if (valid_request &&
       (reqinfo->mode == MODE_GETNEXT || reqinfo->mode == MODE_GETBULK)) {
        /*
         * If this is a GetNext or GetBulk request, then we've identified
         *  the row that ought to include the appropriate next instance.
         *  Convert the request into a Get request, so that the lower-level
         *  handlers don't need to worry about skipping on, and call these
         *  handlers ourselves (so we can undo this again afterwards).
         */
        oldmode = reqinfo->mode;
        reqinfo->mode = MODE_GET;
        result = netsnmp_call_next_handler(handler, reginfo, reqinfo,
                                         requests);
        reqinfo->mode = oldmode;
        handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
        return result;
    }
    else
        /* next handler called automatically - 'AUTO_NEXT' */
        return SNMP_ERR_NOERROR;
}

/** extracts the table being accessed passed from the table_data helper */
netsnmp_table_data *
netsnmp_extract_table(netsnmp_request_info *request)
{
    return (netsnmp_table_data *)
                netsnmp_request_get_list_data(request, TABLE_DATA_TABLE);
}

/** extracts the row being accessed passed from the table_data helper */
netsnmp_table_row *
netsnmp_extract_table_row(netsnmp_request_info *request)
{
    return (netsnmp_table_row *) netsnmp_request_get_list_data(request,
                                                               TABLE_DATA_ROW);
}

#ifndef NETSNMP_FEATURE_REMOVE_EXTRACT_TABLE_ROW_DATA
/** extracts the data from the row being accessed passed from the
 * table_data helper */
void           *
netsnmp_extract_table_row_data(netsnmp_request_info *request)
{
    netsnmp_table_row *row;
    row = (netsnmp_table_row *) netsnmp_extract_table_row(request);
    if (row)
        return row->data;
    else
        return NULL;
}
#endif /* NETSNMP_FEATURE_REMOVE_EXTRACT_TABLE_ROW_DATA */

#ifndef NETSNMP_FEATURE_REMOVE_INSERT_TABLE_ROW
/** inserts a newly created table_data row into a request */
void
netsnmp_insert_table_row(netsnmp_request_info *request,
                         netsnmp_table_row *row)
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
                netsnmp_create_data_list(TABLE_DATA_ROW, row, NULL));
        }
    }
}
#endif /* NETSNMP_FEATURE_REMOVE_INSERT_TABLE_ROW */

/* builds a result given a row, a varbind to set and the data */
int
netsnmp_table_data_build_result(netsnmp_handler_registration *reginfo,
                                netsnmp_agent_request_info *reqinfo,
                                netsnmp_request_info *request,
                                netsnmp_table_row *row,
                                int column,
                                u_char type,
                                u_char * result_data,
                                size_t result_data_len)
{
    oid             build_space[MAX_OID_LEN];

    if (!reginfo || !reqinfo || !request)
        return SNMPERR_GENERR;

    if (reqinfo->mode == MODE_GETNEXT || reqinfo->mode == MODE_GETBULK) {
        /*
         * only need to do this for getnext type cases where oid is changing 
         */
        memcpy(build_space, reginfo->rootoid,   /* registered oid */
               reginfo->rootoid_len * sizeof(oid));
        build_space[reginfo->rootoid_len] = 1;  /* entry */
        build_space[reginfo->rootoid_len + 1] = column; /* column */
        memcpy(build_space + reginfo->rootoid_len + 2,  /* index data */
               row->index_oid, row->index_oid_len * sizeof(oid));
        snmp_set_var_objid(request->requestvb, build_space,
                           reginfo->rootoid_len + 2 + row->index_oid_len);
    }
    snmp_set_var_typed_value(request->requestvb, type,
                             result_data, result_data_len);
    return SNMPERR_SUCCESS;     /* WWWXXX: check for bounds */
}


/* ==================================
 *
 * Table Data API: Row operations
 *     (table-independent rows)
 *
 * ================================== */

/** returns the first row in the table */
netsnmp_table_row *
netsnmp_table_data_get_first_row(netsnmp_table_data *table)
{
    if (!table)
        return NULL;
    return table->first_row;
}

/** returns the next row in the table */
netsnmp_table_row *
netsnmp_table_data_get_next_row(netsnmp_table_data *table,
                                netsnmp_table_row  *row)
{
    if (!row)
        return NULL;
    return row->next;
}

/** finds the data in "datalist" stored at "indexes" */
netsnmp_table_row *
netsnmp_table_data_get(netsnmp_table_data *table,
                       netsnmp_variable_list * indexes)
{
    oid             searchfor[MAX_OID_LEN];
    size_t          searchfor_len = MAX_OID_LEN;

    build_oid_noalloc(searchfor, MAX_OID_LEN, &searchfor_len, NULL, 0,
                      indexes);
    return netsnmp_table_data_get_from_oid(table, searchfor,
                                           searchfor_len);
}

/** finds the data in "datalist" stored at the searchfor oid */
netsnmp_table_row *
netsnmp_table_data_get_from_oid(netsnmp_table_data *table,
                                oid * searchfor, size_t searchfor_len)
{
    netsnmp_table_row *row;
    if (!table)
        return NULL;

    for (row = table->first_row; row != NULL; row = row->next) {
        if (row->index_oid &&
            snmp_oid_compare(searchfor, searchfor_len,
                             row->index_oid, row->index_oid_len) == 0)
            return row;
    }
    return NULL;
}

int
netsnmp_table_data_num_rows(netsnmp_table_data *table)
{
    int i=0;
    netsnmp_table_row *row;
    if (!table)
        return 0;
    for (row = table->first_row; row; row = row->next) {
        i++;
    }
    return i;
}

    /* =====================================
     * Generic API - mostly renamed wrappers
     * ===================================== */

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_DATA_ROW_FIRST
netsnmp_table_row *
netsnmp_table_data_row_first(netsnmp_table_data *table)
{
    return netsnmp_table_data_get_first_row(table);
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_DATA_ROW_FIRST */

netsnmp_table_row *
netsnmp_table_data_row_get(  netsnmp_table_data *table,
                             netsnmp_table_row  *row)
{
    if (!table || !row)
        return NULL;
    return netsnmp_table_data_get_from_oid(table, row->index_oid,
                                                  row->index_oid_len);
}

netsnmp_table_row *
netsnmp_table_data_row_next( netsnmp_table_data *table,
                             netsnmp_table_row  *row)
{
    return netsnmp_table_data_get_next_row(table, row);
}

netsnmp_table_row *
netsnmp_table_data_row_get_byoid( netsnmp_table_data *table,
                                  oid *instance, size_t len)
{
    return netsnmp_table_data_get_from_oid(table, instance, len);
}

netsnmp_table_row *
netsnmp_table_data_row_next_byoid(netsnmp_table_data *table,
                                  oid *instance, size_t len)
{
    netsnmp_table_row *row;

    if (!table || !instance)
        return NULL;
    
    for (row = table->first_row; row; row = row->next) {
        if (snmp_oid_compare(row->index_oid,
                             row->index_oid_len,
                             instance, len) > 0)
            return row;
    }
    return NULL;
}

netsnmp_table_row *
netsnmp_table_data_row_get_byidx( netsnmp_table_data    *table,
                                  netsnmp_variable_list *indexes)
{
    return netsnmp_table_data_get(table, indexes);
}

netsnmp_table_row *
netsnmp_table_data_row_next_byidx(netsnmp_table_data    *table,
                                  netsnmp_variable_list *indexes)
{
    oid    instance[MAX_OID_LEN];
    size_t len    = MAX_OID_LEN;

    if (!table || !indexes)
        return NULL;

    build_oid_noalloc(instance, MAX_OID_LEN, &len, NULL, 0, indexes);
    return netsnmp_table_data_row_next_byoid(table, instance, len);
}

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_DATA_ROW_COUNT
int
netsnmp_table_data_row_count(netsnmp_table_data *table)
{
    return netsnmp_table_data_num_rows(table);
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_DATA_ROW_COUNT */


/* ==================================
 *
 * Table Data API: Row operations
 *     (table-specific rows)
 *
 * ================================== */

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_DATA_ROW_OPERATIONS
void *
netsnmp_table_data_entry_first(netsnmp_table_data *table)
{
    netsnmp_table_row *row =
        netsnmp_table_data_get_first_row(table);
    return (row ? row->data : NULL );
}

void *
netsnmp_table_data_entry_get(  netsnmp_table_data *table,
                               netsnmp_table_row  *row)
{
    return (row ? row->data : NULL );
}

void *
netsnmp_table_data_entry_next( netsnmp_table_data *table,
                               netsnmp_table_row  *row)
{
    row =
        netsnmp_table_data_row_next(table, row);
    return (row ? row->data : NULL );
}

void *
netsnmp_table_data_entry_get_byidx( netsnmp_table_data    *table,
                                    netsnmp_variable_list *indexes)
{
    netsnmp_table_row *row =
        netsnmp_table_data_row_get_byidx(table, indexes);
    return (row ? row->data : NULL );
}

void *
netsnmp_table_data_entry_next_byidx(netsnmp_table_data    *table,
                                    netsnmp_variable_list *indexes)
{
    netsnmp_table_row *row =
        netsnmp_table_data_row_next_byidx(table, indexes);
    return (row ? row->data : NULL );
}

void *
netsnmp_table_data_entry_get_byoid( netsnmp_table_data *table,
                                    oid *instance, size_t len)
{
    netsnmp_table_row *row =
        netsnmp_table_data_row_get_byoid(table, instance, len);
    return (row ? row->data : NULL );
}

void *
netsnmp_table_data_entry_next_byoid(netsnmp_table_data *table,
                                    oid *instance, size_t len)
{
    netsnmp_table_row *row =
        netsnmp_table_data_row_next_byoid(table, instance, len);
    return (row ? row->data : NULL );
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_DATA_ROW_OPERATIONS */

    /* =====================================
     * Generic API - mostly renamed wrappers
     * ===================================== */

/* ==================================
 *
 * Table Data API: Index operations
 *
 * ================================== */

#else /* NETSNMP_FEATURE_REMOVE_TABLE_DATA */
netsnmp_feature_unused(table_data);
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_DATA */

/** @} 
 */
