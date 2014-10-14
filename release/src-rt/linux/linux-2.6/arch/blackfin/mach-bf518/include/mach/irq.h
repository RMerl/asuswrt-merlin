/*
 * Copyright 2008 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later
 */

#ifndef _BF518_IRQ_H_
#define _BF518_IRQ_H_

/*
 * Interrupt source definitions
	Event Source    Core Event Name
	Core        Emulation               **
	Events         (highest priority)  EMU         0
	Reset                   RST         1
	NMI                     NMI         2
	Exception               EVX         3
	Reserved                --          4
	Hardware Error          IVHW        5
	Core Timer              IVTMR       6 *

	.....

	 Software Interrupt 1    IVG14       31
	 Software Interrupt 2    --
	 (lowest priority)  IVG15       32 *
*/

#define NR_PERI_INTS    (2 * 32)

/* The ABSTRACT IRQ definitions */
/** the first seven of the following are fixed, the rest you change if you need to **/
#define IRQ_EMU			0	/* Emulation */
#define IRQ_RST			1	/* reset */
#define IRQ_NMI			2	/* Non Maskable */
#define IRQ_EVX			3	/* Exception */
#define IRQ_UNUSED		4	/* - unused interrupt */
#define IRQ_HWERR		5	/* Hardware Error */
#define IRQ_CORETMR		6	/* Core timer */

#define BFIN_IRQ(x)		((x) + 7)

#define IRQ_PLL_WAKEUP		BFIN_IRQ(0)	/* PLL Wakeup Interrupt */
#define IRQ_DMA0_ERROR		BFIN_IRQ(1)	/* DMA Error 0 (generic) */
#define IRQ_DMAR0_BLK		BFIN_IRQ(2)	/* DMAR0 Block Interrupt */
#define IRQ_DMAR1_BLK		BFIN_IRQ(3)	/* DMAR1 Block Interrupt */
#define IRQ_DMAR0_OVR		BFIN_IRQ(4)	/* DMAR0 Overflow Error */
#define IRQ_DMAR1_OVR		BFIN_IRQ(5)	/* DMAR1 Overflow Error */
#define IRQ_PPI_ERROR		BFIN_IRQ(6)	/* PPI Error */
#define IRQ_MAC_ERROR		BFIN_IRQ(7)	/* MAC Status */
#define IRQ_SPORT0_ERROR	BFIN_IRQ(8)	/* SPORT0 Status */
#define IRQ_SPORT1_ERROR	BFIN_IRQ(9)	/* SPORT1 Status */
#define IRQ_PTP_ERROR		BFIN_IRQ(10)	/* PTP Error Interrupt */
#define IRQ_UART0_ERROR		BFIN_IRQ(12)	/* UART0 Status */
#define IRQ_UART1_ERROR		BFIN_IRQ(13)	/* UART1 Status */
#define IRQ_RTC			BFIN_IRQ(14)	/* RTC */
#define IRQ_PPI      		BFIN_IRQ(15)	/* DMA Channel 0 (PPI) */
#define IRQ_SPORT0_RX		BFIN_IRQ(16)	/* DMA 3 Channel (SPORT0 RX) */
#define IRQ_SPORT0_TX		BFIN_IRQ(17)	/* DMA 4 Channel (SPORT0 TX) */
#define IRQ_RSI			BFIN_IRQ(17)	/* DMA 4 Channel (RSI) */
#define IRQ_SPORT1_RX		BFIN_IRQ(18)	/* DMA 5 Channel (SPORT1 RX/SPI) */
#define IRQ_SPI1		BFIN_IRQ(18)	/* DMA 5 Channel (SPI1) */
#define IRQ_SPORT1_TX		BFIN_IRQ(19)	/* DMA 6 Channel (SPORT1 TX) */
#define IRQ_TWI      		BFIN_IRQ(20)	/* TWI */
#define IRQ_SPI0     		BFIN_IRQ(21)	/* DMA 7 Channel (SPI0) */
#define IRQ_UART0_RX 		BFIN_IRQ(22)	/* DMA8 Channel (UART0 RX) */
#define IRQ_UART0_TX 		BFIN_IRQ(23)	/* DMA9 Channel (UART0 TX) */
#define IRQ_UART1_RX 		BFIN_IRQ(24)	/* DMA10 Channel (UART1 RX) */
#define IRQ_UART1_TX 		BFIN_IRQ(25)	/* DMA11 Channel (UART1 TX) */
#define IRQ_OPTSEC   		BFIN_IRQ(26)	/* OTPSEC Interrupt */
#define IRQ_CNT   		BFIN_IRQ(27)	/* GP Counter */
#define IRQ_MAC_RX   		BFIN_IRQ(28)	/* DMA1 Channel (MAC RX) */
#define IRQ_PORTH_INTA   	BFIN_IRQ(29)	/* Port H Interrupt A */
#define IRQ_MAC_TX		BFIN_IRQ(30)	/* DMA2 Channel (MAC TX) */
#define IRQ_PORTH_INTB		BFIN_IRQ(31)	/* Port H Interrupt B */
#define IRQ_TIMER0		BFIN_IRQ(32)	/* Timer 0 */
#define IRQ_TIMER1		BFIN_IRQ(33)	/* Timer 1 */
#define IRQ_TIMER2		BFIN_IRQ(34)	/* Timer 2 */
#define IRQ_TIMER3		BFIN_IRQ(35)	/* Timer 3 */
#define IRQ_TIMER4		BFIN_IRQ(36)	/* Timer 4 */
#define IRQ_TIMER5		BFIN_IRQ(37)	/* Timer 5 */
#define IRQ_TIMER6		BFIN_IRQ(38)	/* Timer 6 */
#define IRQ_TIMER7		BFIN_IRQ(39)	/* Timer 7 */
#define IRQ_PORTG_INTA		BFIN_IRQ(40)	/* Port G Interrupt A */
#define IRQ_PORTG_INTB		BFIN_IRQ(41)	/* Port G Interrupt B */
#define IRQ_MEM_DMA0		BFIN_IRQ(42)	/* MDMA Stream 0 */
#define IRQ_MEM_DMA1		BFIN_IRQ(43)	/* MDMA Stream 1 */
#define IRQ_WATCH		BFIN_IRQ(44)	/* Software Watchdog Timer */
#define IRQ_PORTF_INTA		BFIN_IRQ(45)	/* Port F Interrupt A */
#define IRQ_PORTF_INTB		BFIN_IRQ(46)	/* Port F Interrupt B */
#define IRQ_SPI0_ERROR		BFIN_IRQ(47)	/* SPI0 Status */
#define IRQ_SPI1_ERROR		BFIN_IRQ(48)	/* SPI1 Error */
#define IRQ_RSI_INT0		BFIN_IRQ(51)	/* RSI Interrupt0 */
#define IRQ_RSI_INT1		BFIN_IRQ(52)	/* RSI Interrupt1 */
#define IRQ_PWM_TRIP		BFIN_IRQ(53)	/* PWM Trip Interrupt */
#define IRQ_PWM_SYNC		BFIN_IRQ(54)	/* PWM Sync Interrupt */
#define IRQ_PTP_STAT		BFIN_IRQ(55)	/* PTP Stat Interrupt */

