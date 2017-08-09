/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-2000,
 *  Copyright (C) Jean François Micouleau      1998-2001.
 *  Copyright (C) Gerald Carter                2003,
 *  Copyright (C) Volker Lendecke              2004
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
#include "system/passwd.h"
#include "utils/net.h"
#include "../libcli/security/security.h"
#include "passdb.h"

/*********************************************************
 Figure out if the input was an NT group or a SID string.
 Return the SID.
**********************************************************/
static bool get_sid_from_input(struct dom_sid *sid, char *input)
{
	GROUP_MAP map;

	if (StrnCaseCmp( input, "S-", 2)) {
		/* Perhaps its the NT group name? */
		if (!pdb_getgrnam(&map, input)) {
			printf(_("NT Group %s doesn't exist in mapping DB\n"),
			       input);
			return false;
		} else {
			*sid = map.sid;
		}
	} else {
		if (!string_to_sid(sid, input)) {
			printf(_("converting sid %s from a string failed!\n"),
			       input);
			return false;
		}
	}
	return true;
}

/*********************************************************
 Dump a GROUP_MAP entry to stdout (long or short listing)
**********************************************************/

static void print_map_entry ( GROUP_MAP map, bool long_list )
{
	if (!long_list)
		d_printf("%s (%s) -> %s\n", map.nt_name,
			 sid_string_tos(&map.sid), gidtoname(map.gid));
	else {
		d_printf("%s\n", map.nt_name);
		d_printf(_("\tSID       : %s\n"), sid_string_tos(&map.sid));
		d_printf(_("\tUnix gid  : %u\n"), (unsigned int)map.gid);
		d_printf(_("\tUnix group: %s\n"), gidtoname(map.gid));
		d_printf(_("\tGroup type: %s\n"),
			 sid_type_lookup(map.sid_name_use));
		d_printf(_("\tComment   : %s\n"), map.comment);
	}

}
/*********************************************************
 List the groups.
**********************************************************/
static int net_groupmap_list(struct net_context *c, int argc, const char **argv)
{
	size_t entries;
	bool long_list = false;
	size_t i;
	fstring ntgroup = "";
	fstring sid_string = "";
	const char list_usage_str[] = N_("net groupmap list [verbose] "
				         "[ntgroup=NT group] [sid=SID]\n"
				         "    verbose\tPrint verbose list\n"
				         "    ntgroup\tNT group to list\n"
				         "    sid\tSID of group to list");

	if (c->display_usage) {
		d_printf("%s\n%s\n", _("Usage: "), list_usage_str);
		return 0;
	}

	if (c->opt_verbose || c->opt_long_list_entries)
		long_list = true;

	/* get the options */
	for ( i=0; i<argc; i++ ) {
		if ( !StrCaseCmp(argv[i], "verbose")) {
			long_list = true;
		}
		else if ( !StrnCaseCmp(argv[i], "ntgroup", strlen("ntgroup")) ) {
			fstrcpy( ntgroup, get_string_param( argv[i] ) );
			if ( !ntgroup[0] ) {
				d_fprintf(stderr, _("must supply a name\n"));
				return -1;
			}
		}
		else if ( !StrnCaseCmp(argv[i], "sid", strlen("sid")) ) {
			fstrcpy( sid_string, get_string_param( argv[i] ) );
			if ( !sid_string[0] ) {
				d_fprintf(stderr, _("must supply a SID\n"));
				return -1;
			}
		}
		else {
			d_fprintf(stderr, _("Bad option: %s\n"), argv[i]);
			d_printf("%s\n%s\n", _("Usage:"), list_usage_str);
			return -1;
		}
	}

	/* list a single group is given a name */
	if ( ntgroup[0] || sid_string[0] ) {
		struct dom_sid sid;
		GROUP_MAP map;

		if ( sid_string[0] )
			fstrcpy( ntgroup, sid_string);

		if (!get_sid_from_input(&sid, ntgroup)) {
			return -1;
		}

		/* Get the current mapping from the database */
		if(!pdb_getgrsid(&map, sid)) {
			d_fprintf(stderr,
				  _("Failure to local group SID in the "
				    "database\n"));
			return -1;
		}

		print_map_entry( map, long_list );
	}
	else {
		GROUP_MAP *map=NULL;
		/* enumerate all group mappings */
		if (!pdb_enum_group_mapping(NULL, SID_NAME_UNKNOWN, &map, &entries, ENUM_ALL_MAPPED))
			return -1;

		for (i=0; i<entries; i++) {
			print_map_entry( map[i], long_list );
		}

		SAFE_FREE(map);
	}

	return 0;
}

