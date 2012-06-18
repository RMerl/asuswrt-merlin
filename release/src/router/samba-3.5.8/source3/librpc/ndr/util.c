/* 
   Unix SMB/CIFS implementation.

   libndr interface

   Copyright (C) Andrew Tridgell 2003
   
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

NTSTATUS ndr_map_error2ntstatus(enum ndr_err_code ndr_err)
{
	switch (ndr_err) {
	case NDR_ERR_SUCCESS:
		return NT_STATUS_OK;
	case NDR_ERR_BUFSIZE:
		return NT_STATUS_BUFFER_TOO_SMALL;
	case NDR_ERR_TOKEN:
		return NT_STATUS_INTERNAL_ERROR;
	case NDR_ERR_ALLOC:
		return NT_STATUS_NO_MEMORY;
	case NDR_ERR_ARRAY_SIZE:
		return NT_STATUS_ARRAY_BOUNDS_EXCEEDED;
	case NDR_ERR_INVALID_POINTER:
		return NT_STATUS_INVALID_PARAMETER_MIX;
	case NDR_ERR_UNREAD_BYTES:
		return NT_STATUS_PORT_MESSAGE_TOO_LONG;
	default:
		break;
	}

	/* we should map all error codes to different status codes */
	return NT_STATUS_INVALID_PARAMETER;
}

/*
 * Convert an ndr error to string
 */

const char *ndr_errstr(enum ndr_err_code err)
{
	switch (err) {
	case NDR_ERR_SUCCESS:
		return "NDR_ERR_SUCCESS";
		break;
	case NDR_ERR_ARRAY_SIZE:
		return "NDR_ERR_ARRAY_SIZE";
		break;
	case NDR_ERR_BAD_SWITCH:
		return "NDR_ERR_BAD_SWITCH";
		break;
	case NDR_ERR_OFFSET:
		return "NDR_ERR_OFFSET";
		break;
	case NDR_ERR_RELATIVE:
		return "NDR_ERR_RELATIVE";
		break;
	case NDR_ERR_CHARCNV:
		return "NDR_ERR_CHARCNV";
		break;
	case NDR_ERR_LENGTH:
		return "NDR_ERR_LENGTH";
		break;
	case NDR_ERR_SUBCONTEXT:
		return "NDR_ERR_SUBCONTEXT";
		break;
	case NDR_ERR_COMPRESSION:
		return "NDR_ERR_COMPRESSION";
		break;
	case NDR_ERR_STRING:
		return "NDR_ERR_STRING";
		break;
	case NDR_ERR_VALIDATE:
		return "NDR_ERR_VALIDATE";
		break;
	case NDR_ERR_BUFSIZE:
		return "NDR_ERR_BUFSIZE";
		break;
	case NDR_ERR_ALLOC:
		return "NDR_ERR_ALLOC";
		break;
	case NDR_ERR_RANGE:
		return "NDR_ERR_RANGE";
		break;
	case NDR_ERR_TOKEN:
		return "NDR_ERR_TOKEN";
		break;
	case NDR_ERR_IPV4ADDRESS:
		return "NDR_ERR_IPV4ADDRESS";
		break;
	case NDR_ERR_INVALID_POINTER:
		return "NDR_ERR_INVALID_POINTER";
		break;
	case NDR_ERR_UNREAD_BYTES:
		return "NDR_ERR_UNREAD_BYTES";
		break;
	case NDR_ERR_NDR64:
		return "NDR_ERR_NDR64";
		break;
	}

	return talloc_asprintf(talloc_tos(), "Unknown NDR error: %d", err);
}

enum ndr_err_code ndr_push_server_id(struct ndr_push *ndr, int ndr_flags, const struct server_id *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS,
					  (uint32_t)r->pid));
#ifdef CLUSTER_SUPPORT
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS,
					  (uint32_t)r->vnn));
#endif
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_server_id(struct ndr_pull *ndr, int ndr_flags, struct server_id *r)
{
	if (ndr_flags & NDR_SCALARS) {
		uint32_t pid;
		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &pid));
#ifdef CLUSTER_SUPPORT
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->vnn));
#endif
		r->pid = (pid_t)pid;
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NDR_ERR_SUCCESS;
}

void ndr_print_server_id(struct ndr_print *ndr, const char *name, const struct server_id *r)
{
	ndr_print_struct(ndr, name, "server_id");
	ndr->depth++;
	ndr_print_uint32(ndr, "id", (uint32_t)r->pid);
#ifdef CLUSTER_SUPPORT
	ndr_print_uint32(ndr, "vnn", (uint32_t)r->vnn);
#endif
	ndr->depth--;
}

enum ndr_err_code ndr_push_file_id(struct ndr_push *ndr, int ndr_flags, const struct file_id *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_udlong(ndr, NDR_SCALARS,
					  (uint64_t)r->devid));
		NDR_CHECK(ndr_push_udlong(ndr, NDR_SCALARS,
					  (uint64_t)r->inode));
		NDR_CHECK(ndr_push_udlong(ndr, NDR_SCALARS,
					  (uint64_t)r->extid));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_file_id(struct ndr_pull *ndr, int ndr_flags, struct file_id *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_udlong(ndr, NDR_SCALARS, &r->devid));
		NDR_CHECK(ndr_pull_udlong(ndr, NDR_SCALARS, &r->inode));
		NDR_CHECK(ndr_pull_udlong(ndr, NDR_SCALARS, &r->extid));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NDR_ERR_SUCCESS;
}

void ndr_print_file_id(struct ndr_print *ndr, const char *name, const struct file_id *r)
{
	ndr_print_struct(ndr, name, "file_id");
	ndr->depth++;
	ndr_print_udlong(ndr, "devid", (uint64_t)r->devid);
	ndr_print_udlong(ndr, "inode", (uint64_t)r->inode);
	ndr_print_udlong(ndr, "extid", (uint64_t)r->extid);
	ndr->depth--;
}

_PUBLIC_ void ndr_print_bool(struct ndr_print *ndr, const char *name, const bool b)
{
	ndr->print(ndr, "%-25s: %s", name, b?"true":"false");
}

_PUBLIC_ void ndr_print_sockaddr_storage(struct ndr_print *ndr, const char *name, const struct sockaddr_storage *ss)
{
	char addr[INET6_ADDRSTRLEN];
	ndr->print(ndr, "%-25s: %s", name, print_sockaddr(addr, sizeof(addr), ss));
}

void *cmdline_lp_ctx;
struct smb_iconv_convenience *lp_iconv_convenience(void *lp_ctx)
{
	return NULL;
}

const struct ndr_syntax_id null_ndr_syntax_id =
{ { 0, 0, 0, { 0, 0 }, { 0, 0, 0, 0, 0, 0 } }, 0 };
