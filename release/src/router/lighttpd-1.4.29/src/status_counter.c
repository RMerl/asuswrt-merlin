#include "status_counter.h"

#include <stdlib.h>

/**
 * The status array can carry all the status information you want
 * the key to the array is <module-prefix>.<name>
 * and the values are counters
 *
 * example:
 *   fastcgi.backends        = 10
 *   fastcgi.active-backends = 6
 *   fastcgi.backend.<key>.load = 24
 *   fastcgi.backend.<key>....
 *
 *   fastcgi.backend.<key>.disconnects = ...
 */

data_integer *status_counter_get_counter(server *srv, const char *s, size_t len) {
	data_integer *di;

	if (NULL == (di = (data_integer *)array_get_element(srv->status, s))) {
		/* not found, create it */

		if (NULL == (di = (data_integer *)array_get_unused_element(srv->status, TYPE_INTEGER))) {
			di = data_integer_init();
		}
		buffer_copy_string_len(di->key, s, len);
		di->value = 0;

		array_insert_unique(srv->status, (data_unset *)di);
	}
	return di;
}

/* dummies of the statistic framework functions
 * they will be moved to a statistics.c later */
int status_counter_inc(server *srv, const char *s, size_t len) {
	data_integer *di = status_counter_get_counter(srv, s, len);

	di->value++;

	return 0;
}

int status_counter_dec(server *srv, const char *s, size_t len) {
	data_integer *di = status_counter_get_counter(srv, s, len);

	if (di->value > 0) di->value--;

	return 0;
}

int status_counter_set(server *srv, const char *s, size_t len, int val) {
	data_integer *di = status_counter_get_counter(srv, s, len);

	di->value = val;

	return 0;
}

