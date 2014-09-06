/*
 * DisMan Event MIB:
 *     Implementation of the mteTriggerTable MIB interface
 * See 'mteTrigger.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "utilities/iquery.h"
#include "disman/event/mteTrigger.h"
#include "disman/event/mteTriggerTable.h"

netsnmp_feature_require(iquery)
netsnmp_feature_require(table_tdata)
#ifndef NETSNMP_NO_WRITE_SUPPORT
netsnmp_feature_require(iquery_pdu_session)
netsnmp_feature_require(check_vb_type_and_max_size)
netsnmp_feature_require(check_vb_oid)
netsnmp_feature_require(check_vb_uint)
netsnmp_feature_require(mtetrigger_removeentry)
netsnmp_feature_require(check_vb_truthvalue)
netsnmp_feature_require(table_tdata_insert_row)
#endif /* NETSNMP_NO_WRITE_SUPPORT */

static netsnmp_table_registration_info *table_info;

/** Initializes the mteTriggerTable module */
void
init_mteTriggerTable(void)
{
    static oid  mteTriggerTable_oid[]   = { 1, 3, 6, 1, 2, 1, 88, 1, 2, 2 };
    size_t      mteTriggerTable_oid_len = OID_LENGTH(mteTriggerTable_oid);
    netsnmp_handler_registration    *reg;

    /*
     * Ensure the (combined) table container is available...
     */
    init_trigger_table_data();

    /*
     * ... then set up the MIB interface to the mteTriggerTable slice
     */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    reg = netsnmp_create_handler_registration("mteTriggerTable",
                                            mteTriggerTable_handler,
                                            mteTriggerTable_oid,
                                            mteTriggerTable_oid_len,
                                            HANDLER_CAN_RWRITE);
#else /* !NETSNMP_NO_WRITE_SUPPORT */
    reg = netsnmp_create_handler_registration("mteTriggerTable",
                                            mteTriggerTable_handler,
                                            mteTriggerTable_oid,
                                            mteTriggerTable_oid_len,
                                            HANDLER_CAN_RONLY);
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    netsnmp_table_helper_add_indexes(table_info,
                                     ASN_OCTET_STR, /* index: mteOwner       */
                                                    /* index: mteTriggerName */
                                     ASN_PRIV_IMPLIED_OCTET_STR,
                                     0);

    table_info->min_column = COLUMN_MTETRIGGERCOMMENT;
    table_info->max_column = COLUMN_MTETRIGGERENTRYSTATUS;

    /* Register this using the (common) trigger_table_data container */
    netsnmp_tdata_register(reg, trigger_table_data, table_info);
    DEBUGMSGTL(("disman:event:init", "Trigger Table\n"));
}

void
shutdown_mteTriggerTable(void)
{
    if (table_info) {
	netsnmp_table_registration_info_free(table_info);
	table_info = NULL;
    }
}


/** handles requests for the mteTriggerTable table */
int
mteTriggerTable_handler(netsnmp_mib_handler *handler,
                        netsnmp_handler_registration *reginfo,
                        netsnmp_agent_request_info *reqinfo,
                        netsnmp_request_info *requests)
{

    netsnmp_request_info       *request;
    netsnmp_table_request_info *tinfo;
    netsnmp_tdata_row          *row;
    struct mteTrigger          *entry;
    char mteOwner[MTE_STR1_LEN+1];
    char mteTName[MTE_STR1_LEN+1];
    long ret;

    DEBUGMSGTL(("disman:event:mib", "Trigger Table handler (%d)\n",
                                     reqinfo->mode));

    switch (reqinfo->mode) {
        /*
         * Read-support (also covers GetNext requests)
         */
    case MODE_GET:
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            entry = (struct mteTrigger *) netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);

            switch (tinfo->colnum) {
            case COLUMN_MTETRIGGERCOMMENT:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                         entry->mteTriggerComment,
                                  strlen(entry->mteTriggerComment));
                break;
            case COLUMN_MTETRIGGERTEST:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                        &entry->mteTriggerTest, 1);
                break;
            case COLUMN_MTETRIGGERSAMPLETYPE:
                ret = (entry->flags & MTE_TRIGGER_FLAG_DELTA ) ?
                           MTE_SAMPLE_DELTA : MTE_SAMPLE_ABSOLUTE;
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, ret);
                break;
            case COLUMN_MTETRIGGERVALUEID:
                snmp_set_var_typed_value(request->requestvb, ASN_OBJECT_ID,
                              (u_char *) entry->mteTriggerValueID,
                                         entry->mteTriggerValueID_len*sizeof(oid));
                break;
            case COLUMN_MTETRIGGERVALUEIDWILDCARD:
                ret = (entry->flags & MTE_TRIGGER_FLAG_VWILD ) ?
                           TV_TRUE : TV_FALSE;
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, ret);
                break;
            case COLUMN_MTETRIGGERTARGETTAG:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                         entry->mteTriggerTarget,
                                  strlen(entry->mteTriggerTarget));
                break;
            case COLUMN_MTETRIGGERCONTEXTNAME:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                         entry->mteTriggerContext,
                                  strlen(entry->mteTriggerContext));
                break;
            case COLUMN_MTETRIGGERCONTEXTNAMEWILDCARD:
                ret = (entry->flags & MTE_TRIGGER_FLAG_CWILD ) ?
                           TV_TRUE : TV_FALSE;
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, ret);
                break;
            case COLUMN_MTETRIGGERFREQUENCY:
                snmp_set_var_typed_integer(request->requestvb, ASN_UNSIGNED,
                                           entry->mteTriggerFrequency);
                break;
            case COLUMN_MTETRIGGEROBJECTSOWNER:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                         entry->mteTriggerOOwner,
                                  strlen(entry->mteTriggerOOwner));
                break;
            case COLUMN_MTETRIGGEROBJECTS:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                         entry->mteTriggerObjects,
                                  strlen(entry->mteTriggerObjects));
                break;
            case COLUMN_MTETRIGGERENABLED:
                ret = (entry->flags & MTE_TRIGGER_FLAG_ENABLED ) ?
                           TV_TRUE : TV_FALSE;
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, ret);
                break;
            case COLUMN_MTETRIGGERENTRYSTATUS:
                ret = (entry->flags & MTE_TRIGGER_FLAG_ACTIVE ) ?
                           RS_ACTIVE : RS_NOTINSERVICE;
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, ret);
                break;
            }
        }
        break;

