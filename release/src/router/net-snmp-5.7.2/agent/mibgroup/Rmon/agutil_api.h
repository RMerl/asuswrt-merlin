/**************************************************************
 * Copyright (C) 2001 Alex Rozin, Optical Access 
 *
 *                     All Rights Reserved
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 * 
 * ALEX ROZIN DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * ALEX ROZIN BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 ******************************************************************/

#ifndef _agutil_api_h_included__
#define _agutil_api_h_included__

#include <string.h>

#if 0                           /* for debug */
#warning MEMORY DEBUG VERSION
void           *dbg_f_AGMALLOC(size_t size);
void            dbg_f_AGFREE(void *ptr);
char           *dbg_f_AGSTRDUP(const char *s);
void            dbg_f_AG_MEM_REPORT(void);
#  define AGMALLOC(X)	dbg_f_AGMALLOC(X)
#  define AGFREE(X)       { dbg_f_AGFREE(X); X = NULL; }
#  define AGSTRDUP(X)     dbg_f_AGSTRDUP(X)
#else
#  define AGMALLOC(X)	malloc(X)
#  define AGFREE(X)	{ free(X); X = NULL; }
#  define AGSTRDUP(X)	strdup(X)
#endif

typedef struct {
    size_t          length;
    oid             objid[MAX_OID_LEN];
} VAR_OID_T;

void            ag_trace(const char *format, ...);

int             AGUTIL_advance_index_name(struct variable *vp, oid * name,
                                          size_t * length, int exact);
int             AGUTIL_get_int_value(u_char * var_val, u_char var_val_type,
                                     size_t var_val_len, long min_value,
                                     long max_value, long *long_tmp);
int             AGUTIL_get_string_value(u_char * var_val,
                                        u_char var_val_type,
                                        size_t var_val_len,
                                        size_t buffer_max_size,
                                        u_char should_zero_limited,
                                        size_t * buffer_actual_size,
                                        char *buffer);
int             AGUTIL_get_oid_value(u_char * var_val, u_char var_val_type,
                                     size_t var_val_len,
                                     VAR_OID_T * data_source_ptr);

u_long          AGUTIL_sys_up_time(void);

#if OPTICALL_ACESS
#define ETH_STATS_T UID_PORT_STATISTICS_T
#else
typedef struct {
    u_int           ifIndex;
    u_long          octets;
    u_long          packets;
    u_long          bcast_pkts;
    u_long          mcast_pkts;
    u_long          crc_align;
    u_long          undersize;
    u_long          oversize;
    u_long          fragments;
    u_long          jabbers;
    u_long          collisions;
    u_long          pkts_64;
    u_long          pkts_65_127;
    u_long          pkts_128_255;
    u_long          pkts_256_511;
    u_long          pkts_512_1023;
    u_long          pkts_1024_1518;
} ETH_STATS_T;
#endif

void            SYSTEM_get_eth_statistics(VAR_OID_T * data_source,
                                          ETH_STATS_T * where);

#endif                          /* _agutil_api_h_included__ */
