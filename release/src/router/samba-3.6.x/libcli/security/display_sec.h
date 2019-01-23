/*
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1999
   Copyright (C) Luke Kenneth Casson Leighton 1996 - 1999

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

#ifndef _LIBCLI_SECURITY_DISPLAY_SEC_H
#define _LIBCLI_SECURITY_DISPLAY_SEC_H

/* The following definitions come from libcli/security/display_sec.c */

char *get_sec_mask_str(TALLOC_CTX *ctx, uint32_t type);
void display_sec_access(uint32_t *info);
void display_sec_ace_flags(uint8_t flags);
void display_sec_ace(struct security_ace *ace);
void display_sec_acl(struct security_acl *sec_acl);
void display_acl_type(uint16_t type);
void display_sec_desc(struct security_descriptor *sec);

#endif /* _LIBCLI_SECURITY_DISPLAY_SEC_H */
