/* 
   Unix SMB/CIFS implementation.

   libndr interface

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

/*
  this provides the core routines for NDR parsing functions

  see http://www.opengroup.org/onlinepubs/9629399/chap14.htm for details
  of NDR encoding rules
*/

#include "includes.h"

#define NDR_BASE_MARSHALL_SIZE 1024

/* this guid indicates NDR encoding in a protocol tower */
const struct dcerpc_syntax_id ndr_transfer_syntax = {
  { 0x8a885d04, 0x1ceb, 0x11c9, {0x9f, 0xe8}, {0x08,0x00,0x2b,0x10,0x48,0x60} },
  2
};

const struct dcerpc_syntax_id ndr64_transfer_syntax = {
  { 0x71710533, 0xbeba, 0x4937, {0x83, 0x19}, {0xb5,0xdb,0xef,0x9c,0xcc,0x36} },
  1
};

/*
  work out the number of bytes needed to align on a n byte boundary
*/
size_t ndr_align_size(uint32_t offset, size_t n)
{
	if ((offset & (n-1)) == 0) return 0;
	return n - (offset & (n-1));
}

/*
  initialise a ndr parse structure from a data blob
*/
struct ndr_pull *ndr_pull_init_blob(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx)
{
	struct ndr_pull *ndr;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	if (!ndr) return NULL;
	ndr->current_mem_ctx = mem_ctx;

	ndr->data = blob->data;
	ndr->data_size = blob->length;

	return ndr;
}

/*
  advance by 'size' bytes
*/
NTSTATUS ndr_pull_advance(struct ndr_pull *ndr, uint32_t size)
{
	ndr->offset += size;
	if (ndr->offset > ndr->data_size) {
		return ndr_pull_error(ndr, NDR_ERR_BUFSIZE, 
				      "ndr_pull_advance by %u failed",
				      size);
	}
	return NT_STATUS_OK;
}

/*
  set the parse offset to 'ofs'
*/
static NTSTATUS ndr_pull_set_offset(struct ndr_pull *ndr, uint32_t ofs)
{
	ndr->offset = ofs;
	if (ndr->offset > ndr->data_size) {
		return ndr_pull_error(ndr, NDR_ERR_BUFSIZE, 
				      "ndr_pull_set_offset %u failed",
				      ofs);
	}
	return NT_STATUS_OK;
}

/* save the offset/size of the current ndr state */
void ndr_pull_save(struct ndr_pull *ndr, struct ndr_pull_save *save)
{
	save->offset = ndr->offset;
	save->data_size = ndr->data_size;
}

/* restore the size/offset of a ndr structure */
void ndr_pull_restore(struct ndr_pull *ndr, struct ndr_pull_save *save)
{
	ndr->offset = save->offset;
	ndr->data_size = save->data_size;
}


/* create a ndr_push structure, ready for some marshalling */
struct ndr_push *ndr_push_init_ctx(TALLOC_CTX *mem_ctx)
{
	struct ndr_push *ndr;

	ndr = talloc_zero(mem_ctx, struct ndr_push);
	if (!ndr) {
		return NULL;
	}

	ndr->flags = 0;
	ndr->alloc_size = NDR_BASE_MARSHALL_SIZE;
	ndr->data = talloc_array(ndr, uint8_t, ndr->alloc_size);
	if (!ndr->data) {
		return NULL;
	}

	return ndr;
}


/* create a ndr_push structure, ready for some marshalling */
struct ndr_push *ndr_push_init(void)
{
	return ndr_push_init_ctx(NULL);
}

/* free a ndr_push structure */
void ndr_push_free(struct ndr_push *ndr)
{
	talloc_free(ndr);
}


/* return a DATA_BLOB structure for the current ndr_push marshalled data */
DATA_BLOB ndr_push_blob(struct ndr_push *ndr)
{
	DATA_BLOB blob;
	blob.data = ndr->data;
	blob.length = ndr->offset;
	blob.free = NULL;
	if (ndr->alloc_size > ndr->offset) {
		ndr->data[ndr->offset] = 0;
	}
	return blob;
}


