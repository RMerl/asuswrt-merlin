/*
 * DisMan Event MIB:
 *     Implementation of the mteEventTable MIB interface
 * See 'mteEvent.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "utilities/iquery.h"
#include "disman/event/mteEvent.h"
#include "disman/event/mteEventTable.h"

netsnmp_feature_require(iquery)
netsnmp_feature_require(table_tdata)
#ifndef NETSNMP_NO_WRITE_SUPPORT
netsnmp_feature_require(iquery_pdu_session)
netsnmp_feature_require(check_vb_type_and_max_size)
netsnmp_feature_require(mteevent_removeentry)
netsnmp_feature_require(check_vb_truthvalue)
netsnmp_feature_require(table_tdata_insert_row)
#endif /* NETSNMP_NO_WRITE_SUPPORT */

static netsnmp_table_registration_info *table_info;

/* Initializes the mteEventTable module */
void
init_mteEventTable(void)
{
    static oid  mteEventTable_oid[]   = { 1, 3, 6, 1, 2, 1, 88, 1, 4, 2 };
    size_t      mteEventTable_oid_len = OID_LENGTH(mteEventTable_oid);
    netsnmp_handler_registration    *reg;

    /*
     * Ensure the (combined) table container is available...
     */
    init_event_table_data();

    /*
     * ... then set up the MIB interface to the mteEventTable slice
     */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    reg = netsnmp_create_handler_registration("mteEventTable",
                                            mteEventTable_handler,
                                            mteEventTable_oid,
                                            mteEventTable_oid_len,
                                            HANDLER_CAN_RWRITE);
#else /* !NETSNMP_NO_WRITE_SUPPORT */
    reg = netsnmp_create_handler_registration("mteEventTable",
                                            mteEventTable_handler,
                                            mteEventTable_oid,
                                            mteEventTable_oid_len,
                                            HANDLER_CAN_RONLY);
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    netsnmp_table_helper_add_indexes(table_info,
                                     ASN_OCTET_STR,   /* index: mteOwner */
                                                      /* index: mteEventName */
                                     ASN_PRIV_IMPLIED_OCTET_STR,
                                     0);

    table_info->min_column = COLUMN_MTEEVENTCOMMENT;
    table_info->max_column = COLUMN_MTEEVENTENTRYSTATUS;

    /* Register this using the (common) event_table_data container */
    netsnmp_tdata_register(reg, event_table_data, table_info);
    DEBUGMSGTL(("disman:event:init", "Event Table container (%p)\n",
                                      event_table_data));
}

void
shutdown_mteEventTable(void)
{
    if (table_info) {
	netsnmp_table_registration_info_free(table_info);
	table_info = NULL;
    }
}

