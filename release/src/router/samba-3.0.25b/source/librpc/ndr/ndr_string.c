/* 
   Unix SMB/CIFS implementation.

   routines for marshalling/unmarshalling string types

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

/**
  pull a general string from the wire
*/
NTSTATUS ndr_pull_string(struct ndr_pull *ndr, int ndr_flags, const char **s)
{
	char *as=NULL;
	uint32_t len1, ofs, len2;
	uint16_t len3;
	int ret;
	charset_t chset = CH_UTF16LE;
	unsigned byte_mul = 2;
	unsigned flags = ndr->flags;
	unsigned c_len_term = 0;

	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}

	if (NDR_BE(ndr)) {
		chset = CH_UTF16BE;
	}

	if (flags & LIBNDR_FLAG_STR_ASCII) {
		chset = CH_DOS;
		byte_mul = 1;
		flags &= ~LIBNDR_FLAG_STR_ASCII;
	}

	if (flags & LIBNDR_FLAG_STR_UTF8) {
		chset = CH_UTF8;
		byte_mul = 1;
		flags &= ~LIBNDR_FLAG_STR_UTF8;
	}

	flags &= ~LIBNDR_FLAG_STR_CONFORMANT;
	if (flags & LIBNDR_FLAG_STR_CHARLEN) {
		c_len_term = 1;
		flags &= ~LIBNDR_FLAG_STR_CHARLEN;
	}

	switch (flags & LIBNDR_STRING_FLAGS) {
	case LIBNDR_FLAG_STR_LEN4|LIBNDR_FLAG_STR_SIZE4:
	case LIBNDR_FLAG_STR_LEN4|LIBNDR_FLAG_STR_SIZE4|LIBNDR_FLAG_STR_NOTERM:
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &len1));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &ofs));
		if (ofs != 0) {
			return ndr_pull_error(ndr, NDR_ERR_STRING, "non-zero array offset with string flags 0x%x\n",
					      ndr->flags & LIBNDR_STRING_FLAGS);
		}
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &len2));
		if (len2 > len1) {
			return ndr_pull_error(ndr, NDR_ERR_STRING, 
					      "Bad string lengths len1=%u ofs=%u len2=%u\n", 
					      len1, ofs, len2);
		}
		NDR_PULL_NEED_BYTES(ndr, (len2 + c_len_term)*byte_mul);
		if (len2 == 0) {
			as = talloc_strdup(ndr->current_mem_ctx, "");
		} else {
			ret = convert_string_talloc(ndr->current_mem_ctx,
						    chset, CH_UNIX, 
						    ndr->data+ndr->offset, 
						    (len2 + c_len_term)*byte_mul,
						    &as, True);
			if (ret == -1) {
				return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
						      "Bad character conversion");
			}
		}
		NDR_CHECK(ndr_pull_advance(ndr, (len2 + c_len_term)*byte_mul));

		if (len1 != len2) {
			DEBUG(6,("len1[%u] != len2[%u] '%s'\n", len1, len2, as));
		}

		/* this is a way of detecting if a string is sent with the wrong
		   termination */
		if (ndr->flags & LIBNDR_FLAG_STR_NOTERM) {
			if (strlen(as) < (len2 + c_len_term)) {
				DEBUG(6,("short string '%s'\n", as));
			}
		} else {
			if (strlen(as) == (len2 + c_len_term)) {
				DEBUG(6,("long string '%s'\n", as));
			}
		}
		*s = as;
		break;

	case LIBNDR_FLAG_STR_SIZE4:
	case LIBNDR_FLAG_STR_SIZE4|LIBNDR_FLAG_STR_NOTERM:
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &len1));
		NDR_PULL_NEED_BYTES(ndr, (len1 + c_len_term)*byte_mul);
		if (len1 == 0) {
			as = talloc_strdup(ndr->current_mem_ctx, "");
		} else {
			ret = convert_string_talloc(ndr->current_mem_ctx,
						    chset, CH_UNIX, 
						    ndr->data+ndr->offset, 
						    (len1 + c_len_term)*byte_mul,
						    &as, False);
			if (ret == -1) {
				return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
						      "Bad character conversion");
			}
		}
		NDR_CHECK(ndr_pull_advance(ndr, (len1 + c_len_term)*byte_mul));

		/* this is a way of detecting if a string is sent with the wrong
		   termination */
		if (ndr->flags & LIBNDR_FLAG_STR_NOTERM) {
			if (strlen(as) < (len1 + c_len_term)) {
				DEBUG(6,("short string '%s'\n", as));
			}
		} else {
			if (strlen(as) == (len1 + c_len_term)) {
				DEBUG(6,("long string '%s'\n", as));
			}
		}
		*s = as;
		break;

	case LIBNDR_FLAG_STR_LEN4:
	case LIBNDR_FLAG_STR_LEN4|LIBNDR_FLAG_STR_NOTERM:
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &ofs));
		if (ofs != 0) {
			return ndr_pull_error(ndr, NDR_ERR_STRING, "non-zero array offset with string flags 0x%x\n",
					      ndr->flags & LIBNDR_STRING_FLAGS);
		}
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &len1));
		NDR_PULL_NEED_BYTES(ndr, (len1 + c_len_term)*byte_mul);
		if (len1 == 0) {
			as = talloc_strdup(ndr->current_mem_ctx, "");
		} else {
			ret = convert_string_talloc(ndr->current_mem_ctx,
						    chset, CH_UNIX, 
						    ndr->data+ndr->offset, 
						    (len1 + c_len_term)*byte_mul,
						    &as, False);
			if (ret == -1) {
				return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
						      "Bad character conversion");
			}
		}
		NDR_CHECK(ndr_pull_advance(ndr, (len1 + c_len_term)*byte_mul));

		/* this is a way of detecting if a string is sent with the wrong
		   termination */
		if (ndr->flags & LIBNDR_FLAG_STR_NOTERM) {
			if (strlen(as) < (len1 + c_len_term)) {
				DEBUG(6,("short string '%s'\n", as));
			}
		} else {
			if (strlen(as) == (len1 + c_len_term)) {
				DEBUG(6,("long string '%s'\n", as));
			}
		}
		*s = as;
		break;


	case LIBNDR_FLAG_STR_SIZE2:
	case LIBNDR_FLAG_STR_SIZE2|LIBNDR_FLAG_STR_NOTERM:
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &len3));
		NDR_PULL_NEED_BYTES(ndr, (len3 + c_len_term)*byte_mul);
		if (len3 == 0) {
			as = talloc_strdup(ndr->current_mem_ctx, "");
		} else {
			ret = convert_string_talloc(ndr->current_mem_ctx,
						    chset, CH_UNIX, 
						    ndr->data+ndr->offset, 
						    (len3 + c_len_term)*byte_mul,
						    &as, False);
			if (ret == -1) {
				return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
						      "Bad character conversion");
			}
		}
		NDR_CHECK(ndr_pull_advance(ndr, (len3 + c_len_term)*byte_mul));

		/* this is a way of detecting if a string is sent with the wrong
		   termination */
		if (ndr->flags & LIBNDR_FLAG_STR_NOTERM) {
			if (strlen(as) < (len3 + c_len_term)) {
				DEBUG(6,("short string '%s'\n", as));
			}
		} else {
			if (strlen(as) == (len3 + c_len_term)) {
				DEBUG(6,("long string '%s'\n", as));
			}
		}
		*s = as;
		break;

	case LIBNDR_FLAG_STR_SIZE2|LIBNDR_FLAG_STR_NOTERM|LIBNDR_FLAG_STR_BYTESIZE:
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &len3));
		NDR_PULL_NEED_BYTES(ndr, len3);
		if (len3 == 0) {
			as = talloc_strdup(ndr->current_mem_ctx, "");
		} else {
			ret = convert_string_talloc(ndr->current_mem_ctx,
						    chset, CH_UNIX, 
						    ndr->data+ndr->offset, 
						    len3, &as, False);
			if (ret == -1) {
				return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
						      "Bad character conversion");
			}
		}
		NDR_CHECK(ndr_pull_advance(ndr, len3));
		*s = as;
		break;

	case LIBNDR_FLAG_STR_NULLTERM:
		if (byte_mul == 1) {
			len1 = ascii_len_n((const char *)(ndr->data+ndr->offset), ndr->data_size - ndr->offset);
		} else {
			len1 = utf16_len_n(ndr->data+ndr->offset, ndr->data_size - ndr->offset);
		}
		ret = convert_string_talloc(ndr->current_mem_ctx,
					    chset, CH_UNIX, 
					    ndr->data+ndr->offset, 
					    len1, &as, False);
		if (ret == -1) {
			return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		NDR_CHECK(ndr_pull_advance(ndr, len1));
		*s = as;
		break;

	case LIBNDR_FLAG_STR_FIXLEN15:
	case LIBNDR_FLAG_STR_FIXLEN32:
		len1 = (flags & LIBNDR_FLAG_STR_FIXLEN32)?32:15;
		NDR_PULL_NEED_BYTES(ndr, len1*byte_mul);
		ret = convert_string_talloc(ndr->current_mem_ctx,
					    chset, CH_UNIX, 
					    ndr->data+ndr->offset, 
					    len1*byte_mul, &as, False);
		if (ret == -1) {
			return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		NDR_CHECK(ndr_pull_advance(ndr, len1*byte_mul));
		*s = as;
		break;

	default:
		return ndr_pull_error(ndr, NDR_ERR_STRING, "Bad string flags 0x%x\n",
				      ndr->flags & LIBNDR_STRING_FLAGS);
	}

	return NT_STATUS_OK;
}


