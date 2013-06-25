 
		
#include <linux/linkage.h>
#include <asm/assembler.h>

ENTRY(arch_idle)
#ifdef CONFIG_MSM7X00A_IDLE
	mrc     p15, 0, r1, c1, c0, 0    /* read current CR    */
	bic     r0, r1, #(1 << 2)        /* clear dcache bit   */
	bic     r0, r0, #(1 << 12)       /* clear icache bit   */
	mcr     p15, 0, r0, c1, c0, 0    /* disable d/i cache  */

	mov     r0, #0                   /* prepare wfi value  */
	mcr     p15, 0, r0, c7, c10, 0   /* flush the cache    */
	mcr     p15, 0, r0, c7, c10, 4   /* memory barrier     */
	mcr     p15, 0, r0, c7, c0, 4    /* wait for interrupt */

	mcr     p15, 0, r1, c1, c0, 0    /* restore d/i cache  */
#endif
	mov     pc, lr
