/*
 * Samba Unix/Linux SMB client library
 *
 * Copyright (C) Gregor Beck 2010
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

/**
 * @brief  Format dot.reg files
 * @file   reg_format.c
 * @author Gregor Beck <gb@sernet.de>
 * @date   Sep 2010
 */

#include "includes.h"
#include "reg_format.h"
#include "reg_parse.h"
#include "reg_parse_internal.h"
#include "cbuf.h"
#include "srprs.h"
#include "registry.h"
#include "registry/reg_objects.h"
#include <assert.h>

static void cstr_unescape(char* val)
{
	all_string_sub(val, "\\r", "\r", 0);
	all_string_sub(val, "\\n", "\n", 0);
	all_string_sub(val, "\\t", "\t", 0);
	all_string_sub(val, "\\\\", "\\", 0);
}

/******************************************************************************/

/**
 * Print value assign to stream.
 *
 * @param[out] ost outstream
 * @param[in]  name string
 *
 * @return numner of bytes written, -1 on error
 * @see srprs_val_name
 */
static int cbuf_print_value_assign(cbuf* ost, const char* name) {
	int ret, n;
	if (*name == '\0') {
		ret = cbuf_putc(ost, '@');
	} else {
		ret = cbuf_print_quoted_string(ost, name);
	}

	if (ret<0) {
		return ret;
	}

	n = cbuf_putc(ost, '=');
	if (n < 0) {
		return n;
	}
	ret += n;

	return ret;
}

enum fmt_hive {
	FMT_HIVE_PRESERVE=0,
	FMT_HIVE_SHORT,
	FMT_HIVE_LONG
};


struct fmt_key {
	enum fmt_hive hive_fmt;
	enum fmt_case hive_case;
	enum fmt_case key_case;
	const char*   sep;
};


static int
cbuf_print_hive(cbuf* ost, const char* hive, int len, const struct fmt_key* fmt)
{
	if (fmt->hive_fmt != FMT_HIVE_PRESERVE) {
		const struct hive_info* hinfo = hive_info(hive, len);
		if (hinfo == NULL) {
			DEBUG(0, ("Unknown hive %*s", len, hive));
		} else {
			switch(fmt->hive_fmt) {
			case FMT_HIVE_SHORT:
				hive = hinfo->short_name;
				len  = hinfo->short_name_len;
				break;
			case FMT_HIVE_LONG:
				hive = hinfo->long_name;
				len  = hinfo->long_name_len;
				break;
			default:
				DEBUG(0, ("Unsupported hive format %d",
					  (int)fmt->hive_fmt));
				return -1;
			}
		}
	}

	return cbuf_puts_case(ost, hive, len, fmt->hive_case);
}

static int
cbuf_print_keyname(cbuf* ost, const char* key[], int n, const struct fmt_key* fmt)
{
	int r, ret=0;
	size_t pos = cbuf_getpos(ost);
	bool hive = true;

	for (; n>0; key++, n--) {
		const char* start = *key;
		while(*start != '\0') {
			const char* end = start;
			while(*end != '\\' && *end != '\0') {
				end++;
			}

			if (hive) {
				r = cbuf_print_hive(ost, start, end-start, fmt);
				if (r < 0) {
					goto fail;
				}

				ret += r;
				hive = false;
			} else {
				r = cbuf_puts(ost, fmt->sep, -1);
				if (r < 0) {
					goto fail;
				}
				ret += r;

				r = cbuf_puts_case(ost, start, end-start, fmt->key_case);
				if (r < 0) {
					goto fail;
				}
				ret += r;
			}

			while(*end == '\\') {
				end++;
			}
			start = end;
		}
	}
	return ret;
fail:
	cbuf_setpos(ost, pos);
	return r;
}
/**@}*/

/**
 * @defgroup reg_format Format dot.reg file.
 * @{
 */

struct reg_format
{
	struct reg_parse_callback reg_parse_callback;
	struct reg_format_callback call;
	unsigned flags;
	smb_iconv_t fromUTF16;
	const char* sep;
};

int reg_format_value_delete(struct reg_format* f, const char* name)
{
	int ret;
	cbuf* line = cbuf_new(f);

	ret = cbuf_print_value_assign(line, name);
	if (ret < 0) {
		goto done;
	}

	ret = cbuf_putc(line, '-');
	if (ret < 0 ) {
		goto done;
	}

	ret = f->call.writeline(f->call.data, cbuf_gets(line, 0));
done:
	talloc_free(line);
	return ret;
}

