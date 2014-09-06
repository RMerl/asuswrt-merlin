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
#ifndef NETSNMP_SWINST_H
#define NETSNMP_SWINST_H

#ifdef  __cplusplus
extern "C" {
#endif

    /*
     * Data structure for a swinst entry 
     */
    typedef struct hrSWInstalledTable_entry {
        netsnmp_index   oid_index;
        
        /*
         * Index values; MIB type is int32, but we use oid so this
         * structure can be used directly with a table_container.
         */
        oid             swIndex;
        
        /*
         * Column values 
         */
        char            swName[64];
        char            swDate[11];
#ifdef NETSNMP_HAVE_SWID
        oid            *swID;
        u_char          swID_len;
#endif
        u_char          swType;
        u_char          swName_len;
        u_char          swDate_len;
    } netsnmp_swinst_entry;
    
#define HRSWINSTALLEDTYPE_UNKNOWN  1
#define HRSWINSTALLEDTYPE_OPERATINGSYSTEM  2
#define HRSWINSTALLEDTYPE_DEVICEDRIVER  3
#define HRSWINSTALLEDTYPE_APPLICATION  4


#define NETSNMP_SWINST_NOFLAGS            0x00000000

#define NETSNMP_SWINST_ALL_OR_NONE        0x00000001
#define NETSNMP_SWINST_DONT_FREE_ITEMS    0x00000002

    netsnmp_container *
    netsnmp_swinst_container_load(netsnmp_container *container, int flags );

    void netsnmp_swinst_container_free(netsnmp_container *container,
                                       u_int flags);
    void netsnmp_swinst_container_free_items(netsnmp_container *container);

    void netsnmp_swinst_entry_remove(netsnmp_container * container,
                                     netsnmp_swinst_entry *entry);

    netsnmp_swinst_entry * netsnmp_swinst_entry_create(int32_t index);
    void netsnmp_swinst_entry_free(netsnmp_swinst_entry *entry);

    int32_t netsnmp_swinst_add_name(const char *name);
    int32_t netsnmp_swinst_get_id(const char *name);
    const char * netsnmp_swinst_get_name(int32_t id);

#ifdef  __cplusplus
}
#endif


#endif /* NETSNMP_SWINST_H */
