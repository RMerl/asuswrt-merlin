/* 
   Unix SMB/CIFS implementation.

   routines for marshalling/unmarshalling basic types

   Copyright (C) Andrew Tridgell 2003
   
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

#define NDR_SVAL(ndr, ofs) (NDR_BE(ndr)?RSVAL(ndr->data,ofs):SVAL(ndr->data,ofs))
#define NDR_IVAL(ndr, ofs) (NDR_BE(ndr)?RIVAL(ndr->data,ofs):IVAL(ndr->data,ofs))
#define NDR_IVALS(ndr, ofs) (NDR_BE(ndr)?RIVALS(ndr->data,ofs):IVALS(ndr->data,ofs))
#define NDR_SSVAL(ndr, ofs, v) do { if (NDR_BE(ndr))  { RSSVAL(ndr->data,ofs,v); } else SSVAL(ndr->data,ofs,v); } while (0)
#define NDR_SIVAL(ndr, ofs, v) do { if (NDR_BE(ndr))  { RSIVAL(ndr->data,ofs,v); } else SIVAL(ndr->data,ofs,v); } while (0)
#define NDR_SIVALS(ndr, ofs, v) do { if (NDR_BE(ndr))  { RSIVALS(ndr->data,ofs,v); } else SIVALS(ndr->data,ofs,v); } while (0)


/*
  check for data leaks from the server by looking for non-zero pad bytes
  these could also indicate that real structure elements have been
  mistaken for padding in the IDL
*/
void ndr_check_padding(struct ndr_pull *ndr, size_t n)
{
	size_t ofs2 = (ndr->offset + (n-1)) & ~(n-1);
	int i;
	for (i=ndr->offset;i<ofs2;i++) {
		if (ndr->data[i] != 0) {
			break;
		}
	}
	if (i<ofs2) {
		DEBUG(0,("WARNING: Non-zero padding to %d: ", (int)n));
		for (i=ndr->offset;i<ofs2;i++) {
			DEBUG(0,("%02x ", ndr->data[i]));
		}
		DEBUG(0,("\n"));
	}

}

/*
  parse a int8_t
*/
NTSTATUS ndr_pull_int8(struct ndr_pull *ndr, int ndr_flags, int8_t *v)
{
	NDR_PULL_NEED_BYTES(ndr, 1);
	*v = (int8_t)CVAL(ndr->data, ndr->offset);
	ndr->offset += 1;
	return NT_STATUS_OK;
}

/*
  parse a uint8_t
*/
NTSTATUS ndr_pull_uint8(struct ndr_pull *ndr, int ndr_flags, uint8_t *v)
{
	NDR_PULL_NEED_BYTES(ndr, 1);
	*v = CVAL(ndr->data, ndr->offset);
	ndr->offset += 1;
	return NT_STATUS_OK;
}

/*
  parse a int16_t
*/
NTSTATUS ndr_pull_int16(struct ndr_pull *ndr, int ndr_flags, int16_t *v)
{
	NDR_PULL_ALIGN(ndr, 2);
	NDR_PULL_NEED_BYTES(ndr, 2);
	*v = (uint16_t)NDR_SVAL(ndr, ndr->offset);
	ndr->offset += 2;
	return NT_STATUS_OK;
}

/*
  parse a uint16_t
*/
NTSTATUS ndr_pull_uint16(struct ndr_pull *ndr, int ndr_flags, uint16_t *v)
{
	NDR_PULL_ALIGN(ndr, 2);
	NDR_PULL_NEED_BYTES(ndr, 2);
	*v = NDR_SVAL(ndr, ndr->offset);
	ndr->offset += 2;
	return NT_STATUS_OK;
}

/*
  parse a int32_t
*/
NTSTATUS ndr_pull_int32(struct ndr_pull *ndr, int ndr_flags, int32_t *v)
{
	NDR_PULL_ALIGN(ndr, 4);
	NDR_PULL_NEED_BYTES(ndr, 4);
	*v = NDR_IVALS(ndr, ndr->offset);
	ndr->offset += 4;
	return NT_STATUS_OK;
}

