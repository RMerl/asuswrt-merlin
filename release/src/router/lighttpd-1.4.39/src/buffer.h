#ifndef _BUFFER_H_
#define _BUFFER_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "settings.h"

#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <time.h>

#if defined HAVE_STDINT_H
# include <stdint.h>
#elif defined HAVE_INTTYPES_H
# include <inttypes.h>
#endif

/* generic string + binary data container; contains a terminating 0 in both
 * cases
 *
 * used == 0 indicates a special "empty" state (unset config values); ptr
 * might be NULL too then. otherwise an empty string has used == 1 (and ptr[0]
 * == 0);
 *
 * copy/append functions will ensure used >= 1 (i.e. never leave it in the
 * special empty state); only buffer_copy_buffer will copy the special empty
 * state.
 */
typedef struct {
	char *ptr;

	/* "used" includes a terminating 0 */
	size_t used;
	/* size of allocated buffer at *ptr */
	size_t size;
} buffer;

/* create new buffer; either empty or copy given data */
buffer* buffer_init(void);
buffer* buffer_init_buffer(const buffer *src); /* src can  be NULL */
buffer* buffer_init_string(const char *str); /* str can  be NULL */

void buffer_free(buffer *b); /* b can be NULL */
/* truncates to used == 0; frees large buffers, might keep smaller ones for reuse */
void buffer_reset(buffer *b); /* b can be NULL */

/* reset b. if NULL != b && NULL != src, move src content to b. reset src. */
void buffer_move(buffer *b, buffer *src);

/* make sure buffer is large enough to store a string of given size
 * and a terminating zero.
 * sets b to an empty string, and may drop old content.
 * @return b->ptr
 */
char* buffer_string_prepare_copy(buffer *b, size_t size);

/* allocate buffer large enough to be able to append a string of given size
 * if b was empty (used == 0) it will contain an empty string (used == 1)
 * afterwards
 * "used" data is preserved; if not empty buffer must contain a
 * zero terminated string.
 */
char* buffer_string_prepare_append(buffer *b, size_t size);

/* use after prepare_(copy,append) when you have written data to the buffer
 * to increase the buffer length by size. also sets the terminating zero.
 * requires enough space is present for the terminating zero (prepare with the
 * same size to be sure).
 */
void buffer_commit(buffer *b, size_t size);

/* sets string length:
 * - always stores a terminating zero to terminate the "new" string
 * - does not modify the string data apart from terminating zero
 * - reallocates the buffer iff needed
 */
void buffer_string_set_length(buffer *b, size_t len);

void buffer_copy_string(buffer *b, const char *s);
void buffer_copy_string_len(buffer *b, const char *s, size_t s_len);
void buffer_copy_buffer(buffer *b, const buffer *src);
/* convert input to hex and store in buffer */
void buffer_copy_string_hex(buffer *b, const char *in, size_t in_len);

void buffer_append_string(buffer *b, const char *s);
void buffer_append_string_len(buffer *b, const char *s, size_t s_len);
void buffer_append_string_buffer(buffer *b, const buffer *src);

void buffer_append_uint_hex(buffer *b, uintmax_t len);
void buffer_append_int(buffer *b, intmax_t val);
void buffer_copy_int(buffer *b, intmax_t val);

void buffer_append_strftime(buffer *b, const char *format, const struct tm *tm);

/* '-', log_10 (2^bits) = bits * log 2 / log 10 < bits * 0.31, terminating 0 */
#define LI_ITOSTRING_LENGTH (2 + (8 * sizeof(intmax_t) * 31 + 99) / 100)

void li_itostrn(char *buf, size_t buf_len, intmax_t val);
void li_itostr(char *buf, intmax_t val); /* buf must have at least LI_ITOSTRING_LENGTH bytes */
void li_utostrn(char *buf, size_t buf_len, uintmax_t val);
void li_utostr(char *buf, uintmax_t val); /* buf must have at least LI_ITOSTRING_LENGTH bytes */

/* buf must be (at least) 2*s_len + 1 big. uses lower-case hex letters. */
void li_tohex(char *buf, const char *s, size_t s_len);