/*
  expand the available space in the buffer to ndr->offset + extra_size
*/
NTSTATUS ndr_push_expand(struct ndr_push *ndr, uint32_t extra_size)
{
	uint32_t size = extra_size + ndr->offset;

	if (size < ndr->offset) {
		/* extra_size overflowed the offset */
		return ndr_push_error(ndr, NDR_ERR_BUFSIZE, "Overflow in push_expand to %u",
					size);
	}

	if (ndr->alloc_size > size) {
		return NT_STATUS_OK;
	}

	ndr->alloc_size += NDR_BASE_MARSHALL_SIZE;
	if (size+1 > ndr->alloc_size) {
		ndr->alloc_size = size+1;
	}
	ndr->data = talloc_realloc(ndr, ndr->data, uint8_t, ndr->alloc_size);
	if (!ndr->data) {
		return ndr_push_error(ndr, NDR_ERR_ALLOC, "Failed to push_expand to %u",
				      ndr->alloc_size);
	}

	return NT_STATUS_OK;
}

void ndr_print_debug_helper(struct ndr_print *ndr, const char *format, ...) _PRINTF_ATTRIBUTE(2,3)
{
	va_list ap;
	char *s = NULL;
	int i;

	va_start(ap, format);
	vasprintf(&s, format, ap);
	va_end(ap);

	for (i=0;i<ndr->depth;i++) {
		DEBUG(0,("    "));
	}

	DEBUG(0,("%s\n", s));
	free(s);
}

static void ndr_print_string_helper(struct ndr_print *ndr, const char *format, ...) _PRINTF_ATTRIBUTE(2,3)
{
	va_list ap;
	int i;

	for (i=0;i<ndr->depth;i++) {
		ndr->private_data = talloc_asprintf_append(
			(char *)ndr->private_data, "    ");
	}

	va_start(ap, format);
	ndr->private_data = talloc_vasprintf_append(
		(char *)ndr->private_data, format, ap);
	va_end(ap);
	ndr->private_data = talloc_asprintf_append(
		(char *)ndr->private_data, "\n");
}

/*
  a useful helper function for printing idl structures via DEBUG()
*/
void ndr_print_debug(ndr_print_fn_t fn, const char *name, void *ptr)
{
	struct ndr_print *ndr;

	ndr = talloc_zero(NULL, struct ndr_print);
	if (!ndr) return;
	ndr->print = ndr_print_debug_helper;
	ndr->depth = 1;
	ndr->flags = 0;
	fn(ndr, name, ptr);
	talloc_free(ndr);
}

/*
  a useful helper function for printing idl unions via DEBUG()
*/
void ndr_print_union_debug(ndr_print_fn_t fn, const char *name, uint32_t level, void *ptr)
{
	struct ndr_print *ndr;

	ndr = talloc_zero(NULL, struct ndr_print);
	if (!ndr) return;
	ndr->print = ndr_print_debug_helper;
	ndr->depth = 1;
	ndr->flags = 0;
	ndr_print_set_switch_value(ndr, ptr, level);
	fn(ndr, name, ptr);
	talloc_free(ndr);
}

/*
  a useful helper function for printing idl function calls via DEBUG()
*/
void ndr_print_function_debug(ndr_print_function_t fn, const char *name, int flags, void *ptr)
{
	struct ndr_print *ndr;

	ndr = talloc_zero(NULL, struct ndr_print);
	if (!ndr) return;
	ndr->print = ndr_print_debug_helper;
	ndr->depth = 1;
	ndr->flags = 0;
	fn(ndr, name, flags, ptr);
	talloc_free(ndr);
}


/*
  a useful helper function for printing idl function calls to a string
*/
char *ndr_print_function_string(TALLOC_CTX *mem_ctx,
				ndr_print_function_t fn, const char *name, 
				int flags, void *ptr)
{
	struct ndr_print *ndr;
	char *ret = NULL;

	ndr = talloc_zero(mem_ctx, struct ndr_print);
	if (!ndr) return NULL;
	if (!(ndr->private_data = talloc_strdup(mem_ctx, ""))) {
		TALLOC_FREE(ndr);
		return NULL;
	}
	ndr->print = ndr_print_string_helper;
	ndr->depth = 1;
	ndr->flags = 0;
	fn(ndr, name, flags, ptr);
	ret = (char *)ndr->private_data;
	talloc_free(ndr);
	return ret;
}

