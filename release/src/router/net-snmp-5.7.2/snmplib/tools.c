/*
 * tools.c
 */

#define NETSNMP_TOOLS_C 1 /* dont re-define malloc wrappers here */

#ifdef HAVE_CRTDBG_H
/*
 * Define _CRTDBG_MAP_ALLOC such that in debug builds (when _DEBUG has been
 * defined) e.g. malloc() is rerouted to _malloc_dbg().
 */
#define _CRTDBG_MAP_ALLOC 1
#include <crtdbg.h>
#endif

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif
#ifdef cygwin
#include <windows.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/output_api.h>
#include <net-snmp/utilities.h>
#include <net-snmp/library/tools.h>     /* for "internal" definitions */

#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/mib.h>
#include <net-snmp/library/scapi.h>

netsnmp_feature_child_of(tools_all, libnetsnmp)

netsnmp_feature_child_of(memory_wrappers, tools_all)
netsnmp_feature_child_of(valgrind, tools_all)
netsnmp_feature_child_of(string_time_to_secs, tools_all)
netsnmp_feature_child_of(netsnmp_check_definedness, valgrind)

netsnmp_feature_child_of(uatime_ready, netsnmp_unused)
netsnmp_feature_child_of(timeval_tticks, netsnmp_unused)

netsnmp_feature_child_of(memory_strdup, memory_wrappers)
netsnmp_feature_child_of(memory_calloc, memory_wrappers)
netsnmp_feature_child_of(memory_malloc, memory_wrappers)
netsnmp_feature_child_of(memory_realloc, memory_wrappers)
netsnmp_feature_child_of(memory_free, memory_wrappers)

#ifndef NETSNMP_FEATURE_REMOVE_MEMORY_WRAPPERS
/**
 * This function is a wrapper for the strdup function.
 *
 * @note The strdup() implementation calls _malloc_dbg() when linking with
 * MSVCRT??D.dll and malloc() when linking with MSVCRT??.dll
 */
char * netsnmp_strdup( const char * ptr)
{
    return strdup(ptr);
}
#endif /* NETSNMP_FEATURE_REMOVE_MEMORY_STRDUP */
#ifndef NETSNMP_FEATURE_REMOVE_MEMORY_CALLOC
/**
 * This function is a wrapper for the calloc function.
 */
void * netsnmp_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}
#endif /* NETSNMP_FEATURE_REMOVE_MEMORY_CALLOC */
#ifndef NETSNMP_FEATURE_REMOVE_MEMORY_MALLOC
/**
 * This function is a wrapper for the malloc function.
 */
void * netsnmp_malloc(size_t size)
{
    return malloc(size);
}
#endif /* NETSNMP_FEATURE_REMOVE_MEMORY_MALLOC */
#ifndef NETSNMP_FEATURE_REMOVE_MEMORY_REALLOC
/**
 * This function is a wrapper for the realloc function.
 */
void * netsnmp_realloc( void * ptr, size_t size)
{
    return realloc(ptr, size);
}
#endif /* NETSNMP_FEATURE_REMOVE_MEMORY_REALLOC */
#ifndef NETSNMP_FEATURE_REMOVE_MEMORY_FREE
/**
 * This function is a wrapper for the free function.
 * It calls free only if the calling parameter has a non-zero value.
 */
void netsnmp_free( void * ptr)
{
    if (ptr)
        free(ptr);
}
#endif /* NETSNMP_FEATURE_REMOVE_MEMORY_FREE */

/**
 * This function increase the size of the buffer pointed at by *buf, which is
 * initially of size *buf_len.  Contents are preserved **AT THE BOTTOM END OF
 * THE BUFFER**.  If memory can be (re-)allocated then it returns 1, else it
 * returns 0.
 * 
 * @param buf  pointer to a buffer pointer
 * @param buf_len      pointer to current size of buffer in bytes
 * 
 * @note
 * The current re-allocation algorithm is to increase the buffer size by
 * whichever is the greater of 256 bytes or the current buffer size, up to
 * a maximum increase of 8192 bytes.  
 */
int
snmp_realloc(u_char ** buf, size_t * buf_len)
{
    u_char         *new_buf = NULL;
    size_t          new_buf_len = 0;

    if (buf == NULL) {
        return 0;
    }

    if (*buf_len <= 255) {
        new_buf_len = *buf_len + 256;
    } else if (*buf_len > 255 && *buf_len <= 8191) {
        new_buf_len = *buf_len * 2;
    } else if (*buf_len > 8191) {
        new_buf_len = *buf_len + 8192;
    }

    if (*buf == NULL) {
        new_buf = (u_char *) malloc(new_buf_len);
    } else {
        new_buf = (u_char *) realloc(*buf, new_buf_len);
    }

    if (new_buf != NULL) {
        *buf = new_buf;
        *buf_len = new_buf_len;
        return 1;
    } else {
        return 0;
    }
}