/** handles requests for the mteEventTable table */
int
mteEventTable_handler(netsnmp_mib_handler *handler,
                      netsnmp_handler_registration *reginfo,
                      netsnmp_agent_request_info *reqinfo,
                      netsnmp_request_info *requests)
{

    netsnmp_request_info       *request;
    netsnmp_table_request_info *tinfo;
    netsnmp_tdata_row          *row;
    struct mteEvent            *entry;
    char mteOwner[MTE_STR1_LEN+1];
    char mteEName[MTE_STR1_LEN+1];
    long ret;

    DEBUGMSGTL(("disman:event:mib", "Event Table handler (%d)\n",
                                     reqinfo->mode));

    switch (reqinfo->mode) {
        /*
         * Read-support (also covers GetNext requests)
         */
    case MODE_GET:
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            entry = (struct mteEvent *) netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);
            if (!entry || !(entry->flags & MTE_EVENT_FLAG_VALID))
                continue;

            switch (tinfo->colnum) {
            case COLUMN_MTEEVENTCOMMENT:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                         entry->mteEventComment,
                                  strlen(entry->mteEventComment));
                break;
            case COLUMN_MTEEVENTACTIONS:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                        &entry->mteEventActions, 1);
                break;
            case COLUMN_MTEEVENTENABLED:
                ret = (entry->flags & MTE_EVENT_FLAG_ENABLED ) ?
                           TV_TRUE : TV_FALSE;
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, ret);
                break;
            case COLUMN_MTEEVENTENTRYSTATUS:
                ret = (entry->flags & MTE_EVENT_FLAG_ACTIVE ) ?
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

            entry = (struct mteEvent *) netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);

            switch (tinfo->colnum) {
            case COLUMN_MTEEVENTCOMMENT:
                ret = netsnmp_check_vb_type_and_max_size(
                          request->requestvb, ASN_OCTET_STR, MTE_STR1_LEN);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                /*
                 * Can't modify the comment of an active row
                 *   (No good reason for this, but that's what the MIB says!)
                 */
                if (entry &&
                    entry->flags & MTE_EVENT_FLAG_ACTIVE ) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_INCONSISTENTVALUE);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTEEVENTACTIONS:
                ret = netsnmp_check_vb_type_and_size(
                          request->requestvb, ASN_OCTET_STR, 1);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                /*
                 * Can't modify the event types of an active row
                 *   (A little more understandable perhaps,
                 *    but still an unnecessary restriction IMO)
                 */
                if (entry &&
                    entry->flags & MTE_EVENT_FLAG_ACTIVE ) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_INCONSISTENTVALUE);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTEEVENTENABLED:
                ret = netsnmp_check_vb_truthvalue(request->requestvb);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                /*
                 * The published version of the Event MIB forbids
                 *   enabling (or disabling) an active row, which
                 *   would make this object completely pointless!
                 * Fortunately this ludicrous decision has since been corrected.
                 */
                break;

            case COLUMN_MTEEVENTENTRYSTATUS:
                ret = netsnmp_check_vb_rowstatus(request->requestvb,
                          (entry ? RS_ACTIVE : RS_NONEXISTENT));
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                /* An active row can only be deleted */
                if (entry &&
                    entry->flags & MTE_EVENT_FLAG_ACTIVE &&
                    *request->requestvb->val.integer == RS_NOTINSERVICE ) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_INCONSISTENTVALUE);
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
            case COLUMN_MTEEVENTENTRYSTATUS:
                switch (*request->requestvb->val.integer) {
                case RS_CREATEANDGO:
                case RS_CREATEANDWAIT:
                    /*
                     * Create an (empty) new row structure
                     */
                    memset(mteOwner, 0, sizeof(mteOwner));
                    memcpy(mteOwner, tinfo->indexes->val.string,
                                     tinfo->indexes->val_len);
                    memset(mteEName, 0, sizeof(mteEName));
                    memcpy(mteEName,
                           tinfo->indexes->next_variable->val.string,
                           tinfo->indexes->next_variable->val_len);

                    row = mteEvent_createEntry(mteOwner, mteEName, 0);
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
            case COLUMN_MTEEVENTENTRYSTATUS:
                switch (*request->requestvb->val.integer) {
                case RS_CREATEANDGO:
                case RS_CREATEANDWAIT:
                    /*
                     * Tidy up after a failed row creation request
                     */ 
                    entry = (struct mteEvent *)
                                netsnmp_tdata_extract_entry(request);
                    if (entry &&
                      !(entry->flags & MTE_EVENT_FLAG_VALID)) {
                        row = (netsnmp_tdata_row *)
                                netsnmp_tdata_extract_row(request);
                        mteEvent_removeEntry( row );
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
            entry = (struct mteEvent *) netsnmp_tdata_extract_entry(request);
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

            entry = (struct mteEvent *) netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);

            switch (tinfo->colnum) {
            case COLUMN_MTEEVENTCOMMENT:
                memset(entry->mteEventComment, 0,
                       sizeof(entry->mteEventComment));
                memcpy(entry->mteEventComment,
                       request->requestvb->val.string,
                       request->requestvb->val_len);
                break;

            case COLUMN_MTEEVENTACTIONS:
                entry->mteEventActions = request->requestvb->val.string[0];
                break;

            case COLUMN_MTEEVENTENABLED:
                if (*request->requestvb->val.integer == TV_TRUE)
                    entry->flags |=  MTE_EVENT_FLAG_ENABLED;
                else
                    entry->flags &= ~MTE_EVENT_FLAG_ENABLED;
                break;

            case COLUMN_MTEEVENTENTRYSTATUS:
                switch (*request->requestvb->val.integer) {
                case RS_ACTIVE:
                    entry->flags |= MTE_EVENT_FLAG_ACTIVE;
                    break;
                case RS_CREATEANDGO:
                    entry->flags |= MTE_EVENT_FLAG_ACTIVE;
                    /* fall-through */
                case RS_CREATEANDWAIT:
                    entry->flags |= MTE_EVENT_FLAG_VALID;
                    entry->session =
                        netsnmp_iquery_pdu_session(reqinfo->asp->pdu);
                    break;

                case RS_DESTROY:
                    row = (netsnmp_tdata_row *)
                               netsnmp_tdata_extract_row(request);
                    mteEvent_removeEntry(row);
                }
            }
        }
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

    }
    DEBUGMSGTL(("disman:event:mib", "Table handler, done\n"));
    return SNMP_ERR_NOERROR;
}
