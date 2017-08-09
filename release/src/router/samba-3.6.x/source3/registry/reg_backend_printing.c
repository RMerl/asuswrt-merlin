/*
 *  Unix SMB/CIFS implementation.
 *  Virtual Windows Registry Layer
 *  Copyright (C) Gerald Carter                     2002-2005
 *  Copyright (c) Andreas Schneider <asn@samba.org> 2010
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

/* Implementation of registry virtual views for printing information */

#include "includes.h"
#include "registry.h"
#include "reg_util_internal.h"
#include "reg_backend_db.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_REGISTRY

/* registry paths used in the print_registry[] */
#define KEY_CONTROL_PRINTERS	"HKLM\\SYSTEM\\CURRENTCONTROLSET\\CONTROL\\PRINT\\PRINTERS"
#define KEY_WINNT_PRINTERS	"HKLM\\SOFTWARE\\MICROSOFT\\WINDOWS NT\\CURRENTVERSION\\PRINT\\PRINTERS"

/* callback table for various registry paths below the ones we service in this module */

struct reg_dyn_tree {
	/* full key path in normalized form */
	const char *path;

	/* callbscks for fetch/store operations */
	int ( *fetch_subkeys) ( const char *path, struct regsubkey_ctr *subkeys );
	bool (*store_subkeys) ( const char *path, struct regsubkey_ctr *subkeys );
	int  (*fetch_values)  ( const char *path, struct regval_ctr *values );
	bool (*store_values)  ( const char *path, struct regval_ctr *values );
};

/*********************************************************************
 *********************************************************************
 ** "HKLM/SYSTEM/CURRENTCONTROLSET/CONTROL/PRINT/PRINTERS"
 ** "HKLM/SOFTWARE/MICROSOFT/WINDOWS NT/CURRENTVERSION/PRINT/PRINTERS"
 *********************************************************************
 *********************************************************************/

static char *create_printer_registry_path(TALLOC_CTX *mem_ctx, const char *key) {
	char *path;
	char *subkey = NULL;

	path = talloc_strdup(mem_ctx, key);
	if (path == NULL) {
		return NULL;
	}

	path = normalize_reg_path(mem_ctx, path);
	if (path == NULL) {
		return NULL;
	}

	if (strncmp(path, KEY_CONTROL_PRINTERS, strlen(KEY_CONTROL_PRINTERS)) == 0) {
		subkey = reg_remaining_path(mem_ctx, key + strlen(KEY_CONTROL_PRINTERS));
		if (subkey == NULL) {
			return NULL;
		}
		return talloc_asprintf(mem_ctx, "%s\\%s", KEY_WINNT_PRINTERS, subkey);
	}

	return NULL;
}

/*********************************************************************
 *********************************************************************/

static int key_printers_fetch_keys( const char *key, struct regsubkey_ctr *subkeys )
{
	TALLOC_CTX *ctx = talloc_tos();
	char *printers_key;

	printers_key = create_printer_registry_path(ctx, key);
	if (printers_key == NULL) {
		/* normalize on the 'HKLM\SOFTWARE\....\Print\Printers' key */
		return regdb_fetch_keys(KEY_WINNT_PRINTERS, subkeys);
	}

	return regdb_fetch_keys(printers_key, subkeys);
}

/**********************************************************************
 *********************************************************************/

static bool key_printers_store_keys( const char *key, struct regsubkey_ctr *subkeys )
{
	TALLOC_CTX *ctx = talloc_tos();
	char *printers_key;

	printers_key = create_printer_registry_path(ctx, key);
	if (printers_key == NULL) {
		/* normalize on the 'HKLM\SOFTWARE\....\Print\Printers' key */
		return regdb_store_keys(KEY_WINNT_PRINTERS, subkeys);
	}

	return regdb_store_keys(printers_key, subkeys);
}

/**********************************************************************
 *********************************************************************/

