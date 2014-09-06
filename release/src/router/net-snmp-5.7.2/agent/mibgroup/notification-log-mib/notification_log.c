#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <sys/types.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/ds_agent.h>
#include <net-snmp/agent/instance.h>
#include <net-snmp/agent/table.h>
#include <net-snmp/agent/table_data.h>
#include <net-snmp/agent/table_dataset.h>
#include "net-snmp/agent/sysORTable.h"
#include "notification_log.h"

netsnmp_feature_require(register_ulong_instance_context)
netsnmp_feature_require(register_read_only_counter32_instance_context)
netsnmp_feature_require(delete_table_data_set)
netsnmp_feature_require(table_dataset)
netsnmp_feature_require(date_n_time)

/*
 * column number definitions for table nlmLogTable
 */

#define COLUMN_NLMLOGINDEX		1
#define COLUMN_NLMLOGTIME		2
#define COLUMN_NLMLOGDATEANDTIME	3
#define COLUMN_NLMLOGENGINEID		4
#define COLUMN_NLMLOGENGINETADDRESS	5
#define COLUMN_NLMLOGENGINETDOMAIN	6
#define COLUMN_NLMLOGCONTEXTENGINEID	7
#define COLUMN_NLMLOGCONTEXTNAME	8
#define COLUMN_NLMLOGNOTIFICATIONID	9

/*
 * column number definitions for table nlmLogVariableTable
 */
#define COLUMN_NLMLOGVARIABLEINDEX		1
#define COLUMN_NLMLOGVARIABLEID			2
#define COLUMN_NLMLOGVARIABLEVALUETYPE		3
#define COLUMN_NLMLOGVARIABLECOUNTER32VAL	4
#define COLUMN_NLMLOGVARIABLEUNSIGNED32VAL	5
#define COLUMN_NLMLOGVARIABLETIMETICKSVAL	6
#define COLUMN_NLMLOGVARIABLEINTEGER32VAL	7
#define COLUMN_NLMLOGVARIABLEOCTETSTRINGVAL	8
#define COLUMN_NLMLOGVARIABLEIPADDRESSVAL	9
#define COLUMN_NLMLOGVARIABLEOIDVAL		10
#define COLUMN_NLMLOGVARIABLECOUNTER64VAL	11
#define COLUMN_NLMLOGVARIABLEOPAQUEVAL		12

static u_long   num_received = 0;
static u_long   num_deleted = 0;

static u_long   max_logged = 1000;      /* goes against the mib default of infinite */
static u_long   max_age = 1440; /* 1440 = 24 hours, which is the mib default */

static netsnmp_table_data_set *nlmLogTable;
static netsnmp_table_data_set *nlmLogVarTable;

static oid nlm_module_oid[] = { SNMP_OID_MIB2, 92 }; /* NOTIFICATION-LOG-MIB::notificationLogMIB */