char * buffer_search_string_len(buffer *b, const char *needle, size_t len);

/* NULL buffer or empty buffer (used == 0);
 * unset "string" (buffer) config options are initialized to used == 0,
 * while setting an empty string leads to used == 1
 */
int buffer_is_empty(const buffer *b);
/* NULL buffer, empty buffer (used == 0) or empty string (used == 1) */
int buffer_string_is_empty(const buffer *b);

int buffer_is_equal(const buffer *a, const buffer *b);
int buffer_is_equal_right_len(const buffer *a, const buffer *b, size_t len);
int buffer_is_equal_string(const buffer *a, const char *s, size_t b_len);
int buffer_is_equal_caseless_string(const buffer *a, const char *s, size_t b_len);
int buffer_caseless_compare(const char *a, size_t a_len, const char *b, size_t b_len);

typedef enum {
	ENCODING_REL_URI, /* for coding a rel-uri (/with space/and%percent) nicely as part of a href */
	ENCODING_REL_URI_PART, /* same as ENC_REL_URL plus coding / too as %2F */
	ENCODING_HTML,         /* & becomes &amp; and so on */
	ENCODING_MINIMAL_XML,  /* minimal encoding for xml */
	ENCODING_HEX,          /* encode string as hex */
	ENCODING_HTTP_HEADER   /* encode \n with \t\n */
} buffer_encoding_t;

void buffer_append_string_encoded(buffer *b, const char *s, size_t s_len, buffer_encoding_t encoding);

/* escape non-printable characters; simple escapes for \t, \r, \n; fallback to \xCC */
void buffer_append_string_c_escaped(buffer *b, const char *s, size_t s_len);

/* to upper case, replace non alpha-numerics with '_'; if is_http_header prefix with "HTTP_" unless s is "content-type" */
void buffer_copy_string_encoded_cgi_varnames(buffer *b, const char *s, size_t s_len, int is_http_header);

void buffer_urldecode_path(buffer *url);
void buffer_urldecode_query(buffer *url);
void buffer_path_simplify(buffer *dest, buffer *src);

void buffer_to_lower(buffer *b);
void buffer_to_upper(buffer *b);


/** deprecated */
char hex2int(unsigned char c);
char int2hex(char i);

int light_isdigit(int c);
int light_isxdigit(int c);
int light_isalpha(int c);
int light_isalnum(int c);

static inline size_t buffer_string_length(const buffer *b); /* buffer string length without terminating 0 */
static inline size_t buffer_string_space(const buffer *b); /* maximum length of string that can be stored without reallocating */
static inline void buffer_append_slash(buffer *b); /* append '/' no non-empty strings not ending in '/' */

#define BUFFER_APPEND_STRING_CONST(x, y) \
	buffer_append_string_len(x, y, sizeof(y) - 1)

#define BUFFER_COPY_STRING_CONST(x, y) \
	buffer_copy_string_len(x, y, sizeof(y) - 1)

#define CONST_STR_LEN(x) x, (x) ? sizeof(x) - 1 : 0
#define CONST_BUF_LEN(x) ((x) ? (x)->ptr : NULL), buffer_string_length(x)


void print_backtrace(FILE *file);
void log_failed_assert(const char *filename, unsigned int line, const char *msg) LI_NORETURN;
#define force_assert(x) do { if (!(x)) log_failed_assert(__FILE__, __LINE__, "assertion failed: " #x); } while(0)
#define SEGFAULT() log_failed_assert(__FILE__, __LINE__, "aborted");

/* inline implementations */

static inline size_t buffer_string_length(const buffer *b) {
	return NULL != b && 0 != b->used ? b->used - 1 : 0;
}

static inline size_t buffer_string_space(const buffer *b) {
	if (NULL == b || b->size == 0) return 0;
	if (0 == b->used) return b->size - 1;
	return b->size - b->used;
}

static inline void buffer_append_slash(buffer *b) {
	size_t len = buffer_string_length(b);
	if (len > 0 && '/' != b->ptr[len-1]) BUFFER_APPEND_STRING_CONST(b, "/");
}

#endif
