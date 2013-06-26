/*
 *  Unix SMB/CIFS implementation.
 *  Eventlog utility  routines
 *  Copyright (C) Marcin Krzysztof Porwit    2005,
 *  Copyright (C) Brian Moran                2005.
 *  Copyright (C) Gerald (Jerry) Carter      2005.
 *  Copyright (C) Guenther Deschner          2009.
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

#include "includes.h"
#include "system/filesys.h"
#include "lib/eventlog/eventlog.h"
#include "../libcli/security/security.h"
#include "util_tdb.h"

/* maintain a list of open eventlog tdbs with reference counts */

static ELOG_TDB *open_elog_list;

/********************************************************************
 Init an Eventlog TDB, and return it. If null, something bad
 happened.
********************************************************************/

TDB_CONTEXT *elog_init_tdb( char *tdbfilename )
{
	TDB_CONTEXT *tdb;

	DEBUG(10,("elog_init_tdb: Initializing eventlog tdb (%s)\n",
		tdbfilename));

	tdb = tdb_open_log( tdbfilename, 0, TDB_DEFAULT,
		O_RDWR|O_CREAT|O_TRUNC, 0660 );

	if ( !tdb ) {
		DEBUG( 0, ( "Can't open tdb for [%s]\n", tdbfilename ) );
		return NULL;
	}

	/* initialize with defaults, copy real values in here from registry */

	tdb_store_int32( tdb, EVT_OLDEST_ENTRY, 1 );
	tdb_store_int32( tdb, EVT_NEXT_RECORD, 1 );
	tdb_store_int32( tdb, EVT_MAXSIZE, 0x80000 );
	tdb_store_int32( tdb, EVT_RETENTION, 0x93A80 );

	tdb_store_int32( tdb, EVT_VERSION, EVENTLOG_DATABASE_VERSION_V1 );

	return tdb;
}

/********************************************************************
 make the tdb file name for an event log, given destination buffer
 and size. Caller must free memory.
********************************************************************/

char *elog_tdbname(TALLOC_CTX *ctx, const char *name )
{
	char *path;
	char *file;
	char *tdbname;

	path = talloc_strdup(ctx, state_path("eventlog"));
	if (!path) {
		return NULL;
	}

	file = talloc_asprintf_strlower_m(path, "%s.tdb", name);
	if (!file) {
		talloc_free(path);
		return NULL;
	}

	tdbname = talloc_asprintf(path, "%s/%s", state_path("eventlog"), file);
	if (!tdbname) {
		talloc_free(path);
		return NULL;
	}

	return tdbname;
}


/********************************************************************
 this function is used to count up the number of bytes in a
 particular TDB
********************************************************************/

struct trav_size_struct {
	int size;
	int rec_count;
};

static int eventlog_tdb_size_fn( TDB_CONTEXT * tdb, TDB_DATA key, TDB_DATA data,
			  void *state )
{
	struct trav_size_struct	 *tsize = (struct trav_size_struct *)state;

	tsize->size += data.dsize;
	tsize->rec_count++;

	return 0;
}

/********************************************************************
 returns the size of the eventlog, and if MaxSize is a non-null
 ptr, puts the MaxSize there. This is purely a way not to have yet
 another function that solely reads the maxsize of the eventlog.
 Yeah, that's it.
********************************************************************/

int elog_tdb_size( TDB_CONTEXT * tdb, int *MaxSize, int *Retention )
{
	struct trav_size_struct tsize;

	if ( !tdb )
		return 0;

	ZERO_STRUCT( tsize );

	tdb_traverse( tdb, eventlog_tdb_size_fn, &tsize );

	if ( MaxSize != NULL ) {
		*MaxSize = tdb_fetch_int32( tdb, EVT_MAXSIZE );
	}

	if ( Retention != NULL ) {
		*Retention = tdb_fetch_int32( tdb, EVT_RETENTION );
	}

	DEBUG( 1,
	       ( "eventlog size: [%d] for [%d] records\n", tsize.size,
		 tsize.rec_count ) );
	return tsize.size;
}

/********************************************************************
 Discard early event logs until we have enough for 'needed' bytes...
 NO checking done beforehand to see that we actually need to do
 this, and it's going to pluck records one-by-one. So, it's best
 to determine that this needs to be done before doing it.

 Setting whack_by_date to True indicates that eventlogs falling
 outside of the retention range need to go...

 return True if we made enough room to accommodate needed bytes
********************************************************************/

