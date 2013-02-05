/*
 * Copyright (C) 1995-1999 Jeffrey A. Uphoff
 * Major rewrite by Olaf Kirch, Dec. 1996.
 * Modified by H.J. Lu, 1998.
 * Tighter access control, Olaf Kirch June 1999.
 *
 * NSM for Linux.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <arpa/inet.h>
#include <dirent.h>

#include "rpcmisc.h"
#include "misc.h"
#include "statd.h"
#include "notlist.h"
#include "ha-callout.h"

notify_list *		rtnl = NULL;	/* Run-time notify list. */

#define LINELEN (4*(8+1)+SM_PRIV_SIZE*2+1)

/*
 * Reject requests from non-loopback addresses in order
 * to prevent attack described in CERT CA-99.05.
 */
static int
caller_is_localhost(struct svc_req *rqstp)
{
	struct sockaddr_in *sin = nfs_getrpccaller_in(rqstp->rq_xprt);
	struct in_addr	caller;

	caller = sin->sin_addr;
	if (caller.s_addr != htonl(INADDR_LOOPBACK)) {
		note(N_WARNING,
			"Call to statd from non-local host %s",
			inet_ntoa(caller));
		return 0;
	}
	return 1;
}

/*
 * Services SM_MON requests.
 */
struct sm_stat_res *
sm_mon_1_svc(struct mon *argp, struct svc_req *rqstp)
{
	static sm_stat_res result;
	char		*mon_name = argp->mon_id.mon_name,
			*my_name  = argp->mon_id.my_id.my_name;
	struct my_id	*id = &argp->mon_id.my_id;
	char            *path;
	char		*cp;
	int             fd;
	notify_list	*clnt;
	struct in_addr	my_addr;
	char		*dnsname;
	struct hostent	*hostinfo = NULL;

	/* Assume that we'll fail. */
	result.res_stat = STAT_FAIL;
	result.state = -1;	/* State is undefined for STAT_FAIL. */

	/* 1.	Reject any remote callers.
	 *	Ignore the my_name specified by the caller, and
	 *	use "127.0.0.1" instead.
	 */
	if (!caller_is_localhost(rqstp))
		goto failure;
	my_addr.s_addr = htonl(INADDR_LOOPBACK);

	/* 2.	Reject any registrations for non-lockd services.
	 *
	 *	This is specific to the linux kernel lockd, which
	 *	makes the callback procedure part of the lockd interface.
	 *	It is also prone to break when lockd changes its callback
	 *	procedure number -- which, in fact, has now happened once.
	 *	There must be a better way....   XXX FIXME
	 */
	if (id->my_prog != 100021 ||
	    (id->my_proc != 16 && id->my_proc != 24))
	{
		note(N_WARNING,
			"Attempt to register callback to %d/%d",
			id->my_prog, id->my_proc);
		goto failure;
	}

	/*
	 * Check hostnames.  If I can't look them up, I won't monitor.  This
	 * might not be legal, but it adds a little bit of safety and sanity.
	 */

	/* must check for /'s in hostname!  See CERT's CA-96.09 for details. */
	if (strchr(mon_name, '/') || mon_name[0] == '.') {
		note(N_CRIT, "SM_MON request for hostname containing '/' "
		     "or starting '.': %s", mon_name);
		note(N_CRIT, "POSSIBLE SPOOF/ATTACK ATTEMPT!");
		goto failure;
	} else if ((hostinfo = gethostbyname(mon_name)) == NULL) {
		note(N_WARNING, "gethostbyname error for %s", mon_name);
		goto failure;
	}

	/* my_name must not have white space */
	for (cp=my_name ; *cp ; cp++)
		if (*cp == ' ' || *cp == '\t' || *cp == '\r' || *cp == '\n')
			*cp = '_';

	/*
	 * Hostnames checked OK.
	 * Now choose a hostname to use for matching.  We cannot
	 * really trust much in the incoming NOTIFY, so to make
	 * sure that multi-homed hosts work nicely, we get an
	 * FQDN now, and use that for matching
	 */
	hostinfo = gethostbyaddr(hostinfo->h_addr,
				 hostinfo->h_length,
				 hostinfo->h_addrtype);
	if (hostinfo)
		dnsname = xstrdup(hostinfo->h_name);
	else
		dnsname = xstrdup(my_name);

	/* Now check to see if this is a duplicate, and warn if so.
	 * I will also return STAT_FAIL. (I *think* this is how I should
	 * handle it.)
	 *
	 * Olaf requests that I allow duplicate SM_MON requests for
	 * hosts due to the way he is coding lockd. No problem,
	 * I'll just do a quickie success return and things should
	 * be happy.
	 */
	clnt = rtnl;

	while ((clnt = nlist_gethost(clnt, mon_name, 0))) {
		if (matchhostname(NL_MY_NAME(clnt), my_name) &&
		    NL_MY_PROC(clnt) == id->my_proc &&
		    NL_MY_PROG(clnt) == id->my_prog &&
		    NL_MY_VERS(clnt) == id->my_vers &&
		    memcmp(NL_PRIV(clnt), argp->priv, SM_PRIV_SIZE) == 0) {
			/* Hey!  We already know you guys! */
			dprintf(N_DEBUG,
				"Duplicate SM_MON request for %s "
				"from procedure on %s",
				mon_name, my_name);

			/* But we'll let you pass anyway. */
			goto success;
		}
		clnt = NL_NEXT(clnt);
	}

	/*
	 * We're committed...ignoring errors.  Let's hope that a malloc()
	 * doesn't fail.  (I should probably fix this assumption.)
	 */
	if (!(clnt = nlist_new(my_name, mon_name, 0))) {
		note(N_WARNING, "out of memory");
		goto failure;
	}

	NL_ADDR(clnt) = my_addr;
	NL_MY_PROG(clnt) = id->my_prog;
	NL_MY_VERS(clnt) = id->my_vers;
	NL_MY_PROC(clnt) = id->my_proc;
	memcpy(NL_PRIV(clnt), argp->priv, SM_PRIV_SIZE);
	clnt->dns_name = dnsname;

	/*
	 * Now, Create file on stable storage for host.
	 */

	path=xmalloc(strlen(SM_DIR)+strlen(dnsname)+2);
	sprintf(path, "%s/%s", SM_DIR, dnsname);
	if ((fd = open(path, O_WRONLY|O_SYNC|O_CREAT|O_APPEND,
		       S_IRUSR|S_IWUSR)) < 0) {
		/* Didn't fly.  We won't monitor. */
		note(N_ERROR, "creat(%s) failed: %s", path, strerror (errno));
		nlist_free(NULL, clnt);
		free(path);
		goto failure;
	}
	{
		char buf[LINELEN + 1 + SM_MAXSTRLEN*2 + 4];
		char *e;
		int i;
		e = buf + sprintf(buf, "%08x %08x %08x %08x ",
				  my_addr.s_addr, id->my_prog,
				  id->my_vers, id->my_proc);
		for (i=0; i<SM_PRIV_SIZE; i++)
			e += sprintf(e, "%02x", 0xff & (argp->priv[i]));
		if (e+1-buf != LINELEN) abort();
		e += sprintf(e, " %s %s\n", mon_name, my_name);
		if (write(fd, buf, e-buf) != (e-buf)) {
			note(N_WARNING, "writing to %s failed: errno %d (%s)",
				path, errno, strerror(errno));
		}
	}

	free(path);
	/* PRC: do the HA callout: */
	ha_callout("add-client", mon_name, my_name, -1);
	nlist_insert(&rtnl, clnt);
	close(fd);
	dprintf(N_DEBUG, "MONITORING %s for %s", mon_name, my_name);
 success:
	result.res_stat = STAT_SUCC;
	/* SUN's sm_inter.x says this should be "state number of local site".
	 * X/Open says '"state" will be contain the state of the remote NSM.'
	 * href=http://www.opengroup.org/onlinepubs/9629799/SM_MON.htm
	 * Linux lockd currently (2.6.21 and prior) ignores whatever is
	 * returned, and given the above contraction, it probably always will..
	 * So we just return what we always returned.  If possible, we
	 * have already told lockd about our state number via a sysctl.
	 * If lockd wants the remote state, it will need to
	 * use SM_STAT (and prayer).
	 */
	result.state = MY_STATE;
	return (&result);

failure:
	note(N_WARNING, "STAT_FAIL to %s for SM_MON of %s", my_name, mon_name);
	return (&result);
}

