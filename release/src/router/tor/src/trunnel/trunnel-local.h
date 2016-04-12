
#ifndef TRUNNEL_LOCAL_H_INCLUDED
#define TRUNNEL_LOCAL_H_INCLUDED

#include "util.h"
#include "compat.h"
#include "crypto.h"

#define trunnel_malloc tor_malloc
#define trunnel_calloc tor_calloc
#define trunnel_strdup tor_strdup
#define trunnel_free_ tor_free_
#define trunnel_realloc tor_realloc
#define trunnel_reallocarray tor_reallocarray
#define trunnel_assert tor_assert
#define trunnel_memwipe(mem, len) memwipe((mem), 0, (len))

#endif
