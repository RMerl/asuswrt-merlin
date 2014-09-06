/*
 * stash_to_next.h 
 */
#ifndef STASH_TO_NEXT_H
#define STASH_TO_NEXT_H

#ifdef __cplusplus
extern          "C" {
#endif


/*
 * The helper merely intercepts GETSTASH requests and converts them to
 * GETNEXT reequests.
 */


netsnmp_mib_handler *netsnmp_get_stash_to_next_handler(void);
Netsnmp_Node_Handler netsnmp_stash_to_next_helper;

#ifdef __cplusplus
}
#endif
#endif
