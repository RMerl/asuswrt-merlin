/*
 * iProc Clock Control Unit
 * The software model repsresents the hardware clock hierarchy
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/delay.h>

#include <asm/clkdev.h>
#include <mach/clkdev.h>

static struct resource ccu_regs = {
	.name = "cru_regs",
	.start = 0x19000000,
	.end =  0x19000fff,
        .flags = IORESOURCE_MEM,
};

/*
 * Clock management scheme is a provisional implementation
 * only intended to retreive the pre-set frequencies for each
 * of the clocks.
 */

/*
 * Get PLL running status and update output frequency
 * for ARMPLL channel 0
 */
static int a9pll0_status(struct clk * clk)
{
	u32 regA, regB, regC;
	u32 pdiv, ndiv_int, ndiv_frac, mdiv;
	u64 x;

	if( clk->type != CLK_PLL)
		return -EINVAL;

	BUG_ON( ! clk->regs_base );
	BUG_ON( ! clk->parent );

	/* read status register */
	regA = readl( clk->regs_base + 0x0c00 );/* IHOST_PROC_CLK_PLLARMA */
	regB = readl( clk->regs_base + 0x0c04 );/* IHOST_PROC_CLK_PLLARMB */
	regC = readl( clk->regs_base + 0x0c08 );/* IHOST_PROC_CLK_PLLARMB */

	/* reg C bit 8 is bypass mode - input frequency to output */
	if( (regC & (1 << 8)) == 1 )
		{
		clk->rate = clk->parent->rate;
		return 0;
		}

	/* reg A bit 28 is "lock" signal, has to be "1" for proper operation */
	if( (regA & (1 << 28)) == 0 )
		{
		clk->rate = 0;
		return -EIO;
		}

	/* Update PLL frequency */


	/* pdiv in bits 24..27 (4 bits) */
	pdiv = ( regA >> 24 ) & 0xf ;
	if( pdiv == 0 ) pdiv = 0x10 ;

	/* feedback divider (int)  in bits 8..17 (10 bits) */
	ndiv_int = ( regA >> 8) & ((1<<10)-1);
	if( ndiv_int == 0 ) ndiv_int = 1 << 10 ;

	/* feedback divider fraction in reg B, bits 0..19 */
	ndiv_frac = regB & ((1<<20)-1);

	/* post-divider is in reg C bits 0..7 */
	mdiv = regC & 0xff ;
	if(mdiv == 0) mdiv = 0x100;

	x = ((u64) ndiv_int << 20) | ndiv_frac ;
	x = (x * clk->parent->rate ) >> 20 ;
	(void) do_div( x, pdiv );


	(void) do_div(x, mdiv);
	clk->rate = (u32)(x);

	return 0;
}


/*
 * Get PLL running status and update output frequency
 * for ARMPLL channel 1
 */
static int a9pll1_status(struct clk * clk)
{
	u32 regA, regB, regC, regD;
	unsigned pdiv, ndiv_int, ndiv_frac, mdiv;
	u64 x;

	if( clk->type != CLK_PLL)
		return -EINVAL;

	BUG_ON( ! clk->regs_base );
	BUG_ON( ! clk->parent );

	/* read status register */
	regA = readl( clk->regs_base+0xc00 );/* IHOST_PROC_CLK_PLLARMB */
	regB = readl( clk->regs_base+0xc04 );/* IHOST_PROC_CLK_PLLARMB */
	regC = readl( clk->regs_base+0xc20 );/* IHOST_PROC_CLK_PLLARMCTRL5 */
	regD = readl( clk->regs_base+0xc24 );/* IHOST_PROC_CLK_PLLARM_OFFSET*/
	
	/* reg C bit 8 is bypass mode - input frequency to output */
	if( (regC & (1 << 8)) == 1 )
		{
		clk->rate = clk->parent->rate;
		return 0;
		}

	/* reg A bit 28 is "lock" signal, has to be "1" for proper operation */
	if( (regA & (1 << 28)) == 0 )
		{
		clk->rate = 0;
		return -EIO;
		}

	/* Update PLL frequency */


	/* pdiv in bits 24..27 (4 bits) */
	pdiv = ( regA >> 24 ) & 0xf ;
	if( pdiv == 0 ) pdiv = 0x10 ;

	/* Check if offset mode is active */
	if( regD & (1<<29) )
		{
		/* pllarm_ndiv_int_offset bits 27:20 */
		ndiv_int = ( regD >> 20 ) & 0xff ;
		if( ndiv_int == 0 ) ndiv_int = 1 << 8 ;

		/* pllarm_ndiv_frac_offset bits 19:0 */
		ndiv_frac = regD & ((1<<20)-1);
		}
	/* If offset not active, channel 0 parameters are used */
	else
		{
		/* feedback divider (int)  in bits 8..17 (10 bits) */
		ndiv_int = ( regA >> 8) & ((1<<10)-1);
		if( ndiv_int == 0 ) ndiv_int = 1 << 10 ;

		/* feedback divider fraction in reg B, bits 0..19 */
		ndiv_frac = regB & ((1<<20)-1);
		}
	/* post-divider is in reg C bits 0..7 */
	mdiv = regC & 0xff ;
	if(mdiv == 0) mdiv = 0x100;


	x = ((u64) ndiv_int << 20) | ndiv_frac ;
	x = (x * clk->parent->rate ) >> 20 ;
	(void) do_div( x, pdiv );

	(void) do_div(x, mdiv);
	clk->rate = (u32)(x);

	return 0;
}