static bool make_way_for_eventlogs( TDB_CONTEXT * the_tdb, int32_t needed,
				    bool whack_by_date )
{
	int32_t start_record, i, new_start;
	int32_t end_record;
	int32_t reclen, tresv1, trecnum, timegen, timewr;
	int nbytes, len, Retention, MaxSize;
	TDB_DATA key, ret;
	time_t current_time, exp_time;

	/* discard some eventlogs */

	/* read eventlogs from oldest_entry -- there can't be any discontinuity in recnos,
	   although records not necessarily guaranteed to have successive times */
	/* */

	/* lock */
	tdb_lock_bystring_with_timeout( the_tdb, EVT_NEXT_RECORD, 1 );
	/* read */
	end_record = tdb_fetch_int32( the_tdb, EVT_NEXT_RECORD );
	start_record = tdb_fetch_int32( the_tdb, EVT_OLDEST_ENTRY );
	Retention = tdb_fetch_int32( the_tdb, EVT_RETENTION );
	MaxSize = tdb_fetch_int32( the_tdb, EVT_MAXSIZE );

	time( &current_time );

	/* calculate ... */
	exp_time = current_time - Retention;	/* discard older than exp_time */

	/* todo - check for sanity in next_record */
	nbytes = 0;

	DEBUG( 3,
	       ( "MaxSize [%d] Retention [%d] Current Time [%u]  exp_time [%u]\n",
		 MaxSize, Retention, (unsigned int)current_time, (unsigned int)exp_time ) );
	DEBUG( 3,
	       ( "Start Record [%u] End Record [%u]\n",
		(unsigned int)start_record,
		(unsigned int)end_record ));

	for ( i = start_record; i < end_record; i++ ) {
		/* read a record, add the amt to nbytes */
		key.dsize = sizeof(int32_t);
		key.dptr = (unsigned char *)&i;
		ret = tdb_fetch( the_tdb, key );
		if ( ret.dsize == 0 ) {
			DEBUG( 8,
			       ( "Can't find a record for the key, record [%d]\n",
				 i ) );
			tdb_unlock_bystring( the_tdb, EVT_NEXT_RECORD );
			return False;
		}
		nbytes += ret.dsize;	/* note this includes overhead */

		len = tdb_unpack( ret.dptr, ret.dsize, "ddddd", &reclen,
				  &tresv1, &trecnum, &timegen, &timewr );
		if (len == -1) {
			DEBUG( 10,("make_way_for_eventlogs: tdb_unpack failed.\n"));
			tdb_unlock_bystring( the_tdb, EVT_NEXT_RECORD );
			SAFE_FREE( ret.dptr );
			return False;
		}

		DEBUG( 8,
		       ( "read record %u, record size is [%d], total so far [%d]\n",
			 (unsigned int)i, reclen, nbytes ) );

		SAFE_FREE( ret.dptr );

		/* note that other servers may just stop writing records when the size limit
		   is reached, and there are no records older than 'retention'. This doesn't
		   like a very useful thing to do, so instead we whack (as in sleeps with the
		   fishes) just enough records to fit the what we need.  This behavior could
		   be changed to 'match', if the need arises. */

		if ( !whack_by_date && ( nbytes >= needed ) )
			break;	/* done */
		if ( whack_by_date && ( timegen >= exp_time ) )
			break;	/* done */
	}

	DEBUG( 3,
	       ( "nbytes [%d] needed [%d] start_record is [%u], should be set to [%u]\n",
		 nbytes, needed, (unsigned int)start_record, (unsigned int)i ) );
	/* todo - remove eventlog entries here and set starting record to start_record... */
	new_start = i;
	if ( start_record != new_start ) {
		for ( i = start_record; i < new_start; i++ ) {
			key.dsize = sizeof(int32_t);
			key.dptr = (unsigned char *)&i;
			tdb_delete( the_tdb, key );
		}

		tdb_store_int32( the_tdb, EVT_OLDEST_ENTRY, new_start );
	}
	tdb_unlock_bystring( the_tdb, EVT_NEXT_RECORD );
	return True;
}

/********************************************************************
  some hygiene for an eventlog - see how big it is, and then
  calculate how many bytes we need to remove
********************************************************************/

