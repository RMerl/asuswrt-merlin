/*
 * table_row.c
 *
 * Helper for registering single row slices of a shared table
 *
 * $Id$
 */
#define TABLE_ROW_DATA  "table_row"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/table.h>
#include <net-snmp/agent/table_container.h>
#include <net-snmp/library/container.h>
#include <net-snmp/library/snmp_assert.h>

netsnmp_feature_child_of(table_row_all, mib_helpers)

netsnmp_feature_child_of(table_row_extract, table_row_all)


/*
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_BEGIN        -1 
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_RESERVE1     0 
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_RESERVE2     1 
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_ACTION       2 
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_COMMIT       3 
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_FREE         4 
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_UNDO         5 
 */

/** @defgroup table_row table_row
 *  Helps you implement a table shared across two or more subagents,
 *  or otherwise split into individual row slices.
 *  @ingroup table
 *
 * @{
 */

static Netsnmp_Node_Handler _table_row_handler;
static Netsnmp_Node_Handler _table_row_default_handler;

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
 * Table Row API: Table maintenance
 *
 * This helper doesn't operate with the complete
 *   table, so these routines are not relevant.
 *
 * ================================== */


/* ==================================
 *
 * Table Row API: MIB maintenance
 *
 * ================================== */

/** returns a netsnmp_mib_handler object for the table_container helper */
netsnmp_mib_handler *
netsnmp_table_row_handler_get(void *row)
{
    netsnmp_mib_handler *handler;

    handler = netsnmp_create_handler("table_row",
                                     _table_row_handler);
    if(NULL == handler) {
        snmp_log(LOG_ERR,
                 "malloc failure in netsnmp_table_row_register\n");
        return NULL;
    }

    handler->myvoid = (void*)row;
    handler->flags |= MIB_HANDLER_INSTANCE;
 /* handler->flags |= MIB_HANDLER_AUTO_NEXT;  ??? */
    
    return handler;
}

int
netsnmp_table_row_register(netsnmp_handler_registration *reginfo,
                           netsnmp_table_registration_info *tabreg,
                           void *row, netsnmp_variable_list *index)
{
    netsnmp_handler_registration *reg2;
    netsnmp_mib_handler *handler;
    oid    row_oid[MAX_OID_LEN];
    size_t row_oid_len, len;
    char   tmp[SNMP_MAXBUF_MEDIUM];

    if ((NULL == reginfo) || (NULL == reginfo->handler) || (NULL == tabreg)) {
        snmp_log(LOG_ERR, "bad param in netsnmp_table_row_register\n");
        return SNMPERR_GENERR;
    }

        /*
         *   The first table_row invoked for a particular table should
         * register the full table as well, with a default handler to
         * process requests for non-existent (or incomplete) rows.
         *
         *   Subsequent table_row registrations attempting to set up
         * this default handler would fail - preferably silently!
         */
    snprintf(tmp, sizeof(tmp), "%s_table", reginfo->handlerName);
    reg2 = netsnmp_create_handler_registration(
              tmp,     _table_row_default_handler,
              reginfo->rootoid, reginfo->rootoid_len,
              reginfo->modes);
    netsnmp_register_table(reg2, tabreg);  /* Ignore return value */

        /*
         * Adjust the OID being registered, to take account
         * of the indexes and column range provided....
         */
    row_oid_len = reginfo->rootoid_len;
    memcpy( row_oid, (u_char *) reginfo->rootoid, row_oid_len * sizeof(oid));
    row_oid[row_oid_len++] = 1;   /* tableEntry */
    row_oid[row_oid_len++] = tabreg->min_column;
    reginfo->range_ubound  = tabreg->max_column;
    reginfo->range_subid   = row_oid_len-1;
    build_oid_noalloc(&row_oid[row_oid_len],
                      MAX_OID_LEN-row_oid_len, &len, NULL, 0, index);
    row_oid_len += len;
    free(reginfo->rootoid);
    reginfo->rootoid = snmp_duplicate_objid(row_oid, row_oid_len);
    reginfo->rootoid_len = row_oid_len;

     
        /*
         * ... insert a minimal handler ...
         */
    handler = netsnmp_table_row_handler_get(row);
    netsnmp_inject_handler(reginfo, handler );

        /*
         * ... and register the row
         */
    return netsnmp_register_handler(reginfo);
}


/** return the row data structure supplied to the table_row helper */
#ifndef NETSNMP_FEATURE_REMOVE_TABLE_ROW_EXTRACT
void *
netsnmp_table_row_extract(netsnmp_request_info *request)
{
    return netsnmp_request_get_list_data(request, TABLE_ROW_DATA);
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_ROW_EXTRACT */
/** @cond */

/**********************************************************************
 **********************************************************************
 *                                                                    *
 *                                                                    *
 * netsnmp_table_row_helper_handler()                           *
 *                                                                    *
 *                                                                    *
 **********************************************************************
 **********************************************************************/

static int
_table_row_handler(netsnmp_mib_handler          *handler,
                   netsnmp_handler_registration *reginfo,
                   netsnmp_agent_request_info   *reqinfo,
                   netsnmp_request_info         *requests)
{
    int             rc = SNMP_ERR_NOERROR;
    netsnmp_request_info *req;
    void                 *row;

    /** sanity checks */
    netsnmp_assert((NULL != handler) && (NULL != handler->myvoid));
    netsnmp_assert((NULL != reginfo) && (NULL != reqinfo));

    DEBUGMSGTL(("table_row", "Mode %s, Got request:\n",
                se_find_label_in_slist("agent_mode",reqinfo->mode)));

    /*
     * First off, get our pointer from the handler.
     * This contains the row that was actually registered.
     * Make this available for each of the requests passed in.
     */
    row = handler->myvoid;
    for (req = requests; req; req=req->next)
        netsnmp_request_add_list_data(req,
                netsnmp_create_data_list(TABLE_ROW_DATA, row, NULL));

    /*
     * Then call the next handler, to actually process the request
     */
    rc = netsnmp_call_next_handler(handler, reginfo, reqinfo, requests);
    if (rc != SNMP_ERR_NOERROR) {
        DEBUGMSGTL(("table_row", "next handler returned %d\n", rc));
    }

    return rc;
}

static int
_table_row_default_handler(netsnmp_mib_handler  *handler,
                   netsnmp_handler_registration *reginfo,
                   netsnmp_agent_request_info   *reqinfo,
                   netsnmp_request_info         *requests)
{
    netsnmp_request_info       *req;
    netsnmp_table_request_info *table_info;
    netsnmp_table_registration_info *tabreg;

    tabreg = netsnmp_find_table_registration_info(reginfo);
    for ( req=requests; req; req=req->next ) {
        table_info = netsnmp_extract_table_info( req );
        if (( table_info->colnum >= tabreg->min_column ) ||
            ( table_info->colnum <= tabreg->max_column )) {
            netsnmp_set_request_error( reqinfo, req, SNMP_NOSUCHINSTANCE );
        } else {
            netsnmp_set_request_error( reqinfo, req, SNMP_NOSUCHOBJECT );
        }
    }
    return SNMP_ERR_NOERROR;
}
/** @endcond */


/* ==================================
 *
 * Table Row API: Row operations
 *
 * This helper doesn't operate with the complete
 *   table, so these routines are not relevant.
 *
 * ================================== */


/* ==================================
 *
 * Table Row API: Index operations
 *
 * This helper doesn't operate with the complete
 *   table, so these routines are not relevant.
 *
 * ================================== */

/** @} */
