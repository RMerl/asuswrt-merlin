/*
  This is a module which is used for time matching
  It is using some modified code from dietlibc (localtime() function)
  that you can find at http://www.fefe.de/dietlibc/
  This file is distributed under the terms of the GNU General Public
  License (GPL). Copies of the GPL can be obtained from: ftp://prep.ai.mit.edu/pub/gnu/GPL
  2001-05-04 Fabrice MARIE <fabrice@netfilter.org> : initial development.
  2001-21-05 Fabrice MARIE <fabrice@netfilter.org> : bug fix in the match code,
     thanks to "Zeng Yu" <zengy@capitel.com.cn> for bug report.
  2001-26-09 Fabrice MARIE <fabrice@netfilter.org> : force the match to be in LOCAL_IN or PRE_ROUTING only.
  2001-30-11 Fabrice : added the possibility to use the match in FORWARD/OUTPUT with a little hack,
     added Nguyen Dang Phuoc Dong <dongnd@tlnet.com.vn> patch to support timezones.
*/

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_time.h>
#include <linux/time.h>

MODULE_AUTHOR("Fabrice MARIE <fabrice@netfilter.org>");
MODULE_DESCRIPTION("Match arrival timestamp");
MODULE_LICENSE("GPL");

struct tm
{
	int tm_sec;                   /* Seconds.     [0-60] (1 leap second) */
	int tm_min;                   /* Minutes.     [0-59] */
	int tm_hour;                  /* Hours.       [0-23] */
	int tm_mday;                  /* Day.         [1-31] */
	int tm_mon;                   /* Month.       [0-11] */
	int tm_year;                  /* Year - 1900.  */
	int tm_wday;                  /* Day of week. [0-6] */
	int tm_yday;                  /* Days in year.[0-365] */
	int tm_isdst;                 /* DST.         [-1/0/1]*/

	long int tm_gmtoff;           /* we don't care, we count from GMT */
	const char *tm_zone;          /* we don't care, we count from GMT */
};

void
localtime(const time_t *timepr, struct tm *r);

static int
/*
match(const struct sk_buff *skb,
      const struct net_device *in,
      const struct net_device *out,
      const void *matchinfo,
      int offset,
      const void *hdr,
      u_int16_t datalen,
      int *hotdrop)
*/
ipt_time_match(const struct sk_buff *skb,
                 const struct net_device *in, const struct net_device *out,
                 const struct xt_match *match, const void *matchinfo,
                 int offset, unsigned int protoff, int *hotdrop)
{
	const struct ipt_time_info *info = matchinfo;   /* match info for rule */
	struct tm currenttime;                          /* time human readable */
	unsigned int packet_time;
	struct timeval kerneltimeval;
	time_t packet_local_time;


	/* if kerneltime=1, we don't read the skb->timestamp but kernel time instead */
	if (info->kerneltime)
	{
		do_gettimeofday(&kerneltimeval);
		packet_local_time = kerneltimeval.tv_sec;
	}
	else {
		packet_local_time = skb->tstamp.tv64;
		//packet_local_time = skb->tstamp.off_sec;
	}

	/* Transform the timestamp of the packet, in a human readable form */
	localtime(&packet_local_time, &currenttime);

	//printk("dbg: Date = yy/mm/dd = %d/%d/%d, week=%d, Time = %d/%d Days Match = %d\n", currenttime.tm_year, currenttime.tm_mon, currenttime.tm_mday, currenttime.tm_wday, currenttime.tm_hour, currenttime.tm_min, info->days_match);
	/* check if we match this timestamp, we start by the days... */
	if (!((1 << (6-currenttime.tm_wday)) & info->days_match)) {
		//printk("dbg: the day doesn't match: currect.tm_wday = 0x%x, info->days_match = 0x%x\n", currenttime.tm_wday, info->days_match);
		return 0; /* the day doesn't match */
	}

	/* ... check the time now */
//	packet_time = (currenttime.tm_hour * 60 * 60) + (currenttime.tm_min * 60) + currenttime.tm_sec;
	packet_time = (currenttime.tm_hour * 60) + currenttime.tm_min;

	if (info->time_start < info->time_stop) {
		if ((packet_time < info->time_start) || (packet_time > info->time_stop)) {
			//printk("dbg: the time doesn't match: %d %d %d\n", packet_time, info->time_start, info->time_stop);
			return 0;
		}
	} else {
		if ((packet_time < info->time_start) && (packet_time > info->time_stop)) {
			//printk("dbg: the time doesn't match: %d %d %d\n", packet_time, info->time_start, info->time_stop);
			return 0;
		}
	}

	//printk("dbg: the time match %d %d %d\n", packet_time, info->time_start, info->time_stop);
	/* here we match ! */
	return 1;
}