/*********************************************************
 Add a new group mapping entry
**********************************************************/

static int net_groupmap_add(struct net_context *c, int argc, const char **argv)
{
	struct dom_sid sid;
	fstring ntgroup = "";
	fstring unixgrp = "";
	fstring string_sid = "";
	fstring type = "";
	fstring ntcomment = "";
	enum lsa_SidType sid_type = SID_NAME_DOM_GRP;
	uint32 rid = 0;
	gid_t gid;
	int i;
	GROUP_MAP map;

	const char *name_type;
	const char add_usage_str[] = N_("net groupmap add "
					"{rid=<int>|sid=<string>}"
					" unixgroup=<string> "
					"[type=<domain|local|builtin>] "
					"[ntgroup=<string>] "
					"[comment=<string>]");

	ZERO_STRUCT(map);

	/* Default is domain group. */
	map.sid_name_use = SID_NAME_DOM_GRP;
	name_type = "domain group";

	if (c->display_usage) {
		d_printf("%s\n%s\n", _("Usage:\n"), add_usage_str);
		return 0;
	}

	/* get the options */
	for ( i=0; i<argc; i++ ) {
		if ( !StrnCaseCmp(argv[i], "rid", strlen("rid")) ) {
			rid = get_int_param(argv[i]);
			if ( rid < DOMAIN_RID_ADMINS ) {
				d_fprintf(stderr,
					  _("RID must be greater than %d\n"),
					  (uint32)DOMAIN_RID_ADMINS-1);
				return -1;
			}
		}
		else if ( !StrnCaseCmp(argv[i], "unixgroup", strlen("unixgroup")) ) {
			fstrcpy( unixgrp, get_string_param( argv[i] ) );
			if ( !unixgrp[0] ) {
				d_fprintf(stderr,_( "must supply a name\n"));
				return -1;
			}
		}
		else if ( !StrnCaseCmp(argv[i], "ntgroup", strlen("ntgroup")) ) {
			fstrcpy( ntgroup, get_string_param( argv[i] ) );
			if ( !ntgroup[0] ) {
				d_fprintf(stderr, _("must supply a name\n"));
				return -1;
			}
		}
		else if ( !StrnCaseCmp(argv[i], "sid", strlen("sid")) ) {
			fstrcpy( string_sid, get_string_param( argv[i] ) );
			if ( !string_sid[0] ) {
				d_fprintf(stderr, _("must supply a SID\n"));
				return -1;
			}
		}
		else if ( !StrnCaseCmp(argv[i], "comment", strlen("comment")) ) {
			fstrcpy( ntcomment, get_string_param( argv[i] ) );
			if ( !ntcomment[0] ) {
				d_fprintf(stderr,
					  _("must supply a comment string\n"));
				return -1;
			}
		}
		else if ( !StrnCaseCmp(argv[i], "type", strlen("type")) )  {
			fstrcpy( type, get_string_param( argv[i] ) );
			switch ( type[0] ) {
				case 'b':
				case 'B':
					sid_type = SID_NAME_WKN_GRP;
					name_type = "wellknown group";
					break;
				case 'd':
				case 'D':
					sid_type = SID_NAME_DOM_GRP;
					name_type = "domain group";
					break;
				case 'l':
				case 'L':
					sid_type = SID_NAME_ALIAS;
					name_type = "alias (local) group";
					break;
				default:
					d_fprintf(stderr,
						  _("unknown group type %s\n"),
						  type);
					return -1;
			}
		}
		else {
			d_fprintf(stderr, _("Bad option: %s\n"), argv[i]);
			return -1;
		}
	}

	if ( !unixgrp[0] ) {
		d_printf("%s\n%s\n", _("Usage:\n"), add_usage_str);
		return -1;
	}

	if ( (gid = nametogid(unixgrp)) == (gid_t)-1 ) {
		d_fprintf(stderr, _("Can't lookup UNIX group %s\n"), unixgrp);
		return -1;
	}

	{
		if (pdb_getgrgid(&map, gid)) {
			d_printf(_("Unix group %s already mapped to SID %s\n"),
				 unixgrp, sid_string_tos(&map.sid));
			return -1;
		}
	}

	if ( (rid == 0) && (string_sid[0] == '\0') ) {
		d_printf(_("No rid or sid specified, choosing a RID\n"));
		if (pdb_capabilities() & PDB_CAP_STORE_RIDS) {
			if (!pdb_new_rid(&rid)) {
				d_printf(_("Could not get new RID\n"));
			}
		} else {
			rid = algorithmic_pdb_gid_to_group_rid(gid);
		}
		d_printf(_("Got RID %d\n"), rid);
	}

	/* append the rid to our own domain/machine SID if we don't have a full SID */
	if ( !string_sid[0] ) {
		sid_compose(&sid, get_global_sam_sid(), rid);
		sid_to_fstring(string_sid, &sid);
	}

	if (!ntcomment[0]) {
		switch (sid_type) {
		case SID_NAME_WKN_GRP:
			fstrcpy(ntcomment, "Wellknown Unix group");
			break;
		case SID_NAME_DOM_GRP:
			fstrcpy(ntcomment, "Domain Unix group");
			break;
		case SID_NAME_ALIAS:
			fstrcpy(ntcomment, "Local Unix group");
			break;
		default:
			fstrcpy(ntcomment, "Unix group");
			break;
		}
	}

	if (!ntgroup[0] )
		fstrcpy( ntgroup, unixgrp );

	if (!NT_STATUS_IS_OK(add_initial_entry(gid, string_sid, sid_type, ntgroup, ntcomment))) {
		d_fprintf(stderr, _("adding entry for group %s failed!\n"), ntgroup);
		return -1;
	}

	d_printf(_("Successfully added group %s to the mapping db as a %s\n"),
		 ntgroup, name_type);
	return 0;
}

