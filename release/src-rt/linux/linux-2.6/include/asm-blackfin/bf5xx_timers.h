/*
 * include/asm/bf5xx_timers.h
 *
 * This file contains the major Data structures and constants
 * used for General Purpose Timer Implementation in BF5xx
 *
 * Copyright (C) 2005 John DeHority
 * Copyright (C) 2006 Hella Aglaia GmbH (awe@aglaia-gmbh.de)
 *
 */

#ifndef _BLACKFIN_TIMERS_H_
#define _BLACKFIN_TIMERS_H_

#undef MAX_BLACKFIN_GPTIMERS
/*
 * BF537: 8 timers:
 */
#if defined(CONFIG_BF537)
#  define MAX_BLACKFIN_GPTIMERS 8
#  define TIMER0_GROUP_REG     TIMER_ENABLE
#endif
/*
 * BF561: 12 timers:
 */
#if defined(CONFIG_BF561)
#  define MAX_BLACKFIN_GPTIMERS 12
#  define TIMER0_GROUP_REG     TMRS8_ENABLE
#  define TIMER8_GROUP_REG     TMRS4_ENABLE
#endif
/*
 * All others: 3 timers:
 */
#if !defined(MAX_BLACKFIN_GPTIMERS)
#  define MAX_BLACKFIN_GPTIMERS 3
#  define TIMER0_GROUP_REG     TIMER_ENABLE
#endif

#define BLACKFIN_GPTIMER_IDMASK ((1UL << MAX_BLACKFIN_GPTIMERS) - 1)
#define BFIN_TIMER_OCTET(x) ((x) >> 3)

/* used in masks for timer_enable() and timer_disable() */
#define TIMER0bit  0x0001  /*  0001b */
#define TIMER1bit  0x0002  /*  0010b */
#define TIMER2bit  0x0004  /*  0100b */

#if (MAX_BLACKFIN_GPTIMERS > 3)
#  define TIMER3bit  0x0008
#  define TIMER4bit  0x0010
#  define TIMER5bit  0x0020
#  define TIMER6bit  0x0040
#  define TIMER7bit  0x0080
#endif

#if (MAX_BLACKFIN_GPTIMERS > 8)
#  define TIMER8bit  0x0100
#  define TIMER9bit  0x0200
#  define TIMER10bit 0x0400
#  define TIMER11bit 0x0800
#endif

#define TIMER0_id   0
#define TIMER1_id   1
#define TIMER2_id   2

#if (MAX_BLACKFIN_GPTIMERS > 3)
#  define TIMER3_id   3
#  define TIMER4_id   4
#  define TIMER5_id   5
#  define TIMER6_id   6
#  define TIMER7_id   7
#endif

#if (MAX_BLACKFIN_GPTIMERS > 8)
#  define TIMER8_id   8
#  define TIMER9_id   9
#  define TIMER10_id 10
#  define TIMER11_id 11
#endif

/* associated timers for ppi framesync: */

#if defined(CONFIG_BF561)
#  define FS0_1_TIMER_ID   TIMER8_id
#  define FS0_2_TIMER_ID   TIMER9_id
#  define FS1_1_TIMER_ID   TIMER10_id
#  define FS1_2_TIMER_ID   TIMER11_id
#  define FS0_1_TIMER_BIT  TIMER8bit
#  define FS0_2_TIMER_BIT  TIMER9bit
#  define FS1_1_TIMER_BIT  TIMER10bit
#  define FS1_2_TIMER_BIT  TIMER11bit
#  undef FS1_TIMER_ID
#  undef FS2_TIMER_ID
#  undef FS1_TIMER_BIT
#  undef FS2_TIMER_BIT
#else
#  define FS1_TIMER_ID  TIMER0_id
#  define FS2_TIMER_ID  TIMER1_id
#  define FS1_TIMER_BIT TIMER0bit
#  define FS2_TIMER_BIT TIMER1bit
#endif

/*
** Timer Configuration Register Bits
*/
#define TIMER_ERR           0xC000
#define TIMER_ERR_OVFL      0x4000
#define TIMER_ERR_PROG_PER  0x8000
#define TIMER_ERR_PROG_PW   0xC000
#define TIMER_EMU_RUN       0x0200
#define	TIMER_TOGGLE_HI     0x0100
#define	TIMER_CLK_SEL       0x0080
#define TIMER_OUT_DIS       0x0040
#define TIMER_TIN_SEL       0x0020
#define TIMER_IRQ_ENA       0x0010
#define TIMER_PERIOD_CNT    0x0008
#define TIMER_PULSE_HI      0x0004
#define TIMER_MODE          0x0003
#define TIMER_MODE_PWM      0x0001
#define TIMER_MODE_WDTH     0x0002
#define TIMER_MODE_EXT_CLK  0x0003

