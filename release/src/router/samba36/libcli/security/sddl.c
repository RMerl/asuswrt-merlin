/* 
   Unix SMB/CIFS implementation.

   security descriptor description language functions

   Copyright (C) Andrew Tridgell 		2005
      
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
#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "system/locale.h"

struct flag_map {
	const char *name;
	uint32_t flag;
};

/*
  map a series of letter codes into a uint32_t
*/
static bool sddl_map_flags(const struct flag_map *map, const char *str, 
			   uint32_t *flags, size_t *len)
{
	const char *str0 = str;
	if (len) *len = 0;
	*flags = 0;
	while (str[0] && isupper(str[0])) {
		int i;
		for (i=0;map[i].name;i++) {
			size_t l = strlen(map[i].name);
			if (strncmp(map[i].name, str, l) == 0) {
				*flags |= map[i].flag;
				str += l;
				if (len) *len += l;
				break;
			}
		}
		if (map[i].name == NULL) {
			DEBUG(1, ("Unknown flag - %s in %s\n", str, str0));
			return false;
		}
	}
	return true;
}

/*
  a mapping between the 2 letter SID codes and sid strings
*/
static const struct {
	const char *code;
	const char *sid;
	uint32_t rid;
} sid_codes[] = {
	{ "WD", SID_WORLD },

	{ "CO", SID_CREATOR_OWNER },
	{ "CG", SID_CREATOR_GROUP },

	{ "NU", SID_NT_NETWORK },
	{ "IU", SID_NT_INTERACTIVE },
	{ "SU", SID_NT_SERVICE },
	{ "AN", SID_NT_ANONYMOUS },
	{ "ED", SID_NT_ENTERPRISE_DCS },
	{ "PS", SID_NT_SELF },
	{ "AU", SID_NT_AUTHENTICATED_USERS },
	{ "RC", SID_NT_RESTRICTED },
	{ "SY", SID_NT_SYSTEM },
	{ "LS", SID_NT_LOCAL_SERVICE },
	{ "NS", SID_NT_NETWORK_SERVICE },

	{ "BA", SID_BUILTIN_ADMINISTRATORS },
	{ "BU", SID_BUILTIN_USERS },
	{ "BG", SID_BUILTIN_GUESTS },
	{ "PU", SID_BUILTIN_POWER_USERS },
	{ "AO", SID_BUILTIN_ACCOUNT_OPERATORS },
	{ "SO", SID_BUILTIN_SERVER_OPERATORS },
	{ "PO", SID_BUILTIN_PRINT_OPERATORS },
	{ "BO", SID_BUILTIN_BACKUP_OPERATORS },
	{ "RE", SID_BUILTIN_REPLICATOR },
	{ "BR", SID_BUILTIN_RAS_SERVERS },
	{ "RU", SID_BUILTIN_PREW2K },
	{ "RD", SID_BUILTIN_REMOTE_DESKTOP_USERS },
	{ "NO", SID_BUILTIN_NETWORK_CONF_OPERATORS },
	{ "IF", SID_BUILTIN_INCOMING_FOREST_TRUST },

	{ "LA", NULL, DOMAIN_RID_ADMINISTRATOR },
	{ "LG", NULL, DOMAIN_RID_GUEST },
	{ "LK", NULL, DOMAIN_RID_KRBTGT },

	{ "ER", NULL, DOMAIN_RID_ENTERPRISE_READONLY_DCS },
	{ "DA", NULL, DOMAIN_RID_ADMINS },
	{ "DU", NULL, DOMAIN_RID_USERS },
	{ "DG", NULL, DOMAIN_RID_GUESTS },
	{ "DC", NULL, DOMAIN_RID_DOMAIN_MEMBERS },
	{ "DD", NULL, DOMAIN_RID_DCS },
	{ "CA", NULL, DOMAIN_RID_CERT_ADMINS },
	{ "SA", NULL, DOMAIN_RID_SCHEMA_ADMINS },
	{ "EA", NULL, DOMAIN_RID_ENTERPRISE_ADMINS },
	{ "PA", NULL, DOMAIN_RID_POLICY_ADMINS },
	{ "RO", NULL, DOMAIN_RID_READONLY_DCS },
	{ "RS", NULL, DOMAIN_RID_RAS_SERVERS }
};

