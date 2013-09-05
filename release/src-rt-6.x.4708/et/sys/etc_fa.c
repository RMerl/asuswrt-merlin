/*
 *  Flow Accelerator setup functions
 *
 * Copyright (C) 2013, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: $
 */
#include <typedefs.h>
#include <osl.h>
#include <bcmdefs.h>
#include <bcmdevs.h>
#include <bcmendian.h>
#include <siutils.h>
#include <hndsoc.h>
#include <fa_core.h>
#include <etc_fa.h>
#include <et_export.h>	/* for et_fa_xx() routines */
#include <chipcommonb.h>
#include <bcmrobo.h>


#define ETC_FA_LOCK_INIT(fai)	et_fa_lock_init((fai)->et)
#define ETC_FA_LOCK(fai)	et_fa_lock((fai)->et)
#define ETC_FA_UNLOCK(fai)	et_fa_unlock((fai)->et)

#ifdef	BCMDBG
#define	ET_ERROR(args)	printf args
#define	ET_TRACE(args)
#else	/* BCMDBG */
#define	ET_ERROR(args)
#define	ET_TRACE(args)
#endif	/* BCMDBG */
#define	ET_MSG(args)


#define NHOP_FULL		CTF_MAX_NEXTHOP_TABLE_INDEX
#define PEEKNEXT_NHIDX(nhi)     ((nhi)->free_idx)
#define GETNEXT_NHIDX(nhi) \
({ \
	uint8 idx = (nhi)->free_idx; \
	(nhi)->free_idx = (nhi)->flist[idx]; \
	(nhi)->flist[idx] = 0; \
	idx; \
})

#define PUTNEXT_NHIDX(nhi, i) \
{ \
	if ((nhi)->free_idx == NHOP_FULL) \
		(nhi)->free_idx = i; \
	(nhi)->flist[i] = (nhi)->free_idx; \
	(nhi)->free_idx = i; \
}

/* FA mode values */
#define CTF_FA_BYPASS   1
#define CTF_FA_NORMAL   2
#define CTF_FA_SW_ACC   3

/* FA dump options */
#define CTF_FA_DUMP_VALID       1
#define CTF_FA_DUMP_VALID_NF    2
#define CTF_FA_DUMP_VALID_NH    3
#define CTF_FA_DUMP_VALID_NP    4
#define CTF_FA_DUMP_ALL_NF	5
#define CTF_FA_DUMP_ALL_NH	6
#define CTF_FA_DUMP_ALL_NP	7
#define CTF_FA_DUMP_ALL		8

#define SELECT_MACC_TABLE_RD(f, t, i) \
{ \
	W_REG(si_osh((f)->sih), &(f)->regs->mem_acc_ctl, \
	CTF_MEMACC_RD_TABLE(t, i)); \
}

#define SELECT_MACC_TABLE_WR(f, t, i) \
{ \
	W_REG(si_osh((f)->sih), &(f)->regs->mem_acc_ctl, \
	CTF_MEMACC_WR_TABLE(t, i)); \
}

#define CTF_FA_MACC_RD(f, d, n) \
{ \
	int i; \
	d[0] = R_REG(si_osh((f)->sih), &(f)->regs->m_accdata[0]); \
	for (i = (n); i; i--) { \
		d[i-1] = R_REG(si_osh((f)->sih), &(f)->regs->m_accdata[i-1]); \
	} \
	W_REG(si_osh((f)->sih), &(f)->regs->mem_acc_ctl, 0); \
}

#define CTF_FA_MACC_WR(f, d, n) \
{ \
	int i; \
	for (i = (n); i; i--) { \
		W_REG(si_osh((f)->sih), &(f)->regs->m_accdata[i-1], d[i-1]); \
	} \
}

#define CTF_FA_GET_NH_DA(d, s) \
{ \
	d[5] = (uint8_t)((s[0] >> 19) & 0xFF); \
	d[4] = (uint8_t)((s[0] >> 27) & 0x1F) | \
		(uint8_t)((s[1] & 0x00000007) << 5); \
	d[3] = (uint8_t)((s[1] >> 3) & 0xFF); \
	d[2] = (uint8_t)((s[1] >> 11) & 0xFF); \
	d[1] = (uint8_t)((s[1] >> 19) & 0xFF); \
	d[0] = (uint8_t)((s[1] >> 27) & 0x1F) | \
		(uint8_t)((s[2] & 0x7) << 5); \
}

#define CTF_FA_SET_NH_ENTRY(d, s, ft, o, vt) \
{ \
	d[0] = (((ft) & 0x1) | (((o) & 0x3) << 1) | \
		 (((vt) & 0xFFFF) << 3) | \
		 (s[5] << 19) | (s[4] & 0x1F) << 27); \
	d[1] = (((s[4] & 0xE0) >> 5) | (s[3] << 3) | \
		 (s[2] << 11) | (s[1] << 19) | \
		 (s[0] & 0x1F) << 27); \
	d[2] = ((s[0] & 0xE0) >> 5); \
}

#define CTF_FA_GET_NP_SA(d, s) \
{ \
	d[5] = (uint8_t)((s[0] >> 1) & 0xFF); \
	d[4] = (uint8_t)((s[0] >> 9) & 0xFF); \
	d[3] = (uint8_t)((s[0] >> 17) & 0xFF); \
	d[2] = (uint8_t)(((s[0] >> 25) & 0x7F) | \
			((s[1] & 0x1) << 7)); \
	d[1] = (uint8_t)((s[1] >> 1) & 0xFF); \
	d[0] = (uint8_t)((s[1] >> 9) & 0xFF); \
}

#define CTF_FA_SET_NP_ENTRY(d, s, e) \
{ \
	d[0] = ((e & 0x1) | (s[5] << 1) | (s[4] << 9) | \
		(s[3] << 17) | (s[2] & 0x7F) << 25); \
	d[1] = ((((s[2] & 0x80) >> 7) << 0) | \
		 (s[1] << 1) | (s[0] << 9)); \
}

#define CTF_FA_W_REG(osh, reg, m, v) \
{ \
	if (m) { \
		uint32 val = R_REG((osh), (reg)); \
		val &= ~(m); \
		(val) |= ((v) & (m)); \
		W_REG((osh), (reg), val); \
	} \
	R_REG((osh), (reg)); \
}

#define CTF_FA_WAR777_ON(f) \
{ \
	if ((f)->pub.flags & FA_777WAR_ENABLED) { \
		CTF_FA_W_REG(si_osh((f)->sih), &((f)->regs->control), \
		CTF_CTL_HWQ_THRESHLD_MASK, 0); \
		OSL_DELAY(1000); \
	} \
}

#define CTF_FA_WAR777_OFF(f) \
{ \
	if ((f)->pub.flags & FA_777WAR_ENABLED) { \
		CTF_FA_W_REG(si_osh((f)->sih), &((f)->regs->control), \
		 CTF_CTL_HWQ_THRESHLD_MASK, \
		 (0x140 << 4)); \
	} \
}

#define	PRINT_FLOW_TABLE_HDR(pr, b) pr(b, "%-4s %-35s %-35s %4s %5s %5s %-3s %-8s %-4s %5s " \
					"%-35s %5s %4s %4s %4s %3s Action\n", "Fidx", \
					"Sip", "dip", "Prot", "SPort", "DPort", "Ts", \
					"Hits", "Dir", "RPort", "Rip", "Valid", "Ipv4", \
					"Nidx", "Pidx", "Dma");

#define	PRINT_NHOP_TABLE_HDR(pr, b) pr(b, "%-5s %-17s %-8s %6s L2_Frame_type\n", "Nhidx", \
					"NH-Mac", "Vlan-TCI", "Tag-op");

#define	PRINT_POOL_TABLE_HDR(pr, b) pr(b, "%-8s %-17s External\n", "Pool-idx", "Rmap_SA");

#define FA_CAPABLE(rev)		((rev) >= 3)
#define FA_NVRAM_OVERRIDDEN()	(getintvar(NULL, "fa_overridden") == 2)
#define FA_FA_UNIT(u)		(((u) == 2) ? TRUE : FALSE)
#define	FA_NAPT(fai)		((fa_info_t *)(fai))->napt
#define FA_NAPT_TPL_CMP(p, v6, sip, dip, proto, sp, dp) \
	(!(((v6) ? memcmp(p->sip, sip, 32) : \
		(p->sip[0] ^ sip[0]) | (p->dip[0] ^ dip[0])) | \
	((p->proto ^ proto) | \
	(p->sp ^ sp) | \
	(p->dp ^ dp))))

#define FA_NAPT_SZ	256

#define FA_NAPT_HASH(v6, sip, dip, sp, dp, proto) \
({ \
	uint32 sum; \
	if (v6) { \
		sum = sip[0] + sip[1] + sip[2] + sip[3] + \
		dip[0] + dip[1] + dip[2] + dip[3] + \
		sp + dp + proto; \
	} else { \
		sum = sip[0] + dip[0] + sp + dp + proto; \
	} \
	sum = ((sum >> 16) + (sum & 0xffff)); \
	(((sum >> 8) + (sum & 0xff)) & (FA_NAPT_SZ - 1)); \
})