bool prune_eventlog( TDB_CONTEXT * tdb )
{
	int MaxSize, Retention, CalcdSize;

	if ( !tdb ) {
		DEBUG( 4, ( "No eventlog tdb handle\n" ) );
		return False;
	}

	CalcdSize = elog_tdb_size( tdb, &MaxSize, &Retention );
	DEBUG( 3,
	       ( "Calculated size [%d] MaxSize [%d]\n", CalcdSize,
		 MaxSize ) );

	if ( CalcdSize > MaxSize ) {
		return make_way_for_eventlogs( tdb, CalcdSize - MaxSize,
					       False );
	}

	return make_way_for_eventlogs( tdb, 0, True );
}

/********************************************************************
********************************************************************/

static bool can_write_to_eventlog( TDB_CONTEXT * tdb, int32_t needed )
{
	int calcd_size;
	int MaxSize, Retention;

	/* see if we can write to the eventlog -- do a policy enforcement */
	if ( !tdb )
		return False;	/* tdb is null, so we can't write to it */


	if ( needed < 0 )
		return False;
	MaxSize = 0;
	Retention = 0;

	calcd_size = elog_tdb_size( tdb, &MaxSize, &Retention );

	if ( calcd_size <= MaxSize )
		return True;	/* you betcha */
	if ( calcd_size + needed < MaxSize )
		return True;

	if ( Retention == 0xffffffff ) {
		return False;	/* see msdn - we can't write no room, discard */
	}
	/*
	   note don't have to test, but always good to show intent, in case changes needed
	   later
	 */

	if ( Retention == 0x00000000 ) {
		/* discard record(s) */
		/* todo  - decide when to remove a bunch vs. just what we need... */
		return make_way_for_eventlogs( tdb, calcd_size - MaxSize,
					       True );
	}

	return make_way_for_eventlogs( tdb, calcd_size - MaxSize, False );
}

/*******************************************************************
*******************************************************************/

ELOG_TDB *elog_open_tdb( const char *logname, bool force_clear, bool read_only )
{
	TDB_CONTEXT *tdb = NULL;
	uint32_t vers_id;
	ELOG_TDB *ptr;
	char *tdbpath = NULL;
	ELOG_TDB *tdb_node = NULL;
	char *eventlogdir;
	TALLOC_CTX *ctx = talloc_tos();

	/* check for invalid options */

	if (force_clear && read_only) {
		DEBUG(1,("elog_open_tdb: Invalid flags\n"));
		return NULL;
	}

	/* first see if we have an open context */

	for ( ptr=open_elog_list; ptr; ptr=ptr->next ) {
		if ( strequal( ptr->name, logname ) ) {
			ptr->ref_count++;

			/* trick to alow clearing of the eventlog tdb.
			   The force_clear flag should imply that someone
			   has done a force close.  So make sure the tdb
			   is NULL.  If this is a normal open, then just
			   return the existing reference */

			if ( force_clear ) {
				SMB_ASSERT( ptr->tdb == NULL );
				break;
			}
			else
				return ptr;
		}
	}

	/* make sure that the eventlog dir exists */

	eventlogdir = state_path( "eventlog" );
	if ( !directory_exist( eventlogdir ) )
		mkdir( eventlogdir, 0755 );

	/* get the path on disk */

	tdbpath = elog_tdbname(ctx, logname);
	if (!tdbpath) {
		return NULL;
	}

	DEBUG(7,("elog_open_tdb: Opening %s...(force_clear == %s)\n",
		tdbpath, force_clear?"True":"False" ));

	/* the tdb wasn't already open or this is a forced clear open */

	if ( !force_clear ) {

		tdb = tdb_open_log( tdbpath, 0, TDB_DEFAULT, read_only ? O_RDONLY : O_RDWR , 0 );
		if ( tdb ) {
			vers_id = tdb_fetch_int32( tdb, EVT_VERSION );

			if ( vers_id != EVENTLOG_DATABASE_VERSION_V1 ) {
				DEBUG(1,("elog_open_tdb: Invalid version [%d] on file [%s].\n",
					vers_id, tdbpath));
				tdb_close( tdb );
				tdb = elog_init_tdb( tdbpath );
			}
		}
	}

	if ( !tdb )
		tdb = elog_init_tdb( tdbpath );

	/* if we got a valid context, then add it to the list */

	if ( tdb ) {
		/* on a forced clear, just reset the tdb context if we already
		   have an open entry in the list */

		if ( ptr ) {
			ptr->tdb = tdb;
			return ptr;
		}

		if ( !(tdb_node = TALLOC_ZERO_P( NULL, ELOG_TDB)) ) {
			DEBUG(0,("elog_open_tdb: talloc() failure!\n"));
			tdb_close( tdb );
			return NULL;
		}

		tdb_node->name = talloc_strdup( tdb_node, logname );
		tdb_node->tdb = tdb;
		tdb_node->ref_count = 1;

		DLIST_ADD( open_elog_list, tdb_node );
	}

	return tdb_node;
}