static void
netsnmp_notif_log_remove_oldest(int count)
{
    netsnmp_table_row *deleterow, *tmprow, *deletevarrow;
    
    DEBUGMSGTL(("notification_log", "deleting %d log entry(s)\n", count));

    deleterow = netsnmp_table_data_set_get_first_row(nlmLogTable);
    for (; count && deleterow; deleterow = tmprow, --count) {
        /*
         * delete contained varbinds
         * xxx-rks: note that this assumes that only the default
         * log is used (ie for the first nlmLogTable row, the
         * first nlmLogVarTable rows will be the right ones).
         * the right thing to do would be to do a find based on
         * the nlmLogTable oid.
         */
        DEBUGMSGTL(("9:notification_log", "  deleting notification\n"));
        DEBUGIF("9:notification_log") {
            DEBUGMSGTL(("9:notification_log",
                        " base oid:"));
            DEBUGMSGOID(("9:notification_log", deleterow->index_oid,
                         deleterow->index_oid_len));
            DEBUGMSG(("9:notification_log", "\n"));
        }
        deletevarrow = netsnmp_table_data_set_get_first_row(nlmLogVarTable);
        for (; deletevarrow; deletevarrow = tmprow) {

            tmprow = netsnmp_table_data_set_get_next_row(nlmLogVarTable,
                                                          deletevarrow);
            
            DEBUGIF("9:notification_log") {
                DEBUGMSGTL(("9:notification_log",
                            "         :"));
                DEBUGMSGOID(("9:notification_log", deletevarrow->index_oid,
                             deletevarrow->index_oid_len));
                DEBUGMSG(("9:notification_log", "\n"));
            }
            if ((deleterow->index_oid_len == deletevarrow->index_oid_len - 1) &&
                snmp_oid_compare(deleterow->index_oid,
                                 deleterow->index_oid_len,
                                 deletevarrow->index_oid,
                                 deleterow->index_oid_len) == 0) {
                DEBUGMSGTL(("9:notification_log", "    deleting varbind\n"));
                netsnmp_table_dataset_remove_and_delete_row(nlmLogVarTable,
                                                             deletevarrow);
            }
            else
                break;
        }
        
        /*
         * delete the master row 
         */
        tmprow = netsnmp_table_data_set_get_next_row(nlmLogTable, deleterow);
        netsnmp_table_dataset_remove_and_delete_row(nlmLogTable,
                                                     deleterow);
        num_deleted++;
    }
    /** should have deleted all of them */
    netsnmp_assert(0 == count);
}

static void
check_log_size(unsigned int clientreg, void *clientarg)
{
    netsnmp_table_row *row;
    netsnmp_table_data_set_storage *data;
    u_long          count = 0;
    u_long          uptime;

    uptime = netsnmp_get_agent_uptime();

    if (!nlmLogTable || !nlmLogTable->table )  {
        DEBUGMSGTL(("notification_log", "missing log table\n"));
        return;
    }

    /*
     * check max allowed count
     */
    count = netsnmp_table_set_num_rows(nlmLogTable);
    DEBUGMSGTL(("notification_log",
                "logged notifications %lu; max %lu\n",
                    count, max_logged));
    if (count > max_logged) {
        count = count - max_logged;
        DEBUGMSGTL(("notification_log", "removing %lu extra notifications\n",
                    count));
        netsnmp_notif_log_remove_oldest(count);
    }

    /*
     * check max age 
     */
    if (0 == max_age)
        return;
    count = 0;
    for (row = netsnmp_table_data_set_get_first_row(nlmLogTable);
         row;
         row = netsnmp_table_data_set_get_next_row(nlmLogTable, row)) {

        data = (netsnmp_table_data_set_storage *) row->data;
        data = netsnmp_table_data_set_find_column(data, COLUMN_NLMLOGTIME);

        if (uptime < ((u_long)(*(data->data.integer) + max_age * 100 * 60)))
            break;
        ++count;
    }

    if (count) {
        DEBUGMSGTL(("notification_log", "removing %lu expired notifications\n",
                    count));
        netsnmp_notif_log_remove_oldest(count);
    }
}


