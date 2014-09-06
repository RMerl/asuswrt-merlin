/*
 * swinst_null.c:
 *     hrSWInstalledTable data access:
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
#include <net-snmp/data_access/swinst.h>

/* ---------------------------------------------------------------------
 */
void
netsnmp_swinst_arch_init(void)
{
    /* Nothing to do */
    return;
}

void
netsnmp_swinst_arch_shutdown(void)
{
    /* Nothing to do */
    return;
}

/* ---------------------------------------------------------------------
 */
int
netsnmp_swinst_arch_load( netsnmp_container *container, u_int flags)
{
    /* Nothing to do */
    DEBUGMSGTL(("swinst:load:arch"," loaded %" NETSNMP_PRIz "d entries\n",
                CONTAINER_SIZE(container)));

    return 0;
}
