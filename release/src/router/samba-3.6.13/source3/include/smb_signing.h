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

#ifndef _SMB_SIGNING_H_
#define _SMB_SIGNING_H_

struct smb_signing_state;

struct smb_signing_state *smb_signing_init(TALLOC_CTX *mem_ctx,
					   bool allowed,
					   bool mandatory);
struct smb_signing_state *smb_signing_init_ex(TALLOC_CTX *mem_ctx,
					      bool allowed,
					      bool mandatory,
					      void *(*alloc_fn)(TALLOC_CTX *, size_t),
					      void (*free_fn)(TALLOC_CTX *, void *));
uint32_t smb_signing_next_seqnum(struct smb_signing_state *si, bool oneway);
void smb_signing_cancel_reply(struct smb_signing_state *si, bool oneway);
void smb_signing_sign_pdu(struct smb_signing_state *si,
			  uint8_t *outbuf, uint32_t seqnum);
bool smb_signing_check_pdu(struct smb_signing_state *si,
			   const uint8_t *inbuf, uint32_t seqnum);
bool smb_signing_set_bsrspyl(struct smb_signing_state *si);
bool smb_signing_activate(struct smb_signing_state *si,
			  const DATA_BLOB user_session_key,
			  const DATA_BLOB response);
bool smb_signing_is_active(struct smb_signing_state *si);
bool smb_signing_is_allowed(struct smb_signing_state *si);
bool smb_signing_is_mandatory(struct smb_signing_state *si);
bool smb_signing_set_negotiated(struct smb_signing_state *si);
bool smb_signing_is_negotiated(struct smb_signing_state *si);

#endif /* _SMB_SIGNING_H_ */