/*
  parse a uint32_t
*/
NTSTATUS ndr_pull_uint32(struct ndr_pull *ndr, int ndr_flags, uint32_t *v)
{
	NDR_PULL_ALIGN(ndr, 4);
	NDR_PULL_NEED_BYTES(ndr, 4);
	*v = NDR_IVAL(ndr, ndr->offset);
	ndr->offset += 4;
	return NT_STATUS_OK;
}

/*
  parse a pointer referent identifier
*/
NTSTATUS ndr_pull_generic_ptr(struct ndr_pull *ndr, uint32_t *v)
{
	NTSTATUS status;
	status = ndr_pull_uint32(ndr, NDR_SCALARS, v);
	if (NT_STATUS_IS_OK(status) && *v != 0) {
		ndr->ptr_count++;
	}
	return status;
}

/*
  parse a ref pointer referent identifier
*/
NTSTATUS ndr_pull_ref_ptr(struct ndr_pull *ndr, uint32_t *v)
{
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, v));
	/* ref pointers always point to data */
	*v = 1;
	return NT_STATUS_OK;
}

/*
  parse a udlong
*/
NTSTATUS ndr_pull_udlong(struct ndr_pull *ndr, int ndr_flags, uint64_t *v)
{
	NDR_PULL_ALIGN(ndr, 4);
	NDR_PULL_NEED_BYTES(ndr, 8);
	*v = NDR_IVAL(ndr, ndr->offset);
	*v |= (uint64_t)(NDR_IVAL(ndr, ndr->offset+4)) << 32;
	ndr->offset += 8;
	return NT_STATUS_OK;
}

/*
  parse a udlongr
*/
NTSTATUS ndr_pull_udlongr(struct ndr_pull *ndr, int ndr_flags, uint64_t *v)
{
	NDR_PULL_ALIGN(ndr, 4);
	NDR_PULL_NEED_BYTES(ndr, 8);
	*v = ((uint64_t)NDR_IVAL(ndr, ndr->offset)) << 32;
	*v |= NDR_IVAL(ndr, ndr->offset+4);
	ndr->offset += 8;
	return NT_STATUS_OK;
}

/*
  parse a dlong
*/
NTSTATUS ndr_pull_dlong(struct ndr_pull *ndr, int ndr_flags, int64_t *v)
{
	return ndr_pull_udlong(ndr, ndr_flags, (uint64_t *)v);
}

/*
  parse a hyper
*/
NTSTATUS ndr_pull_hyper(struct ndr_pull *ndr, int ndr_flags, uint64_t *v)
{
	NDR_PULL_ALIGN(ndr, 8);
	return ndr_pull_udlong(ndr, ndr_flags, v);
}

/*
  parse a pointer
*/
NTSTATUS ndr_pull_pointer(struct ndr_pull *ndr, int ndr_flags, void* *v)
{
	intptr_t h;
	NDR_PULL_ALIGN(ndr, sizeof(h));
	NDR_PULL_NEED_BYTES(ndr, sizeof(h));
	memcpy(&h, ndr->data+ndr->offset, sizeof(h));
	ndr->offset += sizeof(h);
	*v = (void *)h;
	return NT_STATUS_OK;	
}

/*
  pull a NTSTATUS
*/
NTSTATUS ndr_pull_NTSTATUS(struct ndr_pull *ndr, int ndr_flags, NTSTATUS *status)
{
	uint32_t v;
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &v));
	*status = NT_STATUS(v);
	return NT_STATUS_OK;
}

/*
  push a NTSTATUS
*/
NTSTATUS ndr_push_NTSTATUS(struct ndr_push *ndr, int ndr_flags, NTSTATUS status)
{
	return ndr_push_uint32(ndr, ndr_flags, NT_STATUS_V(status));
}

