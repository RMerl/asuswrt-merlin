/*
 * readdir() replacement for MSVC.
 */

#define WIN32IO_IS_STDIO

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/types.h>
#include <net-snmp/library/system.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <tchar.h>
#include <windows.h>


/*
 * Readdir just returns the current string pointer and bumps the
 * * string pointer to the nDllExport entry.
 */
struct direct  *
readdir(DIR * dirp)
{
    int             len;
    static int      dummy = 0;

    if (dirp->curr) {
        /*
         * first set up the structure to return 
         */
        len = strlen(dirp->curr);
        strcpy(dirp->dirstr.d_name, dirp->curr);
        dirp->dirstr.d_namlen = len;

        /*
         * Fake an inode 
         */
        dirp->dirstr.d_ino = dummy++;

        /*
         * Now set up for the nDllExport call to readdir 
         */
        dirp->curr += len + 1;
        if (dirp->curr >= (dirp->start + dirp->size)) {
            dirp->curr = NULL;
        }

        return &(dirp->dirstr);
    } else
        return NULL;
}
