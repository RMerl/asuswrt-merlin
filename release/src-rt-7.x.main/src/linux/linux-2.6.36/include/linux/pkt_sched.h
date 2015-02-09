#ifndef __LINUX_PKT_SCHED_H
#define __LINUX_PKT_SCHED_H

#include <linux/types.h>

/* Logical priority bands not depending on specific packet scheduler.
   Every scheduler will map them to real traffic classes, if it has
   no more precise mechanism to classify packets.

   These numbers have no special meaning, though their coincidence
   with obsolete IPv6 values is not occasional :-). New IPv6 drafts
   preferred full anarchy inspired by diffserv group.

   Note: TC_PRIO_BESTEFFORT does not mean that it is the most unhappy
   class, actually, as rule it will be handled with more care than
   filler or even bulk.
 */

#define TC_PRIO_BESTEFFORT		0
#define TC_PRIO_FILLER			1
#define TC_PRIO_BULK			2
#define TC_PRIO_INTERACTIVE_BULK	4
#define TC_PRIO_INTERACTIVE		6
#define TC_PRIO_CONTROL			7

#define TC_PRIO_MAX			15

/* Generic queue statistics, available for all the elements.
   Particular schedulers may have also their private records.
 */

struct tc_stats {
	__u64	bytes;			/* NUmber of enqueues bytes */
	__u32	packets;		/* Number of enqueued packets	*/
	__u32	drops;			/* Packets dropped because of lack of resources */
	__u32	overlimits;		/* Number of throttle events when this
					 * flow goes out of allocated bandwidth */
	__u32	bps;			/* Current flow byte rate */
	__u32	pps;			/* Current flow packet rate */
	__u32	qlen;
	__u32	backlog;
};

struct tc_estimator {
	signed char	interval;
	unsigned char	ewma_log;
};

/* "Handles"
   ---------

    All the traffic control objects have 32bit identifiers, or "handles".

    They can be considered as opaque numbers from user API viewpoint,
    but actually they always consist of two fields: major and
    minor numbers, which are interpreted by kernel specially,
    that may be used by applications, though not recommended.

    F.e. qdisc handles always have minor number equal to zero,
    classes (or flows) have major equal to parent qdisc major, and
    minor uniquely identifying class inside qdisc.

    Macros to manipulate handles:
 */

#define TC_H_MAJ_MASK (0xFFFF0000U)
#define TC_H_MIN_MASK (0x0000FFFFU)
#define TC_H_MAJ(h) ((h)&TC_H_MAJ_MASK)
#define TC_H_MIN(h) ((h)&TC_H_MIN_MASK)
#define TC_H_MAKE(maj,min) (((maj)&TC_H_MAJ_MASK)|((min)&TC_H_MIN_MASK))

#define TC_H_UNSPEC	(0U)
#define TC_H_ROOT	(0xFFFFFFFFU)
#define TC_H_INGRESS    (0xFFFFFFF1U)

struct tc_ratespec {
	unsigned char	cell_log;
	unsigned char	__reserved;
	unsigned short	overhead;
	short		cell_align;
	unsigned short	mpu;
	__u32		rate;
};

#define TC_RTAB_SIZE	1024

struct tc_sizespec {
	unsigned char	cell_log;
	unsigned char	size_log;
	short		cell_align;
	int		overhead;
	unsigned int	linklayer;
	unsigned int	mpu;
	unsigned int	mtu;
	unsigned int	tsize;
};

enum {
	TCA_STAB_UNSPEC,
	TCA_STAB_BASE,
	TCA_STAB_DATA,
	__TCA_STAB_MAX
};

#define TCA_STAB_MAX (__TCA_STAB_MAX - 1)

/* FIFO section */

struct tc_fifo_qopt {
	__u32	limit;	/* Queue length: bytes for bfifo, packets for pfifo */
};

/* PRIO section */

#define TCQ_PRIO_BANDS	16
#define TCQ_MIN_PRIO_BANDS 2

struct tc_prio_qopt {
	int	bands;			/* Number of bands */
	__u8	priomap[TC_PRIO_MAX+1];	/* Map: logical priority -> PRIO band */
};

/* MULTIQ section */

struct tc_multiq_qopt {
	__u16	bands;			/* Number of bands */
	__u16	max_bands;		/* Maximum number of queues */
};

/* TBF section */

struct tc_tbf_qopt {
	struct tc_ratespec rate;
	struct tc_ratespec peakrate;
	__u32		limit;
	__u32		buffer;
	__u32		mtu;
};

enum {
	TCA_TBF_UNSPEC,
	TCA_TBF_PARMS,
	TCA_TBF_RTAB,
	TCA_TBF_PTAB,
	__TCA_TBF_MAX,
};

#define TCA_TBF_MAX (__TCA_TBF_MAX - 1)


/* TEQL section */

/* TEQL does not require any parameters */

/* SFQ section */

