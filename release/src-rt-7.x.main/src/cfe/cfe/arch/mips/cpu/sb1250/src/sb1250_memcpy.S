#  Copyright (c) 2001, 2002, 2003
#  Broadcom Corporation, Irvine, CA
#
#  BROADCOM PROPRIETARY AND CONFIDENTIAL
#  
#  This software is furnished under a Broadcom license agreement
#  and may only be used in accordance with and subject to the terms 
#  and conditions of such license. 
#
# Author: Mark Vandevoorde

#include <sb1250-include/sbmips.h>

#if defined(__long64)
	
#define LOAD  ld
#define LOADL ldl
#define LOADR ldr
#define STOREL sdl
#define STORER sdr
#define STORE sd
#define ADD   daddu
#define SUB   dsubu
#define SRL   dsrl
#define SRA   dsra
#define SLL   dsll
#define SLLV  dsllv
#define SRLV  dsrlv
#define NBYTES 8
#define LOG_NBYTES 3
	
#else
	
#define LOAD  lw
#define LOADL lwl
#define LOADR lwr
#define STOREL swl
#define STORER swr
#define STORE sw
#define ADD   addu
#define SUB   subu
#define SRL   srl
#define SLL   sll
#define SRA   sra
#define SLLV  sllv
#define SRLV  srlv
#define NBYTES 4
#define LOG_NBYTES 2
	
#endif /* __long64 */
     
#if defined(__MIPSEB)
#define LDFIRST LOADL
#define LDREST  LOADR
#define STFIRST STOREL
#define SHIFT_CPY SRLV
#endif
	
#if defined(__MIPSEL)
#define LDFIRST LOADR
#define LDREST  LOADL
#define STFIRST STORER
#define SHIFT_CPY SLLV
#endif

#define FIRST(unit) ((unit)*NBYTES)
#define REST(unit)  (FIRST(unit)+NBYTES-1)
#define DEST(unit)  FIRST(unit)

#define ADDRMASK (NBYTES-1)

#ifdef _M1WORKAROUND_
#define USE(x)  lbu AT,x; xor zero, AT, zero
#else
#define USE(x)
#endif
	

	.text
	.set noreorder
	.set noat
	.set mips64
	.globl memcpy_sb1
	.ent memcpy_sb1
	.align 4
memcpy_sb1:	
	## Arguments.  Note: dst & src may be unaligned, len may be 0
#define dst a0
#define src a1
#define len a2
	## Temps
#define count a3
#define match t8
#define dst2  match
	## The "issue break"s below are very approximate.
	## Issue delays for dcache fills will perturb the schedule, as will
	## load queue full replay traps, etc.

	## If len < NBYTES use byte operations
	pref	0, 0(src)
	pref	1, 0(dst)
	sltu	t2, len, NBYTES 	# long pipe
	and	t1, dst, ADDRMASK
	## issue break
	pref	0, 1*32(src)
	pref	1, 1*32(dst)
	bnez	t2, copy_bytes_checklen
	 and	t0, src, ADDRMASK
	## issue break
	pref	0, 2*32(src)
	bnez	t1, dst_unaligned
	 pref	1, 2*32(dst)
	## issue break
	bnez	t0, src_unaligned_dst_aligned
	## use delay slot for fall-through
	## src and dst are aligned; need to compute count
both_aligned:
	 SRL	count, len, LOG_NBYTES+3  # +3 for 8 units/iter
	ADD	dst2, dst, 8*NBYTES
	## issue break
	beqz	count, cleanup_both_aligned
	 sltu	t7, len, 4*NBYTES     # useful only when branch taken
	pref	0, 3*32(src)
	pref	1, 3*32(dst)
	and	len, len, NBYTES*8-1  # len = what's left to copy at loop exit
	
	.align 4
#ifdef PASS1
1:
	## issue break
	LOAD	t0, FIRST(0)(src)
	LOAD	t1, FIRST(1)(src)
	## issue break
	ssnop  # To avoid dcfifo full, we schedule 2 cycles of empty LS pipes
	ssnop  # during which the DCFIFO can complete 4 8-byte stores.  Pass3
	ssnop  # should not need these extra cycles.  The other 32
	       # bytes are completed during the 4 cycles where STORE is issued
	       # to LS1.
	       # Note: on Pass1, we need N+1 ssnops to pause N cycles.
	## issue break
	LOAD	t2, FIRST(2)(src)
	LOAD	t3, FIRST(3)(src)
	SUB	count, count, 1
	## issue break
	LOAD	t4, FIRST(4)(src)
	USE(	 FIRST(0-8)(dst2) )
	LOAD	t5, FIRST(5)(src)
	## issue break
	STORE	t0, FIRST(0-8)(dst2)  # PASS1: >= 3 cycles after load
	## issue break
#ifdef __long64
	USE(	 FIRST(4-8)(dst2) )
#else
	USE(	 FIRST(7-8)(dst2) )
