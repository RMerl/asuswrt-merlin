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
#ifndef NETSNMP_DIR_UTILS_H
#define NETSNMP_DIR_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

    /*
     * filter function; return 1 to include file, 0 to exclude
     */
#define NETSNMP_DIR_EXCLUDE 0
#define NETSNMP_DIR_INCLUDE 1
    typedef int (netsnmp_directory_filter)(const void *text, void *ctx);


    /*------------------------------------------------------------------
     *
     * Prototypes
     */
    netsnmp_container *
    netsnmp_directory_container_read_some(netsnmp_container *user_container,
                                          const char *dirname,
                                          netsnmp_directory_filter *filter,
                                          void *filter_ctx, u_int flags);

#define netsnmp_directory_container_read(c,d,f) \
    netsnmp_directory_container_read_some(c,d,NULL,NULL,f);

    void netsnmp_directory_container_free(netsnmp_container *c);

        

    /*------------------------------------------------------------------
     *
     * flags
     */
#define NETSNMP_DIR_RECURSE                           0x0001
#define NETSNMP_DIR_RELATIVE_PATH                     0x0002
#define NETSNMP_DIR_SORTED                            0x0004
/** don't return null if dir empty */
#define NETSNMP_DIR_EMPTY_OK                          0x0008
/** store netsnmp_file instead of filenames */
#define NETSNMP_DIR_NSFILE                            0x0010
/** load stats in netsnmp_file */
#define NETSNMP_DIR_NSFILE_STATS                      0x0020

    
        
#ifdef __cplusplus
}
#endif

#endif /* NETSNMP_DIR_UTILS_H */