struct tc_sfq_qopt {
	unsigned	quantum;	/* Bytes per round allocated to flow */
	int		perturb_period;	/* Period of hash perturbation */
	__u32		limit;		/* Maximal packets in queue */
	unsigned	divisor;	/* Hash divisor  */
	unsigned	flows;		/* Maximal number of flows  */
};

struct tc_sfq_xstats {
	__s32		allot;
};

/*
 *  NOTE: limit, divisor and flows are hardwired to code at the moment.
 *
 *	limit=flows=128, divisor=1024;
 *
 *	The only reason for this is efficiency, it is possible
 *	to change these parameters in compile time.
 */

/* RED section */

enum {
	TCA_RED_UNSPEC,
	TCA_RED_PARMS,
	TCA_RED_STAB,
	__TCA_RED_MAX,
};

#define TCA_RED_MAX (__TCA_RED_MAX - 1)

struct tc_red_qopt {
	__u32		limit;		/* HARD maximal queue length (bytes)	*/
	__u32		qth_min;	/* Min average length threshold (bytes) */
	__u32		qth_max;	/* Max average length threshold (bytes) */
	unsigned char   Wlog;		/* log(W)		*/
	unsigned char   Plog;		/* log(P_max/(qth_max-qth_min))	*/
	unsigned char   Scell_log;	/* cell size for idle damping */
	unsigned char	flags;
#define TC_RED_ECN	1
#define TC_RED_HARDDROP	2
};

struct tc_red_xstats {
	__u32           early;          /* Early drops */
	__u32           pdrop;          /* Drops due to queue limits */
	__u32           other;          /* Drops due to drop() calls */
	__u32           marked;         /* Marked packets */
};

/* GRED section */

#define MAX_DPs 16

enum {
       TCA_GRED_UNSPEC,
       TCA_GRED_PARMS,
       TCA_GRED_STAB,
       TCA_GRED_DPS,
	   __TCA_GRED_MAX,
};

#define TCA_GRED_MAX (__TCA_GRED_MAX - 1)

struct tc_gred_qopt {
	__u32		limit;        /* HARD maximal queue length (bytes)    */
	__u32		qth_min;      /* Min average length threshold (bytes) */
	__u32		qth_max;      /* Max average length threshold (bytes) */
	__u32		DP;           /* upto 2^32 DPs */
	__u32		backlog;
	__u32		qave;
	__u32		forced;
	__u32		early;
	__u32		other;
	__u32		pdrop;
	__u8		Wlog;         /* log(W)               */
	__u8		Plog;         /* log(P_max/(qth_max-qth_min)) */
	__u8		Scell_log;    /* cell size for idle damping */
	__u8		prio;         /* prio of this VQ */
	__u32		packets;
	__u32		bytesin;
};

/* gred setup */
struct tc_gred_sopt {
	__u32		DPs;
	__u32		def_DP;
	__u8		grio;
	__u8		flags;
	__u16		pad1;
};

/* HTB section */
#define TC_HTB_NUMPRIO		8
#define TC_HTB_MAXDEPTH		8
#define TC_HTB_PROTOVER		3 /* the same as HTB and TC's major */

struct tc_htb_opt {
	struct tc_ratespec 	rate;
	struct tc_ratespec 	ceil;
	__u32	buffer;
	__u32	cbuffer;
	__u32	quantum;
	__u32	level;		/* out only */
	__u32	prio;
};
struct tc_htb_glob {
	__u32 version;		/* to match HTB/TC */
    	__u32 rate2quantum;	/* bps->quantum divisor */
    	__u32 defcls;		/* default class number */
	__u32 debug;		/* debug flags */

	/* stats */
	__u32 direct_pkts; /* count of non shapped packets */
};
enum {
	TCA_HTB_UNSPEC,
	TCA_HTB_PARMS,
	TCA_HTB_INIT,
	TCA_HTB_CTAB,
	TCA_HTB_RTAB,
	__TCA_HTB_MAX,
};

#define TCA_HTB_MAX (__TCA_HTB_MAX - 1)

struct tc_htb_xstats {
	__u32 lends;
	__u32 borrows;
	__u32 giants;	/* too big packets (rate will not be accurate) */
	__u32 tokens;
	__u32 ctokens;
};

/* HFSC section */

struct tc_hfsc_qopt {
	__u16	defcls;		/* default class */
};

struct tc_service_curve {
	__u32	m1;		/* slope of the first segment in bps */
	__u32	d;		/* x-projection of the first segment in us */
	__u32	m2;		/* slope of the second segment in bps */
};

struct tc_hfsc_stats {
	__u64	work;		/* total work done */
	__u64	rtwork;		/* work done by real-time criteria */
	__u32	period;		/* current period */
	__u32	level;		/* class level in hierarchy */
};

enum {
	TCA_HFSC_UNSPEC,
	TCA_HFSC_RSC,
	TCA_HFSC_FSC,
	TCA_HFSC_USC,
	__TCA_HFSC_MAX,
};

#define TCA_HFSC_MAX (__TCA_HFSC_MAX - 1)


/* CBQ section */