/** Initialize the nlmLogVariableTable table by defining its contents and how it's structured */
static void
initialize_table_nlmLogVariableTable(const char * context)
{
    static oid      nlmLogVariableTable_oid[] =
        { 1, 3, 6, 1, 2, 1, 92, 1, 3, 2 };
    size_t          nlmLogVariableTable_oid_len =
        OID_LENGTH(nlmLogVariableTable_oid);
    netsnmp_table_data_set *table_set;
    netsnmp_handler_registration *reginfo;

    /*
     * create the table structure itself 
     */
    table_set = netsnmp_create_table_data_set("nlmLogVariableTable");
    nlmLogVarTable = table_set;
    nlmLogVarTable->table->store_indexes = 1;

    /***************************************************
     * Adding indexes
     */
    /*
     * declaring the nlmLogName index
     */
    DEBUGMSGTL(("initialize_table_nlmLogVariableTable",
                "adding index nlmLogName of type ASN_OCTET_STR to table nlmLogVariableTable\n"));
    netsnmp_table_dataset_add_index(table_set, ASN_OCTET_STR);
    /*
     * declaring the nlmLogIndex index
     */
    DEBUGMSGTL(("initialize_table_nlmLogVariableTable",
                "adding index nlmLogIndex of type ASN_UNSIGNED to table nlmLogVariableTable\n"));
    netsnmp_table_dataset_add_index(table_set, ASN_UNSIGNED);
    /*
     * declaring the nlmLogVariableIndex index
     */
    DEBUGMSGTL(("initialize_table_nlmLogVariableTable",
                "adding index nlmLogVariableIndex of type ASN_UNSIGNED to table nlmLogVariableTable\n"));
    netsnmp_table_dataset_add_index(table_set, ASN_UNSIGNED);

    /*
     * adding column nlmLogVariableID of type ASN_OBJECT_ID and access of
     * ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogVariableTable",
                "adding column nlmLogVariableID (#2) of type ASN_OBJECT_ID to table nlmLogVariableTable\n"));
    netsnmp_table_set_add_default_row(table_set, COLUMN_NLMLOGVARIABLEID,
                                      ASN_OBJECT_ID, 0, NULL, 0);
    /*
     * adding column nlmLogVariableValueType of type ASN_INTEGER and
     * access of ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogVariableTable",
                "adding column nlmLogVariableValueType (#3) of type ASN_INTEGER to table nlmLogVariableTable\n"));
    netsnmp_table_set_add_default_row(table_set,
                                      COLUMN_NLMLOGVARIABLEVALUETYPE,
                                      ASN_INTEGER, 0, NULL, 0);
    /*
     * adding column nlmLogVariableCounter32Val of type ASN_COUNTER and
     * access of ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogVariableTable",
                "adding column nlmLogVariableCounter32Val (#4) of type ASN_COUNTER to table nlmLogVariableTable\n"));
    netsnmp_table_set_add_default_row(table_set,
                                      COLUMN_NLMLOGVARIABLECOUNTER32VAL,
                                      ASN_COUNTER, 0, NULL, 0);
    /*
     * adding column nlmLogVariableUnsigned32Val of type ASN_UNSIGNED and
     * access of ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogVariableTable",
                "adding column nlmLogVariableUnsigned32Val (#5) of type ASN_UNSIGNED to table nlmLogVariableTable\n"));
    netsnmp_table_set_add_default_row(table_set,
                                      COLUMN_NLMLOGVARIABLEUNSIGNED32VAL,
                                      ASN_UNSIGNED, 0, NULL, 0);
    /*
     * adding column nlmLogVariableTimeTicksVal of type ASN_TIMETICKS and
     * access of ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogVariableTable",
                "adding column nlmLogVariableTimeTicksVal (#6) of type ASN_TIMETICKS to table nlmLogVariableTable\n"));
    netsnmp_table_set_add_default_row(table_set,
                                      COLUMN_NLMLOGVARIABLETIMETICKSVAL,
                                      ASN_TIMETICKS, 0, NULL, 0);
    /*
     * adding column nlmLogVariableInteger32Val of type ASN_INTEGER and
     * access of ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogVariableTable",
                "adding column nlmLogVariableInteger32Val (#7) of type ASN_INTEGER to table nlmLogVariableTable\n"));
    netsnmp_table_set_add_default_row(table_set,
                                      COLUMN_NLMLOGVARIABLEINTEGER32VAL,
                                      ASN_INTEGER, 0, NULL, 0);
    /*
     * adding column nlmLogVariableOctetStringVal of type ASN_OCTET_STR
     * and access of ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogVariableTable",
                "adding column nlmLogVariableOctetStringVal (#8) of type ASN_OCTET_STR to table nlmLogVariableTable\n"));
    netsnmp_table_set_add_default_row(table_set,
                                      COLUMN_NLMLOGVARIABLEOCTETSTRINGVAL,
                                      ASN_OCTET_STR, 0, NULL, 0);
    /*
     * adding column nlmLogVariableIpAddressVal of type ASN_IPADDRESS and
     * access of ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogVariableTable",
                "adding column nlmLogVariableIpAddressVal (#9) of type ASN_IPADDRESS to table nlmLogVariableTable\n"));
    netsnmp_table_set_add_default_row(table_set,
                                      COLUMN_NLMLOGVARIABLEIPADDRESSVAL,
                                      ASN_IPADDRESS, 0, NULL, 0);
    /*
     * adding column nlmLogVariableOidVal of type ASN_OBJECT_ID and access 
     * of ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogVariableTable",
                "adding column nlmLogVariableOidVal (#10) of type ASN_OBJECT_ID to table nlmLogVariableTable\n"));
    netsnmp_table_set_add_default_row(table_set,
                                      COLUMN_NLMLOGVARIABLEOIDVAL,
                                      ASN_OBJECT_ID, 0, NULL, 0);
    /*
     * adding column nlmLogVariableCounter64Val of type ASN_COUNTER64 and
     * access of ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogVariableTable",
                "adding column nlmLogVariableCounter64Val (#11) of type ASN_COUNTER64 to table nlmLogVariableTable\n"));
    netsnmp_table_set_add_default_row(table_set,
                                      COLUMN_NLMLOGVARIABLECOUNTER64VAL,
                                      ASN_COUNTER64, 0, NULL, 0);
    /*
     * adding column nlmLogVariableOpaqueVal of type ASN_OPAQUE and access 
     * of ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogVariableTable",
                "adding column nlmLogVariableOpaqueVal (#12) of type ASN_OPAQUE to table nlmLogVariableTable\n"));
    netsnmp_table_set_add_default_row(table_set,
                                      COLUMN_NLMLOGVARIABLEOPAQUEVAL,
                                      ASN_OPAQUE, 0, NULL, 0);

    /*
     * registering the table with the master agent 
     */
    /*
     * note: if you don't need a subhandler to deal with any aspects of
     * the request, change nlmLogVariableTable_handler to "NULL" 
     */
    reginfo =
        netsnmp_create_handler_registration ("nlmLogVariableTable",
                                             NULL,
                                             nlmLogVariableTable_oid,
                                             nlmLogVariableTable_oid_len,
                                             HANDLER_CAN_RWRITE);
    if (NULL != context)
        reginfo->contextName = strdup(context);
    netsnmp_register_table_data_set(reginfo, table_set, NULL);
}

