#ifndef __FLASH_MTD_H
#define __FLASH_MTD_H

#define NUM_INFO 5
#define MAX_READ_CNT 0x10000

struct mtd_info {
	char dev[8];
	int size;
	int erasesize;
	char name[12];
};

//extern void flash_mtd_usage(char *cmd);
extern int flash_mtd_init_info(void);
extern int flash_mtd_open(int num, int flags);
extern int flash_mtd_read(int offset, int count);
extern int FRead(char *dst, int src, int count);
extern int flash_mtd_write(int offset, int value);
extern int FWrite(char *src, int dst, int count);
extern int flash_mtd_erase(int start, int end);
//extern int flash_mtd_main(int argc, char *argv[]);

#endif
