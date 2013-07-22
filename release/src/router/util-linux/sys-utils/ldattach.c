/* line discipline loading daemon
 * open a serial device and attach a line discipline on it
 *
 * Usage:
 *	ldattach GIGASET_M101 /dev/ttyS0
 *
 * =====================================================================
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 * =====================================================================
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#include "c.h"
#include "nls.h"

#define dbg(format, arg...) \
	do { if (debug) fprintf(stderr , "%s:" format "\n" , progname , ## arg); } while (0)

#ifndef N_GIGASET_M101
# define N_GIGASET_M101 16
#endif

#ifndef N_PPS
# define N_PPS 18
#endif

/* attach a line discipline ioctl */
#ifndef TIOCSETD
# define TIOCSETD   0x5423
#endif

static const char *progname;
static int debug = 0;

struct ld_table {
	const char	*name;
	int		value;
};

/* currently supported line disciplines, plus some aliases */
static const struct ld_table ld_discs[] =
{
	{ "TTY",	N_TTY },
	{ "SLIP",	N_SLIP },
	{ "MOUSE",	N_MOUSE },
	{ "PPP",	N_PPP },
	{ "STRIP",	N_STRIP },
	{ "AX25",	N_AX25 },
	{ "X25",	N_X25 },
	{ "6PACK",	N_6PACK },
	{ "R3964",	N_R3964 },
	{ "IRDA",	N_IRDA },
	{ "HDLC",	N_HDLC },
	{ "SYNC_PPP",	N_SYNC_PPP },
	{ "SYNCPPP",	N_SYNC_PPP },
	{ "HCI",	N_HCI },
	{ "GIGASET_M101",	N_GIGASET_M101 },
	{ "GIGASET",	N_GIGASET_M101 },
	{ "M101",	N_GIGASET_M101 },
	{ "PPS",	N_PPS },
	{ NULL, 0 }
};

/* known c_iflag names */
static const struct ld_table ld_iflags[] =
{
	{ "IGNBRK",	IGNBRK },
	{ "BRKINT",	BRKINT },
	{ "IGNPAR",	IGNPAR },
	{ "PARMRK",	PARMRK },
	{ "INPCK",	INPCK },
	{ "ISTRIP",	ISTRIP },
	{ "INLCR",	INLCR },
	{ "IGNCR",	IGNCR },
	{ "ICRNL",	ICRNL },
	{ "IUCLC",	IUCLC },
	{ "IXON",	IXON },
	{ "IXANY",	IXANY },
	{ "IXOFF",	IXOFF },
	{ "IMAXBEL",	IMAXBEL },
	{ "IUTF8",	IUTF8 },
	{ NULL, 0 }
};

static int lookup_table(const struct ld_table *tab, const char *str)
{
    const struct ld_table *t;

    for (t = tab; t && t->name; t++)
	if (!strcasecmp(t->name, str))
	    return t->value;
    return -1;
}

static void print_table(FILE *out, const struct ld_table *tab)
{
    const struct ld_table *t;
    int i;

    for (t = tab, i = 1; t && t->name; t++, i++) {
	fprintf(out, "  %-10s", t->name);
	if (!(i % 3))
		fputc('\n', out);
    }
}

static int parse_iflag(char *str, int *set_iflag, int *clr_iflag)
{
    int iflag;
    char *s, *end;

    for (s = strtok(str, ","); s != NULL; s = strtok(NULL, ",")) {
	if (*s == '-')
	    s++;
        if ((iflag = lookup_table(ld_iflags, s)) < 0) {
	    iflag = strtol(s, &end, 0);
	    if (*end || iflag < 0)
	        errx(EXIT_FAILURE, _("invalid iflag: %s"), s);
	}
	if (s > str && *(s - 1) == '-')
	    *clr_iflag |= iflag;
	else
	    *set_iflag |= iflag;
    }

    dbg("iflag (set/clear): %d/%d", *set_iflag, *clr_iflag);
    return 0;
}


static void __attribute__((__noreturn__)) usage(int exitcode)
{
    FILE *out = exitcode == EXIT_SUCCESS ? stdout : stderr;

    fputs(_("\nUsage:\n"), out);
    fprintf(out,
	    _(" %s [ -dhV78neo12 ] [ -s <speed> ] [ -i [-]<iflag> ] <ldisc> <device>\n"),
	    progname);

    fputs(_("\nKnown <ldisc> names:\n"), out);
    print_table(out, ld_discs);

    fputs(_("\nKnown <iflag> names:\n"), out);
    print_table(out, ld_iflags);
    fputc('\n', out);
    exit(exitcode);
}

static int my_cfsetspeed(struct termios *ts, int speed)
{
	/* Standard speeds
	 * -- cfsetspeed() is able to translate number to Bxxx constants
	 */
	if (cfsetspeed(ts, speed) == 0)
		return 0;

	/* Nonstandard speeds
	 * -- we have to bypass glibc and set the speed manually (because
	 *    glibc checks for speed and supports Bxxx bit rates only)...
	 */
#ifdef _HAVE_STRUCT_TERMIOS_C_ISPEED
# define BOTHER 0010000               /* non standard rate */
	dbg("using non-standard speeds");
	ts->c_ospeed = ts->c_ispeed = speed;
	ts->c_cflag &= ~CBAUD;
	ts->c_cflag |= BOTHER;
	return 0;
#else
	return -1;
#endif
}

