/*
 *  Unix SMB/CIFS implementation.
 *  Eventlog utility  routines
 *
 *  Copyright (C) Marcin Krzysztof Porwit    2005
 *  Copyright (C) Brian Moran                2005
 *  Copyright (C) Gerald (Jerry) Carter      2005
 *  Copyright (C) Guenther Deschner          2009
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

#ifndef _LIB_EVENTLOG_PROTO_H_
#define _LIB_EVENTLOG_PROTO_H_

/* The following definitions come from lib/eventlog/eventlog.c  */

TDB_CONTEXT *elog_init_tdb( char *tdbfilename );
char *elog_tdbname(TALLOC_CTX *ctx, const char *name );
int elog_tdb_size( TDB_CONTEXT * tdb, int *MaxSize, int *Retention );
bool prune_eventlog( TDB_CONTEXT * tdb );
ELOG_TDB *elog_open_tdb( const char *logname, bool force_clear, bool read_only );
int elog_close_tdb( ELOG_TDB *etdb, bool force_close );
bool parse_logentry( TALLOC_CTX *mem_ctx, char *line, struct eventlog_Record_tdb *entry, bool * eor );
size_t fixup_eventlog_record_tdb(struct eventlog_Record_tdb *r);
struct eventlog_Record_tdb *evlog_pull_record_tdb(TALLOC_CTX *mem_ctx,
						  TDB_CONTEXT *tdb,
						  uint32_t record_number);
NTSTATUS evlog_push_record_tdb(TALLOC_CTX *mem_ctx,
			       TDB_CONTEXT *tdb,
			       struct eventlog_Record_tdb *r,
			       uint32_t *record_number);
NTSTATUS evlog_push_record(TALLOC_CTX *mem_ctx,
			   TDB_CONTEXT *tdb,
			   struct EVENTLOGRECORD *r,
			   uint32_t *record_number);
struct EVENTLOGRECORD *evlog_pull_record(TALLOC_CTX *mem_ctx,
					 TDB_CONTEXT *tdb,
					 uint32_t record_number);
NTSTATUS evlog_evt_entry_to_tdb_entry(TALLOC_CTX *mem_ctx,
				      const struct EVENTLOGRECORD *e,
				      struct eventlog_Record_tdb *t);
NTSTATUS evlog_tdb_entry_to_evt_entry(TALLOC_CTX *mem_ctx,
				      const struct eventlog_Record_tdb *t,
				      struct EVENTLOGRECORD *e);
NTSTATUS evlog_convert_tdb_to_evt(TALLOC_CTX *mem_ctx,
				  ELOG_TDB *etdb,
				  DATA_BLOB *blob_p,
				  uint32_t *num_records_p);

#endif /* _LIB_EVENTLOG_PROTO_H_ */
