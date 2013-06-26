/*
   Copyright (C) Nadezhda Ivanova 2009

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

/*
 *  Name: create_descriptor
 *
 *  Component: routines for calculating and creating security descriptors
 *  as described in MS-DTYP 2.5.3.x
 *
 *  Description:
 *
 *
 *  Author: Nadezhda Ivanova
 */
#include "includes.h"
#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_security.h"

/* Todos:
 * build the security token dacl as follows:
 * SYSTEM: GA, OWNER: GA, LOGIN_SID:GW|GE
 * Need session id information for the login SID. Probably
 * the best place for this is during token creation
 *
 * Implement SD Invariants
 * ACE sorting rules
 * LDAP_SERVER_SD_FLAGS_OID control
 * ADTS 7.1.3.3 needs to be clarified
 */

/* the mapping function for generic rights for DS.(GA,GR,GW,GX)
 * The mapping function is passed as an argument to the
 * descriptor calculating routine and depends on the security
 * manager that calls the calculating routine.
 * TODO: need similar mappings for the file system and
 * registry security managers in order to make this code
 * generic for all security managers
 */

uint32_t map_generic_rights_ds(uint32_t access_mask)
{
	if (access_mask & SEC_GENERIC_ALL) {
		access_mask |= SEC_ADS_GENERIC_ALL;
		access_mask &= ~SEC_GENERIC_ALL;
	}

	if (access_mask & SEC_GENERIC_EXECUTE) {
		access_mask |= SEC_ADS_GENERIC_EXECUTE;
		access_mask &= ~SEC_GENERIC_EXECUTE;
	}

	if (access_mask & SEC_GENERIC_WRITE) {
		access_mask |= SEC_ADS_GENERIC_WRITE;
		access_mask &= ~SEC_GENERIC_WRITE;
	}

	if (access_mask & SEC_GENERIC_READ) {
		access_mask |= SEC_ADS_GENERIC_READ;
		access_mask &= ~SEC_GENERIC_READ;
	}

	return access_mask;
}

/* Not sure what this has to be,
* and it does not seem to have any influence */
static bool object_in_list(struct GUID *object_list, struct GUID *object)
{
	return true;
}
 
/* returns true if the ACE gontains generic information
 * that needs to be processed additionally */
 
static bool desc_ace_has_generic(TALLOC_CTX *mem_ctx,
			     struct security_ace *ace)
{
	struct dom_sid *co, *cg;
	co = dom_sid_parse_talloc(mem_ctx,  SID_CREATOR_OWNER);
	cg = dom_sid_parse_talloc(mem_ctx,  SID_CREATOR_GROUP);
	if (ace->access_mask & SEC_GENERIC_ALL || ace->access_mask & SEC_GENERIC_READ ||
	    ace->access_mask & SEC_GENERIC_WRITE || ace->access_mask & SEC_GENERIC_EXECUTE) {
		return true;
	}
	if (dom_sid_equal(&ace->trustee, co) || dom_sid_equal(&ace->trustee, cg)) {
		return true;
	}
	return false;
}

/* creates an ace in which the generic information is expanded */

static void desc_expand_generic(TALLOC_CTX *mem_ctx,
				struct security_ace *new_ace,
				struct dom_sid *owner,
				struct dom_sid *group)
{
	struct dom_sid *co, *cg;
	co = dom_sid_parse_talloc(mem_ctx,  SID_CREATOR_OWNER);
	cg = dom_sid_parse_talloc(mem_ctx,  SID_CREATOR_GROUP);
	new_ace->access_mask = map_generic_rights_ds(new_ace->access_mask);
	if (dom_sid_equal(&new_ace->trustee, co)) {
		new_ace->trustee = *owner;
	}
	if (dom_sid_equal(&new_ace->trustee, cg)) {
		new_ace->trustee = *group;
	}
	new_ace->flags = 0x0;
}

static struct security_acl *calculate_inherited_from_parent(TALLOC_CTX *mem_ctx,
							    struct security_acl *acl,
							    bool is_container,
							    struct dom_sid *owner,
							    struct dom_sid *group,
							    struct GUID *object_list)
{
	uint32_t i;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	struct security_acl *tmp_acl = talloc_zero(mem_ctx, struct security_acl);
	struct dom_sid *co, *cg;
	if (!tmp_acl) {
		return NULL;
	}

