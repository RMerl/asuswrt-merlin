#ifndef _IPT_TRIGGER_H_target
#define _IPT_TRIGGER_H_target

#define TRIGGER_TIMEOUT 600	/* 600 secs */

enum ipt_trigger_type
{
	IPT_TRIGGER_DNAT = 1,
	IPT_TRIGGER_IN = 2,
	IPT_TRIGGER_OUT = 3,
	IPT_TRIGGER_REFRESH = 4
};

struct ipt_trigger_ports {
	u_int16_t mport[2];	/* Related destination port range */
	u_int16_t rport[2];	/* Port range to map related destination port range to */
};

struct ipt_trigger_info {
	enum ipt_trigger_type type;
	u_int16_t proto;	/* Related protocol */
	struct ipt_trigger_ports ports;
};

#endif /*_IPT_TRIGGER_H_target*/
