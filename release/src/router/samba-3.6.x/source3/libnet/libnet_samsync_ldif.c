/*
   Unix SMB/CIFS implementation.
   dump the remote SAM using rpc samsync operations

   Copyright (C) Andrew Tridgell 2002
   Copyright (C) Tim Potter 2001,2002
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2005
   Modified by Volker Lendecke 2002
   Copyright (C) Jeremy Allison 2005.
   Copyright (C) Guenther Deschner 2008.

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
#include "libnet/libnet_samsync.h"
#include "smbldap.h"
#include "transfer_file.h"
#include "passdb.h"

#ifdef HAVE_LDAP

/* uid's and gid's for writing deltas to ldif */
static uint32 ldif_gid = 999;
static uint32 ldif_uid = 999;

/* global counters */
static uint32_t g_index = 0;
static uint32_t a_index = 0;

/* Structure for mapping accounts to groups */
/* Array element is the group rid */
typedef struct _groupmap {
	uint32_t rid;
	uint32_t gidNumber;
	const char *sambaSID;
	const char *group_dn;
} GROUPMAP;

typedef struct _accountmap {
	uint32_t rid;
	const char *cn;
} ACCOUNTMAP;

struct samsync_ldif_context {
	GROUPMAP *groupmap;
	ACCOUNTMAP *accountmap;
	bool initialized;
	const char *add_template;
	const char *mod_template;
	char *add_name;
	char *module_name;
	FILE *add_file;
	FILE *mod_file;
	FILE *ldif_file;
	const char *suffix;
	int num_alloced;
};

/****************************************************************
****************************************************************/

