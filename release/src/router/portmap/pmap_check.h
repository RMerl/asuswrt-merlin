/* pmap_check.h */

extern int from_local(struct sockaddr_in *addr);
extern void check_startup(void);
extern int check_default(struct sockaddr_in *addr,
			 u_long  proc, u_long  prog);
#ifdef LOOPBACK_SETUNSET
extern int
check_setunset(SVCXPRT *xprt, SVCXPRT *ludp_xprt, SVCXPRT *ltcp_xprt,
	       u_long  proc, u_long  prog, u_long  port);
#else
extern int
check_setunset(struct sockaddr_in *addr, u_long  proc,
	       u_long  prog, u_long  port);
#endif
extern int check_privileged_port(struct sockaddr_in *addr,
				 u_long  proc,
				 u_long  prog, u_long  port);
extern int check_callit(struct sockaddr_in *addr, u_long  proc,
			u_long  prog, u_long  aproc);
extern int verboselog __attribute__ ((visibility ("hidden")));
extern int allow_severity __attribute__ ((visibility ("hidden")));
extern int deny_severity __attribute__ ((visibility ("hidden")));

#ifdef LOOPBACK_SETUNSET
#define CHECK_SETUNSET	check_setunset
#else
#define CHECK_SETUNSET(xprt,ludp,ltcp,proc,prog,port) \
	check_setunset(svc_getcaller(xprt),proc,prog,port)
#endif

extern int daemon_uid;
extern int daemon_gid;