void ndr_set_flags(uint32_t *pflags, uint32_t new_flags)
{
	/* the big/little endian flags are inter-dependent */
	if (new_flags & LIBNDR_FLAG_LITTLE_ENDIAN) {
		(*pflags) &= ~LIBNDR_FLAG_BIGENDIAN;
	}
	if (new_flags & LIBNDR_FLAG_BIGENDIAN) {
		(*pflags) &= ~LIBNDR_FLAG_LITTLE_ENDIAN;
	}
	if (new_flags & LIBNDR_FLAG_REMAINING) {
		(*pflags) &= ~LIBNDR_ALIGN_FLAGS;
	}
	if (new_flags & LIBNDR_ALIGN_FLAGS) {
		(*pflags) &= ~LIBNDR_FLAG_REMAINING;
	}
	(*pflags) |= new_flags;
}

static NTSTATUS ndr_map_error(enum ndr_err_code ndr_err)
{
	switch (ndr_err) {
	case NDR_ERR_BUFSIZE:
		return NT_STATUS_BUFFER_TOO_SMALL;
	case NDR_ERR_TOKEN:
		return NT_STATUS_INTERNAL_ERROR;
	case NDR_ERR_ALLOC:
		return NT_STATUS_NO_MEMORY;
	case NDR_ERR_ARRAY_SIZE:
		return NT_STATUS_ARRAY_BOUNDS_EXCEEDED;
	default:
		break;
	}

	/* we should map all error codes to different status codes */
	return NT_STATUS_INVALID_PARAMETER;
}

/*
  return and possibly log an NDR error
*/
NTSTATUS ndr_pull_error(struct ndr_pull *ndr,
				 enum ndr_err_code ndr_err,
				 const char *format, ...) _PRINTF_ATTRIBUTE(3,4)
{
	char *s=NULL;
	va_list ap;

	va_start(ap, format);
	vasprintf(&s, format, ap);
	va_end(ap);

	DEBUG(3,("ndr_pull_error(%u): %s\n", ndr_err, s));

	free(s);

	return ndr_map_error(ndr_err);
}

/*
  return and possibly log an NDR error
*/
NTSTATUS ndr_push_error(struct ndr_push *ndr,
				 enum ndr_err_code ndr_err,
				 const char *format, ...)  _PRINTF_ATTRIBUTE(3,4)
{
	char *s=NULL;
	va_list ap;

	va_start(ap, format);
	vasprintf(&s, format, ap);
	va_end(ap);

	DEBUG(3,("ndr_push_error(%u): %s\n", ndr_err, s));

	free(s);

	return ndr_map_error(ndr_err);
}

/*
  handle subcontext buffers, which in midl land are user-marshalled, but
  we use magic in pidl to make them easier to cope with
*/
NTSTATUS ndr_pull_subcontext_start(struct ndr_pull *ndr, 
				   struct ndr_pull **_subndr,
				   size_t header_size,
				   ssize_t size_is)
{
	struct ndr_pull *subndr;
	uint32_t r_content_size;

	switch (header_size) {
	case 0: {
		uint32_t content_size = ndr->data_size - ndr->offset;
		if (size_is >= 0) {
			content_size = size_is;
		}
		r_content_size = content_size;
		break;
	}

	case 2: {
		uint16_t content_size;
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &content_size));
		if (size_is >= 0 && size_is != content_size) {
			return ndr_pull_error(ndr, NDR_ERR_SUBCONTEXT, "Bad subcontext (PULL) size_is(%d) mismatch content_size %d", 
						(int)size_is, (int)content_size);
		}
		r_content_size = content_size;
		break;
	}

	case 4: {
		uint32_t content_size;
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &content_size));
		if (size_is >= 0 && size_is != content_size) {
			return ndr_pull_error(ndr, NDR_ERR_SUBCONTEXT, "Bad subcontext (PULL) size_is(%d) mismatch content_size %d", 
						(int)size_is, (int)content_size);
		}
		r_content_size = content_size;
		break;
	}
	default:
		return ndr_pull_error(ndr, NDR_ERR_SUBCONTEXT, "Bad subcontext (PULL) header_size %d", 
				      (int)header_size);
	}

	NDR_PULL_NEED_BYTES(ndr, r_content_size);

	subndr = talloc_zero(ndr, struct ndr_pull);
	NT_STATUS_HAVE_NO_MEMORY(subndr);
	subndr->flags		= ndr->flags;
	subndr->current_mem_ctx	= ndr->current_mem_ctx;

	subndr->data = ndr->data + ndr->offset;
	subndr->offset = 0;
	subndr->data_size = r_content_size;

	*_subndr = subndr;
	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_subcontext_end(struct ndr_pull *ndr, 
				 struct ndr_pull *subndr,
				 size_t header_size,
				 ssize_t size_is)
{
	uint32_t advance;
	if (size_is >= 0) {
		advance = size_is;
	} else if (header_size > 0) {
		advance = subndr->data_size;
	} else {
		advance = subndr->offset;
	}
	NDR_CHECK(ndr_pull_advance(ndr, advance));
	return NT_STATUS_OK;
}