static NTSTATUS populate_ldap_for_ldif(const char *sid,
				       const char *suffix,
				       const char *builtin_sid,
				       FILE *add_fd)
{
	const char *user_suffix, *group_suffix, *machine_suffix, *idmap_suffix;
	char *user_attr=NULL, *group_attr=NULL;
	char *suffix_attr;
	int len;

	/* Get the suffix attribute */
	suffix_attr = sstring_sub(suffix, '=', ',');
	if (suffix_attr == NULL) {
		len = strlen(suffix);
		suffix_attr = (char*)SMB_MALLOC(len+1);
		if (!suffix_attr) {
			return NT_STATUS_NO_MEMORY;
		}
		memcpy(suffix_attr, suffix, len);
		suffix_attr[len] = '\0';
	}

	/* Write the base */
	fprintf(add_fd, "# %s\n", suffix);
	fprintf(add_fd, "dn: %s\n", suffix);
	fprintf(add_fd, "objectClass: dcObject\n");
	fprintf(add_fd, "objectClass: organization\n");
	fprintf(add_fd, "o: %s\n", suffix_attr);
	fprintf(add_fd, "dc: %s\n", suffix_attr);
	fprintf(add_fd, "\n");
	fflush(add_fd);

	user_suffix = lp_ldap_user_suffix();
	if (user_suffix == NULL) {
		SAFE_FREE(suffix_attr);
		return NT_STATUS_NO_MEMORY;
	}
	/* If it exists and is distinct from other containers,
	   Write the Users entity */
	if (*user_suffix && strcmp(user_suffix, suffix)) {
		user_attr = sstring_sub(lp_ldap_user_suffix(), '=', ',');
		fprintf(add_fd, "# %s\n", user_suffix);
		fprintf(add_fd, "dn: %s\n", user_suffix);
		fprintf(add_fd, "objectClass: organizationalUnit\n");
		fprintf(add_fd, "ou: %s\n", user_attr);
		fprintf(add_fd, "\n");
		fflush(add_fd);
	}


	group_suffix = lp_ldap_group_suffix();
	if (group_suffix == NULL) {
		SAFE_FREE(suffix_attr);
		SAFE_FREE(user_attr);
		return NT_STATUS_NO_MEMORY;
	}
	/* If it exists and is distinct from other containers,
	   Write the Groups entity */
	if (*group_suffix && strcmp(group_suffix, suffix)) {
		group_attr = sstring_sub(lp_ldap_group_suffix(), '=', ',');
		fprintf(add_fd, "# %s\n", group_suffix);
		fprintf(add_fd, "dn: %s\n", group_suffix);
		fprintf(add_fd, "objectClass: organizationalUnit\n");
		fprintf(add_fd, "ou: %s\n", group_attr);
		fprintf(add_fd, "\n");
		fflush(add_fd);
	}

	/* If it exists and is distinct from other containers,
	   Write the Computers entity */
	machine_suffix = lp_ldap_machine_suffix();
	if (machine_suffix == NULL) {
		SAFE_FREE(suffix_attr);
		SAFE_FREE(user_attr);
		SAFE_FREE(group_attr);
		return NT_STATUS_NO_MEMORY;
	}
	if (*machine_suffix && strcmp(machine_suffix, user_suffix) &&
	    strcmp(machine_suffix, suffix)) {
		char *machine_ou = NULL;
		fprintf(add_fd, "# %s\n", machine_suffix);
		fprintf(add_fd, "dn: %s\n", machine_suffix);
		fprintf(add_fd, "objectClass: organizationalUnit\n");
		/* this isn't totally correct as it assumes that
		   there _must_ be an ou. just fixing memleak now. jmcd */
		machine_ou = sstring_sub(lp_ldap_machine_suffix(), '=', ',');
		fprintf(add_fd, "ou: %s\n", machine_ou);
		SAFE_FREE(machine_ou);
		fprintf(add_fd, "\n");
		fflush(add_fd);
	}

	/* If it exists and is distinct from other containers,
	   Write the IdMap entity */
	idmap_suffix = lp_ldap_idmap_suffix();
	if (idmap_suffix == NULL) {
		SAFE_FREE(suffix_attr);
		SAFE_FREE(user_attr);
		SAFE_FREE(group_attr);
		return NT_STATUS_NO_MEMORY;
	}
	if (*idmap_suffix &&
	    strcmp(idmap_suffix, user_suffix) &&
	    strcmp(idmap_suffix, suffix)) {
		char *s;
		fprintf(add_fd, "# %s\n", idmap_suffix);
		fprintf(add_fd, "dn: %s\n", idmap_suffix);
		fprintf(add_fd, "ObjectClass: organizationalUnit\n");
		s = sstring_sub(lp_ldap_idmap_suffix(), '=', ',');
		fprintf(add_fd, "ou: %s\n", s);
		SAFE_FREE(s);
		fprintf(add_fd, "\n");
		fflush(add_fd);
	}

	/* Write the domain entity */
	fprintf(add_fd, "# %s, %s\n", lp_workgroup(), suffix);
	fprintf(add_fd, "dn: sambaDomainName=%s,%s\n", lp_workgroup(),
		suffix);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_DOMINFO);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_IDPOOL);
	fprintf(add_fd, "sambaDomainName: %s\n", lp_workgroup());
	fprintf(add_fd, "sambaSID: %s\n", sid);
	fprintf(add_fd, "uidNumber: %d\n", ++ldif_uid);
	fprintf(add_fd, "gidNumber: %d\n", ++ldif_gid);
	fprintf(add_fd, "\n");
	fflush(add_fd);

	/* Write the Domain Admins entity */
	fprintf(add_fd, "# Domain Admins, %s, %s\n", group_attr,
		suffix);
	fprintf(add_fd, "dn: cn=Domain Admins,ou=%s,%s\n", group_attr,
		suffix);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_POSIXGROUP);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_GROUPMAP);
	fprintf(add_fd, "cn: Domain Admins\n");
	fprintf(add_fd, "memberUid: Administrator\n");
	fprintf(add_fd, "description: Netbios Domain Administrators\n");
	fprintf(add_fd, "gidNumber: 512\n");
	fprintf(add_fd, "sambaSID: %s-512\n", sid);
	fprintf(add_fd, "sambaGroupType: 2\n");
	fprintf(add_fd, "displayName: Domain Admins\n");
	fprintf(add_fd, "\n");
	fflush(add_fd);

	/* Write the Domain Users entity */
	fprintf(add_fd, "# Domain Users, %s, %s\n", group_attr,
		suffix);
	fprintf(add_fd, "dn: cn=Domain Users,ou=%s,%s\n", group_attr,
		suffix);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_POSIXGROUP);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_GROUPMAP);
	fprintf(add_fd, "cn: Domain Users\n");
	fprintf(add_fd, "description: Netbios Domain Users\n");
	fprintf(add_fd, "gidNumber: 513\n");
	fprintf(add_fd, "sambaSID: %s-513\n", sid);
	fprintf(add_fd, "sambaGroupType: 2\n");
	fprintf(add_fd, "displayName: Domain Users\n");
	fprintf(add_fd, "\n");
	fflush(add_fd);

	/* Write the Domain Guests entity */
	fprintf(add_fd, "# Domain Guests, %s, %s\n", group_attr,
		suffix);
	fprintf(add_fd, "dn: cn=Domain Guests,ou=%s,%s\n", group_attr,
		suffix);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_POSIXGROUP);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_GROUPMAP);
	fprintf(add_fd, "cn: Domain Guests\n");
	fprintf(add_fd, "description: Netbios Domain Guests\n");
	fprintf(add_fd, "gidNumber: 514\n");
	fprintf(add_fd, "sambaSID: %s-514\n", sid);
	fprintf(add_fd, "sambaGroupType: 2\n");
	fprintf(add_fd, "displayName: Domain Guests\n");
	fprintf(add_fd, "\n");
	fflush(add_fd);

	/* Write the Domain Computers entity */
	fprintf(add_fd, "# Domain Computers, %s, %s\n", group_attr,
		suffix);
	fprintf(add_fd, "dn: cn=Domain Computers,ou=%s,%s\n",
		group_attr, suffix);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_POSIXGROUP);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_GROUPMAP);
	fprintf(add_fd, "gidNumber: 515\n");
	fprintf(add_fd, "cn: Domain Computers\n");
	fprintf(add_fd, "description: Netbios Domain Computers accounts\n");
	fprintf(add_fd, "sambaSID: %s-515\n", sid);
	fprintf(add_fd, "sambaGroupType: 2\n");
	fprintf(add_fd, "displayName: Domain Computers\n");
	fprintf(add_fd, "\n");
	fflush(add_fd);

	/* Write the Admininistrators Groups entity */
	fprintf(add_fd, "# Administrators, %s, %s\n", group_attr,
		suffix);
	fprintf(add_fd, "dn: cn=Administrators,ou=%s,%s\n", group_attr,
		suffix);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_POSIXGROUP);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_GROUPMAP);
	fprintf(add_fd, "gidNumber: 544\n");
	fprintf(add_fd, "cn: Administrators\n");
	fprintf(add_fd, "description: Netbios Domain Members can fully administer the computer/sambaDomainName\n");
	fprintf(add_fd, "sambaSID: %s-544\n", builtin_sid);
	fprintf(add_fd, "sambaGroupType: 5\n");
	fprintf(add_fd, "displayName: Administrators\n");
	fprintf(add_fd, "\n");

	/* Write the Print Operator entity */
	fprintf(add_fd, "# Print Operators, %s, %s\n", group_attr,
		suffix);
	fprintf(add_fd, "dn: cn=Print Operators,ou=%s,%s\n",
		group_attr, suffix);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_POSIXGROUP);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_GROUPMAP);
	fprintf(add_fd, "gidNumber: 550\n");
	fprintf(add_fd, "cn: Print Operators\n");
	fprintf(add_fd, "description: Netbios Domain Print Operators\n");
	fprintf(add_fd, "sambaSID: %s-550\n", builtin_sid);
	fprintf(add_fd, "sambaGroupType: 5\n");
	fprintf(add_fd, "displayName: Print Operators\n");
	fprintf(add_fd, "\n");
	fflush(add_fd);

	/* Write the Backup Operators entity */
	fprintf(add_fd, "# Backup Operators, %s, %s\n", group_attr,
		suffix);
	fprintf(add_fd, "dn: cn=Backup Operators,ou=%s,%s\n",
		group_attr, suffix);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_POSIXGROUP);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_GROUPMAP);
	fprintf(add_fd, "gidNumber: 551\n");
	fprintf(add_fd, "cn: Backup Operators\n");
	fprintf(add_fd, "description: Netbios Domain Members can bypass file security to back up files\n");
	fprintf(add_fd, "sambaSID: %s-551\n", builtin_sid);
	fprintf(add_fd, "sambaGroupType: 5\n");
	fprintf(add_fd, "displayName: Backup Operators\n");
	fprintf(add_fd, "\n");
	fflush(add_fd);

	/* Write the Replicators entity */
	fprintf(add_fd, "# Replicators, %s, %s\n", group_attr, suffix);
	fprintf(add_fd, "dn: cn=Replicators,ou=%s,%s\n", group_attr,
		suffix);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_POSIXGROUP);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_GROUPMAP);
	fprintf(add_fd, "gidNumber: 552\n");
	fprintf(add_fd, "cn: Replicators\n");
	fprintf(add_fd, "description: Netbios Domain Supports file replication in a sambaDomainName\n");
	fprintf(add_fd, "sambaSID: %s-552\n", builtin_sid);
	fprintf(add_fd, "sambaGroupType: 5\n");
	fprintf(add_fd, "displayName: Replicators\n");
	fprintf(add_fd, "\n");
	fflush(add_fd);

	/* Deallocate memory, and return */
	SAFE_FREE(suffix_attr);
	SAFE_FREE(user_attr);
	SAFE_FREE(group_attr);
	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS map_populate_groups(TALLOC_CTX *mem_ctx,
				    GROUPMAP *groupmap,
				    ACCOUNTMAP *accountmap,
				    const char *sid,
				    const char *suffix,
				    const char *builtin_sid)
{
	char *group_attr = sstring_sub(lp_ldap_group_suffix(), '=', ',');

	/* Map the groups created by populate_ldap_for_ldif */
	groupmap[0].rid		= 512;
	groupmap[0].gidNumber	= 512;
	groupmap[0].sambaSID	= talloc_asprintf(mem_ctx, "%s-512", sid);
	groupmap[0].group_dn	= talloc_asprintf(mem_ctx,
		"cn=Domain Admins,ou=%s,%s", group_attr, suffix);
	if (groupmap[0].sambaSID == NULL || groupmap[0].group_dn == NULL) {
		goto err;
	}

	accountmap[0].rid	= 512;
	accountmap[0].cn	= talloc_strdup(mem_ctx, "Domain Admins");
	if (accountmap[0].cn == NULL) {
		goto err;
	}

	groupmap[1].rid		= 513;
	groupmap[1].gidNumber	= 513;
	groupmap[1].sambaSID	= talloc_asprintf(mem_ctx, "%s-513", sid);
	groupmap[1].group_dn	= talloc_asprintf(mem_ctx,
		"cn=Domain Users,ou=%s,%s", group_attr, suffix);
	if (groupmap[1].sambaSID == NULL || groupmap[1].group_dn == NULL) {
		goto err;
	}

	accountmap[1].rid	= 513;
	accountmap[1].cn	= talloc_strdup(mem_ctx, "Domain Users");
	if (accountmap[1].cn == NULL) {
		goto err;
	}

	groupmap[2].rid		= 514;
	groupmap[2].gidNumber	= 514;
	groupmap[2].sambaSID	= talloc_asprintf(mem_ctx, "%s-514", sid);
	groupmap[2].group_dn	= talloc_asprintf(mem_ctx,
		"cn=Domain Guests,ou=%s,%s", group_attr, suffix);
	if (groupmap[2].sambaSID == NULL || groupmap[2].group_dn == NULL) {
		goto err;
	}

	accountmap[2].rid	= 514;
	accountmap[2].cn	= talloc_strdup(mem_ctx, "Domain Guests");
	if (accountmap[2].cn == NULL) {
		goto err;
	}

	groupmap[3].rid		= 515;
	groupmap[3].gidNumber	= 515;
	groupmap[3].sambaSID	= talloc_asprintf(mem_ctx, "%s-515", sid);
	groupmap[3].group_dn	= talloc_asprintf(mem_ctx,
		"cn=Domain Computers,ou=%s,%s", group_attr, suffix);
	if (groupmap[3].sambaSID == NULL || groupmap[3].group_dn == NULL) {
		goto err;
	}

	accountmap[3].rid	= 515;
	accountmap[3].cn	= talloc_strdup(mem_ctx, "Domain Computers");
	if (accountmap[3].cn == NULL) {
		goto err;
	}

	groupmap[4].rid		= 544;
	groupmap[4].gidNumber	= 544;
	groupmap[4].sambaSID	= talloc_asprintf(mem_ctx, "%s-544", builtin_sid);
	groupmap[4].group_dn	= talloc_asprintf(mem_ctx,
		"cn=Administrators,ou=%s,%s", group_attr, suffix);
	if (groupmap[4].sambaSID == NULL || groupmap[4].group_dn == NULL) {
		goto err;
	}

	accountmap[4].rid	= 515;
	accountmap[4].cn	= talloc_strdup(mem_ctx, "Administrators");
	if (accountmap[4].cn == NULL) {
		goto err;
	}

	groupmap[5].rid		= 550;
	groupmap[5].gidNumber	= 550;
	groupmap[5].sambaSID	= talloc_asprintf(mem_ctx, "%s-550", builtin_sid);
	groupmap[5].group_dn	= talloc_asprintf(mem_ctx,
		"cn=Print Operators,ou=%s,%s", group_attr, suffix);
	if (groupmap[5].sambaSID == NULL || groupmap[5].group_dn == NULL) {
		goto err;
	}

	accountmap[5].rid	= 550;
	accountmap[5].cn	= talloc_strdup(mem_ctx, "Print Operators");
	if (accountmap[5].cn == NULL) {
		goto err;
	}

	groupmap[6].rid		= 551;
	groupmap[6].gidNumber	= 551;
	groupmap[6].sambaSID	= talloc_asprintf(mem_ctx, "%s-551", builtin_sid);
	groupmap[6].group_dn	= talloc_asprintf(mem_ctx,
		"cn=Backup Operators,ou=%s,%s", group_attr, suffix);
	if (groupmap[6].sambaSID == NULL || groupmap[6].group_dn == NULL) {
		goto err;
	}

	accountmap[6].rid	= 551;
	accountmap[6].cn	= talloc_strdup(mem_ctx, "Backup Operators");
	if (accountmap[6].cn == NULL) {
		goto err;
	}

	groupmap[7].rid		= 552;
	groupmap[7].gidNumber	= 552;
	groupmap[7].sambaSID	= talloc_asprintf(mem_ctx, "%s-552", builtin_sid);
	groupmap[7].group_dn	= talloc_asprintf(mem_ctx,
		"cn=Replicators,ou=%s,%s", group_attr, suffix);
	if (groupmap[7].sambaSID == NULL || groupmap[7].group_dn == NULL) {
		goto err;
	}

	accountmap[7].rid	= 551;
	accountmap[7].cn	= talloc_strdup(mem_ctx, "Replicators");
	if (accountmap[7].cn == NULL) {
		goto err;
	}

	SAFE_FREE(group_attr);

	return NT_STATUS_OK;

  err:

	SAFE_FREE(group_attr);
	return NT_STATUS_NO_MEMORY;
}

