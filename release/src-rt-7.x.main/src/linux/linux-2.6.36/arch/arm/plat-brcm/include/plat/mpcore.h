/*
 * ARM A9 MPCORE
 *
 * Platform hardware information and internal API
 */

#ifndef	PLAT_MPCORE_H
#define	PLAT_MPCORE_H

/* MPCORE internally-connected IRQs */
#define	MPCORE_IRQ_GLOBALTIMER	27
#define	MPCORE_IRQ_LOCALTIMER	29

/* 
 NOTE: MPCORE physical based ontained at run-time,
 while its virtual base address is set at compile-time in memory.h
*/

/* MPCORE register offsets */
#define	MPCORE_SCU_OFF		0x0000	/* Coherency controller */
#define	MPCORE_GIC_CPUIF_OFF	0x0100	/* Interrupt controller CPU interface */
#define	MPCORE_GTIMER_OFF	0x0200	/* Global timer */
#define	MPCORE_LTIMER_OFF	0x0600	/* Local (private) timers */
#define	MPCORE_GIC_DIST_OFF	0x1000	/* Interrupt distributor registers */

#ifndef __ASSEMBLY__

extern void __init mpcore_map_io( void );
extern void __init mpcore_init_gic( void );
extern void __init mpcore_init_timer( unsigned long periphclk_freq );

extern void __init 
	mpcore_gtimer_init( 
	void __iomem *base, 
	unsigned long freq,
	unsigned int timer_irq);

extern void __iomem * scu_base_addr(void);
extern void __cpuinit mpcore_cpu_init(void);
extern void plat_wake_secondary_cpu( 
	unsigned cpus, void (* _sec_entry_va)(void) );

#endif

#endif	/* PLAT_MPCORE_H */