	if (!acl) {
		return NULL;
	}
	co = dom_sid_parse_talloc(tmp_ctx,  SID_CREATOR_OWNER);
	cg = dom_sid_parse_talloc(tmp_ctx,  SID_CREATOR_GROUP);

	for (i=0; i < acl->num_aces; i++) {
		struct security_ace *ace = &acl->aces[i];
		if ((ace->flags & SEC_ACE_FLAG_CONTAINER_INHERIT) ||
		    (ace->flags & SEC_ACE_FLAG_OBJECT_INHERIT)) {
			tmp_acl->aces = talloc_realloc(tmp_acl, tmp_acl->aces,
						       struct security_ace,
						       tmp_acl->num_aces+1);
			if (tmp_acl->aces == NULL) {
				talloc_free(tmp_ctx);
				return NULL;
			}

			tmp_acl->aces[tmp_acl->num_aces] = *ace;
			tmp_acl->aces[tmp_acl->num_aces].flags |= SEC_ACE_FLAG_INHERITED_ACE;
			/* remove IO flag from the child's ace */
			if (ace->flags & SEC_ACE_FLAG_INHERIT_ONLY &&
			    !desc_ace_has_generic(tmp_ctx, ace)) {
				tmp_acl->aces[tmp_acl->num_aces].flags &= ~SEC_ACE_FLAG_INHERIT_ONLY;
			}

			if (is_container && (ace->flags & SEC_ACE_FLAG_OBJECT_INHERIT))
			    tmp_acl->aces[tmp_acl->num_aces].flags |= SEC_ACE_FLAG_INHERIT_ONLY;

			if (ace->type == SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT ||
			    ace->type == SEC_ACE_TYPE_ACCESS_DENIED_OBJECT) {
				if (!object_in_list(object_list, &ace->object.object.type.type)) {
					tmp_acl->aces[tmp_acl->num_aces].flags |= SEC_ACE_FLAG_INHERIT_ONLY;
				}

			}
			tmp_acl->num_aces++;
			if (is_container) {
				if (!(ace->flags & SEC_ACE_FLAG_NO_PROPAGATE_INHERIT) &&
				    (desc_ace_has_generic(tmp_ctx, ace))) {
					    tmp_acl->aces = talloc_realloc(tmp_acl,
									   tmp_acl->aces,
									   struct security_ace,
									   tmp_acl->num_aces+1);
					    if (tmp_acl->aces == NULL) {
						    talloc_free(tmp_ctx);
						    return NULL;
					    }
					    tmp_acl->aces[tmp_acl->num_aces] = *ace;
					    desc_expand_generic(tmp_ctx,
								&tmp_acl->aces[tmp_acl->num_aces],
								owner,
								group);
					    tmp_acl->aces[tmp_acl->num_aces].flags = SEC_ACE_FLAG_INHERITED_ACE;
					    tmp_acl->num_aces++;
				}
			}
		}
	}
	if (tmp_acl->num_aces == 0) {
		return NULL;
	}
	if (acl) {
		tmp_acl->revision = acl->revision;
	}
	return tmp_acl;
}

