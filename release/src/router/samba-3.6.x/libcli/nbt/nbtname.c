/*
   Unix SMB/CIFS implementation.

   manipulate nbt name structures

   Copyright (C) Andrew Tridgell 2005

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
  see rfc1002 for the detailed format of compressed names
*/

#include "includes.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "system/locale.h"
#include "lib/util/util_net.h"

/* don't allow an unlimited number of name components */
#define MAX_COMPONENTS 10

/**
  print a nbt string
*/
_PUBLIC_ void ndr_print_nbt_string(struct ndr_print *ndr, const char *name, const char *s)
{
	ndr_print_string(ndr, name, s);
}

/*
  pull one component of a nbt_string
*/
static enum ndr_err_code ndr_pull_component(struct ndr_pull *ndr,
					    uint8_t **component,
					    uint32_t *offset,
					    uint32_t *max_offset)
{
	uint8_t len;
	unsigned int loops = 0;
	while (loops < 5) {
		if (*offset >= ndr->data_size) {
			return ndr_pull_error(ndr, NDR_ERR_STRING,
					      "BAD NBT NAME component");
		}
		len = ndr->data[*offset];
		if (len == 0) {
			*offset += 1;
			*max_offset = MAX(*max_offset, *offset);
			*component = NULL;
			return NDR_ERR_SUCCESS;
		}
		if ((len & 0xC0) == 0xC0) {
			/* its a label pointer */
			if (1 + *offset >= ndr->data_size) {
				return ndr_pull_error(ndr, NDR_ERR_STRING,
						      "BAD NBT NAME component");
			}
			*max_offset = MAX(*max_offset, *offset + 2);
			*offset = ((len&0x3F)<<8) | ndr->data[1 + *offset];
			*max_offset = MAX(*max_offset, *offset);
			loops++;
			continue;
		}
		if ((len & 0xC0) != 0) {
			/* its a reserved length field */
			return ndr_pull_error(ndr, NDR_ERR_STRING,
					      "BAD NBT NAME component");
		}
		if (*offset + len + 2 > ndr->data_size) {
			return ndr_pull_error(ndr, NDR_ERR_STRING,
					      "BAD NBT NAME component");
		}
		*component = (uint8_t*)talloc_strndup(
			ndr->current_mem_ctx,
			(const char *)&ndr->data[1 + *offset], len);
		NDR_ERR_HAVE_NO_MEMORY(*component);
		*offset += len + 1;
		*max_offset = MAX(*max_offset, *offset);
		return NDR_ERR_SUCCESS;
	}

	/* too many pointers */
	return ndr_pull_error(ndr, NDR_ERR_STRING, "BAD NBT NAME component");
}

/**
  pull a nbt_string from the wire
*/
_PUBLIC_ enum ndr_err_code ndr_pull_nbt_string(struct ndr_pull *ndr, int ndr_flags, const char **s)
{
	uint32_t offset = ndr->offset;
	uint32_t max_offset = offset;
	unsigned num_components;
	char *name;

	if (!(ndr_flags & NDR_SCALARS)) {
		return NDR_ERR_SUCCESS;
	}

	name = NULL;

	/* break up name into a list of components */
	for (num_components=0;num_components<MAX_COMPONENTS;num_components++) {
		uint8_t *component = NULL;
		NDR_CHECK(ndr_pull_component(ndr, &component, &offset, &max_offset));
		if (component == NULL) break;
		if (name) {
			name = talloc_asprintf_append_buffer(name, ".%s", component);
			NDR_ERR_HAVE_NO_MEMORY(name);
		} else {
			name = (char *)component;
		}
	}
	if (num_components == MAX_COMPONENTS) {
		return ndr_pull_error(ndr, NDR_ERR_STRING,
				      "BAD NBT NAME too many components");
	}
	if (num_components == 0) {
		name = talloc_strdup(ndr->current_mem_ctx, "");
		NDR_ERR_HAVE_NO_MEMORY(name);
	}

	(*s) = name;
	ndr->offset = max_offset;

	return NDR_ERR_SUCCESS;
}

