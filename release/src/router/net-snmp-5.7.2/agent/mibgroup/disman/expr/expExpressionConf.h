#ifndef EXPEXPRESSIONCONF_H
#define EXPEXPRESSIONCONF_H

void    init_expExpressionConf(void);

void          parse_expression(const char *, char *);
void          parse_expETable( const char *, char *);
SNMPCallback  store_expETable;

#endif   /* EXPEXPRESSIONCONF_H */
