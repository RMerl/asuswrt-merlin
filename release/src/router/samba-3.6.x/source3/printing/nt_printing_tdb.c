/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (c) Andrew Tridgell              1992-2000,
 *  Copyright (c) Jean François Micouleau      1998-2000.
 *  Copyright (c) Gerald Carter                2002-2005.
 *  Copyright (c) Andreas Schneider            2010.
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
#include "printing/nt_printing_tdb.h"
#include "librpc/gen_ndr/spoolss.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "libcli/security/security.h"
#include "util_tdb.h"

#define FORMS_PREFIX "FORMS/"
#define DRIVERS_PREFIX "DRIVERS/"
#define PRINTERS_PREFIX "PRINTERS/"
#define SECDESC_PREFIX "SECDESC/"

#define NTDRIVERS_DATABASE_VERSION_1 1
#define NTDRIVERS_DATABASE_VERSION_2 2
#define NTDRIVERS_DATABASE_VERSION_3 3 /* little endian version of v2 */
#define NTDRIVERS_DATABASE_VERSION_4 4 /* fix generic bits in security descriptors */
#define NTDRIVERS_DATABASE_VERSION_5 5 /* normalize keys in ntprinters.tdb */

static TDB_CONTEXT *tdb_forms; /* used for forms files */
static TDB_CONTEXT *tdb_drivers; /* used for driver files */
static TDB_CONTEXT *tdb_printers; /* used for printers files */

/****************************************************************************
 generate a new TDB_DATA key for storing a printer
****************************************************************************/

static TDB_DATA make_printer_tdbkey(TALLOC_CTX *ctx, const char *sharename )
{
	fstring share;
	char *keystr = NULL;
	TDB_DATA key;

	fstrcpy(share, sharename);
	strlower_m(share);

	keystr = talloc_asprintf(ctx, "%s%s", PRINTERS_PREFIX, share);
	key = string_term_tdb_data(keystr ? keystr : "");

	return key;
}

/****************************************************************************
 generate a new TDB_DATA key for storing a printer security descriptor
****************************************************************************/

static TDB_DATA make_printers_secdesc_tdbkey(TALLOC_CTX *ctx,
					const char* sharename  )
{
	fstring share;
	char *keystr = NULL;
	TDB_DATA key;

	fstrcpy(share, sharename );
	strlower_m(share);

	keystr = talloc_asprintf(ctx, "%s%s", SECDESC_PREFIX, share);
	key = string_term_tdb_data(keystr ? keystr : "");

	return key;
}

/****************************************************************************
 Upgrade the tdb files to version 3
****************************************************************************/

static bool upgrade_to_version_3(void)
{
	TDB_DATA kbuf, newkey, dbuf;

	DEBUG(0,("upgrade_to_version_3: upgrading print tdb's to version 3\n"));

	for (kbuf = tdb_firstkey(tdb_drivers); kbuf.dptr;
			newkey = tdb_nextkey(tdb_drivers, kbuf), free(kbuf.dptr), kbuf=newkey) {

		dbuf = tdb_fetch(tdb_drivers, kbuf);

		if (strncmp((const char *)kbuf.dptr, FORMS_PREFIX, strlen(FORMS_PREFIX)) == 0) {
			DEBUG(0,("upgrade_to_version_3:moving form\n"));
			if (tdb_store(tdb_forms, kbuf, dbuf, TDB_REPLACE) != 0) {
				SAFE_FREE(dbuf.dptr);
				DEBUG(0,("upgrade_to_version_3: failed to move form. Error (%s).\n", tdb_errorstr(tdb_forms)));
				return False;
			}
			if (tdb_delete(tdb_drivers, kbuf) != 0) {
				SAFE_FREE(dbuf.dptr);
				DEBUG(0,("upgrade_to_version_3: failed to delete form. Error (%s)\n", tdb_errorstr(tdb_drivers)));
				return False;
			}
		}

		if (strncmp((const char *)kbuf.dptr, PRINTERS_PREFIX, strlen(PRINTERS_PREFIX)) == 0) {
			DEBUG(0,("upgrade_to_version_3:moving printer\n"));
			if (tdb_store(tdb_printers, kbuf, dbuf, TDB_REPLACE) != 0) {
				SAFE_FREE(dbuf.dptr);
				DEBUG(0,("upgrade_to_version_3: failed to move printer. Error (%s)\n", tdb_errorstr(tdb_printers)));
				return False;
			}
			if (tdb_delete(tdb_drivers, kbuf) != 0) {
				SAFE_FREE(dbuf.dptr);
				DEBUG(0,("upgrade_to_version_3: failed to delete printer. Error (%s)\n", tdb_errorstr(tdb_drivers)));
				return False;
			}
		}

		if (strncmp((const char *)kbuf.dptr, SECDESC_PREFIX, strlen(SECDESC_PREFIX)) == 0) {
			DEBUG(0,("upgrade_to_version_3:moving secdesc\n"));
			if (tdb_store(tdb_printers, kbuf, dbuf, TDB_REPLACE) != 0) {
				SAFE_FREE(dbuf.dptr);
				DEBUG(0,("upgrade_to_version_3: failed to move secdesc. Error (%s)\n", tdb_errorstr(tdb_printers)));
				return False;
			}
			if (tdb_delete(tdb_drivers, kbuf) != 0) {
				SAFE_FREE(dbuf.dptr);
				DEBUG(0,("upgrade_to_version_3: failed to delete secdesc. Error (%s)\n", tdb_errorstr(tdb_drivers)));
				return False;
			}
		}

		SAFE_FREE(dbuf.dptr);
	}

	return True;
}

