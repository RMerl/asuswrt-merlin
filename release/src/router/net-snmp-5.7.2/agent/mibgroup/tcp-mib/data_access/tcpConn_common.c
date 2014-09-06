/*
 *  TcpConn MIB architecture support
 *
 * $Id$
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/tcpConn.h>

#include "tcp-mib/tcpConnectionTable/tcpConnectionTable_constants.h"
#include "tcp-mib/data_access/tcpConn_private.h"

/**---------------------------------------------------------------------*/
/*
 * local static prototypes
 */
static void _access_tcpconn_entry_release(netsnmp_tcpconn_entry * entry,
                                            void *unused);

/**---------------------------------------------------------------------*/
/*
 * external per-architecture functions prototypes
 *
 * These shouldn't be called by the general public, so they aren't in
 * the header file.
 */
extern int
netsnmp_arch_tcpconn_container_load(netsnmp_container* container,
                                      u_int load_flags);
extern int
netsnmp_arch_tcpconn_entry_init(netsnmp_tcpconn_entry *entry);
extern int
netsnmp_arch_tcpconn_entry_copy(netsnmp_tcpconn_entry *lhs,
                                  netsnmp_tcpconn_entry *rhs);
extern void
netsnmp_arch_tcpconn_entry_cleanup(netsnmp_tcpconn_entry *entry);



/**---------------------------------------------------------------------*/
/*
 * container functions
 */
/**
 */
netsnmp_container *
netsnmp_access_tcpconn_container_init(u_int flags)
{
    netsnmp_container *container1;

    DEBUGMSGTL(("access:tcpconn:container", "init\n"));

    /*
     * create the container
     */
    container1 = netsnmp_container_find("access:tcpconn:table_container");
    if (NULL == container1) {
        snmp_log(LOG_ERR, "tcpconn primary container not found\n");
        return NULL;
    }
    container1->container_name = strdup("tcpConnTable");

    return container1;
}

/**
 * @retval NULL  error
 * @retval !NULL pointer to container
 */
netsnmp_container*
netsnmp_access_tcpconn_container_load(netsnmp_container* container, u_int load_flags)
{
    int rc;

    DEBUGMSGTL(("access:tcpconn:container", "load\n"));

    if (NULL == container)
        container = netsnmp_access_tcpconn_container_init(load_flags);
    if (NULL == container) {
        snmp_log(LOG_ERR, "no container specified/found for access_tcpconn\n");
        return NULL;
    }

    rc =  netsnmp_arch_tcpconn_container_load(container, load_flags);
    if (0 != rc) {
        netsnmp_access_tcpconn_container_free(container,
                                                NETSNMP_ACCESS_TCPCONN_FREE_NOFLAGS);
        container = NULL;
    }

    return container;
}

void
netsnmp_access_tcpconn_container_free(netsnmp_container *container, u_int free_flags)
{
    DEBUGMSGTL(("access:tcpconn:container", "free\n"));

    if (NULL == container) {
        snmp_log(LOG_ERR, "invalid container for netsnmp_access_tcpconn_free\n");
        return;
    }

    if(! (free_flags & NETSNMP_ACCESS_TCPCONN_FREE_DONT_CLEAR)) {
        /*
         * free all items.
         */
        CONTAINER_CLEAR(container,
                        (netsnmp_container_obj_func*)_access_tcpconn_entry_release,
                        NULL);
    }

    if(! (free_flags & NETSNMP_ACCESS_TCPCONN_FREE_KEEP_CONTAINER))
        CONTAINER_FREE(container);
}

/**---------------------------------------------------------------------*/
/*
 * tcpconn_entry functions
 */
/**
 */
/**
 */
