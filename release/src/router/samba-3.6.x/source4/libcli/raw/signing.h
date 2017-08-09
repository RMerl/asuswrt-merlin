#ifndef _SIGNING_H
#define _SIGNING_H
/*
   Unix SMB/CIFS implementation.
   SMB Signing

   Andrew Bartlett <abartlet@samba.org> 2003-2004

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

enum smb_signing_engine_state {
	SMB_SIGNING_ENGINE_OFF,
	SMB_SIGNING_ENGINE_BSRSPYL,
	SMB_SIGNING_ENGINE_ON
};

enum smb_signing_state {
	SMB_SIGNING_OFF, SMB_SIGNING_SUPPORTED, 
	SMB_SIGNING_REQUIRED, SMB_SIGNING_AUTO};

struct smb_signing_context {
	enum smb_signing_engine_state signing_state;
	DATA_BLOB mac_key;
	uint32_t next_seq_num;
	bool allow_smb_signing;
	bool doing_signing;
	bool mandatory_signing;
	bool seen_valid; /* Have I ever seen a validly signed packet? */
};

#endif
