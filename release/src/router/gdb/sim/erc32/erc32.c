/*
 * This file is part of SIS.
 * 
 * SIS, SPARC instruction simulator V2.5 Copyright (C) 1995 Jiri Gaisler,
 * European Space Agency
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675
 * Mass Ave, Cambridge, MA 02139, USA.
 * 
 */

/* The control space devices */

#include <sys/types.h>
#include <stdio.h>
#include <termios.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include "sis.h"
#include "end.h"
#include "sim-config.h"

extern int      ctrl_c;
extern int32    sis_verbose;
extern int32    sparclite, sparclite_board;
extern int      rom8,wrp,uben;
extern char     uart_dev1[], uart_dev2[];

int dumbio = 0; /* normal, smart, terminal oriented IO by default */

/* MEC registers */
#define MEC_START 	0x01f80000
#define MEC_END 	0x01f80100

/* Memory exception waitstates */
#define MEM_EX_WS 	1

/* ERC32 always adds one waitstate during RAM std */
#define STD_WS 1

#ifdef ERRINJ
extern int errmec;
#endif

/* The target's byte order is big-endian by default until we load a
   little-endian program.  */

int	current_target_byte_order = BIG_ENDIAN;

#define MEC_WS	0		/* Waitstates per MEC access (0 ws) */
#define MOK	0

/* MEC register addresses */

#define MEC_MCR		0x000
#define MEC_SFR  	0x004
#define MEC_PWDR  	0x008
#define MEC_MEMCFG	0x010
#define MEC_IOCR	0x014
#define MEC_WCR		0x018

#define MEC_MAR0  	0x020
#define MEC_MAR1  	0x024

#define MEC_SSA1 	0x020
#define MEC_SEA1 	0x024
#define MEC_SSA2 	0x028
#define MEC_SEA2 	0x02C
#define MEC_ISR		0x044
#define MEC_IPR		0x048
#define MEC_IMR 	0x04C
#define MEC_ICR 	0x050
#define MEC_IFR 	0x054
#define MEC_WDOG  	0x060
#define MEC_TRAPD  	0x064
#define MEC_RTC_COUNTER	0x080
#define MEC_RTC_RELOAD	0x080
#define MEC_RTC_SCALER	0x084
#define MEC_GPT_COUNTER	0x088
#define MEC_GPT_RELOAD	0x088
#define MEC_GPT_SCALER	0x08C
#define MEC_TIMER_CTRL	0x098
#define MEC_SFSR	0x0A0
#define MEC_FFAR	0x0A4
#define MEC_ERSR	0x0B0
#define MEC_DBG		0x0C0
#define MEC_TCR		0x0D0

#define MEC_BRK		0x0C4
#define MEC_WPR		0x0C8

#define MEC_UARTA	0x0E0
#define MEC_UARTB	0x0E4
#define MEC_UART_CTRL	0x0E8
#define SIM_LOAD	0x0F0

/* Memory exception causes */
#define PROT_EXC	0x3
#define UIMP_ACC	0x4
#define MEC_ACC		0x6
#define WATCH_EXC	0xa
#define BREAK_EXC	0xb

/* Size of UART buffers (bytes) */
#define UARTBUF	1024

/* Number of simulator ticks between flushing the UARTS. 	 */
/* For good performance, keep above 1000			 */
#define UART_FLUSH_TIME	  3000

/* MEC timer control register bits */
#define TCR_GACR 1
#define TCR_GACL 2
#define TCR_GASE 4
#define TCR_GASL 8
#define TCR_TCRCR 0x100
#define TCR_TCRCL 0x200
#define TCR_TCRSE 0x400
#define TCR_TCRSL 0x800

/* New uart defines */
#define UART_TX_TIME	1000
#define UART_RX_TIME	1000
#define UARTA_DR	0x1
#define UARTA_SRE	0x2
#define UARTA_HRE	0x4
#define UARTA_OR	0x40
#define UARTA_CLR	0x80
#define UARTB_DR	0x10000
#define UARTB_SRE	0x20000
#define UARTB_HRE	0x40000
#define UARTB_OR	0x400000
#define UARTB_CLR	0x800000

#define UART_DR		0x100
#define UART_TSE	0x200
#define UART_THE	0x400

/* MEC registers */

static char     fname[256];
static int32    find = 0;
static uint32   mec_ssa[2];	/* Write protection start address */
static uint32   mec_sea[2];	/* Write protection end address */
static uint32   mec_wpr[2];	/* Write protection control fields */
static uint32   mec_sfsr;
static uint32   mec_ffar;
static uint32   mec_ipr;
static uint32   mec_imr;
static uint32   mec_isr;
static uint32   mec_icr;
static uint32   mec_ifr;
static uint32   mec_mcr;	/* MEC control register */
static uint32   mec_memcfg;	/* Memory control register */
static uint32   mec_wcr;	/* MEC waitstate register */
static uint32   mec_iocr;	/* MEC IO control register */
static uint32   posted_irq;
static uint32   mec_ersr;	/* MEC error and status register */
static uint32   mec_tcr;	/* MEC test comtrol register */

static uint32   rtc_counter;
static uint32   rtc_reload;
static uint32   rtc_scaler;
static uint32   rtc_scaler_start;
static uint32   rtc_enabled;
static uint32   rtc_cr;
static uint32   rtc_se;

static uint32   gpt_counter;
static uint32   gpt_reload;
static uint32   gpt_scaler;
static uint32   gpt_scaler_start;
static uint32   gpt_enabled;
static uint32   gpt_cr;
static uint32   gpt_se;

static uint32   wdog_scaler;
static uint32   wdog_counter;
static uint32   wdog_rst_delay;
static uint32   wdog_rston;

enum wdog_type {
    init, disabled, enabled, stopped
};

static enum wdog_type wdog_status;


/* ROM size 1024 Kbyte */
#define ROM_SZ	 	0x100000
#define ROM_MASK 	0x0fffff

/* RAM size 4 Mbyte */
#define RAM_START 	0x02000000
#define RAM_END 	0x02400000
#define RAM_MASK 	0x003fffff

/* SPARClite boards all seem to have RAM at the same place. */
#define RAM_START_SLITE	0x40000000
#define RAM_END_SLITE 	0x40400000
#define RAM_MASK_SLITE 	0x003fffff

/* Memory support variables */

static uint32   mem_ramr_ws;	/* RAM read waitstates */
static uint32   mem_ramw_ws;	/* RAM write waitstates */
static uint32   mem_romr_ws;	/* ROM read waitstates */
static uint32   mem_romw_ws;	/* ROM write waitstates */
static uint32	mem_ramstart;	/* RAM start */
static uint32	mem_ramend;	/* RAM end */
static uint32	mem_rammask;	/* RAM address mask */
static uint32   mem_ramsz;	/* RAM size */
static uint32   mem_romsz;	/* ROM size */
static uint32   mem_accprot;	/* RAM write protection enabled */
static uint32   mem_blockprot;	/* RAM block write protection enabled */

