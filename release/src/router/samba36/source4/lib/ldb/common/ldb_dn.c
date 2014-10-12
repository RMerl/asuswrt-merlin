/*
   ldb database library

   Copyright (C) Simo Sorce 2005

     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

/*
 *  Name: ldb
 *
 *  Component: ldb dn creation and manipulation utility functions
 *
 *  Description: - explode a dn into it's own basic elements
 *                 and put them in a structure (only if necessary)
 *               - manipulate ldb_dn structures
 *
 *  Author: Simo Sorce
 */

#include "ldb_private.h"
#include <ctype.h>

#define LDB_DN_NULL_FAILED(x) if (!(x)) goto failed

#define LDB_FREE(x) do { talloc_free(x); x = NULL; } while(0)

/**
   internal ldb exploded dn structures
*/
struct ldb_dn_component {

	char *name;
	struct ldb_val value;

	char *cf_name;
	struct ldb_val cf_value;
};

struct ldb_dn_ext_component {

	char *name;
	struct ldb_val value;
};

struct ldb_dn {

	struct ldb_context *ldb;

	/* Special DNs are always linearized */
	bool special;
	bool invalid;

	bool valid_case;

	char *linearized;
	char *ext_linearized;
	char *casefold;

	unsigned int comp_num;
	struct ldb_dn_component *components;

	unsigned int ext_comp_num;
	struct ldb_dn_ext_component *ext_components;
};

/* it is helpful to be able to break on this in gdb */
static void ldb_dn_mark_invalid(struct ldb_dn *dn)
{
	dn->invalid = true;
}

/* strdn may be NULL */
struct ldb_dn *ldb_dn_from_ldb_val(TALLOC_CTX *mem_ctx,
                                   struct ldb_context *ldb,
                                   const struct ldb_val *strdn)
{
	struct ldb_dn *dn;

	if (! ldb) return NULL;

	if (strdn && strdn->data
	    && (strnlen((const char*)strdn->data, strdn->length) != strdn->length)) {
		/* The RDN must not contain a character with value 0x0 */
		return NULL;
	}

	dn = talloc_zero(mem_ctx, struct ldb_dn);
	LDB_DN_NULL_FAILED(dn);

	dn->ldb = talloc_get_type(ldb, struct ldb_context);
	if (dn->ldb == NULL) {
		/* the caller probably got the arguments to
		   ldb_dn_new() mixed up */
		talloc_free(dn);
		return NULL;
	}

	if (strdn->data && strdn->length) {
		const char *data = (const char *)strdn->data;
		size_t length = strdn->length;

		if (data[0] == '@') {
			dn->special = true;
		}
		dn->ext_linearized = talloc_strndup(dn, data, length);
		LDB_DN_NULL_FAILED(dn->ext_linearized);

		if (data[0] == '<') {
			const char *p_save, *p = dn->ext_linearized;
			do {
				p_save = p;
				p = strstr(p, ">;");
				if (p) {
					p = p + 2;
				}
			} while (p);

			if (p_save == dn->ext_linearized) {
				dn->linearized = talloc_strdup(dn, "");
			} else {
				dn->linearized = talloc_strdup(dn, p_save);
			}
			LDB_DN_NULL_FAILED(dn->linearized);
		} else {
			dn->linearized = dn->ext_linearized;
			dn->ext_linearized = NULL;
		}
	} else {
		dn->linearized = talloc_strdup(dn, "");
		LDB_DN_NULL_FAILED(dn->linearized);
	}

	return dn;

failed:
	talloc_free(dn);
	return NULL;
}

/* strdn may be NULL */
struct ldb_dn *ldb_dn_new(TALLOC_CTX *mem_ctx,
			  struct ldb_context *ldb,
			  const char *strdn)
{
	struct ldb_val blob;
	blob.data = discard_const_p(uint8_t, strdn);
	blob.length = strdn ? strlen(strdn) : 0;
	return ldb_dn_from_ldb_val(mem_ctx, ldb, &blob);
}

struct ldb_dn *ldb_dn_new_fmt(TALLOC_CTX *mem_ctx,
			      struct ldb_context *ldb,
			      const char *new_fmt, ...)
{
	char *strdn;
	va_list ap;

	if ( (! mem_ctx) || (! ldb)) return NULL;

	va_start(ap, new_fmt);
	strdn = talloc_vasprintf(mem_ctx, new_fmt, ap);
	va_end(ap);

	if (strdn) {
		struct ldb_dn *dn = ldb_dn_new(mem_ctx, ldb, strdn);
		talloc_free(strdn);
		return dn;
	}

	return NULL;
}

/* see RFC2253 section 2.4 */
static int ldb_dn_escape_internal(char *dst, const char *src, int len)
{
	const char *p, *s;
	char *d;
	size_t l;

	p = s = src;
	d = dst;

	while (p - src < len) {
		p += strcspn(p, ",=\n\r+<>#;\\\" ");

		if (p - src == len) /* found no escapable chars */
			break;

		/* copy the part of the string before the stop */
		memcpy(d, s, p - s);
		d += (p - s); /* move to current position */
		
		switch (*p) {
		case ' ':
			if (p == src || (p-src)==(len-1)) {
				/* if at the beginning or end
				 * of the string then escape */
				*d++ = '\\';
				*d++ = *p++;					 
			} else {
				/* otherwise don't escape */
				*d++ = *p++;
			}
			break;

		case '#':
			/* despite the RFC, windows escapes a #
			   anywhere in the string */
		case ',':
		case '+':
		case '"':
		case '\\':
		case '<':
		case '>':
		case '?':
			/* these must be escaped using \c form */
			*d++ = '\\';
			*d++ = *p++;
			break;

		default: {
			/* any others get \XX form */
			unsigned char v;
			const char *hexbytes = "0123456789ABCDEF";
			v = *(const unsigned char *)p;
			*d++ = '\\';
			*d++ = hexbytes[v>>4];
			*d++ = hexbytes[v&0xF];
			p++;
			break;
		}
		}
		s = p; /* move forward */
	}

	/* copy the last part (with zero) and return */
	l = len - (s - src);
	memcpy(d, s, l + 1);

	/* return the length of the resulting string */
	return (l + (d - dst));
}

char *ldb_dn_escape_value(TALLOC_CTX *mem_ctx, struct ldb_val value)
{
	char *dst;

	if (!value.length)
		return NULL;

	/* allocate destination string, it will be at most 3 times the source */
	dst = talloc_array(mem_ctx, char, value.length * 3 + 1);
	if ( ! dst) {
		talloc_free(dst);
		return NULL;
	}

	ldb_dn_escape_internal(dst, (const char *)value.data, value.length);

	dst = talloc_realloc(mem_ctx, dst, char, strlen(dst) + 1);

	return dst;
}

