/* dirutil.c ... directory utilities.
 *               C. Scott Ananian <cananian@alumni.princeton.edu>
 *
 * $Id: dirutil.c,v 1.2 2003/06/17 17:25:47 reink Exp $
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "dirutil.h"

/* Returned malloc'ed string representing basename */
char *basenamex(char *pathname)
{
    char *dup = strdup(pathname);
    char *ptr = strrchr(stripslash(dup), '/');
    if (ptr == NULL) return dup;
    ptr = strdup(ptr+1);
    free(dup);
    return ptr;
}

/* Return malloc'ed string representing directory name (no trailing slash) */
char *dirname(char *pathname)
{
    char *dup = strdup(pathname);
    char *ptr = strrchr(stripslash(dup), '/');
    if (ptr == NULL) { free(dup); return strdup("."); }
    if (ptr == dup && dup[0] == '/') ptr++;
    *ptr = '\0';
    return dup;
}

/* In-place modify a string to remove trailing slashes.  Returns arg.
 * stripslash("/") returns "/";
 */
char *stripslash(char *pathname) {
    int len = strlen(pathname);
    while (len > 1 && pathname[len - 1] == '/')
        pathname[--len] = '\0';
    return pathname;
}

/* ensure dirname exists, creating it if necessary. */
int make_valid_path(char *dir, mode_t mode)
{
    struct stat st;
    char *tmp = NULL, *path = stripslash(strdup(dir));
    int retval;
    if (stat(path, &st) == 0) { /* file exists */
        if (S_ISDIR(st.st_mode)) { retval = 1; goto end; }
        else { retval = 0; goto end; } /* not a directory.  Oops. */
    }
    /* Directory doesn't exist.  Let's make it. */
    /*   Make parent first. */
    if (!make_valid_path(tmp = dirname(path), mode)) { retval = 0; goto end; }
    /*   Now make this 'un. */
    if (mkdir(path, mode) < 0) { retval = 0; goto end; }
    /* Success. */
    retval = 1;

end:
    if (tmp != NULL) free(tmp);
    if (path != NULL) free(path);
    return retval;
}