/*
  decode a SID
  It can either be a special 2 letter code, or in S-* format
*/
static struct dom_sid *sddl_decode_sid(TALLOC_CTX *mem_ctx, const char **sddlp,
				       const struct dom_sid *domain_sid)
{
	const char *sddl = (*sddlp);
	int i;

	/* see if its in the numeric format */
	if (strncmp(sddl, "S-", 2) == 0) {
		struct dom_sid *sid;
		char *sid_str;
		size_t len = strspn(sddl+2, "-0123456789");
		sid_str = talloc_strndup(mem_ctx, sddl, len+2);
		if (!sid_str) {
			return NULL;
		}
		(*sddlp) += len+2;
		sid = dom_sid_parse_talloc(mem_ctx, sid_str);
		talloc_free(sid_str);
		return sid;
	}

	/* now check for one of the special codes */
	for (i=0;i<ARRAY_SIZE(sid_codes);i++) {
		if (strncmp(sid_codes[i].code, sddl, 2) == 0) break;
	}
	if (i == ARRAY_SIZE(sid_codes)) {
		DEBUG(1,("Unknown sddl sid code '%2.2s'\n", sddl));
		return NULL;
	}

	(*sddlp) += 2;

	if (sid_codes[i].sid == NULL) {
		return dom_sid_add_rid(mem_ctx, domain_sid, sid_codes[i].rid);
	}

	return dom_sid_parse_talloc(mem_ctx, sid_codes[i].sid);
}

static const struct flag_map ace_types[] = {
	{ "AU", SEC_ACE_TYPE_SYSTEM_AUDIT },
	{ "AL", SEC_ACE_TYPE_SYSTEM_ALARM },
	{ "OA", SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT },
	{ "OD", SEC_ACE_TYPE_ACCESS_DENIED_OBJECT },
	{ "OU", SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT },
	{ "OL", SEC_ACE_TYPE_SYSTEM_ALARM_OBJECT },
	{ "A",  SEC_ACE_TYPE_ACCESS_ALLOWED },
	{ "D",  SEC_ACE_TYPE_ACCESS_DENIED },
	{ NULL, 0 }
};

static const struct flag_map ace_flags[] = {
	{ "OI", SEC_ACE_FLAG_OBJECT_INHERIT },
	{ "CI", SEC_ACE_FLAG_CONTAINER_INHERIT },
	{ "NP", SEC_ACE_FLAG_NO_PROPAGATE_INHERIT },
	{ "IO", SEC_ACE_FLAG_INHERIT_ONLY },
	{ "ID", SEC_ACE_FLAG_INHERITED_ACE },
	{ "SA", SEC_ACE_FLAG_SUCCESSFUL_ACCESS },
	{ "FA", SEC_ACE_FLAG_FAILED_ACCESS },
	{ NULL, 0 },
};

static const struct flag_map ace_access_mask[] = {
	{ "RP", SEC_ADS_READ_PROP },
	{ "WP", SEC_ADS_WRITE_PROP },
	{ "CR", SEC_ADS_CONTROL_ACCESS },
	{ "CC", SEC_ADS_CREATE_CHILD },
	{ "DC", SEC_ADS_DELETE_CHILD },
	{ "LC", SEC_ADS_LIST },
	{ "LO", SEC_ADS_LIST_OBJECT },
	{ "RC", SEC_STD_READ_CONTROL },
	{ "WO", SEC_STD_WRITE_OWNER },
	{ "WD", SEC_STD_WRITE_DAC },
	{ "SD", SEC_STD_DELETE },
	{ "DT", SEC_ADS_DELETE_TREE },
	{ "SW", SEC_ADS_SELF_WRITE },
	{ "GA", SEC_GENERIC_ALL },
	{ "GR", SEC_GENERIC_READ },
	{ "GW", SEC_GENERIC_WRITE },
	{ "GX", SEC_GENERIC_EXECUTE },
	{ NULL, 0 }
};

