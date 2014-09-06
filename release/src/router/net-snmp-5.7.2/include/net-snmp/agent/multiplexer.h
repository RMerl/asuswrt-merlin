#ifndef NETSNMP_MULTIPLEXER_H
#define NETSNMP_MULTIPLEXER_H

#ifdef __cplusplus
extern          "C" {
#endif

/*
 * The multiplexer helper 
 */

/** @name multiplexer
 *  @{ */

/** @struct netsnmp_mib_handler_methods
 *  Defines the subhandlers to be called by the multiplexer helper
 */
typedef struct netsnmp_mib_handler_methods_s {
   /** called when a GET request is received */
    netsnmp_mib_handler *get_handler;
   /** called when a GETNEXT request is received */
    netsnmp_mib_handler *getnext_handler;
   /** called when a GETBULK request is received */
    netsnmp_mib_handler *getbulk_handler;
   /** called when a SET request is received */
    netsnmp_mib_handler *set_handler;
} netsnmp_mib_handler_methods;

/** @} */

netsnmp_mib_handler
    *netsnmp_get_multiplexer_handler(netsnmp_mib_handler_methods *);

Netsnmp_Node_Handler netsnmp_multiplexer_helper_handler;

#ifdef __cplusplus
}
#endif
#endif                          /* NETSNMP_MULTIPLEXER_H */