/**
  push a general string onto the wire
*/
NTSTATUS ndr_push_string(struct ndr_push *ndr, int ndr_flags, const char *s)
{
	ssize_t s_len, c_len, d_len;
	charset_t chset = CH_UTF16LE;
	unsigned flags = ndr->flags;
	unsigned byte_mul = 2;
	uint8_t *dest = NULL;

	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}

	if (NDR_BE(ndr)) {
		chset = CH_UTF16BE;
	}
	
	s_len = s?strlen(s):0;

	if (flags & LIBNDR_FLAG_STR_ASCII) {
		chset = CH_DOS;
		byte_mul = 1;
		flags &= ~LIBNDR_FLAG_STR_ASCII;
	}

	if (flags & LIBNDR_FLAG_STR_UTF8) {
		chset = CH_UTF8;
		byte_mul = 1;
		flags &= ~LIBNDR_FLAG_STR_UTF8;
	}

	flags &= ~LIBNDR_FLAG_STR_CONFORMANT;

	if (!(flags & 
	      (LIBNDR_FLAG_STR_NOTERM |
	       LIBNDR_FLAG_STR_FIXLEN15 |
	       LIBNDR_FLAG_STR_FIXLEN32))) {
		s_len++;
	}
	d_len = convert_string_talloc(ndr, CH_UNIX, chset, s, s_len, &dest,
				      False);
	if (d_len == -1) {
		return ndr_push_error(ndr, NDR_ERR_CHARCNV, 
				      "Bad character conversion");
	}

	if (flags & LIBNDR_FLAG_STR_BYTESIZE) {
		c_len = d_len;
		flags &= ~LIBNDR_FLAG_STR_BYTESIZE;
	} else if (flags & LIBNDR_FLAG_STR_CHARLEN) {
		c_len = (d_len / byte_mul)-1;
		flags &= ~LIBNDR_FLAG_STR_CHARLEN;
	} else {
		c_len = d_len / byte_mul;
	}

	switch ((flags & LIBNDR_STRING_FLAGS) & ~LIBNDR_FLAG_STR_NOTERM) {
	case LIBNDR_FLAG_STR_LEN4|LIBNDR_FLAG_STR_SIZE4:
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, c_len));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 0));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, c_len));
		NDR_CHECK(ndr_push_bytes(ndr, dest, d_len));
		break;

	case LIBNDR_FLAG_STR_LEN4:
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 0));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, c_len));
		NDR_CHECK(ndr_push_bytes(ndr, dest, d_len));
		break;

	case LIBNDR_FLAG_STR_SIZE4:
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, c_len));
		NDR_CHECK(ndr_push_bytes(ndr, dest, d_len));
		break;

	case LIBNDR_FLAG_STR_SIZE2:
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, c_len));
		NDR_CHECK(ndr_push_bytes(ndr, dest, d_len));
		break;

	case LIBNDR_FLAG_STR_NULLTERM:
		NDR_CHECK(ndr_push_bytes(ndr, dest, d_len));
		break;

	case LIBNDR_FLAG_STR_FIXLEN15:
	case LIBNDR_FLAG_STR_FIXLEN32: {
		ssize_t fix_len = (flags & LIBNDR_FLAG_STR_FIXLEN32)?32:15;
		uint32_t pad_len = fix_len - d_len;
		if (d_len > fix_len) {
			return ndr_push_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		NDR_CHECK(ndr_push_bytes(ndr, dest, d_len));
		if (pad_len != 0) {
			NDR_CHECK(ndr_push_zero(ndr, pad_len));
		}
		break;
	}

	default:
		return ndr_push_error(ndr, NDR_ERR_STRING, "Bad string flags 0x%x\n",
				      ndr->flags & LIBNDR_STRING_FLAGS);
	}

	talloc_free(dest);

	return NT_STATUS_OK;
}