typedef struct {
	union {
#ifdef BIG_ENDIAN
		struct {
			uint32_t	op_code		:3; /* 31:29 */
			uint32_t	reserved	:5; /* 28:24 */
			uint32_t	cl_id		:8; /* 23:16 */
			uint32_t	reason_code	:8; /* 15:8  */
			uint32_t	tc		:3; /* 7:5   */
			uint32_t	src_pid		:5; /* 4:0   */
		} oc0;
		struct {
			uint32_t	op_code		:3; /* 31:29 */
			uint32_t	reserved	:2; /* 28:27 */
			uint32_t	all_bkts_full	:1; /* 26    */
			uint32_t	bkt_id		:2; /* 25:24 */
			uint32_t	napt_flow_id	:8; /* 23:16 */
			uint32_t	hdr_chk_result	:8; /* 15:8  */
			uint32_t	tc		:3; /* 7:5   */
			uint32_t	src_pid		:5; /* 4:0   */
		} oc10;
#else
		struct {
			uint32_t	src_pid		:5; /* 4:0   */
			uint32_t	tc		:3; /* 7:5   */
			uint32_t	reason_code	:8; /* 15:8  */
			uint32_t	cl_id		:8; /* 23:16 */
			uint32_t	reserved	:5; /* 28:24 */
			uint32_t	op_code		:3; /* 31:29 */
		} oc0;
		struct {
			uint32_t	src_pid		:5; /* 4:0   */
			uint32_t	tc		:3; /* 7:5   */
			uint32_t	hdr_chk_result	:8; /* 15:8  */
			uint32_t	napt_flow_id	:8; /* 23:16 */
			uint32_t	bkt_id		:2; /* 25:24 */
			uint32_t	all_bkts_full	:1; /* 26    */
			uint32_t	reserved	:2; /* 28:27 */
			uint32_t	op_code		:3; /* 31:29 */
		} oc10;
#endif /* BIG_ENDIAN */
		uint32_t word;
	};
} bcm_hdr_t;

typedef int (* print_buf_t)(void *buf, const char *f, ...);

typedef struct {
	print_buf_t		p;
	void			*b;
} print_hdl_t;

typedef struct {
	uint8			external;
	uint8			remap_mac[ETHER_ADDR_LEN];
} pool_entry_t;

typedef struct {
	uint8			l2_ftype;
	uint8			tag_op;
	uint16			vlan_tci;
	uint8			nh_mac[ETHER_ADDR_LEN];
} nhop_entry_t;

typedef struct {
	uint8			flist[CTF_MAX_NEXTHOP_TABLE_INDEX];	/* Free nh list */
	uint8			free_idx;				/* current free entry */
	uint16			ref[CTF_MAX_NEXTHOP_TABLE_INDEX];	/* No of napt references */
} next_hop_t;

typedef struct {
	uint32			napt_idx;	/* Index to hash table */
	uint32			hits;		/* hits counter */
	uint8			action;		/* Overwrite bit-0: IP, bit-1 PORT */
	uint8			tgt_dma;	/* on hit send to HOST:1 SWITCH:0 */
} napt_flow_t;

typedef struct fa_napt {
	uint8			pool_idx;	/* Index to pool table */
	uint8			nh_idx;		/* Index to next hop table (routed intf mac-addr) */
	struct ether_addr	dhost;		/* Destination MAC address */
	bool			v6;		/* IPv6 entry */
	napt_flow_t		nfi;		/* NAPT flow info */

	/* 5 tuple info */
	struct fa_napt		*next;		/* Pointer to napt entry */
	uint32			sip[IPADDR_U32_SZ];
	uint32			dip[IPADDR_U32_SZ];
	uint16			sp;
	uint16			dp;
	uint8			proto;
} fa_napt_t;

typedef struct fa_info {
	fa_t			pub;
	uint32			chiprev;	/* Chip rev, A0 ~ A3 */
	si_t			*sih;		/* SiliconBackplane handle */
	char			*vars;		/* nvram variables handle */
	void			*et;		/* pointer to os-specific private state */
	void			*robo;
	bool			enabled;
	void			*fadevid;	/* Ref to gmac connected to FA */
	fa_napt_t		**napt;		/* NAPT connection cache table */
	bool			ftable[CTF_MAX_FLOW_TABLE]; /* Indicate HW napt index used */
	uint8			acc_mode;	/* Flow accelarator mode */
	uint16			nflows;		/* Keep track of no o flfows in NAPT flow table */
	next_hop_t		nhi;		/* Next hop info */
	faregs_t		*regs;		/* FA/CTF register space address(virtual address) */

	void			*faimp_dev;	/* Specific for AUX device */
} fa_info_t;

static fa_info_t *aux_dev = NULL;
static fa_info_t *fa_dev = NULL;
static bool fa_fs_created = FALSE;

static bool
fa_corereg(fa_info_t *fai, uint unit)
{
	uint32 idx = si_coreidx(fai->sih);

	/* GMAC-2 connect FA but FA regs fall in GMAC-3 corereg space so using GMAC-3 as base. */
	if (unit == 2) {
		si_setcore(fai->sih, GMAC_CORE_ID, unit);
		fai->fadevid = (void *)si_addrspace(fai->sih, 0);
		if ((fai->regs = si_setcore(fai->sih, GMAC_CORE_ID, 3)))
			fai->regs = (faregs_t *) ((uint8 *)fai->regs + FA_BASE_OFFSET);

		ET_ERROR(("%s: FA reg:%p, Fadev:%p\n", __FUNCTION__, fai->regs, fai->fadevid));

		si_setcoreidx(fai->sih, idx);
	}

	return (fai->regs && fai->fadevid);
}

static void
fa_clr_all_int(fa_info_t *fai)
{
	osl_t *osh = si_osh(fai->sih);
	faregs_t *regs = fai->regs;

	/* Disable receiving table init done and parse inclomplete errors. */
	W_REG(osh, &regs->status_mask, 0);

	/* Disable receiving pkts for L2/L3 pkt errors */
	W_REG(osh, &regs->rcv_status_en, 0);

	/* disable interrupts on all error HWQ overflow conditions */
	W_REG(osh, &regs->error_mask, 0);
}

static int32
fa_setmode(fa_info_t *fai, uint8 mode)
{
	int32 ret = BCME_OK;
	uint32 val;
	osl_t *osh = si_osh(fai->sih);
	faregs_t *regs = fai->regs;

	if (fai->acc_mode == mode)
		goto out;

	/* Clear mode bits */
	val = R_REG(osh, &regs->control);
	val &= ~(CTF_CTL_SW_ACC_MODE | CTF_CTL_BYPASS_CTF);
	if (mode != CTF_FA_NORMAL)
		val |= (mode == CTF_FA_BYPASS) ? CTF_CTL_BYPASS_CTF : CTF_CTL_SW_ACC_MODE;
	val |= (CTF_CTL_DSBL_MAC_DA_CHECK | CTF_CTL_NAPT_FLOW_INIT |
		CTF_CTL_NEXT_HOP_INIT | CTF_CTL_HWQ_INIT |
		CTF_CTL_LAB_INIT | CTF_CTL_HB_INIT | CTF_CTL_CRC_OWRT);

	W_REG(osh, &regs->control, val);

	SPINWAIT(((R_REG(osh, &regs->status) & CTF_INTSTAT_INIT_DONE) != CTF_INTSTAT_INIT_DONE),
		50000);

	ASSERT((R_REG(osh, &regs->status) & CTF_INTSTAT_INIT_DONE) == CTF_INTSTAT_INIT_DONE);

	if ((R_REG(osh, &regs->status) & CTF_INTSTAT_INIT_DONE) == CTF_INTSTAT_INIT_DONE)
		fai->acc_mode = mode;
	else {
		ET_ERROR(("%s: failed fa_ctl(0x%x)!\n", __FUNCTION__, R_REG(osh, &regs->control)));
		ret = BCME_ERROR;
	}

out:
	return ret;
}

static void
fa_reset_next_hop_tbl(fa_info_t *fai)
{
	uint8 i;

	/* Init next hop free list */
	memset(fai->nhi.ref, 0, sizeof(fai->nhi.ref));
	for (i = 0; i < CTF_MAX_NEXTHOP_TABLE_INDEX; i++)
		fai->nhi.flist[i] = i+1;

	fai->nhi.free_idx = 0;
}

static int
fa_read_pool_entry(fa_info_t *fai, uint32 *tbl, uint32 idx)
{
	int32 ret = BCME_OK;

	CTF_FA_WAR777_ON(fai);

	/* Select Pool table to read  */
	SELECT_MACC_TABLE_RD(fai, CTF_MEMACC_TBL_NP, idx);

	/* Read NP entry */
	CTF_FA_MACC_RD(fai, tbl, 3);

	/* Mem-access complete? */
	SPINWAIT(((R_REG(si_osh(fai->sih), &fai->regs->dbg_status) &
		CTF_DBG_MEM_ACC_BUSY)), 10000);

	if (R_REG(si_osh(fai->sih), &fai->regs->dbg_status) & CTF_DBG_MEM_ACC_BUSY) {
		ET_ERROR(("%s MEM ACC Busy for 10ms\n", __FUNCTION__));
		ret = BCME_BUSY;
		goto done;
	}

done:
	CTF_FA_WAR777_OFF(fai);

	return ret;
}

