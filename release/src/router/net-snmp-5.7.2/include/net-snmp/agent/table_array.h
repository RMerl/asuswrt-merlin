/*
 * table_array.h
 * $Id$
 */
#ifndef _TABLE_ARRAY_HANDLER_H_
#define _TABLE_ARRAY_HANDLER_H_

#ifdef __cplusplus
extern          "C" {
#endif

    /*
     * The table array helper is designed to simplify the task of
     * writing a table handler for the net-snmp agent when the data being
     * accessed is in an oid sorted form and must be accessed externally.
     * 
     * Functionally, it is a specialized version of the more
     * generic table helper but easies the burden of GETNEXT processing by
     * retrieving the appropriate row for ead index through
     * function calls which should be supplied by the module that wishes
     * help.  The module the table_array helps should, afterwards,
     * never be called for the case of "MODE_GETNEXT" and only for the GET
     * and SET related modes instead.
     */

#include <net-snmp/library/container.h>
#include <net-snmp/agent/table.h>

#define TABLE_ARRAY_NAME "table_array"

    /*
     * group_item is to allow us to keep a list of requests without
     * disrupting the actual netsnmp_request_info list.
     */
    typedef struct netsnmp_request_group_item_s {
        netsnmp_request_info *ri;
        netsnmp_table_request_info *tri;
        struct netsnmp_request_group_item_s *next;
    } netsnmp_request_group_item;

    /*
     * structure to keep a list of requests for each unique index
     */
    typedef struct netsnmp_request_group_s {
       /*
        * index for this row. points to someone else's memory, so
        * don't free it!
        */
        netsnmp_index               index;

       /*
        * container in which rows belong
        */
        netsnmp_container           *table;

       /*
        * actual old and new rows
        */
        netsnmp_index               *existing_row;
        netsnmp_index               *undo_info;

       /*
        * flags
        */
       char                          row_created;
       char                          row_deleted;
       char                          fill1;
       char                          fill2;

       /*
        * requests for this row
        */
        netsnmp_request_group_item  *list;

        int                          status;

        void                        *rg_void;

    } netsnmp_request_group;

    typedef int     (Netsnmp_User_Row_Operation_c) (const void *lhs,
                                                    const void *rhs);
    typedef int     (Netsnmp_User_Row_Operation) (void *lhs, void *rhs);
    typedef int     (Netsnmp_User_Get_Processor) (netsnmp_request_info *,
                                                  netsnmp_index
                                                  *,
                                                  netsnmp_table_request_info
                                                  *);
    typedef netsnmp_index
        *(UserRowMethod) (netsnmp_index *);
    typedef int     (Netsnmp_User_Row_Action) (netsnmp_index *,
                                               netsnmp_index *,
                                               netsnmp_request_group *);
    typedef void    (Netsnmp_User_Group_Method) (netsnmp_request_group *);

    /*
     * structure for array callbacks
     */
    typedef struct netsnmp_table_array_callbacks_s {

        Netsnmp_User_Row_Operation   *row_copy;
        Netsnmp_User_Row_Operation_c *row_compare;

        Netsnmp_User_Get_Processor *get_value;


        Netsnmp_User_Row_Action *can_activate;
        Netsnmp_User_Row_Action *activated;
        Netsnmp_User_Row_Action *can_deactivate;
        Netsnmp_User_Row_Action *deactivated;
        Netsnmp_User_Row_Action *can_delete;

        UserRowMethod  *create_row;
        UserRowMethod  *duplicate_row;
        UserRowMethod  *delete_row;    /* always returns NULL */

        Netsnmp_User_Group_Method *set_reserve1;
        Netsnmp_User_Group_Method *set_reserve2;
        Netsnmp_User_Group_Method *set_action;
        Netsnmp_User_Group_Method *set_commit;
        Netsnmp_User_Group_Method *set_free;
        Netsnmp_User_Group_Method *set_undo;

       /** not callbacks, but this is a useful place for them... */
       netsnmp_container* container;
       char can_set;

    } netsnmp_table_array_callbacks;


    int            
        netsnmp_table_container_register(netsnmp_handler_registration *reginfo,
                                     netsnmp_table_registration_info
                                     *tabreq,
                                     netsnmp_table_array_callbacks *cb,
                                     netsnmp_container *container,
                                     int group_rows);

    int netsnmp_table_array_register(netsnmp_handler_registration *reginfo,
                                     netsnmp_table_registration_info *tabreq,
                                     netsnmp_table_array_callbacks *cb,
                                     netsnmp_container *container,
                                     int group_rows);

    netsnmp_container * netsnmp_extract_array_context(netsnmp_request_info *);

    Netsnmp_Node_Handler netsnmp_table_array_helper_handler;

    int
    netsnmp_table_array_check_row_status(netsnmp_table_array_callbacks *cb,
                                         netsnmp_request_group *ag,
                                         long *rs_new, long *rs_old);

#ifdef __cplusplus
}
#endif

#endif                          /* _TABLE_ARRAY_HANDLER_H_ */
