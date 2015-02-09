/****************************************************************************/

/*
 *	mcfne.h -- NE2000 in ColdFire eval boards.
 *
 *	(C) Copyright 1999-2000, Greg Ungerer (gerg@snapgear.com)
 *	(C) Copyright 2000,      Lineo (www.lineo.com)
 *	(C) Copyright 2001,      SnapGear (www.snapgear.com)
 *
 *      19990409 David W. Miller  Converted from m5206ne.h for 5307 eval board
 *
 *      Hacked support for m5206e Cadre III evaluation board
 *      Fred Stevens (fred.stevens@pemstar.com) 13 April 1999
 */

/****************************************************************************/
#ifndef	mcfne_h
#define	mcfne_h
/****************************************************************************/


/*
 *	Support for NE2000 clones devices in ColdFire based boards.
 *	Not all boards address these parts the same way, some use a
 *	direct addressing method, others use a side-band address space
 *	to access odd address registers, some require byte swapping
 *	others do not.
 */
#define	BSWAP(w)	(((w) << 8) | ((w) >> 8))
#define	RSWAP(w)	(w)


/*
 *	Define the basic hardware resources of NE2000 boards.
 */

#if defined(CONFIG_ARN5206)
#define NE2000_ADDR		0x40000300
#define NE2000_ODDOFFSET	0x00010000
#define	NE2000_IRQ_VECTOR	0xf0
#define	NE2000_IRQ_PRIORITY	2
#define	NE2000_IRQ_LEVEL	4
#define	NE2000_BYTE		volatile unsigned short
#endif

#if defined(CONFIG_M5206eC3)
#define	NE2000_ADDR		0x40000300
#define	NE2000_ODDOFFSET	0x00010000
#define	NE2000_IRQ_VECTOR	0x1c
#define	NE2000_IRQ_PRIORITY	2
#define	NE2000_IRQ_LEVEL	4
#define	NE2000_BYTE		volatile unsigned short
#endif

#if defined(CONFIG_M5206e) && defined(CONFIG_NETtel)
#define NE2000_ADDR		0x30000300
#define NE2000_IRQ_VECTOR	25
#define NE2000_IRQ_PRIORITY	1
#define NE2000_IRQ_LEVEL	3
#define	NE2000_BYTE		volatile unsigned char
#endif

#if defined(CONFIG_M5307C3)
#define NE2000_ADDR		0x40000300
#define NE2000_ODDOFFSET	0x00010000
#define NE2000_IRQ_VECTOR	0x1b
#define	NE2000_BYTE		volatile unsigned short
#endif

#if defined(CONFIG_M5272) && defined(CONFIG_NETtel)
#define NE2000_ADDR		0x30600300
#define NE2000_ODDOFFSET	0x00008000
#define NE2000_IRQ_VECTOR	67
#undef	BSWAP
#define	BSWAP(w)		(w)
#define	NE2000_BYTE		volatile unsigned short
#undef	RSWAP
#define	RSWAP(w)		(((w) << 8) | ((w) >> 8))
#endif

#if defined(CONFIG_M5307) && defined(CONFIG_NETtel)
#define NE2000_ADDR0		0x30600300
#define NE2000_ADDR1		0x30800300
#define NE2000_ODDOFFSET	0x00008000
#define NE2000_IRQ_VECTOR0	27
#define NE2000_IRQ_VECTOR1	29
#undef	BSWAP
#define	BSWAP(w)		(w)
#define	NE2000_BYTE		volatile unsigned short
#undef	RSWAP
#define	RSWAP(w)		(((w) << 8) | ((w) >> 8))
#endif

#if defined(CONFIG_M5307) && defined(CONFIG_SECUREEDGEMP3)
#define NE2000_ADDR		0x30600300
#define NE2000_ODDOFFSET	0x00008000
#define NE2000_IRQ_VECTOR	27
#undef	BSWAP
#define	BSWAP(w)		(w)
#define	NE2000_BYTE		volatile unsigned short
#undef	RSWAP
#define	RSWAP(w)		(((w) << 8) | ((w) >> 8))
#endif

#if defined(CONFIG_ARN5307)
#define NE2000_ADDR		0xfe600300
#define NE2000_ODDOFFSET	0x00010000
#define NE2000_IRQ_VECTOR	0x1b
#define NE2000_IRQ_PRIORITY	2
#define NE2000_IRQ_LEVEL	3
#define	NE2000_BYTE		volatile unsigned short
#endif

