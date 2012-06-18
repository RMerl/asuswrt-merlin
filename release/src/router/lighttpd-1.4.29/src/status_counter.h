#ifndef _STATUS_COUNTER_H_
#define _STATUS_COUNTER_H_

#include "array.h"
#include "base.h"

#include <sys/types.h>

data_integer *status_counter_get_counter(server *srv, const char *s, size_t len);
int status_counter_inc(server *srv, const char *s, size_t len);
int status_counter_dec(server *srv, const char *s, size_t len);
int status_counter_set(server *srv, const char *s, size_t len, int val);

#endif
