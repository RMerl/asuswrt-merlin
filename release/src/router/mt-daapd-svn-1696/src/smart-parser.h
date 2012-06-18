/*
 * $Id: smart-parser.h 824 2006-03-12 11:30:58Z rpedde $
 */

#ifndef _SMART_PARSER_H_
#define _SMART_PARSER_H_

typedef void* PARSETREE;

extern PARSETREE sp_init(void);
extern int sp_parse(PARSETREE tree, char *term, int type);
extern int sp_dispose(PARSETREE tree);
extern char *sp_get_error(PARSETREE tree);
extern char *sp_sql_clause(PARSETREE tree);

#define SP_TYPE_PLAYLIST 0
#define SP_TYPE_QUERY    1

#endif /* _SMART_PARSER_H_ */

