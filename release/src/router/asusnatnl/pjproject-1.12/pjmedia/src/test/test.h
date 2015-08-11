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
#ifndef __PJMEDIA_TEST_H__
#define __PJMEDIA_TEST_H__

#include <pjmedia.h>
#include <pjlib.h>

#define HAS_SDP_NEG_TEST	1
#define HAS_JBUF_TEST		1
#define HAS_MIPS_TEST		1
#define HAS_CODEC_VECTOR_TEST	1

int session_test(void);
int rtp_test(void);
int sdp_test(void);
int jbuf_main(void);
int sdp_neg_test(void);
int mips_test(void);
int codec_test_vectors(void);

extern pj_pool_factory *mem;
void app_perror(pj_status_t status, const char *title);

int test_main(void);

#endif	/* __PJMEDIA_TEST_H__ */
