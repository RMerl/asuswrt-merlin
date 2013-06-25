
/*
 * iProc/iHost hardware rlated constants
 */

#define	SOC_CHIPCOMON_A_BASE_PA	0x18000000
#define	SOC_CHIPCOMON_B_BASE_PA	0x18001000

#define	SOC_SDOM_BASE_PA	0x18130000	/* System discover ROM */

#define	SOC_DMU_BASE_PA		0x1800c000	/* Power, Clock controls */

/*
 * UART Enumaration - 
 * ChipcommonB UART0 : ttyS0 
 * ChipcommonA UART0-1: ttyS1-ttyS3
 * TBD: which UARTs chare the I/O pins and are thus mutually exclusive ?!
 */
#define	PLAT_UART0_PA	0x18008000	/* ChipcommonB UART0 */
#define	IRQ_UART0	(32+85)

#define	PLAT_UART1_PA	(SOC_CHIPCOMON_A_BASE_PA+0x300)
#define	PLAT_UART2_PA	(SOC_CHIPCOMON_A_BASE_PA+0x400)
#define	IRQ_CCA		117


/* PL310 L2 Cache Controller base address */
#define	L2CC_BASE_PA		0x19022000	/* Verified */
/* L2 is 16-ways 256KByte w/ Parity */

/*
 * There is a 1KB LUT located at 0xFFFF0400-0xFFFFFFFF, and its first entry
 * is where the secondary entry point needs to be written
*/
#define	SOC_ROM_BASE_PA		0xffff0000
#define	SOC_ROM_LUT_OFF		0x400

/*
 * DRAM begins at 0x8000000 and can be 128MiB, 512MiB or 1GiB continous,
 * but first 128MiB is also aliased at address 0x0.
 */
#define	DRAM_LARGE_REGION_BASE	0x80000000
#define	DRAM_MEMORY_REGION_BASE	0x00000000
#define	DRAM_MEMORY_REGION_SIZE	SZ_128M
