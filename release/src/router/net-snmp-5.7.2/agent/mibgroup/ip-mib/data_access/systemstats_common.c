/*
 *  ipSystemStatsTable and ipIfStatsTable MIB architecture support
 *
 * $Id$
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include "ip-mib/ipSystemStatsTable/ipSystemStatsTable_constants.h"

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/ipstats.h>
#include <net-snmp/data_access/systemstats.h>

/**---------------------------------------------------------------------*/
/*
 * local static vars
 */
static int need_wrap_check = -1;

/*
 * local static prototypes
 */
static void _entry_release(netsnmp_systemstats_entry * entry, void *unused);

/**---------------------------------------------------------------------*/
/*
 * external per-architecture functions prototypes
 *
 * These shouldn't be called by the general public, so they aren't in
 * the header file.
 */
extern int
netsnmp_access_systemstats_container_arch_load(netsnmp_container* container,
                                             u_int load_flags);
extern void
netsnmp_access_systemstats_arch_init(void);

/**---------------------------------------------------------------------*/
/*
 * initialization
 */
void
netsnmp_access_systemstats_init(void)
{
    netsnmp_container * ifcontainer;

    netsnmp_access_systemstats_arch_init();

    /*
     * load once to set up ifIndexes
     */
    ifcontainer = netsnmp_access_systemstats_container_load(NULL, 0);
    if(NULL != ifcontainer)
        netsnmp_access_systemstats_container_free(ifcontainer, 0);

}

/**---------------------------------------------------------------------*/
/*
 * container functions
 */
/**
 * initialize systemstats container
 */
netsnmp_container *
netsnmp_access_systemstats_container_init(u_int flags)
{
    netsnmp_container *container;

    DEBUGMSGTL(("access:systemstats:container", "init\n"));

    /*
     * create the containers. one indexed by ifIndex, the other
     * indexed by ifName.
     */
    container = netsnmp_container_find("access_systemstats:table_container");
    if (NULL == container)
        return NULL;

    return container;
}

/**
 * load systemstats information in specified container
 *
 * @param container empty container, or NULL to have one created for you
 * @param load_flags flags to modify behaviour.
 *
 * @retval NULL  error
 * @retval !NULL pointer to container
 */
netsnmp_container*
netsnmp_access_systemstats_container_load(netsnmp_container* container, u_int load_flags)
{
    int rc;

    DEBUGMSGTL(("access:systemstats:container", "load\n"));

    if (NULL == container) {
        container = netsnmp_access_systemstats_container_init(load_flags);
        if (NULL != container)
            container->container_name = strdup("systemstats_autocreate");
    }
    if (NULL == container) {
        snmp_log(LOG_ERR, "no container specified/found for access_systemstats\n");
        return NULL;
    }

    rc =  netsnmp_access_systemstats_container_arch_load(container, load_flags);
    if (0 != rc) {
        netsnmp_access_systemstats_container_free(container,
                                                NETSNMP_ACCESS_SYSTEMSTATS_FREE_NOFLAGS);
        container = NULL;
    }

    return container;
}

void
netsnmp_access_systemstats_container_free(netsnmp_container *container, u_int free_flags)
{
    DEBUGMSGTL(("access:systemstats:container", "free\n"));

    if (NULL == container) {
        snmp_log(LOG_ERR, "invalid container for netsnmp_access_systemstats_free\n");
        return;
    }

    if(! (free_flags & NETSNMP_ACCESS_SYSTEMSTATS_FREE_DONT_CLEAR)) {
        /*
         * free all items.
         */
        CONTAINER_CLEAR(container,
                        (netsnmp_container_obj_func*)_entry_release,
                        NULL);
    }

    CONTAINER_FREE(container);
}

/**---------------------------------------------------------------------*/
/*
 * entry functions
 */
/**
 */
