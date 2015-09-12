#ifndef __ipt_time_h_included__
#define __ipt_time_h_included__


struct ipt_time_info {
	unsigned int  days_match;   /* 1 bit per day. -SMTWTFS            */
	unsigned int  time_start;   /* 0 < time_start   < 86399           */
	unsigned int   time_stop;    /* 0:0 < time_start < 23:59          */
	int  kerneltime;   /* ignore skb time (and use kerneltime) or not.*/
        time_t date_start;
        time_t date_stop;
};


#endif /* __ipt_time_h_included__ */
