#ifndef _IMQ_H
#define _IMQ_H

#define IMQ_MAX_DEVS	16

#define IMQ_F_IFMASK	0x7f
#define IMQ_F_ENQUEUE	0x80

/* constants for IMQ behaviour (regarding netfilter hooking) */
#define IMQ_BEHAV_INGRESS_AFTER		1
#define IMQ_BEHAV_EGRESS_AFTER		2

#endif /* _IMQ_H */
