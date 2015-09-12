/*
 * Routines to access SPROM and to parse SROM/CIS variables.
 *
 * Despite its file name, OTP contents is also parsed in this file.
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
 * $Id: bcmsrom.c 525691 2015-01-12 08:54:01Z $
 */

/*
 * List of non obvious preprocessor defines used in this file and their meaning:
 * DONGLEBUILD    : building firmware that runs on the dongle's CPU
 * BCM_DONGLEVARS : NVRAM variables can be read from OTP/S(P)ROM.
 * When host may supply nvram vars in addition to the ones in OTP/SROM:
 * 	BCMHOSTVARS    		: full nic / full dongle
 * 	BCM_BMAC_VARS_APPEND	: BMAC
 * BCMDONGLEHOST  : defined when building DHD, code executes on the host in a dongle environment.
 * DHD_SPROM      : defined when building a DHD that supports reading/writing to SPROM
 */

#include <bcm_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <machine/stdarg.h>
#else
#include <stdarg.h>
#endif
#include <bcmutils.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <bcmdevs.h>
#include <bcmendian.h>
#include <sbpcmcia.h>
#include <pcicfg.h>
#include <siutils.h>
#include <bcmsrom.h>
#include <bcmsrom_tbl.h>
#ifdef BCMSPI
#include <spid.h>
#endif

#include <bcmnvram.h>
#include <bcmotp.h>
#ifndef BCMUSBDEV_COMPOSITE
#define BCMUSBDEV_COMPOSITE
#endif
#if defined(BCMUSBDEV)
#include <sbsdio.h>
#include <sbhnddma.h>
#include <sbsdpcmdev.h>
#endif

#if defined(WLTEST) || defined(DHD_SPROM) || defined(BCMDBG)
#include <sbsprom.h>
#endif
#include <proto/ethernet.h>	/* for sprom content groking */
#ifdef BCM_OL_DEV
#include <bcm_ol_msg.h>
#endif


#if defined(BCMDBG_ERR) || defined(WLTEST)
#define	BS_ERROR(args)	printf args
#else
#define	BS_ERROR(args)
#endif

/** curmap: contains host start address of PCI BAR0 window */
static uint8* srom_offset(si_t *sih, void *curmap)
{
	if (sih->ccrev <= 31)
		return (uint8 *)curmap + PCI_BAR0_SPROM_OFFSET;
	if ((sih->cccaps & CC_CAP_SROM) == 0)
		return NULL;
	if (BUSTYPE(sih->bustype) == SI_BUS)
		return (uint8 *)(SI_ENUM_BASE + CC_SROM_OTP);
	return (uint8 *)curmap + PCI_16KB0_CCREGS_OFFSET + CC_SROM_OTP;
}

#if defined(WLTEST) || defined(DHD_SPROM) || defined(BCMDBG)
#define WRITE_ENABLE_DELAY	500	/* 500 ms after write enable/disable toggle */
#define WRITE_WORD_DELAY	20	/* 20 ms between each word write */
#endif

typedef struct varbuf {
	char *base;		/* pointer to buffer base */
	char *buf;		/* pointer to current position */
	unsigned int size;	/* current (residual) size in bytes */
} varbuf_t;
extern char *_vars;
extern uint _varsz;
#ifdef DONGLEBUILD
char * BCMATTACHDATA(_vars_otp) = NULL;
#define DONGLE_STORE_VARS_OTP_PTR(v)	(_vars_otp = (v))
#else
#define DONGLE_STORE_VARS_OTP_PTR(v)
#endif

#define SROM_CIS_SINGLE	1


#if !defined(BCMDONGLEHOST)
static int initvars_srom_si(si_t *sih, osl_t *osh, void *curmap, char **vars, uint *count);
#if defined(BCM_OL_DEV)
static int initvars_tcm_pcidev(si_t *sih, osl_t *osh, void *curmap, char **vars, uint *count);
#endif /* BCM_OL_DEV */
static void _initvars_srom_pci(uint8 sromrev, uint16 *srom, uint off, varbuf_t *b);
static int initvars_srom_pci(si_t *sih, void *curmap, char **vars, uint *count);
static int initvars_cis_pci(si_t *sih, osl_t *osh, void *curmap, char **vars, uint *count);
static int initvars_cis_pcmcia(si_t *sih, osl_t *osh, char **vars, uint *count);
#endif /* !defined(BCMDONGLEHOST) */
#if !defined(BCMUSBDEV_ENABLED) && !defined(BCMSDIODEV_ENABLED) && \
	!defined(BCMDONGLEHOST) && !defined(BCM_OL_DEV) && !defined(BCMPCIEDEV_ENABLED)
static int initvars_flash_si(si_t *sih, char **vars, uint *count);
#endif 
#if !defined(BCMDONGLEHOST)
#ifdef BCMSPI
static int initvars_cis_spi(osl_t *osh, char **vars, uint *count);
#endif /* BCMSPI */
#endif /* !defined(BCMDONGLEHOST) */
static int sprom_cmd_pcmcia(osl_t *osh, uint8 cmd);
static int sprom_read_pcmcia(osl_t *osh, uint16 addr, uint16 *data);
#if defined(WLTEST) || defined(DHD_SPROM) || defined(BCMDBG)
static int sprom_write_pcmcia(osl_t *osh, uint16 addr, uint16 data);
#endif 
static int sprom_read_pci(osl_t *osh, si_t *sih, uint16 *sprom, uint wordoff, uint16 *buf,
                          uint nwords, bool check_crc);
#if !defined(BCMDONGLEHOST)
#if defined(BCMNVRAMW) || defined(BCMNVRAMR)
static int otp_read_pci(osl_t *osh, si_t *sih, uint16 *buf, uint bufsz);
#endif /* defined(BCMNVRAMW) || defined(BCMNVRAMR) */
#endif /* !defined(BCMDONGLEHOST) */
static uint16 srom_cc_cmd(si_t *sih, osl_t *osh, void *ccregs, uint32 cmd, uint wordoff,
                          uint16 data);

#if !defined(BCMDONGLEHOST)
static int initvars_table(osl_t *osh, char *start, char *end, char **vars, uint *count);
static int initvars_flash(si_t *sih, osl_t *osh, char **vp, uint len);
int dbushost_initvars_flash(si_t *sih, osl_t *osh, char **base, uint len);
#endif /* !defined(BCMDONGLEHOST) */

#if defined(BCMUSBDEV)
static int get_si_pcmcia_srom(si_t *sih, osl_t *osh, uint8 *pcmregs,
                              uint boff, uint16 *srom, uint bsz, bool check_crc);
#if defined(WLTEST) || defined(DHD_SPROM) || defined(BCMDBG)
static int set_si_pcmcia_srom(si_t *sih, osl_t *osh, uint8 *pcmregs,
                              uint boff, uint16 *srom, uint bsz);
#endif 
#endif 

#if defined(BCMUSBDEV)
#if defined(BCMUSBDEV_BMAC) || defined(BCM_BMAC_VARS_APPEND)
/* default to bcm94323 P200, other boards should have OTP programmed */
static char BCMATTACHDATA(defaultsromvars_4322usb)[] =
	"vendid=0x14e4\0"
	"subvendid=0x0a5c\0"
	"subdevid=0xbdc\0"
	"macaddr=00:90:4c:d3:04:73\0"
	"sromrev=8\0"
	"devid=0x432b\0"
	"boardrev=0x1200\0"
	"boardflags=0xa00\0"
	"boardflags2=0x602\0"
	"boardtype=0x04a8\0"
	"tssipos2g=0x1\0"
	"extpagain2g=0x0\0"
	"pdetrange2g=0x0\0"
	"triso2g=0x3\0"
	"antswctl2g=0x2\0"
	"tssipos5g=0x1\0"
	"extpagain5g=0x0\0"
	"pdetrange5g=0x0\0"
	"triso5g=0x3\0"
	"antswctl5g=0x2\0"
	"maxp2ga0=0x48\0"
	"itt2ga0=0x20\0"
	"pa2gw0a0=0xFEA8\0"
	"pa2gw1a0=0x16CD\0"
	"pa2gw2a0=0xFAA5\0"
	"maxp5ga0=0x40\0"
	"itt5ga0=0x3e\0"
	"maxp5gha0=0x3c\0"
	"maxp5gla0=0x40\0"
	"pa5gw0a0=0xFEB2\0"
	"pa5gw1a0=0x1471\0"
	"pa5gw2a0=0xFB1F\0"
	"pa5glw0a0=0xFEA2\0"
	"pa5glw1a0=0x149A\0"
	"pa5glw2a0=0xFAFC\0"
	"pa5ghw0a0=0xFEC6\0"
	"pa5ghw1a0=0x13DD\0"
	"pa5ghw2a0=0xFB48\0"
	"maxp2ga1=0x48\0"
	"itt2ga1=0x20\0"
	"pa2gw0a1=0xFEA3\0"
	"pa2gw1a1=0x1687\0"
	"pa2gw2a1=0xFAAA\0"
	"maxp5ga1=0x40\0"
	"itt5ga1=0x3e\0"
	"maxp5gha1=0x3c\0"
	"maxp5gla1=0x40\0"
	"pa5gw0a1=0xFEBC\0"
	"pa5gw1a1=0x14F9\0"
	"pa5gw2a1=0xFB05\0"
	"pa5glw0a1=0xFEBE\0"
	"pa5glw1a1=0x1478\0"
	"pa5glw2a1=0xFB1A\0"
	"pa5ghw0a1=0xFEE1\0"
	"pa5ghw1a1=0x14FD\0"
	"pa5ghw2a1=0xFB38\0"
	"cctl=0\0"
	"ccode=US\0"
	"regrev=0x0\0"
	"ledbh0=0xff\0"
	"ledbh1=0x2\0"
	"ledbh2=0x3\0"
	"ledbh3=0xff\0"
	"leddc=0xa0a0\0"
	"aa2g=0x3\0"
	"aa5g=0x3\0"
	"ag0=0x2\0"
	"ag1=0x2\0"
	"ag2=0xff\0"
	"ag3=0xff\0"
	"txchain=0x3\0"
	"rxchain=0x3\0"
	"antswitch=0\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_43234usb)[] =
	"vendid=0x14e4\0"
	"subvendid=0x0a5c\0"
	"subdevid=0xbdc\0"
	"macaddr=00:90:4c:03:21:23\0"
	"cctl=0\0"
	"ccode=US\0"
	"regrev=0x0\0"
	"ledbh0=0x82\0"
	"ledbh1=0xff\0"
	"ledbh2=0xff\0"
	"ledbh3=0xff\0"
	"leddc=0x0\0"
	"aa2g=0x2\0"
	"aa5g=0x2\0"
	"ag0=0x2\0"
	"ag1=0x2\0"
	"ag2=0x2\0"
	"ag3=0xff\0"
	"txchain=0x2\0"
	"rxchain=0x2\0"
	"antswitch=0\0"
	"sromrev=8\0"
	"devid=0x4346\0"
	"boardrev=0x1403\0"
	"boardflags=0x200\0"
	"boardflags2=0x2000\0"
	"boardtype=0x0521\0"
	"tssipos2g=0x1\0"
	"extpagain2g=0x2\0"
	"pdetrange2g=0x2\0"
	"triso2g=0x3\0"
	"antswctl2g=0x0\0"
	"tssipos5g=0x1\0"
	"extpagain5g=0x2\0"
	"pdetrange5g=0x2\0"
	"triso5g=0x3\0"
	"antswctl5g=0x0\0"
	"ofdm2gpo=0x0\0"
	"ofdm5gpo=0x0\0"
	"ofdm5glpo=0x0\0"
	"ofdm5ghpo=0x0\0"
	"mcs2gpo0=0x0\0"
	"mcs2gpo1=0x0\0"
	"mcs2gpo2=0x0\0"
	"mcs2gpo3=0x0\0"
	"mcs2gpo4=0x4444\0"
	"mcs2gpo5=0x4444\0"
	"mcs2gpo6=0x4444\0"
	"mcs2gpo7=0x4444\0"
	"mcs5gpo4=0x2222\0"
	"mcs5gpo5=0x2222\0"
	"mcs5gpo6=0x2222\0"
	"mcs5gpo7=0x2222\0"
	"mcs5glpo4=0x2222\0"
	"mcs5glpo5=0x2222\0"
	"mcs5glpo6=0x2222\0"
	"mcs5glpo7=0x2222\0"
	"mcs5ghpo4=0x2222\0"
	"mcs5ghpo5=0x2222\0"
	"mcs5ghpo6=0x2222\0"
	"mcs5ghpo7=0x2222\0"
	"maxp2ga0=0x42\0"
	"itt2ga0=0x20\0"
	"itt5ga0=0x3e\0"
	"pa2gw0a0=0xFF21\0"
	"pa2gw1a0=0x13B7\0"
	"pa2gw2a0=0xFB44\0"
	"maxp5ga0=0x3E\0"
	"maxp5gha0=0x3a\0"
	"maxp5gla0=0x3c\0"
	"pa5gw0a0=0xFEB2\0"
	"pa5gw1a0=0x1570\0"
	"pa5gw2a0=0xFAD6\0"
	"pa5glw0a0=0xFE64\0"
	"pa5glw1a0=0x13F7\0"
	"pa5glw2a0=0xFAF6\0"
	"pa5ghw0a0=0xFEAB\0"
	"pa5ghw1a0=0x15BB\0"
	"pa5ghw2a0=0xFAC6\0"
	"maxp2ga1=0x42\0"
	"itt2ga1=0x20\0"
	"itt5ga1=0x3e\0"
	"pa2gw0a1=0xFF17\0"
	"pa2gw1a1=0x13C4\0"
	"pa2gw2a1=0xFB3C\0"
	"maxp5ga1=0x3E\0"
	"maxp5gha1=0x3a\0"
	"maxp5gla1=0x3c\0"
	"pa5gw0a1=0xFE6F\0"
	"pa5gw1a1=0x13CC\0"
	"pa5gw2a1=0xFAF8\0"
	"pa5glw0a1=0xFE87\0"
	"pa5glw1a1=0x14BE\0"
	"pa5glw2a1=0xFAD6\0"
	"pa5ghw0a1=0xFE68\0"
	"pa5ghw1a1=0x13E9\0"
	"pa5ghw2a1=0xFAF6\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_43235usb)[] =
	"vendid=0x14e4\0"
	"subvendid=0x0a5c\0"
	"subdevid=0xbdc\0"
	"macaddr=00:90:4c:05:30:01\0"
	"ccode=US\0"
	"regrev=0x0\0"
	"ledbh0=0x82\0"
	"ledbh1=0xff\0"
	"ledbh2=0xff\0"
	"ledbh3=0xff\0"
	"leddc=0x0\0"
	"aa2g=0x3\0"
	"ag0=0x2\0"
	"ag1=0x2\0"
	"ag2=0xff\0"
	"ag3=0xff\0"
	"txchain=0x3\0"
	"rxchain=0x3\0"
	"antswitch=0\0"
	"sromrev=8\0"
	"devid=0x4347\0"
	"boardrev=0x1113\0"
	"boardflags=0x200\0"
	"boardflags2=0x0\0"
	"boardtype=0x0571\0"
	"tssipos2g=0x1\0"
	"extpagain2g=0x2\0"
	"pdetrange2g=0x2\0"
	"triso2g=0x3\0"
	"antswctl2g=0x0\0"
	"antswctl5g=0x0\0"
	"ofdm2gpo=0x0\0"
	"mcs2gpo0=0x0\0"
	"mcs2gpo1=0x0\0"
	"mcs2gpo2=0x0\0"
	"mcs2gpo3=0x0\0"
	"mcs2gpo4=0x2222\0"
	"mcs2gpo5=0x2222\0"
	"mcs2gpo6=0x2222\0"
	"mcs2gpo7=0x4444\0"
	"maxp2ga0=0x42\0"
	"itt2ga0=0x20\0"
	"pa2gw0a0=0xFF00\0"
	"pa2gw1a0=0x143C\0"
	"pa2gw2a0=0xFB27\0"
	"maxp2ga1=0x42\0"
	"itt2ga1=0x20\0"
	"pa2gw0a1=0xFF22\0"
	"pa2gw1a1=0x142E\0"
	"pa2gw2a1=0xFB45\0"
	"tempthresh=120\0"
	"temps_period=5\0"
	"temp_hysteresis=5\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_4350usb)[] =
	"sromrev=11\0"
	"boardrev=0x1305\0"
	"boardtype=0x695\0"
	"boardflags=0x10401001\0"
	"boardflags2=0x802000\0"
	"boardflags3=0x00000088\0"
	"macaddr=00:90:4c:13:c0:01\0"
	"ccode=0\0"
	"regrev=0\0"
	"cctl=0x0\0"
	"antswitch=0\0"
	"pdgain5g=0\0"
	"pdgain2g=1\0"
	"tworangetssi2g=0\0"
	"tworangetssi5g=0\0"
	"femctrl=10\0"
	"vendid=0x14e4\0"
	"devid=0x43b7\0"
	"xtalfreq=40000\0"
	"rxgains2gelnagaina0=3\0"
	"rxgains2gtrisoa0=5\0"
	"rxgains2gtrelnabypa0=1\0"
	"rxgains5gelnagaina0=3\0"
	"rxgains5gtrisoa0=5\0"
	"rxgains5gtrelnabypa0=1\0"
	"rxgains5gmelnagaina0=3\0"
	"rxgains5gmtrisoa0=5\0"
	"rxgains5gmtrelnabypa0=1\0"
	"rxgains5ghelnagaina0=3\0"
	"rxgains5ghtrisoa0=5\0"
	"rxgains5ghtrelnabypa0=1\0"
	"rxgains2gelnagaina1=3\0"
	"rxgains2gtrisoa1=5\0"
	"rxgains2gtrelnabypa1=1\0"
	"rxgains5gelnagaina1=3\0"
	"rxgains5gtrisoa1=5\0"
	"rxgains5gtrelnabypa1=1\0"
	"rxgains5gmelnagaina1=3\0"
	"rxgains5gmtrisoa1=5\0"
	"rxgains5gmtrelnabypa1=1\0"
	"rxgains5ghelnagaina1=3\0"
	"rxgains5ghtrisoa1=5\0"
	"rxgains5ghtrelnabypa1=1\0"
	"rxchain=3\0"
	"txchain=3\0"
	"aa2g=3\0"
	"aa5g=3\0"
	"agbg0=2\0"
	"agbg1=2\0"
	"aga0=2\0"
	"aga1=2\0"
	"tssiposslope2g=1\0"
	"epagain2g=0\0"
	"papdcap2g=0\0"
	"tssiposslope5g=1\0"
	"epagain5g=0\0"
	"papdcap5g=0\0"
	"gainctrlsph=0\0"
	"tempthresh=255\0"
	"tempoffset=255\0"
	"rawtempsense=0x1ff\0"
	"measpower=0x7f\0"
	"tempsense_slope=0xff\0"
	"tempcorrx=0x3f\0"
	"tempsense_option=0x3\0"
	"pa2ga0=0xff4c,0x1666,0xfd38\0"
	"pa2ga1=0xff4c,0x16c9,0xfd2b\0"
	"pa5ga0=0xfffb,0x1786,0xfdc9,0x0017,0x183b,0xfdd6,0x0054,0x184b,0xfdfd,0x00fb,"
	"0x195b,0xfe90\0"
	"pa5ga1=0x0016,0x1873,0xfdd1,0x002f,0x18d4,0xfdde,0x0065,0x1877,0xfe18,0x00d6,"
	"0x1959,0xfe5b\0"
	"pa5gbw4080a0=0x0064,0x1a0d,0xfdc8,0x0080,0x1ac2,0xfdd2,0x00cd,0x1bae,0xfde6,0x00cb,"
	"0x1b31,0xfdb0\0"
	"pa5gbw4080a1=0x006c,0x1b07,0xfdaa,0x0074,0x1aac,0xfdcb,0x0056,0x1a5b,0xfd6b,0x006b,"
	"0x1a53,0xfd4e\0"
	"maxp2ga0=80\0"
	"maxp5ga0=76,76,76,76\0"
	"maxp2ga1=80\0"
	"maxp5ga1=76,76,76,76\0"
	"subband5gver=0x4\0"
	"paparambwver=2\0"
	"pdoffset2g40mvalid=0\0"
	"pdoffset2g40ma0=1\0"
	"pdoffset2g40ma1=1\0"
	"pdoffset2g40ma2=0\0"
	"pdoffset40ma0=0x0000\0"
	"pdoffset80ma0=0x0000\0"
	"pdoffset40ma1=0x0000\0"
	"pdoffset80ma1=0x0000\0"
	"pdoffset40ma2=0x0000\0"
	"pdoffset80ma2=0x0000\0"
	"cckbw202gpo=0\0"
	"cckbw20ul2gpo=0\0"
	"mcsbw202gpo=0x88642000\0"
	"mcsbw402gpo=0xA8642000\0"
	"dot11agofdmhrbw202gpo=0x2000\0"
	"ofdmlrbw202gpo=0x0020\0"
	"mcsbw205glpo=0xaa864200\0"
	"mcsbw405glpo=0xcca86420\0"
	"mcsbw805glpo=0xcca86420\0"
	"mcsbw1605glpo=0\0"
	"mcsbw205gmpo=0xaa864200\0"
	"mcsbw405gmpo=0xcca86420\0"
	"mcsbw805gmpo=0xcca86420\0"
	"mcsbw1605gmpo=0\0"
	"mcsbw205ghpo=0xaa864200\0"
	"mcsbw405ghpo=0xcca86420\0"
	"mcsbw805ghpo=0xcca86420\0"
	"mcsbw1605ghpo=0\0"
	"mcslr5glpo=0x0\0"
	"mcslr5gmpo=0x0000\0"
	"mcslr5ghpo=0x0000\0"
	"sb20in40hrpo=0x0\0"
	"sb20in80and160hr5glpo=0x0\0"
	"sb40and80hr5glpo=0x0\0"
	"sb20in80and160hr5gmpo=0x0\0"
	"sb40and80hr5gmpo=0x0\0"
	"sb20in80and160hr5ghpo=0x0\0"
	"sb40and80hr5ghpo=0x0\0"
	"sb20in40lrpo=0x0\0"
	"sb20in80and160lr5glpo=0x0\0"
	"sb40and80lr5glpo=0x0\0"
	"sb20in80and160lr5gmpo=0x0\0"
	"sb40and80lr5gmpo=0x0\0"
	"sb20in80and160lr5ghpo=0x0\0"
	"sb40and80lr5ghpo=0x0\0"
	"dot11agduphrpo=0x0\0"
	"dot11agduplrpo=0x0\0"
	"phycal_tempdelta=255\0"
	"temps_period=15\0"
	"temps_hysteresis=15\0"
	"measpower1=0x7f\0"
	"measpower2=0x7f\0"
	"muxenab=0x1\0"
	"aga2=0x0\0"
	"agbg2=0x0\0"
	"mcsbw20ul2gpo=0x0\0"
	"mcsbw20ul5ghpo=0x0\0"
	"mcsbw20ul5glpo=0x0\0"
	"mcsbw20ul5gmpo=0x0\0"
	"pdoffset2g40ma2=0x0\0"
	"pdoffset40ma2=0x0\0"
	"pdoffset80ma2=0x0\0"
	"ledbh0=0xff\0"
	"ledbh1=0xff\0"
	"ledbh2=0xff\0"
	"ledbh3=0xff\0"
	"ledbh4=0xff\0"
	"ledbh5=0xff\0"
	"ledbh6=0xff\0"
	"ledbh7=0xff\0"
	"ledbh8=0xff\0"
	"ledbh9=0xff\0"
	"ledbh10=0xff\0"
	"ledbh11=0xff\0"
	"ledbh12=0xff\0"
	"ledbh13=130\0"
	"ledbh14=131\0"
	"ledbh15=0xff\0"
	"leddc=0x0\0"
	"wpsgpio=15\0"
	"wpsled=1\0"
	"tx_duty_cycle_ofdm_40_5g=0\0"
	"tx_duty_cycle_thresh_40_5g=0\0"
	"tx_duty_cycle_ofdm_80_5g=61\0"
	"tx_duty_cycle_thresh_80_5g=468\0"
	"otpimagesize=484\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_43242usb)[] =
	"devid=0x4374\0"
	"boardtype=0x063A\0"
	"boardrev=0x1100\0"
	"boardflags=0x200\0"
	"boardflags2=0\0"
	"macaddr=00:90:4c:c5:12:38\0"
	"sromrev=9\0"
	"xtalfreq=37400\0"
	"nocrc=1\0"
	"ag0=0x2\0"
	"ag1=0x2\0"
	"ag2=0xff\0"
	"ag3=0xff\0"
	"txchain=0x3\0"
	"rxchain=0x3\0"
	"aa2g=3\0"
	"aa5g=3\0"
	"ccode=ALL\0"
	"regrev=0\0"
	"ledbh0=0xff\0"
	"ledbh1=0xff\0"
	"ledbh2=0xff\0"
	"ledbh3=0xff\0"
	"leddc=0xffff\0"
	"pa2gw0a0=0xFFC3\0"
	"pa2gw1a0=0x143F\0"
	"pa2gw2a0=0xFEE1\0"
	"pa2gw0a1=0xFFBA\0"
	"pa2gw1a1=0x141E\0"
	"pa2gw2a1=0xFEDB\0"
	"maxp2ga0=70\0"
	"maxp2ga1=70\0"
	"maxp5ga0=70\0"
	"maxp5ga1=70\0"
	"maxp5gha0=70\0"
	"maxp5gha1=70\0"
	"maxp5gla0=70\0"
	"maxp5gla1=70\0"
	"pa0itssit=62\0"
	"pa1itssit=62\0"
	"antswctl2g=0x15\0"
	"antswctl5g=0x16\0"
	"antswitch=0x0\0"
	"pa5gw0a0=0xFFC5\0"
	"pa5gw1a0=0x1321\0"
	"pa5gw2a0=0xFEF2\0"
	"pa5gw0a1=0xFFBC\0"
	"pa5gw1a1=0x1386\0"
	"pa5gw2a1=0xFEE3\0"
	"pa5glw0a0=0xFFC4\0"
	"pa5glw1a0=0x12CF\0"
	"pa5glw2a0=0xFEF4\0"
	"pa5glw0a1=0xFFC5\0"
	"pa5glw1a1=0x1391\0"
	"pa5glw2a1=0xFEED\0"
	"pa5ghw0a0=0xFFC4\0"
	"pa5ghw1a0=0x12CF\0"
	"pa5ghw2a0=0xFEF4\0"
	"pa5ghw0a1=0xFFC5\0"
	"pa5ghw1a1=0x1391\0"
	"pa5ghw2a1=0xFEED\0"
	"extpagain2g=2\0"
	"extpagain5g=2\0"
	"pdetrange2g=2\0"
	"pdetrange5g=2\0"
	"triso2g=2\0"
	"triso5g=4\0"
	"tssipos2g=1\0"
	"tssipos5g=1\0"
	"cckbw202gpo=0x0\0"
	"cckbw20ul2gpo=0x0\0"
	"legofdmbw202gpo=0x88888888\0"
	"legofdmbw20ul2gpo=0x88888888\0"
	"mcsbw202gpo=0x88888888\0"
	"mcsbw20ul2gpo=0x88888888\0"
	"mcsbw402gpo=0x88888888\0"
	"mcs32po=0x5555\0"
	"leg40dup2gpo=0x2\0"
	"legofdmbw205glpo=0x22220000\0"
	"legofdmbw20ul5glpo=0x44442222\0"
	"legofdmbw205gmpo=0x22220000\0"
	"legofdmbw20ul5gmpo=0x44442222\0"
	"legofdmbw205ghpo=0x22220000\0"
	"legofdmbw20ul5ghpo=0x44442222\0"
	"mcsbw205glpo=0x44422222\0"
	"mcsbw20ul5glpo=0x66644444\0"
	"mcsbw405glpo=0x66644444\0"
	"mcsbw205gmpo=0x44422222\0"
	"mcsbw20ul5gmpo=0x66644444\0"
	"mcsbw405gmpo=0x66644444\0"
	"mcsbw205ghpo=0x44422222\0"
	"mcsbw20ul5ghpo=0x66644444\0"
	"mcsbw405ghpo=0x66644444\0"
	"itt2ga0=0x20\0"
	"itt5ga0=0x3e\0"
	"itt2ga1=0x20\0"
	"itt5ga1=0x3e\0"
	"tempthresh=120\0"
	"noisecaloffset=10\0"
	"noisecaloffset5g=12\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_43143usb)[] =
	"vendid=0x14e4\0"
	"devid=0x4366\0"
	"subvendid=0xa5c\0"
	"subdevid=0xbdc\0"
	"sromrev=10\0"
	"boardnum=0x1100\0"
	"boardtype=0x0629\0"
	"boardrev=0x1403\0"
	"boardflags=0x000\0"
	"boardflags2=0x000\0"
	"macaddr=00:90:4c:0e:81:23\0"
	"ccode=ALL\0"
	"cctl=0\0"
	"regrev=0\0"
	"ledbh0=0xff\0"
	"ledbh1=0xff\0"
	"ledbh2=0xff\0"
	"ledbh3=0xff\0"
	"leddc=0xffff\0"
	"aa2g=1\0"
	"ag0=2\0"
	"ag1=2\0"
	"txchain=1\0"
	"rxchain=1\0"
	"antswitch=0\0"
	"maxp2ga0=68\0"
	"pa0itssit=0x20\0"
	"pa0b0=6022\0"
	"pa0b1=-709\0"
	"pa0b2=-147\0"
	"cckPwrOffset=3\0"
	"tssipos2g=0\0"
	"extpagain2g=0\0"
	"pdetrange2g=0\0"
	"triso2g=3\0"
	"antswctl2g=0\0"
	"cckbw202gpo=0x0000\0"
	"legofdmbw202gpo=0x43333333\0"
	"mcsbw202gpo=0x63333333\0"
	"mcsbw402gpo=0x66666666\0"
	"swctrlmap_2g=0x00000000,0x00000000,0x00000000,0x00000000,0x000\0"
	"xtalfreq=20000\0"
	"otpimagesize=154\0"
	"tempthresh=120\0"
	"temps_period=5\0"
	"temp_hysteresis=5\0"
	"rssismf2g=0x8\0"
	"rssismc2g=0x8\0"
	"rssisav2g=0x2\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_43236usb)[] =
	"vendid=0x14e4\0"
	"subvendid=0x0a5c\0"
	"subdevid=0xbdc\0"
	"macaddr=00:90:4c:03:21:23\0"
	"cctl=0\0"
	"ccode=US\0"
	"regrev=0x0\0"
	"ledbh0=0x82\0"
	"ledbh1=0xff\0"
	"ledbh2=0xff\0"
	"ledbh3=0xff\0"
	"leddc=0x0\0"
	"aa2g=0x3\0"
	"aa5g=0x3\0"
	"ag0=0x2\0"
	"ag1=0x2\0"
	"ag2=0x2\0"
	"ag3=0xff\0"
	"txchain=0x3\0"
	"rxchain=0x3\0"
	"antswitch=0\0"
	"sromrev=8\0"
	"devid=0x4346\0"
	"boardrev=0x1532\0"
	"boardflags=0x200\0"
	"boardflags2=0x2000\0"
	"boardtype=0x0521\0"
	"tssipos2g=0x1\0"
	"extpagain2g=0x2\0"
	"pdetrange2g=0x2\0"
	"triso2g=0x3\0"
	"antswctl2g=0x0\0"
	"tssipos5g=0x1\0"
	"extpagain5g=0x2\0"
	"pdetrange5g=0x2\0"
	"triso5g=0x3\0"
	"antswctl5g=0x0\0"
	"ofdm2gpo=0x33333333\0"
	"ofdm5gpo=0x0\0"
	"ofdm5glpo=0x0\0"
	"ofdm5ghpo=0x0\0"
	"mcs2gpo0=0x3333\0"
	"mcs2gpo1=0x3333\0"
	"mcs2gpo2=0x3333\0"
	"mcs2gpo3=0x3333\0"
	"mcs2gpo4=0x5555\0"
	"mcs2gpo5=0x5555\0"
	"mcs2gpo6=0x5555\0"
	"mcs2gpo7=0x5555\0"
	"mcs5gpo4=0x2222\0"
	"mcs5gpo5=0x2222\0"
	"mcs5gpo6=0x2222\0"
	"mcs5gpo7=0x2222\0"
	"mcs5glpo4=0x2222\0"
	"mcs5glpo5=0x2222\0"
	"mcs5glpo6=0x2222\0"
	"mcs5glpo7=0x2222\0"
	"mcs5ghpo4=0x2222\0"
	"mcs5ghpo5=0x2222\0"
	"mcs5ghpo6=0x2222\0"
	"mcs5ghpo7=0x2222\0"
	"maxp2ga0=0x48\0"
	"itt2ga0=0x20\0"
	"itt5ga0=0x3e\0"
	"pa2gw0a0=0xFFD8\0"
	"pa2gw1a0=0x171C\0"
	"pa2gw2a0=0xFB14\0"
	"maxp5ga0=0x3e\0"
	"maxp5gha0=0x3a\0"
	"maxp5gla0=0x3c\0"
	"pa5gw0a0=0xFE88\0"
	"pa5gw1a0=0x141C\0"
	"pa5gw2a0=0xFB17\0"
	"pa5glw0a0=0xFE8C\0"
	"pa5glw1a0=0x1493\0"
	"pa5glw2a0=0xFAFC\0"
	"pa5ghw0a0=0xFE86\0"
	"pa5ghw1a0=0x13CC\0"
	"pa5ghw2a0=0xFB20\0"
	"maxp2ga1=0x48\0"
	"itt2ga1=0x20\0"
	"itt5ga1=0x3e\0"
	"pa2gw0a1=0x0020\0"
	"pa2gw1a1=0x1791\0"
	"pa2gw2a1=0xFB5F\0"
	"maxp5ga1=0x3e\0"
	"maxp5gha1=0x3a\0"
	"maxp5gla1=0x3c\0"
	"pa5gw0a1=0xFE7E\0"
	"pa5gw1a1=0x1399\0"
	"pa5gw2a1=0xFB27\0"
	"pa5glw0a1=0xFE82\0"
	"pa5glw1a1=0x13F3\0"
	"pa5glw2a1=0xFB14\0"
	"pa5ghw0a1=0xFE96\0"
	"pa5ghw1a1=0x13BF\0"
	"pa5ghw2a1=0xFB30\0"
	"tempthresh=120\0"
	"temps_period=5\0"
	"temp_hysteresis=5\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_4319usb)[] =
	"sromrev=3\0"
	"vendid=0x14e4\0"
	"devid=0x4338\0"
	"boardtype=0x4e7\0"
	"boardrev=0x1508\0"
	"boardflags=0x200\0"
	"xtalfreq=30000\0"
	"aa2g=3\0"
	"aa5g=0\0"
	"ag0=255\0"
	"opo=0\0"
	"pa0b0=5756\0"
	"pa0b1=64121\0"
	"pa0b2=65153\0"
	"pa0itssit=62\0"
	"pa0maxpwr=76\0"
	"rssismf2g=0xa\0"
	"rssismc2g=0xb\0"
	"rssisav2g=0x3\0"
	"bxa2g=0\0"
	"tri2g=78\0"
	"cckdigfilttype=6\0"
	"rxpo2g=2\0"
	"cckpo=0\0"
	"ofdmpo=0x44441111\0"
	"mcs2gpo0=0xaaaa\0"
	"mcs2gpo1=0xaaaa\0"
	"boardnum=1\0"
	"macaddr=00:90:4c:16:${maclo}\0"
	"otpimagesize=182\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_4360usb)[] =
	"sromrev=11\0"
	"boardtype=0x623\0"
	"venid=0x14e4\0"
	"boardvendor=0x14e4\0"
	"devid=0x43a0\0"
	"boardrev=0x1200\0"
	"boardflags=0x10001000\0"
	"boardflags2=0x0\0"
	"boardflags3=0x0\0"
	"macaddr=00:90:4c:0e:60:11\0"
	"ccode=0\0"
	"regrev=0\0"
	"ledbh0=0xff\0"
	"ledbh1=0xff\0"
	"ledbh2=0xff\0"
	"ledbh3=0xff\0"
	"leddc=0xffff\0"
	"aa2g=0x3\0"
	"aa5g=0x3\0"
	"agbg0=0x2\0"
	"agbg1=0x2\0"
	"agbg2=0xff\0"
	"aga0=0x2\0"
	"aga1=0x2\0"
	"aga2=0xff\0"
	"txchain=0x3\0"
	"rxchain=0x3\0"
	"antswitch=0\0"
	"tssiposslope2g=1\0"
	"epagain2g=0\0"
	"pdgain2g=7\0"
	"tworangetssi2g=0\0"
	"papdcap2g=0\0"
	"femctrl=1\0"
	"tssiposslope5g=1\0"
	"epagain5g=0\0"
	"pdgain5g=7\0"
	"tworangetssi5g=0\0"
	"papdcap5g=0\0"
	"gainctrlsph=0\0"
	"tempthresh=0xff\0"
	"tempoffset=0xff\0"
	"rawtempsense=0x1ff\0"
	"measpower=0x7f\0"
	"tempsense_slope=0xff\0"
	"tempcorrx=0x3f\0"
	"tempsense_option=0x3\0"
	"phycal_tempdelta=255\0"
	"temps_period=15\0"
	"temps_hysteresis=15\0"
	"measpower1=0x7f\0"
	"measpower2=0x7f\0"
	"subband5gver=0x4\0"
	"muxenab=0x01\0"
	"pcieingress_war=15\0"
	"sar2g=18\0"
	"sar5g=15\0"
	"noiselvl2ga0=31\0"
	"noiselvl2ga1=31\0"
	"noiselvl5ga0=31,31,31,31\0"
	"noiselvl5ga1=31,31,31,31\0"
	"rxgainerr2ga0=31\0"
	"rxgainerr2ga1=31\0"
	"rxgainerr5ga0=31,31,31,31\0"
	"rxgainerr5ga1=31,31,31,31\0"
	"maxp2ga0=76\0"
	"pa2ga0=0xff34,0x19d6,0xfccf\0"
	"rxgains2gelnagaina0=4\0"
	"rxgains2gtrisoa0=9\0"
	"rxgains2gtrelnabypa0=1\0"
	"rxgains5gelnagaina0=3\0"
	"rxgains5gtrisoa0=9\0"
	"rxgains5gtrelnabypa0=1\0"
	"maxp5ga0=72,72,76,76\0"
"pa5ga0=0xff2b,0x1652,0xfd2b,0xff2f,0x167a,0xfd29,0xff29,0x1615,0xfd2e,0xff2b,0x15c9,0xfd3a\0"
	"maxp2ga1=76\0"
	"pa2ga1=0xff27,0x1895,0xfced\0"
	"rxgains2gelnagaina1=4\0"
	"rxgains2gtrisoa1=9\0"
	"rxgains2gtrelnabypa1=1\0"
	"rxgains5gelnagaina1=3\0"
	"rxgains5gtrisoa1=9\0"
	"rxgains5gtrelnabypa1=1\0"
	"maxp5ga1=72,72,72,72\0"