static int
fa_read_nhop_entry(fa_info_t *fai, uint32 *tbl, uint32 idx)
{
	int32 ret = BCME_OK;

	CTF_FA_WAR777_ON(fai);

	/* Select Next hop table to read from  */
	SELECT_MACC_TABLE_RD(fai, CTF_MEMACC_TBL_NH, idx);

	/* Read NH entry */
	CTF_FA_MACC_RD(fai, tbl, 3);

	/* Mem-access complete? */
	SPINWAIT(((R_REG(si_osh(fai->sih), &fai->regs->dbg_status) &
		CTF_DBG_MEM_ACC_BUSY)), 10000);

	if (R_REG(si_osh(fai->sih), &fai->regs->dbg_status) & CTF_DBG_MEM_ACC_BUSY) {
		ET_ERROR(("%s MEM ACC Busy for 10ms\n", __FUNCTION__));
		ret = BCME_BUSY;
		goto done;
	}

done:
	CTF_FA_WAR777_OFF(fai);

	return ret;
}

static int
fa_write_nhop_entry(fa_info_t *fai, uint32 *tbl, uint32 idx)
{
	int32 ret = BCME_OK;

	CTF_FA_WAR777_ON(fai);

	/* Select NH table to write  */
	SELECT_MACC_TABLE_WR(fai, CTF_MEMACC_TBL_NH, idx);

	/* Write NH entry */
	CTF_FA_MACC_WR(fai, tbl, 3);

	/* Mem-access complete? */
	SPINWAIT(((R_REG(si_osh(fai->sih), &fai->regs->dbg_status) &
		CTF_DBG_MEM_ACC_BUSY)), 10000);

	if (R_REG(si_osh(fai->sih), &fai->regs->dbg_status) & CTF_DBG_MEM_ACC_BUSY) {
		ET_ERROR(("%s MEM ACC Busy for 10ms\n", __FUNCTION__));
		ret = BCME_BUSY;
		goto done;
	}

done:
	CTF_FA_WAR777_OFF(fai);

	return ret;
}

/* Look for next hop etry matching to nh->nh_mac,
 * if found update index in tb_info->nh_idx
 * NOTE: It must be called under ETC_FA_LOCK protection
 */
static int32
fa_find_nhop(fa_info_t *fai, fa_napt_t *tb_info, nhop_entry_t *nh)
{
	int i;
	uint8 idx = CTF_MAX_NEXTHOP_TABLE_INDEX;
	uint8 eaddr[ETHER_ADDR_LEN];
	uint32 d[3];
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif
	int32 ret = BCME_NOTFOUND;
	fa_napt_t *napt;

	/* If exist use correcponding npi->nh_idx to verify in HW table (double check). */
	for (i = 0; i < FA_NAPT_SZ; i++) {
		napt = FA_NAPT(fai)[i];
		while (napt != NULL) {
			if (ether_cmp(napt->dhost.octet, nh->nh_mac) == 0) {
				idx = napt->nh_idx;
				goto found;
			}

			napt = napt->next;
		}
	}

found:

	if (idx == CTF_MAX_NEXTHOP_TABLE_INDEX)
		idx = 0;

	if (idx >= CTF_MAX_NEXTHOP_TABLE_INDEX) {
		ET_ERROR(("%s NH MAC: %s found at invalid index %d\n", __FUNCTION__,
			bcm_ether_ntoa((struct  ether_addr *)nh->nh_mac, eabuf), idx));
		ASSERT(idx < CTF_MAX_NEXTHOP_TABLE_INDEX);
	}

	for (; idx < CTF_MAX_NEXTHOP_TABLE_INDEX; idx++) {
		if (fa_read_nhop_entry(fai, d, idx) == BCME_BUSY) {
			ET_ERROR(("%s Nhop read timeout!\n", __FUNCTION__));
			ret = BCME_BUSY;
			goto done;
		}

		/* Get DA from NH entry */
		CTF_FA_GET_NH_DA(eaddr, d);

		if (memcmp(nh->nh_mac, eaddr, ETHER_ADDR_LEN) == 0) {
			tb_info->nh_idx = (idx & (CTF_MAX_NEXTHOP_TABLE_INDEX - 1));
			fai->nhi.ref[tb_info->nh_idx]++;
			ret = BCME_OK;
			break;
		}
	}

done:

	return ret;
}

static int
fa_add_nhop_entry(fa_info_t *fai, fa_napt_t *tb_info, nhop_entry_t *nh)
{
	int32 ret = BCME_OK;
	uint32 d[3];

	if (PEEKNEXT_NHIDX(&fai->nhi) == NHOP_FULL) {
		ET_ERROR(("%s Out of next hop entries!\n", __FUNCTION__));
		ret = BCME_BUSY;
		goto done;
	}

	/* Look for existing pool entries */
	ret = fa_find_nhop(fai, tb_info, nh);

	if (ret == BCME_OK || ret == BCME_BUSY)
		goto done;

	/* Prepare NH entry */
	CTF_FA_SET_NH_ENTRY(d, nh->nh_mac, nh->l2_ftype, nh->tag_op, HTOL16(nh->vlan_tci));

	if ((ret = fa_write_nhop_entry(fai, d, PEEKNEXT_NHIDX(&fai->nhi))) == BCME_BUSY) {
		ET_ERROR(("%s Nhop write timeout!\n", __FUNCTION__));
		goto done;
	}

	tb_info->nh_idx = (GETNEXT_NHIDX(&fai->nhi) & (CTF_MAX_NEXTHOP_TABLE_INDEX - 1));
	fai->nhi.ref[tb_info->nh_idx]++;

done:
	return ret;
}

static int
fa_delete_nhop_entry(fa_info_t *fai, fa_napt_t *npi)
{
	int32 ret = BCME_OK;
	uint32 d[3];

	ASSERT(npi->nh_idx < CTF_MAX_NEXTHOP_TABLE_INDEX);

	if ((fai->nhi.flist[npi->nh_idx] != 0) || (fai->nhi.ref[npi->nh_idx] == 0)) {
		ET_ERROR(("%s unexpected idx:%d flist:%d ref:%d\n", __FUNCTION__,
		npi->nh_idx, fai->nhi.flist[npi->nh_idx], fai->nhi.ref[npi->nh_idx]));
		ASSERT((fai->nhi.flist[npi->nh_idx] == 0) && (fai->nhi.ref[npi->nh_idx] != 0));
	}

	if (fai->nhi.ref[npi->nh_idx])
		fai->nhi.ref[npi->nh_idx]--;

	if (!fai->nhi.ref[npi->nh_idx]) {
		/* clear nhop entry */
		memset(d, 0, sizeof(d));

		if ((ret = fa_write_nhop_entry(fai, d, npi->nh_idx)) == BCME_BUSY) {
			ET_ERROR(("%s Nhop write timeout!\n", __FUNCTION__));
			goto done;
		}

		PUTNEXT_NHIDX(&fai->nhi, npi->nh_idx);
	}
	npi->nh_idx = CTF_MAX_NEXTHOP_TABLE_INDEX;

done:
	return ret;
}

/* Look for entry matching to pt->remap_mac in pool table,
 * if found update index in tb_info->pool_idx
 */
static int32
fa_find_pool(fa_info_t *fai, fa_napt_t *tb_info, pool_entry_t *pt)
{
	uint8 i;
	uint8 eaddr[ETHER_ADDR_LEN];
	uint32 d[2];
	int32 ret = BCME_NOTFOUND;

	for (i = 0; i < CTF_MAX_POOL_TABLE_INDEX; i++) {
		/* select Pool table for read */
		SELECT_MACC_TABLE_RD(fai, CTF_MEMACC_TBL_NP, i);
		/* Read d0,d1 from NH table */
		CTF_FA_MACC_RD(fai, d, sizeof(d)/sizeof(d[0]));
		/* mem-access complete? */
		SPINWAIT(((R_REG(si_osh(fai->sih), &fai->regs->dbg_status) &
			CTF_DBG_MEM_ACC_BUSY)), 10000);

		if (R_REG(si_osh(fai->sih), &fai->regs->dbg_status) & CTF_DBG_MEM_ACC_BUSY) {
			ET_ERROR(("%s MEM ACC Busy for 10ms\n", __FUNCTION__));
			ret = BCME_BUSY;
			goto done;
		}

		/* Get DA from NH entry */
		CTF_FA_GET_NP_SA(eaddr, d);

		if ((memcmp(pt->remap_mac, eaddr, ETHER_ADDR_LEN) == 0) &&
			((d[0] & 0x1) == pt->external)) {
			tb_info->pool_idx = (i & (CTF_MAX_POOL_TABLE_INDEX - 1));
			ret = BCME_OK;
			break;
		}
	}
done:

	return ret;
}

static int
fa_add_pool_entry(fa_info_t *fai, fa_napt_t *tb_info, pool_entry_t *pe)
{
	int32 ret = BCME_OK;
	uint32 d[2];

	CTF_FA_WAR777_ON(fai);

	/* Look for existing pool entries */
	ret = fa_find_pool(fai, tb_info, pe);

	if (ret == BCME_OK || ret == BCME_BUSY)
		goto done;

	/* For now expecting only 2 entries, one each direction,
	 * Using ext/int as index.
	 */
	SELECT_MACC_TABLE_WR(fai, CTF_MEMACC_TBL_NP, pe->external);

	/* Prepare NH entry */
	CTF_FA_SET_NP_ENTRY(d, pe->remap_mac, pe->external);

	/* Write NH entry */
	CTF_FA_MACC_WR(fai, d, sizeof(d)/sizeof(d[0]));

	/* mem-access complete? */
	SPINWAIT(((R_REG(si_osh(fai->sih), &fai->regs->dbg_status) &
		CTF_DBG_MEM_ACC_BUSY)), 10000);

	if (R_REG(si_osh(fai->sih), &fai->regs->dbg_status) & CTF_DBG_MEM_ACC_BUSY) {
		ET_ERROR(("%s MEM ACC Busy for 10ms\n", __FUNCTION__));
		ret = BCME_BUSY;
		goto done;
	}
	tb_info->pool_idx = pe->external;

done:
	CTF_FA_WAR777_OFF(fai);

	return ret;
}

