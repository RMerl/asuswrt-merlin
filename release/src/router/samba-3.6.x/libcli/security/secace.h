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

#ifndef _ACE_H_
#define _ACE_H_

#include "librpc/gen_ndr/security.h"

bool sec_ace_object(uint8_t type);
void sec_ace_copy(struct security_ace *ace_dest, const struct security_ace *ace_src);
void init_sec_ace(struct security_ace *t, const struct dom_sid *sid, enum security_ace_type type,
		  uint32_t mask, uint8_t flag);
NTSTATUS sec_ace_add_sid(TALLOC_CTX *ctx, struct security_ace **pp_new, struct security_ace *old, unsigned *num, const struct dom_sid *sid, uint32_t mask);
NTSTATUS sec_ace_mod_sid(struct security_ace *ace, size_t num, const struct dom_sid *sid, uint32_t mask);
NTSTATUS sec_ace_del_sid(TALLOC_CTX *ctx, struct security_ace **pp_new, struct security_ace *old, uint32_t *num, const struct dom_sid *sid);
bool sec_ace_equal(const struct security_ace *s1, const struct security_ace *s2);
int nt_ace_inherit_comp( const struct security_ace *a1, const struct security_ace *a2);
int nt_ace_canon_comp( const struct security_ace *a1, const struct security_ace *a2);
void dacl_sort_into_canonical_order(struct security_ace *srclist, unsigned int num_aces);

#endif /*_ACE_H_*/

