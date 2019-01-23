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

/*
  this provides the core routines for NDR parsing functions

  see http://www.opengroup.org/onlinepubs/9629399/chap14.htm for details
  of NDR encoding rules
*/

#include "includes.h"
#include "librpc/ndr/libndr.h"
#include "../lib/util/dlinklist.h"
#if _SAMBA_BUILD_ == 4
#include "param/param.h"
#endif

#define NDR_BASE_MARSHALL_SIZE 1024

/* this guid indicates NDR encoding in a protocol tower */
const struct ndr_syntax_id ndr_transfer_syntax = {
  { 0x8a885d04, 0x1ceb, 0x11c9, {0x9f, 0xe8}, {0x08,0x00,0x2b,0x10,0x48,0x60} },
  2
};

const struct ndr_syntax_id ndr64_transfer_syntax = {
  { 0x71710533, 0xbeba, 0x4937, {0x83, 0x19}, {0xb5,0xdb,0xef,0x9c,0xcc,0x36} },
  1
};

const struct ndr_syntax_id null_ndr_syntax_id = {
  { 0, 0, 0, { 0, 0 }, { 0, 0, 0, 0, 0, 0 } },
  0
};

/*
  work out the number of bytes needed to align on a n byte boundary
*/
_PUBLIC_ size_t ndr_align_size(uint32_t offset, size_t n)
{
	if ((offset & (n-1)) == 0) return 0;
	return n - (offset & (n-1));
}

/*
  initialise a ndr parse structure from a data blob
*/
_PUBLIC_ struct ndr_pull *ndr_pull_init_blob(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx)
{
	struct ndr_pull *ndr;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	if (!ndr) return NULL;
	ndr->current_mem_ctx = mem_ctx;

	ndr->data = blob->data;
	ndr->data_size = blob->length;

	return ndr;
}

_PUBLIC_ enum ndr_err_code ndr_pull_append(struct ndr_pull *ndr, DATA_BLOB *blob)
{
	enum ndr_err_code ndr_err;
	DATA_BLOB b;
	uint32_t append = 0;
	bool ok;

	if (blob->length == 0) {
		return NDR_ERR_SUCCESS;
	}

	ndr_err = ndr_token_retrieve(&ndr->array_size_list, ndr, &append);
	if (ndr_err == NDR_ERR_TOKEN) {
		append = 0;
		ndr_err = NDR_ERR_SUCCESS;
	}
	NDR_CHECK(ndr_err);

	if (ndr->data_size == 0) {
		ndr->data = NULL;
		append = UINT32_MAX;
	}

	if (append == UINT32_MAX) {
		/*
		 * append == UINT32_MAX means that
		 * ndr->data is either NULL or a valid
		 * talloc child of ndr, which means
		 * we can use data_blob_append() without
		 * data_blob_talloc() of the existing callers data
		 */
		b = data_blob_const(ndr->data, ndr->data_size);
	} else {
		b = data_blob_talloc(ndr, ndr->data, ndr->data_size);
		if (b.data == NULL) {
			return ndr_pull_error(ndr, NDR_ERR_ALLOC, "%s", __location__);
		}
	}

	ok = data_blob_append(ndr, &b, blob->data, blob->length);
	if (!ok) {
		return ndr_pull_error(ndr, NDR_ERR_ALLOC, "%s", __location__);
	}

	ndr->data = b.data;
	ndr->data_size = b.length;

	return ndr_token_store(ndr, &ndr->array_size_list, ndr, UINT32_MAX);
}

_PUBLIC_ enum ndr_err_code ndr_pull_pop(struct ndr_pull *ndr)
{
	uint32_t skip = 0;
	uint32_t append = 0;

	if (ndr->relative_base_offset != 0) {
		return ndr_pull_error(ndr, NDR_ERR_RELATIVE,
				      "%s", __location__);
	}
	if (ndr->relative_highest_offset != 0) {
		return ndr_pull_error(ndr, NDR_ERR_RELATIVE,
				      "%s", __location__);
	}
	if (ndr->relative_list != NULL) {
		return ndr_pull_error(ndr, NDR_ERR_RELATIVE,
				      "%s", __location__);
	}
	if (ndr->relative_base_list != NULL) {
		return ndr_pull_error(ndr, NDR_ERR_RELATIVE,
				      "%s", __location__);
	}

	/*
	 * we need to keep up to 7 bytes
	 * in order to get the aligment right.
	 */
	skip = ndr->offset & 0xFFFFFFF8;

	if (skip == 0) {
		return NDR_ERR_SUCCESS;
	}

	ndr->offset -= skip;
	ndr->data_size -= skip;

	append = ndr_token_peek(&ndr->array_size_list, ndr);
	if (append != UINT32_MAX) {
		/*
		 * here we assume, that ndr->data is not a
		 * talloc child of ndr.
		 */
		ndr->data += skip;
		return NDR_ERR_SUCCESS;
	}

	memmove(ndr->data, ndr->data + skip, ndr->data_size);

	ndr->data = talloc_realloc(ndr, ndr->data, uint8_t, ndr->data_size);
	if (ndr->data_size != 0 && ndr->data == NULL) {
		return ndr_pull_error(ndr, NDR_ERR_ALLOC, "%s", __location__);
	}

	return NDR_ERR_SUCCESS;
}

/*
  advance by 'size' bytes
*/
_PUBLIC_ enum ndr_err_code ndr_pull_advance(struct ndr_pull *ndr, uint32_t size)
{
	ndr->offset += size;
	if (ndr->offset > ndr->data_size) {
		return ndr_pull_error(ndr, NDR_ERR_BUFSIZE, 
				      "ndr_pull_advance by %u failed",
				      size);
	}
	return NDR_ERR_SUCCESS;
}

/*
  set the parse offset to 'ofs'
*/
static enum ndr_err_code ndr_pull_set_offset(struct ndr_pull *ndr, uint32_t ofs)
{
	ndr->offset = ofs;
	if (ndr->offset > ndr->data_size) {
		return ndr_pull_error(ndr, NDR_ERR_BUFSIZE, 
				      "ndr_pull_set_offset %u failed",
				      ofs);
	}
	return NDR_ERR_SUCCESS;
}

/* create a ndr_push structure, ready for some marshalling */
_PUBLIC_ struct ndr_push *ndr_push_init_ctx(TALLOC_CTX *mem_ctx)
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

/* return a DATA_BLOB structure for the current ndr_push marshalled data */
_PUBLIC_ DATA_BLOB ndr_push_blob(struct ndr_push *ndr)
{
	DATA_BLOB blob;
	blob = data_blob_const(ndr->data, ndr->offset);
	if (ndr->alloc_size > ndr->offset) {
		ndr->data[ndr->offset] = 0;
	}
	return blob;
}