#define SYS_IRQS        	BFIN_IRQ(63)	/* 70 */

#define IRQ_PF0         71
#define IRQ_PF1         72
#define IRQ_PF2         73
#define IRQ_PF3         74
#define IRQ_PF4         75
#define IRQ_PF5         76
#define IRQ_PF6         77
#define IRQ_PF7         78
#define IRQ_PF8         79
#define IRQ_PF9         80
#define IRQ_PF10        81
#define IRQ_PF11        82
#define IRQ_PF12        83
#define IRQ_PF13        84
#define IRQ_PF14        85
#define IRQ_PF15        86

#define IRQ_PG0         87
#define IRQ_PG1         88
#define IRQ_PG2         89
#define IRQ_PG3         90
#define IRQ_PG4         91
#define IRQ_PG5         92
#define IRQ_PG6         93
#define IRQ_PG7         94
#define IRQ_PG8         95
#define IRQ_PG9         96
#define IRQ_PG10        97
#define IRQ_PG11        98
#define IRQ_PG12        99
#define IRQ_PG13        100
#define IRQ_PG14        101
#define IRQ_PG15        102

#define IRQ_PH0         103
#define IRQ_PH1         104
#define IRQ_PH2         105
#define IRQ_PH3         106
#define IRQ_PH4         107
#define IRQ_PH5         108
#define IRQ_PH6         109
#define IRQ_PH7         110
#define IRQ_PH8         111
#define IRQ_PH9         112
#define IRQ_PH10        113
#define IRQ_PH11        114
#define IRQ_PH12        115
#define IRQ_PH13        116
#define IRQ_PH14        117
#define IRQ_PH15        118