int
snmp_strcat(u_char ** buf, size_t * buf_len, size_t * out_len,
            int allow_realloc, const u_char * s)
{
    if (buf == NULL || buf_len == NULL || out_len == NULL) {
        return 0;
    }

    if (s == NULL) {
        /*
         * Appending a NULL string always succeeds since it is a NOP.  
         */
        return 1;
    }

    while ((*out_len + strlen((const char *) s) + 1) >= *buf_len) {
        if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
            return 0;
        }
    }

    strcpy((char *) (*buf + *out_len), (const char *) s);
    *out_len += strlen((char *) (*buf + *out_len));
    return 1;
}

/** zeros memory before freeing it.
 *
 *	@param *buf	Pointer at bytes to free.
 *	@param size	Number of bytes in buf.
 */
void
free_zero(void *buf, size_t size)
{
    if (buf) {
        memset(buf, 0, size);
        free(buf);
    }

}                               /* end free_zero() */

#ifndef NETSNMP_FEATURE_REMOVE_USM_SCAPI
/**
 * Returns pointer to allocaed & set buffer on success, size contains
 * number of random bytes filled.  buf is NULL and *size set to KMT
 * error value upon failure.
 *
 *	@param size	Number of bytes to malloc() and fill with random bytes.
 *
 * @return a malloced buffer
 *
 */
u_char         *
malloc_random(size_t * size)
{
    int             rval = SNMPERR_SUCCESS;
    u_char         *buf = (u_char *) calloc(1, *size);

    if (buf) {
        rval = sc_random(buf, size);

        if (rval < 0) {
            free_zero(buf, *size);
            buf = NULL;
        } else {
            *size = rval;
        }
    }

    return buf;

}                               /* end malloc_random() */
#endif /* NETSNMP_FEATURE_REMOVE_USM_SCAPI */

/** Duplicates a memory block.
 *  Copies a existing memory location from a pointer to another, newly
    malloced, pointer.

 *	@param to       Pointer to allocate and copy memory to.
 *      @param from     Pointer to copy memory from.
 *      @param size     Size of the data to be copied.
 *      
 *	@return SNMPERR_SUCCESS	on success, SNMPERR_GENERR on failure.
 */
int
memdup(u_char ** to, const void * from, size_t size)
{
    if (to == NULL)
        return SNMPERR_GENERR;
    if (from == NULL) {
        *to = NULL;
        return SNMPERR_SUCCESS;
    }
    if ((*to = (u_char *) malloc(size)) == NULL)
        return SNMPERR_GENERR;
    memcpy(*to, from, size);
    return SNMPERR_SUCCESS;

}                               /* end memdup() */

#ifndef NETSNMP_FEATURE_REMOVE_NETSNMP_CHECK_DEFINEDNESS
/**
 * When running under Valgrind, check whether all bytes in the range [packet,
 * packet+length) are defined. Let Valgrind print a backtrace if one or more
 * bytes with uninitialized values have been found. This function can help to
 * find the cause of undefined value errors if --track-origins=yes is not
 * sufficient. Does nothing when not running under Valgrind.
 *
 * Note: this requires a fairly recent valgrind.
 */
void
netsnmp_check_definedness(const void *packet, size_t length)
{
#if defined(__VALGRIND_MAJOR__) && defined(__VALGRIND_MINOR__)   \
    && (__VALGRIND_MAJOR__ > 3                                   \
        || (__VALGRIND_MAJOR__ == 3 && __VALGRIND_MINOR__ >= 6))

    if (RUNNING_ON_VALGRIND) {
        int i;
        char vbits;

        for (i = 0; i < length; ++i) {
            if (VALGRIND_GET_VBITS((const char *)packet + i, &vbits, 1) == 1
                && vbits)
                VALGRIND_PRINTF_BACKTRACE("Undefined: byte %d/%d", i,
                                          (int)length);
        }
    }

#endif
}
#endif /* NETSNMP_FEATURE_REMOVE_NETSNMP_CHECK_DEFINEDNESS */

/** copies a (possible) unterminated string of a given length into a
 *  new buffer and null terminates it as well (new buffer MAY be one
 *  byte longer to account for this */
char           *
netsnmp_strdup_and_null(const u_char * from, size_t from_len)
{
    char         *ret;

    if (from_len == 0 || from[from_len - 1] != '\0') {
        ret = (char *)malloc(from_len + 1);
        if (!ret)
            return NULL;
        ret[from_len] = '\0';
    } else {
        ret = (char *)malloc(from_len);
        if (!ret)
            return NULL;
        ret[from_len - 1] = '\0';
    }
    memcpy(ret, from, from_len);
    return ret;
}

/** converts binary to hexidecimal
 *
 *     @param *input            Binary data.
 *     @param len               Length of binary data.
 *     @param **dest            NULL terminated string equivalent in hex.
 *     @param *dest_len         size of destination buffer
 *     @param allow_realloc     flag indicating if buffer can be realloc'd
 *      
 * @return olen	Length of output string not including NULL terminator.
 */