/** Initialize the nlmLogTable table by defining its contents and how it's structured */
static void
initialize_table_nlmLogTable(const char * context)
{
    static oid      nlmLogTable_oid[] = { 1, 3, 6, 1, 2, 1, 92, 1, 3, 1 };
    size_t          nlmLogTable_oid_len = OID_LENGTH(nlmLogTable_oid);
    netsnmp_handler_registration *reginfo;

    /*
     * create the table structure itself 
     */
    nlmLogTable = netsnmp_create_table_data_set("nlmLogTable");

    /***************************************************
     * Adding indexes
     */
    /*
     * declaring the nlmLogIndex index
     */
    DEBUGMSGTL(("initialize_table_nlmLogTable",
                "adding index nlmLogName of type ASN_OCTET_STR to table nlmLogTable\n"));
    netsnmp_table_dataset_add_index(nlmLogTable, ASN_OCTET_STR);

    DEBUGMSGTL(("initialize_table_nlmLogTable",
                "adding index nlmLogIndex of type ASN_UNSIGNED to table nlmLogTable\n"));
    netsnmp_table_dataset_add_index(nlmLogTable, ASN_UNSIGNED);

    /*
     * adding column nlmLogTime of type ASN_TIMETICKS and access of
     * ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogTable",
                "adding column nlmLogTime (#2) of type ASN_TIMETICKS to table nlmLogTable\n"));
    netsnmp_table_set_add_default_row(nlmLogTable, COLUMN_NLMLOGTIME,
                                      ASN_TIMETICKS, 0, NULL, 0);
    /*
     * adding column nlmLogDateAndTime of type ASN_OCTET_STR and access of 
     * ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogTable",
                "adding column nlmLogDateAndTime (#3) of type ASN_OCTET_STR to table nlmLogTable\n"));
    netsnmp_table_set_add_default_row(nlmLogTable,
                                      COLUMN_NLMLOGDATEANDTIME,
                                      ASN_OCTET_STR, 0, NULL, 0);
    /*
     * adding column nlmLogEngineID of type ASN_OCTET_STR and access of
     * ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogTable",
                "adding column nlmLogEngineID (#4) of type ASN_OCTET_STR to table nlmLogTable\n"));
    netsnmp_table_set_add_default_row(nlmLogTable, COLUMN_NLMLOGENGINEID,
                                      ASN_OCTET_STR, 0, NULL, 0);
    /*
     * adding column nlmLogEngineTAddress of type ASN_OCTET_STR and access 
     * of ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogTable",
                "adding column nlmLogEngineTAddress (#5) of type ASN_OCTET_STR to table nlmLogTable\n"));
    netsnmp_table_set_add_default_row(nlmLogTable,
                                      COLUMN_NLMLOGENGINETADDRESS,
                                      ASN_OCTET_STR, 0, NULL, 0);
    /*
     * adding column nlmLogEngineTDomain of type ASN_OBJECT_ID and access
     * of ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogTable",
                "adding column nlmLogEngineTDomain (#6) of type ASN_OBJECT_ID to table nlmLogTable\n"));
    netsnmp_table_set_add_default_row(nlmLogTable,
                                      COLUMN_NLMLOGENGINETDOMAIN,
                                      ASN_OBJECT_ID, 0, NULL, 0);
    /*
     * adding column nlmLogContextEngineID of type ASN_OCTET_STR and
     * access of ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogTable",
                "adding column nlmLogContextEngineID (#7) of type ASN_OCTET_STR to table nlmLogTable\n"));
    netsnmp_table_set_add_default_row(nlmLogTable,
                                      COLUMN_NLMLOGCONTEXTENGINEID,
                                      ASN_OCTET_STR, 0, NULL, 0);
    /*
     * adding column nlmLogContextName of type ASN_OCTET_STR and access of 
     * ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogTable",
                "adding column nlmLogContextName (#8) of type ASN_OCTET_STR to table nlmLogTable\n"));
    netsnmp_table_set_add_default_row(nlmLogTable,
                                      COLUMN_NLMLOGCONTEXTNAME,
                                      ASN_OCTET_STR, 0, NULL, 0);
    /*
     * adding column nlmLogNotificationID of type ASN_OBJECT_ID and access 
     * of ReadOnly 
     */
    DEBUGMSGTL(("initialize_table_nlmLogTable",
                "adding column nlmLogNotificationID (#9) of type ASN_OBJECT_ID to table nlmLogTable\n"));
    netsnmp_table_set_add_default_row(nlmLogTable,
                                      COLUMN_NLMLOGNOTIFICATIONID,
                                      ASN_OBJECT_ID, 0, NULL, 0);

    /*
     * registering the table with the master agent 
     */
    /*
     * note: if you don't need a subhandler to deal with any aspects of
     * the request, change nlmLogTable_handler to "NULL" 
     */
    reginfo =
        netsnmp_create_handler_registration("nlmLogTable", NULL,
                                            nlmLogTable_oid,
                                            nlmLogTable_oid_len,
                                            HANDLER_CAN_RWRITE);
    if (NULL != context)
        reginfo->contextName = strdup(context);
    netsnmp_register_table_data_set(reginfo, nlmLogTable, NULL);

    /*
     * hmm...  5 minutes seems like a reasonable time to check for out
     * dated notification logs right? 
     */
    snmp_alarm_register(300, SA_REPEAT, check_log_size, NULL);
}

