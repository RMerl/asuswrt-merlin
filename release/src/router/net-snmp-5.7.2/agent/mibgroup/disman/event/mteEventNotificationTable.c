/*
 * DisMan Event MIB:
 *     Implementation of the mteEventNotificationTable MIB interface
 * See 'mteEvent.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "disman/event/mteEvent.h"
#include "disman/event/mteEventNotificationTable.h"

netsnmp_feature_require(table_tdata)
#ifndef NETSNMP_NO_WRITE_SUPPORT
netsnmp_feature_require(check_vb_type_and_max_size)
netsnmp_feature_require(check_vb_oid)
#endif /* NETSNMP_NO_WRITE_SUPPORT */

static netsnmp_table_registration_info *table_info;

/* Initializes the mteEventNotificationTable module */
void
init_mteEventNotificationTable(void)
{
    static oid  mteEventNotificationTable_oid[]   = { 1, 3, 6, 1, 2, 1, 88, 1, 4, 3 };
    size_t      mteEventNotificationTable_oid_len = OID_LENGTH(mteEventNotificationTable_oid);
    netsnmp_handler_registration    *reg;

    /*
     * Ensure the (combined) table container is available...
     */
    init_event_table_data();

    /*
     * ... then set up the MIB interface to the mteEventNotificationTable slice
     */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    reg = netsnmp_create_handler_registration("mteEventNotificationTable",
                                            mteEventNotificationTable_handler,
                                            mteEventNotificationTable_oid,
                                            mteEventNotificationTable_oid_len,
                                            HANDLER_CAN_RWRITE);
#else /* !NETSNMP_NO_WRITE_SUPPORT */
    reg = netsnmp_create_handler_registration("mteEventNotificationTable",
                                            mteEventNotificationTable_handler,
                                            mteEventNotificationTable_oid,
                                            mteEventNotificationTable_oid_len,
                                            HANDLER_CAN_RONLY);
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    netsnmp_table_helper_add_indexes(table_info,
                                     ASN_OCTET_STR,   /* index: mteOwner */
                                                      /* index: mteEventName */
                                     ASN_PRIV_IMPLIED_OCTET_STR,
                                     0);

    table_info->min_column = COLUMN_MTEEVENTNOTIFICATION;
    table_info->max_column = COLUMN_MTEEVENTNOTIFICATIONOBJECTS;

    /* Register this using the (common) event_table_data container */
    netsnmp_tdata_register(reg, event_table_data, table_info);
    DEBUGMSGTL(("disman:event:init", "Event Notify Table container (%p)\n",
                                      event_table_data));
}

void
shutdown_mteEventNotificationTable(void)
{
    if (table_info) {
	netsnmp_table_registration_info_free(table_info);
	table_info = NULL;
    }
}


/** handles requests for the mteEventNotificationTable table */
int
mteEventNotificationTable_handler(netsnmp_mib_handler *handler,
                                  netsnmp_handler_registration *reginfo,
                                  netsnmp_agent_request_info *reqinfo,
                                  netsnmp_request_info *requests)
{

    netsnmp_request_info       *request;
    netsnmp_table_request_info *tinfo;
    struct mteEvent            *entry;
    int ret;

    DEBUGMSGTL(("disman:event:mib", "Notification Table handler (%d)\n", reqinfo->mode));

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

            /*
             * The mteEventNotificationTable should only contains entries
             *   for rows where the mteEventActions 'notification(0)' bit
             *   is set. So skip entries where this isn't the case.
             */
            if (!entry || !(entry->mteEventActions & MTE_EVENT_NOTIFICATION))
                continue;

            switch (tinfo->colnum) {
            case COLUMN_MTEEVENTNOTIFICATION:
                snmp_set_var_typed_value(request->requestvb, ASN_OBJECT_ID,
                              (u_char *) entry->mteNotification,
                                         entry->mteNotification_len*sizeof(oid));
                break;
            case COLUMN_MTEEVENTNOTIFICATIONOBJECTSOWNER:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteNotifyOwner,
                                  strlen(entry->mteNotifyOwner));
                break;
            case COLUMN_MTEEVENTNOTIFICATIONOBJECTS:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteNotifyObjects,
                                  strlen(entry->mteNotifyObjects));
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

            tinfo = netsnmp_extract_table_info(request);

            /*
             * Since the mteEventNotificationTable only contains entries
             *   for rows where the mteEventActions 'notification(0)'
             *   bit is set, strictly speaking we should reject
             *   assignments where this isn't the case.
             * But SET requests that include an assignment of the
             *   'notification(0)' bit at the same time are valid,
             *   so would need to be accepted. Unfortunately, this
             *   assignment is only applied in the COMMIT pass, so
             *   it's difficult to detect whether this holds or not.
             *
             * Let's fudge things for now, by processing assignments
             *   even if the 'notification(0)' bit isn't set.
             */

            switch (tinfo->colnum) {
            case COLUMN_MTEEVENTNOTIFICATION:
                ret = netsnmp_check_vb_oid( request->requestvb );
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTEEVENTNOTIFICATIONOBJECTSOWNER:
            case COLUMN_MTEEVENTNOTIFICATIONOBJECTS:
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
             * The Event MIB is somewhat ambiguous as to whether
             *  mteEventNotificationTable (and mteEventSetTable)
             *  entries can be modified once the main mteEventTable
             *  entry has been marked 'active'. 
             * But it's clear from discussion on the DisMan mailing
             *  list is that the intention is not.
             *
             * So check for whether this row is already active,
             *  and reject *all* SET requests if it is.
             */
            entry = (struct mteEvent *) netsnmp_tdata_extract_entry(request);
            if (entry &&
                entry->flags & MTE_EVENT_FLAG_ACTIVE ) {
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

            entry = (struct mteEvent *) netsnmp_tdata_extract_entry(request);
            if (!entry) {
                /*
                 * New rows must be created via the RowStatus column
                 *   (in the main mteEventTable)
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

            entry = (struct mteEvent *) netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);

            switch (tinfo->colnum) {
            case COLUMN_MTEEVENTNOTIFICATION:
                memset(entry->mteNotification, 0, sizeof(entry->mteNotification));
                memcpy(entry->mteNotification, request->requestvb->val.objid,
                                               request->requestvb->val_len);
                entry->mteNotification_len = request->requestvb->val_len/sizeof(oid);
                break;
            case COLUMN_MTEEVENTNOTIFICATIONOBJECTSOWNER:
                memset(entry->mteNotifyOwner, 0, sizeof(entry->mteNotifyOwner));
                memcpy(entry->mteNotifyOwner, request->requestvb->val.string,
                                              request->requestvb->val_len);
                break;
            case COLUMN_MTEEVENTNOTIFICATIONOBJECTS:
                memset(entry->mteNotifyObjects, 0, sizeof(entry->mteNotifyObjects));
                memcpy(entry->mteNotifyObjects, request->requestvb->val.string,
                                                request->requestvb->val_len);
                break;
            }
        }
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    }
    return SNMP_ERR_NOERROR;
}