/*******************************************************************
 Wrapper to handle reference counts to the tdb
*******************************************************************/

int elog_close_tdb( ELOG_TDB *etdb, bool force_close )
{
	TDB_CONTEXT *tdb;

	if ( !etdb )
		return 0;

	etdb->ref_count--;

	SMB_ASSERT( etdb->ref_count >= 0 );

	if ( etdb->ref_count == 0 ) {
		tdb = etdb->tdb;
		DLIST_REMOVE( open_elog_list, etdb );
		TALLOC_FREE( etdb );
		return tdb_close( tdb );
	}

	if ( force_close ) {
		tdb = etdb->tdb;
		etdb->tdb = NULL;
		return tdb_close( tdb );
	}

	return 0;
}

/********************************************************************
 Note that it's a pretty good idea to initialize the Eventlog_entry
 structure to zero's before calling parse_logentry on an batch of
 lines that may resolve to a record.  ALSO, it's a good idea to
 remove any linefeeds (that's EOL to you and me) on the lines
 going in.
********************************************************************/

bool parse_logentry( TALLOC_CTX *mem_ctx, char *line, struct eventlog_Record_tdb *entry, bool * eor )
{
	char *start = NULL, *stop = NULL;

	start = line;

	/* empty line signyfiying record delimeter, or we're at the end of the buffer */
	if ( start == NULL || strlen( start ) == 0 ) {
		DEBUG( 6,
		       ( "parse_logentry: found end-of-record indicator.\n" ) );
		*eor = True;
		return True;
	}
	if ( !( stop = strchr( line, ':' ) ) ) {
		return False;
	}

	DEBUG( 6, ( "parse_logentry: trying to parse [%s].\n", line ) );

	if ( 0 == strncmp( start, "LEN", stop - start ) ) {
		/* This will get recomputed later anyway -- probably not necessary */
		entry->size = atoi( stop + 1 );
	} else if ( 0 == strncmp( start, "RS1", stop - start ) ) {
		/* For now all these reserved entries seem to have the same value,
		   which can be hardcoded to int(1699505740) for now */
		entry->reserved = talloc_strdup(mem_ctx, "eLfL");
	} else if ( 0 == strncmp( start, "RCN", stop - start ) ) {
		entry->record_number = atoi( stop + 1 );
	} else if ( 0 == strncmp( start, "TMG", stop - start ) ) {
		entry->time_generated = atoi( stop + 1 );
	} else if ( 0 == strncmp( start, "TMW", stop - start ) ) {
		entry->time_written = atoi( stop + 1 );
	} else if ( 0 == strncmp( start, "EID", stop - start ) ) {
		entry->event_id = atoi( stop + 1 );
	} else if ( 0 == strncmp( start, "ETP", stop - start ) ) {
		if ( strstr( start, "ERROR" ) ) {
			entry->event_type = EVENTLOG_ERROR_TYPE;
		} else if ( strstr( start, "WARNING" ) ) {
			entry->event_type = EVENTLOG_WARNING_TYPE;
		} else if ( strstr( start, "INFO" ) ) {
			entry->event_type = EVENTLOG_INFORMATION_TYPE;
		} else if ( strstr( start, "AUDIT_SUCCESS" ) ) {
			entry->event_type = EVENTLOG_AUDIT_SUCCESS;
		} else if ( strstr( start, "AUDIT_FAILURE" ) ) {
			entry->event_type = EVENTLOG_AUDIT_FAILURE;
		} else if ( strstr( start, "SUCCESS" ) ) {
			entry->event_type = EVENTLOG_SUCCESS;
		} else {
			/* some other eventlog type -- currently not defined in MSDN docs, so error out */
			return False;
		}
	}

/*
  else if(0 == strncmp(start, "NST", stop - start))
  {
  entry->num_of_strings = atoi(stop + 1);
  }
*/
	else if ( 0 == strncmp( start, "ECT", stop - start ) ) {
		entry->event_category = atoi( stop + 1 );
	} else if ( 0 == strncmp( start, "RS2", stop - start ) ) {
		entry->reserved_flags = atoi( stop + 1 );
	} else if ( 0 == strncmp( start, "CRN", stop - start ) ) {
		entry->closing_record_number = atoi( stop + 1 );
	} else if ( 0 == strncmp( start, "USL", stop - start ) ) {
		entry->sid_length = atoi( stop + 1 );
	} else if ( 0 == strncmp( start, "SRC", stop - start ) ) {
		stop++;
		while ( isspace( stop[0] ) ) {
			stop++;
		}
		entry->source_name_len = strlen_m_term(stop);
		entry->source_name = talloc_strdup(mem_ctx, stop);
		if (entry->source_name_len == (uint32_t)-1 ||
				entry->source_name == NULL) {
			return false;
		}
	} else if ( 0 == strncmp( start, "SRN", stop - start ) ) {
		stop++;
		while ( isspace( stop[0] ) ) {
			stop++;
		}
		entry->computer_name_len = strlen_m_term(stop);
		entry->computer_name = talloc_strdup(mem_ctx, stop);
		if (entry->computer_name_len == (uint32_t)-1 ||
				entry->computer_name == NULL) {
			return false;
		}
	} else if ( 0 == strncmp( start, "SID", stop - start ) ) {
		smb_ucs2_t *dummy = NULL;
		stop++;
		while ( isspace( stop[0] ) ) {
			stop++;
		}
		entry->sid_length = rpcstr_push_talloc(mem_ctx,
				&dummy,
				stop);
		if (entry->sid_length == (uint32_t)-1) {
			return false;
		}
		entry->sid = data_blob_talloc(mem_ctx, dummy, entry->sid_length);
		if (entry->sid.data == NULL) {
			return false;
		}
	} else if ( 0 == strncmp( start, "STR", stop - start ) ) {
		size_t tmp_len;
		int num_of_strings;
		/* skip past initial ":" */
		stop++;
		/* now skip any other leading whitespace */
		while ( isspace(stop[0])) {
			stop++;
		}
		tmp_len = strlen_m_term(stop);
		if (tmp_len == (size_t)-1) {
			return false;
		}
		num_of_strings = entry->num_of_strings;
		if (!add_string_to_array(mem_ctx, stop, &entry->strings,
					 &num_of_strings)) {
			return false;
		}
		if (num_of_strings > 0xffff) {
			return false;
		}
		entry->num_of_strings = num_of_strings;
		entry->strings_len += tmp_len;
	} else if ( 0 == strncmp( start, "DAT", stop - start ) ) {
		/* skip past initial ":" */
		stop++;
		/* now skip any other leading whitespace */
		while ( isspace( stop[0] ) ) {
			stop++;
		}
		entry->data_length = strlen_m(stop);
		entry->data = data_blob_talloc(mem_ctx, stop, entry->data_length);
		if (!entry->data.data) {
			return false;
		}
	} else {
		/* some other eventlog entry -- not implemented, so dropping on the floor */
		DEBUG( 10, ( "Unknown entry [%s]. Ignoring.\n", line ) );
		/* For now return true so that we can keep on parsing this mess. Eventually
		   we will return False here. */
		return true;
	}
	return true;
}

