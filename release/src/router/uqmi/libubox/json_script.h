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
#ifndef __JSON_SCRIPT_H
#define __JSON_SCRIPT_H

#include "avl.h"
#include "blob.h"
#include "blobmsg.h"
#include "utils.h"

struct json_script_file;

struct json_script_ctx {
	struct avl_tree files;
	struct blob_buf buf;

	uint32_t run_seq;

	/*
	 * handle_command: handle a command that was not recognized by the
	 * json_script core (required)
	 *
	 * @cmd: blobmsg container of the processed command
	 * @vars: blobmsg container of current run variables
	 */
	void (*handle_command)(struct json_script_ctx *ctx, const char *name,
			       struct blob_attr *cmd, struct blob_attr *vars);

	/*
	 * handle_expr: handle an expression that was not recognized by the
	 * json_script core (optional)
	 *
	 * @expr: blobmsg container of the processed expression
	 * @vars: blobmsg container of current runtime variables
	 */
	int (*handle_expr)(struct json_script_ctx *ctx, const char *name,
			   struct blob_attr *expr, struct blob_attr *vars);

	/*
	 * handle_var - look up a variable that's not part of the runtime
	 * variable set (optional)
	 */
	const char *(*handle_var)(struct json_script_ctx *ctx, const char *name,
				  struct blob_attr *vars);

	/*
	 * handle_file - load a file by filename (optional)
	 *
	 * in case of wildcards, it can return a chain of json_script files
	 * linked via the ::next pointer. Only the first json_script file is
	 * added to the tree.
	 */
	struct json_script_file *(*handle_file)(struct json_script_ctx *ctx,
						const char *name);

	/*
	 * handle_error - handle a processing error in a command or expression
	 * (optional)
	 * 
	 * @msg: error message
	 * @context: source file context of the error (blobmsg container)
	 */
	void (*handle_error)(struct json_script_ctx *ctx, const char *msg,
			     struct blob_attr *context);
};

struct json_script_file {
	struct avl_node avl;
	struct json_script_file *next;

	unsigned int seq;
	struct blob_attr data[];
};

void json_script_init(struct json_script_ctx *ctx);
void json_script_free(struct json_script_ctx *ctx);

/*
 * json_script_run - run a json script with a set of runtime variables
 *
 * @filename: initial filename to run
 * @vars: blobmsg container of the current runtime variables
 */
void json_script_run(struct json_script_ctx *ctx, const char *filename,
		     struct blob_attr *vars);

struct json_script_file *
json_script_file_from_blobmsg(const char *name, void *data, int len);

/*
 * json_script_find_var - helper function to find a runtime variable from
 * the list passed by json_script user.
 * It is intended to be used by the .handle_var callback
 */
const char *json_script_find_var(struct json_script_ctx *ctx, struct blob_attr *vars,
				 const char *name);

#endif