static int net_groupmap_modify(struct net_context *c, int argc, const char **argv)
{
	struct dom_sid sid;
	GROUP_MAP map;
	fstring ntcomment = "";
	fstring type = "";
	fstring ntgroup = "";
	fstring unixgrp = "";
	fstring sid_string = "";
	enum lsa_SidType sid_type = SID_NAME_UNKNOWN;
	int i;
	gid_t gid;
	const char modify_usage_str[] = N_("net groupmap modify "
					   "{ntgroup=<string>|sid=<SID>} "
					   "[comment=<string>] "
					   "[unixgroup=<string>] "
					   "[type=<domain|local>]");

	if (c->display_usage) {
		d_printf("%s\n%s\n", _("Usage:\n"), modify_usage_str);
		return 0;
	}

	/* get the options */
	for ( i=0; i<argc; i++ ) {
		if ( !StrnCaseCmp(argv[i], "ntgroup", strlen("ntgroup")) ) {
			fstrcpy( ntgroup, get_string_param( argv[i] ) );
			if ( !ntgroup[0] ) {
				d_fprintf(stderr, _("must supply a name\n"));
				return -1;
			}
		}
		else if ( !StrnCaseCmp(argv[i], "sid", strlen("sid")) ) {
			fstrcpy( sid_string, get_string_param( argv[i] ) );
			if ( !sid_string[0] ) {
				d_fprintf(stderr, _("must supply a name\n"));
				return -1;
			}
		}
		else if ( !StrnCaseCmp(argv[i], "comment", strlen("comment")) ) {
			fstrcpy( ntcomment, get_string_param( argv[i] ) );
			if ( !ntcomment[0] ) {
				d_fprintf(stderr,
					  _("must supply a comment string\n"));
				return -1;
			}
		}
		else if ( !StrnCaseCmp(argv[i], "unixgroup", strlen("unixgroup")) ) {
			fstrcpy( unixgrp, get_string_param( argv[i] ) );
			if ( !unixgrp[0] ) {
				d_fprintf(stderr,
					  _("must supply a group name\n"));
				return -1;
			}
		}
		else if ( !StrnCaseCmp(argv[i], "type", strlen("type")) )  {
			fstrcpy( type, get_string_param( argv[i] ) );
			switch ( type[0] ) {
				case 'd':
				case 'D':
					sid_type = SID_NAME_DOM_GRP;
					break;
				case 'l':
				case 'L':
					sid_type = SID_NAME_ALIAS;
					break;
			}
		}
		else {
			d_fprintf(stderr, _("Bad option: %s\n"), argv[i]);
			return -1;
		}
	}

	if ( !ntgroup[0] && !sid_string[0] ) {
		d_printf("%s\n%s\n", _("Usage:\n"), modify_usage_str);
		return -1;
	}

	/* give preference to the SID; if both the ntgroup name and SID
	   are defined, use the SID and assume that the group name could be a
	   new name */

	if ( sid_string[0] ) {
		if (!get_sid_from_input(&sid, sid_string)) {
			return -1;
		}
	}
	else {
		if (!get_sid_from_input(&sid, ntgroup)) {
			return -1;
		}
	}

	/* Get the current mapping from the database */
	if(!pdb_getgrsid(&map, sid)) {
		d_fprintf(stderr,
			 _("Failed to find local group SID in the database\n"));
		return -1;
	}

	/*
	 * Allow changing of group type only between domain and local
	 * We disallow changing Builtin groups !!! (SID problem)
	 */
	if (sid_type == SID_NAME_UNKNOWN) {
		d_fprintf(stderr, _("Can't map to an unknown group type.\n"));
		return -1;
        }

	if (map.sid_name_use == SID_NAME_WKN_GRP) {
		d_fprintf(stderr,
			  _("You can only change between domain and local "
			    "groups.\n"));
		return -1;
	}

	map.sid_name_use=sid_type;

	/* Change comment if new one */
	if ( ntcomment[0] )
		fstrcpy( map.comment, ntcomment );

	if ( ntgroup[0] )
		fstrcpy( map.nt_name, ntgroup );

	if ( unixgrp[0] ) {
		gid = nametogid( unixgrp );
		if ( gid == -1 ) {
			d_fprintf(stderr, _("Unable to lookup UNIX group %s.  "
					    "Make sure the group exists.\n"),
				unixgrp);
			return -1;
		}

		map.gid = gid;
	}

	if ( !NT_STATUS_IS_OK(pdb_update_group_mapping_entry(&map)) ) {
		d_fprintf(stderr, _("Could not update group database\n"));
		return -1;
	}

	d_printf(_("Updated mapping entry for %s\n"), map.nt_name);

	return 0;
}

