/*
 * Ethernet Switch IGMP Snooper
 * Copyright (C) 2014 ASUSTeK Inc.
 * All Rights Reserved.

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <time.h>
#include <sys/time.h>
#include <sys/syslog.h>
#include <netinet/in.h>
#include <netinet/ether.h>

#include "queue.h"

/* snooper */
extern unsigned char ifhwaddr[ETHER_ADDR_LEN];
extern in_addr_t ifaddr;
int listen_query(in_addr_t group, int timeout);
int send_query(in_addr_t group);
int switch_probe(void);

/* igmp */
int accept_igmp(unsigned char *packet, int size, unsigned char *shost, int loopback);
int build_query(unsigned char *packet, int size, in_addr_t group, in_addr_t dst);
int init_querier(int enabled);

/* timer */
#ifdef CLOCK_MONOTONIC
#define TIMER_HZ 1000
#else
#define TIMER_HZ timer_tps
extern long timer_tps;
#endif

#define typecheck(type,x) \
({	type __dummy; \
	typeof(x) __dummy2; \
	(void)(&__dummy == &__dummy2); \
	1; \
})
#define time_after(a, b) \
	(typecheck(unsigned long, a) && \
	 typecheck(unsigned long, b) && \
	 ((long)((b) - (a)) < 0))
#define time_before(a, b) time_after(b, a)
#define time_after_eq(a, b) \
	(typecheck(unsigned long, a) && \
	 typecheck(unsigned long, b) && \
	 ((long)((a) - (b)) >= 0))
#define time_before_eq(a,b) time_after_eq(b, a)
#define time_diff(a, b) \
	(typecheck(unsigned long, a) && \
	 typecheck(unsigned long, b) ? \
	 (long)((a) - (b)) : 0)

struct timer_head;
struct timer_entry {
	TAILQ_ENTRY(timer_entry) link;
	unsigned long expires;
	void (*func)(struct timer_entry *timer, void *data);
	void *data;
};

unsigned long now(void);
void time_to_timeval(unsigned long time, struct timeval *tv);
unsigned long timeval_to_time(struct timeval *tv);
int init_timers(void);
void set_timer(struct timer_entry *timer, void (*func)(struct timer_entry *timer, void *data), void *data);
int mod_timer(struct timer_entry *timer, unsigned long expires);
int del_timer(struct timer_entry *timer);
int timer_pending(struct timer_entry *timer);
int next_timer(struct timeval *tv);
int run_timers(void);
void purge_timers(void);

/* cache */
int init_cache(void);
int get_port(unsigned char *haddr);
int add_member(unsigned char *maddr, in_addr_t addr, int port, int timeout);
int del_member(unsigned char *maddr, in_addr_t addr, int port, int timeout);
int add_router(in_addr_t addr, int port, int timeout);
int expire_members(unsigned char *maddr, int timeout);
int purge_cache(void);

/* utils */
#define FMT_TIME "%lu:%02lu:%02lu.%03lu"
#define ARG_TIMEVAL(a) (a)->tv_sec / 3600, (a)->tv_sec % 3600 / 60, \
    (a)->tv_sec % 60, (a)->tv_usec / 1000
#define FMT_IP "%d.%d.%d.%d"
#define ARG_IP(a) \
    ((unsigned char *) (a))[0], ((unsigned char *) (a))[1], \
    ((unsigned char *) (a))[2], ((unsigned char *) (a))[3]
#define FMT_EA "%02x:%02x:%02x:%02x:%02x:%02x"
#define ARG_EA(a) \
    ((unsigned char *) (a))[0], ((unsigned char *) (a))[1], \
    ((unsigned char *) (a))[2], ((unsigned char *) (a))[3], \
    ((unsigned char *) (a))[4], ((unsigned char *) (a))[5]
#define FMT_PORT "%c"
#define ARG_PORT(a, n) (((a) < 0) ? '?' : ((a) & (1 << n)) ? '0' + n : '.')
#define FMT_PORTS "%c%c%c%c%c%c%c%c%c"
#define ARG_PORTS(a) ARG_PORT((a), 0), \
    ARG_PORT((a), 1), ARG_PORT((a), 2), ARG_PORT((a), 3), ARG_PORT((a), 4), \
    ARG_PORT((a), 5), ARG_PORT((a), 6), ARG_PORT((a), 7), ARG_PORT((a), 8)

int open_socket(int domain, int type, int protocol);
void ether_mtoe(in_addr_t addr, unsigned char *ea);
unsigned int ether_hash(unsigned char *ea);
int logger(int level, char *fmt, ...);
#define log_info(fmt, args...) logger(LOG_INFO, fmt, ##args)
#define log_error(fmt, args...) logger(LOG_ERR, fmt, ##args)
#ifdef DEBUG
#define log_debug(fmt, args...) logger(LOG_DEBUG, fmt, ##args)
#else
#define log_debug(...) do {} while(0);
#endif

/* switch */
#define PORT_MAX 8

int switch_init(char *ifname, int vid, int cputrap);
void switch_done(void);
int switch_get_port(unsigned char *haddr);
int switch_add_portmap(unsigned char *maddr, int portmap);
int switch_del_portmap(unsigned char *maddr, int portmap);
int switch_clr_portmap(unsigned char *maddr);
int switch_set_floodmap(unsigned char *raddr, int portmap);
int switch_clr_floodmap(unsigned char *raddr);