/**
  push a general string onto the wire
*/
size_t ndr_string_array_size(struct ndr_push *ndr, const char *s)
{
	size_t c_len;
	unsigned flags = ndr->flags;
	unsigned byte_mul = 2;
	unsigned c_len_term = 1;

	if (flags & LIBNDR_FLAG_STR_FIXLEN32) {
		return 32;
	}
	if (flags & LIBNDR_FLAG_STR_FIXLEN15) {
		return 15;
	}
	
	c_len = s?strlen(s):0;

	if (flags & (LIBNDR_FLAG_STR_ASCII|LIBNDR_FLAG_STR_UTF8)) {
		byte_mul = 1;
	}

	if (flags & LIBNDR_FLAG_STR_NOTERM) {
		c_len_term = 0;
	}

	c_len = c_len + c_len_term;

	if (flags & LIBNDR_FLAG_STR_BYTESIZE) {
		c_len = c_len * byte_mul;
	}

	return c_len;
}

void ndr_print_string(struct ndr_print *ndr, const char *name, const char *s)
{
	if (s) {
		ndr->print(ndr, "%-25s: '%s'", name, s);
	} else {
		ndr->print(ndr, "%-25s: NULL", name);
	}
}

uint32_t ndr_size_string(int ret, const char * const* string, int flags) 
{
	/* FIXME: Is this correct for all strings ? */
	if(!(*string)) return ret;
	return ret+strlen(*string)+1;
}

