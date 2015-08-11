/*
 * simple utility functions
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#include <stdarg.h>
#include <sys/types.h>

typedef enum {
    l_err,
    l_err_list,
    l_conf,
    l_plugin,
    l_memdebug,
    l_upnp,
    l_last
} ff_lock_t;

/* debugging lock wrappers */

extern void util_mutex_lock(ff_lock_t which);
extern void util_mutex_unlock(ff_lock_t which);

/* simple hashing functions */
extern uint32_t util_djb_hash_block(unsigned char *data, uint32_t len);
extern uint32_t util_djb_hash_str(char *str);

extern int util_must_exit(void);

/* dumb string functions */
int util_split(char *s, char *delimiters, char ***argvp);
void util_dispose_split(char **argv);

extern unsigned char *util_utf8toutf16_alloc(unsigned char *utf8);
extern unsigned char *util_utf16touft8_alloc(unsigned char *utf16, int slen);
extern int util_utf8toutf16(unsigned char *utf16, int dlen, unsigned char *utf8, int slen);
extern int util_utf16toutf8(unsigned char *utf8, int dlen, unsigned char *utf16, int slen);
extern int util_xtoy(unsigned char *dbuffer, int dlen, unsigned char *sbuffer, int slen, char *from, char *to);
extern unsigned char *util_xtoutf8_alloc(unsigned char *x,int slen,char *from);
extern int util_utf16_byte_len(unsigned char *utf16);

extern void util_hexdump(unsigned char *block, int len);
extern char *util_vasprintf(char *fmt, va_list ap);
extern char *util_asprintf(char *fmt, ...);

#endif /* _UTIL_H_ */

