#ifndef LIBIPSET_NFPROTO_H
#define LIBIPSET_NFPROTO_H

/*
 * The constants to select, same as in linux/netfilter.h.
 * Like nf_inet_addr.h, this is just here so that we need not to rely on
 * the presence of a recent-enough netfilter.h.
 */
enum {
	NFPROTO_UNSPEC =  0,
	NFPROTO_IPV4   =  2,
	NFPROTO_ARP    =  3,
	NFPROTO_BRIDGE =  7,
	NFPROTO_IPV6   = 10,
	NFPROTO_DECNET = 12,
	NFPROTO_NUMPROTO,
};

#endif /* LIBIPSET_NFPROTO_H */
