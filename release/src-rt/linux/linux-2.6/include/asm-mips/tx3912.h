/*
 *  include/asm-mips/tx3912.h
 *
 *  Copyright (C) 2001 Steven J. Hill (sjhill@realitydiluted.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Registers for TMPR3912/05 and PR31700 processors
 */
#ifndef _TX3912_H_
#define _TX3912_H_

/*****************************************************************************
 *	Clock Subsystem                                                      *
 *	---------------                                                      *
 *	Chapter 6 in Philips PR31700 and Toshiba TMPR3905/12 User Manuals    *
 *****************************************************************************/
#define TX3912_CLK_CTRL					0x01c0

/*
 * Clock control register values
 */
#define TX3912_CLK_CTRL_CHICLKDIV_MASK			0xff000000
#define TX3912_CLK_CTRL_ENCLKTEST			0x00800000
#define TX3912_CLK_CTRL_CLKTESTSELSIB			0x00400000
#define TX3912_CLK_CTRL_CHIMCLKSEL			0x00200000
#define TX3912_CLK_CTRL_CHICLKDIR			0x00100000
#define TX3912_CLK_CTRL_ENCHIMCLK			0x00080000
#define TX3912_CLK_CTRL_ENVIDCLK			0x00040000
#define TX3912_CLK_CTRL_ENMBUSCLK			0x00020000
#define TX3912_CLK_CTRL_ENSPICLK			0x00010000
#define TX3912_CLK_CTRL_ENTIMERCLK			0x00008000
#define TX3912_CLK_CTRL_ENFASTTIMERCLK			0x00004000
#define TX3912_CLK_CTRL_SIBMCLKDIR			0x00002000
#define TX3912_CLK_CTRL_reserved1			0x00001000
#define TX3912_CLK_CTRL_ENSIBMCLK			0x00000800
#define TX3912_CLK_CTRL_SIBMCLKDIV_6			0x00000600
#define TX3912_CLK_CTRL_SIBMCLKDIV_5			0x00000500
#define TX3912_CLK_CTRL_SIBMCLKDIV_4			0x00000400
#define TX3912_CLK_CTRL_SIBMCLKDIV_3			0x00000300
#define TX3912_CLK_CTRL_SIBMCLKDIV_2			0x00000200
#define TX3912_CLK_CTRL_SIBMCLKDIV_1			0x00000100
#define TX3912_CLK_CTRL_CSERSEL				0x00000080
#define TX3912_CLK_CTRL_CSERDIV_6			0x00000060
#define TX3912_CLK_CTRL_CSERDIV_5			0x00000050
#define TX3912_CLK_CTRL_CSERDIV_4			0x00000040
#define TX3912_CLK_CTRL_CSERDIV_3			0x00000030
#define TX3912_CLK_CTRL_CSERDIV_2			0x00000020
#define TX3912_CLK_CTRL_CSERDIV_1			0x00000010
#define TX3912_CLK_CTRL_ENCSERCLK			0x00000008
#define TX3912_CLK_CTRL_ENIRCLK				0x00000004
#define TX3912_CLK_CTRL_ENUARTACLK			0x00000002
#define TX3912_CLK_CTRL_ENUARTBCLK			0x00000001


/*****************************************************************************
 *	Interrupt Subsystem                                                  *
 *	-------------------                                                  *
 *	Chapter 8 in Philips PR31700 and Toshiba TMPR3905/12 User Manuals    *
 *****************************************************************************/
#define TX3912_INT1_CLEAR				0x0100
#define TX3912_INT2_CLEAR				0x0104
#define TX3912_INT3_CLEAR				0x0108
#define TX3912_INT4_CLEAR				0x010c
#define TX3912_INT5_CLEAR				0x0110
#define TX3912_INT1_ENABLE				0x0118
#define TX3912_INT2_ENABLE				0x011c
#define TX3912_INT3_ENABLE				0x0120
#define TX3912_INT4_ENABLE				0x0124
#define TX3912_INT5_ENABLE				0x0128
#define TX3912_INT6_ENABLE				0x012c
#define TX3912_INT1_STATUS				0x0100
#define TX3912_INT2_STATUS				0x0104
#define TX3912_INT3_STATUS				0x0108
#define TX3912_INT4_STATUS				0x010c
#define TX3912_INT5_STATUS				0x0110
#define TX3912_INT6_STATUS				0x0114