/*
  expand the available space in the buffer to ndr->offset + extra_size
*/
_PUBLIC_ enum ndr_err_code ndr_push_expand(struct ndr_push *ndr, uint32_t extra_size)
{
	uint32_t size = extra_size + ndr->offset;

	if (size < ndr->offset) {
		/* extra_size overflowed the offset */
		return ndr_push_error(ndr, NDR_ERR_BUFSIZE, "Overflow in push_expand to %u",
				      size);
	}

	if (ndr->alloc_size > size) {
		return NDR_ERR_SUCCESS;
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

	return NDR_ERR_SUCCESS;
}

_PUBLIC_ void ndr_print_debugc_helper(struct ndr_print *ndr, const char *format, ...)
{
	va_list ap;
	char *s = NULL;
	uint32_t i;
	int ret;
	int dbgc_class;

	va_start(ap, format);
	ret = vasprintf(&s, format, ap);
	va_end(ap);

	if (ret == -1) {
		return;
	}

	dbgc_class = *(int *)ndr->private_data;

	if (ndr->no_newline) {
		DEBUGADDC(dbgc_class, 1,("%s", s));
		free(s);
		return;
	}

	for (i=0;i<ndr->depth;i++) {
		DEBUGADDC(dbgc_class, 1,("    "));
	}

	DEBUGADDC(dbgc_class, 1,("%s\n", s));
	free(s);
}

_PUBLIC_ void ndr_print_debug_helper(struct ndr_print *ndr, const char *format, ...) 
{
	va_list ap;
	char *s = NULL;
	uint32_t i;
	int ret;

	va_start(ap, format);
	ret = vasprintf(&s, format, ap);
	va_end(ap);

	if (ret == -1) {
		return;
	}

	if (ndr->no_newline) {
		DEBUGADD(1,("%s", s));
		free(s);
		return;
	}

	for (i=0;i<ndr->depth;i++) {
		DEBUGADD(1,("    "));
	}

	DEBUGADD(1,("%s\n", s));
	free(s);
}

_PUBLIC_ void ndr_print_printf_helper(struct ndr_print *ndr, const char *format, ...) 
{
	va_list ap;
	uint32_t i;

	if (!ndr->no_newline) {
		for (i=0;i<ndr->depth;i++) {
			printf("    ");
		}
	}

	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
	if (!ndr->no_newline) {
		printf("\n");
	}
}

_PUBLIC_ void ndr_print_string_helper(struct ndr_print *ndr, const char *format, ...)
{
	va_list ap;
	uint32_t i;

	if (!ndr->no_newline) {
		for (i=0;i<ndr->depth;i++) {
			ndr->private_data = talloc_asprintf_append_buffer(
				(char *)ndr->private_data, "    ");
		}
	}

	va_start(ap, format);
	ndr->private_data = talloc_vasprintf_append_buffer((char *)ndr->private_data, 
						    format, ap);
	va_end(ap);
	if (!ndr->no_newline) {
		ndr->private_data = talloc_asprintf_append_buffer((char *)ndr->private_data,
								  "\n");
	}
}

/*
  a useful helper function for printing idl structures via DEBUGC()
*/
_PUBLIC_ void ndr_print_debugc(int dbgc_class, ndr_print_fn_t fn, const char *name, void *ptr)
{
	struct ndr_print *ndr;

	DEBUGC(dbgc_class, 1,(" "));

	ndr = talloc_zero(NULL, struct ndr_print);
	if (!ndr) return;
	ndr->private_data = &dbgc_class;
	ndr->print = ndr_print_debugc_helper;
	ndr->depth = 1;
	ndr->flags = 0;
	fn(ndr, name, ptr);
	talloc_free(ndr);
}

/*
  a useful helper function for printing idl structures via DEBUG()
*/
_PUBLIC_ void ndr_print_debug(ndr_print_fn_t fn, const char *name, void *ptr)
{
	struct ndr_print *ndr;

	DEBUG(1,(" "));

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
_PUBLIC_ void ndr_print_union_debug(ndr_print_fn_t fn, const char *name, uint32_t level, void *ptr)
{
	struct ndr_print *ndr;

	DEBUG(1,(" "));

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
_PUBLIC_ void ndr_print_function_debug(ndr_print_function_t fn, const char *name, int flags, void *ptr)
{
	struct ndr_print *ndr;

	DEBUG(1,(" "));

	ndr = talloc_zero(NULL, struct ndr_print);
	if (!ndr) return;
	ndr->print = ndr_print_debug_helper;
	ndr->depth = 1;
	ndr->flags = 0;

	fn(ndr, name, flags, ptr);
	talloc_free(ndr);
}

/*
  a useful helper function for printing idl structures to a string
*/
_PUBLIC_ char *ndr_print_struct_string(TALLOC_CTX *mem_ctx, ndr_print_fn_t fn, const char *name, void *ptr)
{
	struct ndr_print *ndr;
	char *ret = NULL;

	ndr = talloc_zero(mem_ctx, struct ndr_print);
	if (!ndr) return NULL;
	ndr->private_data = talloc_strdup(ndr, "");
	if (!ndr->private_data) {
		goto failed;
	}
	ndr->print = ndr_print_string_helper;
	ndr->depth = 1;
	ndr->flags = 0;

	fn(ndr, name, ptr);
	ret = talloc_steal(mem_ctx, (char *)ndr->private_data);
failed:
	talloc_free(ndr);
	return ret;
}

/*
  a useful helper function for printing idl unions to a string
*/
_PUBLIC_ char *ndr_print_union_string(TALLOC_CTX *mem_ctx, ndr_print_fn_t fn, const char *name, uint32_t level, void *ptr)
{
	struct ndr_print *ndr;
	char *ret = NULL;

	ndr = talloc_zero(mem_ctx, struct ndr_print);
	if (!ndr) return NULL;
	ndr->private_data = talloc_strdup(ndr, "");
	if (!ndr->private_data) {
		goto failed;
	}
	ndr->print = ndr_print_string_helper;
	ndr->depth = 1;
	ndr->flags = 0;
	ndr_print_set_switch_value(ndr, ptr, level);
	fn(ndr, name, ptr);
	ret = talloc_steal(mem_ctx, (char *)ndr->private_data);
failed:
	talloc_free(ndr);
	return ret;
}

/*
  a useful helper function for printing idl function calls to a string
*/
_PUBLIC_ char *ndr_print_function_string(TALLOC_CTX *mem_ctx,
				ndr_print_function_t fn, const char *name, 
				int flags, void *ptr)
{
	struct ndr_print *ndr;
	char *ret = NULL;

	ndr = talloc_zero(mem_ctx, struct ndr_print);
	if (!ndr) return NULL;
	ndr->private_data = talloc_strdup(ndr, "");
	if (!ndr->private_data) {
		goto failed;
	}
	ndr->print = ndr_print_string_helper;
	ndr->depth = 1;
	ndr->flags = 0;
	fn(ndr, name, flags, ptr);
	ret = talloc_steal(mem_ctx, (char *)ndr->private_data);
failed:
	talloc_free(ndr);
	return ret;
}

_PUBLIC_ void ndr_set_flags(uint32_t *pflags, uint32_t new_flags)
{
	/* the big/little endian flags are inter-dependent */
	if (new_flags & LIBNDR_FLAG_LITTLE_ENDIAN) {
		(*pflags) &= ~LIBNDR_FLAG_BIGENDIAN;
		(*pflags) &= ~LIBNDR_FLAG_NDR64;
	}
	if (new_flags & LIBNDR_FLAG_BIGENDIAN) {
		(*pflags) &= ~LIBNDR_FLAG_LITTLE_ENDIAN;
		(*pflags) &= ~LIBNDR_FLAG_NDR64;
	}
	if (new_flags & LIBNDR_ALIGN_FLAGS) {
		/* Ensure we only have the passed-in
		   align flag set in the new_flags,
		   remove any old align flag. */
		(*pflags) &= ~LIBNDR_ALIGN_FLAGS;
	}
	if (new_flags & LIBNDR_FLAG_NO_RELATIVE_REVERSE) {
		(*pflags) &= ~LIBNDR_FLAG_RELATIVE_REVERSE;
	}
	(*pflags) |= new_flags;
}

/*
  return and possibly log an NDR error
*/
_PUBLIC_ enum ndr_err_code ndr_pull_error(struct ndr_pull *ndr,
				 enum ndr_err_code ndr_err,
				 const char *format, ...)
{
	char *s=NULL;
	va_list ap;
	int ret;

	if (ndr->flags & LIBNDR_FLAG_INCOMPLETE_BUFFER) {
		switch (ndr_err) {
		case NDR_ERR_BUFSIZE:
			return NDR_ERR_INCOMPLETE_BUFFER;
		default:
			break;
		}
	}

	va_start(ap, format);
	ret = vasprintf(&s, format, ap);
	va_end(ap);

	if (ret == -1) {
		return NDR_ERR_ALLOC;
	}

	DEBUG(1,("ndr_pull_error(%u): %s\n", ndr_err, s));

	free(s);

	return ndr_err;
}

/*
  return and possibly log an NDR error
*/
_PUBLIC_ enum ndr_err_code ndr_push_error(struct ndr_push *ndr,
				 enum ndr_err_code ndr_err,
				 const char *format, ...) 
{
	char *s=NULL;
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = vasprintf(&s, format, ap);
	va_end(ap);

	if (ret == -1) {
		return NDR_ERR_ALLOC;
	}

	DEBUG(1,("ndr_push_error(%u): %s\n", ndr_err, s));

	free(s);

	return ndr_err;
}

/*
  handle subcontext buffers, which in midl land are user-marshalled, but
  we use magic in pidl to make them easier to cope with
*/
_PUBLIC_ enum ndr_err_code ndr_pull_subcontext_start(struct ndr_pull *ndr,
				   struct ndr_pull **_subndr,
				   size_t header_size,
				   ssize_t size_is)
{
	struct ndr_pull *subndr;
	uint32_t r_content_size;
	bool force_le = false;
	bool force_be = false;

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
		NDR_CHECK(ndr_pull_uint3264(ndr, NDR_SCALARS, &content_size));
		if (size_is >= 0 && size_is != content_size) {
			return ndr_pull_error(ndr, NDR_ERR_SUBCONTEXT, "Bad subcontext (PULL) size_is(%d) mismatch content_size %d", 
						(int)size_is, (int)content_size);
		}
		r_content_size = content_size;
		break;
	}
	case 0xFFFFFC01: {
		/*
		 * Common Type Header for the Serialization Stream
		 * See [MS-RPCE] 2.2.6 Type Serialization Version 1
		 */
		uint8_t version;
		uint8_t drep;
		uint16_t hdrlen;
		uint32_t filler;
		uint32_t content_size;
		uint32_t reserved;

		/* version */
		NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &version));

		if (version != 1) {
			return ndr_pull_error(ndr, NDR_ERR_SUBCONTEXT,
					      "Bad subcontext (PULL) Common Type Header version %d != 1",
					      (int)version);
		}

		/*
		 * 0x10 little endian
		 * 0x00 big endian
		 */
		NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &drep));
		if (drep == 0x10) {
			force_le = true;
		} else if (drep == 0x00) {
			force_be = true;
		} else {
			return ndr_pull_error(ndr, NDR_ERR_SUBCONTEXT,
					      "Bad subcontext (PULL) Common Type Header invalid drep 0x%02X",
					      (unsigned int)drep);
		}

		/* length of the "Private Header for Constructed Type" */
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &hdrlen));
		if (hdrlen != 8) {
			return ndr_pull_error(ndr, NDR_ERR_SUBCONTEXT,
					      "Bad subcontext (PULL) Common Type Header length %d != 8",
					      (int)hdrlen);
		}

		/* filler should be ignored */
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &filler));

		/*
		 * Private Header for Constructed Type
		 */
		/* length - will be updated latter */
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &content_size));
		if (size_is >= 0 && size_is != content_size) {
			return ndr_pull_error(ndr, NDR_ERR_SUBCONTEXT, "Bad subcontext (PULL) size_is(%d) mismatch content_size %d",
					      (int)size_is, (int)content_size);
		}
		/* the content size must be a multiple of 8 */
		if ((content_size % 8) != 0) {
			return ndr_pull_error(ndr, NDR_ERR_SUBCONTEXT,
					      "Bad subcontext (PULL) size_is(%d) not padded to 8 content_size %d",
					      (int)size_is, (int)content_size);
		}
		r_content_size = content_size;

		/* reserved */
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &reserved));
		break;
	}
	case 0xFFFFFFFF:
		/*
		 * a shallow copy like subcontext
		 * useful for DCERPC pipe chunks.
		 */
		subndr = talloc_zero(ndr, struct ndr_pull);
		NDR_ERR_HAVE_NO_MEMORY(subndr);

		subndr->flags		= ndr->flags;
		subndr->current_mem_ctx	= ndr->current_mem_ctx;
		subndr->data		= ndr->data;
		subndr->offset		= ndr->offset;
		subndr->data_size	= ndr->data_size;

		*_subndr = subndr;
		return NDR_ERR_SUCCESS;

	default:
		return ndr_pull_error(ndr, NDR_ERR_SUBCONTEXT, "Bad subcontext (PULL) header_size %d", 
				      (int)header_size);
	}

	NDR_PULL_NEED_BYTES(ndr, r_content_size);

	subndr = talloc_zero(ndr, struct ndr_pull);
	NDR_ERR_HAVE_NO_MEMORY(subndr);
	subndr->flags		= ndr->flags & ~LIBNDR_FLAG_NDR64;
	subndr->current_mem_ctx	= ndr->current_mem_ctx;

	subndr->data = ndr->data + ndr->offset;
	subndr->offset = 0;
	subndr->data_size = r_content_size;

	if (force_le) {
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_LITTLE_ENDIAN);
	} else if (force_be) {
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_BIGENDIAN);
	}

	*_subndr = subndr;
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_pull_subcontext_end(struct ndr_pull *ndr,
				 struct ndr_pull *subndr,
				 size_t header_size,
				 ssize_t size_is)
{
	uint32_t advance;
	uint32_t highest_ofs;

	if (header_size == 0xFFFFFFFF) {
		advance = subndr->offset - ndr->offset;
	} else if (size_is >= 0) {
		advance = size_is;
	} else if (header_size > 0) {
		advance = subndr->data_size;
	} else {
		advance = subndr->offset;
	}

	if (subndr->offset > ndr->relative_highest_offset) {
		highest_ofs = subndr->offset;
	} else {
		highest_ofs = subndr->relative_highest_offset;
	}
	if (!(subndr->flags & LIBNDR_FLAG_SUBCONTEXT_NO_UNREAD_BYTES)) {
		/*
		 * avoid an error unless SUBCONTEXT_NO_UNREAD_BYTES is specified
		 */
		highest_ofs = advance;
	}
	if (highest_ofs < advance) {
		return ndr_pull_error(subndr, NDR_ERR_UNREAD_BYTES,
				      "not all bytes consumed ofs[%u] advance[%u]",
				      highest_ofs, advance);
	}

	NDR_CHECK(ndr_pull_advance(ndr, advance));
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_push_subcontext_start(struct ndr_push *ndr,
				   struct ndr_push **_subndr,
				   size_t header_size,
				   ssize_t size_is)
{
	struct ndr_push *subndr;

	subndr = ndr_push_init_ctx(ndr);
	NDR_ERR_HAVE_NO_MEMORY(subndr);
	subndr->flags	= ndr->flags & ~LIBNDR_FLAG_NDR64;

	if (size_is > 0) {
		NDR_CHECK(ndr_push_zero(subndr, size_is));
		subndr->offset = 0;
		subndr->relative_end_offset = size_is;
	}

	*_subndr = subndr;
	return NDR_ERR_SUCCESS;
}

/*
  push a subcontext header 
*/
_PUBLIC_ enum ndr_err_code ndr_push_subcontext_end(struct ndr_push *ndr,
				 struct ndr_push *subndr,
				 size_t header_size,
				 ssize_t size_is)
{
	ssize_t padding_len;

	if (size_is >= 0) {
		padding_len = size_is - subndr->offset;
		if (padding_len < 0) {
			return ndr_push_error(ndr, NDR_ERR_SUBCONTEXT, "Bad subcontext (PUSH) content_size %d is larger than size_is(%d)",
					      (int)subndr->offset, (int)size_is);
		}
		subndr->offset = size_is;
	}

	switch (header_size) {
	case 0: 
		break;

	case 2: 
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, subndr->offset));
		break;

	case 4: 
		NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, subndr->offset));
		break;

	case 0xFFFFFC01:
		/*
		 * Common Type Header for the Serialization Stream
		 * See [MS-RPCE] 2.2.6 Type Serialization Version 1
		 */
		padding_len = NDR_ROUND(subndr->offset, 8) - subndr->offset;
		if (padding_len > 0) {
			NDR_CHECK(ndr_push_zero(subndr, padding_len));
		}

		/* version */
		NDR_CHECK(ndr_push_uint8(ndr, NDR_SCALARS, 1));

		/*
		 * 0x10 little endian
		 * 0x00 big endian
		 */
		NDR_CHECK(ndr_push_uint8(ndr, NDR_SCALARS, NDR_BE(ndr)?0x00:0x10));

		/* length of the "Private Header for Constructed Type" */
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, 8));

		/* filler */
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 0xCCCCCCCC));

		/*
		 * Private Header for Constructed Type
		 */
		/* length - will be updated latter */
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, subndr->offset));

		/* reserved */
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 0));
		break;

	default:
		return ndr_push_error(ndr, NDR_ERR_SUBCONTEXT, "Bad subcontext header size %d", 
				      (int)header_size);
	}

	NDR_CHECK(ndr_push_bytes(ndr, subndr->data, subndr->offset));
	return NDR_ERR_SUCCESS;
}

