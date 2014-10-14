#ifndef _SPARC64_FUTEX_H
#define _SPARC64_FUTEX_H

#include <linux/futex.h>
#include <linux/uaccess.h>
#include <asm/errno.h>
#include <asm/system.h>

#define __futex_cas_op(insn, ret, oldval, uaddr, oparg)	\
	__asm__ __volatile__(				\
	"\n1:	lduwa	[%3] %%asi, %2\n"		\
	"	" insn "\n"				\
	"2:	casa	[%3] %%asi, %2, %1\n"		\
	"	cmp	%2, %1\n"			\
	"	bne,pn	%%icc, 1b\n"			\
	"	 mov	0, %0\n"			\
	"3:\n"						\
	"	.section .fixup,#alloc,#execinstr\n"	\
	"	.align	4\n"				\
	"4:	sethi	%%hi(3b), %0\n"			\
	"	jmpl	%0 + %%lo(3b), %%g0\n"		\
	"	 mov	%5, %0\n"			\
	"	.previous\n"				\
	"	.section __ex_table,\"a\"\n"		\
	"	.align	4\n"				\
	"	.word	1b, 4b\n"			\
	"	.word	2b, 4b\n"			\
	"	.previous\n"				\
	: "=&r" (ret), "=&r" (oldval), "=&r" (tem)	\
	: "r" (uaddr), "r" (oparg), "i" (-EFAULT)	\
	: "memory")

static inline int futex_atomic_op_inuser(int encoded_op, u32 __user *uaddr)
{
	int op = (encoded_op >> 28) & 7;
	int cmp = (encoded_op >> 24) & 15;
	int oparg = (encoded_op << 8) >> 20;
	int cmparg = (encoded_op << 20) >> 20;
	int oldval = 0, ret, tem;

	if (unlikely(!access_ok(VERIFY_WRITE, uaddr, sizeof(u32))))
		return -EFAULT;
	if (unlikely((((unsigned long) uaddr) & 0x3UL)))
		return -EINVAL;

	if (encoded_op & (FUTEX_OP_OPARG_SHIFT << 28))
		oparg = 1 << oparg;

	pagefault_disable();

	switch (op) {
	case FUTEX_OP_SET:
		__futex_cas_op("mov\t%4, %1", ret, oldval, uaddr, oparg);
		break;
	case FUTEX_OP_ADD:
		__futex_cas_op("add\t%2, %4, %1", ret, oldval, uaddr, oparg);
		break;
	case FUTEX_OP_OR:
		__futex_cas_op("or\t%2, %4, %1", ret, oldval, uaddr, oparg);
		break;
	case FUTEX_OP_ANDN:
		__futex_cas_op("andn\t%2, %4, %1", ret, oldval, uaddr, oparg);
		break;
	case FUTEX_OP_XOR:
		__futex_cas_op("xor\t%2, %4, %1", ret, oldval, uaddr, oparg);
		break;
	default:
		ret = -ENOSYS;
	}

	pagefault_enable();

	if (!ret) {
		switch (cmp) {
		case FUTEX_OP_CMP_EQ: ret = (oldval == cmparg); break;
		case FUTEX_OP_CMP_NE: ret = (oldval != cmparg); break;
		case FUTEX_OP_CMP_LT: ret = (oldval < cmparg); break;
		case FUTEX_OP_CMP_GE: ret = (oldval >= cmparg); break;
		case FUTEX_OP_CMP_LE: ret = (oldval <= cmparg); break;
		case FUTEX_OP_CMP_GT: ret = (oldval > cmparg); break;
		default: ret = -ENOSYS;
		}
	}
	return ret;
}

static inline int
futex_atomic_cmpxchg_inatomic(u32 *uval, u32 __user *uaddr,
			      u32 oldval, u32 newval)
{
	int ret = 0;

	__asm__ __volatile__(
	"\n1:	casa	[%4] %%asi, %3, %1\n"
	"2:\n"
	"	.section .fixup,#alloc,#execinstr\n"
	"	.align	4\n"
	"3:	sethi	%%hi(2b), %0\n"
	"	jmpl	%0 + %%lo(2b), %%g0\n"
	"	mov	%5, %0\n"
	"	.previous\n"
	"	.section __ex_table,\"a\"\n"
	"	.align	4\n"
	"	.word	1b, 3b\n"
	"	.previous\n"
	: "+r" (ret), "=r" (newval)
	: "1" (newval), "r" (oldval), "r" (uaddr), "i" (-EFAULT)
	: "memory");

	*uval = newval;
	return ret;
}

#endif /* !(_SPARC64_FUTEX_H) */
