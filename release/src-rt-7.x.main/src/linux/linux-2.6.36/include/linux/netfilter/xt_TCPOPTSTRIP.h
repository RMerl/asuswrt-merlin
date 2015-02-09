#ifndef _XT_TCPOPTSTRIP_H
#define _XT_TCPOPTSTRIP_H

#define tcpoptstrip_set_bit(bmap, idx) \
	(bmap[(idx) >> 5] |= 1U << (idx & 31))
#define tcpoptstrip_test_bit(bmap, idx) \
	(((1U << (idx & 31)) & bmap[(idx) >> 5]) != 0)

struct xt_tcpoptstrip_target_info {
	u_int32_t strip_bmap[8];
};

#endif /* _XT_TCPOPTSTRIP_H */