/*******************************************************************
 Fix an issue with security descriptors.  Printer sec_desc must
 use more than the generic bits that were previously used
 in <= 3.0.14a.  They must also have a owner and group SID assigned.
 Otherwise, any printers than have been migrated to a Windows
 host using printmig.exe will not be accessible.
*******************************************************************/

static int sec_desc_upg_fn( TDB_CONTEXT *the_tdb, TDB_DATA key,
                            TDB_DATA data, void *state )
{
	NTSTATUS status;
	struct sec_desc_buf *sd_orig = NULL;
	struct sec_desc_buf *sd_new, *sd_store;
	struct security_descriptor *sec, *new_sec;
	TALLOC_CTX *ctx = state;
	int result, i;
	uint32 sd_size;
	size_t size_new_sec;

	if (!data.dptr || data.dsize == 0) {
		return 0;
	}

	if ( strncmp((const char *) key.dptr, SECDESC_PREFIX, strlen(SECDESC_PREFIX) ) != 0 ) {
		return 0;
	}

	/* upgrade the security descriptor */

	status = unmarshall_sec_desc_buf(ctx, data.dptr, data.dsize, &sd_orig);
	if (!NT_STATUS_IS_OK(status)) {
		/* delete bad entries */
		DEBUG(0,("sec_desc_upg_fn: Failed to parse original sec_desc for %si.  Deleting....\n",
			(const char *)key.dptr ));
		tdb_delete( tdb_printers, key );
		return 0;
	}

	if (!sd_orig) {
		return 0;
	}
	sec = sd_orig->sd;

	/* is this even valid? */

	if ( !sec->dacl ) {
		return 0;
	}

	/* update access masks */

	for ( i=0; i<sec->dacl->num_aces; i++ ) {
		switch ( sec->dacl->aces[i].access_mask ) {
			case (GENERIC_READ_ACCESS | GENERIC_WRITE_ACCESS | GENERIC_EXECUTE_ACCESS):
				sec->dacl->aces[i].access_mask = PRINTER_ACE_PRINT;
				break;

			case GENERIC_ALL_ACCESS:
				sec->dacl->aces[i].access_mask = PRINTER_ACE_FULL_CONTROL;
				break;

			case READ_CONTROL_ACCESS:
				sec->dacl->aces[i].access_mask = PRINTER_ACE_MANAGE_DOCUMENTS;

			default:	/* no change */
				break;
		}
	}

	/* create a new struct security_descriptor with the appropriate owner and group SIDs */

	new_sec = make_sec_desc( ctx, SD_REVISION, SEC_DESC_SELF_RELATIVE,
				 &global_sid_Builtin_Administrators,
				 &global_sid_Builtin_Administrators,
				 NULL, NULL, &size_new_sec );
	if (!new_sec) {
		return 0;
	}
	sd_new = make_sec_desc_buf( ctx, size_new_sec, new_sec );
	if (!sd_new) {
		return 0;
	}

	if ( !(sd_store = sec_desc_merge_buf( ctx, sd_new, sd_orig )) ) {
		DEBUG(0,("sec_desc_upg_fn: Failed to update sec_desc for %s\n", key.dptr ));
		return 0;
	}

	/* store it back */

	sd_size = ndr_size_security_descriptor(sd_store->sd, 0)
		+ sizeof(struct sec_desc_buf);

	status = marshall_sec_desc_buf(ctx, sd_store, &data.dptr, &data.dsize);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("sec_desc_upg_fn: Failed to parse new sec_desc for %s\n", key.dptr ));
		return 0;
	}

	result = tdb_store( tdb_printers, key, data, TDB_REPLACE );

	/* 0 to continue and non-zero to stop traversal */

	return (result == -1);
}

