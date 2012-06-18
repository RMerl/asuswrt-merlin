#ifndef _IPT_NTH_H
#define _IPT_NTH_H

#include <linux/param.h>
#include <linux/types.h>

#ifndef IPT_NTH_NUM_COUNTERS
#define IPT_NTH_NUM_COUNTERS 16
#endif

struct ipt_nth_info {
	u_int8_t every;
	u_int8_t not;
	u_int8_t startat;
	u_int8_t counter;
	u_int8_t packet;
};

#endif /*_IPT_NTH_H*/