u_int
netsnmp_binary_to_hex(u_char ** dest, size_t *dest_len, int allow_realloc, 
                      const u_char * input, size_t len)
{
    u_int           olen = (len * 2) + 1;
    u_char         *s, *op;
    const u_char   *ip = input;

    if (dest == NULL || dest_len == NULL || input == NULL)
        return 0;

    if (NULL == *dest) {
        s = (unsigned char *) calloc(1, olen);
        *dest_len = olen;
    }
    else
        s = *dest;

    if (*dest_len < olen) {
        if (!allow_realloc)
            return 0;
        *dest_len = olen;
        if (snmp_realloc(dest, dest_len))
            return 0;
    }

    op = s;
    while (ip - input < (int) len) {
        *op++ = VAL2HEX((*ip >> 4) & 0xf);
        *op++ = VAL2HEX(*ip & 0xf);
        ip++;
    }
    *op = '\0';

    if (s != *dest)
        *dest = s;
    *dest_len = olen;

    return olen;

}                               /* end netsnmp_binary_to_hex() */

/** converts binary to hexidecimal
 *
 *	@param *input		Binary data.
 *	@param len		Length of binary data.
 *	@param **output	NULL terminated string equivalent in hex.
 *      
 * @return olen	Length of output string not including NULL terminator.
 *
 * FIX	Is there already one of these in the UCD SNMP codebase?
 *	The old one should be used, or this one should be moved to
 *	snmplib/snmp_api.c.
 */
u_int
binary_to_hex(const u_char * input, size_t len, char **output)
{
    size_t out_len = 0;

    *output = NULL; /* will alloc new buffer */

    return netsnmp_binary_to_hex((u_char**)output, &out_len, 1, input, len);
}                               /* end binary_to_hex() */




/**
 * hex_to_binary2
 *	@param *input		Printable data in base16.
 *	@param len		Length in bytes of data.
 *	@param **output	Binary data equivalent to input.
 *      
 * @return SNMPERR_GENERR on failure, otherwise length of allocated string.
 *
 * Input of an odd length is right aligned.
 *
 * FIX	Another version of "hex-to-binary" which takes odd length input
 *	strings.  It also allocates the memory to hold the binary data.
 *	Should be integrated with the official hex_to_binary() function.
 */
int
hex_to_binary2(const u_char * input, size_t len, char **output)
{
    u_int           olen = (len / 2) + (len % 2);
    char           *s = (char *) calloc(1, (olen) ? olen : 1), *op = s;
    const u_char   *ip = input;


    *output = NULL;
    *op = 0;
    if (len % 2) {
        if (!isxdigit(*ip))
            goto hex_to_binary2_quit;
        *op++ = HEX2VAL(*ip);
        ip++;
    }

    while (ip - input < (int) len) {
        if (!isxdigit(*ip))
            goto hex_to_binary2_quit;
        *op = HEX2VAL(*ip) << 4;
        ip++;

        if (!isxdigit(*ip))
            goto hex_to_binary2_quit;
        *op++ += HEX2VAL(*ip);
        ip++;
    }

    *output = s;
    return olen;

  hex_to_binary2_quit:
    free_zero(s, olen);
    return -1;

}                               /* end hex_to_binary2() */

int
snmp_decimal_to_binary(u_char ** buf, size_t * buf_len, size_t * out_len,
                       int allow_realloc, const char *decimal)
{
    int             subid = 0;
    const char     *cp = decimal;

    if (buf == NULL || buf_len == NULL || out_len == NULL
        || decimal == NULL) {
        return 0;
    }

    while (*cp != '\0') {
        if (isspace((int) *cp) || *cp == '.') {
            cp++;
            continue;
        }
        if (!isdigit((int) *cp)) {
            return 0;
        }
        if ((subid = atoi(cp)) > 255) {
            return 0;
        }
        if ((*out_len >= *buf_len) &&
            !(allow_realloc && snmp_realloc(buf, buf_len))) {
            return 0;
        }
        *(*buf + *out_len) = (u_char) subid;
        (*out_len)++;
        while (isdigit((int) *cp)) {
            cp++;
        }
    }
    return 1;
}

/**
 * convert an ASCII hex string (with specified delimiters) to binary
 *
 * @param buf     address of a pointer (pointer to pointer) for the output buffer.
 *                If allow_realloc is set, the buffer may be grown via snmp_realloc
 *                to accomodate the data.
 *
 * @param buf_len pointer to a size_t containing the initial size of buf.
 *
 * @param offset On input, a pointer to a size_t indicating an offset into buf.
 *                The  binary data will be stored at this offset.
 *                On output, this pointer will have updated the offset to be
 *                the first byte after the converted data.
 *
 * @param allow_realloc If true, the buffer can be reallocated. If false, and
 *                      the buffer is not large enough to contain the string,
 *                      an error will be returned.
 *
 * @param hex     pointer to hex string to be converted. May be prefixed by
 *                "0x" or "0X".
 *
 * @param delim   point to a string of allowed delimiters between bytes.
 *                If not specified, any non-hex characters will be an error.
 *
 * @retval 1  success
 * @retval 0  error
 */
