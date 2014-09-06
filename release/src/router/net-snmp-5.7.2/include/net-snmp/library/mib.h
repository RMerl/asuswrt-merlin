#ifndef MIB_H
#define MIB_H

#ifdef __cplusplus
extern          "C" {
#endif
    /*
     * mib.h - Definitions for the variables as defined in the MIB
     *
     * Update: 1998-07-17 <jhy@gsu.edu>
     * Added prototypes for print_oid_report* functions.
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

#include <stdio.h>              /* for FILE */

#include <net-snmp/mib_api.h>

#define NETSNMP_MIB2_OID 1, 3, 6, 1, 2, 1

#ifndef NETSNMP_NO_LEGACY_DEFINITIONS
#define MIB NETSNMP_MIB2_OID
#endif

#define MIB_IFTYPE_OTHER		    1
#define MIB_IFTYPE_REGULAR1822		    2
#define MIB_IFTYPE_HDH1822		    3
#define MIB_IFTYPE_DDNX25		    4
#define MIB_IFTYPE_RFC877X25		    5
#define MIB_IFTYPE_ETHERNETCSMACD	    6
#define MIB_IFTYPE_ISO88023CSMACD	    7
#define MIB_IFTYPE_ISO88024TOKENBUS	    8
#define MIB_IFTYPE_ISO88025TOKENRING	    9
#define MIB_IFTYPE_ISO88026MAN		    10
#define MIB_IFTYPE_STARLAN		    11
#define MIB_IFTYPE_PROTEON10MBIT	    12
#define MIB_IFTYPE_PROTEON80MBIT	    13
#define MIB_IFTYPE_HYPERCHANNEL		    14
#define MIB_IFTYPE_FDDI			    15
#define MIB_IFTYPE_LAPB			    16
#define MIB_IFTYPE_SDLC			    17
#define MIB_IFTYPE_T1CARRIER		    18
#define MIB_IFTYPE_CEPT			    19
#define MIB_IFTYPE_BASICISDN		    20
#define MIB_IFTYPE_PRIMARYISDN		    21
#define MIB_IFTYPE_PROPPOINTTOPOINTSERIAL   22

#define MIB_IFSTATUS_UP		1
#define MIB_IFSTATUS_DOWN	2
#define MIB_IFSTATUS_TESTING	3

#define MIB_FORWARD_GATEWAY	1
#define MIB_FORWARD_HOST	2

#define MIB_IPROUTETYPE_OTHER	1
#define MIB_IPROUTETYPE_INVALID	2
#define MIB_IPROUTETYPE_DIRECT	3
#define MIB_IPROUTETYPE_REMOTE	4

#define MIB_IPROUTEPROTO_OTHER	    1
#define MIB_IPROUTEPROTO_LOCAL	    2
#define MIB_IPROUTEPROTO_NETMGMT    3
#define MIB_IPROUTEPROTO_ICMP	    4
#define MIB_IPROUTEPROTO_EGP	    5
#define MIB_IPROUTEPROTO_GGP	    6
#define MIB_IPROUTEPROTO_HELLO	    7
#define MIB_IPROUTEPROTO_RIP	    8
#define MIB_IPROUTEPROTO_ISIS	    9
#define MIB_IPROUTEPROTO_ESIS	    10
#define MIB_IPROUTEPROTO_CISCOIGRP  11
#define MIB_IPROUTEPROTO_BBNSPFIGP  12
#define MIB_IPROUTEPROTO_OIGP	    13

#define MIB_TCPRTOALG_OTHER	1
#define MIB_TCPRTOALG_CONSTANT	2
#define MIB_TCPRTOALG_RSRE	3
#define MIB_TCPRTOALG_VANJ	4

#define MIB_TCPCONNSTATE_CLOSED		1
#define MIB_TCPCONNSTATE_LISTEN		2
#define MIB_TCPCONNSTATE_SYNSENT	3
#define MIB_TCPCONNSTATE_SYNRECEIVED	4
#define MIB_TCPCONNSTATE_ESTABLISHED	5
#define MIB_TCPCONNSTATE_FINWAIT1	6
#define MIB_TCPCONNSTATE_FINWAIT2	7
#define MIB_TCPCONNSTATE_CLOSEWAIT	8
#define MIB_TCPCONNSTATE_LASTACK	9
#define MIB_TCPCONNSTATE_CLOSING	10
#define MIB_TCPCONNSTATE_TIMEWAIT	11

#define MIB_EGPNEIGHSTATE_IDLE		1
#define MIB_EGPNEIGHSTATE_AQUISITION	2
#define MIB_EGPNEIGHSTATE_DOWN		3
#define MIB_EGPNEIGHSTATE_UP		4
#define MIB_EGPNEIGHSTATE_CEASE		5

    struct variable_list;
    struct enum_list;

    NETSNMP_IMPORT
    void            print_ascii_dump(FILE *);
    void            register_mib_handlers(void);
    void            netsnmp_set_mib_directory(const char *dir);
    NETSNMP_IMPORT
    char            *netsnmp_get_mib_directory(void);
    void            netsnmp_fixup_mib_directory(void);
    void            netsnmp_mibindex_load( void );
    char *          netsnmp_mibindex_lookup( const char * );
    FILE *          netsnmp_mibindex_new( const char * );
    int             sprint_realloc_description(u_char ** buf, size_t * buf_len,
                                size_t * out_len, int allow_realloc,
                                oid * objid, size_t objidlen, int width);
    NETSNMP_IMPORT
    int             get_wild_node(const char *, oid *, size_t *);
    NETSNMP_IMPORT
    int             get_node(const char *, oid *, size_t *);
    NETSNMP_IMPORT
    struct tree    *get_tree(const oid *, size_t, struct tree *);
    NETSNMP_IMPORT
    struct tree    *get_tree_head(void);
    void            set_function(struct tree *);

    NETSNMP_IMPORT
    int             parse_one_oid_index(oid ** oidStart, size_t * oidLen,
                                        netsnmp_variable_list * data,
                                        int complete);
    NETSNMP_IMPORT
    int             parse_oid_indexes(oid * oidIndex, size_t oidLen,
                                      netsnmp_variable_list * data);
    NETSNMP_IMPORT
    int             build_oid_noalloc(oid * in, size_t in_len,
                                      size_t * out_len, oid * prefix,
                                      size_t prefix_len,
                                      netsnmp_variable_list * indexes);
    NETSNMP_IMPORT
    int             build_oid(oid ** out, size_t * out_len, oid * prefix,
                              size_t prefix_len,
                              netsnmp_variable_list * indexes);
    int             build_oid_segment(netsnmp_variable_list * var);

    NETSNMP_IMPORT
    int             sprint_realloc_variable(u_char ** buf, size_t * buf_len,
                                   size_t * out_len, int allow_realloc,
                                   const oid * objid, size_t objidlen,
                                   const netsnmp_variable_list * variable);

    NETSNMP_IMPORT
#ifndef NETSNMP_DISABLE_MIB_LOADING
    struct tree    *
#else
    void
#endif
                    netsnmp_sprint_realloc_objid_tree(u_char ** buf,
                                                      size_t * buf_len,
                                                      size_t * out_len,
                                                      int allow_realloc,
                                                      int *buf_overflow,
                                                      const oid * objid,
                                                      size_t objidlen);

    NETSNMP_IMPORT
    void
                    netsnmp_sprint_realloc_objid(u_char ** buf,
                                                 size_t * buf_len,
                                                 size_t * out_len,
                                                 int allow_realloc,
                                                 int *buf_overflow,
                                                 const oid * objid,
                                                 size_t objidlen);

    NETSNMP_IMPORT
    int             sprint_realloc_value(u_char ** buf, size_t * buf_len,
                                 size_t * out_len, int allow_realloc,
                                 const oid * objid, size_t objidlen,
                                 const netsnmp_variable_list * variable);

    NETSNMP_IMPORT
    int             sprint_realloc_objid(u_char ** buf, size_t * buf_len,
                                 size_t * out_len, int allow_realloc,
                                 const oid * objid, size_t objidlen);

    NETSNMP_IMPORT
    int             sprint_realloc_by_type(u_char ** buf, size_t * buf_len,
                                           size_t * out_len,
                                           int allow_realloc,
                                           const netsnmp_variable_list * var,
                                           const struct enum_list *enums,
                                           const char *hint,
                                           const char *units);

    NETSNMP_IMPORT
    int             sprint_realloc_hexstring(u_char ** buf,
                                             size_t * buf_len,
                                             size_t * out_len,
                                             int allow_realloc,
                                             const u_char *, size_t);

    NETSNMP_IMPORT
    int             sprint_realloc_asciistring(u_char ** buf,
                                               size_t * buf_len,
                                               size_t * out_len,
                                               int allow_realloc,
                                               const u_char * cp,
                                               size_t len);

    int             sprint_realloc_octet_string(u_char ** buf,
                                                size_t * buf_len,
                                                size_t * out_len,
                                                int allow_realloc,
                                                const netsnmp_variable_list *,
                                                const struct enum_list *,
                                                const char *,
                                                const char *);

    int             sprint_realloc_opaque(u_char ** buf, size_t * buf_len,
                                          size_t * out_len,
                                          int allow_realloc,
                                          const netsnmp_variable_list *,
                                          const struct enum_list *, const char *,
                                          const char *);

    int             sprint_realloc_object_identifier(u_char ** buf,
                                                     size_t * buf_len,
                                                     size_t * out_len,
                                                     int allow_realloc,
                                                     const netsnmp_variable_list
                                                     *, const struct enum_list *,
                                                     const char *,
                                                     const char *);

    int             sprint_realloc_timeticks(u_char ** buf,
                                             size_t * buf_len,
                                             size_t * out_len,
                                             int allow_realloc,
                                             const netsnmp_variable_list *,
                                             const struct enum_list *,
                                             const char *, const char *);

    int             sprint_realloc_hinted_integer(u_char ** buf,
                                                  size_t * buf_len,
                                                  size_t * out_len,
                                                  int allow_realloc, long,
                                                  const char, const char *,
                                                  const char *);

    int             sprint_realloc_integer(u_char ** buf, size_t * buf_len,
                                           size_t * out_len,
                                           int allow_realloc,
                                           const netsnmp_variable_list *,
                                           const struct enum_list *,
                                           const char *, const char *);

    int             sprint_realloc_uinteger(u_char ** buf,
                                            size_t * buf_len,
                                            size_t * out_len,
                                            int allow_realloc,
                                            const netsnmp_variable_list *,
                                            const struct enum_list *,
                                            const char *, const char *);

    int             sprint_realloc_gauge(u_char ** buf, size_t * buf_len,
                                         size_t * out_len,
                                         int allow_realloc,
                                         const netsnmp_variable_list *,
                                         const struct enum_list *, const char *,
                                         const char *);

    int             sprint_realloc_counter(u_char ** buf, size_t * buf_len,
                                           size_t * out_len,
                                           int allow_realloc,
                                           const netsnmp_variable_list *,
                                           const struct enum_list *,
                                           const char *, const char *);

    int             sprint_realloc_networkaddress(u_char ** buf,
                                                  size_t * buf_len,
                                                  size_t * out_len,
                                                  int allow_realloc,
                                                  const netsnmp_variable_list *,
                                                  const struct enum_list *,
                                                  const char *,
                                                  const char *);

    int             sprint_realloc_ipaddress(u_char ** buf,
                                             size_t * buf_len,
                                             size_t * out_len,
                                             int allow_realloc,
                                             const netsnmp_variable_list *,
                                             const struct enum_list *,
                                             const char *, const char *);

    int             sprint_realloc_null(u_char ** buf, size_t * buf_len,
                                        size_t * out_len,
                                        int allow_realloc,
                                        const netsnmp_variable_list *,
                                        const struct enum_list *, const char *,
                                        const char *);

    int             sprint_realloc_bitstring(u_char ** buf,
                                             size_t * buf_len,
                                             size_t * out_len,
                                             int allow_realloc,
                                             const netsnmp_variable_list *,
                                             const struct enum_list *,
                                             const char *, const char *);

    int             sprint_realloc_nsapaddress(u_char ** buf,
                                               size_t * buf_len,
                                               size_t * out_len,
                                               int allow_realloc,
                                               const netsnmp_variable_list *,
                                               const struct enum_list *,
                                               const char *, const char *);

    int             sprint_realloc_counter64(u_char ** buf,
                                             size_t * buf_len,
                                             size_t * out_len,
                                             int allow_realloc,
                                             const netsnmp_variable_list *,
                                             const struct enum_list *,
                                             const char *, const char *);

    int             sprint_realloc_badtype(u_char ** buf, size_t * buf_len,
                                           size_t * out_len,
                                           int allow_realloc,
                                           const netsnmp_variable_list *,
                                           const struct enum_list *,
                                           const char *, const char *);

#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
    int             sprint_realloc_float(u_char ** buf, size_t * buf_len,
                                         size_t * out_len,
                                         int allow_realloc,
                                         const netsnmp_variable_list *,
                                         const struct enum_list *, const char *,
                                         const char *);

    int             sprint_realloc_double(u_char ** buf, size_t * buf_len,
                                          size_t * out_len,
                                          int allow_realloc,
                                          const netsnmp_variable_list *,
                                          const struct enum_list *, const char *,
                                          const char *);
#endif


    int             snprint_by_type(char *buf, size_t buf_len,
                                    netsnmp_variable_list * var,
                                    const struct enum_list *enums,
                                    const char *hint, const char *units);

    int             snprint_hexstring(char *buf, size_t buf_len,
                                      const u_char *, size_t);

    int             snprint_asciistring(char *buf, size_t buf_len,
                                        const u_char * cp, size_t len);

    int             snprint_octet_string(char *buf, size_t buf_len,
                                         const netsnmp_variable_list *,
                                         const struct enum_list *, const char *,
                                         const char *);

    int             snprint_opaque(char *buf, size_t buf_len,
                                   const netsnmp_variable_list *,
                                   const struct enum_list *, const char *,
                                   const char *);

    int             snprint_object_identifier(char *buf, size_t buf_len,
                                              const netsnmp_variable_list *,
                                              const struct enum_list *,
                                              const char *, const char *);

    int             snprint_timeticks(char *buf, size_t buf_len,
                                      const netsnmp_variable_list *,
                                      const struct enum_list *, const char *,
                                      const char *);

    int             snprint_hinted_integer(char *buf, size_t buf_len,
                                           long, const char *,
                                           const char *);

    int             snprint_integer(char *buf, size_t buf_len,
                                    const netsnmp_variable_list *,
                                    const struct enum_list *, const char *,
                                    const char *);

    int             snprint_uinteger(char *buf, size_t buf_len,
                                     const netsnmp_variable_list *,
                                     const struct enum_list *, const char *,
                                     const char *);

    int             snprint_gauge(char *buf, size_t buf_len,
                                  const netsnmp_variable_list *,
                                  const struct enum_list *, const char *,
                                  const char *);

    int             snprint_counter(char *buf, size_t buf_len,
                                    const netsnmp_variable_list *,
                                    const struct enum_list *, const char *,
                                    const char *);

    int             snprint_networkaddress(char *buf, size_t buf_len,
                                           const netsnmp_variable_list *,
                                           const struct enum_list *,
                                           const char *, const char *);

    int             snprint_ipaddress(char *buf, size_t buf_len,
                                      const netsnmp_variable_list *,
                                      const struct enum_list *, const char *,
                                      const char *);

    int             snprint_null(char *buf, size_t buf_len,
                                 const netsnmp_variable_list *,
                                 const struct enum_list *, const char *,
                                 const char *);

    NETSNMP_IMPORT
    int             snprint_bitstring(char *buf, size_t buf_len,
                                      const netsnmp_variable_list *,
                                      const struct enum_list *, const char *,
                                      const char *);

    int             snprint_nsapaddress(char *buf, size_t buf_len,
                                        const netsnmp_variable_list *,
                                        const struct enum_list *, const char *,
                                        const char *);

    int             snprint_counter64(char *buf, size_t buf_len,
                                      const netsnmp_variable_list *,
                                      const struct enum_list *, const char *,
                                      const char *);

    int             snprint_badtype(char *buf, size_t buf_len,
                                    const netsnmp_variable_list *,
                                    const struct enum_list *, const char *,
                                    const char *);

#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
    int             snprint_float(char *buf, size_t buf_len,
                                  const netsnmp_variable_list *,
                                  const struct enum_list *, const char *,
                                  const char *);

    int             snprint_double(char *buf, size_t buf_len,
                                   const netsnmp_variable_list *,
                                   const struct enum_list *, const char *,
                                   const char *);
#endif

    NETSNMP_IMPORT
    void            print_oid_report(FILE *);
    NETSNMP_IMPORT
    void            print_oid_report_enable_labeledoid(void);
    NETSNMP_IMPORT
    void            print_oid_report_enable_oid(void);
    NETSNMP_IMPORT
    void            print_oid_report_enable_suffix(void);
    NETSNMP_IMPORT
    void            print_oid_report_enable_symbolic(void);
    NETSNMP_IMPORT
    void            print_oid_report_enable_mibchildoid(void);

    const char     *parse_octet_hint(const char *hint, const char *value,
	                             unsigned char **new_val, int *new_val_len);

    NETSNMP_IMPORT
    void            clear_tree_flags(register struct tree *tp);

    NETSNMP_IMPORT
    char           *snmp_out_toggle_options(char *);
    NETSNMP_IMPORT
    void            snmp_out_toggle_options_usage(const char *, FILE *);
    NETSNMP_IMPORT
    char           *snmp_in_toggle_options(char *);
    char           *snmp_in_options(char *, int, char * const *);
    NETSNMP_IMPORT
    void            snmp_in_toggle_options_usage(const char *, FILE *);
    NETSNMP_IMPORT
    u_char          mib_to_asn_type(int mib_type);


    int             netsnmp_str2oid(const char *S, oid * O, int L);


#define NETSNMP_STRING_OUTPUT_GUESS  1
#define NETSNMP_STRING_OUTPUT_ASCII  2
#define NETSNMP_STRING_OUTPUT_HEX    3

#define NETSNMP_OID_OUTPUT_SUFFIX  1
#define NETSNMP_OID_OUTPUT_MODULE  2
#define NETSNMP_OID_OUTPUT_FULL    3
#define NETSNMP_OID_OUTPUT_NUMERIC 4
#define NETSNMP_OID_OUTPUT_UCD     5
#define NETSNMP_OID_OUTPUT_NONE    6
#ifdef __cplusplus
}
#endif
#endif                          /* MIB_H */
