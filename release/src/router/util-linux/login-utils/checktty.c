/* checktty.c - linked into login, checks user against /etc/usertty
   Created 25-Aug-95 by Peter Orbaek <poe@daimi.aau.dk>
   Fixed by JDS June 1996 to clear lists and close files

   1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
   - added Native Language Support

*/

#include <sys/param.h>

#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <netdb.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>
#include <limits.h>
#include <netinet/in.h>
#include "nls.h"

#include <sys/sysmacros.h>
#ifdef HAVE_LINUX_MAJOR_H
#include <linux/major.h>
#endif

#include "pathnames.h"
#include "login.h"
#include "strutils.h"

#ifndef TTY_MAJOR
#define TTY_MAJOR 4
#endif

static gid_t mygroups[NGROUPS_MAX];
static int   num_groups;

#define NAMELEN 128

/* linked list of names */
struct grplist {
    struct grplist *next;
    char name[NAMELEN];
};
        
enum State { StateUsers, StateGroups, StateClasses };

#define CLASSNAMELEN 32

struct ttyclass {
    struct grplist *first;
    struct ttyclass *next;
    char classname[CLASSNAMELEN];
};

struct ttyclass *ttyclasses = NULL;

static int
am_in_group(char *group)
{
	struct group *g;
	gid_t *ge;
	
	g = getgrnam(group);
	if (g) {
		for (ge = mygroups; ge < mygroups + num_groups; ge++) {
			if (g->gr_gid== *ge) return 1;
		}
	}
    	return 0;
}

static void
find_groups()
{
	num_groups = getgroups(NGROUPS_MAX, mygroups);
}

static struct ttyclass *
new_class(char *class)
{
    struct ttyclass *tc;

    tc = (struct ttyclass *)malloc(sizeof(struct ttyclass));
    if (tc == NULL) {
	printf(_("login: memory low, login may fail\n"));
	syslog(LOG_WARNING, _("can't malloc for ttyclass"));
	return NULL;
    }

    tc->next = ttyclasses;
    tc->first = NULL;
    xstrncpy(tc->classname, class, CLASSNAMELEN);
    ttyclasses = tc;
    return tc;
}

static void
add_to_class(struct ttyclass *tc, char *tty)
{
    struct grplist *ge;

    if (tc == NULL) return;

    ge = (struct grplist *)malloc(sizeof(struct grplist));
    if (ge == NULL) {
	printf(_("login: memory low, login may fail\n"));
	syslog(LOG_WARNING, _("can't malloc for grplist"));
	return;
    }

    ge->next = tc->first;
    xstrncpy(ge->name, tty, NAMELEN);
    tc->first = ge;
}


/* return true if tty is a pty. Very linux dependent */
static int
isapty(const char *tty)
{
#ifdef __linux__
    char devname[100];
    struct stat stb;

    /* avoid snprintf - old systems do not have it */
    if (strlen(tty) + 6 > sizeof(devname))
	    return 0;
    sprintf(devname, "/dev/%s", tty);

    if((stat(devname, &stb) >= 0) && S_ISCHR(stb.st_mode)) {
	    int majordev = major(stb.st_rdev);

	    /* this is for linux versions before 1.3: use major 4 */
	    if(majordev == TTY_MAJOR && minor(stb.st_rdev) >= 192)
		    return 1;

#if defined(PTY_SLAVE_MAJOR)
	    /* this is for linux 1.3 and newer: use major 3 */
	    if(majordev == PTY_SLAVE_MAJOR)
		    return 1;
#endif

#if defined(UNIX98_PTY_SLAVE_MAJOR) && defined(UNIX98_PTY_MAJOR_COUNT)
	    /* this is for linux 2.1.116 and newer: use majors 136-143 */
	    if(majordev >= UNIX98_PTY_SLAVE_MAJOR &&
	       majordev < UNIX98_PTY_SLAVE_MAJOR + UNIX98_PTY_MAJOR_COUNT)
		    return 1;
#endif

    }
#endif	/* __linux__ */
    return 0;
}


