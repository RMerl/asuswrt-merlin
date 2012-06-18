/*
 * Unix SMB/CIFS implementation.
 * collected prototypes header
 *
 * frozen from "make proto" in May 2008
 *
 * Copyright (C) Michael Adam 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _LIBNET_PROTO_H_
#define _LIBNET_PROTO_H_


/* The following definitions come from libnet/libnet_join.c  */

NTSTATUS libnet_join_ok(const char *netbios_domain_name,
			const char *machine_name,
			const char *dc_name);
WERROR libnet_init_JoinCtx(TALLOC_CTX *mem_ctx,
			   struct libnet_JoinCtx **r);
WERROR libnet_init_UnjoinCtx(TALLOC_CTX *mem_ctx,
			     struct libnet_UnjoinCtx **r);
WERROR libnet_Join(TALLOC_CTX *mem_ctx,
		   struct libnet_JoinCtx *r);
WERROR libnet_Unjoin(TALLOC_CTX *mem_ctx,
		     struct libnet_UnjoinCtx *r);

/* The following definitions come from librpc/gen_ndr/ndr_libnet_join.c  */

_PUBLIC_ void ndr_print_libnet_JoinCtx(struct ndr_print *ndr, const char *name, int flags, const struct libnet_JoinCtx *r);
_PUBLIC_ void ndr_print_libnet_UnjoinCtx(struct ndr_print *ndr, const char *name, int flags, const struct libnet_UnjoinCtx *r);

/* The following definitions come from libnet/libnet_keytab.c  */

#ifdef HAVE_KRB5
krb5_error_code libnet_keytab_init(TALLOC_CTX *mem_ctx,
				   const char *keytab_name,
				   struct libnet_keytab_context **ctx);
krb5_error_code libnet_keytab_add(struct libnet_keytab_context *ctx);

struct libnet_keytab_entry *libnet_keytab_search(struct libnet_keytab_context *ctx,
						 const char *principal, int kvno,
						 const krb5_enctype enctype,
						 TALLOC_CTX *mem_ctx);
NTSTATUS libnet_keytab_add_to_keytab_entries(TALLOC_CTX *mem_ctx,
					     struct libnet_keytab_context *ctx,
					     uint32_t kvno,
					     const char *name,
					     const char *prefix,
					     const krb5_enctype enctype,
					     DATA_BLOB blob);
#endif

/* The following definitions come from libnet/libnet_samsync.c  */

NTSTATUS libnet_samsync_init_context(TALLOC_CTX *mem_ctx,
				     const struct dom_sid *domain_sid,
				     struct samsync_context **ctx_p);
NTSTATUS libnet_samsync(enum netr_SamDatabaseID database_id,
			struct samsync_context *ctx);
NTSTATUS pull_netr_AcctLockStr(TALLOC_CTX *mem_ctx,
			       struct lsa_BinaryString *r,
			       struct netr_AcctLockStr **str_p);

/* The following definitions come from libnet/libnet_dssync.c  */

NTSTATUS libnet_dssync_init_context(TALLOC_CTX *mem_ctx,
				    struct dssync_context **ctx_p);
NTSTATUS libnet_dssync(TALLOC_CTX *mem_ctx,
		       struct dssync_context *ctx);

#endif /*  _LIBNET_PROTO_H_  */