#define TC_CBQ_MAXPRIO		8
#define TC_CBQ_MAXLEVEL		8
#define TC_CBQ_DEF_EWMA		5

struct tc_cbq_lssopt {
	unsigned char	change;
	unsigned char	flags;
#define TCF_CBQ_LSS_BOUNDED	1
#define TCF_CBQ_LSS_ISOLATED	2
	unsigned char  	ewma_log;
	unsigned char  	level;
#define TCF_CBQ_LSS_FLAGS	1
#define TCF_CBQ_LSS_EWMA	2
#define TCF_CBQ_LSS_MAXIDLE	4
#define TCF_CBQ_LSS_MINIDLE	8
#define TCF_CBQ_LSS_OFFTIME	0x10
#define TCF_CBQ_LSS_AVPKT	0x20
	__u32		maxidle;
	__u32		minidle;
	__u32		offtime;
	__u32		avpkt;
};

struct tc_cbq_wrropt {
	unsigned char	flags;
	unsigned char	priority;
	unsigned char	cpriority;
	unsigned char	__reserved;
	__u32		allot;
	__u32		weight;
};

struct tc_cbq_ovl {
	unsigned char	strategy;
#define	TC_CBQ_OVL_CLASSIC	0
#define	TC_CBQ_OVL_DELAY	1
#define	TC_CBQ_OVL_LOWPRIO	2
#define	TC_CBQ_OVL_DROP		3
#define	TC_CBQ_OVL_RCLASSIC	4
	unsigned char	priority2;
	__u16		pad;
	__u32		penalty;
};

struct tc_cbq_police {
	unsigned char	police;
	unsigned char	__res1;
	unsigned short	__res2;
};

struct tc_cbq_fopt {
	__u32		split;
	__u32		defmap;
	__u32		defchange;
};

struct tc_cbq_xstats {
	__u32		borrows;
	__u32		overactions;
	__s32		avgidle;
	__s32		undertime;
};

enum {
	TCA_CBQ_UNSPEC,
	TCA_CBQ_LSSOPT,
	TCA_CBQ_WRROPT,
	TCA_CBQ_FOPT,
	TCA_CBQ_OVL_STRATEGY,
	TCA_CBQ_RATE,
	TCA_CBQ_RTAB,
	TCA_CBQ_POLICE,
	__TCA_CBQ_MAX,
};

#define TCA_CBQ_MAX	(__TCA_CBQ_MAX - 1)

/* dsmark section */

enum {
	TCA_DSMARK_UNSPEC,
	TCA_DSMARK_INDICES,
	TCA_DSMARK_DEFAULT_INDEX,
	TCA_DSMARK_SET_TC_INDEX,
	TCA_DSMARK_MASK,
	TCA_DSMARK_VALUE,
	__TCA_DSMARK_MAX,
};

#define TCA_DSMARK_MAX (__TCA_DSMARK_MAX - 1)

/* ATM  section */

enum {
	TCA_ATM_UNSPEC,
	TCA_ATM_FD,		/* file/socket descriptor */
	TCA_ATM_PTR,		/* pointer to descriptor - later */
	TCA_ATM_HDR,		/* LL header */
	TCA_ATM_EXCESS,		/* excess traffic class (0 for CLP)  */
	TCA_ATM_ADDR,		/* PVC address (for output only) */
	TCA_ATM_STATE,		/* VC state (ATM_VS_*; for output only) */
	__TCA_ATM_MAX,
};

#define TCA_ATM_MAX	(__TCA_ATM_MAX - 1)

/* Network emulator */

enum {
	TCA_NETEM_UNSPEC,
	TCA_NETEM_CORR,
	TCA_NETEM_DELAY_DIST,
	TCA_NETEM_REORDER,
	TCA_NETEM_CORRUPT,
	__TCA_NETEM_MAX,
};

#define TCA_NETEM_MAX (__TCA_NETEM_MAX - 1)

struct tc_netem_qopt {
	__u32	latency;	/* added delay (us) */
	__u32   limit;		/* fifo limit (packets) */
	__u32	loss;		/* random packet loss (0=none ~0=100%) */
	__u32	gap;		/* re-ordering gap (0 for none) */
	__u32   duplicate;	/* random packet dup  (0=none ~0=100%) */
	__u32	jitter;		/* random jitter in latency (us) */
};

struct tc_netem_corr {
	__u32	delay_corr;	/* delay correlation */
	__u32	loss_corr;	/* packet loss correlation */
	__u32	dup_corr;	/* duplicate correlation  */
};

struct tc_netem_reorder {
	__u32	probability;
	__u32	correlation;
};

struct tc_netem_corrupt {
	__u32	probability;
	__u32	correlation;
};

#define NETEM_DIST_SCALE	8192

/* DRR */

enum {
	TCA_DRR_UNSPEC,
	TCA_DRR_QUANTUM,
	__TCA_DRR_MAX
};

#define TCA_DRR_MAX	(__TCA_DRR_MAX - 1)

struct tc_drr_stats {
	__u32	deficit;
};

#endif