netsnmp_systemstats_entry *
netsnmp_access_systemstats_entry_get_by_index(netsnmp_container *container, oid index)
{
    netsnmp_index   tmp;

    DEBUGMSGTL(("access:systemstats:entry", "by_index\n"));

    if (NULL == container) {
        snmp_log(LOG_ERR,
                 "invalid container for netsnmp_access_systemstats_entry_get_by_index\n");
        return NULL;
    }

    tmp.len = 1;
    tmp.oids = &index;

    return (netsnmp_systemstats_entry *) CONTAINER_FIND(container, &tmp);
}

/**
 */
netsnmp_systemstats_entry *
netsnmp_access_systemstats_entry_create(int version, int if_index,
        const char *tableName)
{
    netsnmp_systemstats_entry *entry =
        SNMP_MALLOC_TYPEDEF(netsnmp_systemstats_entry);

    DEBUGMSGTL(("access:systemstats:entry", "create\n"));

    if(NULL == entry)
        return NULL;

    entry->oid_index.len = 2;
    entry->oid_index.oids = entry->index;
    entry->index[0] = version;
    entry->index[1] = if_index;
    entry->tableName = tableName;
    return entry;
}

/**
 */
void
netsnmp_access_systemstats_entry_free(netsnmp_systemstats_entry * entry)
{
    DEBUGMSGTL(("access:systemstats:entry", "free\n"));

    if (NULL == entry)
        return;

    /*
     * SNMP_FREE not needed, for any of these, 
     * since the whole entry is about to be freed
     */

    if (NULL != entry->old_stats)
        free(entry->old_stats);

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
_entry_release(netsnmp_systemstats_entry * entry, void *context)
{
    netsnmp_access_systemstats_entry_free(entry);
}

/*
 * Calculates the entries, which are not provided by OS, but can be 
 * computed from the others.
 */
static void
_calculate_entries(netsnmp_systemstats_entry * entry)
{
    /*
     * HCInForwDatagrams = HCInNoRoutes + HCOutForwDatagrams
     */
    if (!entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINFORWDATAGRAMS]
        && entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFORWDATAGRAMS]
        && entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINNOROUTES]) {
        
        entry->stats.HCInForwDatagrams = entry->stats.HCInNoRoutes;
        u64Incr(&entry->stats.HCInForwDatagrams, &entry->stats.HCOutForwDatagrams);
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINFORWDATAGRAMS] = 1;
    }

    /*
     * HCOutFragReqds = HCOutFragOKs + HCOutFragFails
     */
    if (!entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFRAGREQDS]
        && entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFRAGOKS]
           && entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFRAGFAILS]) {
        
        entry->stats.HCOutFragReqds = entry->stats.HCOutFragOKs;
        u64Incr(&entry->stats.HCOutFragReqds, &entry->stats.HCOutFragFails);
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFRAGREQDS] = 1;
    }
    
    /*
     * HCOutTransmits = HCOutRequests  + HCOutForwDatagrams + HCOutFragCreates  
     *                  - HCOutFragReqds - HCOutNoRoutes  - HCOutDiscards
     */
    if (!entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTTRANSMITS]
        && entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTREQUESTS]
        && entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFORWDATAGRAMS]
           && entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFRAGREQDS]
           && entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTNOROUTES]
           && entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFRAGCREATES]
        && entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTDISCARDS]) {

        U64 tmp, tmp2, tmp3;
        tmp = entry->stats.HCOutRequests;
        u64Incr(&tmp, &entry->stats.HCOutForwDatagrams);
        u64Incr(&tmp, &entry->stats.HCOutFragCreates);
        
        u64Subtract(&tmp, &entry->stats.HCOutFragReqds, &tmp2);
        u64Subtract(&tmp2, &entry->stats.HCOutNoRoutes, &tmp3);
        u64Subtract(&tmp3, &entry->stats.HCOutDiscards, &entry->stats.HCOutTransmits);
                
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTTRANSMITS] = 1;
    }
}

/**
 * update entry stats (checking for counter wrap)
 *
 * @retval  0 : success
 * @retval <0 : error
 */
