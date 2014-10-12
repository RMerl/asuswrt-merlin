/*
 * Samba Unix/Linux SMB client library
 * Copyright (C) Volker Lendecke 2011
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ADDRCHANGE_H__
#define __ADDRCHANGE_H__

#include "lib/talloc/talloc.h"
#include "lib/tevent/tevent.h"
#include "libcli/util/ntstatus.h"
#include "lib/replace/replace.h"
#include "lib/replace/system/network.h"

struct addrchange_context;

NTSTATUS addrchange_context_create(TALLOC_CTX *mem_ctx,
				   struct addrchange_context **pctx);

struct tevent_req *addrchange_send(TALLOC_CTX *mem_ctx,
				   struct tevent_context *ev,
				   struct addrchange_context *ctx);

enum addrchange_type {
	ADDRCHANGE_ADD,
	ADDRCHANGE_DEL
};

NTSTATUS addrchange_recv(struct tevent_req *req, enum addrchange_type *type,
			 struct sockaddr_storage *addr);

#endif