netsnmp_tcpconn_entry *
netsnmp_access_tcpconn_entry_create(void)
{
    netsnmp_tcpconn_entry *entry =
        SNMP_MALLOC_TYPEDEF(netsnmp_tcpconn_entry);
    int rc = 0;

    DEBUGMSGTL(("verbose:access:tcpconn:entry", "create\n"));

    entry->oid_index.len = 1;
    entry->oid_index.oids = &entry->arbitrary_index;

    /*
     * init arch data
     */
    rc = netsnmp_arch_tcpconn_entry_init(entry);
    if (SNMP_ERR_NOERROR != rc) {
        DEBUGMSGT(("access:tcpconn:create","error %d in arch init\n", rc));
        netsnmp_access_tcpconn_entry_free(entry);
    }

    return entry;
}

/**
 */
void
netsnmp_access_tcpconn_entry_free(netsnmp_tcpconn_entry * entry)
{
    if (NULL == entry)
        return;

    DEBUGMSGTL(("verbose:access:tcpconn:entry", "free\n"));

    if (NULL != entry->arch_data)
        netsnmp_arch_tcpconn_entry_cleanup(entry);

    free(entry);
}

#ifdef TCPCONN_DELETE_SUPPORTED

/* XXX TODO: these are currently unsupported everywhere; to enable the
   functions first implement netsnmp_arch_tcpconn_entry_delete in the
   tcpConn_{OS}.c file and then define TCPCONN_DELETE_SUPPORTED in the
   tcpConn_{OS}.h file (which may need to be created first). */

/**
 * update underlying data store (kernel) for entry
 *
 * @retval  0 : success
 * @retval -1 : error
 */
int
netsnmp_access_tcpconn_entry_set(netsnmp_tcpconn_entry * entry)
{
    int rc = SNMP_ERR_NOERROR;

    if (NULL == entry) {
        netsnmp_assert(NULL != entry);
        return -1;
    }
    
    DEBUGMSGTL(("access:tcpconn:entry", "set\n"));

   
    /*
     * only option is delete
     */
    if (! (entry->flags & NETSNMP_ACCESS_TCPCONN_DELETE))
        return -1;
    
    rc = netsnmp_arch_tcpconn_entry_delete(entry);
    
    return rc;
}
#endif /* TCPCONN_DELETE_SUPPORTED */

/**
 * update an old tcpconn_entry from a new one
 *
 * @note: only mib related items are compared. Internal objects
 * such as oid_index, ns_ia_index and flags are not compared.
 *
 * @retval -1  : error
 * @retval >=0 : number of fileds updated
 */
int
netsnmp_access_tcpconn_entry_update(netsnmp_tcpconn_entry *lhs,
                                      netsnmp_tcpconn_entry *rhs)
{
    int rc, changed = 0;

    DEBUGMSGTL(("access:tcpconn:entry", "update\n"));

    if (lhs->tcpConnState != rhs->tcpConnState) {
        ++changed;
        lhs->tcpConnState = rhs->tcpConnState;
    }

    if (lhs->pid != rhs->pid) {
        ++changed;
        lhs->pid = rhs->pid;
    }

    /*
     * copy arch stuff. we don't care if it changed
     */
    rc = netsnmp_arch_tcpconn_entry_copy(lhs,rhs);
    if (0 != rc) {
        snmp_log(LOG_ERR,"arch tcpconn copy failed\n");
        return -1;
    }

    return changed;
}

/**---------------------------------------------------------------------*/
/*
 * Utility routines
 */

/**
 */
void
_access_tcpconn_entry_release(netsnmp_tcpconn_entry * entry, void *context)
{
    netsnmp_access_tcpconn_entry_free(entry);
}


#ifdef NETSNMP_TCPCONN_TEST

int
main(int argc, char** argv)
{
    netsnmp_container *container;

    netsnmp_config("debugTokens access:tcp,verbose:access:tcp,tcp,verbose:tcp");

    netsnmp_container_init_list();

    dodebug = 1;

    container = netsnmp_access_tcpconn_container_load(NULL, 0);
    
    netsnmp_access_tcpconn_container_free(container, 0);
}
#endif