int main(int argc, char **argv)
{
    int tty_fd;
    struct termios ts;
    int speed = 0, bits = '-', parity = '-', stop = '-';
    int set_iflag = 0, clr_iflag = 0;
    int ldisc;
    int optc;
    char *end;
    char *dev;
    static const struct option opttbl[] = {
	{"speed", 1, 0, 's'},
	{"sevenbits", 0, 0, '7'},
	{"eightbits", 0, 0, '8'},
	{"noparity", 0, 0, 'n'},
	{"evenparity", 0, 0, 'e'},
	{"oddparity", 0, 0, 'o'},
	{"onestopbit", 0, 0, '1'},
	{"twostopbits", 0, 0, '2'},
	{"iflag", 1, 0, 'i'},
	{"help", 0, 0, 'h'},
	{"version", 0, 0, 'V'},
	{"debug", 0, 0, 'd'},
	{0, 0, 0, 0}
    };

    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    /* parse options */
    progname = program_invocation_short_name;

    if (argc == 0)
	usage(EXIT_SUCCESS);
    while ((optc = getopt_long(argc, argv, "dhV78neo12s:i:", opttbl, NULL)) >= 0) {
	switch (optc) {
	case 'd':
	    debug++;
	    break;
	case '1':
	case '2':
	    stop = optc;
	    break;
	case '7':
	case '8':
	    bits = optc;
	    break;
	case 'n':
	case 'e':
	case 'o':
	    parity = optc;
	    break;
	case 's':
	    speed = strtol(optarg, &end, 10);
	    if (*end || speed <= 0)
		errx(EXIT_FAILURE, _("invalid speed: %s"), optarg);
	    break;
	case 'i':
	    parse_iflag(optarg, &set_iflag, &clr_iflag);
	    break;
	case 'V':
	    printf(_("ldattach from %s\n"), PACKAGE_STRING);
	    break;
	case 'h':
	    usage(EXIT_SUCCESS);
	default:
	    warnx(_("invalid option"));
	    usage(EXIT_FAILURE);
	}
    }

    if (argc - optind != 2)
	usage(EXIT_FAILURE);

    /* parse line discipline specification */
    ldisc = lookup_table(ld_discs, argv[optind]);
    if (ldisc < 0) {
	ldisc = strtol(argv[optind], &end, 0);
	if (*end || ldisc < 0)
	    errx(EXIT_FAILURE, _("invalid line discipline: %s"), argv[optind]);
    }

    /* open device */
    dev = argv[optind+1];
    if ((tty_fd = open(dev, O_RDWR|O_NOCTTY)) < 0)
	err(EXIT_FAILURE, _("cannot open %s"), dev);
    if (!isatty(tty_fd))
	errx(EXIT_FAILURE, _("%s is not a serial line"), dev);

    dbg("opened %s", dev);

    /* set line speed and format */
    if (tcgetattr(tty_fd, &ts) < 0)
	err(EXIT_FAILURE, _("cannot get terminal attributes for %s"), dev);
    cfmakeraw(&ts);
    if (speed && my_cfsetspeed(&ts, speed) < 0)
	errx(EXIT_FAILURE, _("speed %d unsupported"), speed);
    switch (stop) {
    case '1':
	ts.c_cflag &= ~CSTOPB;
	break;
    case '2':
	ts.c_cflag |= CSTOPB;
	break;
    }
    switch (bits) {
    case '7':
	ts.c_cflag = (ts.c_cflag & ~CSIZE) | CS7;
	break;
    case '8':
	ts.c_cflag = (ts.c_cflag & ~CSIZE) | CS8;
	break;
    }
    switch (parity) {
    case 'n':
	ts.c_cflag &= ~(PARENB|PARODD);
	break;
    case 'e':
	ts.c_cflag |= PARENB;
	ts.c_cflag &= ~PARODD;
	break;
    case 'o':
	ts.c_cflag |= (PARENB|PARODD);
	break;
    }
    ts.c_cflag |= CREAD;	/* just to be on the safe side */

    ts.c_iflag |= set_iflag;
    ts.c_iflag &= ~clr_iflag;

    if (tcsetattr(tty_fd, TCSAFLUSH, &ts) < 0)
	err(EXIT_FAILURE, _("cannot set terminal attributes for %s"), dev);

    dbg("set to raw %d %c%c%c: cflag=0x%x",
	speed, bits, parity, stop, ts.c_cflag);

    /* Attach the line discpline. */
    if (ioctl(tty_fd, TIOCSETD, &ldisc) < 0)
	err(EXIT_FAILURE, _("cannot set line discipline"));

    dbg("line discipline set to %d", ldisc);

    /* Go into background if not in debug mode. */
    if (!debug && daemon(0, 0) < 0)
	err(EXIT_FAILURE, _("cannot daemonize"));

    /* Sleep to keep the line discipline active. */
    pause();

    exit(EXIT_SUCCESS);
}