/*
  explode a DN string into a ldb_dn structure
  based on RFC4514 except that we don't support multiple valued RDNs

  TODO: according to MS-ADTS:3.1.1.5.2 Naming Constraints
  DN must be compliant with RFC2253
*/
static bool ldb_dn_explode(struct ldb_dn *dn)
{
	char *p, *ex_name, *ex_value, *data, *d, *dt, *t;
	bool trim = true;
	bool in_extended = true;
	bool in_ex_name = false;
	bool in_ex_value = false;
	bool in_attr = false;
	bool in_value = false;
	bool in_quote = false;
	bool is_oid = false;
	bool escape = false;
	unsigned int x;
	size_t l;
	int ret;
	char *parse_dn;
	bool is_index;

	if ( ! dn || dn->invalid) return false;

	if (dn->components) {
		return true;
	}

	if (dn->ext_linearized) {
		parse_dn = dn->ext_linearized;
	} else {
		parse_dn = dn->linearized;
	}

	if ( ! parse_dn ) {
		return false;
	}

	is_index = (strncmp(parse_dn, "DN=@INDEX:", 10) == 0);

	/* Empty DNs */
	if (parse_dn[0] == '\0') {
		return true;
	}

	/* Special DNs case */
	if (dn->special) {
		return true;
	}

	/* make sure we free this if allocated previously before replacing */
	LDB_FREE(dn->components);
	dn->comp_num = 0;

	LDB_FREE(dn->ext_components);
	dn->ext_comp_num = 0;

	/* in the common case we have 3 or more components */
	/* make sure all components are zeroed, other functions depend on it */
	dn->components = talloc_zero_array(dn, struct ldb_dn_component, 3);
	if ( ! dn->components) {
		return false;
	}

	/* Components data space is allocated here once */
	data = talloc_array(dn->components, char, strlen(parse_dn) + 1);
	if (!data) {
		return false;
	}

	p = parse_dn;
	t = NULL;
	d = dt = data;

	while (*p) {
		if (in_extended) {

			if (!in_ex_name && !in_ex_value) {

				if (p[0] == '<') {
					p++;
					ex_name = d;
					in_ex_name = true;
					continue;
				} else if (p[0] == '\0') {
					p++;
					continue;
				} else {
					in_extended = false;
					in_attr = true;
					dt = d;

					continue;
				}
			}

			if (in_ex_name && *p == '=') {
				*d++ = '\0';
				p++;
				ex_value = d;
				in_ex_name = false;
				in_ex_value = true;
				continue;
			}

			if (in_ex_value && *p == '>') {
				const struct ldb_dn_extended_syntax *ext_syntax;
				struct ldb_val ex_val = {
					.data = (uint8_t *)ex_value,
					.length = d - ex_value
				};

				*d++ = '\0';
				p++;
				in_ex_value = false;

				/* Process name and ex_value */

				dn->ext_components = talloc_realloc(dn,
								    dn->ext_components,
								    struct ldb_dn_ext_component,
								    dn->ext_comp_num + 1);
				if ( ! dn->ext_components) {
					/* ouch ! */
					goto failed;
				}

				ext_syntax = ldb_dn_extended_syntax_by_name(dn->ldb, ex_name);
				if (!ext_syntax) {
					/* We don't know about this type of extended DN */
					goto failed;
				}

				dn->ext_components[dn->ext_comp_num].name = talloc_strdup(dn->ext_components, ex_name);
				if (!dn->ext_components[dn->ext_comp_num].name) {
					/* ouch */
					goto failed;
				}
				ret = ext_syntax->read_fn(dn->ldb, dn->ext_components,
							  &ex_val, &dn->ext_components[dn->ext_comp_num].value);
				if (ret != LDB_SUCCESS) {
					ldb_dn_mark_invalid(dn);
					goto failed;
				}

				dn->ext_comp_num++;

				if (*p == '\0') {
					/* We have reached the end (extended component only)! */
					talloc_free(data);
					return true;

				} else if (*p == ';') {
					p++;
					continue;
				} else {
					ldb_dn_mark_invalid(dn);
					goto failed;
				}
			}

			*d++ = *p++;
			continue;
		}
		if (in_attr) {
			if (trim) {
				if (*p == ' ') {
					p++;
					continue;
				}

				/* first char */
				trim = false;

				if (!isascii(*p)) {
					/* attr names must be ascii only */
					ldb_dn_mark_invalid(dn);
					goto failed;
				}

				if (isdigit(*p)) {
					is_oid = true;
				} else
				if ( ! isalpha(*p)) {
					/* not a digit nor an alpha,
 					 * invalid attribute name */
					ldb_dn_mark_invalid(dn);
					goto failed;
				}

				/* Copy this character across from parse_dn,
				 * now we have trimmed out spaces */
				*d++ = *p++;
				continue;
			}

			if (*p == ' ') {
				p++;
				/* valid only if we are at the end */
				trim = true;
				continue;
			}

			if (trim && (*p != '=')) {
				/* spaces/tabs are not allowed */
				ldb_dn_mark_invalid(dn);
				goto failed;
			}

			if (*p == '=') {
				/* attribute terminated */
				in_attr = false;
				in_value = true;
				trim = true;
				l = 0;

				/* Terminate this string in d
				 * (which is a copy of parse_dn
				 *  with spaces trimmed) */
				*d++ = '\0';
				dn->components[dn->comp_num].name = talloc_strdup(dn->components, dt);
				if ( ! dn->components[dn->comp_num].name) {
					/* ouch */
					goto failed;
				}

				dt = d;

				p++;
				continue;
			}

			if (!isascii(*p)) {
				/* attr names must be ascii only */
				ldb_dn_mark_invalid(dn);
				goto failed;
			}

			if (is_oid && ( ! (isdigit(*p) || (*p == '.')))) {
				/* not a digit nor a dot,
				 * invalid attribute oid */
				ldb_dn_mark_invalid(dn);
				goto failed;
			} else
			if ( ! (isalpha(*p) || isdigit(*p) || (*p == '-'))) {
				/* not ALPHA, DIGIT or HYPHEN */
				ldb_dn_mark_invalid(dn);
				goto failed;
			}

			*d++ = *p++;
			continue;
		}

		if (in_value) {
			if (in_quote) {
				if (*p == '\"') {
					if (p[-1] != '\\') {
						p++;
						in_quote = false;
						continue;
					}
				}
				*d++ = *p++;
				l++;
				continue;
			}

			if (trim) {
				if (*p == ' ') {
					p++;
					continue;
				}

				/* first char */
				trim = false;

				if (*p == '\"') {
					in_quote = true;
					p++;
					continue;
				}
			}

			switch (*p) {

			/* TODO: support ber encoded values
			case '#':
			*/

			case ',':
				if (escape) {
					*d++ = *p++;
					l++;
					escape = false;
					continue;
				}
				/* ok found value terminator */

				if ( t ) {
					/* trim back */
					d -= (p - t);
					l -= (p - t);
				}

				in_attr = true;
				in_value = false;
				trim = true;

				p++;
				*d++ = '\0';
				dn->components[dn->comp_num].value.data = (uint8_t *)talloc_strdup(dn->components, dt);
				dn->components[dn->comp_num].value.length = l;
				if ( ! dn->components[dn->comp_num].value.data) {
					/* ouch ! */
					goto failed;
				}

				dt = d;

				dn->comp_num++;
				if (dn->comp_num > 2) {
					dn->components = talloc_realloc(dn,
									dn->components,
									struct ldb_dn_component,
									dn->comp_num + 1);
					if ( ! dn->components) {
						/* ouch ! */
						goto failed;
					}
					/* make sure all components are zeroed, other functions depend on this */
					memset(&dn->components[dn->comp_num], '\0', sizeof(struct ldb_dn_component));
				}

				continue;

			case '+':
			case '=':
				/* to main compatibility with earlier
				versions of ldb indexing, we have to
				accept the base64 encoded binary index
				values, which contain a '+' or '='
				which should normally be escaped */
				if (is_index) {
					if ( t ) t = NULL;
					*d++ = *p++;
					l++;
					break;
				}
				/* fall through */
			case '\"':
			case '<':
			case '>':
			case ';':
				/* a string with not escaped specials is invalid (tested) */
				if ( ! escape) {
					ldb_dn_mark_invalid(dn);
					goto failed;
				}
				escape = false;

				*d++ = *p++;
				l++;

				if ( t ) t = NULL;
				break;

			case '\\':
				if ( ! escape) {
					escape = true;
					p++;
					continue;
				}
				escape = false;

				*d++ = *p++;
				l++;

				if ( t ) t = NULL;
				break;

			default:
				if (escape) {
					if (isxdigit(p[0]) && isxdigit(p[1])) {
						if (sscanf(p, "%02x", &x) != 1) {
							/* invalid escaping sequence */
							ldb_dn_mark_invalid(dn);
							goto failed;
						}
						p += 2;
						*d++ = (unsigned char)x;
					} else {
						*d++ = *p++;
					}

					escape = false;
					l++;
					if ( t ) t = NULL;
					break;
				}

				if (*p == ' ') {
					if ( ! t) t = p;
				} else {
					if ( t ) t = NULL;
				}

				*d++ = *p++;
				l++;

				break;
			}

		}
	}

	if (in_attr || in_quote) {
		/* invalid dn */
		ldb_dn_mark_invalid(dn);
		goto failed;
	}

	/* save last element */
	if ( t ) {
		/* trim back */
		d -= (p - t);
		l -= (p - t);
	}

	*d++ = '\0';
	dn->components[dn->comp_num].value.length = l;
	dn->components[dn->comp_num].value.data =
				(uint8_t *)talloc_strdup(dn->components, dt);
	if ( ! dn->components[dn->comp_num].value.data) {
		/* ouch */
		goto failed;
	}

	dn->comp_num++;

	talloc_free(data);
	return true;

failed:
	LDB_FREE(dn->components); /* "data" is implicitly free'd */
	dn->comp_num = 0;
	LDB_FREE(dn->ext_components);
	dn->ext_comp_num = 0;

	return false;
}