static int net_groupmap_delete(struct net_context *c, int argc, const char **argv)
{
	struct dom_sid sid;
	fstring ntgroup = "";
	fstring sid_string = "";
	int i;
	const char delete_usage_str[] = N_("net groupmap delete "
					   "{ntgroup=<string>|sid=<SID>}");

	if (c->display_usage) {
		d_printf("%s\n%s\n", _("Usage:\n"), delete_usage_str);
		return 0;
	}

	/* get the options */
	for ( i=0; i<argc; i++ ) {
		if ( !StrnCaseCmp(argv[i], "ntgroup", strlen("ntgroup")) ) {
			fstrcpy( ntgroup, get_string_param( argv[i] ) );
			if ( !ntgroup[0] ) {
				d_fprintf(stderr, _("must supply a name\n"));
				return -1;
			}
		}
		else if ( !StrnCaseCmp(argv[i], "sid", strlen("sid")) ) {
			fstrcpy( sid_string, get_string_param( argv[i] ) );
			if ( !sid_string[0] ) {
				d_fprintf(stderr, _("must supply a SID\n"));
				return -1;
			}
		}
		else {
			d_fprintf(stderr, _("Bad option: %s\n"), argv[i]);
			return -1;
		}
	}

	if ( !ntgroup[0] && !sid_string[0]) {
		d_printf("%s\n%s\n", _("Usage:\n"), delete_usage_str);
		return -1;
	}

	/* give preference to the SID if we have that */

	if ( sid_string[0] )
		fstrcpy( ntgroup, sid_string );

	if ( !get_sid_from_input(&sid, ntgroup) ) {
		d_fprintf(stderr, _("Unable to resolve group %s to a SID\n"),
			  ntgroup);
		return -1;
	}

	if ( !NT_STATUS_IS_OK(pdb_delete_group_mapping_entry(sid)) ) {
		d_fprintf(stderr,
			  _("Failed to remove group %s from the mapping db!\n"),
			  ntgroup);
		return -1;
	}

	d_printf(_("Sucessfully removed %s from the mapping db\n"), ntgroup);

	return 0;
}

