/*
 * Copyright (C) 2011-2013 Felix Fietkau <nbd@openwrt.org>
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
#ifdef JSONC
        #include <json.h>
#else
        #include <json/json.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <getopt.h>
#include "list.h"

#define MAX_VARLEN	256

static const char *var_prefix = "";
static int var_prefix_len = 0;

static int add_json_element(const char *key, json_object *obj);

static int add_json_object(json_object *obj)
{
	int ret = 0;

	json_object_object_foreach(obj, key, val) {
		ret = add_json_element(key, val);
		if (ret)
			break;
	}
	return ret;
}

static int add_json_array(struct array_list *a)
{
	char seq[12];
	int i, len;
	int ret;

	for (i = 0, len = array_list_length(a); i < len; i++) {
		sprintf(seq, "%d", i);
		ret = add_json_element(seq, array_list_get_idx(a, i));
		if (ret)
			return ret;
	}

	return 0;
}

static void add_json_string(const char *str)
{
	char *ptr = (char *) str;
	int len;
	char *c;

	while ((c = strchr(ptr, '\'')) != NULL) {
		len = c - ptr;
		if (len > 0)
			fwrite(ptr, len, 1, stdout);
		ptr = c + 1;
		c = "'\\''";
		fwrite(c, strlen(c), 1, stdout);
	}
	len = strlen(ptr);
	if (len > 0)
		fwrite(ptr, len, 1, stdout);
}

static void write_key_string(const char *key)
{
	while (*key) {
		putc(isalnum(*key) ? *key : '_', stdout);
		key++;
	}
}

static int add_json_element(const char *key, json_object *obj)
{
	char *type;

	if (!obj)
		return -1;

	switch (json_object_get_type(obj)) {
	case json_type_object:
		type = "object";
		break;
	case json_type_array:
		type = "array";
		break;
	case json_type_string:
		type = "string";
		break;
	case json_type_boolean:
		type = "boolean";
		break;
	case json_type_int:
		type = "int";
		break;
	case json_type_double:
		type = "double";
		break;
	default:
		return -1;
	}

	fprintf(stdout, "json_add_%s '", type);
	write_key_string(key);

	switch (json_object_get_type(obj)) {
	case json_type_object:
		fprintf(stdout, "';\n");
		add_json_object(obj);
		fprintf(stdout, "json_close_object;\n");
		break;
	case json_type_array:
		fprintf(stdout, "';\n");
		add_json_array(json_object_get_array(obj));
		fprintf(stdout, "json_close_array;\n");
		break;
	case json_type_string:
		fprintf(stdout, "' '");
		add_json_string(json_object_get_string(obj));
		fprintf(stdout, "';\n");
		break;
	case json_type_boolean:
		fprintf(stdout, "' %d;\n", json_object_get_boolean(obj));
		break;
	case json_type_int:
		fprintf(stdout, "' %d;\n", json_object_get_int(obj));
		break;
	case json_type_double:
		fprintf(stdout, "' %lf;\n", json_object_get_double(obj));
		break;
	default:
		return -1;
	}

	return 0;
}

static int jshn_parse(const char *str)
{
	json_object *obj;

	obj = json_tokener_parse(str);
	if (is_error(obj) || json_object_get_type(obj) != json_type_object) {
		fprintf(stderr, "Failed to parse message data\n");
		return 1;
	}
	fprintf(stdout, "json_init;\n");
	add_json_object(obj);
	fflush(stdout);

	return 0;
}

static char *get_keys(const char *prefix)
{
	char *keys;

	keys = alloca(var_prefix_len + strlen(prefix) + sizeof("KEYS_") + 1);
	sprintf(keys, "%sKEYS_%s", var_prefix, prefix);
	return getenv(keys);
}

static void get_var(const char *prefix, const char **name, char **var, char **type)
{
	char *tmpname, *varname;

	tmpname = alloca(var_prefix_len + strlen(prefix) + 1 + strlen(*name) + 1 + sizeof("TYPE_"));

	sprintf(tmpname, "%s%s_%s", var_prefix, prefix, *name);
	*var = getenv(tmpname);

	sprintf(tmpname, "%sTYPE_%s_%s", var_prefix, prefix, *name);
	*type = getenv(tmpname);

	sprintf(tmpname, "%sNAME_%s_%s", var_prefix, prefix, *name);
	varname = getenv(tmpname);
	if (varname)
		*name = varname;
}

static json_object *jshn_add_objects(json_object *obj, const char *prefix, bool array);

static void jshn_add_object_var(json_object *obj, bool array, const char *prefix, const char *name)
{
	json_object *new;
	char *var, *type;

	get_var(prefix, &name, &var, &type);
	if (!var || !type)
		return;

	if (!strcmp(type, "array")) {
		new = json_object_new_array();
		jshn_add_objects(new, var, true);
	} else if (!strcmp(type, "object")) {
		new = json_object_new_object();
		jshn_add_objects(new, var, false);
	} else if (!strcmp(type, "string")) {
		new = json_object_new_string(var);
	} else if (!strcmp(type, "int")) {
		new = json_object_new_int(atoi(var));
	} else if (!strcmp(type, "double")) {
		new = json_object_new_double(strtod(var, NULL));
	} else if (!strcmp(type, "boolean")) {
		new = json_object_new_boolean(!!atoi(var));
	} else {
		return;
	}

	if (array)
		json_object_array_add(obj, new);
	else
		json_object_object_add(obj, name, new);
}

static json_object *jshn_add_objects(json_object *obj, const char *prefix, bool array)
{
	char *keys, *key, *brk;

	keys = get_keys(prefix);
	if (!keys || !obj)
		goto out;

	for (key = strtok_r(keys, " ", &brk); key;
	     key = strtok_r(NULL, " ", &brk)) {
		jshn_add_object_var(obj, array, prefix, key);
	}

out:
	return obj;
}

static int jshn_format(bool no_newline)
{
	json_object *obj;

	obj = json_object_new_object();
	jshn_add_objects(obj, "JSON_VAR", false);
	fprintf(stdout, "%s%s", json_object_to_json_string(obj),
		no_newline ? "" : "\n");
	json_object_put(obj);
	return 0;
}

static int usage(const char *progname)
{
	fprintf(stderr, "Usage: %s [-n] -r <message>|-w\n", progname);
	return 2;
}

int main(int argc, char **argv)
{
	bool no_newline = false;
	int ch;

	while ((ch = getopt(argc, argv, "p:nr:w")) != -1) {
		switch(ch) {
		case 'p':
			var_prefix = optarg;
			var_prefix_len = strlen(var_prefix);
			break;
		case 'r':
			return jshn_parse(optarg);
		case 'w':
			return jshn_format(no_newline);
		case 'n':
			no_newline = true;
			break;
		default:
			return usage(argv[0]);
		}
	}
	return usage(argv[0]);
}
