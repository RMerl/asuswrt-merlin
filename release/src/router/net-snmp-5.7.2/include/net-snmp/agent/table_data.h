/*
 * table_iterator.h 
 */
#ifndef _TABLE_DATA_HANDLER_H_
#define _TABLE_DATA_HANDLER_H_

#ifdef __cplusplus
extern          "C" {
#endif

    /*
     * This helper is designed to completely automate the task of storing
     * tables of data within the agent that are not tied to external data
     * sources (like the kernel, hardware, or other processes, etc).  IE,
     * all rows within a table are expected to be added manually using
     * functions found below.
     */

#define TABLE_DATA_NAME "table_data"
#define TABLE_DATA_ROW  "table_data"
#define TABLE_DATA_TABLE "table_data_table"

    typedef struct netsnmp_table_row_s {
        netsnmp_variable_list *indexes; /* stored permanently if store_indexes = 1 */
        oid            *index_oid;
        size_t          index_oid_len;
        void           *data;   /* the data to store */

        struct netsnmp_table_row_s *next, *prev;        /* if used in a list */
    } netsnmp_table_row;

    typedef struct netsnmp_table_data_s {
        netsnmp_variable_list *indexes_template;        /* containing only types */
        char           *name;   /* if !NULL, it's registered globally */
        int             flags;  /* not currently used */
        int             store_indexes;
        netsnmp_table_row *first_row;
        netsnmp_table_row *last_row;
    } netsnmp_table_data;

/* =================================
 * Table Data API: Table maintenance
 * ================================= */

    void       netsnmp_table_data_generate_index_oid( netsnmp_table_row  *row);

    netsnmp_table_data *netsnmp_create_table_data(const char *name);
    netsnmp_table_row  *netsnmp_create_table_data_row(void);
    netsnmp_table_row  *netsnmp_table_data_clone_row( netsnmp_table_row  *row);
    void               *netsnmp_table_data_delete_row(netsnmp_table_row  *row);
    int                 netsnmp_table_data_add_row(   netsnmp_table_data *table,
                                                      netsnmp_table_row  *row);
    void
       netsnmp_table_data_replace_row(netsnmp_table_data *table,
                                      netsnmp_table_row *origrow,
                                      netsnmp_table_row *newrow);
    netsnmp_table_row *netsnmp_table_data_remove_row(netsnmp_table_data *table,
                                                     netsnmp_table_row  *row);
    void   *netsnmp_table_data_remove_and_delete_row(netsnmp_table_data *table,
                                                     netsnmp_table_row  *row);
    void    netsnmp_table_data_delete_table( netsnmp_table_data *table );

/* =================================
 * Table Data API: MIB maintenance
 * ================================= */

    netsnmp_mib_handler *
        netsnmp_get_table_data_handler(netsnmp_table_data           *table);

    int netsnmp_register_table_data(netsnmp_handler_registration    *reginfo,
                                    netsnmp_table_data              *table,
                                    netsnmp_table_registration_info *table_info);
    int netsnmp_register_read_only_table_data(
                                    netsnmp_handler_registration    *reginfo,
                                    netsnmp_table_data              *table,
                                    netsnmp_table_registration_info *table_info);
    Netsnmp_Node_Handler netsnmp_table_data_helper_handler;

    netsnmp_table_data *netsnmp_extract_table(    netsnmp_request_info *);
    netsnmp_table_row  *netsnmp_extract_table_row(netsnmp_request_info *);
    void          *netsnmp_extract_table_row_data(netsnmp_request_info *);
    void netsnmp_insert_table_row(netsnmp_request_info *, netsnmp_table_row *);

    int netsnmp_table_data_build_result(netsnmp_handler_registration *reginfo,
                                        netsnmp_agent_request_info   *reqinfo,
                                        netsnmp_request_info         *request,
                                        netsnmp_table_row *row, int column,
                                        u_char type, u_char * result_data,
                                        size_t result_data_len);

/* =================================
 * Table Data API: Row operations
 * ================================= */

    netsnmp_table_row *netsnmp_table_data_get_first_row(
                                              netsnmp_table_data    *table);
    netsnmp_table_row *netsnmp_table_data_get_next_row(
                                              netsnmp_table_data    *table,
                                              netsnmp_table_row     *row);

    netsnmp_table_row *netsnmp_table_data_get(netsnmp_table_data    *table,
                                              netsnmp_variable_list *indexes);

    netsnmp_table_row *netsnmp_table_data_get_from_oid(
                                              netsnmp_table_data    *table,
                                              oid *  searchfor,
                                              size_t searchfor_len);

    int netsnmp_table_data_num_rows(netsnmp_table_data *table);


/* =================================
 * Table Data API: Index operations
 * ================================= */

#define netsnmp_table_data_add_index(thetable, type) snmp_varlist_add_variable(&thetable->indexes_template, NULL, 0, type, NULL, 0)
#define netsnmp_table_row_add_index(row, type, value, value_len) snmp_varlist_add_variable(&row->indexes, NULL, 0, type, (const u_char *) value, value_len)


#ifdef __cplusplus
}
#endif

#endif                          /* _TABLE_DATA_HANDLER_H_ */