/*******************************************************************
 calculate the correct fields etc for an eventlog entry
*******************************************************************/

size_t fixup_eventlog_record_tdb(struct eventlog_Record_tdb *r)
{
	size_t size = 56; /* static size of integers before buffers start */

	r->source_name_len = strlen_m_term(r->source_name) * 2;
	r->computer_name_len = strlen_m_term(r->computer_name) * 2;
	r->strings_len = ndr_size_string_array(r->strings,
		r->num_of_strings, LIBNDR_FLAG_STR_NULLTERM) * 2;

	/* fix up the eventlog entry structure as necessary */
	r->sid_padding = ( ( 4 - ( ( r->source_name_len + r->computer_name_len ) % 4 ) ) % 4 );
	r->padding =       ( 4 - ( ( r->strings_len + r->data_length ) % 4 ) ) % 4;

	if (r->sid_length == 0) {
		/* Should not pad to a DWORD boundary for writing out the sid if there is
		   no SID, so just propagate the padding to pad the data */
		r->padding += r->sid_padding;
		r->sid_padding = 0;
	}

	size += r->source_name_len;
	size += r->computer_name_len;
	size += r->sid_padding;
	size += r->sid_length;
	size += r->strings_len;
	size += r->data_length;
	size += r->padding;
	/* need another copy of length at the end of the data */
	size += sizeof(r->size);

	r->size = size;

	return size;
}