/* Todo: write hex if str contains CR or LF */
static int
reg_format_value_sz(struct reg_format* f, const char* name, const char* str)
{
	int ret;
	cbuf* line = cbuf_new(f);

	ret = cbuf_print_value_assign(line, name);
	if (ret < 0) {
		goto done;
	}

	ret = cbuf_print_quoted_string(line, str);
	if (ret < 0) {
		goto done;
	}

	ret = f->call.writeline(f->call.data, cbuf_gets(line, 0));

done:
	talloc_free(line);
	return ret;
}

static int reg_format_value_dw(struct reg_format* f, const char* name, uint32_t dw)
{
	int ret;
	cbuf* line = cbuf_new(f);

	ret = cbuf_print_value_assign(line, name);
	if (ret < 0) {
		goto done;
	}

	ret = cbuf_printf(line, "dword:%08x", dw);
	if (ret < 0) {
		goto done;
	}

	ret = f->call.writeline(f->call.data, cbuf_gets(line, 0));
done:
	talloc_free(line);
	return ret;
}

static int reg_format_value_hex(struct reg_format* f, const char* name, uint32_t type,
				const void* data, size_t len)
{
	int n;
	int cpl=0;
	int ret=0;
	const unsigned char* ptr;

	cbuf* line = cbuf_new(f);

	n = cbuf_print_value_assign(line, name);
	if (n < 0) {
		ret = n;
		goto done;
	}

	cpl += n;

	if (type==REG_BINARY && !(f->flags & REG_FMT_HEX_BIN)) {
		n=cbuf_puts(line, "hex:", -1);
	} else {
		n=cbuf_printf(line, "hex(%x):", type);
	}
	if (n < 0) {
		ret = n;
		goto done;
	}

	cpl += n;

	for (ptr=(const unsigned char *)data; len>1; len--,ptr++) {
		n = cbuf_printf(line, "%02x,", (unsigned)(*ptr));
		if (n < 0) {
			return n;
		}
		cpl += n;

		if ( cpl > 76 ) {
			n = cbuf_putc(line, '\\');
			if (n< 0) {
				return n;
			}

			n = f->call.writeline(f->call.data, cbuf_gets(line,0));
			if (n < 0) {
				ret = n;
				goto done;
			}
			ret += n;

			cbuf_clear(line);
			cpl = cbuf_puts(line, "  ", -1);
			if (cpl < 0) {
				ret = cpl;
				goto done;
			}
		}
	}

	if ( len > 0 ) {
		n = cbuf_printf(line, "%02x", (unsigned)(*ptr));
		if (n < 0) {
			ret = n;
			goto done;
		}
		cpl += n;
	}

	n = f->call.writeline(f->call.data, cbuf_gets(line,0));
	if (n < 0) {
		ret = n;
		goto done;
	}
	ret += n;
done:
	talloc_free(line);
	return ret;
}

static bool is_zero_terminated_ucs2(const uint8_t* data, size_t len) {
	const size_t idx = len/sizeof(smb_ucs2_t);
	const smb_ucs2_t *str = (const smb_ucs2_t*)data;

	if ((len % sizeof(smb_ucs2_t)) != 0) {
		return false;
	}

	if (idx == 0) {
		return false;
	}

	return (str[idx-1] == 0);
}

int reg_format_value(struct reg_format* f, const char* name, uint32_t type,
		     const uint8_t* data, size_t len)
{
	int ret = 0;
	void* mem_ctx = talloc_new(f);

	switch (type) {
	case REG_SZ:
		if (!(f->flags & REG_FMT_HEX_SZ)
		    && is_zero_terminated_ucs2(data, len))
		{
			char* str = NULL;
			size_t dlen;
			if (pull_ucs2_talloc(mem_ctx, &str, (const smb_ucs2_t*)data, &dlen)) {
				ret = reg_format_value_sz(f, name, str);
				goto done;
			} else {
				DEBUG(0, ("reg_format_value %s: "
					  "pull_ucs2_talloc failed"
					  ", try to write hex\n", name));
			}
		}
		break;

	case REG_DWORD:
		if (!(f->flags & REG_FMT_HEX_SZ) && (len == sizeof(uint32_t))) {
			uint32_t dw = IVAL(data,0);
			ret = reg_format_value_dw(f, name, dw);
			goto done;
		}
		break;

	case REG_MULTI_SZ:
	case REG_EXPAND_SZ:
		if (f->fromUTF16 && (f->fromUTF16 != ((smb_iconv_t)-1))) {
			char* str = NULL;
			size_t dlen = iconvert_talloc(mem_ctx, f->fromUTF16,
						      (const char*)data, len, &str);
			if (dlen != -1) {
				ret = reg_format_value_hex(f, name, type, str, dlen);
				goto done;
			} else {
				DEBUG(0, ("reg_format_value %s: "
					  "iconvert_talloc failed"
					  ", try to write hex\n", name));
			}
		}
		break;
	default:
		break;
	}

	ret = reg_format_value_hex(f, name, type, data, len);
done:
	talloc_free(mem_ctx);
	return ret;
}