/*
 * This is a crap routine, but I think it's the quickest way to solve the
 * UTF8->base64 problem.
 */

static int fprintf_attr(FILE *add_fd, const char *attr_name,
			const char *fmt, ...)
{
	va_list ap;
	char *value, *p, *base64;
	DATA_BLOB base64_blob;
	bool do_base64 = false;
	int res;

	va_start(ap, fmt);
	value = talloc_vasprintf(NULL, fmt, ap);
	va_end(ap);

	SMB_ASSERT(value != NULL);

	for (p=value; *p; p++) {
		if (*p & 0x80) {
			do_base64 = true;
			break;
		}
	}

	if (!do_base64) {
		bool only_whitespace = true;
		for (p=value; *p; p++) {
			/*
			 * I know that this not multibyte safe, but we break
			 * on the first non-whitespace character anyway.
			 */
			if (!isspace(*p)) {
				only_whitespace = false;
				break;
			}
		}
		if (only_whitespace) {
			do_base64 = true;
		}
	}

	if (!do_base64) {
		res = fprintf(add_fd, "%s: %s\n", attr_name, value);
		TALLOC_FREE(value);
		return res;
	}

	base64_blob.data = (unsigned char *)value;
	base64_blob.length = strlen(value);

	base64 = base64_encode_data_blob(value, base64_blob);
	SMB_ASSERT(base64 != NULL);

	res = fprintf(add_fd, "%s:: %s\n", attr_name, base64);
	TALLOC_FREE(value);
	return res;
}