/**
  push a nbt string to the wire
*/
_PUBLIC_ enum ndr_err_code ndr_push_nbt_string(struct ndr_push *ndr, int ndr_flags, const char *s)
{
	if (!(ndr_flags & NDR_SCALARS)) {
		return NDR_ERR_SUCCESS;
	}

	while (s && *s) {
		enum ndr_err_code ndr_err;
		char *compname;
		size_t complen;
		uint32_t offset;

		/* see if we have pushed the remaing string allready,
		 * if so we use a label pointer to this string
		 */
		ndr_err = ndr_token_retrieve_cmp_fn(&ndr->nbt_string_list, s, &offset, (comparison_fn_t)strcmp, false);
		if (NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			uint8_t b[2];

			if (offset > 0x3FFF) {
				return ndr_push_error(ndr, NDR_ERR_STRING,
						      "offset for nbt string label pointer %u[%08X] > 0x00003FFF",
						      offset, offset);
			}

			b[0] = 0xC0 | (offset>>8);
			b[1] = (offset & 0xFF);

			return ndr_push_bytes(ndr, b, 2);
		}

		complen = strcspn(s, ".");

		/* we need to make sure the length fits into 6 bytes */
		if (complen > 0x3F) {
			return ndr_push_error(ndr, NDR_ERR_STRING,
					      "component length %u[%08X] > 0x0000003F",
					      (unsigned)complen, (unsigned)complen);
		}

		compname = talloc_asprintf(ndr, "%c%*.*s",
						(unsigned char)complen,
						(unsigned char)complen,
						(unsigned char)complen, s);
		NDR_ERR_HAVE_NO_MEMORY(compname);

		/* remember the current componemt + the rest of the string
		 * so it can be reused later
		 */
		NDR_CHECK(ndr_token_store(ndr, &ndr->nbt_string_list, s, ndr->offset));

		/* push just this component into the blob */
		NDR_CHECK(ndr_push_bytes(ndr, (const uint8_t *)compname, complen+1));
		talloc_free(compname);

		s += complen;
		if (*s == '.') s++;
	}

	/* if we reach the end of the string and have pushed the last component
	 * without using a label pointer, we need to terminate the string
	 */
	return ndr_push_bytes(ndr, (const uint8_t *)"", 1);
}


/*
  decompress a 'compressed' name component
 */
static bool decompress_name(char *name, enum nbt_name_type *type)
{
	int i;
	for (i=0;name[2*i];i++) {
		uint8_t c1 = name[2*i];
		uint8_t c2 = name[1+(2*i)];
		if (c1 < 'A' || c1 > 'P' ||
		    c2 < 'A' || c2 > 'P') {
			return false;
		}
		name[i] = ((c1-'A')<<4) | (c2-'A');
	}
	name[i] = 0;
	if (i == 16) {
		*type = (enum nbt_name_type)(name[15]);
		name[15] = 0;
		i--;
	} else {
		*type = NBT_NAME_CLIENT;
	}

	/* trim trailing spaces */
	for (;i>0 && name[i-1]==' ';i--) {
		name[i-1] = 0;
	}

	return true;
}


/*
  compress a name component
 */
static uint8_t *compress_name(TALLOC_CTX *mem_ctx,
			      const uint8_t *name, enum nbt_name_type type)
{
	uint8_t *cname;
	int i;
	uint8_t pad_char;

	if (strlen((const char *)name) > 15) {
		return NULL;
	}

	cname = talloc_array(mem_ctx, uint8_t, 33);
	if (cname == NULL) return NULL;

	for (i=0;name[i];i++) {
		cname[2*i]   = 'A' + (name[i]>>4);
		cname[1+2*i] = 'A' + (name[i]&0xF);
	}
	if (strcmp((const char *)name, "*") == 0) {
		pad_char = 0;
	} else {
		pad_char = ' ';
	}
	for (;i<15;i++) {
		cname[2*i]   = 'A' + (pad_char>>4);
		cname[1+2*i] = 'A' + (pad_char&0xF);
	}

	pad_char = type;
	cname[2*i]   = 'A' + (pad_char>>4);
	cname[1+2*i] = 'A' + (pad_char&0xF);

	cname[32] = 0;
	return cname;
}