#endif
	STORE	t1, FIRST(1-8)(dst2)
	## issue break
	LOAD	t6, FIRST(6)(src)
	LOAD	t7, FIRST(7)(src)
	ADD	src, src, 8*NBYTES   # long pipe OK
	ADD	dst2, dst2, 8*NBYTES  # long pipe OK
#ifdef __long64
	USE(	FIRST(7)(dst) )
#endif
	## issue break
	STORE	t2, FIRST(2)(dst)
	STORE	t3, FIRST(3)(dst)
	## issue break
	STORE	t4, FIRST(4)(dst)
	STORE	t5, FIRST(5)(dst)
	## issue break
	STORE	t6, FIRST(6)(dst)
	STORE	t7, FIRST(7)(dst)
#ifdef __long64
	## issue break
	pref	1, 7*32(dst)
	pref	1, 8*32(dst)
	ADD	dst, dst, 8*NBYTES   # long pipe OK
	## issue break
	pref	0, 6*32(src)
	bnez	count, 1b
	 pref	0, 7*32(src)
#else
	## issue break
	pref	1, 8*32(dst)
	pref	0, 7*32(src)
	## issue break
	bnez	count, 1b
	 ADD	dst, dst, 8*NBYTES   # long pipe OK
#endif
	
#else // not PASS1

	/* USE_DOUBLE _M1WORKAROUND_ DIST  cycles/cacheline
	   yes		  no	     L1      5
	   yes		  yes        L1      6  
	
	   no             no         L1      9
	   no            yes	     L1	     10
	*/

#define src2 t7
1:
#ifdef _M1WORKAROUND_
	lbu	AT, FIRST(0-8)(dst2)
	lbu	t0, FIRST(7-8)(dst2)
	xor	zero, AT, zero
	xor	zero, t0, zero

# ifdef __long64
	lbu	AT, FIRST(4-8)(dst2)
	ADD	zero, 1		# L1 NOP
	xor	zero, AT, zero
# endif
#endif	
	## issue break
	LOAD	t0, FIRST(0)(src)
	STORE	t0, FIRST(0-8)(dst2)
	ADD	src2, src, 0	      # long pipe OK
	
	## issue break
	LOAD	t0, FIRST(1)(src)
	STORE	t0, FIRST(1-8)(dst2)
	
	## issue break
	LOAD	t0, FIRST(2)(src)
	STORE	t0, FIRST(2-8)(dst2)
	ADD	dst2, dst2, 8*NBYTES  # long pipe OK
	
	## issue break
	LOAD	t0, FIRST(3)(src)
	STORE	t0, FIRST(3)(dst)

	## issue break
	LOAD	t0, FIRST(4)(src)
	STORE	t0, FIRST(4)(dst)
	ADD	src, src, 8*NBYTES   # long pipe OK
	
	## issue break
	LOAD	t0, FIRST(5)(src2)
	STORE	t0, FIRST(5)(dst)
	SUB	count, count, 1
	
	## issue break
	LOAD	t0, FIRST(6)(src2)
	STORE	t0, FIRST(6)(dst)

	## issue break
	LOAD	t0, FIRST(7)(src2)
	STORE	t0, FIRST(7)(dst)
	ADD	dst, dst, 8*NBYTES   # long pipe OK

#ifdef __long64
	## issue break
	pref	1, 6*32(dst2)
	pref	1, 7*32(dst2)
	
	## issue break
	pref	0, 7*32(src2)
	bnez	count, 1b
	 pref	0, 8*32(src2)
#else
	## issue break
	pref	1, 7*32(dst2)
	bnez	count, 1b
	 pref	0, 8*32(src2)
#endif
#endif // ifdef-else PASS1


#define rem t6
	sltu	t7, len, 4*NBYTES # inst duplicated in delay slot of branch
cleanup_both_aligned:
	beqz	len, done
	 and	rem, len, ADDRMASK   # rem = len not multiple of NBYTES
	## len > 0; must be safe to read NBYTES
	bnez	t7, less_than_4units
	 SRL	count, len, LOG_NBYTES # count = number of units left
	LOAD	t0, FIRST(0)(src)
	LOAD	t1, FIRST(1)(src)
	SUB	len, len, 4*NBYTES
	LOAD	t2, FIRST(2)(src)
	SUB	count, count, 4	
	ADD	dst, dst, 4*NBYTES
	LOAD	t3, FIRST(3)(src)
	ADD	src, src, 4*NBYTES
	USE(	FIRST(-5)(dst2) )
	STORE	t0, FIRST(-8)(dst2)  # PASS1: >= 3 cycles after load
	STORE	t1, FIRST(-7)(dst2)
	STORE	t2, FIRST(-6)(dst2)
	beqz	len, done
	 STORE	t3, FIRST(-5)(dst2)
less_than_4units:
	beqz	count, copy_bytes_aligned # rem > 0
	 nop
	## count is number of full units left
	## rem is len & ADDRMASK
