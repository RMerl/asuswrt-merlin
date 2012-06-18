#ifndef _IP6T_MARK_H_target
#define _IP6T_MARK_H_target

struct ip6t_mark_target_info {
#ifdef KERNEL_64_USERSPACE_32
	unsigned long long mark;
#else
	unsigned long mark;
#endif
};

#endif /*_IPT_MARK_H_target*/
