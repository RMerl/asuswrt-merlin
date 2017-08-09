/*
   Unix SMB/CIFS implementation.

   KDC structures

   Copyright (C) Andrew Tridgell	2005
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005

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

#ifndef _KDC_KDC_H
#define _KDC_KDC_H

#include "system/kerberos.h"
#include "auth/kerberos/kerberos.h"
#include <hdb.h>
#include <kdc.h>
#include <krb5/windc_plugin.h>
#include "kdc/samba_kdc.h"

struct tsocket_address;

/*
  top level context structure for the kdc server
*/
struct kdc_server {
	struct task_server *task;
	krb5_kdc_configuration *config;
	struct smb_krb5_context *smb_krb5_context;
	struct samba_kdc_base_context *base_ctx;
	struct ldb_context *samdb;
	bool am_rodc;
	uint32_t proxy_timeout;
};

enum kdc_process_ret {
	KDC_PROCESS_OK=0,
	KDC_PROCESS_FAILED,
	KDC_PROCESS_PROXY};

enum kdc_process_ret kpasswdd_process(struct kdc_server *kdc,
				      TALLOC_CTX *mem_ctx,
				      DATA_BLOB *input,
				      DATA_BLOB *reply,
				      struct tsocket_address *peer_addr,
				      struct tsocket_address *my_addr,
				      int datagram_reply);

/* from hdb-samba4.c */
NTSTATUS hdb_samba4_create_kdc(struct samba_kdc_base_context *base_ctx,
			       krb5_context context, struct HDB **db);

/* from proxy.c */
struct tevent_req *kdc_udp_proxy_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct kdc_server *kdc,
				      uint16_t port,
				      DATA_BLOB in);
NTSTATUS kdc_udp_proxy_recv(struct tevent_req *req,
			    TALLOC_CTX *mem_ctx,
			    DATA_BLOB *out);

struct tevent_req *kdc_tcp_proxy_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct kdc_server *kdc,
				      uint16_t port,
				      DATA_BLOB in);
NTSTATUS kdc_tcp_proxy_recv(struct tevent_req *req,
			    TALLOC_CTX *mem_ctx,
			    DATA_BLOB *out);

#endif
