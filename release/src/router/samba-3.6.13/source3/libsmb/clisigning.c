/*
   Unix SMB/CIFS implementation.
   SMB Signing Code
   Copyright (C) Jeremy Allison 2003.
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2002-2003
   Copyright (C) Stefan Metzmacher 2009

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

#include "includes.h"
#include "libsmb/libsmb.h"
#include "smb_signing.h"

bool cli_simple_set_signing(struct cli_state *cli,
			    const DATA_BLOB user_session_key,
			    const DATA_BLOB response)
{
	bool ok;

	ok = smb_signing_activate(cli->signing_state,
				  user_session_key,
				  response);
	if (!ok) {
		return false;
	}

	cli->readbraw_supported = false;
	cli->writebraw_supported = false;

	return true;
}

bool cli_temp_set_signing(struct cli_state *cli)
{
	return smb_signing_set_bsrspyl(cli->signing_state);
}

void cli_calculate_sign_mac(struct cli_state *cli, char *buf, uint32_t *seqnum)
{
	*seqnum = smb_signing_next_seqnum(cli->signing_state, false);
	smb_signing_sign_pdu(cli->signing_state, (uint8_t *)buf, *seqnum);
}

bool cli_check_sign_mac(struct cli_state *cli, const char *buf, uint32_t seqnum)
{
	bool ok;

	ok = smb_signing_check_pdu(cli->signing_state,
				   (const uint8_t *)buf,
				   seqnum);

	if (!ok) {
		return false;
	}

	return true;
}

void cli_set_signing_negotiated(struct cli_state *cli)
{
	smb_signing_set_negotiated(cli->signing_state);
}

bool client_is_signing_on(struct cli_state *cli)
{
	return smb_signing_is_active(cli->signing_state);
}

bool client_is_signing_allowed(struct cli_state *cli)
{
	return smb_signing_is_allowed(cli->signing_state);
}

bool client_is_signing_mandatory(struct cli_state *cli)
{
	return smb_signing_is_mandatory(cli->signing_state);
}
