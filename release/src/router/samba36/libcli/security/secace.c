/* 
 *  Unix SMB/Netbios implementation.
 *  struct security_ace handling functions
 *  Copyright (C) Andrew Tridgell              1992-1998,
 *  Copyright (C) Jeremy R. Allison            1995-2003.
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1998,
 *  Copyright (C) Paul Ashton                  1997-1998.
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
#include "librpc/gen_ndr/ndr_security.h"
#include "libcli/security/security.h"
#include "lib/util/tsort.h"

#define  SEC_ACE_HEADER_SIZE (2 * sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t))

/**
 * Check if ACE has OBJECT type.
 */
bool sec_ace_object(uint8_t type)
{
	if (type == SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT ||
            type == SEC_ACE_TYPE_ACCESS_DENIED_OBJECT ||
            type == SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT ||
            type == SEC_ACE_TYPE_SYSTEM_ALARM_OBJECT) {
		return true;
	}
	return false;
}

/**
 * copy a struct security_ace structure.
 */
void sec_ace_copy(struct security_ace *ace_dest, const struct security_ace *ace_src)
{
	ace_dest->type  = ace_src->type;
	ace_dest->flags = ace_src->flags;
	ace_dest->size  = ace_src->size;
	ace_dest->access_mask = ace_src->access_mask;
	ace_dest->object = ace_src->object;
	ace_dest->trustee = ace_src->trustee;
}

/*******************************************************************
 Sets up a struct security_ace structure.
********************************************************************/

void init_sec_ace(struct security_ace *t, const struct dom_sid *sid, enum security_ace_type type,
		  uint32_t mask, uint8_t flag)
{
	t->type = type;
	t->flags = flag;
	t->size = ndr_size_dom_sid(sid, 0) + 8;
	t->access_mask = mask;

	t->trustee = *sid;
}

/*******************************************************************
 adds new SID with its permissions to ACE list
********************************************************************/

NTSTATUS sec_ace_add_sid(TALLOC_CTX *ctx, struct security_ace **pp_new, struct security_ace *old, unsigned *num, const struct dom_sid *sid, uint32_t mask)
{
	unsigned int i = 0;
	
	if (!ctx || !pp_new || !old || !sid || !num)  return NT_STATUS_INVALID_PARAMETER;

	*num += 1;
	
	if((pp_new[0] = talloc_zero_array(ctx, struct security_ace, *num )) == 0)
		return NT_STATUS_NO_MEMORY;

	for (i = 0; i < *num - 1; i ++)
		sec_ace_copy(&(*pp_new)[i], &old[i]);

	(*pp_new)[i].type  = SEC_ACE_TYPE_ACCESS_ALLOWED;
	(*pp_new)[i].flags = 0;
	(*pp_new)[i].size  = SEC_ACE_HEADER_SIZE + ndr_size_dom_sid(sid, 0);
	(*pp_new)[i].access_mask = mask;
	(*pp_new)[i].trustee = *sid;
	return NT_STATUS_OK;
}

/*******************************************************************
  modify SID's permissions at ACL 
********************************************************************/

NTSTATUS sec_ace_mod_sid(struct security_ace *ace, size_t num, const struct dom_sid *sid, uint32_t mask)
{
	unsigned int i = 0;

	if (!ace || !sid)  return NT_STATUS_INVALID_PARAMETER;

	for (i = 0; i < num; i ++) {
		if (dom_sid_equal(&ace[i].trustee, sid)) {
			ace[i].access_mask = mask;
			return NT_STATUS_OK;
		}
	}
	return NT_STATUS_NOT_FOUND;
}

/*******************************************************************
 delete SID from ACL
********************************************************************/

NTSTATUS sec_ace_del_sid(TALLOC_CTX *ctx, struct security_ace **pp_new, struct security_ace *old, uint32_t *num, const struct dom_sid *sid)
{
	unsigned int i     = 0;
	unsigned int n_del = 0;

	if (!ctx || !pp_new || !old || !sid || !num)  return NT_STATUS_INVALID_PARAMETER;

	if (*num) {
		if((pp_new[0] = talloc_zero_array(ctx, struct security_ace, *num )) == 0)
			return NT_STATUS_NO_MEMORY;
	} else {
		pp_new[0] = NULL;
	}

	for (i = 0; i < *num; i ++) {
		if (!dom_sid_equal(&old[i].trustee, sid))
			sec_ace_copy(&(*pp_new)[i], &old[i]);
		else
			n_del ++;
	}
	if (n_del == 0)
		return NT_STATUS_NOT_FOUND;
	else {
		*num -= n_del;
		return NT_STATUS_OK;
	}
}

