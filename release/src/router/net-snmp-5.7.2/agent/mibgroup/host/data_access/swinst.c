/*
 * swinst.c : hrSWInstalledTable data access
 */
/*
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/swinst.h>

#include <stdlib.h>
#include <unistd.h>

netsnmp_feature_child_of(software_installed, libnetsnmpmibs)

netsnmp_feature_child_of(swinst_entry_remove, netsnmp_unused)

/* ---------------------------------------------------------------------
 */

static void netsnmp_swinst_entry_free_cb(netsnmp_swinst_entry *, void *);

extern void netsnmp_swinst_arch_init(void);
extern void netsnmp_swinst_arch_shutdown(void);
extern int netsnmp_swinst_arch_load(netsnmp_container *, u_int);

void init_swinst( void )
{
    static int initialized = 0;

    DEBUGMSGTL(("swinst", "init called\n"));

    if (initialized)
        return; /* already initialized */


    /*
     * call arch init code
     */
    netsnmp_swinst_arch_init();
}

void shutdown_swinst( void )
{
    DEBUGMSGTL(("swinst", "shutdown called\n"));

    netsnmp_swinst_arch_shutdown();
}

/* ---------------------------------------------------------------------
 */

/*
 * load a container with installed software. If user_container is NULL,
 * a new container will be allocated and returned, and the caller
 * is responsible for releasing the allocated memory when done.
 *
 * if flags contains NETSNMP_SWINST_ALL_OR_NONE and any error occurs,
 * the container will be completely cleared.
 */
netsnmp_container *
netsnmp_swinst_container_load( netsnmp_container *user_container, int flags )
{
    netsnmp_container *container = user_container;
    int arch_rc;

    DEBUGMSGTL(("swinst:container", "load\n"));

    /*
     * create the container, if needed
     */
    if (NULL == container) {
        container = netsnmp_container_find("swinst:table_container");
        if (NULL == container)
            return NULL;
    }
    if (NULL == container->container_name)
        container->container_name = strdup("swinst container");

    /*
     * call the arch specific code to load the container
     */
    arch_rc = netsnmp_swinst_arch_load( container, flags );
    if (arch_rc && (flags & NETSNMP_SWINST_ALL_OR_NONE)) {
        /*
         * caller does not want a partial load, so empty the container.
         * if we created the container, destroy it.
         */
        netsnmp_swinst_container_free_items(container);
        if (container != user_container) {
            netsnmp_swinst_container_free(container, flags);
        }
    }
    
    return container;
}

void
netsnmp_swinst_container_free(netsnmp_container *container, u_int free_flags)
{
    DEBUGMSGTL(("swinst:container", "free\n"));

    if (NULL == container) {
        snmp_log(LOG_ERR,
                 "invalid container for netsnmp_swinst_container_free\n");
        return;
    }

    if(! (free_flags & NETSNMP_SWINST_DONT_FREE_ITEMS))
        netsnmp_swinst_container_free_items(container);

    CONTAINER_FREE(container);
}

/*
 * free a swinst container
 */
void netsnmp_swinst_container_free_items(netsnmp_container *container)
{
    DEBUGMSGTL(("swinst:container", "free_items\n"));

    if (NULL == container) {
        snmp_log(LOG_ERR,
                 "invalid container for netsnmp_swinst_container_free_items\n");
        return;
    }

    /*
     * free all items.
     */
    CONTAINER_CLEAR(container,
                    (netsnmp_container_obj_func*)netsnmp_swinst_entry_free_cb,
                    NULL);
}


/* ---------------------------------------------------------------------
 */

/*
 * create a new row in the table 
 */
netsnmp_swinst_entry *
netsnmp_swinst_entry_create(int32_t swIndex)
{
    netsnmp_swinst_entry *entry;

    entry = SNMP_MALLOC_TYPEDEF(netsnmp_swinst_entry);
    if (!entry)
        return NULL;

    entry->swIndex = swIndex;
    entry->oid_index.len = 1;
    entry->oid_index.oids = &entry->swIndex;

    entry->swType = HRSWINSTALLEDTYPE_APPLICATION;

    return entry;
}

/*
 * free a row
 */
void
netsnmp_swinst_entry_free(netsnmp_swinst_entry *entry)
{
    DEBUGMSGTL(("swinst:entry:free", "index %" NETSNMP_PRIo "u\n",
                entry->swIndex));

    free(entry);
}

/*
 * free a row
 */
static void
netsnmp_swinst_entry_free_cb(netsnmp_swinst_entry *entry, void *context)
{
    free(entry);
}

/*
 * remove a row from the table 
 */
#ifndef NETSNMP_FEATURE_REMOVE_SWINST_ENTRY_REMOVE
void
netsnmp_swinst_entry_remove(netsnmp_container * container,
                            netsnmp_swinst_entry *entry)
{
    DEBUGMSGTL(("swinst:container", "remove\n"));
    if (!entry)
        return;                 /* Nothing to remove */
    CONTAINER_REMOVE(container, entry);
}
#endif /* NETSNMP_FEATURE_REMOVE_SWINST_ENTRY_REMOVE */

/* ---------------------------------------------------------------------
 */

#ifdef TEST
int main(int argc, char *argv[])
{
    const char *tokens = getenv("SNMP_DEBUG");

    netsnmp_container_init_list();

    /** swinst,verbose:swinst */
    if (tokens)
        debug_register_tokens(tokens);
    else
        debug_register_tokens("swinst");
    snmp_set_do_debugging(1);

    init_swinst();
    netsnmp_swinst_container_load(NULL, 0);
    shutdown_swinst();

    return 0;
}
#endif
