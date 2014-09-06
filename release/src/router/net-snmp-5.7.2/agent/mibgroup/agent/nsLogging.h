#ifndef NSLOGGING_H
#define NSLOGGING_H

/*
 * function declarations 
 */
void            init_nsLogging(void);

/*
 * Handler and iterators for the logging table
 */
Netsnmp_Node_Handler handle_nsLoggingTable;
Netsnmp_First_Data_Point  get_first_logging_entry;
Netsnmp_Next_Data_Point   get_next_logging_entry;

#endif /* NSLOGGING_H */