static unsigned char	romb[ROM_SZ];
static unsigned char	ramb[RAM_END - RAM_START];


/* UART support variables */

static int32    fd1, fd2;	/* file descriptor for input file */
static int32    Ucontrol;	/* UART status register */
static unsigned char aq[UARTBUF], bq[UARTBUF];
static int32    anum, aind = 0;
static int32    bnum, bind = 0;
static char     wbufa[UARTBUF], wbufb[UARTBUF];
static unsigned wnuma;
static unsigned wnumb;
static FILE    *f1in, *f1out, *f2in, *f2out;
static struct termios ioc1, ioc2, iocold1, iocold2;
static int      f1open = 0, f2open = 0;

static char     uarta_sreg, uarta_hreg, uartb_sreg, uartb_hreg;
static uint32   uart_stat_reg;
static uint32   uarta_data, uartb_data;

#ifdef ERA
int era = 0;
int erareg;
#endif

/* Forward declarations */

static void	decode_ersr PARAMS ((void));
#ifdef ERRINJ
static void	iucomperr PARAMS ((void));
#endif
static void	mecparerror PARAMS ((void));
static void	decode_memcfg PARAMS ((void));
static void	decode_wcr PARAMS ((void));
static void	decode_mcr PARAMS ((void));
static void	close_port PARAMS ((void));
static void	mec_reset PARAMS ((void));
static void	mec_intack PARAMS ((int32 level));
static void	chk_irq PARAMS ((void));
static void	mec_irq PARAMS ((int32 level));
static void	set_sfsr PARAMS ((uint32 fault, uint32 addr,
				  uint32 asi, uint32 read));
static int32	mec_read PARAMS ((uint32 addr, uint32 asi, uint32 *data));
static int	mec_write PARAMS ((uint32 addr, uint32 data));
static void	port_init PARAMS ((void));
static uint32	read_uart PARAMS ((uint32 addr));
static void	write_uart PARAMS ((uint32 addr, uint32 data));
static void	flush_uart PARAMS ((void));
static void	uarta_tx PARAMS ((void));
static void	uartb_tx PARAMS ((void));
static void	uart_rx PARAMS ((caddr_t arg));
static void	uart_intr PARAMS ((caddr_t arg));
static void	uart_irq_start PARAMS ((void));
static void	wdog_intr PARAMS ((caddr_t arg));
static void	wdog_start PARAMS ((void));
static void	rtc_intr PARAMS ((caddr_t arg));
static void	rtc_start PARAMS ((void));
static uint32	rtc_counter_read PARAMS ((void));
static void	rtc_scaler_set PARAMS ((uint32 val));
static void	rtc_reload_set PARAMS ((uint32 val));
static void	gpt_intr PARAMS ((caddr_t arg));
static void	gpt_start PARAMS ((void));
static uint32	gpt_counter_read PARAMS ((void));
static void	gpt_scaler_set PARAMS ((uint32 val));
static void	gpt_reload_set PARAMS ((uint32 val));
static void	timer_ctrl PARAMS ((uint32 val));
static unsigned char *
		get_mem_ptr PARAMS ((uint32 addr, uint32 size));

static void	fetch_bytes PARAMS ((int asi, unsigned char *mem,
				     uint32 *data, int sz));

static void	store_bytes PARAMS ((unsigned char *mem, uint32 *data, int sz));

extern int	ext_irl;


/* One-time init */

void
init_sim()
{
    port_init();
}

/* Power-on reset init */

void
reset()
{
    mec_reset();
    uart_irq_start();
    wdog_start();
}

static void
decode_ersr()
{
    if (mec_ersr & 0x01) {
	if (!(mec_mcr & 0x20)) {
	    if (mec_mcr & 0x40) {
	        sys_reset();
	        mec_ersr = 0x8000;
	        if (sis_verbose)
	            printf("Error manager reset - IU in error mode\n");
	    } else {
	        sys_halt();
	        mec_ersr |= 0x2000;
	        if (sis_verbose)
	            printf("Error manager halt - IU in error mode\n");
	    }
	} else
	    mec_irq(1);
    }
    if (mec_ersr & 0x04) {
	if (!(mec_mcr & 0x200)) {
	    if (mec_mcr & 0x400) {
	        sys_reset();
	        mec_ersr = 0x8000;
	        if (sis_verbose)
	            printf("Error manager reset - IU comparison error\n");
	    } else {
	        sys_halt();
	        mec_ersr |= 0x2000;
	        if (sis_verbose)
	            printf("Error manager halt - IU comparison error\n");
	    }
	} else
	    mec_irq(1);
    }
    if (mec_ersr & 0x20) { 
	if (!(mec_mcr & 0x2000)) {
	    if (mec_mcr & 0x4000) {
	        sys_reset();
	        mec_ersr = 0x8000;
	        if (sis_verbose)
	            printf("Error manager reset - MEC hardware error\n");
	    } else {
	        sys_halt();
	        mec_ersr |= 0x2000;
	        if (sis_verbose)
	            printf("Error manager halt - MEC hardware error\n");
	    }
	} else
	    mec_irq(1);
    }
}

#ifdef ERRINJ
static void
iucomperr()
{
    mec_ersr |= 0x04;
    decode_ersr();
}
#endif

static void
mecparerror()
{
    mec_ersr |= 0x20;
    decode_ersr();
}


/* IU error mode manager */

void
error_mode(pc)
    uint32          pc;
{

    mec_ersr |= 0x1;
    decode_ersr();
}


/* Check memory settings */

static void
decode_memcfg()
{
    if (rom8) mec_memcfg &= ~0x20000;
    else mec_memcfg |= 0x20000;

    mem_ramsz = (256 * 1024) << ((mec_memcfg >> 10) & 7);
    mem_romsz = (128 * 1024) << ((mec_memcfg >> 18) & 7);

    if (sparclite_board) {
	mem_ramstart = RAM_START_SLITE;
	mem_ramend = RAM_END_SLITE;
	mem_rammask = RAM_MASK_SLITE;
    }
    else {
	mem_ramstart = RAM_START;
	mem_ramend = RAM_END;
	mem_rammask = RAM_MASK;
    }
    if (sis_verbose)
	printf("RAM start: 0x%x, RAM size: %d K, ROM size: %d K\n",
	       mem_ramstart, mem_ramsz >> 10, mem_romsz >> 10);
}

