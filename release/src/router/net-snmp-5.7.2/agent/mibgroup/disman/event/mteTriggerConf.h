#ifndef MTETRIGGERCONF_H
#define MTETRIGGERCONF_H

config_require(utilities/iquery)

/*
 * function declarations 
 */
void            init_mteTriggerConf(void);
void            parse_mteMonitor( const char *, const char *);
void            parse_default_mteMonitors( const char *, const char *);
void            parse_linkUpDown_traps(const char *, const char *);

void            parse_mteTTable(  const char *, char *);
void            parse_mteTDTable( const char *, char *);
void            parse_mteTExTable(const char *, char *);
void            parse_mteTBlTable(const char *, char *);
void            parse_mteTThTable(const char *, char *);
void            parse_mteTriggerTable(const char *, char *);
SNMPCallback    store_mteTTable;
SNMPCallback    clear_mteTTable;

#endif                          /* MTETRIGGERCONF_H */
