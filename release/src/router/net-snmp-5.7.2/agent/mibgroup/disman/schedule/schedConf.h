#ifndef SCHEDCONF_H
#define SCHEDCONF_H

config_require(disman/schedule/schedCore)

/*
 * function declarations 
 */
void            init_schedConf(void);

void            parse_sched_periodic(const char *, char *);
void            parse_sched_timed(   const char *, char *);
void            parse_schedTable(    const char *, char *);
SNMPCallback    store_schedTable;


#endif                          /* SCHEDCONF_H */
