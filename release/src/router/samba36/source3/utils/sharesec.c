/*
 *  Unix SMB/Netbios implementation.
 *  Utility for managing share permissions
 *
 *  Copyright (C) Tim Potter                    2000
 *  Copyright (C) Jeremy Allison                2000
 *  Copyright (C) Jelmer Vernooij               2003
 *  Copyright (C) Gerald (Jerry) Carter         2005.
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
#include "popt_common.h"
#include "../libcli/security/security.h"
#include "passdb/machine_sid.h"

static TALLOC_CTX *ctx;

enum acl_mode { SMB_ACL_DELETE,
	        SMB_ACL_MODIFY,
	        SMB_ACL_ADD,
	        SMB_ACL_SET,
		SMB_SD_DELETE,
	        SMB_ACL_VIEW };

struct perm_value {
	const char *perm;
	uint32 mask;
};

/* These values discovered by inspection */

static const struct perm_value special_values[] = {
	{ "R", SEC_RIGHTS_FILE_READ },
	{ "W", SEC_RIGHTS_FILE_WRITE },
	{ "X", SEC_RIGHTS_FILE_EXECUTE },
	{ "D", SEC_STD_DELETE },
	{ "P", SEC_STD_WRITE_DAC },
	{ "O", SEC_STD_WRITE_OWNER },
	{ NULL, 0 },
};

#define SEC_RIGHTS_DIR_CHANGE ( SEC_RIGHTS_DIR_READ|SEC_STD_DELETE|\
				SEC_RIGHTS_DIR_WRITE|SEC_DIR_TRAVERSE )

static const struct perm_value standard_values[] = {
	{ "READ",   SEC_RIGHTS_DIR_READ|SEC_DIR_TRAVERSE },
	{ "CHANGE", SEC_RIGHTS_DIR_CHANGE },
	{ "FULL",   SEC_RIGHTS_DIR_ALL },
	{ NULL, 0 },
};

/********************************************************************
 print an ACE on a FILE
********************************************************************/

static void print_ace(FILE *f, struct security_ace *ace)
{
	const struct perm_value *v;
	int do_print = 0;
	uint32 got_mask;

	fprintf(f, "%s:", sid_string_tos(&ace->trustee));

	/* Ace type */

	if (ace->type == SEC_ACE_TYPE_ACCESS_ALLOWED) {
		fprintf(f, "ALLOWED");
	} else if (ace->type == SEC_ACE_TYPE_ACCESS_DENIED) {
		fprintf(f, "DENIED");
	} else {
		fprintf(f, "%d", ace->type);
	}

	/* Not sure what flags can be set in a file ACL */

	fprintf(f, "/%d/", ace->flags);

	/* Standard permissions */

	for (v = standard_values; v->perm; v++) {
		if (ace->access_mask == v->mask) {
			fprintf(f, "%s", v->perm);
			return;
		}
	}

	/* Special permissions.  Print out a hex value if we have
	   leftover bits in the mask. */

	got_mask = ace->access_mask;

 again:
	for (v = special_values; v->perm; v++) {
		if ((ace->access_mask & v->mask) == v->mask) {
			if (do_print) {
				fprintf(f, "%s", v->perm);
			}
			got_mask &= ~v->mask;
		}
	}

	if (!do_print) {
		if (got_mask != 0) {
			fprintf(f, "0x%08x", ace->access_mask);
		} else {
			do_print = 1;
			goto again;
		}
	}
}

/********************************************************************
 print an ascii version of a security descriptor on a FILE handle
********************************************************************/

static void sec_desc_print(FILE *f, struct security_descriptor *sd)
{
	uint32 i;

	fprintf(f, "REVISION:%d\n", sd->revision);

	/* Print owner and group sid */

	fprintf(f, "OWNER:%s\n", sid_string_tos(sd->owner_sid));

	fprintf(f, "GROUP:%s\n", sid_string_tos(sd->group_sid));

	/* Print aces */
	for (i = 0; sd->dacl && i < sd->dacl->num_aces; i++) {
		struct security_ace *ace = &sd->dacl->aces[i];
		fprintf(f, "ACL:");
		print_ace(f, ace);
		fprintf(f, "\n");
	}
}