static int
fa_write_napt_entry(fa_info_t *fai, uint32 *tbl, uint32 idx)
{
	int32 ret = BCME_OK;

	CTF_FA_WAR777_ON(fai);

	/* select napt table to write  */
	SELECT_MACC_TABLE_WR(fai, CTF_MEMACC_TBL_NF, idx);

	/* Write napt entry */
	CTF_FA_MACC_WR(fai, tbl, CTF_DATA_SIZE);

	/* mem-access complete? */
	SPINWAIT(((R_REG(si_osh(fai->sih), &fai->regs->dbg_status) &
		CTF_DBG_MEM_ACC_BUSY)), 10000);

	if (R_REG(si_osh(fai->sih), &fai->regs->dbg_status) & CTF_DBG_MEM_ACC_BUSY) {
		ET_ERROR(("%s MEM ACC Busy for 10ms\n", __FUNCTION__));
		ret = BCME_BUSY;
		goto done;
	}

done:
	CTF_FA_WAR777_OFF(fai);

	return ret;
}

static int
fa_read_napt_entry(fa_info_t *fai, uint32 *tbl, uint32 idx)
{
	int32 ret = BCME_OK;

	CTF_FA_WAR777_ON(fai);

	/* select napt table to write  */
	SELECT_MACC_TABLE_RD(fai, CTF_MEMACC_TBL_NF, idx);

	/* Read napt entry */
	CTF_FA_MACC_RD(fai, tbl, CTF_DATA_SIZE);

	/* mem-access complete? */
	SPINWAIT(((R_REG(si_osh(fai->sih), &fai->regs->dbg_status) &
		CTF_DBG_MEM_ACC_BUSY)), 10000);

	if (R_REG(si_osh(fai->sih), &fai->regs->dbg_status) & CTF_DBG_MEM_ACC_BUSY) {
		ET_ERROR(("%s MEM ACC Busy for 10ms\n", __FUNCTION__));
		ret = BCME_BUSY;
		goto done;
	}

done:
	CTF_FA_WAR777_OFF(fai);

	return ret;
}

static void
fa_napt_prep_ipv4_word(fa_info_t *fai, fa_napt_t *napt, fa_napt_ioctl_t *par, uint32 *tbl)
{
	uint external = (par->external == ETFA_NP_INTERNAL) ? CTF_NP_INTERNAL : CTF_NP_EXTERNAL;
	memset(tbl, 0, sizeof(uint32) * CTF_DATA_SIZE);

	tbl[1] = (napt->nfi.action << 3) | (napt->nfi.tgt_dma << 5) | (napt->pool_idx << 6) |
		(napt->nh_idx << 8) | ((NTOH32(par->nat_ip[0]) & 0x1FFFF) << 15);
	tbl[2] = ((NTOH32(par->nat_ip[0]) & 0xFFFE0000) >> 17) | (NTOH16(par->nat_port) << 15) |
		((NTOH16(par->dp) & 0x1) << 31);
	tbl[3] = ((NTOH16(par->dp) & 0xFFFE) >> 1) | (NTOH16(par->sp) << 15) |
		((par->proto == 6) << 31);
	tbl[4] = NTOH32(par->dip[0]);
	tbl[5] = NTOH32(par->sip[0]);
	/* LAN->WAN (INTERNAL), WAN->LAN(EXTERNAL), this is reverse of
	 * external entry we populate in pool entry
	 */
	tbl[6] = external | (1 << 20);
	/* Set ipv4_entry=1 */
	tbl[7] = (1 << 31);
}


/* NOTE: It must be called under ETC_FA_LOCK protection */
static int32
_fa_napt_del(fa_info_t *fai, fa_napt_t *napt)
{
	int32 ret = BCME_OK;
	uint32 tbl[CTF_DATA_SIZE];
	uint8 vidx;

	if (!fai || !napt)
		return BCME_ERROR;

	/* Fetch napt flow entry, ASSERT its valid
	 * If valid, mark valid=0 and write the entry back.
	 * Check the ref count on related nhopt entry, ifi
	 * its 1 then delete nhop entry as well.
	 */
	if ((ret = fa_read_napt_entry(fai, tbl, napt->nfi.napt_idx)) == BCME_BUSY) {
		ET_ERROR(("%s Read napt flow stuck!\n", __FUNCTION__));
		goto done;
	}

	vidx = (napt->v6) ? 7 : 6;
	if (!(tbl[vidx] & (1 << 20))) {
		ET_ERROR(("%s Deleting invalid entry from NAPT flow naptidx:%d"
			" tbl[%d]:0x%x\n", __FUNCTION__, napt->nfi.napt_idx, vidx, tbl[vidx]));
		goto done;
	}

	/* Mark invalid */
	tbl[vidx] &= ~(1 << 20);

	if ((ret = fa_write_napt_entry(fai, tbl, napt->nfi.napt_idx)) == BCME_BUSY) {
		ET_ERROR(("%s Write napt flow stuck!\n", __FUNCTION__));
		goto done;
	}

	ET_TRACE(("%s NAPT entry deleted for hidx:%d\n", __FUNCTION__, napt->nfi.napt_idx));
	fa_delete_nhop_entry(fai, napt);

done:

	return ret;
}

static int32
fa_down(fa_info_t *fai)
{
	int i;
	fa_napt_t *napt, *next;
	int32 ret = BCME_OK;
	osl_t *osh = si_osh(fai->sih);
	faregs_t *regs = fai->regs;

	if (fai->acc_mode == CTF_FA_BYPASS) {
		ET_TRACE(("%s: Already down!\n", __FUNCTION__));
		goto done;
	}

	/* Set to BYPASS mode */
	if ((ret = fa_setmode(fai, CTF_FA_BYPASS)) != BCME_OK)
		goto done;

	/* In BYPASS mode disable BCM HDR in FA, but make sure on
	 * ROBO switch BRCM_HDR_CTL(page:0x2, Off:0x3) clear
	 * related bit for IMP port.
	 */
	W_REG(osh, &regs->bcm_hdr_ctl, 0);

	/* clear L2 skip control */
	W_REG(osh, &regs->l2_skip_ctl, 0);

	/* Init L3 NAPT ctl */
	W_REG(osh, &regs->l3_napt_ctl, 0);

	/* No IPV4 checksum */
	W_REG(osh, &regs->l3_ipv4_type, 0);

	/* Flush all FA-ipc entries */
	if (fai->nflows) {
		ETC_FA_LOCK(fai);
		for (i = 0; i < FA_NAPT_SZ; i++) {
			napt = FA_NAPT(fai)[i];
			while (napt != NULL) {
				_fa_napt_del(fai, napt);
				fai->nflows--;

				next = napt->next;
				MFREE(si_osh(fai->sih), napt, sizeof(fa_napt_t));
				napt = next;
			}
			FA_NAPT(fai)[i] = NULL;
		}
		ETC_FA_UNLOCK(fai);
	}

	fai->pub.flags &= ~(FA_BCM_HDR_RX | FA_BCM_HDR_TX);

done:
	return ret;
}

static int32
fa_up(fa_info_t *fai, uint8 mode)
{
	uint32 val;
	int32 ret = BCME_OK;
	osl_t *osh = si_osh(fai->sih);
	faregs_t *regs = fai->regs;

	if (fai->acc_mode == mode) {
		ET_TRACE(("%s: Already in same mode !\n", __FUNCTION__));
		goto done;
	}

	if (mode == CTF_FA_BYPASS) {
		fa_down(fai);
		goto done;
	}

	/* For now allow only Normal-Mode, SW-Accel-mode(TBD) */
	if (mode == CTF_FA_SW_ACC) {
		ET_ERROR(("%s: SW accelerated mode not supported!\n", __FUNCTION__));
		ret = BCME_BADARG;
		goto done;
	}

	/* Enable FA mode and re-init tables */
	if ((ret = fa_setmode(fai, mode)) != BCME_OK)
		goto done;

	fa_clr_all_int(fai);

	/* Init BRCM hdr control */
	val = (CTF_BRCM_HDR_PARSE_IGN_EN | CTF_BRCM_HDR_HW_EN |
		CTF_BRCM_HDR_SW_RX_EN | CTF_BRCM_HDR_SW_TX_EN);
	W_REG(osh, &regs->bcm_hdr_ctl, val);

	fai->pub.flags |= (FA_BCM_HDR_RX | FA_BCM_HDR_TX);

	/* Init L2 skip control */
	val = CTF_L2SKIP_ET_TO_SNAP_CONV;
	W_REG(osh, &regs->l2_skip_ctl, val);

	/* Init L3 NAPT ctl */
	val = R_REG(osh, &regs->l3_napt_ctl);
	val &= ~(CTFCTL_L3NAPT_HITS_CLR_ON_RD_EN | CTFCTL_L3NAPT_TIMESTAMP);
	val |= (CTFCTL_L3NAPT_HASH_SEL | htons(0x4321));
	W_REG(osh, &regs->l3_napt_ctl, val);

	/* Enable IPV4 checksum */
	val = R_REG(osh, &regs->l3_ipv4_type);
	val |= CTF_L3_IPV4_CKSUM_EN;
	W_REG(osh, &regs->l3_ipv4_type, val);

	fa_reset_next_hop_tbl(fai);

done:
	return ret;
}

