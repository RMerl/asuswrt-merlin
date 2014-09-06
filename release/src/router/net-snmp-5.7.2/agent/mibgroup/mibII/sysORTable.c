#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/table_container.h>
#include <net-snmp/agent/agent_sysORTable.h>
#include <net-snmp/agent/sysORTable.h>

#include "sysORTable.h"

#include <net-snmp/net-snmp-features.h>

netsnmp_feature_require(table_container)


/** Typical data structure for a row entry */
typedef struct sysORTable_entry_s {
    netsnmp_index            oid_index;
    oid                      sysORIndex;
    const struct sysORTable* data;
} sysORTable_entry;

/*
 * column number definitions for table sysORTable
 */
#define COLUMN_SYSORINDEX	1
#define COLUMN_SYSORID		2
#define COLUMN_SYSORDESCR	3
#define COLUMN_SYSORUPTIME	4

static netsnmp_container *table = NULL;
static u_long             sysORLastChange;
static oid                sysORNextIndex = 1;

/** create a new row in the table */
static void
register_foreach(const struct sysORTable* data, void* dummy)
{
    sysORTable_entry *entry;

    sysORLastChange = data->OR_uptime;

    entry = SNMP_MALLOC_TYPEDEF(sysORTable_entry);
    if (!entry) {
	snmp_log(LOG_ERR,
		 "could not allocate storage, sysORTable is inconsistent\n");
    } else {
	const oid firstNext = sysORNextIndex;
	netsnmp_iterator* it = CONTAINER_ITERATOR(table);

	do {
	    const sysORTable_entry* value;
	    const oid cur = sysORNextIndex;

	    if (sysORNextIndex == SNMP_MIN(MAX_SUBID, 2147483647UL))
		sysORNextIndex = 1;
	    else
		++sysORNextIndex;

	    for (value = (sysORTable_entry*)it->curr(it);
		 value && value->sysORIndex < cur;
		 value = (sysORTable_entry*)ITERATOR_NEXT(it)) {
	    }

	    if (value && value->sysORIndex == cur) {
		if (sysORNextIndex < cur)
		    it->reset(it);
	    } else {
		entry->sysORIndex = cur;
		break;
	    }
	} while (firstNext != sysORNextIndex);

	ITERATOR_RELEASE(it);

	if(firstNext == sysORNextIndex) {
            snmp_log(LOG_ERR, "Failed to locate a free index in sysORTable\n");
            free(entry);
	} else {
	    entry->data = data;
	    entry->oid_index.len = 1;
	    entry->oid_index.oids = &entry->sysORIndex;

	    CONTAINER_INSERT(table, entry);
	}
    }
}

static int
register_cb(int major, int minor, void* serv, void* client)
{
    DEBUGMSGTL(("mibII/sysORTable/register_cb",
                "register_cb(%d, %d, %p, %p)\n", major, minor, serv, client));
    register_foreach((struct sysORTable*)serv, NULL);
    return SNMP_ERR_NOERROR;
}

/** remove a row from the table */
static int
unregister_cb(int major, int minor, void* serv, void* client)
{
    sysORTable_entry *value;
    netsnmp_iterator* it = CONTAINER_ITERATOR(table);

    DEBUGMSGTL(("mibII/sysORTable/unregister_cb",
                "unregister_cb(%d, %d, %p, %p)\n", major, minor, serv, client));
    sysORLastChange = ((struct sysORTable*)(serv))->OR_uptime;

    while ((value = (sysORTable_entry*)ITERATOR_NEXT(it)) && value->data != serv);
    ITERATOR_RELEASE(it);
    if(value) {
	CONTAINER_REMOVE(table, value);
	free(value);
    }
    return SNMP_ERR_NOERROR;
}

/** handles requests for the sysORTable table */
static int
sysORTable_handler(netsnmp_mib_handler *handler,
                   netsnmp_handler_registration *reginfo,
                   netsnmp_agent_request_info *reqinfo,
                   netsnmp_request_info *requests)
{
    netsnmp_request_info *request;

    DEBUGMSGTL(("mibII/sysORTable/sysORTable_handler",
                "sysORTable_handler called\n"));

    if (reqinfo->mode != MODE_GET) {
	snmp_log(LOG_ERR,
		 "Got unexpected operation for sysORTable\n");
	return SNMP_ERR_GENERR;
    }

    /*
     * Read-support (also covers GetNext requests)
     */
    request = requests;
    while(request && request->processed)
	request = request->next;
    while(request) {
	sysORTable_entry *table_entry;
	netsnmp_table_request_info *table_info;

	if (NULL == (table_info = netsnmp_extract_table_info(request))) {
	    snmp_log(LOG_ERR,
		     "could not extract table info for sysORTable\n");
	    snmp_set_var_typed_value(
		    request->requestvb, SNMP_ERR_GENERR, NULL, 0);
	} else if(NULL == (table_entry = (sysORTable_entry *)
			   netsnmp_container_table_extract_context(request))) {
	    switch (table_info->colnum) {
	    case COLUMN_SYSORID:
	    case COLUMN_SYSORDESCR:
	    case COLUMN_SYSORUPTIME:
		netsnmp_set_request_error(reqinfo, request,
					  SNMP_NOSUCHINSTANCE);
		break;
	    default:
		netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
		break;
	    }
	} else {
	    switch (table_info->colnum) {
	    case COLUMN_SYSORID:
		snmp_set_var_typed_value(
			request->requestvb, ASN_OBJECT_ID,
			(const u_char*)table_entry->data->OR_oid,
                        table_entry->data->OR_oidlen * sizeof(oid));
		break;
	    case COLUMN_SYSORDESCR:
		snmp_set_var_typed_value(
			request->requestvb, ASN_OCTET_STR,
			(const u_char*)table_entry->data->OR_descr,
			strlen(table_entry->data->OR_descr));
		break;
	    case COLUMN_SYSORUPTIME:
		snmp_set_var_typed_integer(
			request->requestvb, ASN_TIMETICKS,
                        table_entry->data->OR_uptime);
		break;
	    default:
		netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
		break;
	    }
	}
	do {
	    request = request->next;
	} while(request && request->processed);
    }
    return SNMP_ERR_NOERROR;
}