/**
  pull a nbt name from the wire
*/
_PUBLIC_ enum ndr_err_code ndr_pull_nbt_name(struct ndr_pull *ndr, int ndr_flags, struct nbt_name *r)
{
	uint8_t *scope;
	char *cname;
	const char *s;
	bool ok;

	if (!(ndr_flags & NDR_SCALARS)) {
		return NDR_ERR_SUCCESS;
	}

	NDR_CHECK(ndr_pull_nbt_string(ndr, ndr_flags, &s));

	scope = (uint8_t *)strchr(s, '.');
	if (scope) {
		*scope = 0;
		r->scope = talloc_strdup(ndr->current_mem_ctx, (const char *)&scope[1]);
		NDR_ERR_HAVE_NO_MEMORY(r->scope);
	} else {
		r->scope = NULL;
	}

	cname = discard_const_p(char, s);

	/* the first component is limited to 16 bytes in the DOS charset,
	   which is 32 in the 'compressed' form */
	if (strlen(cname) > 32) {
		return ndr_pull_error(ndr, NDR_ERR_STRING,
				      "NBT NAME cname > 32");
	}

	/* decompress the first component */
	ok = decompress_name(cname, &r->type);
	if (!ok) {
		return ndr_pull_error(ndr, NDR_ERR_STRING,
				      "NBT NAME failed to decompress");
	}

	r->name = talloc_strdup(ndr->current_mem_ctx, cname);
	NDR_ERR_HAVE_NO_MEMORY(r->name);

	talloc_free(cname);

	return NDR_ERR_SUCCESS;
}

/**
  push a nbt name to the wire
*/
_PUBLIC_ enum ndr_err_code ndr_push_nbt_name(struct ndr_push *ndr, int ndr_flags, const struct nbt_name *r)
{
	uint8_t *cname, *fullname;
	enum ndr_err_code ndr_err;

	if (!(ndr_flags & NDR_SCALARS)) {
		return NDR_ERR_SUCCESS;
	}

	if (strlen(r->name) > 15) {
		return ndr_push_error(ndr, NDR_ERR_STRING,
				      "nbt_name longer as 15 chars: %s",
				      r->name);
	}

	cname = compress_name(ndr, (const uint8_t *)r->name, r->type);
	NDR_ERR_HAVE_NO_MEMORY(cname);

	if (r->scope) {
		fullname = (uint8_t *)talloc_asprintf(ndr, "%s.%s", cname, r->scope);
		NDR_ERR_HAVE_NO_MEMORY(fullname);
		talloc_free(cname);
	} else {
		fullname = cname;
	}

	ndr_err = ndr_push_nbt_string(ndr, ndr_flags, (const char *)fullname);

	return ndr_err;
}


/**
  copy a nbt name structure
*/
_PUBLIC_ NTSTATUS nbt_name_dup(TALLOC_CTX *mem_ctx, struct nbt_name *name, struct nbt_name *newname)
{
	*newname = *name;
	newname->name = talloc_strdup(mem_ctx, newname->name);
	NT_STATUS_HAVE_NO_MEMORY(newname->name);
	newname->scope = talloc_strdup(mem_ctx, newname->scope);
	if (name->scope) {
		NT_STATUS_HAVE_NO_MEMORY(newname->scope);
	}
	return NT_STATUS_OK;
}

/**
  push a nbt name into a blob
*/
_PUBLIC_ NTSTATUS nbt_name_to_blob(TALLOC_CTX *mem_ctx, DATA_BLOB *blob, struct nbt_name *name)
{
	enum ndr_err_code ndr_err;

	ndr_err = ndr_push_struct_blob(blob, mem_ctx, name, (ndr_push_flags_fn_t)ndr_push_nbt_name);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	return NT_STATUS_OK;
}

/**
  pull a nbt name from a blob
*/
_PUBLIC_ NTSTATUS nbt_name_from_blob(TALLOC_CTX *mem_ctx, const DATA_BLOB *blob, struct nbt_name *name)
{
	enum ndr_err_code ndr_err;

	ndr_err = ndr_pull_struct_blob(blob, mem_ctx, name,
				       (ndr_pull_flags_fn_t)ndr_pull_nbt_name);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	return NT_STATUS_OK;
}


/**
  choose a name to use when calling a server in a NBT session request.
  we use heuristics to see if the name we have been given is a IP
  address, or a too-long name. If it is then use *SMBSERVER, or a
  truncated name
*/
_PUBLIC_ void nbt_choose_called_name(TALLOC_CTX *mem_ctx,
			    struct nbt_name *n, const char *name, int type)
{
	n->scope = NULL;
	n->type = type;

