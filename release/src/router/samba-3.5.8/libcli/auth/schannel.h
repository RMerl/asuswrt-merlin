/*
   Unix SMB/CIFS implementation.

   dcerpc schannel operations

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2005

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

#include "libcli/auth/libcli_auth.h"
#include "libcli/auth/schannel_state.h"

enum schannel_position {
	SCHANNEL_STATE_START = 0,
	SCHANNEL_STATE_UPDATE_1
};

struct schannel_state {
	enum schannel_position state;
	uint32_t seq_num;
	bool initiator;
	struct netlogon_creds_CredentialState *creds;
};

#include "libcli/auth/schannel_proto.h"
