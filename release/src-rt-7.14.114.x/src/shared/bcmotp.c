/*
 * OTP support.
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
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
 * $Id: bcmotp.c 483586 2014-06-10 11:15:53Z $
 */

#include <bcm_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmdevs.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <bcmotp.h>
#if (!defined(_CFE_) && !defined(_CFEZ_)) || defined(CFG_WL)
#include <wlioctl.h>
#endif /* (!_CFE_ && !_CFEZ_) || CFG_WL */

/*
 * There are two different OTP controllers so far:
 * 	1. new IPX OTP controller:	chipc 21, >=23
 * 	2. older HND OTP controller:	chipc 12, 17, 22
 *
 * Define BCMHNDOTP to include support for the HND OTP controller.
 * Define BCMIPXOTP to include support for the IPX OTP controller.
 *
 * NOTE 1: More than one may be defined
 * NOTE 2: If none are defined, the default is to include them all.
 */

#if !defined(BCMHNDOTP) && !defined(BCMIPXOTP)
#define BCMHNDOTP	1
#define BCMIPXOTP	1
#endif

#define OTPTYPE_HND(ccrev)	((ccrev) < 21 || (ccrev) == 22)
#define OTPTYPE_IPX(ccrev)	((ccrev) == 21 || (ccrev) >= 23)

#define OTP_ERR_VAL	0x0001
#define OTP_MSG_VAL	0x0002
#define OTP_DBG_VAL	0x0004
uint32 otp_msg_level = OTP_ERR_VAL;

#if defined(BCMDBG) || defined(BCMDBG_ERR)
#define OTP_ERR(args)	do {if (otp_msg_level & OTP_ERR_VAL) printf args;} while (0)
#else
#define OTP_ERR(args)
#endif

#ifdef BCMDBG
#define OTP_MSG(args)	do {if (otp_msg_level & OTP_MSG_VAL) printf args;} while (0)
#define OTP_DBG(args)	do {if (otp_msg_level & OTP_DBG_VAL) printf args;} while (0)
#else
#define OTP_MSG(args)
#define OTP_DBG(args)
#endif

#define OTPP_TRIES		10000000	/* # of tries for OTPP */
#define OTP_FUSES_PER_BIT	2
#define OTP_WRITE_RETRY		16

#ifdef BCMIPXOTP
#define MAXNUMRDES		9		/* Maximum OTP redundancy entries */
#endif

/* OTP common function type */
typedef int	(*otp_status_t)(void *oh);
typedef int	(*otp_size_t)(void *oh);
typedef void*	(*otp_init_t)(si_t *sih);
typedef uint16	(*otp_read_bit_t)(void *oh, chipcregs_t *cc, uint off);
typedef int	(*otp_read_region_t)(si_t *sih, int region, uint16 *data, uint *wlen);
typedef int	(*otp_nvread_t)(void *oh, char *data, uint *len);
typedef int	(*otp_write_region_t)(void *oh, int region, uint16 *data, uint wlen, uint flags);
typedef int	(*otp_cis_append_region_t)(si_t *sih, int region, char *vars, int count);
typedef int	(*otp_lock_t)(si_t *sih);
typedef int	(*otp_nvwrite_t)(void *oh, uint16 *data, uint wlen);
typedef int	(*otp_dump_t)(void *oh, int arg, char *buf, uint size);
typedef int	(*otp_write_word_t)(void *oh, uint wn, uint16 data);
typedef int	(*otp_read_word_t)(void *oh, uint wn, uint16 *data);
typedef int (*otp_write_bits_t)(void *oh, int bn, int bits, uint8* data);

/* OTP function struct */
typedef struct otp_fn_s {
	otp_size_t		size;
	otp_read_bit_t		read_bit;
	otp_dump_t		dump;
	otp_status_t		status;

#if !defined(BCMDONGLEHOST)
	otp_init_t		init;
	otp_read_region_t	read_region;
	otp_nvread_t		nvread;
	otp_write_region_t	write_region;
	otp_cis_append_region_t	cis_append_region;
	otp_lock_t		lock;
	otp_nvwrite_t		nvwrite;
	otp_write_word_t	write_word;
	otp_read_word_t		read_word;
	otp_write_bits_t	write_bits;
#endif /* !BCMDONGLEHOST */
} otp_fn_t;

typedef struct {
	uint		ccrev;		/* chipc revision */
	otp_fn_t	*fn;		/* OTP functions */
	si_t		*sih;		/* Saved sb handle */
	osl_t		*osh;

#ifdef BCMIPXOTP
	/* IPX OTP section */
	uint16		wsize;		/* Size of otp in words */
	uint16		rows;		/* Geometry */
	uint16		cols;		/* Geometry */
	uint32		status;		/* Flag bits (lock/prog/rv).
					 * (Reflected only when OTP is power cycled)
					 */
	uint16		hwbase;		/* hardware subregion offset */
	uint16		hwlim;		/* hardware subregion boundary */
	uint16		swbase;		/* software subregion offset */
	uint16		swlim;		/* software subregion boundary */
	uint16		fbase;		/* fuse subregion offset */
	uint16		flim;		/* fuse subregion boundary */
	int		otpgu_base;	/* offset to General Use Region */
	uint16		fusebits;	/* num of fusebits */
	bool 		buotp; 		/* Uinified OTP flag */
	uint 		usbmanfid_offset; /* Offset of the usb manfid inside the sdio CIS */
	struct {
		uint8 width;		/* entry width in bits */
		uint8 val_shift;	/* value bit offset in the entry */
		uint8 offsets;		/* # entries */
		uint8 stat_shift;	/* valid bit in otpstatus */
		uint16 offset[MAXNUMRDES];	/* entry offset in OTP */
	} rde_cb;			/* OTP redundancy control blocks */
	uint16		rde_idx;
#endif /* BCMIPXOTP */

#ifdef BCMHNDOTP
	/* HND OTP section */
	uint		size;		/* Size of otp in bytes */
	uint		hwprot;		/* Hardware protection bits */
	uint		signvalid;	/* Signature valid bits */
	int		boundary;	/* hw/sw boundary */
#endif /* BCMHNDOTP */
} otpinfo_t;

static otpinfo_t otpinfo;

/*
 * ROM accessor to avoid struct in shdat. Declare BCMRAMFN() to force the accessor to be excluded
 * from ROM.
 */
static otpinfo_t *
BCMRAMFN(get_otpinfo)(void)
{
	return (otpinfo_t *)&otpinfo;
}

/*
 * IPX OTP Code
 *
 *   Exported functions:
 *	ipxotp_status()
 *	ipxotp_size()
 *	ipxotp_init()
 *	ipxotp_read_bit()
 *	ipxotp_read_region()
 *	ipxotp_read_word()
 *	ipxotp_nvread()
 *	ipxotp_write_region()
 *	ipxotp_write_word()
 *	ipxotp_cis_append_region()
 *	ipxotp_lock()
 *	ipxotp_nvwrite()
 *	ipxotp_dump()
 *
 *   IPX internal functions:
 *	ipxotp_otpr()
 *	_ipxotp_init()
 *	ipxotp_write_bit()
 *	ipxotp_otpwb16()
 *	ipxotp_check_otp_pmu_res()
 *	ipxotp_write_rde()
 *	ipxotp_fix_word16()
 *	ipxotp_check_word16()
 *	ipxotp_max_rgnsz()
 *	ipxotp_otprb16()
 *	ipxotp_uotp_usbmanfid_offset()
 *
 */

#ifdef BCMIPXOTP

#define	OTPWSIZE		16	/* word size */
#define HWSW_RGN(rgn)		(((rgn) == OTP_HW_RGN) ? "h/w" : "s/w")

/* OTP layout */
/* CC revs 21, 24 and 27 OTP General Use Region word offset */
#define REVA4_OTPGU_BASE	12

/* CC revs 23, 25, 26, 28 and above OTP General Use Region word offset */
#define REVB8_OTPGU_BASE	20

/* CC rev 36 OTP General Use Region word offset */
#define REV36_OTPGU_BASE	12

/* Subregion word offsets in General Use region */
#define OTPGU_HSB_OFF		0
#define OTPGU_SFB_OFF		1
#define OTPGU_CI_OFF		2
#define OTPGU_P_OFF		3
#define OTPGU_SROM_OFF		4

/* Flag bit offsets in General Use region  */
#define OTPGU_NEWCISFORMAT_OFF	59
#define OTPGU_HWP_OFF		60
#define OTPGU_SWP_OFF		61
#define OTPGU_CIP_OFF		62
#define OTPGU_FUSEP_OFF		63
#define OTPGU_CIP_MSK		0x4000
#define OTPGU_P_MSK		0xf000
#define OTPGU_P_SHIFT		(OTPGU_HWP_OFF % 16)

/* LOCK but offset */
#define OTP_LOCK_ROW1_LOC_OFF	63	/* 1st ROW lock bit */
#define OTP_LOCK_ROW2_LOC_OFF	127	/* 2nd ROW lock bit */
#define OTP_LOCK_RD_LOC_OFF	128	/* Redundnancy Region lock bit */
#define OTP_LOCK_GU_LOC_OFF	129	/* General User Region lock bit */


/* OTP Size */
#define OTP_SZ_FU_972		((ROUNDUP(972, 16))/8)
#define OTP_SZ_FU_720		((ROUNDUP(720, 16))/8)
#define OTP_SZ_FU_608		((ROUNDUP(608, 16))/8)
#define OTP_SZ_FU_576		((ROUNDUP(576, 16))/8)
#define OTP_SZ_FU_324		((ROUNDUP(324,8))/8)	/* 324 bits */
#define OTP_SZ_FU_792		(792/8)		/* 792 bits */
#define OTP_SZ_FU_288		(288/8)		/* 288 bits */
#define OTP_SZ_FU_216		(216/8)		/* 216 bits */
#define OTP_SZ_FU_72		(72/8)		/* 72 bits */
#define OTP_SZ_CHECKSUM		(16/8)		/* 16 bits */
#define OTP4315_SWREG_SZ	178		/* 178 bytes */
#define OTP_SZ_FU_144		(144/8)		/* 144 bits */
#define OTP_SZ_FU_180		((ROUNDUP(180,8))/8)	/* 180 bits */

/* OTP BT shared region (pre-allocated) */
#define	OTP_BT_BASE_4330	(1760/OTPWSIZE)
#define OTP_BT_END_4330		(1888/OTPWSIZE)
#define OTP_BT_BASE_4324	(2384/OTPWSIZE)
#define	OTP_BT_END_4324		(2640/OTPWSIZE)
#define OTP_BT_BASE_4334	(2512/OTPWSIZE)
#define	OTP_BT_END_4334		(2768/OTPWSIZE)
#define OTP_BT_BASE_4314	(4192/OTPWSIZE)
#define	OTP_BT_END_4314		(4960/OTPWSIZE)
#define OTP_BT_BASE_4350	(4384/OTPWSIZE)
#define OTP_BT_END_4350		(5408/OTPWSIZE)
#define OTP_BT_BASE_4335	(4528/OTPWSIZE)
#define	OTP_BT_END_4335		(5552/OTPWSIZE)
#define OTP_BT_BASE_4345	(4496/OTPWSIZE)
#define OTP_BT_END_4345		(5520/OTPWSIZE)

/* OTP unification */
#if defined(USBSDIOUNIFIEDOTP)
/** Offset in OTP from upper GUR to HNBU_UMANFID tuple value in (16-bit) words */
#define USB_MANIFID_OFFSET_4319		42
#define USB_MANIFID_OFFSET_43143	45 /* See Confluence 43143 SW notebook #1 */
#endif /* USBSDIOUNIFIEDOTP */

#if !defined(BCMDONGLEHOST)
#if defined(BCMNVRAMW)
/* Local */
static int ipxotp_check_otp_pmu_res(chipcregs_t *cc);
static int ipxotp_write_bit(otpinfo_t *oi, chipcregs_t *cc, uint off);
static int ipxotp40n_read2x(void *oh, chipcregs_t *cc, uint off);
static int ipxotp_write_rde_nopc(void *oh, chipcregs_t *cc, int rde, uint bit, uint val);
#endif
#endif /* BCMDONGLEHOST */
static otp_fn_t* get_ipxotp_fn(void);

static int
BCMNMIATTACHFN(ipxotp_status)(void *oh)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	return (int)(oi->status);
}

/** Returns size in bytes */
static int
BCMNMIATTACHFN(ipxotp_size)(void *oh)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	return (int)oi->wsize * 2;
}

static uint16
BCMNMIATTACHFN(ipxotp_read_bit_common)(void *oh, chipcregs_t *cc, uint off)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint k, row, col;
	uint32 otpp, st;
	uint otpwt;

	otpwt = (R_REG(oi->osh, &cc->otplayout) & OTPL_WRAP_TYPE_MASK) >> OTPL_WRAP_TYPE_SHIFT;

	row = off / oi->cols;
	col = off % oi->cols;

	otpp = OTPP_START_BUSY |
		((((otpwt == OTPL_WRAP_TYPE_40NM)? OTPPOC_READ_40NM :
		OTPPOC_READ) << OTPP_OC_SHIFT) & OTPP_OC_MASK) |
	        ((row << OTPP_ROW_SHIFT) & OTPP_ROW_MASK) |
	        ((col << OTPP_COL_SHIFT) & OTPP_COL_MASK);
	OTP_DBG(("%s: off = %d, row = %d, col = %d, otpp = 0x%x",
	         __FUNCTION__, off, row, col, otpp));
	W_REG(oi->osh, &cc->otpprog, otpp);

	for (k = 0;
	     ((st = R_REG(oi->osh, &cc->otpprog)) & OTPP_START_BUSY) && (k < OTPP_TRIES);
	     k ++)
		;
	if (k >= OTPP_TRIES) {
		OTP_ERR(("\n%s: BUSY stuck: st=0x%x, count=%d\n", __FUNCTION__, st, k));
		return 0xffff;
	}
	if (st & OTPP_READERR) {
		OTP_ERR(("\n%s: Could not read OTP bit %d\n", __FUNCTION__, off));
		return 0xffff;
	}
	st = (st & OTPP_VALUE_MASK) >> OTPP_VALUE_SHIFT;

	OTP_DBG((" => %d\n", st));
	return (int)st;
}

static uint16
BCMNMIATTACHFN(ipxotp_read_bit)(void *oh, chipcregs_t *cc, uint off)
{
	otpinfo_t *oi;

	oi = (otpinfo_t *)oh;
	W_REG(oi->osh, &cc->otpcontrol, 0);
	W_REG(oi->osh, &cc->otpcontrol1, 0);

	return ipxotp_read_bit_common(oh, cc, off);
}

#if !defined(BCMDONGLEHOST) || defined(WLTEST)
#if !defined(BCMROMBUILD)
static uint16
BCMNMIATTACHFN(ipxotp_otprb16)(void *oh, chipcregs_t *cc, uint wn)
{
	uint base, i;
	uint16 val;
	uint16 bit;

	base = wn * 16;

	val = 0;
	for (i = 0; i < 16; i++) {
		if ((bit = ipxotp_read_bit(oh, cc, base + i)) == 0xffff)
			break;
		val = val | (bit << i);
	}
	if (i < 16)
		val = 0xffff;

	return val;
}
#endif /* !defined(BCMROMBUILD) */

static uint16
BCMNMIATTACHFN(ipxotp_otpr)(void *oh, chipcregs_t *cc, uint wn)
{
	otpinfo_t *oi;
#if !defined(BCMROMBUILD)
	si_t *sih;
#endif /* !defined(BCMROMBUILD) */
	uint16 val;


	oi = (otpinfo_t *)oh;

	ASSERT(wn < oi->wsize);
	ASSERT(cc != NULL);

#if !defined(BCMROMBUILD)
	sih = oi->sih;
	ASSERT(sih != NULL);
	/* If sprom is available use indirect access(as cc->sromotp maps to srom),
	 * else use random-access.
	 */
	if (si_is_sprom_available(sih))
		val = ipxotp_otprb16(oi, cc, wn);
	else
#endif /* !defined(BCMROMBUILD) */
		val = R_REG(oi->osh, &cc->sromotp[wn]);

	return val;
}
#endif 

#if !defined(BCMDONGLEHOST)
/** OTP BT region size */
static void
BCMNMIATTACHFN(ipxotp_bt_region_get)(otpinfo_t *oi, uint16 *start, uint16 *end)
{
	*start = *end = 0;
	switch (CHIPID(oi->sih->chip)) {
	case BCM4330_CHIP_ID:
		*start = OTP_BT_BASE_4330;
		*end = OTP_BT_END_4330;
		break;
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
		*start = OTP_BT_BASE_4324;
		*end = OTP_BT_END_4324;
		break;
	case BCM4334_CHIP_ID:
	case BCM43341_CHIP_ID:
		*start = OTP_BT_BASE_4334;
		*end = OTP_BT_END_4334;
		break;
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
		*start = OTP_BT_BASE_4314;
		*end = OTP_BT_END_4314;
		break;
	case BCM43602_CHIP_ID:	/* 43602 does not contain a BT core, only GCI/SECI interface. */
	case BCM43462_CHIP_ID:
		break;
	case BCM4345_CHIP_ID:
		*start = OTP_BT_BASE_4345;
		*end = OTP_BT_END_4345;
		break;
	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
		*start = OTP_BT_BASE_4350;
		*end = OTP_BT_END_4350;
		break;
	}
}

/**
 * Calculate max HW/SW region byte size by substracting fuse region and checksum size,
 * osizew is oi->wsize (OTP size - GU size) in words.
 */