static void
decode_wcr()
{
    mem_ramr_ws = mec_wcr & 3;
    mem_ramw_ws = (mec_wcr >> 2) & 3;
    mem_romr_ws = (mec_wcr >> 4) & 0x0f;
    if (rom8) {
    	if (mem_romr_ws > 0 )  mem_romr_ws--;
	mem_romr_ws = 5 + (4*mem_romr_ws);
    }
    mem_romw_ws = (mec_wcr >> 8) & 0x0f;
    if (sis_verbose)
	printf("Waitstates = RAM read: %d, RAM write: %d, ROM read: %d, ROM write: %d\n",
	       mem_ramr_ws, mem_ramw_ws, mem_romr_ws, mem_romw_ws);
}

static void
decode_mcr()
{
    mem_accprot = (mec_wpr[0] | mec_wpr[1]);
    mem_blockprot = (mec_mcr >> 3) & 1;
    if (sis_verbose && mem_accprot)
	printf("Memory block write protection enabled\n");
    if (mec_mcr & 0x08000) {
	mec_ersr |= 0x20;
	decode_ersr();
    }
    if (sis_verbose && (mec_mcr & 2))
	printf("Software reset enabled\n");
    if (sis_verbose && (mec_mcr & 1))
	printf("Power-down mode enabled\n");
}

/* Flush ports when simulator stops */

void
sim_halt()
{
#ifdef FAST_UART
    flush_uart();
#endif
}

int
sim_stop(SIM_DESC sd)
{
  ctrl_c = 1;
  return 1;
}

static void
close_port()
{
    if (f1open && f1in != stdin)
	fclose(f1in);
    if (f2open && f2in != stdin)
	fclose(f2in);
}

void
exit_sim()
{
    close_port();
}

static void
mec_reset()
{
    int             i;

    find = 0;
    for (i = 0; i < 2; i++)
	mec_ssa[i] = mec_sea[i] = mec_wpr[i] = 0;
    mec_mcr = 0x01350014;
    mec_iocr = 0;
    mec_sfsr = 0x078;
    mec_ffar = 0;
    mec_ipr = 0;
    mec_imr = 0x7ffe;
    mec_isr = 0;
    mec_icr = 0;
    mec_ifr = 0;
    mec_memcfg = 0x10000;
    mec_wcr = -1;
    mec_ersr = 0;		/* MEC error and status register */
    mec_tcr = 0;		/* MEC test comtrol register */

    decode_memcfg();
    decode_wcr();
    decode_mcr();

    posted_irq = 0;
    wnuma = wnumb = 0;
    anum = aind = bnum = bind = 0;

    uart_stat_reg = UARTA_SRE | UARTA_HRE | UARTB_SRE | UARTB_HRE;
    uarta_data = uartb_data = UART_THE | UART_TSE;

    rtc_counter = 0xffffffff;
    rtc_reload = 0xffffffff;
    rtc_scaler = 0xff;
    rtc_enabled = 0;
    rtc_cr = 0;
    rtc_se = 0;

    gpt_counter = 0xffffffff;
    gpt_reload = 0xffffffff;
    gpt_scaler = 0xffff;
    gpt_enabled = 0;
    gpt_cr = 0;
    gpt_se = 0;

    wdog_scaler = 255;
    wdog_rst_delay = 255;
    wdog_counter = 0xffff;
    wdog_rston = 0;
    wdog_status = init;

#ifdef ERA
    erareg = 0;
#endif

}



static void
mec_intack(level)
    int32           level;
{
    int             irq_test;

    if (sis_verbose)
	printf("interrupt %d acknowledged\n", level);
    irq_test = mec_tcr & 0x80000;
    if ((irq_test) && (mec_ifr & (1 << level)))
	mec_ifr &= ~(1 << level);
    else
	mec_ipr &= ~(1 << level);
   chk_irq();
}

static void
chk_irq()
{
    int32           i;
    uint32          itmp;
    int		    old_irl;

    old_irl = ext_irl;
    if (mec_tcr & 0x80000) itmp = mec_ifr;
    else itmp  = 0;
    itmp = ((mec_ipr | itmp) & ~mec_imr) & 0x0fffe;
    ext_irl = 0;
    if (itmp != 0) {
	for (i = 15; i > 0; i--) {
	    if (((itmp >> i) & 1) != 0) {
		if ((sis_verbose) && (i > old_irl)) 
		    printf("IU irl: %d\n", i);
		ext_irl = i;
	        set_int(i, mec_intack, i);
		break;
	    }
	}
    }
}

static void
mec_irq(level)
    int32           level;
{
    mec_ipr |= (1 << level);
    chk_irq();
}

static void
set_sfsr(fault, addr, asi, read)
    uint32          fault;
    uint32          addr;
    uint32          asi;
    uint32          read;
{
    if ((asi == 0xa) || (asi == 0xb)) {
	mec_ffar = addr;
	mec_sfsr = (fault << 3) | (!read << 15);
	mec_sfsr |= ((mec_sfsr & 1) ^ 1) | (mec_sfsr & 1);
	switch (asi) {
	case 0xa:
	    mec_sfsr |= 0x0004;
	    break;
	case 0xb:
	    mec_sfsr |= 0x1004;
	    break;
	}
    }
}

static int32
mec_read(addr, asi, data)
    uint32          addr;
    uint32          asi;
    uint32         *data;
{

    switch (addr & 0x0ff) {

    case MEC_MCR:		/* 0x00 */
	*data = mec_mcr;
	break;

    case MEC_MEMCFG:		/* 0x10 */
	*data = mec_memcfg;
	break;

    case MEC_IOCR:
	*data = mec_iocr;	/* 0x14 */
	break;

    case MEC_SSA1:		/* 0x20 */
	*data = mec_ssa[0] | (mec_wpr[0] << 23);
	break;
    case MEC_SEA1:		/* 0x24 */
	*data = mec_sea[0];
	break;
    case MEC_SSA2:		/* 0x28 */
	*data = mec_ssa[1] | (mec_wpr[1] << 23);
	break;
    case MEC_SEA2:		/* 0x2c */
	*data = mec_sea[1];
	break;

    case MEC_ISR:		/* 0x44 */
	*data = mec_isr;
	break;

    case MEC_IPR:		/* 0x48 */
	*data = mec_ipr;
	break;

    case MEC_IMR:		/* 0x4c */
	*data = mec_imr;
	break;

    case MEC_IFR:		/* 0x54 */
	*data = mec_ifr;
	break;

    case MEC_RTC_COUNTER:	/* 0x80 */
	*data = rtc_counter_read();
	break;
    case MEC_RTC_SCALER:	/* 0x84 */
	if (rtc_enabled)
	    *data = rtc_scaler - (now() - rtc_scaler_start);
	else
	    *data = rtc_scaler;
	break;

    case MEC_GPT_COUNTER:	/* 0x88 */
	*data = gpt_counter_read();
	break;

    case MEC_GPT_SCALER:	/* 0x8c */
	if (rtc_enabled)
	    *data = gpt_scaler - (now() - gpt_scaler_start);
	else
	    *data = gpt_scaler;
	break;


    case MEC_SFSR:		/* 0xA0 */
	*data = mec_sfsr;
	break;

    case MEC_FFAR:		/* 0xA4 */
	*data = mec_ffar;
	break;

    case SIM_LOAD:
	fname[find] = 0;
	if (find == 0)
	    strcpy(fname, "simload");
	find = bfd_load(fname);
 	if (find == -1) 
	    *data = 0;
	else
	    *data = 1;
	find = 0;
	break;

    case MEC_ERSR:		/* 0xB0 */
	*data = mec_ersr;
	break;

    case MEC_TCR:		/* 0xD0 */
	*data = mec_tcr;
	break;

    case MEC_UARTA:		/* 0xE0 */
    case MEC_UARTB:		/* 0xE4 */
	if (asi != 0xb) {
	    set_sfsr(MEC_ACC, addr, asi, 1);
	    return (1);
	}
	*data = read_uart(addr);
	break;

    case MEC_UART_CTRL:		/* 0xE8 */

	*data = read_uart(addr);
	break;

    default:
	set_sfsr(MEC_ACC, addr, asi, 1);
	return (1);
	break;
    }
    return (MOK);
}