/********************************************************************
 parse an ACE in the same format as print_ace()
********************************************************************/

static bool parse_ace(struct security_ace *ace, const char *orig_str)
{
	char *p;
	const char *cp;
	char *tok;
	unsigned int atype = 0;
	unsigned int aflags = 0;
	unsigned int amask = 0;
	struct dom_sid sid;
	uint32_t mask;
	const struct perm_value *v;
	char *str = SMB_STRDUP(orig_str);
	TALLOC_CTX *frame = talloc_stackframe();

	if (!str) {
		TALLOC_FREE(frame);
		return False;
	}

	ZERO_STRUCTP(ace);
	p = strchr_m(str,':');
	if (!p) {
		fprintf(stderr, "ACE '%s': missing ':'.\n", orig_str);
		SAFE_FREE(str);
		TALLOC_FREE(frame);
		return False;
	}
	*p = '\0';
	p++;
	/* Try to parse numeric form */

	if (sscanf(p, "%i/%i/%i", &atype, &aflags, &amask) == 3 &&
	    string_to_sid(&sid, str)) {
		goto done;
	}

	/* Try to parse text form */

	if (!string_to_sid(&sid, str)) {
		fprintf(stderr, "ACE '%s': failed to convert '%s' to SID\n",
			orig_str, str);
		SAFE_FREE(str);
		TALLOC_FREE(frame);
		return False;
	}

	cp = p;
	if (!next_token_talloc(frame, &cp, &tok, "/")) {
		fprintf(stderr, "ACE '%s': failed to find '/' character.\n",
			orig_str);
		SAFE_FREE(str);
		TALLOC_FREE(frame);
		return False;
	}

	if (strncmp(tok, "ALLOWED", strlen("ALLOWED")) == 0) {
		atype = SEC_ACE_TYPE_ACCESS_ALLOWED;
	} else if (strncmp(tok, "DENIED", strlen("DENIED")) == 0) {
		atype = SEC_ACE_TYPE_ACCESS_DENIED;
	} else {
		fprintf(stderr, "ACE '%s': missing 'ALLOWED' or 'DENIED' "
			"entry at '%s'\n", orig_str, tok);
		SAFE_FREE(str);
		TALLOC_FREE(frame);
		return False;
	}

	/* Only numeric form accepted for flags at present */
	/* no flags on share permissions */

	if (!(next_token_talloc(frame, &cp, &tok, "/") &&
	      sscanf(tok, "%i", &aflags) && aflags == 0)) {
		fprintf(stderr, "ACE '%s': bad integer flags entry at '%s'\n",
			orig_str, tok);
		SAFE_FREE(str);
		TALLOC_FREE(frame);
		return False;
	}

	if (!next_token_talloc(frame, &cp, &tok, "/")) {
		fprintf(stderr, "ACE '%s': missing / at '%s'\n",
			orig_str, tok);
		SAFE_FREE(str);
		TALLOC_FREE(frame);
		return False;
	}

	if (strncmp(tok, "0x", 2) == 0) {
		if (sscanf(tok, "%i", &amask) != 1) {
			fprintf(stderr, "ACE '%s': bad hex number at '%s'\n",
				orig_str, tok);
			TALLOC_FREE(frame);
			SAFE_FREE(str);
			return False;
		}
		goto done;
	}

	for (v = standard_values; v->perm; v++) {
		if (strcmp(tok, v->perm) == 0) {
			amask = v->mask;
			goto done;
		}
	}

	p = tok;

	while(*p) {
		bool found = False;

		for (v = special_values; v->perm; v++) {
			if (v->perm[0] == *p) {
				amask |= v->mask;
				found = True;
			}
		}

		if (!found) {
			fprintf(stderr, "ACE '%s': bad permission value at "
				"'%s'\n", orig_str, p);
			TALLOC_FREE(frame);
			SAFE_FREE(str);
			return False;
		}
		p++;
	}

	if (*p) {
		TALLOC_FREE(frame);
		SAFE_FREE(str);
		return False;
	}

 done:
	mask = amask;
	init_sec_ace(ace, &sid, atype, mask, aflags);
	SAFE_FREE(str);
	TALLOC_FREE(frame);
	return True;
}


