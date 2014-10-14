#ifndef _IPT_HASHLIMIT_H
#define _IPT_HASHLIMIT_H

#include <linux/netfilter/xt_hashlimit.h>

#define IPT_HASHLIMIT_SCALE	XT_HASHLIMIT_SCALE
#define IPT_HASHLIMIT_HASH_DIP	XT_HASHLIMIT_HASH_DIP
#define IPT_HASHLIMIT_HASH_DPT	XT_HASHLIMIT_HASH_DPT
#define IPT_HASHLIMIT_HASH_SIP	XT_HASHLIMIT_HASH_SIP
#define IPT_HASHLIMIT_HASH_SPT	XT_HASHLIMIT_HASH_SPT

#define ipt_hashlimit_info xt_hashlimit_info

#endif /* _IPT_HASHLIMIT_H */
