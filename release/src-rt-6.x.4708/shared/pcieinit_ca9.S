#include <arminc.h>
#include <hndsoc.h>
#include <sbchipc.h>

#ifndef __arm__
#error __arm__ is NOT defined
#endif

/* Routine begin/end macro */
#if defined(__thumb__)
#define FUNC(x) THUMBLEAF(x)
#else
#define FUNC(x) LEAF(x)
#endif  /* __thumb__ */

#ifdef BCMDBG
#define TRACE(x) \
	ldr	r9,=(x); \
	ldr     r10,=SI_ENUM_BASE; \
	str	r9,[r10,#0x64]

#define TRACE1(x) \
	mov	r9,x; \
	ldr     r10,=SI_ENUM_BASE; \
	str	r9,[r10,#0x68]

#define TRACE2(x) \
	mov	r9,x; \
	ldr     r10,=SI_ENUM_BASE; \
	str	r9,[r10,#0x64]
#else
#define TRACE(x)
#define TRACE1(x)
#define TRACE2(x)
#endif

	.text

FUNC(pcie_phy_init)
	mov	r6,lr
	/*
	 *    Sys Init code -- read OTP, program the H/W blocks
	 *    At completion, take a jump to __hw_init_done
	 */
__sys_init:

	ldr	r0,=SI_ENUM_BASE		/* r0:	core regs SI base address */
	ldr	r4,[r0,#CC_CAPABILITIES]	/* r4:	capabitilies */
	and	r4,r4,#CC_CAP_OTPSIZE
	cmp	r4,#0
	beq	__hw_init_done
	ldr	r1,[r0,#CC_CLKDIV]
	mov	r2,#~CLKD_OTP
	and	r2,r2,r1
	mov	r1,#(0xe << CLKD_OTP_SHIFT)
	orr	r2,r2,r1
	str	r2,[r0,#CC_CLKDIV]		/* Update OtpClk in clkdiv with 14 */
	ldr	r1,=200				/* Wait for 200 cycles for stable OTP clock */
spinn:	sub	r1,r1,#0x1
	cmp	r1,#0x0
	bne	spinn
	lsr	r2,r4,#CC_CAP_OTPSIZE_SHIFT	/* Get OTP size */
	adrl	r1,otp_sizes
	lsl	r2,r2,#0x2
	ldr	r3,[r1,r2]			/* Get the OTP size from otp_sizes table (in bytes) */

	cmp	r3,#0x0				/* skip zero size */
	beq	__hw_init_done
	ldr	r1,[r0,#CC_OTPST]
	and	r1,r1,#OTPS_READY
	cmp	r1,#0x0				/* 0: OTP is not ready for whatever reason */
	beq	__hw_init_done
	add	r2,r3,r0			/* Seek to the end of OTP region */
	/* Scan entire OTP from END -> BEGIN to find first
	 * matching hwinit entry */
	sub	r2,r2,#2
1:	
	add	r4,r2,#CC_SROM_OTP
	ldrh	r1,[r4]
	lsl	r1,r1,#0x10
	sub	r2,r2,#2
	add	r4,r2,#CC_SROM_OTP
	ldrh	r3,[r4]
	orr	r1,r1,r3
	ldr	r4,=0xeaeaeaea			/* SID */
	cmp	r1,r4
	beq	1f
	cmp	r0,r2
	blt	1b
	/* no hwinit data from OTP */
	b	__hw_init_done
1:
	add	r0,r2,#CC_SROM_OTP
	add	r0,r0,#4
	mov	r1,#0x800	/* for the first delay */
	
__init_next_cmd:

	/*
	 *    delay for N cycles
	 *    read OTP : 32b entry comprise {16b loop count, 8b delay, 8b cmd}
	 *    execute OPT cmd.
	 *    Do_Next if any.
	 */

	bl	__init_dly	/* inter-command delay */
	
	ldrh	r1,[r0],#2	/* {delay, Cmd} */
	and	r2,r1,#0x0f	/* extract Cmd */
	cmp	r2,#0x1		/* EID (end of init data) */
	beq	__hw_init_done	/* jump to __hw_init_done on End of Init Data */
	ldrh	r3,[r0],#2	/* Loop Count */

	cmp	r2,#0		/* NOP */
	beq	__init_next_cmd
		
	/* load two 32b datum */
	bl	__init_load64
	mov	r7,r10		/* prepare indirect addr */
	mov	r8,r12		/* prepare indirect data */

	/*
	 * handle various cases
	 */ 
	cmp	r2,#4
	beq	__init_wr_dir
	cmp	r2,#5
	beq	__init_wr_indir
	cmp	r2,#6
	beq	__init_wr_indir_incaddr
	cmp	r2,#7
	beq	__init_wr_indir_incaddr
	cmp	r2,#8
	beq	__init_wr_dir_incaddr_data
	cmp	r2,#9
	beq	__init_wr_indir_incaddr_data	
	cmp	r2,#10	
	beq	__init_wr_dir_incaddr_incdata
	cmp	r2,#11
	beq	__init_wr_indir_incaddr_incdata	
	
	/* write direct (cmd=0x02) is a special case of
	 * write direct multiple (cmd=0x04), under loop count=1

	 * write indirect (cmd=0x03) is a special case of
	 * write indirect multiple (cmd=0x05), under loop count=1
	 */

	/* write direct multiple (cmd=0x04) */
__init_wr_dir:
	str	r12,[r10]		/* Mem[addr] = data */
	subs	r3,r3,#1		/* loop cnt = loop cnt -1 */
	beq	__init_next_cmd		/* loop cnt = 0 */
	bl	__init_load64		/* load next pair of addr, data */
	bl	__init_dly		/* inter-op delay */
	b	__init_wr_dir

	/* write indirect multiple (cmd=0x05) */
__init_wr_indir:
	bl	__init_load64		/* load next pair of addr, data */
	str	r10,[r7]		/* mem[indirect addr] = addr */
	str	r12,[r8]		/* mem[indirect data] = data */
	subs	r3,r3,#1		/* loop cnt = loop cnt -1 */
	beq	__init_next_cmd		/* loop cnt = 0 */
	bl	__init_dly		/* inter-op delay */	
	b	__init_wr_indir

	/* cmd=0x06 */
	/* write direct, (post) auto-inc address */ 
__init_wr_dir_incaddr:
	str	r12,[r10],#4		/* Mem[addr++] = data, auto-inc addr */
	subs	r3,r3,#1		/* loop cnt = loop cnt -1 */
	beq	__init_next_cmd		/* loop cnt = 0 */
	bl	__init_load32		/* load next data */
	bl	__init_dly		/* inter-op delay */
	b	__init_wr_indir_incaddr

	/* cmd=0x07 */
	/* write indirect, (post) auto-inc address */
__init_wr_indir_incaddr:
	bl	__init_load64		/* load first addr & data */
__next_d_0:
	str	r10,[r7]		/* Mem[addr] = addr */
	str	r12,[r8]		/* Mem[data] = data */
	subs	r3,r3,#1		/* loop cnt = loop cnt -1 */
	beq	__init_next_cmd		/* loop cnt = 0 */
	add	r10,r10,#4		/* inc addr by 4 bytes */
	bl	__init_load32		/* load next data */
	bl	__init_dly		/* inter-op delay */
	b	__next_d_0

	/* cmd=0x08 */
	/* write direct multiple, repeatedly the same data to (post) auto-inc address */
__init_wr_dir_incaddr_data:
	str	r12,[r10],#4		/* Mem[addr] = data, auto_inc addr */
	subs	r3,r3,#1		/* loop cnt = loop cnt -1 */
	beq	__init_next_cmd		/* loop cnt = 0 */
	bl	__init_dly		/* inter-op delay */
	b	__init_wr_dir_incaddr_data

	/* cmd=0x09 */
	/* write inirect multiple, repeatedly the same data to (post) auto-inc address */
__init_wr_indir_incaddr_data:
	bl	__init_load64		/* load first addr & data */
__next_d_1:
	str	r10,[r7]		/* Mem[addr] = addr */
	str	r12,[r8]		/* Mem[data] = data */
	subs	r3,r3,#1		/* loop cnt = loop cnt -1 */
	beq	__init_next_cmd		/* loop cnt = 0 */
	add	r10,r10,#4		/* inc addr by 4 bytes */
	bl	__init_dly		/* inter-op delay */
	b	__next_d_1

	/* cmd=0x0a */
	/* write direct multiple, (post) auto-inc addr and data */
__init_wr_dir_incaddr_incdata:
	str	r12,[r10],#4		/* Mem[addr] = data, inc addr */
	subs	r3,r3,#1		/* loop cnt = loop cnt -1 */
	beq	__init_next_cmd		/* loop cnt = 0 */
	add	r12,r12,#1		/* inc data by 1 */
	bl	__init_dly		/* inter-op delay */
	b	__init_wr_dir_incaddr_incdata
	
	/* cmd=0x0b */
	/* write indirect multiple, (post) auto-inc addr and data */	
__init_wr_indir_incaddr_incdata:
	bl	__init_load64		/* load first addr & data */
__next_d_2:
	str	r10,[r7]		/* Mem[addr] = addr */
	str	r12,[r8]		/* Mem[data] = data */
	subs	r3,r3,#1		/* loop cnt = loop cnt -1 */
	beq	__init_next_cmd		/* loop cnt = 0 */
	add	r10,r10,#4		/* inc addr by 4 bytes */
	add	r12,r12,#1		/* inc data by 1 */
	bl	__init_dly		/* inter-op delay */
	b	__next_d_2

	/*
	 * Define two Utilities to save the code space.
	 *
	 * load one or two 32b datum from current OTP position
	 * r10 = {r10, r9}
	 * r12 = {r12, r11}
	 */
__init_load64:
	ldrh	r10,[r0],#2
	ldrh	r9,[r0],#2
	orr	r10,r9,r10,lsl #16
__init_load32:
	ldrh	r12,[r0],#2
	ldrh	r11,[r0],#2
	orr	r12,r11,r12,lsl #16
	bx	lr			 /* return */

	/* impose some inter-op and inter-cmd delay
	 * delay count is passed thru reg. r1
	 */
__init_dly:
	asr	r9,r1,#11	/* r1/8 cycles, effectively (r1&0x0000ff00) >> 3 */
	cmp	r9,#0
	beq	__no_dly
__dly_loop:
	subs	r9,r9,#1	/* loop here */
	bne	__dly_loop
__no_dly:	
	bx	lr		/* return */

	/*
	 * Hardware init. is done
	 */
		
__hw_init_done:
	/* Continuing boot */
	mov	lr,r6
	mov	pc,lr

/* OTP sizes in bytes */
otp_sizes:
	.word	0
	.word	256	/* 2048 bits: 32X64 */
	.word	512	/* 4096 bits: 2*32X64 */ 
	.word	1024	/* 8192 bits: 4*32X64 */
	.word	512	/* 4096 bits: 64X64 */
	.word	768	/* 6144 bits: 5 32X64 */
	.word	0	/* 512 bits: dont care */
	.word	128	/* 1024 bits: 8X64 */

END(pcie_phy_init)