static int key_printers_fetch_values(const char *key, struct regval_ctr *values)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *printers_key;

	printers_key = create_printer_registry_path(ctx, key);
	if (printers_key == NULL) {
		/* normalize on the 'HKLM\SOFTWARE\....\Print\Printers' key */
		return regdb_fetch_values(KEY_WINNT_PRINTERS, values);
	}

	return regdb_fetch_values(printers_key, values);
}

/**********************************************************************
 *********************************************************************/

static bool key_printers_store_values(const char *key, struct regval_ctr *values)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *printers_key;

	printers_key = create_printer_registry_path(ctx, key);
	if (printers_key == NULL) {
		/* normalize on the 'HKLM\SOFTWARE\....\Print\Printers' key */
		return regdb_store_values(KEY_WINNT_PRINTERS, values);
	}

	return regdb_store_values(printers_key, values);
}

/**********************************************************************
 *********************************************************************
 ** Structure to hold dispatch table of ops for various printer keys.
 ** Make sure to always store deeper keys along the same path first so
 ** we ge a more specific match.
 *********************************************************************
 *********************************************************************/

static struct reg_dyn_tree print_registry[] = {
{ KEY_CONTROL_PRINTERS,
	&key_printers_fetch_keys,
	&key_printers_store_keys,
	&key_printers_fetch_values,
	&key_printers_store_values },

{ NULL, NULL, NULL, NULL, NULL }
};


/**********************************************************************
 *********************************************************************
 ** Main reg_printing interface functions
 *********************************************************************
 *********************************************************************/

/***********************************************************************
 Lookup a key in the print_registry table, returning its index.
 -1 on failure
 **********************************************************************/

static int match_registry_path(const char *key)
{
	int i;
	char *path = NULL;
	TALLOC_CTX *ctx = talloc_tos();

	if ( !key )
		return -1;

	path = talloc_strdup(ctx, key);
	if (!path) {
		return -1;
	}
	path = normalize_reg_path(ctx, path);
	if (!path) {
		return -1;
	}

	for ( i=0; print_registry[i].path; i++ ) {
		if (strncmp( path, print_registry[i].path, strlen(print_registry[i].path) ) == 0 )
			return i;
	}

	return -1;
}

/***********************************************************************
 **********************************************************************/

static int regprint_fetch_reg_keys( const char *key, struct regsubkey_ctr *subkeys )
{
	int i = match_registry_path( key );

	if ( i == -1 )
		return -1;

	if ( !print_registry[i].fetch_subkeys )
		return -1;

	return print_registry[i].fetch_subkeys( key, subkeys );
}

/**********************************************************************
 *********************************************************************/

static bool regprint_store_reg_keys( const char *key, struct regsubkey_ctr *subkeys )
{
	int i = match_registry_path( key );

	if ( i == -1 )
		return False;

	if ( !print_registry[i].store_subkeys )
		return False;

	return print_registry[i].store_subkeys( key, subkeys );
}

/**********************************************************************
 *********************************************************************/

static int regprint_fetch_reg_values(const char *key, struct regval_ctr *values)
{
	int i = match_registry_path( key );

	if ( i == -1 )
		return -1;

	/* return 0 values by default since we know the key had
	   to exist because the client opened a handle */

	if ( !print_registry[i].fetch_values )
		return 0;

	return print_registry[i].fetch_values( key, values );
}

/**********************************************************************
 *********************************************************************/

static bool regprint_store_reg_values(const char *key, struct regval_ctr *values)
{
	int i = match_registry_path( key );

	if ( i == -1 )
		return False;

	if ( !print_registry[i].store_values )
		return False;

	return print_registry[i].store_values( key, values );
}

/*
 * Table of function pointers for accessing printing data
 */

struct registry_ops printing_ops = {
	.fetch_subkeys = regprint_fetch_reg_keys,
	.fetch_values = regprint_fetch_reg_values,
	.store_subkeys = regprint_store_reg_keys,
	.store_values = regprint_store_reg_values,
};
