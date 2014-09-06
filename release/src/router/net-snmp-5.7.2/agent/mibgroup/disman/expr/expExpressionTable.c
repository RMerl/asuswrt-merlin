/*
 * DisMan Expression MIB:
 *    Implementation of the expExpressionTable MIB interface
 * See 'expExpression.c' for active behaviour of this table.
 *
 *  (Based on mib2c.table_data.conf output)
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "utilities/iquery.h"
#include "disman/expr/expExpression.h"
#include "disman/expr/expExpressionTable.h"

netsnmp_feature_require(iquery)
netsnmp_feature_require(table_tdata)
#ifndef NETSNMP_NO_WRITE_SUPPORT
netsnmp_feature_require(check_vb_type_and_max_size)
netsnmp_feature_require(table_tdata_insert_row)
netsnmp_feature_require(iquery_pdu_session)
#endif /* NETSNMP_NO_WRITE_SUPPORT */

/* Initializes the expExpressionTable module */
void
init_expExpressionTable(void)
{
    static oid  expExpressionTable_oid[]   = { 1, 3, 6, 1, 2, 1, 90, 1, 2, 1 };
    size_t      expExpressionTable_oid_len = OID_LENGTH(expExpressionTable_oid);
    netsnmp_handler_registration    *reg;
    netsnmp_table_registration_info *table_info;

    /*
     * Ensure the expression table container is available...
     */
    init_expr_table_data();

    /*
     * ... then set up the MIB interface to the expExpressionTable slice
     */
    reg = netsnmp_create_handler_registration("expExpressionTable",
                                            expExpressionTable_handler,
                                            expExpressionTable_oid,
                                            expExpressionTable_oid_len,
                                            HANDLER_CAN_RWRITE);

    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    netsnmp_table_helper_add_indexes(table_info,
                                          /* index: expExpressionOwner */
                                     ASN_OCTET_STR,
                                          /* index: expExpressionName */
                                     ASN_OCTET_STR,
                                     0);

    table_info->min_column = COLUMN_EXPEXPRESSION;
    table_info->max_column = COLUMN_EXPEXPRESSIONENTRYSTATUS;

    /* Register this using the (common) expr_table_data container */
    netsnmp_tdata_register(reg, expr_table_data, table_info);
    DEBUGMSGTL(("disman:expr:init", "Expression Table container (%p)\n",
                                     expr_table_data));
}


