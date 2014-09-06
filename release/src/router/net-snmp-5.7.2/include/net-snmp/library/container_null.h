#ifndef NETSNMP_CONTAINER_NULL_H
#define NETSNMP_CONTAINER_NULL_H


#include <net-snmp/library/container.h>

#ifdef  __cplusplus
extern "C" {
#endif

netsnmp_container *netsnmp_container_get_null(void);

    NETSNMP_IMPORT
    void netsnmp_container_null_init(void);


#ifdef  __cplusplus
}
#endif

#endif /** NETSNMP_CONTAINER_NULL_H */
