/* 
   Unix SMB/CIFS implementation.
   server specific string routines
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
#include "smbd/smbd.h"
#include "smbd/globals.h"

/* Make sure we can't write a string past the end of the buffer */

size_t srvstr_push_fn(const char *function, unsigned int line,
		      const char *base_ptr, uint16 smb_flags2, void *dest,
		      const char *src, int dest_len, int flags)
{
	if (dest_len < 0) {
		return 0;
	}

	/* 'normal' push into size-specified buffer */
	return push_string_base(function, line, base_ptr, smb_flags2, dest, src,
				dest_len, flags);
}

/*******************************************************************
 Add a string to the end of a smb_buf, adjusting bcc and smb_len.
 Return the bytes added
********************************************************************/

ssize_t message_push_string(uint8 **outbuf, const char *str, int flags)
{
	size_t buf_size = smb_len(*outbuf) + 4;
	size_t grow_size;
	size_t result;
	uint8 *tmp;

	/*
	 * We need to over-allocate, now knowing what srvstr_push will
	 * actually use. This is very generous by incorporating potential
	 * padding, the terminating 0 and at most 4 chars per UTF-16 code
	 * point.
	 */
	grow_size = (strlen(str) + 2) * 4;

	if (!(tmp = TALLOC_REALLOC_ARRAY(NULL, *outbuf, uint8,
					 buf_size + grow_size))) {
		DEBUG(0, ("talloc failed\n"));
		return -1;
	}

	result = srvstr_push((char *)tmp, SVAL(tmp, smb_flg2),
			     tmp + buf_size, str, grow_size, flags);

	if (result == (size_t)-1) {
		DEBUG(0, ("srvstr_push failed\n"));
		return -1;
	}

	/*
	 * Ensure we clear out the extra data we have
	 * grown the buffer by, but not written to.
	 */
	if (buf_size + result < buf_size) {
		return -1;
	}
	if (grow_size < result) {
		return -1;
	}

	memset(tmp + buf_size + result, '\0', grow_size - result);

	set_message_bcc((char *)tmp, smb_buflen(tmp) + result);

	*outbuf = tmp;

	return result;
}
