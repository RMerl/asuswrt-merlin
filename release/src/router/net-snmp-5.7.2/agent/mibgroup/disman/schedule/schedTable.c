/*
 * DisMan Schedule MIB:
 *     Core implementation of the schedTable MIB interface.
 * See 'schedCore.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "utilities/iquery.h"
#include "disman/schedule/schedCore.h"
#include "disman/schedule/schedTable.h"

netsnmp_feature_require(iquery)
netsnmp_feature_require(iquery_pdu_session)
netsnmp_feature_require(table_tdata)
netsnmp_feature_require(date_n_time)
netsnmp_feature_require(check_vb_uint)
#ifndef NETSNMP_NO_WRITE_SUPPORT
netsnmp_feature_require(check_vb_type_and_max_size)
netsnmp_feature_require(check_vb_oid)
netsnmp_feature_require(check_vb_truthvalue)
netsnmp_feature_require(table_tdata_insert_row)
#endif /* NETSNMP_NO_WRITE_SUPPORT */

static netsnmp_table_registration_info *table_info;

/** Initializes the schedTable module */
void
init_schedTable(void)
{
    static oid      schedTable_oid[] = { 1, 3, 6, 1, 2, 1, 63, 1, 2 };
    size_t          schedTable_oid_len = OID_LENGTH(schedTable_oid);
    netsnmp_handler_registration    *reg;

    DEBUGMSGTL(("disman:schedule:init", "Initializing table\n"));
    /*
     * Ensure the schedule table container is available...
     */
    init_schedule_container();

    /*
     * ... then set up the MIB interface.
     */
    reg = netsnmp_create_handler_registration("schedTable",
                                            schedTable_handler,
                                            schedTable_oid,
                                            schedTable_oid_len,
                                            HANDLER_CAN_RWRITE);

    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    netsnmp_table_helper_add_indexes(table_info,
                                     ASN_OCTET_STR,  /* index: schedOwner */
                                     ASN_OCTET_STR,  /* index: schedName  */
                                     0);
    table_info->min_column = COLUMN_SCHEDDESCR;
    table_info->max_column = COLUMN_SCHEDTRIGGERS;

    netsnmp_tdata_register(reg, schedule_table, table_info);
}

void
shutdown_schedTable(void)
{
    if (table_info) {
	netsnmp_table_registration_info_free(table_info);
	table_info = NULL;
    }
}