/*******************************************************************
 Upgrade the tdb files to version 4
*******************************************************************/

static bool upgrade_to_version_4(void)
{
	TALLOC_CTX *ctx;
	int result;

	DEBUG(0,("upgrade_to_version_4: upgrading printer security descriptors\n"));

	if ( !(ctx = talloc_init( "upgrade_to_version_4" )) )
		return False;

	result = tdb_traverse( tdb_printers, sec_desc_upg_fn, ctx );

	talloc_destroy( ctx );

	return ( result != -1 );
}

/*******************************************************************
 Fix an issue with security descriptors.  Printer sec_desc must
 use more than the generic bits that were previously used
 in <= 3.0.14a.  They must also have a owner and group SID assigned.
 Otherwise, any printers than have been migrated to a Windows
 host using printmig.exe will not be accessible.
*******************************************************************/

static int normalize_printers_fn( TDB_CONTEXT *the_tdb, TDB_DATA key,
                                  TDB_DATA data, void *state )
{
	TALLOC_CTX *ctx = talloc_tos();
	TDB_DATA new_key;

	if (!data.dptr || data.dsize == 0)
		return 0;

	/* upgrade printer records and security descriptors */

	if ( strncmp((const char *) key.dptr, PRINTERS_PREFIX, strlen(PRINTERS_PREFIX) ) == 0 ) {
		new_key = make_printer_tdbkey(ctx, (const char *)key.dptr+strlen(PRINTERS_PREFIX) );
	}
	else if ( strncmp((const char *) key.dptr, SECDESC_PREFIX, strlen(SECDESC_PREFIX) ) == 0 ) {
		new_key = make_printers_secdesc_tdbkey(ctx, (const char *)key.dptr+strlen(SECDESC_PREFIX) );
	}
	else {
		/* ignore this record */
		return 0;
	}

	/* delete the original record and store under the normalized key */

	if ( tdb_delete( the_tdb, key ) != 0 ) {
		DEBUG(0,("normalize_printers_fn: tdb_delete for [%s] failed!\n",
			key.dptr));
		return 1;
	}

	if ( tdb_store( the_tdb, new_key, data, TDB_REPLACE) != 0 ) {
		DEBUG(0,("normalize_printers_fn: failed to store new record for [%s]!\n",
			key.dptr));
		return 1;
	}

	return 0;
}

/*******************************************************************
 Upgrade the tdb files to version 5
*******************************************************************/

static bool upgrade_to_version_5(void)
{
	TALLOC_CTX *ctx;
	int result;

	DEBUG(0,("upgrade_to_version_5: normalizing printer keys\n"));

	if ( !(ctx = talloc_init( "upgrade_to_version_5" )) )
		return False;

	result = tdb_traverse( tdb_printers, normalize_printers_fn, NULL );

	talloc_destroy( ctx );

	return ( result != -1 );
}