static int net_groupmap_set(struct net_context *c, int argc, const char **argv)
{
	const char *ntgroup = NULL;
	struct group *grp = NULL;
	GROUP_MAP map;
	bool have_map = false;

	if ((argc < 1) || (argc > 2) || c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _(" net groupmap set \"NT Group\" "
			   "[\"unix group\"] [-C \"comment\"] [-L] [-D]\n"));
		return -1;
	}

	if ( c->opt_localgroup && c->opt_domaingroup ) {
		d_printf(_("Can only specify -L or -D, not both\n"));
		return -1;
	}

	ntgroup = argv[0];

	if (argc == 2) {
		grp = getgrnam(argv[1]);

		if (grp == NULL) {
			d_fprintf(stderr, _("Could not find unix group %s\n"),
				  argv[1]);
			return -1;
		}
	}

	have_map = pdb_getgrnam(&map, ntgroup);

	if (!have_map) {
		struct dom_sid sid;
		have_map = ( (strncmp(ntgroup, "S-", 2) == 0) &&
			     string_to_sid(&sid, ntgroup) &&
			     pdb_getgrsid(&map, sid) );
	}

	if (!have_map) {

		/* Ok, add it */

		if (grp == NULL) {
			d_fprintf(stderr,
				  _("Could not find group mapping for %s\n"),
				  ntgroup);
			return -1;
		}

		map.gid = grp->gr_gid;

		if (c->opt_rid == 0) {
			if ( pdb_capabilities() & PDB_CAP_STORE_RIDS ) {
				if ( !pdb_new_rid((uint32*)&c->opt_rid) ) {
					d_fprintf( stderr,
					    _("Could not allocate new RID\n"));
					return -1;
				}
			} else {
				c->opt_rid = algorithmic_pdb_gid_to_group_rid(map.gid);
			}
		}

		sid_compose(&map.sid, get_global_sam_sid(), c->opt_rid);

		map.sid_name_use = SID_NAME_DOM_GRP;
		fstrcpy(map.nt_name, ntgroup);
		fstrcpy(map.comment, "");

		if (!NT_STATUS_IS_OK(pdb_add_group_mapping_entry(&map))) {
			d_fprintf(stderr,
				  _("Could not add mapping entry for %s\n"),
				  ntgroup);
			return -1;
		}
	}

	/* Now we have a mapping entry, update that stuff */

	if ( c->opt_localgroup || c->opt_domaingroup ) {
		if (map.sid_name_use == SID_NAME_WKN_GRP) {
			d_fprintf(stderr,
				  _("Can't change type of the BUILTIN "
				    "group %s\n"),
				  map.nt_name);
			return -1;
		}
	}

	if (c->opt_localgroup)
		map.sid_name_use = SID_NAME_ALIAS;

	if (c->opt_domaingroup)
		map.sid_name_use = SID_NAME_DOM_GRP;

	/* The case (opt_domaingroup && opt_localgroup) was tested for above */

	if ((c->opt_comment != NULL) && (strlen(c->opt_comment) > 0)) {
		fstrcpy(map.comment, c->opt_comment);
	}

	if ((c->opt_newntname != NULL) && (strlen(c->opt_newntname) > 0)) {
		fstrcpy(map.nt_name, c->opt_newntname);
	}

	if (grp != NULL)
		map.gid = grp->gr_gid;

	if (!NT_STATUS_IS_OK(pdb_update_group_mapping_entry(&map))) {
		d_fprintf(stderr, _("Could not update group mapping for %s\n"),
			  ntgroup);
		return -1;
	}

	return 0;
}