static const struct clk_ops a9pll0_ops = {
	.status = a9pll0_status,
};

static const struct clk_ops a9pll1_ops = {
	.status = a9pll1_status,
};


/*
 * iProc A9 PLL
 * could be used as source for generated clocks
 */
static struct clk clk_a9pll[2] = {
	{
	.ops 	= &a9pll0_ops,
	.name 	= "arm_cpu_clk",
	.type	= CLK_PLL,
	.chan	= 0xa,
	},
	{
	.ops 	= &a9pll1_ops,
	.name 	= "arm_cpu_h_clk",
	.type	= CLK_PLL,
	.chan	= 0xb,
	},
};

/*
 * Decode the Frequency ID setting for arm_clk
 */
static int iproc_cru_arm_freq_id( void * __iomem regs_base )
{
	u32 reg_f, reg;
	unsigned policy, fid, i ;
	u8 arm_clk_policy_mask = 0, apb0_clk_policy_mask = 0 ;

	/* bits 0..2 freq# for policy0, 8..10 for policy1, 
	 * 16..18 policy2, 24..26 policy 3
	 */
	reg_f = readl( regs_base+0x008 );/*IHOST_PROC_CLK_POLICY_FREQ*/

	for(i = 0; i < 4; i ++ ) {
		/* Reg IHOST_PROC_CLK_POLICY<i>_MASK*/
		/* bit 20 arm policy mask, bit 21 apb0 policy mask */
		reg = readl( regs_base+0x010+i*4 );
		arm_clk_policy_mask |= (1 & ( reg >> 20 )) << i;
		apb0_clk_policy_mask |=  (1 & ( reg >> 21 )) << i;
	}

	/* TBD: Where to get hardware policy setting ? */
	policy = 0 ;

	/* Check for PLL policy software override */
	reg = readl( regs_base+0xe00 );/* IHOST_PROC_CLK_ARM_DIV */
	if( reg & (1 << 4 ))
		policy = reg & 0xf ;

	fid = (reg_f >> (8 * policy)) & 0xf;

	/* Verify freq_id from debug register */
	reg = readl( regs_base+0xec0 );/* IHOST_PROC_CLK_POLICY_DBG */
	/* Bits 12..14 contain active frequency_id */
	i = 0x7 & (reg >> 12);
	
	if( fid != i ) {
		printk(KERN_WARNING
			"IPROC CRU clock frequency id override %d->%d\n",
			fid, i );
		fid = i ;
		}

	return fid ;
}

/*
 * Get status of any of the ARMPLL output channels
 */
static int a9pll_chan_status(struct clk * clk)
{
	u32 reg;
	unsigned freq_id, div ;

	if( clk->type != CLK_DIV)
		return -EINVAL;

	BUG_ON( ! clk->regs_base );

	freq_id = iproc_cru_arm_freq_id( clk->regs_base );


	switch( clk->chan )
		{
		default:
			return -EINVAL;

		case 0x3a:	/* arm_clk */
			if( freq_id == 7 ) {
				clk->parent = &clk_a9pll[1]; /* arm_clk_h */
				div = 1;
				}
			else if( freq_id == 6 ) {
				clk->parent = &clk_a9pll[0]; /* arm_clk */
				div = 1;
				}
			else if( freq_id == 2 ) {
				struct clk * clk_lcpll_200;
				clk_lcpll_200 =
					clk_get_sys( NULL, "sdio_clk");
				BUG_ON( ! clk_lcpll_200 );
				clk->parent = clk_lcpll_200;
				div = 2;
				}
			else {
				clk->parent = clk_a9pll[0].parent;
				div = 1;
				}
			break;

		case 0x0f:	/* periph_clk */
			div = 2;
			break;

		case 0x0a:
			/* IHOST_PROC_CLK_ARM_DIV */
       			reg = readl( clk->regs_base+0xe00 );
			/* apb0_free_div bits 10:8 */
			div = ( reg >> 8 ) & 0x7;
			if( div == 0 ) div = 8;
			break;

		case 0x0b:
			/* IHOST_PROC_CLK_ARM_DIV */
       			reg = readl( clk->regs_base+0xe00 );
			/* arm_switch_div bits 6:5 */
			div = ( reg >> 5 ) & 0x3 ;
			if( div == 0 ) div = 4;
			break;

		case 0x1a:
			/* IHOST_PROC_CLK_APB_DIV apb_clk_div bits 1:0 */
			reg = readl( clk->regs_base+0xa10 );
			div = reg & 0x3 ;
			if( div == 0 ) div = 4;
			break;

		}

	BUG_ON( ! clk->parent );
	/* Parent may have changed, use top-level API to force recursion */
	clk->rate = clk_get_rate( clk->parent ) / div ;

	return 0;
}


