/*
 *  ipv6ScopeIndexTable MIB architecture support
 *
 * $Id: ipv6scopezone_common.c 14170 2007-04-29 02:22:12Z varun_c $
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/scopezone.h>
/*
 * local static prototypes
 */
static void _entry_release(netsnmp_v6scopezone_entry * entry, void *unused);


/**---------------------------------------------------------------------*/
/*
 * external per-architecture functions prototypes
 *
 * These shouldn't be called by the general public, so they aren't in
 * the header file.
 */
extern int
netsnmp_access_scopezone_container_arch_load(netsnmp_container* container,
                                             u_int load_flags);
extern void
netsnmp_access_scopezone_arch_init(void);

/**
 * initialize systemstats container
 */
netsnmp_container *
netsnmp_access_scopezone_container_init(u_int flags)
{
    netsnmp_container *container;

    DEBUGMSGTL(("access:scopezone:container", "init\n"));
    /*
     * create the containers. one indexed by ifIndex, the other
     * indexed by ifName.
     */
    container = netsnmp_container_find("access_scopezone:table_container");
    if (NULL == container)
        return NULL;

    return container;
}

/**
 * load scopezone information in specified container
 *
 * @param container empty container, or NULL to have one created for you
 * @param load_flags flags to modify behaviour.
 *
 * @retval NULL  error
 * @retval !NULL pointer to container
 */
netsnmp_container*
netsnmp_access_scopezone_container_load(netsnmp_container* container, u_int load_flags)
{
    int rc;

    DEBUGMSGTL(("access:scopezone:container", "load\n"));

    if (NULL == container) {
        container = netsnmp_access_scopezone_container_init(load_flags);
        if (container)
            container->container_name = strdup("scopezone");
    }
    if (NULL == container) {
        snmp_log(LOG_ERR, "no container specified/found for access_scopezone\n");
        return NULL;
    }

    rc =  netsnmp_access_scopezone_container_arch_load(container, load_flags);
    if (0 != rc) {
        netsnmp_access_scopezone_container_free(container,
                                                NETSNMP_ACCESS_SCOPEZONE_FREE_NOFLAGS);
        container = NULL;
    }

    return container;
}

void
netsnmp_access_scopezone_container_free(netsnmp_container *container, u_int free_flags)
{
    DEBUGMSGTL(("access:scopezone:container", "free\n"));

    if (NULL == container) {
        snmp_log(LOG_ERR, "invalid container for netsnmp_access_scopezone_free\n");
        return;
    }

    if(! (free_flags & NETSNMP_ACCESS_SCOPEZONE_FREE_DONT_CLEAR)) {
        /*
         * free all items.
         */
        CONTAINER_CLEAR(container,
                        (netsnmp_container_obj_func*)_entry_release,
                        NULL);
    }

    CONTAINER_FREE(container);
}

/**
 */
netsnmp_v6scopezone_entry *
netsnmp_access_scopezone_entry_create(void)
{
    netsnmp_v6scopezone_entry *entry =
        SNMP_MALLOC_TYPEDEF(netsnmp_v6scopezone_entry);

    DEBUGMSGTL(("access:scopezone:entry", "create\n"));

    if(NULL == entry)
        return NULL;


    entry->oid_index.len = 1;
    entry->oid_index.oids =  &entry->ns_scopezone_index;

    return entry;
}

/**
 */
void
netsnmp_access_scopezone_entry_free(netsnmp_v6scopezone_entry * entry)
{
    DEBUGMSGTL(("access:scopezone:entry", "free\n"));

    if (NULL == entry)
        return;


    free(entry);
}

/**---------------------------------------------------------------------*/
/*
 * Utility routines
 */

/**
 * \internal
 */
static void
_entry_release(netsnmp_v6scopezone_entry * entry, void *context)
{
    netsnmp_access_scopezone_entry_free(entry);
}