int
netsnmp_hex_to_binary(u_char ** buf, size_t * buf_len, size_t * offset,
                      int allow_realloc, const char *hex, const char *delim)
{
    unsigned int    subid = 0;
    const char     *cp = hex;

    if (buf == NULL || buf_len == NULL || offset == NULL || hex == NULL) {
        return 0;
    }

    if ((*cp == '0') && ((*(cp + 1) == 'x') || (*(cp + 1) == 'X'))) {
        cp += 2;
    }

    while (*cp != '\0') {
        if (!isxdigit((int) *cp) ||
            !isxdigit((int) *(cp+1))) {
            if ((NULL != delim) && (NULL != strchr(delim, *cp))) {
                cp++;
                continue;
            }
            return 0;
        }
        if (sscanf(cp, "%2x", &subid) == 0) {
            return 0;
        }
        /*
         * if we dont' have enough space, realloc.
         * (snmp_realloc will adjust buf_len to new size)
         */
        if ((*offset >= *buf_len) &&
            !(allow_realloc && snmp_realloc(buf, buf_len))) {
            return 0;
        }
        *(*buf + *offset) = (u_char) subid;
        (*offset)++;
        if (*++cp == '\0') {
            /*
             * Odd number of hex digits is an error.  
             */
            return 0;
        } else {
            cp++;
        }
    }
    return 1;
}

/**
 * convert an ASCII hex string to binary
 *
 * @note This is a wrapper which calls netsnmp_hex_to_binary with a
 * delimiter string of " ".
 *
 * See netsnmp_hex_to_binary for parameter descriptions.
 *
 * @retval 1  success
 * @retval 0  error
 */
int
snmp_hex_to_binary(u_char ** buf, size_t * buf_len, size_t * offset,
                   int allow_realloc, const char *hex)
{
    return netsnmp_hex_to_binary(buf, buf_len, offset, allow_realloc, hex, " ");
}

/*******************************************************************-o-******
 * dump_chunk
 *
 * Parameters:
 *	*title	(May be NULL.)
 *	*buf
 *	 size
 */
void
dump_chunk(const char *debugtoken, const char *title, const u_char * buf,
           int size)
{
    int             printunit = 64;     /* XXX  Make global. */
    char            chunk[SNMP_MAXBUF], *s, *sp;

    if (title && (*title != '\0')) {
        DEBUGMSGTL((debugtoken, "%s\n", title));
    }


    memset(chunk, 0, SNMP_MAXBUF);
    size = binary_to_hex(buf, size, &s);
    sp = s;

    while (size > 0) {
        if (size > printunit) {
            memcpy(chunk, sp, printunit);
            chunk[printunit] = '\0';
            DEBUGMSGTL((debugtoken, "\t%s\n", chunk));
        } else {
            DEBUGMSGTL((debugtoken, "\t%s\n", sp));
        }

        sp += printunit;
        size -= printunit;
    }


    SNMP_FREE(s);

}                               /* end dump_chunk() */




/*******************************************************************-o-******
 * dump_snmpEngineID
 *
 * Parameters:
 *	*estring
 *	*estring_len
 *      
 * Returns:
 *	Allocated memory pointing to a string of buflen char representing
 *	a printf'able form of the snmpEngineID.
 *
 *	-OR- NULL on error.
 *
 *
 * Translates the snmpEngineID TC into a printable string.  From RFC 2271,
 * Section 5 (pp. 36-37):
 *
 * First bit:	0	Bit string structured by means non-SNMPv3.
 *  		1	Structure described by SNMPv3 SnmpEngineID TC.
 *  
 * Bytes 1-4:		Enterprise ID.  (High bit of first byte is ignored.)
 *  
 * Byte 5:	0	(RESERVED by IANA.)
 *  		1	IPv4 address.		(   4 octets)
 *  		2	IPv6 address.		(  16 octets)
 *  		3	MAC address.		(   6 octets)
 *  		4	Locally defined text.	(0-27 octets)
 *  		5	Locally defined octets.	(0-27 octets)
 *  		6-127	(RESERVED for enterprise.)
 *  
 * Bytes 6-32:		(Determined by byte 5.)
 *  
 *
 * Non-printable characters are given in hex.  Text is given in quotes.
 * IP and MAC addresses are given in standard (UN*X) conventions.  Sections
 * are comma separated.
 *
 * esp, remaining_len and s trace the state of the constructed buffer.
 * s will be defined if there is something to return, and it will point
 * to the end of the constructed buffer.
 *
 *
 * ASSUME  "Text" means printable characters.
 *
 * XXX	Must the snmpEngineID always have a minimum length of 12?
 *	(Cf. part 2 of the TC definition.)
 * XXX	Does not enforce upper-bound of 32 bytes.
 * XXX	Need a switch to decide whether to use DNS name instead of a simple
 *	IP address.
 *
 * FIX	Use something other than snprint_hexstring which doesn't add 
 *	trailing spaces and (sometimes embedded) newlines...
 */