bool ldb_dn_validate(struct ldb_dn *dn)
{
	return ldb_dn_explode(dn);
}

const char *ldb_dn_get_linearized(struct ldb_dn *dn)
{
	unsigned int i;
	size_t len;
	char *d, *n;

	if ( ! dn || ( dn->invalid)) return NULL;

	if (dn->linearized) return dn->linearized;

	if ( ! dn->components) {
		ldb_dn_mark_invalid(dn);
		return NULL;
	}

	if (dn->comp_num == 0) {
		dn->linearized = talloc_strdup(dn, "");
		if ( ! dn->linearized) return NULL;
		return dn->linearized;
	}

	/* calculate maximum possible length of DN */
	for (len = 0, i = 0; i < dn->comp_num; i++) {
		/* name len */
		len += strlen(dn->components[i].name);
		/* max escaped data len */
		len += (dn->components[i].value.length * 3);
		len += 2; /* '=' and ',' */
	}
	dn->linearized = talloc_array(dn, char, len);
	if ( ! dn->linearized) return NULL;

	d = dn->linearized;

	for (i = 0; i < dn->comp_num; i++) {

		/* copy the name */
		n = dn->components[i].name;
		while (*n) *d++ = *n++;

		*d++ = '=';

		/* and the value */
		d += ldb_dn_escape_internal( d,
				(char *)dn->components[i].value.data,
				dn->components[i].value.length);
		*d++ = ',';
	}

	*(--d) = '\0';

	/* don't waste more memory than necessary */
	dn->linearized = talloc_realloc(dn, dn->linearized,
					char, (d - dn->linearized + 1));

	return dn->linearized;
}

static int ldb_dn_extended_component_compare(const void *p1, const void *p2)
{
	const struct ldb_dn_ext_component *ec1 = (const struct ldb_dn_ext_component *)p1;
	const struct ldb_dn_ext_component *ec2 = (const struct ldb_dn_ext_component *)p2;
	return strcmp(ec1->name, ec2->name);
}

char *ldb_dn_get_extended_linearized(TALLOC_CTX *mem_ctx, struct ldb_dn *dn, int mode)
{
	const char *linearized = ldb_dn_get_linearized(dn);
	char *p = NULL;
	unsigned int i;

	if (!linearized) {
		return NULL;
	}

	if (!ldb_dn_has_extended(dn)) {
		return talloc_strdup(mem_ctx, linearized);
	}

	if (!ldb_dn_validate(dn)) {
		return NULL;
	}

	/* sort the extended components by name. The idea is to make
	 * the resulting DNs consistent, plus to ensure that we put
	 * 'DELETED' first, so it can be very quickly recognised
	 */
	TYPESAFE_QSORT(dn->ext_components, dn->ext_comp_num,
		       ldb_dn_extended_component_compare);

	for (i = 0; i < dn->ext_comp_num; i++) {
		const struct ldb_dn_extended_syntax *ext_syntax;
		const char *name = dn->ext_components[i].name;
		struct ldb_val ec_val = dn->ext_components[i].value;
		struct ldb_val val;
		int ret;

		ext_syntax = ldb_dn_extended_syntax_by_name(dn->ldb, name);
		if (!ext_syntax) {
			return NULL;
		}

		if (mode == 1) {
			ret = ext_syntax->write_clear_fn(dn->ldb, mem_ctx,
							&ec_val, &val);
		} else if (mode == 0) {
			ret = ext_syntax->write_hex_fn(dn->ldb, mem_ctx,
							&ec_val, &val);
		} else {
			ret = -1;
		}

		if (ret != LDB_SUCCESS) {
			return NULL;
		}

		if (i == 0) {
			p = talloc_asprintf(mem_ctx, "<%s=%s>", 
					    name, val.data);
		} else {
			p = talloc_asprintf_append_buffer(p, ";<%s=%s>",
							  name, val.data);
		}

		talloc_free(val.data);

		if (!p) {
			return NULL;
		}
	}

	if (dn->ext_comp_num && *linearized) {
		p = talloc_asprintf_append_buffer(p, ";%s", linearized);
	}

	if (!p) {
		return NULL;
	}

	return p;
}

