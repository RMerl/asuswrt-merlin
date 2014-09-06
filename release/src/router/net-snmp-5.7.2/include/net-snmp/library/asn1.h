#ifndef ASN1_H
#define ASN1_H

#include <net-snmp/library/oid.h>

#ifdef __cplusplus
extern          "C" {
#endif

#define PARSE_PACKET	0
#define DUMP_PACKET	1

    /*
     * Definitions for Abstract Syntax Notation One, ASN.1
     * As defined in ISO/IS 8824 and ISO/IS 8825
     *
     *
     */
/***********************************************************
	Copyright 1988, 1989 by Carnegie Mellon University

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/


#define MIN_OID_LEN	    2
#define MAX_OID_LEN	    128 /* max subid's in an oid */
#ifndef MAX_NAME_LEN            /* conflicts with some libraries */
#define MAX_NAME_LEN	    MAX_OID_LEN /* obsolete. use MAX_OID_LEN */
#endif

#define OID_LENGTH(x)  (sizeof(x)/sizeof(oid))


#define ASN_BOOLEAN	    ((u_char)0x01)
#define ASN_INTEGER	    ((u_char)0x02)
#define ASN_BIT_STR	    ((u_char)0x03)
#define ASN_OCTET_STR	    ((u_char)0x04)
#define ASN_NULL	    ((u_char)0x05)
#define ASN_OBJECT_ID	    ((u_char)0x06)
#define ASN_SEQUENCE	    ((u_char)0x10)
#define ASN_SET		    ((u_char)0x11)

#define ASN_UNIVERSAL	    ((u_char)0x00)
#define ASN_APPLICATION     ((u_char)0x40)
#define ASN_CONTEXT	    ((u_char)0x80)
#define ASN_PRIVATE	    ((u_char)0xC0)

#define ASN_PRIMITIVE	    ((u_char)0x00)
#define ASN_CONSTRUCTOR	    ((u_char)0x20)

#define ASN_LONG_LEN	    (0x80)
#define ASN_EXTENSION_ID    (0x1F)
#define ASN_BIT8	    (0x80)

#define IS_CONSTRUCTOR(byte)	((byte) & ASN_CONSTRUCTOR)
#define IS_EXTENSION_ID(byte)	(((byte) & ASN_EXTENSION_ID) == ASN_EXTENSION_ID)

    struct counter64 {
        u_long          high;
        u_long          low;
    };

#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
    typedef struct counter64 integer64;
    typedef struct counter64 unsigned64;

    /*
     * The BER inside an OPAQUE is an context specific with a value of 48 (0x30)
     * plus the "normal" tag. For a Counter64, the tag is 0x46 (i.e., an
     * applications specific tag with value 6). So the value for a 64 bit
     * counter is 0x46 + 0x30, or 0x76 (118 base 10). However, values
     * greater than 30 can not be encoded in one octet. So the first octet
     * has the class, in this case context specific (ASN_CONTEXT), and
     * the special value (i.e., 31) to indicate that the real value follows
     * in one or more octets. The high order bit of each following octet
     * indicates if the value is encoded in additional octets. A high order
     * bit of zero, indicates the last. For this "hack", only one octet
     * will be used for the value. 
     */

    /*
     * first octet of the tag 
     */
#define ASN_OPAQUE_TAG1 (ASN_CONTEXT | ASN_EXTENSION_ID)
    /*
     * base value for the second octet of the tag - the
     * second octet was the value for the tag 
     */
#define ASN_OPAQUE_TAG2 ((u_char)0x30)

#define ASN_OPAQUE_TAG2U ((u_char)0x2f) /* second octet of tag for union */

    /*
     * All the ASN.1 types for SNMP "should have been" defined in this file,
     * but they were not. (They are defined in snmp_impl.h)  Thus, the tag for
     * Opaque and Counter64 is defined, again, here with a different names. 
     */
#define ASN_APP_OPAQUE (ASN_APPLICATION | 4)
#define ASN_APP_COUNTER64 (ASN_APPLICATION | 6)
#define ASN_APP_FLOAT (ASN_APPLICATION | 8)
#define ASN_APP_DOUBLE (ASN_APPLICATION | 9)
#define ASN_APP_I64 (ASN_APPLICATION | 10)
#define ASN_APP_U64 (ASN_APPLICATION | 11)
#define ASN_APP_UNION (ASN_PRIVATE | 1) /* or ASN_PRIV_UNION ? */

    /*
     * value for Counter64 
     */
#define ASN_OPAQUE_COUNTER64 (ASN_OPAQUE_TAG2 + ASN_APP_COUNTER64)
    /*
     * max size of BER encoding of Counter64 
     */
#define ASN_OPAQUE_COUNTER64_MX_BER_LEN 12

    /*
     * value for Float 
     */
#define ASN_OPAQUE_FLOAT (ASN_OPAQUE_TAG2 + ASN_APP_FLOAT)
    /*
     * size of BER encoding of Float 
     */
#define ASN_OPAQUE_FLOAT_BER_LEN 7

    /*
     * value for Double 
     */
#define ASN_OPAQUE_DOUBLE (ASN_OPAQUE_TAG2 + ASN_APP_DOUBLE)
    /*
     * size of BER encoding of Double 
     */
#define ASN_OPAQUE_DOUBLE_BER_LEN 11

    /*
     * value for Integer64 
     */
#define ASN_OPAQUE_I64 (ASN_OPAQUE_TAG2 + ASN_APP_I64)
    /*
     * max size of BER encoding of Integer64 
     */
#define ASN_OPAQUE_I64_MX_BER_LEN 11

    /*
     * value for Unsigned64 
     */
#define ASN_OPAQUE_U64 (ASN_OPAQUE_TAG2 + ASN_APP_U64)
    /*
     * max size of BER encoding of Unsigned64 
     */
#define ASN_OPAQUE_U64_MX_BER_LEN 12

#endif                          /* NETSNMP_WITH_OPAQUE_SPECIAL_TYPES */


#define ASN_PRIV_INCL_RANGE (ASN_PRIVATE | 2)
#define ASN_PRIV_EXCL_RANGE (ASN_PRIVATE | 3)
#define ASN_PRIV_DELEGATED  (ASN_PRIVATE | 5)
#define ASN_PRIV_IMPLIED_OCTET_STR  (ASN_PRIVATE | ASN_OCTET_STR)       /* 4 */
#define ASN_PRIV_IMPLIED_OBJECT_ID  (ASN_PRIVATE | ASN_OBJECT_ID)       /* 6 */
#define ASN_PRIV_RETRY      (ASN_PRIVATE | 7)   /* 199 */
#define IS_DELEGATED(x)   ((x) == ASN_PRIV_DELEGATED)


    int             asn_check_packet(u_char *, size_t);
    NETSNMP_IMPORT
    u_char         *asn_parse_int(u_char *, size_t *, u_char *, long *,
                                  size_t);
    NETSNMP_IMPORT
    u_char         *asn_build_int(u_char *, size_t *, u_char, const long *,
                                  size_t);
    NETSNMP_IMPORT
    u_char         *asn_parse_unsigned_int(u_char *, size_t *, u_char *,
                                           u_long *, size_t);
    NETSNMP_IMPORT
    u_char         *asn_build_unsigned_int(u_char *, size_t *, u_char,
                                           const u_long *, size_t);
    NETSNMP_IMPORT
    u_char         *asn_parse_string(u_char *, size_t *, u_char *,
                                     u_char *, size_t *);
    NETSNMP_IMPORT
    u_char         *asn_build_string(u_char *, size_t *, u_char,
                                     const u_char *, size_t);
    NETSNMP_IMPORT
    u_char         *asn_parse_header(u_char *, size_t *, u_char *);
    u_char         *asn_parse_sequence(u_char *, size_t *, u_char *, u_char expected_type,      /* must be this type */
                                       const char *estr);       /* error message prefix */
    NETSNMP_IMPORT
    u_char         *asn_build_header(u_char *, size_t *, u_char, size_t);
    NETSNMP_IMPORT
    u_char         *asn_build_sequence(u_char *, size_t *, u_char, size_t);
    NETSNMP_IMPORT
    u_char         *asn_parse_length(u_char *, u_long *);
    NETSNMP_IMPORT
    u_char         *asn_build_length(u_char *, size_t *, size_t);
    NETSNMP_IMPORT
    u_char         *asn_parse_objid(u_char *, size_t *, u_char *, oid *,
                                    size_t *);
    NETSNMP_IMPORT
    u_char         *asn_build_objid(u_char *, size_t *, u_char, oid *,
                                    size_t);
    NETSNMP_IMPORT
    u_char         *asn_parse_null(u_char *, size_t *, u_char *);
    NETSNMP_IMPORT
    u_char         *asn_build_null(u_char *, size_t *, u_char);
    NETSNMP_IMPORT
    u_char         *asn_parse_bitstring(u_char *, size_t *, u_char *,
                                        u_char *, size_t *);
    NETSNMP_IMPORT
    u_char         *asn_build_bitstring(u_char *, size_t *, u_char,
                                        const u_char *, size_t);
    NETSNMP_IMPORT
    u_char         *asn_parse_unsigned_int64(u_char *, size_t *, u_char *,
                                             struct counter64 *, size_t);
    NETSNMP_IMPORT
    u_char         *asn_build_unsigned_int64(u_char *, size_t *, u_char,
                                             const struct counter64 *, size_t);
    u_char         *asn_parse_signed_int64(u_char *, size_t *, u_char *,
                                           struct counter64 *, size_t);
    u_char         *asn_build_signed_int64(u_char *, size_t *, u_char,
                                           const struct counter64 *, size_t);
    u_char         *asn_build_float(u_char *, size_t *, u_char, const float *,
                                    size_t);
    u_char         *asn_parse_float(u_char *, size_t *, u_char *, float *,
                                    size_t);
    u_char         *asn_build_double(u_char *, size_t *, u_char, const double *,
                                     size_t);
    u_char         *asn_parse_double(u_char *, size_t *, u_char *,
                                     double *, size_t);

#ifdef NETSNMP_USE_REVERSE_ASNENCODING

    /*
     * Re-allocator function for below.  
     */

    int             asn_realloc(u_char **, size_t *);

    /*
     * Re-allocating reverse ASN.1 encoder functions.  Synopsis:
     * 
     * u_char *buf = (u_char*)malloc(100);
     * u_char type = (ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER);
     * size_t buf_len = 100, offset = 0;
     * long data = 12345;
     * int allow_realloc = 1;
     * 
     * if (asn_realloc_rbuild_int(&buf, &buf_len, &offset, allow_realloc,
     * type, &data, sizeof(long)) == 0) {
     * error;
     * }
     * 
     * NOTE WELL: after calling one of these functions with allow_realloc
     * non-zero, buf might have moved, buf_len might have grown and
     * offset will have increased by the size of the encoded data.
     * You should **NEVER** do something like this:
     * 
     * u_char *buf = (u_char *)malloc(100), *ptr;
     * u_char type = (ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER);
     * size_t buf_len = 100, offset = 0;
     * long data1 = 1234, data2 = 5678;
     * int rc = 0, allow_realloc = 1;
     * 
     * rc  = asn_realloc_rbuild_int(&buf, &buf_len, &offset, allow_realloc,
     * type, &data1, sizeof(long));
     * ptr = buf[buf_len - offset];   / * points at encoding of data1 * /
     * if (rc == 0) {
     * error;
     * }
     * rc  = asn_realloc_rbuild_int(&buf, &buf_len, &offset, allow_realloc,
     * type, &data2, sizeof(long));
     * make use of ptr here;
     * 
     * 
     * ptr is **INVALID** at this point.  In general, you should store the
     * offset value and compute pointers when you need them:
     * 
     * 
     * 
     * u_char *buf = (u_char *)malloc(100), *ptr;
     * u_char type = (ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER);
     * size_t buf_len = 100, offset = 0, ptr_offset;
     * long data1 = 1234, data2 = 5678;
     * int rc = 0, allow_realloc = 1;
     * 
     * rc  = asn_realloc_rbuild_int(&buf, &buf_len, &offset, allow_realloc,
     * type, &data1, sizeof(long));
     * ptr_offset = offset;
     * if (rc == 0) {
     * error;
     * }
     * rc  = asn_realloc_rbuild_int(&buf, &buf_len, &offset, allow_realloc,
     * type, &data2, sizeof(long));
     * ptr = buf + buf_len - ptr_offset
     * make use of ptr here;
     * 
     * 
     * 
     * Here, you can see that ptr will be a valid pointer even if the block of
     * memory has been moved, as it may well have been.  Plenty of examples of
     * usage all over asn1.c, snmp_api.c, snmpusm.c.
     * 
     * The other thing you should **NEVER** do is to pass a pointer to a buffer
     * on the stack as the first argument when allow_realloc is non-zero, unless
     * you really know what you are doing and your machine/compiler allows you to
     * free non-heap memory.  There are rumours that such things exist, but many
     * consider them no more than the wild tales of a fool.
     * 
     * Of course, you can pass allow_realloc as zero, to indicate that you do not
     * wish the packet buffer to be reallocated for some reason; perhaps because
     * it is on the stack.  This may be useful to emulate the functionality of
     * the old API:
     * 
     * u_char my_static_buffer[100], *cp = NULL;
     * size_t my_static_buffer_len = 100;
     * float my_pi = (float)22/(float)7;
     * 
     * cp = asn_rbuild_float(my_static_buffer, &my_static_buffer_len,
     * ASN_OPAQUE_FLOAT, &my_pi, sizeof(float));
     * if (cp == NULL) {
     * error;
     * }
     * 
     * 
     * IS EQUIVALENT TO:
     * 
     * 
     * u_char my_static_buffer[100];
     * size_t my_static_buffer_len = 100, my_offset = 0;
     * float my_pi = (float)22/(float)7;
     * int rc = 0;
     * 
     * rc = asn_realloc_rbuild_float(&my_static_buffer, &my_static_buffer_len,
     * &my_offset, 0,
     * ASN_OPAQUE_FLOAT, &my_pi, sizeof(float));
     * if (rc == 0) {
     * error;
     * }
     * 
     * 
     */


    int             asn_realloc_rbuild_int(u_char ** pkt, size_t * pkt_len,
                                           size_t * offset,
                                           int allow_realloc, u_char type,
                                           const long *data, size_t data_size);

    int             asn_realloc_rbuild_string(u_char ** pkt,
                                              size_t * pkt_len,
                                              size_t * offset,
                                              int allow_realloc,
                                              u_char type,
                                              const u_char * data,
                                              size_t data_size);

    int             asn_realloc_rbuild_unsigned_int(u_char ** pkt,
                                                    size_t * pkt_len,
                                                    size_t * offset,
                                                    int allow_realloc,
                                                    u_char type,
                                                    const u_long * data,
                                                    size_t data_size);

    int             asn_realloc_rbuild_header(u_char ** pkt,
                                              size_t * pkt_len,
                                              size_t * offset,
                                              int allow_realloc,
                                              u_char type,
                                              size_t data_size);

    int             asn_realloc_rbuild_sequence(u_char ** pkt,
                                                size_t * pkt_len,
                                                size_t * offset,
                                                int allow_realloc,
                                                u_char type,
                                                size_t data_size);

    int             asn_realloc_rbuild_length(u_char ** pkt,
                                              size_t * pkt_len,
                                              size_t * offset,
                                              int allow_realloc,
                                              size_t data_size);

    int             asn_realloc_rbuild_objid(u_char ** pkt,
                                             size_t * pkt_len,
                                             size_t * offset,
                                             int allow_realloc,
                                             u_char type, const oid *,
                                             size_t);

    int             asn_realloc_rbuild_null(u_char ** pkt,
                                            size_t * pkt_len,
                                            size_t * offset,
                                            int allow_realloc,
                                            u_char type);

    int             asn_realloc_rbuild_bitstring(u_char ** pkt,
                                                 size_t * pkt_len,
                                                 size_t * offset,
                                                 int allow_realloc,
                                                 u_char type,
                                                 const u_char * data,
                                                 size_t data_size);

    int             asn_realloc_rbuild_unsigned_int64(u_char ** pkt,
                                                      size_t * pkt_len,
                                                      size_t * offset,
                                                      int allow_realloc,
                                                      u_char type,
                                                      struct counter64
                                                      const *data, size_t);

    int             asn_realloc_rbuild_signed_int64(u_char ** pkt,
                                                    size_t * pkt_len,
                                                    size_t * offset,
                                                    int allow_realloc,
                                                    u_char type,
                                                    const struct counter64 *data,
                                                    size_t);

    int             asn_realloc_rbuild_float(u_char ** pkt,
                                             size_t * pkt_len,
                                             size_t * offset,
                                             int allow_realloc,
                                             u_char type, const float *data,
                                             size_t data_size);

    int             asn_realloc_rbuild_double(u_char ** pkt,
                                              size_t * pkt_len,
                                              size_t * offset,
                                              int allow_realloc,
                                              u_char type, const double *data,
                                              size_t data_size);
#endif

#ifdef __cplusplus
}
#endif
#endif                          /* ASN1_H */
