/*
 * DisMan Event MIB:
 *     Implementation of the mteTriggerBooleanTable MIB interface
 * See 'mteTrigger.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "disman/event/mteTrigger.h"
#include "disman/event/mteTriggerBooleanTable.h"

netsnmp_feature_require(table_tdata)
#ifndef NETSNMP_NO_WRITE_SUPPORT
netsnmp_feature_require(check_vb_type_and_max_size)
netsnmp_feature_require(check_vb_truthvalue)
#endif /* NETSNMP_NO_WRITE_SUPPORT */

static netsnmp_table_registration_info *table_info;

/** Initializes the mteTriggerBooleanTable module */
void
init_mteTriggerBooleanTable(void)
{
    static oid mteTBoolTable_oid[]    = { 1, 3, 6, 1, 2, 1, 88, 1, 2, 5 };
    size_t     mteTBoolTable_oid_len  = OID_LENGTH(mteTBoolTable_oid);
    netsnmp_handler_registration    *reg;

    /*
     * Ensure the (combined) table container is available...
     */
    init_trigger_table_data();

    /*
     * ... then set up the MIB interface to the mteTriggerBooleanTable slice
     */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    reg = netsnmp_create_handler_registration("mteTriggerBooleanTable",
                                            mteTriggerBooleanTable_handler,
                                            mteTBoolTable_oid,
                                            mteTBoolTable_oid_len,
                                            HANDLER_CAN_RWRITE);
#else /* !NETSNMP_NO_WRITE_SUPPORT */
    reg = netsnmp_create_handler_registration("mteTriggerBooleanTable",
                                            mteTriggerBooleanTable_handler,
                                            mteTBoolTable_oid,
                                            mteTBoolTable_oid_len,
                                            HANDLER_CAN_RWRITE);
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    netsnmp_table_helper_add_indexes(table_info,
                                     ASN_OCTET_STR, /* index: mteOwner       */
                                                    /* index: mteTriggerName */
                                     ASN_PRIV_IMPLIED_OCTET_STR,
                                     0);

    table_info->min_column = COLUMN_MTETRIGGERBOOLEANCOMPARISON;
    table_info->max_column = COLUMN_MTETRIGGERBOOLEANEVENT;

    /* Register this using the (common) trigger_table_data container */
    netsnmp_tdata_register(reg, trigger_table_data, table_info);
    DEBUGMSGTL(("disman:event:init", "Trigger Bool Table\n"));
}

void
shutdown_mteTriggerBooleanTable(void)
{
    if (table_info) {
	netsnmp_table_registration_info_free(table_info);
	table_info = NULL;
    }
}

