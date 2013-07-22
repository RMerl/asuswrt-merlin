/*
 * Berkeley last for Linux. Currently maintained by poe@daimi.aau.dk at
 * ftp://ftp.daimi.aau.dk/pub/linux/poe/admutil*
 *
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

 /* 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
  * - added Native Language Support
  */

 /* 2001-02-14 Marek Zelem <marek@fornax.sk>
  * - using mmap() on Linux - great speed improvement
  */

/*
 * last
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <utmp.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "pathnames.h"
#include "nls.h"
#include "xalloc.h"
#include "c.h"

#define	SECDAY	(24*60*60)			/* seconds in a day */
#define	NO	0				/* false/no */
#define	YES	1				/* true/yes */

static struct utmp	utmpbuf;

#define	HMAX	(int)sizeof(utmpbuf.ut_host)	/* size of utmp host field */
#define	LMAX	(int)sizeof(utmpbuf.ut_line)	/* size of utmp tty field */
#define	NMAX	(int)sizeof(utmpbuf.ut_name)	/* size of utmp name field */

#ifndef MIN
#define MIN(a,b)	(((a) < (b)) ? (a) : (b))
#endif

/* maximum sizes used for printing */
/* probably we want a two-pass version that computes the right length */
int hmax = MIN(HMAX, 16);
int lmax = MIN(LMAX, 8);
int nmax = MIN(NMAX, 16);

typedef struct arg {
	char	*name;				/* argument */
#define	HOST_TYPE	-2
#define	TTY_TYPE	-3
#define	USER_TYPE	-4
#define INET_TYPE	-5
	int	type;				/* type of arg */
	struct arg	*next;			/* linked list pointer */
} ARG;
ARG	*arglist;				/* head of linked list */

typedef struct ttytab {
	long	logout;				/* log out time */
	char	tty[LMAX + 1];			/* terminal name */
	struct ttytab	*next;			/* linked list pointer */
} TTY;
TTY	*ttylist;				/* head of linked list */

static long	currentout,			/* current logout value */
		maxrec;				/* records to display */
static char	*file = _PATH_WTMP;		/* wtmp file */

static int	doyear = 0;			/* output year in dates */
static int	dolong = 0;			/* print also ip-addr */

static void wtmp(void);
static void addarg(int, char *);
static void hostconv(char *);
static void onintr(int);
static int want(struct utmp *, int);
TTY *addtty(char *);
static char *ttyconv(char *);

int
main(int argc, char **argv) {
	extern int	optind;
	extern char	*optarg;
	int	ch;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((ch = getopt(argc, argv, "0123456789yli:f:h:t:")) != -1)
		switch((char)ch) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/*
			 * kludge: last was originally designed to take
			 * a number after a dash.
			 */
			if (!maxrec)
				maxrec = atol(argv[optind - 1] + 1);
			break;
		case 'f':
			file = optarg;
			break;
		case 'h':
			hostconv(optarg);
			addarg(HOST_TYPE, optarg);
			break;
		case 't':
			addarg(TTY_TYPE, ttyconv(optarg));
			break;
		case 'y':
			doyear = 1;
			break;
		case 'l':
			dolong = 1;
			break;
		case 'i':
			addarg(INET_TYPE, optarg);
			break;
		case '?':
		default:
			fputs(_("usage: last [-#] [-f file] [-t tty] [-h hostname] [user ...]\n"), stderr);
			exit(EXIT_FAILURE);
		}
	for (argv += optind; *argv; ++argv) {
#define	COMPATIBILITY
#ifdef	COMPATIBILITY
		/* code to allow "last p5" to work */
		addarg(TTY_TYPE, ttyconv(*argv));
#endif
		addarg(USER_TYPE, *argv);
	}
	wtmp();

	return EXIT_SUCCESS;
}

static char *utmp_ctime(struct utmp *u)
{
	time_t t = (time_t) u->ut_time;
	return ctime(&t);
}

/*
 * print_partial_line --
 *	print the first part of each output line according to specified format
 */