/*
  filter out all but an acceptable list of extended DN components
 */
void ldb_dn_extended_filter(struct ldb_dn *dn, const char * const *accept_list)
{
	unsigned int i;
	for (i=0; i<dn->ext_comp_num; i++) {
		if (!ldb_attr_in_list(accept_list, dn->ext_components[i].name)) {
			memmove(&dn->ext_components[i],
				&dn->ext_components[i+1],
				(dn->ext_comp_num-(i+1))*sizeof(dn->ext_components[0]));
			dn->ext_comp_num--;
			i--;
		}
	}
	LDB_FREE(dn->ext_linearized);
}


char *ldb_dn_alloc_linearized(TALLOC_CTX *mem_ctx, struct ldb_dn *dn)
{
	return talloc_strdup(mem_ctx, ldb_dn_get_linearized(dn));
}

/*
  casefold a dn. We need to casefold the attribute names, and canonicalize
  attribute values of case insensitive attributes.
*/

static bool ldb_dn_casefold_internal(struct ldb_dn *dn)
{
	unsigned int i;
	int ret;

	if ( ! dn || dn->invalid) return false;

	if (dn->valid_case) return true;

	if (( ! dn->components) && ( ! ldb_dn_explode(dn))) {
		return false;
	}

	for (i = 0; i < dn->comp_num; i++) {
		const struct ldb_schema_attribute *a;

		dn->components[i].cf_name =
			ldb_attr_casefold(dn->components,
					  dn->components[i].name);
		if (!dn->components[i].cf_name) {
			goto failed;
		}

		a = ldb_schema_attribute_by_name(dn->ldb,
						 dn->components[i].cf_name);

		ret = a->syntax->canonicalise_fn(dn->ldb, dn->components,
						 &(dn->components[i].value),
						 &(dn->components[i].cf_value));
		if (ret != 0) {
			goto failed;
		}
	}

	dn->valid_case = true;

	return true;

failed:
	for (i = 0; i < dn->comp_num; i++) {
		LDB_FREE(dn->components[i].cf_name);
		LDB_FREE(dn->components[i].cf_value.data);
	}
	return false;
}

const char *ldb_dn_get_casefold(struct ldb_dn *dn)
{
	unsigned int i;
	size_t len;
	char *d, *n;

	if (dn->casefold) return dn->casefold;

	if (dn->special) {
		dn->casefold = talloc_strdup(dn, dn->linearized);
		if (!dn->casefold) return NULL;
		dn->valid_case = true;
		return dn->casefold;
	}

	if ( ! ldb_dn_casefold_internal(dn)) {
		return NULL;
	}

	if (dn->comp_num == 0) {
		dn->casefold = talloc_strdup(dn, "");
		return dn->casefold;
	}

	/* calculate maximum possible length of DN */
	for (len = 0, i = 0; i < dn->comp_num; i++) {
		/* name len */
		len += strlen(dn->components[i].cf_name);
		/* max escaped data len */
		len += (dn->components[i].cf_value.length * 3);
		len += 2; /* '=' and ',' */
	}
	dn->casefold = talloc_array(dn, char, len);
	if ( ! dn->casefold) return NULL;

	d = dn->casefold;

	for (i = 0; i < dn->comp_num; i++) {

		/* copy the name */
		n = dn->components[i].cf_name;
		while (*n) *d++ = *n++;

		*d++ = '=';

		/* and the value */
		d += ldb_dn_escape_internal( d,
				(char *)dn->components[i].cf_value.data,
				dn->components[i].cf_value.length);
		*d++ = ',';
	}
	*(--d) = '\0';

	/* don't waste more memory than necessary */
	dn->casefold = talloc_realloc(dn, dn->casefold,
				      char, strlen(dn->casefold) + 1);

	return dn->casefold;
}

char *ldb_dn_alloc_casefold(TALLOC_CTX *mem_ctx, struct ldb_dn *dn)
{
	return talloc_strdup(mem_ctx, ldb_dn_get_casefold(dn));
}

/* Determine if dn is below base, in the ldap tree.  Used for
 * evaluating a subtree search.
 * 0 if they match, otherwise non-zero
 */

int ldb_dn_compare_base(struct ldb_dn *base, struct ldb_dn *dn)
{
	int ret;
	unsigned int n_base, n_dn;

	if ( ! base || base->invalid) return 1;
	if ( ! dn || dn->invalid) return -1;

	if (( ! base->valid_case) || ( ! dn->valid_case)) {
		if (base->linearized && dn->linearized) {
			/* try with a normal compare first, if we are lucky
			 * we will avoid exploding and casfolding */
			int dif;
			dif = strlen(dn->linearized) - strlen(base->linearized);
			if (dif < 0) {
				return dif;
			}
			if (strcmp(base->linearized,
				   &dn->linearized[dif]) == 0) {
				return 0;
			}
		}

		if ( ! ldb_dn_casefold_internal(base)) {
			return 1;
		}

		if ( ! ldb_dn_casefold_internal(dn)) {
			return -1;
		}

	}

	/* if base has more components,
	 * they don't have the same base */
	if (base->comp_num > dn->comp_num) {
		return (dn->comp_num - base->comp_num);
	}

	if ((dn->comp_num == 0) || (base->comp_num == 0)) {
		if (dn->special && base->special) {
			return strcmp(base->linearized, dn->linearized);
		} else if (dn->special) {
			return -1;
		} else if (base->special) {
			return 1;
		} else {
			return 0;
		}
	}

	n_base = base->comp_num - 1;
	n_dn = dn->comp_num - 1;

	while (n_base != (unsigned int) -1) {
		char *b_name = base->components[n_base].cf_name;
		char *dn_name = dn->components[n_dn].cf_name;

		char *b_vdata = (char *)base->components[n_base].cf_value.data;
		char *dn_vdata = (char *)dn->components[n_dn].cf_value.data;

		size_t b_vlen = base->components[n_base].cf_value.length;
		size_t dn_vlen = dn->components[n_dn].cf_value.length;

		/* compare attr names */
		ret = strcmp(b_name, dn_name);
		if (ret != 0) return ret;

		/* compare attr.cf_value. */
		if (b_vlen != dn_vlen) {
			return b_vlen - dn_vlen;
		}
		ret = strcmp(b_vdata, dn_vdata);
		if (ret != 0) return ret;

		n_base--;
		n_dn--;
	}

	return 0;
}

