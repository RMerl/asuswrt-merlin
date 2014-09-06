#ifndef MTEOBJECTSCONF_H
#define MTEOBJECTSCONF_H

/*
 * function declarations 
 */
void            init_mteObjectsConf(void);
void            parse_mteOTable(const char *, char *);
SNMPCallback    store_mteOTable;
SNMPCallback    clear_mteOTable;

#endif                          /* MTEOBJECTSCONF_H */