static void
print_partial_line(struct utmp *bp) {
    char *ct;

    ct = utmp_ctime(bp);
    printf("%-*.*s  %-*.*s ", nmax, nmax, bp->ut_name, 
	   lmax, lmax, bp->ut_line);

    if (dolong) {
	if (bp->ut_addr) {
	    struct in_addr foo;
	    foo.s_addr = bp->ut_addr;
	    printf("%-*.*s ", hmax, hmax, inet_ntoa(foo));
	} else {
	    printf("%-*.*s ", hmax, hmax, "");
	}
    } else {
	printf("%-*.*s ", hmax, hmax, bp->ut_host);
    }

    if (doyear) {
	printf("%10.10s %4.4s %5.5s ", ct, ct + 20, ct + 11);
    } else {
	printf("%10.10s %5.5s ", ct, ct + 11);
    }
}

/*
 * wtmp --
 *	read through the wtmp file
 */
static void
wtmp(void) {
	register struct utmp	*bp;		/* current structure */
	register TTY	*T;			/* tty list entry */
	long	delta;				/* time difference */
	char *crmsg = NULL;
	char *ct = NULL;
	int fd;
	struct utmp *utl;
	struct stat st;
	int utl_len;
	int listnr = 0;
	int i;

	utmpname(file);

	{
#if defined(_HAVE_UT_TV)
		struct timeval tv;
		gettimeofday(&tv, NULL);
		utmpbuf.ut_tv.tv_sec = tv.tv_sec;
		utmpbuf.ut_tv.tv_usec = tv.tv_usec;
#else
		time_t t;
		time(&t);
		utmpbuf.ut_time = t;
#endif
	}

	(void)signal(SIGINT, onintr);
	(void)signal(SIGQUIT, onintr);

	if ((fd = open(file,O_RDONLY)) < 0)
		err(EXIT_FAILURE, _("%s: open failed"), file);

	fstat(fd, &st);
	utl_len = st.st_size;
	utl = mmap(NULL, utl_len, PROT_READ|PROT_WRITE,
		   MAP_PRIVATE|MAP_FILE, fd, 0);
	if (utl == NULL)
		err(EXIT_FAILURE, _("%s: mmap failed"), file);

	listnr = utl_len/sizeof(struct utmp);

	if(listnr) 
		ct = utmp_ctime(&utl[0]);

	for(i = listnr - 1; i >= 0; i--) {
		bp = utl+i;
		/*
		 * if the terminal line is '~', the machine stopped.
		 * see utmp(5) for more info.
		 */
		if (!strncmp(bp->ut_line, "~", LMAX)) {
		    /* 
		     * utmp(5) also mentions that the user 
		     * name should be 'shutdown' or 'reboot'.
		     * Not checking the name causes e.g. runlevel
		     * changes to be displayed as 'crash'. -thaele
		     */
		    if (!strncmp(bp->ut_user, "reboot", NMAX) ||
			!strncmp(bp->ut_user, "shutdown", NMAX)) {	
			/* everybody just logged out */
			for (T = ttylist; T; T = T->next)
			    T->logout = -bp->ut_time;
		    }

		    currentout = -bp->ut_time;
		    crmsg = (strncmp(bp->ut_name, "shutdown", NMAX)
			    ? "crash" : "down ");
		    if (!bp->ut_name[0])
			(void)strcpy(bp->ut_name, "reboot");
		    if (want(bp, NO)) {
			ct = utmp_ctime(bp);
			if(bp->ut_type != LOGIN_PROCESS) {
			    print_partial_line(bp);
			    putchar('\n');
			}
			if (maxrec && !--maxrec)
			    return;
		    }
		    continue;
		}
		/* find associated tty */
		for (T = ttylist;; T = T->next) {
		    if (!T) {
			/* add new one */
			T = addtty(bp->ut_line);
			break;
		    }
		    if (!strncmp(T->tty, bp->ut_line, LMAX))
			break;
		}
		if (bp->ut_name[0] && bp->ut_type != LOGIN_PROCESS
		    && bp->ut_type != DEAD_PROCESS
		    && want(bp, YES)) {

		    print_partial_line(bp);

		    if (!T->logout)
			puts(_("  still logged in"));
		    else {
			if (T->logout < 0) {
			    T->logout = -T->logout;
			    printf("- %s", crmsg);
			}
			else
			    printf("- %5.5s", ctime(&T->logout)+11);
			delta = T->logout - bp->ut_time;
			if (delta < SECDAY)
			    printf("  (%5.5s)\n", asctime(gmtime(&delta))+11);
			else
			    printf(" (%ld+%5.5s)\n", delta / SECDAY, asctime(gmtime(&delta))+11);
		    }
		    if (maxrec != -1 && !--maxrec)
			return;
		}
		T->logout = bp->ut_time;
		utmpbuf.ut_time = bp->ut_time;
	}
	munmap(utl,utl_len);
	close(fd);
	if(ct) printf(_("\nwtmp begins %s"), ct); 	/* ct already ends in \n */
}

