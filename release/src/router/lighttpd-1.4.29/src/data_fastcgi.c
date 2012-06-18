#include "array.h"
#include "fastcgi.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static data_unset *data_fastcgi_copy(const data_unset *s) {
	data_fastcgi *src = (data_fastcgi *)s;
	data_fastcgi *ds = data_fastcgi_init();

	buffer_copy_string_buffer(ds->key, src->key);
	buffer_copy_string_buffer(ds->host, src->host);
	ds->is_index_key = src->is_index_key;
	return (data_unset *)ds;
}

static void data_fastcgi_free(data_unset *d) {
	data_fastcgi *ds = (data_fastcgi *)d;

	buffer_free(ds->key);
	buffer_free(ds->host);

	free(d);
}

static void data_fastcgi_reset(data_unset *d) {
	data_fastcgi *ds = (data_fastcgi *)d;

	buffer_reset(ds->key);
	buffer_reset(ds->host);

}

static int data_fastcgi_insert_dup(data_unset *dst, data_unset *src) {
	UNUSED(dst);

	src->free(src);

	return 0;
}

static void data_fastcgi_print(const data_unset *d, int depth) {
	data_fastcgi *ds = (data_fastcgi *)d;
	UNUSED(depth);

	fprintf(stdout, "fastcgi(%s)", ds->host->ptr);
}


data_fastcgi *data_fastcgi_init(void) {
	data_fastcgi *ds;

	ds = calloc(1, sizeof(*ds));

	ds->key = buffer_init();
	ds->host = buffer_init();
	ds->port = 0;
	ds->is_disabled = 0;

	ds->copy = data_fastcgi_copy;
	ds->free = data_fastcgi_free;
	ds->reset = data_fastcgi_reset;
	ds->insert_dup = data_fastcgi_insert_dup;
	ds->print = data_fastcgi_print;
	ds->type = TYPE_FASTCGI;

	return ds;
}