/*
  store a token in the ndr context, for later retrieval
*/
_PUBLIC_ enum ndr_err_code ndr_token_store(TALLOC_CTX *mem_ctx,
			 struct ndr_token_list **list, 
			 const void *key, 
			 uint32_t value)
{
	struct ndr_token_list *tok;
	tok = talloc(mem_ctx, struct ndr_token_list);
	NDR_ERR_HAVE_NO_MEMORY(tok);
	tok->key = key;
	tok->value = value;
	DLIST_ADD((*list), tok);
	return NDR_ERR_SUCCESS;
}

/*
  retrieve a token from a ndr context, using cmp_fn to match the tokens
*/
_PUBLIC_ enum ndr_err_code ndr_token_retrieve_cmp_fn(struct ndr_token_list **list, const void *key, uint32_t *v,
				   comparison_fn_t _cmp_fn, bool _remove_tok)
{
	struct ndr_token_list *tok;
	for (tok=*list;tok;tok=tok->next) {
		if (_cmp_fn && _cmp_fn(tok->key,key)==0) goto found;
		else if (!_cmp_fn && tok->key == key) goto found;
	}
	return NDR_ERR_TOKEN;
found:
	*v = tok->value;
	if (_remove_tok) {
		DLIST_REMOVE((*list), tok);
		talloc_free(tok);
	}
	return NDR_ERR_SUCCESS;
}