/********************************************************************
********************************************************************/

static struct security_descriptor* parse_acl_string(TALLOC_CTX *mem_ctx, const char *szACL, size_t *sd_size )
{
	struct security_descriptor *sd = NULL;
	struct security_ace *ace;
	struct security_acl *theacl;
	int num_ace;
	const char *pacl;
	int i;

	if ( !szACL )
		return NULL;

	pacl = szACL;
	num_ace = count_chars( pacl, ',' ) + 1;

	if ( !(ace = TALLOC_ZERO_ARRAY( mem_ctx, struct security_ace, num_ace )) )
		return NULL;

	for ( i=0; i<num_ace; i++ ) {
		char *end_acl = strchr_m( pacl, ',' );
		fstring acl_string;

		strncpy( acl_string, pacl, MIN( PTR_DIFF( end_acl, pacl ), sizeof(fstring)-1) );
		acl_string[MIN( PTR_DIFF( end_acl, pacl ), sizeof(fstring)-1)] = '\0';

		if ( !parse_ace( &ace[i], acl_string ) )
			return NULL;

		pacl = end_acl;
		pacl++;
	}

	if ( !(theacl = make_sec_acl( mem_ctx, NT4_ACL_REVISION, num_ace, ace )) )
		return NULL;

	sd = make_sec_desc( mem_ctx, SD_REVISION, SEC_DESC_SELF_RELATIVE,
		NULL, NULL, NULL, theacl, sd_size);

	return sd;
}

/* add an ACE to a list of ACEs in a struct security_acl */
static bool add_ace(TALLOC_CTX *mem_ctx, struct security_acl **the_acl, struct security_ace *ace)
{
	struct security_acl *new_ace;
	struct security_ace *aces;
	if (! *the_acl) {
		return (((*the_acl) = make_sec_acl(mem_ctx, 3, 1, ace)) != NULL);
	}

	if (!(aces = SMB_CALLOC_ARRAY(struct security_ace, 1+(*the_acl)->num_aces))) {
		return False;
	}
	memcpy(aces, (*the_acl)->aces, (*the_acl)->num_aces * sizeof(struct
	security_ace));
	memcpy(aces+(*the_acl)->num_aces, ace, sizeof(struct security_ace));
	new_ace = make_sec_acl(mem_ctx,(*the_acl)->revision,1+(*the_acl)->num_aces, aces);
	SAFE_FREE(aces);
	(*the_acl) = new_ace;
	return True;
}

/* The MSDN is contradictory over the ordering of ACE entries in an ACL.
   However NT4 gives a "The information may have been modified by a
   computer running Windows NT 5.0" if denied ACEs do not appear before
   allowed ACEs. */

static int ace_compare(struct security_ace *ace1, struct security_ace *ace2)
{
	if (sec_ace_equal(ace1, ace2))
		return 0;

	if (ace1->type != ace2->type)
		return ace2->type - ace1->type;

	if (dom_sid_compare(&ace1->trustee, &ace2->trustee))
		return dom_sid_compare(&ace1->trustee, &ace2->trustee);

	if (ace1->flags != ace2->flags)
		return ace1->flags - ace2->flags;

	if (ace1->access_mask != ace2->access_mask)
		return ace1->access_mask - ace2->access_mask;

	if (ace1->size != ace2->size)
		return ace1->size - ace2->size;

	return memcmp(ace1, ace2, sizeof(struct security_ace));
}