/** handles requests for the mteTriggerBooleanTable table */
int
mteTriggerBooleanTable_handler(netsnmp_mib_handler *handler,
                               netsnmp_handler_registration *reginfo,
                               netsnmp_agent_request_info *reqinfo,
                               netsnmp_request_info *requests)
{

    netsnmp_request_info       *request;
    netsnmp_table_request_info *tinfo;
    struct mteTrigger          *entry;
    int ret;

    DEBUGMSGTL(("disman:event:mib", "Boolean Table handler (%d)\n",
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
             * The mteTriggerBooleanTable should only contains entries for
             *   rows where the mteTriggerTest 'boolean(1)' bit is set.
             * So skip entries where this isn't the case.
             */
            if (!entry || !(entry->mteTriggerTest & MTE_TRIGGER_BOOLEAN )) {
                netsnmp_request_set_error(request, SNMP_NOSUCHINSTANCE);
                continue;
            }

            switch (tinfo->colnum) {
            case COLUMN_MTETRIGGERBOOLEANCOMPARISON:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->mteTBoolComparison);
                break;
            case COLUMN_MTETRIGGERBOOLEANVALUE:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->mteTBoolValue);
                break;
            case COLUMN_MTETRIGGERBOOLEANSTARTUP:
                ret = (entry->flags & MTE_TRIGGER_FLAG_BSTART ) ?
                           TV_TRUE : TV_FALSE;
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, ret);
                break;
            case COLUMN_MTETRIGGERBOOLEANOBJECTSOWNER:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTBoolObjOwner,
                                  strlen(entry->mteTBoolObjOwner));
                break;
            case COLUMN_MTETRIGGERBOOLEANOBJECTS:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTBoolObjects,
                                  strlen(entry->mteTBoolObjects));
                break;
            case COLUMN_MTETRIGGERBOOLEANEVENTOWNER:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTBoolEvOwner,
                                  strlen(entry->mteTBoolEvOwner));
                break;
            case COLUMN_MTETRIGGERBOOLEANEVENT:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                              (u_char *) entry->mteTBoolEvent,
                                  strlen(entry->mteTBoolEvent));
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
             * Since the mteTriggerBooleanTable only contains entries for
             *   rows where the mteTriggerTest 'boolean(1)' bit is set,
             *   strictly speaking we should reject assignments where
             *   this isn't the case.
             * But SET requests that include an assignment of the
             *   'boolean(1)' bit at the same time are valid, so would
             *    need to be accepted. Unfortunately, this assignment
             *   is only applied in the COMMIT pass, so it's difficult
             *   to detect whether this holds or not.
             *
             * Let's fudge things for now, by processing assignments
             *   even if the 'boolean(1)' bit isn't set.
             */
            switch (tinfo->colnum) {
            case COLUMN_MTETRIGGERBOOLEANCOMPARISON:
                ret = netsnmp_check_vb_int_range(request->requestvb,
                              MTE_BOOL_UNEQUAL, MTE_BOOL_GREATEREQUAL);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERBOOLEANVALUE:
                ret = netsnmp_check_vb_int(request->requestvb);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERBOOLEANSTARTUP:
                ret = netsnmp_check_vb_truthvalue(request->requestvb);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERBOOLEANOBJECTSOWNER:
            case COLUMN_MTETRIGGERBOOLEANOBJECTS:
            case COLUMN_MTETRIGGERBOOLEANEVENTOWNER:
            case COLUMN_MTETRIGGERBOOLEANEVENT:
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
            case COLUMN_MTETRIGGERBOOLEANCOMPARISON:
                entry->mteTBoolComparison = *request->requestvb->val.integer;
                break;
            case COLUMN_MTETRIGGERBOOLEANVALUE:
                entry->mteTBoolValue      = *request->requestvb->val.integer;
                break;
            case COLUMN_MTETRIGGERBOOLEANSTARTUP:
                if (*request->requestvb->val.integer == TV_TRUE)
                    entry->flags |=  MTE_TRIGGER_FLAG_BSTART;
                else
                    entry->flags &= ~MTE_TRIGGER_FLAG_BSTART;
                break;
            case COLUMN_MTETRIGGERBOOLEANOBJECTSOWNER:
                memset(entry->mteTBoolObjOwner, 0, sizeof(entry->mteTBoolObjOwner));
                memcpy(entry->mteTBoolObjOwner, request->requestvb->val.string,
                                                request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGERBOOLEANOBJECTS:
                memset(entry->mteTBoolObjects, 0, sizeof(entry->mteTBoolObjects));
                memcpy(entry->mteTBoolObjects, request->requestvb->val.string,
                                               request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGERBOOLEANEVENTOWNER:
                memset(entry->mteTBoolEvOwner, 0, sizeof(entry->mteTBoolEvOwner));
                memcpy(entry->mteTBoolEvOwner, request->requestvb->val.string,
                                               request->requestvb->val_len);
                break;
            case COLUMN_MTETRIGGERBOOLEANEVENT:
                memset(entry->mteTBoolEvent, 0, sizeof(entry->mteTBoolEvent));
                memcpy(entry->mteTBoolEvent, request->requestvb->val.string,
                                             request->requestvb->val_len);
                break;
            }
        }
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    }
    return SNMP_ERR_NOERROR;
}
