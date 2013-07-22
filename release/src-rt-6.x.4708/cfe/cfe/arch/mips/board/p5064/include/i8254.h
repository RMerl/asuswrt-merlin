/*
 * i8254.h: definitions for i8254 programmable interval timer in P5064
 * Copyright (c) 1997  Algorithmics Ltd
 */

/* Timer 0 is clock interrupt (irq0)
 * Timer 1 is refresh clock
 * Timer 2 is speaker tone
 */

#define PT_CLOCK	0
#define PT_REFRESH	1
#define PT_SPEAKER	2
#define PT_CONTROL	3

#ifndef __ASSEMBLER__
struct i8254 {
    	unsigned char	pt_counter0;
    	unsigned char	pt_counter1;
    	unsigned char	pt_counter2;
    	unsigned char	pt_control;
};

#define pt_clock	pt_counter0
#define pt_refresh	pt_counter1
#define pt_speaker	pt_counter2

#else

#define PT_REG(x)	(x)

#endif

/*
 * control word definitions
 */
#define	PTCW_RBCMD	(3<<6)		/* read-back command */
 #define  PTCW_RB_NCNT	 0x20		   /* rb: no count */
 #define  PTCW_RB_NSTAT	 0x10		   /* rb: no status */
 #define  PTCW_RB_SC(x)	 (0x02<<(x))	   /* rb: select counter x */
#define	PTCW_SC(x)	((x)<<6)	/* select counter x */
#define	PTCW_CLCMD	(0<<4)		/* counter latch command */
#define	PTCW_LSB	(1<<4)		/* r/w least signif. byte only */
#define	PTCW_MSB	(2<<4)		/* r/w most signif. byte only */
#define	PTCW_16B	(3<<4)		/* r/w 16 bits, lsb then msb */
#define	PTCW_MODE(x)	((x)<<1)	/* set mode to x */
#define	PTCW_BCD	0x1		/* operate in BCD mode */

/*
 * Status word definitions
 */
#define PTSW_OUTPUT	0x80		/* output pin active */
#define PTSW_NULL	0x40		/* null count */

/*
 * Mode definitions
 */
#define	MODE_ITC	0		/* interrupt on terminal count */
#define	MODE_HROS	1		/* hw retriggerable one-shot */
#define	MODE_RG		2		/* rate generator */
#define	MODE_SQW	3		/* square wave generator */
#define	MODE_STS	4		/* software triggered strobe */
#define	MODE_HTS	5		/* hardware triggered strobe */

#define PT_CRYSTAL	14318180	/* timer crystal hz (ticks/sec) */
