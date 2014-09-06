/*
 *  Arp MIB architecture support
 *
 * $Id$
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/arp.h>

/**---------------------------------------------------------------------*/
/*
 * arp_entry functions
 */
/**
 */
netsnmp_arp_entry *
netsnmp_access_arp_entry_create(void)
{
    netsnmp_arp_entry *entry =
        SNMP_MALLOC_TYPEDEF(netsnmp_arp_entry);

    if (NULL == entry)
        return NULL;

    entry->oid_index.len = 1;
    entry->oid_index.oids = &entry->ns_arp_index;

    return entry;
}

/**
 */
void
netsnmp_access_arp_entry_free(netsnmp_arp_entry * entry)
{
    free(entry);
}

/**---------------------------------------------------------------------*/
/*
 * Utility routines
 */

/**
 * Update given entry with new data. Calculate new arp_last_updated, if any
 * field is changed.
 */
void netsnmp_access_arp_entry_update(netsnmp_arp_entry *entry,
        netsnmp_arp_entry *new_data)
{
    int modified = 0;

    entry->generation = new_data->generation;
    if (entry->arp_ipaddress_len != new_data->arp_ipaddress_len
            || memcmp(entry->arp_ipaddress, new_data->arp_ipaddress, entry->arp_ipaddress_len) != 0 ) {
        modified = 1;
        entry->arp_ipaddress_len = new_data->arp_ipaddress_len;
        memcpy(entry->arp_ipaddress, new_data->arp_ipaddress, sizeof(entry->arp_ipaddress));
    }
    if (entry->arp_physaddress_len != new_data->arp_physaddress_len ||
            memcmp(entry->arp_physaddress, new_data->arp_physaddress, entry->arp_physaddress_len) != 0) {
         modified = 1;
         entry->arp_physaddress_len = new_data->arp_physaddress_len;
         memcpy(entry->arp_physaddress, new_data->arp_physaddress, sizeof(entry->arp_physaddress_len));
     }
    if (entry->arp_state != new_data->arp_state) {
         modified = 1;
         entry->arp_state = new_data->arp_state;
     }
    if (entry->arp_type != new_data->arp_type) {
         modified = 1;
         entry->arp_type = new_data->arp_type;
     }
    if (entry->flags != new_data->flags) {
         modified = 1;
         entry->flags = new_data->flags;
     }

    if (modified)
        entry->arp_last_updated = netsnmp_get_agent_uptime();
}
