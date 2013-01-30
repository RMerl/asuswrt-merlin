/* vi: set sw=4 ts=4: */
#ifndef UTILS_H
#define UTILS_H 1

#include "libnetlink.h"
#include "ll_map.h"
#include "rtm_map.h"

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

extern family_t preferred_family;
extern smallint show_stats;    /* UNUSED */
extern smallint show_details;  /* UNUSED */
extern smallint show_raw;      /* UNUSED */
extern smallint resolve_hosts; /* UNUSED */
extern smallint oneline;
extern char _SL_;

#ifndef IPPROTO_ESP
#define IPPROTO_ESP	50
#endif
#ifndef IPPROTO_AH
#define IPPROTO_AH	51
#endif

#define SPRINT_BSIZE 64
#define SPRINT_BUF(x)	char x[SPRINT_BSIZE]

extern void incomplete_command(void) NORETURN;

#define NEXT_ARG() do { if (!*++argv) incomplete_command(); } while (0)

typedef struct {
	uint8_t family;
	uint8_t bytelen;
	int16_t bitlen;
	uint32_t data[4];
} inet_prefix;

#define PREFIXLEN_SPECIFIED 1

#define DN_MAXADDL 20
#ifndef AF_DECnet
#define AF_DECnet 12
#endif

struct dn_naddr {
	unsigned short a_len;
	unsigned char  a_addr[DN_MAXADDL];
};

#define IPX_NODE_LEN 6

struct ipx_addr {
	uint32_t ipx_net;
	uint8_t  ipx_node[IPX_NODE_LEN];
};

extern uint32_t get_addr32(char *name);
extern int get_addr_1(inet_prefix *dst, char *arg, int family);
/*extern int get_prefix_1(inet_prefix *dst, char *arg, int family);*/
extern int get_addr(inet_prefix *dst, char *arg, int family);
extern int get_prefix(inet_prefix *dst, char *arg, int family);

extern unsigned get_unsigned(char *arg, const char *errmsg);
extern uint32_t get_u32(char *arg, const char *errmsg);
extern uint16_t get_u16(char *arg, const char *errmsg);

extern const char *rt_addr_n2a(int af, void *addr, char *buf, int buflen);
#ifdef RESOLVE_HOSTNAMES
extern const char *format_host(int af, int len, void *addr, char *buf, int buflen);
#else
#define format_host(af, len, addr, buf, buflen) \
	rt_addr_n2a(af, addr, buf, buflen)
#endif

void invarg(const char *, const char *) NORETURN;
void duparg(const char *, const char *) NORETURN;
void duparg2(const char *, const char *) NORETURN;
int inet_addr_match(inet_prefix *a, inet_prefix *b, int bits);

const char *dnet_ntop(int af, const void *addr, char *str, size_t len);
int dnet_pton(int af, const char *src, void *addr);

const char *ipx_ntop(int af, const void *addr, char *str, size_t len);
int ipx_pton(int af, const char *src, void *addr);

POP_SAVED_FUNCTION_VISIBILITY

#endif
