/*
 * Copyright (C) 2013 Felix Fietkau <nbd@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/stat.h>
#include <regex.h>

#include "avl-cmp.h"
#include "json_script.h"

struct json_call {
	struct json_script_ctx *ctx;
	struct blob_attr *vars;
	unsigned int seq;
};

struct json_handler {
	const char *name;
	int (*cb)(struct json_call *call, struct blob_attr *cur);
};

static int json_process_expr(struct json_call *call, struct blob_attr *cur);
static int json_process_cmd(struct json_call *call, struct blob_attr *cur);

struct json_script_file *
json_script_file_from_blobmsg(const char *name, void *data, int len)
{
	struct json_script_file *f;
	char *new_name;
	int name_len = 0;

	if (name)
		name_len = strlen(name) + 1;

	f = calloc_a(sizeof(*f) + len, &new_name, name_len);
	memcpy(f->data, data, len);
	if (name)
		f->avl.key = strcpy(new_name, name);

	return f;
}

static struct json_script_file *
json_script_get_file(struct json_script_ctx *ctx, const char *filename)
{
	struct json_script_file *f;

	f = avl_find_element(&ctx->files, filename, f, avl);
	if (f)
		return f;

	f = ctx->handle_file(ctx, filename);
	if (!f)
		return NULL;

	avl_insert(&ctx->files, &f->avl);
	return f;
}

static void __json_script_run(struct json_call *call, struct json_script_file *file,
			      struct blob_attr *context)
{
	struct json_script_ctx *ctx = call->ctx;

	if (file->seq == call->seq) {
		if (context)
			ctx->handle_error(ctx, "Recursive include", context);

		return;
	}

	file->seq = call->seq;
	while (file) {
		json_process_cmd(call, file->data);
		file = file->next;
	}
}

const char *json_script_find_var(struct json_script_ctx *ctx, struct blob_attr *vars,
				 const char *name)
{
	struct blob_attr *cur;
	int rem;

	blobmsg_for_each_attr(cur, vars, rem) {
		if (blobmsg_type(cur) != BLOBMSG_TYPE_STRING)
			continue;

		if (strcmp(blobmsg_name(cur), name) != 0)
			continue;

		return blobmsg_data(cur);
	}

	return ctx->handle_var(ctx, name, vars);
}

static const char *
msg_find_var(struct json_call *call, const char *name)
{
	return json_script_find_var(call->ctx, call->vars, name);
}

static void
json_get_tuple(struct blob_attr *cur, struct blob_attr **tb, int t1, int t2)
{
	static struct blobmsg_policy expr_tuple[3] = {
		{ .type = BLOBMSG_TYPE_STRING },
		{},
		{},
	};

	expr_tuple[1].type = t1;
	expr_tuple[2].type = t2;
	blobmsg_parse_array(expr_tuple, 3, tb, blobmsg_data(cur), blobmsg_data_len(cur));
}

static int handle_if(struct json_call *call, struct blob_attr *expr)
{
	struct blob_attr *tb[4];
	int ret;

	static const struct blobmsg_policy if_tuple[4] = {
		{ .type = BLOBMSG_TYPE_STRING },
		{ .type = BLOBMSG_TYPE_ARRAY },
		{ .type = BLOBMSG_TYPE_ARRAY },
		{ .type = BLOBMSG_TYPE_ARRAY },
	};

	blobmsg_parse_array(if_tuple, 4, tb, blobmsg_data(expr), blobmsg_data_len(expr));

	if (!tb[1] || !tb[2])
		return 0;

	ret = json_process_expr(call, tb[1]);
	if (ret < 0)
		return 0;

	if (ret)
		return json_process_cmd(call, tb[2]);

	if (!tb[3])
		return 0;

	return json_process_cmd(call, tb[3]);
}

static int handle_case(struct json_call *call, struct blob_attr *expr)
{
	struct blob_attr *tb[3], *cur;
	const char *var;
	int rem;

	json_get_tuple(expr, tb, BLOBMSG_TYPE_STRING, BLOBMSG_TYPE_TABLE);
	if (!tb[1] || !tb[2])
		return 0;

	var = msg_find_var(call, blobmsg_data(tb[1]));
	if (!var)
		return 0;

	blobmsg_for_each_attr(cur, tb[2], rem) {
		if (!strcmp(var, blobmsg_name(cur)))
			return json_process_cmd(call, cur);
	}

	return 0;
}

static int handle_return(struct json_call *call, struct blob_attr *expr)
{
	return -2;
}

static int handle_include(struct json_call *call, struct blob_attr *expr)
{
	struct blob_attr *tb[3];
	struct json_script_file *f;

	json_get_tuple(expr, tb, BLOBMSG_TYPE_STRING, 0);
	if (!tb[1])
		return 0;

	f = json_script_get_file(call->ctx, blobmsg_data(tb[1]));
	if (!f)
		return 0;

	__json_script_run(call, f, expr);
	return 0;
}

static const struct json_handler cmd[] = {
	{ "if", handle_if },
	{ "case", handle_case },
	{ "return", handle_return },
	{ "include", handle_include },
};

static int eq_regex_cmp(const char *str, const char *pattern, bool regex)
{
	regex_t reg;
	int ret;

	if (!regex)
		return !strcmp(str, pattern);

	if (regcomp(&reg, pattern, REG_EXTENDED | REG_NOSUB))
		return 0;

	ret = !regexec(&reg, str, 0, NULL, 0);
	regfree(&reg);

	return ret;
}

static int expr_eq_regex(struct json_call *call, struct blob_attr *expr, bool regex)
{
	struct json_script_ctx *ctx = call->ctx;
	struct blob_attr *tb[3], *cur;
	const char *var;
	int rem;

	json_get_tuple(expr, tb, BLOBMSG_TYPE_STRING, 0);
	if (!tb[1] || !tb[2])
		return -1;

	var = msg_find_var(call, blobmsg_data(tb[1]));
	if (!var)
		return 0;

	switch(blobmsg_type(tb[2])) {
	case BLOBMSG_TYPE_STRING:
		return eq_regex_cmp(var, blobmsg_data(tb[2]), regex);
	case BLOBMSG_TYPE_ARRAY:
		blobmsg_for_each_attr(cur, tb[2], rem) {
			if (blobmsg_type(cur) != BLOBMSG_TYPE_STRING) {
				ctx->handle_error(ctx, "Unexpected element type", cur);
				return -1;
			}

			if (eq_regex_cmp(var, blobmsg_data(cur), regex))
				return 1;
		}
		return 0;
	default:
		ctx->handle_error(ctx, "Unexpected element type", tb[2]);
		return -1;
	}
}

static int handle_expr_eq(struct json_call *call, struct blob_attr *expr)
{
	return expr_eq_regex(call, expr, false);
}

static int handle_expr_regex(struct json_call *call, struct blob_attr *expr)
{
	return expr_eq_regex(call, expr, true);
}

static int handle_expr_has(struct json_call *call, struct blob_attr *expr)
{
	struct json_script_ctx *ctx = call->ctx;
	struct blob_attr *tb[3], *cur;
	int rem;

	json_get_tuple(expr, tb, 0, 0);
	if (!tb[1])
		return -1;

	switch(blobmsg_type(tb[1])) {
	case BLOBMSG_TYPE_STRING:
		return !!msg_find_var(call, blobmsg_data(tb[1]));
	case BLOBMSG_TYPE_ARRAY:
		blobmsg_for_each_attr(cur, tb[1], rem) {
			if (blobmsg_type(cur) != BLOBMSG_TYPE_STRING) {
				ctx->handle_error(ctx, "Unexpected element type", cur);
				return -1;
			}

			if (msg_find_var(call, blobmsg_data(cur)))
				return 1;
		}
		return 0;
	default:
		ctx->handle_error(ctx, "Unexpected element type", tb[1]);
		return -1;
	}
}

static int expr_and_or(struct json_call *call, struct blob_attr *expr, bool and)
{
	struct blob_attr *cur;
	int ret, rem;
	int i = 0;

	blobmsg_for_each_attr(cur, expr, rem) {
		if (i++ < 1)
			continue;

		ret = json_process_expr(call, cur);
		if (ret < 0)
			return ret;

		if (ret != and)
			return ret;
	}

	return and;
}

static int handle_expr_and(struct json_call *call, struct blob_attr *expr)
{
	return expr_and_or(call, expr, 1);
}

static int handle_expr_or(struct json_call *call, struct blob_attr *expr)
{
	return expr_and_or(call, expr, 0);
}

static int handle_expr_not(struct json_call *call, struct blob_attr *expr)
{
	struct blob_attr *tb[3];

	json_get_tuple(expr, tb, BLOBMSG_TYPE_ARRAY, 0);
	if (!tb[1])
		return -1;

	return json_process_expr(call, tb[1]);
}

static const struct json_handler expr[] = {
	{ "eq", handle_expr_eq },
	{ "regex", handle_expr_regex },
	{ "has", handle_expr_has },
	{ "and", handle_expr_and },
	{ "or", handle_expr_or },
	{ "not", handle_expr_not },
};

static int
__json_process_type(struct json_call *call, struct blob_attr *cur,
		    const struct json_handler *h, int n, bool *found)
{
	const char *name = blobmsg_data(blobmsg_data(cur));
	int i;

	for (i = 0; i < n; i++) {
		if (strcmp(name, h[i].name) != 0)
			continue;

		*found = true;
		return h[i].cb(call, cur);
	}

	*found = false;
	return -1;
}

static int json_process_expr(struct json_call *call, struct blob_attr *cur)
{
	struct json_script_ctx *ctx = call->ctx;
	bool found;
	int ret;

	if (blobmsg_type(cur) != BLOBMSG_TYPE_ARRAY ||
	    blobmsg_type(blobmsg_data(cur)) != BLOBMSG_TYPE_STRING) {
		ctx->handle_error(ctx, "Unexpected element type", cur);
		return -1;
	}

	ret = __json_process_type(call, cur, expr, ARRAY_SIZE(expr), &found);
	if (!found)
		ctx->handle_error(ctx, "Unknown expression type", cur);

	return ret;
}

static int cmd_add_string(struct json_call *call, const char *pattern)
{
	struct json_script_ctx *ctx = call->ctx;
	char *dest, *next, *str;
	int len = 0;
	bool var = false;
	char c = '%';

	dest = blobmsg_alloc_string_buffer(&ctx->buf, NULL, 1);
	next = alloca(strlen(pattern) + 1);
	strcpy(next, pattern);

	for (str = next; str; str = next) {
		const char *cur;
		char *end;
		int cur_len = 0;
		bool cur_var = var;

		end = strchr(str, '%');
		if (end) {
			*end = 0;
			next = end + 1;
			var = !var;
		} else {
			end = str + strlen(str);
			next = NULL;
		}

		if (cur_var) {
			if (next > str) {
				cur = msg_find_var(call, str);
				if (!cur)
					continue;

				cur_len = strlen(cur);
			} else {
				cur = &c;
				cur_len = 1;
			}
		} else {
			if (str == end)
				continue;

			cur = str;
			cur_len = end - str;
		}

		dest = blobmsg_realloc_string_buffer(&ctx->buf, cur_len + 1);
		memcpy(dest + len, cur, cur_len);
		len += cur_len;
	}

	if (var)
		return -1;

	dest[len] = 0;
	blobmsg_add_string_buffer(&ctx->buf);
	return 0;
}

static int cmd_process_strings(struct json_call *call, struct blob_attr *attr)
{
	struct json_script_ctx *ctx = call->ctx;
	struct blob_attr *cur;
	int args = -1;
	int rem, ret;
	void *c;

	blob_buf_init(&ctx->buf, 0);
	c = blobmsg_open_array(&ctx->buf, NULL);
	blobmsg_for_each_attr(cur, attr, rem) {
		if (args++ < 0)
			continue;

		if (blobmsg_type(cur) != BLOBMSG_TYPE_STRING) {
			ctx->handle_error(ctx, "Invalid argument in command", attr);
			return -1;
		}

		ret = cmd_add_string(call, blobmsg_data(cur));
		if (ret) {
			ctx->handle_error(ctx, "Unterminated variable reference in string", attr);
			return ret;
		}
	}

	blobmsg_close_array(&ctx->buf, c);

	return 0;
}

static int __json_process_cmd(struct json_call *call, struct blob_attr *cur)
{
	struct json_script_ctx *ctx = call->ctx;
	const char *name;
	bool found;
	int ret;

	if (blobmsg_type(cur) != BLOBMSG_TYPE_ARRAY ||
	    blobmsg_type(blobmsg_data(cur)) != BLOBMSG_TYPE_STRING) {
		ctx->handle_error(ctx, "Unexpected element type", cur);
		return -1;
	}

	ret = __json_process_type(call, cur, cmd, ARRAY_SIZE(cmd), &found);
	if (found)
		return ret;

	name = blobmsg_data(blobmsg_data(cur));
	ret = cmd_process_strings(call, cur);
	if (ret)
		return ret;

	ctx->handle_command(ctx, name, blob_data(ctx->buf.head), call->vars);

	return 0;
}

static int json_process_cmd(struct json_call *call, struct blob_attr *block)
{
	struct json_script_ctx *ctx = call->ctx;
	struct blob_attr *cur;
	int rem;
	int ret;
	int i = 0;

	if (blobmsg_type(block) != BLOBMSG_TYPE_ARRAY) {
		ctx->handle_error(ctx, "Unexpected element type", block);
		return -1;
	}

	blobmsg_for_each_attr(cur, block, rem) {
		switch(blobmsg_type(cur)) {
		case BLOBMSG_TYPE_STRING:
			if (!i)
				return __json_process_cmd(call, block);
		default:
			ret = json_process_cmd(call, cur);
			if (ret < -1)
				return ret;
			break;
		}
		i++;
	}

	return 0;
}

void json_script_run(struct json_script_ctx *ctx, const char *name,
		     struct blob_attr *vars)
{
	struct json_script_file *file;
	static unsigned int _seq = 0;
	struct json_call call = {
		.ctx = ctx,
		.vars = vars,
		.seq = ++_seq,
	};

	/* overflow */
	if (!call.seq)
		call.seq = ++_seq;

	file = json_script_get_file(ctx, name);
	if (!file)
		return;

	__json_script_run(&call, file, NULL);
}

