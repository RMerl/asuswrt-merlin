/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Marcin Krzysztof Porwit    2005.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* Defines for TDB keys */
#define  EVT_OLDEST_ENTRY  "INFO/oldest_entry"
#define  EVT_NEXT_RECORD   "INFO/next_record"
#define  EVT_VERSION       "INFO/version"
#define  EVT_MAXSIZE       "INFO/maxsize"
#define  EVT_RETENTION     "INFO/retention"

#define ELOG_APPL	"Application"
#define ELOG_SYS	"System"
#define ELOG_SEC	"Security"

typedef struct elog_tdb {
	struct elog_tdb *prev, *next;
	char *name;
	TDB_CONTEXT *tdb;
	int ref_count;
} ELOG_TDB;

#define ELOG_TDB_CTX(x) ((x)->tdb)


#define  EVENTLOG_DATABASE_VERSION_V1    1
