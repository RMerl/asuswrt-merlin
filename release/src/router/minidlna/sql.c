/* MiniDLNA media server
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
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sql.h"
#include "upnpglobalvars.h"
#include "log.h"

int
sql_exec(sqlite3 *db, const char *fmt, ...)
{
	int ret;
	char *errMsg = NULL;
	char *sql;
	va_list ap;
	//DPRINTF(E_DEBUG, L_DB_SQL, "SQL: %s\n", sql);

	va_start(ap, fmt);
	sql = sqlite3_vmprintf(fmt, ap);
	va_end(ap);
	ret = sqlite3_exec(db, sql, 0, 0, &errMsg);
	if( ret != SQLITE_OK )
	{
		DPRINTF(E_ERROR, L_DB_SQL, "SQL ERROR %d [%s]\n%s\n", ret, errMsg, sql);
		if (errMsg)
			sqlite3_free(errMsg);
	}
	sqlite3_free(sql);

	return ret;
}

int
sql_get_table(sqlite3 *db, const char *sql, char ***pazResult, int *pnRow, int *pnColumn)
{
	int ret;
	char *errMsg = NULL;
	//DPRINTF(E_DEBUG, L_DB_SQL, "SQL: %s\n", sql);
	
	ret = sqlite3_get_table(db, sql, pazResult, pnRow, pnColumn, &errMsg);
	if( ret != SQLITE_OK )
	{
		DPRINTF(E_ERROR, L_DB_SQL, "SQL ERROR %d [%s]\n%s\n", ret, errMsg, sql);
		if (errMsg)
			sqlite3_free(errMsg);
	}

	return ret;
}

int
sql_get_int_field(sqlite3 *db, const char *fmt, ...)
{
	va_list		ap;
	int		counter, result;
	char		*sql;
	int		ret;
	sqlite3_stmt	*stmt;
	
	va_start(ap, fmt);
	sql = sqlite3_vmprintf(fmt, ap);
	va_end(ap);

	//DPRINTF(E_DEBUG, L_DB_SQL, "sql: %s\n", sql);

	switch (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL))
	{
		case SQLITE_OK:
			break;
		default:
			DPRINTF(E_ERROR, L_DB_SQL, "prepare failed: %s\n%s\n", sqlite3_errmsg(db), sql);
			sqlite3_free(sql);
			return -1;
	}

	for (counter = 0;
	     ((result = sqlite3_step(stmt)) == SQLITE_BUSY || result == SQLITE_LOCKED) && counter < 2;
	     counter++) {
		 /* While SQLITE_BUSY has a built in timeout,
		    SQLITE_LOCKED does not, so sleep */
		 if (result == SQLITE_LOCKED)
		 	sleep(1);
	}

	switch (result)
	{
		case SQLITE_DONE:
			/* no rows returned */
			ret = 0;
			break;
		case SQLITE_ROW:
			if (sqlite3_column_type(stmt, 0) == SQLITE_NULL)
			{
				ret = 0;
				break;
			}
			ret = sqlite3_column_int(stmt, 0);
			break;
		default:
			DPRINTF(E_WARN, L_DB_SQL, "%s: step failed: %s\n%s\n", __func__, sqlite3_errmsg(db), sql);
			ret = -1;
			break;
 	}
	sqlite3_free(sql);
	sqlite3_finalize(stmt);

	return ret;
}

int64_t
sql_get_int64_field(sqlite3 *db, const char *fmt, ...)
{
	va_list		ap;
	int		counter, result;
	char		*sql;
	int64_t		ret;
	sqlite3_stmt	*stmt;
	
	va_start(ap, fmt);
	sql = sqlite3_vmprintf(fmt, ap);
	va_end(ap);

	//DPRINTF(E_DEBUG, L_DB_SQL, "sql: %s\n", sql);

	switch (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL))
	{
		case SQLITE_OK:
			break;
		default:
			DPRINTF(E_ERROR, L_DB_SQL, "prepare failed: %s\n%s\n", sqlite3_errmsg(db), sql);
			sqlite3_free(sql);
			return -1;
	}

	for (counter = 0;
	     ((result = sqlite3_step(stmt)) == SQLITE_BUSY || result == SQLITE_LOCKED) && counter < 2;
	     counter++) {
		 /* While SQLITE_BUSY has a built in timeout,
		    SQLITE_LOCKED does not, so sleep */
		 if (result == SQLITE_LOCKED)
		 	sleep(1);
	}

	switch (result)
	{
		case SQLITE_DONE:
			/* no rows returned */
			ret = 0;
			break;
		case SQLITE_ROW:
			if (sqlite3_column_type(stmt, 0) == SQLITE_NULL)
			{
				ret = 0;
				break;
			}
			ret = sqlite3_column_int64(stmt, 0);
			break;
		default:
			DPRINTF(E_WARN, L_DB_SQL, "%s: step failed: %s\n%s\n", __func__, sqlite3_errmsg(db), sql);
			ret = -1;
			break;
 	}
	sqlite3_free(sql);
	sqlite3_finalize(stmt);

	return ret;
}

char *
sql_get_text_field(sqlite3 *db, const char *fmt, ...)
{
	va_list         ap;
	int             counter, result, len;
	char            *sql;
	char            *str;
	sqlite3_stmt    *stmt;

	if (db == NULL)
	{
		DPRINTF(E_WARN, L_DB_SQL, "db is NULL\n");
		return NULL;
	}

	va_start(ap, fmt);
	sql = sqlite3_vmprintf(fmt, ap);
	va_end(ap);

	//DPRINTF(E_DEBUG, L_DB_SQL, "sql: %s\n", sql);

	switch (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL))
	{
		case SQLITE_OK:
			break;
		default:
			DPRINTF(E_ERROR, L_DB_SQL, "prepare failed: %s\n%s\n", sqlite3_errmsg(db), sql);
			sqlite3_free(sql);
			return NULL;
	}
	sqlite3_free(sql);

	for (counter = 0;
	     ((result = sqlite3_step(stmt)) == SQLITE_BUSY || result == SQLITE_LOCKED) && counter < 2;
	     counter++)
	{
		/* While SQLITE_BUSY has a built in timeout,
		 * SQLITE_LOCKED does not, so sleep */
		if (result == SQLITE_LOCKED)
			sleep(1);
	}

	switch (result)
	{
		case SQLITE_DONE:
			/* no rows returned */
			str = NULL;
			break;

		case SQLITE_ROW:
			if (sqlite3_column_type(stmt, 0) == SQLITE_NULL)
			{
				str = NULL;
				break;
			}

			len = sqlite3_column_bytes(stmt, 0);
			if ((str = sqlite3_malloc(len + 1)) == NULL)
			{
				DPRINTF(E_ERROR, L_DB_SQL, "malloc failed\n");
				break;
			}

			strncpy(str, (char *)sqlite3_column_text(stmt, 0), len + 1);
			break;

		default:
			DPRINTF(E_WARN, L_DB_SQL, "SQL step failed: %s\n", sqlite3_errmsg(db));
			str = NULL;
			break;
	}
	sqlite3_finalize(stmt);

	return str;
}

int
db_upgrade(sqlite3 *db)
{
	int db_vers;

	db_vers = sql_get_int_field(db, "PRAGMA user_version");

	if (db_vers == DB_VERSION)
		return 0;
	if (db_vers > DB_VERSION)
		return -2;
	if (db_vers < 1)
		return -1;
	if (db_vers < 9)
		return db_vers;
	sql_exec(db, "PRAGMA user_version = %d", DB_VERSION);

	return 0;
}