static void __json_script_file_free(struct json_script_ctx *ctx, struct json_script_file *f)
{
	struct json_script_file *next;

	for (next = f->next; f; f = next, next = f->next)
		free(f);
}

void
json_script_free(struct json_script_ctx *ctx)
{
	struct json_script_file *f, *next;

	avl_remove_all_elements(&ctx->files, f, avl, next)
		__json_script_file_free(ctx, f);

	blob_buf_free(&ctx->buf);
}

static void
__default_handle_error(struct json_script_ctx *ctx, const char *msg,
		       struct blob_attr *context)
{
}

static const char *
__default_handle_var(struct json_script_ctx *ctx, const char *name,
		     struct blob_attr *vars)
{
	return NULL;
}

static int
__default_handle_expr(struct json_script_ctx *ctx, const char *name,
		      struct blob_attr *expr, struct blob_attr *vars)
{
	return -1;
}

static struct json_script_file *
__default_handle_file(struct json_script_ctx *ctx, const char *name)
{
	return NULL;
}

void json_script_init(struct json_script_ctx *ctx)
{
	avl_init(&ctx->files, avl_strcmp, false, NULL);

	if (!ctx->handle_error)
		ctx->handle_error = __default_handle_error;

	if (!ctx->handle_var)
		ctx->handle_var = __default_handle_var;

	if (!ctx->handle_expr)
		ctx->handle_expr = __default_handle_expr;

	if (!ctx->handle_file)
		ctx->handle_file = __default_handle_file;
}
