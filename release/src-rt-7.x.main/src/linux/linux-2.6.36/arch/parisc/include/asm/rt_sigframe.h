#ifndef _ASM_PARISC_RT_SIGFRAME_H
#define _ASM_PARISC_RT_SIGFRAME_H

#define SIGRETURN_TRAMP 4
#define SIGRESTARTBLOCK_TRAMP 5 
#define TRAMP_SIZE (SIGRETURN_TRAMP + SIGRESTARTBLOCK_TRAMP)

struct rt_sigframe {
	unsigned int tramp[TRAMP_SIZE];
	struct siginfo info;
	struct ucontext uc;
};

#define	SIGFRAME		128
#define FUNCTIONCALLFRAME	96
#define PARISC_RT_SIGFRAME_SIZE					\
	(((sizeof(struct rt_sigframe) + FUNCTIONCALLFRAME) + SIGFRAME) & -SIGFRAME)

#endif