"pa5ga1=0xff27,0x171b,0xfd14,0xff2d,0x1741,0xfd0f,0xff33,0x1705,0xfd1a,0xff32,0x1666,0xfd2c\0"
	"END\0";

#endif  /* BCMUSBDEV_BMAC || BCM_BMAC_VARS_APPEND */
#endif  /* BCMUSBDEV */


/* BCMHOSTVARS is enabled only if WLTEST is enabled or BCMEXTNVM is enabled */
#if defined(BCMHOSTVARS)
/* Also used by wl_readconfigdata for vars download */
char BCMATTACHDATA(mfgsromvars)[VARS_MAX];
int BCMATTACHDATA(defvarslen) = 0;
#endif 

#if !defined(BCMDONGLEHOST)
/* BCMHOSTVARS is enabled only if WLTEST is enabled or BCMEXTNVM is enabled */
#if defined(BCMHOSTVARS)
static char BCMATTACHDATA(defaultsromvars_4331)[] =
	"sromrev=9\0"
	"boardrev=0x1104\0"
	"boardflags=0x200\0"
	"boardflags2=0x0\0"
	"boardtype=0x524\0"
	"boardvendor=0x14e4\0"
	"boardnum=0x2064\0"
	"macaddr=00:90:4c:1a:20:64\0"
	"ccode=0x0\0"
	"regrev=0x0\0"
	"ledbh0=0xff\0"
	"ledbh1=0xff\0"
	"ledbh2=0xff\0"
	"ledbh3=0xff\0"
	"leddc=0xffff\0"
	"opo=0x0\0"
	"aa2g=0x7\0"
	"aa5g=0x7\0"
	"ag0=0x2\0"
	"ag1=0x2\0"
	"ag2=0x2\0"
	"ag3=0xff\0"
	"pa0b0=0xfe7f\0"
	"pa0b1=0x15d9\0"
	"pa0b2=0xfac6\0"
	"pa0itssit=0x20\0"
	"pa0maxpwr=0x48\0"
	"pa1b0=0xfe89\0"
	"pa1b1=0x14b1\0"
	"pa1b2=0xfada\0"
	"pa1lob0=0xffff\0"
	"pa1lob1=0xffff\0"
	"pa1lob2=0xffff\0"
	"pa1hib0=0xfe8f\0"
	"pa1hib1=0x13df\0"
	"pa1hib2=0xfafa\0"
	"pa1itssit=0x3e\0"
	"pa1maxpwr=0x3c\0"
	"pa1lomaxpwr=0x3c\0"
	"pa1himaxpwr=0x3c\0"
	"bxa2g=0x3\0"
	"rssisav2g=0x7\0"
	"rssismc2g=0xf\0"
	"rssismf2g=0xf\0"
	"bxa5g=0x3\0"
	"rssisav5g=0x7\0"
	"rssismc5g=0xf\0"
	"rssismf5g=0xf\0"
	"tri2g=0xff\0"
	"tri5g=0xff\0"
	"tri5gl=0xff\0"
	"tri5gh=0xff\0"
	"rxpo2g=0xff\0"
	"rxpo5g=0xff\0"
	"txchain=0x7\0"
	"rxchain=0x7\0"
	"antswitch=0x0\0"
	"tssipos2g=0x1\0"
	"extpagain2g=0x2\0"
	"pdetrange2g=0x4\0"
	"triso2g=0x3\0"
	"antswctl2g=0x0\0"
	"tssipos5g=0x1\0"
	"elna2g=0xff\0"
	"extpagain5g=0x2\0"
	"pdetrange5g=0x4\0"
	"triso5g=0x3\0"
	"antswctl5g=0x0\0"
	"elna5g=0xff\0"
	"cckbw202gpo=0x0\0"
	"cckbw20ul2gpo=0x0\0"
	"legofdmbw202gpo=0x0\0"
	"legofdmbw20ul2gpo=0x0\0"
	"legofdmbw205glpo=0x0\0"
	"legofdmbw20ul5glpo=0x0\0"
	"legofdmbw205gmpo=0x0\0"
	"legofdmbw20ul5gmpo=0x0\0"
	"legofdmbw205ghpo=0x0\0"
	"legofdmbw20ul5ghpo=0x0\0"
	"mcsbw202gpo=0x0\0"
	"mcsbw20ul2gpo=0x0\0"
	"mcsbw402gpo=0x0\0"
	"mcsbw205glpo=0x0\0"
	"mcsbw20ul5glpo=0x0\0"
	"mcsbw405glpo=0x0\0"
	"mcsbw205gmpo=0x0\0"
	"mcsbw20ul5gmpo=0x0\0"
	"mcsbw405gmpo=0x0\0"
	"mcsbw205ghpo=0x0\0"
	"mcsbw20ul5ghpo=0x0\0"
	"mcsbw405ghpo=0x0\0"
	"mcs32po=0x0\0"
	"legofdm40duppo=0x0\0"
	"maxp2ga0=0x48\0"
	"itt2ga0=0x20\0"
	"itt5ga0=0x3e\0"
	"pa2gw0a0=0xfe7f\0"
	"pa2gw1a0=0x15d9\0"
	"pa2gw2a0=0xfac6\0"
	"maxp5ga0=0x3c\0"
	"maxp5gha0=0x3c\0"
	"maxp5gla0=0x3c\0"
	"pa5gw0a0=0xfe89\0"
	"pa5gw1a0=0x14b1\0"
	"pa5gw2a0=0xfada\0"
	"pa5glw0a0=0xffff\0"
	"pa5glw1a0=0xffff\0"
	"pa5glw2a0=0xffff\0"
	"pa5ghw0a0=0xfe8f\0"
	"pa5ghw1a0=0x13df\0"
	"pa5ghw2a0=0xfafa\0"
	"maxp2ga1=0x48\0"
	"itt2ga1=0x20\0"
	"itt5ga1=0x3e\0"
	"pa2gw0a1=0xfe54\0"
	"pa2gw1a1=0x1563\0"
	"pa2gw2a1=0xfa7f\0"
	"maxp5ga1=0x3c\0"
	"maxp5gha1=0x3c\0"
	"maxp5gla1=0x3c\0"
	"pa5gw0a1=0xfe53\0"
	"pa5gw1a1=0x14fe\0"
	"pa5gw2a1=0xfa94\0"
	"pa5glw0a1=0xffff\0"
	"pa5glw1a1=0xffff\0"
	"pa5glw2a1=0xffff\0"
	"pa5ghw0a1=0xfe6e\0"
	"pa5ghw1a1=0x1457\0"
	"pa5ghw2a1=0xfab9\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_4335)[] =
	"sromrev=11\0"
	"boardrev=0x1104\0"
	"boardtype=0x0647\0"
	"boardflags=0x10401001\0"
	"boardflags2=0x0\0"
	"boardflags3=0x0\0"
	"macaddr=00:90:4c:c5:43:55\0"
	"ccode=0\0"
	"regrev=0\0"
	"antswitch=0\0"
	"tworangetssi2g=0\0"
	"tworangetssi5g=0\0"
	"femctrl=4\0"
	"pcieingress_war=15\0"
	"vendid=0x14e4\0"
	"devid=0x43ae\0"
	"manfid=0x2d0\0"
	"#prodid=0x052e\0"
	"nocrc=1\0"
	"xtalfreq=40000\0"
	"extpagain2g=1\0"
	"pdetrange2g=2\0"
	"extpagain5g=1\0"
	"pdetrange5g=2\0"
	"rxgains2gelnagaina0=3\0"
	"rxgains2gtrisoa0=3\0"
	"rxgains2gtrelnabypa0=1\0"
	"rxgains5gelnagaina0=3\0"
	"rxgains5gtrisoa0=4\0"
	"rxgains5gtrelnabypa0=1\0"
	"pdgain5g=10\0"
	"pdgain2g=10\0"
	"rxchain=1\0"
	"txchain=1\0"
	"aa2g=1\0"
	"aa5g=1\0"
	"tssipos5g=1\0"
	"tssipos2g=1\0"
	"pa2ga0=0x0,0x0,0x0\0"
	"pa5ga0=-217,5493,-673\0"
	"tssifloor2g=0x3ff\0"
	"tssifloor5g=0x3ff,0x3ff,0x3ff,0x3ff\0"
	"pdoffset40ma0=0\0"
	"pdoffset80ma0=0\0"
	"END\0";

#endif 
#endif /* !defined(BCMDONGLEHOST) */

#if !defined(BCMDONGLEHOST)
/* BCMHOSTVARS is enabled only if WLTEST is enabled or BCMEXTNVM is enabled */
#if defined(BCMHOSTVARS)
static char BCMATTACHDATA(defaultsromvars_wltest)[] =
	"macaddr=00:90:4c:f8:00:01\0"
	"et0macaddr=00:11:22:33:44:52\0"
	"et0phyaddr=30\0"
	"et0mdcport=0\0"
	"gpio2=robo_reset\0"
	"boardvendor=0x14e4\0"
	"boardflags=0x210\0"
	"boardflags2=0\0"
	"boardtype=0x04c3\0"
	"boardrev=0x1100\0"
	"sromrev=8\0"
	"devid=0x432c\0"
	"ccode=0\0"
	"regrev=0\0"
	"ledbh0=255\0"
	"ledbh1=255\0"
	"ledbh2=255\0"
	"ledbh3=255\0"
	"leddc=0xffff\0"
	"aa2g=3\0"
	"ag0=2\0"
	"ag1=2\0"
	"aa5g=3\0"
	"aa0=2\0"
	"aa1=2\0"
	"txchain=3\0"
	"rxchain=3\0"
	"antswitch=0\0"
	"itt2ga0=0x20\0"
	"maxp2ga0=0x48\0"
	"pa2gw0a0=0xfe9e\0"
	"pa2gw1a0=0x15d5\0"
	"pa2gw2a0=0xfae9\0"
	"itt2ga1=0x20\0"
	"maxp2ga1=0x48\0"
	"pa2gw0a1=0xfeb3\0"
	"pa2gw1a1=0x15c9\0"
	"pa2gw2a1=0xfaf7\0"
	"tssipos2g=1\0"
	"extpagain2g=0\0"
	"pdetrange2g=0\0"
	"triso2g=3\0"
	"antswctl2g=0\0"
	"tssipos5g=1\0"
	"extpagain5g=0\0"
	"pdetrange5g=0\0"
	"triso5g=3\0"
	"antswctl5g=0\0"
	"cck2gpo=0\0"
	"ofdm2gpo=0\0"
	"mcs2gpo0=0\0"
	"mcs2gpo1=0\0"
	"mcs2gpo2=0\0"
	"mcs2gpo3=0\0"
	"mcs2gpo4=0\0"
	"mcs2gpo5=0\0"
	"mcs2gpo6=0\0"
	"mcs2gpo7=0\0"
	"cddpo=0\0"
	"stbcpo=0\0"
	"bw40po=4\0"
	"bwduppo=0\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_4345)[] =
	"sromrev=11\0"
	"vendid=0x14e4\0"
	"devid=0x43ab\0"
	"manfid=0x2d0\0"
	"prodid=0x052e\0"
	"macaddr=00:90:4c:c5:12:38\0"
	"nocrc=1\0"
	"boardtype=0x687\0"
	"boardrev=0x1104\0"
	"xtalfreq=37400\0"
	"boardflags=0x0\0"
	"boardflags2=0x0\0"
	"boardflags3=0x0\0"
	"rxgains2gelnagaina0=0\0"
	"rxgains2gtrisoa0=0\0"
	"rxgains2gtrelnabypa0=0\0"
	"rxgains5gelnagaina0=0\0"
	"rxgains5gtrisoa0=0\0"
	"rxgains5gtrelnabypa0=0\0"
	"pdgain5g=0\0"
	"pdgain2g=0\0"
	"rxchain=1\0"
	"txchain=1\0"
	"aa2g=1\0"
	"aa5g=1\0"
	"tssipos5g=1\0"
	"tssipos2g=1\0"
	"femctrl=0\0"
	"pa2ga0=-228,3230,-453\0"
	"pa5ga0=-218,4514,-665,-162,4750,-642,-130,4995,-640,-80,5144,-607\0"
	"pdoffset40ma0=0\0"
	"pdoffset80ma0=0\0"
	"extpagain5g=3\0"
	"extpagain2g=3\0"
	"maxp2ga0=0x48\0"
	"itt2ga0=0x20\0"
	"itt5ga0=0x3e\0"
	"pa2gw0a0=0xfe7f\0"
	"pa2gw1a0=0x15d9\0"
	"pa2gw2a0=0xfac6\0"
	"maxp5ga0=0x3c\0"
	"maxp5gha0=0x3c\0"
	"maxp5gla0=0x3c\0"
	"cckbw202gpo=0\0"
	"cckbw20ul2gpo=0\0"
	"mcsbw202gpo=2571386880\0"
	"mcsbw402gpo=2571386880\0"
	"swctrlmap_2g=0x00000004,0x00000002,0x00000002,0x000000,0x000\0"
	"swctrlmap_5g=0x00000016,0x00000032,0x00000032,0x000000,0x000\0"
	"swctrlmapext_5g=0x00000000,0x00000000,0x00000000,0x000000,0x000\0"
	"swctrlmapext_2g=0x00000000,0x00000000,0x00000000,0x000000,0x000\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_4345b0)[] =
	"sromrev=11\0"
	"vendid=0x14e4\0"
	"devid=0x43ab\0"
	"manfid=0x2d0\0"
	"prodid=0x069b\0"
	"macaddr=00:90:4c:c5:12:38\0"
	"nocrc=1\0"
	"boardtype=0x69b\0"
	"boardrev=0x1120\0"
	"xtalfreq=37400\0"
	"boardflags=0x10001001\0"
	"boardflags2=0x0\0"
	"boardflags3=0x00000100\0"
	"rxgains2gelnagaina0=1\0"
	"rxgains2gtrisoa0=4\0"
	"rxgains2gtrelnabypa0=0\0"
	"rxgains5gelnagaina0=3\0"
	"rxgains5gtrisoa0=5\0"
	"rxgains5gtrelnabypa0=0\0"
	"pdgain5g=2\0"
	"pdgain2g=2\0"
	"rxchain=1\0"
	"txchain=1\0"
	"aa2g=1\0"
	"aa5g=1\0"
	"tssipos5g=1\0"
	"tssipos2g=1\0"
	"femctrl=0\0"
	"pa2ga0=-202,6212,-784\0"
	"pa5ga0=-104,6044,-676,-96,6104,-676,-82,6122,-674,-60,6190,-672\0"
	"pdoffset40ma0=0\0"
	"pdoffset80ma0=0\0"
	"extpagain5g=3\0"
	"extpagain2g=3\0"
	"maxp2ga0=0x42\0"
	"maxp5ga0=0x42,0x42,0x42,0x42\0"
	"mcsbw205glpo=0x43100000\0"
	"mcsbw205gmpo=0x43100000\0"
	"mcsbw205ghpo=0x43100000\0"
	"mcsbw405glpo=0x43100000\0"
	"mcsbw405gmpo=0x43100000\0"
	"mcsbw405ghpo=0x43100000\0"
	"mcsbw805glpo=0x43100000\0"
	"mcsbw805gmpo=0x43100000\0"
	"mcsbw805ghpo=0x43100000\0"
	"swctrlmap_2g=0x00000004,0x000a0000,0x00020000,0x010a02,0x1ff\0"
	"swctrlmap_5g=0x00000010,0x00600000,0x00200000,0x010a02,0x2f4\0"
	"swctrlmapext_5g=0x00000000,0x00000000,0x00000000,0x000000,0x000\0"
	"swctrlmapext_2g=0x00000000,0x00000000,0x00000000,0x000000,0x000\0"
	"muxenab=1\0";

static char BCMATTACHDATA(defaultsromvars_4350)[] =
	"sromrev=11\0"
	"boardrev=0x1250\0"
	"boardflags=0x02400001\0"
	"boardflags2=0x00800000\0"
	"boardtype=0x68e\0"
	"subvid=0x14e4\0"
	"boardflags3=0xc\0"
	"boardnum=1\0"
	"macaddr=00:90:4c:13:80:01\0"
	"ccode=X0\0"
	"regrev=0\0"
	"aa2g=3\0"
	"aa5g=3\0"
	"agbg0=2\0"
	"agbg1=2\0"
	"aga0=2\0"
	"aga1=2\0"
	"rxchain=3\0"
	"txchain=3\0"
	"antswitch=0\0"
	"tssiposslope2g=1\0"
	"extpagain2g=2\0"
	"epagain2g=2\0"
	"pdgain2g=2\0"
	"tworangetssi2g=0\0"
	"papdcap2g=0\0"
	"femctrl=10\0"
	"tssiposslope5g=1\0"
	"epagain5g=2\0"
	"pdgain5g=2\0"
	"tworangetssi5g=0\0"
	"papdcap5g=0\0"
	"gainctrlsph=0\0"
	"tempthresh=255\0"
	"tempoffset=255\0"
	"rawtempsense=0x1ff\0"
	"measpower=0x7f\0"
	"tempsense_slope=0xff\0"
	"tempcorrx=0x3f\0"
	"tempsense_option=0x3\0"
	"xtalfreq=40000\0"
	"phycal_tempdelta=255\0"
	"temps_period=15\0"
	"temps_hysteresis=15\0"
	"measpower1=0x7f\0"
	"measpower2=0x7f\0"
	"pdoffset2g40ma0=0\0"
	"pdoffset2g40ma1=0\0"
	"pdoffset2g40mvalid=0\0"
	"pdoffset40ma0=0\0"
	"pdoffset40ma1=0\0"
	"pdoffset80ma0=0\0"
	"pdoffset80ma1=0\0"
	"subband5gver=0x4\0"
	"cckbw202gpo=0\0"
	"cckbw20ul2gpo=0\0"
	"mcsbw202gpo=0\0"
	"mcsbw402gpo=0\0"
	"dot11agofdmhrbw202gpo=0\0"
	"ofdmlrbw202gpo=0\0"
	"mcsbw205glpo=0\0"
	"mcsbw405glpo=0\0"
	"mcsbw805glpo=0\0"
	"mcsbw1605glpo=0\0"
	"mcsbw205gmpo=0\0"
	"mcsbw405gmpo=0\0"
	"mcsbw805gmpo=0\0"
	"mcsbw1605gmpo=0\0"
	"mcsbw205ghpo=0\0"
	"mcsbw405ghpo=0\0"
	"mcsbw805ghpo=0\0"
	"mcsbw1605ghpo=0\0"
	"mcslr5glpo=0\0"
	"mcslr5gmpo=0\0"
	"mcslr5ghpo=0\0"
	"sb20in40hrpo=0\0"
	"sb20in80and160hr5glpo=0\0"
	"sb40and80hr5glpo=0\0"
	"sb20in80and160hr5gmpo=0\0"
	"sb40and80hr5gmpo=0\0"
	"sb20in80and160hr5ghpo=0\0"
	"sb40and80hr5ghpo=0\0"
	"sb20in40lrpo=0\0"
	"sb20in80and160lr5glpo=0\0"
	"sb40and80lr5glpo=0\0"
	"sb20in80and160lr5gmpo=0\0"
	"sb40and80lr5gmpo=0\0"
	"sb20in80and160lr5ghpo=0\0"
	"sb40and80lr5ghpo=0\0"
	"dot11agduphrpo=0\0"
	"dot11agduplrpo=0\0"
	"pcieingress_war=15\0"
	"sar2g=18\0"
	"sar5g=15\0"
	"noiselvl2ga0=31\0"
	"noiselvl2ga1=31\0"
	"noiselvl2ga2=31\0"
	"noiselvl5ga0=31,31,31,31\0"
	"noiselvl5ga1=31,31,31,31\0"
	"noiselvl5ga2=31,31,31,31\0"
	"rxgainerr2ga0=63\0"
	"rxgainerr2ga1=31\0"
	"rxgainerr2ga2=31\0"
	"rxgainerr5ga0=63,63,63,63\0"
	"rxgainerr5ga1=31,31,31,31\0"
	"rxgainerr5ga2=31,31,31,31\0"
	"maxp2ga0=80\0"
	"pa2ga0=0xff63,0x15b0,0xfd7b\0"
	"rxgains5gmelnagaina0=0\0"
	"rxgains5gmtrisoa0=4\0"
	"rxgains5gmtrelnabypa0=0\0"
	"rxgains5ghelnagaina0=0\0"
	"rxgains5ghtrisoa0=4\0"
	"rxgains5ghtrelnabypa0=0\0"
	"rxgains2gelnagaina0=4\0"
	"rxgains2gtrisoa0=3\0"
	"rxgains2gtrelnabypa0=0\0"
	"rxgains5gelnagaina0=0\0"
	"rxgains5gtrisoa0=4\0"
	"rxgains5gtrelnabypa0=0\0"
	"maxp5ga0=76,76,76,76\0"
"pa5ga0=0xff3a,0x14d4,0xfd5f,0xff36,0x1626,0xfd2e,0xff42,0x15bd,0xfd47,0xff39,0x15a3,0xfd3d\0"
	"maxp2ga1=80\0"
	"pa2ga1=0xff6c,0x15f6,0xfd7c\0"
	"rxgains5gmelnagaina1=0\0"
	"rxgains5gmtrisoa1=4\0"
	"rxgains5gmtrelnabypa1=0\0"
	"rxgains5ghelnagaina1=0\0"
	"rxgains5ghtrisoa1=4\0"
	"rxgains5ghtrelnabypa1=0\0"
	"rxgains2gelnagaina1=0\0"
	"rxgains2gtrisoa1=3\0"
	"rxgains2gtrelnabypa1=0\0"
	"rxgains5gelnagaina1=0\0"
	"rxgains5gtrisoa1=4\0"
	"rxgains5gtrelnabypa1=0\0"
	"maxp5ga1=76,76,76,76\0"
"pa5ga1=0xff4e,0x1530,0xfd53,0xff58,0x15b4,0xfd4d,0xff58,0x1671,0xfd2f,0xff55,0x15e2,0xfd46\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_4350c0)[] =
	"sromrev=11\0"
	"boardrev=0x1130\0"
	"boardtype=0x06da\0"
	"boardflags=0x12401001\0"
	"boardflags2=0x00800000\0"
	"boardflags3=0x109\0"
	"macaddr=00:90:4c:16:60:01\0"
	"ccode=0\0"
	"regrev=0\0"
	"antswitch=0\0"
	"pdgain5g=0\0"
	"pdgain2g=0\0"
	"tworangetssi2g=0\0"
	"tworangetssi5g=0\0"
	"femctrl=10\0"
	"vendid=0x14e4\0"
	"devid=0x43a3\0"
	"manfid=0x2d0\0"
	"nocrc=1\0"
	"otpimagesize=502\0"
	"xtalfreq=37400\0"
	"rxgains2gelnagaina0=3\0"
	"rxgains2gtrisoa0=5\0"
	"rxgains2gtrelnabypa0=1\0"
	"rxgains5gelnagaina0=3\0"
	"rxgains5gtrisoa0=5\0"
	"rxgains5gtrelnabypa0=1\0"
	"rxgains5gmelnagaina0=3\0"
	"rxgains5gmtrisoa0=5\0"
	"rxgains5gmtrelnabypa0=1\0"
	"rxgains5ghelnagaina0=3\0"
	"rxgains5ghtrisoa0=5\0"
	"rxgains5ghtrelnabypa0=1\0"
	"rxgains2gelnagaina1=3\0"
	"rxgains2gtrisoa1=5\0"
	"rxgains2gtrelnabypa1=1\0"
	"rxgains5gelnagaina1=3\0"
	"rxgains5gtrisoa1=5\0"
	"rxgains5gtrelnabypa1=1\0"
	"rxgains5gmelnagaina1=3\0"
	"rxgains5gmtrisoa1=5\0"
	"rxgains5gmtrelnabypa1=1\0"
	"rxgains5ghelnagaina1=3\0"
	"rxgains5ghtrisoa1=5\0"
	"rxgains5ghtrelnabypa1=1\0"
	"rxchain=3\0"
	"txchain=3\0"
	"aa2g=3\0"
	"aa5g=3\0"
	"agbg0=2\0"
	"agbg1=2\0"
	"aga0=2\0"
	"aga1=2\0"
	"tssipos2g=1\0"
	"extpagain2g=0\0"
	"tssipos5g=1\0"
	"extpagain5g=0\0"
	"tempthresh=255\0"
	"tempoffset=255\0"
	"rawtempsense=0x1ff\0"
	"pa2ga0=-179,6402,-795\0"
	"pa2ga1=-179,6374,-786\0"
	"pa5ga0=-94,5766,-609,-105,5707,-620,-76,5785,-596,-59,5817,-583\0"
	"pa5ga1=-165,5385,-645,-153,5488,-649,-78,5893,-613,-91,5716,-610\0"
	"pa5gbw4080a0=-104,6005,-661,-128,5857,-674,-97,6093,-677,-59,6200,-657\0"
	"pa5gbw4080a1=-170,5645,-687,-168,5655,-686,-167,5636,-685,-166,5661,-695\0"
	"maxp2ga0=48\0"
	"maxp5ga0=48,48,48,48\0"
	"maxp2ga1=48\0"
	"maxp5ga1=48,48,48,48\0"
	"subband5gver=0x4\0"
	"paparambwver=2\0"
	"pdoffset40ma0=0x0000\0"
	"pdoffset80ma0=0x0222\0"
	"pdoffset40ma1=0x0000\0"
	"pdoffset80ma1=0x0222\0"
	"cckbw202gpo=0\0"
	"cckbw20ul2gpo=0\0"
	"mcsbw202gpo=0\0"
	"mcsbw402gpo=0\0"
	"dot11agofdmhrbw202gpo=0\0"
	"ofdmlrbw202gpo=0x0000\0"
	"mcsbw205glpo=0\0"
	"mcsbw405glpo=0\0"
	"mcsbw805glpo=0\0"
	"mcsbw1605glpo=0\0"
	"mcsbw205gmpo=0\0"
	"mcsbw405gmpo=0\0"
	"mcsbw805gmpo=0\0"
	"mcsbw1605gmpo=0\0"
	"mcsbw205ghpo=0\0"
	"mcsbw405ghpo=0\0"
	"mcsbw805ghpo=0\0"
	"mcsbw1605ghpo=0\0"
	"mcslr5glpo=0x0000\0"
	"mcslr5gmpo=0x0000\0"
	"mcslr5ghpo=0x0000\0"
	"sb20in40hrpo=0x0\0"
	"sb20in80and160hr5glpo=0x0\0"
	"sb40and80hr5glpo=0x0\0"
	"sb20in80and160hr5gmpo=0x0\0"
	"sb40and80hr5gmpo=0x0\0"
	"sb20in80and160hr5ghpo=0x0\0"
	"sb40and80hr5ghpo=0x0\0"
	"sb20in40lrpo=0x0\0"
	"sb20in80and160lr5glpo=0x0\0"
	"sb40and80lr5glpo=0x0\0"
	"sb20in80and160lr5gmpo=0x0\0"
	"sb40and80lr5gmpo=0x0\0"
	"sb20in80and160lr5ghpo=0x0\0"
	"sb40and80lr5ghpo=0x0\0"
	"dot11agduphrpo=0x0\0"
	"dot11agduplrpo=0x0\0"
	"phycal_tempdelta=255\0"
	"temps_period=15\0"
	"temps_hysteresis=15\0"
	"swctrlmap_5g=0x00000101,0x06060000,0x02020000,0x000000,0x047\0"
	"swctrlmap_2g=0x00000808,0x30300000,0x20200000,0x803000,0x0ff\0"
	"swctrlmapext_5g=0x00000000,0x00000000,0x00000000,0x000000,0x000\0"
	"swctrlmapext_2g=0x00000000,0x00000000,0x00000000,0x000000,0x000\0"
	"rssi_delta_2g_c0=0,0,0,0\0"
	"rssi_delta_2g_c1=0,0,0,0\0"
	"rssi_delta_5gh_c0=0,0,0,0,0,0\0"
	"rssi_delta_5gh_c1=0,0,0,0,0,0\0"
	"rssi_delta_5gmu_c0=0,0,0,0,0,0\0"
	"rssi_delta_5gmu_c1=0,0,0,0,0,0\0"
	"rssi_delta_5gml_c0=0,0,0,0,0,0\0"
	"rssi_delta_5gml_c1=0,0,0,0,0,0\0"
	"rssi_delta_5gl_c0=0,0,0,0,0,0\0"
	"rssi_delta_5gl_c1=0,0,0,0,0,0\0"
	"rstr_rxgaintempcoeff2g=57\0"
	"rstr_rxgaintempcoeff5gl=57\0"
	"rstr_rxgaintempcoeff5gml=57\0"
	"rstr_rxgaintempcoeff5gmu=57\0"
	"rstr_rxgaintempcoeff5gh=57\0"
	"rssi_cal_rev=0\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_4354a1)[] =
	"sromrev=11\0"
	"boardrev=0x1132\0"
	"boardtype=0x0707\0"
	"boardflags=0x02400201\0"
	"boardflags2=0x00802000\0"
	"boardflags3=0x4008010a\0"
	"macaddr=00:90:4c:14:b0:01\0"
	"ccode=0\0"
	"regrev=0\0"
	"antswitch=0\0"
	"pdgain5g=4\0"
	"pdgain2g=4\0"
	"tworangetssi2g=0\0"
	"tworangetssi5g=0\0"
	"paprdis=0\0"
	"femctrl=10\0"
	"vendid=0x14e4\0"
	"devid=0x43a3\0"
	"manfid=0x2d0\0"
	"nocrc=1\0"
	"otpimagesize=502\0"
	"xtalfreq=37400\0"
	"rxgains2gelnagaina0=0\0"
	"rxgains2gtrisoa0=7\0"
	"rxgains2gtrelnabypa0=0\0"
	"rxgains5gelnagaina0=0\0"
	"rxgains5gtrisoa0=11\0"
	"rxgains5gtrelnabypa0=0\0"
	"rxgains5gmelnagaina0=0\0"
	"rxgains5gmtrisoa0=13\0"
	"rxgains5gmtrelnabypa0=0\0"
	"rxgains5ghelnagaina0=0\0"
	"rxgains5ghtrisoa0=12\0"
	"rxgains5ghtrelnabypa0=0\0"
	"rxgains2gelnagaina1=0\0"
	"rxgains2gtrisoa1=7\0"
	"rxgains2gtrelnabypa1=0\0"
	"rxgains5gelnagaina1=0\0"
	"rxgains5gtrisoa1=10\0"
	"rxgains5gtrelnabypa1=0\0"
	"rxgains5gmelnagaina1=0\0"
	"rxgains5gmtrisoa1=11\0"
	"rxgains5gmtrelnabypa1=0\0"
	"rxgains5ghelnagaina1=0\0"
	"rxgains5ghtrisoa1=11\0"
	"rxgains5ghtrelnabypa1=0\0"
	"rxchain=3\0"
	"txchain=3\0"
	"aa2g=3\0"
	"aa5g=3\0"
	"agbg0=2\0"
	"agbg1=2\0"
	"aga0=2\0"
	"aga1=2\0"
	"tssipos2g=1\0"
	"extpagain2g=2\0"
	"tssipos5g=1\0"
	"extpagain5g=2\0"
	"tempthresh=255\0"
	"tempoffset=255\0"
	"rawtempsense=0x1ff\0"
	"pa2ga0=-147,6192,-705\0"
	"pa2ga1=-161,6041,-701\0"
	"pa5ga0=-194,6069,-739,-188,6137,-743,-185,5931,-725,-171,5898,-715\0"
	"pa5ga1=-190,6248,-757,-190,6275,-759,-190,6225,-757,-184,6131,-746\0"
	"subband5gver=0x4\0"
	"pdoffsetcckma0=0x4\0"
	"pdoffsetcckma1=0x4\0"
	"pdoffset40ma0=0x0000\0"
	"pdoffset80ma0=0x0000\0"
	"pdoffset40ma1=0x0000\0"
	"pdoffset80ma1=0x0000\0"
	"maxp2ga0=74\0"
	"maxp5ga0=74,74,74,74\0"
	"maxp2ga1=74\0"
	"maxp5ga1=74,74,74,74\0"
	"cckbw202gpo=0x0000\0"
	"cckbw20ul2gpo=0x0000\0"
	"mcsbw202gpo=0x99644422\0"
	"mcsbw402gpo=0x99644422\0"
	"dot11agofdmhrbw202gpo=0x6666\0"
	"ofdmlrbw202gpo=0x0022\0"
	"mcsbw205glpo=0x88766663\0"
	"mcsbw405glpo=0x88666663\0"
	"mcsbw805glpo=0xbb666665\0"
	"mcsbw205gmpo=0xd8666663\0"
	"mcsbw405gmpo=0x88666663\0"
	"mcsbw805gmpo=0xcc666665\0"
	"mcsbw205ghpo=0xdc666663\0"
	"mcsbw405ghpo=0xaa666663\0"
	"mcsbw805ghpo=0xdd666665\0"
	"mcslr5glpo=0x0000\0"
	"mcslr5gmpo=0x0000\0"
	"mcslr5ghpo=0x0000\0"
	"sb20in40hrpo=0x0\0"
	"sb20in80and160hr5glpo=0x0\0"
	"sb40and80hr5glpo=0x0\0"
	"sb20in80and160hr5gmpo=0x0\0"
	"sb40and80hr5gmpo=0x0\0"
	"sb20in80and160hr5ghpo=0x0\0"
	"sb40and80hr5ghpo=0x0\0"
	"sb20in40lrpo=0x0\0"
	"sb20in80and160lr5glpo=0x0\0"
	"sb40and80lr5glpo=0x0\0"
	"sb20in80and160lr5gmpo=0x0\0"
	"sb40and80lr5gmpo=0x0\0"
	"sb20in80and160lr5ghpo=0x0\0"
	"sb40and80lr5ghpo=0x0\0"
	"dot11agduphrpo=0x0\0"
	"dot11agduplrpo=0x0\0"
	"phycal_tempdelta=255\0"
	"temps_period=15\0"
	"temps_hysteresis=15\0"
	"AvVmid_c0=2,140,2,145,2,145,2,145,2,145\0"
	"AvVmid_c1=2,140,2,145,2,145,2,145,2,145\0"
	"AvVmid_c2=0,0,0,0,0,0,0,0,0,0\0"
	"rssicorrnorm_c0=4,4\0"
	"rssicorrnorm_c1=4,4\0"
	"rssicorrnorm5g_c0=1,2,3,1,2,3,6,6,8,6,6,8\0"
	"rssicorrnorm5g_c1=1,2,3,2,2,2,7,7,8,7,7,8\0"
	"swctrlmap_5g=0x00000202,0x00000101,0x00000202,0x000000,0x08f\0"
	"swctrlmap_2g=0x00004040,0x00001010,0x00004040,0x200010,0x0ff\0"
	"swctrlmapext_5g=0x00000000,0x00000000,0x00000000,0x000000,0x000\0"
	"swctrlmapext_2g=0x00000000,0x00000000,0x00000000,0x000000,0x000\0"
	"rssi_delta_2g_c0=0,0,0,0\0"
	"rssi_delta_2g_c1=0,0,0,0\0"
	"rssi_delta_5gh_c0=0,0,0,0,0,0\0"
	"rssi_delta_5gh_c1=0,0,0,0,0,0\0"
	"rssi_delta_5gmu_c0=0,0,0,0,0,0\0"
	"rssi_delta_5gmu_c1=0,0,0,0,0,0\0"
	"rssi_delta_5gml_c0=0,0,0,0,0,0\0"
	"rssi_delta_5gml_c1=0,0,0,0,0,0\0"
	"rssi_delta_5gl_c0=0,0,0,0,0,0\0"
	"rssi_delta_5gl_c1=0,0,0,0,0,0\0"
	"rstr_rxgaintempcoeff2g=57\0"
	"rstr_rxgaintempcoeff5gl=57\0"
	"rstr_rxgaintempcoeff5gml=57\0"
	"rstr_rxgaintempcoeff5gmu=57\0"
	"rstr_rxgaintempcoeff5gh=57\0"
	"rssi_cal_rev=0\0"
	"END\0";

/**
 * The contents of this array is a first attempt, is likely incorrect for 43602, needs to be
 * edited in a later stage.
 */
