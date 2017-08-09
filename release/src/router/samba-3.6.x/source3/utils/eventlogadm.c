
/*
 * Samba Unix/Linux SMB client utility 
 * Write Eventlog records to a tdb, perform other eventlog related functions
 *
 *
 * Copyright (C) Brian Moran                2005.
 * Copyright (C) Guenther Deschner          2009.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */


#include "includes.h"
#include "lib/eventlog/eventlog.h"
#include "registry.h"
#include "registry/reg_backend_db.h"
#include "registry/reg_objects.h"
#include "../libcli/registry/util_reg.h"

extern int optind;
extern char *optarg;

int opt_debug = 0;

static void usage( char *s )
{
	printf( "\nUsage: %s [OPTION]\n\n", s );
	printf( " -o write <Eventlog Name> \t\t\t\t\tWrites records to eventlog from STDIN\n" );
	printf( " -o addsource <EventlogName> <sourcename> <msgfileDLLname> \tAdds the specified source & DLL eventlog registry entry\n" );
	printf( " -o dump <Eventlog Name> <starting_record>\t\t\t\t\tDump stored eventlog entries on STDOUT\n" );
	printf( "\nMiscellaneous options:\n" );
	printf( " -s <filename>\t\t\t\t\t\t\tUse configuration file <filename>.\n");
	printf( " -d\t\t\t\t\t\t\t\tturn debug on\n" );
	printf( " -h\t\t\t\t\t\t\t\tdisplay help\n\n" );
}

static void display_eventlog_names( void )
{
	const char **elogs;
	int i;

	elogs = lp_eventlog_list(  );
	printf( "Active eventlog names:\n" );
	printf( "--------------------------------------\n" );
	if ( elogs ) {
		for ( i = 0; elogs[i]; i++ ) {
			printf( "\t%s\n", elogs[i] );
		}
	} 
	else
		printf( "\t<None specified>\n");
}