/*
 * Interrupt 2 register values
 */
#define TX3912_INT2_UARTARXINT				0x80000000
#define TX3912_INT2_UARTARXOVERRUNINT			0x40000000
#define TX3912_INT2_UARTAFRAMEERRINT			0x20000000
#define TX3912_INT2_UARTABREAKINT			0x10000000
#define TX3912_INT2_UARTAPARITYINT			0x08000000
#define TX3912_INT2_UARTATXINT				0x04000000
#define TX3912_INT2_UARTATXOVERRUNINT			0x02000000
#define TX3912_INT2_UARTAEMPTYINT			0x01000000
#define TX3912_INT2_UARTADMAFULLINT			0x00800000
#define TX3912_INT2_UARTADMAHALFINT			0x00400000
#define TX3912_INT2_UARTBRXINT				0x00200000
#define TX3912_INT2_UARTBRXOVERRUNINT			0x00100000
#define TX3912_INT2_UARTBFRAMEERRINT			0x00080000
#define TX3912_INT2_UARTBBREAKINT			0x00040000
#define TX3912_INT2_UARTBPARITYINT			0x00020000
#define TX3912_INT2_UARTBTXINT				0x00010000
#define TX3912_INT2_UARTBTXOVERRUNINT			0x00008000
#define TX3912_INT2_UARTBEMPTYINT			0x00004000
#define TX3912_INT2_UARTBDMAFULLINT			0x00002000
#define TX3912_INT2_UARTBDMAHALFINT			0x00001000
#define TX3912_INT2_UARTA_RX_BITS			0xf8000000
#define TX3912_INT2_UARTA_TX_BITS			0x07c00000
#define TX3912_INT2_UARTB_RX_BITS			0x003e0000
#define TX3912_INT2_UARTB_TX_BITS			0x0001f000

/*
 * Interrupt 5 register values
 */
#define TX3912_INT5_RTCINT				0x80000000
#define TX3912_INT5_ALARMINT				0x40000000
#define TX3912_INT5_PERINT				0x20000000
#define TX3912_INT5_STPTIMERINT				0x10000000
#define TX3912_INT5_POSPWRINT				0x08000000
#define TX3912_INT5_NEGPWRINT				0x04000000
#define TX3912_INT5_POSPWROKINT				0x02000000
#define TX3912_INT5_NEGPWROKINT				0x01000000
#define TX3912_INT5_POSONBUTINT				0x00800000
#define TX3912_INT5_NEGONBUTINT				0x00400000
#define TX3912_INT5_SPIBUFAVAILINT			0x00200000
#define TX3912_INT5_SPIERRINT				0x00100000
#define TX3912_INT5_SPIRCVINT				0x00080000
#define TX3912_INT5_SPIEMPTYINT				0x00040000
#define TX3912_INT5_IRCONSMINT				0x00020000
#define TX3912_INT5_CARSTINT				0x00010000
#define TX3912_INT5_POSCARINT				0x00008000
#define TX3912_INT5_NEGCARINT				0x00004000
#define TX3912_INT5_IOPOSINT6				0x00002000
#define TX3912_INT5_IOPOSINT5				0x00001000
#define TX3912_INT5_IOPOSINT4				0x00000800
#define TX3912_INT5_IOPOSINT3				0x00000400
#define TX3912_INT5_IOPOSINT2				0x00000200
#define TX3912_INT5_IOPOSINT1				0x00000100
#define TX3912_INT5_IOPOSINT0				0x00000080
#define TX3912_INT5_IONEGINT6				0x00000040
#define TX3912_INT5_IONEGINT5				0x00000020
#define TX3912_INT5_IONEGINT4				0x00000010
#define TX3912_INT5_IONEGINT3				0x00000008
#define TX3912_INT5_IONEGINT2				0x00000004
#define TX3912_INT5_IONEGINT1				0x00000002
#define TX3912_INT5_IONEGINT0				0x00000001

/*
 * Interrupt 6 status register values
 */