static struct security_acl *process_user_acl(TALLOC_CTX *mem_ctx,
					     struct security_acl *acl,
					     bool is_container,
					     struct dom_sid *owner,
					     struct dom_sid *group,
					     struct GUID *object_list,
					     bool is_protected)
{
	uint32_t i;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	struct security_acl *tmp_acl = talloc_zero(tmp_ctx, struct security_acl);
	struct security_acl *new_acl;
	struct dom_sid *co, *cg;

	if (!acl)
		return NULL;

	if (!tmp_acl)
		return NULL;

	tmp_acl->revision = acl->revision;
	DEBUG(6,(__location__ ": acl revision %d\n", acl->revision));

	co = dom_sid_parse_talloc(tmp_ctx,  SID_CREATOR_OWNER);
	cg = dom_sid_parse_talloc(tmp_ctx,  SID_CREATOR_GROUP);

	for (i=0; i < acl->num_aces; i++){
		struct security_ace *ace = &acl->aces[i];
		/* Remove ID flags from user-provided ACEs
		 * if we break inheritance, ignore them otherwise */
		if (ace->flags & SEC_ACE_FLAG_INHERITED_ACE) {
			if (is_protected) {
				ace->flags &= ~SEC_ACE_FLAG_INHERITED_ACE;
			} else {
				continue;
			}
		}

		if (ace->flags & SEC_ACE_FLAG_INHERIT_ONLY &&
		    !(ace->flags & SEC_ACE_FLAG_CONTAINER_INHERIT ||
		      ace->flags & SEC_ACE_FLAG_OBJECT_INHERIT))
			continue;

		tmp_acl->aces = talloc_realloc(tmp_acl,
					       tmp_acl->aces,
					       struct security_ace,
					       tmp_acl->num_aces+1);
		tmp_acl->aces[tmp_acl->num_aces] = *ace;
		tmp_acl->num_aces++;
		if (ace->flags & SEC_ACE_FLAG_INHERIT_ONLY) {
			continue;
		}
		/* if the ACE contains CO, CG, GA, GE, GR or GW, and is inheritable
		 * it has to be expanded to two aces, the original as IO,
		 * and another one where these are translated */
		if (desc_ace_has_generic(tmp_ctx, ace)) {
			if (!(ace->flags & SEC_ACE_FLAG_CONTAINER_INHERIT)) {
				desc_expand_generic(tmp_ctx,
						    &tmp_acl->aces[tmp_acl->num_aces-1],
						    owner,
						    group);
			} else {
				/*The original ACE becomes read only */
				tmp_acl->aces[tmp_acl->num_aces-1].flags |= SEC_ACE_FLAG_INHERIT_ONLY;
				tmp_acl->aces = talloc_realloc(tmp_acl, tmp_acl->aces,
							       struct security_ace,
							       tmp_acl->num_aces+1);
				/* add a new ACE with expanded generic info */
				tmp_acl->aces[tmp_acl->num_aces] = *ace;
				desc_expand_generic(tmp_ctx,
						    &tmp_acl->aces[tmp_acl->num_aces],
						    owner,
						    group);
				tmp_acl->num_aces++;
			}
		}
	}
	new_acl = security_acl_dup(mem_ctx,tmp_acl);

	if (new_acl)
		new_acl->revision = acl->revision;

	talloc_free(tmp_ctx);
	return new_acl;
}

static void cr_descr_log_descriptor(struct security_descriptor *sd,
				    const char *message,
				    int level)
{
	if (sd) {
		DEBUG(level,("%s: %s\n", message,
			     ndr_print_struct_string(0,(ndr_print_fn_t)ndr_print_security_descriptor,
						     "", sd)));
	}
	else {
		DEBUG(level,("%s: NULL\n", message));
	}
}

#if 0
static void cr_descr_log_acl(struct security_acl *acl,
				    const char *message,
				    int level)
{
	if (acl) {
		DEBUG(level,("%s: %s\n", message,
			     ndr_print_struct_string(0,(ndr_print_fn_t)ndr_print_security_acl,
						     "", acl)));
	}
	else {
		DEBUG(level,("%s: NULL\n", message));
	}
}
#endif

static bool compute_acl(struct security_descriptor *parent_sd,
			struct security_descriptor *creator_sd,
			bool is_container,
			uint32_t inherit_flags,
			struct GUID *object_list,
			uint32_t (*generic_map)(uint32_t access_mask),
			struct security_token *token,
			struct security_descriptor *new_sd) /* INOUT argument */
{
	struct security_acl *user_dacl, *user_sacl, *inherited_dacl, *inherited_sacl;
	int level = 10;

	if (!parent_sd || !(inherit_flags & SEC_DACL_AUTO_INHERIT)) {
		inherited_dacl = NULL;
	} else if (creator_sd && (creator_sd->type & SEC_DESC_DACL_PROTECTED)) {
		inherited_dacl = NULL;
	} else {
		inherited_dacl = calculate_inherited_from_parent(new_sd,
								 parent_sd->dacl,
								 is_container,
								 new_sd->owner_sid,
								 new_sd->group_sid,
								 object_list);
	}


	if (!parent_sd || !(inherit_flags & SEC_SACL_AUTO_INHERIT)) {
		inherited_sacl = NULL;
	} else if (creator_sd && (creator_sd->type & SEC_DESC_SACL_PROTECTED)) {
		inherited_sacl = NULL;
	} else {
		inherited_sacl = calculate_inherited_from_parent(new_sd,
								 parent_sd->sacl,
								 is_container,
								 new_sd->owner_sid,
								 new_sd->group_sid,
								 object_list);
	}