	if ((name == NULL) || is_ipaddress(name)) {
		n->name = "*SMBSERVER";
		return;
	}
	if (strlen(name) > 15) {
		const char *p = strchr(name, '.');
		char *s;
		if (p - name > 15) {
			n->name = "*SMBSERVER";
			return;
		}
		s = talloc_strndup(mem_ctx, name, PTR_DIFF(p, name));
		n->name = talloc_strdup_upper(mem_ctx, s);
		return;
	}

	n->name = talloc_strdup_upper(mem_ctx, name);
}


/*
  escape a string into a form containing only a small set of characters,
  the rest is hex encoded. This is similar to URL encoding
*/
static const char *nbt_hex_encode(TALLOC_CTX *mem_ctx, const char *s)
{
	int i, len;
	char *ret;
	const char *valid_chars = "_-.$@ ";
#define NBT_CHAR_ALLOW(c) (isalnum((unsigned char)c) || strchr(valid_chars, c))

	for (len=i=0;s[i];i++,len++) {
		if (!NBT_CHAR_ALLOW(s[i])) {
			len += 2;
		}
	}

	ret = talloc_array(mem_ctx, char, len+1);
	if (ret == NULL) return NULL;

	for (len=i=0;s[i];i++) {
		if (NBT_CHAR_ALLOW(s[i])) {
			ret[len++] = s[i];
		} else {
			snprintf(&ret[len], 4, "%%%02x", (unsigned char)s[i]);
			len += 3;
		}
	}
	ret[len] = 0;

	return ret;
}


/**
  form a string for a NBT name
*/
_PUBLIC_ char *nbt_name_string(TALLOC_CTX *mem_ctx, const struct nbt_name *name)
{
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	char *ret;
	if (name->scope) {
		ret = talloc_asprintf(mem_ctx, "%s<%02x>-%s",
				      nbt_hex_encode(tmp_ctx, name->name),
				      name->type,
				      nbt_hex_encode(tmp_ctx, name->scope));
	} else {
		ret = talloc_asprintf(mem_ctx, "%s<%02x>",
				      nbt_hex_encode(tmp_ctx, name->name),
				      name->type);
	}
	talloc_free(tmp_ctx);
	return ret;
}

/**
  pull a nbt name, WINS Replication uses another on wire format for nbt name
*/
_PUBLIC_ enum ndr_err_code ndr_pull_wrepl_nbt_name(struct ndr_pull *ndr, int ndr_flags, const struct nbt_name **_r)
{
	struct nbt_name *r;
	uint8_t *namebuf;
	uint32_t namebuf_len;

	if (!(ndr_flags & NDR_SCALARS)) {
		return NDR_ERR_SUCCESS;
	}

	NDR_CHECK(ndr_pull_align(ndr, 4));
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &namebuf_len));
	if (namebuf_len < 1 || namebuf_len > 255) {
		return ndr_pull_error(ndr, NDR_ERR_ALLOC, "value out of range");
	}
	NDR_PULL_ALLOC_N(ndr, namebuf, namebuf_len);
	NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, namebuf, namebuf_len));

	if ((namebuf_len % 4) == 0) {
		/*
		 * [MS-WINSRA] — v20091104 was wrong
		 * regarding section "2.2.10.1 Name Record"
		 *
		 * If the name buffer is already 4 byte aligned
		 * Windows (at least 2003 SP1 and 2008) add 4 extra
		 * bytes. This can happen when the name has a scope.
		 */
		uint32_t pad;
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &pad));
	}

	NDR_PULL_ALLOC(ndr, r);

	/* oh wow, what a nasty bug in windows ... */
	if (namebuf[0] == 0x1b && namebuf_len >= 16) {
		namebuf[0] = namebuf[15];
		namebuf[15] = 0x1b;
	}

	if (namebuf_len < 17) {
		r->type	= 0x00;

		r->name	= talloc_strndup(r, (char *)namebuf, namebuf_len);
		if (!r->name) return ndr_pull_error(ndr, NDR_ERR_ALLOC, "out of memory");

		r->scope= NULL;

		talloc_free(namebuf);
		*_r = r;
		return NDR_ERR_SUCCESS;
	}

	r->type = namebuf[15];

	namebuf[15] = '\0';
	trim_string((char *)namebuf, NULL, " ");
	r->name = talloc_strdup(r, (char *)namebuf);
	if (!r->name) return ndr_pull_error(ndr, NDR_ERR_ALLOC, "out of memory");

	if (namebuf_len > 17) {
		r->scope = talloc_strndup(r, (char *)(namebuf+16), namebuf_len-17);
		if (!r->scope) return ndr_pull_error(ndr, NDR_ERR_ALLOC, "out of memory");
	} else {
		r->scope = NULL;
	}

	talloc_free(namebuf);
	*_r = r;
	return NDR_ERR_SUCCESS;
}

