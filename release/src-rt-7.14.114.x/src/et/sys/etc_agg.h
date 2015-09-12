/*
 * Broadcom Port Trunking/Aggregation setup functions
 *
 * $Copyright Open Broadcom Corporation$
 *
 * $Id$
 */
#ifndef _etc_agg_h_
#define _etc_agg_h_

#define AGG_INVALID_PID		0xFFFF

/* FIXME, should integrate with FA's definition */
typedef struct {
	union {
#ifdef BIG_ENDIAN
		struct {
			uint32_t	op_code		:3;	/* 31:29 */
			uint32_t	reserved	:5;	/* 28:24 */
			uint32_t	cl_id		:8;	/* 23:16 */
			uint32_t	reason_code	:8;	/* 15:8 */
			uint32_t	tc		:3;	/* 7:5 */
			uint32_t	src_pid		:5;	/* 4:0 */
		} eg_oc0;
		struct {
			uint32_t	op_code		:3;	/* 31:29 */
			uint32_t	reserved	:2;	/* 28:27 */
			uint32_t	all_bkts_full	:1;	/* 26 */
			uint32_t	bkt_id		:2;	/* 25:24 */
			uint32_t	napt_flow_id	:8;	/* 23:16 */
			uint32_t	hdr_chk_result	:8;	/* 15:8 */
			uint32_t	tc		:3;	/* 7:5 */
			uint32_t	src_pid		:5;	/* 4:0 */
		} eg_oc10;
		struct {
			uint32_t	op_code		:3;	/* 31:29 */
			uint32_t	tc		:3;	/* 28:26 */
			uint32_t	te		:2;	/* 25:24 */
			uint32_t	reserved	:24;	/* 23:0 */
		} ing_oc0;
		struct {
			uint32_t	op_code		:3;	/* 31:29 */
			uint32_t	tc		:3;	/* 28:26 */
			uint32_t	te		:2;	/* 25:24 */
			uint32_t	ts		:1;	/* 23 */
			uint32_t	dst_map		:23;	/* 22:0 */
		} ing_oc01;
#else
		struct {
			uint32_t	src_pid		:5;	/* 4:0 */
			uint32_t	tc		:3;	/* 7:5 */
			uint32_t	reason_code	:8;	/* 15:8 */
			uint32_t	cl_id		:8;	/* 23:16 */
			uint32_t	reserved	:5;	/* 28:24 */
			uint32_t	op_code		:3;	/* 31:29 */
		} eg_oc0;
		struct {
			uint32_t	src_pid		:5;	/* 4:0 */
			uint32_t	tc		:3;	/* 7:5 */
			uint32_t	hdr_chk_result	:8;	/* 15:8 */
			uint32_t	napt_flow_id	:8;	/* 23:16 */
			uint32_t	bkt_id		:2;	/* 25:24 */
			uint32_t	all_bkts_full	:1;	/* 26 */
			uint32_t	reserved	:2;	/* 28:27 */
			uint32_t	op_code		:3;	/* 31:29 */
		} eg_oc10;
		struct {
			uint32_t	reserved	:24;	/* 23:0 */
			uint32_t	te		:2;	/* 25:24 */
			uint32_t	tc		:3;	/* 28:26 */
			uint32_t	op_code		:3;	/* 31:29 */
		} ing_oc0;
		struct {
			uint32_t	dst_map		:23;	/* 22:0 */
			uint32_t	ts		:1;	/* 23 */
			uint32_t	te		:2;	/* 25:24 */
			uint32_t	tc		:3;	/* 28:26 */
			uint32_t	op_code		:3;	/* 31:29 */
		} ing_oc01;
#endif /* BIG_ENDIAN */
		uint32_t word;
	};
} bcm53125_hdr_t;

typedef struct agg_pub {
	uint32	flags;
} agg_pub_t;

#define AGG_FLAG_BCM_HDR	0x1

#define AGG_BCM_HDR_ENAB(pub)	((pub) && (((agg_pub_t *)pub)->flags & AGG_FLAG_BCM_HDR))

extern void *agg_get_lacp_header(void *data);
extern uint16 agg_get_lacp_dest_pid(void *lacph);
extern void *agg_process_tx(void *agg, void *p, void *bhdrp, int8 bhdr_roff, uint16 dest_pid);
extern void agg_process_rx(void *agg, void *p, int dataoff, int8 bhdr_roff);
extern int32 agg_get_linksts(void *agg, uint32 *linksts);
extern int32 agg_get_portsts(void *agg, int32 portid, uint32 *link, uint32 *speed,
	uint32 *duplex);
extern int32 agg_group_update(void *agg, int group, uint32 portmap);
extern int32 agg_bhdr_switch(void *agg, uint32 on);
extern void *agg_attach(void *osh, void *et, char *vars, uint coreunit, void *robo);
extern void agg_detach(void *agg);
#endif /* !_etc_agg_h_ */
