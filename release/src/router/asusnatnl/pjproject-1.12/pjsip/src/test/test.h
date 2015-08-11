/* $Id: test.h 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef __TEST_H__
#define __TEST_H__

#include <pjsip/sip_types.h>

extern pjsip_endpoint *endpt;

#define TEST_UDP_PORT	    15060
#define TEST_UDP_PORT_STR   "15060"

/**
 * Memory size to use in caching pool.
 * Default: 2MB
 */
#ifndef PJSIP_TEST_MEM_SIZE
#  define PJSIP_TEST_MEM_SIZE	    (2*1024*1024)
#endif



#define INCLUDE_MESSAGING_GROUP	    1
#define INCLUDE_TRANSPORT_GROUP	    1
#define INCLUDE_TSX_GROUP	    1
#define INCLUDE_INV_GROUP	    1
#define INCLUDE_REGC_GROUP	    1

#define INCLUDE_BENCHMARKS	    1

/*
 * Include tests that normally would fail under certain gcc
 * optimization levels.
 */
#ifndef INCLUDE_GCC_TEST
#   define INCLUDE_GCC_TEST	    0
#endif


#define INCLUDE_URI_TEST	INCLUDE_MESSAGING_GROUP
#define INCLUDE_MSG_TEST	INCLUDE_MESSAGING_GROUP
#define INCLUDE_MULTIPART_TEST	INCLUDE_MESSAGING_GROUP
#define INCLUDE_TXDATA_TEST	INCLUDE_MESSAGING_GROUP
#define INCLUDE_TSX_BENCH	INCLUDE_MESSAGING_GROUP
#define INCLUDE_UDP_TEST	INCLUDE_TRANSPORT_GROUP
#define INCLUDE_LOOP_TEST	INCLUDE_TRANSPORT_GROUP
#define INCLUDE_TCP_TEST	INCLUDE_TRANSPORT_GROUP
#define INCLUDE_RESOLVE_TEST	INCLUDE_TRANSPORT_GROUP
#define INCLUDE_TSX_TEST	INCLUDE_TSX_GROUP
#define INCLUDE_INV_OA_TEST	INCLUDE_INV_GROUP
#define INCLUDE_REGC_TEST	INCLUDE_REGC_GROUP


/* The tests */
int uri_test(void);
int msg_test(void);
int msg_err_test(void);
int multipart_test(void);
int txdata_test(void);
int tsx_bench(void);
int transport_udp_test(void);
int transport_loop_test(void);
int transport_tcp_test(void);
int resolve_test(void);
int regc_test(void);

struct tsx_test_param
{
    int type;
    int port;
    char *tp_type;
};

int tsx_basic_test(struct tsx_test_param *param);
int tsx_uac_test(struct tsx_test_param *param);
int tsx_uas_test(struct tsx_test_param *param);

/* Transport test helpers (transport_test.c). */
int generic_transport_test(pjsip_transport *tp);
int transport_send_recv_test( pjsip_transport_type_e tp_type,
			      pjsip_transport *ref_tp,
			      char *target_url,
			      int *p_usec_rtt);
int transport_rt_test( pjsip_transport_type_e tp_type,
		       pjsip_transport *ref_tp,
		       char *target_url,
		       int *pkt_lost);
int transport_load_test(char *target_url);

/* Invite session */
int inv_offer_answer_test(void);

/* Test main entry */
int  test_main(void);

/* Test utilities. */
void app_perror(const char *msg, pj_status_t status);
int  init_msg_logger(void);
int  msg_logger_set_enabled(pj_bool_t enabled);
void flush_events(unsigned duration);


void report_ival(const char *name, int value, const char *valname, const char *desc);
void report_sval(const char *name, const char* value, const char *valname, const char *desc);


/* Settings. */
extern int log_level;

#endif	/* __TEST_H__ */
