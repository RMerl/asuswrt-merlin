/*
 * host.h
 *
 * Defaults for nlmtest
 */

#ifndef NLMTEST_HOST_H
#define NLMTEST_HOST_H

/*
 * The host on which lockd runs
 */
#define NLMTEST_HOST		"crutch"

/*
 * NFS mount point
 */
#define NLMTEST_DIR		"../../mount/"

/*
 * The default file name and its inode version number.
 * There's no way the test program can find out the version number,
 * so you have to add it here.
 */
#define NLMTEST_FILE		NLMTEST_DIR "COPYING"
#define NLMTEST_VERSION		1

#endif /* NLMTEST_HOST_H */
