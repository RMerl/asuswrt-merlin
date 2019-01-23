/*
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2008-2009
   Copyright (C) Sumit Bose <sbose@redhat.com> 2010

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

bool test_SetupCredentials2(struct dcerpc_pipe *p, struct torture_context *tctx,
			    uint32_t negotiate_flags,
			    struct cli_credentials *machine_credentials,
			    int sec_chan_type,
			    struct netlogon_creds_CredentialState **creds_out);

bool test_SetupCredentials3(struct dcerpc_pipe *p, struct torture_context *tctx,
			    uint32_t negotiate_flags,
			    struct cli_credentials *machine_credentials,
			    struct netlogon_creds_CredentialState **creds_out);
