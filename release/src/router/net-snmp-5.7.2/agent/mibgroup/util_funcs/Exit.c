/*
 * Portions of this file are copyrighted by:
 * Copyright Copyright 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

#include <net-snmp/net-snmp-config.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <net-snmp/library/snmp_logging.h>

#include "Exit.h"

void
Exit(int var)
{
    snmp_log(LOG_ERR, "Server Exiting with code %d\n", var);
    exit(var);
}
