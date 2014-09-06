#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "sctpTables_common.h"
#include "sctpAssocTable.h"
#include "sctpAssocRemAddrTable.h"
#include "sctpAssocLocalAddrTable.h"
#include "sctpLookupLocalPortTable.h"
#include "sctpLookupRemPortTable.h"
#include "sctpLookupRemHostNameTable.h"
#include "sctpLookupRemPrimIPAddrTable.h"
#include "sctpLookupRemIPAddrTable.h"

netsnmp_feature_require(container_lifo)

static void
sctpAssocTable_collect_invalid(void *what, void *magic)
{
    sctpAssocTable_entry *entry = what;
    netsnmp_container *to_delete = magic;

    if (entry->valid)
        entry->valid = 0;
    else
        CONTAINER_INSERT(to_delete, entry);
}

/*
 * Remove all entries from sctpAssocTable, which are not marked as valid.
 * All valid entries are then marked as invalid (to delete them in next cache
 * load, if the entry is not updated). 
 */
void
sctpAssocTable_delete_invalid(netsnmp_container *assocTable)
{
    netsnmp_container *to_delete = netsnmp_container_find("lifo");

    CONTAINER_FOR_EACH(assocTable, sctpAssocTable_collect_invalid,
                       to_delete);

    while (CONTAINER_SIZE(to_delete)) {
        sctpAssocTable_entry *entry = CONTAINER_FIRST(to_delete);
        CONTAINER_REMOVE(assocTable, entry);
        sctpAssocTable_entry_free(entry);
        CONTAINER_REMOVE(to_delete, NULL);
    }
    CONTAINER_FREE(to_delete);
}

static void
sctpAssocRemAddrTable_collect_invalid(void *what, void *magic)
{
    sctpAssocRemAddrTable_entry *entry = what;
    netsnmp_container *to_delete = magic;

    if (entry->valid)
        entry->valid = 0;
    else
        CONTAINER_INSERT(to_delete, entry);
}

/*
 * Remove all entries from sctpAssocRemAddrTable, which are not marked as valid.
 * All valid entries are then marked as invalid (to delete them in next cache
 * load, if the entry is not updated). 
 */
void
sctpAssocRemAddrTable_delete_invalid(netsnmp_container *remAddrTable)
{
    netsnmp_container *to_delete = netsnmp_container_find("lifo");

    CONTAINER_FOR_EACH(remAddrTable, sctpAssocRemAddrTable_collect_invalid,
                       to_delete);

    while (CONTAINER_SIZE(to_delete)) {
        sctpAssocRemAddrTable_entry *entry = CONTAINER_FIRST(to_delete);
        CONTAINER_REMOVE(remAddrTable, entry);
        sctpAssocRemAddrTable_entry_free(entry);
        CONTAINER_REMOVE(to_delete, NULL);
    }
    CONTAINER_FREE(to_delete);
}

static void
sctpAssocLocalAddrTable_collect_invalid(void *what, void *magic)
{
    sctpAssocLocalAddrTable_entry *entry = what;
    netsnmp_container *to_delete = magic;

    if (entry->valid)
        entry->valid = 0;
    else
        CONTAINER_INSERT(to_delete, entry);
}

/*
 * Remove all entries from sctpAssocLocalAddrTable, which are not marked as valid.
 * All valid entries are then marked as invalid (to delete them in next cache
 * load, if the entry is not updated). 
 */
void
sctpAssocLocalAddrTable_delete_invalid(netsnmp_container *localAddrTable)
{
    netsnmp_container *to_delete = netsnmp_container_find("lifo");

    CONTAINER_FOR_EACH(localAddrTable,
                       sctpAssocLocalAddrTable_collect_invalid, to_delete);

    while (CONTAINER_SIZE(to_delete)) {
        sctpAssocLocalAddrTable_entry *entry = CONTAINER_FIRST(to_delete);
        CONTAINER_REMOVE(localAddrTable, entry);
        sctpAssocLocalAddrTable_entry_free(entry);
        CONTAINER_REMOVE(to_delete, NULL);
    }
    CONTAINER_FREE(to_delete);
}