static int
fa_dump_nf_entry(fa_info_t *fai, uint32 napt_idx, print_hdl_t *pr)
{
	uint32 tbl[CTF_DATA_SIZE];
	uint32 tbl1[CTF_DATA_SIZE];
	bool	valid_ipv6, v4;
	print_buf_t prnt;
	void	*b;
	ASSERT(pr && pr->p && pr->b);

	prnt = pr->p;
	b = pr->b;

	fa_read_napt_entry(fai, tbl, napt_idx);

	v4 = (tbl[7] & (1 << 31));
	valid_ipv6 = v4 ? FALSE:(tbl[7] & (1 << 20));

	prnt(b, "%-4.03x ", napt_idx);
	if (!valid_ipv6) {
		prnt(b, "%-35.08x %-35.08x ", LTOH32(tbl[5]), LTOH32(tbl[4]));
	} else {
		napt_idx++;
		fa_read_napt_entry(fai, tbl1, napt_idx);
		prnt(b, "%08x.%08x.%08x.%08x ", tbl[7], tbl[6], tbl[5], tbl[4]);
		prnt(b, "%08x.%08x.%08x.%08x ", tbl[3], tbl[2], tbl[1], tbl[0]);
	}
	prnt(b, "%-4s ", (tbl[3] >> 31) ? "tcp":"udp");
	prnt(b, "%-5.04x ", (LTOH16(tbl[3]) >> 15) & 0xFFFF);
	prnt(b, "%-5.04x ", ((LTOH16(tbl[3]) & 0x7FFF) << 1) | (LTOH16(tbl[2]) >> 31));
	prnt(b, "%-3.01x ", CTF_FA_MACC_DATA0_TS(tbl));
	prnt(b, "%08x ", ((LTOH16(tbl[1]) & 0x7) << 29) | (LTOH16(tbl[0]) >> 3));
	prnt(b, "%-4s ", (tbl[6] & CTF_NP_EXTERNAL) ? "2lan":"2wan");
	prnt(b, "%-5.04x ", (LTOH16(tbl[2]) >> 15) & 0xFFFF);

	if (valid_ipv6) {
		uint32 ipv6[4];
		ipv6[0] = ((tbl[2] & 0xEFFF) | (tbl[1] >> 15));
		ipv6[1] = ((tbl[3] & 0xEFFF) | (tbl[2] >> 15));
		ipv6[2] = ((tbl[4] & 0xEFFF) | (tbl[3] >> 15));
		ipv6[3] = ((tbl[5] & 0xEFFF) | (tbl[4] >> 15));
		prnt(b, "%08x.%08x.%08x.%08x ", ipv6[3], ipv6[2], ipv6[1], ipv6[0]);
	} else {
		prnt(b, "%-35.08x ", ((LTOH32(tbl[2]) & 0x7FFF) << 17) | (LTOH32(tbl[1]) >> 15));
	}
	prnt(b, "%-5.01x ", (v4 ? ((tbl[6] >> 20) & 1):((tbl[7] >> 20) & 1)));
	prnt(b, "%-4.01x ", v4);
	prnt(b, "%-4.02x ", (tbl[1] >> 8) & 0x7F);
	prnt(b, "%-4.01x ", (tbl[1] >> 6) & 0x3);
	prnt(b, "%-3.01x ", (tbl[1] >> 5) & 0x1);
	prnt(b, "%x\n", (tbl[1] >> 3) & 0x3);

	return napt_idx;
}

static void
fa_dump_nh_entry(fa_info_t *fai, uint32 nhidx, print_hdl_t *pr)
{
	uint32 tbl[CTF_DATA_SIZE];
	uint8 eaddr[ETHER_ADDR_LEN];
	char eabuf[ETHER_ADDR_STR_LEN];
	print_buf_t prnt = pr->p;
	void *b = pr->b;

	fa_read_nhop_entry(fai, tbl, nhidx);

	CTF_FA_GET_NH_DA(eaddr, tbl);
	prnt(b, "%-5.02x ", nhidx);
	prnt(b, "%-17s ", bcm_ether_ntoa((struct  ether_addr *)eaddr, eabuf));
	prnt(b, "%-8.04x ",  (LTOH16(tbl[0]) >> 3) & 0xFFFF);
	prnt(b, "%-6.01x ", (tbl[0] >> 1) & 0x3);
	prnt(b, "%d\n", tbl[0] & 0x1);
}

static void
fa_dump_np_entry(fa_info_t *fai, uint32 pool_idx, print_hdl_t *pr)
{
	uint32 tbl[CTF_DATA_SIZE];
	uint8 eaddr[ETHER_ADDR_LEN];
	char eabuf[ETHER_ADDR_STR_LEN];
	print_buf_t prnt = pr->p;
	void *b = pr->b;

	fa_read_pool_entry(fai, tbl, pool_idx);

	CTF_FA_GET_NP_SA(eaddr, tbl);
	prnt(b, "%-8.01x ", pool_idx);
	prnt(b, "%-17s ", bcm_ether_ntoa((struct  ether_addr *)eaddr, eabuf));
	prnt(b, "%d\n", (tbl[0] & 0x1));
}

static void
fa_dump_entries(fa_info_t *fai, uint8 opt, print_buf_t prnt, void *b)
{
	int i;
	print_hdl_t pr = {prnt, b};
	fa_napt_t *napt;

	switch (opt) {
	case CTF_FA_DUMP_ALL_NF:
	{
		prnt(b, "\n===== FLOW TABLE DUMP ======\n");
		PRINT_FLOW_TABLE_HDR(prnt, b);
		for (i = 0; i < CTF_MAX_FLOW_TABLE; i++)
			i = fa_dump_nf_entry(fai, i, &pr);

		break;
	}
	case CTF_FA_DUMP_ALL_NH:
	{
		prnt(b, "\n===== NEXT HOP TABLE DUMP ======\n");
		PRINT_NHOP_TABLE_HDR(prnt, b);
		for (i = 0; i < CTF_MAX_NEXTHOP_TABLE_INDEX; i++)
			fa_dump_nh_entry(fai, i, &pr);

		break;
	}
	case CTF_FA_DUMP_ALL_NP:
	{
		prnt(b, "\n===== POOL TABLE DUMP ======\n");
		PRINT_POOL_TABLE_HDR(prnt, b);
		for (i = 0; i < CTF_MAX_POOL_TABLE_INDEX; i++)
			fa_dump_np_entry(fai, i, &pr);

		break;
	}
	case CTF_FA_DUMP_VALID_NF:
	case CTF_FA_DUMP_VALID_NH:
	case CTF_FA_DUMP_VALID_NP:
	{
		/* next hop idx 0~127, pool idx 0~3 */
		int8 valid_idx[CTF_MAX_NEXTHOP_TABLE_INDEX] = { -1 };

		if (opt == CTF_FA_DUMP_VALID_NF) {
			prnt(b, "\n===== FLOW TABLE VALID ENTRIES ======\n");
			PRINT_FLOW_TABLE_HDR(prnt, b);
		} else if (opt == CTF_FA_DUMP_VALID_NH) {
			prnt(b, "\n===== NEXT HOP TABLE VALID ENTRIES ======\n");
			PRINT_NHOP_TABLE_HDR(prnt, b);
		} else if (opt == CTF_FA_DUMP_VALID_NP) {
			prnt(b, "\n===== POOL TABLE VALID ENTRIES ======\n");
			PRINT_POOL_TABLE_HDR(prnt, b);
		}

		ETC_FA_LOCK(fai);

		memset(valid_idx, -1, sizeof(valid_idx));
		for (i = 0; i < FA_NAPT_SZ; i++) {
			napt = FA_NAPT(fai)[i];
			while (napt != NULL) {
				switch (opt) {
				case CTF_FA_DUMP_VALID_NF:
					fa_dump_nf_entry(fai, napt->nfi.napt_idx, &pr);
					break;
				case CTF_FA_DUMP_VALID_NH:
					if (valid_idx[napt->nh_idx] == -1) {
						fa_dump_nh_entry(fai, napt->nh_idx, &pr);
						valid_idx[napt->nh_idx] = napt->nh_idx;
					}
					break;
				case CTF_FA_DUMP_VALID_NP:
					if (valid_idx[napt->pool_idx] == -1) {
						fa_dump_np_entry(fai, napt->pool_idx, &pr);
						valid_idx[napt->pool_idx] = napt->pool_idx;
					}
					break;
				default:
					break;
				}

				napt = napt->next;
			}
		}

		ETC_FA_UNLOCK(fai);

		break;
	}
	default:
		ET_ERROR(("%s Unexpection option\n", __FUNCTION__));
		break;
	}
}

#ifdef BCMDBG
struct fareg_desc {
	uint8 *fname;
	uint8 *reg_name;
	uint32 reg_off;
};