static int
BCMNMIATTACHFN(ipxotp_max_rgnsz)(otpinfo_t *oi)
{
	int osizew = oi->wsize;
	int ret = 0;
	uint16 checksum;
	uint idx;
	chipcregs_t *cc;

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	checksum = OTP_SZ_CHECKSUM;

	/* for new chips, fusebit is available from cc register */
	if (oi->sih->ccrev >= 35) {
		oi->fusebits = R_REG(oi->osh, &cc->otplayoutextension) & OTPLAYOUTEXT_FUSE_MASK;
		oi->fusebits = ROUNDUP(oi->fusebits, 8);
		oi->fusebits >>= 3; /* bytes */
	}

	si_setcoreidx(oi->sih, idx);

	switch (CHIPID(oi->sih->chip)) {
	case BCM4322_CHIP_ID:	case BCM43221_CHIP_ID:	case BCM43231_CHIP_ID:
	case BCM43239_CHIP_ID:
		oi->fusebits = OTP_SZ_FU_288;
		break;
	case BCM43222_CHIP_ID:	case BCM43111_CHIP_ID:	case BCM43112_CHIP_ID:
	case BCM43224_CHIP_ID:	case BCM43225_CHIP_ID:	case BCM43421_CHIP_ID:
	case BCM43226_CHIP_ID:
		oi->fusebits = OTP_SZ_FU_72;
		break;
	case BCM43236_CHIP_ID:	case BCM43235_CHIP_ID:	case BCM43238_CHIP_ID:
	case BCM43237_CHIP_ID:
	case BCM43234_CHIP_ID:
		oi->fusebits = OTP_SZ_FU_324;
		break;
	case BCM4325_CHIP_ID:
	case BCM5356_CHIP_ID:
		oi->fusebits = OTP_SZ_FU_216;
		break;
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
		oi->fusebits = OTP_SZ_FU_144;
		break;
	case BCM4313_CHIP_ID:
		oi->fusebits = OTP_SZ_FU_72;
		break;
	case BCM4330_CHIP_ID:
	case BCM4334_CHIP_ID:
	case BCM43341_CHIP_ID:
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
		oi->fusebits = OTP_SZ_FU_144;
		break;
	case BCM4319_CHIP_ID:
		oi->fusebits = OTP_SZ_FU_180;
		break;
	case BCM4331_CHIP_ID:
	case BCM43431_CHIP_ID:
		oi->fusebits = OTP_SZ_FU_72;
		break;
	case BCM43131_CHIP_ID:
	case BCM43217_CHIP_ID:
	case BCM43227_CHIP_ID:
	case BCM43228_CHIP_ID:
	case BCM43428_CHIP_ID:
		oi->fusebits = OTP_SZ_FU_72;
		break;
	case BCM4335_CHIP_ID:
		oi->fusebits = OTP_SZ_FU_576;
		break;
	case BCM4345_CHIP_ID:
		oi->fusebits = OTP_SZ_FU_608;
		break;
	case BCM43602_CHIP_ID:
	case BCM43462_CHIP_ID:
		oi->fusebits = OTP_SZ_FU_972;
		break;
	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
		oi->fusebits = OTP_SZ_FU_720;
		break;
	case BCM4360_CHIP_ID:
		oi->fusebits = OTP_SZ_FU_792;
		break;
	default:
		if (oi->fusebits == 0)
			ASSERT(0);	/* Don't konw about this chip */
	}

	ret = osizew*2 - oi->fusebits - checksum;

	if (CHIPID(oi->sih->chip) == BCM4315_CHIP_ID) {
		ret = OTP4315_SWREG_SZ;
	}

	OTP_MSG(("max region size %d bytes\n", ret));
	return ret;
}

/** OTP sizes for 65nm and 130nm */
static int
BCMNMIATTACHFN(ipxotp_otpsize_set_65nm)(otpinfo_t *oi, uint otpsz)
{
	/* Check for otp size */
	switch (otpsz) {
	case 1:	/* 32x64 */
		oi->rows = 32;
		oi->cols = 64;
		oi->wsize = 128;
		break;
	case 2:	/* 64x64 */
		oi->rows = 64;
		oi->cols = 64;
		oi->wsize = 256;
		break;
	case 5:	/* 96x64 */
		oi->rows = 96;
		oi->cols = 64;
		oi->wsize = 384;
		break;
	case 7:	/* 16x64 */ /* 1024 bits */
		oi->rows = 16;
		oi->cols = 64;
		oi->wsize = 64;
		break;
	default:
		/* Don't know the geometry */
		OTP_ERR(("%s: unknown OTP geometry\n", __FUNCTION__));
	}

	return 0;
}

/**  OTP sizes for 40nm */
static int
BCMNMIATTACHFN(ipxotp_otpsize_set_40nm)(otpinfo_t *oi, uint otpsz)
{
	/* Check for otp size */
	switch (otpsz) {
	case 1:	/* 64x32: 2048 bits */
		oi->rows = 64;
		oi->cols = 32;
		break;
	case 2:	/* 96x32: 3072 bits */
		oi->rows = 96;
		oi->cols = 32;
		break;
	case 3:	/* 128x32: 4096 bits */
		oi->rows = 128;
		oi->cols = 32;
		break;
	case 4:	/* 160x32: 5120 bits */
		oi->rows = 160;
		oi->cols = 32;
		break;
	case 5:	/* 192x32: 6144 bits */
		oi->rows = 192;
		oi->cols = 32;
		break;
	case 7:	/* 256x32: 8192 bits */
		oi->rows = 256;
		oi->cols = 32;
		break;
	case 11: /* 384x32: 12288 bits */
		oi->rows = 384;
		oi->cols = 32;
		break;
	default:
		/* Don't know the geometry */
		OTP_ERR(("%s: unknown OTP geometry\n", __FUNCTION__));
	}

	oi->wsize = (oi->cols * oi->rows)/OTPWSIZE;
	return 0;
}

/** OTP unification */
#if defined(USBSDIOUNIFIEDOTP) && defined(BCMNVRAMW)
static void
BCMNMIATTACHFN(ipxotp_uotp_usbmanfid_offset)(otpinfo_t *oi)
{
	OTP_DBG(("%s: chip=0x%x\n", __FUNCTION__, CHIPID(oi->sih->chip)));
	switch (CHIPID(oi->sih->chip)) {
		/* Add cases for supporting chips */
		case BCM4319_CHIP_ID:
			oi->usbmanfid_offset = USB_MANIFID_OFFSET_4319;
			oi->buotp = TRUE;
			break;
		case BCM43143_CHIP_ID:
			oi->usbmanfid_offset = USB_MANIFID_OFFSET_43143;
			oi->buotp = TRUE;
			break;
		case BCM4335_CHIP_ID:
		case BCM4345_CHIP_ID:
		default:
			OTP_ERR(("chip=0x%x does not support Unified OTP.\n",
				CHIPID(oi->sih->chip)));
			break;
	}
}
#endif /* USBSDIOUNIFIEDOTP && BCMNVRAMW */

static void
BCMNMIATTACHFN(_ipxotp_init)(otpinfo_t *oi, chipcregs_t *cc)
{
	uint	k;
	uint32 otpp, st;
	uint16 btsz, btbase = 0, btend = 0;
	uint   otpwt;

	/* record word offset of General Use Region for various chipcommon revs */
	if (oi->sih->ccrev >= 40) {
		/* FIX: Available in rev >= 23; Verify before applying to others */
		oi->otpgu_base = (R_REG(oi->osh, &cc->otplayout) & OTPL_HWRGN_OFF_MASK)
			>> OTPL_HWRGN_OFF_SHIFT;
		ASSERT((oi->otpgu_base - (OTPGU_SROM_OFF * OTPWSIZE)) > 0);
		oi->otpgu_base >>= 4; /* words */
		oi->otpgu_base -= OTPGU_SROM_OFF;
	} else if (oi->sih->ccrev == 21 || oi->sih->ccrev == 24 || oi->sih->ccrev == 27) {
		oi->otpgu_base = REVA4_OTPGU_BASE;
	} else if ((oi->sih->ccrev == 36) || (oi->sih->ccrev == 39)) {
		/* OTP size greater than equal to 2KB (128 words), otpgu_base is similar to rev23 */
		if (oi->wsize >= 128)
			oi->otpgu_base = REVB8_OTPGU_BASE;
		else
			oi->otpgu_base = REV36_OTPGU_BASE;
	} else if (oi->sih->ccrev == 23 || oi->sih->ccrev >= 25) {
		oi->otpgu_base = REVB8_OTPGU_BASE;
	} else {
		OTP_ERR(("%s: chipc rev %d not supported\n", __FUNCTION__, oi->sih->ccrev));
	}

	otpwt = (R_REG(oi->osh, &cc->otplayout) & OTPL_WRAP_TYPE_MASK) >> OTPL_WRAP_TYPE_SHIFT;

	if (otpwt != OTPL_WRAP_TYPE_40NM) {
		/* First issue an init command so the status is up to date */
		otpp = OTPP_START_BUSY | ((OTPPOC_INIT << OTPP_OC_SHIFT) & OTPP_OC_MASK);

		OTP_DBG(("%s: otpp = 0x%x", __FUNCTION__, otpp));
		W_REG(oi->osh, &cc->otpprog, otpp);
		for (k = 0;
			((st = R_REG(oi->osh, &cc->otpprog)) & OTPP_START_BUSY) && (k < OTPP_TRIES);
			k ++)
			;
			if (k >= OTPP_TRIES) {
			OTP_ERR(("\n%s: BUSY stuck: st=0x%x, count=%d\n", __FUNCTION__, st, k));
			return;
		}
	}

	/* Read OTP lock bits and subregion programmed indication bits */
	oi->status = R_REG(oi->osh, &cc->otpstatus);

	if ((CHIPID(oi->sih->chip) == BCM43222_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43111_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43112_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43224_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43225_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43421_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43226_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43236_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43235_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43234_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43238_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43237_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43239_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM4324_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43242_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43243_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43143_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM4331_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43431_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM4360_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43460_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43526_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM4345_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43602_CHIP_ID) ||
		(CHIPID(oi->sih->chip) == BCM43462_CHIP_ID) ||
		BCM4350_CHIP(oi->sih->chip) ||
	0) {
		uint32 p_bits;
		p_bits = (ipxotp_otpr(oi, cc, oi->otpgu_base + OTPGU_P_OFF) & OTPGU_P_MSK)
			>> OTPGU_P_SHIFT;
		oi->status |= (p_bits << OTPS_GUP_SHIFT);
	}
	OTP_DBG(("%s: status 0x%x\n", __FUNCTION__, oi->status));

	/* OTP unification */
	oi->buotp = FALSE; /* Initialize it to false, until its explicitely set true. */
#if defined(USBSDIOUNIFIEDOTP) && defined(BCMNVRAMW)
	ipxotp_uotp_usbmanfid_offset(oi);
#endif /* USBSDIOUNIFIEDOTP && BCMNVRAMW */
	if ((oi->status & (OTPS_GUP_HW | OTPS_GUP_SW)) == (OTPS_GUP_HW | OTPS_GUP_SW)) {
		switch (CHIPID(oi->sih->chip)) {
			/* Add cases for supporting chips */
			case BCM4319_CHIP_ID:
				oi->buotp = TRUE;
				break;
			case BCM43143_CHIP_ID:
				oi->buotp = TRUE;
				break;
			case BCM4335_CHIP_ID:
			case BCM4345_CHIP_ID:
				oi->buotp = TRUE;
				break;
			default:
				OTP_ERR(("chip=0x%x does not support Unified OTP.\n",
					CHIPID(oi->sih->chip)));
				break;
		}
	}

	/*
	 * h/w region base and fuse region limit are fixed to the top and
	 * the bottom of the general use region. Everything else can be flexible.
	 */
	oi->hwbase = oi->otpgu_base + OTPGU_SROM_OFF;
	oi->hwlim = oi->wsize;
	oi->flim = oi->wsize;

	ipxotp_bt_region_get(oi, &btbase, &btend);
	btsz = btend - btbase;
	if (btsz > 0) {
		/* default to not exceed BT base */
		oi->hwlim = btbase;

		/* With BT shared region, swlim and fbase are fixed */
		oi->swlim = btbase;
		oi->fbase = btend;
	}

	/* Update hwlim and swbase */
	if (oi->status & OTPS_GUP_HW) {
		uint16 swbase;
		OTP_DBG(("%s: hw region programmed\n", __FUNCTION__));
		swbase = ipxotp_otpr(oi, cc, oi->otpgu_base + OTPGU_HSB_OFF) / 16;
		if (swbase) {
			oi->hwlim =  swbase;
		}
		oi->swbase = oi->hwlim;
	} else
		oi->swbase = oi->hwbase;

	/* Update swlim and fbase only if no BT region */
	if (btsz == 0) {
		/* subtract fuse and checksum from beginning */
		oi->swlim = ipxotp_max_rgnsz(oi) / 2;

		if (oi->status & OTPS_GUP_SW) {
			OTP_DBG(("%s: sw region programmed\n", __FUNCTION__));
			oi->swlim = ipxotp_otpr(oi, cc, oi->otpgu_base + OTPGU_SFB_OFF) / 16;
			oi->fbase = oi->swlim;
		}
		else
			oi->fbase = oi->swbase;
	}

	OTP_DBG(("%s: OTP limits---\n"
		"hwbase %d/%d hwlim %d/%d\n"
		"swbase %d/%d swlim %d/%d\n"
		"fbase %d/%d flim %d/%d\n", __FUNCTION__,
		oi->hwbase, oi->hwbase * 16, oi->hwlim, oi->hwlim * 16,
		oi->swbase, oi->swbase * 16, oi->swlim, oi->swlim * 16,
		oi->fbase, oi->fbase * 16, oi->flim, oi->flim * 16));
}

static void *
BCMNMIATTACHFN(ipxotp_init)(si_t *sih)
{
	uint idx, otpsz, otpwt;
	chipcregs_t *cc;
	otpinfo_t *oi = NULL;

	OTP_MSG(("%s: Use IPX OTP controller\n", __FUNCTION__));

	/* Make sure we're running IPX OTP */
	ASSERT(OTPTYPE_IPX(sih->ccrev));
	if (!OTPTYPE_IPX(sih->ccrev))
		return NULL;

	/* Make sure OTP is not disabled */
	if (si_is_otp_disabled(sih)) {
		OTP_MSG(("%s: OTP is disabled\n", __FUNCTION__));
#if !defined(WLTEST)
		return NULL;
#endif
	}

	/* Make sure OTP is powered up */
	if (!si_is_otp_powered(sih)) {
		OTP_ERR(("%s: OTP is powered down\n", __FUNCTION__));
		return NULL;
	}

	/* Retrieve OTP region info */
	idx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	oi = get_otpinfo();

	/* in Rev 49 otpsize moved to otplayout[15:12] */
	if (sih->ccrev >= 49) {
		otpsz = (R_REG(oi->osh, &cc->otplayout) & OTPL_ROW_SIZE_MASK)
			>> OTPL_ROW_SIZE_SHIFT;
	} else {
		otpsz = (sih->cccaps & CC_CAP_OTPSIZE) >> CC_CAP_OTPSIZE_SHIFT;
	}
	if (otpsz == 0) {
		OTP_ERR(("%s: No OTP\n", __FUNCTION__));
		oi = NULL;
		goto exit;
	}

	otpwt = (R_REG(oi->osh, &cc->otplayout) & OTPL_WRAP_TYPE_MASK) >> OTPL_WRAP_TYPE_SHIFT;

	if (otpwt == OTPL_WRAP_TYPE_40NM) {
		ipxotp_otpsize_set_40nm(oi, otpsz);
	} else if (otpwt == OTPL_WRAP_TYPE_65NM) {
		ipxotp_otpsize_set_65nm(oi, otpsz);
	} else {
		OTP_ERR(("%s: Unknown wrap type: %d\n", __FUNCTION__, otpwt));
		ASSERT(0);
	}

	OTP_MSG(("%s: rows %u cols %u wsize %u\n", __FUNCTION__, oi->rows, oi->cols, oi->wsize));

#ifdef BCMNVRAMW
	/* Initialize OTP redundancy control blocks */
	if (sih->ccrev >= 49) {
		uint16 offset[] = {266, 284, 302, 330, 348, 366, 394, 412, 430};
		bcopy(offset, &oi->rde_cb.offset, sizeof(offset));
		oi->rde_cb.offsets = ARRAYSIZE(offset);
		oi->rde_cb.width = 18;
		oi->rde_cb.val_shift = 14;
		oi->rde_cb.stat_shift = 16;
	} else if (sih->ccrev >= 40) {
		uint16 offset[] = {269, 286, 303, 333, 350, 367, 397, 414, 431};
		bcopy(offset, &oi->rde_cb.offset, sizeof(offset));
		oi->rde_cb.offsets = ARRAYSIZE(offset);
		oi->rde_cb.width = 17;
		oi->rde_cb.val_shift = 13;
		oi->rde_cb.stat_shift = 16;
	} else if (sih->ccrev == 36) {
		uint16 offset[] = {141, 158, 175};
		bcopy(offset, &oi->rde_cb.offset, sizeof(offset));
		oi->rde_cb.offsets = ARRAYSIZE(offset);
		oi->rde_cb.width = 15;
		oi->rde_cb.val_shift = 13;
		oi->rde_cb.stat_shift = 16;
	} else if (sih->ccrev == 21 || sih->ccrev == 24) {
		uint16 offset[] = {64, 79, 94, 109, 128, 143, 158, 173};
		bcopy(offset, &oi->rde_cb.offset, sizeof(offset));
		oi->rde_cb.offsets = ARRAYSIZE(offset);
		oi->rde_cb.width = 15;
		oi->rde_cb.val_shift = 11;
		oi->rde_cb.stat_shift = 16;
	}
	else if (sih->ccrev == 27) {
		uint16 offset[] = {128, 143, 158, 173};
		bcopy(offset, &oi->rde_cb.offset, sizeof(offset));
		oi->rde_cb.offsets = ARRAYSIZE(offset);
		oi->rde_cb.width = 15;
		oi->rde_cb.val_shift = 11;
		oi->rde_cb.stat_shift = 20;
	}
	else {
		uint16 offset[] = {141, 158, 175, 205, 222, 239, 269, 286, 303};
		bcopy(offset, &oi->rde_cb.offset, sizeof(offset));
		oi->rde_cb.offsets = ARRAYSIZE(offset);
		oi->rde_cb.width = 17;
		oi->rde_cb.val_shift = 13;
		oi->rde_cb.stat_shift = 16;
	}
	ASSERT(oi->rde_cb.offsets <= MAXNUMRDES);
	/* Initialize global rde index */
	oi->rde_idx = 0;
#endif	/* BCMNVRAMW */

	_ipxotp_init(oi, cc);

exit:
	si_setcoreidx(sih, idx);

	return (void *)oi;
}

