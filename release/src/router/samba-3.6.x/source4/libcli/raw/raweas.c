/* 
   Unix SMB/CIFS implementation.
   parsing of EA (extended attribute) lists
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
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"

/*
  work out how many bytes on the wire a ea list will consume. 
  This assumes the names are strict ascii, which should be a
  reasonable assumption
*/
size_t ea_list_size(unsigned int num_eas, struct ea_struct *eas)
{
	unsigned int total = 4;
	int i;
	for (i=0;i<num_eas;i++) {
		total += 4 + strlen(eas[i].name.s)+1 + eas[i].value.length;
	}
	return total;
}

/*
  work out how many bytes on the wire a ea name list will consume. 
*/
static unsigned int ea_name_list_size(unsigned int num_names, struct ea_name *eas)
{
	unsigned int total = 4;
	int i;
	for (i=0;i<num_names;i++) {
		total += 1 + strlen(eas[i].name.s) + 1;
	}
	return total;
}

/*
  work out how many bytes on the wire a chained ea list will consume.
  This assumes the names are strict ascii, which should be a
  reasonable assumption
*/
size_t ea_list_size_chained(unsigned int num_eas, struct ea_struct *eas, unsigned alignment)
{
	unsigned int total = 0;
	int i;
	for (i=0;i<num_eas;i++) {
		unsigned int len = 8 + strlen(eas[i].name.s)+1 + eas[i].value.length;
		len = (len + (alignment-1)) & ~(alignment-1);
		total += len;
	}
	return total;
}

/*
  put a ea_list into a pre-allocated buffer - buffer must be at least
  of size ea_list_size()
*/
void ea_put_list(uint8_t *data, unsigned int num_eas, struct ea_struct *eas)
{
	int i;
	uint32_t ea_size;

	ea_size = ea_list_size(num_eas, eas);

	SIVAL(data, 0, ea_size);
	data += 4;

	for (i=0;i<num_eas;i++) {
		unsigned int nlen = strlen(eas[i].name.s);
		SCVAL(data, 0, eas[i].flags);
		SCVAL(data, 1, nlen);
		SSVAL(data, 2, eas[i].value.length);
		memcpy(data+4, eas[i].name.s, nlen+1);
		memcpy(data+4+nlen+1, eas[i].value.data, eas[i].value.length);
		data += 4+nlen+1+eas[i].value.length;
	}
}


/*
  put a chained ea_list into a pre-allocated buffer - buffer must be
  at least of size ea_list_size()
*/
void ea_put_list_chained(uint8_t *data, unsigned int num_eas, struct ea_struct *eas,
			 unsigned alignment)
{
	int i;

	for (i=0;i<num_eas;i++) {
		unsigned int nlen = strlen(eas[i].name.s);
		uint32_t len = 8+nlen+1+eas[i].value.length;
		unsigned int pad = ((len + (alignment-1)) & ~(alignment-1)) - len;
		if (i == num_eas-1) {
			SIVAL(data, 0, 0);
		} else {
			SIVAL(data, 0, len+pad);
		}
		SCVAL(data, 4, eas[i].flags);
		SCVAL(data, 5, nlen);
		SSVAL(data, 6, eas[i].value.length);
		memcpy(data+8, eas[i].name.s, nlen+1);
		memcpy(data+8+nlen+1, eas[i].value.data, eas[i].value.length);
		memset(data+len, 0, pad);
		data += len + pad;
	}
}


/*
  pull a ea_struct from a buffer. Return the number of bytes consumed
*/
unsigned int ea_pull_struct(const DATA_BLOB *blob,
		      TALLOC_CTX *mem_ctx,
		      struct ea_struct *ea)
{
	uint8_t nlen;
	uint16_t vlen;

	ZERO_STRUCTP(ea);

	if (blob->length < 6) {
		return 0;
	}

	ea->flags = CVAL(blob->data, 0);
	nlen = CVAL(blob->data, 1);
	vlen = SVAL(blob->data, 2);

	if (nlen+1+vlen > blob->length-4) {
		return 0;
	}

	ea->name.s = talloc_strndup(mem_ctx, (const char *)(blob->data+4), nlen);
	ea->name.private_length = nlen;
	ea->value = data_blob_talloc(mem_ctx, NULL, vlen+1);
	if (!ea->value.data) return 0;
	if (vlen) {
		memcpy(ea->value.data, blob->data+4+nlen+1, vlen);
	}
	ea->value.data[vlen] = 0;
	ea->value.length--;

	return 4 + nlen+1 + vlen;
}