static int
mec_write(addr, data)
    uint32          addr;
    uint32          data;
{
    if (sis_verbose > 1)
	printf("MEC write a: %08x, d: %08x\n",addr,data);
    switch (addr & 0x0ff) {

    case MEC_MCR:
	mec_mcr = data;
	decode_mcr();
        if (mec_mcr & 0x08000) mecparerror();
	break;

    case MEC_SFR:
	if (mec_mcr & 0x2) {
	    sys_reset();
	    mec_ersr = 0x4000;
    	    if (sis_verbose)
	    	printf(" Software reset issued\n");
	}
	break;

    case MEC_IOCR:
	mec_iocr = data;
        if (mec_iocr & 0xC0C0C0C0) mecparerror();
	break;

    case MEC_SSA1:		/* 0x20 */
        if (data & 0xFE000000) mecparerror();
	mec_ssa[0] = data & 0x7fffff;
	mec_wpr[0] = (data >> 23) & 0x03;
	mem_accprot = mec_wpr[0] || mec_wpr[1];
	if (sis_verbose && mec_wpr[0])
	    printf("Segment 1 memory protection enabled (0x02%06x - 0x02%06x)\n",
		   mec_ssa[0] << 2, mec_sea[0] << 2);
	break;
    case MEC_SEA1:		/* 0x24 */
        if (data & 0xFF800000) mecparerror();
	mec_sea[0] = data & 0x7fffff;
	break;
    case MEC_SSA2:		/* 0x28 */
        if (data & 0xFE000000) mecparerror();
	mec_ssa[1] = data & 0x7fffff;
	mec_wpr[1] = (data >> 23) & 0x03;
	mem_accprot = mec_wpr[0] || mec_wpr[1];
	if (sis_verbose && mec_wpr[1])
	    printf("Segment 2 memory protection enabled (0x02%06x - 0x02%06x)\n",
		   mec_ssa[1] << 2, mec_sea[1] << 2);
	break;
    case MEC_SEA2:		/* 0x2c */
        if (data & 0xFF800000) mecparerror();
	mec_sea[1] = data & 0x7fffff;
	break;

    case MEC_UARTA:
    case MEC_UARTB:
        if (data & 0xFFFFFF00) mecparerror();
    case MEC_UART_CTRL:
        if (data & 0xFF00FF00) mecparerror();
	write_uart(addr, data);
	break;

    case MEC_GPT_RELOAD:
	gpt_reload_set(data);
	break;

    case MEC_GPT_SCALER:
        if (data & 0xFFFF0000) mecparerror();
	gpt_scaler_set(data);
	break;

    case MEC_TIMER_CTRL:
        if (data & 0xFFFFF0F0) mecparerror();
	timer_ctrl(data);
	break;

    case MEC_RTC_RELOAD:
	rtc_reload_set(data);
	break;

    case MEC_RTC_SCALER:
        if (data & 0xFFFFFF00) mecparerror();
	rtc_scaler_set(data);
	break;

    case MEC_SFSR:		/* 0xA0 */
        if (data & 0xFFFF0880) mecparerror();
	mec_sfsr = 0x78;
	break;

    case MEC_ISR:
        if (data & 0xFFFFE000) mecparerror();
	mec_isr = data;
	break;

    case MEC_IMR:		/* 0x4c */

        if (data & 0xFFFF8001) mecparerror();
	mec_imr = data & 0x7ffe;
	chk_irq();
	break;

    case MEC_ICR:		/* 0x50 */

        if (data & 0xFFFF0001) mecparerror();
	mec_ipr &= ~data & 0x0fffe;
	chk_irq();
	break;

    case MEC_IFR:		/* 0x54 */

        if (mec_tcr & 0x080000) {
            if (data & 0xFFFF0001) mecparerror();
	    mec_ifr = data & 0xfffe;
	    chk_irq();
	}
	break;
    case SIM_LOAD:
	fname[find++] = (char) data;
	break;


    case MEC_MEMCFG:		/* 0x10 */
        if (data & 0xC0E08000) mecparerror();
	mec_memcfg = data;
	decode_memcfg();
	if (mec_memcfg & 0xc0e08000)
	    mecparerror();
	break;

    case MEC_WCR:		/* 0x18 */
	mec_wcr = data;
	decode_wcr();
	break;

    case MEC_ERSR:		/* 0xB0 */
	if (mec_tcr & 0x100000)
            if (data & 0xFFFFEFC0) mecparerror();
	    mec_ersr = data & 0x103f;
	break;

    case MEC_TCR:		/* 0xD0 */
        if (data & 0xFFE1FFC0) mecparerror();
	mec_tcr = data & 0x1e003f;
	break;

    case MEC_WDOG:		/* 0x60 */
	wdog_scaler = (data >> 16) & 0x0ff;
	wdog_counter = data & 0x0ffff;
	wdog_rst_delay = data >> 24;
	wdog_rston = 0;
	if (wdog_status == stopped)
	    wdog_start();
	wdog_status = enabled;
	break;

    case MEC_TRAPD:		/* 0x64 */
	if (wdog_status == init) {
	    wdog_status = disabled;
	    if (sis_verbose)
		printf("Watchdog disabled\n");
	}
	break;

    case MEC_PWDR:
	if (mec_mcr & 1)
	    wait_for_irq();
	break;

    default:
	set_sfsr(MEC_ACC, addr, 0xb, 0);
	return (1);
	break;
    }
    return (MOK);
}


/* MEC UARTS */

static int      ifd1 = -1, ifd2 = -1, ofd1 = -1, ofd2 = -1;

