#ifndef __ipt_time_h_included__
#define __ipt_time_h_included__


struct ipt_time_info {
	u_int8_t  days_match;   /* 1 bit per day. -SMTWTFS                      */
	u_int32_t time_start;   /* 0 <= time_start <= 23*60*60+59*60+59 = 86399 */
	u_int32_t time_stop;    /* 0:0:0 <= time <= 23:59:59                    */
	time_t    date_start;
	time_t    date_stop;
	u_int8_t  kerneltime;   /* ignore skb time (and use kerneltime) or not. */
};


#endif /* __ipt_time_h_included__ */
