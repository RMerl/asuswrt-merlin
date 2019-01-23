/*
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Gerald Carter 2005
   Copyright (C) Volker Lendecke 2007
   Copyright (C) Jeremy Allison 2008
   Copyright (C) Andrew Bartlett 2010

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
#ifndef _ACCESS_CHECK_H_
#define _ACCESS_CHECK_H_

#include "librpc/gen_ndr/security.h"

/* Map generic access rights to object specific rights.  This technique is
   used to give meaning to assigning read, write, execute and all access to
   objects.  Each type of object has its own mapping of generic to object
   specific access rights. */

void se_map_generic(uint32_t *access_mask, const struct generic_mapping *mapping);

/* Map generic access rights to object specific rights for all the ACE's
 * in a security_acl.
 */
void security_acl_map_generic(struct security_acl *sa,
			      const struct generic_mapping *mapping);

/* Map standard access rights to object specific rights.  This technique is
   used to give meaning to assigning read, write, execute and all access to
   objects.  Each type of object has its own mapping of standard to object
   specific access rights. */
void se_map_standard(uint32_t *access_mask, const struct standard_mapping *mapping);

/*
  The main entry point for access checking. If returning ACCESS_DENIED
  this function returns the denied bits in the uint32_t pointed
  to by the access_granted pointer.
*/
NTSTATUS se_access_check(const struct security_descriptor *sd,
			 const struct security_token *token,
			 uint32_t access_desired,
			 uint32_t *access_granted);

/* modified access check for the purposes of DS security
 * Lots of code duplication, it will ve united in just one
 * function eventually */

NTSTATUS sec_access_check_ds(const struct security_descriptor *sd,
			     const struct security_token *token,
			     uint32_t access_desired,
			     uint32_t *access_granted,
			     struct object_tree *tree,
			     struct dom_sid *replace_sid);

bool insert_in_object_tree(TALLOC_CTX *mem_ctx,
			  const struct GUID *guid,
			  uint32_t init_access,
			  struct object_tree **root,
			   struct object_tree **new_node);

/* search by GUID */
struct object_tree *get_object_tree_by_GUID(struct object_tree *root,
					    const struct GUID *guid);

/* Change the granted access per each ACE */
void object_tree_modify_access(struct object_tree *root,
			       uint32_t access_mask);
#endif