void
init_stdio()
{
    if (dumbio)
        return; /* do nothing */
    if (!ifd1)
	tcsetattr(0, TCSANOW, &ioc1);
    if (!ifd2)
	tcsetattr(0, TCSANOW, &ioc2);
}

void
restore_stdio()
{
    if (dumbio)
        return; /* do nothing */
    if (!ifd1)
	tcsetattr(0, TCSANOW, &iocold1);
    if (!ifd2)
	tcsetattr(0, TCSANOW, &iocold2);
}

#define DO_STDIO_READ( _fd_, _buf_, _len_ )          \
             ( dumbio                                \
               ? (0) /* no bytes read, no delay */   \
               : read( _fd_, _buf_, _len_ ) )


static void
port_init()
{

    if (uben) {
    f2in = stdin;
    f1in = NULL;
    f2out = stdout;
    f1out = NULL;
    } else {
    f1in = stdin;
    f2in = NULL;
    f1out = stdout;
    f2out = NULL;
    }
    if (uart_dev1[0] != 0)
	if ((fd1 = open(uart_dev1, O_RDWR | O_NONBLOCK)) < 0) {
	    printf("Warning, couldn't open output device %s\n", uart_dev1);
	} else {
	    if (sis_verbose)
		printf("serial port A on %s\n", uart_dev1);
	    f1in = f1out = fdopen(fd1, "r+");
	    setbuf(f1out, NULL);
	    f1open = 1;
	}
    if (f1in) ifd1 = fileno(f1in);
    if (ifd1 == 0) {
	if (sis_verbose)
	    printf("serial port A on stdin/stdout\n");
        if (!dumbio) {
            tcgetattr(ifd1, &ioc1);
            iocold1 = ioc1;
            ioc1.c_lflag &= ~(ICANON | ECHO);
            ioc1.c_cc[VMIN] = 0;
            ioc1.c_cc[VTIME] = 0;
        }
	f1open = 1;
    }

    if (f1out) {
	ofd1 = fileno(f1out);
    	if (!dumbio && ofd1 == 1) setbuf(f1out, NULL);
    }

    if (uart_dev2[0] != 0)
	if ((fd2 = open(uart_dev2, O_RDWR | O_NONBLOCK)) < 0) {
	    printf("Warning, couldn't open output device %s\n", uart_dev2);
	} else {
	    if (sis_verbose)
		printf("serial port B on %s\n", uart_dev2);
	    f2in = f2out = fdopen(fd2, "r+");
	    setbuf(f2out, NULL);
	    f2open = 1;
	}
    if (f2in)  ifd2 = fileno(f2in);
    if (ifd2 == 0) {
	if (sis_verbose)
	    printf("serial port B on stdin/stdout\n");
        if (!dumbio) {
            tcgetattr(ifd2, &ioc2);
            iocold2 = ioc2;
            ioc2.c_lflag &= ~(ICANON | ECHO);
            ioc2.c_cc[VMIN] = 0;
            ioc2.c_cc[VTIME] = 0;
        }
	f2open = 1;
    }

    if (f2out) {
	ofd2 = fileno(f2out);
        if (!dumbio && ofd2 == 1) setbuf(f2out, NULL);
    }

    wnuma = wnumb = 0;

}

static uint32
read_uart(addr)
    uint32          addr;
{

    unsigned        tmp;

    tmp = 0;
    switch (addr & 0xff) {

    case 0xE0:			/* UART 1 */
#ifndef _WIN32
#ifdef FAST_UART

	if (aind < anum) {
	    if ((aind + 1) < anum)
		mec_irq(4);
	    return (0x700 | (uint32) aq[aind++]);
	} else {
	    if (f1open) {
	        anum = DO_STDIO_READ(ifd1, aq, UARTBUF);
	    }
	    if (anum > 0) {
		aind = 0;
		if ((aind + 1) < anum)
		    mec_irq(4);
		return (0x700 | (uint32) aq[aind++]);
	    } else {
		return (0x600 | (uint32) aq[aind]);
	    }

	}
#else
	tmp = uarta_data;
	uarta_data &= ~UART_DR;
	uart_stat_reg &= ~UARTA_DR;
	return tmp;
#endif
#else
	return(0);
#endif
	break;

    case 0xE4:			/* UART 2 */
#ifndef _WIN32
#ifdef FAST_UART
	if (bind < bnum) {
	    if ((bind + 1) < bnum)
		mec_irq(5);
	    return (0x700 | (uint32) bq[bind++]);
	} else {
	    if (f2open) {
		bnum = DO_STDIO_READ(ifd2, bq, UARTBUF);
	    }
	    if (bnum > 0) {
		bind = 0;
		if ((bind + 1) < bnum)
		    mec_irq(5);
		return (0x700 | (uint32) bq[bind++]);
	    } else {
		return (0x600 | (uint32) bq[bind]);
	    }

	}
#else
	tmp = uartb_data;
	uartb_data &= ~UART_DR;
	uart_stat_reg &= ~UARTB_DR;
	return tmp;
#endif
#else
	return(0);
#endif
	break;

    case 0xE8:			/* UART status register	 */
#ifndef _WIN32
#ifdef FAST_UART

	Ucontrol = 0;
	if (aind < anum) {
	    Ucontrol |= 0x00000001;
	} else {
	    if (f1open) {
	        anum = DO_STDIO_READ(ifd1, aq, UARTBUF);
            }
	    if (anum > 0) {
		Ucontrol |= 0x00000001;
		aind = 0;
		mec_irq(4);
	    }
	}
	if (bind < bnum) {
	    Ucontrol |= 0x00010000;
	} else {
	    if (f2open) {
		bnum = DO_STDIO_READ(ifd2, bq, UARTBUF);
	    }
	    if (bnum > 0) {
		Ucontrol |= 0x00010000;
		bind = 0;
		mec_irq(5);
	    }
	}

	Ucontrol |= 0x00060006;
	return (Ucontrol);
#else
	return (uart_stat_reg);
#endif
#else
	return(0x00060006);
#endif
	break;
    default:
	if (sis_verbose)
	    printf("Read from unimplemented MEC register (%x)\n", addr);

    }
    return (0);
}

