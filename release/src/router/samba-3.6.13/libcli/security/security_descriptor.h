/*
   Unix SMB/CIFS implementation.
   Samba utility functions

   Copyright (C) 2009 Jelmer Vernooij <jelmer@samba.org>

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

#ifndef __SECURITY_DESCRIPTOR_H__
#define __SECURITY_DESCRIPTOR_H__

#include "librpc/gen_ndr/security.h"

struct security_descriptor *security_descriptor_initialise(TALLOC_CTX *mem_ctx);
struct security_descriptor *security_descriptor_copy(TALLOC_CTX *mem_ctx, 
						     const struct security_descriptor *osd);
NTSTATUS security_descriptor_sacl_add(struct security_descriptor *sd,
				      const struct security_ace *ace);
NTSTATUS security_descriptor_dacl_add(struct security_descriptor *sd,
				      const struct security_ace *ace);
NTSTATUS security_descriptor_dacl_del(struct security_descriptor *sd,
				      const struct dom_sid *trustee);
NTSTATUS security_descriptor_sacl_del(struct security_descriptor *sd,
				      const struct dom_sid *trustee);
bool security_ace_equal(const struct security_ace *ace1, 
			const struct security_ace *ace2);
bool security_acl_equal(const struct security_acl *acl1, 
			const struct security_acl *acl2);
bool security_descriptor_equal(const struct security_descriptor *sd1, 
			       const struct security_descriptor *sd2);
bool security_descriptor_mask_equal(const struct security_descriptor *sd1, 
				    const struct security_descriptor *sd2, 
				    uint32_t mask);
struct security_descriptor *security_descriptor_append(struct security_descriptor *sd,
						       ...);
struct security_descriptor *security_descriptor_dacl_create(TALLOC_CTX *mem_ctx,
							    uint16_t sd_type,
							    const char *owner_sid,
							    const char *group_sid,
							    ...);
struct security_descriptor *security_descriptor_sacl_create(TALLOC_CTX *mem_ctx,
							    uint16_t sd_type,
							    const char *owner_sid,
							    const char *group_sid,
							    ...);
struct security_ace *security_ace_create(TALLOC_CTX *mem_ctx,
					 const char *sid_str,
					 enum security_ace_type type,
					 uint32_t access_mask,
					 uint8_t flags);

struct security_acl *security_acl_dup(TALLOC_CTX *mem_ctx,
				      const struct security_acl *oacl);

struct security_acl *security_acl_concatenate(TALLOC_CTX *mem_ctx,
                                              const struct security_acl *acl1,
                                              const struct security_acl *acl2);

uint32_t map_generic_rights_ds(uint32_t access_mask);

struct security_descriptor *create_security_descriptor(TALLOC_CTX *mem_ctx,
						       struct security_descriptor *parent_sd,
						       struct security_descriptor *creator_sd,
						       bool is_container,
						       struct GUID *object_list,
						       uint32_t inherit_flags,
						       struct security_token *token,
						       struct dom_sid *default_owner, /* valid only for DS, NULL for the other RSs */
						       struct dom_sid *default_group, /* valid only for DS, NULL for the other RSs */
						       uint32_t (*generic_map)(uint32_t access_mask));

#endif /* __SECURITY_DESCRIPTOR_H__ */
