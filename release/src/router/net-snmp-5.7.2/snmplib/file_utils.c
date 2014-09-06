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

#if HAVE_SYS_PARAM_H
#   include <sys/param.h>
#endif
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#include <errno.h>

#if HAVE_DMALLOC_H
#  include <dmalloc.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/library/container.h>
#include <net-snmp/library/file_utils.h>

netsnmp_feature_child_of(file_utils_all, libnetsnmp)
netsnmp_feature_child_of(file_utils, file_utils_all)
netsnmp_feature_child_of(file_close, file_utils_all)

#ifndef NETSNMP_FEATURE_REMOVE_FILE_UTILS
/*------------------------------------------------------------------
 *
 * Prototypes
 *
 */




/*------------------------------------------------------------------
 *
 * Core Functions
 *
 */

/**
 * allocate a netsnmp_file structure
 *
 * This routine should be used instead of allocating on the stack,
 * for future compatability.
 */
netsnmp_file *
netsnmp_file_create(void)
{
    netsnmp_file *filei = SNMP_MALLOC_TYPEDEF(netsnmp_file);

    /*
     * 0 is a valid file descriptor, so init to -1
     */
    if (NULL != filei)
        filei->fd = -1;
    else {
        snmp_log(LOG_WARNING,"failed to malloc netsnmp_file structure\n");
    }

    return filei;
}

/**
 * open file and get stats
 */
netsnmp_file *
netsnmp_file_new(const char *name, int fs_flags, mode_t mode, u_int ns_flags)
{
    netsnmp_file *filei = netsnmp_file_fill(NULL, name, fs_flags, mode, 0);
    if (NULL == filei)
        return NULL;

    if (ns_flags & NETSNMP_FILE_STATS) {
        filei->stats = (struct stat*)calloc(1, sizeof(*(filei->stats)));
        if (NULL == filei->stats)
            DEBUGMSGT(("nsfile:new", "no memory for stats\n"));
        else if (stat(name, filei->stats) != 0)
            DEBUGMSGT(("nsfile:new", "error getting stats\n"));
    }

    if (ns_flags & NETSNMP_FILE_AUTO_OPEN)
        netsnmp_file_open(filei);

    return filei;
}

        
/**
 * fill core members in a netsnmp_file structure
 *
 * @param filei      structure to fill; if NULL, a new one will be allocated
 * @param name       file name
 * @param fs_flags   filesystem flags passed to open()
 * @param mode       optional filesystem open modes passed to open()
 * @param ns_flags   net-snmp flags
 */
netsnmp_file *
netsnmp_file_fill(netsnmp_file * filei, const char* name,
                  int fs_flags, mode_t mode, u_int ns_flags)
{
    if (NULL == filei) {
        filei = netsnmp_file_create();
        if (NULL == filei) /* failure already logged */
            return NULL;
    }

    if (NULL != name)
        filei->name = strdup(name);

    filei->fs_flags = fs_flags;
    filei->ns_flags = ns_flags;
    filei->mode = mode;

    return filei;
}

/**
 * release a netsnmp_file structure
 *
 * @retval  see close() man page
 */
int
netsnmp_file_release(netsnmp_file * filei)
{
    int rc = 0;

    if (NULL == filei)
        return -1;

    if ((filei->fd > 0) && NS_FI_AUTOCLOSE(filei->ns_flags))
        rc = close(filei->fd);

    if (NULL != filei->name)
        free(filei->name); /* no point in SNMP_FREE */

    if (NULL != filei->extras)
        netsnmp_free_all_list_data(filei->extras);

    if (NULL != filei->stats)
        free(filei->stats);

    SNMP_FREE(filei);

    return rc;
}

/**
 * open the file associated with a netsnmp_file structure
 *
 * @retval -1  : error opening file
 * @retval >=0 : file descriptor for opened file
 */
int
netsnmp_file_open(netsnmp_file * filei)
{
    /*
     * basic sanity checks
     */
    if ((NULL == filei) || (NULL == filei->name))
        return -1;

    /*
     * if file is already open, just return the fd.
     */
    if (-1 != filei->fd)
        return filei->fd;

    /*
     * try to open the file, loging an error if we failed
     */
    if (0 == filei->mode)
        filei->fd = open(filei->name, filei->fs_flags);
    else
        filei->fd = open(filei->name, filei->fs_flags, filei->mode);

    if (filei->fd < 0) {
        DEBUGMSGTL(("netsnmp_file", "error opening %s (%d)\n", filei->name, errno));
    }

    /*
     * return results
     */
    return filei->fd;
}


/**
 * close the file associated with a netsnmp_file structure
 *
 * @retval  0 : success
 * @retval -1 : error
 */
#ifndef NETSNMP_FEATURE_REMOVE_FILE_CLOSE
int
netsnmp_file_close(netsnmp_file * filei)
{
    int rc;

    /*
     * basic sanity checks
     */
    if ((NULL == filei) || (NULL != filei->name))
        return -1;

    /*
     * make sure it's not already closed
     */
    if (-1 == filei->fd) {
        return 0;
    }

    /*
     * close the file, logging an error if we failed
     */
    rc = close(filei->fd);
    if (rc < 0) {
        DEBUGMSGTL(("netsnmp_file", "error closing %s (%d)\n", filei->name, errno));
    }
    else
        filei->fd = -1;

    return rc;
}
#endif /* NETSNMP_FEATURE_REMOVE_FILE_CLOSE */

void
netsnmp_file_container_free(netsnmp_file *file, void *context)
{
    netsnmp_file_release(file);
}

int
netsnmp_file_compare_name(netsnmp_file *lhs, netsnmp_file *rhs)
{
    netsnmp_assert((NULL != lhs) && (NULL != rhs));
    netsnmp_assert((NULL != lhs->name) && (NULL != rhs->name));

    return strcmp(lhs->name, rhs->name);
}
#else /* NETSNMP_FEATURE_REMOVE_FILE_UTILS */
netsnmp_feature_unused(file_utils);
#endif /* NETSNMP_FEATURE_REMOVE_FILE_UTILS */