void ndr_print_NTSTATUS(struct ndr_print *ndr, const char *name, NTSTATUS r)
{
	ndr->print(ndr, "%-25s: %s", name, nt_errstr(r));
}

/*
  pull a WERROR
*/
NTSTATUS ndr_pull_WERROR(struct ndr_pull *ndr, int ndr_flags, WERROR *status)
{
	uint32_t v;
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &v));
	*status = W_ERROR(v);
	return NT_STATUS_OK;
}

/*
  push a WERROR
*/
NTSTATUS ndr_push_WERROR(struct ndr_push *ndr, int ndr_flags, WERROR status)
{
	return ndr_push_uint32(ndr, NDR_SCALARS, W_ERROR_V(status));
}

void ndr_print_WERROR(struct ndr_print *ndr, const char *name, WERROR r)
{
	ndr->print(ndr, "%-25s: %s", name, dos_errstr(r));
}

/*
  parse a set of bytes
*/
NTSTATUS ndr_pull_bytes(struct ndr_pull *ndr, uint8_t *data, uint32_t n)
{
	NDR_PULL_NEED_BYTES(ndr, n);
	memcpy(data, ndr->data + ndr->offset, n);
	ndr->offset += n;
	return NT_STATUS_OK;
}

/*
  pull an array of uint8
*/
NTSTATUS ndr_pull_array_uint8(struct ndr_pull *ndr, int ndr_flags, uint8_t *data, uint32_t n)
{
	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}
	return ndr_pull_bytes(ndr, data, n);
}

/*
  push a int8_t
*/
NTSTATUS ndr_push_int8(struct ndr_push *ndr, int ndr_flags, int8_t v)
{
	NDR_PUSH_NEED_BYTES(ndr, 1);
	SCVAL(ndr->data, ndr->offset, (uint8_t)v);
	ndr->offset += 1;
	return NT_STATUS_OK;
}

/*
  push a uint8_t
*/
NTSTATUS ndr_push_uint8(struct ndr_push *ndr, int ndr_flags, uint8_t v)
{
	NDR_PUSH_NEED_BYTES(ndr, 1);
	SCVAL(ndr->data, ndr->offset, v);
	ndr->offset += 1;
	return NT_STATUS_OK;
}

/*
  push a int16_t
*/
NTSTATUS ndr_push_int16(struct ndr_push *ndr, int ndr_flags, int16_t v)
{
	NDR_PUSH_ALIGN(ndr, 2);
	NDR_PUSH_NEED_BYTES(ndr, 2);
	NDR_SSVAL(ndr, ndr->offset, (uint16_t)v);
	ndr->offset += 2;
	return NT_STATUS_OK;
}

/*
  push a uint16_t
*/
NTSTATUS ndr_push_uint16(struct ndr_push *ndr, int ndr_flags, uint16_t v)
{
	NDR_PUSH_ALIGN(ndr, 2);
	NDR_PUSH_NEED_BYTES(ndr, 2);
	NDR_SSVAL(ndr, ndr->offset, v);
	ndr->offset += 2;
	return NT_STATUS_OK;
}

/*
  push a int32_t
*/
NTSTATUS ndr_push_int32(struct ndr_push *ndr, int ndr_flags, int32_t v)
{
	NDR_PUSH_ALIGN(ndr, 4);
	NDR_PUSH_NEED_BYTES(ndr, 4);
	NDR_SIVALS(ndr, ndr->offset, v);
	ndr->offset += 4;
	return NT_STATUS_OK;
}

/*
  push a uint32_t
*/
NTSTATUS ndr_push_uint32(struct ndr_push *ndr, int ndr_flags, uint32_t v)
{
	NDR_PUSH_ALIGN(ndr, 4);
	NDR_PUSH_NEED_BYTES(ndr, 4);
	NDR_SIVAL(ndr, ndr->offset, v);
	ndr->offset += 4;
	return NT_STATUS_OK;
}