/**
  pull a general string array from the wire
*/
NTSTATUS ndr_pull_string_array(struct ndr_pull *ndr, int ndr_flags, const char ***_a)
{
	const char **a = *_a;
	uint32_t count;

	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}

	for (count = 0;; count++) {
		TALLOC_CTX *tmp_ctx;
		const char *s = NULL;
		a = talloc_realloc(ndr->current_mem_ctx, a, const char *, count + 2);
		NT_STATUS_HAVE_NO_MEMORY(a);
		a[count]   = NULL;
		a[count+1]   = NULL;

		tmp_ctx = ndr->current_mem_ctx;
		ndr->current_mem_ctx = a;
		NDR_CHECK(ndr_pull_string(ndr, ndr_flags, &s));
		ndr->current_mem_ctx = tmp_ctx;
		if (strcmp("", s)==0) {
			a[count] = NULL;
			break;
		} else {
			a[count] = s;
		}
	}

	*_a =a;
	return NT_STATUS_OK;
}

/**
  push a general string array onto the wire
*/
NTSTATUS ndr_push_string_array(struct ndr_push *ndr, int ndr_flags, const char **a)
{
	uint32_t count;

	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}

	for (count = 0; a && a[count]; count++) {
		NDR_CHECK(ndr_push_string(ndr, ndr_flags, a[count]));
	}

	NDR_CHECK(ndr_push_string(ndr, ndr_flags, ""));

	return NT_STATUS_OK;
}

