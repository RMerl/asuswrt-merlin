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
 *  as described in MS-DTYP 2.5.2.2
 *
 *  Description:
 *
 *
 *  Author: Nadezhda Ivanova
 */
#include "includes.h"
#include "libcli/security/security.h"

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
	if (access_mask & SEC_GENERIC_ALL){
		access_mask |= SEC_ADS_GENERIC_ALL;
		access_mask = ~SEC_GENERIC_ALL;
	}

	if (access_mask & SEC_GENERIC_EXECUTE){
		access_mask |= SEC_ADS_GENERIC_EXECUTE;
		access_mask = ~SEC_GENERIC_EXECUTE;
	}

	if (access_mask & SEC_GENERIC_WRITE){
		access_mask |= SEC_ADS_GENERIC_WRITE;
		access_mask &= ~SEC_GENERIC_WRITE;
	}

	if (access_mask & SEC_GENERIC_READ){
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


static bool contains_inheritable_aces(struct security_acl *acl)
{
        int i;
	if (!acl)
		return false;

	for (i=0; i < acl->num_aces; i++) {
		struct security_ace *ace = &acl->aces[i];
		if ((ace->flags & SEC_ACE_FLAG_CONTAINER_INHERIT) ||
		    (ace->flags & SEC_ACE_FLAG_OBJECT_INHERIT))
			return true;
	}

	return false;
}

static struct security_acl *preprocess_creator_acl(TALLOC_CTX *mem, struct security_acl *acl)
{
	int i;
	struct security_acl *new_acl; 
	if (!acl) {
		return NULL;
	}
	
	new_acl = talloc_zero(mem, struct security_acl);

	for (i=0; i < acl->num_aces; i++) {
		struct security_ace *ace = &acl->aces[i];
		if (!(ace->flags & SEC_ACE_FLAG_INHERITED_ACE)){
			new_acl->aces = talloc_realloc(new_acl, new_acl->aces, struct security_ace,
					   new_acl->num_aces+1);
			if (new_acl->aces == NULL) {
				talloc_free(new_acl);
				return NULL;
			}
			new_acl->aces[new_acl->num_aces] = *ace;
			/*memcpy(&new_acl->aces[new_acl->num_aces], ace,
			  sizeof(struct security_ace)); */
			new_acl->num_aces++;
		}
	}
	if (new_acl)
		new_acl->revision = acl->revision;
	/* Todo what to do if all were inherited and this is empty */
	return new_acl;
}

/* This is not exactly as described in the docs. The original seemed to return
 * only a list of the inherited or flagless ones... */

static bool postprocess_acl(struct security_acl *acl,
			    struct dom_sid *owner,
			    struct dom_sid *group,
			    uint32_t (*generic_map)(uint32_t access_mask))
{
	int i;
	struct dom_sid *co, *cg;
	TALLOC_CTX *tmp_ctx = talloc_new(acl);
	if (!generic_map){
		return false;
	}
	co = dom_sid_parse_talloc(tmp_ctx,  SID_CREATOR_OWNER);
	cg = dom_sid_parse_talloc(tmp_ctx,  SID_CREATOR_GROUP);
	for (i=0; i < acl->num_aces; i++){
		struct security_ace *ace = &acl->aces[i];
		if (!(ace->flags == 0 || ace->flags & SEC_ACE_FLAG_INHERITED_ACE))
			continue;
		if (ace->flags & SEC_ACE_FLAG_INHERIT_ONLY)
			continue;
		if (dom_sid_equal(&ace->trustee, co)){
			ace->trustee = *owner;
			/* perhaps this should be done somewhere else? */
			ace->flags &= ~SEC_ACE_FLAG_CONTAINER_INHERIT;
		}
		if (dom_sid_equal(&ace->trustee, cg)){
			ace->trustee = *group;
			ace->flags &= ~SEC_ACE_FLAG_CONTAINER_INHERIT;
		}
		ace->access_mask = generic_map(ace->access_mask);
	}

	talloc_free(tmp_ctx);
	return true;
}

static struct security_acl *calculate_inherited_from_parent(TALLOC_CTX *mem_ctx,
						struct security_acl *acl,
						bool is_container,
						struct GUID *object_list)
{
	int i;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	struct security_acl *tmp_acl = talloc_zero(tmp_ctx, struct security_acl);
	struct security_acl *inh_acl = talloc_zero(tmp_ctx, struct security_acl);
	struct security_acl *new_acl;
	struct dom_sid *co, *cg;
	if (!tmp_acl || !inh_acl)
		return NULL;

	co = dom_sid_parse_talloc(tmp_ctx,  SID_CREATOR_OWNER);
	cg = dom_sid_parse_talloc(tmp_ctx,  SID_CREATOR_GROUP);

	for (i=0; i < acl->num_aces; i++){
		struct security_ace *ace = &acl->aces[i];
		if (ace->flags & SEC_ACE_FLAG_INHERIT_ONLY)
			continue;

		if ((ace->flags & SEC_ACE_FLAG_CONTAINER_INHERIT) ||
		    (ace->flags & SEC_ACE_FLAG_OBJECT_INHERIT)){
			tmp_acl->aces = talloc_realloc(tmp_acl, tmp_acl->aces, struct security_ace,
						       tmp_acl->num_aces+1);
			if (tmp_acl->aces == NULL) {
				talloc_free(tmp_ctx);
				return NULL;
			}

			tmp_acl->aces[tmp_acl->num_aces] = *ace;
			tmp_acl->aces[tmp_acl->num_aces].flags |= SEC_ACE_FLAG_INHERITED_ACE;

			if (is_container && (ace->flags & SEC_ACE_FLAG_OBJECT_INHERIT))
			    tmp_acl->aces[tmp_acl->num_aces].flags |= SEC_ACE_FLAG_INHERIT_ONLY;

			if (ace->type == SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT ||
			    ace->type == SEC_ACE_TYPE_ACCESS_DENIED_OBJECT){
				if (!object_in_list(object_list, &ace->object.object.type.type)){
					tmp_acl->aces[tmp_acl->num_aces].flags |= SEC_ACE_FLAG_INHERIT_ONLY;
				}

			}
			tmp_acl->num_aces++;
		}
	}

	if (is_container){
		for (i=0; i < acl->num_aces; i++){
			struct security_ace *ace = &acl->aces[i];

			if (ace->flags & SEC_ACE_FLAG_NO_PROPAGATE_INHERIT)
				continue;
			if (!dom_sid_equal(&ace->trustee, co) && !dom_sid_equal(&ace->trustee, cg))
				continue;

			if ((ace->flags & SEC_ACE_FLAG_CONTAINER_INHERIT) ||
			    (ace->flags & SEC_ACE_FLAG_OBJECT_INHERIT)){
				inh_acl->aces = talloc_realloc(inh_acl, inh_acl->aces, struct security_ace,
							       inh_acl->num_aces+1);
				if (inh_acl->aces == NULL){
					talloc_free(tmp_ctx);
					return NULL;
				}
				inh_acl->aces[inh_acl->num_aces] = *ace;
				inh_acl->aces[inh_acl->num_aces].flags |= SEC_ACE_FLAG_INHERIT_ONLY;
				inh_acl->aces[inh_acl->num_aces].flags |= SEC_ACE_FLAG_INHERITED_ACE;
				inh_acl->num_aces++;
			}
		}
	}
	new_acl = security_acl_concatenate(mem_ctx,tmp_acl, inh_acl);
	if (new_acl)
		new_acl->revision = acl->revision;
	talloc_free(tmp_ctx);
	return new_acl;
}

/* In the docs this looks == calculate_inherited_from_parent. However,
 * It shouldn't return the inherited, rather filter them out....
 */
static struct security_acl *calculate_inherited_from_creator(TALLOC_CTX *mem_ctx,
						struct security_acl *acl,
						bool is_container,
						struct GUID *object_list)
{
	int i;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	struct security_acl *tmp_acl = talloc_zero(tmp_ctx, struct security_acl);
/*	struct security_acl *inh_acl = talloc_zero(tmp_ctx, struct security_acl); */
	struct security_acl *new_acl;
	struct dom_sid *co, *cg;

	if (!tmp_acl)
		return NULL;

	tmp_acl->revision = acl->revision;
	DEBUG(6,(__location__ ": acl revision %u\n", acl->revision));

	co = dom_sid_parse_talloc(tmp_ctx,  SID_CREATOR_OWNER);
	cg = dom_sid_parse_talloc(tmp_ctx,  SID_CREATOR_GROUP);

	for (i=0; i < acl->num_aces; i++){
		struct security_ace *ace = &acl->aces[i];
		if (ace->flags & SEC_ACE_FLAG_INHERITED_ACE)
			continue;

		tmp_acl->aces = talloc_realloc(tmp_acl, tmp_acl->aces, struct security_ace,
					      tmp_acl->num_aces+1);
		tmp_acl->aces[tmp_acl->num_aces] = *ace;
		tmp_acl->aces[tmp_acl->num_aces].flags  = 0;
		tmp_acl->num_aces++;

		if (!dom_sid_equal(&ace->trustee, co) && !dom_sid_equal(&ace->trustee, cg))
			continue;

		tmp_acl->aces = talloc_realloc(tmp_acl, tmp_acl->aces, struct security_ace,
					       tmp_acl->num_aces+1);
		tmp_acl->aces[tmp_acl->num_aces] = *ace;
		tmp_acl->aces[tmp_acl->num_aces].flags |= SEC_ACE_FLAG_INHERIT_ONLY;
		tmp_acl->num_aces++;
	}
	new_acl = security_acl_dup(mem_ctx,tmp_acl);

	talloc_free(tmp_ctx);
	return new_acl;
}

static bool compute_acl(int acl_type,
			struct security_descriptor *parent_sd,
			struct security_descriptor *creator_sd,
			bool is_container,
			uint32_t inherit_flags,
			struct GUID *object_list,
			uint32_t (*generic_map)(uint32_t access_mask),
			struct security_token *token,
			struct security_descriptor *new_sd) /* INOUT argument */
{
	struct security_acl *p_acl = NULL, *c_acl = NULL, **new_acl;
	if (acl_type == SEC_DESC_DACL_PRESENT){
		if (parent_sd)
			p_acl = parent_sd->dacl;
		if (creator_sd)
			c_acl = creator_sd->dacl;
		new_acl = &new_sd->dacl;
	}
	else{
		if (parent_sd)
			p_acl = parent_sd->sacl;
		if (creator_sd)
			c_acl = creator_sd->sacl;
		new_acl = &new_sd->sacl;
	}
	if (contains_inheritable_aces(p_acl)){
		if (!c_acl || (c_acl && inherit_flags & SEC_DEFAULT_DESCRIPTOR)){
			*new_acl = calculate_inherited_from_parent(new_sd,
						       p_acl,
						       is_container,
						       object_list);
			if (*new_acl == NULL)
				goto final;
			if (!postprocess_acl(*new_acl, new_sd->owner_sid,
					     new_sd->group_sid, generic_map))
				return false;
			else
				goto final;
		}
		if (c_acl && !(inherit_flags & SEC_DEFAULT_DESCRIPTOR)){
			struct security_acl *pr_acl, *tmp_acl, *tpr_acl;
			tpr_acl = preprocess_creator_acl(new_sd, c_acl);
			tmp_acl = calculate_inherited_from_creator(new_sd,
						      tpr_acl,
						      is_container,
						      object_list);
			/* Todo some refactoring here! */
			if (acl_type == SEC_DESC_DACL_PRESENT &&
			    !(creator_sd->type & SECINFO_PROTECTED_DACL) &&
			    (inherit_flags & SEC_DACL_AUTO_INHERIT)){
				pr_acl = calculate_inherited_from_parent(new_sd,
							     p_acl,
							     is_container,
							     object_list);

				*new_acl = security_acl_concatenate(new_sd, tmp_acl, pr_acl);
				new_sd->type |= SEC_DESC_DACL_AUTO_INHERITED;
			}
			else if (acl_type == SEC_DESC_SACL_PRESENT &&
			    !(creator_sd->type & SECINFO_PROTECTED_SACL) &&
			    (inherit_flags & SEC_SACL_AUTO_INHERIT)){
				pr_acl = calculate_inherited_from_parent(new_sd,
							     p_acl,
							     is_container,
							     object_list);

				*new_acl = security_acl_concatenate(new_sd, tmp_acl, pr_acl);
				new_sd->type |= SEC_DESC_SACL_AUTO_INHERITED;
			}
		}
		if (*new_acl == NULL)
			goto final;
		if (!postprocess_acl(*new_acl, new_sd->owner_sid,
				     new_sd->group_sid,generic_map))
			return false;
		else
			goto final;
	}
	else{
		*new_acl = preprocess_creator_acl(new_sd,c_acl);
		if (*new_acl == NULL)
			goto final;
		if (!postprocess_acl(*new_acl, new_sd->owner_sid,
				     new_sd->group_sid,generic_map))
			return false;
		else
			goto final;
	}
final:
	if (acl_type == SEC_DESC_DACL_PRESENT && new_sd->dacl)
		new_sd->type |= SEC_DESC_DACL_PRESENT;

	if (acl_type == SEC_DESC_SACL_PRESENT && new_sd->sacl)
		new_sd->type |= SEC_DESC_SACL_PRESENT;
	/* This is a hack to handle the fact that
	 * apprantly any AI flag provided by the user is preserved */
	if (creator_sd)
		new_sd->type |= creator_sd->type;
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
			new_owner = token->user_sid;
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
		} else if (!default_group) {
			new_group = token->group_sid;
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

	if (!compute_acl(SEC_DESC_DACL_PRESENT, parent_sd, creator_sd,
			 is_container, inherit_flags, object_list,
			 generic_map,token,new_sd)){
		talloc_free(new_sd);
		return NULL;
	}

	if (!compute_acl(SEC_DESC_SACL_PRESENT, parent_sd, creator_sd,
			 is_container, inherit_flags, object_list,
			 generic_map, token,new_sd)){
		talloc_free(new_sd);
		return NULL;
	}

	return new_sd;
}