NTSTATUS ndr_push_subcontext_start(struct ndr_push *ndr,
				   struct ndr_push **_subndr,
				   size_t header_size,
				   ssize_t size_is)
{
	struct ndr_push *subndr;

	subndr = ndr_push_init_ctx(ndr);
	NT_STATUS_HAVE_NO_MEMORY(subndr);
	subndr->flags	= ndr->flags;

	*_subndr = subndr;
	return NT_STATUS_OK;
}

/*
  push a subcontext header 
*/
NTSTATUS ndr_push_subcontext_end(struct ndr_push *ndr,
				 struct ndr_push *subndr,
				 size_t header_size,
				 ssize_t size_is)
{
	if (size_is >= 0) {
		ssize_t padding_len = size_is - subndr->offset;
		if (padding_len > 0) {
			NDR_CHECK(ndr_push_zero(subndr, padding_len));
		} else if (padding_len < 0) {
			return ndr_push_error(ndr, NDR_ERR_SUBCONTEXT, "Bad subcontext (PUSH) content_size %d is larger than size_is(%d)",
					      (int)subndr->offset, (int)size_is);
		}
	}

	switch (header_size) {
	case 0: 
		break;

	case 2: 
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, subndr->offset));
		break;

	case 4: 
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, subndr->offset));
		break;

	default:
		return ndr_push_error(ndr, NDR_ERR_SUBCONTEXT, "Bad subcontext header size %d", 
				      (int)header_size);
	}

	NDR_CHECK(ndr_push_bytes(ndr, subndr->data, subndr->offset));
	return NT_STATUS_OK;
}

/*
  store a token in the ndr context, for later retrieval
*/
NTSTATUS ndr_token_store(TALLOC_CTX *mem_ctx, 
			 struct ndr_token_list **list, 
			 const void *key, 
			 uint32_t value)
{
	struct ndr_token_list *tok;
	tok = talloc(mem_ctx, struct ndr_token_list);
	if (tok == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	tok->key = key;
	tok->value = value;
	DLIST_ADD((*list), tok);
	return NT_STATUS_OK;
}

/*
  retrieve a token from a ndr context, using cmp_fn to match the tokens
*/
NTSTATUS ndr_token_retrieve_cmp_fn(struct ndr_token_list **list, const void *key, uint32_t *v,
				   comparison_fn_t _cmp_fn, BOOL _remove_tok)
{
	struct ndr_token_list *tok;
	for (tok=*list;tok;tok=tok->next) {
		if (_cmp_fn && _cmp_fn(tok->key,key)==0) goto found;
		else if (!_cmp_fn && tok->key == key) goto found;
	}
	return ndr_map_error(NDR_ERR_TOKEN);
found:
	*v = tok->value;
	if (_remove_tok) {
		DLIST_REMOVE((*list), tok);
		talloc_free(tok);
	}
	return NT_STATUS_OK;		
}

/*
  retrieve a token from a ndr context
*/
NTSTATUS ndr_token_retrieve(struct ndr_token_list **list, const void *key, uint32_t *v)
{
	return ndr_token_retrieve_cmp_fn(list, key, v, NULL, True);
}

/*
  peek at but don't removed a token from a ndr context
*/
uint32_t ndr_token_peek(struct ndr_token_list **list, const void *key)
{
	NTSTATUS status;
	uint32_t v;
	status = ndr_token_retrieve_cmp_fn(list, key, &v, NULL, False);
	if (NT_STATUS_IS_OK(status)) return v;
	return 0;
}

/*
  pull an array size field and add it to the array_size_list token list
*/
NTSTATUS ndr_pull_array_size(struct ndr_pull *ndr, const void *p)
{
	uint32_t size;
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &size));
	return ndr_token_store(ndr, &ndr->array_size_list, p, size);
}

