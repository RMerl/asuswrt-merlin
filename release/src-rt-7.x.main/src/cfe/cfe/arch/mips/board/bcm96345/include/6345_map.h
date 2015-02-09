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
/*   MODULE:  6345_map.h                                               */
/*   PURPOSE: Define addresses of major hardware components of         */
/*            BCM6345                                                  */
/*                                                                     */
/***********************************************************************/
#ifndef __BCM6345_MAP_H
#define __BCM6345_MAP_H

#if __cplusplus
extern "C" {
#endif

#include "bcmtypes.h"
#include "6345_cpu.h"
#include "6345_common.h"
#include "6345_intr.h"

/* macro to convert logical data addresses to physical */
/* DMA hardware must see physical address */
#define LtoP( x )       ( (uint32)x & 0x1fffffff )
#define PtoL( x )       ( LtoP(x) | 0xa0000000 )

/*
** Interrupt Controller
*/
typedef struct IntControl {
  uint32        RevID;          /* (00) */
#define CHIPID          0x634507a0
  uint16        testControl;    /* (04) */
  uint16        blkEnables;     /* (06) */

#define USB_CLK_EN      0x0100
#define EMAC_CLK_EN     0x0080
#define ADSL_CLK_EN     0x0010
#define UART_CLK_EN     0x0008
#define EBI_CLK_EN      0x0004
#define BUS_CLK_EN      0x0002
#define CPU_CLK_EN      0x0001

  uint32        pll_control;    /* (08) */
#define FMSEL_MASK      0xf0000000      // 31:28
#define FMSEL_SHFT      28
#define FM_HI_GEAR      0x08000000      // 27
#define FMCLKSEL        0x04000000      // 26
#define FMDIV_MASK      0x03000000      // 25:24
#define FMDIV_SHFT      24
#define FBDIV_MASK      0x00f00000      // 23:20
#define FBDIV_SHFT      20
#define FB_SEL          0x00010000      // 16
#define FU2SEL_MASK     0x0000f000      // 15:12
#define FU2SEL_SHFT     12
#define FU1SEL_MASK     0x00000f00      // 11:8
#define FU1SEL_SHFT     8
#define FU1PRS_MASK     0x000000e0      // 7:5
#define FU1PRS_SHFT     5
#define FU1POS_MASK     0x00000018      // 4:3
#define FU1POS_SHFT     3
#define SOFT_RESET	0x00000001

  uint32        IrqMask;        /* (0c) */
#define DMACH18         0x80000000
#define DMACH17         0x40000000
#define DMACH16         0x20000000
#define DMACH15         0x10000000
#define DMACH14         0x08000000
#define DMACH13         0x04000000
#define DMACH12         0x02000000
#define DMACH11         0x01000000
#define DMACH10         0x00800000
#define DMACH09         0x00400000
#define DMACH08         0x00200000
#define DMACH07         0x00100000
#define DMACH06         0x00080000
#define DMACH05         0x00040000
#define DMACH04         0x00020000
#define DMACH03         0x00010000
#define DMACH02         0x00008000
#define DMACH01         0x00004000

#define EPHYIRQ         0x00001000
#define EMACIRQ         0x00000100
#define USBIRQ          0x00000020
#define ATMIRQ          0x00000010
#define ADSLIRQ         0x00000008
#define UARTIRQ         0x00000004
#define TIMRIRQ         0x00000001

  uint32        IrqStatus;              /* (10) */
  byte          unused3;                /* (14) */

  /* Upper 4 bits set to 1 to use level, set to 0 to use edge */
  /* Lower 4 bits set disable edge sensitivity */
  byte          extLevelEdgeInsense;

  /* Upper 4 bits enable corresponding ExtIrq, Lower 4 bits clr IRQ */
  byte          extIrqMskandDetClr;

  /* Upper 4 bits are current status of input, lower bits define edge 
   * sensitivity.  If high, IRQ on rising edge. If low, IRQ on falling edge
   */
  byte          extIrqStatEdgeConfig;
} IntControl;

#define INTC ((volatile IntControl * const) INTC_BASE)

/*
** Bus Bridge Registers
*/
typedef struct BusBridge {
  uint16    status;
#define BB_BUSY     0x8000      /* posted operation in progress */
#define BB_RD_PND   0x4000      /* read pending */
#define BB_RD_CMPLT 0x2000      /* read complete */
#define BB_ERROR    0x1000      /* posted write error */
#define BB_TEA      0x0800      /* transfer aborted */
  uint16    abortTimeoutCnt;    /* abort timeout value */

  byte      writePostEnable;
#define BB_POST_TIMR_EN 0x08        /* post writes to timer regs */
#define BB_POST_GPIO_EN 0x04        /* post writes to gpio regs */
#define BB_POST_INTC_EN 0x02        /* post writes to interrupt controller regs */
#define BB_POST_UART_EN 0x01        /* post writes to uart regs */
  byte      unused1[5];
  uint16    postAddr;       /* posted read address (lower half) */
  byte      unused2[3];
  byte      postData;       /* posted read data */
} BusBridge;

/* register offsets (needed for EBI master access) */
#define BB_STATUS       0
#define BB_ABORT_TO_CNT     2
#define BB_WR_POST_EN       4
#define BB_RD_POST_ADDR     10
#define BB_RD_POST_DATA     12

#define BRIDGE *bridge ((volatile BusBridge * const) BB_BASE)

/*
** Timer
*/
typedef struct Timer {
  uint16        unused0;
  byte          TimerMask;
#define TIMER0EN        0x01
#define TIMER1EN        0x02
#define TIMER2EN        0x04
  byte          TimerInts;
#define TIMER0          0x01
#define TIMER1          0x02
#define TIMER2          0x04
#define WATCHDOG        0x08
  uint32        TimerCtl0;
  uint32        TimerCtl1;
  uint32        TimerCtl2;
#define TIMERENABLE     0x80000000
#define RSTCNTCLR       0x40000000      
  uint32        TimerCnt0;
  uint32        TimerCnt1;
  uint32        TimerCnt2;
  uint32        WatchDogDefCount;

  /* Write 0xff00 0x00ff to Start timer
   * Write 0xee00 0x00ee to Stop and re-load default count
   * Read from this register returns current watch dog count
   */
  uint32        WatchDogCtl;

  /* Number of 40-MHz ticks for WD Reset pulse to last */
  uint32        WDResetCount;
} Timer;

#define TIMER ((volatile Timer * const) TIMR_BASE)

/*
** UART
*/
typedef struct UartChannel {
  byte          unused0;
  byte          control;
#define BRGEN           0x80    /* Control register bit defs */
#define TXEN            0x40
#define RXEN            0x20
#define LOOPBK          0x10
#define TXPARITYEN      0x08
#define TXPARITYEVEN    0x04
#define RXPARITYEN      0x02
#define RXPARITYEVEN    0x01

  byte          config;
#define XMITBREAK       0x40
#define BITS5SYM        0x00
#define BITS6SYM        0x10
#define BITS7SYM        0x20
#define BITS8SYM        0x30
#define ONESTOP         0x07
#define TWOSTOP         0x0f
  /* 4-LSBS represent STOP bits/char
   * in 1/8 bit-time intervals.  Zero
   * represents 1/8 stop bit interval.
   * Fifteen represents 2 stop bits.
   */
  byte          fifoctl;
#define RSTTXFIFOS      0x80
#define RSTRXFIFOS      0x40
  /* 5-bit TimeoutCnt is in low bits of this register.
   *  This count represents the number of characters 
   *  idle times before setting receive Irq when below threshold
   */
  uint32        baudword;
  /* When divide SysClk/2/(1+baudword) we should get 32*bit-rate
   */

  byte          txf_levl;       /* Read-only fifo depth */
  byte          rxf_levl;       /* Read-only fifo depth */
  byte          fifocfg;        /* Upper 4-bits are TxThresh, Lower are
                                 *      RxThreshold.  Irq can be asserted
                                 *      when rx fifo> thresh, txfifo<thresh
                                 */
  byte          prog_out;       /* Set value of DTR (Bit0), RTS (Bit1)
                                 *  if these bits are also enabled to GPIO_o
                                 */
#define	DTREN	0x01
#define	RTSEN	0x02

  byte          unused1;
  byte          DeltaIPEdgeNoSense;     /* Low 4-bits, set corr bit to 1 to 
                                         * detect irq on rising AND falling 
                                         * edges for corresponding GPIO_i
                                         * if enabled (edge insensitive)
                                         */
  byte          DeltaIPConfig_Mask;     /* Upper 4 bits: 1 for posedge sense
                                         *      0 for negedge sense if
                                         *      not configured for edge
                                         *      insensitive (see above)
                                         * Lower 4 bits: Mask to enable change
                                         *  detection IRQ for corresponding
                                         *  GPIO_i
                                         */
  byte          DeltaIP_SyncIP;         /* Upper 4 bits show which bits
                                         *  have changed (may set IRQ).
                                         *  read automatically clears bit
                                         * Lower 4 bits are actual status
                                         */

  uint16        intMask;				/* Same Bit defs for Mask and status */
  uint16        intStatus;
#define DELTAIP         0x0001
#define TXUNDERR        0x0002
#define TXOVFERR        0x0004
#define TXFIFOTHOLD     0x0008
#define TXREADLATCH     0x0010
#define TXFIFOEMT       0x0020
#define RXUNDERR        0x0040
#define RXOVFERR        0x0080
#define RXTIMEOUT       0x0100
#define RXFIFOFULL      0x0200
#define RXFIFOTHOLD     0x0400
#define RXFIFONE        0x0800
#define RXFRAMERR       0x1000
#define RXPARERR        0x2000
#define RXBRK           0x4000

  uint16        unused2;
  uint16        Data;                   /* Write to TX, Read from RX */
                                        /* bits 11:8 are BRK,PAR,FRM errors */

  uint32		unused3;
  uint32		unused4;
} Uart;

#define UART ((volatile Uart * const) UART_BASE)

/*
** Gpio Controller
*/
typedef struct GpioControl {
  uint16        unused0;
  byte          unused1;
  byte          TBusSel;

  /* High in bit location enables output */
  uint16        unused2;
  uint16        GPIODir;
  byte          unused3;
  byte          Leds;           //Only bits [3:0]
  uint16        GPIOio;

  /* Defines below show which bit enables which UART signals */
  uint32        UartCtl;
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

  /*********************************************************************
   * Multiple Use Muxed GPIO
   * -----------------------
   *
   * ------
   * GPIO_A
   * ------
   *
   * GPIO[0] -> RI1             I       Controlled by UartCtl[0] ELSE 0
   * GPIO[0] -> DMATC_i         I       Always
   * GPIO[0] -> DMATC_o         O       Controlled by dma_enable_n|dma_drive_n
   * GPIO[0] -> ebi_bsize[0]    I       Always
   *
   * GPIO[1] -> CTS1            I       Controlled by UartCtl[1] ELSE 0
   * GPIO[1] -> DMAACK1 (18)    O       Controlled by dma_enable_n
   * GPIO[1] -> ebi_bg_b        O       Controlled by ebi_master_n
   *
   * GPIO[2] -> DCD1            I       Controlled by UartCtl[2] ELSE 0
   * GPIO[2] -> ebi_bsize[1]    I       Always
   *
   * GPIO[3] -> DSR1            I       Controlled by UartCtl[3] ELSE 0
   * GPIO[3] -> INT2            I       Always
   * GPIO[3] -> ebi_bsize[2]    I       Always
   *
   * GPIO[4] -> DTR1            O       Controlled by UartCtl[4]&GpioDir[4]
   * GPIO[4] -> INT3            I       Always
   * GPIO[4] -> ebi_burst       I       Always
   *
   * GPIO[5] -> RTS1            O       Controlled by UartCtl[5]&GpioDir[5]
   * GPIO[5] -> DMAACK0 (17)    O       Controlled by dma_enable_n
   * GPIO[5] -> ebi_tsize       I       Always
   *
   * GPIO[6] -> sDout1          O       Controlled by UartCtl[6]&GpioDir[6]
   * GPIO[6] -> DMARQ1 (18)     I       Always
   * GPIO[6] -> ebi_bb_i        I       Always
   * GPIO[6] -> ebi_bb_o        O       Controlled by ebi_master_n|ebi_bb_oen
   *
   * GPIO[7] -> sDin1           I       Controlled by UartCtl[7] ELSE 0
   * GPIO[7] -> ebi_br_b        I       Always
   * GPIO[7] -> DMARQ0 (17)     I       Always
   *
   * ------
   * GPIO_B
   * ------
   *
   * GPIO[8] -> RI0             I       Controlled by UartCtl[8] ELSE 0
   * GPIO[8] -> ebi_cs_b[6]     O       Controlled by ebi_cs_en[6]
   *
   * GPIO[9] -> CTS0            I       Controlled by UartCtl[9] ELSE 0
   *
   * GPIO[a] -> DCD0            I       Controlled by UartCtl[a] ELSE 0
   * GPIO[a] -> ebi_cs_b[7]     O       Controlled by ebi_cs_en[7]
   *
   * GPIO[b] -> DSR0            I       Controlled by UartCtl[b] ELSE 0
   * GPIO[b] -> ebi_int_cs_b    I       Always
   *
   * GPIO[c] -> DTR0            O       Controlled by UartCtl[c]&GpioDir[c]
   *
   * GPIO[d] -> RTS0            O       Controlled by UartCtl[d]&&GpioDir[d]
   *
   * GPIO[e] -> INT0            I       Always
   *
   * GPIO[f] -> INT1            I       Always
   *
   * sDout0 -> (bist_en[15]) ? pll_clk48:sDout0_int
   *
   *********************************************************************/

} GpioControl;

#define GPIO ((volatile GpioControl * const) GPIO_BASE)

typedef enum {
    LED_1A = 1,
    LED_2B,
    LED_3C,
    LED_4D
}LEDDEFS;

#define LEDON                   1
#define LEDOFF                  0

/*
** DMA Channel (1 .. 20)
*/
typedef struct DmaChannel {
  uint32        cfg;                    /* (00) assorted configuration */
#define          DMA_FLOWC_EN   0x00000010      /* flow control enable */
#define          DMA_WRAP_EN    0x00000008      /* use DMA_WRAP bit */
#define          DMA_CHAINING   0x00000004      /* chaining mode */
#define          DMA_STALL      0x00000002      
#define          DMA_ENABLE     0x00000001      /* set to enable channel */
  uint32        maxBurst;               /* (04) max burst length permitted */
                                        /*      non-chaining / chaining */
  uint32        startAddr;              /* (08) source addr  / ring start address */
  uint32        length;                 /* (0c) xfer len     / ring len */
#define          DMA_KICKOFF    0x80000000      /* start non-chaining xfer */

  uint32        bufStat;                /* (10) buffer status for non-chaining */
  uint32        intStat;                /* (14) interrupts control and status */
  uint32        intMask;                /* (18) interrupts mask */
#define         DMA_BUFF_DONE   0x00000001      /* buffer done */
#define         DMA_DONE        0x00000002      /* packet xfer complete */
#define         DMA_NO_DESC     0x00000004      /* no valid descriptors */

// DMA HW bits are clugy in this version of chip (mask/status shifted)
#define         DMA_BUFF_DONE_MASK  0x00000004      /* buffer done */
#define         DMA_DONE_MASK       0x00000001      /* packet xfer complete */
#define         DMA_NO_DESC_MASK    0x00000002      /* no valid descriptors */

  uint32        fcThreshold;            /* (1c) flow control threshold */
  uint32        numAlloc;               /* */
  uint32        unused[7];              /* (20-3c) pad to next descriptor */
} DmaChannel;
/* register offsets, useful for ebi master access */
#define DMA_CFG                 0
#define DMA_MAX_BURST           4
#define DMA_START_ADDR          8
#define DMA_LENGTH              12
#define DMA_BUF_STAT            16
#define DMA_INT_STAT            20
#define DMA_FC_THRESHOLD        24
#define DMA_NUM_ALLOC           28


/* paste in your program ...
DmaChannel *dmaChannels  = (DmaChannel *)DMA_BASE;
DmaChannel *dma1 = dmaChannels[1];
*/


/*
** DMA Buffer 
*/
typedef struct DmaDesc {
  uint16        length;                 /* in bytes of data in buffer */
  uint16        status;                 /* buffer status */
#define          DMA_OWN        0x8000  /* cleared by DMA, set by SW */
#define          DMA_EOP        0x0800  /* last buffer in packet */
#define          DMA_SOP        0x0400  /* first buffer in packet */
#define          DMA_WRAP       0x0200  /* */
#define          DMA_APPEND_CRC 0x0100  /* .. for emac tx */
#define          DATA_FLAG      0x0100  /* .. for secmod rx */
#define          AUTH_FAIL_FLAG 0x0100  /* .. for secmod tx */

/* EMAC Descriptor Status definitions */
#define          EMAC_UNDERRUN  0x4000   /* Tx underrun, dg-mod ???) */
#define          EMAC_MISS      0x0080  /* framed address recognition failed (promiscuous) */
#define          EMAC_BRDCAST   0x0040  /* DA is Broadcast */
#define          EMAC_MULT      0x0020  /* DA is multicast */
#define          EMAC_LG        0x0010  /* frame length > RX_LENGTH register value */
#define          EMAC_NO        0x0008  /* Non-Octet aligned */
#define          EMAC_RXER      0x0004  /* RX_ERR on MII while RX_DV assereted */
#define          EMAC_CRC_ERROR 0x0002  /* CRC error */
#define          EMAC_OV        0x0001  /* Overflow */

/* HDLC Descriptor Status definitions */
#define          DMA_HDLC_TX_ABORT      0x0100
#define          DMA_HDLC_RX_OVERRUN    0x4000
#define          DMA_HDLC_RX_TOO_LONG   0x2000
#define          DMA_HDLC_RX_CRC_OK     0x1000
#define          DMA_HDLC_RX_ABORT      0x0100

  uint32        address;                        /* address of data */
} DmaDesc;

/*
** Sdram Controller
*/
typedef struct SdramControllerRegs {
  uint16        unused1;
  uint16        initControl;    /* 02 */
#define SD_POWER_DOWN           0x200   /* put sdram into power down */
#define SD_SELF_REFRESH         0x100   /* enable self refresh mode */
#define SD_SOFT_RESET           0x080   /* soft reset all sdram controller regs */
#define SD_EDO_SELECT           0x040   /* select EDO mode */
#define SD_EDO_WAIT_STATE       0x020   /* add an EDO wait state */
#define SD_8MEG                 0x010   /* map sdram to 8 megs */
#define SD_MASTER_ENABLE        0x008   /* enable accesses to external sdram */
#define SD_MRS                  0x004   /* generate a mode register select cycle */
#define SD_PRECHARGE            0x002   /* generate a precharge cycle */
#define SD_CBR                  0x001   /* generate a refresh cycle */
  uint8         unused2[3];
  uint8         config;         /* 07 */
#define SD_FAST_MEM             0x04    /* 1=CAS latency of 2, 0 = CAS latency of 3 */
#define SD_BURST_LEN            0x03    /* set burst length */
#define SD_BURST_FULL_PAGE      0x00    /* .. full page */
#define SD_BURST_8              0x01    /* .. 8 words */
#define SD_BURST_4              0x02    /* .. 4 words */
#define SD_BURST_2              0x03    /* .. 2 words */
  uint16        unused3;
  uint16        refreshControl; /* 0a */
#define SD_REFRESH_ENABLE       0x8000  /* refresh enable */
#define SD_REFRESH_PERIOD       0x00ff  /* refresh period (16 x n x clock_period) */

  uint32        memoryBase;     /* 0c */
#define SD_MEMBASE_MASK         0xffffe000      /* base address mask */
#define SD_MEMSIZE_8MEG         0x00000001      /* memory is 8 meg */
#define SD_MEMSIZE_2MEG         0x00000001      /* memory is 2 meg */

} SdramControllerRegs;

/*
** External Bus Interface
*/
typedef struct EbiChipSelect {
  uint32        base;                   /* base address in upper 24 bits */
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
  uint32        config;
#define EBI_ENABLE          0x00000001      /* .. enable this range */
#define EBI_WAIT_STATES     0x0000000e      /* .. mask for wait states */
#define EBI_WTST_SHIFT      1               /* .. for shifting wait states */
#define EBI_WORD_WIDE       0x00000010      /* .. 16-bit peripheral, else 8 */
#define EBI_WREN            0x00000020      /* enable posted writes */
#define EBI_POLARITY        0x00000040      /* .. set to invert something, 
                                        **    don't know what yet */
#define EBI_TS_TA_MODE      0x00000080      /* .. use TS/TA mode */
#define EBI_TS_SEL          0x00000100      /* .. drive tsize, not bs_b */
#define EBI_FIFO            0x00000200      /* .. use fifo */
#define EBI_RE              0x00000400      /* .. Reverse Endian */
} EbiChipSelect;

typedef struct EbiRegisters {
  EbiChipSelect cs[6];                  /* size chip select configuration */
  uint32        reserved[4];
  uint32        ebi_config;             /* configuration */
#define EBI_MASTER_ENABLE       0x80000000  /* allow external masters */
#define EBI_EXT_MAST_PRIO       0x40000000  /* maximize ext master priority */
#define EBI_CTRL_ENABLE         0x20000000
#define EBI_TA_ENABLE           0x10000000
  uint32        dma_control;
#define EBI_TX_INV_IRQ_EN       0x00080000
#define EBI_RX_INV_IRQ_EN       0x00040000
#define EBI_TX_PKT_DN_IRQ_EN    0x00020000
#define EBI_RX_PKT_DN_IRQ_EN    0x00010000
#define EBI_TX_INV_CLR          0x00001000
#define EBI_RX_INV_CLR          0x00000800
#define EBI_CHAINING            0x00000400
#define EBI_EXT_MODE            0x00000200
#define EBI_HALF_WORD           0x00000100
#define EBI_TX_PKT_DN_CLR       0x00000080
#define EBI_RX_PKT_DN_CLR       0x00000040
#define EBI_TX_BUF_DN_CLR       0x00000020
#define EBI_RX_BUF_DN_CLR       0x00000010
#define EBI_TX_BUF_DN_IRQ_EN    0x00000008
#define EBI_RX_BUF_DN_IRQ_EN    0x00000004
#define EBI_TX_EN               0x00000002
#define EBI_RX_EN               0x00000001
  uint32        dma_rx_start_addr;
  uint32        dma_rx_buf_size;
  uint32        dma_tx_start_addr;
  uint32        dma_tx_buf_size;
  uint32        dma_status;
#define EBI_TX_INV_DESC         0x00000020
#define EBI_RX_INV_DESC         0x00000010
#define EBI_TX_PKT_DN           0x00000008
#define EBI_RX_PKT_DN           0x00000004
#define EBI_TX_BUF_DN           0x00000002
#define EBI_RX_BUF_DN           0x00000001
} EbiRegisters;

#define EBIC ((volatile EbiRegisters * const) EBIC_BASE)

/*
** EMAC transmit MIB counters
*/
typedef struct EmacTxMib {
  uint32        tx_good_octets;         /* (200) good byte count */
  uint32        tx_good_pkts;           /* (204) good pkt count */
  uint32        tx_octets;              /* (208) good and bad byte count */
  uint32        tx_pkts;                /* (20c) good and bad pkt count */
  uint32        tx_broadcasts_pkts;     /* (210) good broadcast packets */
  uint32        tx_multicasts_pkts;     /* (214) good mulitcast packets */
  uint32        tx_len_64;              /* (218) RMON tx pkt size buckets */
  uint32        tx_len_65_to_127;       /* (21c) */
  uint32        tx_len_128_to_255;      /* (220) */
  uint32        tx_len_256_to_511;      /* (224) */
  uint32        tx_len_512_to_1023;     /* (228) */
  uint32        tx_len_1024_to_max;     /* (22c) */
  uint32        tx_jabber_pkts;         /* (230) > 1518 with bad crc */
  uint32        tx_oversize_pkts;       /* (234) > 1518 with good crc */
  uint32        tx_fragment_pkts;       /* (238) < 63   with bad crc */
  uint32        tx_underruns;           /* (23c) fifo underrun */
  uint32        tx_total_cols;          /* (240) total collisions in all tx pkts */
  uint32        tx_single_cols;         /* (244) tx pkts with single collisions */
  uint32        tx_multiple_cols;       /* (248) tx pkts with multiple collisions */
  uint32        tx_excessive_cols;      /* (24c) tx pkts with excessive cols */
  uint32        tx_late_cols;           /* (250) tx pkts with late cols */
  uint32        tx_defered;             /* (254) tx pkts deferred */
  uint32        tx_carrier_lost;        /* (258) tx pkts with CRS lost */
  uint32        tx_pause_pkts;          /* (25c) tx pause pkts sent */
#define NumEmacTxMibVars        24
} EmacTxMib;

/*
** EMAC receive MIB counters
*/
typedef struct EmacRxMib {
  uint32        rx_good_octets;         /* (280) good byte count */
  uint32        rx_good_pkts;           /* (284) good pkt count */
  uint32        rx_octets;              /* (288) good and bad byte count */
  uint32        rx_pkts;                /* (28c) good and bad pkt count */
  uint32        rx_broadcasts_pkts;     /* (290) good broadcast packets */
  uint32        rx_multicasts_pkts;     /* (294) good mulitcast packets */
  uint32        rx_len_64;              /* (298) RMON rx pkt size buckets */
  uint32        rx_len_65_to_127;       /* (29c) */
  uint32        rx_len_128_to_255;      /* (2a0) */
  uint32        rx_len_256_to_511;      /* (2a4) */
  uint32        rx_len_512_to_1023;     /* (2a8) */
  uint32        rx_len_1024_to_max;     /* (2ac) */
  uint32        rx_jabber_pkts;         /* (2b0) > 1518 with bad crc */
  uint32        rx_oversize_pkts;       /* (2b4) > 1518 with good crc */
  uint32        rx_fragment_pkts;       /* (2b8) < 63   with bad crc */
  uint32        rx_missed_pkts;         /* (2bc) missed packets */
  uint32        rx_crc_align_errs;      /* (2c0) both or either */
  uint32        rx_undersize;           /* (2c4) < 63   with good crc */
  uint32        rx_crc_errs;            /* (2c8) crc errors (only) */
  uint32        rx_align_errs;          /* (2cc) alignment errors (only) */
  uint32        rx_symbol_errs;         /* (2d0) pkts with RXERR assertions (symbol errs) */
  uint32        rx_pause_pkts;          /* (2d4) MAC control, PAUSE */
  uint32        rx_nonpause_pkts;       /* (2d8) MAC control, not PAUSE */
#define NumEmacRxMibVars        23
} EmacRxMib;

typedef struct EmacRegisters {
  uint32        rxControl;              /* (00) receive control */
#define          EMAC_PM_REJ    0x80    /*      - reject DA match in PMx regs */
#define          EMAC_UNIFLOW   0x40    /*      - accept cam match fc */
#define          EMAC_FC_EN     0x20    /*      - enable flow control */
#define          EMAC_LOOPBACK  0x10    /*      - loopback */
#define          EMAC_PROM      0x08    /*      - promiscuous */
#define          EMAC_RDT       0x04    /*      - ignore transmissions */
#define          EMAC_ALL_MCAST 0x02    /*      - ignore transmissions */
#define          EMAC_NO_BCAST  0x01    /*      - ignore transmissions */


  uint32        rxMaxLength;            /* (04) receive max length */
  uint32        txMaxLength;            /* (08) transmit max length */
  uint32        unused1[1];
  uint32        mdioFreq;               /* (10) mdio frequency */
#define          EMAC_MII_PRE_EN 0x0100 /* prepend preamble sequence */
#define          EMAC_MDIO_PRE   0x100  /*      - enable MDIO preamble */
#define          EMAC_MDC_FREQ   0x0ff  /*      - mdio frequency */

  uint32        mdioData;               /* (14) mdio data */
#define          MDIO_WR        0x50020000 /*   - write framing */
#define          MDIO_RD        0x60020000 /*   - read framing */
#define          MDIO_PMD_SHIFT  23
#define          MDIO_REG_SHIFT  18

  uint32        intMask;                /* (18) int mask */
  uint32        intStatus;              /* (1c) int status */
#define          EMAC_FLOW_INT  0x04    /*      - flow control event */
#define          EMAC_MIB_INT   0x02    /*      - mib event */
#define          EMAC_MDIO_INT  0x01    /*      - mdio event */

  uint32        unused2[3];
  uint32        config;                 /* (2c) config */
#define          EMAC_ENABLE    0x001   /*      - enable emac */
#define          EMAC_DISABLE   0x002   /*      - disable emac */
#define          EMAC_SOFT_RST  0x004   /*      - soft reset */
#define          EMAC_SOFT_RESET 0x004  /*      - emac soft reset */
#define          EMAC_EXT_PHY   0x008   /*      - external PHY select */

  uint32        txControl;              /* (30) transmit control */
#define          EMAC_FD        0x001   /*      - full duplex */
#define          EMAC_FLOWMODE  0x002   /*      - flow mode */
#define          EMAC_NOBKOFF   0x004   /*      - no backoff in  */
#define          EMAC_SMALLSLT  0x008   /*      - small slot time */

  uint32        txThreshold;            /* (34) transmit threshold */
  uint32        mibControl;             /* (38) mib control */
#define          EMAC_NO_CLEAR  0x001   /* don't clear on read */

  uint32        unused3[7];

  uint32        pm0DataLo;              /* (58) perfect match 0 data lo */
  uint32        pm0DataHi;              /* (5C) perfect match 0 data hi (15:0) */
  uint32        pm1DataLo;              /* (60) perfect match 1 data lo */
  uint32        pm1DataHi;              /* (64) perfect match 1 data hi (15:0) */
  uint32        pm2DataLo;              /* (68) perfect match 2 data lo */
  uint32        pm2DataHi;              /* (6C) perfect match 2 data hi (15:0) */
  uint32        pm3DataLo;              /* (70) perfect match 3 data lo */
  uint32        pm3DataHi;              /* (74) perfect match 3 data hi (15:0) */
#define          EMAC_CAM_V   0x10000  /*      - cam index */
#define          EMAC_CAM_VALID 0x00010000

  uint32        unused4[98];            /* (78-1fc) */

  EmacTxMib     tx_mib;                 /* (200) emac tx mib */
  uint32        unused5[8];             /* (260-27c) */

  EmacRxMib     rx_mib;                 /* (280) rx mib */

} EmacRegisters;

/* register offsets for subrouting access */
#define EMAC_RX_CONTROL         0x00
#define EMAC_RX_MAX_LENGTH      0x04
#define EMAC_TX_MAC_LENGTH      0x08
#define EMAC_MDIO_FREQ          0x10
#define EMAC_MDIO_DATA          0x14
#define EMAC_INT_MASK           0x18
#define EMAC_INT_STATUS         0x1C
#define EMAC_CAM_DATA_LO        0x20
#define EMAC_CAM_DATA_HI        0x24
#define EMAC_CAM_CONTROL        0x28
#define EMAC_CONTROL            0x2C
#define EMAC_TX_CONTROL         0x30
#define EMAC_TX_THRESHOLD       0x34
#define EMAC_MIB_CONTROL        0x38


#define EMAC ((volatile EmacRegisters * const) EMAC_BASE)

/*
** USB Registers
*/
typedef struct UsbRegisters {
    byte inttf_setting;
    byte current_config;
    uint16 status_frameNum;
#define USB_BUS_RESET   0x1000 
#define USB_SUSPENDED   0x0800 
    byte unused1;
    byte endpt_prnt;
    byte endpt_dirn;
    byte endpt_status;
#define USB_ENDPOINT_0  0x01
#define USB_ENDPOINT_1  0x02
#define USB_ENDPOINT_2  0x04
#define USB_ENDPOINT_3  0x08
#define USB_ENDPOINT_4  0x10
#define USB_ENDPOINT_5  0x20
#define USB_ENDPOINT_6  0x40
#define USB_ENDPOINT_7  0x80
    uint32 unused2;
    byte conf_mem_ctl;
#define USB_CONF_MEM_RD     0x80
#define USB_CONF_MEM_RDY    0x40
    byte unused2a;
    byte conf_mem_read_address;
    byte conf_mem_write_address;

    byte unused3;
    byte dev_req_bytesel;
    uint16 ext_dev_data;

    byte unused4;
    byte clr_fifo;
    byte endpt_stall_reset;  // use same endpoint #'s from above
    byte usb_cntl;
#define USB_FORCE_ERR       0x20
#define USB_SOFT_RESET      0x10
#define USB_RESUME          0x08
#define USB_COMMAND_ERR     0x04
#define USB_COMMAND_OVER    0x02
    byte irq_addr;
    byte iso_out_in_addr;
    byte blk_out_in_addr;
    byte cntl_addr;
    uint32 mux_cntl;
#define USB_TX_DMA_OPER          0x00000000   
#define USB_TX_CNTL_FIFO_OPER    0x00000004   
#define USB_TX_BULK_FIFO_OPER    0x00000008   
#define USB_TX_ISO_FIFO_OPER     0x0000000c   
#define USB_TX_IRQ_OPER          0x00000010   
#define USB_RX_DMA_OPER          0x00000000   
#define USB_RX_CNTL_FIFO_OPER    0x00000001   
#define USB_RX_BULK_FIFO_OPER    0x00000002   
#define USB_RX_ISO_FIFO_OPER     0x00000003   
    uint32 rx_ram_read_port;
    uint32 tx_ram_write_port;
    uint32 fifo_status;
#define USB_CTRLI_FIFO_FULL         0x00000001
#define USB_CTRLI_FIFO_EMPTY        0x00000002
#define USB_CTRLO_FIFO_FULL         0x00000100
#define USB_CTRLO_FIFO_EMPTY        0x00000200
    uint32 irq_status;
    uint32 irq_mask;
#define USB_NEW_CONFIG              0x01   
#define USB_SETUP_COMMAND_RECV      0x02    // non-standard setup command received
#define USB_OUT_FIFO_OV             0x04   
#define USB_RESET_RECV              0x08   
#define USB_SUSPEND_RECV            0x10   
#define USB_FIFO_REWIND             0x20   
#define USB_RX_BULK_FIFO_DATA_AVAIL 0x40   
#define USB_RX_ISO_FIFO_DATA_AVAIL  0x80   
    uint32 endpt_cntl;
#define USB_TX_EOP                  0x0200   
#define USB_R_WK_EN                 0x0100   
    uint32 rx_status_read_port;
    uint32 confmem_read_port;
    uint32 confmem_write_port;
    uint32 fifo_ovf_count;
    uint32 fifo_rewind_cnt;
} UsbRegisters;

#define USB ((volatile UsbRegisters * const) USB_BASE)

/*
** ADSL core Registers
*/

#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)

typedef struct AdslRegisters {
    uint32 core_control;
#define ADSL_RESET		0x01
   
    uint32 core_status;
#define ADSL_HOST_MSG	0x01

    uint32 PAD;
    uint32 bist_status;
    uint32 PAD[4];
    uint32 int_status_i; /* 0x20 */
    uint32 int_mask_i;
    uint32 int_status_f;
    uint32 int_mask_f;
#define ADSL_INT_HOST_MSG		0x00000020
#define ADSL_INT_DESC_ERR		0x00000400
#define ADSL_INT_DATA_ERR		0x00000800
#define ADSL_INT_DESC_PROTO_ERR	0x00001000
#define ADSL_INT_RCV_DESC_UF	0x00002000
#define ADSL_INT_RCV_FIFO_OF	0x00004000
#define ADSL_INT_XMT_FIFO_UF	0x00008000
#define ADSL_INT_RCV			0x00010000
#define ADSL_INT_XMT			0x01000000

    uint32 PAD[116];

	uint32	xmtcontrol_intr; /* 0x200 */
#define ADSL_DMA_XMT_EN			0x00000001
#define ADSL_DMA_XMT_LE			0x00000004
	uint32	xmtaddr_intr;
#define ADSL_DMA_ADDR_MASK		0xFFFFF000
	uint32	xmtptr_intr;
#define ADSL_DMA_LAST_DESC_MASK	0x00000FFF
	uint32	xmtstatus_intr;
#define ADSL_DMA_CURR_DESC_MASK	0x00000FFF
#define ADSL_DMA_XMT_STATE_MASK	0x0000F000
#define ADSL_DMA_XMT_STATE_DIS	0x00000000
#define ADSL_DMA_XMT_STATE_ACT	0x00001000
#define ADSL_DMA_XMT_STATE_IDLE	0x00002000
#define ADSL_DMA_XMT_STATE_STOP	0x00003000

#define ADSL_DMA_XMT_ERR_MASK	0x000F0000
#define ADSL_DMA_XMT_ERR_NONE	0x00000000
#define ADSL_DMA_XMT_ERR_DPE	0x00010000
#define ADSL_DMA_XMT_ERR_FIFO	0x00020000
#define ADSL_DMA_XMT_ERR_DTE	0x00030000
#define ADSL_DMA_XMT_ERR_DRE	0x00040000

	uint32	rcvcontrol_intr;
#define ADSL_DMA_RCV_EN			0x00000001
#define ADSL_DMA_RCV_FO			0x000000FE
	uint32	rcvaddr_intr;
	uint32	rcvptr_intr;
	uint32	rcvstatus_intr;
#define ADSL_DMA_RCV_STATE_MASK	0x0000F000
#define ADSL_DMA_RCV_STATE_DIS	0x00000000
#define ADSL_DMA_RCV_STATE_ACT	0x00001000
#define ADSL_DMA_RCV_STATE_IDLE	0x00002000
#define ADSL_DMA_RCV_STATE_STOP	0x00003000

#define ADSL_DMA_RCV_ERR_MASK	0x000F0000
#define ADSL_DMA_RCV_ERR_NONE	0x00000000
#define ADSL_DMA_RCV_ERR_DPE	0x00010000
#define ADSL_DMA_RCV_ERR_FIFO	0x00020000
#define ADSL_DMA_RCV_ERR_DTE	0x00030000
#define ADSL_DMA_RCV_ERR_DRE	0x00040000

	uint32	xmtcontrol_fast;
	uint32	xmtaddr_fast;
	uint32	xmtptr_fast;
	uint32	xmtstatus_fast;
	uint32	rcvcontrol_fast;
	uint32	rcvaddr_fast;
	uint32	rcvptr_fast;
	uint32	rcvstatus_fast;
	uint32	PAD[48];

	uint32	host_message; /* 0x300 */
	uint32	PAD[805];
    uint32 core_reset;
    uint32 core_error;
    uint32 core_revision;

#define	ADSL_CORE_REV			0x0000000F
#define	ADSL_CORE_TYPE			0x0000FFF0
} AdslRegisters;

#define ADSL ((volatile AdslRegisters * const) ADSL_BASE)

#if __cplusplus
}
#endif

#endif
