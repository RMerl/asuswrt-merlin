#ifndef _IP6T_MARK_H
#define _IP6T_MARK_H

struct ip6t_mark_info {
#ifdef KERNEL_64_USERSPACE_32
    unsigned long long mark, mask;
#else
    unsigned long mark, mask;
#endif
    u_int8_t invert;
};

#endif /*_IPT_MARK_H*/
