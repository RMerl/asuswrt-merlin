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
/**
 * @file table_iterator.h
 * @addtogroup table_iterator
 * @{
 */
#ifndef _TABLE_ITERATOR_HANDLER_H_
#define _TABLE_ITERATOR_HANDLER_H_

#ifdef __cplusplus
extern          "C" {
#endif

    struct netsnmp_iterator_info_s;

    typedef netsnmp_variable_list *
               (Netsnmp_First_Data_Point) (void **loop_context,
                                           void **data_context,
                                           netsnmp_variable_list *,
                                           struct netsnmp_iterator_info_s *);
    typedef netsnmp_variable_list *
               (Netsnmp_Next_Data_Point)  (void **loop_context,
                                           void **data_context,
                                           netsnmp_variable_list *,
                                           struct netsnmp_iterator_info_s *);
    typedef void *(Netsnmp_Make_Data_Context) (void *loop_context,
                                             struct netsnmp_iterator_info_s *);
    typedef void  (Netsnmp_Free_Loop_Context) (void *,
                                             struct netsnmp_iterator_info_s *);
    typedef void  (Netsnmp_Free_Data_Context) (void *,
                                             struct netsnmp_iterator_info_s *);

    /** @typedef struct netsnmp_iterator_info_s netsnmp_iterator_info
     * Typedefs the netsnmp_iterator_info_s struct into netsnmp_iterator_info */

    /** @struct netsnmp_iterator_info_s

     * Holds iterator information containing functions which should be
       called by the iterator_handler to loop over your data set and
       sort it in a SNMP specific manner.
       
       The netsnmp_iterator_info typedef can be used instead of directly calling this struct if you would prefer.
     */
    typedef struct netsnmp_iterator_info_s {
       /** Number of handlers that own this data structure. */
       int refcnt;

       /** Responsible for: returning the first set of "index" data, a
           loop-context pointer, and optionally a data context
           pointer */
        Netsnmp_First_Data_Point *get_first_data_point;

       /** Given the previous loop context, this should return the
           next loop context, associated index set and optionally a
           data context */
        Netsnmp_Next_Data_Point *get_next_data_point;

       /** If a data context wasn't supplied by the
           get_first_data_point or get_next_data_point functions and
           the make_data_context pointer is defined, it will be called
           to convert a loop context into a data context. */
        Netsnmp_Make_Data_Context *make_data_context;

       /** A function which should free the loop context.  This
           function is called at *each* iteration step, which is
           not-optimal for speed purposes.  The use of
           free_loop_context_at_end instead is strongly
           encouraged. This can be set to NULL to avoid its usage. */
        Netsnmp_Free_Loop_Context *free_loop_context;

       /** Frees a data context.  This will be called at any time a
           data context needs to be freed.  This may be at the same
           time as a correspondng loop context is freed, or much much
           later.  Multiple data contexts may be kept in existence at
           any time. */
       Netsnmp_Free_Data_Context *free_data_context;

       /** Frees a loop context at the end of the entire iteration
           sequence.  Generally, this would free the loop context
           allocated by the get_first_data_point function (which would
           then be updated by each call to the get_next_data_point
           function).  It is not called until the get_next_data_point
           function returns a NULL */
        Netsnmp_Free_Loop_Context *free_loop_context_at_end;

       /** This can be used by client handlers to store any
           information they need */
        void           *myvoid;
        int             flags;
#define NETSNMP_ITERATOR_FLAG_SORTED	0x01
#define NETSNMP_HANDLER_OWNS_IINFO	0x02

       /** A pointer to the netsnmp_table_registration_info object
           this iterator is registered along with. */
        netsnmp_table_registration_info *table_reginfo;

        /* Experimental extension - Use At Your Own Risk
           (these two fields may change/disappear without warning) */
        Netsnmp_First_Data_Point *get_row_indexes;
        netsnmp_variable_list *indexes;
    } netsnmp_iterator_info;

#define TABLE_ITERATOR_NAME "table_iterator"

/* ============================
 * Iterator API: Table maintenance
 * ============================ */
        /* N/A */

/* ============================
 * Iterator API: MIB maintenance
 * ============================ */

    void   netsnmp_handler_owns_iterator_info(netsnmp_mib_handler *h);
    netsnmp_mib_handler
          *netsnmp_get_table_iterator_handler(netsnmp_iterator_info *iinfo);
    int netsnmp_register_table_iterator(netsnmp_handler_registration *reginfo,
                                        netsnmp_iterator_info *iinfo);
    void  netsnmp_iterator_delete_table(netsnmp_iterator_info *iinfo);

    void *netsnmp_extract_iterator_context(netsnmp_request_info *);
    void   netsnmp_insert_iterator_context(netsnmp_request_info *, void *);

    Netsnmp_Node_Handler netsnmp_table_iterator_helper_handler;

#define netsnmp_register_table_iterator2(reginfo, iinfo)        \
    (((iinfo)->flags |= NETSNMP_HANDLER_OWNS_IINFO),           \
        netsnmp_register_table_iterator((reginfo), (iinfo)))


/* ============================
 * Iterator API: Row operations
 * ============================ */

void *netsnmp_iterator_row_first(      netsnmp_iterator_info *);
void *netsnmp_iterator_row_get(        netsnmp_iterator_info *, void *);
void *netsnmp_iterator_row_next(       netsnmp_iterator_info *, void *);
void *netsnmp_iterator_row_get_byidx(  netsnmp_iterator_info *,
                                       netsnmp_variable_list *);
void *netsnmp_iterator_row_next_byidx( netsnmp_iterator_info *,
                                       netsnmp_variable_list *);
void *netsnmp_iterator_row_get_byoid(  netsnmp_iterator_info *, oid *, size_t);
void *netsnmp_iterator_row_next_byoid( netsnmp_iterator_info *, oid *, size_t);
int   netsnmp_iterator_row_count(      netsnmp_iterator_info *);


/* ============================
 * Iterator API: Index operations
 * ============================ */

#ifdef __cplusplus
}
#endif

#endif                          /* _TABLE_ITERATOR_HANDLER_H_ */
/** @} */
