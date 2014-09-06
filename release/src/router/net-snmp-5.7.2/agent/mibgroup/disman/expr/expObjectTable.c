/*
 * DisMan Expression MIB:
 *    Implementation of the expExpressionObjectTable MIB interface
 * See 'expObject.c' for active behaviour of this table.
 *
 *  (Based on mib2c.table_data.conf output)
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "disman/expr/expObject.h"
#include "disman/expr/expObjectTable.h"

netsnmp_feature_require(table_tdata)
#ifndef NETSNMP_NO_WRITE_SUPPORT
netsnmp_feature_require(check_vb_oid)
netsnmp_feature_require(check_vb_truthvalue)
netsnmp_feature_require(table_tdata_insert_row)
#endif /* NETSNMP_NO_WRITE_SUPPORT */

/* Initializes the expObjectTable module */
void
init_expObjectTable(void)
{
    static oid   expObjectTable_oid[]   = { 1, 3, 6, 1, 2, 1, 90, 1, 2, 3 };
    size_t       expObjectTable_oid_len = OID_LENGTH(expObjectTable_oid);
    netsnmp_handler_registration    *reg;
    netsnmp_table_registration_info *table_info;

    /*
     * Ensure the expObject table container is available...
     */
    init_expObject_table_data();

    /*
     * ... then set up the MIB interface to the expObjectTable
     */
    reg = netsnmp_create_handler_registration("expObjectTable",
                                            expObjectTable_handler,
                                            expObjectTable_oid,
                                            expObjectTable_oid_len,
                                            HANDLER_CAN_RWRITE);

    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    netsnmp_table_helper_add_indexes(table_info,
                                          /* index: expExpressionOwner */
                                     ASN_OCTET_STR,
                                          /* index: expExpressionName */
                                     ASN_OCTET_STR,
                                          /* index: expObjectIndex */
                                     ASN_UNSIGNED,
                                     0);

    table_info->min_column = COLUMN_EXPOBJECTID;
    table_info->max_column = COLUMN_EXPOBJECTENTRYSTATUS;

    /* Register this using the common expObject_table_data container */
    netsnmp_tdata_register(reg, expObject_table_data, table_info);
    DEBUGMSGTL(("disman:expr:init", "Expression Object Table container (%p)\n",
                                     expObject_table_data));
}