static int
BCMNMIATTACHFN(ipxotp_read_region)(void *oh, int region, uint16 *data, uint *wlen)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint idx;
	chipcregs_t *cc;
	uint base, i, sz;

	/* Validate region selection */
	switch (region) {
	case OTP_HW_RGN:
		/* OTP unification: For unified OTP sz=flim-hwbase */
		if (oi->buotp)
			sz = (uint)oi->flim - oi->hwbase;
		else
			sz = (uint)oi->hwlim - oi->hwbase;
		if (!(oi->status & OTPS_GUP_HW)) {
			OTP_ERR(("%s: h/w region not programmed\n", __FUNCTION__));
			*wlen = sz;
			return BCME_NOTFOUND;
		}
		if (*wlen < sz) {
			OTP_ERR(("%s: buffer too small, should be at least %u\n",
			         __FUNCTION__, oi->hwlim - oi->hwbase));
			*wlen = sz;
			return BCME_BUFTOOSHORT;
		}
		base = oi->hwbase;
		break;
	case OTP_SW_RGN:
		/* OTP unification: For unified OTP sz=flim-swbase */
		if (oi->buotp)
			sz = ((uint)oi->flim - oi->swbase);
		else
			sz = ((uint)oi->swlim - oi->swbase);
		if (!(oi->status & OTPS_GUP_SW)) {
			OTP_ERR(("%s: s/w region not programmed\n", __FUNCTION__));
			*wlen = sz;
			return BCME_NOTFOUND;
		}
		if (*wlen < sz) {
			OTP_ERR(("%s: buffer too small should be at least %u\n",
			         __FUNCTION__, oi->swlim - oi->swbase));
			*wlen = sz;
			return BCME_BUFTOOSHORT;
		}
		base = oi->swbase;
		break;
	case OTP_CI_RGN:
		sz = OTPGU_CI_SZ;
		if (!(oi->status & OTPS_GUP_CI)) {
			OTP_ERR(("%s: chipid region not programmed\n", __FUNCTION__));
			*wlen = sz;
			return BCME_NOTFOUND;
		}
		if (*wlen < sz) {
			OTP_ERR(("%s: buffer too small, should be at least %u\n",
			         __FUNCTION__, OTPGU_CI_SZ));
			*wlen = sz;
			return BCME_BUFTOOSHORT;
		}
		base = oi->otpgu_base + OTPGU_CI_OFF;
		break;
	case OTP_FUSE_RGN:
		sz = (uint)oi->flim - oi->fbase;
		if (!(oi->status & OTPS_GUP_FUSE)) {
			OTP_ERR(("%s: fuse region not programmed\n", __FUNCTION__));
			*wlen = sz;
			return BCME_NOTFOUND;
		}
		if (*wlen < sz) {
			OTP_ERR(("%s: buffer too small, should be at least %u\n",
			         __FUNCTION__, oi->flim - oi->fbase));
			*wlen = sz;
			return BCME_BUFTOOSHORT;
		}
		base = oi->fbase;
		break;
	case OTP_ALL_RGN:
		sz = ((uint)oi->flim - oi->hwbase);
		if (!(oi->status & (OTPS_GUP_HW | OTPS_GUP_SW))) {
			OTP_ERR(("%s: h/w & s/w region not programmed\n", __FUNCTION__));
			*wlen = sz;
			return BCME_NOTFOUND;
		}
		if (*wlen < sz) {
			OTP_ERR(("%s: buffer too small, should be at least %u\n",
				__FUNCTION__, oi->hwlim - oi->hwbase));
			*wlen = sz;
			return BCME_BUFTOOSHORT;
		}
		base = oi->hwbase;
		break;
	default:
		OTP_ERR(("%s: reading region %d is not supported\n", __FUNCTION__, region));
		return BCME_BADARG;
	}

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	/* Read the data */
	for (i = 0; i < sz; i ++)
		data[i] = ipxotp_otpr(oh, cc, base + i);

	si_setcoreidx(oi->sih, idx);
	*wlen = sz;
	return 0;
}

static int
BCMNMIATTACHFN(ipxotp_read_word)(void *oh, uint wn, uint16 *data)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint idx;
	chipcregs_t *cc;

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	/* Read the data */
	*data = ipxotp_otpr(oh, cc, wn);

	si_setcoreidx(oi->sih, idx);
	return 0;
}

static int
BCMNMIATTACHFN(ipxotp_nvread)(void *oh, char *data, uint *len)
{
	return BCME_UNSUPPORTED;
}

#ifdef BCMNVRAMW
static int
BCMNMIATTACHFN(ipxotp_writable)(otpinfo_t *oi, chipcregs_t *cc)
{
	uint otpwt;

	otpwt = (R_REG(oi->osh, &cc->otplayout) & OTPL_WRAP_TYPE_MASK) >> OTPL_WRAP_TYPE_SHIFT;

	if (otpwt == OTPL_WRAP_TYPE_40NM) {
		uint i, k, row, col;
		uint32 otpp, st;
		uint cols[4] = {15, 4, 8, 13};

		row = 0;
		for (i = 0; i < 4; i++) {
			col = cols[i];

			otpp = OTPP_START_BUSY |
				((OTPPOC_PROG_ENABLE_40NM << OTPP_OC_SHIFT) & OTPP_OC_MASK) |
				((row << OTPP_ROW_SHIFT) & OTPP_ROW_MASK) |
				((col << OTPP_COL_SHIFT) & OTPP_COL_MASK);
			OTP_DBG(("%s: row = %d, col = %d, otpp = 0x%x\n",
				__FUNCTION__, row, col, otpp));

			W_REG(oi->osh, &cc->otpprog, otpp);

			for (k = 0;
				((st = R_REG(oi->osh, &cc->otpprog)) & OTPP_START_BUSY) &&
				(k < OTPP_TRIES); k ++)
				;
			if (k >= OTPP_TRIES) {
				OTP_ERR(("\n%s: BUSY stuck: st=0x%x, count=%d\n",
					__FUNCTION__, st, k));
				return -1;
			}
		}

		/* wait till OTP Program mode is unlocked */
		for (k = 0;
			(!((st = R_REG(oi->osh, &cc->otpstatus)) & OTPS_PROGOK)) &&
			(k < OTPP_TRIES); k ++)
			;
		OTP_MSG(("\n%s: OTP Program status: %x\n", __FUNCTION__, st));

		if (k >= OTPP_TRIES) {
			OTP_ERR(("\n%s: OTP Program mode is still locked, OTP is unwritable\n",
				__FUNCTION__));
			return -1;
		}
	}

	OR_REG(oi->osh, &cc->otpcontrol, OTPC_PROGEN);
	return 0;
}

static int
BCMNMIATTACHFN(ipxotp_unwritable)(otpinfo_t *oi, chipcregs_t *cc)
{
	uint otpwt;

	otpwt = (R_REG(oi->osh, &cc->otplayout) & OTPL_WRAP_TYPE_MASK) >> OTPL_WRAP_TYPE_SHIFT;

	if (otpwt == OTPL_WRAP_TYPE_40NM) {
		uint k, row, col;
		uint32 otpp, st;

		row = 0;
		col = 0;

		otpp = OTPP_START_BUSY |
			((OTPPOC_PROG_DISABLE_40NM << OTPP_OC_SHIFT) & OTPP_OC_MASK) |
			((row << OTPP_ROW_SHIFT) & OTPP_ROW_MASK) |
			((col << OTPP_COL_SHIFT) & OTPP_COL_MASK);
		OTP_DBG(("%s: row = %d, col = %d, otpp = 0x%x\n",
			__FUNCTION__, row, col, otpp));

		W_REG(oi->osh, &cc->otpprog, otpp);

		for (k = 0;
			((st = R_REG(oi->osh, &cc->otpprog)) & OTPP_START_BUSY) && (k < OTPP_TRIES);
			k ++)
			;
		if (k >= OTPP_TRIES) {
			OTP_ERR(("\n%s: BUSY stuck: st=0x%x, count=%d\n", __FUNCTION__, st, k));
			return -1;
		}

		/* wait till OTP Program mode is unlocked */
		for (k = 0;
			((st = R_REG(oi->osh, &cc->otpstatus)) & OTPS_PROGOK) && (k < OTPP_TRIES);
			k ++)
			;
		OTP_MSG(("\n%s: OTP Program status: %x\n", __FUNCTION__, st));

		if (k >= OTPP_TRIES) {
			OTP_ERR(("\n%s: OTP Program mode is still unlocked, OTP is writable\n",
				__FUNCTION__));
			return -1;
		}
	}

	AND_REG(oi->osh, &cc->otpcontrol, ~OTPC_PROGEN);
	return 0;
}

static int
BCMNMIATTACHFN(ipxotp_write_bit_common)(otpinfo_t *oi, chipcregs_t *cc, uint off)
{
	uint k, row, col;
	uint32 otpp, st;
	uint otpwt;

	otpwt = (R_REG(oi->osh, &cc->otplayout) & OTPL_WRAP_TYPE_MASK) >> OTPL_WRAP_TYPE_SHIFT;

	row = off / oi->cols;
	col = off % oi->cols;

	otpp = OTPP_START_BUSY |
	        ((1 << OTPP_VALUE_SHIFT) & OTPP_VALUE_MASK) |
		((((otpwt == OTPL_WRAP_TYPE_40NM)? OTPPOC_BIT_PROG_40NM :
		OTPPOC_BIT_PROG) << OTPP_OC_SHIFT) & OTPP_OC_MASK) |
	        ((row << OTPP_ROW_SHIFT) & OTPP_ROW_MASK) |
	        ((col << OTPP_COL_SHIFT) & OTPP_COL_MASK);
	OTP_DBG(("%s: off = %d, row = %d, col = %d, otpp = 0x%x\n",
	         __FUNCTION__, off, row, col, otpp));

	W_REG(oi->osh, &cc->otpprog, otpp);

	for (k = 0;
	     ((st = R_REG(oi->osh, &cc->otpprog)) & OTPP_START_BUSY) && (k < OTPP_TRIES);
	     k ++)
		;
	if (k >= OTPP_TRIES) {
		OTP_ERR(("\n%s: BUSY stuck: st=0x%x, count=%d\n", __FUNCTION__, st, k));
		return -1;
	}

	return 0;
}

#if !defined(BCMDONGLEHOST)
static int
BCMNMIATTACHFN(ipxotp40n_read2x)(void *oh, chipcregs_t *cc, uint off)
{
	otpinfo_t *oi;

	oi = (otpinfo_t *)oh;

	W_REG(oi->osh, &cc->otpcontrol,
		(OTPC_40NM_PCOUNT_V1X << OTPC_40NM_PCOUNT_SHIFT) |
		(OTPC_40NM_REGCSEL_DEF << OTPC_40NM_REGCSEL_SHIFT) |
		(1 << OTPC_40NM_PROGIN_SHIFT) |
		(1 << OTPC_40NM_R2X_SHIFT) |
		(1 << OTPC_40NM_ODM_SHIFT) |
		(1 << OTPC_40NM_DF_SHIFT) |
		(OTPC_40NM_VSEL_R1X << OTPC_40NM_VSEL_SHIFT) |
		(1 << OTPC_40NM_COFAIL_SHIFT));

	W_REG(oi->osh, &cc->otpcontrol1,
		(OTPC1_CPCSEL_DEF << OTPC1_CPCSEL_SHIFT) |
		(OTPC1_TM_R1X << OTPC1_TM_SHIFT));

	return ipxotp_read_bit_common(oh, cc, off);
}

static int
BCMNMIATTACHFN(ipxotp40n_read1x)(void *oh, chipcregs_t *cc, uint off, uint fuse)
{
	otpinfo_t *oi;

	oi = (otpinfo_t *)oh;

	W_REG(oi->osh, &cc->otpcontrol,
		(fuse << OTPC_40NM_PROGSEL_SHIFT) |
		(OTPC_40NM_PCOUNT_V1X << OTPC_40NM_PCOUNT_SHIFT) |
		(OTPC_40NM_REGCSEL_DEF << OTPC_40NM_REGCSEL_SHIFT) |
		(1 << OTPC_40NM_PROGIN_SHIFT) |
		(0 << OTPC_40NM_R2X_SHIFT) |
		(1 << OTPC_40NM_ODM_SHIFT) |
		(1 << OTPC_40NM_DF_SHIFT) |
		(OTPC_40NM_VSEL_R1X << OTPC_40NM_VSEL_SHIFT) |
		(1 << OTPC_40NM_COFAIL_SHIFT));
	W_REG(oi->osh, &cc->otpcontrol1,
		(OTPC1_CPCSEL_DEF << OTPC1_CPCSEL_SHIFT) |
		(OTPC1_TM_R1X << OTPC1_TM_SHIFT));

	return ipxotp_read_bit_common(oh, cc, off);
}

static int
BCMNMIATTACHFN(ipxotp40n_verify1x)(void *oh, chipcregs_t *cc, uint off, uint fuse)
{
	otpinfo_t *oi;

	oi = (otpinfo_t *)oh;

	W_REG(oi->osh, &cc->otpcontrol,
		(fuse << OTPC_40NM_PROGSEL_SHIFT) |
		(OTPC_40NM_PCOUNT_V1X << OTPC_40NM_PCOUNT_SHIFT) |
		(OTPC_40NM_REGCSEL_DEF << OTPC_40NM_REGCSEL_SHIFT) |
		(1 << OTPC_40NM_PROGIN_SHIFT) |
		(0 << OTPC_40NM_R2X_SHIFT) |
		(1 << OTPC_40NM_ODM_SHIFT) |
		(1 << OTPC_40NM_DF_SHIFT) |
		(OTPC_40NM_VSEL_V1X << OTPC_40NM_VSEL_SHIFT) |
		(1 << OTPC_40NM_COFAIL_SHIFT));
	W_REG(oi->osh, &cc->otpcontrol1,
		(OTPC1_CPCSEL_DEF << OTPC1_CPCSEL_SHIFT) |
		(OTPC1_TM_V1X << OTPC1_TM_SHIFT));

	return ipxotp_read_bit_common(oh, cc, off);
}

static int
BCMNMIATTACHFN(ipxotp40n_write_fuse)(otpinfo_t *oi, chipcregs_t *cc, uint off, uint fuse)
{
	W_REG(oi->osh, &cc->otpcontrol,
		(fuse << OTPC_40NM_PROGSEL_SHIFT) |
		(OTPC_40NM_PCOUNT_WR << OTPC_40NM_PCOUNT_SHIFT) |
		(OTPC_40NM_REGCSEL_DEF << OTPC_40NM_REGCSEL_SHIFT) |
		(1 << OTPC_40NM_PROGIN_SHIFT) |
		(0 << OTPC_40NM_R2X_SHIFT) |
		(1 << OTPC_40NM_ODM_SHIFT) |
		(0 << OTPC_40NM_DF_SHIFT) |
		(OTPC_40NM_VSEL_WR << OTPC_40NM_VSEL_SHIFT) |
		(1 << OTPC_40NM_COFAIL_SHIFT) |
		OTPC_PROGEN);

	W_REG(oi->osh, &cc->otpcontrol1,
		(OTPC1_CPCSEL_DEF << OTPC1_CPCSEL_SHIFT) |
		(OTPC1_TM_WR << OTPC1_TM_SHIFT));

	return ipxotp_write_bit_common(oi, cc, off);
}

static int
BCMNMIATTACHFN(ipxotp40n_write_bit)(otpinfo_t *oi, chipcregs_t *cc, uint off)
{
	uint32 oc_orig, oc1_orig;
	uint8 i, j, err = 0;
	int verr0, verr1, rerr0, rerr1, retry, val;

	oc_orig = R_REG(oi->osh, &cc->otpcontrol);
	oc1_orig = R_REG(oi->osh, &cc->otpcontrol1);

	for (i = 0; i < OTP_FUSES_PER_BIT; i++) {
		retry = 0;
		for (j = 0; j < OTP_WRITE_RETRY; j++) {
			/* program fuse */
			ipxotp40n_write_fuse(oi, cc, off, i);

			/* verify fuse */
			val = ipxotp40n_verify1x(oi, cc, off, i);
			if (val == 1)
				break;

			retry++;
		}

		if ((val != 1) && (j == OTP_WRITE_RETRY)) {
			OTP_ERR(("ERROR: New write failed max attempts fuse:%d @ off:%d\n",
				i, off));
		} else if (retry > 0)
			OTP_MSG(("Verify1x multi retries:%d fuse:%d @ off:%d\n",
				retry, i, off));
	}

	/* Post screen */
	verr0 = (ipxotp40n_verify1x(oi, cc, off, 0) == 1) ? TRUE : FALSE;
	verr1 = (ipxotp40n_verify1x(oi, cc, off, 1) == 1) ? TRUE : FALSE;
	rerr0 = (ipxotp40n_read1x(oi, cc, off, 0) == 1) ? TRUE : FALSE;
	rerr1 = (ipxotp40n_read1x(oi, cc, off, 1) == 1) ? TRUE : FALSE;

	if (verr0 && verr1) {
		OTP_MSG(("V0:%d and V1:%d ok off:%d\n", verr0, verr1, off));
	} else if (verr0 && rerr1) {
		OTP_MSG(("V0:%d and R1:%d ok off:%d\n", verr0, rerr1, off));
	} else if (rerr0 && verr1) {
		OTP_MSG(("R0:%d and V1:%d ok off:%d\n", rerr0, verr1, off));
	} else {
		OTP_ERR(("Bit failed post screen v0:%d v1:%d r0:%d r1:%d off:%d\n",
			verr0, verr1, rerr0, rerr1, off));
		err = -1;
	}

	W_REG(oi->osh, &cc->otpcontrol, oc_orig);
	W_REG(oi->osh, &cc->otpcontrol1, oc1_orig);

	return err;
}