bool nt_printing_tdb_upgrade(void)
{
	const char *drivers_path = state_path("ntdrivers.tdb");
	const char *printers_path = state_path("ntprinters.tdb");
	const char *forms_path = state_path("ntforms.tdb");
	bool drivers_exists = file_exist(drivers_path);
	bool printers_exists = file_exist(printers_path);
	bool forms_exists = file_exist(forms_path);
	const char *vstring = "INFO/version";
	int32_t vers_id;

	if (!drivers_exists && !printers_exists && !forms_exists) {
		return true;
	}

	tdb_drivers = tdb_open_log(drivers_path,
				   0,
				   TDB_DEFAULT,
				   O_RDWR|O_CREAT,
				   0600);
	if (tdb_drivers == NULL) {
		DEBUG(0,("nt_printing_init: Failed to open nt drivers "
			 "database %s (%s)\n",
			 drivers_path, strerror(errno)));
		return false;
	}

	tdb_printers = tdb_open_log(printers_path,
				    0,
				    TDB_DEFAULT,
				    O_RDWR|O_CREAT,
				    0600);
	if (tdb_printers == NULL) {
		DEBUG(0,("nt_printing_init: Failed to open nt printers "
			 "database %s (%s)\n",
			 printers_path, strerror(errno)));
		return false;
	}

	tdb_forms = tdb_open_log(forms_path,
				 0,
				 TDB_DEFAULT,
				 O_RDWR|O_CREAT,
				 0600);
	if (tdb_forms == NULL) {
		DEBUG(0,("nt_printing_init: Failed to open nt forms "
			 "database %s (%s)\n",
			 forms_path, strerror(errno)));
		return false;
	}

	/* Samba upgrade */
	vers_id = tdb_fetch_int32(tdb_drivers, vstring);
	if (vers_id == -1) {
		DEBUG(10, ("Fresh database\n"));
		tdb_store_int32(tdb_drivers, vstring, NTDRIVERS_DATABASE_VERSION_5);
		vers_id = NTDRIVERS_DATABASE_VERSION_5;
	}

	if (vers_id != NTDRIVERS_DATABASE_VERSION_5) {
		if ((vers_id == NTDRIVERS_DATABASE_VERSION_1) ||
		    (IREV(vers_id) == NTDRIVERS_DATABASE_VERSION_1)) {
			if (!upgrade_to_version_3()) {
				return false;
			}

			tdb_store_int32(tdb_drivers, vstring, NTDRIVERS_DATABASE_VERSION_3);
			vers_id = NTDRIVERS_DATABASE_VERSION_3;
		}

		if ((vers_id == NTDRIVERS_DATABASE_VERSION_2) ||
		    (IREV(vers_id) == NTDRIVERS_DATABASE_VERSION_2)) {
			/*
			 * Written on a bigendian machine with old fetch_int
			 * code. Save as le. The only upgrade between V2 and V3
			 * is to save the version in little-endian.
			 */
			tdb_store_int32(tdb_drivers, vstring, NTDRIVERS_DATABASE_VERSION_3);
			vers_id = NTDRIVERS_DATABASE_VERSION_3;
		}

		if (vers_id == NTDRIVERS_DATABASE_VERSION_3) {
			if (!upgrade_to_version_4()) {
				return false;
			}
			tdb_store_int32(tdb_drivers, vstring, NTDRIVERS_DATABASE_VERSION_4);
			vers_id = NTDRIVERS_DATABASE_VERSION_4;
		}

		if (vers_id == NTDRIVERS_DATABASE_VERSION_4 ) {
			if (!upgrade_to_version_5()) {
				return false;
			}
			tdb_store_int32(tdb_drivers, vstring, NTDRIVERS_DATABASE_VERSION_5);
			vers_id = NTDRIVERS_DATABASE_VERSION_5;
		}

		if (vers_id != NTDRIVERS_DATABASE_VERSION_5) {
			DEBUG(0,("nt_printing_init: Unknown printer database version [%d]\n", vers_id));
			return false;
		}
	}

	if (tdb_drivers) {
		tdb_close(tdb_drivers);
		tdb_drivers = NULL;
	}

	if (tdb_printers) {
		tdb_close(tdb_printers);
		tdb_printers = NULL;
	}

	if (tdb_forms) {
		tdb_close(tdb_forms);
		tdb_forms = NULL;
	}

	return true;
}