/********************************************************************
 ********************************************************************/

struct eventlog_Record_tdb *evlog_pull_record_tdb(TALLOC_CTX *mem_ctx,
						  TDB_CONTEXT *tdb,
						  uint32_t record_number)
{
	struct eventlog_Record_tdb *r;
	TDB_DATA data, key;

	int32_t srecno;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;

	srecno = record_number;
	key.dptr = (unsigned char *)&srecno;
	key.dsize = sizeof(int32_t);

	data = tdb_fetch(tdb, key);
	if (data.dsize == 0) {
		DEBUG(8,("evlog_pull_record_tdb: "
			"Can't find a record for the key, record %d\n",
			record_number));
		return NULL;
	}

	r = talloc_zero(mem_ctx, struct eventlog_Record_tdb);
	if (!r) {
		goto done;
	}

	blob = data_blob_const(data.dptr, data.dsize);

	ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, r,
			   (ndr_pull_flags_fn_t)ndr_pull_eventlog_Record_tdb);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(10,("evlog_pull_record_tdb: failed to decode record %d\n",
			record_number));
		TALLOC_FREE(r);
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(eventlog_Record_tdb, r);
	}

	DEBUG(10,("evlog_pull_record_tdb: retrieved entry for record %d\n",
		record_number));
 done:
	SAFE_FREE(data.dptr);

	return r;
}

/********************************************************************
 ********************************************************************/

struct EVENTLOGRECORD *evlog_pull_record(TALLOC_CTX *mem_ctx,
					 TDB_CONTEXT *tdb,
					 uint32_t record_number)
{
	struct eventlog_Record_tdb *t;
	struct EVENTLOGRECORD *r;
	NTSTATUS status;

	r = talloc_zero(mem_ctx, struct EVENTLOGRECORD);
	if (!r) {
		return NULL;
	}

	t = evlog_pull_record_tdb(r, tdb, record_number);
	if (!t) {
		talloc_free(r);
		return NULL;
	}

	status = evlog_tdb_entry_to_evt_entry(r, t, r);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(r);
		return NULL;
	}

	r->Length = r->Length2 = ndr_size_EVENTLOGRECORD(r, 0);

	return r;
}

/********************************************************************
 write an eventlog entry. Note that we have to lock, read next
 eventlog, increment, write, write the record, unlock

 coming into this, ee has the eventlog record, and the auxilliary date
 (computer name, etc.) filled into the other structure. Before packing
 into a record, this routine will calc the appropriate padding, etc.,
 and then blast out the record in a form that can be read back in
 ********************************************************************/

NTSTATUS evlog_push_record_tdb(TALLOC_CTX *mem_ctx,
			       TDB_CONTEXT *tdb,
			       struct eventlog_Record_tdb *r,
			       uint32_t *record_number)
{
	TDB_DATA kbuf, ebuf;
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;
	int ret;

	if (!r) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!can_write_to_eventlog(tdb, r->size)) {
		return NT_STATUS_EVENTLOG_CANT_START;
	}

	/* need to read the record number and insert it into the entry here */

	/* lock */
	ret = tdb_lock_bystring_with_timeout(tdb, EVT_NEXT_RECORD, 1);
	if (ret == -1) {
		return NT_STATUS_LOCK_NOT_GRANTED;
	}

	/* read */
	r->record_number = tdb_fetch_int32(tdb, EVT_NEXT_RECORD);

	ndr_err = ndr_push_struct_blob(&blob, mem_ctx, r,
		      (ndr_push_flags_fn_t)ndr_push_eventlog_Record_tdb);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		tdb_unlock_bystring(tdb, EVT_NEXT_RECORD);
		return ndr_map_error2ntstatus(ndr_err);
	}

	/* increment the record count */

	kbuf.dsize = sizeof(int32_t);
	kbuf.dptr = (uint8_t *)&r->record_number;

	ebuf.dsize = blob.length;
	ebuf.dptr  = blob.data;

	ret = tdb_store(tdb, kbuf, ebuf, 0);
	if (ret == -1) {
		tdb_unlock_bystring(tdb, EVT_NEXT_RECORD);
		return NT_STATUS_EVENTLOG_FILE_CORRUPT;
	}

	ret = tdb_store_int32(tdb, EVT_NEXT_RECORD, r->record_number + 1);
	if (ret == -1) {
		tdb_unlock_bystring(tdb, EVT_NEXT_RECORD);
		return NT_STATUS_EVENTLOG_FILE_CORRUPT;
	}
	tdb_unlock_bystring(tdb, EVT_NEXT_RECORD);

	if (record_number) {
		*record_number = r->record_number;
	}

	return NT_STATUS_OK;
}

