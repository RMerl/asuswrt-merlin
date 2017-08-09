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
#include "../lib/crypto/md5.h"
#include "smb_signing.h"

/* Used by the SMB signing functions. */

struct smb_signing_state {
	/* is signing localy allowed */
	bool allowed;

	/* is signing localy mandatory */
	bool mandatory;

	/* is signing negotiated by the peer */
	bool negotiated;

	/* send BSRSPYL signatures */
	bool bsrspyl;

	bool active; /* Have I ever seen a validly signed packet? */

	/* mac_key.length > 0 means signing is started */
	DATA_BLOB mac_key;

	/* the next expected seqnum */
	uint32_t seqnum;

	TALLOC_CTX *mem_ctx;
	void *(*alloc_fn)(TALLOC_CTX *mem_ctx, size_t len);
	void (*free_fn)(TALLOC_CTX *mem_ctx, void *ptr);
};

static void smb_signing_reset_info(struct smb_signing_state *si)
{
	si->active = false;
	si->bsrspyl = false;
	si->seqnum = 0;

	if (si->free_fn) {
		si->free_fn(si->mem_ctx, si->mac_key.data);
	} else {
		talloc_free(si->mac_key.data);
	}
	si->mac_key.data = NULL;
	si->mac_key.length = 0;
}

struct smb_signing_state *smb_signing_init_ex(TALLOC_CTX *mem_ctx,
					      bool allowed,
					      bool mandatory,
					      void *(*alloc_fn)(TALLOC_CTX *, size_t),
					      void (*free_fn)(TALLOC_CTX *, void *))
{
	struct smb_signing_state *si;

	if (alloc_fn) {
		void *p = alloc_fn(mem_ctx, sizeof(struct smb_signing_state));
		if (p == NULL) {
			return NULL;
		}
		memset(p, 0, sizeof(struct smb_signing_state));
		si = (struct smb_signing_state *)p;
		si->mem_ctx = mem_ctx;
		si->alloc_fn = alloc_fn;
		si->free_fn = free_fn;
	} else {
		si = talloc_zero(mem_ctx, struct smb_signing_state);
		if (si == NULL) {
			return NULL;
		}
	}

	if (mandatory) {
		allowed = true;
	}

	si->allowed = allowed;
	si->mandatory = mandatory;

	return si;
}

struct smb_signing_state *smb_signing_init(TALLOC_CTX *mem_ctx,
					   bool allowed,
					   bool mandatory)
{
	return smb_signing_init_ex(mem_ctx, allowed, mandatory, NULL, NULL);
}

static bool smb_signing_good(struct smb_signing_state *si,
			     bool good, uint32_t seq)
{
	if (good) {
		if (!si->active) {
			si->active = true;
		}
		return true;
	}

	if (!si->mandatory && !si->active) {
		/* Non-mandatory signing - just turn off if this is the first bad packet.. */
		DEBUG(5, ("smb_signing_good: signing negotiated but not required and peer\n"
			  "isn't sending correct signatures. Turning off.\n"));
		smb_signing_reset_info(si);
		return true;
	}

	/* Mandatory signing or bad packet after signing started - fail and disconnect. */
	DEBUG(0, ("smb_signing_good: BAD SIG: seq %u\n", (unsigned int)seq));
	return false;
}

static void smb_signing_md5(const DATA_BLOB *mac_key,
			    const uint8_t *buf, uint32_t seq_number,
			    uint8_t calc_md5_mac[16])
{
	const size_t offset_end_of_sig = (smb_ss_field + 8);
	uint8_t sequence_buf[8];
	MD5_CTX md5_ctx;

	/*
	 * Firstly put the sequence number into the first 4 bytes.
	 * and zero out the next 4 bytes.
	 *
	 * We do this here, to avoid modifying the packet.
	 */

	DEBUG(10,("smb_signing_md5: sequence number %u\n", seq_number ));

	SIVAL(sequence_buf, 0, seq_number);
	SIVAL(sequence_buf, 4, 0);

	/* Calculate the 16 byte MAC - but don't alter the data in the
	   incoming packet.

	   This makes for a bit of fussing about, but it's not too bad.
	*/
	MD5Init(&md5_ctx);

	/* intialise with the key */
	MD5Update(&md5_ctx, mac_key->data, mac_key->length);

	/* copy in the first bit of the SMB header */
	MD5Update(&md5_ctx, buf + 4, smb_ss_field - 4);

	/* copy in the sequence number, instead of the signature */
	MD5Update(&md5_ctx, sequence_buf, sizeof(sequence_buf));

	/* copy in the rest of the packet in, skipping the signature */
	MD5Update(&md5_ctx, buf + offset_end_of_sig, 
		  smb_len(buf) - (offset_end_of_sig - 4));

	/* calculate the MD5 sig */
	MD5Final(calc_md5_mac, &md5_ctx);
}

uint32_t smb_signing_next_seqnum(struct smb_signing_state *si, bool oneway)
{
	uint32_t seqnum;

	if (si->mac_key.length == 0) {
		return 0;
	}

	seqnum = si->seqnum;
	if (oneway) {
		si->seqnum += 1;
	} else {
		si->seqnum += 2;
	}

	return seqnum;
}

void smb_signing_cancel_reply(struct smb_signing_state *si, bool oneway)
{
	if (si->mac_key.length == 0) {
		return;
	}

	if (oneway) {
		si->seqnum -= 1;
	} else {
		si->seqnum -= 2;
	}
}

