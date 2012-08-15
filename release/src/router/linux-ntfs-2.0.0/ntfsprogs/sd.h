#ifndef _NTFS_SD_H_
#define _NTFS_SD_H_

#include "types.h"

void init_system_file_sd(int sys_file_no, u8 **sd_val, int *sd_val_len);
void init_root_sd(u8 **sd_val, int *sd_val_len);
void init_secure_sds(char *sd_val);

#endif /* _NTFS_SD_H_ */

