/*
 *  SPNEGO Encapsulation
 *  Server routines
 *  Copyright (C) Simo Sorce 2010.
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

#ifndef _DCESRV_SPNEGO_H_
#define _DCESRV_SPENGO_H_

#include "librpc/crypto/spnego.h"

NTSTATUS spnego_server_auth_start(TALLOC_CTX *mem_ctx,
				  bool do_sign,
				  bool do_seal,
				  bool is_dcerpc,
				  DATA_BLOB *spnego_in,
				  DATA_BLOB *spnego_out,
				  struct spnego_context **spnego_ctx);
NTSTATUS spnego_server_step(struct spnego_context *sp_ctx,
			    TALLOC_CTX *mem_ctx,
			    DATA_BLOB *spnego_in,
			    DATA_BLOB *spnego_out);

#endif /* _DCESRV_SPENGO_H_ */