/** handles requests for the expObjectTable table */
int
expObjectTable_handler(netsnmp_mib_handler *handler,
                       netsnmp_handler_registration *reginfo,
                       netsnmp_agent_request_info *reqinfo,
                       netsnmp_request_info *requests)
{

    netsnmp_request_info       *request;
    netsnmp_table_request_info *tinfo;
    netsnmp_tdata_row          *row;
    struct expObject           *entry;
    struct expExpression       *exp;
    char   expOwner[EXP_STR1_LEN+1];
    char   expName[ EXP_STR1_LEN+1];
    long   objIndex;
    long   ret;
    netsnmp_variable_list *vp;

    DEBUGMSGTL(("disman:expr:mib", "Expression Object Table handler (%d)\n",
                                    reqinfo->mode));
    switch (reqinfo->mode) {
        /*
         * Read-support (also covers GetNext requests)
         */
    case MODE_GET:
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            entry = (struct expObject *)netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);
            if (!entry || !(entry->flags & EXP_OBJ_FLAG_VALID))
                continue;

            switch (tinfo->colnum) {
            case COLUMN_EXPOBJECTID:
                snmp_set_var_typed_value(request->requestvb, ASN_OBJECT_ID,
                              (u_char *) entry->expObjectID,
                                         entry->expObjectID_len*sizeof(oid));
                break;
            case COLUMN_EXPOBJECTIDWILDCARD:
                ret = (entry->flags & EXP_OBJ_FLAG_OWILD) ?
                           TV_TRUE : TV_FALSE;
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, ret);
                break;
            case COLUMN_EXPOBJECTSAMPLETYPE:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->expObjectSampleType);
                break;
            case COLUMN_EXPOBJECTDELTADISCONTINUITYID:
                /*
                 * "This object [and the next two] are instantiated only if
                 *  expObjectSampleType is 'deltaValue' or 'changedValue'"
                 */
                if ( entry->expObjectSampleType == 1 )
                    continue;
                snmp_set_var_typed_value(request->requestvb, ASN_OBJECT_ID,
                              (u_char *) entry->expObjDeltaD,
                                         entry->expObjDeltaD_len*sizeof(oid));
                break;
            case COLUMN_EXPOBJECTDISCONTINUITYIDWILDCARD:
                if ( entry->expObjectSampleType == 1 )
                    continue;
                ret = (entry->flags & EXP_OBJ_FLAG_DWILD) ?
                           TV_TRUE : TV_FALSE;
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, ret);
                break;
            case COLUMN_EXPOBJECTDISCONTINUITYIDTYPE:
                if ( entry->expObjectSampleType == 1 )
                    continue;
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->expObjDiscontinuityType);
                break;
            case COLUMN_EXPOBJECTCONDITIONAL:
                snmp_set_var_typed_value(request->requestvb, ASN_OBJECT_ID,
                              (u_char *) entry->expObjCond,
                                         entry->expObjCond_len*sizeof(oid));
                break;
            case COLUMN_EXPOBJECTCONDITIONALWILDCARD:
		ret = (entry->flags & EXP_OBJ_FLAG_CWILD) ?
                           TV_TRUE : TV_FALSE;
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, ret);
                break;
            case COLUMN_EXPOBJECTENTRYSTATUS:
                /* What would indicate 'notReady' ? */
                ret = (entry->flags & EXP_OBJ_FLAG_ACTIVE) ?
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

            entry = (struct expObject *)
                netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);

            switch (tinfo->colnum) {
            case COLUMN_EXPOBJECTID:
            case COLUMN_EXPOBJECTDELTADISCONTINUITYID:
            case COLUMN_EXPOBJECTCONDITIONAL:
                ret = netsnmp_check_vb_oid(request->requestvb);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;

            case COLUMN_EXPOBJECTIDWILDCARD:
            case COLUMN_EXPOBJECTDISCONTINUITYIDWILDCARD:
            case COLUMN_EXPOBJECTCONDITIONALWILDCARD:
                ret = netsnmp_check_vb_truthvalue(request->requestvb);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;

            case COLUMN_EXPOBJECTSAMPLETYPE:
            case COLUMN_EXPOBJECTDISCONTINUITYIDTYPE:
                ret = netsnmp_check_vb_int_range(request->requestvb, 1, 3);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;

            case COLUMN_EXPOBJECTENTRYSTATUS:
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
            case COLUMN_EXPOBJECTENTRYSTATUS:
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
                    vp = tinfo->indexes->next_variable->next_variable;
                    objIndex = *vp->val.integer;

                    row = expObject_createRow(expOwner, expName, objIndex, 0);
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
            case COLUMN_EXPOBJECTENTRYSTATUS:
                switch (*request->requestvb->val.integer) {
                case RS_CREATEANDGO:
                case RS_CREATEANDWAIT:
                    /*
                     * Tidy up after a failed row creation request
                     */ 
                    entry = (struct expObject *)
                                netsnmp_tdata_extract_entry(request);
                    if (entry &&
                      !(entry->flags & EXP_OBJ_FLAG_VALID)) {
                        row = (netsnmp_tdata_row *)
                                netsnmp_tdata_extract_row(request);
                        expObject_removeEntry( row );
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
            entry = (struct expObject *)
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
        ret = 0;  /* Flag to re-check expExpressionPrefix settings */
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            entry = (struct expObject *) netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);

            switch (tinfo->colnum) {
            case COLUMN_EXPOBJECTID:
                memset(entry->expObjectID, 0, sizeof(entry->expObjectID));
                memcpy(entry->expObjectID, request->requestvb->val.string,
                                           request->requestvb->val_len);
                entry->expObjectID_len = request->requestvb->val_len;
                break;
            case COLUMN_EXPOBJECTIDWILDCARD:
                if (*request->requestvb->val.integer == TV_TRUE)
                    entry->flags |=  EXP_OBJ_FLAG_OWILD;
                else
                    entry->flags &= ~EXP_OBJ_FLAG_OWILD;
                break;
            case COLUMN_EXPOBJECTSAMPLETYPE:
                entry->expObjectSampleType = *request->requestvb->val.integer;
                break;
            case COLUMN_EXPOBJECTDELTADISCONTINUITYID:
                memset(entry->expObjDeltaD, 0, sizeof(entry->expObjDeltaD));
                memcpy(entry->expObjDeltaD, request->requestvb->val.string,
                                            request->requestvb->val_len);
                entry->expObjDeltaD_len = request->requestvb->val_len/sizeof(oid);
               /* XXX
                if ( snmp_oid_compare( entry->expObjDeltaD,
                                       entry->expObjDeltaD_len, 
                                       sysUpTime_inst,
                                       sysUpTime_inst_len ) != 0 )
                    entry->flags |= EXP_OBJ_FLAG_DDISC;
               */

                /*
                 * If the OID used for the expExpressionPrefix object
                 *   has changed, then update the expression structure.
                 */
                if ( entry->flags & EXP_OBJ_FLAG_PREFIX ) {
                    exp = expExpression_getEntry( entry->expOwner,
                                                  entry->expName );
                    memcpy( exp->expPrefix, entry->expObjDeltaD,
                            MAX_OID_LEN*sizeof(oid));
                    exp->expPrefix_len = entry->expObjDeltaD_len;
                }
                break;
            case COLUMN_EXPOBJECTDISCONTINUITYIDWILDCARD:
                if (*request->requestvb->val.integer == TV_TRUE) {
                    /*
                     * Possible new prefix OID candidate
                     * Can't set the value here, since the OID
                     *    assignment might not have been processed yet.
                     */
                    exp = expExpression_getEntry( entry->expOwner,
                                                  entry->expName );
                    if (exp && exp->expPrefix_len == 0 )
                        ret = 1;   /* Set the prefix later  */
                    entry->flags |=  EXP_OBJ_FLAG_DWILD;
                } else {
                    if ( entry->flags | EXP_OBJ_FLAG_PREFIX ) {
                        exp = expExpression_getEntry( entry->expOwner,
                                                      entry->expName );
                        memset( exp->expPrefix, 0, MAX_OID_LEN*sizeof(oid));
                        exp->expPrefix_len = 0;
                        ret = 1;   /* Need a new prefix OID */
                    }
                    entry->flags &= ~EXP_OBJ_FLAG_DWILD;
                }
                break;
            case COLUMN_EXPOBJECTDISCONTINUITYIDTYPE:
                entry->expObjDiscontinuityType =
                           *request->requestvb->val.integer;
                break;
            case COLUMN_EXPOBJECTCONDITIONAL:
                memset(entry->expObjCond, 0, sizeof(entry->expObjCond));
                memcpy(entry->expObjCond, request->requestvb->val.string,
                                          request->requestvb->val_len);
                entry->expObjCond_len = request->requestvb->val_len/sizeof(oid);
                break;
            case COLUMN_EXPOBJECTCONDITIONALWILDCARD:
                if (*request->requestvb->val.integer == TV_TRUE)
                    entry->flags |=  EXP_OBJ_FLAG_CWILD;
                else
                    entry->flags &= ~EXP_OBJ_FLAG_CWILD;
                break;
            case COLUMN_EXPOBJECTENTRYSTATUS:
                switch (*request->requestvb->val.integer) {
                case RS_ACTIVE:
                    entry->flags |= EXP_OBJ_FLAG_ACTIVE;
                    break;
                case RS_NOTINSERVICE:
                    entry->flags &= ~EXP_OBJ_FLAG_ACTIVE;
                    break;
                case RS_CREATEANDGO:
                    entry->flags |= EXP_OBJ_FLAG_ACTIVE;
                    entry->flags |= EXP_OBJ_FLAG_VALID;
                    break;
                case RS_CREATEANDWAIT:
                    entry->flags |= EXP_OBJ_FLAG_VALID;
                    break;

                case RS_DESTROY:
                    row = (netsnmp_tdata_row *)
                               netsnmp_tdata_extract_row(request);
                    expObject_removeEntry(row);
                }
            }
        }

        /*
         * Need to check for changes in expExpressionPrefix handling
         */
        for (request = requests; request; request = request->next) {
            entry = (struct expObject *) netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);

            switch (tinfo->colnum) {
            case COLUMN_EXPOBJECTDISCONTINUITYIDWILDCARD:
                /*
                 * If a column has just been marked as wild,
                 *   then consider using it as the prefix OID
                 */
                if (*request->requestvb->val.integer == TV_TRUE) {
                    exp = expExpression_getEntry( entry->expOwner,
                                                  entry->expName );
                    if ( exp->expPrefix_len == 0 ) {
                        memcpy( exp->expPrefix, entry->expObjDeltaD,
                                MAX_OID_LEN*sizeof(oid));
                        exp->expPrefix_len = entry->expObjDeltaD_len;
                        entry->flags |= EXP_OBJ_FLAG_PREFIX;
                    }
                }
                /*
                 * If it's just been marked as non-wildcarded
                 *   then we need to look for a new candidate.
                 */
                else {

                }
                break;
            }
        }
        break;
    }
    DEBUGMSGTL(("disman:expr:mib", "Expression Object handler - done \n"));
    return SNMP_ERR_NOERROR;
}