#ifdef NETSNMP_ENABLE_TESTING_CODE
char           *
dump_snmpEngineID(const u_char * estring, size_t * estring_len)
{
#define eb(b)	( *(esp+b) & 0xff )

    int             rval = SNMPERR_SUCCESS, gotviolation = 0, slen = 0;
    u_int           remaining_len;

    char            buf[SNMP_MAXBUF], *s = NULL, *t;
    const u_char   *esp = estring;

    struct in_addr  iaddr;



    /*
     * Sanity check.
     */
    if (!estring || (*estring_len <= 0)) {
        QUITFUN(SNMPERR_GENERR, dump_snmpEngineID_quit);
    }
    remaining_len = *estring_len;
    memset(buf, 0, SNMP_MAXBUF);



    /*
     * Test first bit.  Return immediately with a hex string, or
     * begin by formatting the enterprise ID.
     */
    if (!(*esp & 0x80)) {
        snprint_hexstring(buf, SNMP_MAXBUF, esp, remaining_len);
        s = strchr(buf, '\0');
        s -= 1;
        goto dump_snmpEngineID_quit;
    }

    s = buf;
    s += sprintf(s, "enterprise %d, ", ((*(esp + 0) & 0x7f) << 24) |
                 ((*(esp + 1) & 0xff) << 16) |
                 ((*(esp + 2) & 0xff) << 8) | ((*(esp + 3) & 0xff)));
    /*
     * XXX  Ick. 
     */

    if (remaining_len < 5) {    /* XXX  Violating string. */
        goto dump_snmpEngineID_quit;
    }

    esp += 4;                   /* Incremented one more in the switch below. */
    remaining_len -= 5;



    /*
     * Act on the fifth byte.
     */
    switch ((int) *esp++) {
    case 1:                    /* IPv4 address. */

        if (remaining_len < 4)
            goto dump_snmpEngineID_violation;
        memcpy(&iaddr.s_addr, esp, 4);

        if (!(t = inet_ntoa(iaddr)))
            goto dump_snmpEngineID_violation;
        s += sprintf(s, "%s", t);

        esp += 4;
        remaining_len -= 4;
        break;

    case 2:                    /* IPv6 address. */

        if (remaining_len < 16)
            goto dump_snmpEngineID_violation;

        s += sprintf(s,
                     "%02X%02X %02X%02X %02X%02X %02X%02X::"
                     "%02X%02X %02X%02X %02X%02X %02X%02X",
                     eb(0), eb(1), eb(2), eb(3),
                     eb(4), eb(5), eb(6), eb(7),
                     eb(8), eb(9), eb(10), eb(11),
                     eb(12), eb(13), eb(14), eb(15));

        esp += 16;
        remaining_len -= 16;
        break;

    case 3:                    /* MAC address. */

        if (remaining_len < 6)
            goto dump_snmpEngineID_violation;

        s += sprintf(s, "%02X:%02X:%02X:%02X:%02X:%02X",
                     eb(0), eb(1), eb(2), eb(3), eb(4), eb(5));

        esp += 6;
        remaining_len -= 6;
        break;

    case 4:                    /* Text. */

        s += sprintf(s, "\"%.*s\"", (int) (sizeof(buf)-strlen(buf)-3), esp);
        goto dump_snmpEngineID_quit;
        break;

     /*NOTREACHED*/ case 5:    /* Octets. */

        snprint_hexstring(s, (SNMP_MAXBUF - (s-buf)),
                          esp, remaining_len);
        s = strchr(buf, '\0');
        s -= 1;
        goto dump_snmpEngineID_quit;
        break;

       /*NOTREACHED*/ dump_snmpEngineID_violation:
    case 0:                    /* Violation of RESERVED, 
                                 * *   -OR- of expected length.
                                 */
        gotviolation = 1;
        s += sprintf(s, "!!! ");

    default:                   /* Unknown encoding. */

        if (!gotviolation) {
            s += sprintf(s, "??? ");
        }
        snprint_hexstring(s, (SNMP_MAXBUF - (s-buf)),
                          esp, remaining_len);
        s = strchr(buf, '\0');
        s -= 1;

        goto dump_snmpEngineID_quit;

    }                           /* endswitch */



    /*
     * Cases 1-3 (IP and MAC addresses) should not have trailing
     * octets, but perhaps they do.  Throw them in too.  XXX
     */
    if (remaining_len > 0) {
        s += sprintf(s, " (??? ");

        snprint_hexstring(s, (SNMP_MAXBUF - (s-buf)),
                          esp, remaining_len);
        s = strchr(buf, '\0');
        s -= 1;

        s += sprintf(s, ")");
    }



  dump_snmpEngineID_quit:
    if (s) {
        slen = s - buf + 1;
        s = calloc(1, slen);
        memcpy(s, buf, (slen) - 1);
    }

    memset(buf, 0, SNMP_MAXBUF);        /* XXX -- Overkill? XXX: Yes! */

    return s;

#undef eb
}                               /* end dump_snmpEngineID() */
#endif                          /* NETSNMP_ENABLE_TESTING_CODE */


