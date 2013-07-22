/* Reusable SQLite3 wrapper functions
 *
 * Project : minidlna
 * Website : http://sourceforge.net/projects/minidlna/
 * Author  : Justin Maggard
 *
 * MiniDLNA media server
 * Copyright (C) 2008-2009  Justin Maggard
 *
 * This file is part of MiniDLNA.
 *
 * MiniDLNA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MiniDLNA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MiniDLNA. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __SQL_H__
#define __SQL_H__

#include <sqlite3.h>

#ifndef HAVE_SQLITE3_MALLOC
#define sqlite3_malloc(size) sqlite3_mprintf("%*s", size, "")
#endif
#ifndef HAVE_SQLITE3_PREPARE_V2
#define sqlite3_prepare_v2 sqlite3_prepare
#endif

int
sql_exec(sqlite3 *db, const char *fmt, ...);

int
sql_get_table(sqlite3 *db, const char *zSql, char ***pazResult, int *pnRow, int *pnColumn);

int
sql_get_int_field(sqlite3 *db, const char *fmt, ...);

char *
sql_get_text_field(sqlite3 *db, const char *fmt, ...);

int
db_upgrade(sqlite3 *db);

#endif
