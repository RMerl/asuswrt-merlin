/*
 * SNI specific definitions
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1997, 1998 by Ralf Baechle
 * Copyright (C) 2006 Thomas Bogendoerfer (tsbogend@alpha.franken.de)
 */
#ifndef __ASM_SNI_H
#define __ASM_SNI_H

extern unsigned int sni_brd_type;

#define SNI_BRD_10                 2
#define SNI_BRD_10NEW              3
#define SNI_BRD_TOWER_OASIC        4
#define SNI_BRD_MINITOWER          5
#define SNI_BRD_PCI_TOWER          6
#define SNI_BRD_RM200              7
#define SNI_BRD_PCI_MTOWER         8
#define SNI_BRD_PCI_DESKTOP        9
#define SNI_BRD_PCI_TOWER_CPLUS   10
#define SNI_BRD_PCI_MTOWER_CPLUS  11

/* RM400 cpu types */
#define SNI_CPU_M8021           0x01
#define SNI_CPU_M8030           0x04
#define SNI_CPU_M8031           0x06
#define SNI_CPU_M8034           0x0f
#define SNI_CPU_M8037           0x07
#define SNI_CPU_M8040           0x05
#define SNI_CPU_M8043           0x09
#define SNI_CPU_M8050           0x0b
#define SNI_CPU_M8053           0x0d

#define SNI_PORT_BASE		0xb4000000

#ifndef __MIPSEL__
/*
 * ASIC PCI registers for big endian configuration.
 */
#define PCIMT_UCONF		0xbfff0004
#define PCIMT_IOADTIMEOUT2	0xbfff000c
#define PCIMT_IOMEMCONF		0xbfff0014
#define PCIMT_IOMMU		0xbfff001c
#define PCIMT_IOADTIMEOUT1	0xbfff0024
#define PCIMT_DMAACCESS		0xbfff002c
#define PCIMT_DMAHIT		0xbfff0034
#define PCIMT_ERRSTATUS		0xbfff003c
#define PCIMT_ERRADDR		0xbfff0044
#define PCIMT_SYNDROME		0xbfff004c
#define PCIMT_ITPEND		0xbfff0054
#define  IT_INT2		0x01
#define  IT_INTD		0x02
#define  IT_INTC		0x04
#define  IT_INTB		0x08
#define  IT_INTA		0x10
#define  IT_EISA		0x20
#define  IT_SCSI		0x40
#define  IT_ETH			0x80
#define PCIMT_IRQSEL		0xbfff005c
#define PCIMT_TESTMEM		0xbfff0064
#define PCIMT_ECCREG		0xbfff006c
#define PCIMT_CONFIG_ADDRESS	0xbfff0074
#define PCIMT_ASIC_ID		0xbfff007c	/* read */
#define PCIMT_SOFT_RESET	0xbfff007c	/* write */
#define PCIMT_PIA_OE		0xbfff0084
#define PCIMT_PIA_DATAOUT	0xbfff008c
#define PCIMT_PIA_DATAIN	0xbfff0094
#define PCIMT_CACHECONF		0xbfff009c
#define PCIMT_INVSPACE		0xbfff00a4
#else
/*
 * ASIC PCI registers for little endian configuration.
 */
#define PCIMT_UCONF		0xbfff0000
#define PCIMT_IOADTIMEOUT2	0xbfff0008
#define PCIMT_IOMEMCONF		0xbfff0010
#define PCIMT_IOMMU		0xbfff0018
#define PCIMT_IOADTIMEOUT1	0xbfff0020
#define PCIMT_DMAACCESS		0xbfff0028
#define PCIMT_DMAHIT		0xbfff0030
#define PCIMT_ERRSTATUS		0xbfff0038
#define PCIMT_ERRADDR		0xbfff0040
#define PCIMT_SYNDROME		0xbfff0048
#define PCIMT_ITPEND		0xbfff0050
#define  IT_INT2		0x01
#define  IT_INTD		0x02
#define  IT_INTC		0x04
#define  IT_INTB		0x08
#define  IT_INTA		0x10
#define  IT_EISA		0x20
#define  IT_SCSI		0x40
#define  IT_ETH			0x80
#define PCIMT_IRQSEL		0xbfff0058
#define PCIMT_TESTMEM		0xbfff0060
#define PCIMT_ECCREG		0xbfff0068
#define PCIMT_CONFIG_ADDRESS	0xbfff0070
#define PCIMT_ASIC_ID		0xbfff0078	/* read */
#define PCIMT_SOFT_RESET	0xbfff0078	/* write */
#define PCIMT_PIA_OE		0xbfff0080
#define PCIMT_PIA_DATAOUT	0xbfff0088
#define PCIMT_PIA_DATAIN	0xbfff0090
#define PCIMT_CACHECONF		0xbfff0098
#define PCIMT_INVSPACE		0xbfff00a0
#endif

