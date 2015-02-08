/*
 * Top-level clock management API
 * see include/linux/clk.h for description.
 * These routines are hardware-independent,
 * and all hardware-specific code is invoked
 * through the "ops" methods.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>

#include <mach/clkdev.h>

int clk_enable(struct clk *clk)
{
	int ret ;

	ret = atomic_inc_return( &clk->ena_cnt );
	if( ret > 1 )
		return 0;
	/* Continue of count was moved from 0 to 1 - reentrant */
	if( clk->parent )
		ret = clk_enable( clk->parent );
	else
		ret = 0;

	if( ret == 0 )
		{
		if( ! clk->ops || ! clk->ops->enable )
			{
			if( clk->rate )
				ret = 0 ;
			else
				ret = -EIO;
			}
		else
			{
			ret =  clk->ops->enable( clk ) ;
			}
		}

	if( ret != 0 )
		atomic_dec( &clk->ena_cnt );

	return ret ;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
	int ret ;

	ret = atomic_dec_return( &clk->ena_cnt );

	/* Continue if this is the last client to disable - reentrant */
	if( ret > 0 )
		return ;
	BUG_ON( ret < 0 );

	if( ! clk->ops || ! clk->ops->disable )
		return;

	clk->ops->disable( clk );

	if( clk->parent )
		clk_disable( clk->parent );

	return ;
}
EXPORT_SYMBOL(clk_disable);

unsigned long clk_get_rate(struct clk *clk)
{
	/* Recurse to update parent's frequency */
	if( clk->parent )
		clk_get_rate( clk->parent );
	/* Read hardware registers if needed */
	if( clk->ops && clk->ops->status )
		clk->ops->status(clk);
	return clk->rate;
}
EXPORT_SYMBOL(clk_get_rate);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	long ret = -EIO;
	if (clk->ops && clk->ops->round)
		ret = clk->ops->round(clk, rate);
	return ret;
}
EXPORT_SYMBOL(clk_round_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret = -EIO;

	if( rate == clk->rate )
		return 0;

	if (clk->ops && clk->ops->setrate)
		ret = clk->ops->setrate(clk, rate);

	return ret;
}
EXPORT_SYMBOL(clk_set_rate);

/*
 * clk_get(), clk_put() are implemented in arch/arm/common/clock.c
 * but it needs these two stub functions for platform-specific operations.
 * Reeturn 1 on success 0 on failure.
 */

int __clk_get(struct clk *clk)
{
	int ret ;

	ret = atomic_inc_return( &clk->use_cnt );
	if( ret > 1 )
		return 1;
	if( clk->parent )
		return __clk_get( clk->parent );
	return 1;
}

void __clk_put(struct clk *clk)
{
	int ret;

	ret = atomic_dec_return( &clk->use_cnt );
	if( ret > 0 )
		return;

	BUG_ON( ret < 0 );

	if( clk->parent )
		__clk_put( clk->parent );
}
