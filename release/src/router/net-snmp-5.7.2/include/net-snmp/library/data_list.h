/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
/*
 * @file netsnmp_data_list.h
 *
 * @addtogroup agent
 * @addtogroup library *
 *
 * $Id$
 *
 * External definitions for functions and variables in netsnmp_data_list.c.
 *
 * @{
 */

#ifndef DATA_LIST_H
#define DATA_LIST_H

#ifdef __cplusplus
extern          "C" {
#endif

#include <net-snmp/library/snmp_impl.h>
#include <net-snmp/library/tools.h>

    typedef void    (Netsnmp_Free_List_Data) (void *);
    typedef int     (Netsnmp_Save_List_Data) (char *buf, size_t buf_len, void *);
    typedef void *  (Netsnmp_Read_List_Data) (char *buf, size_t buf_len);

/** @struct netsnmp_data_list_s
 * used to iterate through lists of data
 */
    typedef struct netsnmp_data_list_s {
        struct netsnmp_data_list_s *next;
        char           *name;
 	/** The pointer to the data passed on. */
        void           *data;
        /** must know how to free netsnmp_data_list->data */
        Netsnmp_Free_List_Data *free_func;
    } netsnmp_data_list;

    typedef struct netsnmp_data_list_saveinfo_s {
       netsnmp_data_list **datalist;
       const char *type;
       const char *token;
       Netsnmp_Save_List_Data *data_list_save_ptr;
       Netsnmp_Read_List_Data *data_list_read_ptr;
       Netsnmp_Free_List_Data *data_list_free_ptr;
    } netsnmp_data_list_saveinfo;

    NETSNMP_IMPORT
    netsnmp_data_list *
      netsnmp_create_data_list(const char *, void *, Netsnmp_Free_List_Data* );
    void            netsnmp_data_list_add_node(netsnmp_data_list **head,
                                               netsnmp_data_list *node);
    netsnmp_data_list *
      netsnmp_data_list_add_data(netsnmp_data_list **head,
                                 const char *name, void *data,
                                 Netsnmp_Free_List_Data * beer);
    NETSNMP_IMPORT
    void           *netsnmp_get_list_data(netsnmp_data_list *head,
                                          const char *node);
    NETSNMP_IMPORT
    void            netsnmp_free_list_data(netsnmp_data_list *head);    /* single */
    NETSNMP_IMPORT
    void            netsnmp_free_all_list_data(netsnmp_data_list *head);        /* multiple */
    NETSNMP_IMPORT
    int             netsnmp_remove_list_node(netsnmp_data_list **realhead,
                                             const char *name);
    NETSNMP_IMPORT
    netsnmp_data_list *
    netsnmp_get_list_node(netsnmp_data_list *head,
                          const char *name);

    /** depreciated: use netsnmp_data_list_add_node() */
    NETSNMP_IMPORT
    void            netsnmp_add_list_data(netsnmp_data_list **head,
                                          netsnmp_data_list *node);


    void
    netsnmp_register_save_list(netsnmp_data_list **datalist,
                               const char *type, const char *token,
                               Netsnmp_Save_List_Data *data_list_save_ptr,
                               Netsnmp_Read_List_Data *data_list_read_ptr,
                               Netsnmp_Free_List_Data *data_list_free_ptr);
    int
    netsnmp_save_all_data(netsnmp_data_list *head,
                          const char *type, const char *token,
                          Netsnmp_Save_List_Data * data_list_save_ptr);
    SNMPCallback netsnmp_save_all_data_callback;
    void netsnmp_read_data_callback(const char *token, char *line);
#ifdef __cplusplus
}
#endif
#endif
/** @} */