#if defined(CONFIG_M5407C3)
#define NE2000_ADDR		0x40000300
#define NE2000_ODDOFFSET	0x00010000
#define NE2000_IRQ_VECTOR	0x1b
#define	NE2000_BYTE		volatile unsigned short
#endif

/****************************************************************************/

/*
 *	Side-band address space for odd address requires re-mapping
 *	many of the standard ISA access functions.
 */
#ifdef NE2000_ODDOFFSET

#undef outb
#undef outb_p
#undef inb
#undef inb_p
#undef outsb
#undef outsw
#undef insb
#undef insw

#define	outb	ne2000_outb
#define	inb	ne2000_inb
#define	outb_p	ne2000_outb
#define	inb_p	ne2000_inb
#define	outsb	ne2000_outsb
#define	outsw	ne2000_outsw
#define	insb	ne2000_insb
#define	insw	ne2000_insw


#ifndef COLDFIRE_NE2000_FUNCS

void ne2000_outb(unsigned int val, unsigned int addr);
int  ne2000_inb(unsigned int addr);
void ne2000_insb(unsigned int addr, void *vbuf, int unsigned long len);
void ne2000_insw(unsigned int addr, void *vbuf, unsigned long len);
void ne2000_outsb(unsigned int addr, void *vbuf, unsigned long len);
void ne2000_outsw(unsigned int addr, void *vbuf, unsigned long len);

#else

/*
 *	This macro converts a conventional register address into the
 *	real memory pointer of the mapped NE2000 device.
 *	On most NE2000 implementations on ColdFire boards the chip is
 *	mapped in kinda funny, due to its ISA heritage.
 */
#define	NE2000_PTR(addr)	((addr&0x1)?(NE2000_ODDOFFSET+addr-1):(addr))
#define	NE2000_DATA_PTR(addr)	(addr)


void ne2000_outb(unsigned int val, unsigned int addr)
{
	NE2000_BYTE	*rp;

	rp = (NE2000_BYTE *) NE2000_PTR(addr);
	*rp = RSWAP(val);
}

int ne2000_inb(unsigned int addr)
{
	NE2000_BYTE	*rp, val;

	rp = (NE2000_BYTE *) NE2000_PTR(addr);
	val = *rp;
	return((int) ((NE2000_BYTE) RSWAP(val)));
}

void ne2000_insb(unsigned int addr, void *vbuf, int unsigned long len)
{
	NE2000_BYTE	*rp, val;
	unsigned char	*buf;

	buf = (unsigned char *) vbuf;
	rp = (NE2000_BYTE *) NE2000_DATA_PTR(addr);
	for (; (len > 0); len--) {
		val = *rp;
		*buf++ = RSWAP(val);
	}
}

void ne2000_insw(unsigned int addr, void *vbuf, unsigned long len)
{
	volatile unsigned short	*rp;
	unsigned short		w, *buf;

	buf = (unsigned short *) vbuf;
	rp = (volatile unsigned short *) NE2000_DATA_PTR(addr);
	for (; (len > 0); len--) {
		w = *rp;
		*buf++ = BSWAP(w);
	}
}

void ne2000_outsb(unsigned int addr, const void *vbuf, unsigned long len)
{
	NE2000_BYTE	*rp, val;
	unsigned char	*buf;

	buf = (unsigned char *) vbuf;
	rp = (NE2000_BYTE *) NE2000_DATA_PTR(addr);
	for (; (len > 0); len--) {
		val = *buf++;
		*rp = RSWAP(val);
	}
}

void ne2000_outsw(unsigned int addr, const void *vbuf, unsigned long len)
{
	volatile unsigned short	*rp;
	unsigned short		w, *buf;

	buf = (unsigned short *) vbuf;
	rp = (volatile unsigned short *) NE2000_DATA_PTR(addr);
	for (; (len > 0); len--) {
		w = *buf++;
		*rp = BSWAP(w);
	}
}

#endif /* COLDFIRE_NE2000_FUNCS */
#endif /* NE2000_OFFOFFSET */

/****************************************************************************/
#endif	/* mcfne_h */