/** handles requests for the expExpressionTable table */
int
expExpressionTable_handler(netsnmp_mib_handler *handler,
                           netsnmp_handler_registration *reginfo,
                           netsnmp_agent_request_info *reqinfo,
                           netsnmp_request_info *requests)
{

    netsnmp_request_info       *request;
    netsnmp_table_request_info *tinfo;
    netsnmp_tdata_row          *row;
    struct expExpression       *entry;
    char   expOwner[EXP_STR1_LEN+1];
    char   expName[ EXP_STR1_LEN+1];
    long   ret;

    DEBUGMSGTL(("disman:expr:mib", "Expression Table handler (%d)\n",
                                    reqinfo->mode));

    switch (reqinfo->mode) {
        /*
         * Read-support (also covers GetNext requests)
         */
    case MODE_GET:
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            entry = (struct expExpression *)
                    netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);
            if (!entry || !(entry->flags & EXP_FLAG_VALID))
                continue;

            switch (tinfo->colnum) {
            case COLUMN_EXPEXPRESSION:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                (u_char*)entry->expExpression,
                                  strlen(entry->expExpression));
                break;
            case COLUMN_EXPEXPRESSIONVALUETYPE:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->expValueType);
                break;
            case COLUMN_EXPEXPRESSIONCOMMENT:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                (u_char*)entry->expComment,
                                  strlen(entry->expComment));
                break;
            case COLUMN_EXPEXPRESSIONDELTAINTERVAL:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->expDeltaInterval);
                break;
            case COLUMN_EXPEXPRESSIONPREFIX:
                /*
                 * XXX - Need to search expObjectTable for a suitable OID
                 */
                /*
                 * Empty OIDs (len=0) are converted into .0.0
                 *   by the SNMP library automatically :-(
                 */
                if ( entry->expPrefix_len ) {
                    snmp_set_var_typed_value(request->requestvb, ASN_OBJECT_ID,
                              (u_char *) entry->expPrefix,
                                         entry->expPrefix_len*sizeof(oid));
                } else {
                    /* XXX - possibly not needed */
                    request->requestvb->type    = ASN_OBJECT_ID;
                    request->requestvb->val_len = 0;
                }
                break;
            case COLUMN_EXPEXPRESSIONERRORS:
                snmp_set_var_typed_integer(request->requestvb, ASN_COUNTER,
                                           entry->expErrorCount);
                break;
            case COLUMN_EXPEXPRESSIONENTRYSTATUS:
                /* What would indicate 'notReady' ? */
                ret = (entry->flags & EXP_FLAG_ACTIVE) ?
                          RS_ACTIVE : RS_NOTINSERVICE;
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, ret);
                break;
            }
        }
        break;

        /*
         * Write-support
         */
    case MODE_SET_RESERVE1:
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            entry = (struct expExpression *)
                netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);

            switch (tinfo->colnum) {
            case COLUMN_EXPEXPRESSION:
                ret = netsnmp_check_vb_type_and_max_size(
                          request->requestvb, ASN_OCTET_STR, EXP_STR3_LEN);
                /* XXX - check new expression is syntactically valid */
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_EXPEXPRESSIONVALUETYPE:
                ret = netsnmp_check_vb_int_range(request->requestvb, 1, 8);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_EXPEXPRESSIONCOMMENT:
                ret = netsnmp_check_vb_type_and_max_size(
                          request->requestvb, ASN_OCTET_STR, EXP_STR2_LEN);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_EXPEXPRESSIONDELTAINTERVAL:
                ret = netsnmp_check_vb_int_range(request->requestvb, 0, 86400);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_EXPEXPRESSIONENTRYSTATUS:
                ret = netsnmp_check_vb_rowstatus(request->requestvb,
                                        (entry ? RS_ACTIVE : RS_NONEXISTENT));
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            default:
                netsnmp_set_request_error(reqinfo, request,
                                          SNMP_ERR_NOTWRITABLE);
                return SNMP_ERR_NOERROR;
            }
        }
        break;

    case MODE_SET_RESERVE2:
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            tinfo = netsnmp_extract_table_info(request);

            switch (tinfo->colnum) {
            case COLUMN_EXPEXPRESSIONENTRYSTATUS:
                switch (*request->requestvb->val.integer) {
                case RS_CREATEANDGO:
                case RS_CREATEANDWAIT:
                    /*
                     * Create an (empty) new row structure
                     */
                    memset(expOwner, 0, sizeof(expOwner));
                    memcpy(expOwner, tinfo->indexes->val.string,
                                     tinfo->indexes->val_len);
                    memset(expName,  0, sizeof(expName));
                    memcpy(expName,
                           tinfo->indexes->next_variable->val.string,
                           tinfo->indexes->next_variable->val_len);

                    row = expExpression_createRow(expOwner, expName, 0);
                    if (!row) {
                        netsnmp_set_request_error(reqinfo, request,
                                                  SNMP_ERR_RESOURCEUNAVAILABLE);
                        return SNMP_ERR_NOERROR;
                    }
                    netsnmp_insert_tdata_row( request, row );
                }
            }
        }
        break;

    case MODE_SET_FREE:
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            tinfo = netsnmp_extract_table_info(request);

            switch (tinfo->colnum) {
            case COLUMN_EXPEXPRESSIONENTRYSTATUS:
                switch (*request->requestvb->val.integer) {
                case RS_CREATEANDGO:
                case RS_CREATEANDWAIT:
                    /*
                     * Tidy up after a failed row creation request
                     */ 
                    entry = (struct expExpression *)
                                netsnmp_tdata_extract_entry(request);
                    if (entry &&
                      !(entry->flags & EXP_FLAG_VALID)) {
                        row = (netsnmp_tdata_row *)
                                netsnmp_tdata_extract_row(request);
                        expExpression_removeEntry( row );
                    }
                }
            }
        }
        break;

    case MODE_SET_ACTION:
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            tinfo = netsnmp_extract_table_info(request);
            entry = (struct expExpression *)
                    netsnmp_tdata_extract_entry(request);
            if (!entry) {
                /*
                 * New rows must be created via the RowStatus column
                 */
                netsnmp_set_request_error(reqinfo, request,
                                          SNMP_ERR_NOCREATION);
                                      /* or inconsistentName? */
                return SNMP_ERR_NOERROR;

            }
        }
        break;

    case MODE_SET_UNDO:
        break;

    case MODE_SET_COMMIT:
        /*
         * All these assignments are "unfailable", so it's
         *  (reasonably) safe to apply them in the Commit phase
         */
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            entry = (struct expExpression *)
                netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);

            switch (tinfo->colnum) {
            case COLUMN_EXPEXPRESSION:
                memset(entry->expExpression, 0, EXP_STR3_LEN+1);
                memcpy(entry->expExpression,
                       request->requestvb->val.string,
                       request->requestvb->val_len);
                break;
            case COLUMN_EXPEXPRESSIONVALUETYPE:
                entry->expValueType = *request->requestvb->val.integer;
                break;
            case COLUMN_EXPEXPRESSIONCOMMENT:
                memset(entry->expComment, 0, EXP_STR2_LEN+1);
                memcpy(entry->expComment,
                       request->requestvb->val.string,
                       request->requestvb->val_len);
                break;
            case COLUMN_EXPEXPRESSIONDELTAINTERVAL:
                entry->expDeltaInterval = *request->requestvb->val.integer;
                break;
            case COLUMN_EXPEXPRESSIONENTRYSTATUS:
                switch (*request->requestvb->val.integer) {
                case RS_ACTIVE:
                    entry->flags |= EXP_FLAG_ACTIVE;
                    expExpression_enable( entry );
                    break;
                case RS_NOTINSERVICE:
                    entry->flags &= ~EXP_FLAG_ACTIVE;
                    expExpression_disable( entry );
                    break;
                case RS_CREATEANDGO:
                    entry->flags |= EXP_FLAG_ACTIVE;
                    entry->flags |= EXP_FLAG_VALID;
                    entry->session =
                        netsnmp_iquery_pdu_session(reqinfo->asp->pdu);
                    expExpression_enable( entry );
                    break;
                case RS_CREATEANDWAIT:
                    entry->flags |= EXP_FLAG_VALID;
                    entry->session =
                        netsnmp_iquery_pdu_session(reqinfo->asp->pdu);
                    break;

                case RS_DESTROY:
                    row = (netsnmp_tdata_row *)
                               netsnmp_tdata_extract_row(request);
                    expExpression_removeEntry(row);
                }
            }
        }
        break;
    }
    DEBUGMSGTL(("disman:expr:mib", "Expression Table handler - done \n"));
    return SNMP_ERR_NOERROR;
}