void smb_signing_sign_pdu(struct smb_signing_state *si,
			  uint8_t *outbuf, uint32_t seqnum)
{
	uint8_t calc_md5_mac[16];
	uint16_t flags2;

	if (si->mac_key.length == 0) {
		if (!si->bsrspyl) {
			return;
		}
	}

	/* JRA Paranioa test - we should be able to get rid of this... */
	if (smb_len(outbuf) < (smb_ss_field + 8 - 4)) {
		DEBUG(1,("smb_signing_sign_pdu: Logic error. "
			 "Can't check signature on short packet! smb_len = %u\n",
			 smb_len(outbuf)));
		abort();
	}

	/* mark the packet as signed - BEFORE we sign it...*/
	flags2 = SVAL(outbuf,smb_flg2);
	flags2 |= FLAGS2_SMB_SECURITY_SIGNATURES;
	SSVAL(outbuf, smb_flg2, flags2);

	if (si->bsrspyl) {
		/* I wonder what BSRSPYL stands for - but this is what MS
		   actually sends! */
		memcpy(calc_md5_mac, "BSRSPYL ", 8);
	} else {
		smb_signing_md5(&si->mac_key, outbuf,
				seqnum, calc_md5_mac);
	}

	DEBUG(10, ("smb_signing_sign_pdu: sent SMB signature of\n"));
	dump_data(10, calc_md5_mac, 8);

	memcpy(&outbuf[smb_ss_field], calc_md5_mac, 8);

/*	outbuf[smb_ss_field+2]=0;
	Uncomment this to test if the remote server actually verifies signatures...*/
}

bool smb_signing_check_pdu(struct smb_signing_state *si,
			   const uint8_t *inbuf, uint32_t seqnum)
{
	bool good;
	uint8_t calc_md5_mac[16];
	const uint8_t *reply_sent_mac;

	if (si->mac_key.length == 0) {
		return true;
	}

	if (smb_len(inbuf) < (smb_ss_field + 8 - 4)) {
		DEBUG(1,("smb_signing_check_pdu: Can't check signature "
			 "on short packet! smb_len = %u\n",
			 smb_len(inbuf)));
		return False;
	}

	smb_signing_md5(&si->mac_key, inbuf,
			seqnum, calc_md5_mac);

	reply_sent_mac = &inbuf[smb_ss_field];
	good = (memcmp(reply_sent_mac, calc_md5_mac, 8) == 0);

	if (!good) {
		int i;
		const int sign_range = 5;

		DEBUG(5, ("smb_signing_check_pdu: BAD SIG: wanted SMB signature of\n"));
		dump_data(5, calc_md5_mac, 8);

		DEBUG(5, ("smb_signing_check_pdu: BAD SIG: got SMB signature of\n"));
		dump_data(5, reply_sent_mac, 8);

		for (i = -sign_range; i < sign_range; i++) {
			smb_signing_md5(&si->mac_key, inbuf,
					seqnum+i, calc_md5_mac);
			if (memcmp(reply_sent_mac, calc_md5_mac, 8) == 0) {
				DEBUG(0,("smb_signing_check_pdu: "
					 "out of seq. seq num %u matches. "
					 "We were expecting seq %u\n",
					 (unsigned int)seqnum+i,
					 (unsigned int)seqnum));
				break;
			}
		}
	} else {
		DEBUG(10, ("smb_signing_check_pdu: seq %u: "
			   "got good SMB signature of\n",
			   (unsigned int)seqnum));
		dump_data(10, reply_sent_mac, 8);
	}

	return smb_signing_good(si, good, seqnum);
}

bool smb_signing_set_bsrspyl(struct smb_signing_state *si)
{
	if (!si->negotiated) {
		return false;
	}

	if (si->active) {
		return false;
	}

	si->bsrspyl = true;

	return true;
}

bool smb_signing_activate(struct smb_signing_state *si,
			  const DATA_BLOB user_session_key,
			  const DATA_BLOB response)
{
	size_t len;
	off_t ofs;

	if (!user_session_key.length) {
		return false;
	}

	if (!si->negotiated) {
		return false;
	}

	if (si->active) {
		return false;
	}

	if (si->mac_key.length > 0) {
		return false;
	}

	smb_signing_reset_info(si);

	len = response.length + user_session_key.length;
	if (si->alloc_fn) {
		si->mac_key.data = (uint8_t *)si->alloc_fn(si->mem_ctx, len);
		if (si->mac_key.data == NULL) {
			return false;
		}
	} else {
		si->mac_key.data = (uint8_t *)talloc_size(si, len);
		if (si->mac_key.data == NULL) {
			return false;
		}
	}
	si->mac_key.length = len;

	ofs = 0;
	memcpy(&si->mac_key.data[ofs], user_session_key.data, user_session_key.length);

	DEBUG(10, ("smb_signing_activate: user_session_key\n"));
	dump_data(10, user_session_key.data, user_session_key.length);

	if (response.length) {
		ofs = user_session_key.length;
		memcpy(&si->mac_key.data[ofs], response.data, response.length);
		DEBUG(10, ("smb_signing_activate: response_data\n"));
		dump_data(10, response.data, response.length);
	} else {
		DEBUG(10, ("smb_signing_activate: NULL response_data\n"));
	}

	dump_data_pw("smb_signing_activate: mac key is:\n",
		     si->mac_key.data, si->mac_key.length);

	/* Initialise the sequence number */
	si->seqnum = 2;

	return true;
}

bool smb_signing_is_active(struct smb_signing_state *si)
{
	return si->active;
}

bool smb_signing_is_allowed(struct smb_signing_state *si)
{
	return si->allowed;
}

bool smb_signing_is_mandatory(struct smb_signing_state *si)
{
	return si->mandatory;
}

bool smb_signing_set_negotiated(struct smb_signing_state *si)
{
	if (!si->allowed) {
		return false;
	}

	si->negotiated = true;

	return true;
}

bool smb_signing_is_negotiated(struct smb_signing_state *si)
{
	return si->negotiated;
}
