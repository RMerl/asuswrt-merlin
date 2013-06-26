/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Guenther Deschner                  2008.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "../libcli/auth/libcli_auth.h"
#include "../lib/crypto/arcfour.h"
#include "rpc_client/init_netlogon.h"

/*************************************************************************
 inits a netr_CryptPassword structure
 *************************************************************************/

void init_netr_CryptPassword(const char *pwd,
			     unsigned char session_key[16],
			     struct netr_CryptPassword *pwd_buf)
{
	struct samr_CryptPassword password_buf;

	encode_pw_buffer(password_buf.data, pwd, STR_UNICODE);

	arcfour_crypt(password_buf.data, session_key, 516);
	memcpy(pwd_buf->data, password_buf.data, 512);
	pwd_buf->length = IVAL(password_buf.data, 512);
}
