/*
   Samba Unix/Linux SMB client utility profiles.c

   Copyright (C) Richard Sharpe, <rsharpe@richardsharpe.com>   2002
   Copyright (C) Jelmer Vernooij (conversion to popt)          2003
   Copyright (C) Gerald (Jerry) Carter                         2005

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/filesys.h"
#include "popt_common.h"
#include "registry/reg_objects.h"
#include "registry/regfio.h"
#include "../libcli/security/security.h"

/* GLOBAL VARIABLES */

struct dom_sid old_sid, new_sid;
int change = 0, new_val = 0;
int opt_verbose = False;

/********************************************************************
********************************************************************/

static void verbose_output(const char *format, ...) PRINTF_ATTRIBUTE(1,2);
static void verbose_output(const char *format, ...)
{
	va_list args;
	char *var = NULL;

	if (!opt_verbose) {
		return;
	}

	va_start(args, format);
	if ((vasprintf(&var, format, args)) == -1) {
		va_end(args);
		return;
	}

	fprintf(stdout, "%s", var);
	va_end(args);
	SAFE_FREE(var);
}

/********************************************************************
********************************************************************/

static bool swap_sid_in_acl( struct security_descriptor *sd, struct dom_sid *s1, struct dom_sid *s2 )
{
	struct security_acl *theacl;
	int i;
	bool update = False;

	verbose_output("  Owner SID: %s\n", sid_string_tos(sd->owner_sid));
	if ( dom_sid_equal( sd->owner_sid, s1 ) ) {
		sid_copy( sd->owner_sid, s2 );
		update = True;
		verbose_output("  New Owner SID: %s\n",
			sid_string_tos(sd->owner_sid));

	}

	verbose_output("  Group SID: %s\n", sid_string_tos(sd->group_sid));
	if ( dom_sid_equal( sd->group_sid, s1 ) ) {
		sid_copy( sd->group_sid, s2 );
		update = True;
		verbose_output("  New Group SID: %s\n",
			sid_string_tos(sd->group_sid));
	}

	theacl = sd->dacl;
	verbose_output("  DACL: %d entries:\n", theacl->num_aces);
	for ( i=0; i<theacl->num_aces; i++ ) {
		verbose_output("    Trustee SID: %s\n",
			sid_string_tos(&theacl->aces[i].trustee));
		if ( dom_sid_equal( &theacl->aces[i].trustee, s1 ) ) {
			sid_copy( &theacl->aces[i].trustee, s2 );
			update = True;
			verbose_output("    New Trustee SID: %s\n",
				sid_string_tos(&theacl->aces[i].trustee));
		}
	}

#if 0
	theacl = sd->sacl;
	verbose_output("  SACL: %d entries: \n", theacl->num_aces);
	for ( i=0; i<theacl->num_aces; i++ ) {
		verbose_output("    Trustee SID: %s\n",
			sid_string_tos(&theacl->aces[i].trustee));
		if ( dom_sid_equal( &theacl->aces[i].trustee, s1 ) ) {
			sid_copy( &theacl->aces[i].trustee, s2 );
			update = True;
			verbose_output("    New Trustee SID: %s\n",
				sid_string_tos(&theacl->aces[i].trustee));
		}
	}
#endif
	return update;
}

/********************************************************************
********************************************************************/

static bool copy_registry_tree( REGF_FILE *infile, REGF_NK_REC *nk,
                                REGF_NK_REC *parent, REGF_FILE *outfile,
                                const char *parentpath  )
{
	REGF_NK_REC *key, *subkey;
	struct security_descriptor *new_sd;
	struct regval_ctr *values;
	struct regsubkey_ctr *subkeys;
	int i;
	char *path;
	WERROR werr;

	/* swap out the SIDs in the security descriptor */

	if ( !(new_sd = dup_sec_desc( outfile->mem_ctx, nk->sec_desc->sec_desc )) ) {
		fprintf( stderr, "Failed to copy security descriptor!\n" );
		return False;
	}

	verbose_output("ACL for %s%s%s\n", parentpath, parent ? "\\" : "", nk->keyname);
	swap_sid_in_acl( new_sd, &old_sid, &new_sid );

	werr = regsubkey_ctr_init(NULL, &subkeys);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,("copy_registry_tree: talloc() failure!\n"));
		return False;
	}

	werr = regval_ctr_init(subkeys, &values);
	if (!W_ERROR_IS_OK(werr)) {
		TALLOC_FREE( subkeys );
		DEBUG(0,("copy_registry_tree: talloc() failure!\n"));
		return False;
	}

	/* copy values into the struct regval_ctr */

	for ( i=0; i<nk->num_values; i++ ) {
		regval_ctr_addvalue( values, nk->values[i].valuename, nk->values[i].type,
			nk->values[i].data, (nk->values[i].data_size & ~VK_DATA_IN_OFFSET) );
	}

	/* copy subkeys into the struct regsubkey_ctr */

	while ( (subkey = regfio_fetch_subkey( infile, nk )) ) {
		regsubkey_ctr_addkey( subkeys, subkey->keyname );
	}

	key = regfio_write_key( outfile, nk->keyname, values, subkeys, new_sd, parent );

	/* write each one of the subkeys out */

	path = talloc_asprintf(subkeys, "%s%s%s",
			parentpath, parent ? "\\" : "",nk->keyname);
	if (!path) {
		TALLOC_FREE( subkeys );
		return false;
	}

	nk->subkey_index = 0;
	while ((subkey = regfio_fetch_subkey(infile, nk))) {
		if (!copy_registry_tree( infile, subkey, key, outfile, path)) {
			TALLOC_FREE(subkeys);
			return false;
		}
	}

	/* values is a talloc()'d child of subkeys here so just throw it all away */

	TALLOC_FREE( subkeys );

	verbose_output("[%s]\n", path);

	return True;
}

