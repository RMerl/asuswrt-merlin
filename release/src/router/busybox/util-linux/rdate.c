/* vi: set sw=4 ts=4: */
/*
 * The Rdate command will ask a time server for the RFC 868 time
 *  and optionally set the system time.
 *
 * by Sterling Huxley <sterling@europa.com>
 *
 * Licensed under GPL v2 or later, see file License for details.
*/

#include "libbb.h"

enum { RFC_868_BIAS = 2208988800UL };

static void socket_timeout(int sig UNUSED_PARAM)
{
	bb_error_msg_and_die("timeout connecting to time server");
}

static time_t askremotedate(const char *host)
{
	uint32_t nett;
	int fd;

	/* Add a timeout for dead or inaccessible servers */
	alarm(10);
	signal(SIGALRM, socket_timeout);

	fd = create_and_connect_stream_or_die(host, bb_lookup_port("time", "tcp", 37));

	if (safe_read(fd, (void *)&nett, 4) != 4)    /* read time from server */
		bb_error_msg_and_die("%s did not send the complete time", host);
	close(fd);

	/* convert from network byte order to local byte order.
	 * RFC 868 time is the number of seconds
	 * since 00:00 (midnight) 1 January 1900 GMT
	 * the RFC 868 time 2,208,988,800 corresponds to 00:00  1 Jan 1970 GMT
	 * Subtract the RFC 868 time to get Linux epoch
	 */

	return ntohl(nett) - RFC_868_BIAS;
}

int rdate_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int rdate_main(int argc UNUSED_PARAM, char **argv)
{
	time_t remote_time;
	unsigned long flags;

	opt_complementary = "-1";
	flags = getopt32(argv, "sp");

	remote_time = askremotedate(argv[optind]);

	if ((flags & 2) == 0) {
		time_t current_time;

		time(&current_time);
		if (current_time == remote_time)
			bb_error_msg("current time matches remote time");
		else
			if (stime(&remote_time) < 0)
				bb_perror_msg_and_die("can't set time of day");
	}

	if ((flags & 1) == 0)
		printf("%s", ctime(&remote_time));

	return EXIT_SUCCESS;
}