static int
/*
checkentry(const char *tablename,
           const struct ipt_ip *ip,
           void *matchinfo,
           unsigned int matchsize,
           unsigned int hook_mask)
*/
ipt_time_checkentry(const char *tablename, const void *ip,
                      const struct xt_match *match, void *matchinfo,
                      unsigned int hook_mask)
{
	struct ipt_time_info *info = matchinfo;   /* match info for rule */

	/* First, check that we are in the correct hook */
	/* PRE_ROUTING, LOCAL_IN or FROWARD */
	if (hook_mask
            & ~((1 << NF_IP_PRE_ROUTING) | (1 << NF_IP_LOCAL_IN) | (1 << NF_IP_FORWARD) | (1 << NF_IP_LOCAL_OUT)))
	{
		printk("ipt_time: error, only valid for PRE_ROUTING, LOCAL_IN, FORWARD and OUTPUT)\n");
		return 0;
	}

	/* always use kerneltime */
	info->kerneltime = 1;

	/* Check the size */
	//if (matchsize < IPT_ALIGN(sizeof(struct ipt_time_info)))
	//	return 0;

	/* Now check the coherence of the data ... */
	if ((info->time_start > 86399) ||        /* 24*60*60-1 = 86399*/
	    (info->time_stop  > 86399))
	{
		printk(KERN_WARNING "ipt_time: invalid argument\n");
		return 0;
	}

	return 1;
}

/*
static struct ipt_match time_match
= { { NULL, NULL }, "time", &match, &checkentry, NULL, THIS_MODULE };
*/

static struct xt_match time_match[] = {
        {
        .name           = "time",
        .family         = AF_INET,
        .match          = ipt_time_match,
        .matchsize      = sizeof(struct ipt_time_info),
        .checkentry     = ipt_time_checkentry,
        .me             = THIS_MODULE
        },
        {
        .name           = "time",
        .family         = AF_INET6,
        .match          = ipt_time_match,
        .matchsize      = sizeof(struct ipt_time_info),
        .checkentry     = ipt_time_checkentry,
        .me             = THIS_MODULE
        },
};

static int __init init(void)
{
	//return ipt_register_match(&time_match);
	return xt_register_matches(time_match, ARRAY_SIZE(time_match));
}

static void __exit fini(void)
{
	//ipt_unregister_match(&time_match);
	xt_unregister_matches(time_match, ARRAY_SIZE(time_match));
}

module_init(init);
module_exit(fini);


/* The part below is borowed and modified from dietlibc */

/* seconds per day */
#define SPD 24*60*60

void
localtime(const time_t *timepr, struct tm *r) {
	time_t i;
	time_t timep;
	extern struct timezone sys_tz;
	const unsigned int __spm[12] =
		{ 0,
		  (31),
		  (31+28),
		  (31+28+31),
		  (31+28+31+30),
		  (31+28+31+30+31),
		  (31+28+31+30+31+30),
		  (31+28+31+30+31+30+31),
		  (31+28+31+30+31+30+31+31),
		  (31+28+31+30+31+30+31+31+30),
		  (31+28+31+30+31+30+31+31+30+31),
		  (31+28+31+30+31+30+31+31+30+31+30),
		};
	register time_t work;

	timep = (*timepr) - (sys_tz.tz_minuteswest * 60);

	work=timep%(SPD);
	r->tm_sec=work%60; work/=60;
	r->tm_min=work%60; r->tm_hour=work/60;
	work=timep/(SPD);
	r->tm_wday=(4+work)%7;
	for (i=1970; ; ++i) {
		register time_t k= (!(i%4) && ((i%100) || !(i%400)))?366:365;
		if (work>k)
			work-=k;
		else
			break;
	}
	r->tm_year=i-1900;
	for (i=11; i && __spm[i]>work; --i) ;
	r->tm_mon=i;
	r->tm_mday=work-__spm[i]+1;
}
