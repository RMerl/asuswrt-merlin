/*
 *  Unix SMB/CIFS implementation.
 *  Version 3.0
 *  NTLMSSP Signing routines
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-2001
 *  Copyright (C) Andrew Bartlett <abartlet@samba.org> 2003-2005
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
#include "auth/ntlmssp/ntlmssp.h"
#include "auth/gensec/gensec.h"
#include "../libcli/auth/ntlmssp_private.h"

NTSTATUS gensec_ntlmssp_sign_packet(struct gensec_security *gensec_security,
				    TALLOC_CTX *sig_mem_ctx,
				    const uint8_t *data, size_t length,
				    const uint8_t *whole_pdu, size_t pdu_length,
				    DATA_BLOB *sig)
{
	struct gensec_ntlmssp_context *gensec_ntlmssp =
		talloc_get_type_abort(gensec_security->private_data,
				      struct gensec_ntlmssp_context);
	NTSTATUS nt_status;

	nt_status = ntlmssp_sign_packet(gensec_ntlmssp->ntlmssp_state,
					sig_mem_ctx,
					data, length,
					whole_pdu, pdu_length,
					sig);

	return nt_status;
}

NTSTATUS gensec_ntlmssp_check_packet(struct gensec_security *gensec_security,
				     TALLOC_CTX *sig_mem_ctx,
				     const uint8_t *data, size_t length,
				     const uint8_t *whole_pdu, size_t pdu_length,
				     const DATA_BLOB *sig)
{
	struct gensec_ntlmssp_context *gensec_ntlmssp =
		talloc_get_type_abort(gensec_security->private_data,
				      struct gensec_ntlmssp_context);
	NTSTATUS nt_status;

	nt_status = ntlmssp_check_packet(gensec_ntlmssp->ntlmssp_state,
					 data, length,
					 whole_pdu, pdu_length,
					 sig);

	return nt_status;
}

NTSTATUS gensec_ntlmssp_seal_packet(struct gensec_security *gensec_security,
				    TALLOC_CTX *sig_mem_ctx,
				    uint8_t *data, size_t length,
				    const uint8_t *whole_pdu, size_t pdu_length,
				    DATA_BLOB *sig)
{
	struct gensec_ntlmssp_context *gensec_ntlmssp =
		talloc_get_type_abort(gensec_security->private_data,
				      struct gensec_ntlmssp_context);
	NTSTATUS nt_status;

	nt_status = ntlmssp_seal_packet(gensec_ntlmssp->ntlmssp_state,
					sig_mem_ctx,
					data, length,
					whole_pdu, pdu_length,
					sig);

	return nt_status;
}

/*
  wrappers for the ntlmssp_*() functions
*/
NTSTATUS gensec_ntlmssp_unseal_packet(struct gensec_security *gensec_security,
				      TALLOC_CTX *sig_mem_ctx,
				      uint8_t *data, size_t length,
				      const uint8_t *whole_pdu, size_t pdu_length,
				      const DATA_BLOB *sig)
{
	struct gensec_ntlmssp_context *gensec_ntlmssp =
		talloc_get_type_abort(gensec_security->private_data,
				      struct gensec_ntlmssp_context);
	NTSTATUS nt_status;

	nt_status = ntlmssp_unseal_packet(gensec_ntlmssp->ntlmssp_state,
					  data, length,
					  whole_pdu, pdu_length,
					  sig);

	return nt_status;
}

size_t gensec_ntlmssp_sig_size(struct gensec_security *gensec_security, size_t data_size) 
{
	return NTLMSSP_SIG_SIZE;
}

NTSTATUS gensec_ntlmssp_wrap(struct gensec_security *gensec_security, 
			     TALLOC_CTX *out_mem_ctx,
			     const DATA_BLOB *in, 
			     DATA_BLOB *out)
{
	struct gensec_ntlmssp_context *gensec_ntlmssp =
		talloc_get_type_abort(gensec_security->private_data,
				      struct gensec_ntlmssp_context);

	return ntlmssp_wrap(gensec_ntlmssp->ntlmssp_state,
			    out_mem_ctx,
			    in, out);
}


NTSTATUS gensec_ntlmssp_unwrap(struct gensec_security *gensec_security, 
			       TALLOC_CTX *out_mem_ctx,
			       const DATA_BLOB *in, 
			       DATA_BLOB *out)
{
	struct gensec_ntlmssp_context *gensec_ntlmssp =
		talloc_get_type_abort(gensec_security->private_data,
				      struct gensec_ntlmssp_context);

	return ntlmssp_unwrap(gensec_ntlmssp->ntlmssp_state,
			      out_mem_ctx,
			      in, out);
}