copy_units_aligned:	
	LOAD	t0, FIRST(0)(src)
	ADD	src, src, NBYTES
	SUB	count, count, 1
	USE(	FIRST(0)(dst) )
	STORE	t0, FIRST(0)(dst)
	bnez	count, copy_units_aligned
	 ADD	dst, dst, NBYTES
	
	## src and dst are aligned; need to copy rem bytes
	## Do an explicit read/mask/or/write (eliminates branches)
#define mask count
#define mask2 len
	beqz	rem, done
copy_bytes_aligned:	
	 SUB	mask, zero, 1
	SLL	rem, rem, 3          # rem = number of bits to copy
	LOAD	t0, FIRST(0)(src)
	LOAD	t1, FIRST(0)(dst)
	## clear bits in mask where data should be copied
	SHIFT_CPY mask, mask, rem
	nor	mask2, mask, zero
	and	t1, mask, t1	     # t1 = dst bytes to keep
	and	t0, mask2, t0        # t0 = src bytes to copy
	or	t0, t0, t1
	USE(	FIRST(0)(dst) )
	jr	ra
	 STORE	t0, FIRST(0)(dst)
	
dst_unaligned:
	## dst is unaligned
	## t0 = src & ADDRMASK
	## t1 = dst & ADDRMASK; T1 > 0
	## len >= NBYTES
	
	## Copy enough bytes to align dst
	## Set match = (src and dst have same alignment)
	LDFIRST	t3,FIRST(0)(src)
	ADD	t2, zero, NBYTES
	LDREST	t3,REST(0)(src)
	SUB	t2, t2, t1	# t2 = number of bytes copied
	xor	match, t0, t1
	SUB	len, len, t2
	USE(	FIRST(0)(dst) )
	beqz	len, done
	 STFIRST t3,FIRST(0)(dst)
	
	ADD	dst, dst, t2
	beqz	match, both_aligned
	 ADD	src, src, t2
	
src_unaligned_dst_aligned:

	## Set count = number of iters for main loop
	## Update len, src, and dst for number of bytes copied
	SRL	count, len, LOG_NBYTES+2  # +2 for 4 units/iter
	pref	0, 3*32(src)
	beqz	count, cleanup_src_unaligned
	 and	rem, len, ADDRMASK   # rem = len % NBYTES
	pref	1, 3*32(dst)

	USE(	DEST(0)(dst) )
	.align 4
1:
	## issue break
	LDFIRST	t0, FIRST(0)(src)
	LDFIRST	t1, FIRST(1)(src)
	SUB     len, len, 4*NBYTES # hoist out of loop?
	## issue break
	LDREST	t0, REST(0)(src)
	LDREST	t1, REST(1)(src)
	SUB	count, count, 1
	## issue break
	LDFIRST	t2, FIRST(2)(src)
	LDFIRST	t3, FIRST(3)(src)
	## issue break
	LDREST	t2, REST(2)(src)
	LDREST	t3, REST(3)(src)
	## issue break
	pref	0, 7*32(src)	   # 0 is PREF_LOAD  (not streamed)
	ADD	src, src, 4*NBYTES # long pipe
	pref	1, 7*32(dst)       # 1 is PREF_STORE (not streamed)
	## issue break
	USE(	DEST(3)(dst) )
	STORE	t0, DEST(0)(dst) # must be 3 cycles after LDREST
	STORE	t1, DEST(1)(dst)
	## issue break
	STORE	t2, DEST(2)(dst) # must be 3 cycles after LDREST
	STORE	t3, DEST(3)(dst)
	## issue break
	bnez	count, 1b
	 ADD	dst, dst, 4*NBYTES # long pipe?

cleanup_src_unaligned:
	beqz	len, done
	 SRL	count, len, LOG_NBYTES # count = number of units left
	beqz	count, copy_bytes   # if this branch fall

copy_units_src_unaligned:
	 LDFIRST	t0, FIRST(0)(src)
	LDREST	t0, REST(0)(src)
	ADD	src, src, NBYTES
	SUB	count, count, 1
	USE(	FIRST(0)(dst) )
	STORE	t0, FIRST(0)(dst)
	ADD	dst, dst, NBYTES
	bnez	count, copy_units_src_unaligned
	 SUB	len, len, NBYTES

copy_bytes_checklen:
	beqz	len, done
copy_bytes:
	 SUB	len, len, 1
	USE(	0(dst) )
	USE(	NBYTES-2(dst) )
	lb	t0, 0(src)
	beqz	len, done
	 sb	t0, 0(dst)
#define COPY_BYTE(N) \
	lb	t0, N(src); \
	SUB	len, len, 1; \
	beqz	len, done; \
	 sb	t0, N(dst);
	COPY_BYTE(1)
	COPY_BYTE(2)
#ifdef __long64
	COPY_BYTE(3)
	COPY_BYTE(4)
	COPY_BYTE(5)
	COPY_BYTE(6)
#endif
	lb	t0, NBYTES-1(src)
	jr	ra
	 sb	t0, NBYTES-1(dst)
done:
	jr	ra
	 nop
	.end	memcpy_sb1

### The danger of using streaming prefetches is that one stream
### may evict the other before the cpu consumes it.