static char BCMATTACHDATA(defaultsromvars_43602)[] =
	"sromrev=11\0"
	"boardrev=0x1421\0"
	"boardflags=0x10401001\0"
	"boardflags2=0x00000002\0"
	"boardflags3=0x00000003\0"
	"boardtype=0x61b\0"
	"subvid=0x14e4\0"
	"boardnum=62526\0"
	"macaddr=00:90:4c:0d:f4:3e\0"
	"ccode=X0\0"
	"regrev=15\0"
	"aa2g=7\0"
	"aa5g=7\0"
	"agbg0=71\0"
	"agbg1=71\0"
	"agbg2=133\0"
	"aga0=71\0"
	"aga1=133\0"
	"aga2=133\0"
	"antswitch=0\0"
	"tssiposslope2g=1\0"
	"epagain2g=0\0"
	"pdgain2g=9\0"
	"tworangetssi2g=0\0"
	"papdcap2g=0\0"
	"femctrl=2\0"
	"tssiposslope5g=1\0"
	"epagain5g=0\0"
	"pdgain5g=9\0"
	"tworangetssi5g=0\0"
	"papdcap5g=0\0"
	"gainctrlsph=0\0"
	"tempthresh=255\0"
	"tempoffset=255\0"
	"rawtempsense=0x1ff\0"
	"measpower=0x7f\0"
	"tempsense_slope=0xff\0"
	"tempcorrx=0x3f\0"
	"tempsense_option=0x3\0"
	"xtalfreq=40000\0"
	"phycal_tempdelta=255\0"
	"temps_period=15\0"
	"temps_hysteresis=15\0"
	"measpower1=0x7f\0"
	"measpower2=0x7f\0"
	"pdoffset2g40ma0=15\0"
	"pdoffset2g40ma1=15\0"
	"pdoffset2g40ma2=15\0"
	"pdoffset2g40mvalid=1\0"
	"pdoffset40ma0=9010\0"
	"pdoffset40ma1=12834\0"
	"pdoffset40ma2=8994\0"
	"pdoffset80ma0=16\0"
	"pdoffset80ma1=4096\0"
	"pdoffset80ma2=0\0"
	"subband5gver=0x4\0"
	"cckbw202gpo=0\0"
	"cckbw20ul2gpo=0\0"
	"mcsbw202gpo=2571386880\0"
	"mcsbw402gpo=2571386880\0"
	"dot11agofdmhrbw202gpo=17408\0"
	"ofdmlrbw202gpo=0\0"
	"mcsbw205glpo=4001923072\0"
	"mcsbw405glpo=4001923072\0"
	"mcsbw805glpo=4001923072\0"
	"mcsbw1605glpo=0\0"
	"mcsbw205gmpo=3431497728\0"
	"mcsbw405gmpo=3431497728\0"
	"mcsbw805gmpo=3431497728\0"
	"mcsbw1605gmpo=0\0"
	"mcsbw205ghpo=3431497728\0"
	"mcsbw405ghpo=3431497728\0"
	"mcsbw805ghpo=3431497728\0"
	"mcsbw1605ghpo=0\0"
	"mcslr5glpo=0\0"
	"mcslr5gmpo=0\0"
	"mcslr5ghpo=0\0"
	"sb20in40hrpo=0\0"
	"sb20in80and160hr5glpo=0\0"
	"sb40and80hr5glpo=0\0"
	"sb20in80and160hr5gmpo=0\0"
	"sb40and80hr5gmpo=0\0"
	"sb20in80and160hr5ghpo=0\0"
	"sb40and80hr5ghpo=0\0"
	"sb20in40lrpo=0\0"
	"sb20in80and160lr5glpo=0\0"
	"sb40and80lr5glpo=0\0"
	"sb20in80and160lr5gmpo=0\0"
	"sb40and80lr5gmpo=0\0"
	"sb20in80and160lr5ghpo=0\0"
	"sb40and80lr5ghpo=0\0"
	"dot11agduphrpo=0\0"
	"dot11agduplrpo=0\0"
	"pcieingress_war=15\0"
	"sar2g=18\0"
	"sar5g=15\0"
	"noiselvl2ga0=31\0"
	"noiselvl2ga1=31\0"
	"noiselvl2ga2=31\0"
	"noiselvl5ga0=31,31,31,31\0"
	"noiselvl5ga1=31,31,31,31\0"
	"noiselvl5ga2=31,31,31,31\0"
	"rxgainerr2ga0=63\0"
	"rxgainerr2ga1=31\0"
	"rxgainerr2ga2=31\0"
	"rxgainerr5ga0=63,63,63,63\0"
	"rxgainerr5ga1=31,31,31,31\0"
	"rxgainerr5ga2=31,31,31,31\0"
	"maxp2ga0=76\0"
	"pa2ga0=0xff3c,0x172c,0xfd20\0"
	"rxgains5gmelnagaina0=7\0"
	"rxgains5gmtrisoa0=15\0"
	"rxgains5gmtrelnabypa0=1\0"
	"rxgains5ghelnagaina0=7\0"
	"rxgains5ghtrisoa0=15\0"
	"rxgains5ghtrelnabypa0=1\0"
	"rxgains2gelnagaina0=4\0"
	"rxgains2gtrisoa0=7\0"
	"rxgains2gtrelnabypa0=1\0"
	"rxgains5gelnagaina0=3\0"
	"rxgains5gtrisoa0=7\0"
	"rxgains5gtrelnabypa0=1\0"
	"maxp5ga0=76,76,76,76\0"
"pa5ga0=0xff3a,0x14d4,0xfd5f,0xff36,0x1626,0xfd2e,0xff42,0x15bd,0xfd47,0xff39,0x15a3,0xfd3d\0"
	"maxp2ga1=76\0"
	"pa2ga1=0xff2a,0x16b2,0xfd28\0"
	"rxgains5gmelnagaina1=7\0"
	"rxgains5gmtrisoa1=15\0"
	"rxgains5gmtrelnabypa1=1\0"
	"rxgains5ghelnagaina1=7\0"
	"rxgains5ghtrisoa1=15\0"
	"rxgains5ghtrelnabypa1=1\0"
	"rxgains2gelnagaina1=3\0"
	"rxgains2gtrisoa1=6\0"
	"rxgains2gtrelnabypa1=1\0"
	"rxgains5gelnagaina1=3\0"
	"rxgains5gtrisoa1=6\0"
	"rxgains5gtrelnabypa1=1\0"
	"maxp5ga1=76,76,76,76\0"
"pa5ga1=0xff4e,0x1530,0xfd53,0xff58,0x15b4,0xfd4d,0xff58,0x1671,0xfd2f,0xff55,0x15e2,0xfd46\0"
	"maxp2ga2=76\0"
	"pa2ga2=0xff3c,0x1736,0xfd1f\0"
	"rxgains5gmelnagaina2=7\0"
	"rxgains5gmtrisoa2=15\0"
	"rxgains5gmtrelnabypa2=1\0"
	"rxgains5ghelnagaina2=7\0"
	"rxgains5ghtrisoa2=15\0"
	"rxgains5ghtrelnabypa2=1\0"
	"rxgains2gelnagaina2=4\0"
	"rxgains2gtrisoa2=7\0"
	"rxgains2gtrelnabypa2=1\0"
	"rxgains5gelnagaina2=3\0"
	"rxgains5gtrisoa2=7\0"
	"rxgains5gtrelnabypa2=1\0"
	"maxp5ga2=76,76,76,76\0"
"pa5ga2=0xff2d,0x144a,0xfd63,0xff35,0x15d7,0xfd3b,0xff35,0x1668,0xfd2f,0xff31,0x1664,0xfd27\0"
	"END\0";
#endif 
#endif /* !defined(BCMDONGLEHOST) */

static bool srvars_inited = FALSE; /* Use OTP/SROM as global variables */

/* BCMHOSTVARS is enabled only if WLTEST is enabled or BCMEXTNVM is enabled */
#if (!defined(BCMDONGLEHOST) && defined(BCMHOSTVARS)) || (defined(BCMUSBDEV_BMAC) || \
	defined(BCM_BMAC_VARS_APPEND))
/* It must end with pattern of "END" */
static uint
BCMATTACHFN(srom_vars_len)(char *vars)
{
	uint pos = 0;
	uint len;
	char *s;

	for (s = vars; s && *s;) {

		if (strcmp(s, "END") == 0)
			break;

		len = strlen(s);
		s += strlen(s) + 1;
		pos += len + 1;
		/* BS_ERROR(("len %d vars[pos] %s\n", pos, s)); */
		if (pos > 4000) {
			return 0;
		}
	}

	return pos + 4;	/* include the "END\0" */
}
#endif 

#if !defined(BCMDONGLEHOST)
/** Initialization of varbuf structure */
static void
BCMATTACHFN(varbuf_init)(varbuf_t *b, char *buf, uint size)
{
	b->size = size;
	b->base = b->buf = buf;
}

/** append a null terminated var=value string */
static int
BCMATTACHFN(varbuf_append)(varbuf_t *b, const char *fmt, ...)
{
	va_list ap;
	int r;
	size_t len;
	char *s;

	if (b->size < 2)
	  return 0;

	va_start(ap, fmt);
	r = vsnprintf(b->buf, b->size, fmt, ap);
	va_end(ap);

	/* C99 snprintf behavior returns r >= size on overflow,
	 * others return -1 on overflow.
	 * All return -1 on format error.
	 * We need to leave room for 2 null terminations, one for the current var
	 * string, and one for final null of the var table. So check that the
	 * strlen written, r, leaves room for 2 chars.
	 */
	if ((r == -1) || (r > (int)(b->size - 2))) {
		b->size = 0;
		return 0;
	}

	/* Remove any earlier occurrence of the same variable */
	if ((s = strchr(b->buf, '=')) != NULL) {
		len = (size_t)(s - b->buf);
		for (s = b->base; s < b->buf;) {
			if ((bcmp(s, b->buf, len) == 0) && s[len] == '=') {
				len = strlen(s) + 1;
				memmove(s, (s + len), ((b->buf + r + 1) - (s + len)));
				b->buf -= len;
				b->size += (unsigned int)len;
				break;
			}

			while (*s++)
				;
		}
	}

	/* skip over this string's null termination */
	r++;
	b->size -= r;
	b->buf += r;

	return r;
}

/**
 * Initialize local vars from the right source for this platform. Called from siutils.c.
 *
 * vars - pointer to a to-be created pointer area for "environment" variables. Some callers of this
 *        function set 'vars' to NULL, in that case this function will prematurely return.
 *
 * Return 0 on success, nonzero on error.
 */
int
BCMATTACHFN(srom_var_init)(si_t *sih, uint bustype, void *curmap, osl_t *osh,
	char **vars, uint *count)
{
	ASSERT(bustype == BUSTYPE(bustype));
	if (vars == NULL || count == NULL)
		return (0);

	*vars = NULL;
	*count = 0;

	switch (BUSTYPE(bustype)) {
	case SI_BUS:
	/* deliberate fall through */
	case JTAG_BUS:
		/* 43602a0 & 43569 can operate as PCIe full dongle with SPROM */
#ifndef BCM_OL_DEV
		if ((CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43570_CHIP_ID) ||
			((CHIPID(sih->chip) == BCM4356_CHIP_ID) &&
			(CHIPREV(sih->chiprev) > 1)) ||
			(CHIPID(sih->chip) == BCM43462_CHIP_ID)) {
			return initvars_srom_pci(sih, curmap, vars, count);
		} else
			return initvars_srom_si(sih, osh, curmap, vars, count);
#else
		return initvars_srom_si(sih, osh, curmap, vars, count);
#endif /* BCM_OL_DEV */

	case PCI_BUS: {
		int ret;

		ASSERT(curmap != NULL);
		if (curmap == NULL)
			return (-1);

		/* First check for CIS format. if not CIS, try SROM format */
		if ((ret = initvars_cis_pci(sih, osh, curmap, vars, count)))
			return initvars_srom_pci(sih, curmap, vars, count);
		return ret;
	}

	case PCMCIA_BUS:
		return initvars_cis_pcmcia(sih, osh, vars, count);


#ifdef BCMSPI
	case SPI_BUS:
		return initvars_cis_spi(osh, vars, count);
#endif /* BCMSPI */

	default:
		ASSERT(0);
	}
	return (-1);
}
#endif /* !defined(BCMDONGLEHOST) */

