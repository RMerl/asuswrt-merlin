/**
 * @file library/tools.h
 * @defgroup util Memory Utility Routines
 * @ingroup library
 * @{
 */

#ifndef _TOOLS_H
#define _TOOLS_H

#ifdef HAVE_INTTYPES_H
#include <inttypes.h> /* uintptr_t */
#endif

#ifdef __cplusplus
extern          "C" {
#endif



    /*
     * General acros and constants.
     */
#ifdef WIN32
#  define SNMP_MAXPATH MAX_PATH
#else
#  ifdef PATH_MAX
#    define SNMP_MAXPATH PATH_MAX
#  else
#    ifdef MAXPATHLEN
#      define SNMP_MAXPATH MAXPATHLEN
#    else
#      define SNMP_MAXPATH 1024		/* Should be safe enough */
#    endif
#  endif
#endif

#define SNMP_MAXBUF		(1024 * 4)
#define SNMP_MAXBUF_MEDIUM	1024
#define SNMP_MAXBUF_SMALL	512

#define SNMP_MAXBUF_MESSAGE	1500

#define SNMP_MAXOID		64
#define SNMP_MAX_CMDLINE_OIDS	128

#define SNMP_FILEMODE_CLOSED	0600
#define SNMP_FILEMODE_OPEN	0644

#define BYTESIZE(bitsize)       ((bitsize + 7) >> 3)
#define ROUNDUP8(x)		( ( (x+7) >> 3 ) * 8 )

#define SNMP_STRORNULL(x)       ( x ? x : "(null)")

/** @def SNMP_FREE(s)
    Frees a pointer only if it is !NULL and sets its value to NULL */
#define SNMP_FREE(s)    do { if (s) { free((void *)s); s=NULL; } } while(0)

/** @def SNMP_SWIPE_MEM(n, s)
    Frees pointer n only if it is !NULL, sets n to s and sets s to NULL */
#define SNMP_SWIPE_MEM(n,s) do { if (n) free((void *)n); n = s; s=NULL; } while(0)

    /*
     * XXX Not optimal everywhere. 
     */
/** @def SNMP_MALLOC_STRUCT(s)
    Mallocs memory of sizeof(struct s), zeros it and returns a pointer to it. */
#define SNMP_MALLOC_STRUCT(s)   (struct s *) calloc(1, sizeof(struct s))

/** @def SNMP_MALLOC_TYPEDEF(t)
    Mallocs memory of sizeof(t), zeros it and returns a pointer to it. */
#define SNMP_MALLOC_TYPEDEF(td)  (td *) calloc(1, sizeof(td))

/** @def SNMP_ZERO(s,l)
    Zeros l bytes of memory starting at s. */
#define SNMP_ZERO(s,l)	do { if (s) memset(s, 0, l); } while(0)


/**
 * @def NETSNMP_REMOVE_CONST(t, e)
 *
 * Cast away constness without that gcc -Wcast-qual prints a compiler warning,
 * similar to const_cast<> in C++.
 *
 * @param[in] t A pointer type.
 * @param[in] e An expression of a type that can be assigned to the type (const t).
 */
#if defined(__GNUC__)
#define NETSNMP_REMOVE_CONST(t, e)                                      \
    (__extension__ ({ const t tmp = (e); (t)(uintptr_t)tmp; }))
#else
#define NETSNMP_REMOVE_CONST(t, e) ((t)(uintptr_t)(e))
#endif


#define TOUPPER(c)	(c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c)
#define TOLOWER(c)	(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c)

#define HEX2VAL(s) \
	((isalpha(s) ? (TOLOWER(s)-'a'+10) : (TOLOWER(s)-'0')) & 0xf)
#define VAL2HEX(s)	( (s) + (((s) >= 10) ? ('a'-10) : '0') )


/** @def SNMP_MAX(a, b)
    Computers the maximum of a and b. */
#define SNMP_MAX(a,b) ((a) > (b) ? (a) : (b))

/** @def SNMP_MIN(a, b)
    Computers the minimum of a and b. */
#define SNMP_MIN(a,b) ((a) > (b) ? (b) : (a))

/** @def SNMP_MACRO_VAL_TO_STR(s)
 *  Expands to string with value of the s. 
 *  If s is macro, the resulting string is value of the macro.
 *  Example: 
 *   \#define TEST 1234
 *   SNMP_MACRO_VAL_TO_STR(TEST) expands to "1234"
 *   SNMP_MACRO_VAL_TO_STR(TEST+1) expands to "1234+1"
 */
#define SNMP_MACRO_VAL_TO_STR(s) SNMP_MACRO_VAL_TO_STR_PRIV(s)  
#define SNMP_MACRO_VAL_TO_STR_PRIV(s) #s
	
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE  1
#endif

    /*
     * QUIT the FUNction:
     *      e       Error code variable
     *      l       Label to goto to cleanup and get out of the function.
     *
     * XXX  It would be nice if the label could be constructed by the
     *      preprocessor in context.  Limited to a single error return value.
     *      Temporary hack at best.
     */
#define QUITFUN(e, l)			\
	if ( (e) != SNMPERR_SUCCESS) {	\
		rval = SNMPERR_GENERR;	\
		goto l ;		\
	}

    /*
     * DIFFTIMEVAL
     *      Set <diff> to the difference between <now> (current) and <then> (past).
     *
     * ASSUMES that all inputs are (struct timeval)'s.
     * Cf. system.c:calculate_time_diff().
     */
#define DIFFTIMEVAL(now, then, diff) 			\
{							\
	now.tv_sec--;					\
	now.tv_usec += 1000000L;			\
	diff.tv_sec  = now.tv_sec  - then.tv_sec;	\
	diff.tv_usec = now.tv_usec - then.tv_usec;	\
	if (diff.tv_usec > 1000000L){			\
		diff.tv_usec -= 1000000L;		\
		diff.tv_sec++;				\
	}						\
}

/**
 * Compute res = a + b.
 *
 * @pre a and b must be normalized 'struct timeval' values.
 *
 * @note res may be the same variable as one of the operands. In other
 *   words, &a == &res || &b == &res may hold.
 */
#define NETSNMP_TIMERADD(a, b, res)                  \
{                                                    \
    (res)->tv_sec  = (a)->tv_sec  + (b)->tv_sec;     \
    (res)->tv_usec = (a)->tv_usec + (b)->tv_usec;    \
    if ((res)->tv_usec >= 1000000L) {                \
        (res)->tv_usec -= 1000000L;                  \
        (res)->tv_sec++;                             \
    }                                                \
}

/**
 * Compute res = a - b.
 *
 * @pre a and b must be normalized 'struct timeval' values.
 *
 * @note res may be the same variable as one of the operands. In other
 *   words, &a == &res || &b == &res may hold.
 */
#define NETSNMP_TIMERSUB(a, b, res)                             \
{                                                               \
    (res)->tv_sec  = (a)->tv_sec  - (b)->tv_sec - 1;            \
    (res)->tv_usec = (a)->tv_usec - (b)->tv_usec + 1000000L;    \
    if ((res)->tv_usec >= 1000000L) {                           \
        (res)->tv_usec -= 1000000L;                             \
        (res)->tv_sec++;                                        \
    }                                                           \
}


    /*
     * ISTRANSFORM
     * ASSUMES the minimum length for ttype and toid.
     */
#define USM_LENGTH_OID_TRANSFORM	10

#define ISTRANSFORM(ttype, toid)					\
	!snmp_oid_compare(ttype, USM_LENGTH_OID_TRANSFORM,		\
		usm ## toid ## Protocol, USM_LENGTH_OID_TRANSFORM)

#define ENGINETIME_MAX	2147483647      /* ((2^31)-1) */
#define ENGINEBOOT_MAX	2147483647      /* ((2^31)-1) */




    /*
     * Prototypes.
     */

    NETSNMP_IMPORT
    int             snmp_realloc(u_char ** buf, size_t * buf_len);

    void            free_zero(void *buf, size_t size);

    u_char         *malloc_random(size_t * size);
    u_char         *malloc_zero(size_t size);
    NETSNMP_IMPORT
    int             memdup(u_char ** to, const void * from, size_t size);

    void            netsnmp_check_definedness(const void *packet,
                                              size_t length);

    NETSNMP_IMPORT
    u_int           netsnmp_binary_to_hex(u_char ** dest, size_t *dest_len,
                                          int allow_realloc,
                                          const u_char * input, size_t len);

    NETSNMP_IMPORT
    u_int           binary_to_hex(const u_char * input, size_t len,
                                  char **output);
                    /* preferred */
    int             netsnmp_hex_to_binary(u_char ** buf, size_t * buf_len,
                                         size_t * offset, int allow_realloc,
                                         const char *hex, const char *delim);
                    /* calls netsnmp_hex_to_binary w/delim of " " */
    NETSNMP_IMPORT
    int             snmp_hex_to_binary(u_char ** buf, size_t * buf_len,
                                       size_t * offset, int allow_realloc,
                                       const char *hex);
                    /* handles odd lengths */
    NETSNMP_IMPORT
    int             hex_to_binary2(const u_char * input, size_t len,
                                   char **output);

    NETSNMP_IMPORT
    int             snmp_decimal_to_binary(u_char ** buf, size_t * buf_len,
                                           size_t * out_len,
                                           int allow_realloc,
                                           const char *decimal);
#define snmp_cstrcat(b,l,o,a,s) snmp_strcat(b,l,o,a,(const u_char *)s)
    NETSNMP_IMPORT
    int             snmp_strcat(u_char ** buf, size_t * buf_len,
                                size_t * out_len, int allow_realloc,
                                const u_char * s);
    NETSNMP_IMPORT
    char           *netsnmp_strdup_and_null(const u_char * from,
                                            size_t from_len);

    NETSNMP_IMPORT
    void            dump_chunk(const char *debugtoken, const char *title,
                               const u_char * buf, int size);
    char           *dump_snmpEngineID(const u_char * buf, size_t * buflen);

    /** A pointer to an opaque time marker value. */
    typedef void   *marker_t;
    typedef const void* const_marker_t;

    NETSNMP_IMPORT
    marker_t        atime_newMarker(void);
    NETSNMP_IMPORT
    void            atime_setMarker(marker_t pm);
    NETSNMP_IMPORT
    void            netsnmp_get_monotonic_clock(struct timeval* tv);
    NETSNMP_IMPORT
    void            netsnmp_set_monotonic_marker(marker_t *pm);
    NETSNMP_IMPORT
    long            atime_diff(const_marker_t first, const_marker_t second);
    u_long          uatime_diff(const_marker_t first, const_marker_t second);       /* 1/1000th sec */
    NETSNMP_IMPORT
    u_long          uatime_hdiff(const_marker_t first, const_marker_t second);      /* 1/100th sec */
    NETSNMP_IMPORT
    int             atime_ready(const_marker_t pm, int delta_ms);
    NETSNMP_IMPORT
    int             netsnmp_ready_monotonic(const_marker_t pm, int delta_ms);
    int             uatime_ready(const_marker_t pm, unsigned int delta_ms);

    int             marker_tticks(const_marker_t pm);
    int             timeval_tticks(const struct timeval *tv);
    NETSNMP_IMPORT
    char            *netsnmp_getenv(const char *name);
    NETSNMP_IMPORT
    int             netsnmp_setenv(const char *envname, const char *envval,
                                   int overwrite);

    int             netsnmp_addrstr_hton(char *ptr, size_t len);

    NETSNMP_IMPORT
    int             netsnmp_string_time_to_secs(const char *time_string);

#ifdef __cplusplus
}
#endif
#endif                          /* _TOOLS_H */
/* @} */