/*
  decode an ACE
  return true on success, false on failure
  note that this routine modifies the string
*/
static bool sddl_decode_ace(TALLOC_CTX *mem_ctx, struct security_ace *ace, char *str,
			    const struct dom_sid *domain_sid)
{
	const char *tok[6];
	const char *s;
	int i;
	uint32_t v;
	struct dom_sid *sid;

	ZERO_STRUCTP(ace);

	/* parse out the 6 tokens */
	tok[0] = str;
	for (i=0;i<5;i++) {
		char *ptr = strchr(str, ';');
		if (ptr == NULL) return false;
		*ptr = 0;
		str = ptr+1;
		tok[i+1] = str;
	}

	/* parse ace type */
	if (!sddl_map_flags(ace_types, tok[0], &v, NULL)) {
		return false;
	}
	ace->type = v;

	/* ace flags */
	if (!sddl_map_flags(ace_flags, tok[1], &v, NULL)) {
		return false;
	}
	ace->flags = v;
	
	/* access mask */
	if (strncmp(tok[2], "0x", 2) == 0) {
		ace->access_mask = strtol(tok[2], NULL, 16);
	} else {
		if (!sddl_map_flags(ace_access_mask, tok[2], &v, NULL)) {
			return false;
		}
		ace->access_mask = v;
	}

	/* object */
	if (tok[3][0] != 0) {
		NTSTATUS status = GUID_from_string(tok[3], 
						   &ace->object.object.type.type);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
		ace->object.object.flags |= SEC_ACE_OBJECT_TYPE_PRESENT;
	}

	/* inherit object */
	if (tok[4][0] != 0) {
		NTSTATUS status = GUID_from_string(tok[4], 
						   &ace->object.object.inherited_type.inherited_type);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
		ace->object.object.flags |= SEC_ACE_INHERITED_OBJECT_TYPE_PRESENT;
	}

	/* trustee */
	s = tok[5];
	sid = sddl_decode_sid(mem_ctx, &s, domain_sid);
	if (sid == NULL) {
		return false;
	}
	ace->trustee = *sid;
	talloc_free(sid);

	return true;
}

static const struct flag_map acl_flags[] = {
	{ "P", SEC_DESC_DACL_PROTECTED },
	{ "AR", SEC_DESC_DACL_AUTO_INHERIT_REQ },
	{ "AI", SEC_DESC_DACL_AUTO_INHERITED },
	{ NULL, 0 }
};

/*
  decode an ACL
*/
static struct security_acl *sddl_decode_acl(struct security_descriptor *sd, 
					    const char **sddlp, uint32_t *flags,
					    const struct dom_sid *domain_sid)
{
	const char *sddl = *sddlp;
	struct security_acl *acl;
	size_t len;

	*flags = 0;

	acl = talloc_zero(sd, struct security_acl);
	if (acl == NULL) return NULL;
	acl->revision = SECURITY_ACL_REVISION_ADS;

	if (isupper(sddl[0]) && sddl[1] == ':') {
		/* its an empty ACL */
		return acl;
	}

	/* work out the ACL flags */
	if (!sddl_map_flags(acl_flags, sddl, flags, &len)) {
		talloc_free(acl);
		return NULL;
	}
	sddl += len;

	/* now the ACEs */
	while (*sddl == '(') {
		char *astr;
		len = strcspn(sddl+1, ")");
		astr = talloc_strndup(acl, sddl+1, len);
		if (astr == NULL || sddl[len+1] != ')') {
			talloc_free(acl);
			return NULL;
		}
		acl->aces = talloc_realloc(acl, acl->aces, struct security_ace, 
					   acl->num_aces+1);
		if (acl->aces == NULL) {
			talloc_free(acl);
			return NULL;
		}
		if (!sddl_decode_ace(acl->aces, &acl->aces[acl->num_aces], 
				     astr, domain_sid)) {
			talloc_free(acl);
			return NULL;
		}
		switch (acl->aces[acl->num_aces].type) {
		case SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT:
		case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT:
		case SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT:
		case SEC_ACE_TYPE_SYSTEM_ALARM_OBJECT:
			acl->revision = SECURITY_ACL_REVISION_ADS;
			break;
		default:
			break;
		}
		talloc_free(astr);
		sddl += len+2;
		acl->num_aces++;
	}

	(*sddlp) = sddl;
	return acl;
}

