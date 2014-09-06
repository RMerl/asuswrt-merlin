/*
 * scalar.h 
 */
#ifndef NETSNMP_SCALAR_H
#define NETSNMP_SCALAR_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The scalar helper is designed to simplify the task of adding simple
 * scalar objects to the mib tree.
 */

/*
 * GETNEXTs are auto-converted to a GET.
 * * non-valid GETs are dropped.
 * * The client can assume that if you're called for a GET, it shouldn't
 * * have to check the oid at all.  Just answer.
 */

int netsnmp_register_scalar(netsnmp_handler_registration *reginfo);
int netsnmp_register_read_only_scalar(netsnmp_handler_registration *reginfo);

#define SCALAR_HANDLER_NAME "scalar"

netsnmp_mib_handler *netsnmp_get_scalar_handler(void);

Netsnmp_Node_Handler netsnmp_scalar_helper_handler;

#ifdef __cplusplus
}
#endif

#endif /** NETSNMP_SCALAR_H */
