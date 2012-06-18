/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * create raw socket for icmp (IPv6 version) protocol
 * and drop root privileges if running setuid
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#if ENABLE_FEATURE_IPV6
int FAST_FUNC create_icmp6_socket(void)
{
	int sock;
#if 0
	struct protoent *proto;
	proto = getprotobyname("ipv6-icmp");
	/* if getprotobyname failed, just silently force
	 * proto->p_proto to have the correct value for "ipv6-icmp" */
	sock = socket(AF_INET6, SOCK_RAW,
			(proto ? proto->p_proto : IPPROTO_ICMPV6));
#else
	sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
#endif
	if (sock < 0) {
		if (errno == EPERM)
			bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);
		bb_perror_msg_and_die(bb_msg_can_not_create_raw_socket);
	}

	/* drop root privs if running setuid */
	xsetuid(getuid());

	return sock;
}
#endif