/**
 * Create a new real-time marker.
 *
 * \deprecated Use netsnmp_set_monotonic_marker() instead.
 *
 * @note Caller must free time marker when no longer needed.
 */
marker_t
atime_newMarker(void)
{
    marker_t        pm = (marker_t) calloc(1, sizeof(struct timeval));
    gettimeofday((struct timeval *) pm, NULL);
    return pm;
}

/**
 * Set a time marker to the current value of the real-time clock.
 * \deprecated Use netsnmp_set_monotonic_marker() instead.
 */
void
atime_setMarker(marker_t pm)
{
    if (!pm)
        return;

    gettimeofday((struct timeval *) pm, NULL);
}

/**
 * Query the current value of the monotonic clock.
 *
 * Returns the current value of a monotonic clock if such a clock is provided by
 * the operating system or the wall clock time if no such clock is provided by
 * the operating system. A monotonic clock is a clock that is never adjusted
 * backwards and that proceeds at the same rate as wall clock time.
 *
 * @param[out] tv Pointer to monotonic clock time.
 */
void netsnmp_get_monotonic_clock(struct timeval* tv)
{
#if defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)
    struct timespec ts;
    int res;

    res = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (res >= 0) {
        tv->tv_sec = ts.tv_sec;
        tv->tv_usec = ts.tv_nsec / 1000;
    } else {
        netsnmp_assert(FALSE);
        memset(tv, 0, sizeof(*tv));
    }
#elif defined(WIN32)
    /*
     * Windows: return tick count. Note: the rate at which the tick count
     * increases is not adjusted by the time synchronization algorithm, so
     * expect an error of <= 100 ppm for the rate at which this clock
     * increases.
     */
    typedef ULONGLONG (WINAPI * pfGetTickCount64)(void);
    static int s_initialized;
    static pfGetTickCount64 s_pfGetTickCount64;
    uint64_t now64;

    if (!s_initialized) {
        HMODULE hKernel32 = GetModuleHandle("kernel32");
        s_pfGetTickCount64 =
            (pfGetTickCount64) GetProcAddress(hKernel32, "GetTickCount64");
        s_initialized = TRUE;
    }

    if (s_pfGetTickCount64) {
        /* Windows Vista, Windows 2008 or any later Windows version */
        now64 = (*s_pfGetTickCount64)();
    } else {
        /* Windows XP, Windows 2003 or any earlier Windows version */
        static uint32_t s_wraps, s_last;
        uint32_t now;

        now = GetTickCount();
        if (now < s_last)
            s_wraps++;
        s_last = now;
        now64 = ((uint64_t)s_wraps << 32) | now;
    }
    tv->tv_sec = now64 / 1000;
    tv->tv_usec = (now64 % 1000) * 1000;
#else
    /* At least FreeBSD 4 doesn't provide monotonic clock support. */
#warning Not sure how to query a monotonically increasing clock on your system. \
Timers will not work correctly if the system clock is adjusted by e.g. ntpd.
    gettimeofday(tv, NULL);
#endif
}

/**
 * Set a time marker to the current value of the monotonic clock.
 */
void
netsnmp_set_monotonic_marker(marker_t *pm)
{
    if (!*pm)
        *pm = malloc(sizeof(struct timeval));
    if (*pm)
        netsnmp_get_monotonic_clock(*pm);
}

/**
 * Returns the difference (in msec) between the two markers
 *
 * \deprecated Don't use in new code.
 */
long
atime_diff(const_marker_t first, const_marker_t second)
{
    struct timeval diff;

    NETSNMP_TIMERSUB((const struct timeval *) second, (const struct timeval *) first, &diff);

    return (long)(diff.tv_sec * 1000 + diff.tv_usec / 1000);
}

/**
 * Returns the difference (in u_long msec) between the two markers
 *
 * \deprecated Don't use in new code.
 */
u_long
uatime_diff(const_marker_t first, const_marker_t second)
{
    struct timeval diff;

    NETSNMP_TIMERSUB((const struct timeval *) second, (const struct timeval *) first, &diff);

    return (((u_long) diff.tv_sec) * 1000 + diff.tv_usec / 1000);
}

/**
 * Returns the difference (in u_long 1/100th secs) between the two markers
 * (functionally this is what sysUpTime needs)
 *
 * \deprecated Don't use in new code.
 */
u_long
uatime_hdiff(const_marker_t first, const_marker_t second)
{
    struct timeval diff;

    NETSNMP_TIMERSUB((const struct timeval *) second, (const struct timeval *) first, &diff);
    return ((u_long) diff.tv_sec) * 100 + diff.tv_usec / 10000;
}

