#ifndef EXPOBJECTCONF_H
#define EXPOBJECTCONF_H

void    init_expObjectConf(void);

void          parse_expOTable(const char *, char *);
SNMPCallback  store_expOTable;

#endif   /* EXPOBJECTCONF_H */