	if (!creator_sd || (inherit_flags & SEC_DEFAULT_DESCRIPTOR)) {
		user_dacl = NULL;
		user_sacl = NULL;
	} else {
		user_dacl = process_user_acl(new_sd,
					     creator_sd->dacl,
					     is_container,
					     new_sd->owner_sid,
					     new_sd->group_sid,
					     object_list,
					     creator_sd->type & SEC_DESC_DACL_PROTECTED);
		user_sacl = process_user_acl(new_sd,
					     creator_sd->sacl,
					     is_container,
					     new_sd->owner_sid,
					     new_sd->group_sid,
					     object_list,
					     creator_sd->type & SEC_DESC_SACL_PROTECTED);
	}
	cr_descr_log_descriptor(parent_sd, __location__"parent_sd", level);
	cr_descr_log_descriptor(creator_sd,__location__ "creator_sd", level);

	new_sd->dacl = security_acl_concatenate(new_sd, user_dacl, inherited_dacl);
	if (new_sd->dacl) {
		new_sd->type |= SEC_DESC_DACL_PRESENT;
	}
	if (inherited_dacl) {
		new_sd->type |= SEC_DESC_DACL_AUTO_INHERITED;
	}

	new_sd->sacl = security_acl_concatenate(new_sd, user_sacl, inherited_sacl);
	if (new_sd->sacl) {
		new_sd->type |= SEC_DESC_SACL_PRESENT;
	}
	if (inherited_sacl) {
		new_sd->type |= SEC_DESC_SACL_AUTO_INHERITED;
	}
	/* This is a hack to handle the fact that
	 * apprantly any AI flag provided by the user is preserved */
	if (creator_sd)
		new_sd->type |= creator_sd->type;
	cr_descr_log_descriptor(new_sd, __location__"final sd", level);
	return true;
}

struct security_descriptor *create_security_descriptor(TALLOC_CTX *mem_ctx,
						       struct security_descriptor *parent_sd,
						       struct security_descriptor *creator_sd,
						       bool is_container,
						       struct GUID *object_list,
						       uint32_t inherit_flags,
						       struct security_token *token,
						       struct dom_sid *default_owner, /* valid only for DS, NULL for the other RSs */
						       struct dom_sid *default_group, /* valid only for DS, NULL for the other RSs */
						       uint32_t (*generic_map)(uint32_t access_mask))
{
	struct security_descriptor *new_sd;
	struct dom_sid *new_owner = NULL;
	struct dom_sid *new_group = NULL;

	new_sd = security_descriptor_initialise(mem_ctx);
	if (!new_sd) {
		return NULL;
	}

	if (!creator_sd || !creator_sd->owner_sid) {
		if ((inherit_flags & SEC_OWNER_FROM_PARENT) && parent_sd) {
			new_owner = parent_sd->owner_sid;
		} else if (!default_owner) {
			new_owner = &token->sids[PRIMARY_USER_SID_INDEX];
		} else {
			new_owner = default_owner;
			new_sd->type |= SEC_DESC_OWNER_DEFAULTED;
		}
	} else {
		new_owner = creator_sd->owner_sid;
	}

	if (!creator_sd || !creator_sd->group_sid){
		if ((inherit_flags & SEC_GROUP_FROM_PARENT) && parent_sd) {
			new_group = parent_sd->group_sid;
		} else if (!default_group && token->num_sids > PRIMARY_GROUP_SID_INDEX) {
			new_group = &token->sids[PRIMARY_GROUP_SID_INDEX];
		} else if (!default_group) {
			/* This will happen only for anonymous, which has no other groups */
			new_group = &token->sids[PRIMARY_USER_SID_INDEX];
		} else {
			new_group = default_group;
			new_sd->type |= SEC_DESC_GROUP_DEFAULTED;
		}
	} else {
		new_group = creator_sd->group_sid;
	}

	new_sd->owner_sid = talloc_memdup(new_sd, new_owner, sizeof(struct dom_sid));
	new_sd->group_sid = talloc_memdup(new_sd, new_group, sizeof(struct dom_sid));
	if (!new_sd->owner_sid || !new_sd->group_sid){
		talloc_free(new_sd);
		return NULL;
	}

	if (!compute_acl(parent_sd, creator_sd,
			 is_container, inherit_flags, object_list,
			 generic_map,token,new_sd)){
		talloc_free(new_sd);
		return NULL;
	}

	return new_sd;
}
