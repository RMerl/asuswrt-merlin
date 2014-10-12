/* 
   Unix SMB/CIFS implementation.
   client string routines
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2003

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

size_t clistr_push_fn(const char *function,
			unsigned int line,
			struct cli_state *cli,
			void *dest,
			const char *src,
			int dest_len,
			int flags)
{
	size_t buf_used = PTR_DIFF(dest, cli->outbuf);
	if (dest_len == -1) {
		if (((ptrdiff_t)dest < (ptrdiff_t)cli->outbuf) || (buf_used > cli->bufsize)) {
			DEBUG(0, ("Pushing string of 'unlimited' length into non-SMB buffer!\n"));
			return push_string_base(function, line,
						cli->outbuf,
						(uint16_t)(cli_ucs2(cli) ? FLAGS2_UNICODE_STRINGS : 0),
						dest, src, -1, flags);
		}
		return push_string_base(function, line, 
					cli->outbuf,
					(uint16_t)(cli_ucs2(cli) ? FLAGS2_UNICODE_STRINGS : 0),
					dest, src, cli->bufsize - buf_used,
					flags);
	}

	/* 'normal' push into size-specified buffer */
	return push_string_base(function, line, 
				cli->outbuf,
				(uint16_t)(cli_ucs2(cli) ? FLAGS2_UNICODE_STRINGS : 0),
				dest, src, dest_len, flags);
}

size_t clistr_pull_fn(const char *function,
			unsigned int line,
			const char *inbuf,
			char *dest,
			const void *src,
			int dest_len,
			int src_len,
			int flags)
{
	return pull_string_fn(function, line, inbuf,
			      SVAL(inbuf, smb_flg2), dest, src, dest_len,
			      src_len, flags);
}

size_t clistr_pull_talloc_fn(const char *function,
				unsigned int line,
				TALLOC_CTX *ctx,
				const char *base,
				uint16_t flags2,
				char **pp_dest,
				const void *src,
				int src_len,
				int flags)
{
	return pull_string_talloc_fn(function,
					line,
					ctx,
					base,
					flags2,
					pp_dest,
					src,
					src_len,
					flags);
}

size_t clistr_align_out(struct cli_state *cli, const void *p, int flags)
{
	return align_string(cli->outbuf, (const char *)p, flags);
}
