#ifndef _ATALK_UNICODE_H
#define _ATALK_UNICODE_H 1

#include <errno.h>
#include <stdint.h>
#include <sys/param.h>

#define ucs2_t uint16_t

#ifndef MIN
#define MIN(a,b)     ((a)<(b)?(a):(b))
#endif /* ! MIN */

#ifndef MAX
#define MAX(a,b)     ((a)>(b)?(a):(b))
#endif /* ! MIN */

#define SAFE_FREE(x) do { if ((x) != NULL) {free(x); x=NULL;} } while(0)

#ifndef EILSEQ
#define EILSEQ       84      /* Illegal byte sequence.  */
#endif

/* generic iconv conversion structure */
typedef struct {
    size_t (*direct)(void *cd, char **inbuf, size_t *inbytesleft,
                     char **outbuf, size_t *outbytesleft);
    size_t (*pull)(void *cd, char **inbuf, size_t *inbytesleft,
                   char **outbuf, size_t *outbytesleft);
    size_t (*push)(void *cd, char **inbuf, size_t *inbytesleft,
                   char **outbuf, size_t *outbytesleft);
    void *cd_direct, *cd_pull, *cd_push;
    char *from_name, *to_name;
} *atalk_iconv_t;

#define CHARSET_CLIENT      1
#define CHARSET_VOLUME      2
#define CHARSET_PRECOMPOSED 4
#define CHARSET_DECOMPOSED  8
#define CHARSET_MULTIBYTE   16
#define CHARSET_WIDECHAR    32
#define CHARSET_ICONV       64

#define IGNORE_CHAR '_'

/* conversion flags */
#define CONV_IGNORE         (1<<0) /* return the first convertable characters. */
#define CONV_ESCAPEHEX      (1<<1) /* escape unconvertable chars with :[UCS2HEX], */
                                   /* also escape '/'. Escape ':' if also CONV_ALLOW_COLON, */
                                   /* else ':' raises EILSEQ */
#define CONV_ESCAPEDOTS     (1<<2) /* escape leading dots with :2600 */
#define CONV_UNESCAPEHEX    (1<<3)
#define CONV_TOUPPER        (1<<4) /* convert to UPPERcase */
#define CONV_TOLOWER        (1<<5) /* convert to lowercase */
#define CONV_PRECOMPOSE     (1<<6) /* precompose */
#define CONV_DECOMPOSE      (1<<7) /* precompose */
#define CONV_FORCE          (1<<8) /* force convertion */
#define CONV__EILSEQ        (1<<9) /* ignore EILSEQ, replace with IGNORE_CHAR (try USC2) */

/* conversion return flags */
#define CONV_REQMANGLE  (1<<14) /* mangling of returned name is required */
#define CONV_REQESCAPE  (1<<15) /* espace unconvertable chars with :[UCS2HEX] */

/* this defines the charset types used in samba */
typedef enum {CH_UCS2=0, CH_UTF8=1, CH_MAC=2, CH_UNIX=3, CH_UTF8_MAC=4} charset_t;

#define NUM_CHARSETS 5

/*
 *   for each charset we have a function that pulls from that charset to
 *     a ucs2 buffer, and a function that pushes to a ucs2 buffer
 */

struct charset_functions {
    const char *name;
    const long kTextEncoding;
    size_t (*pull)(void *, char **inbuf, size_t *inbytesleft,
                   char **outbuf, size_t *outbytesleft);
    size_t (*push)(void *, char **inbuf, size_t *inbytesleft,
                   char **outbuf, size_t *outbytesleft);
    uint32_t flags;
    const char *iname;
    struct charset_functions *prev, *next;
};

/* from iconv.c */
extern atalk_iconv_t atalk_iconv_open (const char *, const char *);
extern size_t atalk_iconv (atalk_iconv_t, const char **, size_t *, char **, size_t *);
extern int atalk_iconv_close (atalk_iconv_t);
extern struct charset_functions *find_charset_functions (const char *);
extern int atalk_register_charset (struct charset_functions *);

