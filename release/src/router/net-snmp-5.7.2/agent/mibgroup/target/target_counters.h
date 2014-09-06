/*
 * testhandler.h 
 */

config_exclude(target/target_counters)

void            init_target_counters(void);

Netsnmp_Node_Handler get_unavailable_context_count;
Netsnmp_Node_Handler get_unknown_context_count;