/*
  push a udlong
*/
NTSTATUS ndr_push_udlong(struct ndr_push *ndr, int ndr_flags, uint64_t v)
{
	NDR_PUSH_ALIGN(ndr, 4);
	NDR_PUSH_NEED_BYTES(ndr, 8);
	NDR_SIVAL(ndr, ndr->offset, (v & 0xFFFFFFFF));
	NDR_SIVAL(ndr, ndr->offset+4, (v>>32));
	ndr->offset += 8;
	return NT_STATUS_OK;
}

/*
  push a udlongr
*/
NTSTATUS ndr_push_udlongr(struct ndr_push *ndr, int ndr_flags, uint64_t v)
{
	NDR_PUSH_ALIGN(ndr, 4);
	NDR_PUSH_NEED_BYTES(ndr, 8);
	NDR_SIVAL(ndr, ndr->offset, (v>>32));
	NDR_SIVAL(ndr, ndr->offset+4, (v & 0xFFFFFFFF));
	ndr->offset += 8;
	return NT_STATUS_OK;
}

/*
  push a dlong
*/
NTSTATUS ndr_push_dlong(struct ndr_push *ndr, int ndr_flags, int64_t v)
{
	return ndr_push_udlong(ndr, NDR_SCALARS, (uint64_t)v);
}

/*
  push a hyper
*/
NTSTATUS ndr_push_hyper(struct ndr_push *ndr, int ndr_flags, uint64_t v)
{
	NDR_PUSH_ALIGN(ndr, 8);
	return ndr_push_udlong(ndr, NDR_SCALARS, v);
}

/*
  push a pointer
*/
NTSTATUS ndr_push_pointer(struct ndr_push *ndr, int ndr_flags, void* v)
{
	intptr_t h = (intptr_t)v;
	NDR_PUSH_ALIGN(ndr, sizeof(h));
	NDR_PUSH_NEED_BYTES(ndr, sizeof(h));
	memcpy(ndr->data+ndr->offset, &h, sizeof(h));
	ndr->offset += sizeof(h);
	return NT_STATUS_OK;	
}

NTSTATUS ndr_push_align(struct ndr_push *ndr, size_t size)
{
	NDR_PUSH_ALIGN(ndr, size);
	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_align(struct ndr_pull *ndr, size_t size)
{
	NDR_PULL_ALIGN(ndr, size);
	return NT_STATUS_OK;
}

/*
  push some bytes
*/
NTSTATUS ndr_push_bytes(struct ndr_push *ndr, const uint8_t *data, uint32_t n)
{
	NDR_PUSH_NEED_BYTES(ndr, n);
	memcpy(ndr->data + ndr->offset, data, n);
	ndr->offset += n;
	return NT_STATUS_OK;
}

/*
  push some zero bytes
*/
NTSTATUS ndr_push_zero(struct ndr_push *ndr, uint32_t n)
{
	NDR_PUSH_NEED_BYTES(ndr, n);
	memset(ndr->data + ndr->offset, 0, n);
	ndr->offset += n;
	return NT_STATUS_OK;
}

/*
  push an array of uint8
*/
NTSTATUS ndr_push_array_uint8(struct ndr_push *ndr, int ndr_flags, const uint8_t *data, uint32_t n)
{
	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}
	return ndr_push_bytes(ndr, data, n);
}

/*
  save the current position
 */
void ndr_push_save(struct ndr_push *ndr, struct ndr_push_save *save)
{
	save->offset = ndr->offset;
}

/*
  restore the position
 */
void ndr_push_restore(struct ndr_push *ndr, struct ndr_push_save *save)
{
	ndr->offset = save->offset;
}

/*
  push a unique non-zero value if a pointer is non-NULL, otherwise 0
*/
NTSTATUS ndr_push_unique_ptr(struct ndr_push *ndr, const void *p)
{
	uint32_t ptr = 0;
	if (p) {
		ptr = ndr->ptr_count * 4;
		ptr |= 0x00020000;
		ndr->ptr_count++;
	}
	return ndr_push_uint32(ndr, NDR_SCALARS, ptr);
}