static int net_groupmap_cleanup(struct net_context *c, int argc, const char **argv)
{
	GROUP_MAP *map = NULL;
	size_t i, entries;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net groupmap cleanup\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Delete all group mappings"));
		return 0;
	}

	if (!pdb_enum_group_mapping(NULL, SID_NAME_UNKNOWN, &map, &entries,
				    ENUM_ALL_MAPPED)) {
		d_fprintf(stderr, _("Could not list group mappings\n"));
		return -1;
	}

	for (i=0; i<entries; i++) {

		if (map[i].gid == -1)
			printf(_("Group %s is not mapped\n"), map[i].nt_name);

		if (!sid_check_is_in_our_domain(&map[i].sid)) {
			printf(_("Deleting mapping for NT Group %s, sid %s\n"),
			       map[i].nt_name,
			       sid_string_tos(&map[i].sid));
			pdb_delete_group_mapping_entry(map[i].sid);
		}
	}

	SAFE_FREE(map);

	return 0;
}

static int net_groupmap_addmem(struct net_context *c, int argc, const char **argv)
{
	struct dom_sid alias, member;

	if ( (argc != 2) ||
	     c->display_usage ||
	     !string_to_sid(&alias, argv[0]) ||
	     !string_to_sid(&member, argv[1]) ) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net groupmap addmem alias-sid member-sid\n"));
		return -1;
	}

	if (!NT_STATUS_IS_OK(pdb_add_aliasmem(&alias, &member))) {
		d_fprintf(stderr, _("Could not add sid %s to alias %s\n"),
			 argv[1], argv[0]);
		return -1;
	}

	return 0;
}

static int net_groupmap_delmem(struct net_context *c, int argc, const char **argv)
{
	struct dom_sid alias, member;

	if ( (argc != 2) ||
	     c->display_usage ||
	     !string_to_sid(&alias, argv[0]) ||
	     !string_to_sid(&member, argv[1]) ) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net groupmap delmem alias-sid member-sid\n"));
		return -1;
	}

	if (!NT_STATUS_IS_OK(pdb_del_aliasmem(&alias, &member))) {
		d_fprintf(stderr, _("Could not delete sid %s from alias %s\n"),
			 argv[1], argv[0]);
		return -1;
	}

	return 0;
}

static int net_groupmap_listmem(struct net_context *c, int argc, const char **argv)
{
	struct dom_sid alias;
	struct dom_sid *members;
	size_t i, num;

	if ( (argc != 1) ||
	     c->display_usage ||
	     !string_to_sid(&alias, argv[0]) ) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net groupmap listmem alias-sid\n"));
		return -1;
	}

	members = NULL;
	num = 0;

	if (!NT_STATUS_IS_OK(pdb_enum_aliasmem(&alias, talloc_tos(),
					       &members, &num))) {
		d_fprintf(stderr, _("Could not list members for sid %s\n"),
			  argv[0]);
		return -1;
	}

	for (i = 0; i < num; i++) {
		printf("%s\n", sid_string_tos(&(members[i])));
	}

	TALLOC_FREE(members);

	return 0;
}