/********************************************************************
 ********************************************************************/

NTSTATUS evlog_push_record(TALLOC_CTX *mem_ctx,
			   TDB_CONTEXT *tdb,
			   struct EVENTLOGRECORD *r,
			   uint32_t *record_number)
{
	struct eventlog_Record_tdb *t;
	NTSTATUS status;

	t = talloc_zero(mem_ctx, struct eventlog_Record_tdb);
	if (!t) {
		return NT_STATUS_NO_MEMORY;
	}

	status = evlog_evt_entry_to_tdb_entry(t, r, t);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(t);
		return status;
	}

	status = evlog_push_record_tdb(mem_ctx, tdb, t, record_number);
	talloc_free(t);

	return status;
}

/********************************************************************
 ********************************************************************/

NTSTATUS evlog_evt_entry_to_tdb_entry(TALLOC_CTX *mem_ctx,
				      const struct EVENTLOGRECORD *e,
				      struct eventlog_Record_tdb *t)
{
	uint32_t i;

	ZERO_STRUCTP(t);

	t->size				= e->Length;
	t->reserved			= e->Reserved;
	t->record_number		= e->RecordNumber;
	t->time_generated		= e->TimeGenerated;
	t->time_written			= e->TimeWritten;
	t->event_id			= e->EventID;
	t->event_type			= e->EventType;
	t->num_of_strings		= e->NumStrings;
	t->event_category		= e->EventCategory;
	t->reserved_flags		= e->ReservedFlags;
	t->closing_record_number	= e->ClosingRecordNumber;

	t->stringoffset			= e->StringOffset;
	t->sid_length			= e->UserSidLength;
	t->sid_offset			= e->UserSidOffset;
	t->data_length			= e->DataLength;
	t->data_offset			= e->DataOffset;

	t->source_name_len		= 2 * strlen_m_term(e->SourceName);
	t->source_name			= talloc_strdup(mem_ctx, e->SourceName);
	NT_STATUS_HAVE_NO_MEMORY(t->source_name);

	t->computer_name_len		= 2 * strlen_m_term(e->Computername);
	t->computer_name		= talloc_strdup(mem_ctx, e->Computername);
	NT_STATUS_HAVE_NO_MEMORY(t->computer_name);

	/* t->sid_padding; */
	if (e->UserSidLength > 0) {
		const char *sid_str = NULL;
		smb_ucs2_t *dummy = NULL;
		sid_str = sid_string_talloc(mem_ctx, &e->UserSid);
		t->sid_length = rpcstr_push_talloc(mem_ctx, &dummy, sid_str);
		if (t->sid_length == -1) {
			return NT_STATUS_NO_MEMORY;
		}
		t->sid = data_blob_talloc(mem_ctx, (uint8_t *)dummy, t->sid_length);
		NT_STATUS_HAVE_NO_MEMORY(t->sid.data);
	}

	t->strings			= talloc_array(mem_ctx, const char *, e->NumStrings);
	for (i=0; i < e->NumStrings; i++) {
		t->strings[i]		= talloc_strdup(t->strings, e->Strings[i]);
		NT_STATUS_HAVE_NO_MEMORY(t->strings[i]);
	}

	t->strings_len			= 2 * ndr_size_string_array(t->strings, t->num_of_strings, LIBNDR_FLAG_STR_NULLTERM);
	t->data				= data_blob_talloc(mem_ctx, e->Data, e->DataLength);
	/* t->padding			= r->Pad; */

	return NT_STATUS_OK;
}

/********************************************************************
 ********************************************************************/

NTSTATUS evlog_tdb_entry_to_evt_entry(TALLOC_CTX *mem_ctx,
				      const struct eventlog_Record_tdb *t,
				      struct EVENTLOGRECORD *e)
{
	uint32_t i;

	ZERO_STRUCTP(e);

