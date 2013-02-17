 /*
  * pmap_dump - dump portmapper table in format readable by pmap_set
  * 
  * Author: Wietse Venema (wietse@wzv.win.tue.nl), dept. of Mathematics and
  * Computing Science, Eindhoven University of Technology, The Netherlands.
  */

#include <stdio.h>
#include <sys/types.h>
#ifdef SYSV40
#include <netinet/in.h>
#include <rpc/rpcent.h>
#else
#include <netdb.h>
#endif
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>

static char *protoname(u_long proto);

int
main(int argc, char **argv)
{
    struct sockaddr_in addr;
    struct pmaplist *list;
    struct rpcent *rpc;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(PMAPPORT);

    for (list = pmap_getmaps(&addr); list; list = list->pml_next) {
	rpc = getrpcbynumber((int) list->pml_map.pm_prog);
	printf("%10lu %4lu %5s %6lu  %s\n",
	       list->pml_map.pm_prog,
	       list->pml_map.pm_vers,
	       protoname(list->pml_map.pm_prot),
	       list->pml_map.pm_port,
	       rpc ? rpc->r_name : "");
    }
    return (fclose(stdout) ? (perror(argv[0]), 1) : 0);
}

static char *protoname(u_long proto)
{
    static char buf[BUFSIZ];

    switch (proto) {
    case IPPROTO_UDP:
	return ("udp");
    case IPPROTO_TCP:
	return ("tcp");
    default:
	sprintf(buf, "%lu", proto);
	return (buf);
    }
}
