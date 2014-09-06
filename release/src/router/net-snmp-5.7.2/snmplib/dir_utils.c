/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>

#include <stdio.h>
#include <ctype.h>
#if HAVE_STDLIB_H
#   include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#   include <unistd.h>
#endif
#if HAVE_STRING_H
#   include <string.h>
#else
#  include <strings.h>
#endif

#include <sys/types.h>
#if HAVE_LIMITS_H
#include <limits.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#include <errno.h>

#if HAVE_DMALLOC_H
#  include <dmalloc.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/library/container.h>
#include <net-snmp/library/file_utils.h>
#include <net-snmp/library/dir_utils.h>

netsnmp_feature_child_of(container_directory, container_types)
#ifdef NETSNMP_FEATURE_REQUIRE_CONTAINER_DIRECTORY
netsnmp_feature_require(file_utils)
netsnmp_feature_require(container_free_all)
#endif /* NETSNMP_FEATURE_REQUIRE_CONTAINER_DIRECTORY */

#ifndef NETSNMP_FEATURE_REMOVE_CONTAINER_DIRECTORY
static int
_insert_nsfile( netsnmp_container *c, const char *name, struct stat *stats,
                u_int flags);

/*
 * read file names in a directory, with an optional filter
 */
netsnmp_container *
netsnmp_directory_container_read_some(netsnmp_container *user_container,
                                      const char *dirname,
                                      netsnmp_directory_filter *filter,
                                      void *filter_ctx, u_int flags)
{
    DIR               *dir;
    netsnmp_container *container = user_container;
    struct dirent     *file;
    char               path[SNMP_MAXPATH];
    size_t             dirname_len;
    int                rc;
    struct stat        statbuf;
    netsnmp_file       ns_file_tmp;

    if ((flags & NETSNMP_DIR_RELATIVE_PATH) && (flags & NETSNMP_DIR_RECURSE)) {
        DEBUGMSGTL(("directory:container",
                    "no support for relative path with recursion\n"));
        return NULL;
    }

    DEBUGMSGTL(("directory:container", "reading %s\n", dirname));

    /*
     * create the container, if needed
     */
    if (NULL == container) {
        if (flags & NETSNMP_DIR_NSFILE) {
            container = netsnmp_container_find("nsfile_directory_container:"
                                               "binary_array");
            if (container) {
                container->compare = (netsnmp_container_compare*)
                    netsnmp_file_compare_name;
                container->free_item = (netsnmp_container_obj_func *)
                    netsnmp_file_container_free;
            }
        }
        else
            container = netsnmp_container_find("directory_container:cstring");
        if (NULL == container)
            return NULL;
        container->container_name = strdup(dirname);
        /** default to unsorted */
        if (! (flags & NETSNMP_DIR_SORTED))
            CONTAINER_SET_OPTIONS(container, CONTAINER_KEY_UNSORTED, rc);
    }

    dir = opendir(dirname);
    if (NULL == dir) {
        DEBUGMSGTL(("directory:container", "  not a dir\n"));
        if (container != user_container)
            netsnmp_directory_container_free(container);
        return NULL;
    }

    /** copy dirname into path */
    if (flags & NETSNMP_DIR_RELATIVE_PATH)
        dirname_len = 0;
    else {
        dirname_len = strlen(dirname);
        strlcpy(path, dirname, sizeof(path));
        if ((dirname_len + 2) > sizeof(path)) {
            /** not enough room for files */
            closedir(dir);
            if (container != user_container)
                netsnmp_directory_container_free(container);
            return NULL;
        }
        path[dirname_len] = '/';
        path[++dirname_len] = '\0';
    }

    /** iterate over dir */
    while ((file = readdir(dir))) {

        if ((file->d_name == NULL) || (file->d_name[0] == 0))
            continue;

        /** skip '.' and '..' */
        if ((file->d_name[0] == '.') &&
            ((file->d_name[1] == 0) ||
             ((file->d_name[1] == '.') && ((file->d_name[2] == 0)))))
            continue;

        strlcpy(&path[dirname_len], file->d_name, sizeof(path) - dirname_len);
        if (NULL != filter) {
            if (flags & NETSNMP_DIR_NSFILE_STATS) {
                /** use local vars for now */
                if (stat(path, &statbuf) != 0) {
                    snmp_log(LOG_ERR, "could not stat %s\n", file->d_name);
                    break;
                }
                ns_file_tmp.stats = &statbuf;
                ns_file_tmp.name = path;
                rc = (*filter)(&ns_file_tmp, filter_ctx);
            }
            else
                rc = (*filter)(path, filter_ctx);
            if (0 == rc) {
                DEBUGMSGTL(("directory:container:filtered", "%s\n",
                            file->d_name));
                continue;
            }
        }

        DEBUGMSGTL(("directory:container:found", "%s\n", path));
        if ((flags & NETSNMP_DIR_RECURSE) 
#if defined(HAVE_STRUCT_DIRENT_D_TYPE) && defined(DT_DIR)
            && (file->d_type == DT_DIR)
#elif defined(S_ISDIR)
            && (stat(file->d_name, &statbuf) != 0) && (S_ISDIR(statbuf.st_mode))
#endif
            ) {
            /** xxx add the dir as well? not for now.. maybe another flag? */
            netsnmp_directory_container_read(container, path, flags);
        }
        else if (flags & NETSNMP_DIR_NSFILE) {
            if (_insert_nsfile( container, file->d_name,
                                filter ? &statbuf : NULL, flags ) < 0)
                break;
        }
        else {
            char *dup = strdup(path);
            if (NULL == dup) {
                snmp_log(LOG_ERR,
                         "strdup failed while building directory container\n");
                break;
            }
            rc = CONTAINER_INSERT(container, dup);
            if (-1 == rc ) {
                DEBUGMSGTL(("directory:container", "  err adding %s\n", path));
                free(dup);
            }
        }
    } /* while */

    closedir(dir);

    rc = CONTAINER_SIZE(container);
    DEBUGMSGTL(("directory:container", "  container now has %d items\n", rc));
    if ((0 == rc) && !(flags & NETSNMP_DIR_EMPTY_OK)) {
        netsnmp_directory_container_free(container);
        return NULL;
    }
    
    return container;
}

