#ifndef _LIGHTTPD_SETTINGS_H_
#define _LIGHTTPD_SETTINGS_H_

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#ifndef __USE_GNU
# define __USE_GNU /* a hack in my eyes, <fcntl.h> F_SETSIG should work with _GNU_SOURCE */
#endif

#ifdef __GNUC__
# define LI_NORETURN __attribute__((noreturn))
#else
# define LI_NORETURN
#endif

#define UNUSED(x) ( (void)(x) )

#define BV(x) (1 << x)

#define INET_NTOP_CACHE_MAX 4
#define FILE_CACHE_MAX      16

/**
 * max size of a buffer which will just be reset
 * to ->used = 0 instead of really freeing the buffer
 *
 * 64kB (no real reason, just a guess)
 */
#define BUFFER_MAX_REUSE_SIZE  (4 * 1024)

/* both should be way smaller than SSIZE_MAX :) */
#define MAX_READ_LIMIT (256*1024)
#define MAX_WRITE_LIMIT (256*1024)

/**
 * max size of the HTTP request header
 *
 * 32k should be enough for everything (just a guess)
 *
 */
#define MAX_HTTP_REQUEST_HEADER  (32 * 1024)

typedef enum { HANDLER_UNSET,
		HANDLER_GO_ON,
		HANDLER_FINISHED,
		HANDLER_COMEBACK,
		HANDLER_WAIT_FOR_EVENT,
		HANDLER_ERROR,
		HANDLER_WAIT_FOR_FD
} handler_t;

#define HTTP_LINGER_TIMEOUT 5

/* we use it in a enum */
#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#endif
