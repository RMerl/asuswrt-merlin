#include "array.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static data_unset *data_string_copy(const data_unset *s) {
	data_string *src = (data_string *)s;
	data_string *ds = data_string_init();

	buffer_copy_string_buffer(ds->key, src->key);
	buffer_copy_string_buffer(ds->value, src->value);
	ds->is_index_key = src->is_index_key;
	return (data_unset *)ds;
}

static void data_string_free(data_unset *d) {
	data_string *ds = (data_string *)d;

	buffer_free(ds->key);
	buffer_free(ds->value);

	free(d);
}

static void data_string_reset(data_unset *d) {
	data_string *ds = (data_string *)d;

	/* reused array elements */
	buffer_reset(ds->key);
	buffer_reset(ds->value);
}

static int data_string_insert_dup(data_unset *dst, data_unset *src) {
	data_string *ds_dst = (data_string *)dst;
	data_string *ds_src = (data_string *)src;

	if (ds_dst->value->used) {
		buffer_append_string_len(ds_dst->value, CONST_STR_LEN(", "));
		buffer_append_string_buffer(ds_dst->value, ds_src->value);
	} else {
		buffer_copy_string_buffer(ds_dst->value, ds_src->value);
	}

	src->free(src);

	return 0;
}

static int data_response_insert_dup(data_unset *dst, data_unset *src) {
	data_string *ds_dst = (data_string *)dst;
	data_string *ds_src = (data_string *)src;

	if (ds_dst->value->used) {
		buffer_append_string_len(ds_dst->value, CONST_STR_LEN("\r\n"));
		buffer_append_string_buffer(ds_dst->value, ds_dst->key);
		buffer_append_string_len(ds_dst->value, CONST_STR_LEN(": "));
		buffer_append_string_buffer(ds_dst->value, ds_src->value);
	} else {
		buffer_copy_string_buffer(ds_dst->value, ds_src->value);
	}

	src->free(src);

	return 0;
}


static void data_string_print(const data_unset *d, int depth) {
	data_string *ds = (data_string *)d;
	unsigned int i;
	UNUSED(depth);

	/* empty and uninitialized strings */
	if (ds->value->used < 1) {
		fputs("\"\"", stdout);
		return;
	}

	/* print out the string as is, except prepend " with backslash */
	putc('"', stdout);
	for (i = 0; i < ds->value->used - 1; i++) {
		unsigned char c = ds->value->ptr[i];
		if (c == '"') {
			fputs("\\\"", stdout);
		} else {
			putc(c, stdout);
		}
	}
	putc('"', stdout);
}


data_string *data_string_init(void) {
	data_string *ds;

	ds = calloc(1, sizeof(*ds));
	assert(ds);

	ds->key = buffer_init();
	ds->value = buffer_init();

	ds->copy = data_string_copy;
	ds->free = data_string_free;
	ds->reset = data_string_reset;
	ds->insert_dup = data_string_insert_dup;
	ds->print = data_string_print;
	ds->type = TYPE_STRING;

	return ds;
}

data_string *data_response_init(void) {
	data_string *ds;

	ds = data_string_init();
	ds->insert_dup = data_response_insert_dup;

	return ds;
}
