#include "array.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static data_unset *data_count_copy(const data_unset *s) {
	data_count *src = (data_count *)s;
	data_count *ds = data_count_init();

	buffer_copy_string_buffer(ds->key, src->key);
	ds->count = src->count;
	ds->is_index_key = src->is_index_key;
	return (data_unset *)ds;
}

static void data_count_free(data_unset *d) {
	data_count *ds = (data_count *)d;

	buffer_free(ds->key);

	free(d);
}

static void data_count_reset(data_unset *d) {
	data_count *ds = (data_count *)d;

	buffer_reset(ds->key);

	ds->count = 0;
}

static int data_count_insert_dup(data_unset *dst, data_unset *src) {
	data_count *ds_dst = (data_count *)dst;
	data_count *ds_src = (data_count *)src;

	ds_dst->count += ds_src->count;

	src->free(src);

	return 0;
}

static void data_count_print(const data_unset *d, int depth) {
	data_count *ds = (data_count *)d;
	UNUSED(depth);

	fprintf(stdout, "count(%d)", ds->count);
}


data_count *data_count_init(void) {
	data_count *ds;

	ds = calloc(1, sizeof(*ds));

	ds->key = buffer_init();
	ds->count = 1;

	ds->copy = data_count_copy;
	ds->free = data_count_free;
	ds->reset = data_count_reset;
	ds->insert_dup = data_count_insert_dup;
	ds->print = data_count_print;
	ds->type = TYPE_COUNT;

	return ds;
}