int
sctpAssocRemAddrTable_add_or_update(netsnmp_container *remAddrTable,
                                    sctpAssocRemAddrTable_entry * entry)
{
    /*
     * we have full sctpAssocLocalAddrTable entry, update or add it in the container 
     */
    sctpAssocRemAddrTable_entry *old;

    entry->valid = 1;

    /*
     * try to find it in the container 
     */
    sctpAssocRemAddrTable_entry_update_index(entry);
    old = CONTAINER_FIND(remAddrTable, entry);

    if (old != NULL) {
        /*
         * update existing entry, don't overwrite the timestamp 
         */
        time_t          timestamp = old->sctpAssocRemAddrStartTime;
        if (timestamp == 0 && entry->sctpAssocRemAddrStartTime == 0)
            timestamp = netsnmp_get_agent_uptime();     /* set the timestamp if it was not set before */
        sctpAssocRemAddrTable_entry_copy(entry, old);
        old->sctpAssocRemAddrStartTime = timestamp;
        sctpAssocRemAddrTable_entry_free(entry);

    } else {
        /*
         * the entry is new, add it there 
         */
        if (entry->sctpAssocRemAddrStartTime == 0) {
            entry->sctpAssocRemAddrStartTime = netsnmp_get_agent_uptime();
        }
        CONTAINER_INSERT(remAddrTable, entry);
    }

    return SNMP_ERR_NOERROR;
}

int
sctpAssocLocalAddrTable_add_or_update(netsnmp_container *localAddrTable,
                                      sctpAssocLocalAddrTable_entry *
                                      entry)
{
    /*
     * we have full sctpAssocLocalAddrTable entry, update or add it in the container 
     */
    sctpAssocLocalAddrTable_entry *old;

    entry->valid = 1;

    /*
     * try to find it in the container 
     */
    sctpAssocLocalAddrTable_entry_update_index(entry);
    old = CONTAINER_FIND(localAddrTable, entry);

    if (old != NULL) {
        /*
         * update existing entry, don't overwrite the timestamp
         */
        time_t          timestamp = old->sctpAssocLocalAddrStartTime;
        if (timestamp == 0 && entry->sctpAssocLocalAddrStartTime == 0)
            timestamp = netsnmp_get_agent_uptime();     /* set the timestamp if it was not set before */
        sctpAssocLocalAddrTable_entry_copy(entry, old);
        old->sctpAssocLocalAddrStartTime = timestamp;
        sctpAssocLocalAddrTable_entry_free(entry);

    } else {
        /*
         * the entry is new, add it there 
         */
        if (entry->sctpAssocLocalAddrStartTime == 0) {
            entry->sctpAssocLocalAddrStartTime =
                netsnmp_get_agent_uptime();
        }
        CONTAINER_INSERT(localAddrTable, entry);
    }

    return SNMP_ERR_NOERROR;
}

int
sctpAssocTable_add_or_update(netsnmp_container *assocTable,
                             sctpAssocTable_entry * entry)
{
    /*
     * we have full sctpAssocTable entry, update or add it in the container 
     */
    sctpAssocTable_entry *old;

    entry->valid = 1;

    /*
     * try to find it in the container 
     */
    sctpAssocTable_entry_update_index(entry);
    old = CONTAINER_FIND(assocTable, entry);

    if (old != NULL) {
        /*
         * update existing entry, don't overwrite the timestamp
         */
        time_t          timestamp = old->sctpAssocStartTime;
        if (timestamp == 0 && entry->sctpAssocStartTime == 0
            && entry->sctpAssocState >= SCTPASSOCSTATE_ESTABLISHED)
            timestamp = netsnmp_get_agent_uptime();     /* set the timestamp if it was not set before and entry reaches the right state */
        sctpAssocTable_entry_copy(entry, old);
        old->sctpAssocStartTime = timestamp;
        sctpAssocTable_entry_free(entry);

    } else {
        /*
         * the entry is new, add it there 
         */
        if (entry->sctpAssocStartTime == 0
            && entry->sctpAssocState >= SCTPASSOCSTATE_ESTABLISHED) {
            entry->sctpAssocStartTime = netsnmp_get_agent_uptime();
        }
        CONTAINER_INSERT(assocTable, entry);
    }

    return SNMP_ERR_NOERROR;
}

