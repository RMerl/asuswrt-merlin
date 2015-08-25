/*
 * $Id: db-sql-sqlite3.h 731 2006-01-10 23:57:43Z rpedde $
 * sqlite2-specific db implementation
 *
 * Copyright (C) 2005 Ron Pedde (ron@pedde.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _DB_SQL_SQLITE3_
#define _DB_SQL_SQLITE3_

extern int db_sqlite3_open(char **pe, char *dsn);
extern int db_sqlite3_close(void);

/* simple utility functions */
extern int db_sqlite3_exec(char **pe, int loglevel, char *fmt, ...);
extern char *db_sqlite3_vmquery(char *fmt,va_list ap);
extern void db_sqlite3_vmfree(char *query);

/* walk through a table */
extern int db_sqlite3_enum_begin(char **pe, char *fmt, ...);
extern int db_sqlite3_enum_fetch(char **pe, SQL_ROW *pr);
extern int db_sqlite3_enum_end(char **pe);
extern int db_sqlite3_enum_restart(char **pe);

extern int db_sqlite3_event(int event_type);
extern int db_sqlite3_insert_id(void);

#endif /* _DB_SQL_SQLITE3_ */

