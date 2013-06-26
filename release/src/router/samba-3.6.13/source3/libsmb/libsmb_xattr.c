/* 
   Unix SMB/Netbios implementation.
   SMB client library implementation
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Richard Sharpe 2000, 2002
   Copyright (C) John Terpstra 2000
   Copyright (C) Tom Jansen (Ninja ISD) 2002 
   Copyright (C) Derrell Lipman 2003-2008
   Copyright (C) Jeremy Allison 2007, 2008

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
#include "libsmb/libsmb.h"
#include "libsmbclient.h"
#include "libsmb_internal.h"
#include "../librpc/gen_ndr/ndr_lsa.h"
#include "rpc_client/rpc_client.h"
#include "rpc_client/cli_lsarpc.h"
#include "../libcli/security/security.h"

/*
 * Find an lsa pipe handle associated with a cli struct.
 */
static struct rpc_pipe_client *
find_lsa_pipe_hnd(struct cli_state *ipc_cli)
{
	struct rpc_pipe_client *pipe_hnd;

	for (pipe_hnd = ipc_cli->pipe_list;
             pipe_hnd;
             pipe_hnd = pipe_hnd->next) {
		if (ndr_syntax_id_equal(&pipe_hnd->abstract_syntax,
					&ndr_table_lsarpc.syntax_id)) {
			return pipe_hnd;
		}
	}
	return NULL;
}

/*
 * Sort ACEs according to the documentation at
 * http://support.microsoft.com/kb/269175, at least as far as it defines the
 * order.
 */

static int
ace_compare(struct security_ace *ace1,
            struct security_ace *ace2)
{
        bool b1;
        bool b2;

        /* If the ACEs are equal, we have nothing more to do. */
        if (sec_ace_equal(ace1, ace2)) {
		return 0;
        }

        /* Inherited follow non-inherited */
        b1 = ((ace1->flags & SEC_ACE_FLAG_INHERITED_ACE) != 0);
        b2 = ((ace2->flags & SEC_ACE_FLAG_INHERITED_ACE) != 0);
        if (b1 != b2) {
                return (b1 ? 1 : -1);
        }

        /*
         * What shall we do with AUDITs and ALARMs?  It's undefined.  We'll
         * sort them after DENY and ALLOW.
         */
        b1 = (ace1->type != SEC_ACE_TYPE_ACCESS_ALLOWED &&
              ace1->type != SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT &&
              ace1->type != SEC_ACE_TYPE_ACCESS_DENIED &&
              ace1->type != SEC_ACE_TYPE_ACCESS_DENIED_OBJECT);
        b2 = (ace2->type != SEC_ACE_TYPE_ACCESS_ALLOWED &&
              ace2->type != SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT &&
              ace2->type != SEC_ACE_TYPE_ACCESS_DENIED &&
              ace2->type != SEC_ACE_TYPE_ACCESS_DENIED_OBJECT);
        if (b1 != b2) {
                return (b1 ? 1 : -1);
        }

        /* Allowed ACEs follow denied ACEs */
        b1 = (ace1->type == SEC_ACE_TYPE_ACCESS_ALLOWED ||
              ace1->type == SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT);
        b2 = (ace2->type == SEC_ACE_TYPE_ACCESS_ALLOWED ||
              ace2->type == SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT);
        if (b1 != b2) {
                return (b1 ? 1 : -1);
        }

        /*
         * ACEs applying to an entity's object follow those applying to the
         * entity itself
         */
        b1 = (ace1->type == SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT ||
              ace1->type == SEC_ACE_TYPE_ACCESS_DENIED_OBJECT);
        b2 = (ace2->type == SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT ||
              ace2->type == SEC_ACE_TYPE_ACCESS_DENIED_OBJECT);
        if (b1 != b2) {
                return (b1 ? 1 : -1);
        }

        /*
         * If we get this far, the ACEs are similar as far as the
         * characteristics we typically care about (those defined by the
         * referenced MS document).  We'll now sort by characteristics that
         * just seems reasonable.
         */

	if (ace1->type != ace2->type) {
		return ace2->type - ace1->type;
        }

	if (dom_sid_compare(&ace1->trustee, &ace2->trustee)) {
		return dom_sid_compare(&ace1->trustee, &ace2->trustee);
        }

	if (ace1->flags != ace2->flags) {
		return ace1->flags - ace2->flags;
        }

	if (ace1->access_mask != ace2->access_mask) {
		return ace1->access_mask - ace2->access_mask;
        }

	if (ace1->size != ace2->size) {
		return ace1->size - ace2->size;
        }

	return memcmp(ace1, ace2, sizeof(struct security_ace));
}


static void
sort_acl(struct security_acl *the_acl)
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

/* convert a SID to a string, either numeric or username/group */
static void
convert_sid_to_string(struct cli_state *ipc_cli,
                      struct policy_handle *pol,
                      fstring str,
                      bool numeric,
                      struct dom_sid *sid)
{
	char **domains = NULL;
	char **names = NULL;
	enum lsa_SidType *types = NULL;
	struct rpc_pipe_client *pipe_hnd = find_lsa_pipe_hnd(ipc_cli);
	TALLOC_CTX *ctx;

	sid_to_fstring(str, sid);

	if (numeric) {
		return;     /* no lookup desired */
	}

	if (!pipe_hnd) {
		return;
	}

	/* Ask LSA to convert the sid to a name */

	ctx = talloc_stackframe();

	if (!NT_STATUS_IS_OK(rpccli_lsa_lookup_sids(pipe_hnd, ctx,
                                                    pol, 1, sid, &domains,
                                                    &names, &types)) ||
	    !domains || !domains[0] || !names || !names[0]) {
		TALLOC_FREE(ctx);
		return;
	}

	/* Converted OK */

	slprintf(str, sizeof(fstring) - 1, "%s%s%s",
		 domains[0], lp_winbind_separator(),
		 names[0]);

	TALLOC_FREE(ctx);
}

/* convert a string to a SID, either numeric or username/group */
static bool
convert_string_to_sid(struct cli_state *ipc_cli,
                      struct policy_handle *pol,
                      bool numeric,
                      struct dom_sid *sid,
                      const char *str)
{
	enum lsa_SidType *types = NULL;
	struct dom_sid *sids = NULL;
	bool result = True;
	TALLOC_CTX *ctx = NULL;
	struct rpc_pipe_client *pipe_hnd = find_lsa_pipe_hnd(ipc_cli);

	if (!pipe_hnd) {
		return False;
	}

        if (numeric) {
                if (strncmp(str, "S-", 2) == 0) {
                        return string_to_sid(sid, str);
                }

                result = False;
                goto done;
        }

	ctx = talloc_stackframe();
	if (!NT_STATUS_IS_OK(rpccli_lsa_lookup_names(pipe_hnd, ctx,
                                                     pol, 1, &str,
                                                     NULL, 1, &sids,
                                                     &types))) {
		result = False;
		goto done;
	}

	sid_copy(sid, &sids[0]);
done:
	TALLOC_FREE(ctx);
	return result;
}


