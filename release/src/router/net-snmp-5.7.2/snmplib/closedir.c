/*
 * closedir() replacement for MSVC.
 */

#define WIN32IO_IS_STDIO

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/types.h>
#include <net-snmp/library/system.h>

/*
 * free the memory allocated by opendir 
 */
int
closedir(DIR * dirp)
{
    free(dirp->start);
    free(dirp);
    return 1;
}