/* IPv4 -- pattern is x.x.x.x/y.y.y.y (net/mask)*/
static int
hnmatch_ip4(const char *pat)
{
	int x1, x2, x3, x4, y1, y2, y3, y4;
	unsigned long p, mask, a;
	unsigned char *ha;

	/* pattern is an IP QUAD address and a mask x.x.x.x/y.y.y.y */
	if (sscanf(pat, "%d.%d.%d.%d/%d.%d.%d.%d",
			&x1, &x2, &x3, &x4, &y1, &y2, &y3, &y4) < 8)
		return 0;

	p = (((unsigned long)x1<<24)+((unsigned long)x2<<16)
	     +((unsigned long)x3<<8)+((unsigned long)x4));
	mask = (((unsigned long)y1<<24)+((unsigned long)y2<<16)
		+((unsigned long)y3<<8)+((unsigned long)y4));

	if (hostaddress[0] == 0)
		return 0;

	ha = (unsigned char *)hostaddress;
	a = (((unsigned long)ha[0]<<24)+((unsigned long)ha[1]<<16)
	     +((unsigned long)ha[2]<<8)+((unsigned long)ha[3]));
	return ((p & mask) == (a & mask));
}

/* IPv6 -- pattern is [hex:hex:....]/number ([net]/mask) */
static int
hnmatch_ip6(const char *pat)
{
	char *patnet;
	char *patmask;
	struct in6_addr addr;
	struct addrinfo hints, *res;
	struct sockaddr_in6 net;
	int mask_len, i = 0;
	char *p;

	if (pat == NULL || *pat != '[')
		return 0;

	memcpy(&addr, hostaddress, sizeof(addr));

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;

	patnet = strdup(pat);

	/* match IPv6 address against [netnumber]/prefixlen */
	if (!(p = strchr(patnet, ']')))
		goto mismatch;

	*p++ = '\0';

	if (! (*p == '/' && isdigit((unsigned char) *(p + 1))))
		goto mismatch;

	patmask = p + 1;

	/* prepare net address */
	if (getaddrinfo(patnet + 1, NULL, &hints, &res) != 0)
		goto mismatch;

	memcpy(&net, res->ai_addr, sizeof(net));
	freeaddrinfo(res);

	/* convert mask to number */
	if ((mask_len = atoi(patmask)) < 0 || mask_len > 128)
		goto mismatch;

	/* compare */
	while (mask_len > 0) {
		if (mask_len < 32) {
			uint32_t mask = htonl(~(0xffffffff >> mask_len));

			if ((*(uint32_t *)&addr.s6_addr[i] & mask) !=
			    (*(uint32_t *)&net.sin6_addr.s6_addr[i] & mask))
				goto mismatch;
			break;
		}
		if (*(uint32_t *)&addr.s6_addr[i] !=
		    *(uint32_t *)&net.sin6_addr.s6_addr[i])
			goto mismatch;
		i += 4;
		mask_len -= 32;
	}

	free(patnet);
	return 1;

mismatch:
	free(patnet);
	return 0;
}

/* match the hostname hn against the pattern pat */
static int
hnmatch(const char *hn, const char *pat)
{

	if ((hn == NULL) && (strcmp(pat, "localhost") == 0))
		return 1;
	if ((hn == NULL) || *hn == '\0')
		return 0;

	if (*pat >= '0' && *pat <= '9')
		return hostfamily == AF_INET ? hnmatch_ip4(pat) : 0;
	else if (*pat == '[')
		return hostfamily == AF_INET6 ? hnmatch_ip6(pat) : 0;
	else {
		/* pattern is a suffix of a FQDN */
		int 	n = strlen(pat),
			m = strlen(hn);

		if (n > m)
			return 0;
		return (strcasecmp(pat, hn + m - n) == 0);
	}
}

#ifdef MAIN_TEST_CHECKTTY

char	hostaddress[16];
sa_family_t hostfamily;
char	*hostname;

void sleepexit(int eval __attribute__ ((__unused__)))
{
	/* dummy for this test */
}

void badlogin(const char *s __attribute__ ((__unused__)))
{
	/* dummy for this test */
}