/*
  retrieve a token from a ndr context
*/
_PUBLIC_ enum ndr_err_code ndr_token_retrieve(struct ndr_token_list **list, const void *key, uint32_t *v)
{
	return ndr_token_retrieve_cmp_fn(list, key, v, NULL, true);
}

/*
  peek at but don't removed a token from a ndr context
*/
_PUBLIC_ uint32_t ndr_token_peek(struct ndr_token_list **list, const void *key)
{
	enum ndr_err_code status;
	uint32_t v;

	status = ndr_token_retrieve_cmp_fn(list, key, &v, NULL, false);
	if (!NDR_ERR_CODE_IS_SUCCESS(status)) {
		return 0;
	}

	return v;
}

/*
  pull an array size field and add it to the array_size_list token list
*/
_PUBLIC_ enum ndr_err_code ndr_pull_array_size(struct ndr_pull *ndr, const void *p)
{
	uint32_t size;
	NDR_CHECK(ndr_pull_uint3264(ndr, NDR_SCALARS, &size));
	return ndr_token_store(ndr, &ndr->array_size_list, p, size);
}

/*
  get the stored array size field
*/
_PUBLIC_ uint32_t ndr_get_array_size(struct ndr_pull *ndr, const void *p)
{
	return ndr_token_peek(&ndr->array_size_list, p);
}