/*
  get the stored array size field
*/
uint32_t ndr_get_array_size(struct ndr_pull *ndr, const void *p)
{
	return ndr_token_peek(&ndr->array_size_list, p);
}

/*
  check the stored array size field
*/
NTSTATUS ndr_check_array_size(struct ndr_pull *ndr, void *p, uint32_t size)
{
	uint32_t stored;
	stored = ndr_token_peek(&ndr->array_size_list, p);
	if (stored != size) {
		return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, 
				      "Bad array size - got %u expected %u\n",
				      stored, size);
	}
	return NT_STATUS_OK;
}

/*
  pull an array length field and add it to the array_length_list token list
*/
NTSTATUS ndr_pull_array_length(struct ndr_pull *ndr, const void *p)
{
	uint32_t length, offset;
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &offset));
	if (offset != 0) {
		return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, 
				      "non-zero array offset %u\n", offset);
	}
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &length));
	return ndr_token_store(ndr, &ndr->array_length_list, p, length);
}

/*
  get the stored array length field
*/
uint32_t ndr_get_array_length(struct ndr_pull *ndr, const void *p)
{
	return ndr_token_peek(&ndr->array_length_list, p);
}

/*
  check the stored array length field
*/
NTSTATUS ndr_check_array_length(struct ndr_pull *ndr, void *p, uint32_t length)
{
	uint32_t stored;
	stored = ndr_token_peek(&ndr->array_length_list, p);
	if (stored != length) {
		return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, 
				      "Bad array length - got %u expected %u\n",
				      stored, length);
	}
	return NT_STATUS_OK;
}

/*
  store a switch value
 */
NTSTATUS ndr_push_set_switch_value(struct ndr_push *ndr, const void *p, uint32_t val)
{
	return ndr_token_store(ndr, &ndr->switch_list, p, val);
}

NTSTATUS ndr_pull_set_switch_value(struct ndr_pull *ndr, const void *p, uint32_t val)
{
	return ndr_token_store(ndr, &ndr->switch_list, p, val);
}

NTSTATUS ndr_print_set_switch_value(struct ndr_print *ndr, const void *p, uint32_t val)
{
	return ndr_token_store(ndr, &ndr->switch_list, p, val);
}

/*
  retrieve a switch value
 */
uint32_t ndr_push_get_switch_value(struct ndr_push *ndr, const void *p)
{
	return ndr_token_peek(&ndr->switch_list, p);
}

uint32_t ndr_pull_get_switch_value(struct ndr_pull *ndr, const void *p)
{
	return ndr_token_peek(&ndr->switch_list, p);
}

uint32_t ndr_print_get_switch_value(struct ndr_print *ndr, const void *p)
{
	return ndr_token_peek(&ndr->switch_list, p);
}

/*
  pull a struct from a blob using NDR
*/
NTSTATUS ndr_pull_struct_blob(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx, void *p,
			      ndr_pull_flags_fn_t fn)
{
	struct ndr_pull *ndr;
	ndr = ndr_pull_init_blob(blob, mem_ctx);
	if (!ndr) {
		return NT_STATUS_NO_MEMORY;
	}
	return fn(ndr, NDR_SCALARS|NDR_BUFFERS, p);
}

/*
  pull a struct from a blob using NDR - failing if all bytes are not consumed
*/
NTSTATUS ndr_pull_struct_blob_all(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx, void *p,
				  ndr_pull_flags_fn_t fn)
{
	struct ndr_pull *ndr;
	NTSTATUS status;

	ndr = ndr_pull_init_blob(blob, mem_ctx);
	if (!ndr) {
		return NT_STATUS_NO_MEMORY;
	}
	status = fn(ndr, NDR_SCALARS|NDR_BUFFERS, p);
	if (!NT_STATUS_IS_OK(status)) return status;
	if (ndr->offset != ndr->data_size) {
		return NT_STATUS_BUFFER_TOO_SMALL;
	}
	return status;
}

