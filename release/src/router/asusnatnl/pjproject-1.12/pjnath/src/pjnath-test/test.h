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
#include <pjlib.h>
#include <pjlib-util.h>
#include <pjnath.h>

#define INCLUDE_STUN_TEST	    1
#define INCLUDE_ICE_TEST	    1
#define INCLUDE_STUN_SOCK_TEST	    1
#define INCLUDE_TURN_SOCK_TEST	    1

int stun_test(void);
int sess_auth_test(void);
int stun_sock_test(void);
int turn_sock_test(void);
int ice_test(void);
int test_main(void);

extern void app_perror(const char *title, pj_status_t rc);
extern pj_pool_factory *mem;

////////////////////////////////////
/*
 * Utilities
 */
pj_status_t create_stun_config(pj_pool_t *pool, pj_stun_config *stun_cfg);
void destroy_stun_config(pj_stun_config *stun_cfg);

void poll_events(pj_stun_config *stun_cfg, unsigned msec,
		 pj_bool_t first_event_only);

typedef struct pjlib_state
{
    unsigned	timer_cnt;	/* Number of timer entries */
    unsigned	pool_used_cnt;	/* Number of app pools	    */
} pjlib_state;


void capture_pjlib_state(pj_stun_config *cfg, struct pjlib_state *st);
int check_pjlib_state(pj_stun_config *cfg, 
		      const struct pjlib_state *initial_st);


#define ERR_MEMORY_LEAK	    1
#define ERR_TIMER_LEAK	    2