/*
  check the stored array size field
*/
_PUBLIC_ enum ndr_err_code ndr_check_array_size(struct ndr_pull *ndr, void *p, uint32_t size)
{
	uint32_t stored;
	stored = ndr_token_peek(&ndr->array_size_list, p);
	if (stored != size) {
		return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, 
				      "Bad array size - got %u expected %u\n",
				      stored, size);
	}
	return NDR_ERR_SUCCESS;
}

/*
  pull an array length field and add it to the array_length_list token list
*/
_PUBLIC_ enum ndr_err_code ndr_pull_array_length(struct ndr_pull *ndr, const void *p)
{
	uint32_t length, offset;
	NDR_CHECK(ndr_pull_uint3264(ndr, NDR_SCALARS, &offset));
	if (offset != 0) {
		return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, 
				      "non-zero array offset %u\n", offset);
	}
	NDR_CHECK(ndr_pull_uint3264(ndr, NDR_SCALARS, &length));
	return ndr_token_store(ndr, &ndr->array_length_list, p, length);
}

/*
  get the stored array length field
*/
_PUBLIC_ uint32_t ndr_get_array_length(struct ndr_pull *ndr, const void *p)
{
	return ndr_token_peek(&ndr->array_length_list, p);
}

/*
  check the stored array length field
*/
_PUBLIC_ enum ndr_err_code ndr_check_array_length(struct ndr_pull *ndr, void *p, uint32_t length)
{
	uint32_t stored;
	stored = ndr_token_peek(&ndr->array_length_list, p);
	if (stored != length) {
		return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, 
				      "Bad array length - got %u expected %u\n",
				      stored, length);
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_push_pipe_chunk_trailer(struct ndr_push *ndr, int ndr_flags, uint32_t count)
{
	if (ndr->flags & LIBNDR_FLAG_NDR64) {
		int64_t tmp = 0 - (int64_t)count;
		uint64_t ncount = tmp;

		NDR_CHECK(ndr_push_hyper(ndr, ndr_flags, ncount));
	}

	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_check_pipe_chunk_trailer(struct ndr_pull *ndr, int ndr_flags, uint32_t count)
{
	if (ndr->flags & LIBNDR_FLAG_NDR64) {
		int64_t tmp = 0 - (int64_t)count;
		uint64_t ncount1 = tmp;
		uint64_t ncount2;

		NDR_CHECK(ndr_pull_hyper(ndr, ndr_flags, &ncount2));
		if (ncount1 == ncount2) {
			return NDR_ERR_SUCCESS;
		}

		return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE,
			"Bad pipe trailer[%lld should be %lld] size was %lu\"",
			(unsigned long long)ncount2,
			(unsigned long long)ncount1,
			(unsigned long)count);
	}

	return NDR_ERR_SUCCESS;
}

/*
  store a switch value
 */
_PUBLIC_ enum ndr_err_code ndr_push_set_switch_value(struct ndr_push *ndr, const void *p, uint32_t val)
{
	return ndr_token_store(ndr, &ndr->switch_list, p, val);
}

_PUBLIC_ enum ndr_err_code ndr_pull_set_switch_value(struct ndr_pull *ndr, const void *p, uint32_t val)
{
	return ndr_token_store(ndr, &ndr->switch_list, p, val);
}

_PUBLIC_ enum ndr_err_code ndr_print_set_switch_value(struct ndr_print *ndr, const void *p, uint32_t val)
{
	return ndr_token_store(ndr, &ndr->switch_list, p, val);
}

/*
  retrieve a switch value
 */
_PUBLIC_ uint32_t ndr_push_get_switch_value(struct ndr_push *ndr, const void *p)
{
	return ndr_token_peek(&ndr->switch_list, p);
}

_PUBLIC_ uint32_t ndr_pull_get_switch_value(struct ndr_pull *ndr, const void *p)
{
	return ndr_token_peek(&ndr->switch_list, p);
}

_PUBLIC_ uint32_t ndr_print_get_switch_value(struct ndr_print *ndr, const void *p)
{
	return ndr_token_peek(&ndr->switch_list, p);
}

/*
  pull a struct from a blob using NDR
*/
_PUBLIC_ enum ndr_err_code ndr_pull_struct_blob(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx, void *p,
			      ndr_pull_flags_fn_t fn)
{
	struct ndr_pull *ndr;
	ndr = ndr_pull_init_blob(blob, mem_ctx);
	NDR_ERR_HAVE_NO_MEMORY(ndr);
	NDR_CHECK_FREE(fn(ndr, NDR_SCALARS|NDR_BUFFERS, p));
	talloc_free(ndr);
	return NDR_ERR_SUCCESS;
}

/*
  pull a struct from a blob using NDR - failing if all bytes are not consumed
*/
_PUBLIC_ enum ndr_err_code ndr_pull_struct_blob_all(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx, 
						    void *p, ndr_pull_flags_fn_t fn)
{
	struct ndr_pull *ndr;
	uint32_t highest_ofs;
	ndr = ndr_pull_init_blob(blob, mem_ctx);
	NDR_ERR_HAVE_NO_MEMORY(ndr);
	NDR_CHECK_FREE(fn(ndr, NDR_SCALARS|NDR_BUFFERS, p));
	if (ndr->offset > ndr->relative_highest_offset) {
		highest_ofs = ndr->offset;
	} else {
		highest_ofs = ndr->relative_highest_offset;
	}
	if (highest_ofs < ndr->data_size) {
		enum ndr_err_code ret;
		ret = ndr_pull_error(ndr, NDR_ERR_UNREAD_BYTES,
				     "not all bytes consumed ofs[%u] size[%u]",
				     highest_ofs, ndr->data_size);
		talloc_free(ndr);
		return ret;
	}
	talloc_free(ndr);
	return NDR_ERR_SUCCESS;
}

/*
  pull a union from a blob using NDR, given the union discriminator
*/
_PUBLIC_ enum ndr_err_code ndr_pull_union_blob(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx, 
					       void *p,
			     uint32_t level, ndr_pull_flags_fn_t fn)
{
	struct ndr_pull *ndr;
	ndr = ndr_pull_init_blob(blob, mem_ctx);
	NDR_ERR_HAVE_NO_MEMORY(ndr);
	NDR_CHECK_FREE(ndr_pull_set_switch_value(ndr, p, level));
	NDR_CHECK_FREE(fn(ndr, NDR_SCALARS|NDR_BUFFERS, p));
	talloc_free(ndr);
	return NDR_ERR_SUCCESS;
}

/*
  pull a union from a blob using NDR, given the union discriminator,
  failing if all bytes are not consumed
*/
_PUBLIC_ enum ndr_err_code ndr_pull_union_blob_all(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx, 
						   void *p,
			     uint32_t level, ndr_pull_flags_fn_t fn)
{
	struct ndr_pull *ndr;
	uint32_t highest_ofs;
	ndr = ndr_pull_init_blob(blob, mem_ctx);
	NDR_ERR_HAVE_NO_MEMORY(ndr);
	NDR_CHECK_FREE(ndr_pull_set_switch_value(ndr, p, level));
	NDR_CHECK_FREE(fn(ndr, NDR_SCALARS|NDR_BUFFERS, p));
	if (ndr->offset > ndr->relative_highest_offset) {
		highest_ofs = ndr->offset;
	} else {
		highest_ofs = ndr->relative_highest_offset;
	}
	if (highest_ofs < ndr->data_size) {
		enum ndr_err_code ret;
		ret = ndr_pull_error(ndr, NDR_ERR_UNREAD_BYTES,
				     "not all bytes consumed ofs[%u] size[%u]",
				     highest_ofs, ndr->data_size);
		talloc_free(ndr);
		return ret;
	}
	talloc_free(ndr);
	return NDR_ERR_SUCCESS;
}

/*
  push a struct to a blob using NDR
*/
_PUBLIC_ enum ndr_err_code ndr_push_struct_blob(DATA_BLOB *blob, TALLOC_CTX *mem_ctx, const void *p, ndr_push_flags_fn_t fn)
{
	struct ndr_push *ndr;
	ndr = ndr_push_init_ctx(mem_ctx);
	NDR_ERR_HAVE_NO_MEMORY(ndr);

	NDR_CHECK(fn(ndr, NDR_SCALARS|NDR_BUFFERS, p));

	*blob = ndr_push_blob(ndr);
	talloc_steal(mem_ctx, blob->data);
	talloc_free(ndr);

	return NDR_ERR_SUCCESS;
}

/*
  push a union to a blob using NDR
*/
_PUBLIC_ enum ndr_err_code ndr_push_union_blob(DATA_BLOB *blob, TALLOC_CTX *mem_ctx, void *p,
			     uint32_t level, ndr_push_flags_fn_t fn)
{
	struct ndr_push *ndr;
	ndr = ndr_push_init_ctx(mem_ctx);
	NDR_ERR_HAVE_NO_MEMORY(ndr);

	NDR_CHECK(ndr_push_set_switch_value(ndr, p, level));
	NDR_CHECK(fn(ndr, NDR_SCALARS|NDR_BUFFERS, p));

	*blob = ndr_push_blob(ndr);
	talloc_steal(mem_ctx, blob->data);
	talloc_free(ndr);

	return NDR_ERR_SUCCESS;
}

/*
  generic ndr_size_*() handler for structures
*/
_PUBLIC_ size_t ndr_size_struct(const void *p, int flags, ndr_push_flags_fn_t push)
{
	struct ndr_push *ndr;
	enum ndr_err_code status;
	size_t ret;

	/* avoid recursion */
	if (flags & LIBNDR_FLAG_NO_NDR_SIZE) return 0;

	ndr = ndr_push_init_ctx(NULL);
	if (!ndr) return 0;
	ndr->flags |= flags | LIBNDR_FLAG_NO_NDR_SIZE;
	status = push(ndr, NDR_SCALARS|NDR_BUFFERS, discard_const(p));
	if (!NDR_ERR_CODE_IS_SUCCESS(status)) {
		talloc_free(ndr);
		return 0;
	}
	ret = ndr->offset;
	talloc_free(ndr);
	return ret;
}

/*
  generic ndr_size_*() handler for unions
*/
_PUBLIC_ size_t ndr_size_union(const void *p, int flags, uint32_t level, ndr_push_flags_fn_t push)
{
	struct ndr_push *ndr;
	enum ndr_err_code status;
	size_t ret;

	/* avoid recursion */
	if (flags & LIBNDR_FLAG_NO_NDR_SIZE) return 0;

	ndr = ndr_push_init_ctx(NULL);
	if (!ndr) return 0;
	ndr->flags |= flags | LIBNDR_FLAG_NO_NDR_SIZE;

	status = ndr_push_set_switch_value(ndr, p, level);
	if (!NDR_ERR_CODE_IS_SUCCESS(status)) {
		talloc_free(ndr);
		return 0;
	}
	status = push(ndr, NDR_SCALARS|NDR_BUFFERS, p);
	if (!NDR_ERR_CODE_IS_SUCCESS(status)) {
		talloc_free(ndr);
		return 0;
	}
	ret = ndr->offset;
	talloc_free(ndr);
	return ret;
}

/*
  get the current base for relative pointers for the push
*/
_PUBLIC_ uint32_t ndr_push_get_relative_base_offset(struct ndr_push *ndr)
{
	return ndr->relative_base_offset;
}

/*
  restore the old base for relative pointers for the push
*/
_PUBLIC_ void ndr_push_restore_relative_base_offset(struct ndr_push *ndr, uint32_t offset)
{
	ndr->relative_base_offset = offset;
}

/*
  setup the current base for relative pointers for the push
  called in the NDR_SCALAR stage
*/
_PUBLIC_ enum ndr_err_code ndr_push_setup_relative_base_offset1(struct ndr_push *ndr, const void *p, uint32_t offset)
{
	ndr->relative_base_offset = offset;
	return ndr_token_store(ndr, &ndr->relative_base_list, p, offset);
}

/*
  setup the current base for relative pointers for the push
  called in the NDR_BUFFERS stage
*/
_PUBLIC_ enum ndr_err_code ndr_push_setup_relative_base_offset2(struct ndr_push *ndr, const void *p)
{
	return ndr_token_retrieve(&ndr->relative_base_list, p, &ndr->relative_base_offset);
}

/*
  push a relative object - stage1
  this is called during SCALARS processing
*/
_PUBLIC_ enum ndr_err_code ndr_push_relative_ptr1(struct ndr_push *ndr, const void *p)
{
	if (p == NULL) {
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 0));
		return NDR_ERR_SUCCESS;
	}
	NDR_CHECK(ndr_push_align(ndr, 4));
	NDR_CHECK(ndr_token_store(ndr, &ndr->relative_list, p, ndr->offset));
	return ndr_push_uint32(ndr, NDR_SCALARS, 0xFFFFFFFF);
}