/*
  pull a union from a blob using NDR, given the union discriminator
*/
NTSTATUS ndr_pull_union_blob(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx, void *p,
			     uint32_t level, ndr_pull_flags_fn_t fn)
{
	struct ndr_pull *ndr;
	NTSTATUS status;

	ndr = ndr_pull_init_blob(blob, mem_ctx);
	if (!ndr) {
		return NT_STATUS_NO_MEMORY;
	}
	ndr_pull_set_switch_value(ndr, p, level);
	status = fn(ndr, NDR_SCALARS|NDR_BUFFERS, p);
	if (!NT_STATUS_IS_OK(status)) return status;
	if (ndr->offset != ndr->data_size) {
		return NT_STATUS_BUFFER_TOO_SMALL;
	}
	return status;
}

/*
  push a struct to a blob using NDR
*/
NTSTATUS ndr_push_struct_blob(DATA_BLOB *blob, TALLOC_CTX *mem_ctx, const void *p,
			      ndr_push_flags_fn_t fn)
{
	NTSTATUS status;
	struct ndr_push *ndr;
	ndr = ndr_push_init_ctx(mem_ctx);
	if (!ndr) {
		return NT_STATUS_NO_MEMORY;
	}
	status = fn(ndr, NDR_SCALARS|NDR_BUFFERS, p);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	*blob = ndr_push_blob(ndr);

	return NT_STATUS_OK;
}

/*
  push a union to a blob using NDR
*/
NTSTATUS ndr_push_union_blob(DATA_BLOB *blob, TALLOC_CTX *mem_ctx, void *p,
			     uint32_t level, ndr_push_flags_fn_t fn)
{
	NTSTATUS status;
	struct ndr_push *ndr;
	ndr = ndr_push_init_ctx(mem_ctx);
	if (!ndr) {
		return NT_STATUS_NO_MEMORY;
	}
	ndr_push_set_switch_value(ndr, p, level);
	status = fn(ndr, NDR_SCALARS|NDR_BUFFERS, p);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	*blob = ndr_push_blob(ndr);

	return NT_STATUS_OK;
}

/*
  generic ndr_size_*() handler for structures
*/
size_t ndr_size_struct(const void *p, int flags, ndr_push_flags_fn_t push)
{
	struct ndr_push *ndr;
	NTSTATUS status;
	size_t ret;

	/* avoid recursion */
	if (flags & LIBNDR_FLAG_NO_NDR_SIZE) return 0;

	ndr = ndr_push_init_ctx(NULL);
	if (!ndr) return 0;
	ndr->flags |= flags | LIBNDR_FLAG_NO_NDR_SIZE;
	status = push(ndr, NDR_SCALARS|NDR_BUFFERS, p);
	if (!NT_STATUS_IS_OK(status)) {
		return 0;
	}
	ret = ndr->offset;
	talloc_free(ndr);
	return ret;
}

/*
  generic ndr_size_*() handler for unions
*/
size_t ndr_size_union(const void *p, int flags, uint32_t level, ndr_push_flags_fn_t push)
{
	struct ndr_push *ndr;
	NTSTATUS status;
	size_t ret;

	/* avoid recursion */
	if (flags & LIBNDR_FLAG_NO_NDR_SIZE) return 0;

	ndr = ndr_push_init_ctx(NULL);
	if (!ndr) return 0;
	ndr->flags |= flags | LIBNDR_FLAG_NO_NDR_SIZE;
	ndr_push_set_switch_value(ndr, p, level);
	status = push(ndr, NDR_SCALARS|NDR_BUFFERS, p);
	if (!NT_STATUS_IS_OK(status)) {
		return 0;
	}
	ret = ndr->offset;
	talloc_free(ndr);
	return ret;
}

/*
  get the current base for relative pointers for the push
*/
uint32_t ndr_push_get_relative_base_offset(struct ndr_push *ndr)
{
	return ndr->relative_base_offset;
}

/*
  restore the old base for relative pointers for the push
*/
void ndr_push_restore_relative_base_offset(struct ndr_push *ndr, uint32_t offset)
{
	ndr->relative_base_offset = offset;
}

/*
  setup the current base for relative pointers for the push
  called in the NDR_SCALAR stage
*/
NTSTATUS ndr_push_setup_relative_base_offset1(struct ndr_push *ndr, const void *p, uint32_t offset)
{
	ndr->relative_base_offset = offset;
	return ndr_token_store(ndr, &ndr->relative_base_list, p, offset);
}