#define PCIMT_PCI_CONF		0xbfff0100

/*
 * Data port for the PCI bus in IO space
 */
#define PCIMT_CONFIG_DATA	0x0cfc

/*
 * Board specific registers
 */
#define PCIMT_CSMSR		0xbfd00000
#define PCIMT_CSSWITCH		0xbfd10000
#define PCIMT_CSITPEND		0xbfd20000
#define PCIMT_AUTO_PO_EN	0xbfd30000
#define PCIMT_CLR_TEMP		0xbfd40000
#define PCIMT_AUTO_PO_DIS	0xbfd50000
#define PCIMT_EXMSR		0xbfd60000
#define PCIMT_UNUSED1		0xbfd70000
#define PCIMT_CSWCSM		0xbfd80000
#define PCIMT_UNUSED2		0xbfd90000
#define PCIMT_CSLED		0xbfda0000
#define PCIMT_CSMAPISA		0xbfdb0000
#define PCIMT_CSRSTBP		0xbfdc0000
#define PCIMT_CLRPOFF		0xbfdd0000
#define PCIMT_CSTIMER		0xbfde0000
#define PCIMT_PWDN		0xbfdf0000

/*
 * A20R based boards
 */
#define A20R_PT_CLOCK_BASE      0xbc040000
#define A20R_PT_TIM0_ACK        0xbc050000
#define A20R_PT_TIM1_ACK        0xbc060000

#define SNI_MIPS_IRQ_CPU_TIMER  (MIPS_CPU_IRQ_BASE+7)

#define SNI_A20R_IRQ_BASE       MIPS_CPU_IRQ_BASE
#define SNI_A20R_IRQ_TIMER      (SNI_A20R_IRQ_BASE+5)

#define SNI_DS1216_A20R_BASE    0xbc081ffc
#define SNI_DS1216_RM200_BASE   0xbcd41ffc

#define SNI_PCIT_INT_REG        0xbfff000c

#define SNI_PCIT_INT_START      24
#define SNI_PCIT_INT_END        30

#define PCIT_IRQ_ETHERNET       (MIPS_CPU_IRQ_BASE + 5)
#define PCIT_IRQ_INTA           (SNI_PCIT_INT_START + 0)
#define PCIT_IRQ_INTB           (SNI_PCIT_INT_START + 1)
#define PCIT_IRQ_INTC           (SNI_PCIT_INT_START + 2)
#define PCIT_IRQ_INTD           (SNI_PCIT_INT_START + 3)
#define PCIT_IRQ_SCSI0          (SNI_PCIT_INT_START + 4)
#define PCIT_IRQ_SCSI1          (SNI_PCIT_INT_START + 5)


/*
 * Interrupt 0-16 are EISA interrupts.  Interrupts from 16 on are assigned
 * to the other interrupts generated by ASIC PCI.
 *
 * INT2 is a wired-or of the push button interrupt, high temperature interrupt
 * ASIC PCI interrupt.
 */
#define PCIMT_KEYBOARD_IRQ	 1
#define PCIMT_IRQ_INT2		24
#define PCIMT_IRQ_INTD		25
#define PCIMT_IRQ_INTC		26
#define PCIMT_IRQ_INTB		27
#define PCIMT_IRQ_INTA		28
#define PCIMT_IRQ_EISA		29
#define PCIMT_IRQ_SCSI		30

#define PCIMT_IRQ_ETHERNET	(MIPS_CPU_IRQ_BASE+6)

#if 0
#define PCIMT_IRQ_TEMPERATURE	24
#define PCIMT_IRQ_EISA_NMI	25
#define PCIMT_IRQ_POWER_OFF	26
#define PCIMT_IRQ_BUTTON	27
#endif

/*
 * Base address for the mapped 16mb EISA bus segment.
 */
#define PCIMT_EISA_BASE		0xb0000000

/* PCI EISA Interrupt acknowledge  */
#define PCIMT_INT_ACKNOWLEDGE	0xba000000

/* board specific init functions */
extern void sni_a20r_init (void);
extern void sni_pcit_init (void);
extern void sni_rm200_init (void);
extern void sni_pcimt_init (void);

/* board specific irq init functions */
extern void sni_a20r_irq_init (void);
extern void sni_pcit_irq_init (void);
extern void sni_pcit_cplus_irq_init (void);
extern void sni_rm200_irq_init (void);
extern void sni_pcimt_irq_init (void);

/* timer inits */
extern void sni_cpu_time_init(void);

/* common irq stuff */
extern void (*sni_hwint)(void);
extern struct irqaction sni_isa_irq;

#endif /* __ASM_SNI_H */