static void sort_acl(struct security_acl *the_acl)
{
	uint32 i;
	if (!the_acl) return;

	TYPESAFE_QSORT(the_acl->aces, the_acl->num_aces, ace_compare);

	for (i=1;i<the_acl->num_aces;) {
		if (sec_ace_equal(&the_acl->aces[i-1], &the_acl->aces[i])) {
			int j;
			for (j=i; j<the_acl->num_aces-1; j++) {
				the_acl->aces[j] = the_acl->aces[j+1];
			}
			the_acl->num_aces--;
		} else {
			i++;
		}
	}
}


static int change_share_sec(TALLOC_CTX *mem_ctx, const char *sharename, char *the_acl, enum acl_mode mode)
{
	struct security_descriptor *sd = NULL;
	struct security_descriptor *old = NULL;
	size_t sd_size = 0;
	uint32 i, j;

	if (mode != SMB_ACL_SET && mode != SMB_SD_DELETE) {
	    if (!(old = get_share_security( mem_ctx, sharename, &sd_size )) ) {
		fprintf(stderr, "Unable to retrieve permissions for share "
			"[%s]\n", sharename);
		return -1;
	    }
	}

	if ( (mode != SMB_ACL_VIEW && mode != SMB_SD_DELETE) &&
	    !(sd = parse_acl_string(mem_ctx, the_acl, &sd_size )) ) {
		fprintf( stderr, "Failed to parse acl\n");
		return -1;
	}

	switch (mode) {
	case SMB_ACL_VIEW:
		sec_desc_print( stdout, old);
		return 0;
	case SMB_ACL_DELETE:
	    for (i=0;sd->dacl && i<sd->dacl->num_aces;i++) {
		bool found = False;

		for (j=0;old->dacl && j<old->dacl->num_aces;j++) {
		    if (sec_ace_equal(&sd->dacl->aces[i], &old->dacl->aces[j])) {
			uint32 k;
			for (k=j; k<old->dacl->num_aces-1;k++) {
			    old->dacl->aces[k] = old->dacl->aces[k+1];
			}
			old->dacl->num_aces--;
			found = True;
			break;
		    }
		}

		if (!found) {
		printf("ACL for ACE:");
		print_ace(stdout, &sd->dacl->aces[i]);
		printf(" not found\n");
		}
	    }
	    break;
	case SMB_ACL_MODIFY:
	    for (i=0;sd->dacl && i<sd->dacl->num_aces;i++) {
		bool found = False;

		for (j=0;old->dacl && j<old->dacl->num_aces;j++) {
		    if (dom_sid_equal(&sd->dacl->aces[i].trustee,
			&old->dacl->aces[j].trustee)) {
			old->dacl->aces[j] = sd->dacl->aces[i];
			found = True;
		    }
		}

		if (!found) {
		    printf("ACL for SID %s not found\n",
			   sid_string_tos(&sd->dacl->aces[i].trustee));
		}
	    }

	    if (sd->owner_sid) {
		old->owner_sid = sd->owner_sid;
	    }

	    if (sd->group_sid) {
		old->group_sid = sd->group_sid;
	    }
	    break;
	case SMB_ACL_ADD:
	    for (i=0;sd->dacl && i<sd->dacl->num_aces;i++) {
		add_ace(mem_ctx, &old->dacl, &sd->dacl->aces[i]);
	    }
	    break;
	case SMB_ACL_SET:
	    old = sd;
	    break;
	case SMB_SD_DELETE:
	    if (!delete_share_security(sharename)) {
		fprintf( stderr, "Failed to delete security descriptor for "
			 "share [%s]\n", sharename );
		return -1;
	    }
	    return 0;
	}

	/* Denied ACE entries must come before allowed ones */
	sort_acl(old->dacl);

	if ( !set_share_security( sharename, old ) ) {
	    fprintf( stderr, "Failed to store acl for share [%s]\n", sharename );
	    return 2;
	}
	return 0;
}

