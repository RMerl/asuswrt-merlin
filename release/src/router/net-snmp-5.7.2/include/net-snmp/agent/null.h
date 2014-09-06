#ifndef AGENT_NULL_H
#define AGENT_NULL_H

#ifdef __cplusplus
extern          "C" {
#endif

/*
 * null.h 
 */

/*
 * literally does nothing and is used as a final handler for
 * "do-nothing" nodes that must exist solely for mib tree storage
 * usage..
 */

int      netsnmp_register_null(oid *, size_t);
int      netsnmp_register_null_context(oid *, size_t, const char *contextName);

Netsnmp_Node_Handler netsnmp_null_handler;

#ifdef __cplusplus
}
#endif
#endif