#ifdef OTP_DEBUG
int
BCMNMIATTACHFN(otp_read1x)(void *oh, uint off, uint fuse)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	chipcregs_t *cc;
	uint idx, otpwt;
	int val = 0;

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	otpwt = (R_REG(oi->osh, &cc->otplayout) & OTPL_WRAP_TYPE_MASK) >> OTPL_WRAP_TYPE_SHIFT;
	if ((otpwt != OTPL_WRAP_TYPE_40NM) || (oi->sih->ccrev < 40))
		goto exit;

	val = ipxotp40n_read1x(oi, cc, off, fuse);

exit:
	si_setcoreidx(oi->sih, idx);
	return val;
}

int
BCMNMIATTACHFN(otp_verify1x)(void *oh, uint off, uint fuse)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	int err = 0;
	chipcregs_t *cc;
	uint idx, otpwt;

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	otpwt = (R_REG(oi->osh, &cc->otplayout) & OTPL_WRAP_TYPE_MASK) >> OTPL_WRAP_TYPE_SHIFT;
	if ((otpwt != OTPL_WRAP_TYPE_40NM) || (oi->sih->ccrev < 40))
		goto exit;

	err = ipxotp40n_verify1x(oi, cc, off, fuse);
	if (err != 1)
		OTP_ERR(("v1x failed fuse:%d @ off:%d\n", fuse, off));
exit:
	si_setcoreidx(oi->sih, idx);
	return err;
}

/**
 * Repair is to fix damaged bits; not intended to fix programming errors
 * This is limited and for 4334 only nine repair entries available
 */
int
BCMNMIATTACHFN(otp_repair_bit)(void *oh, uint off, uint val)
{
	otpinfo_t *oi = (otpinfo_t *)oh;

	return ipxotp_write_rde(oi, -1, off, val);
}

int
BCMNMIATTACHFN(otp_write_ones_old)(void *oh, uint off, uint bits)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint idx;
	chipcregs_t *cc;
	uint32 i;
	uint32 min_res_mask = 0;

	if (off < 0 || off + bits > oi->rows * oi->cols)
		return BCME_RANGE;

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	W_REG(oi->osh, &cc->otpcontrol, 0);
	W_REG(oi->osh, &cc->otpcontrol1, 0);

	ipxotp_writable(oi, cc);
	for (i = 0; i < bits; i++) {
		ipxotp_write_bit_common(oi, cc, off++);
	}
	ipxotp_unwritable(oi, cc);

	si_otp_power(oi->sih, FALSE, &min_res_mask);
	si_otp_power(oi->sih, TRUE, &min_res_mask);
	_ipxotp_init(oi, cc);

	si_setcoreidx(oi->sih, idx);
	return BCME_OK;
}

int
BCMNMIATTACHFN(otp_write_ones)(void *oh, uint off, uint bits)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint idx;
	chipcregs_t *cc;
	uint32 i;
	int err;
	uint32 min_res_mask = 0;

	if (off < 0 || off + bits > oi->rows * oi->cols)
		return BCME_RANGE;

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	ipxotp_writable(oi, cc);
	for (i = 0; i < bits; i++) {
		err = ipxotp_write_bit(oi, cc, off);
		if (err != 0) {
			OTP_ERR(("%s: write bit failed: %d\n", __FUNCTION__, off));

			err = ipxotp_write_rde_nopc(oi, cc,
				ipxotp_check_otp_pmu_res(cc), off, 1);
			if (err != 0)
				OTP_ERR(("%s: repair bit failed: %d\n", __FUNCTION__, off));
			else
				OTP_ERR(("%s: repair bit ok: %d\n", __FUNCTION__, off));
		}

		off++;
	}
	ipxotp_unwritable(oi, cc);

	si_otp_power(oi->sih, FALSE, &min_res_mask);
	si_otp_power(oi->sih, TRUE, &min_res_mask);
	_ipxotp_init(oi, cc);

	si_setcoreidx(oi->sih, idx);
	return BCME_OK;
}

#endif /* OTP_DEBUG */

static int
BCMNMIATTACHFN(ipxotp_write_bit)(otpinfo_t *oi, chipcregs_t *cc, uint off)
{
	uint otpwt;
	int status = 0;

	otpwt = (R_REG(oi->osh, &cc->otplayout) & OTPL_WRAP_TYPE_MASK) >> OTPL_WRAP_TYPE_SHIFT;

	if (otpwt == OTPL_WRAP_TYPE_40NM) {
		/* Can damage fuse in 40nm so safeguard against reprogramming */
		if (ipxotp40n_read2x(oi, cc, off) != 1) {
			status = ipxotp40n_write_bit(oi, cc, off);
		} else {
			OTP_MSG(("Bit already programmed: %d\n", off));
		}
	} else {
		AND_REG(oi->osh, &cc->otpcontrol, OTPC_PROGEN);
		W_REG(oi->osh, &cc->otpcontrol1, 0);

		status = ipxotp_write_bit_common(oi, cc, off);
	}

	return status;
}

static int
BCMNMIATTACHFN(ipxotp_write_bits)(void *oh, int bn, int bits, uint8* data)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint idx;
	chipcregs_t *cc;
	int i, j;
	uint8 temp;
	int err;
	uint32 min_res_mask = 0;

	if (bn < 0 || bn + bits > oi->rows * oi->cols)
		return BCME_RANGE;

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	ipxotp_writable(oi, cc);
	for (i = 0; i < bits; ) {
		temp = *data++;
		for (j = 0; j < 8; j++, i++) {
			if (i >= bits)
				break;
			if (temp & 0x01)
			{
				if (ipxotp_write_bit(oi, cc, (uint)(i + bn)) != 0) {
					OTP_ERR(("%s: write bit failed: %d\n",
						__FUNCTION__, i + bn));

					err = ipxotp_write_rde_nopc(oi, cc,
						ipxotp_check_otp_pmu_res(cc), i + bn, 1);
					if (err != 0) {
						OTP_ERR(("%s: repair bit failed: %d\n",
							__FUNCTION__, i + bn));
						AND_REG(oi->osh, &cc->otpcontrol, ~OTPC_PROGEN);
						return -1;
					} else
						OTP_ERR(("%s: repair bit ok: %d\n",
							__FUNCTION__, i + bn));
				}
			}
			temp >>= 1;
		}
	}
	ipxotp_unwritable(oi, cc);

	si_otp_power(oi->sih, FALSE, &min_res_mask);
	si_otp_power(oi->sih, TRUE, &min_res_mask);
	_ipxotp_init(oi, cc);

	si_setcoreidx(oi->sih, idx);
	return BCME_OK;
}
#endif /* !BCMDONGLEHOST */


static int
BCMNMIATTACHFN(ipxotp_write_lock_bit)(otpinfo_t *oi, chipcregs_t *cc, uint off)
{
	uint k, row, col;
	uint32 otpp, st;
	uint otpwt;

	otpwt = (R_REG(oi->osh, &cc->otplayout) & OTPL_WRAP_TYPE_MASK) >> OTPL_WRAP_TYPE_SHIFT;

	row = off / oi->cols;
	col = off % oi->cols;

	otpp = OTPP_START_BUSY |
		((((otpwt == OTPL_WRAP_TYPE_40NM)? OTPPOC_ROW_LOCK_40NM :
		OTPPOC_ROW_LOCK) << OTPP_OC_SHIFT) & OTPP_OC_MASK) |
	    ((row << OTPP_ROW_SHIFT) & OTPP_ROW_MASK) |
	    ((col << OTPP_COL_SHIFT) & OTPP_COL_MASK);
	OTP_DBG(("%s: off = %d, row = %d, col = %d, otpp = 0x%x\n",
	         __FUNCTION__, off, row, col, otpp));

	W_REG(oi->osh, &cc->otpprog, otpp);

	for (k = 0;
	     ((st = R_REG(oi->osh, &cc->otpprog)) & OTPP_START_BUSY) && (k < OTPP_TRIES);
	     k ++)
		;
	if (k >= OTPP_TRIES) {
		OTP_ERR(("\n%s: BUSY stuck: st=0x%x, count=%d\n", __FUNCTION__, st, k));
		return -1;
	}

	return 0;
}

static int
BCMNMIATTACHFN(ipxotp_otpwb16)(otpinfo_t *oi, chipcregs_t *cc, int wn, uint16 data)
{
	uint base, i;
	int rc = 0;

	base = wn * 16;
	for (i = 0; i < 16; i++) {
		if (data & (1 << i)) {
			rc = ipxotp_write_bit(oi, cc, base + i);
			if (rc != 0) {
				OTP_ERR(("%s: write bit failed:%d\n", __FUNCTION__, base + i));

				rc = ipxotp_write_rde_nopc(oi, cc,
					ipxotp_check_otp_pmu_res(cc), base + i, 1);
				if (rc != 0) {
					OTP_ERR(("%s: repair bit failed:%d\n",
						__FUNCTION__, base + i));
					break;
				} else
					OTP_ERR(("%s: repair bit ok:%d\n", __FUNCTION__, base + i));
			}
		}
	}

	return rc;
}

/* Write OTP redundancy entry:
 *  rde - redundancy entry index (-ve for "next")
 *  bit - bit offset
 *  val - bit value
 */

/** Check if for a particular chip OTP PMU resource is available */
static int
BCMNMIATTACHFN(ipxotp_check_otp_pmu_res)(chipcregs_t *cc)
{
	switch (cc->chipid & 0x0000ffff) {
		case BCM43131_CHIP_ID:
		case BCM43217_CHIP_ID:
		case BCM43227_CHIP_ID:
		case BCM43228_CHIP_ID:
			/* OTP PMU resource not available, hence use global rde index */
			return OTP_GLOBAL_RDE_IDX;
		default:
			/* OTP PMU resource available, hence calculate rde index */
			return -1;
	}
	return -1;
}

/** Assumes already writable and bypasses power-cycling */
static int
BCMNMIATTACHFN(ipxotp_write_rde_nopc)(void *oh, chipcregs_t *cc, int rde, uint bit, uint val)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint i, temp;
	int err = BCME_OK;

#ifdef BCMDBG
	if ((rde >= (int)oi->rde_cb.offsets) || (bit >= (uint)(oi->rows * oi->cols)) || (val > 1))
		return BCME_RANGE;
#endif

	if (rde < 0) {
		for (rde = 0; rde < oi->rde_cb.offsets - 1; rde++) {
			if ((oi->status & (1 << (oi->rde_cb.stat_shift + rde))) == 0)
				break;
		}
		OTP_ERR(("%s: Auto rde index %d\n", __FUNCTION__, rde));
	}
	else if (rde == OTP_GLOBAL_RDE_IDX) {
		/* Chips which do not have a OTP PMU res, OTP can't be pwr cycled from the drv. */
		/* Hence we need to have a count of the global rde, and populate accordingly. */

		/* Find the next available rde location */
		while (oi->status & (1 << (oi->rde_cb.stat_shift + oi->rde_idx))) {
			OTP_MSG(("%s: rde %d already in use, status 0x%08x\n", __FUNCTION__,
				rde, oi->status));
			oi->rde_idx++;
		}
		rde = oi->rde_idx++;

		if (rde >= MAXNUMRDES) {
			OTP_MSG(("%s: No rde location available to fix.\n", __FUNCTION__));
			return BCME_ERROR;
		}
	}

	if (oi->status & (1 << (oi->rde_cb.stat_shift + rde))) {
		OTP_ERR(("%s: rde %d already in use, status 0x%08x\n", __FUNCTION__,
		         rde, oi->status));
		return BCME_ERROR;
	}

	temp = ~(~0 << oi->rde_cb.width) &
	        ((~0 << (oi->rde_cb.val_shift + 1)) | (val << oi->rde_cb.val_shift) | bit);

	OTP_MSG(("%s: rde %d bit %d val %d bmp 0x%08x\n", __FUNCTION__, rde, bit, val, temp));

	for (i = 0; i < oi->rde_cb.width; i ++) {
		if (!(temp & (1 << i)))
			continue;
		if (ipxotp_write_bit(oi, cc, oi->rde_cb.offset[rde] + i) != 0)
			err = BCME_ERROR;
	}

	/* no power-cyclying to just set status */
	oi->status |= (1 << (oi->rde_cb.stat_shift + rde));

	return err;
}

int
BCMNMIATTACHFN(ipxotp_write_rde)(void *oh, int rde, uint bit, uint val)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint idx;
	chipcregs_t *cc;
	int err;
	uint32 min_res_mask = 0;

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	/* Enable Write */
	ipxotp_writable(oi, cc);

	err = ipxotp_write_rde_nopc(oh, cc, rde, bit, val);

	/* Disable Write */
	ipxotp_unwritable(oi, cc);

	si_otp_power(oi->sih, FALSE, &min_res_mask);
	si_otp_power(oi->sih, TRUE, &min_res_mask);
	_ipxotp_init(oi, cc);

	si_setcoreidx(oi->sih, idx);
	return err;
}

/** Set up redundancy entries for the specified bits */
static int
BCMNMIATTACHFN(ipxotp_fix_word16)(void *oh, uint wn, uint16 mask, uint16 val, chipcregs_t *cc)
{
	otpinfo_t *oi;
	uint bit;
	int rc = 0;

	oi = (otpinfo_t *)oh;

	ASSERT(oi != NULL);
	ASSERT(wn < oi->wsize);

	for (bit = wn * 16; mask; bit++, mask >>= 1, val >>= 1) {
		if (mask & 1) {
			if ((rc = ipxotp_write_rde(oi, ipxotp_check_otp_pmu_res(cc), bit, val & 1)))
				break;
		}
	}

	return rc;
}

static int
BCMNMIATTACHFN(ipxotp_check_word16)(void *oh, chipcregs_t *cc, uint wn, uint16 val)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint16 word = ipxotp_otpr(oi, cc, wn);
	int rc = 0;

	if ((word ^= val)) {
		OTP_MSG(("%s: word %d is 0x%04x, wanted 0x%04x, fixing...\n",
			__FUNCTION__, wn, (word ^ val), val));

		if ((rc = ipxotp_fix_word16(oi, wn, word, val, cc))) {
			OTP_ERR(("FAILED, ipxotp_fix_word16 returns %d\n", rc));
			/* Fatal error, unfixable. MFGC will have to fail. Board
			 * needs to be discarded!!
			 */
			return BCME_NORESOURCE;
		}
	}

	return BCME_OK;
}

/** expects the caller to disable interrupts before calling this routine */
static int
BCMNMIATTACHFN(ipxotp_write_region)(void *oh, int region, uint16 *data, uint wlen, uint flags)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint idx;
	chipcregs_t *cc;
	uint base, i;
	int otpgu_bit_base;
	bool rewrite = FALSE;
	int rc = 0;
	uint32 min_res_mask = 0;
#if defined(DONGLEBUILD)
	uint16 *origdata = NULL;
#endif /* DONGLEBUILD */

	otpgu_bit_base = oi->otpgu_base * 16;

	/* Validate region selection */
	switch (region) {
	case OTP_HW_RGN:
		if (wlen > (uint)(oi->hwlim - oi->hwbase)) {
			OTP_ERR(("%s: wlen %u exceeds OTP h/w region limit %u\n",
			         __FUNCTION__, wlen, oi->hwlim - oi->hwbase));
			return BCME_BUFTOOLONG;
		}
		rewrite = !!(oi->status & OTPS_GUP_HW);
		base = oi->hwbase;
		break;
	case OTP_SW_RGN:
		if (wlen > (uint)(oi->swlim - oi->swbase)) {
			OTP_ERR(("%s: wlen %u exceeds OTP s/w region limit %u\n",
			         __FUNCTION__, wlen, oi->swlim - oi->swbase));
			return BCME_BUFTOOLONG;
		}
		rewrite = !!(oi->status & OTPS_GUP_SW);
		base = oi->swbase;
		break;
	case OTP_CI_RGN:
		if (oi->status & OTPS_GUP_CI) {
			OTP_ERR(("%s: chipid region has been programmed\n", __FUNCTION__));
			return BCME_ERROR;
		}
		if (wlen > OTPGU_CI_SZ) {
			OTP_ERR(("%s: wlen %u exceeds OTP ci region limit %u\n",
			         __FUNCTION__, wlen, OTPGU_CI_SZ));
			return BCME_BUFTOOLONG;
		}
		if ((wlen == OTPGU_CI_SZ) && (data[OTPGU_CI_SZ - 1] & OTPGU_P_MSK) != 0) {
			OTP_ERR(("%s: subregion programmed bits not zero\n", __FUNCTION__));
			return BCME_BADARG;
		}
		base = oi->otpgu_base + OTPGU_CI_OFF;
		break;
	case OTP_FUSE_RGN:
		if (oi->status & OTPS_GUP_FUSE) {
			OTP_ERR(("%s: fuse region has been programmed\n", __FUNCTION__));
			return BCME_ERROR;
		}
		if (wlen > (uint)(oi->flim - oi->fbase)) {
			OTP_ERR(("%s: wlen %u exceeds OTP ci region limit %u\n",
			         __FUNCTION__, wlen, oi->flim - oi->fbase));
			return BCME_BUFTOOLONG;
		}
		base = oi->flim - wlen;
		break;
	default:
		OTP_ERR(("%s: writing region %d is not supported\n", __FUNCTION__, region));
		return BCME_ERROR;
	}

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

#if defined(DONGLEBUILD)
	/* Check for conflict; Since some bits might be programmed at ATE time, we need to
	 * avoid redundancy by clearing already written bits, but copy original for verification.
	 */
	if ((origdata = (uint16*)MALLOC(oi->osh, wlen * 2)) == NULL) {
		rc = BCME_NOMEM;
		goto exit;
	}
	for (i = 0; i < wlen; i++) {
		origdata[i] = data[i];
		data[i] = ipxotp_otpr(oi, cc, base + i);
		if (data[i] & ~origdata[i]) {
			OTP_ERR(("%s: %s region: word %d incompatible (0x%04x->0x%04x)\n",
				__FUNCTION__, HWSW_RGN(region), i, data[i], origdata[i]));
			rc = BCME_BADARG;
			goto exit;
		}
		data[i] ^= origdata[i];
	}