/*
  push a 'simple' full non-zero value if a pointer is non-NULL, otherwise 0
*/
NTSTATUS ndr_push_sptr_ptr(struct ndr_push *ndr, const void *p)
{
	uint32_t ptr = 0;
	if (p) {
		ndr->ptr_count++;
		ptr = ndr->ptr_count;
	}
	return ndr_push_uint32(ndr, NDR_SCALARS, ptr);
}

/*
  push always a 0, if a pointer is NULL it's a fatal error
*/
NTSTATUS ndr_push_ref_ptr(struct ndr_push *ndr)
{
	return ndr_push_uint32(ndr, NDR_SCALARS, 0xAEF1AEF1);
}

/*
  push a NTTIME
*/
NTSTATUS ndr_push_NTTIME(struct ndr_push *ndr, int ndr_flags, NTTIME t)
{
	NDR_CHECK(ndr_push_udlong(ndr, ndr_flags, t));
	return NT_STATUS_OK;
}

/*
  pull a NTTIME
*/
NTSTATUS ndr_pull_NTTIME(struct ndr_pull *ndr, int ndr_flags, NTTIME *t)
{
	NDR_CHECK(ndr_pull_udlong(ndr, ndr_flags, t));
	return NT_STATUS_OK;
}

/*
  push a NTTIME
*/
NTSTATUS ndr_push_NTTIME_1sec(struct ndr_push *ndr, int ndr_flags, NTTIME t)
{
	t /= 10000000;
	NDR_CHECK(ndr_push_hyper(ndr, ndr_flags, t));
	return NT_STATUS_OK;
}

/*
  pull a NTTIME_1sec
*/
NTSTATUS ndr_pull_NTTIME_1sec(struct ndr_pull *ndr, int ndr_flags, NTTIME *t)
{
	NDR_CHECK(ndr_pull_hyper(ndr, ndr_flags, t));
	(*t) *= 10000000;
	return NT_STATUS_OK;
}

/*
  pull a NTTIME_hyper
*/
NTSTATUS ndr_pull_NTTIME_hyper(struct ndr_pull *ndr, int ndr_flags, NTTIME *t)
{
	NDR_CHECK(ndr_pull_hyper(ndr, ndr_flags, t));
	return NT_STATUS_OK;
}

/*
  push a NTTIME_hyper
*/
NTSTATUS ndr_push_NTTIME_hyper(struct ndr_push *ndr, int ndr_flags, NTTIME t)
{
	NDR_CHECK(ndr_push_hyper(ndr, ndr_flags, t));
	return NT_STATUS_OK;
}

/*
  push a time_t
*/
NTSTATUS ndr_push_time_t(struct ndr_push *ndr, int ndr_flags, time_t t)
{
	return ndr_push_uint32(ndr, ndr_flags, t);
}

/*
  pull a time_t
*/
NTSTATUS ndr_pull_time_t(struct ndr_pull *ndr, int ndr_flags, time_t *t)
{
	uint32_t tt;
	NDR_CHECK(ndr_pull_uint32(ndr, ndr_flags, &tt));
	*t = tt;
	return NT_STATUS_OK;
}


void ndr_print_struct(struct ndr_print *ndr, const char *name, const char *type)
{
	ndr->print(ndr, "%s: struct %s", name, type);
}

void ndr_print_enum(struct ndr_print *ndr, const char *name, const char *type, 
		    const char *val, uint32_t value)
{
	if (ndr->flags & LIBNDR_PRINT_ARRAY_HEX) {
		ndr->print(ndr, "%-25s: %s (0x%X)", name, val?val:"UNKNOWN_ENUM_VALUE", value);
	} else {
		ndr->print(ndr, "%-25s: %s (%d)", name, val?val:"UNKNOWN_ENUM_VALUE", value);
	}
}

