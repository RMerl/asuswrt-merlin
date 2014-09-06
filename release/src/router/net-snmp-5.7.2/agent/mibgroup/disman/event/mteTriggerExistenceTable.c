/*
 * DisMan Event MIB:
 *     Implementation of the mteTriggerExistenceTable MIB interface
 * See 'mteTrigger.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "disman/event/mteTrigger.h"
#include "disman/event/mteTriggerExistenceTable.h"

netsnmp_feature_require(table_tdata)
#ifndef NETSNMP_NO_WRITE_SUPPORT
netsnmp_feature_require(check_vb_type_and_max_size)
#endif /* NETSNMP_NO_WRITE_SUPPORT */

static netsnmp_table_registration_info *table_info;

/* Initializes the mteTriggerExistenceTable module */
void
init_mteTriggerExistenceTable(void)
{
    static oid mteTExistTable_oid[]   = { 1, 3, 6, 1, 2, 1, 88, 1, 2, 4 };
    size_t     mteTExistTable_oid_len = OID_LENGTH(mteTExistTable_oid);
    netsnmp_handler_registration    *reg;
    int        rc;

    /*
     * Ensure the (combined) table container is available...
     */
    init_trigger_table_data();

    /*
     * ... then set up the MIB interface to the mteTriggerExistenceTable slice
     */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    reg = netsnmp_create_handler_registration("mteTriggerExistenceTable",
                                            mteTriggerExistenceTable_handler,
                                            mteTExistTable_oid,
                                            mteTExistTable_oid_len,
                                            HANDLER_CAN_RWRITE);
#else /* !NETSNMP_NO_WRITE_SUPPORT */
    reg = netsnmp_create_handler_registration("mteTriggerExistenceTable",
                                            mteTriggerExistenceTable_handler,
                                            mteTExistTable_oid,
                                            mteTExistTable_oid_len,
                                            HANDLER_CAN_RONLY);
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    netsnmp_table_helper_add_indexes(table_info,
                                     ASN_OCTET_STR, /* index: mteOwner       */
                                                    /* index: mteTriggerName */
                                     ASN_PRIV_IMPLIED_OCTET_STR,
                                     0);

    table_info->min_column = COLUMN_MTETRIGGEREXISTENCETEST;
    table_info->max_column = COLUMN_MTETRIGGEREXISTENCEEVENT;

    /* Register this using the (common) trigger_table_data container */
    rc = netsnmp_tdata_register(reg, trigger_table_data, table_info);
    if (rc != SNMPERR_SUCCESS)
        return;

    netsnmp_handler_owns_table_info(reg->handler->next);
    DEBUGMSGTL(("disman:event:init", "Trigger Exist Table\n"));
}


/** handles requests for the mteTriggerExistenceTable table */
int
mteTriggerExistenceTable_handler(netsnmp_mib_handler *handler,
                                 netsnmp_handler_registration *reginfo,
                                 netsnmp_agent_request_info *reqinfo,
                                 netsnmp_request_info *requests)
{

    netsnmp_request_info       *request;
    netsnmp_table_request_info *tinfo;
    struct mteTrigger          *entry;
    int ret;

    DEBUGMSGTL(("disman:event:mib", "Exist Table handler (%d)\n",
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
             * The mteTriggerExistenceTable should only contains entries for
             *   rows where the mteTriggerTest 'existence(0)' bit is set.
             * So skip entries where this isn't the case.
             */
            if (!entry || !(entry->mteTriggerTest & MTE_TRIGGER_EXISTENCE )) {
                netsnmp_request_set_error(request, SNMP_NOSUCHINSTANCE);
                continue;
            }

            switch (tinfo->colnum) {
            case COLUMN_MTETRIGGEREXISTENCETEST:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *)&entry->mteTExTest, 1);
                break;
            case COLUMN_MTETRIGGEREXISTENCESTARTUP:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *)&entry->mteTExStartup, 1);
                break;
            case COLUMN_MTETRIGGEREXISTENCEOBJECTSOWNER:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTExObjOwner,
                                  strlen(entry->mteTExObjOwner));
                break;
            case COLUMN_MTETRIGGEREXISTENCEOBJECTS:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTExObjects,
                                  strlen(entry->mteTExObjects));
                break;
            case COLUMN_MTETRIGGEREXISTENCEEVENTOWNER:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTExEvOwner,
                                  strlen(entry->mteTExEvOwner));
                break;
            case COLUMN_MTETRIGGEREXISTENCEEVENT:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTExEvent,
                                  strlen(entry->mteTExEvent));
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
             * Since the mteTriggerExistenceTable only contains entries for
             *   rows where the mteTriggerTest 'existence(0)' bit is set,
             *   strictly speaking we should reject assignments where
             *   this isn't the case.
             * But SET requests that include an assignment of the
             *   'existence(0)' bit at the same time are valid, so would
             *    need to be accepted. Unfortunately, this assignment
             *   is only applied in the COMMIT pass, so it's difficult
             *   to detect whether this holds or not.
             *
             * Let's fudge things for now, by processing assignments
             *   even if the 'existence(0)' bit isn't set.
             */
            switch (tinfo->colnum) {
            case COLUMN_MTETRIGGEREXISTENCETEST:
            case COLUMN_MTETRIGGEREXISTENCESTARTUP:
                ret = netsnmp_check_vb_type_and_size(
                         request->requestvb, ASN_OCTET_STR, 1);
                if (ret != SNMP_ERR_NOERROR ) {
                    netsnmp_set_request_error(reqinfo, request, ret );
                    return SNMP_ERR_NOERROR;
                }
                break;

            case COLUMN_MTETRIGGEREXISTENCEOBJECTSOWNER:
            case COLUMN_MTETRIGGEREXISTENCEOBJECTS:
            case COLUMN_MTETRIGGEREXISTENCEEVENTOWNER:
            case COLUMN_MTETRIGGEREXISTENCEEVENT:
                ret = netsnmp_check_vb_type_and_max_size(
                          request->requestvb, ASN_OCTET_STR, MTE_STR1_LEN);
                if (ret != SNMP_ERR_NOERROR ) {
                    netsnmp_set_request_error(reqinfo, request, ret );
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
            case COLUMN_MTETRIGGEREXISTENCETEST:
                entry->mteTExTest    = request->requestvb->val.string[0];
                break;
            case COLUMN_MTETRIGGEREXISTENCESTARTUP:
                entry->mteTExStartup = request->requestvb->val.string[0];
                break;
            case COLUMN_MTETRIGGEREXISTENCEOBJECTSOWNER:
                memset(entry->mteTExObjOwner, 0, sizeof(entry->mteTExObjOwner));
                memcpy(entry->mteTExObjOwner, request->requestvb->val.string,
                                              request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGEREXISTENCEOBJECTS:
                memset(entry->mteTExObjects, 0, sizeof(entry->mteTExObjects));
                memcpy(entry->mteTExObjects, request->requestvb->val.string,
                                             request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGEREXISTENCEEVENTOWNER:
                memset(entry->mteTExEvOwner, 0, sizeof(entry->mteTExEvOwner));
                memcpy(entry->mteTExEvOwner, request->requestvb->val.string,
                                             request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGEREXISTENCEEVENT:
                memset(entry->mteTExEvent, 0, sizeof(entry->mteTExEvent));
                memcpy(entry->mteTExEvent, request->requestvb->val.string,
                                           request->requestvb->val_len);
                break;
            }
        }
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    }
    return SNMP_ERR_NOERROR;
}
