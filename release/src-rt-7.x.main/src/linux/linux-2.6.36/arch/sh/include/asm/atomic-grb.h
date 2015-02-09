#ifndef __ASM_SH_ATOMIC_GRB_H
#define __ASM_SH_ATOMIC_GRB_H

static inline void atomic_add(int i, atomic_t *v)
{
	int tmp;

	__asm__ __volatile__ (
		"   .align 2              \n\t"
		"   mova    1f,   r0      \n\t" /* r0 = end point */
		"   mov    r15,   r1      \n\t" /* r1 = saved sp */
		"   mov    #-6,   r15     \n\t" /* LOGIN: r15 = size */
		"   mov.l  @%1,   %0      \n\t" /* load  old value */
		"   add     %2,   %0      \n\t" /* add */
		"   mov.l   %0,   @%1     \n\t" /* store new value */
		"1: mov     r1,   r15     \n\t" /* LOGOUT */
		: "=&r" (tmp),
		  "+r"  (v)
		: "r"   (i)
		: "memory" , "r0", "r1");
}

static inline void atomic_sub(int i, atomic_t *v)
{
	int tmp;

	__asm__ __volatile__ (
		"   .align 2              \n\t"
		"   mova    1f,   r0      \n\t" /* r0 = end point */
		"   mov     r15,  r1      \n\t" /* r1 = saved sp */
		"   mov    #-6,   r15     \n\t" /* LOGIN: r15 = size */
		"   mov.l  @%1,   %0      \n\t" /* load  old value */
		"   sub     %2,   %0      \n\t" /* sub */
		"   mov.l   %0,   @%1     \n\t" /* store new value */
		"1: mov     r1,   r15     \n\t" /* LOGOUT */
		: "=&r" (tmp),
		  "+r"  (v)
		: "r"   (i)
		: "memory" , "r0", "r1");
}

static inline int atomic_add_return(int i, atomic_t *v)
{
	int tmp;

	__asm__ __volatile__ (
		"   .align 2              \n\t"
		"   mova    1f,   r0      \n\t" /* r0 = end point */
		"   mov    r15,   r1      \n\t" /* r1 = saved sp */
		"   mov    #-6,   r15     \n\t" /* LOGIN: r15 = size */
		"   mov.l  @%1,   %0      \n\t" /* load  old value */
		"   add     %2,   %0      \n\t" /* add */
		"   mov.l   %0,   @%1     \n\t" /* store new value */
		"1: mov     r1,   r15     \n\t" /* LOGOUT */
		: "=&r" (tmp),
		  "+r"  (v)
		: "r"   (i)
		: "memory" , "r0", "r1");

	return tmp;
}

static inline int atomic_sub_return(int i, atomic_t *v)
{
	int tmp;

	__asm__ __volatile__ (
		"   .align 2              \n\t"
		"   mova    1f,   r0      \n\t" /* r0 = end point */
		"   mov    r15,   r1      \n\t" /* r1 = saved sp */
		"   mov    #-6,   r15     \n\t" /* LOGIN: r15 = size */
		"   mov.l  @%1,   %0      \n\t" /* load  old value */
		"   sub     %2,   %0      \n\t" /* sub */
		"   mov.l   %0,   @%1     \n\t" /* store new value */
		"1: mov     r1,   r15     \n\t" /* LOGOUT */
		: "=&r" (tmp),
		  "+r"  (v)
		: "r"   (i)
		: "memory", "r0", "r1");

	return tmp;
}

static inline void atomic_clear_mask(unsigned int mask, atomic_t *v)
{
	int tmp;
	unsigned int _mask = ~mask;

	__asm__ __volatile__ (
		"   .align 2              \n\t"
		"   mova    1f,   r0      \n\t" /* r0 = end point */
		"   mov    r15,   r1      \n\t" /* r1 = saved sp */
		"   mov    #-6,   r15     \n\t" /* LOGIN: r15 = size */
		"   mov.l  @%1,   %0      \n\t" /* load  old value */
		"   and     %2,   %0      \n\t" /* add */
		"   mov.l   %0,   @%1     \n\t" /* store new value */
		"1: mov     r1,   r15     \n\t" /* LOGOUT */
		: "=&r" (tmp),
		  "+r"  (v)
		: "r"   (_mask)
		: "memory" , "r0", "r1");
}

static inline void atomic_set_mask(unsigned int mask, atomic_t *v)
{
	int tmp;

	__asm__ __volatile__ (
		"   .align 2              \n\t"
		"   mova    1f,   r0      \n\t" /* r0 = end point */
		"   mov    r15,   r1      \n\t" /* r1 = saved sp */
		"   mov    #-6,   r15     \n\t" /* LOGIN: r15 = size */
		"   mov.l  @%1,   %0      \n\t" /* load  old value */
		"   or      %2,   %0      \n\t" /* or */
		"   mov.l   %0,   @%1     \n\t" /* store new value */
		"1: mov     r1,   r15     \n\t" /* LOGOUT */
		: "=&r" (tmp),
		  "+r"  (v)
		: "r"   (mask)
		: "memory" , "r0", "r1");
}

#endif /* __ASM_SH_ATOMIC_GRB_H */