/****************************************************************
****************************************************************/

static NTSTATUS fetch_group_info_to_ldif(TALLOC_CTX *mem_ctx,
					 struct netr_DELTA_GROUP *r,
					 GROUPMAP *groupmap,
					 FILE *add_fd,
					 const char *sid,
					 const char *suffix)
{
	const char *groupname = r->group_name.string;
	uint32 grouptype = 0, g_rid = 0;
	char *group_attr = sstring_sub(lp_ldap_group_suffix(), '=', ',');

	/* Set up the group type (always 2 for group info) */
	grouptype = 2;

	/* These groups are entered by populate_ldap_for_ldif */
	if (strcmp(groupname, "Domain Admins") == 0 ||
            strcmp(groupname, "Domain Users") == 0 ||
	    strcmp(groupname, "Domain Guests") == 0 ||
	    strcmp(groupname, "Domain Computers") == 0 ||
	    strcmp(groupname, "Administrators") == 0 ||
	    strcmp(groupname, "Print Operators") == 0 ||
	    strcmp(groupname, "Backup Operators") == 0 ||
	    strcmp(groupname, "Replicators") == 0) {
		SAFE_FREE(group_attr);
		return NT_STATUS_OK;
	} else {
		/* Increment the gid for the new group */
	        ldif_gid++;
	}

	/* Map the group rid, gid, and dn */
	g_rid = r->rid;
	groupmap->rid = g_rid;
	groupmap->gidNumber = ldif_gid;
	groupmap->sambaSID	= talloc_asprintf(mem_ctx, "%s-%d", sid, g_rid);
	groupmap->group_dn	= talloc_asprintf(mem_ctx,
	     "cn=%s,ou=%s,%s", groupname, group_attr, suffix);
	if (groupmap->sambaSID == NULL || groupmap->group_dn == NULL) {
		SAFE_FREE(group_attr);
		return NT_STATUS_NO_MEMORY;
	}

	/* Write the data to the temporary add ldif file */
	fprintf(add_fd, "# %s, %s, %s\n", groupname, group_attr,
		suffix);
	fprintf_attr(add_fd, "dn", "cn=%s,ou=%s,%s", groupname, group_attr,
		     suffix);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_POSIXGROUP);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_GROUPMAP);
	fprintf_attr(add_fd, "cn", "%s", groupname);
	fprintf(add_fd, "gidNumber: %d\n", ldif_gid);
	fprintf(add_fd, "sambaSID: %s\n", groupmap->sambaSID);
	fprintf(add_fd, "sambaGroupType: %d\n", grouptype);
	fprintf_attr(add_fd, "displayName", "%s", groupname);
	fprintf(add_fd, "\n");
	fflush(add_fd);

	SAFE_FREE(group_attr);
	/* Return */
	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS fetch_account_info_to_ldif(TALLOC_CTX *mem_ctx,
					   struct netr_DELTA_USER *r,
					   GROUPMAP *groupmap,
					   ACCOUNTMAP *accountmap,
					   FILE *add_fd,
					   const char *sid,
					   const char *suffix,
					   int alloced)
{
	fstring username, logonscript, homedrive, homepath = "", homedir = "";
	fstring hex_nt_passwd, hex_lm_passwd;
	fstring description, profilepath, fullname, sambaSID;
	char *flags, *user_rdn;
	const char *ou;
	const char* nopasswd = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
	uchar zero_buf[16];
	uint32 rid = 0, group_rid = 0, gidNumber = 0;
	time_t unix_time;
	int i, ret;

	memset(zero_buf, '\0', sizeof(zero_buf));

	/* Get the username */
	fstrcpy(username, r->account_name.string);

	/* Get the rid */
	rid = r->rid;

	/* Map the rid and username for group member info later */
	accountmap->rid = rid;
	accountmap->cn = talloc_strdup(mem_ctx, username);
	NT_STATUS_HAVE_NO_MEMORY(accountmap->cn);

	/* Get the home directory */
	if (r->acct_flags & ACB_NORMAL) {
		fstrcpy(homedir, r->home_directory.string);
		if (!*homedir) {
			snprintf(homedir, sizeof(homedir), "/home/%s", username);
		} else {
			snprintf(homedir, sizeof(homedir), "/nobodyshomedir");
		}
		ou = lp_ldap_user_suffix();
	} else {
		ou = lp_ldap_machine_suffix();
		snprintf(homedir, sizeof(homedir), "/machinehomedir");
	}

        /* Get the logon script */
	fstrcpy(logonscript, r->logon_script.string);

        /* Get the home drive */
	fstrcpy(homedrive, r->home_drive.string);

        /* Get the home path */
	fstrcpy(homepath, r->home_directory.string);

	/* Get the description */
	fstrcpy(description, r->description.string);

	/* Get the display name */
	fstrcpy(fullname, r->full_name.string);

	/* Get the profile path */
	fstrcpy(profilepath, r->profile_path.string);

	/* Get lm and nt password data */
	if (memcmp(r->lmpassword.hash, zero_buf, 16) != 0) {
		pdb_sethexpwd(hex_lm_passwd, r->lmpassword.hash, r->acct_flags);
	} else {
		pdb_sethexpwd(hex_lm_passwd, NULL, 0);
	}
	if (memcmp(r->ntpassword.hash, zero_buf, 16) != 0) {
		pdb_sethexpwd(hex_nt_passwd, r->ntpassword.hash, r->acct_flags);
	} else {
		pdb_sethexpwd(hex_nt_passwd, NULL, 0);
	}
	unix_time = nt_time_to_unix(r->last_password_change);

	/* Increment the uid for the new user */
	ldif_uid++;

	/* Set up group id and sambaSID for the user */
	group_rid = r->primary_gid;
	for (i=0; i<alloced; i++) {
		if (groupmap[i].rid == group_rid) break;
	}
	if (i == alloced){
		DEBUG(1, ("Could not find rid %d in groupmap array\n",
			  group_rid));
		return NT_STATUS_UNSUCCESSFUL;
	}
	gidNumber = groupmap[i].gidNumber;
	ret = snprintf(sambaSID, sizeof(sambaSID), "%s", groupmap[i].sambaSID);
	if (ret < 0 || ret == sizeof(sambaSID)) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* Set up sambaAcctFlags */
	flags = pdb_encode_acct_ctrl(r->acct_flags,
				     NEW_PW_FORMAT_SPACE_PADDED_LEN);

	/* Add the user to the temporary add ldif file */
	/* this isn't quite right...we can't assume there's just OU=. jmcd */
	user_rdn = sstring_sub(ou, '=', ',');
	fprintf(add_fd, "# %s, %s, %s\n", username, user_rdn, suffix);
	fprintf_attr(add_fd, "dn", "uid=%s,ou=%s,%s", username, user_rdn,
		     suffix);
	SAFE_FREE(user_rdn);
	fprintf(add_fd, "ObjectClass: top\n");
	fprintf(add_fd, "objectClass: inetOrgPerson\n");
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_POSIXACCOUNT);
	fprintf(add_fd, "objectClass: shadowAccount\n");
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_SAMBASAMACCOUNT);
	fprintf_attr(add_fd, "cn", "%s", username);
	fprintf_attr(add_fd, "sn", "%s", username);
	fprintf_attr(add_fd, "uid", "%s", username);
	fprintf(add_fd, "uidNumber: %d\n", ldif_uid);
	fprintf(add_fd, "gidNumber: %d\n", gidNumber);
	fprintf_attr(add_fd, "homeDirectory", "%s", homedir);
	if (*homepath)
		fprintf_attr(add_fd, "sambaHomePath", "%s", homepath);
        if (*homedrive)
                fprintf_attr(add_fd, "sambaHomeDrive", "%s", homedrive);
        if (*logonscript)
                fprintf_attr(add_fd, "sambaLogonScript", "%s", logonscript);
	fprintf(add_fd, "loginShell: %s\n",
		((r->acct_flags & ACB_NORMAL) ?
		 "/bin/bash" : "/bin/false"));
	fprintf(add_fd, "gecos: System User\n");
	if (*description)
		fprintf_attr(add_fd, "description", "%s", description);
	fprintf(add_fd, "sambaSID: %s-%d\n", sid, rid);
	fprintf(add_fd, "sambaPrimaryGroupSID: %s\n", sambaSID);
	if(*fullname)
		fprintf_attr(add_fd, "displayName", "%s", fullname);
	if(*profilepath)
		fprintf_attr(add_fd, "sambaProfilePath", "%s", profilepath);
	if (strcmp(nopasswd, hex_lm_passwd) != 0)
		fprintf(add_fd, "sambaLMPassword: %s\n", hex_lm_passwd);
	if (strcmp(nopasswd, hex_nt_passwd) != 0)
		fprintf(add_fd, "sambaNTPassword: %s\n", hex_nt_passwd);
	fprintf(add_fd, "sambaPwdLastSet: %d\n", (int)unix_time);
	fprintf(add_fd, "sambaAcctFlags: %s\n", flags);
	fprintf(add_fd, "\n");
	fflush(add_fd);

	/* Return */
	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS fetch_alias_info_to_ldif(TALLOC_CTX *mem_ctx,
					 struct netr_DELTA_ALIAS *r,
					 GROUPMAP *groupmap,
					 FILE *add_fd,
					 const char *sid,
					 const char *suffix,
					 enum netr_SamDatabaseID database_id)
{
	fstring aliasname, description;
	uint32 grouptype = 0, g_rid = 0;
	char *group_attr = sstring_sub(lp_ldap_group_suffix(), '=', ',');

