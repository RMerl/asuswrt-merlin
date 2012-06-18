#ifndef __ipt_time_h_included__
#define __ipt_time_h_included__


struct ipt_time_info {
	u_int8_t days_match;	/* 1 bit per day (bit 0 = Sunday) */
	u_int32_t time_start;	/* 0 < time_start < 24*60*60-1 = 86399 */
	u_int32_t time_stop;		/* 0 < time_end < 24*60*60-1 = 86399 */
	u_int8_t kerneltime;			/* ignore skb time (and use kerneltime) or not. */
	time_t date_start;
	time_t date_stop;
};


#endif /* __ipt_time_h_included__ */
