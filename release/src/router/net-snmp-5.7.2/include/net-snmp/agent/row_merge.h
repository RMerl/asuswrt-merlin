#ifndef ROW_MERGE_H
#define ROW_MERGE_H

/*
 * This row_merge helper splits a whole bunch of requests into chunks
 * based on the row index that they refer to, and passes all requests
 * for a given row to the lower handlers.
 */

#ifdef __cplusplus
extern          "C" {
#endif

    typedef struct netsnmp_row_merge_status_x {
       char                  count;    /* number of requests */
       char                  rows;     /* number of rows (unique indexes) */
       char                  current;  /* current row number */
       char                  reserved; /* reserver for future use */

       netsnmp_request_info  **saved_requests; /* internal use */
       char                  *saved_status;    /* internal use */
    } netsnmp_row_merge_status;

    netsnmp_mib_handler *netsnmp_get_row_merge_handler(int);
    int   netsnmp_register_row_merge(netsnmp_handler_registration *reginfo);
    void  netsnmp_init_row_merge(void);

    int netsnmp_row_merge_status_first(netsnmp_handler_registration *reginfo,
                                       netsnmp_agent_request_info *reqinfo);
    int netsnmp_row_merge_status_last(netsnmp_handler_registration *reginfo,
                                      netsnmp_agent_request_info *reqinfo);

    Netsnmp_Node_Handler netsnmp_row_merge_helper_handler;

#ifdef __cplusplus
}
#endif
#endif