int reg_format_comment(struct reg_format* f, const char* txt)
{
	int ret;
	cbuf* line = cbuf_new(f);

	ret = cbuf_putc(line,';');
	if (ret<0) {
		goto done;
	}

	ret = cbuf_puts(line, txt, -1);
	if (ret < 0) {
		goto done;
	}

	ret = f->call.writeline(f->call.data, cbuf_gets(line, 0));
done:
	talloc_free(line);
	return ret;
}


/******************************************************************************/



struct reg_format* reg_format_new(const void* talloc_ctx,
				  struct reg_format_callback cb,
				  const char* str_enc, unsigned flags,
				  const char* sep)
{
	static const struct reg_parse_callback reg_parse_callback_default = {
		.key = (reg_parse_callback_key_t)&reg_format_key,
		.val = (reg_parse_callback_val_t)&reg_format_value,
		.val_del = (reg_parse_callback_val_del_t)&reg_format_value_delete,
		.comment = (reg_parse_callback_comment_t)&reg_format_comment,
	};

	struct reg_format* f = talloc_zero(talloc_ctx, struct reg_format);
	if (f == NULL) {
		return NULL;
	}

	f->reg_parse_callback = reg_parse_callback_default;
	f->reg_parse_callback.data = f;

	f->call = cb;
	f->flags = flags;
	f->sep   = sep;

	if (str_enc && !set_iconv(&f->fromUTF16, str_enc, "UTF-16LE")) {
		DEBUG(0, ("reg_format_new: failed to set encoding: %s\n",
			  str_enc));
		goto fail;
	}

	assert(&f->reg_parse_callback == (struct reg_parse_callback*)f);
	return f;
fail:
	talloc_free(f);
	return NULL;
}

int reg_format_set_options(struct reg_format* fmt, const char* options)
{
	static const char* DEFAULT ="enc=unix,flags=0,sep=\\";

	int ret = 0;
	char *key, *val;
	void* ctx = talloc_new(fmt);

	if (options == NULL) {
		options = DEFAULT;
	}

	while (srprs_option(&options, ctx, &key, &val)) {
		if ((strcmp(key, "enc") == 0) || (strcmp(key, "strenc") == 0)) {
			if (!set_iconv(&fmt->fromUTF16, val, "UTF-16LE")) {
				DEBUG(0, ("Failed to set encoding: %s\n", val));
				ret = -1;
			}
		} else if ((strcmp(key, "flags") == 0) && (val != NULL)) {
			char* end = NULL;
			if (val != NULL) {
				fmt->flags = strtol(val, &end, 0);
			}
			if ((end==NULL) || (*end != '\0')) {
				DEBUG(0, ("Invalid flags format: %s\n",
					  val ? val : "<NULL>"));
				ret = -1;
			}
		} else if ((strcmp(key, "sep") == 0) && (val != NULL)) {
			cstr_unescape(val);
			fmt->sep = talloc_steal(fmt, val);
		}

		/* else if (strcmp(key, "hive") == 0) { */
		/* 	if (strcmp(val, "short") == 0) { */
		/* 		f->hive_fmt = REG_FMT_SHORT_HIVES; */
		/* 	} else if (strcmp(val, "long") == 0) { */
		/* 		f->hive_fmt = REG_FMT_LONG_HIVES; */
		/* 	} else if (strcmp(val, "preserve") == 0) { */
		/* 		f->hive_fmt = REG_FMT_PRESERVE_HIVES; */
		/* 	} else { */
		/* 		DEBUG(0, ("Invalid hive format: %s\n", val)); */
		/* 		ret = -1; */
		/* 	} */
		/* } */
	}
	talloc_free(ctx);
	return ret;
}

