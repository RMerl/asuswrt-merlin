#ifndef NOTIFICATION_LOG_H
#define NOTIFICATION_LOG_H
#include <net-snmp/agent/agent_handler.h>

#ifdef __cplusplus
extern          "C" {
#endif
/*
 * function declarations 
 */
void init_notification_log(void);
void shutdown_notification_log(void);

void log_notification(netsnmp_pdu *pdu, netsnmp_transport *transport);

#ifdef __cplusplus
}
#endif

#endif                          /* NOTIFICATION_LOG_H */
