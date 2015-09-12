/*
 * Broadcom ARM BSP
 * Internal API declarations
 */


#ifndef __ASSEMBLY__
struct clk ;

extern void __init soc_map_io( struct clk * );
extern void __init soc_init_irq( void );
extern void __init soc_add_devices( void );
extern void __init soc_dmu_init( struct clk * ref_clk );
extern void __init soc_pmu_clk_init( struct clk * ref_clk );
extern void __init soc_init_timer( void );
extern void         soc_clocks_show( void );
extern void __init soc_cru_init( struct clk * ref_clk );
extern void	cru_clocks_show( void );
extern int soc_cpu_reset(unsigned cpu);

#endif