/*
  pull a ea_list from a buffer
*/
NTSTATUS ea_pull_list(const DATA_BLOB *blob, 
		      TALLOC_CTX *mem_ctx,
		      unsigned int *num_eas, struct ea_struct **eas)
{
	int n;
	uint32_t ea_size, ofs;

	if (blob->length < 4) {
		return NT_STATUS_INFO_LENGTH_MISMATCH;
	}

	ea_size = IVAL(blob->data, 0);
	if (ea_size > blob->length) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	ofs = 4;
	n = 0;
	*num_eas = 0;
	*eas = NULL;

	while (ofs < ea_size) {
		unsigned int len;
		DATA_BLOB blob2;

		blob2.data = blob->data + ofs;
		blob2.length = ea_size - ofs;

		*eas = talloc_realloc(mem_ctx, *eas, struct ea_struct, n+1);
		if (! *eas) return NT_STATUS_NO_MEMORY;

		len = ea_pull_struct(&blob2, mem_ctx, &(*eas)[n]);
		if (len == 0) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		ofs += len;
		n++;
	}

	*num_eas = n;

	return NT_STATUS_OK;
}


/*
  pull a chained ea_list from a buffer
*/
NTSTATUS ea_pull_list_chained(const DATA_BLOB *blob, 
			      TALLOC_CTX *mem_ctx,
			      unsigned int *num_eas, struct ea_struct **eas)
{
	int n;
	uint32_t ofs;

	if (blob->length < 4) {
		return NT_STATUS_INFO_LENGTH_MISMATCH;
	}

	ofs = 0;
	n = 0;
	*num_eas = 0;
	*eas = NULL;

	while (ofs < blob->length) {
		unsigned int len;
		DATA_BLOB blob2;
		uint32_t next_ofs = IVAL(blob->data, ofs);

		blob2.data = blob->data + ofs + 4;
		blob2.length = blob->length - (ofs + 4);

		*eas = talloc_realloc(mem_ctx, *eas, struct ea_struct, n+1);
		if (! *eas) return NT_STATUS_NO_MEMORY;

		len = ea_pull_struct(&blob2, mem_ctx, &(*eas)[n]);
		if (len == 0) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		ofs += next_ofs;

		if (ofs+4 > blob->length) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		n++;
		if (next_ofs == 0) break;
	}

	*num_eas = n;

	return NT_STATUS_OK;
}


/*
  pull a ea_name from a buffer. Return the number of bytes consumed
*/
static unsigned int ea_pull_name(const DATA_BLOB *blob,
			   TALLOC_CTX *mem_ctx,
			   struct ea_name *ea)
{
	uint8_t nlen;

	if (blob->length < 2) {
		return 0;
	}

	nlen = CVAL(blob->data, 0);

	if (nlen+2 > blob->length) {
		return 0;
	}

	ea->name.s = talloc_strndup(mem_ctx, (const char *)(blob->data+1), nlen);
	ea->name.private_length = nlen;

	return nlen+2;
}


/*
  pull a ea_name list from a buffer
*/
NTSTATUS ea_pull_name_list(const DATA_BLOB *blob, 
			   TALLOC_CTX *mem_ctx,
			   unsigned int *num_names, struct ea_name **ea_names)
{
	int n;
	uint32_t ea_size, ofs;

	if (blob->length < 4) {
		return NT_STATUS_INFO_LENGTH_MISMATCH;
	}

	ea_size = IVAL(blob->data, 0);
	if (ea_size > blob->length) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	ofs = 4;
	n = 0;
	*num_names = 0;
	*ea_names = NULL;

	while (ofs < ea_size) {
		unsigned int len;
		DATA_BLOB blob2;

		blob2.data = blob->data + ofs;
		blob2.length = ea_size - ofs;

		*ea_names = talloc_realloc(mem_ctx, *ea_names, struct ea_name, n+1);
		if (! *ea_names) return NT_STATUS_NO_MEMORY;

		len = ea_pull_name(&blob2, mem_ctx, &(*ea_names)[n]);
		if (len == 0) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		ofs += len;
		n++;
	}

	*num_names = n;

	return NT_STATUS_OK;
}


/*
  put a ea_name list into a data blob
*/
bool ea_push_name_list(TALLOC_CTX *mem_ctx,
		       DATA_BLOB *data, unsigned int num_names, struct ea_name *eas)
{
	int i;
	uint32_t ea_size;
	uint32_t off;

	ea_size = ea_name_list_size(num_names, eas);

	*data = data_blob_talloc(mem_ctx, NULL, ea_size);
	if (data->data == NULL) {
		return false;
	}

	SIVAL(data->data, 0, ea_size);
	off = 4;

	for (i=0;i<num_names;i++) {
		unsigned int nlen = strlen(eas[i].name.s);
		SCVAL(data->data, off, nlen);
		memcpy(data->data+off+1, eas[i].name.s, nlen+1);
		off += 1+nlen+1;
	}

	return true;
}