#endif /* DONGLEBUILD */

	OTP_MSG(("%s: writing new bits in %s region\n", __FUNCTION__, HWSW_RGN(region)));

	/* Enable Write */
	ipxotp_writable(oi, cc);

	/* Write the data */
	for (i = 0; i < wlen; i++) {
		rc = ipxotp_otpwb16(oi, cc, base + i, data[i]);
		if (rc != 0) {
			OTP_ERR(("%s: otpwb16 failed: %d 0x%x\n", __FUNCTION__, base + i, data[i]));
			ipxotp_unwritable(oi, cc);
			goto exit;
		}
	}

	/* One time set region flag: Update boundary/flag in memory and in OTP */
	if (!rewrite) {
		switch (region) {
		case OTP_HW_RGN:
			/* OTP unification */
			if (oi->buotp) {
				ipxotp_otpwb16(oi, cc, oi->otpgu_base + OTPGU_HSB_OFF,
					((base + oi->usbmanfid_offset) * 16));
				ipxotp_write_bit(oi, cc, otpgu_bit_base + OTPGU_SWP_OFF);
			} else
				ipxotp_otpwb16(oi, cc, oi->otpgu_base + OTPGU_HSB_OFF,
				(base + i) * 16);
			ipxotp_write_bit(oi, cc, otpgu_bit_base + OTPGU_HWP_OFF);
			/* Set new CIS format bit when the ciswrite command ask us
			 * to do so, or for listed chips
			 */
			if (0 ||
#if (!defined(_CFE_) && !defined(_CFEZ_)) || defined(CFG_WL)
				(flags & CISH_FLAG_PCIECIS) ||
#endif /* (!_CFE_ && !_CFEZ_) || CFG_WL */
				CHIPID(oi->sih->chip) == BCM4336_CHIP_ID ||
				CHIPID(oi->sih->chip) == BCM43362_CHIP_ID ||
				CHIPID(oi->sih->chip) == BCM43341_CHIP_ID ||
				CHIPID(oi->sih->chip) == BCM43242_CHIP_ID ||
				CHIPID(oi->sih->chip) == BCM43243_CHIP_ID ||
				CHIPID(oi->sih->chip) == BCM43143_CHIP_ID ||
				CHIPID(oi->sih->chip) == BCM4324_CHIP_ID ||
				CHIPID(oi->sih->chip) == BCM4335_CHIP_ID ||
				((CHIPID(oi->sih->chip) == BCM4345_CHIP_ID) &&
				CST4345_CHIPMODE_SDIOD(oi->sih->chipst)) ||
				((BCM4350_CHIP(oi->sih->chip)) &&
				CST4350_CHIPMODE_SDIOD(oi->sih->chipst)))
				ipxotp_write_bit(oi, cc, otpgu_bit_base + OTPGU_NEWCISFORMAT_OFF);
			break;
		case OTP_SW_RGN:
			/* Write HW region limit as well */
			ipxotp_otpwb16(oi, cc, oi->otpgu_base + OTPGU_HSB_OFF, base * 16);
			/* write max swlim(covert to bits) to the sw/fuse boundary */
			ipxotp_otpwb16(oi, cc, oi->otpgu_base + OTPGU_SFB_OFF, oi->swlim * 16);
			ipxotp_write_bit(oi, cc, otpgu_bit_base + OTPGU_SWP_OFF);
			break;
		case OTP_CI_RGN:
			ipxotp_write_bit(oi, cc, otpgu_bit_base + OTPGU_CIP_OFF);
			/* Also set the OTPGU_CIP_MSK bit in the input so verification
			 * doesn't fail
			 */
			if (wlen >= OTPGU_CI_SZ)
				data[OTPGU_CI_SZ - 1] |= OTPGU_CIP_MSK;
			break;
		case OTP_FUSE_RGN:
			ipxotp_otpwb16(oi, cc, oi->otpgu_base + OTPGU_SFB_OFF, base * 16);
			ipxotp_write_bit(oi, cc, otpgu_bit_base + OTPGU_FUSEP_OFF);
			break;
		}
	}

	/* Disable Write */
	ipxotp_unwritable(oi, cc);

	/* Sync region info by retrieving them again (use PMU bit to power cycle OTP) */
	si_otp_power(oi->sih, FALSE, &min_res_mask);
	si_otp_power(oi->sih, TRUE, &min_res_mask);

	/* Check and fix for region size and region programmed bits */
	if (!rewrite) {
		uint16	boundary_off = 0, boundary_val = 0;
		uint16	programmed_off = 0;
		uint16	bit = 0;

		switch (region) {
		case OTP_HW_RGN:
			boundary_off = OTPGU_HSB_OFF;
			/* OTP unification */
			if (oi->buotp) {
				boundary_val = ((base + oi->usbmanfid_offset) * 16);
			} else
				boundary_val = (base + i) * 16;
			programmed_off = OTPGU_HWP_OFF;
			break;
		case OTP_SW_RGN:
			/* Also write 0 to HW region boundary */
			if ((rc = ipxotp_check_word16(oi, cc, oi->otpgu_base + OTPGU_HSB_OFF,
				base * 16)))
				goto exit;
			boundary_off = OTPGU_SFB_OFF;
			boundary_val = oi->swlim * 16;
			programmed_off = OTPGU_SWP_OFF;
			break;
		case OTP_CI_RGN:
			/* No CI region boundary */
			programmed_off = OTPGU_CIP_OFF;
			break;
		case OTP_FUSE_RGN:
			boundary_off = OTPGU_SFB_OFF;
			boundary_val = base * 16;
			programmed_off = OTPGU_FUSEP_OFF;
			break;
		}

		/* Do the actual checking and return BCME_NORESOURCE if we cannot fix */
		if ((region != OTP_CI_RGN) &&
			(rc = ipxotp_check_word16(oi, cc, oi->otpgu_base + boundary_off,
			boundary_val))) {
			goto exit;
		}

		if ((bit = ipxotp_read_bit(oh, cc, otpgu_bit_base + programmed_off)) == 0xffff) {
			OTP_ERR(("\n%s: FAILED bit %d reads %d\n", __FUNCTION__, otpgu_bit_base +
				programmed_off, bit));
			goto exit;
		} else if (bit == 0) {	/* error detected, fix it */
			OTP_ERR(("\n%s: FAILED bit %d reads %d, fixing\n", __FUNCTION__,
				otpgu_bit_base + programmed_off, bit));
			if ((rc = ipxotp_write_rde(oi, ipxotp_check_otp_pmu_res(cc),
				otpgu_bit_base + programmed_off, 1))) {
				OTP_ERR(("\n%s: cannot fix, ipxotp_write_rde returns %d\n",
					__FUNCTION__, rc));
				goto exit;
			}
		}
	}

	/* Update status, apply WAR */
	_ipxotp_init(oi, cc);

#if defined(DONGLEBUILD)
	/* Recover original data... */
	if (origdata)
		bcopy(origdata, data, wlen * 2);
#endif /* DONGLEBUILD */

	/* ...Check again so we can verify and fix where possible */
	for (i = 0; i < wlen; i++) {
		if ((rc = ipxotp_check_word16(oi, cc, base + i, data[i])))
			goto exit;
	}

exit:
#if defined(DONGLEBUILD)
	if (origdata)
		MFREE(oi->osh, origdata, wlen * 2);
#endif /* DONGLEBUILD */
	si_setcoreidx(oi->sih, idx);
	return rc;
}

static int
BCMNMIATTACHFN(ipxotp_write_word)(void *oh, uint wn, uint16 data)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	int rc = 0;
	uint16 origdata;
	uint idx;
	chipcregs_t *cc;

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	/* Check for conflict */
	origdata = data;
	data = ipxotp_otpr(oi, cc, wn);
	if (data & ~origdata) {
		OTP_ERR(("%s: word %d incompatible (0x%04x->0x%04x)\n",
			__FUNCTION__, wn, data, origdata));
		rc = BCME_BADARG;
		goto exit;
	}
	data ^= origdata;

	/* Enable Write */
	ipxotp_writable(oi, cc);

	rc = ipxotp_otpwb16(oi, cc, wn, data);

	/* Disable Write */
	ipxotp_unwritable(oi, cc);

	data = origdata;
	if ((rc = ipxotp_check_word16(oi, cc, wn, data)))
		goto exit;
exit:
	si_setcoreidx(oi->sih, idx);
	return rc;
}

static int
BCMNMIATTACHFN(_ipxotp_cis_append_region)(si_t *sih, int region, char *vars, int count)
{
#define TUPLE_MATCH(_s1, _s2, _s3, _v1, _v2, _v3) \
	(((_s1) == 0x80) ? \
		(((_s1) == (_v1)) && ((_s2) == (_v2)) && ((_s3) == (_v3))) \
		: (((_s1) == (_v1)) && ((_s2) == (_v2))))

	uint8 *cis;
	osl_t *osh;
	uint sz = OTP_SZ_MAX/2; /* size in words */
	int rc = 0;
	bool newchip = FALSE;
	uint overwrite = 0;

	ASSERT(region == OTP_HW_RGN || region == OTP_SW_RGN);

	osh = si_osh(sih);
	if ((cis = MALLOC(osh, OTP_SZ_MAX)) == NULL) {
		return BCME_ERROR;
	}

	bzero(cis, OTP_SZ_MAX);

	rc = otp_read_region(sih, region, (uint16 *)cis, &sz);
	newchip = (rc == BCME_NOTFOUND) ? TRUE : FALSE;
	if ((rc != 0) && (rc != BCME_NOTFOUND)) {
		return BCME_ERROR;
	}
	rc = 0;

	/* zero count for read, non-zero count for write */
	if (count) {
		int i = 0, newlen = 0;

		if (newchip) {
			int termw_len = 0;	/* length of termination word */

			/* convert halfwords to bytes offset */
			newlen = sz * 2;

			if ((CHIPID(sih->chip) == BCM4322_CHIP_ID) ||
				(CHIPID(sih->chip) == BCM43231_CHIP_ID) ||
				(CHIPID(sih->chip) == BCM4315_CHIP_ID) ||
				(CHIPID(sih->chip) == BCM4319_CHIP_ID)) {
				/* bootloader WAR, refer to above twiki link */
				cis[newlen-1] = 0x00;
				cis[newlen-2] = 0xff;
				cis[newlen-3] = 0x00;
				cis[newlen-4] = 0xff;
				cis[newlen-5] = 0xff;
				cis[newlen-6] = 0x1;
				cis[newlen-7] = 0x2;
				termw_len = 7;
			} else {
				cis[newlen-1] = 0xff;
				cis[newlen-2] = 0xff;
				termw_len = 2;
			}

			if (count >= newlen - termw_len) {
				OTP_MSG(("OTP left %x bytes; buffer %x bytes\n", newlen, count));
				rc = BCME_BUFTOOLONG;
			}
		} else {
			int end = 0;
			if (region == OTP_SW_RGN) {
				/* Walk through the leading zeros (could be 0 or 8 bytes for now) */
				for (i = 0; i < (int)sz*2; i++)
					if (cis[i] != 0)
						break;

				if (i >= (int)((sz*2) - 2))
					i = 0;
			} else {
				/* move pass the hardware header */
				if ((sih->ccrev >= 45) && (sih->buscoretype == PCIE2_CORE_ID)) {
					/* PCIE GEN2 */
					i += 128;
				} else if (sih->ccrev >= 36) {
					uint32 otp_layout;
					otp_layout = si_corereg(sih, SI_CC_IDX,
						OFFSETOF(chipcregs_t, otplayout), 0, 0);
					if (otp_layout & OTP_CISFORMAT_NEW) {
						i += 12; /* new sdio header format, 6 half words */
					} else {
						i += 8; /* old sdio header format */
					}
				} else {
					return BCME_ERROR; /* old chip, not suppported */
				}
			}

			/* Find the place to append */
			for (; i < (int)sz*2; i++) {
				if (cis[i] == 0)
					break;

				/* find the last tuple with same BMCM HNBU */
				if (TUPLE_MATCH(cis[i], cis[i+1], cis[i+2],
					vars[0], vars[1], vars[2])) {
					overwrite = i;
				}
				i += ((int)cis[i+1] + 1);
			}
			for (end = i; end < (int)sz*2; end++) {
				if (cis[end] != 0)
					break;
			}

			/* If the tuple exist, check if it can be overwritten */
			if (overwrite)
			{
				int j;

				/* found, check if it is compiatable for fix */
				for (j = 0; j < cis[overwrite+1] + 2; j++) {
					if ((cis[overwrite+j] ^ vars[j]) & cis[overwrite+j]) {
						break;
					}
				}
				if (j == cis[overwrite+1] + 2) {
					i = overwrite;
				}
			}

			newlen = i + count;
			if (newlen & 1)		/* make it even-sized buffer */
				newlen++;

			if (newlen >= (end - 1)) {
				OTP_MSG(("OTP left %x bytes; buffer %x bytes\n", end-i, count));
				rc = BCME_BUFTOOLONG;
			}
		}

		/* copy the buffer */
		memcpy(&cis[i], vars, count);
#ifdef BCMNVRAMW
		/* Write the buffer back */
		if (!rc)
			rc = otp_write_region(sih, region, (uint16*)cis, newlen/2, 0);

		/* Print the buffer */
		OTP_MSG(("Buffer of size %d bytes to write:\n", newlen));
		for (i = 0; i < newlen; i++) {
			OTP_DBG(("%02x ", cis[i] & 0xff));
			if ((i % 16) == 15) {
				OTP_DBG(("\n"));
			}
		}
		OTP_MSG(("\n"));
#endif /* BCMNVRAMW */
	}
	if (cis)
		MFREE(osh, cis, OTP_SZ_MAX);

	return (rc);
}

/**
 * given a caller supplied CIS (in *vars), appends the tuples in the CIS to the existing CIS in
 * OTP. Tuples are appended to extend the CIS, or to overrule prior written tuples.
 */
static int
BCMNMIATTACHFN(ipxotp_cis_append_region)(si_t *sih, int region, char *vars, int count)
{
	int result;
	char *tuple;
	int tuplePos;
	int tupleCount;
	int remainingCount;

	result = BCME_ERROR;

	/* zero count for read, non-zero count for write */
	if (count) {
		if (count < (int)(*(vars + 1) + 2)) {
			OTP_MSG(("vars (count): %d bytes; tuple len: %d bytes\n",
				count, (int)(*(vars + 1))));
			return BCME_ERROR;
		}

		/* get first tuple from vars. */
		tuplePos = 0;
		remainingCount = count;

		/* separate vars into tuples and write tuples one by one to OTP. */
		do {
			tupleCount = (int)(*(vars + tuplePos + 1) + 2);
			if (remainingCount < tupleCount) {
				OTP_MSG(("vars (remainingCount): %d bytes; tuple len: %d bytes\n",
					remainingCount, tupleCount));
				return (result);
			}
			tuple = vars + tuplePos;

			result = _ipxotp_cis_append_region(sih, region, tuple, tupleCount);
			if (result != BCME_OK)
				return (result);

			/* get next tuple from vars. */
			remainingCount -= tupleCount;
			tuplePos += (((int)*(tuple + 1)) + 2);
		} while (remainingCount > 0);
	} else {
		return (_ipxotp_cis_append_region(sih, region, vars, count));
	}

	return (result);
}

/* No need to lock for IPXOTP */
static int
BCMNMIATTACHFN(ipxotp_lock)(void *oh)
{
	uint idx;
	chipcregs_t *cc;
	otpinfo_t *oi = (otpinfo_t *)oh;
	int err = 0, rc = 0;
	uint32 min_res_mask = 0;

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	/* Enable Write */
	ipxotp_writable(oi, cc);

	err = ipxotp_write_lock_bit(oi, cc, OTP_LOCK_ROW1_LOC_OFF);
	if (err) {
		OTP_ERR(("fail to lock ROW1\n"));
		rc = -1;
	}
	err =  ipxotp_write_lock_bit(oi, cc, OTP_LOCK_ROW2_LOC_OFF);
	if (err) {
		OTP_ERR(("fail to lock ROW2\n"));
		rc = -2;
	}
	err = ipxotp_write_lock_bit(oi, cc, OTP_LOCK_RD_LOC_OFF);
	if (err) {
		OTP_ERR(("fail to lock RD\n"));
		rc = -3;
	}
	err = ipxotp_write_lock_bit(oi, cc, OTP_LOCK_GU_LOC_OFF);
	if (err) {
		OTP_ERR(("fail to lock GU\n"));
		rc = -4;
	}

	/* Disable Write */
	ipxotp_unwritable(oi, cc);

	/* Sync region info by retrieving them again (use PMU bit to power cycle OTP) */
	si_otp_power(oi->sih, FALSE, &min_res_mask);
	si_otp_power(oi->sih, TRUE, &min_res_mask);

	/* Update status, apply WAR */
	_ipxotp_init(oi, cc);

	si_setcoreidx(oi->sih, idx);

	return rc;
}

static int
BCMNMIATTACHFN(ipxotp_nvwrite)(void *oh, uint16 *data, uint wlen)
{
	return -1;
}
#endif /* BCMNVRAMW */
#endif /* !defined(BCMDONGLEHOST) */

#if defined(WLTEST) && !defined(BCMROMBUILD)
static int
BCMNMIATTACHFN(ipxotp_dump)(void *oh, int arg, char *buf, uint size)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	chipcregs_t *cc;
	uint idx, i, count;
	uint16 val;
	struct bcmstrbuf b;

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	count = ipxotp_size(oh);

	bcm_binit(&b, buf, size);
	for (i = 0; i < count / 2; i++) {
		if (!(i % 4))
			bcm_bprintf(&b, "\n0x%04x:", 2 * i);
		if (arg == 0)
			val = ipxotp_otpr(oh, cc, i);
		else
			val = ipxotp_otprb16(oi, cc, i);
		bcm_bprintf(&b, " 0x%04x", val);
	}
	bcm_bprintf(&b, "\n");

	si_setcoreidx(oi->sih, idx);

	return ((int)(b.buf - b.origbuf));
}
#endif	