#define TX3912_INT6_STATUS_IRQHIGH			0x80000000
#define TX3912_INT6_STATUS_IRQLOW			0x40000000
#define TX3912_INT6_STATUS_reserved6			0x3fffffc0
#define TX3912_INT6_STATUS_INTVEC_POSNEGPWROKINT	0x0000003c
#define TX3912_INT6_STATUS_INTVEC_ALARMINT		0x00000038
#define TX3912_INT6_STATUS_INTVEC_PERINT		0x00000034
#define TX3912_INT6_STATUS_INTVEC_reserved5		0x00000030
#define TX3912_INT6_STATUS_INTVEC_UARTARXINT		0x0000002c
#define TX3912_INT6_STATUS_INTVEC_UARTBRXINT		0x00000028
#define TX3912_INT6_STATUS_INTVEC_reserved4		0x00000024
#define TX3912_INT6_STATUS_INTVEC_IOPOSINT65		0x00000020
#define TX3912_INT6_STATUS_INTVEC_reserved3		0x0000001c
#define TX3912_INT6_STATUS_INTVEC_IONEGINT65		0x00000018
#define TX3912_INT6_STATUS_INTVEC_reserved2		0x00000014
#define TX3912_INT6_STATUS_INTVEC_SNDDMACNTINT		0x00000010
#define TX3912_INT6_STATUS_INTVEC_TELDMACNTINT		0x0000000c
#define TX3912_INT6_STATUS_INTVEC_CHIDMACNTINT		0x00000008
#define TX3912_INT6_STATUS_INTVEC_IOPOSNEGINT0		0x00000004
#define TX3912_INT6_STATUS_INTVEC_STDHANDLER		0x00000000
#define TX3912_INT6_STATUS_reserved1			0x00000003

/*
 * Interrupt 6 enable register values
 */
#define TX3912_INT6_ENABLE_reserved5			0xfff80000
#define TX3912_INT6_ENABLE_GLOBALEN			0x00040000
#define TX3912_INT6_ENABLE_IRQPRITEST			0x00020000
#define TX3912_INT6_ENABLE_IRQTEST			0x00010000
#define TX3912_INT6_ENABLE_PRIORITYMASK_POSNEGPWROKINT	0x00008000
#define TX3912_INT6_ENABLE_PRIORITYMASK_ALARMINT	0x00004000
#define TX3912_INT6_ENABLE_PRIORITYMASK_PERINT		0x00002000
#define TX3912_INT6_ENABLE_PRIORITYMASK_reserved4	0x00001000
#define TX3912_INT6_ENABLE_PRIORITYMASK_UARTARXINT	0x00000800
#define TX3912_INT6_ENABLE_PRIORITYMASK_UARTBRXINT	0x00000400
#define TX3912_INT6_ENABLE_PRIORITYMASK_reserved3	0x00000200
#define TX3912_INT6_ENABLE_PRIORITYMASK_IOPOSINT65	0x00000100
#define TX3912_INT6_ENABLE_PRIORITYMASK_reserved2	0x00000080
#define TX3912_INT6_ENABLE_PRIORITYMASK_IONEGINT65	0x00000040
#define TX3912_INT6_ENABLE_PRIORITYMASK_reserved1	0x00000020
#define TX3912_INT6_ENABLE_PRIORITYMASK_SNDDMACNTINT	0x00000010
#define TX3912_INT6_ENABLE_PRIORITYMASK_TELDMACNTINT	0x00000008
#define TX3912_INT6_ENABLE_PRIORITYMASK_CHIDMACNTINT	0x00000004
#define TX3912_INT6_ENABLE_PRIORITYMASK_IOPOSNEGINT0	0x00000002
#define TX3912_INT6_ENABLE_PRIORITYMASK_STDHANDLER	0x00000001
#define TX3912_INT6_ENABLE_HIGH_PRIORITY		0x0000ffff


/*****************************************************************************
 *	Power Subsystem                                                      *
 *	---------------                                                      *
 *	Chapter 11 in Philips PR31700 User Manual                            *
 *	Chapter 12 in Toshiba TMPR3905/12 User Manual                        *
 *****************************************************************************/
#define TX3912_POWER_CTRL				0x01c4

/*
 * Power control register values
 */
