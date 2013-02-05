#ifndef TCP_WRAPPER_H
#define TCP_WRAPPER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

extern int verboselog;

extern int allow_severity;
extern int deny_severity;

extern int good_client(char *daemon, struct sockaddr_in *addr);
extern int from_local (struct sockaddr_in *addr);
extern int check_default(char *daemon, struct sockaddr_in *addr,
			 u_long proc, u_long prog);

#endif /* TCP_WRAPPER_H */