static int
notification_log_config_handler(netsnmp_mib_handler *handler,
                                netsnmp_handler_registration *reginfo,
                                netsnmp_agent_request_info *reqinfo,
                                netsnmp_request_info *requests)
{
    /*
     *this handler exists only to act as a trigger when the
     * configuration variables get set to a value and thus
     * notifications must be possibly deleted from our archives.
     */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    if (reqinfo->mode == MODE_SET_COMMIT)
        check_log_size(0, NULL);
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    return SNMP_ERR_NOERROR;
}

void
init_notification_log(void)
{
    static oid      my_nlmStatsGlobalNotificationsLogged_oid[] =
        { 1, 3, 6, 1, 2, 1, 92, 1, 2, 1, 0 };
    static oid      my_nlmStatsGlobalNotificationsBumped_oid[] =
        { 1, 3, 6, 1, 2, 1, 92, 1, 2, 2, 0 };
    static oid      my_nlmConfigGlobalEntryLimit_oid[] =
        { 1, 3, 6, 1, 2, 1, 92, 1, 1, 1, 0 };
    static oid      my_nlmConfigGlobalAgeOut_oid[] =
        { 1, 3, 6, 1, 2, 1, 92, 1, 1, 2, 0 };
    char * context;
    char * apptype;

    context = netsnmp_ds_get_string(NETSNMP_DS_APPLICATION_ID, 
                                    NETSNMP_DS_NOTIF_LOG_CTX);

    DEBUGMSGTL(("notification_log", "registering with '%s' context\n",
                   SNMP_STRORNULL(context)));

    /*
     * static variables 
     */
    netsnmp_register_read_only_counter32_instance_context
        ("nlmStatsGlobalNotificationsLogged",
         my_nlmStatsGlobalNotificationsLogged_oid,
         OID_LENGTH(my_nlmStatsGlobalNotificationsLogged_oid),
         &num_received, NULL, context);

    netsnmp_register_read_only_counter32_instance_context
        ("nlmStatsGlobalNotificationsBumped",
         my_nlmStatsGlobalNotificationsBumped_oid,
         OID_LENGTH(my_nlmStatsGlobalNotificationsBumped_oid),
         &num_deleted, NULL, context);

    netsnmp_register_ulong_instance_context("nlmConfigGlobalEntryLimit",
                                            my_nlmConfigGlobalEntryLimit_oid,
                                            OID_LENGTH
                                            (my_nlmConfigGlobalEntryLimit_oid),
                                            &max_logged,
                                            notification_log_config_handler,
                                            context);

    netsnmp_register_ulong_instance_context("nlmConfigGlobalAgeOut",
                                            my_nlmConfigGlobalAgeOut_oid,
                                            OID_LENGTH
                                            (my_nlmConfigGlobalAgeOut_oid),
                                            &max_age,
                                            notification_log_config_handler,
                                            context);

    /*
     * tables 
     */
    initialize_table_nlmLogVariableTable(context);
    initialize_table_nlmLogTable(context);

    /*
     * disable flag 
     */
    apptype = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
                                    NETSNMP_DS_LIB_APPTYPE);
    netsnmp_ds_register_config(ASN_BOOLEAN, apptype, "dontRetainLogs",
                               NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_AGENT_DONT_RETAIN_NOTIFICATIONS);
    netsnmp_ds_register_config(ASN_BOOLEAN, apptype, "doNotRetainNotificationLogs",
                               NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_AGENT_DONT_RETAIN_NOTIFICATIONS);