int
main(int argc __attribute__ ((__unused__)),
     char **argv __attribute__ ((__unused__)))
{
	struct addrinfo hints, *info = NULL;
	struct addrexp {
		const char *range;
		const char *ip;
	} alist[] = {
		{ "130.225.16.0/255.255.254.0",	"130.225.16.1" },
		{ "130.225.16.0/255.255.254.0",	"10.20.30.1" },
		{ "130.225.0.0/255.254.0.0",	"130.225.16.1" },
		{ "130.225.0.0/255.254.0.0",	"130.225.17.1" },
		{ "130.225.0.0/255.254.0.0",	"150.160.170.180" },
		{ "[3ffe:505:2:1::]/64",	"3ffe:505:2:1::" },
		{ "[3ffe:505:2:1::]/64",	"3ffe:505:2:2::" },
		{ "[3ffe:505:2:1::]/64",	"3ffe:505:2:1:ffff:ffff::" },
		{ NULL, NULL }
	}, *item;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_NUMERICHOST |  AI_PASSIVE | AI_ADDRCONFIG;
	hints.ai_socktype = SOCK_STREAM;

	for (item = alist; item->range; item++) {

		printf("hnmatch() on %-30s <-- %-15s: ", item->range, item->ip);

		if (getaddrinfo(item->ip, NULL, &hints, &info)==0 && info) {
			if (info->ai_family == AF_INET)	{
			    struct sockaddr_in *sa =
					(struct sockaddr_in *) info->ai_addr;
			    memcpy(hostaddress, &(sa->sin_addr),
					sizeof(sa->sin_addr));
			}
			else if (info->ai_family == AF_INET6) {
			    struct sockaddr_in6 *sa =
					(struct sockaddr_in6 *) info->ai_addr;
			    memcpy(hostaddress, &(sa->sin6_addr),
					sizeof(sa->sin6_addr));
			}
			hostfamily = info->ai_family;
			freeaddrinfo(info);
			printf("%s\n", hnmatch("dummy", item->range) ?
						"match" : "mismatch");
		}
		else
			printf("getaddrinfo() failed\n");

	}
	return EXIT_SUCCESS;
}
#endif /* MAIN_TEST_CHECKTTY */

static char *wdays[] = { "sun", "mon", "tue", "wed", "thu", "fri", "sat" };

/* example timespecs:

   mon:tue:wed:8-17

   meaning monday, tuesday or wednesday between 8:00 and 17:59

   4:5:13:fri

   meaning fridays from 4:00 to 5:59 and from 13:00 to 13:59
*/
static int
timeok(struct tm *t, char *spec)
{
    char *p, *q;
    int dayok = 0;
    int hourok = 0;
    int h, h2;
    char *sp;

    sp = spec;
    while ((p = strsep(&sp, ":"))) {
	if (*p >= '0' && *p <= '9') {
	    h = atoi(p);
	    if (h == t->tm_hour) hourok = 1;
	    if ((q = strchr(p, '-')) && (q[1] >= '0' && q[1] <= '9')) {
		h2 = atoi(q+1);
		if (h <= t->tm_hour && t->tm_hour <= h2) hourok = 1;
	    }
	} else if (strcasecmp(wdays[t->tm_wday], p) == 0) {
	    dayok = 1;
	}
    }

    return (dayok && hourok);
}

/* return true if tty equals class or is in the class defined by class.
   Also return true if hostname matches the hostname pattern, class
   or a pattern in the class named by class. */
static int
in_class(const char *tty, char *class)
{
    struct ttyclass *tc;
    struct grplist *ge;
    time_t t;
    char *p;
    char timespec[256];
    struct tm *tm;
    char *n;

    time(&t);
    tm = localtime(&t);

    if (class[0] == '[') {
	if ((p = strchr(class, ']'))) {
	    *p = 0;
	    xstrncpy(timespec, class+1, sizeof(timespec));
	    *p = ']';
	    if(!timeok(tm, timespec)) return 0;
	    class = p+1;
	}
	/* really ought to warn about syntax error */
    }

    if (strcmp(tty, class) == 0) return 1;

    if ((class[0] == '@') && isapty(tty)
	&& hnmatch(hostname, class+1)) return 1;

    for (tc = ttyclasses; tc; tc = tc->next) {
	if (strcmp(tc->classname, class) == 0) {
	    for (ge = tc->first; ge; ge = ge->next) {

		n = ge->name;
		if (n[0] == '[') {
		    if ((p = strchr(n, ']'))) {
			*p = 0;
			xstrncpy(timespec, n+1, sizeof(timespec));
			*p = ']';
			if(!timeok(tm, timespec)) continue;
			n = p+1;
		    }
		    /* really ought to warn about syntax error */
		}

		if (strcmp(n, tty) == 0) return 1;

		if ((n[0] == '@') && isapty(tty)
		    && hnmatch(hostname, n+1)) return 1;
	    }
	    return 0;
	}
    }
    return 0;
}