/** support only 16-bit word read from srom */
int
srom_read(si_t *sih, uint bustype, void *curmap, osl_t *osh,
          uint byteoff, uint nbytes, uint16 *buf, bool check_crc)
{
	uint i, off, nw;

	ASSERT(bustype == BUSTYPE(bustype));

	/* check input - 16-bit access only */
	if (byteoff & 1 || nbytes & 1 || (byteoff + nbytes) > SROM_MAX)
		return 1;

	off = byteoff / 2;
	nw = nbytes / 2;

#ifdef BCMPCIEDEV
	if ((BUSTYPE(bustype) == SI_BUS) &&
	    ((CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
	     (CHIPID(sih->chip) == BCM43570_CHIP_ID) ||
	     (CHIPID(sih->chip) == BCM43462_CHIP_ID))) {
#else
	if (BUSTYPE(bustype) == PCI_BUS) {
#endif /* BCMPCIEDEV */
		if (!curmap)
			return 1;

		if (si_is_sprom_available(sih)) {
			uint16 *srom;

			srom = (uint16 *)srom_offset(sih, curmap);
			if (srom == NULL)
				return 1;

			if (sprom_read_pci(osh, sih, srom, off, buf, nw, check_crc))
				return 1;
		}
#if !defined(BCMDONGLEHOST) && (defined(BCMNVRAMW) || defined(BCMNVRAMR))
		else if (!((BUSTYPE(bustype) == SI_BUS) &&
			((CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43462_CHIP_ID)))) {
			if (otp_read_pci(osh, sih, buf, nbytes))
				return 1;
		}
#endif /* !BCMDONGLEHOST && (BCMNVRAMW||BCMNVRAMR) */
	} else if (BUSTYPE(bustype) == PCMCIA_BUS) {
		for (i = 0; i < nw; i++) {
			if (sprom_read_pcmcia(osh, (uint16)(off + i), (uint16 *)(buf + i)))
				return 1;
		}
#ifdef BCMSPI
	} else if (BUSTYPE(bustype) == SPI_BUS) {
	                if (bcmsdh_cis_read(NULL, SDIO_FUNC_1, (uint8 *)buf, byteoff + nbytes) != 0)
				return 1;
#endif /* BCMSPI */
	} else if (BUSTYPE(bustype) == SI_BUS) {
#if defined(BCMUSBDEV)
		if (SPROMBUS == PCMCIA_BUS) {
			uint origidx;
			void *regs;
			int rc;
			bool wasup;

			/* Don't bother if we can't talk to SPROM */
			if (!si_is_sprom_available(sih))
				return 1;

			origidx = si_coreidx(sih);
			regs = si_setcore(sih, PCMCIA_CORE_ID, 0);
			if (!regs)
				regs = si_setcore(sih, SDIOD_CORE_ID, 0);
			ASSERT(regs != NULL);

			if (!(wasup = si_iscoreup(sih)))
				si_core_reset(sih, 0, 0);

			rc = get_si_pcmcia_srom(sih, osh, regs, byteoff, buf, nbytes, check_crc);

			if (!wasup)
				si_core_disable(sih, 0);

			si_setcoreidx(sih, origidx);
			return rc;
		}
#endif 

		return 1;
	} else {
		return 1;
	}

	return 0;
}

#if defined(WLTEST) || defined(DHD_SPROM) || defined(BCMDBG)
/** support only 16-bit word write into srom */
int
srom_write(si_t *sih, uint bustype, void *curmap, osl_t *osh,
           uint byteoff, uint nbytes, uint16 *buf)
{
	uint i, nw, crc_range;
	uint16 *old, *new;
	uint8 crc;
	volatile uint32 val32;
	int rc = 1;

	ASSERT(bustype == BUSTYPE(bustype));

	old = MALLOC(osh, SROM_MAXW * sizeof(uint16));
	new = MALLOC(osh, SROM_MAXW * sizeof(uint16));

	if (old == NULL || new == NULL)
		goto done;

	/* check input - 16-bit access only. use byteoff 0x55aa to indicate
	 * srclear
	 */
	if ((byteoff != 0x55aa) && ((byteoff & 1) || (nbytes & 1)))
		goto done;

	if ((byteoff != 0x55aa) && ((byteoff + nbytes) > SROM_MAX))
		goto done;

	if (BUSTYPE(bustype) == PCMCIA_BUS) {
		crc_range = SROM_MAX;
	}
#if defined(BCMUSBDEV)
	else {
		crc_range = srom_size(sih, osh);
	}
#else
	else {
		crc_range = (SROM8_SIGN + 1) * 2;	/* must big enough for SROM8 */
	}
#endif 

	nw = crc_range / 2;
	/* read first small number words from srom, then adjust the length, read all */
	if (srom_read(sih, bustype, curmap, osh, 0, crc_range, old, FALSE))
		goto done;

	BS_ERROR(("%s: old[SROM4_SIGN] 0x%x, old[SROM8_SIGN] 0x%x\n",
	          __FUNCTION__, old[SROM4_SIGN], old[SROM8_SIGN]));
	/* Deal with blank srom */
	if (old[0] == 0xffff) {
		/* see if the input buffer is valid SROM image or not */
		if (buf[SROM11_SIGN] == SROM11_SIGNATURE) {
			BS_ERROR(("%s: buf[SROM11_SIGN] 0x%x\n",
				__FUNCTION__, buf[SROM11_SIGN]));

			/* block invalid buffer size */
			if (nbytes < SROM11_WORDS * 2) {
				rc = BCME_BUFTOOSHORT;
				goto done;
			} else if (nbytes > SROM11_WORDS * 2) {
				rc = BCME_BUFTOOLONG;
				goto done;
			}

			nw = SROM11_WORDS;
		} else if ((buf[SROM4_SIGN] == SROM4_SIGNATURE) ||
			(buf[SROM8_SIGN] == SROM4_SIGNATURE)) {
			BS_ERROR(("%s: buf[SROM4_SIGN] 0x%x, buf[SROM8_SIGN] 0x%x\n",
				__FUNCTION__, buf[SROM4_SIGN], buf[SROM8_SIGN]));

			/* block invalid buffer size */
			if (nbytes < SROM4_WORDS * 2) {
				rc = BCME_BUFTOOSHORT;
				goto done;
			} else if (nbytes > SROM4_WORDS * 2) {
				rc = BCME_BUFTOOLONG;
				goto done;
			}

			nw = SROM4_WORDS;
		} else if (nbytes == SROM_WORDS * 2){ /* the other possible SROM format */
			BS_ERROR(("%s: Not SROM4 or SROM8.\n", __FUNCTION__));

			nw = SROM_WORDS;
		} else {
			BS_ERROR(("%s: Invalid input file signature\n", __FUNCTION__));
			rc = BCME_BADARG;
			goto done;
		}
		crc_range = nw * 2;
		if (srom_read(sih, bustype, curmap, osh, 0, crc_range, old, FALSE))
			goto done;
	} else if (old[SROM11_SIGN] == SROM11_SIGNATURE) {
		nw = SROM11_WORDS;
		crc_range = nw * 2;
		if (srom_read(sih, bustype, curmap, osh, 0, crc_range, old, FALSE))
			goto done;
	} else if ((old[SROM4_SIGN] == SROM4_SIGNATURE) ||
	           (old[SROM8_SIGN] == SROM4_SIGNATURE)) {
		nw = SROM4_WORDS;
		crc_range = nw * 2;
		if (srom_read(sih, bustype, curmap, osh, 0, crc_range, old, FALSE))
			goto done;
	} else {
		/* Assert that we have already read enough for sromrev 2 */
		ASSERT(crc_range >= SROM_WORDS * 2);
		nw = SROM_WORDS;
		crc_range = nw * 2;
	}

	if (byteoff == 0x55aa) {
		/* Erase request */
		crc_range = 0;
		memset((void *)new, 0xff, nw * 2);
	} else {
		/* Copy old contents */
		bcopy((void *)old, (void *)new, nw * 2);
		/* make changes */
		bcopy((void *)buf, (void *)&new[byteoff / 2], nbytes);
	}

	if (crc_range) {
		/* calculate crc */
		htol16_buf(new, crc_range);
		crc = ~hndcrc8((uint8 *)new, crc_range - 1, CRC8_INIT_VALUE);
		ltoh16_buf(new, crc_range);
		new[nw - 1] = (crc << 8) | (new[nw - 1] & 0xff);
	}


#ifdef BCMPCIEDEV
	if ((BUSTYPE(bustype) == SI_BUS) &&
	    ((CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
	     (CHIPID(sih->chip) == BCM43462_CHIP_ID))) {
#else
	if (BUSTYPE(bustype) == PCI_BUS) {
#endif /* BCMPCIEDEV */
		uint16 *srom = NULL;
		void *ccregs = NULL;
		uint32 ccval = 0;

		if ((CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43431_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43462_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43526_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM4352_CHIP_ID)) {
			/* save current control setting */
			ccval = si_chipcontrl_read(sih);
		}

		if ((CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43431_CHIP_ID)) {
			/* Disable Ext PA lines to allow reading from SROM */
			si_chipcontrl_epa4331(sih, FALSE);
		} else if (((CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43462_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM4352_CHIP_ID)) &&
			(CHIPREV(sih->chiprev) <= 2)) {
			si_chipcontrl_srom4360(sih, TRUE);
		}

		/* enable writes to the SPROM */
		if (sih->ccrev > 31) {
			if (BUSTYPE(sih->bustype) == SI_BUS)
				ccregs = (void *)SI_ENUM_BASE;
			else
				ccregs = (void *)((uint8 *)curmap + PCI_16KB0_CCREGS_OFFSET);
			srom = (uint16 *)((uint8 *)ccregs + CC_SROM_OTP);
			(void)srom_cc_cmd(sih, osh, ccregs, SRC_OP_WREN, 0, 0);
		} else {
			srom = (uint16 *)((uint8 *)curmap + PCI_BAR0_SPROM_OFFSET);
			val32 = OSL_PCI_READ_CONFIG(osh, PCI_SPROM_CONTROL, sizeof(uint32));
			val32 |= SPROM_WRITEEN;
			OSL_PCI_WRITE_CONFIG(osh, PCI_SPROM_CONTROL, sizeof(uint32), val32);
		}
		bcm_mdelay(WRITE_ENABLE_DELAY);
		/* write srom */
		for (i = 0; i < nw; i++) {
			if (old[i] != new[i]) {
				if (sih->ccrev > 31) {
					if ((sih->cccaps & CC_CAP_SROM) == 0) {
						/* No srom support in this chip */
						BS_ERROR(("srom_write, invalid srom, skip\n"));
					} else
						(void)srom_cc_cmd(sih, osh, ccregs, SRC_OP_WRITE,
							i, new[i]);
				} else {
					W_REG(osh, &srom[i], new[i]);
				}
				bcm_mdelay(WRITE_WORD_DELAY);
			}
		}
		/* disable writes to the SPROM */
		if (sih->ccrev > 31) {
			(void)srom_cc_cmd(sih, osh, ccregs, SRC_OP_WRDIS, 0, 0);
		} else {
			OSL_PCI_WRITE_CONFIG(osh, PCI_SPROM_CONTROL, sizeof(uint32), val32 &
			                     ~SPROM_WRITEEN);
		}

		if ((CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43431_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43462_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM4352_CHIP_ID)) {
			/* Restore config after reading SROM */
			si_chipcontrl_restore(sih, ccval);
		}

	} else if (BUSTYPE(bustype) == PCMCIA_BUS) {
		/* enable writes to the SPROM */
		if (sprom_cmd_pcmcia(osh, SROM_WEN))
			goto done;
		bcm_mdelay(WRITE_ENABLE_DELAY);
		/* write srom */
		for (i = 0; i < nw; i++) {
			if (old[i] != new[i]) {
				sprom_write_pcmcia(osh, (uint16)(i), new[i]);
				bcm_mdelay(WRITE_WORD_DELAY);
			}
		}
		/* disable writes to the SPROM */
		if (sprom_cmd_pcmcia(osh, SROM_WDS))
			goto done;
	} else if (BUSTYPE(bustype) == SI_BUS) {
#if defined(BCMUSBDEV)
		if (SPROMBUS == PCMCIA_BUS) {
			uint origidx;
			void *regs;
			bool wasup;

			origidx = si_coreidx(sih);
			regs = si_setcore(sih, PCMCIA_CORE_ID, 0);
			if (!regs)
				regs = si_setcore(sih, SDIOD_CORE_ID, 0);
			ASSERT(regs != NULL);

			if (!(wasup = si_iscoreup(sih)))
				si_core_reset(sih, 0, 0);

			rc = set_si_pcmcia_srom(sih, osh, regs, byteoff, buf, nbytes);

			if (!wasup)
				si_core_disable(sih, 0);

			si_setcoreidx(sih, origidx);
			goto done;
		}
#endif 
		goto done;
	} else {
		goto done;
	}

	bcm_mdelay(WRITE_ENABLE_DELAY);
	rc = 0;

done:
	if (old != NULL)
		MFREE(osh, old, SROM_MAXW * sizeof(uint16));
	if (new != NULL)
		MFREE(osh, new, SROM_MAXW * sizeof(uint16));

	return rc;
}

/** support only 16-bit word write into srom */
int
srom_write_short(si_t *sih, uint bustype, void *curmap, osl_t *osh,
                 uint byteoff, uint16 value)
{
	volatile uint32 val32;
	int rc = 1;

	ASSERT(bustype == BUSTYPE(bustype));


	if (byteoff & 1)
		goto done;

#ifdef BCMPCIEDEV
	if ((BUSTYPE(bustype) == SI_BUS) &&
	    ((CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
	     (CHIPID(sih->chip) == BCM43462_CHIP_ID))) {
#else
	if (BUSTYPE(bustype) == PCI_BUS) {
#endif /* BCMPCIEDEV */
		uint16 *srom = NULL;
		void *ccregs = NULL;
		uint32 ccval = 0;

		if ((CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43431_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43526_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43462_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM4352_CHIP_ID)) {
			/* save current control setting */
			ccval = si_chipcontrl_read(sih);
		}

		if ((CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43431_CHIP_ID)) {
			/* Disable Ext PA lines to allow reading from SROM */
			si_chipcontrl_epa4331(sih, FALSE);
		} else if (((CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43462_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM4352_CHIP_ID)) &&
			(CHIPREV(sih->chiprev) <= 2)) {
			si_chipcontrl_srom4360(sih, TRUE);
		}

		/* enable writes to the SPROM */
		if (sih->ccrev > 31) {
			if (BUSTYPE(sih->bustype) == SI_BUS)
				ccregs = (void *)SI_ENUM_BASE;
			else
				ccregs = (void *)((uint8 *)curmap + PCI_16KB0_CCREGS_OFFSET);
			srom = (uint16 *)((uint8 *)ccregs + CC_SROM_OTP);
			(void)srom_cc_cmd(sih, osh, ccregs, SRC_OP_WREN, 0, 0);
		} else {
			srom = (uint16 *)((uint8 *)curmap + PCI_BAR0_SPROM_OFFSET);
			val32 = OSL_PCI_READ_CONFIG(osh, PCI_SPROM_CONTROL, sizeof(uint32));
			val32 |= SPROM_WRITEEN;
			OSL_PCI_WRITE_CONFIG(osh, PCI_SPROM_CONTROL, sizeof(uint32), val32);
		}
		bcm_mdelay(WRITE_ENABLE_DELAY);
		/* write srom */
		if (sih->ccrev > 31) {
			if ((sih->cccaps & CC_CAP_SROM) == 0) {
				/* No srom support in this chip */
				BS_ERROR(("srom_write, invalid srom, skip\n"));
			} else
				(void)srom_cc_cmd(sih, osh, ccregs, SRC_OP_WRITE,
				                   byteoff/2, value);
		} else {
			W_REG(osh, &srom[byteoff/2], value);
		}
		bcm_mdelay(WRITE_WORD_DELAY);

		/* disable writes to the SPROM */
		if (sih->ccrev > 31) {
			(void)srom_cc_cmd(sih, osh, ccregs, SRC_OP_WRDIS, 0, 0);
		} else {
			OSL_PCI_WRITE_CONFIG(osh, PCI_SPROM_CONTROL, sizeof(uint32), val32 &
			                     ~SPROM_WRITEEN);
		}

		if ((CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43431_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43526_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43462_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM4352_CHIP_ID)) {
			/* Restore config after reading SROM */
			si_chipcontrl_restore(sih, ccval);
		}

	} else if (BUSTYPE(bustype) == PCMCIA_BUS) {
		/* enable writes to the SPROM */
		if (sprom_cmd_pcmcia(osh, SROM_WEN))
			goto done;
		bcm_mdelay(WRITE_ENABLE_DELAY);
		/* write srom */
		sprom_write_pcmcia(osh, (uint16)(byteoff/2), value);
		bcm_mdelay(WRITE_WORD_DELAY);

		/* disable writes to the SPROM */
		if (sprom_cmd_pcmcia(osh, SROM_WDS))
			goto done;
	} else if (BUSTYPE(bustype) == SI_BUS) {
#if defined(BCMUSBDEV)
		if (SPROMBUS == PCMCIA_BUS) {
			uint origidx;
			void *regs;
			bool wasup;

			origidx = si_coreidx(sih);
			regs = si_setcore(sih, PCMCIA_CORE_ID, 0);
			if (!regs)
				regs = si_setcore(sih, SDIOD_CORE_ID, 0);
			ASSERT(regs != NULL);

			if (!(wasup = si_iscoreup(sih)))
				si_core_reset(sih, 0, 0);

			rc = set_si_pcmcia_srom(sih, osh, regs, byteoff, &value, 2);

			if (!wasup)
				si_core_disable(sih, 0);

			si_setcoreidx(sih, origidx);
			goto done;
		}
#endif 
		goto done;
	} else {
		goto done;
	}

	bcm_mdelay(WRITE_ENABLE_DELAY);
	rc = 0;

done:
	return rc;
}

#endif 

#if defined(BCMUSBDEV)
#define SI_PCMCIA_READ(osh, regs, fcr) \
		R_REG(osh, (volatile uint8 *)(regs) + 0x600 + (fcr) - 0x700 / 2)
#define SI_PCMCIA_WRITE(osh, regs, fcr, v) \
		W_REG(osh, (volatile uint8 *)(regs) + 0x600 + (fcr) - 0x700 / 2, v)

/** set PCMCIA srom command register */
static int
srom_cmd_si_pcmcia(osl_t *osh, uint8 *pcmregs, uint8 cmd)
{
	uint8 status = 0;
	uint wait_cnt = 0;

	/* write srom command register */
	SI_PCMCIA_WRITE(osh, pcmregs, SROM_CS, cmd);

	/* wait status */
	while (++wait_cnt < 1000000) {
		status = SI_PCMCIA_READ(osh, pcmregs, SROM_CS);
		if (status & SROM_DONE)
			return 0;
		OSL_DELAY(1);
	}

	BS_ERROR(("sr_cmd: Give up after %d tries, stat = 0x%x\n", wait_cnt, status));
	return 1;
}

/** read a word from the PCMCIA srom over SI */
static int
srom_read_si_pcmcia(osl_t *osh, uint8 *pcmregs, uint16 addr, uint16 *data)
{
	uint8 addr_l, addr_h,  data_l, data_h;

	addr_l = (uint8)((addr * 2) & 0xff);
	addr_h = (uint8)(((addr * 2) >> 8) & 0xff);

	/* set address */
	SI_PCMCIA_WRITE(osh, pcmregs, SROM_ADDRH, addr_h);
	SI_PCMCIA_WRITE(osh, pcmregs, SROM_ADDRL, addr_l);

	/* do read */
	if (srom_cmd_si_pcmcia(osh, pcmregs, SROM_READ))
		return 1;

	/* read data */
	data_h = SI_PCMCIA_READ(osh, pcmregs, SROM_DATAH);
	data_l = SI_PCMCIA_READ(osh, pcmregs, SROM_DATAL);
	*data = ((uint16)data_h << 8) | data_l;

	return 0;
}

#if defined(WLTEST) || defined(DHD_SPROM) || defined(BCMDBG)
/* write a word to the PCMCIA srom over SI */
static int
srom_write_si_pcmcia(osl_t *osh, uint8 *pcmregs, uint16 addr, uint16 data)
{
	uint8 addr_l, addr_h, data_l, data_h;
	int rc;

	addr_l = (uint8)((addr * 2) & 0xff);
	addr_h = (uint8)(((addr * 2) >> 8) & 0xff);

	/* set address */
	SI_PCMCIA_WRITE(osh, pcmregs, SROM_ADDRH, addr_h);
	SI_PCMCIA_WRITE(osh, pcmregs, SROM_ADDRL, addr_l);

	data_l = (uint8)(data & 0xff);
	data_h = (uint8)((data >> 8) & 0xff);

	/* write data */
	SI_PCMCIA_WRITE(osh, pcmregs, SROM_DATAH, data_h);
	SI_PCMCIA_WRITE(osh, pcmregs, SROM_DATAL, data_l);

	/* do write */
	rc = srom_cmd_si_pcmcia(osh, pcmregs, SROM_WRITE);
	OSL_DELAY(20000);
	return rc;
}
#endif 

/*
 * Read the srom for the pcmcia-srom over si case.
 * Return 0 on success, nonzero on error.
 */
static int
get_si_pcmcia_srom(si_t *sih, osl_t *osh, uint8 *pcmregs,
                   uint boff, uint16 *srom, uint bsz, bool check_crc)
{
	uint i, nw, woff, wsz;
	int err = 0;

	/* read must be at word boundary */
	ASSERT((boff & 1) == 0 && (bsz & 1) == 0);

	/* read sprom size and validate the parms */
	if ((nw = srom_size(sih, osh)) == 0) {
		BS_ERROR(("get_si_pcmcia_srom: sprom size unknown\n"));
		err = -1;
		goto out;
	}
	if (boff + bsz > 2 * nw) {
		BS_ERROR(("get_si_pcmcia_srom: sprom size exceeded\n"));
		err = -2;
		goto out;
	}

	/* read in sprom contents */
	for (woff = boff / 2, wsz = bsz / 2, i = 0;
	     woff < nw && i < wsz; woff ++, i ++) {
		if (srom_read_si_pcmcia(osh, pcmregs, (uint16)woff, &srom[i])) {
			BS_ERROR(("get_si_pcmcia_srom: sprom read failed\n"));
			err = -3;
			goto out;
		}
	}

	if (check_crc) {
		if (srom[0] == 0xffff) {
			/* The hardware thinks that an srom that starts with 0xffff
			 * is blank, regardless of the rest of the content, so declare
			 * it bad.
			 */
			BS_ERROR(("%s: srom[0] == 0xffff, assuming unprogrammed srom\n",
			          __FUNCTION__));
			err = -4;
			goto out;
		}

		/* fixup the endianness so crc8 will pass */
		htol16_buf(srom, nw * 2);
		if (hndcrc8((uint8 *)srom, nw * 2, CRC8_INIT_VALUE) != CRC8_GOOD_VALUE) {
			BS_ERROR(("%s: bad crc\n", __FUNCTION__));
			err = -5;
		}
		/* now correct the endianness of the byte array */
		ltoh16_buf(srom, nw * 2);
	}

out:
	return err;
}

#if defined(WLTEST) || defined(DHD_SPROM) || defined(BCMDBG)
/*
 * Write the srom for the pcmcia-srom over si case.
 * Return 0 on success, nonzero on error.
 */
static int
set_si_pcmcia_srom(si_t *sih, osl_t *osh, uint8 *pcmregs,
                   uint boff, uint16 *srom, uint bsz)
{
	uint i, nw, woff, wsz;
	uint16 word;
	uint8 crc;
	int err = 0;

	/* write must be at word boundary */
	ASSERT((boff & 1) == 0 && (bsz & 1) == 0);

	/* read sprom size and validate the parms */
	if ((nw = srom_size(sih, osh)) == 0) {
		BS_ERROR(("set_si_pcmcia_srom: sprom size unknown\n"));
		err = -1;
		goto out;
	}
	if (boff + bsz > 2 * nw) {
		BS_ERROR(("set_si_pcmcia_srom: sprom size exceeded\n"));
		err = -2;
		goto out;
	}

	/* enable write */
	if (srom_cmd_si_pcmcia(osh, pcmregs, SROM_WEN)) {
		BS_ERROR(("set_si_pcmcia_srom: sprom wen failed\n"));
		err = -3;
		goto out;
	}

	/* write buffer to sprom */
	for (woff = boff / 2, wsz = bsz / 2, i = 0;
	     woff < nw && i < wsz; woff ++, i ++) {
		if (srom_write_si_pcmcia(osh, pcmregs, (uint16)woff, srom[i])) {
			BS_ERROR(("set_si_pcmcia_srom: sprom write failed\n"));
			err = -4;
			goto out;
		}
	}

	/* fix crc */
	crc = CRC8_INIT_VALUE;
	for (woff = 0; woff < nw; woff ++) {
		if (srom_read_si_pcmcia(osh, pcmregs, (uint16)woff, &word)) {
			BS_ERROR(("set_si_pcmcia_srom: sprom fix crc read failed\n"));
			err = -5;
			goto out;
		}
		word = htol16(word);
		crc = hndcrc8((uint8 *)&word, woff != nw - 1 ? 2 : 1, crc);
	}
	word = (~crc << 8) + (ltoh16(word) & 0xff);
	if (srom_write_si_pcmcia(osh, pcmregs, (uint16)(woff - 1), word)) {
		BS_ERROR(("set_si_pcmcia_srom: sprom fix crc write failed\n"));
		err = -6;
		goto out;
	}

	/* disable write */
	if (srom_cmd_si_pcmcia(osh, pcmregs, SROM_WDS)) {
		BS_ERROR(("set_si_pcmcia_srom: sprom wds failed\n"));
		err = -7;
		goto out;
	}

out:
	return err;
}
#endif 
#endif 

/**
 * These 'vstr_*' definitions are used to convert from CIS format to a 'NVRAM var=val' format, the
 * NVRAM format is used throughout the rest of the firmware.
 */
#if !defined(BCMDONGLEHOST)
static const char BCMATTACHDATA(vstr_manf)[] = "manf=%s";
static const char BCMATTACHDATA(vstr_productname)[] = "productname=%s";
static const char BCMATTACHDATA(vstr_manfid)[] = "manfid=0x%x";
static const char BCMATTACHDATA(vstr_prodid)[] = "prodid=0x%x";
static const char BCMATTACHDATA(vstr_regwindowsz)[] = "regwindowsz=%d";
static const char BCMATTACHDATA(vstr_sromrev)[] = "sromrev=%d";
static const char BCMATTACHDATA(vstr_chiprev)[] = "chiprev=%d";
static const char BCMATTACHDATA(vstr_subvendid)[] = "subvendid=0x%x";
static const char BCMATTACHDATA(vstr_subdevid)[] = "subdevid=0x%x";
static const char BCMATTACHDATA(vstr_boardrev)[] = "boardrev=0x%x";
static const char BCMATTACHDATA(vstr_aa2g)[] = "aa2g=0x%x";
static const char BCMATTACHDATA(vstr_aa5g)[] = "aa5g=0x%x";
static const char BCMATTACHDATA(vstr_ag)[] = "ag%d=0x%x";
static const char BCMATTACHDATA(vstr_cc)[] = "cc=%d";
static const char BCMATTACHDATA(vstr_opo)[] = "opo=%d";
static const char BCMATTACHDATA(vstr_pa0b)[][9] = { "pa0b0=%d", "pa0b1=%d", "pa0b2=%d" };
static const char BCMATTACHDATA(vstr_pa0b_lo)[][12] =
	{ "pa0b0_lo=%d", "pa0b1_lo=%d", "pa0b2_lo=%d" };
static const char BCMATTACHDATA(vstr_pa0itssit)[] = "pa0itssit=%d";
static const char BCMATTACHDATA(vstr_pa0maxpwr)[] = "pa0maxpwr=%d";
static const char BCMATTACHDATA(vstr_pa1b)[][9] = { "pa1b0=%d", "pa1b1=%d", "pa1b2=%d" };
static const char BCMATTACHDATA(vstr_pa1lob)[][11] =
	{ "pa1lob0=%d", "pa1lob1=%d", "pa1lob2=%d" };
static const char BCMATTACHDATA(vstr_pa1hib)[][11] =
	{ "pa1hib0=%d", "pa1hib1=%d", "pa1hib2=%d" };
static const char BCMATTACHDATA(vstr_pa1itssit)[] = "pa1itssit=%d";
static const char BCMATTACHDATA(vstr_pa1maxpwr)[] = "pa1maxpwr=%d";
static const char BCMATTACHDATA(vstr_pa1lomaxpwr)[] = "pa1lomaxpwr=%d";
static const char BCMATTACHDATA(vstr_pa1himaxpwr)[] = "pa1himaxpwr=%d";
static const char BCMATTACHDATA(vstr_oem)[] = "oem=%02x%02x%02x%02x%02x%02x%02x%02x";
static const char BCMATTACHDATA(vstr_boardflags)[] = "boardflags=0x%x";
static const char BCMATTACHDATA(vstr_boardflags2)[] = "boardflags2=0x%x";
static const char BCMATTACHDATA(vstr_boardflags3)[] = "boardflags3=0x%x";
static const char BCMATTACHDATA(vstr_ledbh)[] = "ledbh%d=0x%x";
static const char BCMATTACHDATA(vstr_noccode)[] = "ccode=0x0";
static const char BCMATTACHDATA(vstr_ccode)[] = "ccode=%c%c";
static const char BCMATTACHDATA(vstr_cctl)[] = "cctl=0x%x";
static const char BCMATTACHDATA(vstr_cckpo)[] = "cckpo=0x%x";
static const char BCMATTACHDATA(vstr_ofdmpo)[] = "ofdmpo=0x%x";
static const char BCMATTACHDATA(vstr_rdlid)[] = "rdlid=0x%x";
static const char BCMATTACHDATA(vstr_rdlrndis)[] = "rdlrndis=%d";
static const char BCMATTACHDATA(vstr_rdlrwu)[] = "rdlrwu=%d";
static const char BCMATTACHDATA(vstr_usbfs)[] = "usbfs=%d";
static const char BCMATTACHDATA(vstr_wpsgpio)[] = "wpsgpio=%d";
static const char BCMATTACHDATA(vstr_wpsled)[] = "wpsled=%d";
static const char BCMATTACHDATA(vstr_rdlsn)[] = "rdlsn=%d";
static const char BCMATTACHDATA(vstr_rssismf2g)[] = "rssismf2g=%d";
static const char BCMATTACHDATA(vstr_rssismc2g)[] = "rssismc2g=%d";
static const char BCMATTACHDATA(vstr_rssisav2g)[] = "rssisav2g=%d";
static const char BCMATTACHDATA(vstr_bxa2g)[] = "bxa2g=%d";
static const char BCMATTACHDATA(vstr_rssismf5g)[] = "rssismf5g=%d";
static const char BCMATTACHDATA(vstr_rssismc5g)[] = "rssismc5g=%d";
static const char BCMATTACHDATA(vstr_rssisav5g)[] = "rssisav5g=%d";
static const char BCMATTACHDATA(vstr_bxa5g)[] = "bxa5g=%d";
static const char BCMATTACHDATA(vstr_tri2g)[] = "tri2g=%d";
static const char BCMATTACHDATA(vstr_tri5gl)[] = "tri5gl=%d";
static const char BCMATTACHDATA(vstr_tri5g)[] = "tri5g=%d";
static const char BCMATTACHDATA(vstr_tri5gh)[] = "tri5gh=%d";
static const char BCMATTACHDATA(vstr_rxpo2g)[] = "rxpo2g=%d";
static const char BCMATTACHDATA(vstr_rxpo5g)[] = "rxpo5g=%d";
static const char BCMATTACHDATA(vstr_boardtype)[] = "boardtype=0x%x";
static const char BCMATTACHDATA(vstr_leddc)[] = "leddc=0x%04x";
static const char BCMATTACHDATA(vstr_vendid)[] = "vendid=0x%x";
static const char BCMATTACHDATA(vstr_devid)[] = "devid=0x%x";
static const char BCMATTACHDATA(vstr_xtalfreq)[] = "xtalfreq=%d";
static const char BCMATTACHDATA(vstr_txchain)[] = "txchain=0x%x";
static const char BCMATTACHDATA(vstr_rxchain)[] = "rxchain=0x%x";
static const char BCMATTACHDATA(vstr_elna2g)[] = "elna2g=0x%x";
static const char BCMATTACHDATA(vstr_elna5g)[] = "elna5g=0x%x";
static const char BCMATTACHDATA(vstr_antswitch)[] = "antswitch=0x%x";
static const char BCMATTACHDATA(vstr_regrev)[] = "regrev=0x%x";
static const char BCMATTACHDATA(vstr_antswctl2g)[] = "antswctl2g=0x%x";
static const char BCMATTACHDATA(vstr_triso2g)[] = "triso2g=0x%x";
static const char BCMATTACHDATA(vstr_pdetrange2g)[] = "pdetrange2g=0x%x";
static const char BCMATTACHDATA(vstr_extpagain2g)[] = "extpagain2g=0x%x";
static const char BCMATTACHDATA(vstr_tssipos2g)[] = "tssipos2g=0x%x";
static const char BCMATTACHDATA(vstr_antswctl5g)[] = "antswctl5g=0x%x";
static const char BCMATTACHDATA(vstr_triso5g)[] = "triso5g=0x%x";
static const char BCMATTACHDATA(vstr_pdetrange5g)[] = "pdetrange5g=0x%x";
static const char BCMATTACHDATA(vstr_extpagain5g)[] = "extpagain5g=0x%x";
static const char BCMATTACHDATA(vstr_tssipos5g)[] = "tssipos5g=0x%x";
static const char BCMATTACHDATA(vstr_maxp2ga)[] = "maxp2ga%d=0x%x";
static const char BCMATTACHDATA(vstr_itt2ga0)[] = "itt2ga0=0x%x";
static const char BCMATTACHDATA(vstr_pa)[] = "pa%dgw%da%d=0x%x";
static const char BCMATTACHDATA(vstr_pahl)[] = "pa%dg%cw%da%d=0x%x";
static const char BCMATTACHDATA(vstr_maxp5ga0)[] = "maxp5ga0=0x%x";
static const char BCMATTACHDATA(vstr_itt5ga0)[] = "itt5ga0=0x%x";
static const char BCMATTACHDATA(vstr_maxp5gha0)[] = "maxp5gha0=0x%x";
static const char BCMATTACHDATA(vstr_maxp5gla0)[] = "maxp5gla0=0x%x";
static const char BCMATTACHDATA(vstr_itt2ga1)[] = "itt2ga1=0x%x";
static const char BCMATTACHDATA(vstr_maxp5ga1)[] = "maxp5ga1=0x%x";
static const char BCMATTACHDATA(vstr_itt5ga1)[] = "itt5ga1=0x%x";
static const char BCMATTACHDATA(vstr_maxp5gha1)[] = "maxp5gha1=0x%x";
static const char BCMATTACHDATA(vstr_maxp5gla1)[] = "maxp5gla1=0x%x";
static const char BCMATTACHDATA(vstr_cck2gpo)[] = "cck2gpo=0x%x";
static const char BCMATTACHDATA(vstr_ofdm2gpo)[] = "ofdm2gpo=0x%x";
static const char BCMATTACHDATA(vstr_ofdm5gpo)[] = "ofdm5gpo=0x%x";
static const char BCMATTACHDATA(vstr_ofdm5glpo)[] = "ofdm5glpo=0x%x";
static const char BCMATTACHDATA(vstr_ofdm5ghpo)[] = "ofdm5ghpo=0x%x";
static const char BCMATTACHDATA(vstr_cddpo)[] = "cddpo=0x%x";
static const char BCMATTACHDATA(vstr_stbcpo)[] = "stbcpo=0x%x";
static const char BCMATTACHDATA(vstr_bw40po)[] = "bw40po=0x%x";
static const char BCMATTACHDATA(vstr_bwduppo)[] = "bwduppo=0x%x";
static const char BCMATTACHDATA(vstr_mcspo)[] = "mcs%dgpo%d=0x%x";
static const char BCMATTACHDATA(vstr_mcspohl)[] = "mcs%dg%cpo%d=0x%x";
static const char BCMATTACHDATA(vstr_custom)[] = "customvar%d=0x%x";
static const char BCMATTACHDATA(vstr_cckdigfilttype)[] = "cckdigfilttype=%d";
static const char BCMATTACHDATA(vstr_usbflags)[] = "usbflags=0x%x";
#ifdef BCM_BOOTLOADER
static const char BCMATTACHDATA(vstr_mdio)[] = "mdio%d=0x%%x";
static const char BCMATTACHDATA(vstr_mdioex)[] = "mdioex%d=0x%%x";
static const char BCMATTACHDATA(vstr_brmin)[] = "brmin=0x%x";
static const char BCMATTACHDATA(vstr_brmax)[] = "brmax=0x%x";
static const char BCMATTACHDATA(vstr_pllreg)[] = "pll%d=0x%x";
static const char BCMATTACHDATA(vstr_ccreg)[] = "chipc%d=0x%x";
static const char BCMATTACHDATA(vstr_regctrl)[] = "reg%d=0x%x";
static const char BCMATTACHDATA(vstr_time)[] = "r%dt=0x%x";
static const char BCMATTACHDATA(vstr_depreg)[] = "r%dd=0x%x";
static const char BCMATTACHDATA(vstr_usbpredly)[] = "usbpredly=0x%x";
static const char BCMATTACHDATA(vstr_usbpostdly)[] = "usbpostdly=0x%x";
static const char BCMATTACHDATA(vstr_usbrdy)[] = "usbrdy=0x%x";
static const char BCMATTACHDATA(vstr_hsicphyctrl1)[] = "hsicphyctrl1=0x%x";
static const char BCMATTACHDATA(vstr_hsicphyctrl2)[] = "hsicphyctrl2=0x%x";
static const char BCMATTACHDATA(vstr_usbdevctrl)[] = "usbdevctrl=0x%x";
static const char BCMATTACHDATA(vstr_bldr_reset_timeout)[] = "bldr_to=0x%x";
static const char BCMATTACHDATA(vstr_muxenab)[] = "muxenab=0x%x";
static const char BCMATTACHDATA(vstr_pubkey)[] = "pubkey=%s";
#endif /* BCM_BOOTLOADER */
static const char BCMATTACHDATA(vstr_boardnum)[] = "boardnum=%d";
static const char BCMATTACHDATA(vstr_macaddr)[] = "macaddr=%s";
static const char BCMATTACHDATA(vstr_usbepnum)[] = "usbepnum=0x%x";
#ifdef BCMUSBDEV_COMPOSITE
static const char BCMATTACHDATA(vstr_usbdesc_composite)[] = "usbdesc_composite=0x%x";
#endif /* BCMUSBDEV_COMPOSITE */
static const char BCMATTACHDATA(vstr_usbutmi_ctl)[] = "usbutmi_ctl=0x%x";
static const char BCMATTACHDATA(vstr_usbssphy_utmi_ctl0)[] = "usbssphy_utmi_ctl0=0x%x";
static const char BCMATTACHDATA(vstr_usbssphy_utmi_ctl1)[] = "usbssphy_utmi_ctl1=0x%x";
static const char BCMATTACHDATA(vstr_usbssphy_utmi_ctl2)[] = "usbssphy_utmi_ctl2=0x%x";
static const char BCMATTACHDATA(vstr_usbssphy_sleep0)[] = "usbssphy_sleep0=0x%x";
static const char BCMATTACHDATA(vstr_usbssphy_sleep1)[] = "usbssphy_sleep1=0x%x";
static const char BCMATTACHDATA(vstr_usbssphy_sleep2)[] = "usbssphy_sleep2=0x%x";
static const char BCMATTACHDATA(vstr_usbssphy_sleep3)[] = "usbssphy_sleep3=0x%x";
static const char BCMATTACHDATA(vstr_usbssphy_mdio)[] = "usbssmdio%d=0x%x,0x%x,0x%x,0x%x";
static const char BCMATTACHDATA(vstr_usb30phy_noss)[] = "usbnoss=0x%x";
static const char BCMATTACHDATA(vstr_usb30phy_u1u2)[] = "usb30u1u2=0x%x";
static const char BCMATTACHDATA(vstr_usb30phy_regs)[] = "usb30regs%d=0x%x,0x%x,0x%x,0x%x";

/* Power per rate for SROM V9 */
static const char BCMATTACHDATA(vstr_cckbw202gpo)[][19] =
	{ "cckbw202gpo=0x%x", "cckbw20ul2gpo=0x%x" };
static const char BCMATTACHDATA(vstr_legofdmbw202gpo)[][23] =
	{ "legofdmbw202gpo=0x%x", "legofdmbw20ul2gpo=0x%x" };
static const char BCMATTACHDATA(vstr_legofdmbw205gpo)[][24] =
	{ "legofdmbw205glpo=0x%x", "legofdmbw20ul5glpo=0x%x",
	"legofdmbw205gmpo=0x%x", "legofdmbw20ul5gmpo=0x%x",
	"legofdmbw205ghpo=0x%x", "legofdmbw20ul5ghpo=0x%x" };

static const char BCMATTACHDATA(vstr_mcs2gpo)[][19] =
{ "mcsbw202gpo=0x%x", "mcsbw20ul2gpo=0x%x", "mcsbw402gpo=0x%x"};

static const char BCMATTACHDATA(vstr_mcs5glpo)[][20] =
	{ "mcsbw205glpo=0x%x", "mcsbw20ul5glpo=0x%x", "mcsbw405glpo=0x%x"};

static const char BCMATTACHDATA(vstr_mcs5gmpo)[][20] =
	{ "mcsbw205gmpo=0x%x", "mcsbw20ul5gmpo=0x%x", "mcsbw405gmpo=0x%x"};

static const char BCMATTACHDATA(vstr_mcs5ghpo)[][20] =
	{ "mcsbw205ghpo=0x%x", "mcsbw20ul5ghpo=0x%x", "mcsbw405ghpo=0x%x"};

static const char BCMATTACHDATA(vstr_mcs32po)[] = "mcs32po=0x%x";
static const char BCMATTACHDATA(vstr_legofdm40duppo)[] = "legofdm40duppo=0x%x";

/* SROM V11 */
static const char BCMATTACHDATA(vstr_tempthresh)[] = "tempthresh=%d";	/* HNBU_TEMPTHRESH */
static const char BCMATTACHDATA(vstr_temps_period)[] = "temps_period=%d";
static const char BCMATTACHDATA(vstr_temps_hysteresis)[] = "temps_hysteresis=%d";
static const char BCMATTACHDATA(vstr_tempoffset)[] = "tempoffset=%d";
static const char BCMATTACHDATA(vstr_tempsense_slope)[] = "tempsense_slope=%d";
static const char BCMATTACHDATA(vstr_temp_corrx)[] = "tempcorrx=%d";
static const char BCMATTACHDATA(vstr_tempsense_option)[] = "tempsense_option=%d";
static const char BCMATTACHDATA(vstr_phycal_tempdelta)[] = "phycal_tempdelta=%d";
static const char BCMATTACHDATA(vstr_tssiposslopeg)[] = "tssiposslope%dg=%d";	/* HNBU_FEM_CFG */
static const char BCMATTACHDATA(vstr_epagaing)[] = "epagain%dg=%d";
static const char BCMATTACHDATA(vstr_pdgaing)[] = "pdgain%dg=%d";
static const char BCMATTACHDATA(vstr_tworangetssi)[] = "tworangetssi%dg=%d";
static const char BCMATTACHDATA(vstr_papdcap)[] = "papdcap%dg=%d";
static const char BCMATTACHDATA(vstr_femctrl)[] = "femctrl=%d";
static const char BCMATTACHDATA(vstr_gainctrlsph)[] = "gainctrlsph=%d";
static const char BCMATTACHDATA(vstr_subband5gver)[] = "subband5gver=%d";	/* HNBU_ACPA_CX */
static const char BCMATTACHDATA(vstr_pa2ga)[] = "pa2ga%d=0x%x,0x%x,0x%x";
static const char BCMATTACHDATA(vstr_maxp5ga)[] = "maxp5ga%d=0x%x,0x%x,0x%x,0x%x";
static const char BCMATTACHDATA(vstr_pa5ga)[] = "pa5ga%d=0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,"
	"0x%x,0x%x,0x%x,0x%x,0x%x,0x%x";
static const char BCMATTACHDATA(vstr_pa2gccka)[] = "pa2gccka%d=0x%x,0x%x,0x%x";
static const char BCMATTACHDATA(vstr_pa5gbw40a)[] = "pa5gbw40a%d=0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,"
	"0x%x,0x%x,0x%x,0x%x,0x%x,0x%x";
static const char BCMATTACHDATA(vstr_pa5gbw80a)[] = "pa5gbw80a%d=0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,"
	"0x%x,0x%x,0x%x,0x%x,0x%x,0x%x";
static const char BCMATTACHDATA(vstr_pa5gbw4080a)[] = "pa5gbw4080a%d=0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,"
	"0x%x,0x%x,0x%x,0x%x,0x%x,0x%x";
static const char BCMATTACHDATA(vstr_rxgainsgelnagaina)[] = "rxgains%dgelnagaina%d=%d";
static const char BCMATTACHDATA(vstr_rxgainsgtrisoa)[] = "rxgains%dgtrisoa%d=%d";
static const char BCMATTACHDATA(vstr_rxgainsgtrelnabypa)[] = "rxgains%dgtrelnabypa%d=%d";
static const char BCMATTACHDATA(vstr_rxgainsgxelnagaina)[] = "rxgains%dg%celnagaina%d=%d";
static const char BCMATTACHDATA(vstr_rxgainsgxtrisoa)[] = "rxgains%dg%ctrisoa%d=%d";
static const char BCMATTACHDATA(vstr_rxgainsgxtrelnabypa)[] = "rxgains%dg%ctrelnabypa%d=%d";
static const char BCMATTACHDATA(vstr_measpower)[] = "measpower=0x%x";	/* HNBU_MEAS_PWR */
static const char BCMATTACHDATA(vstr_measpowerX)[] = "measpower%d=0x%x";
static const char BCMATTACHDATA(vstr_pdoffsetma)[] = "pdoffset%dma%d=0x%x";	/* HNBU_PDOFF */
static const char BCMATTACHDATA(vstr_pdoffset2gma)[] = "pdoffset2g%dma%d=0x%x";	/* HNBU_PDOFF_2G */
static const char BCMATTACHDATA(vstr_pdoffset2gmvalid)[] = "pdoffset2g%dmvalid=0x%x";
static const char BCMATTACHDATA(vstr_rawtempsense)[] = "rawtempsense=0x%x";
/* HNBU_ACPPR_2GPO */
static const char BCMATTACHDATA(vstr_dot11agofdmhrbw202gpo)[] = "dot11agofdmhrbw202gpo=0x%x";
static const char BCMATTACHDATA(vstr_ofdmlrbw202gpo)[] = "ofdmlrbw202gpo=0x%x";
static const char BCMATTACHDATA(vstr_mcsbw805gpo)[] = "mcsbw805g%cpo=0x%x"; /* HNBU_ACPPR_5GPO */
static const char BCMATTACHDATA(vstr_mcsbw1605gpo)[] = "mcsbw1605g%cpo=0x%x";
static const char BCMATTACHDATA(vstr_mcslr5gpo)[] = "mcslr5g%cpo=0x%x";
static const char BCMATTACHDATA(vstr_sb20in40rpo)[] = "sb20in40%crpo=0x%x"; /* HNBU_ACPPR_SBPO */
static const char BCMATTACHDATA(vstr_sb20in80and160r5gpo)[] = "sb20in80and160%cr5g%cpo=0x%x";
static const char BCMATTACHDATA(vstr_sb40and80r5gpo)[] = "sb40and80%cr5g%cpo=0x%x";
static const char BCMATTACHDATA(vstr_dot11agduprpo)[] = "dot11agdup%crpo=0x%x";
static const char BCMATTACHDATA(vstr_noiselvl2ga)[] = "noiselvl2ga%d=%d";	/* HNBU_NOISELVL */
static const char BCMATTACHDATA(vstr_noiselvl5ga)[] = "noiselvl5ga%d=%d,%d,%d,%d";
static const char BCMATTACHDATA(vstr_rxgainerr2ga)[] = "rxgainerr2ga%d=0x%x"; /* HNBU_RXGAIN_ERR */
static const char BCMATTACHDATA(vstr_rxgainerr5ga)[] = "rxgainerr5ga%d=0x%x,0x%x,0x%x,0x%x";
static const char BCMATTACHDATA(vstr_agbg)[] = "agbg%d=0x%x";	/* HNBU_AGBGA */
static const char BCMATTACHDATA(vstr_aga)[] = "aga%d=0x%x";
static const char BCMATTACHDATA(vstr_txduty_ofdm)[] = "tx_duty_cycle_ofdm_%d_5g=%d";
static const char BCMATTACHDATA(vstr_txduty_thresh)[] = "tx_duty_cycle_thresh_%d_5g=%d";
static const char BCMATTACHDATA(vstr_paparambwver)[] = "paparambwver=%d";
static const char BCMATTACHDATA(vstr_usb30u1u2)[] = "usb30u1u2=0x%x";
static const char BCMATTACHDATA(vstr_usb30regs0)[] = "usb30regs0=0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,"
							"0x%x,0x%x,0x%x,0x%x";
static const char BCMATTACHDATA(vstr_usb30regs1)[] = "usb30regs1=0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,"
							"0x%x,0x%x,0x%x,0x%x";

static const char BCMATTACHDATA(vstr_uuid)[] = "uuid=%s";

static const char BCMATTACHDATA(vstr_wowlgpio)[] = "wowl_gpio=%d";
static const char BCMATTACHDATA(vstr_wowlgpiopol)[] = "wowl_gpiopol=%d";

static const char BCMATTACHDATA(rstr_ag0)[] = "ag0";
static const char BCMATTACHDATA(vstr_paparamrpcalvars)[][20] =
	{"rpcal2g=0x%x", "rpcal5gb0=0x%x", "rpcal5gb1=0x%x",
	"rpcal5gb2=0x%x", "rpcal5gb3=0x%x"};

static const char BCMATTACHDATA(vstr_gpdn)[] = "gpdn=0x%x";

static const char BCMATTACHDATA(vstr_end)[] = "END\0";
uint8 patch_pair = 0;

/* For dongle HW, accept partial calibration parameters */
#if defined(BCMUSBDEV) || defined(BCMDONGLEHOST)
#define BCMDONGLECASE(n) case n:
#else
#define BCMDONGLECASE(n)
#endif

#ifdef BCM_BOOTLOADER
/* The format of the PMUREGS OTP Tuple ->
 * 1 byte -> Lower 5 bits has the address of the register
 *                 Higher 3 bits has the mode of the register like
 *                 PLL, ChipCtrl, RegCtrl, UpDwn or Dependency mask
 * 4 bytes -> Value of the register to be updated.
 */
#define PMUREGS_MODE_MASK	0xE0
#define PMUREGS_MODE_SHIFT	5
#define PMUREGS_ADDR_MASK	0x1F
#define PMUREGS_TPL_SIZE	5

enum {
	PMU_PLLREG_MODE,
	PMU_CCREG_MODE,
	PMU_VOLTREG_MODE,
	PMU_RES_TIME_MODE,
	PMU_RESDEPEND_MODE
};

#define USBREGS_TPL_SIZE	5
enum {
	USB_DEV_CTRL_REG,
	HSIC_PHY_CTRL1_REG,
	HSIC_PHY_CTRL2_REG
};

#define USBRDY_DLY_TYPE	0x8000	/* Bit indicating if the byte is pre or post delay value */
#define USBRDY_DLY_MASK	0x7FFF	/* Bits indicating the amount of delay */
#define USBRDY_MAXOTP_SIZE	5	/* Max size of the OTP parameter */

#endif /* BCM_BOOTLOADER */

#ifdef BCM_BMAC_VARS_APPEND
int
BCMATTACHFN(srom_probe_boardtype)(uint8 *pcis[], uint ciscnt)
{
	int i;
	uint cisnum;
	uint8 *cis, tup, tlen;

	for (cisnum = 0; cisnum < ciscnt; cisnum++) {
		cis = *pcis++;
		i = 0;
		do {
			tup = cis[i++];
			if (tup == CISTPL_NULL || tup == CISTPL_END)
				tlen = 0;
			else
				tlen = cis[i++];

			if ((i + tlen) >= CIS_SIZE)
				break;

			if ((tup == CISTPL_BRCM_HNBU) && (cis[i] == HNBU_BOARDTYPE)) {
				return (int)((cis[i + 2] << 8) + cis[i + 1]);
			}

			i += tlen;

		} while (tup != CISTPL_END);
	}

	return 0;
}
#endif /* BCM_BMAC_VARS_APPEND */

/**
 * Both SROM and OTP contain variables in 'CIS' format, whereas the rest of the firmware works with
 * 'variable/value' string pairs.
 */
int
BCMATTACHFN(srom_parsecis)(osl_t *osh, uint8 *pcis[], uint ciscnt, char **vars, uint *count)
{
	char eabuf[32];
	char *base;
	varbuf_t b;
	uint8 *cis, tup, tlen, sromrev = 1;
	int i, j;
#ifndef BCM_BOOTLOADER
	bool ag_init = FALSE;
#endif
	uint32 w32;
	uint funcid;
	uint cisnum;
	int32 boardnum;
	int err;
	bool standard_cis;

	ASSERT(count != NULL);

	if (vars == NULL) {
		ASSERT(0);	/* crash debug images for investigation */
		return BCME_BADARG;
	}

	boardnum = -1;

	base = MALLOC(osh, MAXSZ_NVRAM_VARS);
	ASSERT(base != NULL);
	if (!base)
		return -2;

	varbuf_init(&b, base, MAXSZ_NVRAM_VARS);
	bzero(base, MAXSZ_NVRAM_VARS);
#ifdef BCM_BMAC_VARS_APPEND
	/* 43236 use defaultsromvars_43236usb as the base,
	 * then append and update it with the content from OTP.
	 * Only revision/board specfic content or updates used to override
	 * the driver default will be stored in OTP
	 */
	*count -= (strlen(vstr_end) + 1 + 1); /* back off the termnating END\0\0 from fakenvram */
	bcopy(*vars, base, *count);
	b.buf += *count;
#else
	/* Append from vars if there's already something inside */
	if (*vars && **vars && (*count >= 3)) {
		/* back off \0 at the end, leaving only one \0 for the last param */
		while (((*vars)[(*count)-1] == '\0') && ((*vars)[(*count)-2] == '\0'))
			(*count)--;

		bcopy(*vars, base, *count);
		b.buf += *count;
	}
#endif /* BCM_BMAC_VARS_APPEND */
	eabuf[0] = '\0';
	for (cisnum = 0; cisnum < ciscnt; cisnum++) {
		cis = *pcis++;
		i = 0;
		funcid = 0;
		standard_cis = TRUE;
		do {
			if (standard_cis) {
				tup = cis[i++];
				if (tup == CISTPL_NULL || tup == CISTPL_END)
					tlen = 0;
				else
					tlen = cis[i++];
			} else {
				if (cis[i] == CISTPL_NULL || cis[i] == CISTPL_END) {
					tlen = 0;
					tup = cis[i];
				} else {
					tlen = cis[i];
					tup = CISTPL_BRCM_HNBU;
				}
				++i;
			}
			if ((i + tlen) >= CIS_SIZE)
				break;

			switch (tup) {
			case CISTPL_VERS_1:
				/* assume the strings are good if the version field checks out */
				if (((cis[i + 1] << 8) + cis[i]) >= 0x0008) {
					varbuf_append(&b, vstr_manf, &cis[i + 2]);
					varbuf_append(&b, vstr_productname,
					              &cis[i + 3 + strlen((char *)&cis[i + 2])]);
					break;
				}

			case CISTPL_MANFID:
				varbuf_append(&b, vstr_manfid, (cis[i + 1] << 8) + cis[i]);
				varbuf_append(&b, vstr_prodid, (cis[i + 3] << 8) + cis[i + 2]);
				break;

			case CISTPL_FUNCID:
				funcid = cis[i];
				break;

			case CISTPL_FUNCE:
				switch (funcid) {
				case CISTPL_FID_SDIO:
					funcid = 0;
					break;
				default:
					/* set macaddr if HNBU_MACADDR not seen yet */
					if (eabuf[0] == '\0' && cis[i] == LAN_NID &&
						!(ETHER_ISNULLADDR(&cis[i + 2])) &&
						!(ETHER_ISMULTI(&cis[i + 2]))) {
						ASSERT(cis[i + 1] == ETHER_ADDR_LEN);
						bcm_ether_ntoa((struct ether_addr *)&cis[i + 2],
						               eabuf);

						/* set boardnum if HNBU_BOARDNUM not seen yet */
						if (boardnum == -1)
							boardnum = (cis[i + 6] << 8) + cis[i + 7];
					}
					break;
				}
				break;

			case CISTPL_CFTABLE:
				varbuf_append(&b, vstr_regwindowsz, (cis[i + 7] << 8) | cis[i + 6]);
				break;

			case CISTPL_BRCM_HNBU:
				switch (cis[i]) {
				case HNBU_SROMREV:
					sromrev = cis[i + 1];
					varbuf_append(&b, vstr_sromrev, sromrev);
					break;

				case HNBU_XTALFREQ:
					varbuf_append(&b, vstr_xtalfreq,
					              (cis[i + 4] << 24) |
					              (cis[i + 3] << 16) |
					              (cis[i + 2] << 8) |
					              cis[i + 1]);
					break;

				case HNBU_CHIPID:
					varbuf_append(&b, vstr_vendid, (cis[i + 2] << 8) +
					              cis[i + 1]);
					varbuf_append(&b, vstr_devid, (cis[i + 4] << 8) +
					              cis[i + 3]);
					if (tlen >= 7) {
						varbuf_append(&b, vstr_chiprev,
						              (cis[i + 6] << 8) + cis[i + 5]);
					}
					if (tlen >= 9) {
						varbuf_append(&b, vstr_subvendid,
						              (cis[i + 8] << 8) + cis[i + 7]);
					}
					if (tlen >= 11) {
						varbuf_append(&b, vstr_subdevid,
						              (cis[i + 10] << 8) + cis[i + 9]);
						/* subdevid doubles for boardtype */
						varbuf_append(&b, vstr_boardtype,
						              (cis[i + 10] << 8) + cis[i + 9]);
					}
					break;

				case HNBU_BOARDNUM:
					boardnum = (cis[i + 2] << 8) + cis[i + 1];
					break;

				case HNBU_PATCH:
					{
						char vstr_paddr[16];
						char vstr_pdata[16];

						/* retrieve the patch pairs
						 * from tlen/6; where 6 is
						 * sizeof(patch addr(2)) +
						 * sizeof(patch data(4)).
						 */
						patch_pair = tlen/6;

						for (j = 0; j < patch_pair; j++) {
							snprintf(vstr_paddr, sizeof(vstr_paddr),
								"pa%d=0x%%x", j);
							snprintf(vstr_pdata, sizeof(vstr_pdata),
								"pd%d=0x%%x", j);

							varbuf_append(&b, vstr_paddr,
								(cis[i + (j*6) + 2] << 8) |
								cis[i + (j*6) + 1]);

							varbuf_append(&b, vstr_pdata,
								(cis[i + (j*6) + 6] << 24) |
								(cis[i + (j*6) + 5] << 16) |
								(cis[i + (j*6) + 4] << 8) |
								cis[i + (j*6) + 3]);
						}
					}
					break;

				case HNBU_BOARDREV:
					if (tlen == 2)
						varbuf_append(&b, vstr_boardrev, cis[i + 1]);
					else
						varbuf_append(&b, vstr_boardrev,
							(cis[i + 2] << 8) + cis[i + 1]);
					break;

				case HNBU_BOARDFLAGS:
					w32 = (cis[i + 2] << 8) + cis[i + 1];
					if (tlen >= 5)
						w32 |= ((cis[i + 4] << 24) + (cis[i + 3] << 16));
					varbuf_append(&b, vstr_boardflags, w32);

					if (tlen >= 7) {
						w32 = (cis[i + 6] << 8) + cis[i + 5];
						if (tlen >= 9)
							w32 |= ((cis[i + 8] << 24) +
								(cis[i + 7] << 16));
						varbuf_append(&b, vstr_boardflags2, w32);
					}
					if (tlen >= 11) {
						w32 = (cis[i + 10] << 8) + cis[i + 9];
						if (tlen >= 13)
							w32 |= ((cis[i + 12] << 24) +
								(cis[i + 11] << 16));
						varbuf_append(&b, vstr_boardflags3, w32);
					}
					break;

				case HNBU_USBFS:
					varbuf_append(&b, vstr_usbfs, cis[i + 1]);
					break;

				case HNBU_BOARDTYPE:
					varbuf_append(&b, vstr_boardtype,
					              (cis[i + 2] << 8) + cis[i + 1]);
					break;

				case HNBU_HNBUCIS:
					/*
					 * what follows is a nonstandard HNBU CIS
					 * that lacks CISTPL_BRCM_HNBU tags
					 *
					 * skip 0xff (end of standard CIS)
					 * after this tuple
					 */
					tlen++;
					standard_cis = FALSE;
					break;

				case HNBU_USBEPNUM:
					varbuf_append(&b, vstr_usbepnum,
						(cis[i + 2] << 8) | cis[i + 1]);
					break;

				case HNBU_PATCH_AUTOINC: {
						char vstr_paddr[16];
						char vstr_pdata[16];
						uint32 addr_inc;
						uint8 pcnt;

						addr_inc = (cis[i + 4] << 24) |
							(cis[i + 3] << 16) |
							(cis[i + 2] << 8) |
							(cis[i + 1]);

						pcnt = (tlen - 5)/4;
						for (j = 0; j < pcnt; j++) {
							snprintf(vstr_paddr, sizeof(vstr_paddr),
								"pa%d=0x%%x", j + patch_pair);
							snprintf(vstr_pdata, sizeof(vstr_pdata),
								"pd%d=0x%%x", j + patch_pair);

							varbuf_append(&b, vstr_paddr, addr_inc);
							varbuf_append(&b, vstr_pdata,
								(cis[i + (j*4) + 8] << 24) |
								(cis[i + (j*4) + 7] << 16) |
								(cis[i + (j*4) + 6] << 8) |
								cis[i + (j*4) + 5]);
							addr_inc += 4;
						}
						patch_pair += pcnt;
					}
					break;
				case HNBU_PATCH2:
					{
						char vstr_paddr[16];
						char vstr_pdata[16];

						/* retrieve the patch pairs
						 * from tlen/8; where 8 is
						 * sizeof(patch addr(4)) +
						 * sizeof(patch data(4)).
						 */
						patch_pair = tlen/8;

						for (j = 0; j < patch_pair; j++) {
							snprintf(vstr_paddr, sizeof(vstr_paddr),
								"pa%d=0x%%x", j);
							snprintf(vstr_pdata, sizeof(vstr_pdata),
								"pd%d=0x%%x", j);

							varbuf_append(&b, vstr_paddr,
								(cis[i + (j*8) + 4] << 24) |
								(cis[i + (j*8) + 3] << 16) |
								(cis[i + (j*8) + 2] << 8) |
								cis[i + (j*8) + 1]);

							varbuf_append(&b, vstr_pdata,
								(cis[i + (j*8) + 8] << 24) |
								(cis[i + (j*8) + 7] << 16) |
								(cis[i + (j*8) + 6] << 8) |
								cis[i + (j*8) + 5]);
						}
					}
					break;
				case HNBU_PATCH_AUTOINC8: {
						char vstr_paddr[16];
						char vstr_pdatah[16];
						char vstr_pdatal[16];
						uint32 addr_inc;
						uint8 pcnt;

						addr_inc = (cis[i + 4] << 24) |
							(cis[i + 3] << 16) |
							(cis[i + 2] << 8) |
							(cis[i + 1]);

						pcnt = (tlen - 5)/8;
						for (j = 0; j < pcnt; j++) {
							snprintf(vstr_paddr, sizeof(vstr_paddr),
								"pa%d=0x%%x", j + patch_pair);
							snprintf(vstr_pdatah, sizeof(vstr_pdatah),
								"pdh%d=0x%%x", j + patch_pair);
							snprintf(vstr_pdatal, sizeof(vstr_pdatal),
								"pdl%d=0x%%x", j + patch_pair);

							varbuf_append(&b, vstr_paddr, addr_inc);
							varbuf_append(&b, vstr_pdatal,
								(cis[i + (j*8) + 8] << 24) |
								(cis[i + (j*8) + 7] << 16) |
								(cis[i + (j*8) + 6] << 8) |
								cis[i + (j*8) + 5]);
							varbuf_append(&b, vstr_pdatah,
								(cis[i + (j*8) + 12] << 24) |
								(cis[i + (j*8) + 11] << 16) |
								(cis[i + (j*8) + 10] << 8) |
								cis[i + (j*8) + 9]);
							addr_inc += 8;
						}
						patch_pair += pcnt;
					}
					break;
				case HNBU_PATCH8:
					{
						char vstr_paddr[16];
						char vstr_pdatah[16];
						char vstr_pdatal[16];

						/* retrieve the patch pairs
						 * from tlen/8; where 8 is
						 * sizeof(patch addr(4)) +
						 * sizeof(patch data(4)).
						 */
						patch_pair = tlen/12;

						for (j = 0; j < patch_pair; j++) {
							snprintf(vstr_paddr, sizeof(vstr_paddr),
								"pa%d=0x%%x", j);
							snprintf(vstr_pdatah, sizeof(vstr_pdatah),
								"pdh%d=0x%%x", j);
							snprintf(vstr_pdatal, sizeof(vstr_pdatal),
								"pdl%d=0x%%x", j);

							varbuf_append(&b, vstr_paddr,
								(cis[i + (j*12) + 4] << 24) |
								(cis[i + (j*12) + 3] << 16) |
								(cis[i + (j*12) + 2] << 8) |
								cis[i + (j*12) + 1]);

							varbuf_append(&b, vstr_pdatal,
								(cis[i + (j*12) + 8] << 24) |
								(cis[i + (j*12) + 7] << 16) |
								(cis[i + (j*12) + 6] << 8) |
								cis[i + (j*12) + 5]);

							varbuf_append(&b, vstr_pdatah,
								(cis[i + (j*12) + 12] << 24) |
								(cis[i + (j*12) + 11] << 16) |
								(cis[i + (j*12) + 10] << 8) |
								cis[i + (j*12) + 9]);
						}
					}
					break;
				case HNBU_USBFLAGS:
					varbuf_append(&b, vstr_usbflags,
					              (cis[i + 4] << 24) |
					              (cis[i + 3] << 16) |
					              (cis[i + 2] << 8) |
					              cis[i + 1]);
					break;
#ifdef BCM_BOOTLOADER
				case HNBU_MDIOEX_REGLIST:
				case HNBU_MDIO_REGLIST: {
					/* Format: addr (8 bits) | val (16 bits) */
					const uint8 msize = 3;
					char mdiostr[24];
					const char *mdiodesc;
					uint8 *st;

					mdiodesc = (cis[i] == HNBU_MDIO_REGLIST) ?
						vstr_mdio : vstr_mdioex;

					ASSERT(((tlen - 1) % msize) == 0);

					st = &cis[i + 1]; /* start of reg list */
					for (j = 0; j < (tlen - 1); j += msize, st += msize) {
						snprintf(mdiostr, sizeof(mdiostr),
							mdiodesc, st[0]);
						varbuf_append(&b, mdiostr, (st[2] << 8) | st[1]);
					}
				}
					break;
				case HNBU_BRMIN:
					varbuf_append(&b, vstr_brmin,
					              (cis[i + 4] << 24) |
					              (cis[i + 3] << 16) |
					              (cis[i + 2] << 8) |
					              cis[i + 1]);
					break;

				case HNBU_BRMAX:
					varbuf_append(&b, vstr_brmax,
					              (cis[i + 4] << 24) |
					              (cis[i + 3] << 16) |
					              (cis[i + 2] << 8) |
					              cis[i + 1]);
					break;
#endif /* BCM_BOOTLOADER */

				case HNBU_RDLID:
					varbuf_append(&b, vstr_rdlid,
					              (cis[i + 2] << 8) | cis[i + 1]);
					break;

				case HNBU_GCI_CCR:
					{
						/* format:
						* |0x80 | 	<== brcm
						* |len|		<== variable, multiple of 5
						* |tup|		<== tupletype
						* |ccreg_ix0|<== ix of ccreg [1byte]
						* |ccreg_val0|<= corr value [4bytes]
						*	---
						* Multiple registers are possible. for eg: we
						*	can specify reg_ix3val3 and reg_ix5val5, etc
						*/
						char vstr_gci_ccreg_entry[16];
						int num_entries = 0;

						/* retrieve the index-value pairs
						 * from tlen/5; where 5 is
						 * sizeof(ccreg_ix(1)) +
						 * sizeof(ccreg_val(4)).
						 */
						num_entries = tlen/5;

						for (j = 0; j < num_entries; j++) {
							snprintf(vstr_gci_ccreg_entry,
								sizeof(vstr_gci_ccreg_entry),
								"gcr%d=0x%%x", cis[i + (j*5) + 1]);

							varbuf_append(&b, vstr_gci_ccreg_entry,
								(cis[i + (j*5) + 5] << 24) |
								(cis[i + (j*5) + 4] << 16) |
								(cis[i + (j*5) + 3] << 8) |
								cis[i + (j*5) + 2]);
						}
					}
					break;

#ifdef BCM_BOOTLOADER
				case HNBU_RDLRNDIS:
					varbuf_append(&b, vstr_rdlrndis, cis[i + 1]);
					break;

				case HNBU_RDLRWU:
					varbuf_append(&b, vstr_rdlrwu, cis[i + 1]);
					break;

				case HNBU_RDLSN:
					if (tlen >= 5)
						varbuf_append(&b, vstr_rdlsn,
						              (cis[i + 4] << 24) |
						              (cis[i + 3] << 16) |
						              (cis[i + 2] << 8) |
						              cis[i + 1]);
					else
						varbuf_append(&b, vstr_rdlsn,
						              (cis[i + 2] << 8) |
						              cis[i + 1]);
					break;

				case HNBU_PMUREGS:
					{
						uint8 offset = 1, mode_addr, mode, addr;
						const char *fmt;

						do {
							mode_addr = cis[i+offset];

							mode = (mode_addr & PMUREGS_MODE_MASK)
								>> PMUREGS_MODE_SHIFT;
							addr = mode_addr & PMUREGS_ADDR_MASK;

							switch (mode) {
								case PMU_PLLREG_MODE:
									fmt = vstr_pllreg;
									break;
								case PMU_CCREG_MODE:
									fmt = vstr_ccreg;
									break;
								case PMU_VOLTREG_MODE:
									fmt = vstr_regctrl;
									break;
								case PMU_RES_TIME_MODE:
									fmt = vstr_time;
									break;
								case PMU_RESDEPEND_MODE:
									fmt = vstr_depreg;
									break;
								default:
									fmt = NULL;
									break;
							}

							if (fmt != NULL) {
								varbuf_append(&b, fmt, addr,
								(cis[i + offset + 4] << 24) |
								(cis[i + offset + 3] << 16) |
								(cis[i + offset + 2] << 8) |
								cis[i + offset + 1]);
							}

							offset += PMUREGS_TPL_SIZE;
						} while (offset < tlen);
					}
					break;

				case HNBU_USBREGS:
					{
						uint8 offset = 1, usb_reg;
						const char *fmt;

						do {
							usb_reg = cis[i+offset];

							switch (usb_reg) {
								case USB_DEV_CTRL_REG:
									fmt = vstr_usbdevctrl;
									break;
								case HSIC_PHY_CTRL1_REG:
									fmt = vstr_hsicphyctrl1;
									break;
								case HSIC_PHY_CTRL2_REG:
									fmt = vstr_hsicphyctrl2;
									break;
								default:
									fmt = NULL;
									break;
							}

							if (fmt != NULL) {
								varbuf_append(&b, fmt,
								(cis[i + offset + 4] << 24) |
								(cis[i + offset + 3] << 16) |
								(cis[i + offset + 2] << 8) |
								cis[i + offset + 1]);
							}

							offset += USBREGS_TPL_SIZE;
						} while (offset < tlen);
					}
					break;

				case HNBU_USBRDY:
					/* The first byte of this tuple indicate if the host
					 * needs to be informed about the readiness of
					 * the HSIC/USB for enumeration on which GPIO should
					 * the device assert this event.
					 */
					varbuf_append(&b, vstr_usbrdy, cis[i + 1]);

					/* The following fields in this OTP are optional.
					 * The remaining bytes will indicate the delay required
					 * before and/or after the ch_init(). The delay is defined
					 * using 16-bits of this the MSB(bit15 of 15:0) will be
					 * used indicate if the parameter is for Pre or Post delay.
					 */
					for (j = 2; j < USBRDY_MAXOTP_SIZE && j < tlen;
						j += 2) {
						uint16 usb_delay;

						usb_delay = cis[i + j] | (cis[i + j + 1] << 8);

						/* The bit-15 of the delay field will indicate the
						 * type of delay (pre or post).
						 */
						if (usb_delay & USBRDY_DLY_TYPE) {
							varbuf_append(&b, vstr_usbpostdly,
							(usb_delay & USBRDY_DLY_MASK));
						} else {
							varbuf_append(&b, vstr_usbpredly,
							(usb_delay & USBRDY_DLY_MASK));
						}
					}
					break;

				case HNBU_BLDR_TIMEOUT:
					/* The Delay after USBConnect for timeout till dongle
					 * receives get_descriptor request.
					 */
					varbuf_append(&b, vstr_bldr_reset_timeout,
						(cis[i + 1] | (cis[i + 2] << 8)));
					break;

				case HNBU_MUXENAB:
					varbuf_append(&b, vstr_muxenab, cis[i + 1]);
					break;
				case HNBU_PUBKEY:
					/* The public key is in binary format in OTP,
					 * convert to string format before appending
					 * buffer string.
					 *  public key(12 bytes) + crc (1byte) = 129
					 */
					{
						unsigned char a[300];
						int k, j;

						for (k = 1, j = 0; k < 129; k++)
							j += sprintf((char *)(a + j), "%02x",
								cis[i + k]);

						a[256] = 0;

						varbuf_append(&b, vstr_pubkey, a);
					}
					break;
#else
				case HNBU_AA:
					varbuf_append(&b, vstr_aa2g, cis[i + 1]);
					if (tlen >= 3)
						varbuf_append(&b, vstr_aa5g, cis[i + 2]);
					break;

				case HNBU_AG:
					varbuf_append(&b, vstr_ag, 0, cis[i + 1]);
					if (tlen >= 3)
						varbuf_append(&b, vstr_ag, 1, cis[i + 2]);
					if (tlen >= 4)
						varbuf_append(&b, vstr_ag, 2, cis[i + 3]);
					if (tlen >= 5)
						varbuf_append(&b, vstr_ag, 3, cis[i + 4]);
					ag_init = TRUE;
					break;

				case HNBU_ANT5G:
					varbuf_append(&b, vstr_aa5g, cis[i + 1]);
					varbuf_append(&b, vstr_ag, 1, cis[i + 2]);
					break;

				case HNBU_CC:
					ASSERT(sromrev == 1);
					varbuf_append(&b, vstr_cc, cis[i + 1]);
					break;

				case HNBU_PAPARMS:
				{
					uint8 pa0_lo_offset = 0;
					switch (tlen) {
					case 2:
						ASSERT(sromrev == 1);
						varbuf_append(&b, vstr_pa0maxpwr, cis[i + 1]);
						break;
					case 10:
					case 16:
						ASSERT(sromrev >= 2);
						varbuf_append(&b, vstr_opo, cis[i + 9]);
						if (tlen >= 13 && pa0_lo_offset == 0)
							pa0_lo_offset = 9;
						/* FALLTHROUGH */
					case 9:
					case 15:
						varbuf_append(&b, vstr_pa0maxpwr, cis[i + 8]);
						if (tlen >= 13 && pa0_lo_offset == 0)
							pa0_lo_offset = 8;
						/* FALLTHROUGH */
					BCMDONGLECASE(8)
					BCMDONGLECASE(14)
						varbuf_append(&b, vstr_pa0itssit, cis[i + 7]);
						varbuf_append(&b, vstr_maxp2ga, 0, cis[i + 7]);
						if (tlen >= 13 && pa0_lo_offset == 0)
							pa0_lo_offset = 7;
						/* FALLTHROUGH */
					BCMDONGLECASE(7)
					BCMDONGLECASE(13)
					        for (j = 0; j < 3; j++) {
							varbuf_append(&b, vstr_pa0b[j],
							              (cis[i + (j * 2) + 2] << 8) +
							              cis[i + (j * 2) + 1]);
						}
						if (tlen >= 13 && pa0_lo_offset == 0)
							pa0_lo_offset = 6;

						if (tlen >= 13 && pa0_lo_offset != 0) {
							for (j = 0; j < 3; j++) {
								varbuf_append(&b, vstr_pa0b_lo[j],
								 (cis[pa0_lo_offset+i+(j*2)+2]<<8)+
								 cis[pa0_lo_offset+i+(j*2)+1]);
							}
						}
						break;
					default:
						ASSERT((tlen == 2) || (tlen == 9) || (tlen == 10) ||
							(tlen == 15) || (tlen == 16));
						break;
					}
					break;
				}
				case HNBU_PAPARMS5G:
					ASSERT((sromrev == 2) || (sromrev == 3));
					switch (tlen) {
					case 23:
						varbuf_append(&b, vstr_pa1himaxpwr, cis[i + 22]);
						varbuf_append(&b, vstr_pa1lomaxpwr, cis[i + 21]);
						varbuf_append(&b, vstr_pa1maxpwr, cis[i + 20]);
						/* FALLTHROUGH */
					case 20:
						varbuf_append(&b, vstr_pa1itssit, cis[i + 19]);
						/* FALLTHROUGH */
					case 19:
						for (j = 0; j < 3; j++) {
							varbuf_append(&b, vstr_pa1b[j],
							              (cis[i + (j * 2) + 2] << 8) +
							              cis[i + (j * 2) + 1]);
						}
						for (j = 3; j < 6; j++) {
							varbuf_append(&b, vstr_pa1lob[j - 3],
							              (cis[i + (j * 2) + 2] << 8) +
							              cis[i + (j * 2) + 1]);
						}
						for (j = 6; j < 9; j++) {
							varbuf_append(&b, vstr_pa1hib[j - 6],
							              (cis[i + (j * 2) + 2] << 8) +
							              cis[i + (j * 2) + 1]);
						}
						break;
					default:
						ASSERT((tlen == 19) ||
						       (tlen == 20) || (tlen == 23));
						break;
					}
					break;

				case HNBU_OEM:
					ASSERT(sromrev == 1);
					varbuf_append(&b, vstr_oem,
					              cis[i + 1], cis[i + 2],
					              cis[i + 3], cis[i + 4],
					              cis[i + 5], cis[i + 6],
					              cis[i + 7], cis[i + 8]);
					break;

				case HNBU_LEDS:
					for (j = 1; j <= 4; j++) {
						if (cis[i + j] != 0xff) {
							varbuf_append(&b, vstr_ledbh, j-1,
							cis[i + j]);
						}
					}
					if (tlen < 13) break;

					for (j = 5; j <= 12; j++) {
						if (cis[i + j] != 0xff) {
							varbuf_append(&b, vstr_ledbh, j-1,
							cis[i + j]);
						}
					}
					if (tlen < 17) break;

					for (j = 13; j <= 16; j++) {
						if (cis[i + j] != 0xff) {
							varbuf_append(&b, vstr_ledbh, j-1,
							cis[i + j]);
						}
					}
					break;

				case HNBU_CCODE:
					ASSERT(sromrev > 1);
					if ((cis[i + 1] == 0) || (cis[i + 2] == 0))
						varbuf_append(&b, vstr_noccode);
					else
						varbuf_append(&b, vstr_ccode,
						              cis[i + 1], cis[i + 2]);
					varbuf_append(&b, vstr_cctl, cis[i + 3]);
					break;

				case HNBU_CCKPO:
					ASSERT(sromrev > 2);
					varbuf_append(&b, vstr_cckpo,
					              (cis[i + 2] << 8) | cis[i + 1]);
					break;

				case HNBU_OFDMPO:
					ASSERT(sromrev > 2);
					varbuf_append(&b, vstr_ofdmpo,
					              (cis[i + 4] << 24) |
					              (cis[i + 3] << 16) |
					              (cis[i + 2] << 8) |
					              cis[i + 1]);
					break;

				case HNBU_WPS:
					varbuf_append(&b, vstr_wpsgpio, cis[i + 1]);
					if (tlen >= 3)
						varbuf_append(&b, vstr_wpsled, cis[i + 2]);
					break;

				case HNBU_RSSISMBXA2G:
					ASSERT(sromrev == 3);
					varbuf_append(&b, vstr_rssismf2g, cis[i + 1] & 0xf);
					varbuf_append(&b, vstr_rssismc2g, (cis[i + 1] >> 4) & 0xf);
					varbuf_append(&b, vstr_rssisav2g, cis[i + 2] & 0x7);
					varbuf_append(&b, vstr_bxa2g, (cis[i + 2] >> 3) & 0x3);
					break;

				case HNBU_RSSISMBXA5G:
					ASSERT(sromrev == 3);
					varbuf_append(&b, vstr_rssismf5g, cis[i + 1] & 0xf);
					varbuf_append(&b, vstr_rssismc5g, (cis[i + 1] >> 4) & 0xf);
					varbuf_append(&b, vstr_rssisav5g, cis[i + 2] & 0x7);
					varbuf_append(&b, vstr_bxa5g, (cis[i + 2] >> 3) & 0x3);
					break;

				case HNBU_TRI2G:
					ASSERT(sromrev == 3);
					varbuf_append(&b, vstr_tri2g, cis[i + 1]);
					break;

				case HNBU_TRI5G:
					ASSERT(sromrev == 3);
					varbuf_append(&b, vstr_tri5gl, cis[i + 1]);
					varbuf_append(&b, vstr_tri5g, cis[i + 2]);
					varbuf_append(&b, vstr_tri5gh, cis[i + 3]);
					break;

				case HNBU_RXPO2G:
					ASSERT(sromrev == 3);
					varbuf_append(&b, vstr_rxpo2g, cis[i + 1]);
					break;

				case HNBU_RXPO5G:
					ASSERT(sromrev == 3);
					varbuf_append(&b, vstr_rxpo5g, cis[i + 1]);
					break;

				case HNBU_MACADDR:
					if (!(ETHER_ISNULLADDR(&cis[i+1])) &&
					    !(ETHER_ISMULTI(&cis[i+1]))) {
						bcm_ether_ntoa((struct ether_addr *)&cis[i + 1],
						               eabuf);

						/* set boardnum if HNBU_BOARDNUM not seen yet */
						if (boardnum == -1)
							boardnum = (cis[i + 5] << 8) + cis[i + 6];
					}
					break;

				case HNBU_LEDDC:
					/* CIS leddc only has 16bits, convert it to 32bits */
					w32 = ((cis[i + 2] << 24) | /* oncount */
					       (cis[i + 1] << 8)); /* offcount */
					varbuf_append(&b, vstr_leddc, w32);
					break;

				case HNBU_CHAINSWITCH:
					varbuf_append(&b, vstr_txchain, cis[i + 1]);
					varbuf_append(&b, vstr_rxchain, cis[i + 2]);
					varbuf_append(&b, vstr_antswitch,
					      (cis[i + 4] << 8) + cis[i + 3]);
					break;

				case HNBU_ELNA2G:
					varbuf_append(&b, vstr_elna2g, cis[i + 1]);
					break;

				case HNBU_ELNA5G:
					varbuf_append(&b, vstr_elna5g, cis[i + 1]);
					break;

				case HNBU_REGREV:
					varbuf_append(&b, vstr_regrev, cis[i + 1]);
					break;

				case HNBU_FEM: {
					uint16 fem = (cis[i + 2] << 8) + cis[i + 1];
					varbuf_append(&b, vstr_antswctl2g, (fem &
						SROM8_FEM_ANTSWLUT_MASK) >>
						SROM8_FEM_ANTSWLUT_SHIFT);
					varbuf_append(&b, vstr_triso2g, (fem &
						SROM8_FEM_TR_ISO_MASK) >>
						SROM8_FEM_TR_ISO_SHIFT);
					varbuf_append(&b, vstr_pdetrange2g, (fem &
						SROM8_FEM_PDET_RANGE_MASK) >>
						SROM8_FEM_PDET_RANGE_SHIFT);
					varbuf_append(&b, vstr_extpagain2g, (fem &
						SROM8_FEM_EXTPA_GAIN_MASK) >>
						SROM8_FEM_EXTPA_GAIN_SHIFT);
					varbuf_append(&b, vstr_tssipos2g, (fem &
						SROM8_FEM_TSSIPOS_MASK) >>
						SROM8_FEM_TSSIPOS_SHIFT);
					if (tlen < 5) break;

					fem = (cis[i + 4] << 8) + cis[i + 3];
					varbuf_append(&b, vstr_antswctl5g, (fem &
						SROM8_FEM_ANTSWLUT_MASK) >>
						SROM8_FEM_ANTSWLUT_SHIFT);
					varbuf_append(&b, vstr_triso5g, (fem &
						SROM8_FEM_TR_ISO_MASK) >>
						SROM8_FEM_TR_ISO_SHIFT);
					varbuf_append(&b, vstr_pdetrange5g, (fem &
						SROM8_FEM_PDET_RANGE_MASK) >>
						SROM8_FEM_PDET_RANGE_SHIFT);
					varbuf_append(&b, vstr_extpagain5g, (fem &
						SROM8_FEM_EXTPA_GAIN_MASK) >>
						SROM8_FEM_EXTPA_GAIN_SHIFT);
					varbuf_append(&b, vstr_tssipos5g, (fem &
						SROM8_FEM_TSSIPOS_MASK) >>
						SROM8_FEM_TSSIPOS_SHIFT);
					break;
					}

				case HNBU_PAPARMS_C0:
					varbuf_append(&b, vstr_maxp2ga, 0, cis[i + 1]);
					varbuf_append(&b, vstr_itt2ga0, cis[i + 2]);
					varbuf_append(&b, vstr_pa, 2, 0, 0,
						(cis[i + 4] << 8) + cis[i + 3]);
					varbuf_append(&b, vstr_pa, 2, 1, 0,
						(cis[i + 6] << 8) + cis[i + 5]);
					varbuf_append(&b, vstr_pa, 2, 2, 0,
						(cis[i + 8] << 8) + cis[i + 7]);
					if (tlen < 31) break;

					varbuf_append(&b, vstr_maxp5ga0, cis[i + 9]);
					varbuf_append(&b, vstr_itt5ga0, cis[i + 10]);
					varbuf_append(&b, vstr_maxp5gha0, cis[i + 11]);
					varbuf_append(&b, vstr_maxp5gla0, cis[i + 12]);
					varbuf_append(&b, vstr_pa, 5, 0, 0,
						(cis[i + 14] << 8) + cis[i + 13]);
					varbuf_append(&b, vstr_pa, 5, 1, 0,
						(cis[i + 16] << 8) + cis[i + 15]);
					varbuf_append(&b, vstr_pa, 5, 2, 0,
						(cis[i + 18] << 8) + cis[i + 17]);
					varbuf_append(&b, vstr_pahl, 5, 'l', 0, 0,
						(cis[i + 20] << 8) + cis[i + 19]);
					varbuf_append(&b, vstr_pahl, 5, 'l', 1, 0,
						(cis[i + 22] << 8) + cis[i + 21]);
					varbuf_append(&b, vstr_pahl, 5, 'l', 2, 0,
						(cis[i + 24] << 8) + cis[i + 23]);
					varbuf_append(&b, vstr_pahl, 5, 'h', 0, 0,
						(cis[i + 26] << 8) + cis[i + 25]);
					varbuf_append(&b, vstr_pahl, 5, 'h', 1, 0,
						(cis[i + 28] << 8) + cis[i + 27]);
					varbuf_append(&b, vstr_pahl, 5, 'h', 2, 0,
						(cis[i + 30] << 8) + cis[i + 29]);
					break;

				case HNBU_PAPARMS_C1:
					varbuf_append(&b, vstr_maxp2ga, 1, cis[i + 1]);
					varbuf_append(&b, vstr_itt2ga1, cis[i + 2]);
					varbuf_append(&b, vstr_pa, 2, 0, 1,
						(cis[i + 4] << 8) + cis[i + 3]);
					varbuf_append(&b, vstr_pa, 2, 1, 1,
						(cis[i + 6] << 8) + cis[i + 5]);
					varbuf_append(&b, vstr_pa, 2, 2, 1,
						(cis[i + 8] << 8) + cis[i + 7]);
					if (tlen < 31) break;

					varbuf_append(&b, vstr_maxp5ga1, cis[i + 9]);
					varbuf_append(&b, vstr_itt5ga1, cis[i + 10]);
					varbuf_append(&b, vstr_maxp5gha1, cis[i + 11]);
					varbuf_append(&b, vstr_maxp5gla1, cis[i + 12]);
					varbuf_append(&b, vstr_pa, 5, 0, 1,
						(cis[i + 14] << 8) + cis[i + 13]);
					varbuf_append(&b, vstr_pa, 5, 1, 1,
						(cis[i + 16] << 8) + cis[i + 15]);
					varbuf_append(&b, vstr_pa, 5, 2, 1,
						(cis[i + 18] << 8) + cis[i + 17]);
					varbuf_append(&b, vstr_pahl, 5, 'l', 0, 1,
						(cis[i + 20] << 8) + cis[i + 19]);
					varbuf_append(&b, vstr_pahl, 5, 'l', 1, 1,
						(cis[i + 22] << 8) + cis[i + 21]);
					varbuf_append(&b, vstr_pahl, 5, 'l', 2, 1,
						(cis[i + 24] << 8) + cis[i + 23]);
					varbuf_append(&b, vstr_pahl, 5, 'h', 0, 1,
						(cis[i + 26] << 8) + cis[i + 25]);
					varbuf_append(&b, vstr_pahl, 5, 'h', 1, 1,
						(cis[i + 28] << 8) + cis[i + 27]);
					varbuf_append(&b, vstr_pahl, 5, 'h', 2, 1,
						(cis[i + 30] << 8) + cis[i + 29]);
					break;

				case HNBU_PO_CCKOFDM:
					varbuf_append(&b, vstr_cck2gpo,
						(cis[i + 2] << 8) + cis[i + 1]);
					varbuf_append(&b, vstr_ofdm2gpo,
						(cis[i + 6] << 24) + (cis[i + 5] << 16) +
						(cis[i + 4] << 8) + cis[i + 3]);
					if (tlen < 19) break;

					varbuf_append(&b, vstr_ofdm5gpo,
						(cis[i + 10] << 24) + (cis[i + 9] << 16) +
						(cis[i + 8] << 8) + cis[i + 7]);
					varbuf_append(&b, vstr_ofdm5glpo,
						(cis[i + 14] << 24) + (cis[i + 13] << 16) +
						(cis[i + 12] << 8) + cis[i + 11]);
					varbuf_append(&b, vstr_ofdm5ghpo,
						(cis[i + 18] << 24) + (cis[i + 17] << 16) +
						(cis[i + 16] << 8) + cis[i + 15]);
					break;

				case HNBU_PO_MCS2G:
					for (j = 0; j <= (tlen/2); j++) {
						varbuf_append(&b, vstr_mcspo, 2, j,
							(cis[i + 2 + 2*j] << 8) + cis[i + 1 + 2*j]);
					}
					break;

				case HNBU_PO_MCS5GM:
					for (j = 0; j <= (tlen/2); j++) {
						varbuf_append(&b, vstr_mcspo, 5, j,
							(cis[i + 2 + 2*j] << 8) + cis[i + 1 + 2*j]);
					}
					break;

				case HNBU_PO_MCS5GLH:
					for (j = 0; j <= (tlen/4); j++) {
						varbuf_append(&b, vstr_mcspohl, 5, 'l', j,
							(cis[i + 2 + 2*j] << 8) + cis[i + 1 + 2*j]);
					}

					for (j = 0; j <= (tlen/4); j++) {
						varbuf_append(&b, vstr_mcspohl, 5, 'h', j,
							(cis[i + ((tlen/2)+2) + 2*j] << 8) +
							cis[i + ((tlen/2)+1) + 2*j]);
					}

					break;

				case HNBU_PO_CDD:
					varbuf_append(&b, vstr_cddpo,
					              (cis[i + 2] << 8) + cis[i + 1]);
					break;

				case HNBU_PO_STBC:
					varbuf_append(&b, vstr_stbcpo,
					              (cis[i + 2] << 8) + cis[i + 1]);
					break;

				case HNBU_PO_40M:
					varbuf_append(&b, vstr_bw40po,
					              (cis[i + 2] << 8) + cis[i + 1]);
					break;

				case HNBU_PO_40MDUP:
					varbuf_append(&b, vstr_bwduppo,
					              (cis[i + 2] << 8) + cis[i + 1]);
					break;

				case HNBU_OFDMPO5G:
					varbuf_append(&b, vstr_ofdm5gpo,
						(cis[i + 4] << 24) + (cis[i + 3] << 16) +
						(cis[i + 2] << 8) + cis[i + 1]);
					varbuf_append(&b, vstr_ofdm5glpo,
						(cis[i + 8] << 24) + (cis[i + 7] << 16) +
						(cis[i + 6] << 8) + cis[i + 5]);
					varbuf_append(&b, vstr_ofdm5ghpo,
						(cis[i + 12] << 24) + (cis[i + 11] << 16) +
						(cis[i + 10] << 8) + cis[i + 9]);
					break;
				/* Power per rate for SROM V9 */
				case HNBU_CCKBW202GPO:
					varbuf_append(&b, vstr_cckbw202gpo[0],
						((cis[i + 2] << 8) + cis[i + 1]));
					if (tlen > 4)
						varbuf_append(&b, vstr_cckbw202gpo[1],
							((cis[i + 4] << 8) + cis[i + 3]));
					break;

				case HNBU_LEGOFDMBW202GPO:
					varbuf_append(&b, vstr_legofdmbw202gpo[0],
						((cis[i + 4] << 24) + (cis[i + 3] << 16) +
						(cis[i + 2] << 8) + cis[i + 1]));
					if (tlen > 6)  {
						varbuf_append(&b, vstr_legofdmbw202gpo[1],
							((cis[i + 8] << 24) + (cis[i + 7] << 16) +
							(cis[i + 6] << 8) + cis[i + 5]));
					}
					break;

				case HNBU_LEGOFDMBW205GPO:
					for (j = 0; j < 6; j++) {
						if (tlen < (2 + 4 * j))
							break;
						varbuf_append(&b, vstr_legofdmbw205gpo[j],
							((cis[4 * j + i + 4] << 24)
							+ (cis[4 * j + i + 3] << 16)
							+ (cis[4 * j + i + 2] << 8)
							+ cis[4 * j + i + 1]));
					}
					break;

				case HNBU_MCS2GPO:
					for (j = 0; j < 3; j++) {
						if (tlen < (2 + 4 * j))
							break;
						varbuf_append(&b, vstr_mcs2gpo[j],
							((cis[4 * j + i + 4] << 24)
							+ (cis[4 * j + i + 3] << 16)
							+ (cis[4 * j + i + 2] << 8)
							+ cis[4 * j + i + 1]));
					}
					break;

				case HNBU_MCS5GLPO:
					for (j = 0; j < 3; j++) {
						if (tlen < (2 + 4 * j))
							break;
						varbuf_append(&b, vstr_mcs5glpo[j],
							((cis[4 * j + i + 4] << 24)
							+ (cis[4 * j + i + 3] << 16)
							+ (cis[4 * j + i + 2] << 8)
							+ cis[4 * j + i + 1]));
					}
					break;

				case HNBU_MCS5GMPO:
					for (j = 0; j < 3; j++) {
						if (tlen < (2 + 4 * j))
							break;
						varbuf_append(&b, vstr_mcs5gmpo[j],
							((cis[4 * j + i + 4] << 24)
							+ (cis[4 * j + i + 3] << 16)
							+ (cis[4 * j + i + 2] << 8)
							+ cis[4 * j + i + 1]));
					}
					break;

				case HNBU_MCS5GHPO:
					for (j = 0; j < 3; j++) {
						if (tlen < (2 + 4 * j))
							break;
						varbuf_append(&b, vstr_mcs5ghpo[j],
							((cis[4 * j + i + 4] << 24)
							+ (cis[4 * j + i + 3] << 16)
							+ (cis[4 * j + i + 2] << 8)
							+ cis[4 * j + i + 1]));
					}
					break;

				case HNBU_MCS32PO:
					varbuf_append(&b, vstr_mcs32po,
						(cis[i + 2] << 8) + cis[i + 1]);
					break;

				case HNBU_LEG40DUPPO:
					varbuf_append(&b, vstr_legofdm40duppo,
						(cis[i + 2] << 8) + cis[i + 1]);
					break;

				case HNBU_CUSTOM1:
					varbuf_append(&b, vstr_custom, 1, ((cis[i + 4] << 24) +
						(cis[i + 3] << 16) + (cis[i + 2] << 8) +
						cis[i + 1]));
					break;

#if defined(BCMCCISSR3)
				case HNBU_SROM3SWRGN:
					if (tlen >= 73) {
						uint16 srom[35];
						uint8 srev = cis[i + 1 + 70];
						ASSERT(srev == 3);
						/* make tuple value 16-bit aligned and parse it */
						bcopy(&cis[i + 1], srom, sizeof(srom));
						_initvars_srom_pci(srev, srom, SROM3_SWRGN_OFF, &b);
						/* 2.4G antenna gain is included in SROM */
						ag_init = TRUE;
						/* Ethernet MAC address is included in SROM */
						eabuf[0] = 0;
						boardnum = -1;
					}
					/* create extra variables */
					if (tlen >= 75)
						varbuf_append(&b, vstr_vendid,
						              (cis[i + 1 + 73] << 8) +
						              cis[i + 1 + 72]);
					if (tlen >= 77)
						varbuf_append(&b, vstr_devid,
						              (cis[i + 1 + 75] << 8) +
						              cis[i + 1 + 74]);
					if (tlen >= 79)
						varbuf_append(&b, vstr_xtalfreq,
						              (cis[i + 1 + 77] << 8) +
						              cis[i + 1 + 76]);
					break;
#endif	

				case HNBU_CCKFILTTYPE:
					varbuf_append(&b, vstr_cckdigfilttype,
						(cis[i + 1]));
					break;

				case HNBU_TEMPTHRESH:
					varbuf_append(&b, vstr_tempthresh,
						(cis[i + 1]));
					/* period in msb nibble */
					varbuf_append(&b, vstr_temps_period,
						(cis[i + 2] & SROM11_TEMPS_PERIOD_MASK) >>
						SROM11_TEMPS_PERIOD_SHIFT);
					/* hysterisis in lsb nibble */
					varbuf_append(&b, vstr_temps_hysteresis,
						(cis[i + 2] & SROM11_TEMPS_HYSTERESIS_MASK) >>
						SROM11_TEMPS_HYSTERESIS_SHIFT);
					if (tlen >= 4) {
						varbuf_append(&b, vstr_tempoffset,
						(cis[i + 3]));
						varbuf_append(&b, vstr_tempsense_slope,
						(cis[i + 4]));
						varbuf_append(&b, vstr_temp_corrx,
						(cis[i + 5] & SROM11_TEMPCORRX_MASK) >>
						SROM11_TEMPCORRX_SHIFT);
						varbuf_append(&b, vstr_tempsense_option,
						(cis[i + 5] & SROM11_TEMPSENSE_OPTION_MASK) >>
						SROM11_TEMPSENSE_OPTION_SHIFT);
						varbuf_append(&b, vstr_phycal_tempdelta,
						(cis[i + 6]));
					}
					break;

				case HNBU_FEM_CFG: {
					/* fem_cfg1 */
					uint16 fem_cfg = (cis[i + 2] << 8) + cis[i + 1];
					varbuf_append(&b, vstr_femctrl,
						(fem_cfg & SROM11_FEMCTRL_MASK) >>
						SROM11_FEMCTRL_SHIFT);
					varbuf_append(&b, vstr_papdcap, 2,
						(fem_cfg & SROM11_PAPDCAP_MASK) >>
						SROM11_PAPDCAP_SHIFT);
					varbuf_append(&b, vstr_tworangetssi, 2,
						(fem_cfg & SROM11_TWORANGETSSI_MASK) >>
						SROM11_TWORANGETSSI_SHIFT);
					varbuf_append(&b, vstr_pdgaing, 2,
						(fem_cfg & SROM11_PDGAIN_MASK) >>
						SROM11_PDGAIN_SHIFT);
					varbuf_append(&b, vstr_epagaing, 2,
						(fem_cfg & SROM11_EPAGAIN_MASK) >>
						SROM11_EPAGAIN_SHIFT);
					varbuf_append(&b, vstr_tssiposslopeg, 2,
						(fem_cfg & SROM11_TSSIPOSSLOPE_MASK) >>
						SROM11_TSSIPOSSLOPE_SHIFT);
					/* fem_cfg2 */
					fem_cfg = (cis[i + 4] << 8) + cis[i + 3];
					varbuf_append(&b, vstr_gainctrlsph,
						(fem_cfg & SROM11_GAINCTRLSPH_MASK) >>
						SROM11_GAINCTRLSPH_SHIFT);
					varbuf_append(&b, vstr_papdcap, 5,
						(fem_cfg & SROM11_PAPDCAP_MASK) >>
						SROM11_PAPDCAP_SHIFT);
					varbuf_append(&b, vstr_tworangetssi, 5,
						(fem_cfg & SROM11_TWORANGETSSI_MASK) >>
						SROM11_TWORANGETSSI_SHIFT);
					varbuf_append(&b, vstr_pdgaing, 5,
						(fem_cfg & SROM11_PDGAIN_MASK) >>
						SROM11_PDGAIN_SHIFT);
					varbuf_append(&b, vstr_epagaing, 5,
						(fem_cfg & SROM11_EPAGAIN_MASK) >>
						SROM11_EPAGAIN_SHIFT);
					varbuf_append(&b, vstr_tssiposslopeg, 5,
						(fem_cfg & SROM11_TSSIPOSSLOPE_MASK) >>
						SROM11_TSSIPOSSLOPE_SHIFT);
					break;
				}

				case HNBU_ACPA_C0:
				{
					const int a = 0;

					varbuf_append(&b, vstr_subband5gver,
					              (cis[i + 2] << 8) + cis[i + 1]);
					varbuf_append(&b, vstr_maxp2ga, a,
					              (cis[i + 4] << 8) + cis[i + 3]);
					/* pa2g */
					varbuf_append(&b, vstr_pa2ga, a,
						(cis[i + 6] << 8) + cis[i + 5],
						(cis[i + 8] << 8) + cis[i + 7],
						(cis[i + 10] << 8) + cis[i + 9]);
					/* maxp5g */
					varbuf_append(&b, vstr_maxp5ga, a,
						cis[i + 11],
						cis[i + 12],
						cis[i + 13],
						cis[i + 14]);
					/* pa5g */
					varbuf_append(&b, vstr_pa5ga, a,
						(cis[i + 16] << 8) + cis[i + 15],
						(cis[i + 18] << 8) + cis[i + 17],
						(cis[i + 20] << 8) + cis[i + 19],
						(cis[i + 22] << 8) + cis[i + 21],
						(cis[i + 24] << 8) + cis[i + 23],
						(cis[i + 26] << 8) + cis[i + 25],
						(cis[i + 28] << 8) + cis[i + 27],
						(cis[i + 30] << 8) + cis[i + 29],
						(cis[i + 32] << 8) + cis[i + 31],
						(cis[i + 34] << 8) + cis[i + 33],
						(cis[i + 36] << 8) + cis[i + 35],
						(cis[i + 38] << 8) + cis[i + 37]);
					break;
				}

				case HNBU_ACPA_C1:
				{
					const int a = 1;

					varbuf_append(&b, vstr_maxp2ga, a,
					              (cis[i + 2] << 8) + cis[i + 1]);
					/* pa2g */
					varbuf_append(&b, vstr_pa2ga, a,
						(cis[i + 4] << 8) + cis[i + 3],
						(cis[i + 6] << 8) + cis[i + 5],
						(cis[i + 8] << 8) + cis[i + 7]);
					/* maxp5g */
					varbuf_append(&b, vstr_maxp5ga, a,
						cis[i + 9],
						cis[i + 10],
						cis[i + 11],
						cis[i + 12]);
					/* pa5g */
					varbuf_append(&b, vstr_pa5ga, a,
						(cis[i + 14] << 8) + cis[i + 13],
						(cis[i + 16] << 8) + cis[i + 15],
						(cis[i + 18] << 8) + cis[i + 17],
						(cis[i + 20] << 8) + cis[i + 19],
						(cis[i + 22] << 8) + cis[i + 21],
						(cis[i + 24] << 8) + cis[i + 23],
						(cis[i + 26] << 8) + cis[i + 25],
						(cis[i + 28] << 8) + cis[i + 27],
						(cis[i + 30] << 8) + cis[i + 29],
						(cis[i + 32] << 8) + cis[i + 31],
						(cis[i + 34] << 8) + cis[i + 33],
						(cis[i + 36] << 8) + cis[i + 35]);
					break;
				}

				case HNBU_ACPA_C2:
				{
					const int a = 2;

					varbuf_append(&b, vstr_maxp2ga, a,
					              (cis[i + 2] << 8) + cis[i + 1]);
					/* pa2g */
					varbuf_append(&b, vstr_pa2ga, a,
						(cis[i + 4] << 8) + cis[i + 3],
						(cis[i + 6] << 8) + cis[i + 5],
						(cis[i + 8] << 8) + cis[i + 7]);
					/* maxp5g */
					varbuf_append(&b, vstr_maxp5ga, a,
						cis[i + 9],
						cis[i + 10],
						cis[i + 11],
						cis[i + 12]);
					/* pa5g */
					varbuf_append(&b, vstr_pa5ga, a,
						(cis[i + 14] << 8) + cis[i + 13],
						(cis[i + 16] << 8) + cis[i + 15],
						(cis[i + 18] << 8) + cis[i + 17],
						(cis[i + 20] << 8) + cis[i + 19],
						(cis[i + 22] << 8) + cis[i + 21],
						(cis[i + 24] << 8) + cis[i + 23],
						(cis[i + 26] << 8) + cis[i + 25],
						(cis[i + 28] << 8) + cis[i + 27],
						(cis[i + 30] << 8) + cis[i + 29],
						(cis[i + 32] << 8) + cis[i + 31],
						(cis[i + 34] << 8) + cis[i + 33],
						(cis[i + 36] << 8) + cis[i + 35]);
					break;
				}

				case HNBU_MEAS_PWR:
					varbuf_append(&b, vstr_measpower, cis[i + 1]);
					varbuf_append(&b, vstr_measpowerX, 1, (cis[i + 2]));
					varbuf_append(&b, vstr_measpowerX, 2, (cis[i + 3]));
					varbuf_append(&b, vstr_rawtempsense,
						((cis[i + 5] & 0x1) << 8) + cis[i + 4]);
					break;

				case HNBU_PDOFF:
					varbuf_append(&b, vstr_pdoffsetma, 40, 0,
					      (cis[i + 2] << 8) + cis[i + 1]);
					varbuf_append(&b, vstr_pdoffsetma, 40, 1,
					      (cis[i + 4] << 8) + cis[i + 3]);
					varbuf_append(&b, vstr_pdoffsetma, 40, 2,
					      (cis[i + 6] << 8) + cis[i + 5]);
					varbuf_append(&b, vstr_pdoffsetma, 80, 0,
					      (cis[i + 8] << 8) + cis[i + 7]);
					varbuf_append(&b, vstr_pdoffsetma, 80, 1,
					      (cis[i + 10] << 8) + cis[i + 9]);
					varbuf_append(&b, vstr_pdoffsetma, 80, 2,
					      (cis[i + 12] << 8) + cis[i + 11]);
					break;

				case HNBU_ACPPR_2GPO:
					varbuf_append(&b, vstr_dot11agofdmhrbw202gpo,
					              (cis[i + 2] << 8) + cis[i + 1]);
					varbuf_append(&b, vstr_ofdmlrbw202gpo,
					              (cis[i + 4] << 8) + cis[i + 3]);
					break;

				case HNBU_ACPPR_5GPO:
					varbuf_append(&b, vstr_mcsbw805gpo, 'l',
						(cis[i + 4] << 24) + (cis[i + 3] << 16) +
						(cis[i + 2] << 8) + cis[i + 1]);
					varbuf_append(&b, vstr_mcsbw1605gpo, 'l',
						(cis[i + 8] << 24) + (cis[i + 7] << 16) +
						(cis[i + 6] << 8) + cis[i + 5]);
					varbuf_append(&b, vstr_mcsbw805gpo, 'm',
						(cis[i + 12] << 24) + (cis[i + 11] << 16) +
						(cis[i + 10] << 8) + cis[i + 9]);
					varbuf_append(&b, vstr_mcsbw1605gpo, 'm',
						(cis[i + 16] << 24) + (cis[i + 15] << 16) +
						(cis[i + 14] << 8) + cis[i + 13]);
					varbuf_append(&b, vstr_mcsbw805gpo, 'h',
						(cis[i + 20] << 24) + (cis[i + 19] << 16) +
						(cis[i + 18] << 8) + cis[i + 17]);
					varbuf_append(&b, vstr_mcsbw1605gpo, 'h',
						(cis[i + 24] << 24) + (cis[i + 23] << 16) +
						(cis[i + 22] << 8) + cis[i + 21]);
					varbuf_append(&b, vstr_mcslr5gpo, 'l',
					              (cis[i + 26] << 8) + cis[i + 25]);
					varbuf_append(&b, vstr_mcslr5gpo, 'm',
					              (cis[i + 28] << 8) + cis[i + 27]);
					varbuf_append(&b, vstr_mcslr5gpo, 'h',
					              (cis[i + 30] << 8) + cis[i + 29]);
					break;

				case HNBU_ACPPR_SBPO:
					varbuf_append(&b, vstr_sb20in40rpo, 'h',
					              (cis[i + 2] << 8) + cis[i + 1]);
					varbuf_append(&b, vstr_sb20in80and160r5gpo, 'h', 'l',
					              (cis[i + 4] << 8) + cis[i + 3]);
					varbuf_append(&b, vstr_sb40and80r5gpo, 'h', 'l',
					              (cis[i + 6] << 8) + cis[i + 5]);
					varbuf_append(&b, vstr_sb20in80and160r5gpo, 'h', 'm',
					              (cis[i + 8] << 8) + cis[i + 7]);
					varbuf_append(&b, vstr_sb40and80r5gpo, 'h', 'm',
					              (cis[i + 10] << 8) + cis[i + 9]);
					varbuf_append(&b, vstr_sb20in80and160r5gpo, 'h', 'h',
					              (cis[i + 12] << 8) + cis[i + 11]);
					varbuf_append(&b, vstr_sb40and80r5gpo, 'h', 'h',
					              (cis[i + 14] << 8) + cis[i + 13]);
					varbuf_append(&b, vstr_sb20in40rpo, 'l',
					              (cis[i + 16] << 8) + cis[i + 15]);
					varbuf_append(&b, vstr_sb20in80and160r5gpo, 'l', 'l',
					              (cis[i + 18] << 8) + cis[i + 17]);
					varbuf_append(&b, vstr_sb40and80r5gpo, 'l', 'l',
					              (cis[i + 20] << 8) + cis[i + 19]);
					varbuf_append(&b, vstr_sb20in80and160r5gpo, 'l', 'm',
					              (cis[i + 22] << 8) + cis[i + 21]);
					varbuf_append(&b, vstr_sb40and80r5gpo, 'l', 'm',
					              (cis[i + 24] << 8) + cis[i + 23]);
					varbuf_append(&b, vstr_sb20in80and160r5gpo, 'l', 'h',
					              (cis[i + 26] << 8) + cis[i + 25]);
					varbuf_append(&b, vstr_sb40and80r5gpo, 'l', 'h',
					              (cis[i + 28] << 8) + cis[i + 27]);
					varbuf_append(&b, vstr_dot11agduprpo, 'h',
					              (cis[i + 30] << 8) + cis[i + 24]);
					varbuf_append(&b, vstr_dot11agduprpo, 'l',
					              (cis[i + 32] << 8) + cis[i + 26]);
					break;

				case HNBU_NOISELVL:
					/* noiselvl2g */
					varbuf_append(&b, vstr_noiselvl2ga, 0,
					              (cis[i + 1] & 0x1f));
					varbuf_append(&b, vstr_noiselvl2ga, 1,
					              (cis[i + 2] & 0x1f));
					varbuf_append(&b, vstr_noiselvl2ga, 2,
					              (cis[i + 3] & 0x1f));
					/* noiselvl5g */
					varbuf_append(&b, vstr_noiselvl5ga, 0,
					              (cis[i + 4] & 0x1f),
					              (cis[i + 5] & 0x1f),
					              (cis[i + 6] & 0x1f),
					              (cis[i + 7] & 0x1f));
					varbuf_append(&b, vstr_noiselvl5ga, 1,
					              (cis[i + 8] & 0x1f),
					              (cis[i + 9] & 0x1f),
					              (cis[i + 10] & 0x1f),
					              (cis[i + 11] & 0x1f));
					varbuf_append(&b, vstr_noiselvl5ga, 2,
					              (cis[i + 12] & 0x1f),
					              (cis[i + 13] & 0x1f),
					              (cis[i + 14] & 0x1f),
					              (cis[i + 15] & 0x1f));
					break;

				case HNBU_RXGAIN_ERR:
					varbuf_append(&b, vstr_rxgainerr2ga, 0,
					              (cis[i + 1] & 0x3f));
					varbuf_append(&b, vstr_rxgainerr2ga, 1,
					              (cis[i + 2] & 0x1f));
					varbuf_append(&b, vstr_rxgainerr2ga, 2,
					              (cis[i + 3] & 0x1f));
					varbuf_append(&b, vstr_rxgainerr5ga, 0,
					              (cis[i + 4] & 0x3f),
					              (cis[i + 5] & 0x3f),
					              (cis[i + 6] & 0x3f),
					              (cis[i + 7] & 0x3f));
					varbuf_append(&b, vstr_rxgainerr5ga, 1,
					              (cis[i + 8] & 0x1f),
					              (cis[i + 9] & 0x1f),
					              (cis[i + 10] & 0x1f),
					              (cis[i + 11] & 0x1f));
					varbuf_append(&b, vstr_rxgainerr5ga, 2,
					              (cis[i + 12] & 0x1f),
					              (cis[i + 13] & 0x1f),
					              (cis[i + 14] & 0x1f),
					              (cis[i + 15] & 0x1f));
					break;

				case HNBU_AGBGA:
					varbuf_append(&b, vstr_agbg, 0, cis[i + 1]);
					varbuf_append(&b, vstr_agbg, 1, cis[i + 2]);
					varbuf_append(&b, vstr_agbg, 2, cis[i + 3]);
					varbuf_append(&b, vstr_aga, 0, cis[i + 4]);
					varbuf_append(&b, vstr_aga, 1, cis[i + 5]);
					varbuf_append(&b, vstr_aga, 2, cis[i + 6]);
					break;

				case HNBU_ACRXGAINS_C0: {
					int a = 0;

					/* rxgains */
					uint16 rxgains = (cis[i + 2] << 8) + cis[i + 1];
					varbuf_append(&b, vstr_rxgainsgtrelnabypa, 5, a,
						(rxgains & SROM11_RXGAINS5GTRELNABYPA_MASK) >>
						SROM11_RXGAINS5GTRELNABYPA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgtrisoa, 5, a,
						(rxgains & SROM11_RXGAINS5GTRISOA_MASK) >>
						SROM11_RXGAINS5GTRISOA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgelnagaina, 5, a,
						(rxgains & SROM11_RXGAINS5GELNAGAINA_MASK) >>
						SROM11_RXGAINS5GELNAGAINA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgtrelnabypa, 2, a,
						(rxgains & SROM11_RXGAINS2GTRELNABYPA_MASK) >>
						SROM11_RXGAINS2GTRELNABYPA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgtrisoa, 2, a,
						(rxgains & SROM11_RXGAINS2GTRISOA_MASK) >>
						SROM11_RXGAINS2GTRISOA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgelnagaina, 2, a,
						(rxgains & SROM11_RXGAINS2GELNAGAINA_MASK) >>
						SROM11_RXGAINS2GELNAGAINA_SHIFT);
					/* rxgains1 */
					rxgains = (cis[i + 4] << 8) + cis[i + 3];
					varbuf_append(&b, vstr_rxgainsgxtrelnabypa, 5, 'h', a,
						(rxgains & SROM11_RXGAINS5GTRELNABYPA_MASK) >>
						SROM11_RXGAINS5GTRELNABYPA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgxtrisoa, 5, 'h', a,
						(rxgains & SROM11_RXGAINS5GTRISOA_MASK) >>
						SROM11_RXGAINS5GTRISOA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgxelnagaina, 5, 'h', a,
						(rxgains & SROM11_RXGAINS5GELNAGAINA_MASK) >>
						SROM11_RXGAINS5GELNAGAINA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgxtrelnabypa, 5, 'm', a,
						(rxgains & SROM11_RXGAINS5GTRELNABYPA_MASK) >>
						SROM11_RXGAINS5GTRELNABYPA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgxtrisoa, 5, 'm', a,
						(rxgains & SROM11_RXGAINS5GTRISOA_MASK) >>
						SROM11_RXGAINS5GTRISOA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgxelnagaina, 5, 'm', a,
						(rxgains & SROM11_RXGAINS5GELNAGAINA_MASK) >>
						SROM11_RXGAINS5GELNAGAINA_SHIFT);
					break;
				}

				case HNBU_ACRXGAINS_C1: {
					int a = 1;

					/* rxgains */
					uint16 rxgains = (cis[i + 2] << 8) + cis[i + 1];
					varbuf_append(&b, vstr_rxgainsgtrelnabypa, 5, a,
						(rxgains & SROM11_RXGAINS5GTRELNABYPA_MASK) >>
						SROM11_RXGAINS5GTRELNABYPA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgtrisoa, 5, a,
						(rxgains & SROM11_RXGAINS5GTRISOA_MASK) >>
						SROM11_RXGAINS5GTRISOA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgelnagaina, 5, a,
						(rxgains & SROM11_RXGAINS5GELNAGAINA_MASK) >>
						SROM11_RXGAINS5GELNAGAINA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgtrelnabypa, 2, a,
						(rxgains & SROM11_RXGAINS2GTRELNABYPA_MASK) >>
						SROM11_RXGAINS2GTRELNABYPA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgtrisoa, 2, a,
						(rxgains & SROM11_RXGAINS2GTRISOA_MASK) >>
						SROM11_RXGAINS2GTRISOA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgelnagaina, 2, a,
						(rxgains & SROM11_RXGAINS2GELNAGAINA_MASK) >>
						SROM11_RXGAINS2GELNAGAINA_SHIFT);
					/* rxgains1 */
					rxgains = (cis[i + 4] << 8) + cis[i + 3];
					varbuf_append(&b, vstr_rxgainsgxtrelnabypa, 5, 'h', a,
						(rxgains & SROM11_RXGAINS5GTRELNABYPA_MASK) >>
						SROM11_RXGAINS5GTRELNABYPA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgxtrisoa, 5, 'h', a,
						(rxgains & SROM11_RXGAINS5GTRISOA_MASK) >>
						SROM11_RXGAINS5GTRISOA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgxelnagaina, 5, 'h', a,
						(rxgains & SROM11_RXGAINS5GELNAGAINA_MASK) >>
						SROM11_RXGAINS5GELNAGAINA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgxtrelnabypa, 5, 'm', a,
						(rxgains & SROM11_RXGAINS5GTRELNABYPA_MASK) >>
						SROM11_RXGAINS5GTRELNABYPA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgxtrisoa, 5, 'm', a,
						(rxgains & SROM11_RXGAINS5GTRISOA_MASK) >>
						SROM11_RXGAINS5GTRISOA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgxelnagaina, 5, 'm', a,
						(rxgains & SROM11_RXGAINS5GELNAGAINA_MASK) >>
						SROM11_RXGAINS5GELNAGAINA_SHIFT);
					break;
				}

				case HNBU_ACRXGAINS_C2: {
					int a = 2;

					/* rxgains */
					uint16 rxgains = (cis[i + 2] << 8) + cis[i + 1];
					varbuf_append(&b, vstr_rxgainsgtrelnabypa, 5, a,
						(rxgains & SROM11_RXGAINS5GTRELNABYPA_MASK) >>
						SROM11_RXGAINS5GTRELNABYPA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgtrisoa, 5, a,
						(rxgains & SROM11_RXGAINS5GTRISOA_MASK) >>
						SROM11_RXGAINS5GTRISOA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgelnagaina, 5, a,
						(rxgains & SROM11_RXGAINS5GELNAGAINA_MASK) >>
						SROM11_RXGAINS5GELNAGAINA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgtrelnabypa, 2, a,
						(rxgains & SROM11_RXGAINS2GTRELNABYPA_MASK) >>
						SROM11_RXGAINS2GTRELNABYPA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgtrisoa, 2, a,
						(rxgains & SROM11_RXGAINS2GTRISOA_MASK) >>
						SROM11_RXGAINS2GTRISOA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgelnagaina, 2, a,
						(rxgains & SROM11_RXGAINS2GELNAGAINA_MASK) >>
						SROM11_RXGAINS2GELNAGAINA_SHIFT);
					/* rxgains1 */
					rxgains = (cis[i + 4] << 8) + cis[i + 3];
					varbuf_append(&b, vstr_rxgainsgxtrelnabypa, 5, 'h', a,
						(rxgains & SROM11_RXGAINS5GTRELNABYPA_MASK) >>
						SROM11_RXGAINS5GTRELNABYPA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgxtrisoa, 5, 'h', a,
						(rxgains & SROM11_RXGAINS5GTRISOA_MASK) >>
						SROM11_RXGAINS5GTRISOA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgxelnagaina, 5, 'h', a,
						(rxgains & SROM11_RXGAINS5GELNAGAINA_MASK) >>
						SROM11_RXGAINS5GELNAGAINA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgxtrelnabypa, 5, 'm', a,
						(rxgains & SROM11_RXGAINS5GTRELNABYPA_MASK) >>
						SROM11_RXGAINS5GTRELNABYPA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgxtrisoa, 5, 'm', a,
						(rxgains & SROM11_RXGAINS5GTRISOA_MASK) >>
						SROM11_RXGAINS5GTRISOA_SHIFT);
					varbuf_append(&b, vstr_rxgainsgxelnagaina, 5, 'm', a,
						(rxgains & SROM11_RXGAINS5GELNAGAINA_MASK) >>
						SROM11_RXGAINS5GELNAGAINA_SHIFT);
					break;
				}

				case HNBU_TXDUTY: {
					varbuf_append(&b, vstr_txduty_ofdm, 40,
					              (cis[i + 2] << 8) + cis[i + 1]);
					varbuf_append(&b, vstr_txduty_thresh, 40,
					              (cis[i + 4] << 8) + cis[i + 3]);
					varbuf_append(&b, vstr_txduty_ofdm, 80,
					              (cis[i + 6] << 8) + cis[i + 5]);
					varbuf_append(&b, vstr_txduty_thresh, 80,
					              (cis[i + 8] << 8) + cis[i + 7]);
					break;
				}

				case HNBU_UUID:
					{
					/* uuid format 12345678-1234-5678-1234-567812345678 */

					char uuidstr[37]; /* 32 ids, 4 '-', 1 Null */

					snprintf(uuidstr, sizeof(uuidstr),
						"%02X%02X%02X%02X-%02X%02X-%02X%02X-"
						"%02X%02X-%02X%02X%02X%02X%02X%02X",
						cis[i + 1], cis[i + 2], cis[i + 3], cis[i + 4],
						cis[i + 5], cis[i + 6], cis[i + 7], cis[i + 8],
						cis[i + 9], cis[i + 10], cis[i + 11], cis[i + 12],
						cis[i + 13], cis[i + 14], cis[i + 15], cis[i + 16]);

					varbuf_append(&b, vstr_uuid, uuidstr);
					break;

					}
				case HNBU_WOWLGPIO:
					varbuf_append(&b, vstr_wowlgpio, ((cis[i + 1]) & 0x7F));
					varbuf_append(&b, vstr_wowlgpiopol,
						(((cis[i + 1]) >> 7) & 0x1));
					break;

#endif /* !BCM_BOOTLOADER */
#ifdef BCMUSBDEV_COMPOSITE
				case HNBU_USBDESC_COMPOSITE:
					varbuf_append(&b, vstr_usbdesc_composite,
						(cis[i + 2] << 8) | cis[i + 1]);
					break;
#endif /* BCMUSBDEV_COMPOSITE */
				case HNBU_USBUTMI_CTL:
					varbuf_append(&b, vstr_usbutmi_ctl,
						(cis[i + 2] << 8) | cis[i + 1]);
					break;

				case HNBU_USBSSPHY_UTMI_CTL0:
					varbuf_append(&b, vstr_usbssphy_utmi_ctl0,
						(cis[i + 4] << 24) | (cis[i + 3] << 16) |
						(cis[i + 2] << 8) | cis[i + 1]);
					break;

				case HNBU_USBSSPHY_UTMI_CTL1:
					varbuf_append(&b, vstr_usbssphy_utmi_ctl1,
						(cis[i + 4] << 24) | (cis[i + 3] << 16) |
						(cis[i + 2] << 8) | cis[i + 1]);
					break;

				case HNBU_USBSSPHY_UTMI_CTL2:
					varbuf_append(&b, vstr_usbssphy_utmi_ctl2,
						(cis[i + 4] << 24) | (cis[i + 3] << 16) |
						(cis[i + 2] << 8) | cis[i + 1]);
					break;

				case HNBU_USBSSPHY_SLEEP0:
					varbuf_append(&b, vstr_usbssphy_sleep0,
						(cis[i + 2] << 8) | cis[i + 1]);
					break;

				case HNBU_USBSSPHY_SLEEP1:
					varbuf_append(&b, vstr_usbssphy_sleep1,
						(cis[i + 2] << 8) | cis[i + 1]);
					break;

				case HNBU_USBSSPHY_SLEEP2:
					varbuf_append(&b, vstr_usbssphy_sleep2,
						(cis[i + 2] << 8) | cis[i + 1]);
					break;

				case HNBU_USBSSPHY_SLEEP3:
					varbuf_append(&b, vstr_usbssphy_sleep3,
						(cis[i + 2] << 8) | cis[i + 1]);
					break;
				case HNBU_USBSSPHY_MDIO:
					{
					int setnum, k;

					setnum = (cis[i + 1])/4;
					if (setnum == 0)
						break;
					for (j = 0; j < setnum; j++) {
						k = j*12;
						varbuf_append(&b, vstr_usbssphy_mdio, j,
						(cis[i+4+k]<<16) | (cis[i+3+k]<<8) | cis[i+2+k],
						(cis[i+7+k]<<16) | (cis[i+6+k]<<8) | cis[i+5+k],
						(cis[i+10+k]<<16) | (cis[i+9+k]<<8) | cis[i+8+k],
						(cis[i+13+k]<<16) | (cis[i+12+k]<<8) | cis[i+11+k]);
						}
					}
					break;
				case HNBU_USB30PHY_NOSS:
					{
						varbuf_append(&b, vstr_usb30phy_noss, cis[i + 1]);
					}
					break;
				case HNBU_USB30PHY_U1U2:
					{
						varbuf_append(&b, vstr_usb30phy_u1u2, cis[i + 1]);
					}
					break;
				case HNBU_USB30PHY_REGS:
					{
						varbuf_append(&b, vstr_usb30phy_regs, 0,
							cis[i+4]|cis[i+3]|cis[i+2]|cis[i+1],
							cis[i+8]|cis[i+7]|cis[i+6]|cis[i+5],
							cis[i+12]|cis[i+11]|cis[i+10]|cis[i+9],
							cis[i+16]|cis[i+15]|cis[i+14]|cis[i+13]);
						varbuf_append(&b, vstr_usb30phy_regs, 1,
							cis[i+20]|cis[i+19]|cis[i+18]|cis[i+17],
							cis[i+24]|cis[i+23]|cis[i+22]|cis[i+21],
							cis[i+28]|cis[i+27]|cis[i+26]|cis[i+25],
							cis[i+32]|cis[i+31]|cis[i+30]|cis[i+29]);

					}
					break;

				case HNBU_PDOFF_2G:
					{
					uint16 pdoff_2g = (cis[i + 2] << 8) + cis[i + 1];
					varbuf_append(&b, vstr_pdoffset2gma, 40, 0,
						(pdoff_2g & SROM11_PDOFF_2G_40M_A0_MASK) >>
						SROM11_PDOFF_2G_40M_A0_SHIFT);
					varbuf_append(&b, vstr_pdoffset2gma, 40, 1,
						(pdoff_2g & SROM11_PDOFF_2G_40M_A1_MASK) >>
						SROM11_PDOFF_2G_40M_A1_SHIFT);
					varbuf_append(&b, vstr_pdoffset2gma, 40, 2,
						(pdoff_2g & SROM11_PDOFF_2G_40M_A2_MASK) >>
						SROM11_PDOFF_2G_40M_A2_SHIFT);
					varbuf_append(&b, vstr_pdoffset2gmvalid, 40,
						(pdoff_2g & SROM11_PDOFF_2G_40M_VALID_MASK) >>
						SROM11_PDOFF_2G_40M_VALID_SHIFT);
					break;
					}

				case HNBU_ACPA_CCK:
					varbuf_append(&b, vstr_pa2gccka, 0,
					        (cis[i + 2] << 8) + cis[i + 1],
						(cis[i + 4] << 8) + cis[i + 3],
						(cis[i + 6] << 8) + cis[i + 5]);
					break;

				case HNBU_ACPA_40:
					varbuf_append(&b, vstr_pa5gbw40a, 0,
					        (cis[i + 2] << 8) + cis[i + 1],
						(cis[i + 4] << 8) + cis[i + 3],
						(cis[i + 6] << 8) + cis[i + 5],
					        (cis[i + 8] << 8) + cis[i + 7],
						(cis[i + 10] << 8) + cis[i + 9],
						(cis[i + 12] << 8) + cis[i + 11],
					        (cis[i + 14] << 8) + cis[i + 13],
						(cis[i + 16] << 8) + cis[i + 15],
						(cis[i + 18] << 8) + cis[i + 17],
					        (cis[i + 20] << 8) + cis[i + 19],
						(cis[i + 22] << 8) + cis[i + 21],
						(cis[i + 24] << 8) + cis[i + 23]);
					break;

				case HNBU_ACPA_80:
					varbuf_append(&b, vstr_pa5gbw80a, 0,
					        (cis[i + 2] << 8) + cis[i + 1],
						(cis[i + 4] << 8) + cis[i + 3],
						(cis[i + 6] << 8) + cis[i + 5],
					        (cis[i + 8] << 8) + cis[i + 7],
						(cis[i + 10] << 8) + cis[i + 9],
						(cis[i + 12] << 8) + cis[i + 11],
					        (cis[i + 14] << 8) + cis[i + 13],
						(cis[i + 16] << 8) + cis[i + 15],
						(cis[i + 18] << 8) + cis[i + 17],
					        (cis[i + 20] << 8) + cis[i + 19],
						(cis[i + 22] << 8) + cis[i + 21],
						(cis[i + 24] << 8) + cis[i + 23]);
					break;

				case HNBU_ACPA_4080:
					varbuf_append(&b, vstr_pa5gbw4080a, 0,
					        (cis[i + 2] << 8) + cis[i + 1],
						(cis[i + 4] << 8) + cis[i + 3],
						(cis[i + 6] << 8) + cis[i + 5],
					        (cis[i + 8] << 8) + cis[i + 7],
						(cis[i + 10] << 8) + cis[i + 9],
						(cis[i + 12] << 8) + cis[i + 11],
					        (cis[i + 14] << 8) + cis[i + 13],
						(cis[i + 16] << 8) + cis[i + 15],
						(cis[i + 18] << 8) + cis[i + 17],
					        (cis[i + 20] << 8) + cis[i + 19],
						(cis[i + 22] << 8) + cis[i + 21],
						(cis[i + 24] << 8) + cis[i + 23]);
					varbuf_append(&b, vstr_pa5gbw4080a, 1,
					        (cis[i + 26] << 8) + cis[i + 25],
						(cis[i + 28] << 8) + cis[i + 27],
						(cis[i + 30] << 8) + cis[i + 29],
					        (cis[i + 32] << 8) + cis[i + 31],
						(cis[i + 34] << 8) + cis[i + 33],
						(cis[i + 36] << 8) + cis[i + 35],
					        (cis[i + 38] << 8) + cis[i + 37],
						(cis[i + 40] << 8) + cis[i + 39],
						(cis[i + 42] << 8) + cis[i + 41],
					        (cis[i + 44] << 8) + cis[i + 43],
						(cis[i + 46] << 8) + cis[i + 45],
						(cis[i + 48] << 8) + cis[i + 47]);
					break;

				case HNBU_SUBBAND5GVER:
					varbuf_append(&b, vstr_subband5gver,
					        (cis[i + 2] << 8) + cis[i + 1]);
					break;

				case HNBU_PAPARAMBWVER:
					varbuf_append(&b, vstr_paparambwver, cis[i + 1]);
					break;

				case HNBU_USB30U1U2:
					varbuf_append(&b, vstr_usb30u1u2,
						(cis[i + 2] << 8) | cis[i + 1]);
					break;

				case HNBU_USB30REGS0:
					varbuf_append(&b, vstr_usb30regs0,
						(cis[i+4] << 24) | (cis[i+3] << 16) |
						(cis[i+2] << 8) | cis[i+1],
						(cis[i+8] << 24) | (cis[i+7] << 16) |
						(cis[i+6] << 8) | cis[i+5],
						(cis[i+12] << 24) | (cis[i+11] << 16) |
						(cis[i+10] << 8) | cis[i+9],
						(cis[i+16] << 24) | (cis[i+15] << 16) |
						(cis[i+14] << 8) | cis[i+13],
						(cis[i+20] << 24) | (cis[i+19] << 16) |
						(cis[i+18] << 8) | cis[i+17],
						(cis[i+24] << 24) | (cis[i+23] << 16) |
						(cis[i+22] << 8) | cis[i+21],
						(cis[i+28] << 24) | (cis[i+27] << 16) |
						(cis[i+26] << 8) | cis[i+25],
						(cis[i+32] << 24) | (cis[i+31] << 16) |
						(cis[i+30] << 8) | cis[i+29],
						(cis[i+36] << 24) | (cis[i+35] << 16) |
						(cis[i+34] << 8) | cis[i+33],
						(cis[i+40] << 24) | (cis[i+39] << 16) |
						(cis[i+38] << 8) | cis[i+37]);
					break;

				case HNBU_USB30REGS1:
					varbuf_append(&b, vstr_usb30regs1,
						(cis[i+4] << 24) | (cis[i+3] << 16) |
						(cis[i+2] << 8) | cis[i+1],
						(cis[i+8] << 24) | (cis[i+7] << 16) |
						(cis[i+6] << 8) | cis[i+5],
						(cis[i+12] << 24) | (cis[i+11] << 16) |
						(cis[i+10] << 8) | cis[i+9],
						(cis[i+16] << 24) | (cis[i+15] << 16) |
						(cis[i+14] << 8) | cis[i+13],
						(cis[i+20] << 24) | (cis[i+19] << 16) |
						(cis[i+18] << 8) | cis[i+17],
						(cis[i+24] << 24) | (cis[i+23] << 16) |
						(cis[i+22] << 8) | cis[i+21],
						(cis[i+28] << 24) | (cis[i+27] << 16) |
						(cis[i+26] << 8) | cis[i+25],
						(cis[i+32] << 24) | (cis[i+31] << 16) |
						(cis[i+30] << 8) | cis[i+29],
						(cis[i+36] << 24) | (cis[i+35] << 16) |
						(cis[i+34] << 8) | cis[i+33],
						(cis[i+40] << 24) | (cis[i+39] << 16) |
						(cis[i+38] << 8) | cis[i+37]);
					break;
				case HNBU_TXBFRPCALS:
				/* note: all 5 rpcal parameters are expected to be */
				/* inside one tuple record, i.e written with one  */
				/* wl wrvar cmd as follows: */
				/* wl wrvar rpcal2g=0x1211 ... rpcal5gb3=0x0  */
					if (tlen != 11 ) { /* sanity check */
						BS_ERROR(("%s:incorrect length:%d for"
							" HNBU_TXBFRPCALS tuple\n",
							__FUNCTION__, tlen));
						break;
					}

					varbuf_append(&b, vstr_paparamrpcalvars[0],
						(cis[i + 1] + (cis[i + 2] << 8)));
					varbuf_append(&b, vstr_paparamrpcalvars[1],
						(cis[i + 3]  +  (cis[i + 4] << 8)));
					varbuf_append(&b, vstr_paparamrpcalvars[2],
						(cis[i + 5]  +  (cis[i + 6] << 8)));
					varbuf_append(&b, vstr_paparamrpcalvars[3],
						(cis[i + 7]  +  (cis[i + 8] << 8)));
					varbuf_append(&b, vstr_paparamrpcalvars[4],
						(cis[i + 9]  +  (cis[i + 10] << 8)));
					break;

				case HNBU_GPIO_PULL_DOWN:
					varbuf_append(&b, vstr_gpdn,
					              (cis[i + 4] << 24) |
					              (cis[i + 3] << 16) |
					              (cis[i + 2] << 8) |
					              cis[i + 1]);
					break;
				} /* CISTPL_BRCM_HNBU */
				break;
			} /* switch (tup) */

			i += tlen;
		} while (tup != CISTPL_END);
	}

	if (boardnum != -1) {
		varbuf_append(&b, vstr_boardnum, boardnum);
	}

	if (eabuf[0]) {
		varbuf_append(&b, vstr_macaddr, eabuf);
	}

#ifndef BCM_BOOTLOADER
	/* if there is no antenna gain field, set default */
	if (sromrev <= 10 && getvar(NULL, rstr_ag0) == NULL && ag_init == FALSE) {
		varbuf_append(&b, vstr_ag, 0, 0xff);
	}
#endif

#if defined(BCMUSBDEV_BMAC) || defined(BCM_BMAC_VARS_APPEND)
	varbuf_append(&b, vstr_end, NULL);
#endif /* BCMUSBDEV_BMAC */

	/* final nullbyte terminator */
	ASSERT(b.size >= 1);
	*b.buf++ = '\0';

	ASSERT(b.buf - base <= MAXSZ_NVRAM_VARS);

	/* initvars_table() MALLOCs, copies and assigns the MALLOCed buffer to '*vars' */
	err = initvars_table(osh, base /* start */, b.buf /* end */, vars, count);

	MFREE(osh, base, MAXSZ_NVRAM_VARS);
	return err;
}
#endif /* !defined(BCMDONGLEHOST) */

/** set PCMCIA sprom command register */
static int
sprom_cmd_pcmcia(osl_t *osh, uint8 cmd)
{
	uint8 status = 0;
	uint wait_cnt = 1000;

	/* write sprom command register */
	OSL_PCMCIA_WRITE_ATTR(osh, SROM_CS, &cmd, 1);

	/* wait status */
	while (wait_cnt--) {
		OSL_PCMCIA_READ_ATTR(osh, SROM_CS, &status, 1);
		if (status & SROM_DONE)
			return 0;
	}

	return 1;
}

/** read a word from the PCMCIA srom */
static int
sprom_read_pcmcia(osl_t *osh, uint16 addr, uint16 *data)
{
	uint8 addr_l, addr_h, data_l, data_h;

	addr_l = (uint8)((addr * 2) & 0xff);
	addr_h = (uint8)(((addr * 2) >> 8) & 0xff);

	/* set address */
	OSL_PCMCIA_WRITE_ATTR(osh, SROM_ADDRH, &addr_h, 1);
	OSL_PCMCIA_WRITE_ATTR(osh, SROM_ADDRL, &addr_l, 1);

	/* do read */
	if (sprom_cmd_pcmcia(osh, SROM_READ))
		return 1;

	/* read data */
	data_h = data_l = 0;
	OSL_PCMCIA_READ_ATTR(osh, SROM_DATAH, &data_h, 1);
	OSL_PCMCIA_READ_ATTR(osh, SROM_DATAL, &data_l, 1);

	*data = (data_h << 8) | data_l;
	return 0;
}

#if defined(WLTEST) || defined(DHD_SPROM) || defined(BCMDBG)
/** write a word to the PCMCIA srom */
static int
sprom_write_pcmcia(osl_t *osh, uint16 addr, uint16 data)
{
	uint8 addr_l, addr_h, data_l, data_h;

	addr_l = (uint8)((addr * 2) & 0xff);
	addr_h = (uint8)(((addr * 2) >> 8) & 0xff);
	data_l = (uint8)(data & 0xff);
	data_h = (uint8)((data >> 8) & 0xff);

	/* set address */
	OSL_PCMCIA_WRITE_ATTR(osh, SROM_ADDRH, &addr_h, 1);
	OSL_PCMCIA_WRITE_ATTR(osh, SROM_ADDRL, &addr_l, 1);

	/* write data */
	OSL_PCMCIA_WRITE_ATTR(osh, SROM_DATAH, &data_h, 1);
	OSL_PCMCIA_WRITE_ATTR(osh, SROM_DATAL, &data_l, 1);

	/* do write */
	return sprom_cmd_pcmcia(osh, SROM_WRITE);
}
#endif 

/**
 * In chips with chipcommon rev 32 and later, the srom is in chipcommon,
 * not in the bus cores.
 */
static uint16
srom_cc_cmd(si_t *sih, osl_t *osh, void *ccregs, uint32 cmd, uint wordoff, uint16 data)
{
	chipcregs_t *cc = (chipcregs_t *)ccregs;
	uint wait_cnt = 1000;

	if ((cmd == SRC_OP_READ) || (cmd == SRC_OP_WRITE)) {
		W_REG(osh, &cc->sromaddress, wordoff * 2);
		if (cmd == SRC_OP_WRITE)
			W_REG(osh, &cc->sromdata, data);
	}

	W_REG(osh, &cc->sromcontrol, SRC_START | cmd);

	while (wait_cnt--) {
		if ((R_REG(osh, &cc->sromcontrol) & SRC_BUSY) == 0)
			break;
	}

	if (!wait_cnt) {
		BS_ERROR(("%s: Command 0x%x timed out\n", __FUNCTION__, cmd));
		return 0xffff;
	}
	if (cmd == SRC_OP_READ)
		return (uint16)R_REG(osh, &cc->sromdata);
	else
		return 0xffff;
}

static int
sprom_read_pci(osl_t *osh, si_t *sih, uint16 *sprom, uint wordoff, uint16 *buf, uint nwords,
	bool check_crc)
{
	int err = 0;
	uint i;
	void *ccregs = NULL;
	uint32 ccval = 0;

	if ((CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43431_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43462_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM4352_CHIP_ID)) {
		/* save current control setting */
		ccval = si_chipcontrl_read(sih);
	}

	if ((CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43431_CHIP_ID)) {
		/* Disable Ext PA lines to allow reading from SROM */
		si_chipcontrl_epa4331(sih, FALSE);
	} else if (((CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43462_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4352_CHIP_ID)) &&
		(CHIPREV(sih->chiprev) <= 2)) {
		si_chipcontrl_srom4360(sih, TRUE);
	}

	/* read the sprom */
	for (i = 0; i < nwords; i++) {

		if (sih->ccrev > 31 && ISSIM_ENAB(sih)) {
			/* use indirect since direct is too slow on QT */
			if ((sih->cccaps & CC_CAP_SROM) == 0) {
				err = 1;
				goto error;
			}

			ccregs = (void *)((uint8 *)sprom - CC_SROM_OTP);
			buf[i] = srom_cc_cmd(sih, osh, ccregs, SRC_OP_READ, wordoff + i, 0);

		} else {
			if (ISSIM_ENAB(sih))
				buf[i] = R_REG(osh, &sprom[wordoff + i]);

			buf[i] = R_REG(osh, &sprom[wordoff + i]);
		}

	}

	/* bypass crc checking for simulation to allow srom hack */
	if (ISSIM_ENAB(sih)) {
		goto error;
	}

#if defined(WLTEST) && defined(MACOSX)
	if ((CHIPID(sih->chip) == BCM4350_CHIP_ID)) {
		goto error;
	}
#endif

	if (check_crc) {

		if (buf[0] == 0xffff) {
			/* The hardware thinks that an srom that starts with 0xffff
			 * is blank, regardless of the rest of the content, so declare
			 * it bad.
			 */
			BS_ERROR(("%s: buf[0] = 0x%x, returning bad-crc\n", __FUNCTION__, buf[0]));
			err = 1;
			goto error;
		}

		/* fixup the endianness so crc8 will pass */
		htol16_buf(buf, nwords * 2);
		if (hndcrc8((uint8 *)buf, nwords * 2, CRC8_INIT_VALUE) != CRC8_GOOD_VALUE) {
			/* DBG only pci always read srom4 first, then srom8/9 */
			/* BS_ERROR(("%s: bad crc\n", __FUNCTION__)); */
			err = 1;
		}
		/* now correct the endianness of the byte array */
		ltoh16_buf(buf, nwords * 2);
	}

error:
	if ((CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43431_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43462_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM4352_CHIP_ID) ||
	    BCM4350_CHIP(sih->chip)) {
		/* Restore config after reading SROM */
		si_chipcontrl_restore(sih, ccval);
	}

	return err;
}

#if !defined(BCMDONGLEHOST)
#if defined(BCMNVRAMW) || defined(BCMNVRAMR)
static int
BCMATTACHFN(otp_read_pci)(osl_t *osh, si_t *sih, uint16 *buf, uint bufsz)
{
	uint8 *otp;
	uint sz = OTP_SZ_MAX/2; /* size in words */
	int err = 0;

	ASSERT(bufsz <= OTP_SZ_MAX);

	if ((otp = MALLOC(osh, OTP_SZ_MAX)) == NULL) {
		return BCME_ERROR;
	}

	bzero(otp, OTP_SZ_MAX);

	err = otp_read_region(sih, OTP_HW_RGN, (uint16 *)otp, &sz);

	if (err) {
		MFREE(osh, otp, OTP_SZ_MAX);
		return err;
	}

	bcopy(otp, buf, bufsz);

	/* Check CRC */
	if (((uint16 *)otp)[0] == 0xffff) {
		/* The hardware thinks that an srom that starts with 0xffff
		 * is blank, regardless of the rest of the content, so declare
		 * it bad.
		 */
		BS_ERROR(("%s: otp[0] = 0x%x, returning bad-crc\n",
		          __FUNCTION__, ((uint16 *)otp)[0]));
		MFREE(osh, otp, OTP_SZ_MAX);
		return 1;
	}

	/* fixup the endianness so crc8 will pass */
	htol16_buf(otp, OTP_SZ_MAX);
	if (hndcrc8(otp, SROM4_WORDS * 2, CRC8_INIT_VALUE) != CRC8_GOOD_VALUE &&
		hndcrc8(otp, SROM10_WORDS * 2, CRC8_INIT_VALUE) != CRC8_GOOD_VALUE &&
		hndcrc8(otp, SROM11_WORDS * 2, CRC8_INIT_VALUE) != CRC8_GOOD_VALUE) {
		BS_ERROR(("%s: bad crc\n", __FUNCTION__));
		err = 1;
	}

	MFREE(osh, otp, OTP_SZ_MAX);

	return err;
}
#endif /* defined(BCMNVRAMW) || defined(BCMNVRAMR) */
#endif /* !defined(BCMDONGLEHOST) */

int
srom_otp_write_region_crc(si_t *sih, uint nbytes, uint16* buf16, bool write)
{
#if defined(WLTEST) || defined(BCMDBG)
	int err = 0, crc = 0;
#if !defined(BCMDONGLEHOST)
	uint8 *buf8;

	/* Check nbytes is not odd or too big */
	if ((nbytes & 1) || (nbytes > SROM_MAX))
		return 1;

	/* block invalid buffer size */
	if (nbytes < SROM4_WORDS * 2)
		return BCME_BUFTOOSHORT;
	else if (nbytes > SROM11_WORDS * 2)
		return BCME_BUFTOOLONG;

	/* Verify signatures */
	if (!((buf16[SROM4_SIGN] == SROM4_SIGNATURE) ||
		(buf16[SROM8_SIGN] == SROM4_SIGNATURE) ||
		(buf16[SROM10_SIGN] == SROM4_SIGNATURE) ||
		(buf16[SROM11_SIGN] == SROM11_SIGNATURE))) {
		BS_ERROR(("%s: wrong signature SROM4_SIGN %x SROM8_SIGN %x SROM10_SIGN %x\n",
			__FUNCTION__, buf16[SROM4_SIGN], buf16[SROM8_SIGN], buf16[SROM10_SIGN]));
		return BCME_ERROR;
	}

	/* Check CRC */
	if (buf16[0] == 0xffff) {
		/* The hardware thinks that an srom that starts with 0xffff
		 * is blank, regardless of the rest of the content, so declare
		 * it bad.
		 */
		BS_ERROR(("%s: invalid buf16[0] = 0x%x\n", __FUNCTION__, buf16[0]));
		goto out;
	}

	buf8 = (uint8*)buf16;
	/* fixup the endianness and then calculate crc */
	htol16_buf(buf8, nbytes);
	crc = ~hndcrc8(buf8, nbytes - 1, CRC8_INIT_VALUE);
	/* now correct the endianness of the byte array */
	ltoh16_buf(buf8, nbytes);

	if (nbytes == SROM11_WORDS * 2)
		buf16[SROM11_CRCREV] = (crc << 8) | (buf16[SROM11_CRCREV] & 0xff);
	else if (nbytes == SROM10_WORDS * 2)
		buf16[SROM10_CRCREV] = (crc << 8) | (buf16[SROM10_CRCREV] & 0xff);
	else
		buf16[SROM4_CRCREV] = (crc << 8) | (buf16[SROM4_CRCREV] & 0xff);

#ifdef BCMNVRAMW
	/* Write the CRC back */
	if (write)
		err = otp_write_region(sih, OTP_HW_RGN, buf16, nbytes/2, 0);
#endif /* BCMNVRAMW */

out:
#endif /* !defined(BCMDONGLEHOST) */
	return write ? err : crc;
#else
	return 0;
#endif 
}

#if !defined(BCMDONGLEHOST)
/**
 * Create variable table from memory.
 * Return 0 on success, nonzero on error.
 */
static int
BCMATTACHFN(initvars_table)(osl_t *osh, char *start, char *end, char **vars, uint *count)
{
	int c = (int)(end - start);

	/* do it only when there is more than just the null string */
	if (c > 1) {
		char *vp = MALLOC(osh, c);
		ASSERT(vp != NULL);
		if (!vp)
			return BCME_NOMEM;
		bcopy(start, vp, c);
		*vars = vp;
		*count = c;
	}
	else {
		*vars = NULL;
		*count = 0;
	}

	return 0;
}

int
BCMATTACHFN(dbushost_initvars_flash)(si_t *sih, osl_t *osh, char **base, uint len)
{
	return initvars_flash(sih, osh, base, len);
}

/**
 * Find variables with <devpath> from flash. 'base' points to the beginning
 * of the table upon enter and to the end of the table upon exit when success.
 * Return 0 on success, nonzero on error.
 */
static int
BCMATTACHFN(initvars_flash)(si_t *sih, osl_t *osh, char **base, uint len)
{
	char *vp = *base;
	char *flash;
	int err;
	char *s;
	uint l, dl, copy_len;
	char devpath[SI_DEVPATH_BUFSZ], devpath_pcie[SI_DEVPATH_BUFSZ];
	char coded_name[SI_DEVPATH_BUFSZ] = {0};
	int path_len, coded_len, devid_len, pcie_path_len;

	/* allocate memory and read in flash */
	if (!(flash = MALLOC(osh, MAX_NVRAM_SPACE)))
		return BCME_NOMEM;
	if ((err = nvram_getall(flash, MAX_NVRAM_SPACE)))
		goto exit;

	/* create legacy devpath prefix */
	si_devpath(sih, devpath, sizeof(devpath));
	path_len = strlen(devpath);

	if (BUSTYPE(sih->bustype) == PCI_BUS) {
		si_devpath_pcie(sih, devpath_pcie, sizeof(devpath_pcie));
		pcie_path_len = strlen(devpath_pcie);
	} else
		pcie_path_len = 0;

	/* create coded devpath prefix */
	si_coded_devpathvar(sih, coded_name, sizeof(coded_name), "devid");

	/* coded_name now is 'xx:devid, eat ending 'devid' */
	/* to be 'xx:' */
	devid_len = strlen("devid");
	coded_len = strlen(coded_name);
	if (coded_len > devid_len) {
		coded_name[coded_len - devid_len] = '\0';
		coded_len -= devid_len;
	}
	else
		coded_len = 0;

	/* grab vars with the <devpath> prefix or <coded_name> previx in name */
	for (s = flash; s && *s; s += l + 1) {
		l = strlen(s);

		/* skip non-matching variable */
		if (strncmp(s, devpath, path_len) == 0)
			dl = path_len;
		else if (pcie_path_len && strncmp(s, devpath_pcie, pcie_path_len) == 0)
			dl = pcie_path_len;
		else if (coded_len && strncmp(s, coded_name, coded_len) == 0)
			dl = coded_len;
		else
			continue;

		/* is there enough room to copy? */
		copy_len = l - dl + 1;
		if (len < copy_len) {
			err = BCME_BUFTOOSHORT;
			goto exit;
		}

		/* no prefix, just the name=value */
		strncpy(vp, &s[dl], copy_len);
		vp += copy_len;
		len -= copy_len;
	}

	/* add null string as terminator */
	if (len < 1) {
		err = BCME_BUFTOOSHORT;
		goto exit;
	}
	*vp++ = '\0';

	*base = vp;

exit:	MFREE(osh, flash, MAX_NVRAM_SPACE);
	return err;
}
#endif /* !defined(BCMDONGLEHOST) */

#if !defined(BCMUSBDEV_ENABLED) && !defined(BCMSDIODEV_ENABLED) && !defined(BCM_OL_DEV) \
	&&!defined(BCMPCIEDEV_ENABLED)
#if !defined(BCMDONGLEHOST)
/**
 * Initialize nonvolatile variable table from flash.
 * Return 0 on success, nonzero on error.
 */
static int
BCMATTACHFN(initvars_flash_si)(si_t *sih, char **vars, uint *count)
{
	osl_t *osh = si_osh(sih);
	char *vp, *base;
	int err;

	ASSERT(vars != NULL);
	ASSERT(count != NULL);

	base = vp = MALLOC(osh, MAXSZ_NVRAM_VARS);
	ASSERT(vp != NULL);
	if (!vp)
		return BCME_NOMEM;

	if ((err = initvars_flash(sih, osh, &vp, MAXSZ_NVRAM_VARS)) == 0)
		err = initvars_table(osh, base, vp, vars, count);

	MFREE(osh, base, MAXSZ_NVRAM_VARS);

	return err;
}
#endif /* !defined(BCMDONGLEHOST) */
#endif	

#if !defined(BCMDONGLEHOST)

/** returns position of rightmost bit that was set in caller supplied mask */
static uint
mask_shift(uint16 mask)
{
	uint i;
	for (i = 0; i < (sizeof(mask) << 3); i ++) {
		if (mask & (1 << i))
			return i;
	}
	ASSERT(mask);
	return 0;
}

static uint
mask_width(uint16 mask)
{
	int i;
	for (i = (sizeof(mask) << 3) - 1; i >= 0; i --) {
		if (mask & (1 << i))
			return (uint)(i - mask_shift(mask) + 1);
	}
	ASSERT(mask);
	return 0;
}

#ifdef BCMASSERT_SUPPORT
static bool
mask_valid(uint16 mask)
{
	uint shift = mask_shift(mask);
	uint width = mask_width(mask);
	return mask == ((~0 << shift) & ~(~0 << (shift + width)));
}
#endif

/**
 * Parses caller supplied SROM contents into name=value pairs. Global array pci_sromvars[] contains
 * the link between a word offset in SROM and the corresponding NVRAM variable name.'srom' points to
 * the SROM word array. 'off' specifies the offset of the first word 'srom' points to, which should
 * be either 0 or SROM3_SWRG_OFF (full SROM or software region).
 */
static void
BCMATTACHFN(_initvars_srom_pci)(uint8 sromrev, uint16 *srom, uint off, varbuf_t *b)
{
	uint16 w;
	uint32 val;
	const sromvar_t *srv;
	uint width;
	uint flags;
	uint32 sr = (1 << sromrev);
	bool in_array = FALSE;
	static char array_temp[256];
	uint array_curr = 0;
	const char* array_name = NULL;

	varbuf_append(b, "sromrev=%d", sromrev);

	for (srv = pci_sromvars; srv->name != NULL; srv ++) {
		const char *name;
		static bool in_array2 = FALSE;
		static char array_temp2[256];
		static uint array_curr2 = 0;
		static const char* array_name2 = NULL;

		if ((srv->revmask & sr) == 0)
			continue;

		if (srv->off < off)
			continue;

		flags = srv->flags;
		name = srv->name;

		/* This entry is for mfgc only. Don't generate param for it, */
		if (flags & SRFL_NOVAR)
			continue;

		if (flags & SRFL_ETHADDR) {
			char eabuf[ETHER_ADDR_STR_LEN];
			struct ether_addr ea;

			ea.octet[0] = (srom[srv->off - off] >> 8) & 0xff;
			ea.octet[1] = srom[srv->off - off] & 0xff;
			ea.octet[2] = (srom[srv->off + 1 - off] >> 8) & 0xff;
			ea.octet[3] = srom[srv->off + 1 - off] & 0xff;
			ea.octet[4] = (srom[srv->off + 2 - off] >> 8) & 0xff;
			ea.octet[5] = srom[srv->off + 2 - off] & 0xff;
			bcm_ether_ntoa(&ea, eabuf);

			varbuf_append(b, "%s=%s", name, eabuf);
		} else {
#ifdef BCMASSERT_SUPPORT
			ASSERT(mask_valid(srv->mask));
#endif /* BCMASSERT_SUPPORT */
			ASSERT(mask_width(srv->mask));

			/* Start of an array */
			if (sromrev >= 10 && (srv->flags & SRFL_ARRAY) && !in_array2) {
				array_curr2 = 0;
				array_name2 = (const char*)srv->name;
				memset((void*)array_temp2, 0, sizeof(array_temp2));
				in_array2 = TRUE;
			}

			w = srom[srv->off - off];
			val = (w & srv->mask) >> mask_shift(srv->mask);
			width = mask_width(srv->mask);

			while (srv->flags & SRFL_MORE) {
				srv ++;
				ASSERT(srv->name != NULL);

				if (srv->off == 0 || srv->off < off)
					continue;

#ifdef BCMASSERT_SUPPORT
				ASSERT(mask_valid(srv->mask));
#endif /* BCMASSERT_SUPPORT */
				ASSERT(mask_width(srv->mask));

				w = srom[srv->off - off];
				val += ((w & srv->mask) >> mask_shift(srv->mask)) << width;
				width += mask_width(srv->mask);
			}

			if ((flags & SRFL_NOFFS) && ((int)val == (1 << width) - 1))
				continue;

			/* Array support starts in sromrev 10. Skip arrays for sromrev <= 9 */
			if (sromrev <= 9 && srv->flags & SRFL_ARRAY) {
				while (srv->flags & SRFL_ARRAY)
					srv ++;
				srv ++;
			}

			if (in_array2) {
				int ret;

				if (flags & SRFL_PRHEX) {
					ret = snprintf(array_temp2 + array_curr2,
						sizeof(array_temp2) - array_curr2, "0x%x,", val);
				} else if ((flags & SRFL_PRSIGN) &&
					(val & (1 << (width - 1)))) {
					ret = snprintf(array_temp2 + array_curr2,
						sizeof(array_temp2) - array_curr2, "%d,",
						(int)(val | (~0 << width)));
				} else {
					ret = snprintf(array_temp2 + array_curr2,
						sizeof(array_temp2) - array_curr2, "%u,", val);
				}

				if (ret > 0) {
					array_curr2 += ret;
				} else {
					BS_ERROR(("%s: array %s parsing error. buffer too short.\n",
						__FUNCTION__, array_name2));
					ASSERT(0);

					/* buffer too small, skip this param */
					while (srv->flags & SRFL_ARRAY)
						srv ++;
					srv ++;
					in_array2 = FALSE;
					continue;
				}

				if (!(srv->flags & SRFL_ARRAY)) { /* Array ends */
					/* Remove the last ',' */
					array_temp2[array_curr2-1] = '\0';
					in_array2 = FALSE;
					varbuf_append(b, "%s=%s", array_name2, array_temp2);
				}
			} else if (flags & SRFL_CCODE) {
				if (val == 0)
					varbuf_append(b, "ccode=");
				else
					varbuf_append(b, "ccode=%c%c", (val >> 8), (val & 0xff));
			} else if (flags & SRFL_LEDDC) {
				/* LED Powersave duty cycle has to be scaled:
				*(oncount >> 24) (offcount >> 8)
				*/
				uint32 w32 = (((val >> 8) & 0xff) << 24) | /* oncount */
					     (((val & 0xff)) << 8); /* offcount */
				varbuf_append(b, "leddc=%d", w32);
			} else if (flags & SRFL_PRHEX) {
				varbuf_append(b, "%s=0x%x", name, val);
			} else if ((flags & SRFL_PRSIGN) && (val & (1 << (width - 1)))) {
				varbuf_append(b, "%s=%d", name, (int)(val | (~0 << width)));
			} else {
				varbuf_append(b, "%s=%u", name, val);
			}
		}
	}

	if (sromrev >= 4) {
		/* Do per-path variables */
		uint p, pb, psz, path_num;

		if (sromrev >= 11) {
			pb = SROM11_PATH0;
			psz = SROM11_PATH1 - SROM11_PATH0;
			path_num = MAX_PATH_SROM_11;
		} else if (sromrev >= 8) {
			pb = SROM8_PATH0;
			psz = SROM8_PATH1 - SROM8_PATH0;
			path_num = MAX_PATH_SROM;
		} else {
			pb = SROM4_PATH0;
			psz = SROM4_PATH1 - SROM4_PATH0;
			path_num = MAX_PATH_SROM;
		}

		for (p = 0; p < path_num; p++) {
			for (srv = perpath_pci_sromvars; srv->name != NULL; srv ++) {
				if ((srv->revmask & sr) == 0)
					continue;

				if (pb + srv->off < off)
					continue;

				/* This entry is for mfgc only. Don't generate param for it, */
				if (srv->flags & SRFL_NOVAR)
					continue;

				/* Start of an array */
				if (sromrev >= 10 && (srv->flags & SRFL_ARRAY) && !in_array) {
					array_curr = 0;
					array_name = (const char*)srv->name;
					memset((void*)array_temp, 0, sizeof(array_temp));
					in_array = TRUE;
				}

				w = srom[pb + srv->off - off];

#ifdef BCMASSERT_SUPPORT
				ASSERT(mask_valid(srv->mask));
#endif /* BCMASSERT_SUPPORT */
				val = (w & srv->mask) >> mask_shift(srv->mask);
				width = mask_width(srv->mask);

				flags = srv->flags;

				/* Cheating: no per-path var is more than 1 word */

				if ((srv->flags & SRFL_NOFFS) && ((int)val == (1 << width) - 1))
					continue;

				if (in_array) {
					int ret;

					if (flags & SRFL_PRHEX) {
						ret = snprintf(array_temp + array_curr,
						  sizeof(array_temp) - array_curr, "0x%x,", val);
					} else if ((flags & SRFL_PRSIGN) &&
						(val & (1 << (width - 1)))) {
						ret = snprintf(array_temp + array_curr,
							sizeof(array_temp) - array_curr, "%d,",
							(int)(val | (~0 << width)));
					} else {
						ret = snprintf(array_temp + array_curr,
						  sizeof(array_temp) - array_curr, "%u,", val);
					}

					if (ret > 0) {
						array_curr += ret;
					} else {
						BS_ERROR(
						("%s: array %s parsing error. buffer too short.\n",
						__FUNCTION__, array_name));
						ASSERT(0);

						/* buffer too small, skip this param */
						while (srv->flags & SRFL_ARRAY)
							srv ++;
						srv ++;
						in_array = FALSE;
						continue;
					}

					if (!(srv->flags & SRFL_ARRAY)) { /* Array ends */
						/* Remove the last ',' */
						array_temp[array_curr-1] = '\0';
						in_array = FALSE;
						varbuf_append(b, "%s%d=%s",
							array_name, p, array_temp);
					}
				} else if (srv->flags & SRFL_PRHEX)
					varbuf_append(b, "%s%d=0x%x", srv->name, p, val);
				else
					varbuf_append(b, "%s%d=%d", srv->name, p, val);
			}
			pb += psz;
		}
	} /* per path variables */
} /* _initvars_srom_pci */


/**
 * Initialize nonvolatile variable table from sprom, or OTP when SPROM is not available, or
 * optionally a set of 'defaultsromvars' (compiled-in) variables when both OTP and SPROM bear no
 * contents.
 *
 * On success, a buffer containing var/val pairs is allocated and returned in params vars and count.
 *
 * Return 0 on success, nonzero on error.
 */
static int
BCMATTACHFN(initvars_srom_pci)(si_t *sih, void *curmap, char **vars, uint *count)
{
	uint16 *srom, *sromwindow;
	uint8 sromrev = 0;
	uint32 sr;
	varbuf_t b;
	char *vp, *base = NULL;
	osl_t *osh = si_osh(sih);
	bool flash = FALSE;
	int err = 0;

	/*
	 * Apply CRC over SROM content regardless SROM is present or not, and use variable
	 * <devpath>sromrev's existance in flash to decide if we should return an error when CRC
	 * fails or read SROM variables from flash.
	 */
	srom = MALLOC(osh, SROM_MAX);
	ASSERT(srom != NULL);
	if (!srom)
		return -2;

	sromwindow = (uint16 *)srom_offset(sih, curmap);
	if (si_is_sprom_available(sih)) {
		err = sprom_read_pci(osh, sih, sromwindow, 0, srom, SROM11_WORDS, TRUE);

		if (err == 0) {
			if (srom[SROM11_SIGN] == SROM11_SIGNATURE)		/* srom 11  */
				sromrev = srom[SROM11_CRCREV] & 0xff;
#if defined(WLTEST) && defined(MACOSX)
			if (CHIPID(sih->chip) == BCM4350_CHIP_ID)
				sromrev = 11;
#endif
		} else {
			err = sprom_read_pci(osh, sih, sromwindow, 0, srom, SROM4_WORDS, TRUE);

			if (err == 0) {
				if ((srom[SROM4_SIGN] == SROM4_SIGNATURE) ||	/* srom 4    */
				    (srom[SROM8_SIGN] == SROM4_SIGNATURE) ) { 	/* srom 8, 9 */
					sromrev = srom[SROM4_CRCREV] & 0xff;
				}
			} else {
				err = sprom_read_pci(osh, sih, sromwindow, 0,
					srom, SROM_WORDS, TRUE);

				if (err == 0) {
					/* srom is good and is rev < 4 */
					/* top word of sprom contains version and crc8 */
					sromrev = srom[SROM_CRCREV] & 0xff;
					/* bcm4401 sroms misprogrammed */
					if (sromrev == 0x10)
						sromrev = 1;
				}
			}
		}
	}

#if defined(BCMNVRAMW) || defined(BCMNVRAMR)
	/* Use OTP if SPROM not available */
	else if ((err = otp_read_pci(osh, sih, srom, SROM_MAX)) == 0) {
		/* OTP only contain SROM rev8/rev9/rev10/Rev11 for now */
		if (srom[SROM11_SIGN] == SROM11_SIGNATURE)
			sromrev = srom[SROM11_CRCREV] & 0xff;
		else if (srom[SROM10_SIGN] == SROM10_SIGNATURE)
			sromrev = srom[SROM10_CRCREV] & 0xff;
		else
			sromrev = srom[SROM4_CRCREV] & 0xff;
	}
#endif /* defined(BCMNVRAMW) || defined(BCMNVRAMR) */
	else {
		err = 1;
		BS_ERROR(("Neither SPROM nor OTP has valid image\n"));
	}

	BS_ERROR(("srom rev:%d\n", sromrev));


	/* We want internal/wltest driver to come up with default sromvars so we can
	 * program a blank SPROM/OTP.
	 */
	if (err || sromrev == 0) {
		char *value;
#if defined(BCMHOSTVARS)
		uint32 val;
#endif

		if ((value = si_getdevpathvar(sih, "sromrev"))) {
			sromrev = (uint8)bcm_strtoul(value, NULL, 0);
			flash = TRUE;
			goto varscont;
		}

		BS_ERROR(("%s, SROM CRC Error\n", __FUNCTION__));

#ifndef DONGLEBUILD
		if ((value = si_getnvramflvar(sih, "sromrev"))) {
			err = 0;
			goto errout;
		}
#endif
/* BCMHOSTVARS is enabled only if WLTEST is enabled or BCMEXTNVM is enabled */
#if defined(BCMHOSTVARS)
		val = OSL_PCI_READ_CONFIG(osh, PCI_SPROM_CONTROL, sizeof(uint32));
		if ((si_is_sprom_available(sih) && srom[0] == 0xffff) ||
#ifdef BCMQT
			(si_is_sprom_available(sih) && sromrev == 0) ||
#endif
			(val & SPROM_OTPIN_USE)) {
			vp = base = mfgsromvars;

			if (defvarslen == 0) {
				BS_ERROR(("No nvm file, use generic default (for programming"
					" SPROM/OTP only)\n"));

				if (((CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
					(CHIPID(sih->chip) == BCM43431_CHIP_ID)) &&
					(CHIPREV(sih->chiprev) < 3)) {

					defvarslen = srom_vars_len(defaultsromvars_4331);
					bcopy(defaultsromvars_4331, vp, defvarslen);
				} else if ((CHIPID(sih->chip) == BCM4350_CHIP_ID)) {
					/* For 4350 B1 and older */
					if (CHIPREV(sih->chiprev) <= 2) {
						defvarslen = srom_vars_len(defaultsromvars_4350);
						bcopy(defaultsromvars_4350, vp, defvarslen);
					} else {
						defvarslen = srom_vars_len(defaultsromvars_4350c0);
						bcopy(defaultsromvars_4350c0, vp, defvarslen);
					}
				} else if ((CHIPID(sih->chip) == BCM43569_CHIP_ID) ||
					(CHIPID(sih->chip) == BCM43570_CHIP_ID)) {
					defvarslen = srom_vars_len(defaultsromvars_4350c0);
					bcopy(defaultsromvars_4350c0, vp, defvarslen);
				} else if ((CHIPID(sih->chip) == BCM4354_CHIP_ID) ||
					(CHIPID(sih->chip) == BCM4356_CHIP_ID)) {
					if (CHIPREV(sih->chiprev) >= 1) {
						defvarslen = srom_vars_len(defaultsromvars_4354a1);
						bcopy(defaultsromvars_4354a1, vp, defvarslen);
					} else {
						defvarslen = srom_vars_len(defaultsromvars_4350c0);
						bcopy(defaultsromvars_4350c0, vp, defvarslen);
					}
				} else if (CHIPID(sih->chip) == BCM4335_CHIP_ID) {
					defvarslen = srom_vars_len(defaultsromvars_4335);
					bcopy(defaultsromvars_4335, vp, defvarslen);
				}
				else if (CHIPID(sih->chip) == BCM4345_CHIP_ID) {
					if (CHIPREV(sih->chiprev) <= 3) {
						defvarslen = srom_vars_len(defaultsromvars_4345);
						bcopy(defaultsromvars_4345, vp, defvarslen);
					} else {
						defvarslen = srom_vars_len(defaultsromvars_4345b0);
						bcopy(defaultsromvars_4345b0, vp, defvarslen);
					}
				} else if ((CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
					(CHIPID(sih->chip) == BCM43462_CHIP_ID)) {
					defvarslen = srom_vars_len(defaultsromvars_43602);
					bcopy(defaultsromvars_43602, vp, defvarslen);
				} else {
					/* For 4311 A1 there is no signature to indicate that OTP is
					 * programmed, so can't really verify the OTP is
					 * unprogrammed or a bad OTP.
					 */
					if (CHIPID(sih->chip) == BCM4311_CHIP_ID) {
						const char *devid = "devid=0x4311";
						const size_t devid_strlen = strlen(devid);
						BS_ERROR(("setting the devid to be 4311\n"));
						bcopy(devid, vp, devid_strlen + 1);
						vp += devid_strlen + 1;
					}
					defvarslen = srom_vars_len(defaultsromvars_wltest);
					bcopy(defaultsromvars_wltest, vp, defvarslen);
				}
			} else {
				BS_ERROR(("Use nvm file as default\n"));
			}

			vp += defvarslen;
			/* add final null terminator */
			*vp++ = '\0';

			BS_ERROR(("Used %d bytes of defaultsromvars\n", defvarslen));
			goto varsdone;

		} else if ((((CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43431_CHIP_ID)) &&
			(CHIPREV(sih->chiprev) < 3)) || (CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43462_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM4352_CHIP_ID)) {
			base = vp = mfgsromvars;

			if ((CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
			    (CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
			    (CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
			    (CHIPID(sih->chip) == BCM43462_CHIP_ID) ||
			    (CHIPID(sih->chip) == BCM4352_CHIP_ID))
				BS_ERROR(("4360 BOOT w/o SPROM or OTP\n"));
			else
				BS_ERROR(("4331 BOOT w/o SPROM or OTP\n"));

			if (defvarslen == 0) {
				if ((CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
					(CHIPID(sih->chip) == BCM43462_CHIP_ID)) {
					defvarslen = srom_vars_len(defaultsromvars_43602);
					bcopy(defaultsromvars_43602, vp, defvarslen);
				} else if ((CHIPID(sih->chip) == BCM4354_CHIP_ID) ||
					(CHIPID(sih->chip) == BCM4356_CHIP_ID)) {
					if (CHIPREV(sih->chiprev) >= 1) {
						defvarslen = srom_vars_len(defaultsromvars_4354a1);
						bcopy(defaultsromvars_4354a1, vp, defvarslen);
					} else {
						defvarslen = srom_vars_len(defaultsromvars_4350c0);
						bcopy(defaultsromvars_4350c0, vp, defvarslen);
					}
				} else {
					defvarslen = srom_vars_len(defaultsromvars_4331);
					bcopy(defaultsromvars_4331, vp, defvarslen);
				}
			}
			vp += defvarslen;
			*vp++ = '\0';
			goto varsdone;
		} else if ((CHIPID(sih->chip) == BCM4335_CHIP_ID) ||
			0) {
			base = vp = mfgsromvars;

			defvarslen = srom_vars_len(defaultsromvars_4335);
			bcopy(defaultsromvars_4335, vp, defvarslen);

			vp += defvarslen;
			*vp++ = '\0';
			goto varsdone;
		} else if (CHIPID(sih->chip) == BCM4345_CHIP_ID) {
			base = vp = mfgsromvars;
			if (CHIPREV(sih->chiprev) <= 3) {
				defvarslen = srom_vars_len(defaultsromvars_4345);
				bcopy(defaultsromvars_4345, vp, defvarslen);
			} else {
				defvarslen = srom_vars_len(defaultsromvars_4345b0);
				bcopy(defaultsromvars_4345b0, vp, defvarslen);
			}

			vp += defvarslen;
			*vp++ = '\0';
			goto varsdone;
		} else
#endif 
		{
			err = -1;
			goto errout;
		}
	}

varscont:
	/* Bitmask for the sromrev */
	sr = 1 << sromrev;

	/* srom version check: Current valid versions: 1-5, 8-11, SROM_MAXREV */
	if ((sr & 0xf3e) == 0) {
		BS_ERROR(("Invalid SROM rev %d\n", sromrev));
		err = -2;
		goto errout;
	}

	ASSERT(vars != NULL);
	ASSERT(count != NULL);

	base = vp = MALLOC(osh, MAXSZ_NVRAM_VARS);
	ASSERT(vp != NULL);
	if (!vp) {
		err = -2;
		goto errout;
	}

	/* read variables from flash */
	if (flash) {
		if ((err = initvars_flash(sih, osh, &vp, MAXSZ_NVRAM_VARS)))
			goto errout;
		goto varsdone;
	}

	varbuf_init(&b, base, MAXSZ_NVRAM_VARS);

	/* parse SROM into name=value pairs. */
	_initvars_srom_pci(sromrev, srom, 0, &b);


	/* final nullbyte terminator */
	ASSERT(b.size >= 1);
	vp = b.buf;
	*vp++ = '\0';

	ASSERT((vp - base) <= MAXSZ_NVRAM_VARS);

varsdone:
	err = initvars_table(osh, base, vp, vars, count); /* allocates buffer in 'vars' */

errout:
/* BCMHOSTVARS are enabled only if WLTEST is enabled or BCMEXTNVM is enabled */
#if defined(BCMHOSTVARS)
	if (base && (base != mfgsromvars))
#else
	if (base)
#endif 
		MFREE(osh, base, MAXSZ_NVRAM_VARS);

	MFREE(osh, srom, SROM_MAX);
	return err;
}

/**
 * initvars_cis_pci() parses OTP CIS.
 * Return error if the content is not in CIS format or OTP is not present.
 */
static int
BCMATTACHFN(initvars_cis_pci)(si_t *sih, osl_t *osh, void *curmap, char **vars, uint *count)
{
	uint wsz = 0, sz = 0, base_len = 0;
	void *oh = NULL;
	int rc = BCME_OK;
	uint16 *cisbuf = NULL;
	uint8 *cis = NULL;
#if defined(BCMHOSTVARS)
	char *vp = NULL;
#endif 
	char *base = NULL;
	bool wasup;
	uint32 min_res_mask = 0;

	/* Bail out if we've dealt with OTP/SPROM before! */
	if (srvars_inited)
		goto exit;

	/* Turn on OTP if it's not already on */
	if (!(wasup = si_is_otp_powered(sih)))
		si_otp_power(sih, TRUE, &min_res_mask);

	if (si_cis_source(sih) != CIS_OTP)
		rc = BCME_NOTFOUND;
	else if ((oh = otp_init(sih)) == NULL)
		rc = BCME_ERROR;
	else if (!(otp_newcis(oh) && (otp_status(oh) & OTPS_GUP_HW)))
		rc = BCME_NOTFOUND;
	else if ((sz = otp_size(oh)) != 0) {
		if ((cisbuf = (uint16*)MALLOC(osh, sz))) {
			/* otp_size() returns bytes, not words. */
			wsz = sz >> 1;
			rc = otp_read_region(sih, OTP_HW_RGN, cisbuf, &wsz);

			/* Bypass the HW header and signature */
			cis = (uint8*)(cisbuf + SROM11_SIGN);
			BS_ERROR(("%s: Parsing CIS in OTP.\n", __FUNCTION__));
		} else
			rc = BCME_NOMEM;
	}

	/* Restore original OTP state */
	if (!wasup)
		si_otp_power(sih, FALSE, &min_res_mask);

	if (rc != BCME_OK) {
		BS_ERROR(("%s: Not CIS format\n", __FUNCTION__));
		goto exit;
	}

#if defined(BCMHOSTVARS)
	if (defvarslen) {
		vp = mfgsromvars;
		vp += defvarslen;

		/* allocates buffer in 'vars' */
		rc = initvars_table(osh, mfgsromvars, vp, &base, &base_len);
		if (rc)
			goto exit;

		*vars = base;
		*count = base_len;

		BS_ERROR(("%s external nvram %d bytes\n", __FUNCTION__, defvarslen));
	}

#endif 

	/* Parse the CIS and allocate a(nother) buffer in 'vars' */
	rc = srom_parsecis(osh, &cis, SROM_CIS_SINGLE, vars, count);

	srvars_inited = TRUE;
exit:
	/* Clean up */
	if (base)
		MFREE(osh, base, base_len);
	if (cisbuf)
		MFREE(osh, cisbuf, sz);

	/* return OK so the driver will load & use defaults if bad srom/otp */
	return rc;
}

/**
 * Read the cis and call parsecis to allocate and initialize the NVRAM vars buffer.
 * Return 0 on success, nonzero on error.
 */
static int
BCMATTACHFN(initvars_cis_pcmcia)(si_t *sih, osl_t *osh, char **vars, uint *count)
{
	uint8 *cis = NULL;
	int rc;
	uint data_sz;

	data_sz = (sih->buscorerev == 1) ? SROM_MAX : CIS_SIZE;

	if ((cis = MALLOC(osh, data_sz)) == NULL)
		return (-2);

	if (sih->buscorerev == 1) {
		if (srom_read(sih, PCMCIA_BUS, (void *)NULL, osh, 0, data_sz, (uint16 *)cis,
		              TRUE)) {
			MFREE(osh, cis, data_sz);
			return (-1);
		}
		/* fix up endianess for 16-bit data vs 8-bit parsing */
		htol16_buf((uint16 *)cis, data_sz);
	} else
		OSL_PCMCIA_READ_ATTR(osh, 0, cis, data_sz);

	rc = srom_parsecis(osh, &cis, SROM_CIS_SINGLE, vars, count);

	MFREE(osh, cis, data_sz);

	return (rc);
}
#endif /* !defined(BCMDONGLEHOST) */


#if !defined(BCMDONGLEHOST)
#ifdef BCMSPI
/**
 * Read the SPI cis and call parsecis to allocate and initialize the NVRAM vars buffer.
 * Return 0 on success, nonzero on error.
 */
static int
BCMATTACHFN(initvars_cis_spi)(osl_t *osh, char **vars, uint *count)
{
	uint8 *cis;
	int rc;

#ifdef NDIS
	uint8 cisd[SBSDIO_CIS_SIZE_LIMIT];
	cis = (uint8*)cisd;
#else
	if ((cis = MALLOC(osh, SBSDIO_CIS_SIZE_LIMIT)) == NULL) {
		return -1;
	}
#endif /* NDIS */

	bzero(cis, SBSDIO_CIS_SIZE_LIMIT);

	if (bcmsdh_cis_read(NULL, SDIO_FUNC_1, cis, SBSDIO_CIS_SIZE_LIMIT) != 0) {
#ifdef NDIS
		/* nothing to do */
#else
		MFREE(osh, cis, SBSDIO_CIS_SIZE_LIMIT);
#endif /* NDIS */
		return -2;
	}

	rc = srom_parsecis(osh, &cis, SDIO_FUNC_1, vars, count);

#ifdef NDIS
	/* nothing to do here */
#else
	MFREE(osh, cis, SBSDIO_CIS_SIZE_LIMIT);
#endif

	return (rc);
}
#endif /* BCMSPI */
#endif /* !defined(BCMDONGLEHOST) */

#if defined(BCMUSBDEV)
/** Return sprom size in 16-bit words */
uint
srom_size(si_t *sih, osl_t *osh)
{
	uint size = 0;
	if (SPROMBUS == PCMCIA_BUS) {
		uint32 origidx;
		sdpcmd_regs_t *pcmregs;
		bool wasup;

		origidx = si_coreidx(sih);
		pcmregs = si_setcore(sih, PCMCIA_CORE_ID, 0);
		if (!pcmregs)
			pcmregs = si_setcore(sih, SDIOD_CORE_ID, 0);
		ASSERT(pcmregs);

		if (!(wasup = si_iscoreup(sih)))
			si_core_reset(sih, 0, 0);

		/* not worry about earlier core revs */
		/* valid for only pcmcia core */
		if (si_coreid(sih) == PCMCIA_CORE_ID)
			if (si_corerev(sih) < 8)
				goto done;


		switch (SI_PCMCIA_READ(osh, pcmregs, SROM_INFO) & SRI_SZ_MASK) {
		case 1:
			size = 256;	/* SROM_INFO == 1 means 4kbit */
			break;
		case 2:
			size = 1024;	/* SROM_INFO == 2 means 16kbit */
			break;
		default:
			break;
		}

	done:
		if (!wasup)
			si_core_disable(sih, 0);

		si_setcoreidx(sih, origidx);
	}
	return size;
}
#endif 

/**
 * initvars are different for BCMUSBDEV and BCMSDIODEV.  This is OK when supporting both at
 * the same time, but only because all of the code is in attach functions and not in ROM.
 */

#if defined(BCMUSBDEV_ENABLED)
#if defined(BCMUSBDEV_BMAC) || defined(BCM_BMAC_VARS_APPEND)
/**
 * Read the cis and call parsecis to allocate and initialize the NVRAM vars buffer.
 * OTP only. Return 0 on success (*vars points to NVRAM buffer), nonzero on error.
 */
static int
BCMATTACHFN(initvars_cis_usbdriver)(si_t *sih, osl_t *osh, char **vars, uint *count)
{
	uint8 *cis;
	uint sz = OTP_SZ_MAX/2; /* size in words */
	int rc = BCME_OK;

	if ((cis = MALLOC(osh, OTP_SZ_MAX)) == NULL) {
		return -1;
	}

	bzero(cis, OTP_SZ_MAX);

	if (otp_read_region(sih, OTP_SW_RGN, (uint16 *)cis, &sz)) {
		BS_ERROR(("%s: OTP read SW region failure.\n*", __FUNCTION__));
		rc = -2;
	} else {
		BS_ERROR(("%s: OTP programmed. use OTP for srom vars\n*", __FUNCTION__));
		rc = srom_parsecis(osh, &cis, SROM_CIS_SINGLE, vars, count);
	}

	MFREE(osh, cis, OTP_SZ_MAX);

	return (rc);
}

/**
 * For driver(not bootloader), if nvram is not downloadable or missing, use default. Despite the
 * 'srom' in the function name, this function only deals with OTP.
 */
static int
BCMATTACHFN(initvars_srom_si_usbdriver)(si_t *sih, osl_t *osh, char **vars, uint *varsz)
{
	uint len;
	char *base;
	char *fakevars;
	int rc = -1;

	base = fakevars = NULL;
	len = 0;
	switch (CHIPID(sih->chip)) {
		case BCM4322_CHIP_ID:   case BCM43221_CHIP_ID:  case BCM43231_CHIP_ID:
			fakevars = defaultsromvars_4322usb;
			break;
		case BCM43236_CHIP_ID: case BCM43235_CHIP_ID:  case BCM43238_CHIP_ID:
		case BCM43234_CHIP_ID:
			/* check against real chipid instead of compile time flag */
			if (CHIPID(sih->chip) == BCM43234_CHIP_ID) {
				fakevars = defaultsromvars_43234usb;
			} else if (CHIPID(sih->chip) == BCM43235_CHIP_ID) {
				fakevars = defaultsromvars_43235usb;
			} else
				fakevars = defaultsromvars_43236usb;
			break;

		case BCM4319_CHIP_ID:
			fakevars = defaultsromvars_4319usb;
			break;
		case BCM43242_CHIP_ID:
		case BCM43243_CHIP_ID:
			fakevars = defaultsromvars_43242usb;
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
			fakevars = defaultsromvars_4350usb;
			break;

		case BCM4360_CHIP_ID:
		case BCM4352_CHIP_ID:
		case BCM43460_CHIP_ID:
		case BCM43526_CHIP_ID:
			fakevars = defaultsromvars_4360usb;
			break;
		case BCM43143_CHIP_ID:
			fakevars = defaultsromvars_43143usb;
			break;
		case BCM43602_CHIP_ID: /* fall through: 43602 hasn't got a USB interface */
		case BCM43462_CHIP_ID: /* fall through: 43462 hasn't got a USB interface */
		default:
			ASSERT(0);
			return rc;
	}

#ifndef BCM_BMAC_VARS_APPEND
	if (BCME_OK == initvars_cis_usbdriver(sih, osh, vars, varsz)) {
		/* Make OTP variables global */
		if (srvars_inited == FALSE) {
			nvram_append((void *)sih, *vars, *varsz);
			DONGLE_STORE_VARS_OTP_PTR(*vars);
		}
		return BCME_OK;
	}
#endif /* BCM_BMAC_VARS_APPEND */

	/* NO OTP, if nvram downloaded, use it */
	if ((_varsz != 0) && (_vars != NULL)) {
		len  = _varsz + (strlen(vstr_end));
		base = MALLOC(osh, len + 2); /* plus 2 terminating \0 */
		if (base == NULL) {
			BS_ERROR(("initvars_srom_si: MALLOC failed.\n"));
			return BCME_ERROR;
		}
		bzero(base, len + 2);

		/* make a copy of the _vars, _vars is at the top of the memory, cannot append
		 * END\0\0 to it. copy the download vars to base, back of the terminating \0,
		 * then append END\0\0
		 */
		bcopy((void *)_vars, base, _varsz);
		/* backoff all the terminating \0s except the one the for the last string */
		len = _varsz;
		while (!base[len - 1])
			len--;
		len++; /* \0  for the last string */
		/* append END\0\0 to the end */
		bcopy((void *)vstr_end, (base + len), strlen(vstr_end));
		len += (strlen(vstr_end) + 2);
		*vars = base;
		*varsz = len;

		BS_ERROR(("%s USB nvram downloaded %d bytes\n", __FUNCTION__, _varsz));
	} else {
		/* Fall back to fake srom vars if OTP not programmed */
		len = srom_vars_len(fakevars);
		base = MALLOC(osh, (len + 1));
		if (base == NULL) {
			BS_ERROR(("initvars_srom_si: MALLOC failed.\n"));
			return BCME_ERROR;
		}
		bzero(base, (len + 1));
		bcopy(fakevars, base, len);
		*(base + len) = '\0';		/* add final nullbyte terminator */
		*vars = base;
		*varsz = len + 1;
		BS_ERROR(("initvars_srom_usbdriver: faked nvram %d bytes\n", len));
	}

#ifdef BCM_BMAC_VARS_APPEND
	if (BCME_OK == initvars_cis_usbdriver(sih, osh, vars, varsz)) { /* OTP only */
		if (base)
			MFREE(osh, base, (len + 1));
	}
#endif	/* BCM_BMAC_VARS_APPEND */
	/* Make OTP/SROM variables global */
	if (srvars_inited == FALSE) {
		nvram_append((void *)sih, *vars, *varsz);
		srvars_inited = TRUE;
		DONGLE_STORE_VARS_OTP_PTR(*vars);
	}
	return BCME_OK;

}
#endif /* BCMUSBDEV_BMAC || BCM_BMAC_VARS_APPEND */

#ifdef BCM_DONGLEVARS
/*** reads a CIS structure (so not an SROM-MAP structure) from either OTP or SROM */
static int
BCMATTACHFN(initvars_srom_si_bl)(si_t *sih, osl_t *osh, void *curmap, char **vars, uint *varsz)
{
	int sel = 0;		/* where to read srom/cis: 0 - none, 1 - otp, 2 - sprom */
	uint sz = 0;		/* srom size in bytes */
	void *oh = NULL;
	int rc = BCME_OK;

	if ((oh = otp_init(sih)) != NULL && (otp_status(oh) & OTPS_GUP_SW)) {
		/* Access OTP if it is present, powered on, and programmed */
		sz = otp_size(oh);
		sel = 1;
	} else if ((sz = srom_size(sih, osh)) != 0) {
		/* Access the SPROM if it is present */
		sz <<= 1;
		sel = 2;
	}

	/* Read CIS in OTP/SPROM */
	if (sel != 0) {
		uint16 *srom;
		uint8 *body = NULL;
		uint otpsz = sz;

		ASSERT(sz);

		/* Allocate memory */
		if ((srom = (uint16 *)MALLOC(osh, sz)) == NULL)
			return BCME_NOMEM;

		/* Read CIS */
		switch (sel) {
		case 1:
			rc = otp_read_region(sih, OTP_SW_RGN, srom, &otpsz);
			sz = otpsz;
			body = (uint8 *)srom;
			break;
		case 2:
			rc = srom_read(sih, SI_BUS, curmap, osh, 0, sz, srom, TRUE);
			/* sprom has 8 byte h/w header */
			body = (uint8 *)srom + SBSDIO_SPROM_CIS_OFFSET;
			break;
		default:
			/* impossible to come here */
			ASSERT(0);
			break;
		}

		/* Parse CIS */
		if (rc == BCME_OK) {
			/* each word is in host endian */
			htol16_buf((uint8 *)srom, sz);
			ASSERT(body);
			rc = srom_parsecis(osh, &body, SROM_CIS_SINGLE, vars, varsz);
		}

		MFREE(osh, srom, sz);	/* Clean up */

		/* Make SROM variables global */
		if (rc == BCME_OK) {
			nvram_append((void *)sih, *vars, *varsz);
			DONGLE_STORE_VARS_OTP_PTR(*vars);
		}
	}

	return BCME_OK;
}
#endif	/* #ifdef BCM_DONGLEVARS */

/**
 * initvars_srom_si() is defined multiple times in this file. This is the 1st variant for chips with
 * an active USB interface. It is called only for bus types SI_BUS and JTAG_BUS, and only for CIS
 * format in SPROM and/or OTP. Reads OTP or SPROM (bootloader only) and appends parsed contents to
 * caller supplied var/value pairs.
 */
static int
BCMATTACHFN(initvars_srom_si)(si_t *sih, osl_t *osh, void *curmap, char **vars, uint *varsz)
{

	/* Bail out if we've dealt with OTP/SPROM before! */
	if (srvars_inited)
		goto exit;

#if defined(BCMUSBDEV_BMAC) || defined(BCM_BMAC_VARS_APPEND)
	/* read OTP or use faked var array */
	switch (CHIPID(sih->chip)) {
		case BCM4322_CHIP_ID:   case BCM43221_CHIP_ID:  case BCM43231_CHIP_ID:
		case BCM43236_CHIP_ID:  case BCM43235_CHIP_ID:  case BCM43238_CHIP_ID:
		case BCM43234_CHIP_ID:
		case BCM4319_CHIP_ID:
		case BCM43242_CHIP_ID:
		case BCM43243_CHIP_ID:
		case BCM4360_CHIP_ID:
		case BCM43460_CHIP_ID:
		case BCM43526_CHIP_ID:
		case BCM4352_CHIP_ID:
		case BCM4350_CHIP_ID:
		case BCM4354_CHIP_ID:
		case BCM4356_CHIP_ID:
		case BCM43556_CHIP_ID:
		case BCM43558_CHIP_ID:
		case BCM43566_CHIP_ID:
		case BCM43568_CHIP_ID:
		case BCM43569_CHIP_ID:
		case BCM43570_CHIP_ID:
		case BCM43143_CHIP_ID:
		if (BCME_OK != initvars_srom_si_usbdriver(sih, osh, vars, varsz)) /* OTP only */
			goto exit;
		return BCME_OK;
		case BCM43602_CHIP_ID: /* 43602 does not support USB */
		case BCM43462_CHIP_ID: /* 43462 does not support USB */
		default:
			;
	}
#endif  /* BCMUSBDEV_BMAC || BCM_BMAC_VARS_APPEND */

#ifdef BCM_DONGLEVARS	/* this flag should be defined for usb bootloader, to read OTP or \
	SROM */
	if (BCME_OK != initvars_srom_si_bl(sih, osh, curmap, vars, varsz)) /* CIS format only */
		return BCME_ERROR;
#endif

	/* update static local var to skip for next call */
	srvars_inited = TRUE;

exit:
	/* Tell the caller there is no individual SROM variables */
	*vars = NULL;
	*varsz = 0;

	/* return OK so the driver will load & use defaults if bad srom/otp */
	return BCME_OK;
}

#elif defined(BCMSDIODEV_ENABLED)

#ifdef BCM_DONGLEVARS
static uint8 BCMATTACHDATA(defcis4325)[] = { 0x20, 0x4, 0xd0, 0x2, 0x25, 0x43, 0xff, 0xff };
static uint8 BCMATTACHDATA(defcis4315)[] = { 0x20, 0x4, 0xd0, 0x2, 0x15, 0x43, 0xff, 0xff };
static uint8 BCMATTACHDATA(defcis4329)[] = { 0x20, 0x4, 0xd0, 0x2, 0x29, 0x43, 0xff, 0xff };
static uint8 BCMATTACHDATA(defcis4319)[] = { 0x20, 0x4, 0xd0, 0x2, 0x19, 0x43, 0xff, 0xff };
static uint8 BCMATTACHDATA(defcis4336)[] = { 0x20, 0x4, 0xd0, 0x2, 0x36, 0x43, 0xff, 0xff };
static uint8 BCMATTACHDATA(defcis4330)[] = { 0x20, 0x4, 0xd0, 0x2, 0x30, 0x43, 0xff, 0xff };
static uint8 BCMATTACHDATA(defcis43237)[] = { 0x20, 0x4, 0xd0, 0x2, 0xe5, 0xa8, 0xff, 0xff };
static uint8 BCMATTACHDATA(defcis4324)[] = { 0x20, 0x4, 0xd0, 0x2, 0x24, 0x43, 0xff, 0xff };
static uint8 BCMATTACHDATA(defcis4335)[] = { 0x20, 0x4, 0xd0, 0x2, 0x24, 0x43, 0xff, 0xff };
static uint8 BCMATTACHDATA(defcis4350)[] = { 0x20, 0x4, 0xd0, 0x2, 0x50, 0x43, 0xff, 0xff };
static uint8 BCMATTACHDATA(defcis43143)[] = { 0x20, 0x4, 0xd0, 0x2, 0x87, 0xa8, 0xff, 0xff };
static uint8 BCMATTACHDATA(defcis43430)[] = { 0x20, 0x4, 0xd0, 0x2, 0xa6, 0xa9, 0xff, 0xff };

#ifdef BCM_BMAC_VARS_APPEND

static char BCMATTACHDATA(defaultsromvars_4319sdio)[] =
	"sromrev=3\0"
	"vendid=0x14e4\0"
	"devid=0x4338\0"
	"boardtype=0x05a1\0"
	"boardrev=0x1102\0"
	"boardflags=0x400201\0"
	"boardflags2=0x80\0"
	"xtalfreq=26000\0"
	"aa2g=3\0"
	"aa5g=0\0"
	"ag0=0\0"
	"opo=0\0"
	"pa0b0=0x1675\0"
	"pa0b1=0xfa74\0"
	"pa0b2=0xfea1\0"
	"pa0itssit=62\0"
	"pa0maxpwr=78\0"
	"rssismf2g=0xa\0"
	"rssismc2g=0xb\0"
	"rssisav2g=0x3\0"
	"bxa2g=0\0"
	"cckdigfilttype=6\0"
	"rxpo2g=2\0"
	"cckpo=0\0"
	"ofdmpo=0x55553333\0"
	"mcs2gpo0=0x9999\0"
	"mcs2gpo1=0x9999\0"
	"mcs2gpo2=0x0000\0"
	"mcs2gpo3=0x0000\0"
	"mcs2gpo4=0x9999\0"
	"mcs2gpo5=0x9999\0"
	"macaddr=00:90:4c:06:c0:19\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_4319sdio_hmb)[] =
	"sromrev=3\0"
	"vendid=0x14e4\0"
	"devid=0x4338\0"
	"boardtype=0x058c\0"
	"boardrev=0x1102\0"
	"boardflags=0x400201\0"
	"boardflags2=0x80\0"
	"xtalfreq=26000\0"
	"aa2g=3\0"
	"aa5g=0\0"
	"ag0=0\0"
	"opo=0\0"
	"pa0b0=0x1675\0"
	"pa0b1=0xfa74\0"
	"pa0b2=0xfea1\0"
	"pa0itssit=62\0"
	"pa0maxpwr=78\0"
	"rssismf2g=0xa \0"
	"rssismc2g=0xb \0"
	"rssisav2g=0x3 \0"
	"bxa2g=0\0"
	"cckdigfilttype=6\0"
	"rxpo2g=2\0"
	"cckpo=0\0"
	"ofdmpo=0x55553333\0"
	"mcs2gpo0=0x9999\0"
	"mcs2gpo1=0x9999\0"
	"mcs2gpo2=0x0000\0"
	"mcs2gpo3=0x0000\0"
	"mcs2gpo4=0x9999\0"
	"mcs2gpo5=0x9999\0"
	"macaddr=00:90:4c:06:c0:19\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_4319sdio_usbsd)[] =
	"sromrev=3\0"
	"vendid=0x14e4\0"
	"devid=0x4338\0"
	"boardtype=0x05a2\0"
	"boardrev=0x1100\0"
	"boardflags=0x400201\0"
	"boardflags2=0x80\0"
	"xtalfreq=30000\0"
	"aa2g=3\0"
	"aa5g=0\0"
	"ag0=0\0"
	"opo=0\0"
	"pa0b0=0x1675\0"
	"pa0b1=0xfa74\0"
	"pa0b2=0xfea1\0"
	"pa0itssit=62\0"
	"pa0maxpwr=78\0"
	"rssismf2g=0xa \0"
	"rssismc2g=0xb \0"
	"rssisav2g=0x3 \0"
	"bxa2g=0\0"
	"cckdigfilttype=6\0"
	"rxpo2g=2\0"
	"cckpo=0\0"
	"ofdmpo=0x55553333\0"
	"mcs2gpo0=0x9999\0"
	"mcs2gpo1=0x9999\0"
	"mcs2gpo2=0x0000\0"
	"mcs2gpo3=0x0000\0"
	"mcs2gpo4=0x9999\0"
	"mcs2gpo5=0x9999\0"
	"macaddr=00:90:4c:08:90:00\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_43237)[] =
	"vendid=0x14e4\0"
	"devid=0x4355\0"
	"boardtype=0x0583\0"
	"boardrev=0x1103\0"
	"boardnum=0x1\0"
	"boardflags=0x200\0"
	"boardflags2=0\0"
	"sromrev=8\0"
	"macaddr=00:90:4c:51:a8:e4\0"
	"ccode=0\0"
	"regrev=0\0"
	"ledbh0=0xff\0"
	"ledbh1=0xff\0"
	"ledbh2=0xff\0"
	"ledbh3=0xff\0"
	"leddc=0xffff\0"
	"opo=0x0\0"
	"aa2g=0x3\0"
	"aa5g=0x3\0"
	"ag0=0x2\0"
	"ag1=0x2\0"
	"ag2=0xff\0"
	"ag3=0xff\0"
	"pa0b0=0xfed1\0"
	"pa0b1=0x15fd\0"
	"pa0b2=0xfac2\0"
	"pa0itssit=0x20\0"
	"pa0maxpwr=0x4c\0"
	"pa1b0=0xfecd\0"
	"pa1b1=0x1497\0"
	"pa1b2=0xfae3\0"
	"pa1lob0=0xfe87\0"
	"pa1lob1=0x1637\0"
	"pa1lob2=0xfa8e\0"
	"pa1hib0=0xfedc\0"
	"pa1hib1=0x144b\0"
	"pa1hib2=0xfb01\0"
	"pa1itssit=0x3e\0"
	"pa1maxpwr=0x40\0"
	"pa1lomaxpwr=0x3a\0"
	"pa1himaxpwr=0x3c\0"
	"bxa2g=0x3\0"
	"rssisav2g=0x7\0"
	"rssismc2g=0xf\0"
	"rssismf2g=0xf\0"
	"bxa5g=0x3\0"
	"rssisav5g=0x7\0"
	"rssismc5g=0xf\0"
	"rssismf5g=0xf\0"
	"tri2g=0xff\0"
	"tri5g=0xff\0"
	"tri5gl=0xff\0"
	"tri5gh=0xff\0"
	"rxpo2g=0xff\0"
	"rxpo5g=0xff\0"
	"txchain=0x3\0"
	"rxchain=0x3\0"
	"antswitch=0x0\0"
	"tssipos2g=0x1\0"
	"extpagain2g=0x2\0"
	"pdetrange2g=0x2\0"
	"triso2g=0x3\0"
	"antswctl2g=0x0\0"
	"tssipos5g=0x1\0"
	"extpagain5g=0x2\0"
	"pdetrange5g=0x2\0"
	"triso5g=0x3\0"
	"cck2gpo=0x0\0"
	"ofdm2gpo=0x0\0"
	"ofdm5gpo=0x0\0"
	"ofdm5glpo=0x0\0"
	"ofdm5ghpo=0x0\0"
	"mcs2gpo0=0x0\0"
	"mcs2gpo1=0x0\0"
	"mcs2gpo2=0x0\0"
	"mcs2gpo3=0x0\0"
	"mcs2gpo4=0x0\0"
	"mcs2gpo5=0x0\0"
	"mcs2gpo6=0x0\0"
	"mcs2gpo7=0x0\0"
	"mcs5gpo0=0x0\0"
	"mcs5gpo1=0x0\0"
	"mcs5gpo2=0x0\0"
	"mcs5gpo3=0x0\0"
	"mcs5gpo4=0x0\0"
	"mcs5gpo5=0x0\0"
	"mcs5gpo6=0x0\0"
	"mcs5gpo7=0x0\0"
	"mcs5glpo0=0x0\0"
	"mcs5glpo1=0x0\0"
	"mcs5glpo2=0x0\0"
	"mcs5glpo3=0x0\0"
	"mcs5glpo4=0x0\0"
	"mcs5glpo5=0x0\0"
	"mcs5glpo6=0x0\0"
	"mcs5glpo7=0x0\0"
	"mcs5ghpo0=0x0\0"
	"mcs5ghpo1=0x0\0"
	"mcs5ghpo2=0x0\0"
	"mcs5ghpo3=0x0\0"
	"mcs5ghpo4=0x0\0"
	"mcs5ghpo5=0x0\0"
	"mcs5ghpo6=0x0\0"
	"mcs5ghpo7=0x0\0"
	"cddpo=0x0\0"
	"stbcpo=0x0\0"
	"bw40po=0x0\0"
	"bwduppo=0x0\0"
	"maxp2ga0=0x4c\0"
	"pa2gw0a0=0xfed1\0"
	"pa2gw1a0=0x15fd\0"
	"pa2gw2a0=0xfac2\0"
	"maxp5ga0=0x3c\0"
	"maxp5gha0=0x3c\0"
	"maxp5gla0=0x3c\0"
	"pa5gw0a0=0xfeb0\0"
	"pa5gw1a0=0x1491\0"
	"pa5gw2a0=0xfaf8\0"
	"pa5glw0a0=0xfeaa\0"
	"pa5glw1a0=0x14b9\0"
	"pa5glw2a0=0xfaf0\0"
	"pa5ghw0a0=0xfec5\0"
	"pa5ghw1a0=0x1439\0"
	"pa5ghw2a0=0xfb18\0"
	"maxp2ga1=0x4c\0"
	"itt2ga0=0x20\0"
	"itt5ga0=0x3e\0"
	"itt2ga1=0x20\0"
	"itt5ga1=0x3e\0"
	"pa2gw0a1=0xfed2\0"
	"pa2gw1a1=0x15d9\0"
	"pa2gw2a1=0xfac6\0"
	"maxp5ga1=0x3a\0"
	"maxp5gha1=0x3a\0"
	"maxp5gla1=0x3a\0"
	"pa5gw0a1=0xfebe\0"
	"pa5gw1a1=0x1306\0"
	"pa5gw2a1=0xfb63\0"
	"pa5glw0a1=0xfece\0"
	"pa5glw1a1=0x1361\0"
	"pa5glw2a1=0xfb5f\0"
	"pa5ghw0a1=0xfe9e\0"
	"pa5ghw1a1=0x12ca\0"
	"pa5ghw2a1=0xfb41\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_43143sdio)[] =
	"vendid=0x14e4\0"
	"subvendid=0x0a5c\0"
	"subdevid=0xbdc\0"
	"macaddr=00:90:4c:0e:81:23\0"
	"xtalfreq=20000\0"
	"cctl=0\0"
	"ccode=US\0"
	"regrev=0x0\0"
	"ledbh0=0xff\0"
	"ledbh1=0xff\0"
	"ledbh2=0xff\0"
	"ledbh3=0xff\0"
	"leddc=0xffff\0"
	"aa2g=0x3\0"
	"ag0=0x2\0"
	"txchain=0x1\0"
	"rxchain=0x1\0"
	"antswitch=0\0"
	"sromrev=10\0"
	"devid=0x4366\0"
	"boardrev=0x1100\0"
	"boardflags=0x200\0"
	"boardflags2=0x2000\0"
	"boardtype=0x0628\0"
	"tssipos2g=0x1\0"
	"extpagain2g=0x0\0"
	"pdetrange2g=0x0\0"
	"triso2g=0x3\0"
	"antswctl2g=0x0\0"
	"ofdm2gpo=0x0\0"
	"mcs2gpo0=0x0\0"
	"mcs2gpo1=0x0\0"
	"maxp2ga0=0x48\0"
	"tempthresh=120\0"
	"temps_period=5\0"
	"temp_hysteresis=5\0"
	"boardnum=0x1100\0"
	"pa0b0=5832\0"
	"pa0b1=-705\0"
	"pa0b2=-170\0"
	"cck2gpo=0\0"
	"swctrlmap_2g=0x06020602,0x0c080c08,0x04000400,0x00080808,0x6ff\0"
	"otpimagesize=154\0"
	"END\0";

static const char BCMATTACHDATA(rstr_load_driver_default_for_chip_X)[] =
	"load driver default for chip %x\n";
static const char BCMATTACHDATA(rstr_unknown_chip_X)[] = "unknown chip %x\n";

static int
BCMATTACHFN(srom_load_nvram)(si_t *sih, osl_t *osh, uint8 *pcis[], uint ciscnt,  char **vars,
	uint *varsz)
{
	uint len = 0, base_len;
	char *base;
	char *fakevars;

	base = fakevars = NULL;
	switch (CHIPID(sih->chip)) {
		case BCM4319_CHIP_ID:
			printf(rstr_load_driver_default_for_chip_X, CHIPID(sih->chip));
			fakevars = defaultsromvars_4319sdio;
			if (si_cis_source(sih) == CIS_OTP) {
				switch (srom_probe_boardtype(pcis, ciscnt)) {
					case BCM94319SDHMB_SSID:
						fakevars = defaultsromvars_4319sdio_hmb;
						break;
					case BCM94319USBSDB_SSID:
						fakevars = defaultsromvars_4319sdio_usbsd;
						break;
					default:
						fakevars = defaultsromvars_4319sdio;
						break;
				}
			}
			break;
		case BCM43237_CHIP_ID:
			printf(rstr_load_driver_default_for_chip_X, CHIPID(sih->chip));
			fakevars = defaultsromvars_43237;
			break;
		case BCM43143_CHIP_ID:
			printf(rstr_load_driver_default_for_chip_X, CHIPID(sih->chip));
			fakevars = defaultsromvars_43143sdio;
			break;
		default:
			printf(rstr_unknown_chip_X, CHIPID(sih->chip));
			return BCME_ERROR;		/* fakevars == NULL for switch default */
	}


	/* NO OTP, if nvram downloaded, use it */
	if ((_varsz != 0) && (_vars != NULL)) {
		len  = _varsz + (strlen(vstr_end));
		base_len = len + 2;  /* plus 2 terminating \0 */
		base = MALLOC(osh, base_len);
		if (base == NULL) {
			BS_ERROR(("initvars_srom_si: MALLOC failed.\n"));
			return BCME_ERROR;
		}
		bzero(base, base_len);

		/* make a copy of the _vars, _vars is at the top of the memory, cannot append
		 * END\0\0 to it. copy the download vars to base, back of the terminating \0,
		 * then append END\0\0
		 */
		bcopy((void *)_vars, base, _varsz);
		/* backoff all the terminating \0s except the one the for the last string */
		len = _varsz;
		while (!base[len - 1])
			len--;
		len++; /* \0  for the last string */
		/* append END\0\0 to the end */
		bcopy((void *)vstr_end, (base + len), strlen(vstr_end));
		len += (strlen(vstr_end) + 2);
		*vars = base;
		*varsz = len;

		BS_ERROR(("%s nvram downloaded %d bytes\n", __FUNCTION__, _varsz));
	} else {
		/* Fall back to fake srom vars if OTP not programmed */
		len = srom_vars_len(fakevars);
		base = MALLOC(osh, (len + 1));
		base_len = len + 1;
		if (base == NULL) {
			BS_ERROR(("initvars_srom_si: MALLOC failed.\n"));
			return BCME_ERROR;
		}
		bzero(base, base_len);
		bcopy(fakevars, base, len);
		*(base + len) = '\0';           /* add final nullbyte terminator */
		*vars = base;
		*varsz = len + 1;
		BS_ERROR(("srom_load_driver)default: faked nvram %d bytes\n", len));
	}
	/* Parse the CIS */
	if ((srom_parsecis(osh, pcis, ciscnt, vars, varsz)) == BCME_OK) {
		nvram_append((void *)sih, *vars, *varsz);
		DONGLE_STORE_VARS_OTP_PTR(*vars);
	}
	MFREE(osh, base, base_len);
	return BCME_OK;
}

#endif /* BCM_BMAC_VARS_APPEND */

/**
 * initvars_srom_si() is defined multiple times in this file. This is the 2nd variant for chips with
 * an active SDIOd interface using DONGLEVARS
 */
static int
BCMATTACHFN(initvars_srom_si)(si_t *sih, osl_t *osh, void *curmap, char **vars, uint *varsz)
{
	int cis_src;
	uint msz = 0;
	uint sz = 0;
	void *oh = NULL;
	int rc = BCME_OK;
	bool	new_cisformat = FALSE;

	uint16 *cisbuf = NULL;

	/* # sdiod fns + common + extra */
	uint8 *cis[SBSDIO_NUM_FUNCTION + 2] = { 0 };

	uint ciss = 0;
	uint8 *defcis;
	uint hdrsz;

	/* Bail out if we've dealt with OTP/SPROM before! */
	if (srvars_inited)
		goto exit;

	/* Initialize default and cis format count */
	switch (CHIPID(sih->chip)) {
	case BCM4325_CHIP_ID: ciss = 3; defcis = defcis4325; hdrsz = 8; break;
	case BCM4315_CHIP_ID: ciss = 3; defcis = defcis4315; hdrsz = 8; break;
	case BCM4329_CHIP_ID: ciss = 4; defcis = defcis4329; hdrsz = 12; break;
	case BCM4319_CHIP_ID: ciss = 3; defcis = defcis4319; hdrsz = 12; break;
	case BCM4336_CHIP_ID: ciss = 1; defcis = defcis4336; hdrsz = 4; break;
	case BCM43362_CHIP_ID: ciss = 1; defcis = defcis4336; hdrsz = 4; break;
	case BCM4330_CHIP_ID: ciss = 1; defcis = defcis4330; hdrsz = 4; break;
	case BCM43237_CHIP_ID: ciss = 1; defcis = defcis43237; hdrsz = 4; break;
	case BCM4324_CHIP_ID: ciss = 1; defcis = defcis4324; hdrsz = 4; break;
	case BCM4314_CHIP_ID: ciss = 1; defcis = defcis4330; hdrsz = 4; break;
	case BCM4334_CHIP_ID: ciss = 1; defcis = defcis4330; hdrsz = 4; break;
	case BCM43341_CHIP_ID: ciss = 1; defcis = defcis4330; hdrsz = 12; break;
	case BCM43143_CHIP_ID: ciss = 1; defcis = defcis43143; hdrsz = 4; break;
	case BCM4335_CHIP_ID: ciss = 1; defcis = defcis4335; hdrsz = 4; break;
	case BCM4345_CHIP_ID: ciss = 1; defcis = defcis4335; hdrsz = 4; break;
	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID: ciss = 1; defcis = defcis4350; hdrsz = 4; break;
	default:
		BS_ERROR(("%s: Unknown chip 0x%04x\n", __FUNCTION__, CHIPID(sih->chip)));
		return BCME_ERROR;
	}
	if (sih->ccrev >= 36) {
		uint32 otplayout;
		otplayout = si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, otplayout), 0, 0);
		if (otplayout & OTP_CISFORMAT_NEW) {
			ciss = 1;
			hdrsz = 2;
			new_cisformat = TRUE;
		}
		else {
			ciss = 3;
			hdrsz = 12;
		}
	}

	cis_src = si_cis_source(sih);
	switch (cis_src) {
	case CIS_SROM:
		sz = srom_size(sih, osh) << 1;
		break;
	case CIS_OTP:
		if (((oh = otp_init(sih)) != NULL) && (otp_status(oh) & OTPS_GUP_HW))
			sz = otp_size(oh);
		break;
	}

	if (sz != 0) {
		if ((cisbuf = (uint16*)MALLOC(osh, sz)) == NULL)
			return BCME_NOMEM;
		msz = sz;

		switch (cis_src) {
		case CIS_SROM:
			rc = srom_read(sih, SI_BUS, curmap, osh, 0, sz, cisbuf, FALSE);
			break;
		case CIS_OTP:
			sz >>= 1;
			rc = otp_read_region(sih, OTP_HW_RGN, cisbuf, &sz);
			sz <<= 1;
			break;
		}

		ASSERT(sz > hdrsz);
		if (rc == BCME_OK) {
			if ((cisbuf[0] == 0xffff) || (cisbuf[0] == 0)) {
				MFREE(osh, cisbuf, msz);
				cisbuf = NULL;
			} else if (new_cisformat) {
				cis[0] = (uint8*)(cisbuf + hdrsz);
			} else {
				cis[0] = (uint8*)cisbuf + hdrsz;
				cis[1] = (uint8*)cisbuf + hdrsz +
				        (cisbuf[1] >> 8) + ((cisbuf[2] & 0x00ff) << 8) -
				        SBSDIO_CIS_BASE_COMMON;
				cis[2] = (uint8*)cisbuf + hdrsz +
				        cisbuf[3] - SBSDIO_CIS_BASE_COMMON;
				cis[3] = (uint8*)cisbuf + hdrsz +
				        cisbuf[4] - SBSDIO_CIS_BASE_COMMON;
				ASSERT((cis[1] >= cis[0]) && (cis[1] < (uint8*)cisbuf + sz));
				ASSERT((cis[2] >= cis[0]) && (cis[2] < (uint8*)cisbuf + sz));
				ASSERT(((cis[3] >= cis[0]) && (cis[3] < (uint8*)cisbuf + sz)) ||
				        (ciss <= 3));
			}
		}
	}

	/* Use default if strapped to, or strapped source empty */
	if (cisbuf == NULL) {
		ciss = 1;
		cis[0] = defcis;
	}

#ifdef BCM_BMAC_VARS_APPEND
	srom_load_nvram(sih, osh, cis, ciss, vars, varsz);
#else
	/* Parse the CIS */
	if (rc == BCME_OK) {
		if ((rc = srom_parsecis(osh, cis, ciss, vars, varsz)) == BCME_OK) {
			nvram_append((void *)sih, *vars, *varsz);
			DONGLE_STORE_VARS_OTP_PTR(*vars);
		}
	}
#endif /* BCM_BMAC_VARS_APPEND */
	/* Clean up */
	if (cisbuf != NULL)
		MFREE(osh, cisbuf, msz);

	srvars_inited = TRUE;
exit:
	/* Tell the caller there is no individual SROM variables */
	*vars = NULL;
	*varsz = 0;

	/* return OK so the driver will load & use defaults if bad srom/otp */
	return BCME_OK;
} /* initvars_srom_si */
#else /* BCM_DONGLEVARS */

/**
 * initvars_srom_si() is defined multiple times in this file. This is the variant for chips with an
 * active SDIOd interface but without BCM_DONGLEVARS
 */
static int
BCMATTACHFN(initvars_srom_si)(si_t *sih, osl_t *osh, void *curmap, char **vars, uint *varsz)
{
	*vars = NULL;
	*varsz = 0;
	return BCME_OK;
}
#endif /* BCM_DONGLEVARS */

#elif defined(BCMPCIEDEV_ENABLED)

static int
BCMATTACHFN(initvars_srom_si)(si_t *sih, osl_t *osh, void *curmap, char **vars, uint *varsz)
{
#ifdef BCM_DONGLEVARS
	void *oh = NULL;
	uint8 *cis;
	uint sz = 0;
	int rc;

	if (si_cis_source(sih) != CIS_OTP)
		return BCME_OK;

	if (((oh = otp_init(sih)) != NULL) && (otp_status(oh) & OTPS_GUP_HW))
		sz = otp_size(oh);

	if (sz == 0)
		return BCME_OK;

	if ((cis = MALLOC(osh, sz)) == NULL)
		return BCME_NOMEM;
	sz >>= 1;
	rc = otp_read_region(sih, OTP_HW_RGN, (uint16 *)cis, &sz);
	sz <<= 1;

	/* account for the Hardware header */
	if (sz == 128)
		return BCME_OK;

	cis += 128;

	if (*(uint16 *)cis == SROM11_SIGNATURE) {
		return BCME_OK;
	}

	if ((rc = srom_parsecis(osh, &cis, SROM_CIS_SINGLE, vars, varsz)) == BCME_OK)
		nvram_append((void *)sih, *vars, *varsz);

	return rc;
#else /* BCM_DONGLEVARS */
	*vars = NULL;
	*varsz = 0;
	return BCME_OK;
#endif /* BCM_DONGLEVARS */
}

#else /* !BCMUSBDEV && !BCMSDIODEV  && !BCMPCIEDEV */

#ifndef BCMDONGLEHOST

#if defined(BCM_OL_DEV)
/**
 * Initialize nonvolatile variable table from shared info in TCM.
 */

extern olmsg_shared_info_t *ppcie_shared;

static int
BCMATTACHFN(initvars_tcm_pcidev)(si_t *sih, osl_t *osh, void *curmap, char **vars, uint *varsz)
{
	*vars = (char *)ppcie_shared->vars;
	*varsz = (int)ppcie_shared->vars_size;

	return BCME_OK;
}
#endif /* BCM_OL_DEV */

/**
 * initvars_srom_si() is defined multiple times in this file. This is the variant for:
 * !BCMDONGLEHOST && !BCMUSBDEV && !BCMSDIODEV && !BCMPCIEDEV
 * So this function is defined for PCI (not PCIe) builds that are also non DHD builds.
 * On success, a buffer containing var/val values has been allocated in parameter 'vars'.
 */
static int
BCMATTACHFN(initvars_srom_si)(si_t *sih, osl_t *osh, void *curmap, char **vars, uint *varsz)
{
#if defined(BCM_OL_DEV)
	/* Initialize nonvolatile variable table from shared info in TCM. */
	return initvars_tcm_pcidev(sih, osh, curmap, vars, varsz);
#else
	/* Search flash nvram section for srom variables */
	return initvars_flash_si(sih, vars, varsz);
#endif /* BCM_OL_DEV */
} /* initvars_srom_si */
#endif /* !BCMDONGLEHOST */
#endif	

void
BCMATTACHFN(srom_var_deinit)(si_t *sih)
{
	srvars_inited = FALSE;
}
