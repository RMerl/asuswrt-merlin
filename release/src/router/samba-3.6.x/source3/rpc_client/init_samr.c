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
#include "../lib/crypto/md5.h"
#include "../lib/crypto/arcfour.h"
#include "rpc_client/init_samr.h"

/*************************************************************************
 inits a samr_CryptPasswordEx structure
 *************************************************************************/

void init_samr_CryptPasswordEx(const char *pwd,
			       DATA_BLOB *session_key,
			       struct samr_CryptPasswordEx *pwd_buf)
{
	/* samr_CryptPasswordEx */

	uchar pwbuf[532];
	MD5_CTX md5_ctx;
	uint8_t confounder[16];
	DATA_BLOB confounded_session_key = data_blob(NULL, 16);

	encode_pw_buffer(pwbuf, pwd, STR_UNICODE);

	generate_random_buffer((uint8_t *)confounder, 16);

	MD5Init(&md5_ctx);
	MD5Update(&md5_ctx, confounder, 16);
	MD5Update(&md5_ctx, session_key->data,
			    session_key->length);
	MD5Final(confounded_session_key.data, &md5_ctx);

	arcfour_crypt_blob(pwbuf, 516, &confounded_session_key);
	memcpy(&pwbuf[516], confounder, 16);

	memcpy(pwd_buf->data, pwbuf, sizeof(pwbuf));
	data_blob_free(&confounded_session_key);
}

/*************************************************************************
 inits a samr_CryptPassword structure
 *************************************************************************/

void init_samr_CryptPassword(const char *pwd,
			     DATA_BLOB *session_key,
			     struct samr_CryptPassword *pwd_buf)
{
	/* samr_CryptPassword */

	encode_pw_buffer(pwd_buf->data, pwd, STR_UNICODE);
	arcfour_crypt_blob(pwd_buf->data, 516, session_key);
}