/*
  decode a security descriptor in SDDL format
*/
struct security_descriptor *sddl_decode(TALLOC_CTX *mem_ctx, const char *sddl,
					const struct dom_sid *domain_sid)
{
	struct security_descriptor *sd;
	sd = talloc_zero(mem_ctx, struct security_descriptor);

	sd->revision = SECURITY_DESCRIPTOR_REVISION_1;
	sd->type     = SEC_DESC_SELF_RELATIVE;
	
	while (*sddl) {
		uint32_t flags;
		char c = sddl[0];
		if (sddl[1] != ':') goto failed;

		sddl += 2;
		switch (c) {
		case 'D':
			if (sd->dacl != NULL) goto failed;
			sd->dacl = sddl_decode_acl(sd, &sddl, &flags, domain_sid);
			if (sd->dacl == NULL) goto failed;
			sd->type |= flags | SEC_DESC_DACL_PRESENT;
			break;
		case 'S':
			if (sd->sacl != NULL) goto failed;
			sd->sacl = sddl_decode_acl(sd, &sddl, &flags, domain_sid);
			if (sd->sacl == NULL) goto failed;
			/* this relies on the SEC_DESC_SACL_* flags being
			   1 bit shifted from the SEC_DESC_DACL_* flags */
			sd->type |= (flags<<1) | SEC_DESC_SACL_PRESENT;
			break;
		case 'O':
			if (sd->owner_sid != NULL) goto failed;
			sd->owner_sid = sddl_decode_sid(sd, &sddl, domain_sid);
			if (sd->owner_sid == NULL) goto failed;
			break;
		case 'G':
			if (sd->group_sid != NULL) goto failed;
			sd->group_sid = sddl_decode_sid(sd, &sddl, domain_sid);
			if (sd->group_sid == NULL) goto failed;
			break;
		}
	}

	return sd;

failed:
	DEBUG(2,("Badly formatted SDDL '%s'\n", sddl));
	talloc_free(sd);
	return NULL;
}

/*
  turn a set of flags into a string
*/
static char *sddl_flags_to_string(TALLOC_CTX *mem_ctx, const struct flag_map *map,
				  uint32_t flags, bool check_all)
{
	int i;
	char *s;

	/* try to find an exact match */
	for (i=0;map[i].name;i++) {
		if (map[i].flag == flags) {
			return talloc_strdup(mem_ctx, map[i].name);
		}
	}

	s = talloc_strdup(mem_ctx, "");

	/* now by bits */
	for (i=0;map[i].name;i++) {
		if ((flags & map[i].flag) != 0) {
			s = talloc_asprintf_append_buffer(s, "%s", map[i].name);
			if (s == NULL) goto failed;
			flags &= ~map[i].flag;
		}
	}

	if (check_all && flags != 0) {
		goto failed;
	}

	return s;

failed:
	talloc_free(s);
	return NULL;
}

/*
  encode a sid in SDDL format
*/
static char *sddl_encode_sid(TALLOC_CTX *mem_ctx, const struct dom_sid *sid,
			     const struct dom_sid *domain_sid)
{
	int i;
	char *sidstr;

	sidstr = dom_sid_string(mem_ctx, sid);
	if (sidstr == NULL) return NULL;

	/* seen if its a well known sid */ 
	for (i=0;sid_codes[i].sid;i++) {
		if (strcmp(sidstr, sid_codes[i].sid) == 0) {
			talloc_free(sidstr);
			return talloc_strdup(mem_ctx, sid_codes[i].code);
		}
	}

	/* or a well known rid in our domain */
	if (dom_sid_in_domain(domain_sid, sid)) {
		uint32_t rid = sid->sub_auths[sid->num_auths-1];
		for (;i<ARRAY_SIZE(sid_codes);i++) {
			if (rid == sid_codes[i].rid) {
				talloc_free(sidstr);
				return talloc_strdup(mem_ctx, sid_codes[i].code);
			}
		}
	}
	
	talloc_free(sidstr);

	/* TODO: encode well known sids as two letter codes */
	return dom_sid_string(mem_ctx, sid);
}