void ndr_print_bitmap_flag(struct ndr_print *ndr, size_t size, const char *flag_name, uint32_t flag, uint32_t value)
{
	/* this is an attempt to support multi-bit bitmap masks */
	value &= flag;

	while (!(flag & 1)) {
		flag >>= 1;
		value >>= 1;
	}	
	if (flag == 1) {
		ndr->print(ndr, "   %d: %-25s", value, flag_name);
	} else {
		ndr->print(ndr, "0x%02x: %-25s (%d)", value, flag_name, value);
	}
}

void ndr_print_int8(struct ndr_print *ndr, const char *name, int8_t v)
{
	ndr->print(ndr, "%-25s: %d", name, v);
}

void ndr_print_uint8(struct ndr_print *ndr, const char *name, uint8_t v)
{
	ndr->print(ndr, "%-25s: 0x%02x (%u)", name, v, v);
}

void ndr_print_int16(struct ndr_print *ndr, const char *name, int16_t v)
{
	ndr->print(ndr, "%-25s: %d", name, v);
}

void ndr_print_uint16(struct ndr_print *ndr, const char *name, uint16_t v)
{
	ndr->print(ndr, "%-25s: 0x%04x (%u)", name, v, v);
}

void ndr_print_int32(struct ndr_print *ndr, const char *name, int32_t v)
{
	ndr->print(ndr, "%-25s: %d", name, v);
}

void ndr_print_uint32(struct ndr_print *ndr, const char *name, uint32_t v)
{
	ndr->print(ndr, "%-25s: 0x%08x (%u)", name, v, v);
}

void ndr_print_udlong(struct ndr_print *ndr, const char *name, uint64_t v)
{
	ndr->print(ndr, "%-25s: 0x%016llx (%llu)", name, v, v);
}

void ndr_print_udlongr(struct ndr_print *ndr, const char *name, uint64_t v)
{
	ndr_print_udlong(ndr, name, v);
}

void ndr_print_dlong(struct ndr_print *ndr, const char *name, int64_t v)
{
	ndr->print(ndr, "%-25s: 0x%016llx (%lld)", name, v, v);
}

void ndr_print_hyper(struct ndr_print *ndr, const char *name, uint64_t v)
{
	ndr_print_dlong(ndr, name, v);
}

void ndr_print_pointer(struct ndr_print *ndr, const char *name, void *v)
{
	ndr->print(ndr, "%-25s: %p", name, v);
}

void ndr_print_ptr(struct ndr_print *ndr, const char *name, const void *p)
{
	if (p) {
		ndr->print(ndr, "%-25s: *", name);
	} else {
		ndr->print(ndr, "%-25s: NULL", name);
	}
}

void ndr_print_NTTIME(struct ndr_print *ndr, const char *name, NTTIME t)
{
	ndr->print(ndr, "%-25s: %s", name, nt_time_string(ndr, t));
}

void ndr_print_NTTIME_1sec(struct ndr_print *ndr, const char *name, NTTIME t)
{
	/* this is a standard NTTIME here
	 * as it's already converted in the pull/push code
	 */
	ndr_print_NTTIME(ndr, name, t);
}

void ndr_print_NTTIME_hyper(struct ndr_print *ndr, const char *name, NTTIME t)
{
	ndr_print_NTTIME(ndr, name, t);
}

void ndr_print_time_t(struct ndr_print *ndr, const char *name, time_t t)
{
	if (t == (time_t)-1 || t == 0) {
		ndr->print(ndr, "%-25s: (time_t)%d", name, (int)t);
	} else {
		ndr->print(ndr, "%-25s: %s", name, timestring(ndr, t));
	}
}

void ndr_print_union(struct ndr_print *ndr, const char *name, int level, const char *type)
{
	if (ndr->flags & LIBNDR_PRINT_ARRAY_HEX) {
		ndr->print(ndr, "%-25s: union %s(case 0x%X)", name, type, level);
	} else {
		ndr->print(ndr, "%-25s: union %s(case %d)", name, type, level);
	}
}