int reg_format_key(struct reg_format* f, const char* key[], size_t n, bool del)
{
	int ret, r;
	cbuf* line = cbuf_new(f);
	struct fmt_key key_fmt = {
		.key_case  = (f->flags >>  4) & 0x0F,
		.hive_case = (f->flags >>  8) & 0x0F,
		.hive_fmt  = (f->flags >> 12) & 0x0F,
		.sep       = f->sep,
	};

	ret = cbuf_putc(line, '[');
	if (ret < 0) {
		goto done;
	}

	if (del) {
		ret = cbuf_putc(line, '-');
		if (ret < 0) {
			goto done;
		}
	}

	ret = cbuf_print_keyname(line, key, n, &key_fmt);
	if (ret < 0) {
		goto done;
	}

	ret = cbuf_putc(line, ']');
	if (ret < 0) {
		goto done;
	}

	ret = f->call.writeline(f->call.data, "");
	if (ret < 0) {
		goto done;
	}

	r = f->call.writeline(f->call.data, cbuf_gets(line, 0));
	if (r < 0) {
		ret = r;
		goto done;
	}
	ret += r;

done:
	talloc_free(line);
	return ret;
}


int reg_format_registry_key(struct reg_format* f, struct registry_key* key,
			    bool del)
{
	return reg_format_key(f, (const char**)&key->key->name, 1, del);
}

int reg_format_registry_value(struct reg_format* f, const char* name,
			      struct registry_value* val)
{
	return reg_format_value(f, name, val->type,
				val->data.data, val->data.length);
}

int reg_format_regval_blob(struct reg_format* f, const char* name,
			   struct regval_blob* val)
{

	return reg_format_value(f,
				name ? name : regval_name(val),
				regval_type(val),
				regval_data_p(val),
				regval_size(val));
}

/**@}*/


struct reg_format_file
{
	FILE* file;
	const char* encoding;
	smb_iconv_t fromUnix;
	char* nl;
	size_t nl_len;
};


static int reg_format_file_close(struct reg_format* fmt)
{
	struct reg_format_file* fmt_ctx
		= (struct reg_format_file*) fmt->call.data;
	int ret = 0;
	FILE* file = fmt_ctx->file;

	if (fmt_ctx->encoding) {
		char buf[32];
		snprintf(buf, sizeof(buf), "coding: %s", fmt_ctx->encoding);
		reg_format_comment(fmt, "Local Variables:");
		reg_format_comment(fmt, buf);
		reg_format_comment(fmt, "End:");
	}

	if (file != NULL) {
		ret = fclose(file);
	}

	return ret;
}

static int reg_format_file_writeline(void* ptr, const char* line)
{
	size_t size;
	char* dst=NULL;
	struct reg_format_file* fmt_ctx = (struct reg_format_file*)ptr;
	int ret, r;

	size = iconvert_talloc(ptr, fmt_ctx->fromUnix, line, strlen(line), &dst);
	if (size == -1 ) {
		DEBUG(0, ("reg_format_file_writeline: iconvert_talloc failed >%s<\n",  line));
		return -1;
	}

	ret = fwrite(dst, 1, size, fmt_ctx->file);
	if (ret < 0) {
		goto done;
	}

	r = fwrite(fmt_ctx->nl, 1, fmt_ctx->nl_len, fmt_ctx->file);
	ret = (r < 0) ? r : ret + r;

done:
	talloc_free(dst);
	return ret;
}

struct reg_format_file_opt {
	const char* head;
	const char* nl;
	const char* enc;
	bool bom;
	const char* str_enc;
	unsigned flags;
	const char* sep;
};

static struct reg_format_file_opt reg_format_file_opt(void* mem_ctx, const char* opt)
{
	static const struct reg_format_file_opt REG4 = {
		.head = "REGEDIT4",
		.nl   = "\r\n",
		.enc  = "dos",
		.str_enc = "dos",
		.bom  = false,
		.flags = (FMT_HIVE_LONG << 12),
		.sep   = "\\",
	};

	static const struct reg_format_file_opt REG5 = {
		.head = "Windows Registry Editor Version 5.00",
		.nl   = "\r\n",
		.enc  = "UTF-16LE",
		.str_enc = "UTF-16LE",
		.bom  = true,
		.flags = (FMT_HIVE_LONG << 12),
		.sep   = "\\",
	};

	struct reg_format_file_opt ret = {
		.head = REG5.head,
		.nl   = "\n",
		.enc  = "unix",
		.bom  = false,
		.str_enc = "UTF-16LE",
		.flags = 0,
		.sep   = "\\",
	};