/* from utf16_case.c */
extern ucs2_t    toupper_w  (ucs2_t);
extern uint32_t  toupper_sp (uint32_t);
extern ucs2_t    tolower_w  (ucs2_t);
extern uint32_t  tolower_sp (uint32_t);

/* from util_unistr.c */
extern int      strupper_w (ucs2_t *);
extern int      strlower_w (ucs2_t *);
extern int      islower_w  (ucs2_t);
extern int      isupper_w  (ucs2_t);
extern size_t   strlen_w   (const ucs2_t *);
extern size_t   strnlen_w  (const ucs2_t *, size_t);
extern ucs2_t*  strchr_w   (const ucs2_t *, ucs2_t);
extern ucs2_t   *strcasechr_w (const ucs2_t *s, ucs2_t c);
extern int      strcmp_w   (const ucs2_t *, const ucs2_t *);
extern int      strncmp_w  (const ucs2_t *, const ucs2_t *, size_t);
extern int      strcasecmp_w  (const ucs2_t *, const ucs2_t *);
extern int      strncasecmp_w (const ucs2_t *, const ucs2_t *, size_t);
extern ucs2_t   *strstr_w (const ucs2_t *s, const ucs2_t *ins);
extern ucs2_t   *strcasestr_w (const ucs2_t *, const ucs2_t *);
extern ucs2_t   *strndup_w (const ucs2_t *, size_t);
extern ucs2_t   *strdup_w  (const ucs2_t *);
extern ucs2_t   *strncpy_w (ucs2_t *, const ucs2_t *, const size_t);
extern ucs2_t   *strncat_w (ucs2_t *, const ucs2_t *, const size_t);
extern ucs2_t   *strcat_w  (ucs2_t *, const ucs2_t *);
extern size_t   precompose_w (ucs2_t *, size_t, ucs2_t *,size_t *);
extern size_t   decompose_w  (ucs2_t *, size_t, ucs2_t *,size_t *);
extern size_t   utf8_charlen ( char* );
extern size_t   utf8_strlen_validate ( char *);

/* from charcnv.c */
extern int      set_charset_name(charset_t, const char *);
extern void     free_charset_names(void);
extern void     init_iconv (void);
extern size_t   convert_string (charset_t, charset_t, void const *, size_t, void *, size_t);
extern size_t   convert_string_allocate (charset_t, charset_t, void const *, size_t, char **);
extern size_t   utf8_strupper (const char *, size_t, char *, size_t);
extern size_t   utf8_strlower (const char *, size_t, char *, size_t);
extern size_t   unix_strupper (const char *, size_t, char *, size_t);
extern size_t   unix_strlower (const char *, size_t, char *, size_t);
extern size_t   charset_strupper (charset_t, const char *, size_t, char *, size_t);
extern size_t   charset_strlower (charset_t, const char *, size_t, char *, size_t);

extern size_t   charset_to_ucs2_allocate (charset_t, ucs2_t **dest, const char *src);
extern size_t   charset_to_utf8_allocate (charset_t, char **dest, const char *src);
extern size_t   ucs2_to_charset_allocate (charset_t, char **dest, const ucs2_t *src);
extern size_t   utf8_to_charset_allocate (charset_t, char **dest, const char *src);
extern size_t   ucs2_to_charset (charset_t, const ucs2_t *src, char *dest, size_t);

extern size_t   convert_charset (charset_t, charset_t, charset_t, const char *, size_t, char *, size_t, uint16_t *);

extern size_t   charset_precompose ( charset_t, char *, size_t, char *, size_t);
extern size_t   charset_decompose  ( charset_t, char *, size_t, char *, size_t);
extern size_t   utf8_precompose ( char *, size_t, char *, size_t);
extern size_t   utf8_decompose  ( char *, size_t, char *, size_t);

extern charset_t add_charset (const char* name);

#endif  /* _ATALK_UNICODE_H */
