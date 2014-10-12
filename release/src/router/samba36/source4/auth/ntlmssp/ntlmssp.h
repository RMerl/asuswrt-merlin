/*
   Unix SMB/CIFS implementation.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell 1992-1997
   Copyright (C) Luke Kenneth Casson Leighton 1996-1997
   Copyright (C) Paul Ashton 1997
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

#include "../librpc/gen_ndr/ntlmssp.h"
#include "../libcli/auth/ntlmssp.h"

struct gensec_ntlmssp_context {
	struct gensec_security *gensec_security;
	struct ntlmssp_state *ntlmssp_state;
	struct auth_context *auth_context;
	struct auth_user_info_dc *user_info_dc;
};

struct loadparm_context;
struct auth_session_info;

NTSTATUS gensec_ntlmssp_init(void);

#include "auth/ntlmssp/proto.h"
