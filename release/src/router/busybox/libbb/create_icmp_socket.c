/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * create raw socket for icmp protocol
 * and drop root privileges if running setuid
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

int FAST_FUNC create_icmp_socket(void)
{
	int sock;
#if 0
	struct protoent *proto;
	proto = getprotobyname("icmp");
	/* if getprotobyname failed, just silently force
	 * proto->p_proto to have the correct value for "icmp" */
	sock = socket(AF_INET, SOCK_RAW,
			(proto ? proto->p_proto : 1)); /* 1 == ICMP */
#else
	sock = socket(AF_INET, SOCK_RAW, 1); /* 1 == ICMP */
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