#define TX3912_POWER_CTRL_ONBUTN			0x80000000
#define TX3912_POWER_CTRL_PWRINT			0x40000000
#define TX3912_POWER_CTRL_PWROK				0x20000000
#define TX3912_POWER_CTRL_VIDRF_MASK			0x18000000
#define TX3912_POWER_CTRL_SLOWBUS			0x04000000
#define TX3912_POWER_CTRL_DIVMOD			0x02000000
#define TX3912_POWER_CTRL_reserved2			0x01ff0000
#define TX3912_POWER_CTRL_STPTIMERVAL_MASK		0x0000f000
#define TX3912_POWER_CTRL_ENSTPTIMER			0x00000800
#define TX3912_POWER_CTRL_ENFORCESHUTDWN		0x00000400
#define TX3912_POWER_CTRL_FORCESHUTDWN			0x00000200
#define TX3912_POWER_CTRL_FORCESHUTDWNOCC		0x00000100
#define TX3912_POWER_CTRL_SELC2MS			0x00000080
#define TX3912_POWER_CTRL_reserved1			0x00000040
#define TX3912_POWER_CTRL_BPDBVCC3			0x00000020
#define TX3912_POWER_CTRL_STOPCPU			0x00000010
#define TX3912_POWER_CTRL_DBNCONBUTN			0x00000008
#define TX3912_POWER_CTRL_COLDSTART			0x00000004
#define TX3912_POWER_CTRL_PWRCS				0x00000002
#define TX3912_POWER_CTRL_VCCON				0x00000001


/*****************************************************************************
 *	Timer Subsystem                                                      *
 *	---------------                                                      *
 *	Chapter 14 in Philips PR31700 User Manual                            *
 *	Chapter 15 in Toshiba TMPR3905/12 User Manual                        *
 *****************************************************************************/
#define TX3912_RTC_HIGH					0x0140
#define TX3912_RTC_LOW					0x0144
#define TX3912_RTC_ALARM_HIGH				0x0148
#define TX3912_RTC_ALARM_LOW				0x014c
#define TX3912_TIMER_CTRL				0x0150
#define TX3912_TIMER_PERIOD				0x0154

/*
 * Timer control register values
 */
#define TX3912_TIMER_CTRL_FREEZEPRE			0x00000080
#define TX3912_TIMER_CTRL_FREEZERTC			0x00000040
#define TX3912_TIMER_CTRL_FREEZETIMER			0x00000020
#define TX3912_TIMER_CTRL_ENPERTIMER			0x00000010
#define TX3912_TIMER_CTRL_RTCCLEAR			0x00000008
#define TX3912_TIMER_CTRL_TESTC8MS			0x00000004
#define TX3912_TIMER_CTRL_ENTESTCLK			0x00000002
#define TX3912_TIMER_CTRL_ENRTCTST			0x00000001

/*
 * The periodic timer has granularity of 868 nanoseconds which
 * results in a count of (1.152 x 10^6 / 100) in order to achieve
 * a 10 millisecond periodic system clock.
 */
#define TX3912_SYS_TIMER_VALUE				(1152000/HZ)


/*****************************************************************************
 *	UART Subsystem                                                       *
 *	--------------                                                       *
 *	Chapter 15 in Philips PR31700 User Manual                            *
 *	Chapter 16 in Toshiba TMPR3905/12 User Manual                        *
 *****************************************************************************/
#define TX3912_UARTA_CTRL1				0x00b0
#define TX3912_UARTA_CTRL2				0x00b4
#define TX3912_UARTA_DMA_CTRL1				0x00b8
#define TX3912_UARTA_DMA_CTRL2				0x00bc
#define TX3912_UARTA_DMA_CNT				0x00c0
#define TX3912_UARTA_DATA				0x00c4
#define TX3912_UARTB_CTRL1				0x00c8
#define TX3912_UARTB_CTRL2				0x00cc
#define TX3912_UARTB_DMA_CTRL1				0x00d0
#define TX3912_UARTB_DMA_CTRL2				0x00d4
#define TX3912_UARTB_DMA_CNT				0x00d8
#define TX3912_UARTB_DATA				0x00dc

/*
 * UART Control Register 1 values
 */
