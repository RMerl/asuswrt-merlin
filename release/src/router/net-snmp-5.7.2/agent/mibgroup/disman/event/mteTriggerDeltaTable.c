/*
 * DisMan Event MIB:
 *     Implementation of the mteTriggerDeltaTable MIB interface
 * See 'mteTrigger.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "disman/event/mteTrigger.h"
#include "disman/event/mteTriggerDeltaTable.h"

netsnmp_feature_require(table_tdata)
#ifndef NETSNMP_NO_WRITE_SUPPORT
netsnmp_feature_require(check_vb_oid)
netsnmp_feature_require(check_vb_truthvalue)
#endif /* NETSNMP_NO_WRITE_SUPPORT */

/** Initializes the mteTriggerDeltaTable module */
void
init_mteTriggerDeltaTable(void)
{
    static oid  mteTDeltaTable_oid[]   = { 1, 3, 6, 1, 2, 1, 88, 1, 2, 3 };
    size_t      mteTDeltaTable_oid_len = OID_LENGTH(mteTDeltaTable_oid);
    netsnmp_handler_registration    *reg;
    netsnmp_table_registration_info *table_info;
    int         rc;

    /*
     * Ensure the (combined) table container is available...
     */
    init_trigger_table_data();

    /*
     * ... then set up the MIB interface to the mteTriggerDeltaTable slice
     */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    reg = netsnmp_create_handler_registration("mteTriggerDeltaTable",
                                            mteTriggerDeltaTable_handler,
                                            mteTDeltaTable_oid,
                                            mteTDeltaTable_oid_len,
                                            HANDLER_CAN_RWRITE);
#else /* !NETSNMP_NO_WRITE_SUPPORT */
    reg = netsnmp_create_handler_registration("mteTriggerDeltaTable",
                                            mteTriggerDeltaTable_handler,
                                            mteTDeltaTable_oid,
                                            mteTDeltaTable_oid_len,
                                            HANDLER_CAN_RONLY);
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    netsnmp_table_helper_add_indexes(table_info,
                                     ASN_OCTET_STR, /* index: mteOwner       */
                                                    /* index: mteTriggerName */
                                     ASN_PRIV_IMPLIED_OCTET_STR,
                                     0);

    table_info->min_column = COLUMN_MTETRIGGERDELTADISCONTINUITYID;
    table_info->max_column = COLUMN_MTETRIGGERDELTADISCONTINUITYIDTYPE;

    /* Register this using the (common) trigger_table_data container */
    rc = netsnmp_tdata_register(reg, trigger_table_data, table_info);
    if (rc != SNMPERR_SUCCESS)
        return;
    netsnmp_handler_owns_table_info(reg->handler->next);
    DEBUGMSGTL(("disman:event:init", "Trigger Delta Table\n"));
}


/** handles requests for the mteTriggerDeltaTable table */
int
mteTriggerDeltaTable_handler(netsnmp_mib_handler *handler,
                             netsnmp_handler_registration *reginfo,
                             netsnmp_agent_request_info *reqinfo,
                             netsnmp_request_info *requests)
{

    netsnmp_request_info       *request;
    netsnmp_table_request_info *tinfo;
    struct mteTrigger          *entry;
    int ret;

    DEBUGMSGTL(("disman:event:mib", "Delta Table handler (%d)\n",
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
             *   rows where the mteTriggerSampleType is 'deltaValue(2)'
             * So skip entries where this isn't the case.
             */
            if (!entry || !(entry->flags & MTE_TRIGGER_FLAG_DELTA )) {
                netsnmp_request_set_error(request, SNMP_NOSUCHINSTANCE);
                continue;
            }

            switch (tinfo->colnum) {
            case COLUMN_MTETRIGGERDELTADISCONTINUITYID:
                snmp_set_var_typed_value(request->requestvb, ASN_OBJECT_ID,
                              (u_char *) entry->mteDeltaDiscontID,
                                         entry->mteDeltaDiscontID_len*sizeof(oid));
                break;
            case COLUMN_MTETRIGGERDELTADISCONTINUITYIDWILDCARD:
                ret = (entry->flags & MTE_TRIGGER_FLAG_DWILD ) ?
                           TV_TRUE : TV_FALSE;
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, ret);
                break;
            case COLUMN_MTETRIGGERDELTADISCONTINUITYIDTYPE:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->mteDeltaDiscontIDType);
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
             * Since the mteTriggerDeltaTable only contains entries for
             *   rows where mteTriggerSampleType is 'deltaValue(2)',
             *   strictly speaking we should reject assignments where
             *   this isn't the case.
             * But SET requests that include an assignment of
             *   'deltaValue(2)' at the same time are valid, so would
             *    need to be accepted. Unfortunately, this assignment
             *   is only applied in the COMMIT pass, so it's difficult
             *   to detect whether this holds or not.
             *
             * Let's fudge things for now, by processing
             *   assignments even if this value isn't set.
             */
            switch (tinfo->colnum) {
            case COLUMN_MTETRIGGERDELTADISCONTINUITYID:
                ret = netsnmp_check_vb_oid(request->requestvb);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERDELTADISCONTINUITYIDWILDCARD:
                ret = netsnmp_check_vb_truthvalue(request->requestvb);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERDELTADISCONTINUITYIDTYPE:
                ret = netsnmp_check_vb_int_range(request->requestvb,
                                                 MTE_DELTAD_TTICKS,
                                                 MTE_DELTAD_DATETIME);
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
            case COLUMN_MTETRIGGERDELTADISCONTINUITYID:
                if ( snmp_oid_compare(
                       request->requestvb->val.objid,
                       request->requestvb->val_len/sizeof(oid),
                       _sysUpTime_instance, _sysUpTime_inst_len) != 0 ) {
                    memset(entry->mteDeltaDiscontID, 0,
                           sizeof(entry->mteDeltaDiscontID));
                    memcpy(entry->mteDeltaDiscontID,
                           request->requestvb->val.string,
                           request->requestvb->val_len);
                    entry->mteDeltaDiscontID_len =
                        request->requestvb->val_len/sizeof(oid);
                    entry->flags &= ~MTE_TRIGGER_FLAG_SYSUPT;
                }
                break;
            case COLUMN_MTETRIGGERDELTADISCONTINUITYIDWILDCARD:
                if (*request->requestvb->val.integer == TV_TRUE)
                    entry->flags |=  MTE_TRIGGER_FLAG_DWILD;
                else
                    entry->flags &= ~MTE_TRIGGER_FLAG_DWILD;
                break;
            case COLUMN_MTETRIGGERDELTADISCONTINUITYIDTYPE:
                entry->mteDeltaDiscontIDType = *request->requestvb->val.integer;
                break;
            }
        }
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    }
    return SNMP_ERR_NOERROR;
}