static void
write_uart(addr, data)
    uint32          addr;
    uint32          data;
{
    unsigned char   c;

    c = (unsigned char) data;
    switch (addr & 0xff) {

    case 0xE0:			/* UART A */
#ifdef FAST_UART
	if (f1open) {
	    if (wnuma < UARTBUF)
	        wbufa[wnuma++] = c;
	    else {
	        while (wnuma)
		    wnuma -= fwrite(wbufa, 1, wnuma, f1out);
	        wbufa[wnuma++] = c;
	    }
	}
	mec_irq(4);
#else
	if (uart_stat_reg & UARTA_SRE) {
	    uarta_sreg = c;
	    uart_stat_reg &= ~UARTA_SRE;
	    event(uarta_tx, 0, UART_TX_TIME);
	} else {
	    uarta_hreg = c;
	    uart_stat_reg &= ~UARTA_HRE;
	}
#endif
	break;

    case 0xE4:			/* UART B */
#ifdef FAST_UART
	if (f2open) {
	    if (wnumb < UARTBUF)
		wbufb[wnumb++] = c;
	    else {
		while (wnumb)
		    wnumb -= fwrite(wbufb, 1, wnumb, f2out);
		wbufb[wnumb++] = c;
	    }
	}
	mec_irq(5);
#else
	if (uart_stat_reg & UARTB_SRE) {
	    uartb_sreg = c;
	    uart_stat_reg &= ~UARTB_SRE;
	    event(uartb_tx, 0, UART_TX_TIME);
	} else {
	    uartb_hreg = c;
	    uart_stat_reg &= ~UARTB_HRE;
	}
#endif
	break;
    case 0xE8:			/* UART status register */
#ifndef FAST_UART
	if (data & UARTA_CLR) {
	    uart_stat_reg &= 0xFFFF0000;
	    uart_stat_reg |= UARTA_SRE | UARTA_HRE;
	}
	if (data & UARTB_CLR) {
	    uart_stat_reg &= 0x0000FFFF;
	    uart_stat_reg |= UARTB_SRE | UARTB_HRE;
	}
#endif
	break;
    default:
	if (sis_verbose)
	    printf("Write to unimplemented MEC register (%x)\n", addr);

    }
}

static void
flush_uart()
{
    while (wnuma && f1open)
	wnuma -= fwrite(wbufa, 1, wnuma, f1out);
    while (wnumb && f2open)
	wnumb -= fwrite(wbufb, 1, wnumb, f2out);
}



static void
uarta_tx()
{

    while (f1open && fwrite(&uarta_sreg, 1, 1, f1out) != 1);
    if (uart_stat_reg & UARTA_HRE) {
	uart_stat_reg |= UARTA_SRE;
    } else {
	uarta_sreg = uarta_hreg;
	uart_stat_reg |= UARTA_HRE;
	event(uarta_tx, 0, UART_TX_TIME);
    }
    mec_irq(4);
}

static void
uartb_tx()
{
    while (f2open && fwrite(&uartb_sreg, 1, 1, f2out) != 1);
    if (uart_stat_reg & UARTB_HRE) {
	uart_stat_reg |= UARTB_SRE;
    } else {
	uartb_sreg = uartb_hreg;
	uart_stat_reg |= UARTB_HRE;
	event(uartb_tx, 0, UART_TX_TIME);
    }
    mec_irq(5);
}

static void
uart_rx(arg)
    caddr_t         arg;
{
    int32           rsize;
    char            rxd;


    rsize = 0;
    if (f1open)
        rsize = DO_STDIO_READ(ifd1, &rxd, 1);
    if (rsize > 0) {
	uarta_data = UART_DR | rxd;
	if (uart_stat_reg & UARTA_HRE)
	    uarta_data |= UART_THE;
	if (uart_stat_reg & UARTA_SRE)
	    uarta_data |= UART_TSE;
	if (uart_stat_reg & UARTA_DR) {
	    uart_stat_reg |= UARTA_OR;
	    mec_irq(7);		/* UART error interrupt */
	}
	uart_stat_reg |= UARTA_DR;
	mec_irq(4);
    }
    rsize = 0;
    if (f2open)
        rsize = DO_STDIO_READ(ifd2, &rxd, 1);
    if (rsize) {
	uartb_data = UART_DR | rxd;
	if (uart_stat_reg & UARTB_HRE)
	    uartb_data |= UART_THE;
	if (uart_stat_reg & UARTB_SRE)
	    uartb_data |= UART_TSE;
	if (uart_stat_reg & UARTB_DR) {
	    uart_stat_reg |= UARTB_OR;
	    mec_irq(7);		/* UART error interrupt */
	}
	uart_stat_reg |= UARTB_DR;
	mec_irq(5);
    }
    event(uart_rx, 0, UART_RX_TIME);
}

static void
uart_intr(arg)
    caddr_t         arg;
{
    read_uart(0xE8);		/* Check for UART interrupts every 1000 clk */
    flush_uart();		/* Flush UART ports      */
    event(uart_intr, 0, UART_FLUSH_TIME);
}


static void
uart_irq_start()
{
#ifdef FAST_UART
    event(uart_intr, 0, UART_FLUSH_TIME);
#else
#ifndef _WIN32
    event(uart_rx, 0, UART_RX_TIME);
#endif
#endif
}

/* Watch-dog */

static void
wdog_intr(arg)
    caddr_t         arg;
{
    if (wdog_status == disabled) {
	wdog_status = stopped;
    } else {

	if (wdog_counter) {
	    wdog_counter--;
	    event(wdog_intr, 0, wdog_scaler + 1);
	} else {
	    if (wdog_rston) {
		printf("Watchdog reset!\n");
		sys_reset();
		mec_ersr = 0xC000;
	    } else {
		mec_irq(15);
		wdog_rston = 1;
		wdog_counter = wdog_rst_delay;
		event(wdog_intr, 0, wdog_scaler + 1);
	    }
	}
    }
}

static void
wdog_start()
{
    event(wdog_intr, 0, wdog_scaler + 1);
    if (sis_verbose)
	printf("Watchdog started, scaler = %d, counter = %d\n",
	       wdog_scaler, wdog_counter);
}


/* MEC timers */


static void
rtc_intr(arg)
    caddr_t         arg;
{
    if (rtc_counter == 0) {

	mec_irq(13);
	if (rtc_cr)
	    rtc_counter = rtc_reload;
	else
	    rtc_se = 0;
    } else
	rtc_counter -= 1;
    if (rtc_se) {
	event(rtc_intr, 0, rtc_scaler + 1);
	rtc_scaler_start = now();
	rtc_enabled = 1;
    } else {
	if (sis_verbose)
	    printf("RTC stopped\n\r");
	rtc_enabled = 0;
    }
}

static void
rtc_start()
{
    if (sis_verbose)
	printf("RTC started (period %d)\n\r", rtc_scaler + 1);
    event(rtc_intr, 0, rtc_scaler + 1);
    rtc_scaler_start = now();
    rtc_enabled = 1;
}

static uint32
rtc_counter_read()
{
    return (rtc_counter);
}

static void
rtc_scaler_set(val)
    uint32          val;
{
    rtc_scaler = val & 0x0ff;	/* eight-bit scaler only */
}

static void
rtc_reload_set(val)
    uint32          val;
{
    rtc_reload = val;
}

