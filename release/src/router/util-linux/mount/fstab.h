#ifndef MOUNT_FSTAB_H
#define MOUNT_FSTAB_H

#include "mount_mntent.h"

#ifdef HAVE_LIBMOUNT_MOUNT
#define USE_UNSTABLE_LIBMOUNT_API
#include <libmount.h>
#endif

int mtab_is_writable(void);
int mtab_is_a_symlink(void);
int mtab_does_not_exist(void);
void reset_mtab_info(void);
int is_mounted_once(const char *name);

struct mntentchn {
	struct mntentchn *nxt, *prev;
	struct my_mntent m;
};

struct mntentchn *mtab_head (void);
struct mntentchn *getmntfile (const char *name);
struct mntentchn *getmntfilebackward (const char *name, struct mntentchn *mcprev);
struct mntentchn *getmntoptfile (const char *file);
struct mntentchn *getmntdirbackward (const char *dir, struct mntentchn *mc);
struct mntentchn *getmntdevbackward (const char *dev, struct mntentchn *mc);

struct mntentchn *fstab_head (void);
struct mntentchn *getfs_by_dir (const char *dir);
struct mntentchn *getfs_by_spec (const char *spec);
struct mntentchn *getfs_by_devname (const char *devname);
struct mntentchn *getfs_by_devdir (const char *dev, const char *dir);
struct mntentchn *getfs_by_uuid (const char *uuid);
struct mntentchn *getfs_by_label (const char *label);

void lock_mtab (void);
void unlock_mtab (void);
void update_mtab (const char *special, struct my_mntent *with);

char *get_option(const char *optname, const char *src, size_t *len);
char *get_option_value(const char *list, const char *s);

#endif /* MOUNT_FSTAB_H */
