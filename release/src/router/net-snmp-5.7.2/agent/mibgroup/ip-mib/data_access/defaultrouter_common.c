/*
 *  defaultrouter MIB architecture support
 *
 * $Id:$
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/defaultrouter.h>

#include "ip-mib/ipDefaultRouterTable/ipDefaultRouterTable.h"

/**---------------------------------------------------------------------*/
/*
 * local static prototypes
 */
static int _access_defaultrouter_entry_compare_addr(const void *lhs,
                                                    const void *rhs);
static void _access_defaultrouter_entry_release(netsnmp_defaultrouter_entry * entry,
                                                void *unused);

/**---------------------------------------------------------------------*/
/*
 * external per-architecture functions prototypes
 *
 * These shouldn't be called by the general public, so they aren't in
 * the header file.
 */
extern int
netsnmp_arch_defaultrouter_entry_init(netsnmp_defaultrouter_entry *entry);

extern int
netsnmp_arch_defaultrouter_container_load(netsnmp_container* container,
                                          u_int load_flags);

/**---------------------------------------------------------------------*/
/*
 * container functions
 */
/**
 */
netsnmp_container *
netsnmp_access_defaultrouter_container_init(u_int flags)
{
    netsnmp_container *container1;

    DEBUGMSGTL(("access:defaultrouter:container", "init\n"));

    /*
     * create the containers. one indexed by ifIndex, the other
     * indexed by ifName.
     */
    container1 = netsnmp_container_find("access_defaultrouter:table_container");
    if (NULL == container1) {
        snmp_log(LOG_ERR, "defaultrouter primary container is not found\n");
        return NULL;
    }
    container1->container_name = strdup("dr_index");

    if (flags & NETSNMP_ACCESS_DEFAULTROUTER_INIT_ADDL_IDX_BY_ADDR) {
        netsnmp_container *container2 =
            netsnmp_container_find("defaultrouter_addr:access_defaultrouter:table_container");
        if (NULL == container2) {
            snmp_log(LOG_ERR, "defaultrouter secondary container not found\n");
            CONTAINER_FREE(container1);
            return NULL;
        }

        container2->compare = _access_defaultrouter_entry_compare_addr;
        container2->container_name = strdup("dr_addr");

        netsnmp_container_add_index(container1, container2);
    }

    return container1;
}

/**
 * @retval NULL  error
 * @retval !NULL pointer to container
 */
netsnmp_container*
netsnmp_access_defaultrouter_container_load(netsnmp_container* container,
                                            u_int load_flags)
{
    int rc;
     u_int container_flags = 0;

    DEBUGMSGTL(("access:defaultrouter:container", "load\n"));

    if (NULL == container) {
        if (load_flags & NETSNMP_ACCESS_DEFAULTROUTER_LOAD_ADDL_IDX_BY_ADDR) {
            container_flags |=
                NETSNMP_ACCESS_DEFAULTROUTER_INIT_ADDL_IDX_BY_ADDR;
        }
        container =
            netsnmp_access_defaultrouter_container_init(container_flags);
    }

    if (NULL == container) {
        snmp_log(LOG_ERR, "no container specified/found for access_defaultrouter\n");
        return NULL;
    }

    rc =  netsnmp_arch_defaultrouter_container_load(container, load_flags);
    if (0 != rc) {
        netsnmp_access_defaultrouter_container_free(container,
                                    NETSNMP_ACCESS_DEFAULTROUTER_FREE_NOFLAGS);
        container = NULL;
    }

    return container;
}

void
netsnmp_access_defaultrouter_container_free(netsnmp_container *container,
                                            u_int free_flags)
{
    DEBUGMSGTL(("access:defaultrouter:container", "free\n"));

    if (NULL == container) {
        snmp_log(LOG_ERR,
                 "invalid container for netsnmp_access_defaultrouter_free\n");
        return;
    }

    if(! (free_flags & NETSNMP_ACCESS_DEFAULTROUTER_FREE_DONT_CLEAR)) {
        /*
         * free all items.
         */
        CONTAINER_CLEAR(container,
                        (netsnmp_container_obj_func*)_access_defaultrouter_entry_release,
                        NULL);
    }

    if(! (free_flags & NETSNMP_ACCESS_DEFAULTROUTER_FREE_KEEP_CONTAINER))
        CONTAINER_FREE(container);
}

/**---------------------------------------------------------------------*/
/*
 * defaultrouter_entry functions
 */
/**
 */
/**
 */
