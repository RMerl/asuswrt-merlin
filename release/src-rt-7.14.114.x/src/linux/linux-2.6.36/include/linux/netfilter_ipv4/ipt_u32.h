#ifndef _IPT_U32_H
#define _IPT_U32_H
#include <linux/netfilter_ipv4/ip_tables.h>

enum ipt_u32_ops
{
	IPT_U32_AND,
	IPT_U32_LEFTSH,
	IPT_U32_RIGHTSH,
	IPT_U32_AT
};

struct ipt_u32_location_element
{
	u_int32_t number;
	u_int8_t nextop;
};
struct ipt_u32_value_element
{
	u_int32_t min;
	u_int32_t max;
};
/* *** any way to allow for an arbitrary number of elements?
   for now I settle for a limit of 10 of each */
#define U32MAXSIZE 10
struct ipt_u32_test
{
	u_int8_t nnums;
	struct ipt_u32_location_element location[U32MAXSIZE+1];
	u_int8_t nvalues;
	struct ipt_u32_value_element value[U32MAXSIZE+1];
};

struct ipt_u32
{
	u_int8_t ntests;
	u_int8_t invert;
	struct ipt_u32_test tests[U32MAXSIZE+1];
};

#endif /*_IPT_U32_H*/