static otp_fn_t ipxotp_fn = {
	(otp_size_t)ipxotp_size,
	(otp_read_bit_t)ipxotp_read_bit,
	(otp_dump_t)NULL,		/* Assigned in otp_init */
	(otp_status_t)ipxotp_status,

#if !defined(BCMDONGLEHOST)
	(otp_init_t)ipxotp_init,
	(otp_read_region_t)ipxotp_read_region,
	(otp_nvread_t)ipxotp_nvread,
#ifdef BCMNVRAMW
	(otp_write_region_t)ipxotp_write_region,
	(otp_cis_append_region_t)ipxotp_cis_append_region,
	(otp_lock_t)ipxotp_lock,
	(otp_nvwrite_t)ipxotp_nvwrite,
	(otp_write_word_t)ipxotp_write_word,
#else /* BCMNVRAMW */
	(otp_write_region_t)NULL,
	(otp_cis_append_region_t)NULL,
	(otp_lock_t)NULL,
	(otp_nvwrite_t)NULL,
	(otp_write_word_t)NULL,
#endif /* BCMNVRAMW */
	(otp_read_word_t)ipxotp_read_word,
#endif /* !defined(BCMDONGLEHOST) */
#if !defined(BCMDONGLEHOST)
#ifdef BCMNVRAMW
	(otp_write_bits_t)ipxotp_write_bits
#else
	(otp_write_bits_t)NULL
#endif /* BCMNVRAMW */
#endif /* !BCMDONGLEHOST */
};

/*
 * ROM accessor to avoid struct in shdat. Declare BCMRAMFN() to force the accessor to be excluded
 * from ROM.
 */
static otp_fn_t *
BCMRAMFN(get_ipxotp_fn)(void)
{
	return &ipxotp_fn;
}
#endif /* BCMIPXOTP */

/*
 * HND OTP Code
 *
 *   Exported functions:
 *	hndotp_status()
 *	hndotp_size()
 *	hndotp_init()
 *	hndotp_read_bit()
 *	hndotp_read_region()
 *	hndotp_read_word()
 *	hndotp_nvread()
 *	hndotp_write_region()
 *	hndotp_cis_append_region()
 *	hndotp_lock()
 *	hndotp_nvwrite()
 *	hndotp_dump()
 *
 *   HND internal functions:
 * 	hndotp_otpr()
 * 	hndotp_otproff()
 *	hndotp_write_bit()
 *	hndotp_write_word()
 *	hndotp_valid_rce()
 *	hndotp_write_rce()
 *	hndotp_write_row()
 *	hndotp_otprb16()
 *
 */

#ifdef BCMHNDOTP

/* Fields in otpstatus */
#define	OTPS_PROGFAIL		0x80000000
#define	OTPS_PROTECT		0x00000007
#define	OTPS_HW_PROTECT		0x00000001
#define	OTPS_SW_PROTECT		0x00000002
#define	OTPS_CID_PROTECT	0x00000004
#define	OTPS_RCEV_MSK		0x00003f00
#define	OTPS_RCEV_SHIFT		8

/* Fields in the otpcontrol register */
#define	OTPC_RECWAIT		0xff000000
#define	OTPC_PROGWAIT		0x00ffff00
#define	OTPC_PRW_SHIFT		8
#define	OTPC_MAXFAIL		0x00000038
#define	OTPC_VSEL		0x00000006
#define	OTPC_SELVL		0x00000001

/* OTP regions (Word offsets from otp size) */
#define	OTP_SWLIM_OFF	(-4)
#define	OTP_CIDBASE_OFF	0
#define	OTP_CIDLIM_OFF	4

/* Predefined OTP words (Word offset from otp size) */
#define	OTP_BOUNDARY_OFF (-4)
#define	OTP_HWSIGN_OFF	(-3)
#define	OTP_SWSIGN_OFF	(-2)
#define	OTP_CIDSIGN_OFF	(-1)
#define	OTP_CID_OFF	0
#define	OTP_PKG_OFF	1
#define	OTP_FID_OFF	2
#define	OTP_RSV_OFF	3
#define	OTP_LIM_OFF	4
#define	OTP_RD_OFF	4	/* Redundancy row starts here */
#define	OTP_RC0_OFF	28	/* Redundancy control word 1 */
#define	OTP_RC1_OFF	32	/* Redundancy control word 2 */
#define	OTP_RC_LIM_OFF	36	/* Redundancy control word end */

#define	OTP_HW_REGION	OTPS_HW_PROTECT
#define	OTP_SW_REGION	OTPS_SW_PROTECT
#define	OTP_CID_REGION	OTPS_CID_PROTECT

#if OTP_HW_REGION != OTP_HW_RGN
#error "incompatible OTP_HW_RGN"
#endif
#if OTP_SW_REGION != OTP_SW_RGN
#error "incompatible OTP_SW_RGN"
#endif
#if OTP_CID_REGION != OTP_CI_RGN
#error "incompatible OTP_CI_RGN"
#endif

/* Redundancy entry definitions */
#define	OTP_RCE_ROW_SZ		6
#define	OTP_RCE_SIGN_MASK	0x7fff
#define	OTP_RCE_ROW_MASK	0x3f
#define	OTP_RCE_BITS		21
#define	OTP_RCE_SIGN_SZ		15
#define	OTP_RCE_BIT0		1

#define	OTP_WPR		4
#define	OTP_SIGNATURE	0x578a
#define	OTP_MAGIC	0x4e56

static int
BCMNMIATTACHFN(hndotp_status)(void *oh)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	return ((int)(oi->hwprot | oi->signvalid));
}

static int
BCMNMIATTACHFN(hndotp_size)(void *oh)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	return ((int)(oi->size));
}

#if !defined(BCMDONGLEHOST) || defined(WLTEST)
static uint16
BCMNMIATTACHFN(hndotp_otpr)(void *oh, chipcregs_t *cc, uint wn)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	osl_t *osh;
	volatile uint16 *ptr;

	ASSERT(wn < ((oi->size / 2) + OTP_RC_LIM_OFF));
	ASSERT(cc != NULL);

	osh = si_osh(oi->sih);

	ptr = (volatile uint16 *)((volatile char *)cc + CC_SROM_OTP);
	return (R_REG(osh, &ptr[wn]));
}
#endif 

#if !defined(BCMDONGLEHOST)
static uint16
BCMNMIATTACHFN(hndotp_otproff)(void *oh, chipcregs_t *cc, int woff)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	osl_t *osh;
	volatile uint16 *ptr;

	ASSERT(woff >= (-((int)oi->size / 2)));
	ASSERT(woff < OTP_LIM_OFF);
	ASSERT(cc != NULL);

	osh = si_osh(oi->sih);

	ptr = (volatile uint16 *)((volatile char *)cc + CC_SROM_OTP);

	return (R_REG(osh, &ptr[(oi->size / 2) + woff]));
}
#endif /* !defined(BCMDONGLEHOST) */

static uint16
BCMNMIATTACHFN(hndotp_read_bit)(void *oh, chipcregs_t *cc, uint idx)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint k, row, col;
	uint32 otpp, st;
	osl_t *osh;

	osh = si_osh(oi->sih);
	row = idx / 65;
	col = idx % 65;

	otpp = OTPP_START_BUSY | OTPP_READ |
	        ((row << OTPP_ROW_SHIFT) & OTPP_ROW_MASK) |
	        (col & OTPP_COL_MASK);

	OTP_DBG(("%s: idx = %d, row = %d, col = %d, otpp = 0x%x", __FUNCTION__,
	         idx, row, col, otpp));

	W_REG(osh, &cc->otpprog, otpp);
	st = R_REG(osh, &cc->otpprog);
	for (k = 0; ((st & OTPP_START_BUSY) == OTPP_START_BUSY) && (k < OTPP_TRIES); k++)
		st = R_REG(osh, &cc->otpprog);

	if (k >= OTPP_TRIES) {
		OTP_ERR(("\n%s: BUSY stuck: st=0x%x, count=%d\n", __FUNCTION__, st, k));
		return 0xffff;
	}
	if (st & OTPP_READERR) {
		OTP_ERR(("\n%s: Could not read OTP bit %d\n", __FUNCTION__, idx));
		return 0xffff;
	}
	st = (st & OTPP_VALUE_MASK) >> OTPP_VALUE_SHIFT;
	OTP_DBG((" => %d\n", st));
	return (uint16)st;
}

#if !defined(BCMDONGLEHOST)
static void *
BCMNMIATTACHFN(hndotp_init)(si_t *sih)
{
	uint idx;
	chipcregs_t *cc;
	otpinfo_t *oi;
	uint32 cap = 0, clkdiv, otpdiv = 0;
	void *ret = NULL;
	osl_t *osh;

	OTP_MSG(("%s: Use HND OTP controller\n", __FUNCTION__));

	oi = get_otpinfo();

	idx = si_coreidx(sih);
	osh = si_osh(oi->sih);

	/* Check for otp */
	if ((cc = si_setcoreidx(sih, SI_CC_IDX)) != NULL) {
		cap = R_REG(osh, &cc->capabilities);
		if ((cap & CC_CAP_OTPSIZE) == 0) {
			/* Nothing there */
			goto out;
		}

		/* As of right now, support only 4320a2, 4311a1 and 4312 */
		ASSERT((oi->ccrev == 12) || (oi->ccrev == 17) || (oi->ccrev == 22));
		if (!((oi->ccrev == 12) || (oi->ccrev == 17) || (oi->ccrev == 22)))
			return NULL;

		/* Read the OTP byte size. chipcommon rev >= 18 has RCE so the size is
		 * 8 row (64 bytes) smaller
		 */
		oi->size = 1 << (((cap & CC_CAP_OTPSIZE) >> CC_CAP_OTPSIZE_SHIFT)
			+ CC_CAP_OTPSIZE_BASE);
		if (oi->ccrev >= 18) {
			oi->size -= ((OTP_RC0_OFF - OTP_BOUNDARY_OFF) * 2);
		} else {
			OTP_ERR(("Negative otp size, shouldn't happen for programmed chip."));
			oi->size = 0;
		}

		oi->hwprot = (int)(R_REG(osh, &cc->otpstatus) & OTPS_PROTECT);
		oi->boundary = -1;

		/* Check the region signature */
		if (hndotp_otproff(oi, cc, OTP_HWSIGN_OFF) == OTP_SIGNATURE) {
			oi->signvalid |= OTP_HW_REGION;
			oi->boundary = hndotp_otproff(oi, cc, OTP_BOUNDARY_OFF);
		}

		if (hndotp_otproff(oi, cc, OTP_SWSIGN_OFF) == OTP_SIGNATURE)
			oi->signvalid |= OTP_SW_REGION;

		if (hndotp_otproff(oi, cc, OTP_CIDSIGN_OFF) == OTP_SIGNATURE)
			oi->signvalid |= OTP_CID_REGION;

		/* Set OTP clkdiv for stability */
		if (oi->ccrev == 22)
			otpdiv = 12;

		if (otpdiv) {
			clkdiv = R_REG(osh, &cc->clkdiv);
			clkdiv = (clkdiv & ~CLKD_OTP) | (otpdiv << CLKD_OTP_SHIFT);
			W_REG(osh, &cc->clkdiv, clkdiv);
			OTP_MSG(("%s: set clkdiv to %x\n", __FUNCTION__, clkdiv));
		}
		OSL_DELAY(10);

		ret = (void *)oi;
	}

	OTP_MSG(("%s: ccrev %d\tsize %d bytes\thwprot %x\tsignvalid %x\tboundary %x\n",
		__FUNCTION__, oi->ccrev, oi->size, oi->hwprot, oi->signvalid,
		oi->boundary));

out:	/* All done */
	si_setcoreidx(sih, idx);

	return ret;
}

static int
BCMNMIATTACHFN(hndotp_read_region)(void *oh, int region, uint16 *data, uint *wlen)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint32 idx, st;
	chipcregs_t *cc;
	int i;

	/* Only support HW region (no active chips use HND OTP SW region) */
	ASSERT(region == OTP_HW_REGION);

	OTP_MSG(("%s: region %x wlen %d\n", __FUNCTION__, region, *wlen));

	/* Region empty? */
	st = oi->hwprot | oi-> signvalid;
	if ((st & region) == 0)
		return BCME_NOTFOUND;

	*wlen = ((int)*wlen < oi->boundary/2) ? *wlen : (uint)oi->boundary/2;

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	for (i = 0; i < (int)*wlen; i++)
		data[i] = hndotp_otpr(oh, cc, i);

	si_setcoreidx(oi->sih, idx);

	return 0;
}

static int
BCMNMIATTACHFN(hndotp_read_word)(void *oh, uint wn, uint16 *data)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint32 idx;
	chipcregs_t *cc;

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	*data = hndotp_otpr(oh, cc, wn);

	si_setcoreidx(oi->sih, idx);
	return 0;
}

static int
BCMNMIATTACHFN(hndotp_nvread)(void *oh, char *data, uint *len)
{
	int rc = 0;
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint32 base, bound, lim = 0, st;
	int i, chunk, gchunks, tsz = 0;
	uint32 idx;
	chipcregs_t *cc;
	uint offset;
	uint16 *rawotp = NULL;

	/* save the orig core */
	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	st = hndotp_status(oh);
	if (!(st & (OTP_HW_REGION | OTP_SW_REGION))) {
		OTP_ERR(("OTP not programmed\n"));
		rc = -1;
		goto out;
	}

	/* Read the whole otp so we can easily manipulate it */
	lim = hndotp_size(oh);
	if (lim == 0) {
		OTP_ERR(("OTP size is 0\n"));
		rc = -1;
		goto out;
	}
	if ((rawotp = MALLOC(si_osh(oi->sih), lim)) == NULL) {
		OTP_ERR(("Out of memory for rawotp\n"));
		rc = -2;
		goto out;
	}
	for (i = 0; i < (int)(lim / 2); i++)
		rawotp[i] = hndotp_otpr(oh, cc,  i);

	if ((st & OTP_HW_REGION) == 0) {
		OTP_ERR(("otp: hw region not written (0x%x)\n", st));

		/* This could be a programming failure in the first
		 * chunk followed by one or more good chunks
		 */
		for (i = 0; i < (int)(lim / 2); i++)
			if (rawotp[i] == OTP_MAGIC)
				break;

		if (i < (int)(lim / 2)) {
			base = i;
			bound = (i * 2) + rawotp[i + 1];
			OTP_MSG(("otp: trying chunk at 0x%x-0x%x\n", i * 2, bound));
		} else {
			OTP_MSG(("otp: unprogrammed\n"));
			rc = -3;
			goto out;
		}
	} else {
		bound = rawotp[(lim / 2) + OTP_BOUNDARY_OFF];

		/* There are two cases: 1) The whole otp is used as nvram
		 * and 2) There is a hardware header followed by nvram.
		 */
		if (rawotp[0] == OTP_MAGIC) {
			base = 0;
			if (bound != rawotp[1])
				OTP_MSG(("otp: Bound 0x%x != chunk0 len 0x%x\n", bound,
				         rawotp[1]));
		} else
			base = bound;
	}

	/* Find and copy the data */

	chunk = 0;
	gchunks = 0;
	i = base / 2;
	offset = 0;
	while ((i < (int)(lim / 2)) && (rawotp[i] == OTP_MAGIC)) {
		int dsz, rsz = rawotp[i + 1];

		if (((i * 2) + rsz) >= (int)lim) {
			OTP_MSG(("  bad chunk size, chunk %d, base 0x%x, size 0x%x\n",
			         chunk, i * 2, rsz));
			/* Bad length, try to find another chunk anyway */
			rsz = 6;
		}
		if (hndcrc16((uint8 *)&rawotp[i], rsz,
		             CRC16_INIT_VALUE) == CRC16_GOOD_VALUE) {
			/* Good crc, copy the vars */
			OTP_MSG(("  good chunk %d, base 0x%x, size 0x%x\n",
			         chunk, i * 2, rsz));
			gchunks++;
			dsz = rsz - 6;
			tsz += dsz;
			if (offset + dsz >= *len) {
				OTP_MSG(("Out of memory for otp\n"));
				goto out;
			}
			bcopy((char *)&rawotp[i + 2], &data[offset], dsz);
			offset += dsz;
			/* Remove extra null characters at the end */
			while (offset > 1 &&
			       data[offset - 1] == 0 && data[offset - 2] == 0)
				offset --;
			i += rsz / 2;
		} else {
			/* bad length or crc didn't check, try to find the next set */
			OTP_MSG(("  chunk %d @ 0x%x size 0x%x: bad crc, ",
			         chunk, i * 2, rsz));
			if (rawotp[i + (rsz / 2)] == OTP_MAGIC) {
				/* Assume length is good */
				i += rsz / 2;
			} else {
				while (++i < (int)(lim / 2))
					if (rawotp[i] == OTP_MAGIC)
						break;
			}
			if (i < (int)(lim / 2))
				OTP_MSG(("trying next base 0x%x\n", i * 2));
			else
				OTP_MSG(("no more chunks\n"));
		}
		chunk++;
	}

	OTP_MSG(("  otp size = %d, boundary = 0x%x, nv base = 0x%x\n", lim, bound, base));
	if (tsz != 0) {
		OTP_MSG(("  Found %d bytes in %d good chunks out of %d\n", tsz, gchunks, chunk));
	} else {
		OTP_MSG(("  No good chunks found out of %d\n", chunk));
	}

	*len = offset;

out:
	if (rawotp)
		MFREE(si_osh(oi->sih), rawotp, lim);
	si_setcoreidx(oi->sih, idx);

	return rc;
}

#ifdef BCMNVRAMW
#if defined(BCMDBG) || defined(WLTEST)
static	uint st_n, st_s, st_hwm, pp_hwm;
#ifdef	OTP_FORCEFAIL
static	uint forcefail_bitcount = 0;
#endif /* OTP_FORCEFAIL */
#endif /* BCMDBG || WLTEST */

static int
BCMNMIATTACHFN(hndotp_write_bit)(void *oh, chipcregs_t *cc, int bn, bool bit, int no_retry)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint row, col, j, k;
	uint32 pwait, init_pwait, otpc, otpp, pst, st;
	osl_t *osh;

	osh = si_osh(oi->sih);
	ASSERT((bit >> 1) == 0);

