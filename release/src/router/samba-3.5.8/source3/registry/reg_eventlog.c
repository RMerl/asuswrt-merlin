
/* 
 *  Unix SMB/CIFS implementation.
 *  Virtual Windows Registry Layer
 *  Copyright (C) Marcin Krzysztof Porwit    2005,
 *  Copyright (C) Brian Moran                2005.
 *  Copyright (C) Gerald (Jerry) Carter      2005.
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

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_REGISTRY

/**********************************************************************
 for an eventlog, add in the default values
*********************************************************************/

bool eventlog_init_keys(void)
{
	/* Find all of the eventlogs, add keys for each of them */
	const char **elogs = lp_eventlog_list();
	char *evtlogpath = NULL;
	char *evtfilepath = NULL;
	struct regsubkey_ctr *subkeys;
	struct regval_ctr *values;
	uint32 uiMaxSize;
	uint32 uiRetention;
	uint32 uiCategoryCount;
	DATA_BLOB data;
	TALLOC_CTX *ctx = talloc_tos();
	WERROR werr;

	while (elogs && *elogs) {
		werr = regsubkey_ctr_init(ctx, &subkeys);
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG( 0, ( "talloc() failure!\n" ) );
			return False;
		}
		regdb_fetch_keys(KEY_EVENTLOG, subkeys);
		regsubkey_ctr_addkey( subkeys, *elogs );
		if ( !regdb_store_keys( KEY_EVENTLOG, subkeys ) ) {
			TALLOC_FREE(subkeys);
			return False;
		}
		TALLOC_FREE(subkeys);

		/* add in the key of form KEY_EVENTLOG/Application */
		DEBUG( 5,
		       ( "Adding key of [%s] to path of [%s]\n", *elogs,
			 KEY_EVENTLOG ) );

		evtlogpath = talloc_asprintf(ctx, "%s\\%s",
			  KEY_EVENTLOG, *elogs);
		if (!evtlogpath) {
			return false;
		}
		/* add in the key of form KEY_EVENTLOG/Application/Application */
		DEBUG( 5,
		       ( "Adding key of [%s] to path of [%s]\n", *elogs,
			 evtlogpath ) );
		werr = regsubkey_ctr_init(ctx, &subkeys);
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG( 0, ( "talloc() failure!\n" ) );
			return False;
		}
		regdb_fetch_keys( evtlogpath, subkeys );
		regsubkey_ctr_addkey( subkeys, *elogs );

		if ( !regdb_store_keys( evtlogpath, subkeys ) ) {
			TALLOC_FREE(subkeys);
			return False;
		}
		TALLOC_FREE( subkeys );

		/* now add the values to the KEY_EVENTLOG/Application form key */
		if (!(values = TALLOC_ZERO_P(ctx, struct regval_ctr))) {
			DEBUG( 0, ( "talloc() failure!\n" ) );
			return False;
		}
		DEBUG( 5,
		       ( "Storing values to eventlog path of [%s]\n",
			 evtlogpath ) );
		regdb_fetch_values( evtlogpath, values );


		if (!regval_ctr_key_exists(values, "MaxSize")) {

			/* assume we have none, add them all */

			/* hard code some initial values */

			/* uiDisplayNameId = 0x00000100; */
			uiMaxSize = 0x00080000;
			uiRetention = 0x93A80;

			regval_ctr_addvalue(values, "MaxSize", REG_DWORD,
					     (char *)&uiMaxSize,
					     sizeof(uint32));

			regval_ctr_addvalue(values, "Retention", REG_DWORD,
					     (char *)&uiRetention,
					     sizeof(uint32));

			regval_ctr_addvalue_sz(values, "PrimaryModule", *elogs);
			push_reg_sz(talloc_tos(), &data, *elogs);

			regval_ctr_addvalue(values, "Sources", REG_MULTI_SZ,
					     (char *)data.data,
					     data.length);

			evtfilepath = talloc_asprintf(ctx,
					"%%SystemRoot%%\\system32\\config\\%s.tdb",
					*elogs);
			if (!evtfilepath) {
				TALLOC_FREE(values);
			}
			push_reg_sz(talloc_tos(), &data, evtfilepath);
			regval_ctr_addvalue(values, "File", REG_EXPAND_SZ, (char *)data.data,
					     data.length);
			regdb_store_values(evtlogpath, values);

		}

		TALLOC_FREE(values);

		/* now do the values under KEY_EVENTLOG/Application/Application */
		TALLOC_FREE(evtlogpath);
		evtlogpath = talloc_asprintf(ctx, "%s\\%s\\%s",
			  KEY_EVENTLOG, *elogs, *elogs);
		if (!evtlogpath) {
			return false;
		}
		if (!(values = TALLOC_ZERO_P(ctx, struct regval_ctr))) {
			DEBUG( 0, ( "talloc() failure!\n" ) );
			return False;
		}
		DEBUG( 5,
		       ( "Storing values to eventlog path of [%s]\n",
			 evtlogpath));
		regdb_fetch_values(evtlogpath, values);
		if (!regval_ctr_key_exists( values, "CategoryCount")) {

			/* hard code some initial values */

			uiCategoryCount = 0x00000007;
			regval_ctr_addvalue( values, "CategoryCount",
					     REG_DWORD,
					     ( char * ) &uiCategoryCount,
					     sizeof( uint32 ) );
			push_reg_sz(talloc_tos(), &data,
				      "%SystemRoot%\\system32\\eventlog.dll");

			regval_ctr_addvalue( values, "CategoryMessageFile",
					     REG_EXPAND_SZ,
					     ( char * ) data.data,
					     data.length);
			regdb_store_values( evtlogpath, values );
		}
		TALLOC_FREE(values);
		elogs++;
	}

	return true;
}