void ndr_print_bad_level(struct ndr_print *ndr, const char *name, uint16_t level)
{
	ndr->print(ndr, "UNKNOWN LEVEL %u", level);
}

void ndr_print_array_uint8(struct ndr_print *ndr, const char *name, 
			   const uint8_t *data, uint32_t count)
{
	int i;

	if (count <= 600 && (ndr->flags & LIBNDR_PRINT_ARRAY_HEX)) {
		char s[1202];
		for (i=0;i<count;i++) {
			snprintf(&s[i*2], 3, "%02x", data[i]);
		}
		s[i*2] = 0;
		ndr->print(ndr, "%-25s: %s", name, s);
		return;
	}

	ndr->print(ndr, "%s: ARRAY(%d)", name, count);
	ndr->depth++;
	for (i=0;i<count;i++) {
		char *idx=NULL;
		asprintf(&idx, "[%d]", i);
		if (idx) {
			ndr_print_uint8(ndr, idx, data[i]);
			free(idx);
		}
	}
	ndr->depth--;	
}

void ndr_print_DATA_BLOB(struct ndr_print *ndr, const char *name, DATA_BLOB r)
{
	ndr->print(ndr, "%-25s: DATA_BLOB length=%u", name, r.length);
	if (r.length) {
		dump_data(10, (const char *)r.data, r.length);
	}
}


/*
  push a DATA_BLOB onto the wire. 
*/
NTSTATUS ndr_push_DATA_BLOB(struct ndr_push *ndr, int ndr_flags, DATA_BLOB blob)
{
	if (ndr->flags & LIBNDR_ALIGN_FLAGS) {
		if (ndr->flags & LIBNDR_FLAG_ALIGN2) {
			blob.length = NDR_ALIGN(ndr, 2);
		} else if (ndr->flags & LIBNDR_FLAG_ALIGN4) {
			blob.length = NDR_ALIGN(ndr, 4);
		} else if (ndr->flags & LIBNDR_FLAG_ALIGN8) {
			blob.length = NDR_ALIGN(ndr, 8);
		}
		NDR_PUSH_ALLOC_SIZE(ndr, blob.data, blob.length);
		data_blob_clear(&blob);
	} else if (!(ndr->flags & LIBNDR_FLAG_REMAINING)) {
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, blob.length));
	}
	NDR_CHECK(ndr_push_bytes(ndr, blob.data, blob.length));
	return NT_STATUS_OK;
}

/*
  pull a DATA_BLOB from the wire. 
*/
NTSTATUS ndr_pull_DATA_BLOB(struct ndr_pull *ndr, int ndr_flags, DATA_BLOB *blob)
{
	uint32_t length = 0;

	if (ndr->flags & LIBNDR_ALIGN_FLAGS) {
		if (ndr->flags & LIBNDR_FLAG_ALIGN2) {
			length = NDR_ALIGN(ndr, 2);
		} else if (ndr->flags & LIBNDR_FLAG_ALIGN4) {
			length = NDR_ALIGN(ndr, 4);
		} else if (ndr->flags & LIBNDR_FLAG_ALIGN8) {
			length = NDR_ALIGN(ndr, 8);
		}
		if (ndr->data_size - ndr->offset < length) {
			length = ndr->data_size - ndr->offset;
		}
	} else if (ndr->flags & LIBNDR_FLAG_REMAINING) {
		length = ndr->data_size - ndr->offset;
	} else {
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &length));
	}
	NDR_PULL_NEED_BYTES(ndr, length);
	*blob = data_blob_talloc(ndr->current_mem_ctx, ndr->data+ndr->offset, length);
	ndr->offset += length;
	return NT_STATUS_OK;
}

uint32_t ndr_size_DATA_BLOB(int ret, const DATA_BLOB *data, int flags)
{
	return ret + data->length;
}