	/* Get the alias name */
	fstrcpy(aliasname, r->alias_name.string);

	/* Get the alias description */
	fstrcpy(description, r->description.string);

	/* Set up the group type */
	switch (database_id) {
	case SAM_DATABASE_DOMAIN:
		grouptype = 4;
		break;
	case SAM_DATABASE_BUILTIN:
		grouptype = 5;
		break;
	default:
		grouptype = 4;
		break;
	}

	/*
	  These groups are entered by populate_ldap_for_ldif
	  Note that populate creates a group called Relicators,
	  but NT returns a group called Replicator
	*/
	if (strcmp(aliasname, "Domain Admins") == 0 ||
	    strcmp(aliasname, "Domain Users") == 0 ||
	    strcmp(aliasname, "Domain Guests") == 0 ||
	    strcmp(aliasname, "Domain Computers") == 0 ||
	    strcmp(aliasname, "Administrators") == 0 ||
	    strcmp(aliasname, "Print Operators") == 0 ||
	    strcmp(aliasname, "Backup Operators") == 0 ||
	    strcmp(aliasname, "Replicator") == 0) {
		SAFE_FREE(group_attr);
		return NT_STATUS_OK;
	} else {
		/* Increment the gid for the new group */
		ldif_gid++;
	}

	/* Map the group rid and gid */
	g_rid = r->rid;
	groupmap->gidNumber = ldif_gid;
	groupmap->sambaSID = talloc_asprintf(mem_ctx, "%s-%d", sid, g_rid);
	if (groupmap->sambaSID == NULL) {
		SAFE_FREE(group_attr);
		return NT_STATUS_NO_MEMORY;
	}

