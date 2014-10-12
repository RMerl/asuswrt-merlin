/* 
   Unix SMB/CIFS implementation.
   Samba3 interfaces
   Copyright (C) Jelmer Vernooij			2005.
   
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

#ifndef _SAMBA3_H /* _SAMBA3_H */
#define _SAMBA3_H 

#include "librpc/gen_ndr/security.h"
#include "librpc/gen_ndr/samr.h"

struct samr_Password *smbpasswd_gethexpwd(TALLOC_CTX *mem_ctx, const char *p);
char *smbpasswd_sethexpwd(TALLOC_CTX *mem_ctx, struct samr_Password *pwd, uint16_t acb_info);
uint16_t smbpasswd_decode_acb_info(const char *p);
char *smbpasswd_encode_acb_info(TALLOC_CTX *mem_ctx, uint16_t acb_info);

#endif /* _SAMBA3_H */