	void* tmp_ctx = talloc_new(mem_ctx);

	char *key, *val;

	if (opt == NULL) {
		goto done;
	}

	while(srprs_option(&opt, tmp_ctx, &key, &val)) {
		if (strcmp(key, "enc") == 0) {
			ret.enc     = talloc_steal(mem_ctx, val);
			ret.str_enc = ret.enc;
		} else if (strcmp(key, "strenc") == 0) {
			ret.str_enc = talloc_steal(mem_ctx, val);
		} else if (strcmp(key, "fileenc") == 0) {
			ret.enc = talloc_steal(mem_ctx, val);
		} else if ((strcmp(key, "flags") == 0) && (val != NULL)) {
			char* end = NULL;
			if (val != NULL) {
				ret.flags = strtol(val, &end, 0);
			}
			if ((end==NULL) || (*end != '\0')) {
				DEBUG(0, ("Invalid flags format: %s\n",
					  val ? val : "<NULL>"));
			}
		} else if ((strcmp(key, "sep") == 0) && (val != NULL)) {
			cstr_unescape(val);
			ret.sep = talloc_steal(mem_ctx, val);
		} else if (strcmp(key, "head") == 0) {
			cstr_unescape(val);
			ret.head = talloc_steal(mem_ctx, val);
		} else if (strcmp(key, "nl") == 0) {
			cstr_unescape(val);
			ret.nl = talloc_steal(mem_ctx, val);
		} else if (strcmp(key, "bom") == 0) {
			if (val == NULL) {
				ret.bom = true;
			} else {
				ret.bom = atoi(val);
			}
		} else if (strcmp(key, "regedit4") == 0) {
			ret = REG4;
		} else if (strcmp(key, "regedit5") == 0) {
			ret = REG5;
		}
	}
done:
	talloc_free(tmp_ctx);
	return ret;
}


struct reg_format* reg_format_file(const void* talloc_ctx,
				   const char* filename,
				   const char* options)
{
	struct reg_format_file* fmt_ctx;
	struct reg_format* fmt;
	int ret;
	struct reg_format_file_opt opt;

	struct reg_format_callback reg_format_cb = {
		.writeline = &reg_format_file_writeline
	};

	fmt_ctx = talloc_zero(talloc_ctx, struct reg_format_file);
	if (fmt_ctx == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	opt = reg_format_file_opt(fmt_ctx, options);

	reg_format_cb.data = fmt_ctx;

	fmt = reg_format_new(talloc_ctx, reg_format_cb,
			     opt.str_enc, opt.flags, opt.sep);
	if (fmt == NULL) {
		errno = ENOMEM;
		talloc_free(fmt_ctx);
		return NULL;
	}

	talloc_steal(fmt, fmt_ctx);

	if (!set_iconv(&fmt->fromUTF16, opt.str_enc, "UTF-16LE")) { /* HACK */
		DEBUG(0, ("reg_format_file: failed to set string encoding %s",
			      opt.str_enc));
		goto fail;
	}

	if (!set_iconv(&fmt_ctx->fromUnix, opt.enc, "unix")) {
		DEBUG(0, ("reg_format_file: failed to set file encoding %s",
			  opt.enc));
		goto fail;
	}
	fmt_ctx->encoding = talloc_strdup(fmt_ctx, get_charset(opt.enc));

	fmt_ctx->file = fopen(filename, "w");
	if (fmt_ctx->file == NULL) {
		DEBUG(0, ("reg_format_file: fopen failed: %s\n", strerror(errno)));
		goto fail;
	}

	if (setvbuf(fmt_ctx->file, NULL, _IOFBF, 64000) < 0) {
		DEBUG(0, ("reg_format_file: setvbuf failed: %s\n", strerror(errno)));
	}

	talloc_set_destructor(fmt, reg_format_file_close);

	fmt_ctx->nl_len = iconvert_talloc(fmt, fmt_ctx->fromUnix, opt.nl, strlen(opt.nl), &fmt_ctx->nl);
	if (fmt_ctx->nl_len == -1) {
		DEBUG(0, ("iconvert_talloc failed\n"));
		goto fail;
	}

	if (opt.bom) {
		ret = write_bom(fmt_ctx->file, opt.enc, -1);
		if (ret < 0) {
			goto fail;
		}
	}

	ret = fmt->call.writeline(fmt->call.data, opt.head);
	if (ret < 0) {
		goto fail;
	}

	return fmt;
fail:
	talloc_free(fmt);
	return NULL;
}
