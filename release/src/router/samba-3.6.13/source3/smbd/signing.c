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
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "smb_signing.h"

/***********************************************************
 Called to validate an incoming packet from the client.
************************************************************/

bool srv_check_sign_mac(struct smbd_server_connection *conn,
			const char *inbuf, uint32_t *seqnum,
			bool trusted_channel)
{
	/* Check if it's a non-session message. */
	if(CVAL(inbuf,0)) {
		return true;
	}

	if (trusted_channel) {
		NTSTATUS status;

		if (smb_len(inbuf) < (smb_ss_field + 8 - 4)) {
			DEBUG(1,("smb_signing_check_pdu: Can't check signature "
				 "on short packet! smb_len = %u\n",
				 smb_len(inbuf)));
			return false;
		}

		status = NT_STATUS(IVAL(inbuf, smb_ss_field + 4));
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1,("smb_signing_check_pdu: trusted channel passed %s\n",
				 nt_errstr(status)));
			return false;
		}

		*seqnum = IVAL(inbuf, smb_ss_field);
		return true;
	}

	*seqnum = smb_signing_next_seqnum(conn->smb1.signing_state, false);
	return smb_signing_check_pdu(conn->smb1.signing_state,
				     (const uint8_t *)inbuf,
				     *seqnum);
}

/***********************************************************
 Called to sign an outgoing packet to the client.
************************************************************/

void srv_calculate_sign_mac(struct smbd_server_connection *conn,
			    char *outbuf, uint32_t seqnum)
{
	/* Check if it's a non-session message. */
	if(CVAL(outbuf,0)) {
		return;
	}

	smb_signing_sign_pdu(conn->smb1.signing_state, (uint8_t *)outbuf, seqnum);
}


/***********************************************************
 Called to indicate a oneway request
************************************************************/
void srv_cancel_sign_response(struct smbd_server_connection *conn)
{
	smb_signing_cancel_reply(conn->smb1.signing_state, true);
}

struct smbd_shm_signing {
	size_t shm_size;
	uint8_t *shm_pointer;

	/* we know the signing engine will only allocate 2 chunks */
	uint8_t *ptr1;
	size_t len1;
	uint8_t *ptr2;
	size_t len2;
};

static int smbd_shm_signing_destructor(struct smbd_shm_signing *s)
{
	anonymous_shared_free(s->shm_pointer);
	return 0;
}

static void *smbd_shm_signing_alloc(TALLOC_CTX *mem_ctx, size_t len)
{
	struct smbd_shm_signing *s = talloc_get_type_abort(mem_ctx,
				     struct smbd_shm_signing);

	if (s->ptr1 == NULL) {
		s->len1 = len;
		if (len % 8) {
			s->len1 += (8 - (len % 8));
		}
		if (s->len1 > s->shm_size) {
			s->len1 = 0;
			errno = ENOMEM;
			return NULL;
		}
		s->ptr1 = s->shm_pointer;
		return s->ptr1;
	}

	if (s->ptr2 == NULL) {
		s->len2 = len;
		if (s->len2 > (s->shm_size - s->len1)) {
			s->len2 = 0;
			errno = ENOMEM;
			return NULL;
		}
		s->ptr2 = s->shm_pointer + s->len1;
		return s->ptr2;
	}

	errno = ENOMEM;
	return NULL;
}

static void smbd_shm_signing_free(TALLOC_CTX *mem_ctx, void *ptr)
{
	struct smbd_shm_signing *s = talloc_get_type_abort(mem_ctx,
				     struct smbd_shm_signing);

	if (s->ptr2 == ptr) {
		s->ptr2 = NULL;
		s->len2 = 0;
	}
}

/***********************************************************
 Called by server negprot when signing has been negotiated.
************************************************************/

bool srv_init_signing(struct smbd_server_connection *conn)
{
	bool allowed = true;
	bool mandatory = false;

	switch (lp_server_signing()) {
	case Required:
		mandatory = true;
		break;
	case Auto:
		break;
	case True:
		break;
	case False:
		allowed = false;
		break;
	}

	if (lp_async_smb_echo_handler()) {
		struct smbd_shm_signing *s;

		/* setup the signing state in shared memory */
		s = talloc_zero(smbd_event_context(), struct smbd_shm_signing);
		if (s == NULL) {
			return false;
		}
		s->shm_size = 4096;
		s->shm_pointer =
			(uint8_t *)anonymous_shared_allocate(s->shm_size);
		if (s->shm_pointer == NULL) {
			talloc_free(s);
			return false;
		}
		talloc_set_destructor(s, smbd_shm_signing_destructor);
		conn->smb1.signing_state = smb_signing_init_ex(s,
							allowed, mandatory,
							smbd_shm_signing_alloc,
							smbd_shm_signing_free);
		if (!conn->smb1.signing_state) {
			return false;
		}
		return true;
	}

	conn->smb1.signing_state = smb_signing_init(smbd_event_context(),
						    allowed, mandatory);
	if (!conn->smb1.signing_state) {
		return false;
	}

	return true;
}

void srv_set_signing_negotiated(struct smbd_server_connection *conn)
{
	smb_signing_set_negotiated(conn->smb1.signing_state);
}

/***********************************************************
 Returns whether signing is active. We can't use sendfile or raw
 reads/writes if it is.
************************************************************/

bool srv_is_signing_active(struct smbd_server_connection *conn)
{
	return smb_signing_is_active(conn->smb1.signing_state);
}


/***********************************************************
 Returns whether signing is negotiated. We can't use it unless it was
 in the negprot.
************************************************************/

bool srv_is_signing_negotiated(struct smbd_server_connection *conn)
{
	return smb_signing_is_negotiated(conn->smb1.signing_state);
}

/***********************************************************
 Turn on signing from this packet onwards.
************************************************************/

void srv_set_signing(struct smbd_server_connection *conn,
		     const DATA_BLOB user_session_key,
		     const DATA_BLOB response)
{
	bool negotiated;
	bool mandatory;

	if (!user_session_key.length)
		return;

	negotiated = smb_signing_is_negotiated(conn->smb1.signing_state);
	mandatory = smb_signing_is_mandatory(conn->smb1.signing_state);

	if (!negotiated && !mandatory) {
		DEBUG(5,("srv_set_signing: signing negotiated = %u, "
			 "mandatory_signing = %u. Not allowing smb signing.\n",
			 negotiated, mandatory));
		return;
	}

	if (!smb_signing_activate(conn->smb1.signing_state,
				  user_session_key, response)) {
		return;
	}

	DEBUG(3,("srv_set_signing: turning on SMB signing: "
		 "signing negotiated = %u, mandatory_signing = %u.\n",
		 negotiated, mandatory));
}