#ifdef	OTP_FORCEFAIL
	OTP_MSG(("%s: [0x%x] = 0x%x\n", __FUNCTION__, wn * 2, data));
#endif

	/* This is bit-at-a-time writing, future cores may do word-at-a-time */
	if (oi->ccrev == 12) {
		otpc = 0x20000001;
		init_pwait = 0x00000200;
	} else if (oi->ccrev == 22) {
		otpc = 0x20000001;
		init_pwait = 0x00000400;
	} else {
		otpc = 0x20000001;
		init_pwait = 0x00004000;
	}

	pwait = init_pwait;
	row = bn / 65;
	col = bn % 65;
	otpp = OTPP_START_BUSY |
		((bit << OTPP_VALUE_SHIFT) & OTPP_VALUE_MASK) |
		((row << OTPP_ROW_SHIFT) & OTPP_ROW_MASK) |
		(col & OTPP_COL_MASK);
	j = 0;
	while (1) {
		j++;
		if (j > 1) {
			OTP_DBG(("row %d, col %d, val %d, otpc 0x%x, otpp 0x%x\n",
				row, col, bit, (otpc | pwait), otpp));
		}
		W_REG(osh, &cc->otpcontrol, otpc | pwait);
		W_REG(osh, &cc->otpprog, otpp);
		pst = R_REG(osh, &cc->otpprog);
		for (k = 0; ((pst & OTPP_START_BUSY) == OTPP_START_BUSY) && (k < OTPP_TRIES); k++)
			pst = R_REG(osh, &cc->otpprog);
#if defined(BCMDBG) || defined(WLTEST)
		if (k > pp_hwm)
			pp_hwm = k;
#endif /* BCMDBG || WLTEST */
		if (k >= OTPP_TRIES) {
			OTP_ERR(("BUSY stuck: pst=0x%x, count=%d\n", pst, k));
			st = OTPS_PROGFAIL;
			break;
		}
		st = R_REG(osh, &cc->otpstatus);
		if (((st & OTPS_PROGFAIL) == 0) || (pwait == OTPC_PROGWAIT) || (no_retry)) {
			break;
		} else {
			if ((oi->ccrev == 12) || (oi->ccrev == 22))
				pwait = (pwait << 3) & OTPC_PROGWAIT;
			else
				pwait = (pwait << 1) & OTPC_PROGWAIT;
			if (pwait == 0)
				pwait = OTPC_PROGWAIT;
		}
	}
#if defined(BCMDBG) || defined(WLTEST)
	st_n++;
	st_s += j;
	if (j > st_hwm)
		 st_hwm = j;
#ifdef	OTP_FORCEFAIL
	if (forcefail_bitcount++ == OTP_FORCEFAIL * 16) {
		OTP_DBG(("Forcing PROGFAIL on bit %d (FORCEFAIL = %d/0x%x)\n",
			forcefail_bitcount, OTP_FORCEFAIL, OTP_FORCEFAIL));
		st = OTPS_PROGFAIL;
	}
#endif
#endif /* BCMDBG || WLTEST */
	if (st & OTPS_PROGFAIL) {
		OTP_ERR(("After %d tries: otpc = 0x%x, otpp = 0x%x/0x%x, otps = 0x%x\n",
		       j, otpc | pwait, otpp, pst, st));
		OTP_ERR(("otp prog failed. bit=%d, ppret=%d, ret=%d\n", bit, k, j));
		return 1;
	}

	return 0;
}

#if !defined(BCMDONGLEHOST)
static int
BCMNMIATTACHFN(hndotp_write_bits)(void *oh, int bn, int bits, uint8* data)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint idx;
	chipcregs_t *cc;
	int i, j;
	uint8 temp;

	if (bn < 0 || bn + bits >= oi->rows * oi->cols)
		return BCME_RANGE;

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	for (i = 0; i < bits; ) {
		temp = *data++;
		for (j = 0; j < 8; j++, i++) {
			if (i >= bits)
				break;
			if (hndotp_write_bit(oh, cc, i + bn, (temp & 0x01), 0) != 0) {
				return -1;
			}
			temp >>= 1;
		}
	}

	si_setcoreidx(oi->sih, idx);
	return BCME_OK;
}
#endif /* !BCMDONGLEHOST */

static int
BCMNMIATTACHFN(hndotp_write_word)(void *oh, chipcregs_t *cc, int wn, uint16 data)
{
	uint base, i;
	int err = 0;

	OTP_MSG(("%s: wn %d data %x\n", __FUNCTION__, wn, data));

	/* There is one test bit for each row */
	base = (wn * 16) + (wn / 4);

	for (i = 0; i < 16; i++) {
		err += hndotp_write_bit(oh, cc, base + i, data & 1, 0);
		data >>= 1;
		/* abort write after first error to avoid stress the charge-pump */
		if (err) {
			OTP_DBG(("%s: wn %d fail on bit %d\n", __FUNCTION__, wn, i));
			break;
		}
	}

	return err;
}

static int
BCMNMIATTACHFN(hndotp_valid_rce)(void *oh, chipcregs_t *cc, int i)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	osl_t *osh;
	uint32 hwv, fw, rce, e, sign, row, st;

	ASSERT(oi->ccrev >= 18);

	/* HW valid bit */
	osh = si_osh(oi->sih);
	st = R_REG(osh, &cc->otpstatus);
	hwv = (st & OTPS_RCEV_MSK) & (1 << (OTPS_RCEV_SHIFT + i));
	BCM_REFERENCE(hwv);

	if (i < 3) {
		e = i;
		fw = hndotp_size(oh)/2 + OTP_RC0_OFF + e;
	} else {
		e = i - 3;
		fw = hndotp_size(oh)/2 + OTP_RC1_OFF + e;
	}

	rce = hndotp_otpr(oh, cc, fw+1) << 16 | hndotp_otpr(oh, cc, fw);
	rce >>= ((e * OTP_RCE_BITS) + OTP_RCE_BIT0 - (e * 16));
	row = rce & OTP_RCE_ROW_MASK;
	sign = (rce >> OTP_RCE_ROW_SZ) & OTP_RCE_SIGN_MASK;

	OTP_MSG(("rce %d sign %x row %d hwv %x\n", i, sign, row, hwv));

	return (sign == OTP_SIGNATURE) ? row : -1;
}

static int
BCMNMIATTACHFN(hndotp_write_rce)(void *oh, chipcregs_t *cc, int r, uint16* data)
{
	int i, rce = -1;
	uint32	sign;

	ASSERT(((otpinfo_t *)oh)->ccrev >= 18);
	ASSERT(r >= 0 && r < hndotp_size(oh)/(2*OTP_WPR));
	ASSERT(data);

	for (rce = OTP_RCE_ROW_SZ -1; rce >= 0; rce--) {
		int e, rt, rcr, bit, err = 0;

		int rr = hndotp_valid_rce(oh, cc, rce);
		/* redundancy row in use already */
		if (rr != -1) {
			if (rr == r) {
				OTP_MSG(("%s: row %d already replaced by RCE %d",
					__FUNCTION__, r, rce));
				return 0;
			}

			continue; /* If row used, go for the next row */
		}

		/*
		 * previously used bad rce entry maybe treaed as valid rce and used again, abort on
		 * first bit error to avoid stress the charge pump
		 */

		/* Write the data to the redundant row */
		for (i = 0; i < OTP_WPR; i++) {
			err += hndotp_write_word(oh, cc, hndotp_size(oh)/2+OTP_RD_OFF+rce*4+i,
				data[i]);
			if (err) {
				OTP_MSG(("fail to write redundant row %d\n", rce));
				break;
			}
		}

		/* Now write the redundant row index */
		if (rce < 3) {
			e = rce;
			rcr = hndotp_size(oh)/2 + OTP_RC0_OFF;
		} else {
			e = rce - 3;
			rcr = hndotp_size(oh)/2 + OTP_RC1_OFF;
		}

		/* Write row numer bit-by-bit */
		bit = (rcr * 16 + rcr / 4) + e * OTP_RCE_BITS + OTP_RCE_BIT0;
		rt = r;
		for (i = 0; i < OTP_RCE_ROW_SZ; i++) {
			/* If any timeout happened, invalidate the subsequent bits with 0 */
			if (hndotp_write_bit(oh, cc, bit, (rt & (err ? 0 : 1)), err)) {
				OTP_MSG(("%s: timeout fixing row %d with RCE %d - at row"
					" number bit %x\n", __FUNCTION__, r, rce, i));
				err++;
			}
			rt >>= 1;
			bit ++;
		}

		/* Write the RCE signature bit-by-bit */
		sign = OTP_SIGNATURE;
		for (i = 0; i < OTP_RCE_SIGN_SZ; i++) {
			/* If any timeout happened, invalidate the subsequent bits with 0 */
			if (hndotp_write_bit(oh, cc, bit, (sign & (err ? 0 : 1)), err)) {
				OTP_MSG(("%s: timeout fixing row %d with RCE %d - at row"
					" number bit %x\n", __FUNCTION__, r, rce, i));
				err++;
			}
			sign >>= 1;
			bit ++;
		}

		if (err) {
			OTP_ERR(("%s: row %d not fixed by RCE %d due to %d timeouts. try next"
				" RCE\n", __FUNCTION__, r, rce, err));
			continue;
		} else {
			OTP_MSG(("%s: Fixed row %d by RCE %d\n", __FUNCTION__, r, rce));
			return BCME_OK;
		}
	}

	OTP_ERR(("All RCE's are in use. Failed fixing OTP.\n"));
	/* Fatal error, unfixable. MFGC will have to fail. Board needs to be discarded!!  */
	return BCME_NORESOURCE;
}

/** Write a row and fix it with RCE if any error detected */
static int
BCMNMIATTACHFN(hndotp_write_row)(void *oh, chipcregs_t *cc, int wn, uint16* data, bool rewrite)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	int err = 0, i;

	ASSERT(wn % OTP_WPR == 0);

	/* Write the data */
	for (i = 0; i < OTP_WPR; i++) {
		if (rewrite && (data[i] == hndotp_otpr(oh, cc, wn+i)))
			continue;

		err += hndotp_write_word(oh, cc, wn + i, data[i]);
	}

	/* Fix this row if any error */
	if (err && (oi->ccrev >= 18)) {
		OTP_DBG(("%s: %d write errors in row %d. Fixing...\n", __FUNCTION__, err, wn/4));
		if ((err = hndotp_write_rce(oh, cc, wn / OTP_WPR, data)))
			OTP_MSG(("%s: failed to fix row %d\n", __FUNCTION__, wn/4));
	}

	return err;
}

/** expects the caller to disable interrupts before calling this routine */
static int
BCMNMIATTACHFN(hndotp_write_region)(void *oh, int region, uint16 *data, uint wlen, uint flags)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint32 st;
	uint wn, base = 0, lim;
	int ret = BCME_OK;
	uint idx;
	chipcregs_t *cc;
	bool rewrite = FALSE;
	uint32	save_clk;

	ASSERT(wlen % OTP_WPR == 0);

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	/* Check valid region */
	if ((region != OTP_HW_REGION) &&
	    (region != OTP_SW_REGION) &&
	    (region != OTP_CID_REGION)) {
		ret = BCME_BADARG;
		goto out;
	}

	/* Region already written? */
	st = oi->hwprot | oi-> signvalid;
	if ((st & region) != 0)
		rewrite = TRUE;

	/* HW and CID have to be written before SW */
	if ((((st & (OTP_HW_REGION | OTP_CID_REGION)) == 0) &&
		(st & OTP_SW_REGION) != 0)) {
		OTP_ERR(("%s: HW/CID region should be programmed first\n", __FUNCTION__));
		ret = BCME_BADARG;
		goto out;
	}

	/* Bounds for the region */
	lim = (oi->size / 2) + OTP_SWLIM_OFF;
	if (region == OTP_HW_REGION) {
		base = 0;
	} else if (region == OTP_SW_REGION) {
		base = oi->boundary / 2;
	} else if (region == OTP_CID_REGION) {
		base = (oi->size / 2) + OTP_CID_OFF;
		lim = (oi->size / 2) + OTP_LIM_OFF;
	}

	if (wlen > (lim - base)) {
		ret = BCME_BUFTOOLONG;
		goto out;
	}
	lim = base + wlen;

#if defined(BCMDBG) || defined(WLTEST)
	st_n = st_s = st_hwm = pp_hwm = 0;
#endif /* BCMDBG || WLTEST */

	/* force ALP for progrramming stability */
	save_clk = R_REG(oi->osh, &cc->clk_ctl_st);
	OR_REG(oi->osh, &cc->clk_ctl_st, CCS_FORCEALP);
	OSL_DELAY(10);

	/* Write the data row by row */
	for (wn = base; wn < lim; wn += OTP_WPR, data += OTP_WPR) {
		if ((ret = hndotp_write_row(oh, cc, wn, data, rewrite)) != 0) {
			if (ret == BCME_NORESOURCE) {
				OTP_ERR(("%s: Abort at word %x\n", __FUNCTION__, wn));
				break;
			}
		}
	}

	/* Don't need to update signature & boundary if rewrite */
	if (rewrite)
		goto out_rclk;

	/* Done with the data, write the signature & boundary if needed */
	if (region == OTP_HW_REGION) {
		if (hndotp_write_word(oh, cc, (oi->size / 2) + OTP_BOUNDARY_OFF, lim * 2) != 0) {
			ret = BCME_NORESOURCE;
			goto out_rclk;
		}
		if (hndotp_write_word(oh, cc, (oi->size / 2) + OTP_HWSIGN_OFF,
			OTP_SIGNATURE) != 0) {
			ret = BCME_NORESOURCE;
			goto out_rclk;
		}
		oi->boundary = lim * 2;
		oi->signvalid |= OTP_HW_REGION;
	} else if (region == OTP_SW_REGION) {
		if (hndotp_write_word(oh, cc, (oi->size / 2) + OTP_SWSIGN_OFF,
			OTP_SIGNATURE) != 0) {
			ret = BCME_NORESOURCE;
			goto out_rclk;
		}
		oi->signvalid |= OTP_SW_REGION;
	} else if (region == OTP_CID_REGION) {
		if (hndotp_write_word(oh, cc, (oi->size / 2) + OTP_CIDSIGN_OFF,
			OTP_SIGNATURE) != 0) {
			ret = BCME_NORESOURCE;
			goto out_rclk;
		}
		oi->signvalid |= OTP_CID_REGION;
	}

out_rclk:
	/* Restore clock */
	W_REG(oi->osh, &cc->clk_ctl_st, save_clk);

out:
#if defined(BCMDBG) || defined(WLTEST)
	OTP_MSG(("bits written: %d, average (%d/%d): %d, max retry: %d, pp max: %d\n",
		st_n, st_s, st_n, st_n?(st_s / st_n):0, st_hwm, pp_hwm));
#endif

	si_setcoreidx(oi->sih, idx);

	return ret;
}

/** For HND OTP, there's no space for appending after filling in SROM image */
static int
BCMNMIATTACHFN(hndotp_cis_append_region)(si_t *sih, int region, char *vars, int count)
{
	return otp_write_region(sih, region, (uint16*)vars, count/2, 0);
}

/**
 * Fill all unwritten RCE signature with 0 and return the number of them.
 * HNDOTP needs lock due to the randomness of unprogrammed content.
 */
static int
BCMNMIATTACHFN(hndotp_lock)(void *oh)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	int i, j, e, rcr, bit, ret = 0;
	uint32 st, idx;
	chipcregs_t *cc;

	ASSERT(oi->ccrev >= 18);

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	/* Region already written? */
	st = oi->hwprot | oi-> signvalid;
	if ((st & (OTP_HW_REGION | OTP_SW_REGION)) == 0) {
		si_setcoreidx(oi->sih, idx);
		return BCME_NOTREADY;	/* Don't lock unprogrammed OTP */
	}

	/* Find the highest valid RCE */
	for (i = 0; i < OTP_RCE_ROW_SZ -1; i++) {
		if ((hndotp_valid_rce(oh, cc, i) != -1))
			break;
	}
	i--;	/* Start invalidating from the next RCE */

	for (; i >= 0; i--) {
		if ((hndotp_valid_rce(oh, cc, i) == -1)) {

			ret++;	/* This is a unprogrammed row */

			/* Invalidate the row with 0 */
			if (i < 3) {
				e = i;
				rcr = hndotp_size(oh)/2 + OTP_RC0_OFF;
			} else {
				e = i - 3;
				rcr = hndotp_size(oh)/2 + OTP_RC1_OFF;
			}

			/* Fill row numer and signature with 0 bit-by-bit */
			bit = (rcr * 16 + rcr / 4) + e * OTP_RCE_BITS + OTP_RCE_BIT0;
			for (j = 0; j < (OTP_RCE_ROW_SZ + OTP_RCE_SIGN_SZ); j++) {
				hndotp_write_bit(oh, cc, bit, 0, 1);
				bit ++;
			}

			OTP_MSG(("locking rce %d\n", i));
		}
	}

	si_setcoreidx(oi->sih, idx);

	return ret;
}

/* expects the caller to disable interrupts before calling this routine */
static int
BCMNMIATTACHFN(hndotp_nvwrite)(void *oh, uint16 *data, uint wlen)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint32 st;
	uint16 crc, clen, *p, hdr[2];
	uint wn, base = 0, lim;
	int err, gerr = 0;
	uint idx;
	chipcregs_t *cc;

	/* otp already written? */
	st = oi->hwprot | oi-> signvalid;
	if ((st & (OTP_HW_REGION | OTP_SW_REGION)) == (OTP_HW_REGION | OTP_SW_REGION))
		return BCME_EPERM;

	/* save the orig core */
	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	/* Bounds for the region */
	lim = (oi->size / 2) + OTP_SWLIM_OFF;
	base = 0;

	/* Look for possible chunks from the end down */
	wn = lim;
	while (wn > 0) {
		wn--;
		if (hndotp_otpr(oh, cc, wn) == OTP_MAGIC) {
			base = wn + (hndotp_otpr(oh, cc, wn + 1) / 2);
			break;
		}
	}
	if (base == 0) {
		OTP_MSG(("Unprogrammed otp\n"));
	} else {
		OTP_MSG(("Found some chunks, skipping to 0x%x\n", base * 2));
	}
	if ((wlen + 3) > (lim - base)) {
		err =  BCME_NORESOURCE;
		goto out;
	}

