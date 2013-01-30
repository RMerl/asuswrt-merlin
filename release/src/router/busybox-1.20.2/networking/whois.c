/* vi: set sw=4 ts=4: */
/*
 * whois - tiny client for the whois directory service
 *
 * Copyright (c) 2011 Pere Orga <gotrunks@gmail.com>
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
/* TODO
 * Add ipv6 support
 * Add proxy support
 */

//config:config WHOIS
//config:	bool "whois"
//config:	default y
//config:	help
//config:	  whois is a client for the whois directory service

//applet:IF_WHOIS(APPLET(whois, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_WHOIS) += whois.o

//usage:#define whois_trivial_usage
//usage:       "[-h SERVER] [-p PORT] NAME..."
//usage:#define whois_full_usage "\n\n"
//usage:       "Query WHOIS info about NAME\n"
//usage:     "\n	-h,-p	Server to query"

#include "libbb.h"

static void pipe_out(int fd)
{
	FILE *fp;
	char buf[1024];

	fp = xfdopen_for_read(fd);
	while (fgets(buf, sizeof(buf), fp)) {
		char *p = strpbrk(buf, "\r\n");
		if (p)
			*p = '\0';
		puts(buf);
	}

	fclose(fp); /* closes fd too */
}

int whois_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int whois_main(int argc UNUSED_PARAM, char **argv)
{
	int port = 43;
	const char *host = "whois-servers.net";

	opt_complementary = "-1:p+";
	getopt32(argv, "h:p:", &host, &port);

	argv += optind;
	do {
		int fd = create_and_connect_stream_or_die(host, port);
		fdprintf(fd, "%s\r\n", *argv);
		pipe_out(fd);
	}
	while (*++argv);

	return EXIT_SUCCESS;
}