/********************************************************************
  main program
********************************************************************/

int main(int argc, const char *argv[])
{
	int opt;
	int retval = 0;
	enum acl_mode mode = SMB_ACL_SET;
	static char *the_acl = NULL;
	fstring sharename;
	bool force_acl = False;
	int snum;
	poptContext pc;
	bool initialize_sid = False;
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "remove", 'r', POPT_ARG_STRING, &the_acl, 'r', "Remove ACEs", "ACL" },
		{ "modify", 'm', POPT_ARG_STRING, &the_acl, 'm', "Modify existing ACEs", "ACL" },
		{ "add", 'a', POPT_ARG_STRING, &the_acl, 'a', "Add ACEs", "ACL" },
		{ "replace", 'R', POPT_ARG_STRING, &the_acl, 'R', "Overwrite share permission ACL", "ACLS" },
		{ "delete", 'D', POPT_ARG_NONE, NULL, 'D', "Delete the entire security descriptor" },
		{ "view", 'v', POPT_ARG_NONE, NULL, 'v', "View current share permissions" },
		{ "machine-sid", 'M', POPT_ARG_NONE, NULL, 'M', "Initialize the machine SID" },
		{ "force", 'F', POPT_ARG_NONE, NULL, 'F', "Force storing the ACL", "ACLS" },
		POPT_COMMON_SAMBA
		{ NULL }
	};

	if ( !(ctx = talloc_stackframe()) ) {
		fprintf( stderr, "Failed to initialize talloc context!\n");
		return -1;
	}

	/* set default debug level to 1 regardless of what smb.conf sets */
	setup_logging( "sharesec", DEBUG_STDERR);

	load_case_tables();

	lp_set_cmdline("log level", "1");

	pc = poptGetContext("sharesec", argc, argv, long_options, 0);

	poptSetOtherOptionHelp(pc, "sharename\n");

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case 'r':
			the_acl = smb_xstrdup(poptGetOptArg(pc));
			mode = SMB_ACL_DELETE;
			break;

		case 'm':
			the_acl = smb_xstrdup(poptGetOptArg(pc));
			mode = SMB_ACL_MODIFY;
			break;

		case 'a':
			the_acl = smb_xstrdup(poptGetOptArg(pc));
			mode = SMB_ACL_ADD;
			break;

		case 'R':
			the_acl = smb_xstrdup(poptGetOptArg(pc));
			mode = SMB_ACL_SET;
			break;

		case 'D':
			mode = SMB_SD_DELETE;
			break;

		case 'v':
			mode = SMB_ACL_VIEW;
			break;

		case 'F':
			force_acl = True;
			break;

		case 'M':
			initialize_sid = True;
			break;
		}
	}

	setlinebuf(stdout);

	lp_load_with_registry_shares( get_dyn_CONFIGFILE(), False, False, False,
				      True );

	/* check for initializing secrets.tdb first */

	if ( initialize_sid ) {
		struct dom_sid *sid = get_global_sam_sid();

		if ( !sid ) {
			fprintf( stderr, "Failed to retrieve Machine SID!\n");
			return 3;
		}

		printf ("%s\n", sid_string_tos( sid ) );
		return 0;
	}

	if ( mode == SMB_ACL_VIEW && force_acl ) {
		fprintf( stderr, "Invalid combination of -F and -v\n");
		return -1;
	}

	/* get the sharename */

	if(!poptPeekArg(pc)) {
		poptPrintUsage(pc, stderr, 0);
		return -1;
	}

	fstrcpy(sharename, poptGetArg(pc));

	snum = lp_servicenumber( sharename );

	if ( snum == -1 && !force_acl ) {
		fprintf( stderr, "Invalid sharename: %s\n", sharename);
		return -1;
	}

	retval = change_share_sec(ctx, sharename, the_acl, mode);

	talloc_destroy(ctx);

	return retval;
}
