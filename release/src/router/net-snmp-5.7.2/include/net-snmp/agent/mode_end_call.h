/*
 * mode_end_call.h 
 */
#ifndef MODE_END_CALL_H
#define MODE_END_CALL_H

#ifdef __cplusplus
extern          "C" {
#endif

#define NETSNMP_MODE_END_ALL_MODES -999

typedef struct netsnmp_mode_handler_list_s {
   struct netsnmp_mode_handler_list_s *next;
   int mode;
   netsnmp_mib_handler *callback_handler;
} netsnmp_mode_handler_list;

/*
 * The helper calls another handler after each mode has been
 * processed.
 */

/* public functions */
netsnmp_mib_handler *
netsnmp_get_mode_end_call_handler(netsnmp_mode_handler_list *endlist);

netsnmp_mode_handler_list *
netsnmp_mode_end_call_add_mode_callback(netsnmp_mode_handler_list *endlist,
                                        int mode,
                                        netsnmp_mib_handler *callbackh);

/* internal */
Netsnmp_Node_Handler netsnmp_mode_end_call_helper;

#ifdef __cplusplus
}
#endif
#endif