/**
  push a nbt name, WINS Replication uses another on wire format for nbt name
*/
_PUBLIC_ enum ndr_err_code ndr_push_wrepl_nbt_name(struct ndr_push *ndr, int ndr_flags, const struct nbt_name *r)
{
	uint8_t *namebuf;
	uint32_t namebuf_len;
	uint32_t _name_len;
	uint32_t scope_len = 0;

	if (r == NULL) {
		return ndr_push_error(ndr, NDR_ERR_INVALID_POINTER,
				      "wrepl_nbt_name NULL pointer");
	}

	if (!(ndr_flags & NDR_SCALARS)) {
		return NDR_ERR_SUCCESS;
	}

	_name_len = strlen(r->name);
	if (_name_len > 15) {
		return ndr_push_error(ndr, NDR_ERR_STRING,
				      "wrepl_nbt_name longer as 15 chars: %s",
				      r->name);
	}

	if (r->scope) {
		scope_len = strlen(r->scope);
	}
	if (scope_len > 238) {
		return ndr_push_error(ndr, NDR_ERR_STRING,
				      "wrepl_nbt_name scope longer as 238 chars: %s",
				      r->scope);
	}

	namebuf = (uint8_t *)talloc_asprintf(ndr, "%-15s%c%s",
					     r->name, 'X',
					     (r->scope?r->scope:""));
	if (!namebuf) return ndr_push_error(ndr, NDR_ERR_ALLOC, "out of memory");

	namebuf_len = strlen((char *)namebuf) + 1;

	/*
	 * we need to set the type here, and use a place-holder in the talloc_asprintf()
	 * as the type can be 0x00, and then the namebuf_len = strlen(namebuf); would give wrong results
	 */
	namebuf[15] = r->type;

	/* oh wow, what a nasty bug in windows ... */
	if (r->type == 0x1b) {
		namebuf[15] = namebuf[0];
		namebuf[0] = 0x1b;
	}

	NDR_CHECK(ndr_push_align(ndr, 4));
	NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, namebuf_len));
	NDR_CHECK(ndr_push_array_uint8(ndr, NDR_SCALARS, namebuf, namebuf_len));

	if ((namebuf_len % 4) == 0) {
		/*
		 * [MS-WINSRA] — v20091104 was wrong
		 * regarding section "2.2.10.1 Name Record"
		 *
		 * If the name buffer is already 4 byte aligned
		 * Windows (at least 2003 SP1 and 2008) add 4 extra
		 * bytes. This can happen when the name has a scope.
		 */
		NDR_CHECK(ndr_push_zero(ndr, 4));
	}

	talloc_free(namebuf);
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ void ndr_print_wrepl_nbt_name(struct ndr_print *ndr, const char *name, const struct nbt_name *r)
{
	char *s = nbt_name_string(ndr, r);
	ndr_print_string(ndr, name, s);
	talloc_free(s);
}

_PUBLIC_ enum ndr_err_code ndr_push_nbt_res_rec(struct ndr_push *ndr, int ndr_flags, const struct nbt_res_rec *r)
{
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_PRINT_ARRAY_HEX);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_push_align(ndr, 4));
			NDR_CHECK(ndr_push_nbt_name(ndr, NDR_SCALARS, &r->name));
			NDR_CHECK(ndr_push_nbt_qtype(ndr, NDR_SCALARS, r->rr_type));
			NDR_CHECK(ndr_push_nbt_qclass(ndr, NDR_SCALARS, r->rr_class));
			NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->ttl));
			NDR_CHECK(ndr_push_set_switch_value(ndr, &r->rdata, ((((r->rr_type) == NBT_QTYPE_NETBIOS) && ((r->rdata).data.length == 2))?0:r->rr_type)));
			NDR_CHECK(ndr_push_nbt_rdata(ndr, NDR_SCALARS, &r->rdata));
		}
		if (ndr_flags & NDR_BUFFERS) {
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}