#if 0
    /* xxx-rks: config for max size; should be peristent too, & tied to mib */
    netsnmp_ds_register_config(ASN_INTEGER,
                               netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
                                                     NETSNMP_DS_LIB_APPTYPE),
                               "notificationLogMax",
                               NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_AGENT_NOTIF_LOG_MAX);
#endif

    REGISTER_SYSOR_ENTRY(nlm_module_oid, 
        "The MIB module for logging SNMP Notifications.");
}

void
shutdown_notification_log(void)
{
    max_logged = 0;
    check_log_size(0, NULL);
    netsnmp_delete_table_data_set(nlmLogTable);
    nlmLogTable = NULL;

    UNREGISTER_SYSOR_ENTRY(nlm_module_oid);
}

void
log_notification(netsnmp_pdu *pdu, netsnmp_transport *transport)
{
    long            tmpl;
    netsnmp_table_row *row;

    static u_long   default_num = 0;

    static oid      snmptrapoid[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };
    size_t          snmptrapoid_len = OID_LENGTH(snmptrapoid);
    netsnmp_variable_list *vptr;
    u_char         *logdate;
    size_t          logdate_size;
    time_t          timetnow;

    u_long          vbcount = 0;
    u_long          tmpul;
    int             col;
    netsnmp_pdu    *orig_pdu = pdu;

    if (!nlmLogVarTable
        || netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                                  NETSNMP_DS_APP_DONT_LOG)) {
        return;
    }

    DEBUGMSGTL(("notification_log", "logging something\n"));
    row = netsnmp_create_table_data_row();

    ++num_received;
    default_num++;

    /*
     * indexes to the table 
     */
    netsnmp_table_row_add_index(row, ASN_OCTET_STR, "default",
                                strlen("default"));
    netsnmp_table_row_add_index(row, ASN_UNSIGNED, &default_num,
                                sizeof(default_num));

    /*
     * add the data 
     */
    tmpl = netsnmp_get_agent_uptime();
    netsnmp_set_row_column(row, COLUMN_NLMLOGTIME, ASN_TIMETICKS,
                           &tmpl, sizeof(tmpl));
    time(&timetnow);
    logdate = date_n_time(&timetnow, &logdate_size);
    netsnmp_set_row_column(row, COLUMN_NLMLOGDATEANDTIME, ASN_OCTET_STR,
                           logdate, logdate_size);
    netsnmp_set_row_column(row, COLUMN_NLMLOGENGINEID, ASN_OCTET_STR,
                           pdu->securityEngineID,
                           pdu->securityEngineIDLen);
    if (transport && transport->domain == netsnmpUDPDomain) {
        /*
         * check for the udp domain 
         */
        struct sockaddr_in *addr =
            (struct sockaddr_in *) pdu->transport_data;
        if (addr) {
            char            buf[sizeof(in_addr_t) +
                                sizeof(addr->sin_port)];
            in_addr_t       locaddr = htonl(addr->sin_addr.s_addr);
            u_short         portnum = htons(addr->sin_port);
            memcpy(buf, &locaddr, sizeof(in_addr_t));
            memcpy(buf + sizeof(in_addr_t), &portnum,
                   sizeof(addr->sin_port));
            netsnmp_set_row_column(row, COLUMN_NLMLOGENGINETADDRESS,
                                   ASN_OCTET_STR, buf,
                                   sizeof(in_addr_t) +
                                   sizeof(addr->sin_port));
        }
    }
    if (transport)
        netsnmp_set_row_column(row, COLUMN_NLMLOGENGINETDOMAIN,
                               ASN_OBJECT_ID,
                               transport->domain,
                               sizeof(oid) * transport->domain_length);
    netsnmp_set_row_column(row, COLUMN_NLMLOGCONTEXTENGINEID,
                           ASN_OCTET_STR, pdu->contextEngineID,
                           pdu->contextEngineIDLen);
    netsnmp_set_row_column(row, COLUMN_NLMLOGCONTEXTNAME, ASN_OCTET_STR,
                           pdu->contextName, pdu->contextNameLen);

    if (pdu->command == SNMP_MSG_TRAP)
	pdu = convert_v1pdu_to_v2(orig_pdu);
    for (vptr = pdu->variables; vptr; vptr = vptr->next_variable) {
        if (snmp_oid_compare(snmptrapoid, snmptrapoid_len,
                             vptr->name, vptr->name_length) == 0) {
            netsnmp_set_row_column(row, COLUMN_NLMLOGNOTIFICATIONID,
                                   ASN_OBJECT_ID, vptr->val.string,
                                   vptr->val_len);
        } else {
            netsnmp_table_row *myrow;
            myrow = netsnmp_create_table_data_row();

            /*
             * indexes to the table 
             */
            netsnmp_table_row_add_index(myrow, ASN_OCTET_STR, "default",
                                        strlen("default"));
            netsnmp_table_row_add_index(myrow, ASN_UNSIGNED, &default_num,
                                        sizeof(default_num));
            vbcount++;
            netsnmp_table_row_add_index(myrow, ASN_UNSIGNED, &vbcount,
                                        sizeof(vbcount));

            /*
             * OID 
             */
            netsnmp_set_row_column(myrow, COLUMN_NLMLOGVARIABLEID,
                                   ASN_OBJECT_ID, vptr->name,
                                   vptr->name_length * sizeof(oid));

            /*
             * value 
             */
            switch (vptr->type) {
            case ASN_OBJECT_ID:
                tmpul = 7;
                col = COLUMN_NLMLOGVARIABLEOIDVAL;
                break;

            case ASN_INTEGER:
                tmpul = 4;
                col = COLUMN_NLMLOGVARIABLEINTEGER32VAL;
                break;

            case ASN_UNSIGNED:
                tmpul = 2;
                col = COLUMN_NLMLOGVARIABLEUNSIGNED32VAL;
                break;

            case ASN_COUNTER:
                tmpul = 1;
                col = COLUMN_NLMLOGVARIABLECOUNTER32VAL;
                break;

            case ASN_TIMETICKS:
                tmpul = 3;
                col = COLUMN_NLMLOGVARIABLETIMETICKSVAL;
                break;

            case ASN_OCTET_STR:
                tmpul = 6;
                col = COLUMN_NLMLOGVARIABLEOCTETSTRINGVAL;
                break;

            case ASN_IPADDRESS:
                tmpul = 5;
                col = COLUMN_NLMLOGVARIABLEIPADDRESSVAL;
                break;

            case ASN_COUNTER64:
                tmpul = 8;
                col = COLUMN_NLMLOGVARIABLECOUNTER64VAL;
                break;

            case ASN_OPAQUE:
                tmpul = 9;
                col = COLUMN_NLMLOGVARIABLEOPAQUEVAL;
                break;

            default:
                /*
                 * unsupported 
                 */
                DEBUGMSGTL(("notification_log",
                            "skipping type %d\n", vptr->type));
                (void)netsnmp_table_dataset_delete_row(myrow);
                continue;
            }
            netsnmp_set_row_column(myrow, COLUMN_NLMLOGVARIABLEVALUETYPE,
                                   ASN_INTEGER, & tmpul,
                                   sizeof(tmpul));
            netsnmp_set_row_column(myrow, col, vptr->type,
                                   vptr->val.string, vptr->val_len);
            DEBUGMSGTL(("notification_log",
                        "adding a row to the variables table\n"));
            netsnmp_table_dataset_add_row(nlmLogVarTable, myrow);
        }
    }

    if (pdu != orig_pdu)
        snmp_free_pdu( pdu );

    /*
     * store the row 
     */
    netsnmp_table_dataset_add_row(nlmLogTable, row);

    check_log_size(0, NULL);
    DEBUGMSGTL(("notification_log", "done logging something\n"));
}
