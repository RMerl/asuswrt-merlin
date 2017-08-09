/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Jelmer Vernooij 2005

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
#include "system/filesys.h"
#include "auth/credentials/credentials.h"
#include "lib/cmdline/credentials.h"

static const char *cmdline_get_userpassword(struct cli_credentials *credentials)
{
	char *ret;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);

	const char *prompt_name = cli_credentials_get_unparsed_name(credentials, mem_ctx);
	const char *prompt;

	prompt = talloc_asprintf(mem_ctx, "Password for [%s]:", 
				 prompt_name);

	ret = getpass(prompt);

	talloc_free(mem_ctx);
	return ret;
}

bool cli_credentials_set_cmdline_callbacks(struct cli_credentials *cred)
{
	if (isatty(fileno(stdout))) {
		cli_credentials_set_password_callback(cred, cmdline_get_userpassword);
		return true;
	}

	return false;
}