/**
 * Test: Has (marked time plus delta) exceeded current time ?
 * Returns 0 if test fails or cannot be tested (no marker).
 *
 * \deprecated Use netsnmp_ready_monotonic() instead.
 */
int
atime_ready(const_marker_t pm, int delta_ms)
{
    marker_t        now;
    long            diff;
    if (!pm)
        return 0;

    now = atime_newMarker();

    diff = atime_diff(pm, now);
    free(now);
    if (diff < delta_ms)
        return 0;

    return 1;
}

#ifndef NETSNMP_FEATURE_REMOVE_UATIME_READY
/**
 * Test: Has (marked time plus delta) exceeded current time ?
 * Returns 0 if test fails or cannot be tested (no marker).
 *
 * \deprecated Use netsnmp_ready_monotonic() instead.
 */
int
uatime_ready(const_marker_t pm, unsigned int delta_ms)
{
    marker_t        now;
    u_long          diff;
    if (!pm)
        return 0;

    now = atime_newMarker();

    diff = uatime_diff(pm, now);
    free(now);
    if (diff < delta_ms)
        return 0;

    return 1;
}
#endif /* NETSNMP_FEATURE_REMOVE_UATIME_READY */

/**
 * Is the current time past (marked time plus delta) ?
 *
 * @param[in] pm Pointer to marked time as obtained via
 *   netsnmp_set_monotonic_marker().
 * @param[in] delta_ms Time delta in milliseconds.
 *
 * @return pm != NULL && now >= (*pm + delta_ms)
 */
int
netsnmp_ready_monotonic(const_marker_t pm, int delta_ms)
{
    struct timeval  now, diff, delta;

    netsnmp_assert(delta_ms >= 0);
    if (pm) {
        netsnmp_get_monotonic_clock(&now);
        NETSNMP_TIMERSUB(&now, (const struct timeval *) pm, &diff);
        delta.tv_sec = delta_ms / 1000;
        delta.tv_usec = (delta_ms % 1000) * 1000UL;
        return timercmp(&diff, &delta, >=) ? TRUE : FALSE;
    } else {
        return FALSE;
    }
}


        /*
         * Time-related utility functions
         */

/**
 * Return the number of timeTicks since the given marker
 *
 * \deprecated Don't use in new code.
 */
int
marker_tticks(const_marker_t pm)
{
    int             res;
    marker_t        now = atime_newMarker();

    res = atime_diff(pm, now);
    free(now);
    return res / 10;            /* atime_diff works in msec, not csec */
}

#ifndef NETSNMP_FEATURE_REMOVE_TIMEVAL_TTICKS
/**
 * \deprecated Don't use in new code.
 */
int
timeval_tticks(const struct timeval *tv)
{
    return marker_tticks((const_marker_t) tv);
}
#endif /* NETSNMP_FEATURE_REMOVE_TIMEVAL_TTICKS */

/**
 * Non Windows:  Returns a pointer to the desired environment variable  
 *               or NULL if the environment variable does not exist.  
 *               
 * Windows:      Returns a pointer to the desired environment variable  
 *               if it exists.  If it does not, the variable is looked up
 *               in the registry in HKCU\\Net-SNMP or HKLM\\Net-SNMP
 *               (whichever it finds first) and stores the result in the 
 *               environment variable.  It then returns a pointer to 
 *               environment variable.
 */

