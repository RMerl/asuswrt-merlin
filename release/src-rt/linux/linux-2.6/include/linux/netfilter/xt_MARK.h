#ifndef _XT_MARK_H_target
#define _XT_MARK_H_target

/* Version 0 */
struct xt_mark_target_info {
#ifdef KERNEL_64_USERSPACE_32
	unsigned long long mark, mask;
#else
	unsigned long mark, mask;
#endif
};


/* Version 1 */
enum {
	XT_MARK_SET=0,
	XT_MARK_AND,
	XT_MARK_OR,
};

struct xt_mark_target_info_v1 {
#ifdef KERNEL_64_USERSPACE_32
	unsigned long long mark, mask;
#else
	unsigned long mark, mask;
#endif
	u_int8_t mode;
};

#endif /*_XT_MARK_H_target */
