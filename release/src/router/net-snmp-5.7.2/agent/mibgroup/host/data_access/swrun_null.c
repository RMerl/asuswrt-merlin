/*
 * swrun_null.c:
 *     hrSWRunTable data access:
 *     dummy interface for non-supported systems
 */
#include <net-snmp/net-snmp-config.h>

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/container.h>
#include <net-snmp/library/snmp_debug.h>
#include <net-snmp/data_access/swrun.h>

/* ---------------------------------------------------------------------
 */
void
netsnmp_arch_swrun_init(void)
{
    /* Nothing to do */
    return;
}

/* ---------------------------------------------------------------------
 */
int
netsnmp_arch_swrun_container_load( netsnmp_container *container, u_int flags)
{
    /* Nothing to do */
    DEBUGMSGTL(("swrun:load:arch"," loaded %d entries\n",
                CONTAINER_SIZE(container)));

    return 0;
}
