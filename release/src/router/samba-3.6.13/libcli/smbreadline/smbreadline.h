/*
   Unix SMB/CIFS implementation.
   Samba readline wrapper implementation
   Copyright (C) Simo Sorce 2001
   Copyright (C) Andrew Tridgell 2001

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

#ifndef __SMBREADLINE_H__
#define __SMBREADLINE_H__

char *smb_readline(const char *prompt, void (*callback)(void),
		   char **(completion_fn)(const char *text, int start, int end));
const char *smb_readline_get_line_buffer(void);
void smb_readline_ca_char(char c);
void smb_readline_done(void);

#endif /* __SMBREADLINE_H__ */
