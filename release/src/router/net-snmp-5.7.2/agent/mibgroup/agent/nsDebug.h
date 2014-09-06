#ifndef NSDEBUG_H
#define NSDEBUG_H

/*
 * function declarations 
 */
void            init_nsDebug(void);

/*
 * Handlers for the scalar objects
 */
Netsnmp_Node_Handler handle_nsDebugEnabled;
Netsnmp_Node_Handler handle_nsDebugOutputAll;
Netsnmp_Node_Handler handle_nsDebugDumpPdu;

/*
 * Handler and iterators for the debug table
 */
Netsnmp_Node_Handler handle_nsDebugTable;
Netsnmp_First_Data_Point  get_first_debug_entry;
Netsnmp_Next_Data_Point   get_next_debug_entry;

#endif /* NSDEBUG_H */
