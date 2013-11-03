#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <atalk/compat.h>

#if !defined HAVE_DIRFD && defined SOLARIS
#include <dirent.h>
int dirfd(DIR *dir)
{
    return dir->d_fd;
}
#endif

#ifndef HAVE_STRNLEN
size_t strnlen(const char *s, size_t max)
{
    size_t len;
  
    for (len = 0; len < max; len++) {
        if (s[len] == '\0') {
            break;
        }
    }
    return len;  
}
#endif