void
netsnmp_directory_container_free(netsnmp_container *container)
{
    CONTAINER_FREE_ALL(container, NULL);
    CONTAINER_FREE(container);
}

static int
_insert_nsfile( netsnmp_container *c, const char *name, struct stat *stats,
                u_int flags)
{
    int           rc;
    netsnmp_file *ns_file = netsnmp_file_new(name, 0, 0, 0);
    if (NULL == ns_file) {
        snmp_log(LOG_ERR, "error creating ns_file\n");
        return -1;
    }

    if (flags & NETSNMP_DIR_NSFILE_STATS) {
        ns_file->stats = (struct stat*)calloc(1,sizeof(*(ns_file->stats)));
        if (NULL == ns_file->stats) {
            snmp_log(LOG_ERR, "error creating stats for ns_file\n");
            netsnmp_file_release(ns_file);
            return -1;
        }
    
        /** use stats from earlier if we have them */
        if (stats)
            memcpy(ns_file->stats, stats, sizeof(*stats));
        else
            stat(ns_file->name, ns_file->stats);
    }

    rc = CONTAINER_INSERT(c, ns_file);
    if (-1 == rc ) {
        DEBUGMSGTL(("directory:container", "  err adding %s\n", name));
        netsnmp_file_release(ns_file);
    }

    return 0;
}
#else  /* NETSNMP_FEATURE_REMOVE_CONTAINER_DIRECTORY */
netsnmp_feature_unused(container_directory);
#endif /* NETSNMP_FEATURE_REMOVE_CONTAINER_DIRECTORY */