char *netsnmp_getenv(const char *name)
{
#if !defined (WIN32) && !defined (cygwin)
  return (getenv(name));
#else
  char *temp = NULL;  
  HKEY hKey;
  unsigned char * key_value = NULL;
  DWORD key_value_size = 0;
  DWORD key_value_type = 0;
  DWORD getenv_worked = 0;

  DEBUGMSGTL(("read_config", "netsnmp_getenv called with name: %s\n",name));

  if (!(name))
    return NULL;
  
  /* Try environment variable first */ 
  temp = getenv(name);
  if (temp) {
    getenv_worked = 1;
    DEBUGMSGTL(("read_config", "netsnmp_getenv will return from ENV: %s\n",temp));
  }
  
  /* Next try HKCU */
  if (temp == NULL)
  {
    if (getenv("SNMP_IGNORE_WINDOWS_REGISTRY"))
      return NULL;

    if (RegOpenKeyExA(
          HKEY_CURRENT_USER, 
          "SOFTWARE\\Net-SNMP", 
          0, 
          KEY_QUERY_VALUE, 
          &hKey) == ERROR_SUCCESS) {   
      
      if (RegQueryValueExA(
            hKey, 
            name, 
            NULL, 
            &key_value_type, 
            NULL,               /* Just get the size */
            &key_value_size) == ERROR_SUCCESS) {

        SNMP_FREE(key_value);

        /* Allocate memory needed +1 to allow RegQueryValueExA to NULL terminate the
         * string data in registry is missing one (which is unlikely).
         */
        key_value = malloc((sizeof(char) * key_value_size)+sizeof(char));
        
        if (RegQueryValueExA(
              hKey, 
              name, 
              NULL, 
              &key_value_type, 
              key_value, 
              &key_value_size) == ERROR_SUCCESS) {
        }
        temp = (char *) key_value;
      }
      RegCloseKey(hKey);
      if (temp)
        DEBUGMSGTL(("read_config", "netsnmp_getenv will return from HKCU: %s\n",temp));
    }
  }

  /* Next try HKLM */
  if (temp == NULL)
  {
    if (RegOpenKeyExA(
          HKEY_LOCAL_MACHINE, 
          "SOFTWARE\\Net-SNMP", 
          0, 
          KEY_QUERY_VALUE, 
          &hKey) == ERROR_SUCCESS) {   
      
      if (RegQueryValueExA(
            hKey, 
            name, 
            NULL, 
            &key_value_type, 
            NULL,               /* Just get the size */
            &key_value_size) == ERROR_SUCCESS) {

        SNMP_FREE(key_value);

        /* Allocate memory needed +1 to allow RegQueryValueExA to NULL terminate the
         * string data in registry is missing one (which is unlikely).
         */
        key_value = malloc((sizeof(char) * key_value_size)+sizeof(char));
        
        if (RegQueryValueExA(
              hKey, 
              name, 
              NULL, 
              &key_value_type, 
              key_value, 
              &key_value_size) == ERROR_SUCCESS) {
        }
        temp = (char *) key_value;

      }
      RegCloseKey(hKey);
      if (temp)
        DEBUGMSGTL(("read_config", "netsnmp_getenv will return from HKLM: %s\n",temp));
    }
  }
  
  if (temp && !getenv_worked) {
    setenv(name, temp, 1);
    SNMP_FREE(temp);
  }

  DEBUGMSGTL(("read_config", "netsnmp_getenv returning: %s\n",getenv(name)));

  return(getenv(name));
#endif
}

/**
 * Set an environment variable.
 *
 * This function is only necessary on Windows for the MSVC and MinGW
 * environments. If the process that uses the Net-SNMP DLL (e.g. a Perl
 * interpreter) and the Net-SNMP have been built with a different compiler
 * version then each will have a separate set of environment variables.
 * This function allows to set an environment variable such that it gets
 * noticed by the Net-SNMP DLL.
 */
int netsnmp_setenv(const char *envname, const char *envval, int overwrite)
{
    return setenv(envname, envval, overwrite);
}

/*
 * swap the order of an inet addr string
 */
int
netsnmp_addrstr_hton(char *ptr, size_t len)
{
#ifndef WORDS_BIGENDIAN
    char tmp[8];
    
    if (8 == len) {
        tmp[0] = ptr[6];
        tmp[1] = ptr[7];
        tmp[2] = ptr[4];
        tmp[3] = ptr[5];
        tmp[4] = ptr[2];
        tmp[5] = ptr[3];
        tmp[6] = ptr[0];
        tmp[7] = ptr[1];
        memcpy (ptr, &tmp, 8);
    }
    else if (32 == len) {
        netsnmp_addrstr_hton(ptr   , 8);
        netsnmp_addrstr_hton(ptr+8 , 8);
        netsnmp_addrstr_hton(ptr+16, 8);
        netsnmp_addrstr_hton(ptr+24, 8);
    }
    else
        return -1;
#endif

    return 0;
}

#ifndef NETSNMP_FEATURE_REMOVE_STRING_TIME_TO_SECS
/**
 * Takes a time string like 4h and converts it to seconds.
 * The string time given may end in 's' for seconds (the default
 * anyway if no suffix is specified),
 * 'm' for minutes, 'h' for hours, 'd' for days, or 'w' for weeks.  The
 * upper case versions are also accepted.
 *
 * @param time_string The time string to convert.
 *
 * @return seconds converted from the string
 * @return -1  : on failure
 */
int
netsnmp_string_time_to_secs(const char *time_string) {
    int secs = -1;
    if (!time_string || !time_string[0])
        return secs;

    secs = atoi(time_string);

    if (isdigit((unsigned char)time_string[strlen(time_string)-1]))
        return secs; /* no letter specified, it's already in seconds */
    
    switch (time_string[strlen(time_string)-1]) {
    case 's':
    case 'S':
        /* already in seconds */
        break;

    case 'm':
    case 'M':
        secs = secs * 60;
        break;

    case 'h':
    case 'H':
        secs = secs * 60 * 60;
        break;

    case 'd':
    case 'D':
        secs = secs * 60 * 60 * 24;
        break;

    case 'w':
    case 'W':
        secs = secs * 60 * 60 * 24 * 7;
        break;

    default:
        snmp_log(LOG_ERR, "time string %s contains an invalid suffix letter\n",
                 time_string);
        return -1;
    }

    DEBUGMSGTL(("string_time_to_secs", "Converted time string %s to %d\n",
                time_string, secs));
    return secs;
}
#endif /* NETSNMP_FEATURE_REMOVE_STRING_TIME_TO_SECS */