/*
  encode an ACE in SDDL format
*/
static char *sddl_encode_ace(TALLOC_CTX *mem_ctx, const struct security_ace *ace,
			     const struct dom_sid *domain_sid)
{
	char *sddl = NULL;
	TALLOC_CTX *tmp_ctx;
	const char *sddl_type="", *sddl_flags="", *sddl_mask="",
		*sddl_object="", *sddl_iobject="", *sddl_trustee="";

	tmp_ctx = talloc_new(mem_ctx);
	if (tmp_ctx == NULL) {
		DEBUG(0, ("talloc_new failed\n"));
		return NULL;
	}

	sddl_type = sddl_flags_to_string(tmp_ctx, ace_types, ace->type, true);
	if (sddl_type == NULL) {
		goto failed;
	}

	sddl_flags = sddl_flags_to_string(tmp_ctx, ace_flags, ace->flags,
					  true);
	if (sddl_flags == NULL) {
		goto failed;
	}

	sddl_mask = sddl_flags_to_string(tmp_ctx, ace_access_mask,
					 ace->access_mask, true);
	if (sddl_mask == NULL) {
		sddl_mask = talloc_asprintf(tmp_ctx, "0x%08x",
					     ace->access_mask);
		if (sddl_mask == NULL) {
			goto failed;
		}
	}

	if (ace->type == SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT ||
	    ace->type == SEC_ACE_TYPE_ACCESS_DENIED_OBJECT ||
	    ace->type == SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT ||
	    ace->type == SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT) {
		if (ace->object.object.flags & SEC_ACE_OBJECT_TYPE_PRESENT) {
			sddl_object = GUID_string(
				tmp_ctx, &ace->object.object.type.type);
			if (sddl_object == NULL) {
				goto failed;
			}
		}

		if (ace->object.object.flags & SEC_ACE_INHERITED_OBJECT_TYPE_PRESENT) {
			sddl_iobject = GUID_string(tmp_ctx, &ace->object.object.inherited_type.inherited_type);
			if (sddl_iobject == NULL) {
				goto failed;
			}
		}
	}

	sddl_trustee = sddl_encode_sid(tmp_ctx, &ace->trustee, domain_sid);
	if (sddl_trustee == NULL) {
		goto failed;
	}

	sddl = talloc_asprintf(mem_ctx, "%s;%s;%s;%s;%s;%s",
			       sddl_type, sddl_flags, sddl_mask, sddl_object,
			       sddl_iobject, sddl_trustee);

failed:
	talloc_free(tmp_ctx);
	return sddl;
}

/*
  encode an ACL in SDDL format
*/
static char *sddl_encode_acl(TALLOC_CTX *mem_ctx, const struct security_acl *acl,
			     uint32_t flags, const struct dom_sid *domain_sid)
{
	char *sddl;
	uint32_t i;

	/* add any ACL flags */
	sddl = sddl_flags_to_string(mem_ctx, acl_flags, flags, false);
	if (sddl == NULL) goto failed;

	/* now the ACEs, encoded in braces */
	for (i=0;i<acl->num_aces;i++) {
		char *ace = sddl_encode_ace(sddl, &acl->aces[i], domain_sid);
		if (ace == NULL) goto failed;
		sddl = talloc_asprintf_append_buffer(sddl, "(%s)", ace);
		if (sddl == NULL) goto failed;
		talloc_free(ace);
	}

	return sddl;

failed:
	talloc_free(sddl);
	return NULL;
}


/*
  encode a security descriptor to SDDL format
*/
char *sddl_encode(TALLOC_CTX *mem_ctx, const struct security_descriptor *sd,
		  const struct dom_sid *domain_sid)
{
	char *sddl;
	TALLOC_CTX *tmp_ctx;

	/* start with a blank string */
	sddl = talloc_strdup(mem_ctx, "");
	if (sddl == NULL) goto failed;

	tmp_ctx = talloc_new(mem_ctx);

	if (sd->owner_sid != NULL) {
		char *sid = sddl_encode_sid(tmp_ctx, sd->owner_sid, domain_sid);
		if (sid == NULL) goto failed;
		sddl = talloc_asprintf_append_buffer(sddl, "O:%s", sid);
		if (sddl == NULL) goto failed;
	}

	if (sd->group_sid != NULL) {
		char *sid = sddl_encode_sid(tmp_ctx, sd->group_sid, domain_sid);
		if (sid == NULL) goto failed;
		sddl = talloc_asprintf_append_buffer(sddl, "G:%s", sid);
		if (sddl == NULL) goto failed;
	}

	if ((sd->type & SEC_DESC_DACL_PRESENT) && sd->dacl != NULL) {
		char *acl = sddl_encode_acl(tmp_ctx, sd->dacl, sd->type, domain_sid);
		if (acl == NULL) goto failed;
		sddl = talloc_asprintf_append_buffer(sddl, "D:%s", acl);
		if (sddl == NULL) goto failed;
	}

	if ((sd->type & SEC_DESC_SACL_PRESENT) && sd->sacl != NULL) {
		char *acl = sddl_encode_acl(tmp_ctx, sd->sacl, sd->type>>1, domain_sid);
		if (acl == NULL) goto failed;
		sddl = talloc_asprintf_append_buffer(sddl, "S:%s", acl);
		if (sddl == NULL) goto failed;
	}

	talloc_free(tmp_ctx);
	return sddl;

failed:
	talloc_free(sddl);
	return NULL;
}


