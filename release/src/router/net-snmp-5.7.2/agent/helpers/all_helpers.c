/** @name handler
 *  @{ */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/debug_handler.h>
#include <net-snmp/agent/serialize.h>
#include <net-snmp/agent/read_only.h>
#include <net-snmp/agent/bulk_to_next.h>
#include <net-snmp/agent/table_dataset.h>
#include <net-snmp/agent/stash_cache.h>

netsnmp_feature_child_of(mib_helpers, libnetsnmpagent)


/** call the initialization sequence for all handlers with init_ routines. */
void
netsnmp_init_helpers(void)
{
    netsnmp_init_debug_helper();
    netsnmp_init_serialize();
    netsnmp_init_read_only_helper();
    netsnmp_init_bulk_to_next_helper();
#ifndef NETSNMP_FEATURE_REMOVE_TABLE_DATASET
    netsnmp_init_table_dataset();
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_DATASET */

#ifndef NETSNMP_FEATURE_REMOVE_ROW_MERGE
    netsnmp_init_row_merge();
#endif /* NETSNMP_FEATURE_REMOVE_ROW_MERGE */
#ifndef NETSNMP_FEATURE_REMOVE_STASH_CACHE
    netsnmp_init_stash_cache_helper();
#endif /* NETSNMP_FEATURE_REMOVE_STASH_CACHE */
}

/** @defgroup utilities utility_handlers
 *  Simplify request processing
 *  A group of handlers intended to simplify certain aspects of processing
 *  a request for a MIB object.  These helpers do not implement any MIB
 *  objects themselves.  Rather they handle specific generic situations,
 *  either returning an error, or passing a (possibly simpler) request
 *  down to lower level handlers.
 *  @ingroup handler
 */

/** @defgroup leaf leaf_handlers
 *  Process individual leaf objects
 *  A group of handlers to implement individual leaf objects and instances
 *  (both scalar objects, and individual objects and instances within a table).
 *  These handlers will typically allow control to be passed down to a lower
 *  level, user-provided handler, but this is (usually) optional.
 *  @ingroup handler
 */

/** @} */