/* parse an struct security_ace in the same format as print_ace() */
static bool
parse_ace(struct cli_state *ipc_cli,
          struct policy_handle *pol,
          struct security_ace *ace,
          bool numeric,
          char *str)
{
	char *p;
	const char *cp;
	char *tok;
	unsigned int atype;
        unsigned int aflags;
        unsigned int amask;
	struct dom_sid sid;
	uint32_t mask;
	const struct perm_value *v;
        struct perm_value {
                const char perm[7];
                uint32 mask;
        };
	TALLOC_CTX *frame = talloc_stackframe();

        /* These values discovered by inspection */
        static const struct perm_value special_values[] = {
                { "R", 0x00120089 },
                { "W", 0x00120116 },
                { "X", 0x001200a0 },
                { "D", 0x00010000 },
                { "P", 0x00040000 },
                { "O", 0x00080000 },
                { "", 0 },
        };

        static const struct perm_value standard_values[] = {
                { "READ",   0x001200a9 },
                { "CHANGE", 0x001301bf },
                { "FULL",   0x001f01ff },
                { "", 0 },
        };

	ZERO_STRUCTP(ace);
	p = strchr_m(str,':');
	if (!p) {
		TALLOC_FREE(frame);
		return False;
	}
	*p = '\0';
	p++;
	/* Try to parse numeric form */

	if (sscanf(p, "%i/%i/%i", &atype, &aflags, &amask) == 3 &&
	    convert_string_to_sid(ipc_cli, pol, numeric, &sid, str)) {
		goto done;
	}

	/* Try to parse text form */

	if (!convert_string_to_sid(ipc_cli, pol, numeric, &sid, str)) {
		TALLOC_FREE(frame);
		return false;
	}

	cp = p;
	if (!next_token_talloc(frame, &cp, &tok, "/")) {
		TALLOC_FREE(frame);
		return false;
	}

	if (StrnCaseCmp(tok, "ALLOWED", strlen("ALLOWED")) == 0) {
		atype = SEC_ACE_TYPE_ACCESS_ALLOWED;
	} else if (StrnCaseCmp(tok, "DENIED", strlen("DENIED")) == 0) {
		atype = SEC_ACE_TYPE_ACCESS_DENIED;
	} else {
		TALLOC_FREE(frame);
		return false;
	}

	/* Only numeric form accepted for flags at present */

	if (!(next_token_talloc(frame, &cp, &tok, "/") &&
	      sscanf(tok, "%i", &aflags))) {
		TALLOC_FREE(frame);
		return false;
	}

	if (!next_token_talloc(frame, &cp, &tok, "/")) {
		TALLOC_FREE(frame);
		return false;
	}

	if (strncmp(tok, "0x", 2) == 0) {
		if (sscanf(tok, "%i", &amask) != 1) {
			TALLOC_FREE(frame);
			return false;
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
			TALLOC_FREE(frame);
		 	return false;
		}
		p++;
	}

	if (*p) {
		TALLOC_FREE(frame);
		return false;
	}

done:
	mask = amask;
	init_sec_ace(ace, &sid, atype, mask, aflags);
	TALLOC_FREE(frame);
	return true;
}

/* add an struct security_ace to a list of struct security_aces in a struct security_acl */
static bool
add_ace(struct security_acl **the_acl,
        struct security_ace *ace,
        TALLOC_CTX *ctx)
{
	struct security_acl *newacl;
	struct security_ace *aces;

	if (! *the_acl) {
		(*the_acl) = make_sec_acl(ctx, 3, 1, ace);
		return True;
	}

	if ((aces = SMB_CALLOC_ARRAY(struct security_ace,
                                     1+(*the_acl)->num_aces)) == NULL) {
		return False;
	}
	memcpy(aces, (*the_acl)->aces, (*the_acl)->num_aces * sizeof(struct security_ace));
	memcpy(aces+(*the_acl)->num_aces, ace, sizeof(struct security_ace));
	newacl = make_sec_acl(ctx, (*the_acl)->revision,
                              1+(*the_acl)->num_aces, aces);
	SAFE_FREE(aces);
	(*the_acl) = newacl;
	return True;
}


/* parse a ascii version of a security descriptor */
static struct security_descriptor *
sec_desc_parse(TALLOC_CTX *ctx,
               struct cli_state *ipc_cli,
               struct policy_handle *pol,
               bool numeric,
               const char *str)
{
	const char *p = str;
	char *tok;
	struct security_descriptor *ret = NULL;
	size_t sd_size;
	struct dom_sid *group_sid=NULL;
        struct dom_sid *owner_sid=NULL;
	struct security_acl *dacl=NULL;
	int revision=1;

	while (next_token_talloc(ctx, &p, &tok, "\t,\r\n")) {

		if (StrnCaseCmp(tok,"REVISION:", 9) == 0) {
			revision = strtol(tok+9, NULL, 16);
			continue;
		}

		if (StrnCaseCmp(tok,"OWNER:", 6) == 0) {
			if (owner_sid) {
				DEBUG(5,("OWNER specified more than once!\n"));
				goto done;
			}
			owner_sid = SMB_CALLOC_ARRAY(struct dom_sid, 1);
			if (!owner_sid ||
			    !convert_string_to_sid(ipc_cli, pol,
                                                   numeric,
                                                   owner_sid, tok+6)) {
				DEBUG(5, ("Failed to parse owner sid\n"));
				goto done;
			}
			continue;
		}

		if (StrnCaseCmp(tok,"OWNER+:", 7) == 0) {
			if (owner_sid) {
				DEBUG(5,("OWNER specified more than once!\n"));
				goto done;
			}
			owner_sid = SMB_CALLOC_ARRAY(struct dom_sid, 1);
			if (!owner_sid ||
			    !convert_string_to_sid(ipc_cli, pol,
                                                   False,
                                                   owner_sid, tok+7)) {
				DEBUG(5, ("Failed to parse owner sid\n"));
				goto done;
			}
			continue;
		}

		if (StrnCaseCmp(tok,"GROUP:", 6) == 0) {
			if (group_sid) {
				DEBUG(5,("GROUP specified more than once!\n"));
				goto done;
			}
			group_sid = SMB_CALLOC_ARRAY(struct dom_sid, 1);
			if (!group_sid ||
			    !convert_string_to_sid(ipc_cli, pol,
                                                   numeric,
                                                   group_sid, tok+6)) {
				DEBUG(5, ("Failed to parse group sid\n"));
				goto done;
			}
			continue;
		}

		if (StrnCaseCmp(tok,"GROUP+:", 7) == 0) {
			if (group_sid) {
				DEBUG(5,("GROUP specified more than once!\n"));
				goto done;
			}
			group_sid = SMB_CALLOC_ARRAY(struct dom_sid, 1);
			if (!group_sid ||
			    !convert_string_to_sid(ipc_cli, pol,
                                                   False,
                                                   group_sid, tok+6)) {
				DEBUG(5, ("Failed to parse group sid\n"));
				goto done;
			}
			continue;
		}

		if (StrnCaseCmp(tok,"ACL:", 4) == 0) {
			struct security_ace ace;
			if (!parse_ace(ipc_cli, pol, &ace, numeric, tok+4)) {
				DEBUG(5, ("Failed to parse ACL %s\n", tok));
				goto done;
			}
			if(!add_ace(&dacl, &ace, ctx)) {
				DEBUG(5, ("Failed to add ACL %s\n", tok));
				goto done;
			}
			continue;
		}

		if (StrnCaseCmp(tok,"ACL+:", 5) == 0) {
			struct security_ace ace;
			if (!parse_ace(ipc_cli, pol, &ace, False, tok+5)) {
				DEBUG(5, ("Failed to parse ACL %s\n", tok));
				goto done;
			}
			if(!add_ace(&dacl, &ace, ctx)) {
				DEBUG(5, ("Failed to add ACL %s\n", tok));
				goto done;
			}
			continue;
		}

		DEBUG(5, ("Failed to parse security descriptor\n"));
		goto done;
	}

	ret = make_sec_desc(ctx, revision, SEC_DESC_SELF_RELATIVE, 
			    owner_sid, group_sid, NULL, dacl, &sd_size);

done:
	SAFE_FREE(group_sid);
	SAFE_FREE(owner_sid);
	return ret;
}