	e->Length		= t->size;
	e->Reserved		= t->reserved;
	e->RecordNumber		= t->record_number;
	e->TimeGenerated	= t->time_generated;
	e->TimeWritten		= t->time_written;
	e->EventID		= t->event_id;
	e->EventType		= t->event_type;
	e->NumStrings		= t->num_of_strings;
	e->EventCategory	= t->event_category;
	e->ReservedFlags	= t->reserved_flags;
	e->ClosingRecordNumber	= t->closing_record_number;

	e->StringOffset		= t->stringoffset;
	e->UserSidLength	= t->sid_length;
	e->UserSidOffset	= t->sid_offset;
	e->DataLength		= t->data_length;
	e->DataOffset		= t->data_offset;

	e->SourceName		= talloc_strdup(mem_ctx, t->source_name);
	NT_STATUS_HAVE_NO_MEMORY(e->SourceName);

	e->Computername		= talloc_strdup(mem_ctx, t->computer_name);
	NT_STATUS_HAVE_NO_MEMORY(e->Computername);

	if (t->sid_length > 0) {
		const char *sid_str = NULL;
		size_t len;
		if (!convert_string_talloc(mem_ctx, CH_UTF16, CH_UNIX,
					   t->sid.data, t->sid.length,
					   (void *)&sid_str, &len, false)) {
			return NT_STATUS_INVALID_SID;
		}
		if (len > 0) {
			string_to_sid(&e->UserSid, sid_str);
		}
	}

	e->Strings		= talloc_array(mem_ctx, const char *, t->num_of_strings);
	for (i=0; i < t->num_of_strings; i++) {
		e->Strings[i] = talloc_strdup(e->Strings, t->strings[i]);
		NT_STATUS_HAVE_NO_MEMORY(e->Strings[i]);
	}

	e->Data			= (uint8_t *)talloc_memdup(mem_ctx, t->data.data, t->data_length);
	e->Pad			= talloc_strdup(mem_ctx, "");
	NT_STATUS_HAVE_NO_MEMORY(e->Pad);

	e->Length2		= t->size;

	return NT_STATUS_OK;
}

/********************************************************************
 ********************************************************************/

NTSTATUS evlog_convert_tdb_to_evt(TALLOC_CTX *mem_ctx,
				  ELOG_TDB *etdb,
				  DATA_BLOB *blob_p,
				  uint32_t *num_records_p)
{
	NTSTATUS status = NT_STATUS_OK;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	uint32_t num_records = 0;
	struct EVENTLOG_EVT_FILE evt;
	uint32_t count = 1;
	size_t endoffset = 0;

	ZERO_STRUCT(evt);

	while (1) {

		struct eventlog_Record_tdb *r;
		struct EVENTLOGRECORD e;

		r = evlog_pull_record_tdb(mem_ctx, etdb->tdb, count);
		if (!r) {
			break;
		}

		status = evlog_tdb_entry_to_evt_entry(mem_ctx, r, &e);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}

		endoffset += ndr_size_EVENTLOGRECORD(&e, 0);

		ADD_TO_ARRAY(mem_ctx, struct EVENTLOGRECORD, e, &evt.records, &num_records);
		count++;
	}

	evt.hdr.StartOffset		= 0x30;
	evt.hdr.EndOffset		= evt.hdr.StartOffset + endoffset;
	evt.hdr.CurrentRecordNumber	= count;
	evt.hdr.OldestRecordNumber	= 1;
	evt.hdr.MaxSize			= tdb_fetch_int32(etdb->tdb, EVT_MAXSIZE);
	evt.hdr.Flags			= 0;
	evt.hdr.Retention		= tdb_fetch_int32(etdb->tdb, EVT_RETENTION);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(EVENTLOGHEADER, &evt.hdr);
	}

	evt.eof.BeginRecord		= 0x30;
	evt.eof.EndRecord		= evt.hdr.StartOffset + endoffset;
	evt.eof.CurrentRecordNumber	= evt.hdr.CurrentRecordNumber;
	evt.eof.OldestRecordNumber	= evt.hdr.OldestRecordNumber;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(EVENTLOGEOF, &evt.eof);
	}

	ndr_err = ndr_push_struct_blob(&blob, mem_ctx, &evt,
		   (ndr_push_flags_fn_t)ndr_push_EVENTLOG_EVT_FILE);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		goto done;
	}

	*blob_p = blob;
	*num_records_p = num_records;

 done:
	return status;
}