/*
** Timer Status Register Bits
*/
#define TIMER_STATUS_TIMIL0 0x0001
#define TIMER_STATUS_TIMIL1 0x0002
#define TIMER_STATUS_TIMIL2 0x0004
#if (MAX_BLACKFIN_GPTIMERS > 3)
#  define TIMER_STATUS_TIMIL3 0x00000008
#  define TIMER_STATUS_TIMIL4 0x00010000
#  define TIMER_STATUS_TIMIL5 0x00020000
#  define TIMER_STATUS_TIMIL6 0x00040000
#  define TIMER_STATUS_TIMIL7 0x00080000
#  if (MAX_BLACKFIN_GPTIMERS > 8)
#    define TIMER_STATUS_TIMIL8  0x0001
#    define TIMER_STATUS_TIMIL9  0x0002
#    define TIMER_STATUS_TIMIL10 0x0004
#    define TIMER_STATUS_TIMIL11 0x0008
#  endif
#  define TIMER_STATUS_INTR   0x000F000F
#else
#  define TIMER_STATUS_INTR   0x0007	/* any timer interrupt */
#endif

#define TIMER_STATUS_TOVF0  0x0010	/* timer 0 overflow error */
#define TIMER_STATUS_TOVF1  0x0020
#define TIMER_STATUS_TOVF2  0x0040
#if (MAX_BLACKFIN_GPTIMERS > 3)
#  define TIMER_STATUS_TOVF3  0x00000080
#  define TIMER_STATUS_TOVF4  0x00100000
#  define TIMER_STATUS_TOVF5  0x00200000
#  define TIMER_STATUS_TOVF6  0x00400000
#  define TIMER_STATUS_TOVF7  0x00800000
#  if (MAX_BLACKFIN_GPTIMERS > 8)
#    define TIMER_STATUS_TOVF8   0x0010
#    define TIMER_STATUS_TOVF9   0x0020
#    define TIMER_STATUS_TOVF10  0x0040
#    define TIMER_STATUS_TOVF11  0x0080
#  endif
#  define TIMER_STATUS_OFLOW  0x00F000F0
#else
#  define TIMER_STATUS_OFLOW  0x0070	/* any timer overflow */
#endif

/*
** Timer Slave Enable Status : write 1 to clear
*/
#define TIMER_STATUS_TRUN0  0x1000
#define TIMER_STATUS_TRUN1  0x2000
#define TIMER_STATUS_TRUN2  0x4000
#if (MAX_BLACKFIN_GPTIMERS > 3)
#  define TIMER_STATUS_TRUN3  0x00008000
#  define TIMER_STATUS_TRUN4  0x10000000
#  define TIMER_STATUS_TRUN5  0x20000000
#  define TIMER_STATUS_TRUN6  0x40000000
#  define TIMER_STATUS_TRUN7  0x80000000
#  define TIMER_STATUS_TRUN   0xF000F000
#  if (MAX_BLACKFIN_GPTIMERS > 8)
#    define TIMER_STATUS_TRUN8  0x1000
#    define TIMER_STATUS_TRUN9  0x2000
#    define TIMER_STATUS_TRUN10 0x4000
#    define TIMER_STATUS_TRUN11 0x8000
#  endif
#else
#  define TIMER_STATUS_TRUN   0x7000
#endif

/*******************************************************************************
*	GP_TIMER API's
*******************************************************************************/

void  set_gptimer_pwidth    (int timer_id, int width);
int   get_gptimer_pwidth    (int timer_id);
void  set_gptimer_period    (int timer_id, int period);
int   get_gptimer_period    (int timer_id);
int   get_gptimer_count     (int timer_id);
short get_gptimer_intr      (int timer_id);
void  set_gptimer_config    (int timer_id, short config);
short get_gptimer_config    (int timer_id);
void  set_gptimer_pulse_hi  (int timer_id);
void  clear_gptimer_pulse_hi(int timer_id);
void  enable_gptimers       (short mask);
void  disable_gptimers      (short mask);
short get_enabled_timers    (void);
int   get_gptimer_status    (int octet);
void  set_gptimer_status    (int octet, int value);

#endif
