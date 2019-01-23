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

#ifndef __SDDL_H__
#define __SDDL_H__

#include "librpc/gen_ndr/security.h"

struct security_descriptor *sddl_decode(TALLOC_CTX *mem_ctx, const char *sddl,
					const struct dom_sid *domain_sid);
char *sddl_encode(TALLOC_CTX *mem_ctx, const struct security_descriptor *sd,
		  const struct dom_sid *domain_sid);


#endif /* __SDDL_H__ */
