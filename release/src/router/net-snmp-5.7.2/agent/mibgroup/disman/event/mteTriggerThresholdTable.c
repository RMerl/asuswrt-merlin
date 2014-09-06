/*
 * DisMan Event MIB:
 *     Implementation of the mteTriggerThresholdTable MIB interface
 * See 'mteTrigger.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "disman/event/mteTrigger.h"
#include "disman/event/mteTriggerThresholdTable.h"

netsnmp_feature_require(table_tdata)
#ifndef NETSNMP_NO_WRITE_SUPPORT
netsnmp_feature_require(check_vb_type_and_max_size)
#endif /* NETSNMP_NO_WRITE_SUPPORT */

static netsnmp_table_registration_info *table_info;

/** Initializes the mteTriggerThresholdTable module */
void
init_mteTriggerThresholdTable(void)
{
    static oid mteTThreshTable_oid[]   = { 1, 3, 6, 1, 2, 1, 88, 1, 2, 6 };
    size_t     mteTThreshTable_oid_len = OID_LENGTH(mteTThreshTable_oid);
    netsnmp_handler_registration    *reg;

    /*
     * Ensure the (combined) table container is available...
     */
    init_trigger_table_data();

    /*
     * ... then set up the MIB interface to the mteTriggerThresholdTable slice
     */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    reg = netsnmp_create_handler_registration("mteTriggerThresholdTable",
                                            mteTriggerThresholdTable_handler,
                                            mteTThreshTable_oid,
                                            mteTThreshTable_oid_len,
                                            HANDLER_CAN_RWRITE);
#else /* !NETSNMP_NO_WRITE_SUPPORT */
    reg = netsnmp_create_handler_registration("mteTriggerThresholdTable",
                                            mteTriggerThresholdTable_handler,
                                            mteTThreshTable_oid,
                                            mteTThreshTable_oid_len,
                                            HANDLER_CAN_RONLY);
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    netsnmp_table_helper_add_indexes(table_info,
                                     ASN_OCTET_STR, /* index: mteOwner       */
                                                    /* index: mteTriggerName */
                                     ASN_PRIV_IMPLIED_OCTET_STR,
                                     0);

    table_info->min_column = COLUMN_MTETRIGGERTHRESHOLDSTARTUP;
    table_info->max_column = COLUMN_MTETRIGGERTHRESHOLDDELTAFALLINGEVENT;

    /* Register this using the (common) trigger_table_data container */
    netsnmp_tdata_register(reg, trigger_table_data, table_info);
    DEBUGMSGTL(("disman:event:init", "Trigger Threshold Table\n"));
}

void
shutdown_mteTriggerThresholdTable(void)
{
    if (table_info) {
	netsnmp_table_registration_info_free(table_info);
	table_info = NULL;
    }
}