/* start JDS - SBA */
static void 
free_group(struct grplist *ge)
{
    if (ge) {
	memset(ge->name, 0, NAMELEN);
	free_group(ge->next);
	free(ge->next);
	ge->next = NULL;
    }
}

static void 
free_class(struct ttyclass *tc)
{
    if (tc) {
	memset(tc->classname, 0, CLASSNAMELEN);
	free_group(tc->first);
	tc->first = NULL;
	free_class(tc->next);
	free(tc->next);
	tc->next = NULL;
    }
}

static void 
free_all(void)
{
    free_class(ttyclasses);
    ttyclasses = NULL;
}
/* end JDS - SBA */

void
checktty(const char *user, const char *tty, struct passwd *pwd)
{
    FILE *f;
    char buf[256], defaultbuf[256];
    char *ptr;
    enum State state = StateUsers;
    int found_match = 0;

    /* no /etc/usertty, default to allow access */
    if (!(f = fopen(_PATH_USERTTY, "r"))) return;

    if (pwd == NULL) {
	fclose(f);
	return;  /* misspelled username handled elsewhere */
    }

    find_groups();

    defaultbuf[0] = 0;
    while(fgets(buf, 255, f)) {

	/* strip comments */
	for(ptr = buf; ptr < buf + 256; ptr++) 
	  if(*ptr == '#') *ptr = 0;

	if (buf[0] == '*') {
	    xstrncpy(defaultbuf, buf, 256);
	    continue;
	}

	if (strncmp("GROUPS", buf, 6) == 0) {
	    state = StateGroups;
	    continue;
	} else if (strncmp("USERS", buf, 5) == 0) {
	    state = StateUsers;
	    continue;
	} else if (strncmp("CLASSES", buf, 7) == 0) {
	    state = StateClasses;
	    continue;
	}

	strtok(buf, " \t");
	if((state == StateUsers && (strncmp(user, buf, 8) == 0))
	   || (state == StateGroups && am_in_group(buf))) {
	    found_match = 1;  /* we found a line matching the user */
	    while((ptr = strtok(NULL, "\t\n "))) {
		if (in_class(tty, ptr)) {
		    fclose(f);
		    free_all(); /* JDS */
		    return;
		}
	    }
	} else if (state == StateClasses) {
	    /* define a new tty/host class */
	    struct ttyclass *tc = new_class(buf);

	    while ((ptr = strtok(NULL, "\t\n "))) {
		add_to_class(tc, ptr);
	    }
	}
    }
    fclose(f);

    /* user is not explicitly mentioned in /etc/usertty, if there was
       a default rule, use that */
    if (defaultbuf[0]) {
	strtok(defaultbuf, " \t");
	while((ptr = strtok(NULL, "\t\n "))) {
	    if (in_class(tty, ptr)) {
		free_all(); /* JDS */
		return;
	    }
	}

	/* there was a default rule, but user didn't match, reject! */
	printf(_("Login on %s from %s denied by default.\n"), tty, hostname);
	badlogin(user);
	sleepexit(EXIT_FAILURE);
    }

    if (found_match) {
	/* if we get here, /etc/usertty exists, there's a line
	   matching our username, but it doesn't contain the
	   name of the tty where the user is trying to log in.
	   So deny access! */

	printf(_("Login on %s from %s denied.\n"), tty, hostname);
	badlogin(user);
	sleepexit(EXIT_FAILURE);
    }

    /* users not matched in /etc/usertty are by default allowed access
       on all tty's */
    free_all(); /* JDS */
}