	/* Write the data to the temporary add ldif file */
	fprintf(add_fd, "# %s, %s, %s\n", aliasname, group_attr,
		suffix);
	fprintf_attr(add_fd, "dn", "cn=%s,ou=%s,%s", aliasname, group_attr,
		     suffix);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_POSIXGROUP);
	fprintf(add_fd, "objectClass: %s\n", LDAP_OBJ_GROUPMAP);
	fprintf(add_fd, "cn: %s\n", aliasname);
	fprintf(add_fd, "gidNumber: %d\n", ldif_gid);
	fprintf(add_fd, "sambaSID: %s\n", groupmap->sambaSID);
	fprintf(add_fd, "sambaGroupType: %d\n", grouptype);
	fprintf_attr(add_fd, "displayName", "%s", aliasname);
	if (description[0])
		fprintf_attr(add_fd, "description", "%s", description);
	fprintf(add_fd, "\n");
	fflush(add_fd);

	SAFE_FREE(group_attr);
	/* Return */
	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS fetch_groupmem_info_to_ldif(struct netr_DELTA_GROUP_MEMBER *r,
					    uint32_t id_rid,
					    GROUPMAP *groupmap,
					    ACCOUNTMAP *accountmap,
					    FILE *mod_fd, int alloced)
{
	fstring group_dn;
	uint32 group_rid = 0, rid = 0;
	int i, j, k;

	/* Get the dn for the group */
	if (r->num_rids > 0) {
		group_rid = id_rid;
		for (j=0; j<alloced; j++) {
			if (groupmap[j].rid == group_rid) break;
		}
		if (j == alloced){
			DEBUG(1, ("Could not find rid %d in groupmap array\n",
				  group_rid));
			return NT_STATUS_UNSUCCESSFUL;
		}
		snprintf(group_dn, sizeof(group_dn), "%s", groupmap[j].group_dn);
		fprintf(mod_fd, "dn: %s\n", group_dn);

		/* Get the cn for each member */
		for (i=0; i < r->num_rids; i++) {
			rid = r->rids[i];
			for (k=0; k<alloced; k++) {
				if (accountmap[k].rid == rid) break;
			}
			if (k == alloced){
				DEBUG(1, ("Could not find rid %d in "
					  "accountmap array\n", rid));
				return NT_STATUS_UNSUCCESSFUL;
			}
			fprintf(mod_fd, "memberUid: %s\n", accountmap[k].cn);
		}
		fprintf(mod_fd, "\n");
	}
	fflush(mod_fd);

	/* Return */
	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS ldif_init_context(TALLOC_CTX *mem_ctx,
				  enum netr_SamDatabaseID database_id,
				  const char *ldif_filename,
				  const char *domain_sid_str,
				  struct samsync_ldif_context **ctx)
{
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	struct samsync_ldif_context *r;
	const char *add_template = "/tmp/add.ldif.XXXXXX";
	const char *mod_template = "/tmp/mod.ldif.XXXXXX";
	const char *builtin_sid = "S-1-5-32";

	/* Get other smb.conf data */
	if (!(lp_workgroup()) || !*(lp_workgroup())) {
		DEBUG(0,("workgroup missing from smb.conf--exiting\n"));
		exit(1);
	}

	/* Get the ldap suffix */
	if (!(lp_ldap_suffix()) || !*(lp_ldap_suffix())) {
		DEBUG(0,("ldap suffix missing from smb.conf--exiting\n"));
		exit(1);
	}

	if (*ctx && (*ctx)->initialized) {
		return NT_STATUS_OK;
	}

	r = TALLOC_ZERO_P(mem_ctx, struct samsync_ldif_context);
	NT_STATUS_HAVE_NO_MEMORY(r);

	/* Get the ldap suffix */
	r->suffix = lp_ldap_suffix();

	/* Ensure we have an output file */
	if (ldif_filename) {
		r->ldif_file = fopen(ldif_filename, "a");
	} else {
		r->ldif_file = stdout;
	}

	if (!r->ldif_file) {
		fprintf(stderr, "Could not open %s\n", ldif_filename);
		DEBUG(1, ("Could not open %s\n", ldif_filename));
		status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	r->add_template = talloc_strdup(mem_ctx, add_template);
	r->mod_template = talloc_strdup(mem_ctx, mod_template);
	if (!r->add_template || !r->mod_template) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	r->add_name = talloc_strdup(mem_ctx, add_template);
	r->module_name = talloc_strdup(mem_ctx, mod_template);
	if (!r->add_name || !r->module_name) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	/* Open the add and mod ldif files */
	if (!(r->add_file = fdopen(mkstemp(r->add_name),"w"))) {
		DEBUG(1, ("Could not open %s\n", r->add_name));
		status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}
	if (!(r->mod_file = fdopen(mkstemp(r->module_name),"w"))) {
		DEBUG(1, ("Could not open %s\n", r->module_name));
		status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	/* Allocate initial memory for groupmap and accountmap arrays */
	r->groupmap = TALLOC_ZERO_ARRAY(mem_ctx, GROUPMAP, 8);
	r->accountmap = TALLOC_ZERO_ARRAY(mem_ctx, ACCOUNTMAP, 8);
	if (r->groupmap == NULL || r->accountmap == NULL) {
		DEBUG(1,("GROUPMAP talloc failed\n"));
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	/* Remember how many we malloced */
	r->num_alloced = 8;

	/* Initial database population */
	if (database_id == SAM_DATABASE_DOMAIN) {

		status = populate_ldap_for_ldif(domain_sid_str,
						r->suffix,
						builtin_sid,
						r->add_file);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}

		status = map_populate_groups(mem_ctx,
					     r->groupmap,
					     r->accountmap,
					     domain_sid_str,
					     r->suffix,
					     builtin_sid);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}
	}

	r->initialized = true;

	*ctx = r;

	return NT_STATUS_OK;
 done:
	TALLOC_FREE(r);
	return status;
}

/****************************************************************
****************************************************************/

static void ldif_free_context(struct samsync_ldif_context *r)
{
	if (!r) {
		return;
	}

	/* Close and delete the ldif files */
	if (r->add_file) {
		fclose(r->add_file);
	}

	if ((r->add_name != NULL) &&
	    strcmp(r->add_name, r->add_template) && (unlink(r->add_name))) {
		DEBUG(1,("unlink(%s) failed, error was (%s)\n",
			 r->add_name, strerror(errno)));
	}

	if (r->mod_file) {
		fclose(r->mod_file);
	}

	if ((r->module_name != NULL) &&
	    strcmp(r->module_name, r->mod_template) && (unlink(r->module_name))) {
		DEBUG(1,("unlink(%s) failed, error was (%s)\n",
			 r->module_name, strerror(errno)));
	}

	if (r->ldif_file && (r->ldif_file != stdout)) {
		fclose(r->ldif_file);
	}

	TALLOC_FREE(r);
}

/****************************************************************
****************************************************************/

static void ldif_write_output(enum netr_SamDatabaseID database_id,
			      struct samsync_ldif_context *l)
{
	/* Write ldif data to the user's file */
	if (database_id == SAM_DATABASE_DOMAIN) {
		fprintf(l->ldif_file,
			"# SAM_DATABASE_DOMAIN: ADD ENTITIES\n");
		fprintf(l->ldif_file,
			"# =================================\n\n");
		fflush(l->ldif_file);
	} else if (database_id == SAM_DATABASE_BUILTIN) {
		fprintf(l->ldif_file,
			"# SAM_DATABASE_BUILTIN: ADD ENTITIES\n");
		fprintf(l->ldif_file,
			"# ==================================\n\n");
		fflush(l->ldif_file);
	}
	fseek(l->add_file, 0, SEEK_SET);
	transfer_file(fileno(l->add_file), fileno(l->ldif_file), (size_t) -1);

	if (database_id == SAM_DATABASE_DOMAIN) {
		fprintf(l->ldif_file,
			"# SAM_DATABASE_DOMAIN: MODIFY ENTITIES\n");
		fprintf(l->ldif_file,
			"# ====================================\n\n");
		fflush(l->ldif_file);
	} else if (database_id == SAM_DATABASE_BUILTIN) {
		fprintf(l->ldif_file,
			"# SAM_DATABASE_BUILTIN: MODIFY ENTITIES\n");
		fprintf(l->ldif_file,
			"# =====================================\n\n");
		fflush(l->ldif_file);
	}
	fseek(l->mod_file, 0, SEEK_SET);
	transfer_file(fileno(l->mod_file), fileno(l->ldif_file), (size_t) -1);
}

/****************************************************************
****************************************************************/

static NTSTATUS fetch_sam_entry_ldif(TALLOC_CTX *mem_ctx,
				     enum netr_SamDatabaseID database_id,
				     struct netr_DELTA_ENUM *r,
				     struct samsync_context *ctx,
				     uint32_t *a_index_p,
				     uint32_t *g_index_p)
{
	union netr_DELTA_UNION u = r->delta_union;
	union netr_DELTA_ID_UNION id = r->delta_id_union;
	struct samsync_ldif_context *l =
		talloc_get_type_abort(ctx->private_data, struct samsync_ldif_context);

	switch (r->delta_type) {
		case NETR_DELTA_DOMAIN:
			break;

		case NETR_DELTA_GROUP:
			fetch_group_info_to_ldif(mem_ctx,
						 u.group,
						 &l->groupmap[*g_index_p],
						 l->add_file,
						 ctx->domain_sid_str,
						 l->suffix);
			(*g_index_p)++;
			break;

		case NETR_DELTA_USER:
			fetch_account_info_to_ldif(mem_ctx,
						   u.user,
						   l->groupmap,
						   &l->accountmap[*a_index_p],
						   l->add_file,
						   ctx->domain_sid_str,
						   l->suffix,
						   l->num_alloced);
			(*a_index_p)++;
			break;

		case NETR_DELTA_ALIAS:
			fetch_alias_info_to_ldif(mem_ctx,
						 u.alias,
						 &l->groupmap[*g_index_p],
						 l->add_file,
						 ctx->domain_sid_str,
						 l->suffix,
						 database_id);
			(*g_index_p)++;
			break;

		case NETR_DELTA_GROUP_MEMBER:
			fetch_groupmem_info_to_ldif(u.group_member,
						    id.rid,
						    l->groupmap,
						    l->accountmap,
						    l->mod_file,
						    l->num_alloced);
			break;

		case NETR_DELTA_ALIAS_MEMBER:
		case NETR_DELTA_POLICY:
		case NETR_DELTA_ACCOUNT:
		case NETR_DELTA_TRUSTED_DOMAIN:
		case NETR_DELTA_SECRET:
		case NETR_DELTA_RENAME_GROUP:
		case NETR_DELTA_RENAME_USER:
		case NETR_DELTA_RENAME_ALIAS:
		case NETR_DELTA_DELETE_GROUP:
		case NETR_DELTA_DELETE_USER:
		case NETR_DELTA_MODIFY_COUNT:
		default:
			break;
	} /* end of switch */

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS ldif_realloc_maps(TALLOC_CTX *mem_ctx,
				  struct samsync_ldif_context *l,
				  uint32_t num_entries)
{
	/* Re-allocate memory for groupmap and accountmap arrays */
	l->groupmap = TALLOC_REALLOC_ARRAY(mem_ctx,
					   l->groupmap,
					   GROUPMAP,
					   num_entries + l->num_alloced);

	l->accountmap = TALLOC_REALLOC_ARRAY(mem_ctx,
					     l->accountmap,
					     ACCOUNTMAP,
					     num_entries + l->num_alloced);

	if (l->groupmap == NULL || l->accountmap == NULL) {
		DEBUG(1,("GROUPMAP talloc failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/* Initialize the new records */
	memset(&(l->groupmap[l->num_alloced]), 0,
	       sizeof(GROUPMAP) * num_entries);
	memset(&(l->accountmap[l->num_alloced]), 0,
	       sizeof(ACCOUNTMAP) * num_entries);

	/* Remember how many we alloced this time */
	l->num_alloced += num_entries;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS init_ldif(TALLOC_CTX *mem_ctx,
			  struct samsync_context *ctx,
			  enum netr_SamDatabaseID database_id,
			  uint64_t *sequence_num)
{
	NTSTATUS status;
	struct samsync_ldif_context *ldif_ctx =
		(struct samsync_ldif_context *)ctx->private_data;

	status = ldif_init_context(mem_ctx,
				   database_id,
				   ctx->output_filename,
				   ctx->domain_sid_str,
				   &ldif_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	ctx->private_data = ldif_ctx;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS fetch_sam_entries_ldif(TALLOC_CTX *mem_ctx,
				       enum netr_SamDatabaseID database_id,
				       struct netr_DELTA_ENUM_ARRAY *r,
				       uint64_t *sequence_num,
				       struct samsync_context *ctx)
{
	NTSTATUS status;
	int i;
	struct samsync_ldif_context *ldif_ctx =
		(struct samsync_ldif_context *)ctx->private_data;

	status = ldif_realloc_maps(mem_ctx, ldif_ctx, r->num_deltas);
	if (!NT_STATUS_IS_OK(status)) {
		goto failed;
	}

	for (i = 0; i < r->num_deltas; i++) {
		status = fetch_sam_entry_ldif(mem_ctx, database_id,
					      &r->delta_enum[i], ctx,
					      &a_index, &g_index);
		if (!NT_STATUS_IS_OK(status)) {
			goto failed;
		}
	}

	return NT_STATUS_OK;

 failed:
	ldif_free_context(ldif_ctx);
	ctx->private_data = NULL;

	return status;
}

/****************************************************************
****************************************************************/

static NTSTATUS close_ldif(TALLOC_CTX *mem_ctx,
			   struct samsync_context *ctx,
			   enum netr_SamDatabaseID database_id,
			   uint64_t sequence_num)
{
	struct samsync_ldif_context *ldif_ctx =
		(struct samsync_ldif_context *)ctx->private_data;

	/* This was the last query */
	ldif_write_output(database_id, ldif_ctx);
	if (ldif_ctx->ldif_file != stdout) {
		ctx->result_message = talloc_asprintf(ctx,
			"Vampired %d accounts and %d groups to %s",
			a_index, g_index, ctx->output_filename);
	}

	ldif_free_context(ldif_ctx);
	ctx->private_data = NULL;

	return NT_STATUS_OK;
}

#else /* HAVE_LDAP */

static NTSTATUS init_ldif(TALLOC_CTX *mem_ctx,
			  struct samsync_context *ctx,
			  enum netr_SamDatabaseID database_id,
			  uint64_t *sequence_num)
{
	return NT_STATUS_NOT_SUPPORTED;
}

static NTSTATUS fetch_sam_entries_ldif(TALLOC_CTX *mem_ctx,
				       enum netr_SamDatabaseID database_id,
				       struct netr_DELTA_ENUM_ARRAY *r,
				       uint64_t *sequence_num,
				       struct samsync_context *ctx)
{
	return NT_STATUS_NOT_SUPPORTED;
}

static NTSTATUS close_ldif(TALLOC_CTX *mem_ctx,
			   struct samsync_context *ctx,
			   enum netr_SamDatabaseID database_id,
			   uint64_t sequence_num)
{
	return NT_STATUS_NOT_SUPPORTED;
}

#endif

const struct samsync_ops libnet_samsync_ldif_ops = {
	.startup		= init_ldif,
	.process_objects	= fetch_sam_entries_ldif,
	.finish			= close_ldif,
};
