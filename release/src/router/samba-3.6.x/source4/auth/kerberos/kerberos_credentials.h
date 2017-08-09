/*
   Unix SMB/CIFS implementation.

   Kerberos utility functions for GENSEC

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2010

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

krb5_error_code kinit_to_ccache(TALLOC_CTX *parent_ctx,
				struct cli_credentials *credentials,
				struct smb_krb5_context *smb_krb5_context,
				struct tevent_context *event_ctx,
				krb5_ccache ccache,
				enum credentials_obtained *obtained,
				const char **error_string);
