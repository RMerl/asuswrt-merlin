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
#ifndef __6352_INTR_H
#define __6352_INTR_H

#include "6345_common.h"

#ifdef __cplusplus
    extern "C" {
#endif

    /*=====================================================================*/
    /* BCM-6352 Interrupt Level Assignments                                */
    /*=====================================================================*/
#define MIPS_TIMER_INT                  7

    /*=====================================================================*/
    /* BCM-6352 External Interrupt Level Assignments                       */
    /*=====================================================================*/
#define  EXT_IRQ_LEVEL0                 3
#define  EXT_IRQ_LEVEL1                 4
#define  EXT_IRQ_LEVEL2                 5
#define  EXT_IRQ_LEVEL3                 6

    /*=====================================================================*/
    /* Linux ISR Table Offset                                              */
    /*=====================================================================*/
#define EXTERNAL_ISR_TABLE_OFFSET       0x30
#define INTERNAL_ISR_TABLE_OFFSET       0x08
#define DMA_ISR_TABLE_OFFSET            (INTERNAL_ISR_TABLE_OFFSET + 13)

    /*=====================================================================*/
    /* Logical Peripheral Interrupt IDs                                    */
    /*=====================================================================*/

    /* DMA channel interrupt IDs */        
#define INTERRUPT_ID_EMAC_RX_CHAN       (DMA_ISR_TABLE_OFFSET + EMAC_RX_CHAN)
#define INTERRUPT_ID_EMAC_TX_CHAN       (DMA_ISR_TABLE_OFFSET + EMAC_TX_CHAN)
#define INTERRUPT_ID_EBI_RX_CHAN        (DMA_ISR_TABLE_OFFSET + EBI_RX_CHAN)
#define INTERRUPT_ID_EBI_TX_CHAN        (DMA_ISR_TABLE_OFFSET + EBI_TX_CHAN)
#define INTERRUPT_ID_RESERVED_RX_CHAN   (DMA_ISR_TABLE_OFFSET + RESERVED_RX_CHAN)
#define INTERRUPT_ID_RESERVED_TX_CHAN   (DMA_ISR_TABLE_OFFSET + RESERVED_TX_CHAN)
#define INTERRUPT_ID_USB_BULK_RX_CHAN   (DMA_ISR_TABLE_OFFSET + USB_BULK_RX_CHAN)
#define INTERRUPT_ID_USB_BULK_TX_CHAN   (DMA_ISR_TABLE_OFFSET + USB_BULK_TX_CHAN)
#define INTERRUPT_ID_USB_CNTL_RX_CHAN   (DMA_ISR_TABLE_OFFSET + USB_CNTL_RX_CHAN)
#define INTERRUPT_ID_USB_CNTL_TX_CHAN   (DMA_ISR_TABLE_OFFSET + USB_CNTL_TX_CHAN)
#define INTERRUPT_ID_USB_ISO_RX_CHAN    (DMA_ISR_TABLE_OFFSET + USB_ISO_RX_CHAN)
#define INTERRUPT_ID_USB_ISO_TX_CHAN    (DMA_ISR_TABLE_OFFSET + USB_ISO_TX_CHAN)

    /* Internal peripheral interrupt IDs */
#define INTERRUPT_ID_TIMER_IRQ          (INTERNAL_ISR_TABLE_OFFSET +  0)
#define INTERRUPT_ID_UART_IRQ           (INTERNAL_ISR_TABLE_OFFSET +  2)
#define INTERRUPT_ID_ADSL_IRQ           (INTERNAL_ISR_TABLE_OFFSET +  3)
#define INTERRUPT_ID_ATM_IRQ            (INTERNAL_ISR_TABLE_OFFSET +  4)
#define INTERRUPT_ID_USB_IRQ            (INTERNAL_ISR_TABLE_OFFSET +  5)
#define INTERRUPT_ID_EMAC_IRQ           (INTERNAL_ISR_TABLE_OFFSET +  8)
#define INTERRUPT_ID_EPHY_IRQ           (INTERNAL_ISR_TABLE_OFFSET +  12)

    /* External peripheral interrupt IDs */
#define INTERRUPT_ID_EXTERNAL_0         (EXTERNAL_ISR_TABLE_OFFSET +  0)
#define INTERRUPT_ID_EXTERNAL_1         (EXTERNAL_ISR_TABLE_OFFSET +  1)
#define INTERRUPT_ID_EXTERNAL_2         (EXTERNAL_ISR_TABLE_OFFSET +  2)
#define INTERRUPT_ID_EXTERNAL_3         (EXTERNAL_ISR_TABLE_OFFSET +  3)

#define EXT_INT_0                       0
#define EXT_INT_1                       1
#define EXT_INT_2                       2
#define EXT_INT_3                       3

    /* defines */
struct pt_regs;
typedef unsigned int (*FN_ISR) (unsigned int);
typedef void (*FN_HANDLER) (int, void *, struct pt_regs *);

    /* prototypes */
extern void enable_brcm_irq(unsigned int irq);
extern void disable_brcm_irq(unsigned int irq);
extern int request_external_irq(unsigned int irq,
    void (*handler)(int, void *, struct pt_regs *), unsigned long irqflags, 
    const char * devname, void *dev_id);
extern unsigned int BcmHalMapInterrupt(FN_ISR isr, unsigned int param,
    unsigned int interruptId);
extern void dump_intr_regs(void);

    /* compatibility definitions */
#define BcmHalInterruptEnable(irq)      enable_brcm_irq( irq )
#define BcmHalInterruptDisable(irq)     disable_brcm_irq( irq )

#ifdef __cplusplus
    }
#endif                    

#endif  /* __BCM6352_H */