void load_state(void)
{
	DIR *d;
	struct dirent *de;
	char buf[LINELEN + 1 + SM_MAXSTRLEN + 2];

	d = opendir(SM_DIR);
	if (!d)
		return;
	while ((de = readdir(d))) {
		char *path;
		FILE *f;
		int p;

		if (de->d_name[0] == '.')
			continue;
		path = xmalloc(strlen(SM_DIR)+strlen(de->d_name)+2);
		sprintf(path, "%s/%s", SM_DIR, de->d_name);
		f = fopen(path, "r");
		free(path);
		if (f == NULL)
			continue;
		while (fgets(buf, sizeof(buf), f) != NULL) {
			int addr, proc, prog, vers;
			char priv[SM_PRIV_SIZE];
			char *monname, *myname;
			char *b;
			int i;
			notify_list	*clnt;

			buf[sizeof(buf)-1] = 0;
			b = strchr(buf, '\n');
			if (b) *b = 0;
			sscanf(buf, "%x %x %x %x ",
			       &addr, &prog, &vers, &proc);
			b = buf+36;
			for (i=0; i<SM_PRIV_SIZE; i++) {
				sscanf(b, "%2x", &p);
				priv[i] = p;
				b += 2;
			}
			b++;
			monname = b;
			while (*b && *b != ' ') b++;
			if (*b) *b++ = '\0';
			while (*b == ' ') b++;
			myname = b;
			clnt = nlist_new(myname, monname, 0);
			if (!clnt)
				break;
			NL_ADDR(clnt).s_addr = addr;
			NL_MY_PROG(clnt) = prog;
			NL_MY_VERS(clnt) = vers;
			NL_MY_PROC(clnt) = proc;
			clnt->dns_name = xstrdup(de->d_name);
			memcpy(NL_PRIV(clnt), priv, SM_PRIV_SIZE);
			nlist_insert(&rtnl, clnt);
		}
		fclose(f);
	}
	closedir(d);
}




