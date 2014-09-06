#ifndef DEBUG_HANDLER_H
#define DEBUG_HANDLER_H

#ifdef __cplusplus
extern          "C" {
#endif

netsnmp_mib_handler *netsnmp_get_debug_handler(void);
void            netsnmp_init_debug_helper(void);

Netsnmp_Node_Handler netsnmp_debug_helper;

#ifdef __cplusplus
}
#endif
#endif                          /* DEBUG_HANDLER_H */