static struct fareg_desc fareg[] = {
	{"control", "CTF_CONTROL", OFFSETOF(faregs_t, control)},
	{"mem_acc_ctl", "CTF_MEM_ACC_CONTROL", OFFSETOF(faregs_t, mem_acc_ctl)},
	{"bcm_hdr_ctl", "CTF_BRCM_HDR_CONTROL", OFFSETOF(faregs_t, bcm_hdr_ctl)},
	{"l2_skip_ctl", "CTF_L2_SKIP_CONTROL", OFFSETOF(faregs_t, l2_skip_ctl)},
	{"l2_tag", "CTF_L2_TAG_TYPE", OFFSETOF(faregs_t, l2_tag)},
	{"l2_llc_max_len", "CTF_L2_LLC_MAX_LENGTH", OFFSETOF(faregs_t, l2_llc_max_len)},
	{"l2_snap_typelo", "CTF_L2_LLC_SNAP_TYPE_LO", OFFSETOF(faregs_t, l2_snap_typelo)},
	{"l2_snap_typehi", "CTF_L2_LLC_SNAP_TYPE_HI", OFFSETOF(faregs_t, l2_snap_typehi)},
	{"l2_ethtype", "CTF_ETHERTYPE", OFFSETOF(faregs_t, l2_ethtype)},
	{"l3_ipv6_type", "CTF_L3_IPV6_TYPE", OFFSETOF(faregs_t, l3_ipv6_type)},
	{"l3_ipv4_type", "CTF_L3_IPV4_TYPE", OFFSETOF(faregs_t, l3_ipv4_type)},
	{"l3_napt_ctl", "CTF_L3_NAPT_CONTROL", OFFSETOF(faregs_t, l3_napt_ctl)},
	{"status", "CTF_STATUS", OFFSETOF(faregs_t, status)},
	{"status_mask", "CTF_STATUS_MASK", OFFSETOF(faregs_t, status_mask)},
	{"rcv_status_en", "CTF_RECV_STATUS_EN", OFFSETOF(faregs_t, rcv_status_en)},
	{"error", "CTF_ERROR", OFFSETOF(faregs_t, error)},
	{"error_mask", "CTF_ERR_MASK", OFFSETOF(faregs_t, error_mask)},
	{"dbg_ctl", "CTF_DBG_CONTROL", OFFSETOF(faregs_t, dbg_ctl)},
	{"dbg_status", "CTF_DBG_STATUS", OFFSETOF(faregs_t, dbg_status)},
	{"mem_dbg", "CTF_MEM_DBG", OFFSETOF(faregs_t, mem_dbg)},
	{"ecc_dbg", "CTF_ECC_DBG", OFFSETOF(faregs_t, ecc_dbg)},
	{"ecc_error", "CTF_ECC_ERROR", OFFSETOF(faregs_t, ecc_error)},
	{"ecc_error_mask", "CTF_ECC_ERR_MASK", OFFSETOF(faregs_t, ecc_error_mask)},
	{"hwq_max_depth", "CTF_HWQ_MAX_DEPTH", OFFSETOF(faregs_t, hwq_max_depth)},
	{"lab_max_depth", "CTF_LAB_MAX_DEPTH", OFFSETOF(faregs_t, lab_max_depth)},
	{"stats", "CTF_STATS", OFFSETOF(faregs_t, stats)},
	{"eccst", "CTF_ECC_STATS", OFFSETOF(faregs_t, eccst)},
	{"m_accdata", "CTF_MEM_ACC_DATA", OFFSETOF(faregs_t, m_accdata)},
	{"all", "ALL", 0xFFFF}
};
#endif /* BCMDBG */

static uint32
fa_chip_rev(si_t *sih)
{
	uint32 rev = 0;

	if (BCM4707_CHIP(CHIPID(sih->chip))) {
		uint32 *srab_base;

		/* Get chip revision */
		srab_base = (uint32 *)REG_MAP(CHIPCB_SRAB_BASE, SI_CORE_SIZE);
		W_REG(si_osh(sih),
			(uint32 *)((uint32)srab_base + CHIPCB_SRAB_CMDSTAT_OFFSET), 0x02400001);
		rev = R_REG(si_osh(sih),
			(uint32 *)((uint32)srab_base + CHIPCB_SRAB_RDL_OFFSET)) & 0x3;
		REG_UNMAP(srab_base);
	}

	return rev;
}

static int
fa_devices_qualify(si_t *sih, int unit, bool *isaux)
{
	int ret = 0, val = 0;
	bool aux = FALSE;

	/* Two cases on FA board:
	 * 1. FA bypass mode: Ignore GMAC-0/1
	 * 2. FA normal mode: Ignore GMAC1 only, GAMC0 will be used as AUX input
	 * 3. Bypass the et0macaddr checking when ctf_fa_mode = 2
	 */
	/* Ignore !FA and !AUX unit when ctf_fa_mode = 2 */
	if (FA_CAPABLE(fa_chip_rev(sih)) && !FA_FA_UNIT(unit) &&
	    (val = getintvar(NULL, "ctf_fa_mode")) != 0) {
		if (val != 2 || !FA_AUX_UNIT(unit)) {
			ET_ERROR(("et%d: FA present, ignore it\n", unit));
			ret = -1;
			goto done;
		}
		aux = TRUE;
	}

done:
	if (isaux)
		*isaux = aux;

	return ret;
}

static fa_napt_t *
fa_napt_lkup_ll(fa_info_t *fai, bool v6, uint32 *sip, uint32 *dip, uint8 proto,
	uint16 sp, uint16 dp)
{
	uint32 hash;
	fa_napt_t *napt;

	hash = FA_NAPT_HASH(v6, sip, dip, proto, sp, dp);

	/* IP cache lookup */
	napt = FA_NAPT(fai)[hash];
	while (napt != NULL) {
		if (FA_NAPT_TPL_CMP(napt, v6, sip, dip, proto, sp, dp))
			return (napt);
		napt = napt->next;
	}

	return (NULL);
}

/*
 * Create and intialize fa instance
 */

int
fa_probe(si_t *sih, int unit)
{
	if (!fa_fs_created && FA_CAPABLE(fa_chip_rev(sih))) {
		et_fa_fs_create();
		fa_fs_created = TRUE;
	}

	/* By pass fa devices quality if FA nvram settings not ready */
	if (!FA_NVRAM_OVERRIDDEN())
		return 0;

	return fa_devices_qualify(sih, unit, NULL);
}

fa_t *
fa_attach(si_t *sih, void *et, char *vars, uint unit, void *robo)
{
	fa_info_t *fai = NULL;
	bool is_auxdev = FALSE;

	/* By pass fa attach if FA nvram settings are not ready */
	if (!FA_NVRAM_OVERRIDDEN() || getintvar(NULL, "ctf_fa_mode") == 0)
		return NULL;

	if (fa_devices_qualify(sih, unit, &is_auxdev) != 0)
		return NULL;

	/* Allocate private info structure */
	if ((fai = MALLOC(si_osh(sih), sizeof(fa_info_t))) == NULL) {
		ET_ERROR(("%s: out of memory, malloced %d bytes", __FUNCTION__,
		          MALLOCED(si_osh(sih))));
		return NULL;
	}
	bzero((char *)fai, sizeof(fa_info_t));

	fai->et = et;
	fai->sih = sih;
	fai->vars = vars;
	fai->chiprev = fa_chip_rev(sih);
	fai->robo = robo;

	if (is_auxdev) {
		ASSERT(aux_dev == NULL);

		aux_dev = fai;
		fai->pub.flags |= FA_AUX_DEV;

		if (fa_dev)
			fai->faimp_dev = et_fa_get_fa_dev(fa_dev->et);

		robo_fa_aux_init(fai->robo);

		goto aux_done;
	}

	if (!fa_corereg(fai, unit)) {
		MFREE(si_osh(sih), fai, sizeof(fa_info_t));
		ET_ERROR(("%s: FA regs dev not found\n", __FUNCTION__));
		return NULL;
	}

	/* A2 chipid need to enable this WAR selectively. */
	if (BCM4707_CHIP(CHIPID(sih->chip)) && fai->chiprev == 2) {
			fai->pub.flags |= FA_777WAR_ENABLED;
			ET_ERROR(("%s: Enabling FA WAR CHIPID(0x%x) CHIPREV(0x%x)\n",
				__FUNCTION__, CHIPID(sih->chip), CHIPREV(sih->chiprev)));
	}

	if ((fai->napt = MALLOC(si_osh(sih), (FA_NAPT_SZ * sizeof(fa_napt_t *)))) == NULL) {
			ET_ERROR(("%s: napt malloc failed\n", __FUNCTION__));
			MFREE(si_osh(fai->sih), fai, sizeof(fa_info_t));
			return NULL;
	}
	bzero((char *)fai->napt, (FA_NAPT_SZ * sizeof(fa_napt_t *)));

	ETC_FA_LOCK_INIT(fai);

	ASSERT(fa_dev == NULL);

	fa_dev = fai;
	fai->pub.flags |= FA_FA_DEV;

	if (aux_dev) {
		aux_dev->faimp_dev = et_fa_get_fa_dev(fa_dev->et);

		/* Do it again otherwise robo_attach for FA interface will disable it */
		robo_fa_aux_init(fai->robo);
	}

aux_done:
	return (&fai->pub);
}

void
fa_detach(fa_t *fa)
{
	fa_info_t *fai = (fa_info_t *)fa;

	if (FA_IS_FA_DEV(fa))
		fa_down(fai);

	if (fai->napt)
		MFREE(si_osh(fai->sih), fai->napt, (FA_NAPT_SZ * sizeof(fa_napt_t *)));

	MFREE(si_osh(fai->sih), fai, sizeof(fa_info_t));

	if (fa_fs_created) {
		et_fa_fs_clean();
		fa_fs_created = FALSE;
	}
}

