/* 
   Unix SMB/CIFS implementation.

   SMB2 client calls 

   Copyright (C) Andrew Tridgell 2005
   
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

#include "libcli/raw/interfaces.h"

struct smb2_negprot {
	struct {
		uint16_t dialect_count;    /* size of dialects array */
		uint16_t security_mode;    /* 0==signing disabled   
					      1==signing enabled */
		uint16_t reserved;
		uint32_t capabilities;
		struct GUID client_guid;
		NTTIME   start_time;
		uint16_t *dialects;
	} in;
	struct {
		/* static body buffer 64 (0x40) bytes */
		/* uint16_t buffer_code;  0x41 = 0x40 + 1 */
		uint16_t security_mode; /* SMB2_NEGOTIATE_SIGNING_* */
		uint16_t dialect_revision;
		uint16_t reserved;
		struct GUID server_guid;
		uint32_t capabilities;
		uint32_t max_transact_size;
		uint32_t max_read_size;
		uint32_t max_write_size;
		NTTIME   system_time;
		NTTIME   server_start_time;
		/* uint16_t secblob_ofs */
		/* uint16_t secblob_size */
		uint32_t reserved2;
		DATA_BLOB secblob;
	} out;
};

/* NOTE! the getinfo fs and file levels exactly match up with the
   'passthru' SMB levels, which are levels >= 1000. The SMB2 client
   lib uses the names from the libcli/raw/ library */

struct smb2_getinfo {
	struct {
		/* static body buffer 40 (0x28) bytes */
		/* uint16_t buffer_code;  0x29 = 0x28 + 1 */
		uint8_t info_type;
		uint8_t info_class;
		uint32_t output_buffer_length;
		/* uint32_t input_buffer_offset; */
		uint32_t reserved;
		uint32_t input_buffer_length;
		uint32_t additional_information; /* SMB2_GETINFO_ADD_* */
		uint32_t getinfo_flags; /* level specific */
		union smb_handle file;
		DATA_BLOB blob;
	} in;

	struct {
		/* static body buffer 8 (0x08) bytes */
		/* uint16_t buffer_code; 0x09 = 0x08 + 1 */
		/* uint16_t blob_ofs; */
		/* uint16_t blob_size; */

		/* dynamic body */
		DATA_BLOB blob;
	} out;
};

struct smb2_setinfo {
	struct {
		uint16_t level;
		uint32_t flags;
		union smb_handle file;
		DATA_BLOB blob;
	} in;
};

struct cli_credentials;
struct tevent_context;
struct resolve_context;
struct gensec_settings;
#include "libcli/smb2/smb2_proto.h"
