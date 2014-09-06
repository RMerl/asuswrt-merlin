/*
 * bulk_to_next.h 
 */
#ifndef BULK_TO_NEXT_H
#define BULK_TO_NEXT_H

#ifdef __cplusplus
extern          "C" {
#endif


/*
 * The helper merely intercepts GETBULK requests and converts them to
 * * GETNEXT reequests.
 */


netsnmp_mib_handler *netsnmp_get_bulk_to_next_handler(void);
void            netsnmp_init_bulk_to_next_helper(void);
void            netsnmp_bulk_to_next_fix_requests(netsnmp_request_info
                                                  *requests);

Netsnmp_Node_Handler netsnmp_bulk_to_next_helper;

#ifdef __cplusplus
}
#endif
#endif