/*
 * Enable fa function on this interface.
 */
int
fa_enable_device(fa_t *fa)
{
	int mode;
	fa_info_t *fai = (fa_info_t *)fa;

	if (FA_IS_AUX_DEV(fa))
		return 0;

	/* If FA is present check what mode(bypass/normal) to operate in.
	 * NOTE: For now support bypass and normal mode alone.
	 * TBD: SW-Accelerated mode.
	 */
	mode = getintvar(fai->vars, "ctf_fa_mode");

	ASSERT(mode != 0);
	if (mode != CTF_FA_BYPASS && mode != CTF_FA_NORMAL) {
		ET_ERROR(("%s: FA mode expected (%d) or (%d) found (%d)\n",
			__FUNCTION__, CTF_FA_BYPASS, CTF_FA_NORMAL, mode));
		return -1;
	}

	fa_up(fai, mode);

	/* Update the state to enabled/disabled */
	fai->enabled = TRUE;

	return 0;
}

void *
fa_process_tx(fa_t *fa, void *p)
{
	fa_info_t *fai = (fa_info_t *)fa;
	osl_t *osh = si_osh(fai->sih);

	BCM_REFERENCE(osh);

	if (PKTHEADROOM(osh, p) < 4)
		p = PKTEXPHEADROOM(osh, p, 4);

	/* For now always opcode 0x0 */
	PKTPUSH(osh, p, 4);
	memset(PKTDATA(osh, p), 0, 4);
	if (PKTLEN(osh, p) < 68)
		PKTSETLEN(osh, p, 68);

	return p;
}

void
fa_process_rx(fa_t *fa, void *p)
{
	bcm_hdr_t bhdr;
	uint8 pull = 4;
	fa_info_t *fai = (fa_info_t *)fa;

	BCM_REFERENCE(fai);

	if (FA_IS_AUX_DEV(fa)) {
		PKTSETFADEV(p, fai->faimp_dev);
		PKTSETFAAUX(p);
		PKTSETFAHIDX(p, BCM_FA_INVALID_IDX_VAL);
		return;
	}

	bhdr.word = ntohl(*((uint32 *)PKTDATA(si_osh(fai->sih), p)));
	if (bhdr.oc10.op_code == 0x2) {
		uint32 nid = (bhdr.oc10.napt_flow_id * 4) + bhdr.oc10.bkt_id;

		if (bhdr.oc10.all_bkts_full) {
			ET_ERROR(("%s All bkts full, leave to SW CTF\n", __FUNCTION__));
			nid = BCM_FA_INVALID_IDX_VAL;
		}
		PKTSETFAHIDX(p, nid);
	}

	if (bhdr.oc10.op_code == 0x1) {
		pull = 8;
		ET_ERROR(("%s Rcvd 8-byte  brcm-hdr\n", __FUNCTION__));
	}

	PKTPULL(si_osh(fai->sih), p, pull);
}

int32
fa_napt_add(fa_t *fa, fa_napt_ioctl_t *par)
{
	uint32 hash, napt_idx;
	uint32 *sip, *dip;
	uint16 sp, dp;
	uint8 proto;

	int32 ret = BCME_OK;
	nhop_entry_t nh;
	pool_entry_t pe;
	fa_napt_t *napt = NULL;
	uint32 tbl[CTF_DATA_SIZE];
	fa_info_t *fai = (fa_info_t *)fa;

	/* Validate the input params */
	if (fai == NULL || par == NULL)
		return BCME_ERROR;

	/* If FA is in bypass mode, no use adding entries */
	if (fai->acc_mode != CTF_FA_NORMAL)
		return BCME_OK;

	/* Are both TX/RX FA capable */
	if (!FA_IS_FA_DEV(fa) || par->txif != par->rxif)
		return BCME_ERROR;

	napt_idx = PKTGETFAHIDX(par->pkt);
	if (napt_idx == BCM_FA_INVALID_IDX_VAL) {
		ET_TRACE(("%s Invalid hash and bkt idx!\n", __FUNCTION__));
		return BCME_ERROR;
	}

	ASSERT(napt_idx < CTF_MAX_FLOW_TABLE);

	sip = par->sip;
	dip = par->dip;
	proto = par->proto;
	sp = par->sp;
	dp = par->dp;

	ETC_FA_LOCK(fai);

	if (fai->ftable[napt_idx]) {
		ET_TRACE(("%s flow table index %d has been used!\n", __FUNCTION__, napt_idx));
		ret = BCME_ERROR;
		goto err;
	}

	if (fa_napt_lkup_ll(fai, par->v6, sip, dip, proto, sp, dp) != NULL) {
		ET_TRACE(("%s: Adding duplicate entry\n", __FUNCTION__));
		ret = BCME_ERROR;
		goto err;
	}

	if ((napt = MALLOC(si_osh(fai->sih), sizeof(fa_napt_t))) == NULL) {
		ET_ERROR(("%s: out of memory, malloced %d bytes", __FUNCTION__,
		          MALLOCED(si_osh(fai->sih))));
		ret = BCME_ERROR;
		goto err;
	}
	bzero((char *)napt, sizeof(fa_napt_t));

	napt->nfi.napt_idx = napt_idx;
	memcpy(napt->dhost.octet, par->dhost.octet, ETH_ALEN);

	/* Next hop settings */
	nh.l2_ftype = 0;
	nh.tag_op = (par->tag_op == ETFA_NH_OP_CTAG) ? CTF_NH_OP_CTAG : CTF_NH_OP_NOTAG;
	/* For now deriving vlan prio from ip_tos
	 * should be ok, as our switch has fixmap from tos -> VLAN_PCP
	 */
	nh.vlan_tci = (((par->tos >> IPV4_TOS_PREC_SHIFT) & VLAN_PRI_MASK) << VLAN_PRI_SHIFT);
	nh.vlan_tci |= par->vid & VLAN_VID_MASK;
	ether_copy(par->dhost.octet, nh.nh_mac);

	/* Decide direction in pool entry, at this point all we know is it
	 * should be reverse of whats is in corresponding flow entry.
	 */
	pe.external = (par->external == ETFA_NP_INTERNAL) ? CTF_NP_EXTERNAL : CTF_NP_INTERNAL;
	ether_copy(par->shost.octet, pe.remap_mac);

	/* Add(or get if already present) next-hop and pool table entries */
	if (((ret = fa_add_nhop_entry(fai, napt, &nh)) == BCME_BUSY) ||
		((ret = fa_add_pool_entry(fai, napt, &pe)) == BCME_BUSY))
		goto err;

	napt->nfi.action = CTF_NAPT_OVRW_IP;
	if (((par->external == ETFA_NP_EXTERNAL) && (par->sp != par->nat_port)) ||
	    ((par->external == ETFA_NP_INTERNAL) && (par->dp != par->nat_port)))
		napt->nfi.action |= CTF_NAPT_OVRW_PORT;
	napt->nfi.tgt_dma = 0;

	/* Add Napt entry if not exist */
	if (!par->v6) {
		/* prepare napt ipv4 entry */
		fa_napt_prep_ipv4_word(fai, napt, par, tbl);
	} else {
		/* For now IPV6 NAT is not supported in SW CTF as well,
		 * might not be required, so disabling this part untill
		 * we find need to enable, and ofcourse make required changes
		 * in SW CTF to save NAT IPV6 IP.
		 */
		ET_ERROR(("%s Ignore NAPT entry add for IPV6\n", __FUNCTION__));
		ret = BCME_ERROR;
		goto err;
	}

	ET_TRACE(("%s NAPT entry add for napt idx:%d\n", __FUNCTION__, napt->nfi.napt_idx));
	if ((ret = fa_write_napt_entry(fai, tbl, napt->nfi.napt_idx)) == BCME_BUSY)
		goto err;

	/* Save 5 tuple info */
	memcpy(napt->sip, par->sip, sizeof(napt->sip));
	memcpy(napt->dip, par->dip, sizeof(napt->dip));
	napt->sp = par->sp;
	napt->dp = par->dp;
	napt->proto = par->proto;

	hash = FA_NAPT_HASH(par->v6, sip, dip, proto, sp, dp);
	napt->next = FA_NAPT(fai)[hash];
	FA_NAPT(fai)[hash] = napt;

	fai->ftable[napt_idx] = TRUE;
	fai->nflows++;

err:

	if (ret != BCME_OK && napt)
		MFREE(si_osh(fai->sih), napt, sizeof(fa_napt_t));

	ETC_FA_UNLOCK(fai);

	return ret;
}