/*********************************************************************
 for an eventlog, add in a source name. If the eventlog doesn't
 exist (not in the list) do nothing.   If a source for the log
 already exists, change the information (remove, replace)
*********************************************************************/

bool eventlog_add_source( const char *eventlog, const char *sourcename,
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
		DEBUG( 0,
		       ( "Eventlog [%s] not found in list of valid event logs\n",
			 eventlog ) );
		return false;	/* invalid named passed in */
	}

	/* have to assume that the evenlog key itself exists at this point */
	/* add in a key of [sourcename] under the eventlog key */

	/* todo add to Sources */

	if (!( values = TALLOC_ZERO_P(ctx, struct regval_ctr))) {
		DEBUG( 0, ( "talloc() failure!\n" ));
		return false;
	}

	evtlogpath = talloc_asprintf(ctx, "%s\\%s", KEY_EVENTLOG, eventlog);
	if (!evtlogpath) {
		TALLOC_FREE(values);
		return false;
	}

	regdb_fetch_values( evtlogpath, values );


	if ( !( rval = regval_ctr_getvalue( values, "Sources" ) ) ) {
		DEBUG( 0, ( "No Sources value for [%s]!\n", eventlog ) );
		return False;
	}
	/* perhaps this adding a new string to a multi_sz should be a fn? */
	/* check to see if it's there already */

	if ( rval->type != REG_MULTI_SZ ) {
		DEBUG( 0,
		       ( "Wrong type for Sources, should be REG_MULTI_SZ\n" ) );
		return False;
	}
	/* convert to a 'regulah' chars to do some comparisons */

	already_in = False;
	wrklist = NULL;
	dump_data( 1, rval->data_p, rval->size );

	blob = data_blob_const(rval->data_p, rval->size);
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
				DEBUG( 5,
				       ( "Source name [%s] already in list for [%s] \n",
					 sourcename, eventlog ) );
				already_in = True;
				break;
			}
			wp++;
		}
	} else {
		DEBUG( 3,
		       ( "Nothing in the sources list, this might be a problem\n" ) );
	}

	wp = wrklist;

	if ( !already_in ) {
		/* make a new list with an additional entry; copy values, add another */
		wp = TALLOC_ARRAY(ctx, const char *, numsources + 2 );

		if ( !wp ) {
			DEBUG( 0, ( "talloc() failed \n" ) );
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
				     ( char * ) blob.data, blob.length);
		regdb_store_values( evtlogpath, values );
		data_blob_free(&blob);
	} else {
		DEBUG( 3,
		       ( "Source name [%s] found in existing list of sources\n",
			 sourcename ) );
	}
	TALLOC_FREE(values);
	TALLOC_FREE(wrklist);	/*  */

	werr = regsubkey_ctr_init(ctx, &subkeys);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG( 0, ( "talloc() failure!\n" ) );
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
		DEBUG( 5,
		       ( " Source name [%s] for eventlog [%s] didn't exist, adding \n",
			 sourcename, eventlog ) );
		regsubkey_ctr_addkey( subkeys, sourcename );
		if ( !regdb_store_keys( evtlogpath, subkeys ) )
			return False;
	}
	TALLOC_FREE(subkeys);

	/* at this point KEY_EVENTLOG/<eventlog>/<sourcename> key is in there. Now need to add EventMessageFile */

	/* now allocate room for the source's subkeys */

	werr = regsubkey_ctr_init(ctx, &subkeys);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG( 0, ( "talloc() failure!\n" ) );
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
	if ( !( values = TALLOC_ZERO_P(ctx, struct regval_ctr ) ) ) {
		DEBUG( 0, ( "talloc() failure!\n" ) );
		return False;
	}
	DEBUG( 5,
	       ( "Storing EventMessageFile [%s] to eventlog path of [%s]\n",
		 messagefile, evtlogpath ) );

	regdb_fetch_values( evtlogpath, values );

	regval_ctr_addvalue_sz(values, "EventMessageFile", messagefile);
	regdb_store_values( evtlogpath, values );

	TALLOC_FREE(values);

	return True;
}