netsnmp_defaultrouter_entry *
netsnmp_access_defaultrouter_entry_create(void)
{
    int rc = 0;
    netsnmp_defaultrouter_entry *entry =
        SNMP_MALLOC_TYPEDEF(netsnmp_defaultrouter_entry);

    DEBUGMSGTL(("access:defaultrouter:entry", "create\n"));

    if(NULL == entry)
        return NULL;

    entry->oid_index.len = 1;
    entry->oid_index.oids = &entry->ns_dr_index;

    /*
     * set up defaults
     */
    entry->dr_lifetime   = IPDEFAULTROUTERLIFETIME_MAX;
    entry->dr_preference = IPDEFAULTROUTERPREFERENCE_MEDIUM;

    rc = netsnmp_arch_defaultrouter_entry_init(entry);
    if (SNMP_ERR_NOERROR != rc) {
        DEBUGMSGT(("access:defaultrouter:create","error %d in arch init\n", rc));
        netsnmp_access_defaultrouter_entry_free(entry);
        entry = NULL;
    }

    return entry;
}

void
netsnmp_access_defaultrouter_entry_free(netsnmp_defaultrouter_entry * entry)
{
    if (NULL == entry)
        return;

    free(entry);
}

/**
 * update an old defaultrouter_entry from a new one
 *
 * @note: only mib related items are compared. Internal objects
 * such as oid_index, ns_dr_index and flags are not compared.
 *
 * @retval -1  : error
 * @retval >=0 : number of fields updated
 */
int
netsnmp_access_defaultrouter_entry_update(netsnmp_defaultrouter_entry *lhs,
                                      netsnmp_defaultrouter_entry *rhs)
{
    int changed = 0;

    if (lhs->dr_addresstype != rhs->dr_addresstype) {
        ++changed;
        lhs->dr_addresstype = rhs->dr_addresstype;
    }

    if (lhs->dr_address_len != rhs->dr_address_len) {
        changed += 2;
        lhs->dr_address_len = rhs->dr_address_len;
        memcpy(lhs->dr_address, rhs->dr_address, rhs->dr_address_len);
    }
    else if (memcmp(lhs->dr_address, rhs->dr_address, rhs->dr_address_len) != 0) {
        ++changed;
        memcpy(lhs->dr_address, rhs->dr_address, rhs->dr_address_len);
    }

    if (lhs->dr_if_index != rhs->dr_if_index) {
        ++changed;
        lhs->dr_if_index = rhs->dr_if_index;
    }

    if (lhs->dr_lifetime != rhs->dr_lifetime) {
        ++changed;
        lhs->dr_lifetime = rhs->dr_lifetime;
    }

    if (lhs->dr_preference != rhs->dr_preference) {
        ++changed;
        lhs->dr_preference = rhs->dr_preference;
    }

    return changed;
}

/**
 * copy an  defaultrouter_entry
 *
 * @retval -1  : error
 * @retval 0   : no error
 */
int
netsnmp_access_defaultrouter_entry_copy(netsnmp_defaultrouter_entry *lhs,
                                    netsnmp_defaultrouter_entry *rhs)
{
    lhs->dr_addresstype = rhs->dr_addresstype;
    lhs->dr_address_len = rhs->dr_address_len;
    memcpy(lhs->dr_address, rhs->dr_address, rhs->dr_address_len);
    lhs->dr_if_index    = rhs->dr_if_index;
    lhs->dr_lifetime    = rhs->dr_lifetime;
    lhs->dr_preference  = rhs->dr_preference;

    return 0;
}

/**---------------------------------------------------------------------*/
/*
 * Utility routines
 */

/**
 */
void
_access_defaultrouter_entry_release(netsnmp_defaultrouter_entry * entry, void *context)
{
    netsnmp_access_defaultrouter_entry_free(entry);
}

static int _access_defaultrouter_entry_compare_addr(const void *lhs,
                                                const void *rhs)
{
    const netsnmp_defaultrouter_entry *lh = (const netsnmp_defaultrouter_entry *)lhs;
    const netsnmp_defaultrouter_entry *rh = (const netsnmp_defaultrouter_entry *)rhs;

    netsnmp_assert(NULL != lhs);
    netsnmp_assert(NULL != rhs);

    /*
     * compare address length
     */
    if (lh->dr_address_len < rh->dr_address_len)
        return -1;
    else if (lh->dr_address_len > rh->dr_address_len)
        return 1;

    /*
     * length equal, compare address
     */
    return memcmp(lh->dr_address, rh->dr_address, lh->dr_address_len);
}