/*
  setup the current base for relative pointers for the push
  called in the NDR_BUFFERS stage
*/
NTSTATUS ndr_push_setup_relative_base_offset2(struct ndr_push *ndr, const void *p)
{
	return ndr_token_retrieve(&ndr->relative_base_list, p, &ndr->relative_base_offset);
}

/*
  push a relative object - stage1
  this is called during SCALARS processing
*/
NTSTATUS ndr_push_relative_ptr1(struct ndr_push *ndr, const void *p)
{
	if (p == NULL) {
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 0));
		return NT_STATUS_OK;
	}
	NDR_CHECK(ndr_push_align(ndr, 4));
	NDR_CHECK(ndr_token_store(ndr, &ndr->relative_list, p, ndr->offset));
	return ndr_push_uint32(ndr, NDR_SCALARS, 0xFFFFFFFF);
}

/*
  push a relative object - stage2
  this is called during buffers processing
*/
NTSTATUS ndr_push_relative_ptr2(struct ndr_push *ndr, const void *p)
{
	struct ndr_push_save save;
	uint32_t ptr_offset = 0xFFFFFFFF;
	if (p == NULL) {
		return NT_STATUS_OK;
	}
	ndr_push_save(ndr, &save);
	NDR_CHECK(ndr_token_retrieve(&ndr->relative_list, p, &ptr_offset));
	if (ptr_offset > ndr->offset) {
		return ndr_push_error(ndr, NDR_ERR_BUFSIZE, 
				      "ndr_push_relative_ptr2 ptr_offset(%u) > ndr->offset(%u)",
				      ptr_offset, ndr->offset);
	}
	ndr->offset = ptr_offset;
	if (save.offset < ndr->relative_base_offset) {
		return ndr_push_error(ndr, NDR_ERR_BUFSIZE, 
				      "ndr_push_relative_ptr2 save.offset(%u) < ndr->relative_base_offset(%u)",
				      save.offset, ndr->relative_base_offset);
	}	
	NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, save.offset - ndr->relative_base_offset));
	ndr_push_restore(ndr, &save);
	return NT_STATUS_OK;
}

/*
  get the current base for relative pointers for the pull
*/
uint32_t ndr_pull_get_relative_base_offset(struct ndr_pull *ndr)
{
	return ndr->relative_base_offset;
}

/*
  restore the old base for relative pointers for the pull
*/
void ndr_pull_restore_relative_base_offset(struct ndr_pull *ndr, uint32_t offset)
{
	ndr->relative_base_offset = offset;
}

/*
  setup the current base for relative pointers for the pull
  called in the NDR_SCALAR stage
*/
NTSTATUS ndr_pull_setup_relative_base_offset1(struct ndr_pull *ndr, const void *p, uint32_t offset)
{
	ndr->relative_base_offset = offset;
	return ndr_token_store(ndr, &ndr->relative_base_list, p, offset);
}

/*
  setup the current base for relative pointers for the pull
  called in the NDR_BUFFERS stage
*/
NTSTATUS ndr_pull_setup_relative_base_offset2(struct ndr_pull *ndr, const void *p)
{
	return ndr_token_retrieve(&ndr->relative_base_list, p, &ndr->relative_base_offset);
}

/*
  pull a relative object - stage1
  called during SCALARS processing
*/
NTSTATUS ndr_pull_relative_ptr1(struct ndr_pull *ndr, const void *p, uint32_t rel_offset)
{
	rel_offset += ndr->relative_base_offset;
	if (rel_offset > ndr->data_size) {
		return ndr_pull_error(ndr, NDR_ERR_BUFSIZE, 
				      "ndr_pull_relative_ptr1 rel_offset(%u) > ndr->data_size(%u)",
				      rel_offset, ndr->data_size);
	}
	return ndr_token_store(ndr, &ndr->relative_list, p, rel_offset);
}

/*
  pull a relative object - stage2
  called during BUFFERS processing
*/
NTSTATUS ndr_pull_relative_ptr2(struct ndr_pull *ndr, const void *p)
{
	uint32_t rel_offset;
	NDR_CHECK(ndr_token_retrieve(&ndr->relative_list, p, &rel_offset));
	return ndr_pull_set_offset(ndr, rel_offset);
}