/* Obtain the current dos attributes */
static DOS_ATTR_DESC *
dos_attr_query(SMBCCTX *context,
               TALLOC_CTX *ctx,
               const char *filename,
               SMBCSRV *srv)
{
        struct timespec create_time_ts;
        struct timespec write_time_ts;
        struct timespec access_time_ts;
        struct timespec change_time_ts;
        SMB_OFF_T size = 0;
        uint16 mode = 0;
	SMB_INO_T inode = 0;
        DOS_ATTR_DESC *ret;

        ret = TALLOC_P(ctx, DOS_ATTR_DESC);
        if (!ret) {
                errno = ENOMEM;
                return NULL;
        }

        /* Obtain the DOS attributes */
        if (!SMBC_getatr(context, srv, CONST_DISCARD(char *, filename),
                         &mode, &size,
                         &create_time_ts,
                         &access_time_ts,
                         &write_time_ts,
                         &change_time_ts,
                         &inode)) {
                errno = SMBC_errno(context, srv->cli);
                DEBUG(5, ("dos_attr_query Failed to query old attributes\n"));
                return NULL;
        }

        ret->mode = mode;
        ret->size = size;
        ret->create_time = convert_timespec_to_time_t(create_time_ts);
        ret->access_time = convert_timespec_to_time_t(access_time_ts);
        ret->write_time = convert_timespec_to_time_t(write_time_ts);
        ret->change_time = convert_timespec_to_time_t(change_time_ts);
        ret->inode = inode;

        return ret;
}


/* parse a ascii version of a security descriptor */
static void
dos_attr_parse(SMBCCTX *context,
               DOS_ATTR_DESC *dad,
               SMBCSRV *srv,
               char *str)
{
        int n;
        const char *p = str;
	char *tok = NULL;
	TALLOC_CTX *frame = NULL;
        struct {
                const char * create_time_attr;
                const char * access_time_attr;
                const char * write_time_attr;
                const char * change_time_attr;
        } attr_strings;

        /* Determine whether to use old-style or new-style attribute names */
        if (context->internal->full_time_names) {
                /* new-style names */
                attr_strings.create_time_attr = "CREATE_TIME";
                attr_strings.access_time_attr = "ACCESS_TIME";
                attr_strings.write_time_attr = "WRITE_TIME";
                attr_strings.change_time_attr = "CHANGE_TIME";
        } else {
                /* old-style names */
                attr_strings.create_time_attr = NULL;
                attr_strings.access_time_attr = "A_TIME";
                attr_strings.write_time_attr = "M_TIME";
                attr_strings.change_time_attr = "C_TIME";
        }

        /* if this is to set the entire ACL... */
        if (*str == '*') {
                /* ... then increment past the first colon if there is one */
                if ((p = strchr(str, ':')) != NULL) {
                        ++p;
                } else {
                        p = str;
                }
        }

	frame = talloc_stackframe();
	while (next_token_talloc(frame, &p, &tok, "\t,\r\n")) {
		if (StrnCaseCmp(tok, "MODE:", 5) == 0) {
                        long request = strtol(tok+5, NULL, 16);
                        if (request == 0) {
                                dad->mode = (request |
                                             (IS_DOS_DIR(dad->mode)
                                              ? FILE_ATTRIBUTE_DIRECTORY
                                              : FILE_ATTRIBUTE_NORMAL));
                        } else {
                                dad->mode = request;
                        }
			continue;
		}

		if (StrnCaseCmp(tok, "SIZE:", 5) == 0) {
                        dad->size = (SMB_OFF_T)atof(tok+5);
			continue;
		}

                n = strlen(attr_strings.access_time_attr);
                if (StrnCaseCmp(tok, attr_strings.access_time_attr, n) == 0) {
                        dad->access_time = (time_t)strtol(tok+n+1, NULL, 10);
			continue;
		}

                n = strlen(attr_strings.change_time_attr);
                if (StrnCaseCmp(tok, attr_strings.change_time_attr, n) == 0) {
                        dad->change_time = (time_t)strtol(tok+n+1, NULL, 10);
			continue;
		}

                n = strlen(attr_strings.write_time_attr);
                if (StrnCaseCmp(tok, attr_strings.write_time_attr, n) == 0) {
                        dad->write_time = (time_t)strtol(tok+n+1, NULL, 10);
			continue;
		}

		if (attr_strings.create_time_attr != NULL) {
			n = strlen(attr_strings.create_time_attr);
			if (StrnCaseCmp(tok, attr_strings.create_time_attr,
					n) == 0) {
				dad->create_time = (time_t)strtol(tok+n+1,
								  NULL, 10);
				continue;
			}
		}

		if (StrnCaseCmp(tok, "INODE:", 6) == 0) {
                        dad->inode = (SMB_INO_T)atof(tok+6);
			continue;
		}
	}
	TALLOC_FREE(frame);
}

/*****************************************************
 Retrieve the acls for a file.
*******************************************************/

