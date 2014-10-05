#ifndef __ipt_time_h_included__
#define __ipt_time_h_included__


struct ipt_time_info {
	u_int8_t  days_match;   /* 1 bit per day. -SMTWTFS                      */
	u_int16_t time_start;   /* 0 < time_start < 23*60+59 = 1439             */
	u_int16_t time_stop;    /* 0:0 < time_stat < 23:59                      */

				/* FIXME: Keep this one for userspace iptables binary compability: */
	u_int8_t  kerneltime;   /* ignore skb time (and use kerneltime) or not. */

	time_t    date_start;
	time_t    date_stop;
};


#endif /* __ipt_time_h_included__ */