/* compare DNs using casefolding compare functions.

   If they match, then return 0
 */

int ldb_dn_compare(struct ldb_dn *dn0, struct ldb_dn *dn1)
{
	unsigned int i;
	int ret;

	if (( ! dn0) || dn0->invalid || ! dn1 || dn1->invalid) {
		return -1;
	}

	if (( ! dn0->valid_case) || ( ! dn1->valid_case)) {
		if (dn0->linearized && dn1->linearized) {
			/* try with a normal compare first, if we are lucky
			 * we will avoid exploding and casfolding */
			if (strcmp(dn0->linearized, dn1->linearized) == 0) {
				return 0;
			}
		}

		if ( ! ldb_dn_casefold_internal(dn0)) {
			return 1;
		}

		if ( ! ldb_dn_casefold_internal(dn1)) {
			return -1;
		}

	}

	if (dn0->comp_num != dn1->comp_num) {
		return (dn1->comp_num - dn0->comp_num);
	}

	if (dn0->comp_num == 0) {
		if (dn0->special && dn1->special) {
			return strcmp(dn0->linearized, dn1->linearized);
		} else if (dn0->special) {
			return 1;
		} else if (dn1->special) {
			return -1;
		} else {
			return 0;
		}
	}

	for (i = 0; i < dn0->comp_num; i++) {
		char *dn0_name = dn0->components[i].cf_name;
		char *dn1_name = dn1->components[i].cf_name;

		char *dn0_vdata = (char *)dn0->components[i].cf_value.data;
		char *dn1_vdata = (char *)dn1->components[i].cf_value.data;

		size_t dn0_vlen = dn0->components[i].cf_value.length;
		size_t dn1_vlen = dn1->components[i].cf_value.length;

		/* compare attr names */
		ret = strcmp(dn0_name, dn1_name);
		if (ret != 0) {
			return ret;
		}

		/* compare attr.cf_value. */
		if (dn0_vlen != dn1_vlen) {
			return dn0_vlen - dn1_vlen;
		}
		ret = strcmp(dn0_vdata, dn1_vdata);
		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}

static struct ldb_dn_component ldb_dn_copy_component(
						TALLOC_CTX *mem_ctx,
						struct ldb_dn_component *src)
{
	struct ldb_dn_component dst;

	memset(&dst, 0, sizeof(dst));

	if (src == NULL) {
		return dst;
	}

	dst.value = ldb_val_dup(mem_ctx, &(src->value));
	if (dst.value.data == NULL) {
		return dst;
	}

	dst.name = talloc_strdup(mem_ctx, src->name);
	if (dst.name == NULL) {
		LDB_FREE(dst.value.data);
		return dst;
	}

	if (src->cf_value.data) {
		dst.cf_value = ldb_val_dup(mem_ctx, &(src->cf_value));
		if (dst.cf_value.data == NULL) {
			LDB_FREE(dst.value.data);
			LDB_FREE(dst.name);
			return dst;
		}

		dst.cf_name = talloc_strdup(mem_ctx, src->cf_name);
		if (dst.cf_name == NULL) {
			LDB_FREE(dst.cf_name);
			LDB_FREE(dst.value.data);
			LDB_FREE(dst.name);
			return dst;
		}
	} else {
		dst.cf_value.data = NULL;
		dst.cf_name = NULL;
	}

	return dst;
}

static struct ldb_dn_ext_component ldb_dn_ext_copy_component(
						TALLOC_CTX *mem_ctx,
						struct ldb_dn_ext_component *src)
{
	struct ldb_dn_ext_component dst;

	memset(&dst, 0, sizeof(dst));

	if (src == NULL) {
		return dst;
	}

	dst.value = ldb_val_dup(mem_ctx, &(src->value));
	if (dst.value.data == NULL) {
		return dst;
	}

	dst.name = talloc_strdup(mem_ctx, src->name);
	if (dst.name == NULL) {
		LDB_FREE(dst.value.data);
		return dst;
	}

	return dst;
}

struct ldb_dn *ldb_dn_copy(TALLOC_CTX *mem_ctx, struct ldb_dn *dn)
{
	struct ldb_dn *new_dn;

	if (!dn || dn->invalid) {
		return NULL;
	}

	new_dn = talloc_zero(mem_ctx, struct ldb_dn);
	if ( !new_dn) {
		return NULL;
	}

	*new_dn = *dn;

	if (dn->components) {
		unsigned int i;

		new_dn->components =
			talloc_zero_array(new_dn,
					  struct ldb_dn_component,
					  dn->comp_num);
		if ( ! new_dn->components) {
			talloc_free(new_dn);
			return NULL;
		}

		for (i = 0; i < dn->comp_num; i++) {
			new_dn->components[i] =
				ldb_dn_copy_component(new_dn->components,
						      &dn->components[i]);
			if ( ! new_dn->components[i].value.data) {
				talloc_free(new_dn);
				return NULL;
			}
		}
	}

	if (dn->ext_components) {
		unsigned int i;

		new_dn->ext_components =
			talloc_zero_array(new_dn,
					  struct ldb_dn_ext_component,
					  dn->ext_comp_num);
		if ( ! new_dn->ext_components) {
			talloc_free(new_dn);
			return NULL;
		}

		for (i = 0; i < dn->ext_comp_num; i++) {
			new_dn->ext_components[i] =
				 ldb_dn_ext_copy_component(
						new_dn->ext_components,
						&dn->ext_components[i]);
			if ( ! new_dn->ext_components[i].value.data) {
				talloc_free(new_dn);
				return NULL;
			}
		}
	}

	if (dn->casefold) {
		new_dn->casefold = talloc_strdup(new_dn, dn->casefold);
		if ( ! new_dn->casefold) {
			talloc_free(new_dn);
			return NULL;
		}
	}

	if (dn->linearized) {
		new_dn->linearized = talloc_strdup(new_dn, dn->linearized);
		if ( ! new_dn->linearized) {
			talloc_free(new_dn);
			return NULL;
		}
	}

	if (dn->ext_linearized) {
		new_dn->ext_linearized = talloc_strdup(new_dn,
							dn->ext_linearized);
		if ( ! new_dn->ext_linearized) {
			talloc_free(new_dn);
			return NULL;
		}
	}

	return new_dn;
}

/* modify the given dn by adding a base.
 *
 * return true if successful and false if not
 * if false is returned the dn may be marked invalid
 */
bool ldb_dn_add_base(struct ldb_dn *dn, struct ldb_dn *base)
{
	const char *s;
	char *t;

	if ( !base || base->invalid || !dn || dn->invalid) {
		return false;
	}

	if (dn->components) {
		unsigned int i;

		if ( ! ldb_dn_validate(base)) {
			return false;
		}

		s = NULL;
		if (dn->valid_case) {
			if ( ! (s = ldb_dn_get_casefold(base))) {
				return false;
			}
		}

		dn->components = talloc_realloc(dn,
						dn->components,
						struct ldb_dn_component,
						dn->comp_num + base->comp_num);
		if ( ! dn->components) {
			ldb_dn_mark_invalid(dn);
			return false;
		}

		for (i = 0; i < base->comp_num; dn->comp_num++, i++) {
			dn->components[dn->comp_num] =
				ldb_dn_copy_component(dn->components,
							&base->components[i]);
			if (dn->components[dn->comp_num].value.data == NULL) {
				ldb_dn_mark_invalid(dn);
				return false;
			}
		}

		if (dn->casefold && s) {
			if (*dn->casefold) {
				t = talloc_asprintf(dn, "%s,%s",
						    dn->casefold, s);
			} else {
				t = talloc_strdup(dn, s);
			}
			LDB_FREE(dn->casefold);
			dn->casefold = t;
		}
	}

	if (dn->linearized) {

		s = ldb_dn_get_linearized(base);
		if ( ! s) {
			return false;
		}

		if (*dn->linearized) {
			t = talloc_asprintf(dn, "%s,%s",
					    dn->linearized, s);
		} else {
			t = talloc_strdup(dn, s);
		}
		if ( ! t) {
			ldb_dn_mark_invalid(dn);
			return false;
		}
		LDB_FREE(dn->linearized);
		dn->linearized = t;
	}

	/* Wipe the ext_linearized DN,
	 * the GUID and SID are almost certainly no longer valid */
	LDB_FREE(dn->ext_linearized);
	LDB_FREE(dn->ext_components);
	dn->ext_comp_num = 0;

	return true;
}

/* modify the given dn by adding a base.
 *
 * return true if successful and false if not
 * if false is returned the dn may be marked invalid
 */
bool ldb_dn_add_base_fmt(struct ldb_dn *dn, const char *base_fmt, ...)
{
	struct ldb_dn *base;
	char *base_str;
	va_list ap;
	bool ret;

	if ( !dn || dn->invalid) {
		return false;
	}

	va_start(ap, base_fmt);
	base_str = talloc_vasprintf(dn, base_fmt, ap);
	va_end(ap);

	if (base_str == NULL) {
		return false;
	}

	base = ldb_dn_new(base_str, dn->ldb, base_str);

	ret = ldb_dn_add_base(dn, base);

	talloc_free(base_str);

	return ret;
}

/* modify the given dn by adding children elements.
 *
 * return true if successful and false if not
 * if false is returned the dn may be marked invalid
 */
bool ldb_dn_add_child(struct ldb_dn *dn, struct ldb_dn *child)
{
	const char *s;
	char *t;

	if ( !child || child->invalid || !dn || dn->invalid) {
		return false;
	}

	if (dn->components) {
		unsigned int n;
		unsigned int i, j;

		if (dn->comp_num == 0) {
			return false;
		}

		if ( ! ldb_dn_validate(child)) {
			return false;
		}

		s = NULL;
		if (dn->valid_case) {
			if ( ! (s = ldb_dn_get_casefold(child))) {
				return false;
			}
		}

		n = dn->comp_num + child->comp_num;

		dn->components = talloc_realloc(dn,
						dn->components,
						struct ldb_dn_component,
						n);
		if ( ! dn->components) {
			ldb_dn_mark_invalid(dn);
			return false;
		}

		for (i = dn->comp_num - 1, j = n - 1; i != (unsigned int) -1;
		     i--, j--) {
			dn->components[j] = dn->components[i];
		}

		for (i = 0; i < child->comp_num; i++) {
			dn->components[i] =
				ldb_dn_copy_component(dn->components,
							&child->components[i]);
			if (dn->components[i].value.data == NULL) {
				ldb_dn_mark_invalid(dn);
				return false;
			}
		}

		dn->comp_num = n;

		if (dn->casefold && s) {
			t = talloc_asprintf(dn, "%s,%s", s, dn->casefold);
			LDB_FREE(dn->casefold);
			dn->casefold = t;
		}
	}

	if (dn->linearized) {
		if (dn->linearized[0] == '\0') {
			return false;
		}

		s = ldb_dn_get_linearized(child);
		if ( ! s) {
			return false;
		}

		t = talloc_asprintf(dn, "%s,%s", s, dn->linearized);
		if ( ! t) {
			ldb_dn_mark_invalid(dn);
			return false;
		}
		LDB_FREE(dn->linearized);
		dn->linearized = t;
	}

	/* Wipe the ext_linearized DN,
	 * the GUID and SID are almost certainly no longer valid */
	LDB_FREE(dn->ext_linearized);
	LDB_FREE(dn->ext_components);
	dn->ext_comp_num = 0;

	return true;
}

/* modify the given dn by adding children elements.
 *
 * return true if successful and false if not
 * if false is returned the dn may be marked invalid
 */
bool ldb_dn_add_child_fmt(struct ldb_dn *dn, const char *child_fmt, ...)
{
	struct ldb_dn *child;
	char *child_str;
	va_list ap;
	bool ret;

	if ( !dn || dn->invalid) {
		return false;
	}

	va_start(ap, child_fmt);
	child_str = talloc_vasprintf(dn, child_fmt, ap);
	va_end(ap);

	if (child_str == NULL) {
		return false;
	}

	child = ldb_dn_new(child_str, dn->ldb, child_str);

	ret = ldb_dn_add_child(dn, child);

	talloc_free(child_str);

	return ret;
}

bool ldb_dn_remove_base_components(struct ldb_dn *dn, unsigned int num)
{
	unsigned int i;

	if ( ! ldb_dn_validate(dn)) {
		return false;
	}

	if (dn->comp_num < num) {
		return false;
	}

	/* free components */
	for (i = dn->comp_num - num; i < dn->comp_num; i++) {
		LDB_FREE(dn->components[i].name);
		LDB_FREE(dn->components[i].value.data);
		LDB_FREE(dn->components[i].cf_name);
		LDB_FREE(dn->components[i].cf_value.data);
	}

	dn->comp_num -= num;

	if (dn->valid_case) {
		for (i = 0; i < dn->comp_num; i++) {
			LDB_FREE(dn->components[i].cf_name);
			LDB_FREE(dn->components[i].cf_value.data);
		}
		dn->valid_case = false;
	}

	LDB_FREE(dn->casefold);
	LDB_FREE(dn->linearized);

	/* Wipe the ext_linearized DN,
	 * the GUID and SID are almost certainly no longer valid */
	LDB_FREE(dn->ext_linearized);
	LDB_FREE(dn->ext_components);
	dn->ext_comp_num = 0;

	return true;
}

bool ldb_dn_remove_child_components(struct ldb_dn *dn, unsigned int num)
{
	unsigned int i, j;

	if ( ! ldb_dn_validate(dn)) {
		return false;
	}

	if (dn->comp_num < num) {
		return false;
	}

	for (i = 0, j = num; j < dn->comp_num; i++, j++) {
		if (i < num) {
			LDB_FREE(dn->components[i].name);
			LDB_FREE(dn->components[i].value.data);
			LDB_FREE(dn->components[i].cf_name);
			LDB_FREE(dn->components[i].cf_value.data);
		}
		dn->components[i] = dn->components[j];
	}

	dn->comp_num -= num;

	if (dn->valid_case) {
		for (i = 0; i < dn->comp_num; i++) {
			LDB_FREE(dn->components[i].cf_name);
			LDB_FREE(dn->components[i].cf_value.data);
		}
		dn->valid_case = false;
	}

	LDB_FREE(dn->casefold);
	LDB_FREE(dn->linearized);

	/* Wipe the ext_linearized DN,
	 * the GUID and SID are almost certainly no longer valid */
	LDB_FREE(dn->ext_linearized);
	LDB_FREE(dn->ext_components);
	dn->ext_comp_num = 0;

	return true;
}

struct ldb_dn *ldb_dn_get_parent(TALLOC_CTX *mem_ctx, struct ldb_dn *dn)
{
	struct ldb_dn *new_dn;

	new_dn = ldb_dn_copy(mem_ctx, dn);
	if ( !new_dn ) {
		return NULL;
	}

	if ( ! ldb_dn_remove_child_components(new_dn, 1)) {
		talloc_free(new_dn);
		return NULL;
	}

	return new_dn;
}

/* Create a 'canonical name' string from a DN:

   ie dc=samba,dc=org -> samba.org/
      uid=administrator,ou=users,dc=samba,dc=org = samba.org/users/administrator

   There are two formats,
   the EX format has the last '/' replaced with a newline (\n).

*/
static char *ldb_dn_canonical(TALLOC_CTX *mem_ctx, struct ldb_dn *dn, int ex_format) {
	unsigned int i;
	TALLOC_CTX *tmpctx;
	char *cracked = NULL;
	const char *format = (ex_format ? "\n" : "/" );

	if ( ! ldb_dn_validate(dn)) {
		return NULL;
	}

	tmpctx = talloc_new(mem_ctx);

	/* Walk backwards down the DN, grabbing 'dc' components at first */
	for (i = dn->comp_num - 1; i != (unsigned int) -1; i--) {
		if (ldb_attr_cmp(dn->components[i].name, "dc") != 0) {
			break;
		}
		if (cracked) {
			cracked = talloc_asprintf(tmpctx, "%s.%s",
						  ldb_dn_escape_value(tmpctx,
							dn->components[i].value),
						  cracked);
		} else {
			cracked = ldb_dn_escape_value(tmpctx,
							dn->components[i].value);
		}
		if (!cracked) {
			goto done;
		}
	}

	/* Only domain components?  Finish here */
	if (i == (unsigned int) -1) {
		cracked = talloc_strdup_append_buffer(cracked, format);
		talloc_steal(mem_ctx, cracked);
		goto done;
	}

	/* Now walk backwards appending remaining components */
	for (; i > 0; i--) {
		cracked = talloc_asprintf_append_buffer(cracked, "/%s",
							ldb_dn_escape_value(tmpctx,
							dn->components[i].value));
		if (!cracked) {
			goto done;
		}
	}

	/* Last one, possibly a newline for the 'ex' format */
	cracked = talloc_asprintf_append_buffer(cracked, "%s%s", format,
						ldb_dn_escape_value(tmpctx,
							dn->components[i].value));

	talloc_steal(mem_ctx, cracked);
done:
	talloc_free(tmpctx);
	return cracked;
}

/* Wrapper functions for the above, for the two different string formats */
char *ldb_dn_canonical_string(TALLOC_CTX *mem_ctx, struct ldb_dn *dn) {
	return ldb_dn_canonical(mem_ctx, dn, 0);

}

char *ldb_dn_canonical_ex_string(TALLOC_CTX *mem_ctx, struct ldb_dn *dn) {
	return ldb_dn_canonical(mem_ctx, dn, 1);
}

int ldb_dn_get_comp_num(struct ldb_dn *dn)
{
	if ( ! ldb_dn_validate(dn)) {
		return -1;
	}
	return dn->comp_num;
}

int ldb_dn_get_extended_comp_num(struct ldb_dn *dn)
{
	if ( ! ldb_dn_validate(dn)) {
		return -1;
	}
	return dn->ext_comp_num;
}

const char *ldb_dn_get_component_name(struct ldb_dn *dn, unsigned int num)
{
	if ( ! ldb_dn_validate(dn)) {
		return NULL;
	}
	if (num >= dn->comp_num) return NULL;
	return dn->components[num].name;
}

const struct ldb_val *ldb_dn_get_component_val(struct ldb_dn *dn,
						unsigned int num)
{
	if ( ! ldb_dn_validate(dn)) {
		return NULL;
	}
	if (num >= dn->comp_num) return NULL;
	return &dn->components[num].value;
}

const char *ldb_dn_get_rdn_name(struct ldb_dn *dn)
{
	if ( ! ldb_dn_validate(dn)) {
		return NULL;
	}
	if (dn->comp_num == 0) return NULL;
	return dn->components[0].name;
}

const struct ldb_val *ldb_dn_get_rdn_val(struct ldb_dn *dn)
{
	if ( ! ldb_dn_validate(dn)) {
		return NULL;
	}
	if (dn->comp_num == 0) return NULL;
	return &dn->components[0].value;
}

int ldb_dn_set_component(struct ldb_dn *dn, int num,
			 const char *name, const struct ldb_val val)
{
	char *n;
	struct ldb_val v;

	if ( ! ldb_dn_validate(dn)) {
		return LDB_ERR_OTHER;
	}

	if (num >= dn->comp_num) {
		return LDB_ERR_OTHER;
	}

	n = talloc_strdup(dn, name);
	if ( ! n) {
		return LDB_ERR_OTHER;
	}

	v.length = val.length;
	v.data = (uint8_t *)talloc_memdup(dn, val.data, v.length+1);
	if ( ! v.data) {
		talloc_free(n);
		return LDB_ERR_OTHER;
	}

	talloc_free(dn->components[num].name);
	talloc_free(dn->components[num].value.data);
	dn->components[num].name = n;
	dn->components[num].value = v;

	if (dn->valid_case) {
		unsigned int i;
		for (i = 0; i < dn->comp_num; i++) {
			LDB_FREE(dn->components[i].cf_name);
			LDB_FREE(dn->components[i].cf_value.data);
		}
		dn->valid_case = false;
	}
	LDB_FREE(dn->casefold);
	LDB_FREE(dn->linearized);

	/* Wipe the ext_linearized DN,
	 * the GUID and SID are almost certainly no longer valid */
	LDB_FREE(dn->ext_linearized);
	LDB_FREE(dn->ext_components);
	dn->ext_comp_num = 0;

	return LDB_SUCCESS;
}

const struct ldb_val *ldb_dn_get_extended_component(struct ldb_dn *dn,
						    const char *name)
{
	unsigned int i;
	if ( ! ldb_dn_validate(dn)) {
		return NULL;
	}
	for (i=0; i < dn->ext_comp_num; i++) {
		if (ldb_attr_cmp(dn->ext_components[i].name, name) == 0) {
			return &dn->ext_components[i].value;
		}
	}
	return NULL;
}

int ldb_dn_set_extended_component(struct ldb_dn *dn,
				  const char *name, const struct ldb_val *val)
{
	struct ldb_dn_ext_component *p;
	unsigned int i;
	struct ldb_val v2;

	if ( ! ldb_dn_validate(dn)) {
		return LDB_ERR_OTHER;
	}

	if (!ldb_dn_extended_syntax_by_name(dn->ldb, name)) {
		/* We don't know how to handle this type of thing */
		return LDB_ERR_INVALID_DN_SYNTAX;
	}

	for (i=0; i < dn->ext_comp_num; i++) {
		if (ldb_attr_cmp(dn->ext_components[i].name, name) == 0) {
			if (val) {
				dn->ext_components[i].value =
					ldb_val_dup(dn->ext_components, val);

				dn->ext_components[i].name =
					talloc_strdup(dn->ext_components, name);
				if (!dn->ext_components[i].name ||
				    !dn->ext_components[i].value.data) {
					ldb_dn_mark_invalid(dn);
					return LDB_ERR_OPERATIONS_ERROR;
				}
			} else {
				if (i != (dn->ext_comp_num - 1)) {
					memmove(&dn->ext_components[i],
						&dn->ext_components[i+1],
						((dn->ext_comp_num-1) - i) *
						  sizeof(*dn->ext_components));
				}
				dn->ext_comp_num--;

				dn->ext_components = talloc_realloc(dn,
						   dn->ext_components,
						   struct ldb_dn_ext_component,
						   dn->ext_comp_num);
				if (!dn->ext_components) {
					ldb_dn_mark_invalid(dn);
					return LDB_ERR_OPERATIONS_ERROR;
				}
			}
			LDB_FREE(dn->ext_linearized);

			return LDB_SUCCESS;
		}
	}

	if (val == NULL) {
		/* removing a value that doesn't exist is not an error */
		return LDB_SUCCESS;
	}

	v2 = *val;

	p = dn->ext_components
		= talloc_realloc(dn,
				 dn->ext_components,
				 struct ldb_dn_ext_component,
				 dn->ext_comp_num + 1);
	if (!dn->ext_components) {
		ldb_dn_mark_invalid(dn);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	p[dn->ext_comp_num].value = ldb_val_dup(dn->ext_components, &v2);
	p[dn->ext_comp_num].name = talloc_strdup(p, name);

	if (!dn->ext_components[i].name || !dn->ext_components[i].value.data) {
		ldb_dn_mark_invalid(dn);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	dn->ext_components = p;
	dn->ext_comp_num++;

	LDB_FREE(dn->ext_linearized);

	return LDB_SUCCESS;
}

void ldb_dn_remove_extended_components(struct ldb_dn *dn)
{
	LDB_FREE(dn->ext_linearized);
	LDB_FREE(dn->ext_components);
	dn->ext_comp_num = 0;
}

bool ldb_dn_is_valid(struct ldb_dn *dn)
{
	if ( ! dn) return false;
	return ! dn->invalid;
}

bool ldb_dn_is_special(struct ldb_dn *dn)
{
	if ( ! dn || dn->invalid) return false;
	return dn->special;
}

bool ldb_dn_has_extended(struct ldb_dn *dn)
{
	if ( ! dn || dn->invalid) return false;
	if (dn->ext_linearized && (dn->ext_linearized[0] == '<')) return true;
	return dn->ext_comp_num != 0;
}

bool ldb_dn_check_special(struct ldb_dn *dn, const char *check)
{
	if ( ! dn || dn->invalid) return false;
	return ! strcmp(dn->linearized, check);
}

bool ldb_dn_is_null(struct ldb_dn *dn)
{
	if ( ! dn || dn->invalid) return false;
	if (ldb_dn_has_extended(dn)) return false;
	if (dn->linearized && (dn->linearized[0] == '\0')) return true;
	return false;
}

/*
  this updates dn->components, taking the components from ref_dn.
  This is used by code that wants to update the DN path of a DN
  while not impacting on the extended DN components
 */
int ldb_dn_update_components(struct ldb_dn *dn, const struct ldb_dn *ref_dn)
{
	dn->components = talloc_realloc(dn, dn->components,
					struct ldb_dn_component, ref_dn->comp_num);
	if (!dn->components) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	memcpy(dn->components, ref_dn->components,
	       sizeof(struct ldb_dn_component)*ref_dn->comp_num);
	dn->comp_num = ref_dn->comp_num;

	LDB_FREE(dn->casefold);
	LDB_FREE(dn->linearized);
	LDB_FREE(dn->ext_linearized);

	return LDB_SUCCESS;
}

/*
  minimise a DN. The caller must pass in a validated DN.

  If the DN has an extended component then only the first extended
  component is kept, the DN string is stripped.

  The existing dn is modified
 */
bool ldb_dn_minimise(struct ldb_dn *dn)
{
	unsigned int i;

	if (!ldb_dn_validate(dn)) {
		return false;
	}
	if (dn->ext_comp_num == 0) {
		return true;
	}

	/* free components */
	for (i = 0; i < dn->comp_num; i++) {
		LDB_FREE(dn->components[i].name);
		LDB_FREE(dn->components[i].value.data);
		LDB_FREE(dn->components[i].cf_name);
		LDB_FREE(dn->components[i].cf_value.data);
	}
	dn->comp_num = 0;
	dn->valid_case = false;

	LDB_FREE(dn->casefold);
	LDB_FREE(dn->linearized);

	/* note that we don't free dn->components as this there are
	 * several places in ldb_dn.c that rely on it being non-NULL
	 * for an exploded DN
	 */

	for (i = 1; i < dn->ext_comp_num; i++) {
		LDB_FREE(dn->ext_components[i].name);
		LDB_FREE(dn->ext_components[i].value.data);
	}
	dn->ext_comp_num = 1;

	dn->ext_components = talloc_realloc(dn, dn->ext_components, struct ldb_dn_ext_component, 1);
	if (dn->ext_components == NULL) {
		ldb_dn_mark_invalid(dn);
		return false;
	}

	LDB_FREE(dn->ext_linearized);

	return true;
}