static int
cacl_get(SMBCCTX *context,
         TALLOC_CTX *ctx,
         SMBCSRV *srv,
         struct cli_state *ipc_cli,
         struct policy_handle *pol,
         char *filename,
         char *attr_name,
         char *buf,
         int bufsize)
{
	uint32 i;
        int n = 0;
        int n_used;
        bool all;
        bool all_nt;
        bool all_nt_acls;
        bool all_dos;
        bool some_nt;
        bool some_dos;
        bool exclude_nt_revision = False;
        bool exclude_nt_owner = False;
        bool exclude_nt_group = False;
        bool exclude_nt_acl = False;
        bool exclude_dos_mode = False;
        bool exclude_dos_size = False;
        bool exclude_dos_create_time = False;
        bool exclude_dos_access_time = False;
        bool exclude_dos_write_time = False;
        bool exclude_dos_change_time = False;
        bool exclude_dos_inode = False;
        bool numeric = True;
        bool determine_size = (bufsize == 0);
	uint16_t fnum;
	struct security_descriptor *sd;
	fstring sidstr;
        fstring name_sandbox;
        char *name;
        char *pExclude;
        char *p;
        struct timespec create_time_ts;
	struct timespec write_time_ts;
        struct timespec access_time_ts;
        struct timespec change_time_ts;
	time_t create_time = (time_t)0;
	time_t write_time = (time_t)0;
        time_t access_time = (time_t)0;
        time_t change_time = (time_t)0;
	SMB_OFF_T size = 0;
	uint16 mode = 0;
	SMB_INO_T ino = 0;
	struct cli_state *cli = srv->cli;
        struct {
                const char * create_time_attr;
                const char * access_time_attr;
                const char * write_time_attr;
                const char * change_time_attr;
        } attr_strings;
        struct {
                const char * create_time_attr;
                const char * access_time_attr;
                const char * write_time_attr;
                const char * change_time_attr;
        } excl_attr_strings;

        /* Determine whether to use old-style or new-style attribute names */
        if (context->internal->full_time_names) {
                /* new-style names */
                attr_strings.create_time_attr = "CREATE_TIME";
                attr_strings.access_time_attr = "ACCESS_TIME";
                attr_strings.write_time_attr = "WRITE_TIME";
                attr_strings.change_time_attr = "CHANGE_TIME";

                excl_attr_strings.create_time_attr = "CREATE_TIME";
                excl_attr_strings.access_time_attr = "ACCESS_TIME";
                excl_attr_strings.write_time_attr = "WRITE_TIME";
                excl_attr_strings.change_time_attr = "CHANGE_TIME";
        } else {
                /* old-style names */
                attr_strings.create_time_attr = NULL;
                attr_strings.access_time_attr = "A_TIME";
                attr_strings.write_time_attr = "M_TIME";
                attr_strings.change_time_attr = "C_TIME";

                excl_attr_strings.create_time_attr = NULL;
                excl_attr_strings.access_time_attr = "dos_attr.A_TIME";
                excl_attr_strings.write_time_attr = "dos_attr.M_TIME";
                excl_attr_strings.change_time_attr = "dos_attr.C_TIME";
        }

        /* Copy name so we can strip off exclusions (if any are specified) */
        strncpy(name_sandbox, attr_name, sizeof(name_sandbox) - 1);

        /* Ensure name is null terminated */
        name_sandbox[sizeof(name_sandbox) - 1] = '\0';

        /* Play in the sandbox */
        name = name_sandbox;

        /* If there are any exclusions, point to them and mask them from name */
        if ((pExclude = strchr(name, '!')) != NULL)
        {
                *pExclude++ = '\0';
        }

        all = (StrnCaseCmp(name, "system.*", 8) == 0);
        all_nt = (StrnCaseCmp(name, "system.nt_sec_desc.*", 20) == 0);
        all_nt_acls = (StrnCaseCmp(name, "system.nt_sec_desc.acl.*", 24) == 0);
        all_dos = (StrnCaseCmp(name, "system.dos_attr.*", 17) == 0);
        some_nt = (StrnCaseCmp(name, "system.nt_sec_desc.", 19) == 0);
        some_dos = (StrnCaseCmp(name, "system.dos_attr.", 16) == 0);
        numeric = (* (name + strlen(name) - 1) != '+');

        /* Look for exclusions from "all" requests */
        if (all || all_nt || all_dos) {
                /* Exclusions are delimited by '!' */
                for (;
                     pExclude != NULL;
                     pExclude = (p == NULL ? NULL : p + 1)) {

                        /* Find end of this exclusion name */
                        if ((p = strchr(pExclude, '!')) != NULL)
                        {
                                *p = '\0';
                        }

                        /* Which exclusion name is this? */
                        if (StrCaseCmp(pExclude,
                                       "nt_sec_desc.revision") == 0) {
                                exclude_nt_revision = True;
                        }
                        else if (StrCaseCmp(pExclude,
                                            "nt_sec_desc.owner") == 0) {
                                exclude_nt_owner = True;
                        }
                        else if (StrCaseCmp(pExclude,
                                            "nt_sec_desc.group") == 0) {
                                exclude_nt_group = True;
                        }
                        else if (StrCaseCmp(pExclude,
                                            "nt_sec_desc.acl") == 0) {
                                exclude_nt_acl = True;
                        }
                        else if (StrCaseCmp(pExclude,
                                            "dos_attr.mode") == 0) {
                                exclude_dos_mode = True;
                        }
                        else if (StrCaseCmp(pExclude,
                                            "dos_attr.size") == 0) {
                                exclude_dos_size = True;
                        }
                        else if (excl_attr_strings.create_time_attr != NULL &&
                                 StrCaseCmp(pExclude,
                                            excl_attr_strings.change_time_attr) == 0) {
                                exclude_dos_create_time = True;
                        }
                        else if (StrCaseCmp(pExclude,
                                            excl_attr_strings.access_time_attr) == 0) {
                                exclude_dos_access_time = True;
                        }
                        else if (StrCaseCmp(pExclude,
                                            excl_attr_strings.write_time_attr) == 0) {
                                exclude_dos_write_time = True;
                        }
                        else if (StrCaseCmp(pExclude,
                                            excl_attr_strings.change_time_attr) == 0) {
                                exclude_dos_change_time = True;
                        }
                        else if (StrCaseCmp(pExclude, "dos_attr.inode") == 0) {
                                exclude_dos_inode = True;
                        }
                        else {
                                DEBUG(5, ("cacl_get received unknown exclusion: %s\n",
                                          pExclude));
                                errno = ENOATTR;
                                return -1;
                        }
                }
        }

        n_used = 0;

        /*
         * If we are (possibly) talking to an NT or new system and some NT
         * attributes have been requested...
         */
        if (ipc_cli && (all || some_nt || all_nt_acls)) {
		char *targetpath = NULL;
	        struct cli_state *targetcli = NULL;

                /* Point to the portion after "system.nt_sec_desc." */
                name += 19;     /* if (all) this will be invalid but unused */

		if (!cli_resolve_path(ctx, "", context->internal->auth_info,
				cli, filename,
				&targetcli, &targetpath)) {
			DEBUG(5, ("cacl_get Could not resolve %s\n",
				filename));
                        errno = ENOENT;
                        return -1;
		}

                /* ... then obtain any NT attributes which were requested */
                if (!NT_STATUS_IS_OK(cli_ntcreate(targetcli, targetpath, 0, CREATE_ACCESS_READ, 0,
				FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, 0x0, 0x0, &fnum))) {
			DEBUG(5, ("cacl_get failed to open %s: %s\n",
				targetpath, cli_errstr(targetcli)));
			errno = 0;
			return -1;
		}

		sd = cli_query_secdesc(targetcli, fnum, ctx);

                if (!sd) {
                        DEBUG(5,
                              ("cacl_get Failed to query old descriptor\n"));
                        errno = 0;
                        return -1;
                }

                cli_close(targetcli, fnum);

                if (! exclude_nt_revision) {
                        if (all || all_nt) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx,
                                                            "REVISION:%d",
                                                            sd->revision);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "REVISION:%d",
                                                     sd->revision);
                                }
                        } else if (StrCaseCmp(name, "revision") == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, "%d",
                                                            sd->revision);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize, "%d",
                                                     sd->revision);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_nt_owner) {
                        /* Get owner and group sid */
                        if (sd->owner_sid) {
                                convert_sid_to_string(ipc_cli, pol,
                                                      sidstr,
                                                      numeric,
                                                      sd->owner_sid);
                        } else {
                                fstrcpy(sidstr, "");
                        }

                        if (all || all_nt) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, ",OWNER:%s",
                                                            sidstr);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else if (sidstr[0] != '\0') {
                                        n = snprintf(buf, bufsize,
                                                     ",OWNER:%s", sidstr);
                                }
                        } else if (StrnCaseCmp(name, "owner", 5) == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, "%s", sidstr);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize, "%s",
                                                     sidstr);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_nt_group) {
                        if (sd->group_sid) {
                                convert_sid_to_string(ipc_cli, pol,
                                                      sidstr, numeric,
                                                      sd->group_sid);
                        } else {
                                fstrcpy(sidstr, "");
                        }

                        if (all || all_nt) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, ",GROUP:%s",
                                                            sidstr);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else if (sidstr[0] != '\0') {
                                        n = snprintf(buf, bufsize,
                                                     ",GROUP:%s", sidstr);
                                }
                        } else if (StrnCaseCmp(name, "group", 5) == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, "%s", sidstr);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "%s", sidstr);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_nt_acl) {
                        /* Add aces to value buffer  */
                        for (i = 0; sd->dacl && i < sd->dacl->num_aces; i++) {

                                struct security_ace *ace = &sd->dacl->aces[i];
                                convert_sid_to_string(ipc_cli, pol,
                                                      sidstr, numeric,
                                                      &ace->trustee);

                                if (all || all_nt) {
                                        if (determine_size) {
                                                p = talloc_asprintf(
                                                        ctx, 
                                                        ",ACL:"
                                                        "%s:%d/%d/0x%08x", 
                                                        sidstr,
                                                        ace->type,
                                                        ace->flags,
                                                        ace->access_mask);
                                                if (!p) {
                                                        errno = ENOMEM;
                                                        return -1;
                                                }
                                                n = strlen(p);
                                        } else {
                                                n = snprintf(
                                                        buf, bufsize,
                                                        ",ACL:%s:%d/%d/0x%08x", 
                                                        sidstr,
                                                        ace->type,
                                                        ace->flags,
                                                        ace->access_mask);
                                        }
                                } else if ((StrnCaseCmp(name, "acl", 3) == 0 &&
                                            StrCaseCmp(name+3, sidstr) == 0) ||
                                           (StrnCaseCmp(name, "acl+", 4) == 0 &&
                                            StrCaseCmp(name+4, sidstr) == 0)) {
                                        if (determine_size) {
                                                p = talloc_asprintf(
                                                        ctx, 
                                                        "%d/%d/0x%08x", 
                                                        ace->type,
                                                        ace->flags,
                                                        ace->access_mask);
                                                if (!p) {
                                                        errno = ENOMEM;
                                                        return -1;
                                                }
                                                n = strlen(p);
                                        } else {
                                                n = snprintf(buf, bufsize,
                                                             "%d/%d/0x%08x", 
                                                             ace->type,
                                                             ace->flags,
                                                             ace->access_mask);
                                        }
                                } else if (all_nt_acls) {
                                        if (determine_size) {
                                                p = talloc_asprintf(
                                                        ctx, 
                                                        "%s%s:%d/%d/0x%08x",
                                                        i ? "," : "",
                                                        sidstr,
                                                        ace->type,
                                                        ace->flags,
                                                        ace->access_mask);
                                                if (!p) {
                                                        errno = ENOMEM;
                                                        return -1;
                                                }
                                                n = strlen(p);
                                        } else {
                                                n = snprintf(buf, bufsize,
                                                             "%s%s:%d/%d/0x%08x",
                                                             i ? "," : "",
                                                             sidstr,
                                                             ace->type,
                                                             ace->flags,
                                                             ace->access_mask);
                                        }
                                }
                                if (!determine_size && n > bufsize) {
                                        errno = ERANGE;
                                        return -1;
                                }
                                buf += n;
                                n_used += n;
                                bufsize -= n;
                                n = 0;
                        }
                }

                /* Restore name pointer to its original value */
                name -= 19;
        }

        if (all || some_dos) {
                /* Point to the portion after "system.dos_attr." */
                name += 16;     /* if (all) this will be invalid but unused */

                /* Obtain the DOS attributes */
                if (!SMBC_getatr(context, srv, filename, &mode, &size, 
                                 &create_time_ts,
                                 &access_time_ts,
                                 &write_time_ts,
                                 &change_time_ts,
                                 &ino)) {

                        errno = SMBC_errno(context, srv->cli);
                        return -1;
                }

                create_time = convert_timespec_to_time_t(create_time_ts);
                access_time = convert_timespec_to_time_t(access_time_ts);
                write_time = convert_timespec_to_time_t(write_time_ts);
                change_time = convert_timespec_to_time_t(change_time_ts);

                if (! exclude_dos_mode) {
                        if (all || all_dos) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx,
                                                            "%sMODE:0x%x",
                                                            (ipc_cli &&
                                                             (all || some_nt)
                                                             ? ","
                                                             : ""),
                                                            mode);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "%sMODE:0x%x",
                                                     (ipc_cli &&
                                                      (all || some_nt)
                                                      ? ","
                                                      : ""),
                                                     mode);
                                }
                        } else if (StrCaseCmp(name, "mode") == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, "0x%x", mode);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "0x%x", mode);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_dos_size) {
                        if (all || all_dos) {
                                if (determine_size) {
                                        p = talloc_asprintf(
                                                ctx,
                                                ",SIZE:%.0f",
                                                (double)size);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     ",SIZE:%.0f",
                                                     (double)size);
                                }
                        } else if (StrCaseCmp(name, "size") == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(
                                                ctx,
                                                "%.0f",
                                                (double)size);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "%.0f",
                                                     (double)size);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_dos_create_time &&
                    attr_strings.create_time_attr != NULL) {
                        if (all || all_dos) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx,
                                                            ",%s:%lu",
                                                            attr_strings.create_time_attr,
                                                            (unsigned long) create_time);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     ",%s:%lu",
                                                     attr_strings.create_time_attr,
                                                     (unsigned long) create_time);
                                }
                        } else if (StrCaseCmp(name, attr_strings.create_time_attr) == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, "%lu", (unsigned long) create_time);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "%lu", (unsigned long) create_time);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_dos_access_time) {
                        if (all || all_dos) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx,
                                                            ",%s:%lu",
                                                            attr_strings.access_time_attr,
                                                            (unsigned long) access_time);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     ",%s:%lu",
                                                     attr_strings.access_time_attr,
                                                     (unsigned long) access_time);
                                }
                        } else if (StrCaseCmp(name, attr_strings.access_time_attr) == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, "%lu", (unsigned long) access_time);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "%lu", (unsigned long) access_time);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_dos_write_time) {
                        if (all || all_dos) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx,
                                                            ",%s:%lu",
                                                            attr_strings.write_time_attr,
                                                            (unsigned long) write_time);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     ",%s:%lu",
                                                     attr_strings.write_time_attr,
                                                     (unsigned long) write_time);
                                }
                        } else if (StrCaseCmp(name, attr_strings.write_time_attr) == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, "%lu", (unsigned long) write_time);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "%lu", (unsigned long) write_time);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_dos_change_time) {
                        if (all || all_dos) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx,
                                                            ",%s:%lu",
                                                            attr_strings.change_time_attr,
                                                            (unsigned long) change_time);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     ",%s:%lu",
                                                     attr_strings.change_time_attr,
                                                     (unsigned long) change_time);
                                }
                        } else if (StrCaseCmp(name, attr_strings.change_time_attr) == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(ctx, "%lu", (unsigned long) change_time);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "%lu", (unsigned long) change_time);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                if (! exclude_dos_inode) {
                        if (all || all_dos) {
                                if (determine_size) {
                                        p = talloc_asprintf(
                                                ctx,
                                                ",INODE:%.0f",
                                                (double)ino);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     ",INODE:%.0f",
                                                     (double) ino);
                                }
                        } else if (StrCaseCmp(name, "inode") == 0) {
                                if (determine_size) {
                                        p = talloc_asprintf(
                                                ctx,
                                                "%.0f",
                                                (double) ino);
                                        if (!p) {
                                                errno = ENOMEM;
                                                return -1;
                                        }
                                        n = strlen(p);
                                } else {
                                        n = snprintf(buf, bufsize,
                                                     "%.0f",
                                                     (double) ino);
                                }
                        }

                        if (!determine_size && n > bufsize) {
                                errno = ERANGE;
                                return -1;
                        }
                        buf += n;
                        n_used += n;
                        bufsize -= n;
                        n = 0;
                }

                /* Restore name pointer to its original value */
                name -= 16;
        }

        if (n_used == 0) {
                errno = ENOATTR;
                return -1;
        }

	return n_used;
}

