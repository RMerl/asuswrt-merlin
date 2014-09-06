/*
 * opendir() replacement for MSVC.
 */

#define WIN32IO_IS_STDIO
#define PATHLEN	1024

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/types.h>
#include <net-snmp/library/system.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <tchar.h>
#include <windows.h>


/*
 * The idea here is to read all the directory names into a string table
 * * (separated by nulls) and when one of the other dir functions is called
 * * return the pointer to the current file name.
 */
DIR            *
opendir(const char *filename)
{
    DIR            *p;
    long            len;
    long            idx;
    char            scannamespc[PATHLEN];
    char           *scanname = scannamespc;
    struct stat     sbuf;
    WIN32_FIND_DATA FindData;
    HANDLE          fh;

    /*
     * check to see if filename is a directory 
     */
    if ((stat(filename, &sbuf) < 0) || ((sbuf.st_mode & S_IFDIR) == 0)) {
        return NULL;
    }

    /*
     * get the file system characteristics 
     */
    /*
     * if(GetFullPathName(filename, SNMP_MAXPATH, root, &dummy)) {
     * *    if(dummy = strchr(root, '\\'))
     * *        *++dummy = '\0';
     * *    if(GetVolumeInformation(root, volname, SNMP_MAXPATH, &serial,
     * *                            &maxname, &flags, 0, 0)) {
     * *        downcase = !(flags & FS_CASE_IS_PRESERVED);
     * *    }
     * *  }
     * *  else {
     * *    downcase = TRUE;
     * *  }
     */

    /*
     * Create the search pattern 
     */
    strcpy(scanname, filename);

    if (strchr("/\\", *(scanname + strlen(scanname) - 1)) == NULL)
        strcat(scanname, "/*");
    else
        strcat(scanname, "*");

    /*
     * do the FindFirstFile call 
     */
    fh = FindFirstFile(scanname, &FindData);
    if (fh == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    /*
     * Get us a DIR structure 
     */
    p = (DIR *) malloc(sizeof(DIR));
    /*
     * Newz(1303, p, 1, DIR); 
     */
    if (p == NULL)
        return NULL;

    /*
     * now allocate the first part of the string table for
     * * the filenames that we find.
     */
    idx = strlen(FindData.cFileName) + 1;
    p->start = (char *) malloc(idx);
    /*
     * New(1304, p->start, idx, char);
     */
    if (p->start == NULL) {
        free(p);
        return NULL;
    }
    strcpy(p->start, FindData.cFileName);
    /*
     * if(downcase)
     * *    strlwr(p->start);
     */
    p->nfiles = 0;

    /*
     * loop finding all the files that match the wildcard
     * * (which should be all of them in this directory!).
     * * the variable idx should point one past the null terminator
     * * of the previous string found.
     */
    while (FindNextFile(fh, &FindData)) {
        len = strlen(FindData.cFileName);
        /*
         * bump the string table size by enough for the
         * * new name and it's null terminator
         */
        p->start = (char *) realloc((void *) p->start, idx + len + 1);
        /*
         * Renew(p->start, idx+len+1, char);
         */
        if (p->start == NULL) {
            free(p);
            return NULL;
        }
        strcpy(&p->start[idx], FindData.cFileName);
        /*
         * if (downcase) 
         * *        strlwr(&p->start[idx]);
         */
        p->nfiles++;
        idx += len + 1;
    }
    FindClose(fh);
    p->size = idx;
    p->curr = p->start;
    return p;
}