#define GPIO_IRQ_BASE	IRQ_PF0

#define IRQ_MAC_PHYINT		119 /* PHY_INT Interrupt */
#define IRQ_MAC_MMCINT		120 /* MMC Counter Interrupt */
#define IRQ_MAC_RXFSINT		121 /* RX Frame-Status Interrupt */
#define IRQ_MAC_TXFSINT		122 /* TX Frame-Status Interrupt */
#define IRQ_MAC_WAKEDET		123 /* Wake-Up Interrupt */
#define IRQ_MAC_RXDMAERR	124 /* RX DMA Direction Error Interrupt */
#define IRQ_MAC_TXDMAERR	125 /* TX DMA Direction Error Interrupt */
#define IRQ_MAC_STMDONE		126 /* Station Mgt. Transfer Done Interrupt */

#define NR_MACH_IRQS	(IRQ_MAC_STMDONE + 1)
#define NR_IRQS		(NR_MACH_IRQS + NR_SPARE_IRQS)

#define IVG7            7
#define IVG8            8
#define IVG9            9
#define IVG10           10
#define IVG11           11
#define IVG12           12
#define IVG13           13
#define IVG14           14
#define IVG15           15

/* IAR0 BIT FIELDS */
#define IRQ_PLL_WAKEUP_POS	0
#define IRQ_DMA0_ERROR_POS	4
#define IRQ_DMAR0_BLK_POS 	8
#define IRQ_DMAR1_BLK_POS 	12
#define IRQ_DMAR0_OVR_POS 	16
#define IRQ_DMAR1_OVR_POS 	20
#define IRQ_PPI_ERROR_POS 	24
#define IRQ_MAC_ERROR_POS 	28

/* IAR1 BIT FIELDS */
#define IRQ_SPORT0_ERROR_POS	0
#define IRQ_SPORT1_ERROR_POS	4
#define IRQ_PTP_ERROR_POS	8
#define IRQ_UART0_ERROR_POS 	16
#define IRQ_UART1_ERROR_POS 	20
#define IRQ_RTC_POS         	24
#define IRQ_PPI_POS         	28

/* IAR2 BIT FIELDS */
#define IRQ_SPORT0_RX_POS	0
#define IRQ_SPORT0_TX_POS	4
#define IRQ_RSI_POS		4
#define IRQ_SPORT1_RX_POS	8
#define IRQ_SPI1_POS		8
#define IRQ_SPORT1_TX_POS	12
#define IRQ_TWI_POS      	16
#define IRQ_SPI0_POS      	20
#define IRQ_UART0_RX_POS 	24
#define IRQ_UART0_TX_POS 	28

/* IAR3 BIT FIELDS */
#define IRQ_UART1_RX_POS  	0
#define IRQ_UART1_TX_POS  	4
#define IRQ_OPTSEC_POS    	8
#define IRQ_CNT_POS       	12
#define IRQ_MAC_RX_POS    	16
#define IRQ_PORTH_INTA_POS	20
#define IRQ_MAC_TX_POS    	24
#define IRQ_PORTH_INTB_POS	28

/* IAR4 BIT FIELDS */
#define IRQ_TIMER0_POS		0
#define IRQ_TIMER1_POS		4
#define IRQ_TIMER2_POS		8
#define IRQ_TIMER3_POS		12
#define IRQ_TIMER4_POS		16
#define IRQ_TIMER5_POS		20
#define IRQ_TIMER6_POS		24
#define IRQ_TIMER7_POS		28

/* IAR5 BIT FIELDS */
#define IRQ_PORTG_INTA_POS	0
#define IRQ_PORTG_INTB_POS	4
#define IRQ_MEM_DMA0_POS  	8
#define IRQ_MEM_DMA1_POS  	12
#define IRQ_WATCH_POS     	16
#define IRQ_PORTF_INTA_POS	20
#define IRQ_PORTF_INTB_POS	24
#define IRQ_SPI0_ERROR_POS 	28

/* IAR6 BIT FIELDS */
#define IRQ_SPI1_ERROR_POS  	0
#define IRQ_RSI_INT0_POS   	12
#define IRQ_RSI_INT1_POS   	16
#define IRQ_PWM_TRIP_POS   	20
#define IRQ_PWM_SYNC_POS   	24
#define IRQ_PTP_STAT_POS    	28

#endif				/* _BF518_IRQ_H_ */