#if defined(BCMDBG) || defined(WLTEST)
	st_n = st_s = st_hwm = pp_hwm = 0;
#endif /* BCMDBG || WLTEST */

	/* Prepare the header and crc */
	hdr[0] = OTP_MAGIC;
	hdr[1] = (wlen + 3) * 2;
	crc = hndcrc16((uint8 *)hdr, sizeof(hdr), CRC16_INIT_VALUE);
	crc = hndcrc16((uint8 *)data, wlen * 2, crc);
	crc = ~crc;

	do {
		p = data;
		wn = base + 2;
		lim = base + wlen + 2;

		OTP_MSG(("writing chunk, 0x%x bytes @ 0x%x-0x%x\n", wlen * 2,
		         base * 2, (lim + 1) * 2));

		/* Write the header */
		err = hndotp_write_word(oh, cc, base, hdr[0]);

		/* Write the data */
		while (wn < lim) {
			err += hndotp_write_word(oh, cc, wn++, *p++);

			/* If there has been an error, close this chunk */
			if (err != 0) {
				OTP_MSG(("closing early @ 0x%x\n", wn * 2));
				break;
			}
		}

		/* If we wrote the whole chunk, write the crc */
		if (wn == lim) {
			OTP_MSG(("  whole chunk written, crc = 0x%x\n", crc));
			err += hndotp_write_word(oh, cc, wn++, crc);
			clen = hdr[1];
		} else {
			/* If there was an error adjust the count to point to
			 * the word after the error so we can start the next
			 * chunk there.
			 */
			clen = (wn - base) * 2;
			OTP_MSG(("  partial chunk written, chunk len = 0x%x\n", clen));
		}
		/* And now write the chunk length */
		err += hndotp_write_word(oh, cc, base + 1, clen);

		if (base == 0) {
			/* Write the signature and boundary if this is the HW region,
			 * but don't report failure if either of these 2 writes fail.
			 */
			if (hndotp_write_word(oh, cc, (oi->size / 2) + OTP_BOUNDARY_OFF,
			    wn * 2) == 0)
				gerr += hndotp_write_word(oh, cc, (oi->size / 2) + OTP_HWSIGN_OFF,
				                       OTP_SIGNATURE);
			else
				gerr++;
			oi->boundary = wn * 2;
			oi->signvalid |= OTP_HW_REGION;
		}

		if (err != 0) {
			gerr += err;
			/* Errors, do it all over again if there is space left */
			if ((wlen + 3) <= ((oi->size / 2) + OTP_SWLIM_OFF - wn)) {
				base = wn;
				lim = base + wlen + 2;
				OTP_ERR(("Programming errors, retry @ 0x%x\n", wn * 2));
			} else {
				OTP_ERR(("Programming errors, no space left ( 0x%x)\n", wn * 2));
				break;
			}
		}
	} while (err != 0);

	OTP_MSG(("bits written: %d, average (%d/%d): %d, max retry: %d, pp max: %d\n",
	       st_n, st_s, st_n, st_s / st_n, st_hwm, pp_hwm));

	if (gerr != 0)
		OTP_MSG(("programming %s after %d errors\n", (err == 0) ? "succedded" : "failed",
		         gerr));
out:
	/* done */
	si_setcoreidx(oi->sih, idx);

	if (err)
		return BCME_ERROR;
	else
		return 0;
}
#endif /* BCMNVRAMW */
#endif /* !defined(BCMDONGLEHOST) */

#if defined(WLTEST) && !defined(BCMROMBUILD)
static uint16
BCMNMIATTACHFN(hndotp_otprb16)(void *oh, chipcregs_t *cc, uint wn)
{
	uint base, i;
	uint16 val, bit;

	base = (wn * 16) + (wn / 4);
	val = 0;
	for (i = 0; i < 16; i++) {
		if ((bit = hndotp_read_bit(oh, cc, base + i)) == 0xffff)
			break;
		val = val | (bit << i);
	}
	if (i < 16)
		val = 0xaaaa;
	return val;
}

static int
BCMNMIATTACHFN(hndotp_dump)(void *oh, int arg, char *buf, uint size)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	chipcregs_t *cc;
	uint idx, i, count, lil;
	uint16 val;
	struct bcmstrbuf b;

	idx = si_coreidx(oi->sih);
	cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	if (arg >= 16)
		arg -= 16;

	if (arg == 2) {
		count = 66 * 4;
		lil = 3;
	} else {
		count = (oi->size / 2) + OTP_RC_LIM_OFF;
		lil = 7;
	}

	OTP_MSG(("%s: arg %d, size %d, words %d\n", __FUNCTION__, arg, size, count));
	bcm_binit(&b, buf, size);
	for (i = 0; i < count; i++) {
		if ((i & lil) == 0)
			bcm_bprintf(&b, "0x%04x:", 2 * i);

		if (arg == 0)
			val = hndotp_otpr(oh, cc, i);
		else
			val = hndotp_otprb16(oi, cc, i);
		bcm_bprintf(&b, " 0x%04x", val);
		if ((i & lil) == lil) {
			if (arg == 2) {
				bcm_bprintf(&b, " %d\n",
				            hndotp_read_bit(oh, cc, ((i / 4) * 65) + 64) & 1);
			} else {
				bcm_bprintf(&b, "\n");
			}
		}
	}
	if ((i & lil) != lil)
		bcm_bprintf(&b, "\n");

	OTP_MSG(("%s: returning %d, left %d, wn %d\n",
		__FUNCTION__, (int)(b.buf - b.origbuf), b.size, i));

	si_setcoreidx(oi->sih, idx);

	return ((int)(b.buf - b.origbuf));
}
#endif	

static otp_fn_t hndotp_fn = {
	(otp_size_t)hndotp_size,
	(otp_read_bit_t)hndotp_read_bit,
	(otp_dump_t)NULL,		/* Assigned in otp_init */
	(otp_status_t)hndotp_status,

#if !defined(BCMDONGLEHOST)
	(otp_init_t)hndotp_init,
	(otp_read_region_t)hndotp_read_region,
	(otp_nvread_t)hndotp_nvread,
#ifdef BCMNVRAMW
	(otp_write_region_t)hndotp_write_region,
	(otp_cis_append_region_t)hndotp_cis_append_region,
	(otp_lock_t)hndotp_lock,
	(otp_nvwrite_t)hndotp_nvwrite,
	(otp_write_word_t)NULL,
#else /* BCMNVRAMW */
	(otp_write_region_t)NULL,
	(otp_cis_append_region_t)NULL,
	(otp_lock_t)NULL,
	(otp_nvwrite_t)NULL,
	(otp_write_word_t)NULL,
#endif /* BCMNVRAMW */
	(otp_read_word_t)hndotp_read_word,
#endif /* !defined(BCMDONGLEHOST) */
#if !defined(BCMDONGLEHOST)
#ifdef BCMNVRAMW
	(otp_write_bits_t)hndotp_write_bits
#else
	(otp_write_bits_t)NULL
#endif /* BCMNVRAMW */
#endif /* !BCMDONGLEHOST */
};

#endif /* BCMHNDOTP */

/*
 * Common Code: Compiled for IPX / HND / AUTO
 *	otp_status()
 *	otp_size()
 *	otp_read_bit()
 *	otp_init()
 * 	otp_read_region()
 * 	otp_read_word()
 * 	otp_nvread()
 * 	otp_write_region()
 * 	otp_write_word()
 * 	otp_cis_append_region()
 * 	otp_lock()
 * 	otp_nvwrite()
 * 	otp_dump()
 */

int
BCMNMIATTACHFN(otp_status)(void *oh)
{
	otpinfo_t *oi = (otpinfo_t *)oh;

	return oi->fn->status(oh);
}

int
BCMNMIATTACHFN(otp_size)(void *oh)
{
	otpinfo_t *oi = (otpinfo_t *)oh;

	return oi->fn->size(oh);
}

uint16
BCMNMIATTACHFN(otp_read_bit)(void *oh, uint offset)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	uint idx = si_coreidx(oi->sih);
	chipcregs_t *cc = si_setcoreidx(oi->sih, SI_CC_IDX);
	uint16 readBit = (uint16)oi->fn->read_bit(oh, cc, offset);
	si_setcoreidx(oi->sih, idx);
	return readBit;
}

#if !defined(BCMDONGLEHOST) && defined(BCMNVRAMW)
int
BCMNMIATTACHFN(otp_write_bits)(void *oh, uint offset, int bits, uint8* data)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	return oi->fn->write_bits(oh, offset, bits, data);
}
#endif /* BCMNVRAMW && !BCMDONGLEHOST */

void *
BCMNMIATTACHFN(otp_init)(si_t *sih)
{
	otpinfo_t *oi;
	void *ret = NULL;
#if !defined(BCMDONGLEHOST)
	uint32 min_res_mask = 0;
	bool wasup = FALSE;
#endif

	oi = get_otpinfo();
	bzero(oi, sizeof(otpinfo_t));

	oi->ccrev = sih->ccrev;

#ifdef BCMIPXOTP
	if (OTPTYPE_IPX(oi->ccrev)) {
#if defined(WLTEST) && !defined(BCMROMBUILD)
		/* Dump function is excluded from ROM */
		get_ipxotp_fn()->dump = ipxotp_dump;
#endif
		oi->fn = get_ipxotp_fn();
	}
#endif /* BCMIPXOTP */

#ifdef BCMHNDOTP
	if (OTPTYPE_HND(oi->ccrev)) {
#if defined(WLTEST) && !defined(BCMROMBUILD)
		/* Dump function is excluded from ROM */
		hndotp_fn.dump = hndotp_dump;
#endif
		oi->fn = &hndotp_fn;
	}
#endif /* BCMHNDOTP */

	if (oi->fn == NULL) {
		OTP_ERR(("otp_init: unsupported OTP type\n"));
		return NULL;
	}

	oi->sih = sih;
	oi->osh = si_osh(oi->sih);

#if !defined(BCMDONGLEHOST)
	if (!(wasup = si_is_otp_powered(sih)))
		si_otp_power(sih, TRUE, &min_res_mask);

	ret = (oi->fn->init)(sih);

	if (!wasup)
		si_otp_power(sih, FALSE, &min_res_mask);
#endif

	return ret;
}

#if !defined(BCMDONGLEHOST)
int
BCMNMIATTACHFN(otp_newcis)(void *oh)
{
	int ret = 0;
#if defined(BCMIPXOTP)
	otpinfo_t *oi = (otpinfo_t *)oh;
	int otpgu_bit_base = oi->otpgu_base * 16;

	ret = otp_read_bit(oh, otpgu_bit_base + OTPGU_NEWCISFORMAT_OFF);
	OTP_MSG(("New Cis format bit %d value: %x\n", otpgu_bit_base + OTPGU_NEWCISFORMAT_OFF,
		ret));
#endif

	return ret;
}

int
BCMNMIATTACHFN(otp_read_region)(si_t *sih, int region, uint16 *data, uint *wlen)
{
	bool wasup = FALSE;
	void *oh;
	int err = 0;
	uint32 min_res_mask = 0;

	if (!(wasup = si_is_otp_powered(sih)))
		si_otp_power(sih, TRUE, &min_res_mask);

	if (!si_is_otp_powered(sih) || si_is_otp_disabled(sih)) {
		err = BCME_NOTREADY;
		goto out;
	}

	oh = otp_init(sih);
	if (oh == NULL) {
		OTP_ERR(("otp_init failed.\n"));
		err = BCME_ERROR;
		goto out;
	}

	err = (((otpinfo_t*)oh)->fn->read_region)(oh, region, data, wlen);

out:
	if (!wasup)
		si_otp_power(sih, FALSE, &min_res_mask);

	return err;
}

int
BCMNMIATTACHFN(otp_read_word)(si_t *sih, uint wn, uint16 *data)
{
	bool wasup = FALSE;
	void *oh;
	int err = 0;
	uint32 min_res_mask = 0;

	if (!(wasup = si_is_otp_powered(sih)))
		si_otp_power(sih, TRUE, &min_res_mask);

	if (!si_is_otp_powered(sih) || si_is_otp_disabled(sih)) {
		err = BCME_NOTREADY;
		goto out;
	}

	oh = otp_init(sih);
	if (oh == NULL) {
		OTP_ERR(("otp_init failed.\n"));
		err = BCME_ERROR;
		goto out;
	}

	if (((otpinfo_t*)oh)->fn->read_word == NULL) {
		err = BCME_UNSUPPORTED;
		goto out;
	}
	err = (((otpinfo_t*)oh)->fn->read_word)(oh, wn, data);

out:
	if (!wasup)
		si_otp_power(sih, FALSE, &min_res_mask);

	return err;
}

int
BCMNMIATTACHFN(otp_nvread)(void *oh, char *data, uint *len)
{
	otpinfo_t *oi = (otpinfo_t *)oh;

	return oi->fn->nvread(oh, data, len);
}

#ifdef BCMNVRAMW
int
BCMNMIATTACHFN(otp_write_region)(si_t *sih, int region, uint16 *data, uint wlen, uint flags)
{
	bool wasup = FALSE;
	void *oh;
	int err = 0;
	uint32 min_res_mask = 0;

	if (!(wasup = si_is_otp_powered(sih)))
		si_otp_power(sih, TRUE, &min_res_mask);

	if (!si_is_otp_powered(sih) || si_is_otp_disabled(sih)) {
		err = BCME_NOTREADY;
		goto out;
	}

	oh = otp_init(sih);
	if (oh == NULL) {
		OTP_ERR(("otp_init failed.\n"));
		err = BCME_ERROR;
		goto out;
	}

	err = (((otpinfo_t*)oh)->fn->write_region)(oh, region, data, wlen, flags);

out:
	if (!wasup)
		si_otp_power(sih, FALSE, &min_res_mask);

	return err;
}

int
BCMNMIATTACHFN(otp_write_word)(si_t *sih, uint wn, uint16 data)
{
	bool wasup = FALSE;
	void *oh;
	int err = 0;
	uint32 min_res_mask = 0;

	if (!(wasup = si_is_otp_powered(sih)))
		si_otp_power(sih, TRUE, &min_res_mask);

	if (!si_is_otp_powered(sih) || si_is_otp_disabled(sih)) {
		err = BCME_NOTREADY;
		goto out;
	}

	oh = otp_init(sih);
	if (oh == NULL) {
		OTP_ERR(("otp_init failed.\n"));
		err = BCME_ERROR;
		goto out;
	}

	if (((otpinfo_t*)oh)->fn->write_word == NULL) {
		err = BCME_UNSUPPORTED;
		goto out;
	}
	err = (((otpinfo_t*)oh)->fn->write_word)(oh, wn, data);

out:
	if (!wasup)
		si_otp_power(sih, FALSE, &min_res_mask);

	return err;
}

int
BCMNMIATTACHFN(otp_cis_append_region)(si_t *sih, int region, char *vars, int count)
{
	void *oh = otp_init(sih);

	if (oh == NULL) {
		OTP_ERR(("otp_init failed.\n"));
		return -1;
	}
	return (((otpinfo_t*)oh)->fn->cis_append_region)(sih, region, vars, count);
}

int
BCMNMIATTACHFN(otp_lock)(si_t *sih)
{
	bool wasup = FALSE;
	void *oh;
	int ret = 0;
	uint32 min_res_mask = 0;

	if (!(wasup = si_is_otp_powered(sih)))
		si_otp_power(sih, TRUE, &min_res_mask);

	if (!si_is_otp_powered(sih) || si_is_otp_disabled(sih)) {
		ret = BCME_NOTREADY;
		goto out;
	}

	oh = otp_init(sih);
	if (oh == NULL) {
		OTP_ERR(("otp_init failed.\n"));
		ret = BCME_ERROR;
		goto out;
	}

	ret = (((otpinfo_t*)oh)->fn->lock)(oh);

out:
	if (!wasup)
		si_otp_power(sih, FALSE, &min_res_mask);

	return ret;
}

int
BCMNMIATTACHFN(otp_nvwrite)(void *oh, uint16 *data, uint wlen)
{
	otpinfo_t *oi = (otpinfo_t *)oh;

	return oi->fn->nvwrite(oh, data, wlen);
}
#endif /* BCMNVRAMW */

#if defined(WLTEST)
int
BCMNMIATTACHFN(otp_dump)(void *oh, int arg, char *buf, uint size)
{
	otpinfo_t *oi = (otpinfo_t *)oh;

	if (oi->fn->dump == NULL)
		return BCME_UNSUPPORTED;
	else
		return oi->fn->dump(oh, arg, buf, size);
}

int
BCMNMIATTACHFN(otp_dumpstats)(void *oh, int arg, char *buf, uint size)
{
	otpinfo_t *oi = (otpinfo_t *)oh;
	struct bcmstrbuf b;

	bcm_binit(&b, buf, size);

	bcm_bprintf(&b, "\nOTP, ccrev 0x%04x\n", oi->ccrev);
#if defined(BCMIPXOTP)
	bcm_bprintf(&b, "wsize %d rows %d cols %d\n", oi->wsize, oi->rows, oi->cols);
	bcm_bprintf(&b, "hwbase %d hwlim %d swbase %d swlim %d fbase %d flim %d fusebits %d\n",
		oi->hwbase, oi->hwlim, oi->swbase, oi->swlim, oi->fbase, oi->flim, oi->fusebits);
	bcm_bprintf(&b, "otpgu_base %d status %d\n", oi->otpgu_base, oi->status);
#endif
#if defined(BCMHNDOTP)
	bcm_bprintf(&b, "OLD OTP, size %d hwprot 0x%x signvalid 0x%x boundary %d\n",
		oi->size, oi->hwprot, oi->signvalid, oi->boundary);
#endif
	bcm_bprintf(&b, "\n");

	return 200;	/* real buf length, pick one to cover above print */
}

#endif	

#endif /* !defined(BCMDONGLEHOST) */