/*
  push a short relative object - stage1
  this is called during SCALARS processing
*/
_PUBLIC_ enum ndr_err_code ndr_push_short_relative_ptr1(struct ndr_push *ndr, const void *p)
{
	if (p == NULL) {
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, 0));
		return NDR_ERR_SUCCESS;
	}
	NDR_CHECK(ndr_push_align(ndr, 2));
	NDR_CHECK(ndr_token_store(ndr, &ndr->relative_list, p, ndr->offset));
	return ndr_push_uint16(ndr, NDR_SCALARS, 0xFFFF);
}
/*
  push a relative object - stage2
  this is called during buffers processing
*/
static enum ndr_err_code ndr_push_relative_ptr2(struct ndr_push *ndr, const void *p)
{
	uint32_t save_offset;
	uint32_t ptr_offset = 0xFFFFFFFF;
	if (p == NULL) {
		return NDR_ERR_SUCCESS;
	}
	save_offset = ndr->offset;
	NDR_CHECK(ndr_token_retrieve(&ndr->relative_list, p, &ptr_offset));
	if (ptr_offset > ndr->offset) {
		return ndr_push_error(ndr, NDR_ERR_BUFSIZE, 
				      "ndr_push_relative_ptr2 ptr_offset(%u) > ndr->offset(%u)",
				      ptr_offset, ndr->offset);
	}
	ndr->offset = ptr_offset;
	if (save_offset < ndr->relative_base_offset) {
		return ndr_push_error(ndr, NDR_ERR_BUFSIZE, 
				      "ndr_push_relative_ptr2 save_offset(%u) < ndr->relative_base_offset(%u)",
				      save_offset, ndr->relative_base_offset);
	}	
	NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, save_offset - ndr->relative_base_offset));
	ndr->offset = save_offset;
	return NDR_ERR_SUCCESS;
}
/*
  push a short relative object - stage2
  this is called during buffers processing
*/
_PUBLIC_ enum ndr_err_code ndr_push_short_relative_ptr2(struct ndr_push *ndr, const void *p)
{
	uint32_t save_offset;
	uint32_t ptr_offset = 0xFFFF;
	if (p == NULL) {
		return NDR_ERR_SUCCESS;
	}
	save_offset = ndr->offset;
	NDR_CHECK(ndr_token_retrieve(&ndr->relative_list, p, &ptr_offset));
	if (ptr_offset > ndr->offset) {
		return ndr_push_error(ndr, NDR_ERR_BUFSIZE,
				      "ndr_push_short_relative_ptr2 ptr_offset(%u) > ndr->offset(%u)",
				      ptr_offset, ndr->offset);
	}
	ndr->offset = ptr_offset;
	if (save_offset < ndr->relative_base_offset) {
		return ndr_push_error(ndr, NDR_ERR_BUFSIZE,
				      "ndr_push_relative_ptr2 save_offset(%u) < ndr->relative_base_offset(%u)",
				      save_offset, ndr->relative_base_offset);
	}
	NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, save_offset - ndr->relative_base_offset));
	ndr->offset = save_offset;
	return NDR_ERR_SUCCESS;
}

