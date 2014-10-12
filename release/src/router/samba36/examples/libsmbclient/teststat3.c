#include <libsmbclient.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "get_auth_data_fn.h"

/*
 * This test is intended to ensure that the timestamps returned by
 * libsmbclient using smbc_stat() are the same as those returned by
 * smbc_fstat().
 */


int main(int argc, char* argv[])
{
        int             fd;
        struct stat     st1;
        struct stat     st2;
        char            mtime[32];
        char            ctime[32];
        char            atime[32];
        char *          pUrl = argv[1];

        if(argc != 2)
        {
                printf("usage: %s <file_url>\n", argv[0]);
                return 1;
        }

        
        smbc_init(get_auth_data_fn, 0);
        
        if (smbc_stat(pUrl, &st1) < 0)
        {
                perror("smbc_stat");
                return 1;
        }
        
        if ((fd = smbc_open(pUrl, O_RDONLY, 0)) < 0)
        {
                perror("smbc_open");
                return 1;
        }

        if (smbc_fstat(fd, &st2) < 0)
        {
                perror("smbc_fstat");
                return 1;
        }
        
        smbc_close(fd);

#define COMPARE(name, field)                                            \
        if (st1.field != st2.field)                                     \
        {                                                               \
                printf("Field " name " MISMATCH: st1=%lu, st2=%lu\n",   \
                       (unsigned long) st1.field,                       \
                       (unsigned long) st2.field);                      \
        }

        COMPARE("st_dev", st_dev);
        COMPARE("st_ino", st_ino);
        COMPARE("st_mode", st_mode);
        COMPARE("st_nlink", st_nlink);
        COMPARE("st_uid", st_uid);
        COMPARE("st_gid", st_gid);
        COMPARE("st_rdev", st_rdev);
        COMPARE("st_size", st_size);
        COMPARE("st_blksize", st_blksize);
        COMPARE("st_blocks", st_blocks);
        COMPARE("st_atime", st_atime);
        COMPARE("st_mtime", st_mtime);
        COMPARE("st_ctime", st_ctime);

        return 0;
}