/*********************************************************************
*********************************************************************/

int main( int argc, char *argv[] )
{
	TALLOC_CTX *frame = talloc_stackframe();
	int opt;
	REGF_FILE *infile, *outfile;
	REGF_NK_REC *nk;
	char *orig_filename, *new_filename;
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "change-sid", 'c', POPT_ARG_STRING, NULL, 'c', "Provides SID to change" },
		{ "new-sid", 'n', POPT_ARG_STRING, NULL, 'n', "Provides SID to change to" },
		{ "verbose", 'v', POPT_ARG_NONE, &opt_verbose, 'v', "Verbose output" },
		POPT_COMMON_SAMBA
		POPT_COMMON_VERSION
		POPT_TABLEEND
	};
	poptContext pc;

	load_case_tables();

	/* setup logging options */

	setup_logging( "profiles", DEBUG_STDERR);

	pc = poptGetContext("profiles", argc, (const char **)argv, long_options,
		POPT_CONTEXT_KEEP_FIRST);

	poptSetOtherOptionHelp(pc, "<profilefile>");

	/* Now, process the arguments */

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case 'c':
			change = 1;
			if (!string_to_sid(&old_sid, poptGetOptArg(pc))) {
				fprintf(stderr, "Argument to -c should be a SID in form of S-1-5-...\n");
				poptPrintUsage(pc, stderr, 0);
				exit(254);
			}
			break;

		case 'n':
			new_val = 1;
			if (!string_to_sid(&new_sid, poptGetOptArg(pc))) {
				fprintf(stderr, "Argument to -n should be a SID in form of S-1-5-...\n");
				poptPrintUsage(pc, stderr, 0);
				exit(253);
			}
			break;

		}
	}

	poptGetArg(pc);

	if (!poptPeekArg(pc)) {
		poptPrintUsage(pc, stderr, 0);
		exit(1);
	}

	if ((!change && new_val) || (change && !new_val)) {
		fprintf(stderr, "You must specify both -c and -n if one or the other is set!\n");
		poptPrintUsage(pc, stderr, 0);
		exit(252);
	}

	orig_filename = talloc_strdup(frame, poptPeekArg(pc));
	if (!orig_filename) {
		exit(ENOMEM);
	}
	new_filename = talloc_asprintf(frame,
					"%s.new",
					orig_filename);
	if (!new_filename) {
		exit(ENOMEM);
	}

	if (!(infile = regfio_open( orig_filename, O_RDONLY, 0))) {
		fprintf( stderr, "Failed to open %s!\n", orig_filename );
		fprintf( stderr, "Error was (%s)\n", strerror(errno) );
		exit (1);
	}

	if ( !(outfile = regfio_open( new_filename, (O_RDWR|O_CREAT|O_TRUNC),
				      (S_IRUSR|S_IWUSR) )) ) {
		fprintf( stderr, "Failed to open new file %s!\n", new_filename );
		fprintf( stderr, "Error was (%s)\n", strerror(errno) );
		exit (1);
	}

	/* actually do the update now */

	if ((nk = regfio_rootkey( infile )) == NULL) {
		fprintf(stderr, "Could not get rootkey\n");
		exit(3);
	}

	if (!copy_registry_tree( infile, nk, NULL, outfile, "")) {
		fprintf(stderr, "Failed to write updated registry file!\n");
		exit(2);
	}

	/* cleanup */

	regfio_close(infile);
	regfio_close(outfile);

	poptFreeContext(pc);

	TALLOC_FREE(frame);
	return 0;
}