/*
  push a relative object - stage2 start
  this is called during buffers processing
*/
_PUBLIC_ enum ndr_err_code ndr_push_relative_ptr2_start(struct ndr_push *ndr, const void *p)
{
	if (p == NULL) {
		return NDR_ERR_SUCCESS;
	}
	if (!(ndr->flags & LIBNDR_FLAG_RELATIVE_REVERSE)) {
		uint32_t relative_offset;
		size_t pad;
		size_t align = 1;

		if (ndr->offset < ndr->relative_base_offset) {
			return ndr_push_error(ndr, NDR_ERR_BUFSIZE,
				      "ndr_push_relative_ptr2_start ndr->offset(%u) < ndr->relative_base_offset(%u)",
				      ndr->offset, ndr->relative_base_offset);
		}

		relative_offset = ndr->offset - ndr->relative_base_offset;

		if (ndr->flags & LIBNDR_FLAG_NOALIGN) {
			align = 1;
		} else if (ndr->flags & LIBNDR_FLAG_ALIGN2) {
			align = 2;
		} else if (ndr->flags & LIBNDR_FLAG_ALIGN4) {
			align = 4;
		} else if (ndr->flags & LIBNDR_FLAG_ALIGN8) {
			align = 8;
		}

		pad = ndr_align_size(relative_offset, align);
		if (pad) {
			NDR_CHECK(ndr_push_zero(ndr, pad));
		}

		return ndr_push_relative_ptr2(ndr, p);
	}
	if (ndr->relative_end_offset == -1) {
		return ndr_push_error(ndr, NDR_ERR_RELATIVE,
			      "ndr_push_relative_ptr2_start RELATIVE_REVERSE flag set and relative_end_offset %d",
			      ndr->relative_end_offset);
	}
	NDR_CHECK(ndr_token_store(ndr, &ndr->relative_begin_list, p, ndr->offset));
	return NDR_ERR_SUCCESS;
}

