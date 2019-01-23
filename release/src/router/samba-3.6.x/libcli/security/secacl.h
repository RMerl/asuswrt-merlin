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

#ifndef _SECACL_H_
#define _SECACL_H_

#include "librpc/gen_ndr/security.h"

struct security_acl *make_sec_acl(TALLOC_CTX *ctx, enum security_acl_revision revision,
		      int num_aces, struct security_ace *ace_list);
struct security_acl *dup_sec_acl(TALLOC_CTX *ctx, struct security_acl *src);


#endif /*_SECACL_H_*/