static void
gpt_intr(arg)
    caddr_t         arg;
{
    if (gpt_counter == 0) {
	mec_irq(12);
	if (gpt_cr)
	    gpt_counter = gpt_reload;
	else
	    gpt_se = 0;
    } else
	gpt_counter -= 1;
    if (gpt_se) {
	event(gpt_intr, 0, gpt_scaler + 1);
	gpt_scaler_start = now();
	gpt_enabled = 1;
    } else {
	if (sis_verbose)
	    printf("GPT stopped\n\r");
	gpt_enabled = 0;
    }
}

static void
gpt_start()
{
    if (sis_verbose)
	printf("GPT started (period %d)\n\r", gpt_scaler + 1);
    event(gpt_intr, 0, gpt_scaler + 1);
    gpt_scaler_start = now();
    gpt_enabled = 1;
}

static uint32
gpt_counter_read()
{
    return (gpt_counter);
}

static void
gpt_scaler_set(val)
    uint32          val;
{
    gpt_scaler = val & 0x0ffff;	/* 16-bit scaler */
}

static void
gpt_reload_set(val)
    uint32          val;
{
    gpt_reload = val;
}

static void
timer_ctrl(val)
    uint32          val;
{

    rtc_cr = ((val & TCR_TCRCR) != 0);
    if (val & TCR_TCRCL) {
	rtc_counter = rtc_reload;
    }
    if (val & TCR_TCRSL) {
    }
    rtc_se = ((val & TCR_TCRSE) != 0);
    if (rtc_se && (rtc_enabled == 0))
	rtc_start();

    gpt_cr = (val & TCR_GACR);
    if (val & TCR_GACL) {
	gpt_counter = gpt_reload;
    }
    if (val & TCR_GACL) {
    }
    gpt_se = (val & TCR_GASE) >> 2;
    if (gpt_se && (gpt_enabled == 0))
	gpt_start();
}


/* Retrieve data from target memory.  MEM points to location from which
   to read the data; DATA points to words where retrieved data will be
   stored in host byte order.  SZ contains log(2) of the number of bytes
   to retrieve, and can be 0 (1 byte), 1 (one half-word), 2 (one word),
   or 3 (two words). */

static void
fetch_bytes (asi, mem, data, sz)
    int		    asi;
    unsigned char  *mem;
    uint32	   *data;
    int		    sz;
{
    if (CURRENT_TARGET_BYTE_ORDER == BIG_ENDIAN
	|| asi == 8 || asi == 9) {
	switch (sz) {
	case 3:
	    data[1] =  (((uint32) mem[7]) & 0xff) |
		      ((((uint32) mem[6]) & 0xff) <<  8) |
		      ((((uint32) mem[5]) & 0xff) << 16) |
		      ((((uint32) mem[4]) & 0xff) << 24);
	    /* Fall through to 2 */
	case 2:
	    data[0] =  (((uint32) mem[3]) & 0xff) |
		      ((((uint32) mem[2]) & 0xff) <<  8) |
		      ((((uint32) mem[1]) & 0xff) << 16) |
		      ((((uint32) mem[0]) & 0xff) << 24);
	    break;
	case 1:
	    data[0] =  (((uint32) mem[1]) & 0xff) |
		      ((((uint32) mem[0]) & 0xff) << 8);
	    break;
	case 0:
	    data[0] = mem[0] & 0xff;
	    break;
	    
	}
    } else {
	switch (sz) {
	case 3:
	    data[1] = ((((uint32) mem[7]) & 0xff) << 24) |
		      ((((uint32) mem[6]) & 0xff) << 16) |
		      ((((uint32) mem[5]) & 0xff) <<  8) |
		       (((uint32) mem[4]) & 0xff);
	    /* Fall through to 4 */
	case 2:
	    data[0] = ((((uint32) mem[3]) & 0xff) << 24) |
		      ((((uint32) mem[2]) & 0xff) << 16) |
		      ((((uint32) mem[1]) & 0xff) <<  8) |
		       (((uint32) mem[0]) & 0xff);
	    break;
	case 1:
	    data[0] = ((((uint32) mem[1]) & 0xff) <<  8) |
		       (((uint32) mem[0]) & 0xff);
	    break;
	case 0:
	    data[0] = mem[0] & 0xff;
	    break;
	}
    }
}


/* Store data in target byte order.  MEM points to location to store data;
   DATA points to words in host byte order to be stored.  SZ contains log(2)
   of the number of bytes to retrieve, and can be 0 (1 byte), 1 (one half-word),
   2 (one word), or 3 (two words). */

static void
store_bytes (mem, data, sz)
    unsigned char  *mem;
    uint32	   *data;
    int		    sz;
{
    if (CURRENT_TARGET_BYTE_ORDER == LITTLE_ENDIAN) {
	switch (sz) {
	case 3:
	    mem[7] = (data[1] >> 24) & 0xff;
	    mem[6] = (data[1] >> 16) & 0xff;
	    mem[5] = (data[1] >>  8) & 0xff;
	    mem[4] = data[1] & 0xff;
	    /* Fall through to 2 */
	case 2:
	    mem[3] = (data[0] >> 24) & 0xff;
	    mem[2] = (data[0] >> 16) & 0xff;
	    /* Fall through to 1 */
	case 1:
	    mem[1] = (data[0] >>  8) & 0xff;
	    /* Fall through to 0 */
	case 0:
	    mem[0] = data[0] & 0xff;
	    break;
	}
    } else {
	switch (sz) {
	case 3:
	    mem[7] = data[1] & 0xff;
	    mem[6] = (data[1] >>  8) & 0xff;
	    mem[5] = (data[1] >> 16) & 0xff;
	    mem[4] = (data[1] >> 24) & 0xff;
	    /* Fall through to 2 */
	case 2:
	    mem[3] = data[0] & 0xff;
	    mem[2] = (data[0] >>  8) & 0xff;
	    mem[1] = (data[0] >> 16) & 0xff;
	    mem[0] = (data[0] >> 24) & 0xff;
	    break;
	case 1:
	    mem[1] = data[0] & 0xff;
	    mem[0] = (data[0] >> 8) & 0xff;
	    break;
	case 0:
	    mem[0] = data[0] & 0xff;
	    break;
	    
	}
    }
}


/* Memory emulation */