int
netsnmp_access_systemstats_entry_update_stats(netsnmp_systemstats_entry * prev_vals,
                                              netsnmp_systemstats_entry * new_vals)
{
    DEBUGMSGTL(("access:systemstats", "check_wrap\n"));
    
    /*
     * sanity checks
     */
    if ((NULL == prev_vals) || (NULL == new_vals) ||
        (prev_vals->index[0] != new_vals->index[0])
        || (prev_vals->index[1] != new_vals->index[1]))
        return -1;

    /*
     * if we've determined that we have 64 bit counters, just copy them.
     */
    if (0 == need_wrap_check) {
        memcpy(&prev_vals->stats, &new_vals->stats, sizeof(new_vals->stats));
        _calculate_entries(prev_vals);
        return 0;
    }

    if (NULL == prev_vals->old_stats) {
        /*
         * if we don't have old stats, they can't have wrapped, so just copy
         */
        prev_vals->old_stats = SNMP_MALLOC_TYPEDEF(netsnmp_ipstats);
        if (NULL == prev_vals->old_stats) {
            return -2;
        }
        memcpy(&prev_vals->stats, &new_vals->stats, sizeof(new_vals->stats));
    }
    else {
        /*
         * update straight 32 bit counters
         */
        memcpy(&prev_vals->stats.columnAvail[0], &new_vals->stats.columnAvail[0], sizeof(new_vals->stats.columnAvail));
        prev_vals->stats.InHdrErrors = new_vals->stats.InHdrErrors;
        prev_vals->stats.InAddrErrors = new_vals->stats.InAddrErrors;
        prev_vals->stats.InUnknownProtos = new_vals->stats.InUnknownProtos;
        prev_vals->stats.InTruncatedPkts = new_vals->stats.InTruncatedPkts;
        prev_vals->stats.ReasmReqds = new_vals->stats.ReasmReqds;
        prev_vals->stats.ReasmOKs = new_vals->stats.ReasmOKs;
        prev_vals->stats.ReasmFails = new_vals->stats.ReasmFails;
        prev_vals->stats.InDiscards = new_vals->stats.InDiscards;

        /*
         * update 64bit counters
         */
        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINNOROUTES])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCInNoRoutes,
                                       &new_vals->stats.HCInNoRoutes,
                                       &prev_vals->old_stats->HCInNoRoutes,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCInNoRoutes to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTNOROUTES])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCOutNoRoutes,
                                       &new_vals->stats.HCOutNoRoutes,
                                       &prev_vals->old_stats->HCOutNoRoutes,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCOutNoRoutes to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTDISCARDS])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCOutDiscards,
                                       &new_vals->stats.HCOutDiscards,
                                       &prev_vals->old_stats->HCOutDiscards,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCOutDiscards to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFRAGREQDS])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCOutFragReqds,
                                       &new_vals->stats.HCOutFragReqds,
                                       &prev_vals->old_stats->HCOutFragReqds,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCOutFragReqds to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFRAGOKS])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCOutFragOKs,
                                       &new_vals->stats.HCOutFragOKs,
                                       &prev_vals->old_stats->HCOutFragOKs,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCOutFragOKs to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFRAGFAILS])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCOutFragFails,
                                       &new_vals->stats.HCOutFragFails,
                                       &prev_vals->old_stats->HCOutFragFails,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCOutFragFails to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFRAGCREATES])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCOutFragCreates,
                                       &new_vals->stats.HCOutFragCreates,
                                       &prev_vals->old_stats->HCOutFragCreates,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCOutFragCreates to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINRECEIVES])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCInReceives,
                                       &new_vals->stats.HCInReceives,
                                       &prev_vals->old_stats->HCInReceives,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCInReceives to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINOCTETS])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCInOctets,
                                       &new_vals->stats.HCInOctets,
                                       &prev_vals->old_stats->HCInOctets,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCInOctets to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINFORWDATAGRAMS])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCInForwDatagrams,
                                       &new_vals->stats.HCInForwDatagrams,
                                       &prev_vals->old_stats->HCInForwDatagrams,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCInForwDatagrams to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINDELIVERS])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCInDelivers,
                                       &new_vals->stats.HCInDelivers,
                                       &prev_vals->old_stats->HCInDelivers,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCInDelivers to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTREQUESTS])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCOutRequests,
                                       &new_vals->stats.HCOutRequests,
                                       &prev_vals->old_stats->HCOutRequests,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCOutRequests to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFORWDATAGRAMS])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCOutForwDatagrams,
                                       &new_vals->stats.HCOutForwDatagrams,
                                       &prev_vals->old_stats->HCOutForwDatagrams,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCOutForwDatagrams to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTTRANSMITS])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCOutTransmits,
                                       &new_vals->stats.HCOutTransmits,
                                       &prev_vals->old_stats->HCOutTransmits,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCOutTransmits to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTOCTETS])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCOutOctets,
                                       &new_vals->stats.HCOutOctets,
                                       &prev_vals->old_stats->HCOutOctets,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCOutOctets to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINMCASTPKTS])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCInMcastPkts,
                                       &new_vals->stats.HCInMcastPkts,
                                       &prev_vals->old_stats->HCInMcastPkts,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCInMcastPkts to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINMCASTOCTETS])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCInMcastOctets,
                                       &new_vals->stats.HCInMcastOctets,
                                       &prev_vals->old_stats->HCInMcastOctets,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCInMcastOctets to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTMCASTPKTS])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCOutMcastPkts,
                                       &new_vals->stats.HCOutMcastPkts,
                                       &prev_vals->old_stats->HCOutMcastPkts,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCOutMcastPkts to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTMCASTOCTETS])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCOutMcastOctets,
                                       &new_vals->stats.HCOutMcastOctets,
                                       &prev_vals->old_stats->HCOutMcastOctets,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCOutMcastOctets to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINBCASTPKTS])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCInBcastPkts,
                                       &new_vals->stats.HCInBcastPkts,
                                       &prev_vals->old_stats->HCInBcastPkts,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCInBcastPkts to 64bits in %s\n",
                        prev_vals->tableName));

        if (new_vals->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTBCASTPKTS])
            if (0 != netsnmp_c64_check32_and_update(
                                       &prev_vals->stats.HCOutBcastPkts,
                                       &new_vals->stats.HCOutBcastPkts,
                                       &prev_vals->old_stats->HCOutBcastPkts,
                                       &need_wrap_check))
                DEBUGMSGTL(("access:systemstats",
                        "Error expanding HCOutBcastPkts to 64bits in %s\n",
                        prev_vals->tableName));
    }

    /*
     * if we've decided we no longer need to check wraps, free old stats
     */
    if (0 == need_wrap_check) {
        SNMP_FREE(prev_vals->old_stats);
    } else {
        /*
         * update old stats from new stats.
         * careful - old_stats is a pointer to stats...
         */
        memcpy(prev_vals->old_stats, &new_vals->stats, sizeof(new_vals->stats));
    }

    _calculate_entries(prev_vals);

    return 0;
}

/**
 * update systemstats entry data (checking for counter wraps)
 *
 * Given an existing entry, update it with the new values from another
 * entry.
 *
 * @retval -2 : malloc failed
 * @retval -1 : systemstatss not the same
 * @retval  0 : no error
 */
int
netsnmp_access_systemstats_entry_update(netsnmp_systemstats_entry * lhs,
                                        netsnmp_systemstats_entry * rhs)
{
    DEBUGMSGTL(("access:systemstats", "copy\n"));
    
    if ((NULL == lhs) || (NULL == rhs) ||
        (lhs->index[0] != rhs->index[0])
        || (lhs->index[1] != rhs->index[1]))
        return -1;

    /*
     * update stats
     */
    netsnmp_access_systemstats_entry_update_stats(lhs, rhs);

    /*
     * update other data
     */
    lhs->flags = rhs->flags;
    
    return 0;
}

/**---------------------------------------------------------------------*/
/*
 *
 */
