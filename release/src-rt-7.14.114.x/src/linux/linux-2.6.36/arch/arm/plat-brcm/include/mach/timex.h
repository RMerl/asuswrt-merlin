/*
 * This file exists to satisfy compile-time dependency from:
 * arch/arm/include/asm/timex.h
 * It must be a value known at compile time for <linux/jiffies.h>
 * but its value is never used in the resulting code.
 * If "get_cycles()" inline function in <asm/timex.h> is rewritten,
 * then in combination with this constant it could be used to measure
 * microsecond elapsed time using the global timer clock-source.
 * -LR
 */

#define CLOCK_TICK_RATE		(1000000)