/*****************************************************
set the ACLs on a file given an ascii description
*******************************************************/
static int
cacl_set(SMBCCTX *context,
	TALLOC_CTX *ctx,
	struct cli_state *cli,
	struct cli_state *ipc_cli,
	struct policy_handle *pol,
	const char *filename,
	char *the_acl,
	int mode,
	int flags)
{
	uint16_t fnum = (uint16_t)-1;
        int err = 0;
	struct security_descriptor *sd = NULL, *old;
        struct security_acl *dacl = NULL;
	struct dom_sid *owner_sid = NULL;
	struct dom_sid *group_sid = NULL;
	uint32 i, j;
	size_t sd_size;
	int ret = 0;
        char *p;
        bool numeric = True;
	char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	NTSTATUS status;

        /* the_acl will be null for REMOVE_ALL operations */
        if (the_acl) {
                numeric = ((p = strchr(the_acl, ':')) != NULL &&
                           p > the_acl &&
                           p[-1] != '+');

                /* if this is to set the entire ACL... */
                if (*the_acl == '*') {
                        /* ... then increment past the first colon */
                        the_acl = p + 1;
                }

                sd = sec_desc_parse(ctx, ipc_cli, pol, numeric, the_acl);
                if (!sd) {
			errno = EINVAL;
			return -1;
                }
        }

	/* SMBC_XATTR_MODE_REMOVE_ALL is the only caller
	   that doesn't deref sd */

	if (!sd && (mode != SMBC_XATTR_MODE_REMOVE_ALL)) {
		errno = EINVAL;
		return -1;
	}

	if (!cli_resolve_path(ctx, "", context->internal->auth_info,
			cli, filename,
			&targetcli, &targetpath)) {
		DEBUG(5,("cacl_set: Could not resolve %s\n", filename));
		errno = ENOENT;
		return -1;
	}

	/* The desired access below is the only one I could find that works
	   with NT4, W2KP and Samba */

	if (!NT_STATUS_IS_OK(cli_ntcreate(targetcli, targetpath, 0, CREATE_ACCESS_READ, 0,
				FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, 0x0, 0x0, &fnum))) {
                DEBUG(5, ("cacl_set failed to open %s: %s\n",
                          targetpath, cli_errstr(targetcli)));
                errno = 0;
		return -1;
	}

	old = cli_query_secdesc(targetcli, fnum, ctx);

	if (!old) {
                DEBUG(5, ("cacl_set Failed to query old descriptor\n"));
                errno = 0;
		return -1;
	}

	cli_close(targetcli, fnum);

	switch (mode) {
	case SMBC_XATTR_MODE_REMOVE_ALL:
                old->dacl->num_aces = 0;
                dacl = old->dacl;
                break;

        case SMBC_XATTR_MODE_REMOVE:
		for (i=0;sd->dacl && i<sd->dacl->num_aces;i++) {
			bool found = False;

			for (j=0;old->dacl && j<old->dacl->num_aces;j++) {
                                if (sec_ace_equal(&sd->dacl->aces[i],
                                                  &old->dacl->aces[j])) {
					uint32 k;
					for (k=j; k<old->dacl->num_aces-1;k++) {
						old->dacl->aces[k] =
                                                        old->dacl->aces[k+1];
					}
					old->dacl->num_aces--;
					found = True;
                                        dacl = old->dacl;
					break;
				}
			}

			if (!found) {
                                err = ENOATTR;
                                ret = -1;
                                goto failed;
			}
		}
		break;

	case SMBC_XATTR_MODE_ADD:
		for (i=0;sd->dacl && i<sd->dacl->num_aces;i++) {
			bool found = False;

			for (j=0;old->dacl && j<old->dacl->num_aces;j++) {
				if (dom_sid_equal(&sd->dacl->aces[i].trustee,
					      &old->dacl->aces[j].trustee)) {
                                        if (!(flags & SMBC_XATTR_FLAG_CREATE)) {
                                                err = EEXIST;
                                                ret = -1;
                                                goto failed;
                                        }
                                        old->dacl->aces[j] = sd->dacl->aces[i];
                                        ret = -1;
					found = True;
				}
			}

			if (!found && (flags & SMBC_XATTR_FLAG_REPLACE)) {
                                err = ENOATTR;
                                ret = -1;
                                goto failed;
			}

                        for (i=0;sd->dacl && i<sd->dacl->num_aces;i++) {
                                add_ace(&old->dacl, &sd->dacl->aces[i], ctx);
                        }
		}
                dacl = old->dacl;
		break;

	case SMBC_XATTR_MODE_SET:
 		old = sd;
                owner_sid = old->owner_sid;
                group_sid = old->group_sid;
                dacl = old->dacl;
		break;

        case SMBC_XATTR_MODE_CHOWN:
                owner_sid = sd->owner_sid;
                break;

        case SMBC_XATTR_MODE_CHGRP:
                group_sid = sd->group_sid;
                break;
	}

	/* Denied ACE entries must come before allowed ones */
	sort_acl(old->dacl);

	/* Create new security descriptor and set it */
	sd = make_sec_desc(ctx, old->revision, SEC_DESC_SELF_RELATIVE,
			   owner_sid, group_sid, NULL, dacl, &sd_size);

	if (!NT_STATUS_IS_OK(cli_ntcreate(targetcli, targetpath, 0,
                             WRITE_DAC_ACCESS | WRITE_OWNER_ACCESS, 0,
			     FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, 0x0, 0x0, &fnum))) {
		DEBUG(5, ("cacl_set failed to open %s: %s\n",
                          targetpath, cli_errstr(targetcli)));
                errno = 0;
		return -1;
	}

	status = cli_set_secdesc(targetcli, fnum, sd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5, ("ERROR: secdesc set failed: %s\n",
			  nt_errstr(status)));
		ret = -1;
	}

	/* Clean up */