/** handles requests for the mteTriggerThresholdTable table */
int
mteTriggerThresholdTable_handler(netsnmp_mib_handler *handler,
                                 netsnmp_handler_registration *reginfo,
                                 netsnmp_agent_request_info *reqinfo,
                                 netsnmp_request_info *requests)
{

    netsnmp_request_info       *request;
    netsnmp_table_request_info *tinfo;
    struct mteTrigger          *entry;
    int ret;

    DEBUGMSGTL(("disman:event:mib", "Threshold Table handler (%d)\n",
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

            /*
             * The mteTriggerThresholdTable should only contains entries for
             *   rows where the mteTriggerTest 'threshold(2)' bit is set.
             * So skip entries where this isn't the case.
             */
            if (!entry || !(entry->mteTriggerTest & MTE_TRIGGER_THRESHOLD )) {
                netsnmp_request_set_error(request, SNMP_NOSUCHINSTANCE);
                continue;
            }

            switch (tinfo->colnum) {
            case COLUMN_MTETRIGGERTHRESHOLDSTARTUP:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->mteTThStartup);
                break;
            case COLUMN_MTETRIGGERTHRESHOLDRISING:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->mteTThRiseValue);
                break;
            case COLUMN_MTETRIGGERTHRESHOLDFALLING:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->mteTThFallValue);
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISING:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->mteTThDRiseValue);
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLING:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->mteTThDFallValue);
                break;
            case COLUMN_MTETRIGGERTHRESHOLDOBJECTSOWNER:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTThObjOwner,
                                  strlen(entry->mteTThObjOwner));
                break;
            case COLUMN_MTETRIGGERTHRESHOLDOBJECTS:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTThObjects,
                                  strlen(entry->mteTThObjects));
                break;
            case COLUMN_MTETRIGGERTHRESHOLDRISINGEVENTOWNER:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTThRiseOwner,
                                  strlen(entry->mteTThRiseOwner));
                break;
            case COLUMN_MTETRIGGERTHRESHOLDRISINGEVENT:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTThRiseEvent,
                                  strlen(entry->mteTThRiseEvent));
                break;
            case COLUMN_MTETRIGGERTHRESHOLDFALLINGEVENTOWNER:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTThFallOwner,
                                  strlen(entry->mteTThFallOwner));
                break;
            case COLUMN_MTETRIGGERTHRESHOLDFALLINGEVENT:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTThFallEvent,
                                  strlen(entry->mteTThFallEvent));
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISINGEVENTOWNER:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTThDRiseOwner,
                                  strlen(entry->mteTThDRiseOwner));
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISINGEVENT:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTThDRiseEvent,
                                  strlen(entry->mteTThDRiseEvent));
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLINGEVENTOWNER:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTThDFallOwner,
                                  strlen(entry->mteTThDFallOwner));
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLINGEVENT:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTThDFallEvent,
                                  strlen(entry->mteTThDFallEvent));
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

            /*
             * Since the mteTriggerThresholdTable only contains entries for
             *   rows where the mteTriggerTest 'threshold(2)' bit is set,
             *   strictly speaking we should reject assignments where
             *   this isn't the case.
             * But SET requests that include an assignment of the
             *   'threshold(2)' bit at the same time are valid, so would
             *    need to be accepted. Unfortunately, this assignment
             *   is only applied in the COMMIT pass, so it's difficult
             *   to detect whether this holds or not.
             *
             * Let's fudge things for now, by processing assignments
             *   even if the 'threshold(2)' bit isn't set.
             */
            switch (tinfo->colnum) {
            case COLUMN_MTETRIGGERTHRESHOLDSTARTUP:
                ret = netsnmp_check_vb_int_range(request->requestvb,
                                                 MTE_THRESH_START_RISE,
                                                 MTE_THRESH_START_RISEFALL );
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERTHRESHOLDRISING:
            case COLUMN_MTETRIGGERTHRESHOLDFALLING:
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISING:
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLING:
                ret = netsnmp_check_vb_int(request->requestvb);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERTHRESHOLDOBJECTSOWNER:
            case COLUMN_MTETRIGGERTHRESHOLDOBJECTS:
            case COLUMN_MTETRIGGERTHRESHOLDRISINGEVENTOWNER:
            case COLUMN_MTETRIGGERTHRESHOLDRISINGEVENT:
            case COLUMN_MTETRIGGERTHRESHOLDFALLINGEVENTOWNER:
            case COLUMN_MTETRIGGERTHRESHOLDFALLINGEVENT:
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISINGEVENTOWNER:
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISINGEVENT:
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLINGEVENTOWNER:
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLINGEVENT:
                ret = netsnmp_check_vb_type_and_max_size(
                          request->requestvb, ASN_OCTET_STR, MTE_STR1_LEN);
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
             * The Event MIB is somewhat ambiguous as to whether the
             *  various trigger table entries can be modified once the
             *  main mteTriggerTable entry has been marked 'active'. 
             * But it's clear from discussion on the DisMan mailing
             *  list is that the intention is not.
             *
             * So check for whether this row is already active,
             *  and reject *all* SET requests if it is.
             */
            entry = (struct mteTrigger *) netsnmp_tdata_extract_entry(request);
            if (entry &&
                entry->flags & MTE_TRIGGER_FLAG_ACTIVE ) {
                netsnmp_set_request_error(reqinfo, request,
                                          SNMP_ERR_INCONSISTENTVALUE);
                return SNMP_ERR_NOERROR;
            }
        }
        break;

    case MODE_SET_RESERVE2:
    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        break;

    case MODE_SET_ACTION:
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            entry = (struct mteTrigger *) netsnmp_tdata_extract_entry(request);
            if (!entry) {
                /*
                 * New rows must be created via the RowStatus column
                 *   (in the main mteTriggerTable)
                 */
                netsnmp_set_request_error(reqinfo, request,
                                          SNMP_ERR_NOCREATION);
                                      /* or inconsistentName? */
                return SNMP_ERR_NOERROR;
            }
        }
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
            case COLUMN_MTETRIGGERTHRESHOLDSTARTUP:
                entry->mteTThStartup    = *request->requestvb->val.integer;
                break;
            case COLUMN_MTETRIGGERTHRESHOLDRISING:
                entry->mteTThRiseValue  = *request->requestvb->val.integer;
                break;
            case COLUMN_MTETRIGGERTHRESHOLDFALLING:
                entry->mteTThFallValue  = *request->requestvb->val.integer;
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISING:
                entry->mteTThDRiseValue = *request->requestvb->val.integer;
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLING:
                entry->mteTThDFallValue = *request->requestvb->val.integer;
                break;
            case COLUMN_MTETRIGGERTHRESHOLDOBJECTSOWNER:
                memset(entry->mteTThObjOwner, 0, sizeof(entry->mteTThObjOwner));
                memcpy(entry->mteTThObjOwner, request->requestvb->val.string,
                                              request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGERTHRESHOLDOBJECTS:
                memset(entry->mteTThObjects,  0, sizeof(entry->mteTThObjects));
                memcpy(entry->mteTThObjects,  request->requestvb->val.string,
                                              request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGERTHRESHOLDRISINGEVENTOWNER:
                memset(entry->mteTThRiseOwner, 0, sizeof(entry->mteTThRiseOwner));
                memcpy(entry->mteTThRiseOwner, request->requestvb->val.string,
                                               request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGERTHRESHOLDRISINGEVENT:
                memset(entry->mteTThRiseEvent, 0, sizeof(entry->mteTThRiseEvent));
                memcpy(entry->mteTThRiseEvent, request->requestvb->val.string,
                                               request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGERTHRESHOLDFALLINGEVENTOWNER:
                memset(entry->mteTThFallOwner, 0, sizeof(entry->mteTThFallOwner));
                memcpy(entry->mteTThFallOwner, request->requestvb->val.string,
                                               request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGERTHRESHOLDFALLINGEVENT:
                memset(entry->mteTThFallEvent, 0, sizeof(entry->mteTThFallEvent));
                memcpy(entry->mteTThFallEvent, request->requestvb->val.string,
                                               request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISINGEVENTOWNER:
                memset(entry->mteTThDRiseOwner, 0, sizeof(entry->mteTThDRiseOwner));
                memcpy(entry->mteTThDRiseOwner, request->requestvb->val.string,
                                                request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISINGEVENT:
                memset(entry->mteTThDRiseEvent, 0, sizeof(entry->mteTThDRiseEvent));
                memcpy(entry->mteTThDRiseEvent, request->requestvb->val.string,
                                                request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLINGEVENTOWNER:
                memset(entry->mteTThDFallOwner, 0, sizeof(entry->mteTThDFallOwner));
                memcpy(entry->mteTThDFallOwner, request->requestvb->val.string,
                                                request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLINGEVENT:
                memset(entry->mteTThDFallEvent, 0, sizeof(entry->mteTThDFallEvent));
                memcpy(entry->mteTThDFallEvent, request->requestvb->val.string,
                                                request->requestvb->val_len);
                break;
            }
        }
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

    }
    return SNMP_ERR_NOERROR;
}
