/*
 * table_tdata.h 
 */
#ifndef _TABLE_TDATA_HANDLER_H_
#define _TABLE_TDATA_HANDLER_H_

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

#define TABLE_TDATA_NAME  "table_tdata"
#define TABLE_TDATA_ROW   "table_tdata"
#define TABLE_TDATA_TABLE "table_tdata_table"

#define TDATA_FLAG_NO_STORE_INDEXES   0x01
#define TDATA_FLAG_NO_CONTAINER       0x02 /* user will provide container */

    /*
     * The (table-independent) per-row data structure
     * This is a wrapper round the table-specific per-row data
     *   structure, which is referred to as a "table entry"
     *
     * It should be regarded as an opaque, private data structure,
     *   and shouldn't be accessed directly.
     */
    typedef struct netsnmp_tdata_row_s {
        netsnmp_index   oid_index;      /* table_container index format */
        netsnmp_variable_list *indexes; /* stored permanently if store_indexes = 1 */
        void           *data;   /* the data to store */
    } netsnmp_tdata_row;

    /*
     * The data structure to hold a complete table.
     *
     * This should be regarded as an opaque, private data structure,
     *   and shouldn't be accessed directly.
     */
    typedef struct netsnmp_tdata_s {
        netsnmp_variable_list *indexes_template;        /* containing only types */
        char           *name;   /* if !NULL, it's registered globally */
        int             flags;  /* This field may legitimately be accessed by external code */
        netsnmp_container *container;
    } netsnmp_tdata;

/* Backwards compatability with the previous (poorly named) data structures */
typedef  struct netsnmp_tdata_row_s netsnmp_table_data2row;
typedef  struct netsnmp_tdata_s     netsnmp_table_data2;


/* ============================
 * TData API: Table maintenance
 * ============================ */

    netsnmp_tdata     *netsnmp_tdata_create_table(const char *name, long flags);
    void               netsnmp_tdata_delete_table(netsnmp_tdata *table);
    netsnmp_tdata_row *netsnmp_tdata_create_row(void);
    netsnmp_tdata_row *netsnmp_tdata_clone_row( netsnmp_tdata_row *row);
    int                netsnmp_tdata_copy_row(  netsnmp_tdata_row *dst_row,
                                                netsnmp_tdata_row *src_row);
    void           *netsnmp_tdata_delete_row(   netsnmp_tdata_row *row);

    int             netsnmp_tdata_add_row(      netsnmp_tdata     *table,
                                                netsnmp_tdata_row *row);
    void            netsnmp_tdata_replace_row(  netsnmp_tdata     *table,
                                                netsnmp_tdata_row *origrow,
                                                netsnmp_tdata_row *newrow);
    netsnmp_tdata_row *netsnmp_tdata_remove_row(netsnmp_tdata     *table,
                                                netsnmp_tdata_row *row);
    void   *netsnmp_tdata_remove_and_delete_row(netsnmp_tdata     *table,
                                                netsnmp_tdata_row *row);


/* ============================
 * TData API: MIB maintenance
 * ============================ */

    netsnmp_mib_handler *netsnmp_get_tdata_handler(netsnmp_tdata *table);

    int netsnmp_tdata_register(  netsnmp_handler_registration    *reginfo,
                                 netsnmp_tdata                   *table,
                                 netsnmp_table_registration_info *table_info);
    int netsnmp_tdata_unregister(netsnmp_handler_registration    *reginfo);

    netsnmp_tdata      *netsnmp_tdata_extract_table(    netsnmp_request_info *);
    netsnmp_container  *netsnmp_tdata_extract_container(netsnmp_request_info *);
    netsnmp_tdata_row  *netsnmp_tdata_extract_row(      netsnmp_request_info *);
    void               *netsnmp_tdata_extract_entry(    netsnmp_request_info *);

    void netsnmp_insert_tdata_row(netsnmp_request_info *, netsnmp_tdata_row *);
    void netsnmp_remove_tdata_row(netsnmp_request_info *, netsnmp_tdata_row *);


/* ============================
 * TData API: Row operations
 * ============================ */

    void * netsnmp_tdata_row_entry( netsnmp_tdata_row *row );
    netsnmp_tdata_row *netsnmp_tdata_row_first(netsnmp_tdata     *table);
    netsnmp_tdata_row *netsnmp_tdata_row_get(  netsnmp_tdata     *table,
                                               netsnmp_tdata_row *row);
    netsnmp_tdata_row *netsnmp_tdata_row_next( netsnmp_tdata     *table,
                                               netsnmp_tdata_row *row);

    netsnmp_tdata_row *netsnmp_tdata_row_get_byidx(netsnmp_tdata      *table,
                                                netsnmp_variable_list *indexes);
    netsnmp_tdata_row *netsnmp_tdata_row_get_byoid(netsnmp_tdata      *table,
                                                oid   *searchfor,
                                                size_t searchfor_len);
    netsnmp_tdata_row *netsnmp_tdata_row_next_byidx(netsnmp_tdata     *table,
                                                netsnmp_variable_list *indexes);
    netsnmp_tdata_row *netsnmp_tdata_row_next_byoid(netsnmp_tdata     *table,
                                                oid   *searchfor,
                                                size_t searchfor_len);

    int netsnmp_tdata_row_count(netsnmp_tdata *table);


/* ============================
 * TData API: Index operations
 * ============================ */

#define netsnmp_tdata_add_index(thetable, type) snmp_varlist_add_variable(&thetable->indexes_template, NULL, 0, type, NULL, 0)
#define netsnmp_tdata_row_add_index(row, type, value, value_len) snmp_varlist_add_variable(&row->indexes, NULL, 0, type, (const u_char *) value, value_len)

    int netsnmp_tdata_compare_idx(        netsnmp_tdata_row     *row,
                                          netsnmp_variable_list *indexes);
    int netsnmp_tdata_compare_oid(        netsnmp_tdata_row     *row,
                                          oid *compareto, size_t compareto_len);
    int netsnmp_tdata_compare_subtree_idx(netsnmp_tdata_row     *row,
                                          netsnmp_variable_list *indexes);
    int netsnmp_tdata_compare_subtree_oid(netsnmp_tdata_row     *row,
                                          oid *compareto, size_t compareto_len);


#ifdef __cplusplus
}
#endif

#endif                          /* _TABLE_TDATA_HANDLER_H_ */
