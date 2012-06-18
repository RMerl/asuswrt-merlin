#ifndef _LINUX_NETFILTER_XT_RECENT_H
#define _LINUX_NETFILTER_XT_RECENT_H 1

enum {
	XT_RECENT_CHECK    = 1 << 0,
	XT_RECENT_SET      = 1 << 1,
	XT_RECENT_UPDATE   = 1 << 2,
	XT_RECENT_REMOVE   = 1 << 3,
	XT_RECENT_TTL      = 1 << 4,
	XT_RECENT_REAP     = 1 << 5,

	XT_RECENT_SOURCE   = 0,
	XT_RECENT_DEST     = 1,

	XT_RECENT_NAME_LEN = 200,
};

/* Only allowed with --rcheck and --update */
#define XT_RECENT_MODIFIERS (XT_RECENT_TTL|XT_RECENT_REAP)

#define XT_RECENT_VALID_FLAGS (XT_RECENT_CHECK|XT_RECENT_SET|XT_RECENT_UPDATE|\
			       XT_RECENT_REMOVE|XT_RECENT_TTL|XT_RECENT_REAP)

struct xt_recent_mtinfo {
	u_int32_t seconds;
	u_int32_t hit_count;
	u_int8_t check_set;
	u_int8_t invert;
	char name[XT_RECENT_NAME_LEN];
	u_int8_t side;
};

#endif /* _LINUX_NETFILTER_XT_RECENT_H */