/** handles requests for the schedTable table */
int
schedTable_handler(netsnmp_mib_handler *handler,
                   netsnmp_handler_registration *reginfo,
                   netsnmp_agent_request_info *reqinfo,
                   netsnmp_request_info *requests)
{

    netsnmp_request_info       *request;
    netsnmp_table_request_info *tinfo;
    netsnmp_tdata_row          *row;
    struct schedTable_entry    *entry;
    int    recalculate = 0;
    size_t len;
    char  *cp;
    char   owner[SCHED_STR1_LEN+1];
    char   name[ SCHED_STR1_LEN+1];
    int    ret;

    DEBUGMSGTL(("disman:schedule:mib", "Schedule handler (%d)\n",
                                        reqinfo->mode));
    switch (reqinfo->mode) {
        /*
         * Read-support (also covers GetNext requests)
         */
    case MODE_GET:
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            entry = (struct schedTable_entry *)
                    netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info( request);

            switch (tinfo->colnum) {
            case COLUMN_SCHEDDESCR:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                         entry->schedDescr,
                                  strlen(entry->schedDescr));
                break;
            case COLUMN_SCHEDINTERVAL:
                snmp_set_var_typed_integer(request->requestvb, ASN_UNSIGNED,
                                           entry->schedInterval);
                break;
            case COLUMN_SCHEDWEEKDAY:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                        &entry->schedWeekDay,
                                  sizeof(entry->schedWeekDay));
                break;
            case COLUMN_SCHEDMONTH:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                         entry->schedMonth,
                                  sizeof(entry->schedMonth));
                break;
            case COLUMN_SCHEDDAY:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                         entry->schedDay,
                                  sizeof(entry->schedDay));
                break;
            case COLUMN_SCHEDHOUR:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                         entry->schedHour,
                                  sizeof(entry->schedHour));
                break;
            case COLUMN_SCHEDMINUTE:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                         entry->schedMinute,
                                  sizeof(entry->schedMinute));
                break;
            case COLUMN_SCHEDCONTEXTNAME:
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                         entry->schedContextName,
                                  strlen(entry->schedContextName));
                break;
            case COLUMN_SCHEDVARIABLE:
                snmp_set_var_typed_value(request->requestvb, ASN_OBJECT_ID,
                               (u_char *)entry->schedVariable,
                                         entry->schedVariable_len*sizeof(oid));
                break;
            case COLUMN_SCHEDVALUE:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->schedValue);
                break;
            case COLUMN_SCHEDTYPE:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->schedType);
                break;
            case COLUMN_SCHEDADMINSTATUS:
                ret = (entry->flags & SCHEDULE_FLAG_ENABLED ) ?
                           TV_TRUE : TV_FALSE;
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, ret);
                break;
            case COLUMN_SCHEDOPERSTATUS:
                ret = (entry->flags & SCHEDULE_FLAG_ENABLED ) ?
                           TV_TRUE : TV_FALSE;
                /*
                 * Check for one-shot entries that have already fired
                 */
                if ((entry->schedType == SCHED_TYPE_ONESHOT) &&
                    (entry->schedLastRun != 0 ))
                    ret = 3;  /* finished(3) */
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, ret);
                break;
            case COLUMN_SCHEDFAILURES:
                snmp_set_var_typed_integer(request->requestvb, ASN_COUNTER,
                                           entry->schedFailures);
                break;
            case COLUMN_SCHEDLASTFAILURE:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->schedLastFailure);
                break;
            case COLUMN_SCHEDLASTFAILED:
                /*
                 * Convert 'schedLastFailed' timestamp
                 *   into DateAndTime string
                 */
                cp = (char *) date_n_time( &entry->schedLastFailed, &len );
                snmp_set_var_typed_value(request->requestvb, ASN_OCTET_STR,
                                         cp, len);
                break;
            case COLUMN_SCHEDSTORAGETYPE:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->schedStorageType);
                break;
            case COLUMN_SCHEDROWSTATUS:
                ret = (entry->flags & SCHEDULE_FLAG_ACTIVE ) ?
                           TV_TRUE : TV_FALSE;
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, ret);
                break;
            case COLUMN_SCHEDTRIGGERS:
                snmp_set_var_typed_integer(request->requestvb, ASN_COUNTER,
                                           entry->schedTriggers);
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

            entry = (struct schedTable_entry *)
                    netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info( request);

            switch (tinfo->colnum) {
            case COLUMN_SCHEDDESCR:
                ret = netsnmp_check_vb_type_and_max_size(
                          request->requestvb, ASN_OCTET_STR, SCHED_STR2_LEN);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDINTERVAL:
                ret = netsnmp_check_vb_uint( request->requestvb );
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDWEEKDAY:
                ret = netsnmp_check_vb_type_and_size(
                          request->requestvb, ASN_OCTET_STR, 1);
                /* XXX - check for bit(7) set */
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDMONTH:
                ret = netsnmp_check_vb_type_and_size(  /* max_size ?? */
                          request->requestvb, ASN_OCTET_STR, 2);
                /* XXX - check for bit(12)-bit(15) set */
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDDAY:
                ret = netsnmp_check_vb_type_and_size(  /* max_size ?? */
                          request->requestvb, ASN_OCTET_STR, 4+4);
                /* XXX - check for bit(62) or bit(63) set */
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDHOUR:
                ret = netsnmp_check_vb_type_and_size(  /* max_size ?? */
                          request->requestvb, ASN_OCTET_STR, 3);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDMINUTE:
                ret = netsnmp_check_vb_type_and_size(  /* max_size ?? */
                          request->requestvb, ASN_OCTET_STR, 8);
                /* XXX - check for bit(60)-bit(63) set */
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDCONTEXTNAME:
                ret = netsnmp_check_vb_type_and_max_size(
                          request->requestvb, ASN_OCTET_STR, SCHED_STR1_LEN);
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDVARIABLE:
                ret = netsnmp_check_vb_oid( request->requestvb );
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDVALUE:
                ret = netsnmp_check_vb_int( request->requestvb );
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDTYPE:
                ret = netsnmp_check_vb_int_range( request->requestvb,
                             SCHED_TYPE_PERIODIC, SCHED_TYPE_ONESHOT );
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDADMINSTATUS:
                ret = netsnmp_check_vb_truthvalue( request->requestvb );
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_WRONGTYPE);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDSTORAGETYPE:
                ret = netsnmp_check_vb_int_range( request->requestvb,
                                                  ST_NONE, ST_READONLY );
                /* XXX - check valid/consistent assignments */
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDROWSTATUS:
                ret = netsnmp_check_vb_rowstatus( request->requestvb,
                          (entry ? RS_ACTIVE: RS_NONEXISTENT));
                /* XXX - check consistency assignments */
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
            case COLUMN_SCHEDROWSTATUS:
                switch (*request->requestvb->val.integer) {
                case RS_CREATEANDGO:
                case RS_CREATEANDWAIT:
                    /*
                     * Create an (empty) new row structure
                     */
                    memset(owner, 0, SCHED_STR1_LEN+1);
                    memset(name,  0, SCHED_STR1_LEN+1);
                    memcpy(owner, tinfo->indexes->val.string,
                                  tinfo->indexes->val_len);
                    memcpy(name,  tinfo->indexes->next_variable->val.string,
                                  tinfo->indexes->next_variable->val_len);
                    row = schedTable_createEntry(owner, name);
                    if (!row) {
                        netsnmp_set_request_error(reqinfo, request,
                                                  SNMP_ERR_RESOURCEUNAVAILABLE);
                        return SNMP_ERR_NOERROR;
                    }
                    netsnmp_insert_tdata_row(request, row);
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
            case COLUMN_SCHEDROWSTATUS:
                switch (*request->requestvb->val.integer) {
                case RS_CREATEANDGO:
                case RS_CREATEANDWAIT:
                    /*
                     * Tidy up after a failed row creation request
                     */ 
                    entry = (struct schedTable_entry *)
                                netsnmp_tdata_extract_entry(request);
                    if (entry &&
                      !(entry->flags & SCHEDULE_FLAG_VALID)) {
                        row = (netsnmp_tdata_row *)
                                netsnmp_tdata_extract_row(request);
                        schedTable_removeEntry(row);
                    }
                }
            }
        }
        break;

    case MODE_SET_ACTION:
        for (request = requests; request; request = request->next) {
            entry = (struct schedTable_entry *)
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
        entry = NULL;
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            entry = (struct schedTable_entry *)
                    netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info( request);

            switch (tinfo->colnum) {
            case COLUMN_SCHEDDESCR:
                memset(entry->schedDescr, 0, sizeof(entry->schedDescr));
                memcpy(entry->schedDescr, request->requestvb->val.string,
                                          request->requestvb->val_len);
                break;
            case COLUMN_SCHEDINTERVAL:
                entry->schedInterval = *request->requestvb->val.integer;
                recalculate = 1;
                break;
            case COLUMN_SCHEDWEEKDAY:
                entry->schedWeekDay  = request->requestvb->val.string[0];
                recalculate = 1;
                break;
            case COLUMN_SCHEDMONTH:
                entry->schedMonth[0] = request->requestvb->val.string[0];
                entry->schedMonth[1] = request->requestvb->val.string[1];
                recalculate = 1;
                break;
            case COLUMN_SCHEDDAY:
                memset(entry->schedDay, 0, sizeof(entry->schedDay));
                memcpy(entry->schedDay, request->requestvb->val.string,
                                        request->requestvb->val_len);
                recalculate = 1;
                break;
            case COLUMN_SCHEDHOUR:
                entry->schedHour[0]  = request->requestvb->val.string[0];
                entry->schedHour[1]  = request->requestvb->val.string[1];
                entry->schedHour[2]  = request->requestvb->val.string[2];
                recalculate = 1;
                break;
            case COLUMN_SCHEDMINUTE:
                memset(entry->schedMinute, 0, sizeof(entry->schedMinute));
                memcpy(entry->schedMinute, request->requestvb->val.string,
                                           request->requestvb->val_len);
                recalculate = 1;
                break;
            case COLUMN_SCHEDCONTEXTNAME:
                memset(entry->schedContextName, 0, sizeof(entry->schedContextName));
                memcpy(entry->schedContextName,
                                           request->requestvb->val.string,
                                           request->requestvb->val_len);
                break;
            case COLUMN_SCHEDVARIABLE:
                memset(entry->schedVariable, 0, sizeof(entry->schedVariable));
                memcpy(entry->schedVariable,
                                           request->requestvb->val.string,
                                           request->requestvb->val_len);
                entry->schedVariable_len =
                                  request->requestvb->val_len/sizeof(oid);
                break;
            case COLUMN_SCHEDVALUE:
                entry->schedValue = *request->requestvb->val.integer;
                break;
            case COLUMN_SCHEDTYPE:
                entry->schedType  = *request->requestvb->val.integer;
                break;
            case COLUMN_SCHEDADMINSTATUS:
                if (*request->requestvb->val.integer == TV_TRUE)
                    entry->flags |=  SCHEDULE_FLAG_ENABLED;
                else
                    entry->flags &= ~SCHEDULE_FLAG_ENABLED;
                break;
            case COLUMN_SCHEDSTORAGETYPE:
                entry->schedStorageType = *request->requestvb->val.integer;
                break;
            case COLUMN_SCHEDROWSTATUS:
                switch (*request->requestvb->val.integer) {
                case RS_ACTIVE:
                    entry->flags |= SCHEDULE_FLAG_ACTIVE;
                    break;
                case RS_CREATEANDGO:
                    entry->flags |= SCHEDULE_FLAG_ACTIVE;
                    entry->flags |= SCHEDULE_FLAG_VALID;
                    entry->session =
                        netsnmp_iquery_pdu_session(reqinfo->asp->pdu);
                    break;
                case RS_CREATEANDWAIT:
                    entry->flags |= SCHEDULE_FLAG_VALID;
                    entry->session =
                        netsnmp_iquery_pdu_session(reqinfo->asp->pdu);
                    break;

                case RS_DESTROY:
                    row = (netsnmp_tdata_row *)
                               netsnmp_tdata_extract_row(request);
                    schedTable_removeEntry(row);
                }
                recalculate = 1;
                break;
            }
        }
        if (recalculate) {
            netsnmp_assert(entry);
            sched_nextTime(entry);
        }
        break;
    }
    return SNMP_ERR_NOERROR;
}
