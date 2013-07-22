#ifndef IS_MOUNTED_H
#define IS_MOUNTED_H

#define MF_MOUNTED	1
#define MF_ISROOT	2
#define MF_READONLY	4
#define MF_SWAP		8
#define MF_BUSY		16

extern int is_mounted(const char *file);
extern int check_mount_point(const char *device, int *mount_flags,
				 char *mtpt, int mtlen);

#endif /* IS_MOUNTED_H */