/*
  push a relative object - stage2 end
  this is called during buffers processing
*/
_PUBLIC_ enum ndr_err_code ndr_push_relative_ptr2_end(struct ndr_push *ndr, const void *p)
{
	uint32_t begin_offset = 0xFFFFFFFF;
	ssize_t len;
	uint32_t correct_offset = 0;
	uint32_t align = 1;
	uint32_t pad = 0;

	if (p == NULL) {
		return NDR_ERR_SUCCESS;
	}

	if (!(ndr->flags & LIBNDR_FLAG_RELATIVE_REVERSE)) {
		return NDR_ERR_SUCCESS;
	}

	if (ndr->flags & LIBNDR_FLAG_NO_NDR_SIZE) {
		/* better say more than calculation a too small buffer */
		NDR_PUSH_ALIGN(ndr, 8);
		return NDR_ERR_SUCCESS;
	}

	if (ndr->relative_end_offset < ndr->offset) {
		return ndr_push_error(ndr, NDR_ERR_RELATIVE,
				      "ndr_push_relative_ptr2_end:"
				      "relative_end_offset %u < offset %u",
				      ndr->relative_end_offset, ndr->offset);
	}

	NDR_CHECK(ndr_token_retrieve(&ndr->relative_begin_list, p, &begin_offset));

	/* we have marshalled a buffer, see how long it was */
	len = ndr->offset - begin_offset;

	if (len < 0) {
		return ndr_push_error(ndr, NDR_ERR_RELATIVE,
				      "ndr_push_relative_ptr2_end:"
				      "offset %u - begin_offset %u < 0",
				      ndr->offset, begin_offset);
	}

	if (ndr->relative_end_offset < len) {
		return ndr_push_error(ndr, NDR_ERR_RELATIVE,
				      "ndr_push_relative_ptr2_end:"
				      "relative_end_offset %u < len %lld",
				      ndr->offset, (long long)len);
	}

	/* the reversed offset is at the end of the main buffer */
	correct_offset = ndr->relative_end_offset - len;

	if (ndr->flags & LIBNDR_FLAG_NOALIGN) {
		align = 1;
	} else if (ndr->flags & LIBNDR_FLAG_ALIGN2) {
		align = 2;
	} else if (ndr->flags & LIBNDR_FLAG_ALIGN4) {
		align = 4;
	} else if (ndr->flags & LIBNDR_FLAG_ALIGN8) {
		align = 8;
	}

	pad = ndr_align_size(correct_offset, align);
	if (pad) {
		correct_offset += pad;
		correct_offset -= align;
	}

	if (correct_offset < begin_offset) {
		return ndr_push_error(ndr, NDR_ERR_RELATIVE,
				      "ndr_push_relative_ptr2_end: "
				      "correct_offset %u < begin_offset %u",
				      correct_offset, begin_offset);
	}

	if (len > 0) {
		uint32_t clear_size = correct_offset - begin_offset;

		clear_size = MIN(clear_size, len);

		/* now move the marshalled buffer to the end of the main buffer */
		memmove(ndr->data + correct_offset, ndr->data + begin_offset, len);

		if (clear_size) {
			/* and wipe out old buffer within the main buffer */
			memset(ndr->data + begin_offset, '\0', clear_size);
		}
	}

	/* and set the end offset for the next buffer */
	ndr->relative_end_offset = correct_offset;

	/* finally write the offset to the main buffer */
	ndr->offset = correct_offset;
	NDR_CHECK(ndr_push_relative_ptr2(ndr, p));

	/* restore to where we were in the main buffer */
	ndr->offset = begin_offset;

	return NDR_ERR_SUCCESS;
}

/*
  get the current base for relative pointers for the pull
*/
_PUBLIC_ uint32_t ndr_pull_get_relative_base_offset(struct ndr_pull *ndr)
{
	return ndr->relative_base_offset;
}

/*
  restore the old base for relative pointers for the pull
*/
_PUBLIC_ void ndr_pull_restore_relative_base_offset(struct ndr_pull *ndr, uint32_t offset)
{
	ndr->relative_base_offset = offset;
}

/*
  setup the current base for relative pointers for the pull
  called in the NDR_SCALAR stage
*/
_PUBLIC_ enum ndr_err_code ndr_pull_setup_relative_base_offset1(struct ndr_pull *ndr, const void *p, uint32_t offset)
{
	ndr->relative_base_offset = offset;
	return ndr_token_store(ndr, &ndr->relative_base_list, p, offset);
}

/*
  setup the current base for relative pointers for the pull
  called in the NDR_BUFFERS stage
*/
_PUBLIC_ enum ndr_err_code ndr_pull_setup_relative_base_offset2(struct ndr_pull *ndr, const void *p)
{
	return ndr_token_retrieve(&ndr->relative_base_list, p, &ndr->relative_base_offset);
}

/*
  pull a relative object - stage1
  called during SCALARS processing
*/
_PUBLIC_ enum ndr_err_code ndr_pull_relative_ptr1(struct ndr_pull *ndr, const void *p, uint32_t rel_offset)
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
_PUBLIC_ enum ndr_err_code ndr_pull_relative_ptr2(struct ndr_pull *ndr, const void *p)
{
	uint32_t rel_offset;
	NDR_CHECK(ndr_token_retrieve(&ndr->relative_list, p, &rel_offset));
	return ndr_pull_set_offset(ndr, rel_offset);
}

const static struct {
	enum ndr_err_code err;
	const char *string;
} ndr_err_code_strings[] = {
	{ NDR_ERR_SUCCESS, "Success" },
	{ NDR_ERR_ARRAY_SIZE, "Bad Array Size" },
	{ NDR_ERR_BAD_SWITCH, "Bad Switch" },
	{ NDR_ERR_OFFSET, "Offset Error" },
	{ NDR_ERR_RELATIVE, "Relative Pointer Error" },
	{ NDR_ERR_CHARCNV, "Character Conversion Error" },
	{ NDR_ERR_LENGTH, "Length Error" },
	{ NDR_ERR_SUBCONTEXT, "Subcontext Error" },
	{ NDR_ERR_COMPRESSION, "Compression Error" },
	{ NDR_ERR_STRING, "String Error" },
	{ NDR_ERR_VALIDATE, "Validate Error" },
	{ NDR_ERR_BUFSIZE, "Buffer Size Error" },
	{ NDR_ERR_ALLOC, "Allocation Error" },
	{ NDR_ERR_RANGE, "Range Error" },
	{ NDR_ERR_TOKEN, "Token Error" },
	{ NDR_ERR_IPV4ADDRESS, "IPv4 Address Error" },
	{ NDR_ERR_INVALID_POINTER, "Invalid Pointer" },
	{ NDR_ERR_UNREAD_BYTES, "Unread Bytes" },
	{ NDR_ERR_NDR64, "NDR64 assertion error" },
	{ NDR_ERR_INCOMPLETE_BUFFER, "Incomplete Buffer" },
	{ 0, NULL }
};

_PUBLIC_ const char *ndr_map_error2string(enum ndr_err_code ndr_err)
{
	int i;
	for (i = 0; ndr_err_code_strings[i].string != NULL; i++) {
		if (ndr_err_code_strings[i].err == ndr_err)
			return ndr_err_code_strings[i].string;
	}
	return "Unknown error";
}
