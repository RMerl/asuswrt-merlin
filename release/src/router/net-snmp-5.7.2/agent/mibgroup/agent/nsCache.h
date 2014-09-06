#ifndef NSCACHE_H
#define NSCACHE_H

/*
 * function declarations 
 */
void            init_nsCache(void);

/*
 * Handlers for the scalar objects
 */
Netsnmp_Node_Handler handle_nsCacheTimeout;
Netsnmp_Node_Handler handle_nsCacheEnabled;

/*
 * Handler and iterators for the cache table
 */
Netsnmp_Node_Handler handle_nsCacheTable;
Netsnmp_First_Data_Point  get_first_cache_entry;
Netsnmp_Next_Data_Point   get_next_cache_entry;

#endif /* NSCACHE_H */
