/*
 * Copyright (C) 1996 Olaf Kirch
 * Modified by Jeffrey A. Uphoff, 1997, 1999.
 *
 * NSM for Linux.
 */

/*
 * System-dependent declarations
 */

#ifdef FD_SETSIZE
# define FD_SET_TYPE	fd_set
# define SVC_FDSET	svc_fdset
#else
# define FD_SET_TYPE	int
# define SVC_FDSET	svc_fds
#endif
