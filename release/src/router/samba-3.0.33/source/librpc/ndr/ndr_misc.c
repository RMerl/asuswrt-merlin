/* 
   Unix SMB/CIFS implementation.

   UUID/GUID/policy_handle functions

   Copyright (C) Theodore Ts'o               1996, 1997,
   Copyright (C) Jim McDonough                     2002.
   Copyright (C) Andrew Tridgell                   2003.
   Copyright (C) Stefan (metze) Metzmacher         2004.
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"

NTSTATUS ndr_push_GUID(struct ndr_push *ndr, int ndr_flags, const struct GUID *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->time_low));
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, r->time_mid));
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, r->time_hi_and_version));
		NDR_CHECK(ndr_push_array_uint8(ndr, NDR_SCALARS, r->clock_seq, 2));
		NDR_CHECK(ndr_push_array_uint8(ndr, NDR_SCALARS, r->node, 6));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_GUID(struct ndr_pull *ndr, int ndr_flags, struct GUID *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->time_low));
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->time_mid));
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->time_hi_and_version));
		NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, r->clock_seq, 2));
		NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, r->node, 6));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NT_STATUS_OK;
}

size_t ndr_size_GUID(const struct GUID *r, int flags)
{
	return ndr_size_struct(r, flags, (ndr_push_flags_fn_t)ndr_push_GUID);
}

/**
  build a GUID from a string
*/
NTSTATUS GUID_from_string(const char *s, struct GUID *guid)
{
	NTSTATUS status = NT_STATUS_INVALID_PARAMETER;
	uint32_t time_low;
	uint32_t time_mid, time_hi_and_version;
	uint32_t clock_seq[2];
	uint32_t node[6];
	int i;

	if (s == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (11 == sscanf(s, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			 &time_low, &time_mid, &time_hi_and_version, 
			 &clock_seq[0], &clock_seq[1],
			 &node[0], &node[1], &node[2], &node[3], &node[4], &node[5])) {
	        status = NT_STATUS_OK;
	} else if (11 == sscanf(s, "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
				&time_low, &time_mid, &time_hi_and_version, 
				&clock_seq[0], &clock_seq[1],
				&node[0], &node[1], &node[2], &node[3], &node[4], &node[5])) {
		status = NT_STATUS_OK;
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	guid->time_low = time_low;
	guid->time_mid = time_mid;
	guid->time_hi_and_version = time_hi_and_version;
	guid->clock_seq[0] = clock_seq[0];
	guid->clock_seq[1] = clock_seq[1];
	for (i=0;i<6;i++) {
		guid->node[i] = node[i];
	}

	return NT_STATUS_OK;
}

/**
 * generate a random GUID
 */
struct GUID GUID_random(void)
{
	struct GUID guid;

	generate_random_buffer((uint8_t *)&guid, sizeof(guid));
	guid.clock_seq[0] = (guid.clock_seq[0] & 0x3F) | 0x80;
	guid.time_hi_and_version = (guid.time_hi_and_version & 0x0FFF) | 0x4000;

	return guid;
}

/**
 * generate an empty GUID 
 */
struct GUID GUID_zero(void)
{
	struct GUID guid;

	ZERO_STRUCT(guid);

	return guid;
}

/**
 * see if a range of memory is all zero. A NULL pointer is considered
 * to be all zero 
 */
BOOL all_zero(const uint8_t *ptr, size_t size)
{
	int i;
	if (!ptr) return True;
	for (i=0;i<size;i++) {
		if (ptr[i]) return False;
	}
	return True;
}


BOOL GUID_all_zero(const struct GUID *u)
{
	if (u->time_low != 0 ||
	    u->time_mid != 0 ||
	    u->time_hi_and_version != 0 ||
	    u->clock_seq[0] != 0 ||
	    u->clock_seq[1] != 0 ||
	    !all_zero(u->node, 6)) {
		return False;
	}
	return True;
}

BOOL GUID_equal(const struct GUID *u1, const struct GUID *u2)
{
	if (u1->time_low != u2->time_low ||
	    u1->time_mid != u2->time_mid ||
	    u1->time_hi_and_version != u2->time_hi_and_version ||
	    u1->clock_seq[0] != u2->clock_seq[0] ||
	    u1->clock_seq[1] != u2->clock_seq[1] ||
	    memcmp(u1->node, u2->node, 6) != 0) {
		return False;
	}
	return True;
}

/**
  its useful to be able to display these in debugging messages
*/
char *GUID_string(TALLOC_CTX *mem_ctx, const struct GUID *guid)
{
	return talloc_asprintf(mem_ctx, 
			       "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			       guid->time_low, guid->time_mid,
			       guid->time_hi_and_version,
			       guid->clock_seq[0],
			       guid->clock_seq[1],
			       guid->node[0], guid->node[1],
			       guid->node[2], guid->node[3],
			       guid->node[4], guid->node[5]);
}

char *GUID_string2(TALLOC_CTX *mem_ctx, const struct GUID *guid)
{
	char *ret, *s = GUID_string(mem_ctx, guid);
	ret = talloc_asprintf(mem_ctx, "{%s}", s);
	talloc_free(s);
	return ret;
}

void ndr_print_GUID(struct ndr_print *ndr, const char *name, const struct GUID *guid)
{
	ndr->print(ndr, "%-25s: %s", name, GUID_string(ndr, guid));
}

BOOL policy_handle_empty(struct policy_handle *h) 
{
	return (h->handle_type == 0 && GUID_all_zero(&h->uuid));
}

NTSTATUS ndr_push_policy_handle(struct ndr_push *ndr, int ndr_flags, const struct policy_handle *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->handle_type));
		NDR_CHECK(ndr_push_GUID(ndr, NDR_SCALARS, &r->uuid));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_policy_handle(struct ndr_pull *ndr, int ndr_flags, struct policy_handle *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->handle_type));
		NDR_CHECK(ndr_pull_GUID(ndr, NDR_SCALARS, &r->uuid));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NT_STATUS_OK;
}

void ndr_print_policy_handle(struct ndr_print *ndr, const char *name, const struct policy_handle *r)
{
	ndr_print_struct(ndr, name, "policy_handle");
	ndr->depth++;
	ndr_print_uint32(ndr, "handle_type", r->handle_type);
	ndr_print_GUID(ndr, "uuid", &r->uuid);
	ndr->depth--;
}

NTSTATUS ndr_push_server_id(struct ndr_push *ndr, int ndr_flags, const struct server_id *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS,
					  (uint32_t)r->id.pid));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_server_id(struct ndr_pull *ndr, int ndr_flags, struct server_id *r)
{
	if (ndr_flags & NDR_SCALARS) {
		uint32_t pid;
		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &pid));
		r->id.pid = (pid_t)pid;
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NT_STATUS_OK;
}

void ndr_print_server_id(struct ndr_print *ndr, const char *name, const struct server_id *r)
{
	ndr_print_struct(ndr, name, "server_id");
	ndr->depth++;
	ndr_print_uint32(ndr, "id", (uint32_t)r->id.pid);
	ndr->depth--;
}