#define TX3912_UART_CTRL1_UARTON			0x80000000
#define TX3912_UART_CTRL1_EMPTY				0x40000000
#define TX3912_UART_CTRL1_PRXHOLDFULL			0x20000000
#define TX3912_UART_CTRL1_RXHOLDFULL			0x10000000
#define TX3912_UART_CTRL1_reserved1			0x0fff0000
#define TX3912_UART_CTRL1_ENDMARX			0x00008000
#define TX3912_UART_CTRL1_ENDMATX			0x00004000
#define TX3912_UART_CTRL1_TESTMODE			0x00002000
#define TX3912_UART_CTRL1_ENBREAKHALT			0x00001000
#define TX3912_UART_CTRL1_ENDMATEST			0x00000800
#define TX3912_UART_CTRL1_ENDMALOOP			0x00000400
#define TX3912_UART_CTRL1_PULSEOPT1			0x00000200
#define TX3912_UART_CTRL1_PULSEOPT1			0x00000100
#define TX3912_UART_CTRL1_DTINVERT			0x00000080
#define TX3912_UART_CTRL1_DISTXD			0x00000040
#define TX3912_UART_CTRL1_TWOSTOP			0x00000020
#define TX3912_UART_CTRL1_LOOPBACK			0x00000010
#define TX3912_UART_CTRL1_BIT_7				0x00000008
#define TX3912_UART_CTRL1_EVENPARITY			0x00000004
#define TX3912_UART_CTRL1_ENPARITY			0x00000002
#define TX3912_UART_CTRL1_ENUART			0x00000001

/*
 * UART Control Register 2 values
 */
#define TX3912_UART_CTRL2_B230400			0x0000	/*   0 */
#define TX3912_UART_CTRL2_B115200			0x0001	/*   1 */
#define TX3912_UART_CTRL2_B76800			0x0002	/*   2 */
#define TX3912_UART_CTRL2_B57600			0x0003	/*   3 */
#define TX3912_UART_CTRL2_B38400			0x0005	/*   5 */
#define TX3912_UART_CTRL2_B19200			0x000b	/*  11 */
#define TX3912_UART_CTRL2_B9600				0x0016	/*  22 */
#define TX3912_UART_CTRL2_B4800				0x002f	/*  47 */
#define TX3912_UART_CTRL2_B2400				0x005f	/*  95 */
#define TX3912_UART_CTRL2_B1200				0x00bf	/* 191 */
#define TX3912_UART_CTRL2_B600				0x017f	/* 383 */
#define TX3912_UART_CTRL2_B300				0x02ff	/* 767 */

/*****************************************************************************
 *	Video Subsystem                                                      *
 *	---------------                                                      *
 *	Chapter 16 in Philips PR31700 User Manual                            *
 *	Chapter 17 in Toshiba TMPR3905/12 User Manual                        *
 *****************************************************************************/
#define TX3912_VIDEO_CTRL1				0x0028
#define TX3912_VIDEO_CTRL2				0x002c
#define TX3912_VIDEO_CTRL3				0x0030
#define TX3912_VIDEO_CTRL4				0x0034
#define TX3912_VIDEO_CTRL5				0x0038
#define TX3912_VIDEO_CTRL6				0x003c
#define TX3912_VIDEO_CTRL7				0x0040
#define TX3912_VIDEO_CTRL8				0x0044
#define TX3912_VIDEO_CTRL9				0x0048
#define TX3912_VIDEO_CTRL10				0x004c
#define TX3912_VIDEO_CTRL11				0x0050
#define TX3912_VIDEO_CTRL12				0x0054
#define TX3912_VIDEO_CTRL13				0x0058
#define TX3912_VIDEO_CTRL14				0x005c

/*
 * Video Control Register 1 values
 */
#define TX3912_VIDEO_CTRL1_LINECNT			0xffc00000
#define TX3912_VIDEO_CTRL1_LOADDLY			0x00200000
#define TX3912_VIDEO_CTRL1_BAUDVAL			0x001f0000
#define TX3912_VIDEO_CTRL1_VIDDONEVAL			0x0000fe00
#define TX3912_VIDEO_CTRL1_ENFREEZEFRAME		0x00000100
#define TX3912_VIDEO_CTRL1_BITSEL_MASK			0x000000c0
#define TX3912_VIDEO_CTRL1_BITSEL_8BIT_COLOR		0x000000c0
#define TX3912_VIDEO_CTRL1_BITSEL_4BIT_GRAY		0x00000080
#define TX3912_VIDEO_CTRL1_BITSEL_2BIT_GRAY		0x00000040
#define TX3912_VIDEO_CTRL1_DISPSPLIT			0x00000020
#define TX3912_VIDEO_CTRL1_DISP8			0x00000010
#define TX3912_VIDEO_CTRL1_DFMODE			0x00000008
#define TX3912_VIDEO_CTRL1_INVVID			0x00000004
#define TX3912_VIDEO_CTRL1_DISPON			0x00000002
#define TX3912_VIDEO_CTRL1_ENVID			0x00000001

#endif	/* _TX3912_H_ */
