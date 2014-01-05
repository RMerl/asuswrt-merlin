/*
 * addrinfo.c - converts names to network addresses
 * $Id: addrinfo.c 646 2010-05-01 07:49:06Z remi $
 */

/*************************************************************************
 *  Copyright © 2002-2009 Rémi Denis-Courmont.                           *
 *  This program is free software: you can redistribute and/or modify    *
 *  it under the terms of the GNU General Public License as published by *
 *  the Free Software Foundation, versions 2 or 3 of the license.        *
 *                                                                       *
 *  This program is distributed in the hope that it will be useful,      *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 *  GNU General Public License for more details.                         *
 *                                                                       *
 *  You should have received a copy of the GNU General Public License    *
 *  along with this program. If not, see <http://www.gnu.org/licenses/>. *
 *************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <gettext.h>

#include <stdio.h>
#include <string.h> /* strchr() */
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <locale.h>
#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#ifndef AI_IDN
# define AI_IDN 0
#endif

static void
gai_perror (int errval, const char *msg)
{
	if (errval == EAI_SYSTEM)
		perror (msg);
	else
		fprintf (stderr, "%s: %s\n", msg, gai_strerror (errval));
}


static int
printnames (const char *name, int family, int aflags, int nflags, bool single)
{
	struct addrinfo hints, *res;

	memset (&hints, 0, sizeof (hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = aflags;

	int check = getaddrinfo (name, NULL, &hints, &res);
	if (check)
	{
		gai_perror (check, name);
		return -1;
	}

	for (struct addrinfo *ptr = res; ptr != NULL; ptr = ptr->ai_next)
	{
		char hostname[NI_MAXHOST];

		check = getnameinfo (ptr->ai_addr, ptr->ai_addrlen,
		                     hostname, sizeof (hostname),
		                     NULL, 0, nflags);

		if (check)
			gai_perror (check, name);
		else
			fputs (hostname, stdout);

		if (single)
			break;

		if (ptr->ai_next != NULL)
			fputc (' ', stdout);
	}

	fputc ('\n', stdout);
	freeaddrinfo (res);
	return 0;
}


static int
printnamesf (FILE *input, int family, int aflags, int nflags, bool single)
{
	do
	{
		char buf[NI_MAXHOST + 1], *ptr;

		if (fgets (buf, sizeof (buf), input) != NULL)
		{
			ptr = strchr (buf, '\n');
			if (ptr != NULL)
				*ptr = '\0';

			printnames (buf, family, aflags, nflags, single);
		}
	}
	while (!ferror (input) && !feof (input));

	if (ferror (input))
	{
		perror (_("Input error"));
		return -1;
	}

	return 0;
}


static int usage (const char *path)
{
	printf (_(
"Usage: %s [-4|-6] [hostnames]\n"
"Converts names to addresses.\n"
"\n"
"  -4, --ipv4     only lookup IPv4 addresses\n"
"  -6, --ipv6     only lookup IPv6 addresses\n"
"  -c, --config   only return addresses for locally configured protocols\n"
"  -h, --help     display this help and exit\n"
"  -m, --multiple print multiple results separated by spaces\n"
"  -n, --numeric  do not perform forward hostname lookup\n"
"  -r, --reverse  perform reverse address to hostname lookup\n"
"  -V, --version  display program version and exit\n"), path);
	return 0;
}


static int quick_usage (const char *path)
{
	fprintf (stderr, _("Try \"%s -h\" for more information.\n"), path);
	return 2;
}


static int version (void)
{
        printf (_("addrinfo %s (%s)\n"), VERSION, "$Rev: 646 $");
	printf (_(" built %s on %s\n"), __DATE__, PACKAGE_BUILD_HOSTNAME);
        printf (_("Configured with: %s\n"), PACKAGE_CONFIGURE_INVOCATION);
        puts (_("Written by Remi Denis-Courmont\n"));

        printf (_("Copyright (C) %u-%u Remi Denis-Courmont\n"), 2002, 2007);
	puts (_("This is free software; see the source for copying conditions.\n"
	        "There is NO warranty; not even for MERCHANTABILITY or\n"
	        "FITNESS FOR A PARTICULAR PURPOSE.\n"));
        return 0;
}

static const struct option lopts[] =
{
	{ "ipv4",     no_argument,       NULL, '4' },
	{ "ipv6",     no_argument,       NULL, '6' },
	{ "config",   no_argument,       NULL, 'c' },
	{ "help",     no_argument,       NULL, 'h' },
	{ "multiple", no_argument,       NULL, 'm' },
	{ "numeric",  no_argument,       NULL, 'n' },
	{ "reverse",  no_argument,       NULL, 'r' },
	{ "version",  no_argument,       NULL, 'V' },
	{ NULL,       0,                 NULL, 0   }
};

static const char sopts[] = "46chmnrV";

int main (int argc, char *argv[])
{
	setlocale (LC_ALL, "");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);

	int val, family = AF_UNSPEC, aflags = AI_IDN, nflags = NI_NUMERICHOST;
	bool single = true;

	while ((val = getopt_long (argc, argv, sopts, lopts, NULL)) != EOF)
		switch (val)
		{
			case '4':
				family = AF_INET;
				break;

			case '6':
				family = AF_INET6;
				break;

			case 'c':
				aflags |= AI_ADDRCONFIG;
				break;

			case 'h':
				return usage (argv[0]);

			case 'm':
				single = false;
				break;

			case 'n':
				aflags |= AI_NUMERICHOST;
				break;

			case 'r':
				nflags &= ~NI_NUMERICHOST;
				break;

			case 'V':
				return version ();

			case '?':
			default:
				return quick_usage (argv[0]);
		}

	setvbuf (stdout, NULL, _IOLBF, 0);
	val = 0;
	if (optind < argc)
	{
		while (optind < argc)
			printnames (argv[optind++], family, aflags, nflags, single);
	}
	else
		val = printnamesf (stdin, family, aflags, nflags, single);

	return val;
}