int
memory_read(asi, addr, data, sz, ws)
    int32           asi;
    uint32          addr;
    uint32         *data;
    int32           sz;
    int32          *ws;
{
    int32           mexc;

#ifdef ERRINJ
    if (errmec) {
	if (sis_verbose)
	    printf("Inserted MEC error %d\n",errmec);
	set_sfsr(errmec, addr, asi, 1);
	if (errmec == 5) mecparerror();
	if (errmec == 6) iucomperr();
	errmec = 0;
	return(1);
    }
#endif;

    if ((addr >= mem_ramstart) && (addr < (mem_ramstart + mem_ramsz))) {
	fetch_bytes (asi, &ramb[addr & mem_rammask], data, sz);
	*ws = mem_ramr_ws;
	return (0);
    } else if ((addr >= MEC_START) && (addr < MEC_END)) {
	mexc = mec_read(addr, asi, data);
	if (mexc) {
	    set_sfsr(MEC_ACC, addr, asi, 1);
	    *ws = MEM_EX_WS;
	} else {
	    *ws = 0;
	}
	return (mexc);

#ifdef ERA

    } else if (era) {
    	if ((addr < 0x100000) || 
	    ((addr>= 0x80000000) && (addr < 0x80100000))) {
	    fetch_bytes (asi, &romb[addr & ROM_MASK], data, sz);
	    *ws = 4;
	    return (0);
	} else if ((addr >= 0x10000000) && 
		   (addr < (0x10000000 + (512 << (mec_iocr & 0x0f)))) &&
		   (mec_iocr & 0x10))  {
	    *data = erareg;
	    return (0);
	}
	
    } else  if (addr < mem_romsz) {
	    fetch_bytes (asi, &romb[addr], data, sz);
	    *ws = mem_romr_ws;
	    return (0);

#else
    } else if (addr < mem_romsz) {
	fetch_bytes (asi, &romb[addr], data, sz);
	*ws = mem_romr_ws;
	return (0);
#endif

    }

    printf("Memory exception at %x (illegal address)\n", addr);
    set_sfsr(UIMP_ACC, addr, asi, 1);
    *ws = MEM_EX_WS;
    return (1);
}

int
memory_write(asi, addr, data, sz, ws)
    int32           asi;
    uint32          addr;
    uint32         *data;
    int32           sz;
    int32          *ws;
{
    uint32          byte_addr;
    uint32          byte_mask;
    uint32          waddr;
    uint32         *ram;
    int32           mexc;
    int             i;
    int             wphit[2];

#ifdef ERRINJ
    if (errmec) {
	if (sis_verbose)
	    printf("Inserted MEC error %d\n",errmec);
	set_sfsr(errmec, addr, asi, 0);
	if (errmec == 5) mecparerror();
	if (errmec == 6) iucomperr();
	errmec = 0;
	return(1);
    }
#endif;

    if ((addr >= mem_ramstart) && (addr < (mem_ramstart + mem_ramsz))) {
	if (mem_accprot) {

	    waddr = (addr & 0x7fffff) >> 2;
	    for (i = 0; i < 2; i++)
		wphit[i] =
		    (((asi == 0xa) && (mec_wpr[i] & 1)) ||
		     ((asi == 0xb) && (mec_wpr[i] & 2))) &&
		    ((waddr >= mec_ssa[i]) && ((waddr | (sz == 3)) < mec_sea[i]));

	    if (((mem_blockprot) && (wphit[0] || wphit[1])) ||
		((!mem_blockprot) &&
		 !((mec_wpr[0] && wphit[0]) || (mec_wpr[1] && wphit[1]))
		 )) {
		if (sis_verbose)
		    printf("Memory access protection error at 0x%08x\n", addr);
		set_sfsr(PROT_EXC, addr, asi, 0);
		*ws = MEM_EX_WS;
		return (1);
	    }
	}

	store_bytes (&ramb[addr & mem_rammask], data, sz);

	switch (sz) {
	case 0:
	case 1:
	    *ws = mem_ramw_ws + 3;
	    break;
	case 2:
	    *ws = mem_ramw_ws;
	    break;
	case 3:
	    *ws = 2 * mem_ramw_ws + STD_WS;
	    break;
	}
	return (0);
    } else if ((addr >= MEC_START) && (addr < MEC_END)) {
	if ((sz != 2) || (asi != 0xb)) {
	    set_sfsr(MEC_ACC, addr, asi, 0);
	    *ws = MEM_EX_WS;
	    return (1);
	}
	mexc = mec_write(addr, *data);
	if (mexc) {
	    set_sfsr(MEC_ACC, addr, asi, 0);
	    *ws = MEM_EX_WS;
	} else {
	    *ws = 0;
	}
	return (mexc);

#ifdef ERA

    } else if (era) {
    	if ((erareg & 2) && 
	((addr < 0x100000) || ((addr >= 0x80000000) && (addr < 0x80100000)))) {
	    addr &= ROM_MASK;
	    *ws = sz == 3 ? 8 : 4;
	    store_bytes (&romb[addr], data, sz);
            return (0);
	} else if ((addr >= 0x10000000) && 
		   (addr < (0x10000000 + (512 << (mec_iocr & 0x0f)))) &&
		   (mec_iocr & 0x10))  {
	    erareg = *data & 0x0e;
	    return (0);
	}

    } else if ((addr < mem_romsz) && (mec_memcfg & 0x10000) && (wrp) &&
               (((mec_memcfg & 0x20000) && (sz > 1)) || 
		(!(mec_memcfg & 0x20000) && (sz == 0)))) {

	*ws = mem_romw_ws + 1;
	if (sz == 3)
	    *ws += mem_romw_ws + STD_WS;
	store_bytes (&romb[addr], data, sz);
        return (0);

#else
    } else if ((addr < mem_romsz) && (mec_memcfg & 0x10000) && (wrp) &&
               (((mec_memcfg & 0x20000) && (sz > 1)) || 
		(!(mec_memcfg & 0x20000) && (sz == 0)))) {

	*ws = mem_romw_ws + 1;
	if (sz == 3)
            *ws += mem_romw_ws + STD_WS;
	store_bytes (&romb[addr], data, sz);
        return (0);

#endif

    }
	
    *ws = MEM_EX_WS;
    set_sfsr(UIMP_ACC, addr, asi, 0);
    return (1);
}

static unsigned char  *
get_mem_ptr(addr, size)
    uint32          addr;
    uint32          size;
{
    if ((addr + size) < ROM_SZ) {
	return (&romb[addr]);
    } else if ((addr >= mem_ramstart) && ((addr + size) < mem_ramend)) {
	return (&ramb[addr & mem_rammask]);
    }

#ifdef ERA
      else if ((era) && ((addr <0x100000) || 
	((addr >= (unsigned) 0x80000000) && ((addr + size) < (unsigned) 0x80100000)))) {
	return (&romb[addr & ROM_MASK]);
    }
#endif

    return ((char *) -1);
}

int
sis_memory_write(addr, data, length)
    uint32          addr;
    char           *data;
    uint32          length;
{
    char           *mem;

    if ((mem = get_mem_ptr(addr, length)) == ((char *) -1))
	return (0);

    memcpy(mem, data, length);
    return (length);
}

int
sis_memory_read(addr, data, length)
    uint32          addr;
    char           *data;
    uint32          length;
{
    char           *mem;

    if ((mem = get_mem_ptr(addr, length)) == ((char *) -1))
	return (0);

    memcpy(data, mem, length);
    return (length);
}