#ifndef NETSNMP_NO_WRITE_SUPPORT
        /*
         * Write-support
         */
    case MODE_SET_RESERVE1:
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            entry = (struct mteTrigger *) netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);

            switch (tinfo->colnum) {
            case COLUMN_MTETRIGGERCOMMENT:
            case COLUMN_MTETRIGGERTARGETTAG:
            case COLUMN_MTETRIGGERCONTEXTNAME:
                ret = netsnmp_check_vb_type_and_max_size(
                          request->requestvb, ASN_OCTET_STR, MTE_STR2_LEN);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERTEST:
                ret = netsnmp_check_vb_type_and_size(
                          request->requestvb, ASN_OCTET_STR, 1);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERSAMPLETYPE:
                ret = netsnmp_check_vb_int_range(request->requestvb,
                                    MTE_SAMPLE_ABSOLUTE, MTE_SAMPLE_DELTA);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERVALUEID:
                ret = netsnmp_check_vb_oid(request->requestvb);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERVALUEIDWILDCARD:
            case COLUMN_MTETRIGGERCONTEXTNAMEWILDCARD:
            case COLUMN_MTETRIGGERENABLED:
                ret = netsnmp_check_vb_truthvalue(request->requestvb);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;

            case COLUMN_MTETRIGGERFREQUENCY:
                ret = netsnmp_check_vb_uint(request->requestvb);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGEROBJECTSOWNER:
            case COLUMN_MTETRIGGEROBJECTS:
                ret = netsnmp_check_vb_type_and_max_size(
                          request->requestvb, ASN_OCTET_STR, MTE_STR1_LEN);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERENTRYSTATUS:
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


            /*
             * Once a row has been made active, it cannot be
             *   modified except to delete it.  There's no good
             *   reason for this, but that's what the MIB says.
             *
             * The published version of the Event MIB even forbids
             *   enabling (or disabling) an active row, which
             *   would make this object completely pointless!
             * Fortunately this ludicrous decision has since been corrected.
             */
            if (entry &&
                entry->flags & MTE_TRIGGER_FLAG_ACTIVE ) {
                /* check for the acceptable assignments */
                if ((tinfo->colnum == COLUMN_MTETRIGGERENABLED) ||
                    (tinfo->colnum == COLUMN_MTETRIGGERENTRYSTATUS &&
                         *request->requestvb->val.integer != RS_NOTINSERVICE))
                    continue;

                /* Otherwise, reject this request */
                netsnmp_set_request_error(reqinfo, request,
                                          SNMP_ERR_INCONSISTENTVALUE);
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
            case COLUMN_MTETRIGGERENTRYSTATUS:
                switch (*request->requestvb->val.integer) {
                case RS_CREATEANDGO:
                case RS_CREATEANDWAIT:
                    /*
                     * Create an (empty) new row structure
                     */
                    memset(mteOwner, 0, sizeof(mteOwner));
                    memcpy(mteOwner, tinfo->indexes->val.string,
                                     tinfo->indexes->val_len);
                    memset(mteTName, 0, sizeof(mteTName));
                    memcpy(mteTName,
                           tinfo->indexes->next_variable->val.string,
                           tinfo->indexes->next_variable->val_len);

                    row = mteTrigger_createEntry(mteOwner, mteTName, 0);
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
            case COLUMN_MTETRIGGERENTRYSTATUS:
                switch (*request->requestvb->val.integer) {
                case RS_CREATEANDGO:
                case RS_CREATEANDWAIT:
                    /*
                     * Tidy up after a failed row creation request
                     */ 
                    entry = (struct mteTrigger *)
                                netsnmp_tdata_extract_entry(request);
                    if (entry &&
                      !(entry->flags & MTE_TRIGGER_FLAG_VALID)) {
                        row = (netsnmp_tdata_row *)
                                netsnmp_tdata_extract_row(request);
                        mteTrigger_removeEntry( row );
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
            entry = (struct mteTrigger *) netsnmp_tdata_extract_entry(request);
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

            entry = (struct mteTrigger *) netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);

            switch (tinfo->colnum) {
            case COLUMN_MTETRIGGERCOMMENT:
                memset(entry->mteTriggerComment, 0,
                       sizeof(entry->mteTriggerComment));
                memcpy(entry->mteTriggerComment,
                       request->requestvb->val.string,
                       request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGERTEST:
                entry->mteTriggerTest = request->requestvb->val.string[0];
                break;
            case COLUMN_MTETRIGGERSAMPLETYPE:
                if (*request->requestvb->val.integer == MTE_SAMPLE_DELTA)
                    entry->flags |=  MTE_TRIGGER_FLAG_DELTA;
                else
                    entry->flags &= ~MTE_TRIGGER_FLAG_DELTA;
                break;
            case COLUMN_MTETRIGGERVALUEID:
                memset(entry->mteTriggerValueID, 0,
                       sizeof(entry->mteTriggerValueID));
                memcpy(entry->mteTriggerValueID,
                       request->requestvb->val.string,
                       request->requestvb->val_len);
                entry->mteTriggerValueID_len = request->requestvb->val_len/sizeof(oid);
                break;
            case COLUMN_MTETRIGGERVALUEIDWILDCARD:
                if (*request->requestvb->val.integer == TV_TRUE)
                    entry->flags |=  MTE_TRIGGER_FLAG_VWILD;
                else
                    entry->flags &= ~MTE_TRIGGER_FLAG_VWILD;
                break;
            case COLUMN_MTETRIGGERTARGETTAG:
                memset(entry->mteTriggerTarget, 0,
                       sizeof(entry->mteTriggerTarget));
                memcpy(entry->mteTriggerTarget,
                       request->requestvb->val.string,
                       request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGERCONTEXTNAME:
                memset(entry->mteTriggerContext, 0,
                       sizeof(entry->mteTriggerContext));
                memcpy(entry->mteTriggerContext,
                       request->requestvb->val.string,
                       request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGERCONTEXTNAMEWILDCARD:
                if (*request->requestvb->val.integer == TV_TRUE)
                    entry->flags |=  MTE_TRIGGER_FLAG_CWILD;
                else
                    entry->flags &= ~MTE_TRIGGER_FLAG_CWILD;
                break;
            case COLUMN_MTETRIGGERFREQUENCY:
                entry->mteTriggerFrequency = *request->requestvb->val.integer;
                break;
            case COLUMN_MTETRIGGEROBJECTSOWNER:
                memset(entry->mteTriggerOOwner, 0,
                       sizeof(entry->mteTriggerOOwner));
                memcpy(entry->mteTriggerOOwner,
                       request->requestvb->val.string,
                       request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGEROBJECTS:
                memset(entry->mteTriggerObjects, 0,
                       sizeof(entry->mteTriggerObjects));
                memcpy(entry->mteTriggerObjects,
                       request->requestvb->val.string,
                       request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGERENABLED:
                if (*request->requestvb->val.integer == TV_TRUE)
                    entry->flags |=  MTE_TRIGGER_FLAG_ENABLED;
                else
                    entry->flags &= ~MTE_TRIGGER_FLAG_ENABLED;
                break;
            case COLUMN_MTETRIGGERENTRYSTATUS:
                switch (*request->requestvb->val.integer) {
                case RS_ACTIVE:
                    entry->flags |= MTE_TRIGGER_FLAG_ACTIVE;
                    mteTrigger_enable( entry );
                    break;
                case RS_CREATEANDGO:
                    entry->flags |= MTE_TRIGGER_FLAG_ACTIVE;
                    entry->flags |= MTE_TRIGGER_FLAG_VALID;
                    entry->session =
                        netsnmp_iquery_pdu_session(reqinfo->asp->pdu);
                    mteTrigger_enable( entry );
                    break;
                case RS_CREATEANDWAIT:
                    entry->flags |= MTE_TRIGGER_FLAG_VALID;
                    entry->session =
                        netsnmp_iquery_pdu_session(reqinfo->asp->pdu);
                    break;

                case RS_DESTROY:
                    row = (netsnmp_tdata_row *)
                               netsnmp_tdata_extract_row(request);
                    mteTrigger_removeEntry(row);
                }
                break;
            }
        }

        /** set up to save persistent store */
        snmp_store_needed(NULL);

        break;

#endif /* !NETSNMP_NO_WRITE_SUPPORT */

    }
    return SNMP_ERR_NOERROR;
}
