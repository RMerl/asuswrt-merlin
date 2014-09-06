/* HEADER Testing netsnmp_large_fd_set */

netsnmp_large_fd_set fds;
netsnmp_large_fd_set_init(&fds, 2000);
OKF(fds.lfs_setsize == 2000, ("initialization"));
OKF(netsnmp_large_fd_set_resize(&fds, 2000) == 1, ("resizing to 2000"));
NETSNMP_LARGE_FD_ZERO(&fds);

{
    int i;
    for (i = 0; i < fds.lfs_setsize; ++i) {
        OKF(!NETSNMP_LARGE_FD_ISSET(i, &fds), ("%d is not set", i));
	NETSNMP_LARGE_FD_SET(i, &fds);
	OKF(NETSNMP_LARGE_FD_ISSET(i, &fds), ("%d is set", i));
	NETSNMP_LARGE_FD_CLR(i, &fds);
	OKF(!NETSNMP_LARGE_FD_ISSET(i, &fds), ("%d is not set", i));
    }
}

OKF(netsnmp_large_fd_set_resize(&fds, 3000) == 1, ("resizing to 3000"));

{
    int i;
    for (i = 0; i < fds.lfs_setsize; ++i) {
        OKF(!NETSNMP_LARGE_FD_ISSET(i, &fds), ("%d is not set", i));
	NETSNMP_LARGE_FD_SET(i, &fds);
	OKF(NETSNMP_LARGE_FD_ISSET(i, &fds), ("%d is set", i));
	NETSNMP_LARGE_FD_CLR(i, &fds);
	OKF(!NETSNMP_LARGE_FD_ISSET(i, &fds), ("%d is not set", i));
    }
}

OKF(netsnmp_large_fd_set_resize(&fds, 1000) == 1, ("resizing to 1000"));

{
    int i;
    for (i = 0; i < fds.lfs_setsize; ++i) {
        OKF(!NETSNMP_LARGE_FD_ISSET(i, &fds), ("%d is not set", i));
	NETSNMP_LARGE_FD_SET(i, &fds);
	OKF(NETSNMP_LARGE_FD_ISSET(i, &fds), ("%d is set", i));
	NETSNMP_LARGE_FD_CLR(i, &fds);
	OKF(!NETSNMP_LARGE_FD_ISSET(i, &fds), ("%d is not set", i));
    }
}

netsnmp_large_fd_set_cleanup(&fds);
