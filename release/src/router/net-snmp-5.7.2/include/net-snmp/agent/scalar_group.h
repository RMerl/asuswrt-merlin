/*
 * scalar.h 
 */
#ifndef NETSNMP_SCALAR_GROUP_H
#define NETSNMP_SCALAR_GROUP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The scalar group helper is designed to implement a group of
 * scalar objects all in one go, making use of the scalar and
 * instance helpers.
 *
 * GETNEXTs are auto-converted to a GET.  Non-valid GETs are dropped.
 * The client-provided handler just needs to check the OID name to
 * see which object is being requested, but can otherwise assume that
 * things are fine.
 */

typedef struct netsnmp_scalar_group_s {
    oid lbound;		/* XXX - or do we need a more flexible arrangement? */
    oid ubound;
} netsnmp_scalar_group;

int netsnmp_register_scalar_group(netsnmp_handler_registration *reginfo,
	                          oid first, oid last);
netsnmp_mib_handler *netsnmp_get_scalar_group_handler(oid first, oid last);
Netsnmp_Node_Handler netsnmp_scalar_group_helper_handler;

#ifdef __cplusplus
}
#endif

#endif /** NETSNMP_SCALAR_GROUP_H */
