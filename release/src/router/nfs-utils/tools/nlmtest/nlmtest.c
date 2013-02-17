/*
 * nlmtest
 *
 * Simple tool for NLM testing. You will have to adjust the values in
 * host.h to your test system.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#include "config.h"

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <nfs/nfs.h>
#include <getopt.h>
#include "nlm_prot.h"
#include "host.h"

static char		myhostname[256];
static int		hostnamelen;

static void		makelock(struct nlm_lock *, u_int32_t, off_t, off_t);
static void		makeowner(struct netobj *, u_int32_t);
static void		makefileh(struct netobj *);
static char *		nlm_stat_name(int status);
static char *		holderstr(struct netobj *oh);

int
main(int argc, char **argv)
{
	CLIENT		*client;
	nlm_testargs	testargs;
	nlm_lockargs	lockargs;
	nlm_unlockargs	unlockargs;
	nlm_lock	alock;
	nlm_testres	*testres;
	nlm_res		*lockres;
	char		*filename = NLMTEST_FILE;
	char		*svchost = NLMTEST_HOST;
	unsigned long	offset = 0, length = 0;
	int		exclusive = 0;
	int		blocking = 0;
	int		unlock = 0;
	u_int32_t		cookie = 4321;
	u_int32_t		mypid = 1234;
	int		c;

	while ((c = getopt(argc, argv, "bf:h:l:o:p:ux")) != EOF) {
		switch(c) {
		case 'b':
			blocking = 1;
			break;
		case 'f':
			filename = optarg;
			break;
		case 'h':
			svchost = optarg;
			break;
		case 'l':
			length = atoi(optarg);
			break;
		case 'o':
			offset = atoi(optarg);
			break;
		case 'p':
			mypid = atoi(optarg);
			break;
		case 'u':
			unlock = 1;
			break;
		case 'x':
			exclusive = 1;
			break;
		default:
			fprintf(stderr, "nlmtest: bad option %c\n", c);
			exit (2);
		}
	}

	client = clnt_create(svchost, NLM_PROG, NLM_VERS, "udp");
	if (client == NULL) {
		clnt_pcreateerror("localhost");
		exit(1);
	}

	/* Get local host name */
	if (gethostname(myhostname, sizeof(myhostname)) < 0)
		strcpy(myhostname, "unknown");
	hostnamelen = strlen(myhostname);

	makelock(&alock, mypid, offset, length);

	testargs.cookie.n_bytes = (void*)&cookie;
	testargs.cookie.n_len   = 4;
	testargs.exclusive      = exclusive;
	testargs.alock          = alock;

	if ((testres = nlm_test_1(&testargs, client)) == NULL) {
		clnt_perror(client, "nlm_test call failed:");
		exit (1);
	}
	printf ("nlm_test reply:\n"
		"\tcookie:      %d\n"
		"\tstatus:      %s\n",
			*(int*)(testres->cookie.n_bytes),
			nlm_stat_name(testres->stat.stat)
		);

	if (testres->stat.stat == nlm_denied) {
		nlm_holder *holder = &(testres->stat.nlm_testrply_u.holder);
		printf ("\tconflicting lock:\n"
			"\t oh:         %s\n"
			"\t pid:        %d\n"
			"\t offset:     %d\n"
			"\t length:     %d\n"
			"\t exclusive:  %d\n",
			holderstr(&holder->oh),
			holder->svid,
			holder->l_offset,
			holder->l_len,
			holder->exclusive);
	}

	if (testres->stat.stat != nlm_granted && !unlock && !blocking)
		return 1;

	if (unlock) {
		unlockargs.cookie.n_bytes = (void*)&cookie;
		unlockargs.cookie.n_len   = sizeof(cookie);
		unlockargs.alock          = alock;

		if ((lockres = nlm_unlock_1(&unlockargs, client)) == NULL) {
			clnt_perror(client, "nlm_unlock call failed:");
			exit (1);
		}
		printf ("nlm_unlock reply:\n"
			"\tcookie:      %d\n"
			"\tstatus:      %s\n",
				*(int*)(lockres->cookie.n_bytes),
				nlm_stat_name(lockres->stat.stat)
			);
	} else {
		lockargs.cookie.n_bytes = (void*)&cookie;
		lockargs.cookie.n_len   = sizeof(cookie);
		lockargs.exclusive      = exclusive;
		lockargs.alock          = alock;
		lockargs.reclaim        = 0;
		lockargs.state          = 0;

		if ((lockres = nlm_lock_1(&lockargs, client)) == NULL) {
			clnt_perror(client, "nlm_lock call failed:");
			exit (1);
		}
		printf ("nlm_lock reply:\n"
			"\tcookie:      %d\n"
			"\tstatus:      %s\n",
				*(int*)(lockres->cookie.n_bytes),
				nlm_stat_name(lockres->stat.stat)
			);
	}

	return 0;
}

static char *
nlm_stat_name(int status)
{
	static char	buf[12];

	switch (status) {
	case nlm_granted:
		return "nlm_granted";
	case nlm_denied:
		return "nlm_denied";
	case nlm_denied_nolocks:
		return "nlm_denied_nolocks";
	case nlm_blocked:
		return "nlm_blocked";
	case nlm_denied_grace_period:
		return "nlm_denied_grace_period";
	}
	sprintf(buf, "%d", status);
	return buf;
}

static char *
holderstr(struct netobj *oh)
{
	static char	buffer[4096];
	unsigned char	c, *sp;
	int		i;

	for (i = 0, sp = buffer; i < oh->n_len; i++) {
		c = (unsigned char) oh->n_bytes[i];
		if (c < 0x20 || c > 0x7f)
			sp += sprintf(sp, "\\%03o", c);
		else
			*sp++ = c;
	}
	*sp++ = '\0';

	return buffer;
}

static void
makelock(struct nlm_lock *alock, u_int32_t mypid, off_t offset, off_t length)
{
	makeowner(&alock->oh, mypid);	/* Create owner handle */
	makefileh(&alock->fh);		/* Create file handle */

	alock->caller_name = myhostname;
	alock->svid        = mypid;
	alock->l_offset    = offset;
	alock->l_len       = length;
}

static void
makeowner(struct netobj *oh, u_int32_t mypid)
{
	static char	ohdata[1024];

	oh->n_bytes = ohdata;
	oh->n_len   = hostnamelen + 1 + 4;

	strcpy(ohdata, myhostname);
	memcpy(ohdata + hostnamelen + 1, &mypid, 4);
}

static void
makefileh(struct netobj *fh)
{
	static struct knfs_fh	f;
	struct stat		stb;
#error this needs updating if it is still wanted
	memset(&f, 0, sizeof(f));
#if 0
	if (stat(NLMTEST_DIR, &stb) < 0) {
		perror("couldn't stat mount point " NLMTEST_DIR);
		exit(1);
	}
	f.fh_xdev = stb.st_dev;
	f.fh_xino = stb.st_ino;

	if (stat(NLMTEST_DIR, &stb) < 0) {
		perror("couldn't stat mount point " NLMTEST_DIR);
		exit(1);
	}
	f.fh_dev = stb.st_dev;
	f.fh_ino = stb.st_ino;

	f.fh_version = NLMTEST_VERSION;
#else
	f.fh_xdev = 0x801;
	f.fh_xino = 37596;
	f.fh_dev  = 0x801;
	f.fh_ino  = 37732;
#endif

	fh->n_len   = 32;
	fh->n_bytes = (void *) &f;
}
