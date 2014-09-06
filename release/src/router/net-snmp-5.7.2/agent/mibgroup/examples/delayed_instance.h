#ifndef DELAYED_INSTANCE_H
#define DELAYED_INSTANCE_H

#ifdef __cplusplus
extern "C" {
#endif

Netsnmp_Node_Handler delayed_instance_handler;
void            init_delayed_instance(void);
SNMPAlarmCallback return_delayed_response;

#ifdef __cplusplus
}
#endif

#endif /* DELAYED_INSTANCE_H */