failed:
	cli_close(targetcli, fnum);

        if (err != 0) {
                errno = err;
        }

	return ret;
}


int
SMBC_setxattr_ctx(SMBCCTX *context,
                  const char *fname,
                  const char *name,
                  const void *value,
                  size_t size,
                  int flags)
{
        int ret;
        int ret2;
        SMBCSRV *srv = NULL;
        SMBCSRV *ipc_srv = NULL;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
        DOS_ATTR_DESC *dad = NULL;
        struct {
                const char * create_time_attr;
                const char * access_time_attr;
                const char * write_time_attr;
                const char * change_time_attr;
        } attr_strings;
        TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {
		errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
		return -1;
	}

	if (!fname) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	DEBUG(4, ("smbc_setxattr(%s, %s, %.*s)\n",
                  fname, name, (int) size, (const char*)value));

	if (SMBC_parse_path(frame,
                            context,
                            fname,
                            &workgroup,
                            &server,
                            &share,
                            &path,
                            &user,
                            &password,
                            NULL)) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, smbc_getUser(context));
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

	srv = SMBC_server(frame, context, True,
                          server, share, &workgroup, &user, &password);
	if (!srv) {
		TALLOC_FREE(frame);
		return -1;  /* errno set by SMBC_server */
	}

        if (! srv->no_nt_session) {
                ipc_srv = SMBC_attr_server(frame, context, server, share,
                                           &workgroup, &user, &password);
                if (! ipc_srv) {
                        srv->no_nt_session = True;
                }
        } else {
                ipc_srv = NULL;
        }

        /*
         * Are they asking to set the entire set of known attributes?
         */
        if (StrCaseCmp(name, "system.*") == 0 ||
            StrCaseCmp(name, "system.*+") == 0) {
                /* Yup. */
                char *namevalue =
                        talloc_asprintf(talloc_tos(), "%s:%s",
                                        name+7, (const char *) value);
                if (! namevalue) {
                        errno = ENOMEM;
                        ret = -1;
			TALLOC_FREE(frame);
                        return -1;
                }

                if (ipc_srv) {
                        ret = cacl_set(context, talloc_tos(), srv->cli,
                                       ipc_srv->cli, &ipc_srv->pol, path,
                                       namevalue,
                                       (*namevalue == '*'
                                        ? SMBC_XATTR_MODE_SET
                                        : SMBC_XATTR_MODE_ADD),
                                       flags);
                } else {
                        ret = 0;
                }

                /* get a DOS Attribute Descriptor with current attributes */
                dad = dos_attr_query(context, talloc_tos(), path, srv);
                if (dad) {
                        /* Overwrite old with new, using what was provided */
                        dos_attr_parse(context, dad, srv, namevalue);

                        /* Set the new DOS attributes */
                        if (! SMBC_setatr(context, srv, path,
                                          dad->create_time,
                                          dad->access_time,
                                          dad->write_time,
                                          dad->change_time,
                                          dad->mode)) {

                                /* cause failure if NT failed too */
                                dad = NULL; 
                        }
                }

                /* we only fail if both NT and DOS sets failed */
                if (ret < 0 && ! dad) {
                        ret = -1; /* in case dad was null */
                }
                else {
                        ret = 0;
                }

		TALLOC_FREE(frame);
                return ret;
        }

        /*
         * Are they asking to set an access control element or to set
         * the entire access control list?
         */
        if (StrCaseCmp(name, "system.nt_sec_desc.*") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.*+") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.revision") == 0 ||
            StrnCaseCmp(name, "system.nt_sec_desc.acl", 22) == 0 ||
            StrnCaseCmp(name, "system.nt_sec_desc.acl+", 23) == 0) {

                /* Yup. */
                char *namevalue =
                        talloc_asprintf(talloc_tos(), "%s:%s",
                                        name+19, (const char *) value);

                if (! ipc_srv) {
                        ret = -1; /* errno set by SMBC_server() */
                }
                else if (! namevalue) {
                        errno = ENOMEM;
                        ret = -1;
                } else {
                        ret = cacl_set(context, talloc_tos(), srv->cli,
                                       ipc_srv->cli, &ipc_srv->pol, path,
                                       namevalue,
                                       (*namevalue == '*'
                                        ? SMBC_XATTR_MODE_SET
                                        : SMBC_XATTR_MODE_ADD),
                                       flags);
                }
		TALLOC_FREE(frame);
                return ret;
        }

        /*
         * Are they asking to set the owner?
         */
        if (StrCaseCmp(name, "system.nt_sec_desc.owner") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.owner+") == 0) {

                /* Yup. */
                char *namevalue =
                        talloc_asprintf(talloc_tos(), "%s:%s",
                                        name+19, (const char *) value);

                if (! ipc_srv) {
                        ret = -1; /* errno set by SMBC_server() */
                }
                else if (! namevalue) {
                        errno = ENOMEM;
                        ret = -1;
                } else {
                        ret = cacl_set(context, talloc_tos(), srv->cli,
                                       ipc_srv->cli, &ipc_srv->pol, path,
                                       namevalue, SMBC_XATTR_MODE_CHOWN, 0);
                }
		TALLOC_FREE(frame);
                return ret;
        }

        /*
         * Are they asking to set the group?
         */
        if (StrCaseCmp(name, "system.nt_sec_desc.group") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.group+") == 0) {

                /* Yup. */
                char *namevalue =
                        talloc_asprintf(talloc_tos(), "%s:%s",
                                        name+19, (const char *) value);

                if (! ipc_srv) {
                        /* errno set by SMBC_server() */
                        ret = -1;
                }
                else if (! namevalue) {
                        errno = ENOMEM;
                        ret = -1;
                } else {
                        ret = cacl_set(context, talloc_tos(), srv->cli,
                                       ipc_srv->cli, &ipc_srv->pol, path,
                                       namevalue, SMBC_XATTR_MODE_CHGRP, 0);
                }
		TALLOC_FREE(frame);
                return ret;
        }

        /* Determine whether to use old-style or new-style attribute names */
        if (context->internal->full_time_names) {
                /* new-style names */
                attr_strings.create_time_attr = "system.dos_attr.CREATE_TIME";
                attr_strings.access_time_attr = "system.dos_attr.ACCESS_TIME";
                attr_strings.write_time_attr = "system.dos_attr.WRITE_TIME";
                attr_strings.change_time_attr = "system.dos_attr.CHANGE_TIME";
        } else {
                /* old-style names */
                attr_strings.create_time_attr = NULL;
                attr_strings.access_time_attr = "system.dos_attr.A_TIME";
                attr_strings.write_time_attr = "system.dos_attr.M_TIME";
                attr_strings.change_time_attr = "system.dos_attr.C_TIME";
        }

        /*
         * Are they asking to set a DOS attribute?
         */
        if (StrCaseCmp(name, "system.dos_attr.*") == 0 ||
            StrCaseCmp(name, "system.dos_attr.mode") == 0 ||
            (attr_strings.create_time_attr != NULL &&
             StrCaseCmp(name, attr_strings.create_time_attr) == 0) ||
            StrCaseCmp(name, attr_strings.access_time_attr) == 0 ||
            StrCaseCmp(name, attr_strings.write_time_attr) == 0 ||
            StrCaseCmp(name, attr_strings.change_time_attr) == 0) {

                /* get a DOS Attribute Descriptor with current attributes */
                dad = dos_attr_query(context, talloc_tos(), path, srv);
                if (dad) {
                        char *namevalue =
                                talloc_asprintf(talloc_tos(), "%s:%s",
                                                name+16, (const char *) value);
                        if (! namevalue) {
                                errno = ENOMEM;
                                ret = -1;
                        } else {
                                /* Overwrite old with provided new params */
                                dos_attr_parse(context, dad, srv, namevalue);

                                /* Set the new DOS attributes */
                                ret2 = SMBC_setatr(context, srv, path,
                                                   dad->create_time,
                                                   dad->access_time,
                                                   dad->write_time,
                                                   dad->change_time,
                                                   dad->mode);

                                /* ret2 has True (success) / False (failure) */
                                if (ret2) {
                                        ret = 0;
                                } else {
                                        ret = -1;
                                }
                        }
                } else {
                        ret = -1;
                }

		TALLOC_FREE(frame);
                return ret;
        }

        /* Unsupported attribute name */
        errno = EINVAL;
	TALLOC_FREE(frame);
        return -1;
}

