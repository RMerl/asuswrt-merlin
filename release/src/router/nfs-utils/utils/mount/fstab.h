#ifndef _NFS_UTILS_MOUNT_FSTAB_H
#define _NFS_UTILS_MOUNT_FSTAB_H

#include "nfs_mntent.h"

#ifndef _PATH_FSTAB
#define _PATH_FSTAB "/etc/fstab"
#endif

int mtab_is_writable(void);
int mtab_does_not_exist(void);
void reset_mtab_info(void);

struct mntentchn {
	struct mntentchn *nxt, *prev;
	struct mntent m;
};

struct mntentchn *getmntoptfile (const char *file);
struct mntentchn *getmntdirbackward (const char *dir, struct mntentchn *mc);
struct mntentchn *getmntdevbackward (const char *dev, struct mntentchn *mc);

struct mntentchn *getfsfile (const char *file);
struct mntentchn *getfsspec (const char *spec);

void lock_mtab (void);
void unlock_mtab (void);
void update_mtab (const char *special, struct mntent *with);

#endif	/* _NFS_UTILS_MOUNT_FSTAB_H */