/*
 * want --
 *	see if want this entry
 */
static int
want(struct utmp *bp, int check) {
	register ARG	*step;

	if (check) {
		/*
		 * when uucp and ftp log in over a network, the entry in
		 * the utmp file is the name plus their process id.  See
		 * etc/ftpd.c and usr.bin/uucp/uucpd.c for more information.
		 */
		if (!strncmp(bp->ut_line, "ftp", sizeof("ftp") - 1))
			bp->ut_line[3] = '\0';
		else if (!strncmp(bp->ut_line, "uucp", sizeof("uucp") - 1))
			bp->ut_line[4] = '\0';
	}
	if (!arglist)
		return YES;

	for (step = arglist; step; step = step->next)
		switch(step->type) {
		case HOST_TYPE:
			if (!strncmp(step->name, bp->ut_host, HMAX))
				return YES;
			break;
		case TTY_TYPE:
			if (!strncmp(step->name, bp->ut_line, LMAX))
				return YES;
			break;
		case USER_TYPE:
			if (!strncmp(step->name, bp->ut_name, NMAX))
				return YES;
			break;
		case INET_TYPE:
			if ((in_addr_t) bp->ut_addr == inet_addr(step->name))
			  return YES;
			break;
	}
	return NO;
}

/*
 * addarg --
 *	add an entry to a linked list of arguments
 */
static void
addarg(int type, char *arg) {
	register ARG	*cur;

	cur = xmalloc(sizeof(ARG));
	cur->next = arglist;
	cur->type = type;
	cur->name = arg;
	arglist = cur;
}

/*
 * addtty --
 *	add an entry to a linked list of ttys
 */
TTY *
addtty(char *ttyname) {
	register TTY	*cur;

	cur = xmalloc(sizeof(TTY));
	cur->next = ttylist;
	cur->logout = currentout;
	memcpy(cur->tty, ttyname, LMAX);
	return(ttylist = cur);
}

/*
 * hostconv --
 *	convert the hostname to search pattern; if the supplied host name
 *	has a domain attached that is the same as the current domain, rip
 *	off the domain suffix since that's what login(1) does.
 */
static void
hostconv(char *arg) {
	static int	first = 1;
	static char	*hostdot,
			name[MAXHOSTNAMELEN];
	char	*argdot;

	if (!(argdot = strchr(arg, '.')))
		return;
	if (first) {
		first = 0;
		if (gethostname(name, sizeof(name)))
			err(EXIT_FAILURE, _("gethostname failed"));

		hostdot = strchr(name, '.');
	}
	if (hostdot && !strcmp(hostdot, argdot))
		*argdot = '\0';
}

/*
 * ttyconv --
 *	convert tty to correct name.
 */
static char *
ttyconv(char *arg) {
	char	*mval;

	/*
	 * kludge -- we assume that all tty's end with
	 * a two character suffix.
	 */
	if (strlen(arg) == 2) {
		/* either 6 for "ttyxx" or 8 for "console" */
		mval = xmalloc(8);
		if (!strcmp(arg, "co"))
			(void)strcpy(mval, "console");
		else {
			(void)strcpy(mval, "tty");
			(void)strcpy(mval + 3, arg);
		}
		return mval;
	}
	if (!strncmp(arg, "/dev/", sizeof("/dev/") - 1))
		return arg + 5;

	return arg;
}

/*
 * onintr --
 *	on interrupt, we inform the user how far we've gotten
 */
static void
onintr(int signo) {
	char	*ct;

	ct = utmp_ctime(&utmpbuf);
	printf(_("\ninterrupted %10.10s %5.5s \n"), ct, ct + 11);
	if (signo == SIGINT)
		_exit(EXIT_FAILURE);
	fflush(stdout);			/* fix required for rsh */
}