static void
sctpTables_add_local_port(sctpAssocTable_entry * assoc,
                          sctpTables_containers * containers)
{
    sctpLookupLocalPortTable_entry *entry;

    entry = sctpLookupLocalPortTable_entry_create();
    if (entry == NULL) {
        DEBUGMSGTL(("sctp:tables:fill_lookup",
                    "cannot create sctpLookupLocalPortTable_entry"));
        return;
    }

    entry->sctpAssocId = assoc->sctpAssocId;
    entry->sctpAssocLocalPort = assoc->sctpAssocLocalPort;
    entry->sctpLookupLocalPortStartTime = assoc->sctpAssocStartTime;
    sctpLookupLocalPortTable_entry_update_index(entry);
    CONTAINER_INSERT(containers->sctpLookupLocalPortTable, entry);
}

static void
sctpTables_add_rem_port(sctpAssocTable_entry * assoc,
                        sctpTables_containers * containers)
{
    sctpLookupRemPortTable_entry *entry;

    entry = sctpLookupRemPortTable_entry_create();
    if (entry == NULL) {
        DEBUGMSGTL(("sctp:tables:fill_lookup",
                    "cannot create sctpLookupRemPortTable_entry"));
        return;
    }

    entry->sctpAssocId = assoc->sctpAssocId;
    entry->sctpAssocRemPort = assoc->sctpAssocRemPort;
    entry->sctpLookupRemPortStartTime = assoc->sctpAssocStartTime;
    sctpLookupRemPortTable_entry_update_index(entry);
    CONTAINER_INSERT(containers->sctpLookupRemPortTable, entry);
}

static void
sctpTables_add_rem_hostname(sctpAssocTable_entry * assoc,
                            sctpTables_containers * containers)
{
    sctpLookupRemHostNameTable_entry *entry;

    if (assoc->sctpAssocRemHostName_len == 0)
        return;                 /* the association does not know its hostname */

    entry = sctpLookupRemHostNameTable_entry_create();
    if (entry == NULL) {
        DEBUGMSGTL(("sctp:tables:fill_lookup",
                    "cannot create sctpLookupRemHostNameTable_entry"));
        return;
    }

    entry->sctpAssocId = assoc->sctpAssocId;
    entry->sctpAssocRemHostName_len = assoc->sctpAssocRemHostName_len;
    memcpy(entry->sctpAssocRemHostName, assoc->sctpAssocRemHostName,
           assoc->sctpAssocRemHostName_len);
    entry->sctpLookupRemHostNameStartTime = assoc->sctpAssocStartTime;

    sctpLookupRemHostNameTable_entry_update_index(entry);
    CONTAINER_INSERT(containers->sctpLookupRemHostNameTable, entry);
}

static void
sctpTables_add_rem_prim_ip_addr(sctpAssocTable_entry * assoc,
                                sctpTables_containers * containers)
{
    sctpLookupRemPrimIPAddrTable_entry *entry;

    entry = sctpLookupRemPrimIPAddrTable_entry_create();
    if (entry == NULL) {
        DEBUGMSGTL(("sctp:tables:fill_lookup",
                    "cannot create sctpLookupRemPrimIPAddrTable_entry"));
        return;
    }

    entry->sctpAssocId = assoc->sctpAssocId;
    entry->sctpAssocRemPrimAddrType = assoc->sctpAssocRemPrimAddrType;
    entry->sctpAssocRemPrimAddr_len = assoc->sctpAssocRemPrimAddr_len;
    memcpy(entry->sctpAssocRemPrimAddr, assoc->sctpAssocRemPrimAddr,
           assoc->sctpAssocRemPrimAddr_len);
    entry->sctpLookupRemPrimIPAddrStartTime = assoc->sctpAssocStartTime;

    sctpLookupRemPrimIPAddrTable_entry_update_index(entry);
    CONTAINER_INSERT(containers->sctpLookupRemPrimIPAddrTable, entry);
}

static void
sctpTables_fill_lookup_assoc(void *what, void *magic)
{
    sctpAssocTable_entry *entry = what;
    sctpTables_containers *containers = magic;

    sctpTables_add_local_port(entry, containers);
    sctpTables_add_rem_port(entry, containers);
    sctpTables_add_rem_hostname(entry, containers);
    sctpTables_add_rem_prim_ip_addr(entry, containers);
}