/*******************************************************************
 Compares two struct security_ace structures
********************************************************************/

bool sec_ace_equal(const struct security_ace *s1, const struct security_ace *s2)
{
	/* Trivial case */

	if (!s1 && !s2) {
		return true;
	}

	if (!s1 || !s2) {
		return false;
	}

	/* Check top level stuff */

	if (s1->type != s2->type || s1->flags != s2->flags ||
	    s1->access_mask != s2->access_mask) {
		return false;
	}

	/* Check SID */

	if (!dom_sid_equal(&s1->trustee, &s2->trustee)) {
		return false;
	}

	return true;
}

int nt_ace_inherit_comp(const struct security_ace *a1, const struct security_ace *a2)
{
	int a1_inh = a1->flags & SEC_ACE_FLAG_INHERITED_ACE;
	int a2_inh = a2->flags & SEC_ACE_FLAG_INHERITED_ACE;

	if (a1_inh == a2_inh)
		return 0;

	if (!a1_inh && a2_inh)
		return -1;
	return 1;
}

/*******************************************************************
  Comparison function to apply the order explained below in a group.
*******************************************************************/

int nt_ace_canon_comp( const struct security_ace *a1,  const struct security_ace *a2)
{
	if ((a1->type == SEC_ACE_TYPE_ACCESS_DENIED) &&
				(a2->type != SEC_ACE_TYPE_ACCESS_DENIED))
		return -1;

	if ((a2->type == SEC_ACE_TYPE_ACCESS_DENIED) &&
				(a1->type != SEC_ACE_TYPE_ACCESS_DENIED))
		return 1;

	/* Both access denied or access allowed. */

	/* 1. ACEs that apply to the object itself */

	if (!(a1->flags & SEC_ACE_FLAG_INHERIT_ONLY) &&
			(a2->flags & SEC_ACE_FLAG_INHERIT_ONLY))
		return -1;
	else if (!(a2->flags & SEC_ACE_FLAG_INHERIT_ONLY) &&
			(a1->flags & SEC_ACE_FLAG_INHERIT_ONLY))
		return 1;

	/* 2. ACEs that apply to a subobject of the object, such as
	 * a property set or property. */

	if (a1->flags & (SEC_ACE_FLAG_CONTAINER_INHERIT|SEC_ACE_FLAG_OBJECT_INHERIT) &&
			!(a2->flags & (SEC_ACE_FLAG_CONTAINER_INHERIT|SEC_ACE_FLAG_OBJECT_INHERIT)))
		return -1;
	else if (a2->flags & (SEC_ACE_FLAG_CONTAINER_INHERIT|SEC_ACE_FLAG_OBJECT_INHERIT) &&
			!(a1->flags & (SEC_ACE_FLAG_CONTAINER_INHERIT|SEC_ACE_FLAG_OBJECT_INHERIT)))
		return 1;

	return 0;
}

/*******************************************************************
 Functions to convert a SEC_DESC ACE DACL list into canonical order.
 JRA.

--- from http://msdn.microsoft.com/library/default.asp?url=/library/en-us/security/security/order_of_aces_in_a_dacl.asp

The following describes the preferred order:

 To ensure that noninherited ACEs have precedence over inherited ACEs,
 place all noninherited ACEs in a group before any inherited ACEs.
 This ordering ensures, for example, that a noninherited access-denied ACE
 is enforced regardless of any inherited ACE that allows access.

 Within the groups of noninherited ACEs and inherited ACEs, order ACEs according to ACE type, as the following shows:
	1. Access-denied ACEs that apply to the object itself
	2. Access-denied ACEs that apply to a subobject of the object, such as a property set or property
	3. Access-allowed ACEs that apply to the object itself
	4. Access-allowed ACEs that apply to a subobject of the object"

********************************************************************/

void dacl_sort_into_canonical_order(struct security_ace *srclist, unsigned int num_aces)
{
	unsigned int i;

	if (!srclist || num_aces == 0)
		return;

	/* Sort so that non-inherited ACE's come first. */
	TYPESAFE_QSORT(srclist, num_aces, nt_ace_inherit_comp);

	/* Find the boundary between non-inherited ACEs. */
	for (i = 0; i < num_aces; i++ ) {
		struct security_ace *curr_ace = &srclist[i];

		if (curr_ace->flags & SEC_ACE_FLAG_INHERITED_ACE)
			break;
	}

	/* i now points at entry number of the first inherited ACE. */

	/* Sort the non-inherited ACEs. */
	TYPESAFE_QSORT(srclist, i, nt_ace_canon_comp);

	/* Now sort the inherited ACEs. */
	TYPESAFE_QSORT(&srclist[i], num_aces - i, nt_ace_canon_comp);
}