/*
 * Services SM_UNMON requests.
 *
 * There is no statement in the X/Open spec's about returning an error
 * for requests to unmonitor a host that we're *not* monitoring.  I just
 * return the state of the NSM when I get such foolish requests for lack
 * of any better ideas.  (I also log the "offense.")
 */
struct sm_stat *
sm_unmon_1_svc(struct mon_id *argp, struct svc_req *rqstp)
{
	static sm_stat  result;
	notify_list	*clnt;
	char		*mon_name = argp->mon_name,
			*my_name  = argp->my_id.my_name;
	struct my_id	*id = &argp->my_id;
	char		*cp;

	result.state = MY_STATE;

	if (!caller_is_localhost(rqstp))
		goto failure;

	/* my_name must not have white space */
	for (cp=my_name ; *cp ; cp++)
		if (*cp == ' ' || *cp == '\t' || *cp == '\r' || *cp == '\n')
			*cp = '_';


	/* Check if we're monitoring anyone. */
	if (rtnl == NULL) {
		note(N_WARNING,
			"Received SM_UNMON request from %s for %s while not "
			"monitoring any hosts.", my_name, argp->mon_name);
		return (&result);
	}
	clnt = rtnl;

	/*
	 * OK, we are.  Now look for appropriate entry in run-time list.
	 * There should only be *one* match on this, since I block "duplicate"
	 * SM_MON calls.  (Actually, duplicate calls are allowed, but only one
	 * entry winds up in the list the way I'm currently handling them.)
	 */
	while ((clnt = nlist_gethost(clnt, mon_name, 0))) {
		if (matchhostname(NL_MY_NAME(clnt), my_name) &&
			NL_MY_PROC(clnt) == id->my_proc &&
			NL_MY_PROG(clnt) == id->my_prog &&
			NL_MY_VERS(clnt) == id->my_vers) {
			/* Match! */
			dprintf(N_DEBUG, "UNMONITORING %s for %s",
					mon_name, my_name);

			/* PRC: do the HA callout: */
			ha_callout("del-client", mon_name, my_name, -1);

			xunlink(SM_DIR, clnt->dns_name);
			nlist_free(&rtnl, clnt);

			return (&result);
		} else
			clnt = NL_NEXT(clnt);
	}

 failure:
	note(N_WARNING, "Received erroneous SM_UNMON request from %s for %s",
		my_name, mon_name);
	return (&result);
}


struct sm_stat *
sm_unmon_all_1_svc(struct my_id *argp, struct svc_req *rqstp)
{
	short int       count = 0;
	static sm_stat  result;
	notify_list	*clnt;
	char		*my_name = argp->my_name;

	if (!caller_is_localhost(rqstp))
		goto failure;

	result.state = MY_STATE;

	if (rtnl == NULL) {
		note(N_WARNING, "Received SM_UNMON_ALL request from %s "
			"while not monitoring any hosts", my_name);
		return (&result);
	}
	clnt = rtnl;

	while ((clnt = nlist_gethost(clnt, my_name, 1))) {
		if (NL_MY_PROC(clnt) == argp->my_proc &&
			NL_MY_PROG(clnt) == argp->my_prog &&
			NL_MY_VERS(clnt) == argp->my_vers) {
			/* Watch stack! */
			char            mon_name[SM_MAXSTRLEN + 1];
			notify_list	*temp;

			dprintf(N_DEBUG,
				"UNMONITORING (SM_UNMON_ALL) %s for %s",
				NL_MON_NAME(clnt), NL_MY_NAME(clnt));
			strncpy(mon_name, NL_MON_NAME(clnt),
				sizeof (mon_name) - 1);
			mon_name[sizeof (mon_name) - 1] = '\0';
			temp = NL_NEXT(clnt);
			/* PRC: do the HA callout: */
			ha_callout("del-client", mon_name, my_name, -1);
			xunlink(SM_DIR, clnt->dns_name);
			nlist_free(&rtnl, clnt);
			++count;
			clnt = temp;
		} else
			clnt = NL_NEXT(clnt);
	}

	if (!count) {
		dprintf(N_DEBUG, "SM_UNMON_ALL request from %s with no "
			"SM_MON requests from it.", my_name);
	}

 failure:
	return (&result);
}