/*********************************************************************
 for an eventlog, add in a source name. If the eventlog doesn't
 exist (not in the list) do nothing.   If a source for the log
 already exists, change the information (remove, replace)
*********************************************************************/
static bool eventlog_add_source( const char *eventlog, const char *sourcename,
				 const char *messagefile )
{
	/* Find all of the eventlogs, add keys for each of them */
	/* need to add to the value KEY_EVENTLOG/<eventlog>/Sources string (Creating if necessary)
	   need to add KEY of source to KEY_EVENTLOG/<eventlog>/<source> */

	const char **elogs = lp_eventlog_list(  );
	const char **wrklist, **wp;
	char *evtlogpath = NULL;
	struct regsubkey_ctr *subkeys;
	struct regval_ctr *values;
	struct regval_blob *rval;
	int ii = 0;
	bool already_in;
	int i;
	int numsources = 0;
	TALLOC_CTX *ctx = talloc_tos();
	WERROR werr;
	DATA_BLOB blob;

	if (!elogs) {
		return False;
	}

	for ( i = 0; elogs[i]; i++ ) {
		if ( strequal( elogs[i], eventlog ) )
			break;
	}

	if ( !elogs[i] ) {
		d_printf("Eventlog [%s] not found in list of valid event logs\n",
			 eventlog);
		return false;	/* invalid named passed in */
	}

	/* have to assume that the evenlog key itself exists at this point */
	/* add in a key of [sourcename] under the eventlog key */

	/* todo add to Sources */

	werr = regval_ctr_init(ctx, &values);
	if(!W_ERROR_IS_OK(werr)) {
		d_printf("talloc() failure!\n");
		return false;
	}

	evtlogpath = talloc_asprintf(ctx, "%s\\%s", KEY_EVENTLOG, eventlog);
	if (!evtlogpath) {
		TALLOC_FREE(values);
		return false;
	}

	regdb_fetch_values( evtlogpath, values );


	if ( !( rval = regval_ctr_getvalue( values, "Sources" ) ) ) {
		d_printf("No Sources value for [%s]!\n", eventlog);
		return False;
	}
	/* perhaps this adding a new string to a multi_sz should be a fn? */
	/* check to see if it's there already */

	if ( regval_type(rval) != REG_MULTI_SZ ) {
		d_printf("Wrong type for Sources, should be REG_MULTI_SZ\n");
		return False;
	}
	/* convert to a 'regulah' chars to do some comparisons */

	already_in = False;
	wrklist = NULL;
	dump_data(1, regval_data_p(rval), regval_size(rval));

	blob = data_blob_const(regval_data_p(rval), regval_size(rval));
	if (!pull_reg_multi_sz(talloc_tos(), &blob, &wrklist)) {
		return false;
	}

	for (ii=0; wrklist[ii]; ii++) {
		numsources++;
	}

	if (numsources > 0) {
		/* see if it's in there already */
		wp = wrklist;

		while (wp && *wp ) {
			if ( strequal( *wp, sourcename ) ) {
				d_printf("Source name [%s] already in list for [%s] \n",
					 sourcename, eventlog);
				already_in = True;
				break;
			}
			wp++;
		}
	} else {
		d_printf("Nothing in the sources list, this might be a problem\n");
	}

	wp = wrklist;

	if ( !already_in ) {
		/* make a new list with an additional entry; copy values, add another */
		wp = TALLOC_ARRAY(ctx, const char *, numsources + 2 );

		if ( !wp ) {
			d_printf("talloc() failed \n");
			return False;
		}
		memcpy( wp, wrklist, sizeof( char * ) * numsources );
		*( wp + numsources ) = ( char * ) sourcename;
		*( wp + numsources + 1 ) = NULL;
		if (!push_reg_multi_sz(ctx, &blob, wp)) {
			return false;
		}
		dump_data( 1, blob.data, blob.length);
		regval_ctr_addvalue( values, "Sources", REG_MULTI_SZ,
				     blob.data, blob.length);
		regdb_store_values( evtlogpath, values );
		data_blob_free(&blob);
	} else {
		d_printf("Source name [%s] found in existing list of sources\n",
			 sourcename);
	}
	TALLOC_FREE(values);
	TALLOC_FREE(wrklist);	/*  */

	werr = regsubkey_ctr_init(ctx, &subkeys);
	if (!W_ERROR_IS_OK(werr)) {
		d_printf("talloc() failure!\n");
		return False;
	}
	TALLOC_FREE(evtlogpath);
	evtlogpath = talloc_asprintf(ctx, "%s\\%s", KEY_EVENTLOG, eventlog );
	if (!evtlogpath) {
		TALLOC_FREE(subkeys);
		return false;
	}

	regdb_fetch_keys( evtlogpath, subkeys );

	if ( !regsubkey_ctr_key_exists( subkeys, sourcename ) ) {
		d_printf(" Source name [%s] for eventlog [%s] didn't exist, adding \n",
			 sourcename, eventlog);
		regsubkey_ctr_addkey( subkeys, sourcename );
		if ( !regdb_store_keys( evtlogpath, subkeys ) )
			return False;
	}
	TALLOC_FREE(subkeys);

	/* at this point KEY_EVENTLOG/<eventlog>/<sourcename> key is in there. Now need to add EventMessageFile */

	/* now allocate room for the source's subkeys */

	werr = regsubkey_ctr_init(ctx, &subkeys);
	if (!W_ERROR_IS_OK(werr)) {
		d_printf("talloc() failure!\n");
		return False;
	}
	TALLOC_FREE(evtlogpath);
	evtlogpath = talloc_asprintf(ctx, "%s\\%s\\%s",
		  KEY_EVENTLOG, eventlog, sourcename);
	if (!evtlogpath) {
		TALLOC_FREE(subkeys);
		return false;
	}

	regdb_fetch_keys( evtlogpath, subkeys );

	/* now add the values to the KEY_EVENTLOG/Application form key */
	werr = regval_ctr_init(ctx, &values);
	if (!W_ERROR_IS_OK(werr)) {
		d_printf("talloc() failure!\n");
		return False;
	}
	d_printf("Storing EventMessageFile [%s] to eventlog path of [%s]\n",
		 messagefile, evtlogpath);

	regdb_fetch_values( evtlogpath, values );

	regval_ctr_addvalue_sz(values, "EventMessageFile", messagefile);
	regdb_store_values( evtlogpath, values );

	TALLOC_FREE(values);

	return True;
}

static int DoAddSourceCommand( int argc, char **argv, bool debugflag, char *exename )
{

	if ( argc < 3 ) {
		printf( "need more arguments:\n" );
		printf( "-o addsource EventlogName SourceName /path/to/EventMessageFile.dll\n" );
		return -1;
	}
	/* must open the registry before we access it */
	if (!W_ERROR_IS_OK(regdb_init())) {
		printf( "Can't open the registry.\n" );
		return -1;
	}

	if ( !eventlog_add_source( argv[0], argv[1], argv[2] ) )
		return -2;
	return 0;
}