void ndr_print_string_array(struct ndr_print *ndr, const char *name, const char **a)
{
	uint32_t count;
	uint32_t i;

	for (count = 0; a && a[count]; count++) {}

	ndr->print(ndr, "%s: ARRAY(%d)", name, count);
	ndr->depth++;
	for (i=0;i<count;i++) {
		char *idx=NULL;
		asprintf(&idx, "[%d]", i);
		if (idx) {
			ndr_print_string(ndr, idx, a[i]);
			free(idx);
		}
	}
	ndr->depth--;
}

/**
 * Return number of elements in a string including the last (zeroed) element 
 */
uint32_t ndr_string_length(const void *_var, uint32_t element_size)
{
	uint32_t i;
	uint8_t zero[4] = {0,0,0,0};
	const char *var = (const char *)_var;

	for (i = 0; memcmp(var+i*element_size,zero,element_size) != 0; i++);

	return i+1;
}

NTSTATUS ndr_check_string_terminator(struct ndr_pull *ndr, uint32_t count, uint32_t element_size)
{
	uint32_t i;
	struct ndr_pull_save save_offset;

	ndr_pull_save(ndr, &save_offset);
	ndr_pull_advance(ndr, (count - 1) * element_size);
	NDR_PULL_NEED_BYTES(ndr, element_size);

	for (i = 0; i < element_size; i++) {
		 if (ndr->data[ndr->offset+i] != 0) {
			ndr_pull_restore(ndr, &save_offset);

			return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, "String terminator not present or outside string boundaries");
		 }
	}

	ndr_pull_restore(ndr, &save_offset);

	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_charset(struct ndr_pull *ndr, int ndr_flags, const char **var, uint32_t length, uint8_t byte_mul, charset_t chset)
{
	int ret;
	if (length == 0) {
		*var = talloc_strdup(ndr->current_mem_ctx, "");
		return NT_STATUS_OK;
	}

	if (NDR_BE(ndr) && chset == CH_UTF16) {
		chset = CH_UTF16BE;
	}

	NDR_PULL_NEED_BYTES(ndr, length*byte_mul);

	ret = convert_string_talloc(ndr->current_mem_ctx,
				    chset, CH_UNIX, 
				    ndr->data+ndr->offset, 
				    length*byte_mul,
				    var, False);
	if (ret == -1) {
		return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
				      "Bad character conversion");
	}
	NDR_CHECK(ndr_pull_advance(ndr, length*byte_mul));

	return NT_STATUS_OK;
}

NTSTATUS ndr_push_charset(struct ndr_push *ndr, int ndr_flags, const char *var,
			  uint32_t length, uint8_t byte_mul, charset_t chset)
{
	ssize_t ret, required;

	if (NDR_BE(ndr) && chset == CH_UTF16) {
		chset = CH_UTF16BE;
	}

	required = byte_mul * length;
	
	NDR_PUSH_NEED_BYTES(ndr, required);
	ret = convert_string(CH_UNIX, chset, 
			     var, strlen(var),
			     ndr->data+ndr->offset, required, False);
	if (ret == -1) {
		return ndr_push_error(ndr, NDR_ERR_CHARCNV, 
				      "Bad character conversion");
	}

	/* Make sure the remaining part of the string is filled with zeroes */
	if (ret < required) {
		memset(ndr->data+ndr->offset+ret, 0, required-ret);
	}

	ndr->offset += required;

	return NT_STATUS_OK;
}

/* Return number of elements in a string in the specified charset */
uint32_t ndr_charset_length(const void *var, int chset)
{
	/* FIXME: Treat special chars special here, taking chset into account */
	/* Also include 0 byte */
	return strlen((const char *)var)+1;
}