static void
sctpTables_add_rem_ip_addr(sctpAssocRemAddrTable_entry * rem_addr,
                           sctpTables_containers * containers)
{
    sctpLookupRemIPAddrTable_entry *entry;

    entry = sctpLookupRemIPAddrTable_entry_create();
    if (entry == NULL) {
        DEBUGMSGTL(("sctp:tables:fill_lookup",
                    "cannot create sctpLookupRemIPAddrTable_entry"));
        return;
    }

    entry->sctpAssocId = rem_addr->sctpAssocId;
    entry->sctpAssocRemAddrType = rem_addr->sctpAssocRemAddrType;
    entry->sctpAssocRemAddr_len = rem_addr->sctpAssocRemAddr_len;
    memcpy(entry->sctpAssocRemAddr, rem_addr->sctpAssocRemAddr,
           rem_addr->sctpAssocRemAddr_len);
    entry->sctpLookupRemIPAddrStartTime =
        rem_addr->sctpAssocRemAddrStartTime;

    sctpLookupRemIPAddrTable_entry_update_index(entry);
    CONTAINER_INSERT(containers->sctpLookupRemIPAddrTable, entry);
}

static void
sctpTables_fill_lookup_rem_addr(void *what, void *magic)
{
    sctpAssocRemAddrTable_entry *entry = what;
    sctpTables_containers *containers = magic;
    sctpTables_add_rem_ip_addr(entry, containers);
}

int
sctpTables_fill_lookup(sctpTables_containers * containers)
{
    /*
     * clear all the lookup tables 
     */
    sctpLookupLocalPortTable_container_clear(containers->
                                             sctpLookupLocalPortTable);
    sctpLookupRemPortTable_container_clear(containers->
                                           sctpLookupRemPortTable);
    sctpLookupRemHostNameTable_container_clear(containers->
                                               sctpLookupRemHostNameTable);
    sctpLookupRemPrimIPAddrTable_container_clear(containers->
                                                 sctpLookupRemPrimIPAddrTable);
    sctpLookupRemIPAddrTable_container_clear(containers->
                                             sctpLookupRemIPAddrTable);

    /*
     * fill the lookup tables 
     */
    CONTAINER_FOR_EACH(containers->sctpAssocTable,
                       sctpTables_fill_lookup_assoc, containers);
    CONTAINER_FOR_EACH(containers->sctpAssocRemAddrTable,
                       sctpTables_fill_lookup_rem_addr, containers);

    return SNMP_ERR_NOERROR;
}



int
sctpTables_load(void)
{
    sctpTables_containers containers;
    int             ret;
    u_long          flags = 0;

    containers.sctpAssocTable = sctpAssocTable_get_container();
    containers.sctpAssocRemAddrTable =
        sctpAssocRemAddrTable_get_container();
    containers.sctpAssocLocalAddrTable =
        sctpAssocLocalAddrTable_get_container();
    containers.sctpLookupLocalPortTable =
        sctpLookupLocalPortTable_get_container();
    containers.sctpLookupRemPortTable =
        sctpLookupRemPortTable_get_container();
    containers.sctpLookupRemHostNameTable =
        sctpLookupRemHostNameTable_get_container();
    containers.sctpLookupRemPrimIPAddrTable =
        sctpLookupRemPrimIPAddrTable_get_container();
    containers.sctpLookupRemIPAddrTable =
        sctpLookupRemIPAddrTable_get_container();

    ret = sctpTables_arch_load(&containers, &flags);

    if (flags & SCTP_TABLES_LOAD_FLAG_DELETE_INVALID) {
        sctpAssocTable_delete_invalid(containers.sctpAssocTable);
        sctpAssocRemAddrTable_delete_invalid(containers.
                                             sctpAssocRemAddrTable);
        sctpAssocLocalAddrTable_delete_invalid(containers.
                                               sctpAssocLocalAddrTable);
    }

    if (flags & SCTP_TABLES_LOAD_FLAG_AUTO_LOOKUP) {
        ret = sctpTables_fill_lookup(&containers);
    }

    return ret;
}
