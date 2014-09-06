/*
 * cheap and dirty network database lookup functions 
 */
/*
 * uses local files only 
 */
/*
 * currently searches the protocols only 
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#if (defined(WIN32) || defined(cygwin) || defined(aix4))

#ifdef aix4
#define _NO_PROTO               /* Hack, you say ? */
#endif

#include <stdio.h>
#include <sys/types.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif

static int      h_stay_open, s_stay_open, p_stay_open, n_stay_open;
static FILE    *h_fp, *s_fp, *p_fp, *n_fp;
static char    *p_fn;
#ifdef notused
static char    *h_fn, *s_fn, *n_fn;
#endif

#ifdef aix4
#define ROOT_BASE "/etc/"
#define PROT_FN "protocols"
#else
#define ROOT_BASE "\\SYSTEM32\\DRIVERS\\ETC\\"
#define PROT_FN "protocol"
#endif
#define HOST_FN "hosts"
#define SERV_FN "services"
#define NETW_FN "networks"

static int      pre_env_done = 0;
static void
pre_env(void)
{
    const char     *cproot;
    const char     *cp = "";

    if (pre_env_done)
        return;
    pre_env_done = 1;

#ifndef aix4
    cp = getenv("SYSTEMROOT");
    if (cp) {
        ;
        /*
         * printf ("Root is '%s'\n", cp); 
         */
    } else
        cp = "C:\\WINNT";
#endif

    cproot = ROOT_BASE;
    p_fn =
        (char *) malloc(3 + strlen(cp) + strlen(cproot) + strlen(PROT_FN));
    if (p_fn)
        sprintf(p_fn, "%s%s%s", cp, cproot, PROT_FN);
#ifdef notused
    h_fn =
        (char *) malloc(3 + strlen(cp) + strlen(cproot) + strlen(HOST_FN));
    if (h_fn)
        sprintf(h_fn, "%s%s%s", cp, cproot, HOST_FN);
    s_fn =
        (char *) malloc(3 + strlen(cp) + strlen(cproot) + strlen(SERV_FN));
    if (s_fn)
        sprintf(s_fn, "%s%s%s", cp, cproot, SERV_FN);
    n_fn =
        (char *) malloc(3 + strlen(cp) + strlen(cproot) + strlen(NETW_FN));
    if (n_fn)
        sprintf(n_fn, "%s%s%s", cp, cproot, NETW_FN);
#endif
}

/*
 * sets can open. ends must close. 
 */
void
endhostent(void)
{
    if (h_fp)
        fclose(h_fp);
    h_fp = 0;
}

void
endservent(void)
{
    if (s_fp)
        fclose(s_fp);
    s_fp = 0;
}

void
endprotoent(void)
{
    if (p_fp)
        fclose(p_fp);
    p_fp = 0;
}

void
endnetent(void)
{
    if (n_fp)
        fclose(n_fp);
    n_fp = 0;
}

void
sethostent(int stay_open)
{
    pre_env();
    endhostent();
    h_stay_open = stay_open;
}

void
setservent(int stay_open)
{
    pre_env();
    endservent();
    s_stay_open = stay_open;
}

void
setprotoent(int stay_open)
{
    pre_env();
    endprotoent();
    p_stay_open = stay_open;
}

void
setnetent(int stay_open)
{
    pre_env();
    endnetent();
    n_stay_open = stay_open;
}

#define STRTOK_DELIMS " \t\n"

/*
 * get next entry from data base file, or from NIS if possible. 
 */
/*
 * returns 0 if there are no more entries to read. 
 */
struct hostent *
gethostent(void)
{
    return 0;
}
struct servent *
getservent(void)
{
    return 0;
}

struct protoent *
getprotoent(void)
{
    char           *cp, **alp, lbuf[256];
    static struct protoent spx;
    static char    *ali[10];
    struct protoent *px = &spx;
    int             linecnt = 0;
    char            *st;

    for (alp = ali; *alp; free(*alp), *alp = 0, alp++);
    if (px->p_name)
        free(px->p_name);
    px->p_aliases = ali;

    if (!p_fn)
        return 0;
    if (!p_fp)
        p_fp = fopen(p_fn, "r");

    if (!p_fp)
        return 0;
    while (fgets(lbuf, sizeof(lbuf), p_fp)) {
        linecnt++;
        cp = lbuf;
        if (*cp == '#')
            continue;

        cp = strtok_r(lbuf, STRTOK_DELIMS, &st);
        if (!cp)
            continue;
        if (cp)
            px->p_name = strdup(cp);

        cp = strtok_r(NULL, STRTOK_DELIMS, &st);
        if (!cp) {
            free(px->p_name);
            continue;
        }
        px->p_proto = (short) atoi(cp);

        for (alp = px->p_aliases; cp; alp++) {
            cp = strtok_r(NULL, STRTOK_DELIMS, &st);
            if (!cp)
                break;
            if (*cp == '#')
                break;
            *alp = strdup(cp);
        }

        return (px);
    }

    return 0;
}
struct netent  *
getnetent(void)
{
    return 0;
}

struct netent  *
getnetbyaddr(long net, int type)
{
    return 0;
}

/*
 * Return the network number from an internet address 
 */
u_long
inet_netof(struct in_addr in)
{
    register u_long i = ntohl(in.s_addr);
    u_long          ii = (i >> 24) & 0xff;
    if (0 == (ii & 0x80))
        return ((0xff000000 & i) >> 24);
    if (0x80 == (ii & 0xc0))
        return ((0xffff0000 & i) >> 16);
    /*
     * if (0xc0 == (ii & 0xe0))
     */
    return ((0xffffff00 & i) >> 8);
}

/*
 * Return the host number from an internet address 
 */
u_long
inet_lnaof(struct in_addr in)
{
    register u_long i = ntohl(in.s_addr);
    u_long          ii = (i >> 24) & 0xff;
    if (0 == (ii & 0x80))
        return (0x00ffffff & i);
    if (0x80 == (ii & 0xc0))
        return (0x0000ffff & i);
    /*
     * if (0xc0 == (ii & 0xe0)) 
     */
    return (0x000000ff & i);
}

#endif                          /* WIN32 or cygwin */