int32
fa_napt_del(fa_t *fa, fa_napt_ioctl_t *par)
{
	uint32 hash;
	fa_napt_t *napt, *prev = NULL;
	uint32 *sip, *dip;
	uint16 sp, dp;
	uint8 proto;
	int32 ret = BCME_OK;
	fa_info_t *fai = (fa_info_t *)fa;

	/* Validate the input params */
	if (fai == NULL || par == NULL)
		return BCME_ERROR;

	/* If FA is in bypass mode, no use deleting entries */
	if (fai->acc_mode != CTF_FA_NORMAL)
		return BCME_OK;

	sip = par->sip;
	dip = par->dip;
	proto = par->proto;
	sp = par->sp;
	dp = par->dp;
	hash = FA_NAPT_HASH(par->v6, sip, dip, proto, sp, dp);

	ETC_FA_LOCK(fai);

	napt = FA_NAPT(fai)[hash];

	/* Keep track of prev pointer for deletion */
	while (napt != NULL) {
		if (!FA_NAPT_TPL_CMP(napt, par->v6, sip, dip, proto, sp, dp)) {
			prev = napt;
			napt = napt->next;
			continue;
		}

		/* Remove the entry from hash list */
		if (prev != NULL)
			prev->next = napt->next;
		else
			FA_NAPT(fai)[hash] = napt->next;

		ASSERT(napt->v6 == par->v6);

		ret = _fa_napt_del(fai, napt);

		fai->ftable[napt->nfi.napt_idx] = FALSE;
		fai->nflows--;

		break;
	}

	if (napt)
		MFREE(si_osh(fai->sih), napt, sizeof(fa_napt_t));

	ETC_FA_UNLOCK(fai);

	return ret;
}

void
fa_conntrack(fa_t *fa, fa_napt_ioctl_t *par)
{
	uint32 *sip, *dip;
	uint16 sp, dp;
	uint8 proto;
	fa_info_t *fai = (fa_info_t *)fa;

	/* Validate the input params */
	if (fai == NULL || par == NULL)
		return;

	/* If FA is in bypass mode, no use deleting entries */
	if (fai->acc_mode != CTF_FA_NORMAL)
		return;

	if (!PKTISFAAUX(par->pkt))
		return;

	sip = par->sip;
	dip = par->dip;
	proto = par->proto;
	sp = par->sp;
	dp = par->dp;

	ETC_FA_LOCK(fai);

	if (fa_napt_lkup_ll(fai, par->v6, sip, dip, proto, sp, dp))
		PKTSETFAFREED(par->pkt);

	ETC_FA_UNLOCK(fai);
}

void
fa_napt_live(fa_t *fa, fa_napt_ioctl_t *par)
{
	uint32 *sip, *dip;
	uint16 sp, dp;
	uint8 proto;

	uint32 hits;
	uint32 tbl[CTF_DATA_SIZE];
	fa_napt_t *napt;
	fa_info_t *fai = (fa_info_t *)fa;

	/* Validate the input params */
	if (fai == NULL || par == NULL)
		return;

	par->live = FALSE;

	/* If FA is in bypass mode, no use deleting entries */
	if (fai->acc_mode != CTF_FA_NORMAL)
		return;

	sip = par->sip;
	dip = par->dip;
	proto = par->proto;
	sp = par->sp;
	dp = par->dp;

	ETC_FA_LOCK(fai);

	if ((napt = fa_napt_lkup_ll(fai, par->v6, sip, dip, proto, sp, dp)) == NULL)
		goto err;

	if (fa_read_napt_entry(fai, tbl, napt->nfi.napt_idx) != BCME_OK) {
		ET_ERROR(("%s: FA HW IPC not found\n", __FUNCTION__));
		goto err;
	}

	/* update the hits counter */
	hits = ((LTOH16(tbl[1]) & 0x7) << 29 | LTOH16(tbl[0]) >> 3);
	if (napt->nfi.hits != hits) {
		napt->nfi.hits = hits;
		par->live = TRUE;
	}

err:

	ETC_FA_UNLOCK(fai);
}

void
fa_et_up(fa_t *fa)
{
	fa_info_t *fai = (fa_info_t *)fa;

	if (!FA_IS_FA_DEV(fa))
		return;

	/* Enable AUX and et_up */
	if (aux_dev) {
		robo_fa_aux_enable(fai->robo, TRUE);
		et_up(aux_dev->et);
	}
}

void
fa_et_down(fa_t *fa)
{
	fa_info_t *fai = (fa_info_t *)fa;

	if (!FA_IS_FA_DEV(fa))
		return;

	/* Just disable AUX, FA interface GMAC reset disable FA function */
	if (aux_dev)
		robo_fa_aux_enable(fai->robo, FALSE);
}

void
fa_set_name(fa_t *fa, char *name)
{
	/* The ethernet network interface uses "eth0". Use aux0 instead */
	if (FA_IS_AUX_DEV(fa))
		strncpy(name, "aux", 3);
}

int
fa_read_proc(char *buffer, char **start, off_t offset, int length, int *eof, void *data)
{
	int len;

	if (offset > 0) {
		*eof = 1;
		return 0;
	}

	/* Give the processed buffer back to userland */
	if (!length) {
		ET_ERROR(("%s: Not enough return buf space\n", __FUNCTION__));
		return 0;
	}

	len = sprintf(buffer, "%d\n", 1);
	return len;
}

void
fa_dump(fa_t *fa, struct bcmstrbuf *b, bool all)
{
	uint8 NF = all ? CTF_FA_DUMP_ALL_NF : CTF_FA_DUMP_VALID_NF;
	uint8 NH = all ? CTF_FA_DUMP_ALL_NH : CTF_FA_DUMP_VALID_NH;
	uint8 NP = all ? CTF_FA_DUMP_ALL_NP : CTF_FA_DUMP_VALID_NP;
	fa_info_t *fai = (fa_info_t *)fa;

	if (!fai || !b || FA_IS_AUX_DEV(fa))
		return;

	if (fai->acc_mode != CTF_FA_NORMAL) {
		bcm_bprintf(b, "Not in FA Normal mode!\n");
		return;
	}

	bcm_bprintf(b, "Number of flows: %d\n", fai->nflows);

	fa_dump_entries(fai, NF, (print_buf_t) bcm_bprintf, b);
	fa_dump_entries(fai, NH, (print_buf_t) bcm_bprintf, b);
	fa_dump_entries(fai, NP, (print_buf_t) bcm_bprintf, b);
}

void
fa_regs_show(fa_t *fa, struct bcmstrbuf *b)
{
#ifdef BCMDBG
	osl_t *osh;
	uint32 val, *reg;
	int i, nregs = sizeof(fareg)/sizeof(fareg[0]);
	fa_info_t *fai = (fa_info_t *)fa;

	if (!fai || !b || FA_IS_AUX_DEV(fa))
		return;

	osh = si_osh(fai->sih);

	bcm_bprintf(b, "\nFA regs dump\n");

	bcm_bprintf(b, "Chip rev %d\n", fai->chiprev);
	bcm_bprintf(b, "Accelerator mode: %s\n",
		(fai->acc_mode == CTF_FA_BYPASS) ? "BYPASS" :
		(fai->acc_mode == CTF_FA_NORMAL) ? "NORMAL" :
		(fai->acc_mode == CTF_FA_SW_ACC) ? "SW" : "UNKNOWN");

	/* reg dump */
	for (i = 0; i < (nregs -4); i++) {
		reg = (uint32 *)((unsigned long)(fai->regs) + fareg[i].reg_off);
		bcm_bprintf(b, "%s(0x%p):0x%08x\n", fareg[i].reg_name, reg, R_REG(osh, reg));
	}

	/* statistic dump */
	reg = (uint32 *)((unsigned long)(fai->regs) + OFFSETOF(faregs_t, stats));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "HITS", &reg[0], R_REG(osh, &reg[0]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "MISS", &reg[1], R_REG(osh, &reg[1]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "SNAP_FAIL", &reg[2], R_REG(osh, &reg[2]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "ETYPE_FAIL", &reg[3], R_REG(osh, &reg[3]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "VER_FAIL", &reg[4], R_REG(osh, &reg[4]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "FRAG_FAIL", &reg[5], R_REG(osh, &reg[5]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "PROTO_EXT_FAIL", &reg[6], R_REG(osh, &reg[6]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "V4_CSUM_FAIL", &reg[7], R_REG(osh, &reg[7]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "V4_OPTION_FAIL", &reg[8], R_REG(osh, &reg[8]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "V4_HDR_LEN_FAIL", &reg[9], R_REG(osh, &reg[9]));

	reg = (uint32 *)((unsigned long)(fai->regs) + OFFSETOF(faregs_t, eccst));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "ECC_NAPT_FLOW_STAT", &reg[0], R_REG(osh, &reg[0]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "ECC_NEXT_HOP_STAT", &reg[1], R_REG(osh, &reg[1]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "ECC_HWQ_STAT", &reg[2], R_REG(osh, &reg[2]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "ECC_LAB_STAT", &reg[3], R_REG(osh, &reg[3]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "ECC_HB_STAT", &reg[4], R_REG(osh, &reg[4]));

	reg = (uint32 *)((unsigned long)(fai->regs) + OFFSETOF(faregs_t, m_accdata));
	CTF_FA_WAR777_ON(fai);
	val = R_REG(osh, reg);
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "MEM_ACC_DATA7", &reg[7], R_REG(osh, &reg[7]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "MEM_ACC_DATA6", &reg[6], R_REG(osh, &reg[6]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "MEM_ACC_DATA5", &reg[5], R_REG(osh, &reg[5]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "MEM_ACC_DATA4", &reg[4], R_REG(osh, &reg[4]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "MEM_ACC_DATA3", &reg[3], R_REG(osh, &reg[3]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "MEM_ACC_DATA2", &reg[2], R_REG(osh, &reg[2]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "MEM_ACC_DATA1", &reg[1], R_REG(osh, &reg[1]));
	bcm_bprintf(b, "%s(0x%p):0x%08x\n", "MEM_ACC_DATA0", &reg[0], R_REG(osh, &reg[0]));
	CTF_FA_WAR777_OFF(fai);
#endif /* BCMDBG */
}