int
SMBC_getxattr_ctx(SMBCCTX *context,
                  const char *fname,
                  const char *name,
                  const void *value,
                  size_t size)
{
        int ret;
        SMBCSRV *srv = NULL;
        SMBCSRV *ipc_srv = NULL;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
        struct {
                const char * create_time_attr;
                const char * access_time_attr;
                const char * write_time_attr;
                const char * change_time_attr;
        } attr_strings;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {
                errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
                return -1;
        }

        if (!fname) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

        DEBUG(4, ("smbc_getxattr(%s, %s)\n", fname, name));

        if (SMBC_parse_path(frame,
                            context,
                            fname,
                            &workgroup,
                            &server,
                            &share,
                            &path,
                            &user,
                            &password,
                            NULL)) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

        if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, smbc_getUser(context));
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

        srv = SMBC_server(frame, context, True,
                          server, share, &workgroup, &user, &password);
        if (!srv) {
		TALLOC_FREE(frame);
                return -1;  /* errno set by SMBC_server */
        }

        if (! srv->no_nt_session) {
                ipc_srv = SMBC_attr_server(frame, context, server, share,
                                           &workgroup, &user, &password);
                if (! ipc_srv) {
                        srv->no_nt_session = True;
                }
        } else {
                ipc_srv = NULL;
        }

        /* Determine whether to use old-style or new-style attribute names */
        if (context->internal->full_time_names) {
                /* new-style names */
                attr_strings.create_time_attr = "system.dos_attr.CREATE_TIME";
                attr_strings.access_time_attr = "system.dos_attr.ACCESS_TIME";
                attr_strings.write_time_attr = "system.dos_attr.WRITE_TIME";
                attr_strings.change_time_attr = "system.dos_attr.CHANGE_TIME";
        } else {
                /* old-style names */
                attr_strings.create_time_attr = NULL;
                attr_strings.access_time_attr = "system.dos_attr.A_TIME";
                attr_strings.write_time_attr = "system.dos_attr.M_TIME";
                attr_strings.change_time_attr = "system.dos_attr.C_TIME";
        }

        /* Are they requesting a supported attribute? */
        if (StrCaseCmp(name, "system.*") == 0 ||
            StrnCaseCmp(name, "system.*!", 9) == 0 ||
            StrCaseCmp(name, "system.*+") == 0 ||
            StrnCaseCmp(name, "system.*+!", 10) == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.*") == 0 ||
            StrnCaseCmp(name, "system.nt_sec_desc.*!", 21) == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.*+") == 0 ||
            StrnCaseCmp(name, "system.nt_sec_desc.*+!", 22) == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.revision") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.owner") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.owner+") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.group") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.group+") == 0 ||
            StrnCaseCmp(name, "system.nt_sec_desc.acl", 22) == 0 ||
            StrnCaseCmp(name, "system.nt_sec_desc.acl+", 23) == 0 ||
            StrCaseCmp(name, "system.dos_attr.*") == 0 ||
            StrnCaseCmp(name, "system.dos_attr.*!", 18) == 0 ||
            StrCaseCmp(name, "system.dos_attr.mode") == 0 ||
            StrCaseCmp(name, "system.dos_attr.size") == 0 ||
            (attr_strings.create_time_attr != NULL &&
             StrCaseCmp(name, attr_strings.create_time_attr) == 0) ||
            StrCaseCmp(name, attr_strings.access_time_attr) == 0 ||
            StrCaseCmp(name, attr_strings.write_time_attr) == 0 ||
            StrCaseCmp(name, attr_strings.change_time_attr) == 0 ||
            StrCaseCmp(name, "system.dos_attr.inode") == 0) {

                /* Yup. */
                char *filename = (char *) name;
                ret = cacl_get(context, talloc_tos(), srv,
                               ipc_srv == NULL ? NULL : ipc_srv->cli, 
                               &ipc_srv->pol, path,
                               filename,
                               CONST_DISCARD(char *, value),
                               size);
                if (ret < 0 && errno == 0) {
                        errno = SMBC_errno(context, srv->cli);
                }
		TALLOC_FREE(frame);
                return ret;
        }

        /* Unsupported attribute name */
        errno = EINVAL;
	TALLOC_FREE(frame);
        return -1;
}