static const struct clk_ops a9pll_chan_ops = {
	.status = a9pll_chan_status,
};

/*
 * iProc A9 PLL output clocks
 */
static struct clk clk_a9chan[] = {
	{ .ops = &a9pll_chan_ops, .type = CLK_DIV, .parent = &clk_a9pll[0],
	.name = "arm_clk", 	.chan = 0x3a },
	{ .ops = &a9pll_chan_ops, .type = CLK_DIV, .parent = &clk_a9chan[0],
	.name = "periph_clk", 	.chan = 0x0f },
	{ .ops = &a9pll_chan_ops, .type = CLK_DIV, .parent = &clk_a9chan[0],
	.name = "apb0_free", 	.chan = 0x0a },
	{ .ops = &a9pll_chan_ops, .type = CLK_DIV, .parent = &clk_a9chan[0],
	.name = "arm_switch", 	.chan = 0x0b },
	{ .ops = &a9pll_chan_ops, .type = CLK_DIV, .parent = &clk_a9chan[0],
	.name = "apb_clk", 	.chan = 0x1a },
};

static struct clk_lookup cru_clk_lookups[] = {
	{ .con_id= "armpll_clk", 	.clk= &clk_a9pll[0], },
	{ .con_id= "armpll_h_clk",	.clk= &clk_a9pll[1], },
	{ .con_id= "arm_clk", 	.clk= &clk_a9chan[0], },
	{ .con_id= "periph_clk",.clk= &clk_a9chan[1], },
	{ .con_id= "apb0_free",	.clk= &clk_a9chan[2], },
	{ .con_id= "axi_clk",	.clk= &clk_a9chan[3], },
	{ .con_id= "apb_clk",	.clk= &clk_a9chan[4], },
};

void __init soc_cru_init( struct clk * src_clk )
{
	void * __iomem reg_base;
	unsigned i;

	BUG_ON( request_resource( &iomem_resource, &ccu_regs ));

	reg_base = ioremap( ccu_regs.start, resource_size(&ccu_regs));

	BUG_ON( IS_ERR_OR_NULL(reg_base ));

	/* Initialize clocks */
	for(i = 0; i < ARRAY_SIZE(clk_a9pll); i++ )
		{
		clk_a9pll[i].regs_base = reg_base ;
		clk_a9pll[i].parent = src_clk ;
		}

	for(i = 0; i < ARRAY_SIZE(clk_a9chan); i++ )
		{
		clk_a9chan[i].regs_base = reg_base ;
		}

	/* Install clock sources into the lookup table */
	clkdev_add_table(cru_clk_lookups, 
			ARRAY_SIZE(cru_clk_lookups));
}

void cru_clocks_show( void )
{
	unsigned i;

	printk( "CRU Clocks:\n" );
	for(i = 0; i < ARRAY_SIZE( cru_clk_lookups); i++ )
		{
		printk( "%s: (%s) %lu\n",
			cru_clk_lookups[i].con_id,
			cru_clk_lookups[i].clk->name,
			clk_get_rate( cru_clk_lookups[i].clk )
			);
		}
	printk( "CRU Clocks# %u\n", i );
}

/*
 * Perform reset of one of the two cores by means of the IPROC CRU
 */
int soc_cpu_reset(unsigned cpu, bool reset)
{
	void * __iomem reg_base;
	u32 reg;

	reg_base =	clk_a9pll[0].regs_base ;

	if( cpu >= 2 || ! reg_base )
		return -EINVAL;

	/*
	register IHOST_PROC_RST_WR_ACCESS	 0x19000F00
	<password> bits[23:8] Read will return 0. 0x0000
	<rstmgr_acc> bits[0] Reset manager access enable. 
		This bit must be 1 for any write access to the remaining Reset 
		manager registers. This field is protected from accidental
		modification. Writing is allowed only when write data 
		bits [23:8] contain A5A5.
	*/

	/* Enable writing to reset control bits */
	writel( 0xa5a5 << 8 | 0x1, reg_base + 0xf00 );

	/*
	register IHOST_PROC_RST_A9_CORE_SOFT_RSTN	0x19000F08
	<a9_core_1_soft_rstn> bit[1] - Soft reset for a9_core_1. 
		When this bit is 0 a9_core_1 will be in reset state. Default=1
	<a9_core_0_soft_rstn> bit[0] - Soft reset for a9_core_0.
		When this bit is 0 a9_core_0 will be in reset state. Default=1
	*/
	reg = 0x3 ^ (1 << cpu) ;
	writel( reg, reg_base + 0xf00 );
	writel( 0x3, reg_base + 0xf00 );

	/* Disable writing to reset control bits */
	writel( 0x0, reg_base + 0xf00 );

	return 0;
}
