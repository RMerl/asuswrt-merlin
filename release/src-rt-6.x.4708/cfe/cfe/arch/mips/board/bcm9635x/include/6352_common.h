/*
<:copyright-broadcom 
 
 Copyright (c) 2002 Broadcom Corporation 
 All Rights Reserved 
 No portions of this material may be reproduced in any form without the 
 written permission of: 
          Broadcom Corporation 
          16215 Alton Parkway 
          Irvine, California 92619 
 All information contained in this document is Broadcom Corporation 
 company private, proprietary, and trade secret. 
 
:>
*/
/***********************************************************************/
/*                                                                     */
/*   MODULE: 6352_common.h                                             */
/*   DATE:    96/12/19                                                 */
/*   PURPOSE: Define addresses of major hardware components of         */
/*            BCM6352                                                  */
/*                                                                     */
/***********************************************************************/
#ifndef __BCM6352_MAP_COMMON_H
#define __BCM6352_MAP_COMMON_H

#if __cplusplus
extern "C" {
#endif

/* matches isb_decoder.v */
#define INTC_BASE     0xfffe0000    /* interrupts controller registers */
#define BB_BASE       0xfffe0100    /* bus bridge registers */
#define TIMR_BASE     0xfffe0200    /* timer registers */
#define UART_BASE     0xfffe0300    /* uart registers */
#define GPIO_BASE     0xfffe0400    /* gpio registers */
#define APM_BASE      0xfffe0a00    /* APM Registers */
#define VPM_BASE      0xfffe0b00    /* VPM Registers */
#define MTACTL_BASE   0xfffe0c00    /* APM/VPM/ORM Control Registers */
#define ESWITCH_BASE  0xfffe1000    /* Ethernet switch registers */
#define EBIC_BASE     0xfffe2000    /* EBI control registers */
#define USB_BASE      0xfffe2100    /* USB controll registers */
#define SEC_BASE      0xfffe2200    /* security module registers */
#define SDRAM_BASE    0xfffe2300    /* SDRAM control registers */
#define HDLC_BASE     0xfffe2400    /* HDLC registers */
#define DMA_BASE      0xfffe2800    /* DMA control registers */
#define ATM_BASE      0xfffe3000    /* ATM registers */
#define DSP_BASE      0xfffe4000    /* DSP registers */

/* DMA channel assignments */
#define EMAC_RX_CHAN            1
#define EMAC_TX_CHAN            2
#define DSP_RX_CHAN             3
#define DSP_TX_CHAN             4
#define EBI_RX_CHAN             5
#define EBI_TX_CHAN             6
#define HDLC_RX_CHAN            7
#define HDLC_TX_CHAN            8
#define RESERVED_RX_CHAN        9
#define RESERVED_TX_CHAN        10
#define SEC_RX_CHAN             11
#define SEC_TX_CHAN             12
#define USB_BULK_RX_CHAN        13
#define USB_BULK_TX_CHAN        14
#define USB_ISO_RX_CHAN         15
#define USB_ISO_TX_CHAN         16
#define USB_CNTL_RX_CHAN        17
#define USB_CNTL_TX_CHAN        18

/*
#-----------------------------------------------------------------------*
#                                                                       *
#************************************************************************
*/
#define SDR_INIT_CTL        0x00
    /* Control Bits */
#define SDR_9BIT_COL        (1<<11)
#define SDR_32BIT           (1<<10)
#define SDR_PWR_DN          (1<<9)
#define SDR_SELF_REF        (1<<8)
#define SDR_SOFT_RST        (1<<7)
#define SDR_64x32           (3<<4)
#define SDR_128MEG          (2<<4)
#define SDR_64MEG           (1<<4)
#define SDR_16MEG           (0<<4)
#define SDR_ENABLE          (1<<3)
#define SDR_MRS_CMD         (1<<2)
#define SDR_PRE_CMD         (1<<1)
#define SDR_CBR_CMD         (1<<0)

#define SDR_CFG_REG         0x04
    /* Control Bits */
#define SDR_FULL_PG         0x00
#define SDR_BURST8          0x01
#define SDR_BURST4          0x02
#define SDR_BURST2          0x03
#define SDR_FAST_MEM        (1<<2)
#define SDR_SLOW_MEM        0x00

#define SDR_REF_CTL         0x08
    /* Control Bits */
#define SDR_REF_EN          (1<<15)

#define SDR_MEM_BASE        0x0c
    /*  Control Bits */
#define DRAM2MBSPC          0x00000000
#define DRAM8MBSPC          0x00000001
#define DRAM16MBSPC         0x00000002
#define DRAM32MBSPC         0x00000003
#define DRAM64MBSPC         0x00000004

#define DRAM2MEG            0x00000000  /*  See SDRAM config */
#define DRAM8MEG            0x00000001  /*  See SDRAM config */
#define DRAM16MEG           0x00000002  /*  See SDRAM config */
#define DRAM32MEG           0x00000003  /*  See SDRAM config */
#define DRAM64MEG           0x00000004  /*  See SDRAM config */

/*
#-----------------------------------------------------------------------*
#                                                                       *
#************************************************************************
*/
#define CS0BASE         0x00
#define CS0CNTL         0x04
#define CS1BASE         0x08
#define CS1CNTL         0x0c
#define CS2BASE         0x10
#define CS2CNTL         0x14
#define CS3BASE         0x18
#define CS3CNTL         0x1c
#define CS4BASE         0x20
#define CS4CNTL         0x24
#define CS5BASE         0x28
#define CS5CNTL         0x2c
#define CS6BASE         0x30
#define CS6CNTL         0x34
#define CS7BASE         0x38
#define CS7CNTL         0x3c
#define EBICONFIG       0x40

/*
# CSxBASE settings
#   Size in low 4 bits
#   Base Address for match in upper 24 bits
*/
#define EBI_SIZE_8K         0
#define EBI_SIZE_16K        1
#define EBI_SIZE_32K        2
#define EBI_SIZE_64K        3
#define EBI_SIZE_128K       4
#define EBI_SIZE_256K       5
#define EBI_SIZE_512K       6
#define EBI_SIZE_1M         7
#define EBI_SIZE_2M         8
#define EBI_SIZE_4M         9
#define EBI_SIZE_8M         10
#define EBI_SIZE_16M        11
#define EBI_SIZE_32M        12
#define EBI_SIZE_64M        13
#define EBI_SIZE_128M       14
#define EBI_SIZE_256M       15

/* CSxCNTL settings */
#define EBI_ENABLE          0x00000001  /* .. enable this range */
#define EBI_WAIT_STATES     0x0000000e  /* .. mask for wait states */
#define ZEROWT              0x00000000  /* ..  0 WS */
#define ONEWT               0x00000002  /* ..  1 WS */
#define TWOWT               0x00000004  /* ..  2 WS */
#define THREEWT             0x00000006  /* ..  3 WS */
#define FOURWT              0x00000008  /* ..  4 WS */
#define FIVEWT              0x0000000a  /* ..  5 WS */
#define SIXWT               0x0000000c  /* ..  6 WS */
#define SEVENWT             0x0000000e  /* ..  7 WS */
#define EBI_WORD_WIDE       0x00000010  /* .. 16-bit peripheral, else 8 */
#define EBI_POLARITY        0x00000040  /* .. set to invert chip select polarity */
#define EBI_TS_TA_MODE      0x00000080  /* .. use TS/TA mode */
#define EBI_TS_SEL          0x00000100  /* .. drive tsize, not bs_b */
#define EBI_FIFO            0x00000200  /* .. enable fifo */
#define EBI_RE              0x00000400  /* .. Reverse Endian */

/* EBICONFIG settings */
#define EBI_MASTER_ENABLE   0x80000000  /* allow external masters */
#define EBI_EXT_MAST_PRIO   0x40000000  /* maximize ext master priority */
#define EBI_CTRL_ENABLE     0x20000000
#define EBI_TA_ENABLE       0x10000000

#define BRGEN            0x80   /* Control register bit defs */
#define TXEN             0x40
#define RXEN             0x20
#define LOOPBK           0x10
#define TXPARITYEN       0x08
#define TXPARITYEVEN     0x04
#define RXPARITYEN       0x02
#define RXPARITYEVEN     0x01
#define XMITBREAK        0x40
#define BITS5SYM         0x00
#define BITS6SYM         0x10
#define BITS7SYM         0x20
#define BITS8SYM         0x30
#define BAUD115200       0x0a
#define ONESTOP          0x07
#define TWOSTOP          0x0f
#define TX4              0x40
#define RX4              0x04
#define RSTTXFIFOS       0x80
#define RSTRXFIFOS       0x40
#define DELTAIP          0x0001
#define TXUNDERR         0x0002
#define TXOVFERR         0x0004
#define TXFIFOTHOLD      0x0008
#define TXREADLATCH      0x0010
#define TXFIFOEMT        0x0020
#define RXUNDERR         0x0040
#define RXOVFERR         0x0080
#define RXTIMEOUT        0x0100
#define RXFIFOFULL       0x0200
#define RXFIFOTHOLD      0x0400
#define RXFIFONE         0x0800
#define RXFRAMERR        0x1000
#define RXPARERR         0x2000
#define RXBRK            0x4000
          
#define RXIRQS           0x7fc0
#define TXIRQS           0x003e

#define CPU_CLK_EN       0x0001
#define UART_CLK_EN      0x0008

#define BLKEN            06
#define FMSEL            0x08

#define UART0CONTROL     0x01
#define UART0CONFIG      0x02
#define UART0RXTIMEOUT   0x03
#define UART0BAUD        0x04
#define UART0FIFOCFG     0x0a
#define UART0INTMASK     0x10
#define UART0INTSTAT     0x12
#define UART0DATA        0x17

#define GPIOTBUSSEL      0x03
#define GPIODIR          0x06
#define GPIOLED          0x09
#define GPIOIO           0x0a
#define GPIOUARTCTL      0x0c

/*Defines below show which bit enables which UART signals */
#define RI1_EN          0x0001
#define CTS1_EN         0x0002
#define DCD1_EN         0x0004
#define DSR1_EN         0x0008
#define DTR1_EN         0x0010
#define RTS1_EN         0x0020
#define DO1_EN          0x0040
#define DI1_EN          0x0080
#define RI0_EN          0x0100
#define CTS0_EN         0x0200
#define DCD0_EN         0x0400
#define DSR0_EN         0x0800
#define DTR0_EN         0x1000
#define RTS0_EN         0x2000

#if __cplusplus
}
#endif

#endif
