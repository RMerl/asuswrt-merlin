#ifndef __ASM_MSR_H
#define __ASM_MSR_H

#include <asm/msr-index.h>

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

#include <asm/errno.h>

static inline unsigned long long native_read_msr(unsigned int msr)
{
	unsigned long long val;

	asm volatile("rdmsr" : "=A" (val) : "c" (msr));
	return val;
}

static inline unsigned long long native_read_msr_safe(unsigned int msr,
						      int *err)
{
	unsigned long long val;

	asm volatile("2: rdmsr ; xorl %0,%0\n"
		     "1:\n\t"
		     ".section .fixup,\"ax\"\n\t"
		     "3:  movl %3,%0 ; jmp 1b\n\t"
		     ".previous\n\t"
 		     ".section __ex_table,\"a\"\n"
		     "   .align 4\n\t"
		     "   .long 	2b,3b\n\t"
		     ".previous"
		     : "=r" (*err), "=A" (val)
		     : "c" (msr), "i" (-EFAULT));

	return val;
}

static inline void native_write_msr(unsigned int msr, unsigned long long val)
{
	asm volatile("wrmsr" : : "c" (msr), "A"(val));
}

static inline int native_write_msr_safe(unsigned int msr,
					unsigned long long val)
{
	int err;
	asm volatile("2: wrmsr ; xorl %0,%0\n"
		     "1:\n\t"
		     ".section .fixup,\"ax\"\n\t"
		     "3:  movl %4,%0 ; jmp 1b\n\t"
		     ".previous\n\t"
 		     ".section __ex_table,\"a\"\n"
		     "   .align 4\n\t"
		     "   .long 	2b,3b\n\t"
		     ".previous"
		     : "=a" (err)
		     : "c" (msr), "0" ((u32)val), "d" ((u32)(val>>32)),
		       "i" (-EFAULT));
	return err;
}

static inline unsigned long long native_read_tsc(void)
{
	unsigned long long val;
	asm volatile("rdtsc" : "=A" (val));
	return val;
}

static inline unsigned long long native_read_pmc(void)
{
	unsigned long long val;
	asm volatile("rdpmc" : "=A" (val));
	return val;
}

#ifdef CONFIG_PARAVIRT
#include <asm/paravirt.h>
#else
#include <linux/errno.h>
/*
 * Access to machine-specific registers (available on 586 and better only)
 * Note: the rd* operations modify the parameters directly (without using
 * pointer indirection), this allows gcc to optimize better
 */

#define rdmsr(msr,val1,val2)						\
	do {								\
		u64 __val = native_read_msr(msr);			\
		(val1) = (u32)__val;					\
		(val2) = (u32)(__val >> 32);				\
	} while(0)

static inline void wrmsr(u32 __msr, u32 __low, u32 __high)
{
	native_write_msr(__msr, ((u64)__high << 32) | __low);
}

#define rdmsrl(msr,val)							\
	((val) = native_read_msr(msr))

#define wrmsrl(msr,val)	native_write_msr(msr, val)

/* wrmsr with exception handling */
static inline int wrmsr_safe(u32 __msr, u32 __low, u32 __high)
{
	return native_write_msr_safe(__msr, ((u64)__high << 32) | __low);
}

/* rdmsr with exception handling */
#define rdmsr_safe(msr,p1,p2)						\
	({								\
		int __err;						\
		u64 __val = native_read_msr_safe(msr, &__err);		\
		(*p1) = (u32)__val;					\
		(*p2) = (u32)(__val >> 32);				\
		__err;							\
	})

#define rdtscl(low)						\
	((low) = (u32)native_read_tsc())

#define rdtscll(val)						\
	((val) = native_read_tsc())

#define write_tsc(val1,val2) wrmsr(0x10, val1, val2)

#define rdpmc(counter,low,high)					\
	do {							\
		u64 _l = native_read_pmc();			\
		(low)  = (u32)_l;				\
		(high) = (u32)(_l >> 32);			\
	} while(0)
#endif	/* !CONFIG_PARAVIRT */

#ifdef CONFIG_SMP
void rdmsr_on_cpu(unsigned int cpu, u32 msr_no, u32 *l, u32 *h);
void wrmsr_on_cpu(unsigned int cpu, u32 msr_no, u32 l, u32 h);
int rdmsr_safe_on_cpu(unsigned int cpu, u32 msr_no, u32 *l, u32 *h);
int wrmsr_safe_on_cpu(unsigned int cpu, u32 msr_no, u32 l, u32 h);
#else  /*  CONFIG_SMP  */
static inline void rdmsr_on_cpu(unsigned int cpu, u32 msr_no, u32 *l, u32 *h)
{
	rdmsr(msr_no, *l, *h);
}
static inline void wrmsr_on_cpu(unsigned int cpu, u32 msr_no, u32 l, u32 h)
{
	wrmsr(msr_no, l, h);
}
static inline int rdmsr_safe_on_cpu(unsigned int cpu, u32 msr_no, u32 *l, u32 *h)
{
	return rdmsr_safe(msr_no, l, h);
}
static inline int wrmsr_safe_on_cpu(unsigned int cpu, u32 msr_no, u32 l, u32 h)
{
	return wrmsr_safe(msr_no, l, h);
}
#endif  /*  CONFIG_SMP  */
#endif
#endif
#endif /* __ASM_MSR_H */