int
SMBC_removexattr_ctx(SMBCCTX *context,
                     const char *fname,
                     const char *name)
{
        int ret;
        SMBCSRV *srv = NULL;
        SMBCSRV *ipc_srv = NULL;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {
                errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
                return -1;
        }

        if (!fname) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

        DEBUG(4, ("smbc_removexattr(%s, %s)\n", fname, name));

	if (SMBC_parse_path(frame,
                            context,
                            fname,
                            &workgroup,
                            &server,
                            &share,
                            &path,
                            &user,
                            &password,
                            NULL)) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

        if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, smbc_getUser(context));
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

        srv = SMBC_server(frame, context, True,
                          server, share, &workgroup, &user, &password);
        if (!srv) {
		TALLOC_FREE(frame);
                return -1;  /* errno set by SMBC_server */
        }

        if (! srv->no_nt_session) {
                ipc_srv = SMBC_attr_server(frame, context, server, share,
                                           &workgroup, &user, &password);
                if (! ipc_srv) {
                        srv->no_nt_session = True;
                }
        } else {
                ipc_srv = NULL;
        }

        if (! ipc_srv) {
		TALLOC_FREE(frame);
                return -1; /* errno set by SMBC_attr_server */
        }

        /* Are they asking to set the entire ACL? */
        if (StrCaseCmp(name, "system.nt_sec_desc.*") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.*+") == 0) {

                /* Yup. */
                ret = cacl_set(context, talloc_tos(), srv->cli,
                               ipc_srv->cli, &ipc_srv->pol, path,
                               NULL, SMBC_XATTR_MODE_REMOVE_ALL, 0);
		TALLOC_FREE(frame);
                return ret;
        }

        /*
         * Are they asking to remove one or more spceific security descriptor
         * attributes?
         */
        if (StrCaseCmp(name, "system.nt_sec_desc.revision") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.owner") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.owner+") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.group") == 0 ||
            StrCaseCmp(name, "system.nt_sec_desc.group+") == 0 ||
            StrnCaseCmp(name, "system.nt_sec_desc.acl", 22) == 0 ||
            StrnCaseCmp(name, "system.nt_sec_desc.acl+", 23) == 0) {

                /* Yup. */
                ret = cacl_set(context, talloc_tos(), srv->cli,
                               ipc_srv->cli, &ipc_srv->pol, path,
                               CONST_DISCARD(char *, name) + 19,
                               SMBC_XATTR_MODE_REMOVE, 0);
		TALLOC_FREE(frame);
                return ret;
        }

        /* Unsupported attribute name */
        errno = EINVAL;
	TALLOC_FREE(frame);
        return -1;
}

int
SMBC_listxattr_ctx(SMBCCTX *context,
                   const char *fname,
                   char *list,
                   size_t size)
{
        /*
         * This isn't quite what listxattr() is supposed to do.  This returns
         * the complete set of attribute names, always, rather than only those
         * attribute names which actually exist for a file.  Hmmm...
         */
        size_t retsize;
        const char supported_old[] =
                "system.*\0"
                "system.*+\0"
                "system.nt_sec_desc.revision\0"
                "system.nt_sec_desc.owner\0"
                "system.nt_sec_desc.owner+\0"
                "system.nt_sec_desc.group\0"
                "system.nt_sec_desc.group+\0"
                "system.nt_sec_desc.acl.*\0"
                "system.nt_sec_desc.acl\0"
                "system.nt_sec_desc.acl+\0"
                "system.nt_sec_desc.*\0"
                "system.nt_sec_desc.*+\0"
                "system.dos_attr.*\0"
                "system.dos_attr.mode\0"
                "system.dos_attr.c_time\0"
                "system.dos_attr.a_time\0"
                "system.dos_attr.m_time\0"
                ;
        const char supported_new[] =
                "system.*\0"
                "system.*+\0"
                "system.nt_sec_desc.revision\0"
                "system.nt_sec_desc.owner\0"
                "system.nt_sec_desc.owner+\0"
                "system.nt_sec_desc.group\0"
                "system.nt_sec_desc.group+\0"
                "system.nt_sec_desc.acl.*\0"
                "system.nt_sec_desc.acl\0"
                "system.nt_sec_desc.acl+\0"
                "system.nt_sec_desc.*\0"
                "system.nt_sec_desc.*+\0"
                "system.dos_attr.*\0"
                "system.dos_attr.mode\0"
                "system.dos_attr.create_time\0"
                "system.dos_attr.access_time\0"
                "system.dos_attr.write_time\0"
                "system.dos_attr.change_time\0"
                ;
        const char * supported;

        if (context->internal->full_time_names) {
                supported = supported_new;
                retsize = sizeof(supported_new);
        } else {
                supported = supported_old;
                retsize = sizeof(supported_old);
        }

        if (size == 0) {
                return retsize;
        }

        if (retsize > size) {
                errno = ERANGE;
                return -1;
        }

        /* this can't be strcpy() because there are embedded null characters */
        memcpy(list, supported, retsize);
        return retsize;
}
