#ifndef __UTILS_H__
#define __UTILS_H__ 1

#include <asm/types.h>
#include <resolv.h>
#include <stdlib.h>

#include "libnetlink.h"
#include "ll_map.h"
#include "rtm_map.h"

extern int preferred_family;
extern int show_stats;
extern int show_details;
extern int show_raw;
extern int resolve_hosts;
extern int oneline;
extern int timestamp;
extern char * _SL_;
extern int max_flush_loops;

#ifndef IPPROTO_ESP
#define IPPROTO_ESP	50
#endif
#ifndef IPPROTO_AH
#define IPPROTO_AH	51
#endif
#ifndef IPPROTO_COMP
#define IPPROTO_COMP	108
#endif
#ifndef IPSEC_PROTO_ANY
#define IPSEC_PROTO_ANY	255
#endif

#define SPRINT_BSIZE 64
#define SPRINT_BUF(x)	char x[SPRINT_BSIZE]

extern void incomplete_command(void) __attribute__((noreturn));

#define NEXT_ARG() do { argv++; if (--argc <= 0) incomplete_command(); } while(0)
#define NEXT_ARG_OK() (argc - 1 > 0)
#define PREV_ARG() do { argv--; argc++; } while(0)

typedef struct
{
	__u8 family;
	__u8 bytelen;
	__s16 bitlen;
	__u32 flags;
	__u32 data[8];
} inet_prefix;

#define PREFIXLEN_SPECIFIED 1

#define DN_MAXADDL 20
#ifndef AF_DECnet
#define AF_DECnet 12
#endif

struct dn_naddr
{
        unsigned short          a_len;
        unsigned char a_addr[DN_MAXADDL];
};

#define IPX_NODE_LEN 6

struct ipx_addr {
	u_int32_t ipx_net;
	u_int8_t  ipx_node[IPX_NODE_LEN];
};

extern __u32 get_addr32(const char *name);
extern int get_addr_1(inet_prefix *dst, const char *arg, int family);
extern int get_prefix_1(inet_prefix *dst, char *arg, int family);
extern int get_addr(inet_prefix *dst, const char *arg, int family);
extern int get_prefix(inet_prefix *dst, char *arg, int family);
extern int mask2bits(__u32 netmask);

extern int get_integer(int *val, const char *arg, int base);
extern int get_unsigned(unsigned *val, const char *arg, int base);
extern int get_time_rtt(unsigned *val, const char *arg, int *raw);
#define get_byte get_u8
#define get_ushort get_u16
#define get_short get_s16
extern int get_u64(__u64 *val, const char *arg, int base);
extern int get_u32(__u32 *val, const char *arg, int base);
extern int get_u16(__u16 *val, const char *arg, int base);
extern int get_s16(__s16 *val, const char *arg, int base);
extern int get_u8(__u8 *val, const char *arg, int base);
extern int get_s8(__s8 *val, const char *arg, int base);

extern char* hexstring_n2a(const __u8 *str, int len, char *buf, int blen);
extern __u8* hexstring_a2n(const char *str, __u8 *buf, int blen);

extern const char *format_host(int af, int len, const void *addr,
			       char *buf, int buflen);
extern const char *rt_addr_n2a(int af, int len, const void *addr,
			       char *buf, int buflen);

void missarg(const char *) __attribute__((noreturn));
void invarg(const char *, const char *) __attribute__((noreturn));
void duparg(const char *, const char *) __attribute__((noreturn));
void duparg2(const char *, const char *) __attribute__((noreturn));
int matches(const char *arg, const char *pattern);
extern int inet_addr_match(const inet_prefix *a, const inet_prefix *b, int bits);

const char *dnet_ntop(int af, const void *addr, char *str, size_t len);
int dnet_pton(int af, const char *src, void *addr);

const char *ipx_ntop(int af, const void *addr, char *str, size_t len);
int ipx_pton(int af, const char *src, void *addr);

extern int __iproute2_hz_internal;
extern int __get_hz(void);

static __inline__ int get_hz(void)
{
	if (__iproute2_hz_internal == 0)
		__iproute2_hz_internal = __get_hz();
	return __iproute2_hz_internal;
}

extern int __iproute2_user_hz_internal;
extern int __get_user_hz(void);

static __inline__ int get_user_hz(void)
{
	if (__iproute2_user_hz_internal == 0)
		__iproute2_user_hz_internal = __get_user_hz();
	return __iproute2_user_hz_internal;
}

static inline __u32 nl_mgrp(__u32 group)
{
	if (group > 31 ) {
		fprintf(stderr, "Use setsockopt for this group %d\n", group);
		exit(-1);
	}
	return group ? (1 << (group - 1)) : 0;
}


int print_timestamp(FILE *fp);

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

extern int cmdlineno;
extern ssize_t getcmdline(char **line, size_t *len, FILE *in);
extern int makeargs(char *line, char *argv[], int maxargs);

struct iplink_req;
int iplink_parse(int argc, char **argv, struct iplink_req *req,
		char **name, char **type, char **link, char **dev,
		int *group);
#endif /* __UTILS_H__ */