static int DoWriteCommand( int argc, char **argv, bool debugflag, char *exename )
{
	FILE *f1;
	char *argfname;
	ELOG_TDB *etdb;
	NTSTATUS status;

	/* fixed constants are bad bad bad  */
	char linein[1024];
	bool is_eor;
	struct eventlog_Record_tdb ee;
	uint32_t record_number = 0;
	TALLOC_CTX *mem_ctx = talloc_tos();

	f1 = stdin;
	if ( !f1 ) {
		printf( "Can't open STDIN\n" );
		return -1;
	}

	if ( debugflag ) {
		printf( "Starting write for eventlog [%s]\n", argv[0] );
		display_eventlog_names(  );
	}

	argfname = argv[0];

	if ( !( etdb = elog_open_tdb( argfname, False, False ) ) ) {
		printf( "can't open the eventlog TDB (%s)\n", argfname );
		return -1;
	}

	ZERO_STRUCT( ee );	/* MUST initialize between records */

	while ( !feof( f1 ) ) {
		if (fgets( linein, sizeof( linein ) - 1, f1 ) == NULL) {
			break;
		}
		if ((strlen(linein) > 0)
		    && (linein[strlen(linein)-1] == '\n')) {
			linein[strlen(linein)-1] = 0;
		}

		if ( debugflag )
			printf( "Read line [%s]\n", linein );

		is_eor = False;


		parse_logentry( mem_ctx, ( char * ) &linein, &ee, &is_eor );
		/* should we do something with the return code? */

		if ( is_eor ) {
			fixup_eventlog_record_tdb( &ee );

			if ( opt_debug )
				printf( "record number [%d], tg [%d] , tw [%d]\n",
					ee.record_number, (int)ee.time_generated, (int)ee.time_written );

			if ( ee.time_generated != 0 ) {

				/* printf("Writing to the event log\n"); */

				status = evlog_push_record_tdb( mem_ctx, ELOG_TDB_CTX(etdb),
								&ee, &record_number );
				if ( !NT_STATUS_IS_OK(status) ) {
					printf( "Can't write to the event log: %s\n",
						nt_errstr(status) );
				} else {
					if ( opt_debug )
						printf( "Wrote record %d\n",
							record_number );
				}
			} else {
				if ( opt_debug )
					printf( "<null record>\n" );
			}
			ZERO_STRUCT( ee );	/* MUST initialize between records */
		}
	}

	elog_close_tdb( etdb , False );

	return 0;
}

static int DoDumpCommand(int argc, char **argv, bool debugflag, char *exename)
{
	ELOG_TDB *etdb;
	TALLOC_CTX *mem_ctx = talloc_tos();
	const char *tdb_filename;
	uint32_t count = 1;

	if (argc > 2) {
		return -1;
	}

	tdb_filename = argv[0];

	if (argc > 1) {
		count = atoi(argv[1]);
	}

	etdb = elog_open_tdb(argv[0], false, true);
	if (!etdb) {
		printf("can't open the eventlog TDB (%s)\n", argv[0]);
		return -1;
	}

	while (1) {

		struct eventlog_Record_tdb *r;
		char *s;

		r = evlog_pull_record_tdb(mem_ctx, etdb->tdb, count);
		if (!r) {
			break;
		}

		printf("displaying record: %d\n", count);

		s = NDR_PRINT_STRUCT_STRING(mem_ctx, eventlog_Record_tdb, r);
		if (s) {
			printf("%s\n", s);
			talloc_free(s);
		}
		count++;
	}

	elog_close_tdb(etdb, false);

	return 0;
}

/* would be nice to use the popT stuff here, however doing so forces us to drag in a lot of other infrastructure */

int main( int argc, char *argv[] )
{
	int opt, rc;
	char *exename;
	char *configfile = NULL;
	TALLOC_CTX *frame = talloc_stackframe();


	fstring opname;

	load_case_tables();

	opt_debug = 0;		/* todo set this from getopts */

	exename = argv[0];

	/* default */

	fstrcpy( opname, "write" );	/* the default */

#if 0				/* TESTING CODE */
	eventlog_add_source( "System", "TestSourceX", "SomeTestPathX" );
#endif
	while ( ( opt = getopt( argc, argv, "dho:s:" ) ) != EOF ) {
		switch ( opt ) {

		case 'o':
			fstrcpy( opname, optarg );
			break;

		case 'h':
			usage( exename );
			display_eventlog_names(  );
			exit( 0 );
			break;

		case 'd':
			opt_debug = 1;
			break;
		case 's':
			configfile = talloc_strdup(frame, optarg);
			break;

		}
	}

	argc -= optind;
	argv += optind;

	if ( argc < 1 ) {
		printf( "\nNot enough arguments!\n" );
		usage( exename );
		exit( 1 );
	}

	if ( configfile == NULL ) {
		lp_load(get_dyn_CONFIGFILE(), True, False, False, True);
	} else if (!lp_load(configfile, True, False, False, True)) {
		printf("Unable to parse configfile '%s'\n",configfile);
		exit( 1 );
	}

	/*  note that the separate command types should call usage if they need to... */
	while ( 1 ) {
		if ( !StrCaseCmp( opname, "addsource" ) ) {
			rc = DoAddSourceCommand( argc, argv, opt_debug,
						 exename );
			break;
		}
		if ( !StrCaseCmp( opname, "write" ) ) {
			rc = DoWriteCommand( argc, argv, opt_debug, exename );
			break;
		}
		if ( !StrCaseCmp( opname, "dump" ) ) {
			rc = DoDumpCommand( argc, argv, opt_debug, exename );
			break;
		}
		printf( "unknown command [%s]\n", opname );
		usage( exename );
		exit( 1 );
		break;
	}
	TALLOC_FREE(frame);
	return rc;
}