static bool print_alias_memberships(TALLOC_CTX *mem_ctx,
				    const struct dom_sid *domain_sid,
				    const struct dom_sid *member)
{
	uint32 *alias_rids;
	size_t i, num_alias_rids;

	alias_rids = NULL;
	num_alias_rids = 0;

	if (!NT_STATUS_IS_OK(pdb_enum_alias_memberships(
				     mem_ctx, domain_sid, member, 1,
				     &alias_rids, &num_alias_rids))) {
		d_fprintf(stderr, _("Could not list memberships for sid %s\n"),
			 sid_string_tos(member));
		return false;
	}

	for (i = 0; i < num_alias_rids; i++) {
		struct dom_sid alias;
		sid_compose(&alias, domain_sid, alias_rids[i]);
		printf("%s\n", sid_string_tos(&alias));
	}

	return true;
}

static int net_groupmap_memberships(struct net_context *c, int argc, const char **argv)
{
	TALLOC_CTX *mem_ctx;
	struct dom_sid *domain_sid, member;

	if ( (argc != 1) ||
	     c->display_usage ||
	     !string_to_sid(&member, argv[0]) ) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net groupmap memberships sid\n"));
		return -1;
	}

	mem_ctx = talloc_init("net_groupmap_memberships");
	if (mem_ctx == NULL) {
		d_fprintf(stderr, _("talloc_init failed\n"));
		return -1;
	}

	domain_sid = get_global_sam_sid();
	if (domain_sid == NULL) {
		d_fprintf(stderr, _("Could not get domain sid\n"));
		return -1;
	}

	if (!print_alias_memberships(mem_ctx, domain_sid, &member) ||
	    !print_alias_memberships(mem_ctx, &global_sid_Builtin, &member))
		return -1;

	talloc_destroy(mem_ctx);

	return 0;
}

/***********************************************************
 migrated functionality from smbgroupedit
 **********************************************************/
int net_groupmap(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"add",
			net_groupmap_add,
			NET_TRANSPORT_LOCAL,
			N_("Create a new group mapping"),
			N_("net groupmap add\n"
			   "    Create a new group mapping")
		},
		{
			"modify",
			net_groupmap_modify,
			NET_TRANSPORT_LOCAL,
			N_("Update a group mapping"),
			N_("net groupmap modify\n"
			   "    Modify an existing group mapping")
		},
		{
			"delete",
			net_groupmap_delete,
			NET_TRANSPORT_LOCAL,
			N_("Remove a group mapping"),
			N_("net groupmap delete\n"
			   "    Remove a group mapping")
		},
		{
			"set",
			net_groupmap_set,
			NET_TRANSPORT_LOCAL,
			N_("Set group mapping"),
			N_("net groupmap set\n"
			   "    Set a group mapping")
		},
		{
			"cleanup",
			net_groupmap_cleanup,
			NET_TRANSPORT_LOCAL,
			N_("Remove foreign group mapping entries"),
			N_("net groupmap cleanup\n"
			   "    Remove foreign group mapping entries")
		},
		{
			"addmem",
			net_groupmap_addmem,
			NET_TRANSPORT_LOCAL,
			N_("Add a foreign alias member"),
			N_("net groupmap addmem\n"
			   "    Add a foreign alias member")
		},
		{
			"delmem",
			net_groupmap_delmem,
			NET_TRANSPORT_LOCAL,
			N_("Delete foreign alias member"),
			N_("net groupmap delmem\n"
			   "    Delete foreign alias member")
		},
		{
			"listmem",
			net_groupmap_listmem,
			NET_TRANSPORT_LOCAL,
			N_("List foreign group members"),
			N_("net groupmap listmem\n"
			   "    List foreign alias members")
		},
		{
			"memberships",
			net_groupmap_memberships,
			NET_TRANSPORT_LOCAL,
			N_("List foreign group memberships"),
			N_("net groupmap memberships\n"
			   "    List foreign group memberships")
		},
		{
			"list",
			net_groupmap_list,
			NET_TRANSPORT_LOCAL,
			N_("List current group map"),
			N_("net groupmap list\n"
			   "    List current group map")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c,argc, argv, "net groupmap", func);
}

