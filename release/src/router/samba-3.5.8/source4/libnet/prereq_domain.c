/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Rafal Szczesniak  2006
   
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
#include "libnet/libnet.h"
#include "libcli/composite/composite.h"
#include "auth/credentials/credentials.h"
#include "librpc/ndr/libndr.h"
#include "librpc/gen_ndr/samr.h"
#include "librpc/gen_ndr/ndr_samr.h"
#include "librpc/gen_ndr/lsa.h"
#include "librpc/gen_ndr/ndr_lsa.h"


bool samr_domain_opened(struct libnet_context *ctx, const char *domain_name,
			struct composite_context **parent_ctx,
			struct libnet_DomainOpen *domain_open,
			void (*continue_fn)(struct composite_context*),
			void (*monitor)(struct monitor_msg*))
{
	struct composite_context *domopen_req;

	if (parent_ctx == NULL || *parent_ctx == NULL) return false;

	if (domain_name == NULL) {
		/*
		 * Try to guess the domain name from credentials,
		 * if it's not been explicitly specified.
		 */

		if (policy_handle_empty(&ctx->samr.handle)) {
			domain_open->in.type        = DOMAIN_SAMR;
			domain_open->in.domain_name = cli_credentials_get_domain(ctx->cred);
			domain_open->in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;

		} else {
			composite_error(*parent_ctx, NT_STATUS_INVALID_PARAMETER);
			return true;
		}

	} else {
		/*
		 * The domain name has been specified, so check whether the same
		 * domain is already opened. If it is - just return NULL. Start
		 * opening a new domain otherwise.
		 */

		if (policy_handle_empty(&ctx->samr.handle) ||
		    !strequal(domain_name, ctx->samr.name)) {
			domain_open->in.type        = DOMAIN_SAMR;
			domain_open->in.domain_name = domain_name;
			domain_open->in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;

		} else {
			/* domain has already been opened and it's the same domain
			   as requested */
			return true;
		}
	}

	/* send request to open the domain */
	domopen_req = libnet_DomainOpen_send(ctx, domain_open, monitor);
	if (composite_nomem(domopen_req, *parent_ctx)) return false;
	
	composite_continue(*parent_ctx, domopen_req, continue_fn, *parent_ctx);
	return false;
}


bool lsa_domain_opened(struct libnet_context *ctx, const char *domain_name,
		       struct composite_context **parent_ctx,
		       struct libnet_DomainOpen *domain_open,
		       void (*continue_fn)(struct composite_context*),
		       void (*monitor)(struct monitor_msg*))
{
	struct composite_context *domopen_req;
	
	if (parent_ctx == NULL || *parent_ctx == NULL) return false;

	if (domain_name == NULL) {
		/*
		 * Try to guess the domain name from credentials,
		 * if it's not been explicitly specified.
		 */

		if (policy_handle_empty(&ctx->lsa.handle)) {
			domain_open->in.type        = DOMAIN_LSA;
			domain_open->in.domain_name = cli_credentials_get_domain(ctx->cred);
			domain_open->in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;

		} else {
			composite_error(*parent_ctx, NT_STATUS_INVALID_PARAMETER);
			/* this ensures the calling function exits and composite function error
			   gets noticed quickly */
			return true;
		}

	} else {
		/*
		 * The domain name has been specified, so check whether the same
		 * domain is already opened. If it is - just return NULL. Start
		 * opening a new domain otherwise.
		 */

		if (policy_handle_empty(&ctx->lsa.handle) ||
		    !strequal(domain_name, ctx->lsa.name)) {
			domain_open->in.type        = DOMAIN_LSA;
			domain_open->in.domain_name = domain_name;
			domain_open->in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;

		} else {
			/* domain has already been opened and it's the same domain
			   as requested */
			return true;
		}
	}

	/* send request to open the domain */
	domopen_req = libnet_DomainOpen_send(ctx, domain_open, monitor);
	/* see the comment above to find out why true is returned here */
	if (composite_nomem(domopen_req, *parent_ctx)) return true;
	
	composite_continue(*parent_ctx, domopen_req, continue_fn, *parent_ctx);
	return false;
}
