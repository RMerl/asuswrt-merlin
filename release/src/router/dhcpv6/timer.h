/*	$KAME: timer.h,v 1.1 2002/05/16 06:04:08 jinmei Exp $	*/

/*
 * Copyright (C) 2002 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* a < b */
#define TIMEVAL_LT(a, b) (((a).tv_sec < (b).tv_sec) ||\
			  (((a).tv_sec == (b).tv_sec) && \
			    ((a).tv_usec < (b).tv_usec)))
/* a <= b */
#define TIMEVAL_LEQ(a, b) (((a).tv_sec < (b).tv_sec) ||\
			   (((a).tv_sec == (b).tv_sec) &&\
 			    ((a).tv_usec <= (b).tv_usec)))
/* a == b */
#define TIMEVAL_EQUAL(a, b) ((a).tv_sec == (b).tv_sec &&\
			     (a).tv_usec == (b).tv_usec)

struct dhcp6_timer {
	LIST_ENTRY(dhcp6_timer) link;

	struct timeval tm;

	struct dhcp6_timer *(*expire) __P((void *));
	void *expire_data;
};

void dhcp6_timer_init __P((void));
struct dhcp6_timer *dhcp6_add_timer __P((struct dhcp6_timer *(*) __P((void *)),
					 void *));
void dhcp6_set_timer __P((struct timeval *, struct dhcp6_timer *));
void dhcp6_remove_timer __P((struct dhcp6_timer **));
struct timeval * dhcp6_check_timer __P((void));
struct timeval * dhcp6_timer_rest __P((struct dhcp6_timer *));

void timeval_sub __P((struct timeval *, struct timeval *,
			     struct timeval *));
