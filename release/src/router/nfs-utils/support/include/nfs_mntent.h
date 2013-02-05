/*
 * 2006-06-08 Amit Gud <agud@redhat.com>
 * - Moved code snippets here from util-linux/mount/my_mntent.h
 */

#ifndef _NFS_MNTENT_H
#define _NFS_MNTENT_H
#include <mntent.h>

#define ERR_MAX 5

typedef struct mntFILEstruct {
	FILE *mntent_fp;
	char *mntent_file;
	int mntent_lineno;
	int mntent_errs;
	int mntent_softerrs;
} mntFILE;

mntFILE *nfs_setmntent (const char *file, char *mode);
void nfs_endmntent (mntFILE *mfp);
int nfs_addmntent (mntFILE *mfp, struct mntent *mnt);
struct nfs_mntent *my_getmntent (mntFILE *mfp);
struct mntent *nfs_getmntent (mntFILE *mfp);

#endif /* _NFS_MNTENT_H */