#ifdef USING_MIBII_SYSTEM_MIB_MODULE
extern oid      system_module_oid[];
extern int      system_module_oid_len;
extern int      system_module_count;
#endif

static netsnmp_handler_registration *sysORLastChange_reg;
static netsnmp_watcher_info sysORLastChange_winfo;
static netsnmp_handler_registration *sysORTable_reg;
static netsnmp_table_registration_info *sysORTable_table_info;

/** Initializes the sysORTable module */
void
init_sysORTable(void)
{
    const oid sysORLastChange_oid[] = { 1, 3, 6, 1, 2, 1, 1, 8 };
    const oid sysORTable_oid[] = { 1, 3, 6, 1, 2, 1, 1, 9 };

    sysORTable_table_info =
        SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);

    table = netsnmp_container_find("sysORTable:table_container");

    if (sysORTable_table_info == NULL || table == NULL) {
        SNMP_FREE(sysORTable_table_info);
        CONTAINER_FREE(table);
        return;
    }
    table->container_name = strdup("sysORTable");

    netsnmp_table_helper_add_indexes(sysORTable_table_info,
                                     ASN_INTEGER, /** index: sysORIndex */
                                     0);
    sysORTable_table_info->min_column = COLUMN_SYSORID;
    sysORTable_table_info->max_column = COLUMN_SYSORUPTIME;

    sysORLastChange_reg =
        netsnmp_create_handler_registration(
            "mibII/sysORLastChange", NULL,
            sysORLastChange_oid, OID_LENGTH(sysORLastChange_oid),
            HANDLER_CAN_RONLY);
    netsnmp_init_watcher_info(
	    &sysORLastChange_winfo,
            &sysORLastChange, sizeof(u_long),
            ASN_TIMETICKS, WATCHER_FIXED_SIZE);
    netsnmp_register_watched_scalar(sysORLastChange_reg,
				    &sysORLastChange_winfo);

    sysORTable_reg =
        netsnmp_create_handler_registration(
            "mibII/sysORTable", sysORTable_handler,
            sysORTable_oid, OID_LENGTH(sysORTable_oid), HANDLER_CAN_RONLY);
    netsnmp_container_table_register(sysORTable_reg, sysORTable_table_info,
                                     table, TABLE_CONTAINER_KEY_NETSNMP_INDEX);

    sysORLastChange = netsnmp_get_agent_uptime();

    /*
     * Initialise the contents of the table here
     */
    netsnmp_sysORTable_foreach(&register_foreach, NULL);

    /*
     * Register callbacks
     */
    snmp_register_callback(SNMP_CALLBACK_APPLICATION,
                           SNMPD_CALLBACK_REG_SYSOR, register_cb, NULL);
    snmp_register_callback(SNMP_CALLBACK_APPLICATION,
                           SNMPD_CALLBACK_UNREG_SYSOR, unregister_cb, NULL);

#ifdef USING_MIBII_SYSTEM_MIB_MODULE
    if (++system_module_count == 3)
        REGISTER_SYSOR_TABLE(system_module_oid, system_module_oid_len,
                             "The MIB module for SNMPv2 entities");
#endif
}

void
shutdown_sysORTable(void)
{
#ifdef USING_MIBII_SYSTEM_MIB_MODULE
    if (system_module_count-- == 3)
        UNREGISTER_SYSOR_TABLE(system_module_oid, system_module_oid_len);
#endif

    snmp_unregister_callback(SNMP_CALLBACK_APPLICATION,
                             SNMPD_CALLBACK_UNREG_SYSOR, unregister_cb, NULL,
                             1);
    snmp_unregister_callback(SNMP_CALLBACK_APPLICATION,
                             SNMPD_CALLBACK_REG_SYSOR, register_cb, NULL, 1);

    if (table)
        CONTAINER_CLEAR(table, netsnmp_container_simple_free, NULL);
    netsnmp_container_table_unregister(sysORTable_reg);
    sysORTable_reg = NULL;
    table = NULL;
    netsnmp_table_registration_info_free(sysORTable_table_info);
    sysORTable_table_info = NULL;
    netsnmp_unregister_handler(sysORLastChange_reg);
    sysORLastChange_reg = NULL;
}
