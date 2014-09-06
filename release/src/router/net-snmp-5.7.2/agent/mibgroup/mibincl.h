/*
 * mibincl.h
 */

#include <stdio.h>
#include <sys/types.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_MALLOC_H
#include <malloc.h>
#endif
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <net-snmp/agent/mib_module_config.h>

#include <net-snmp/library/asn1.h>
#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/snmp_impl.h>
#include <net-snmp/library/snmp_client.h>

#include <net-snmp/agent/snmp_vars.h>
#include <net-snmp/agent/agent_read_config.h>
#include <net-snmp/agent/agent_handler.h>
#include <net-snmp/agent/agent_registry.h>
#include <net-snmp/agent/var_struct.h>

#include <net-snmp/library/snmp.h>
#include <net-snmp/library/mib.h>
#include <net-snmp/library/snmp_debug.h>
#include <net-snmp/library/snmp_logging.h>
#include <net-snmp/library/snmp_alarm.h>
#include <net-snmp/library/read_config.h>
#include <net-snmp/library/tools.h>
#include <net-snmp/agent/agent_trap.h>
#include <net-snmp/library/callback.h>
#define u_char unsigned char
#define u_short unsigned short
