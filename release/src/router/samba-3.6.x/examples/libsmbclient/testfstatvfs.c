#include <sys/types.h>
#include <sys/statvfs.h>
#include <stdio.h> 
#include <unistd.h>
#include <string.h> 
#include <time.h> 
#include <errno.h>
#include <libsmbclient.h> 
#include "get_auth_data_fn.h"


int main(int argc, char * argv[]) 
{ 
    int             i;
    int             fd;
    int             ret;
    int             debug = 0;
    char *          p;
    char            path[2048];
    struct stat     statbuf;
    struct statvfs  statvfsbuf;
    
    smbc_init(get_auth_data_fn, debug); 
    
    for (;;)
    {
        fprintf(stdout, "Path: ");
        *path = '\0';
        fgets(path, sizeof(path) - 1, stdin);
        if (strlen(path) == 0)
        {
            return 0;
        }

        p = path + strlen(path) - 1;
        if (*p == '\n')
        {
            *p = '\0';
        }
    
        /* Determine if it's a file or a folder */
        if (smbc_stat(path, &statbuf) < 0)
        {
            perror("smbc_stat");
            continue;
        }

        if (S_ISREG(statbuf.st_mode))
        {
            if ((fd = smbc_open(path, O_RDONLY, 0)) < 0)
            {
                perror("smbc_open");
                continue;
            }
        }
        else
        {
            if ((fd = smbc_opendir(path)) < 0)
            {
                perror("smbc_opendir");
                continue;
            }
        }

        ret = smbc_fstatvfs(fd, &statvfsbuf);

        smbc_close(fd);

        if (ret < 0)
        {
            perror("fstatvfs");
        }
        else
        {
            printf("\n");
            printf("Block Size: %lu\n", statvfsbuf.f_bsize);
            printf("Fragment Size: %lu\n", statvfsbuf.f_frsize);
            printf("Blocks: %llu\n",
                   (unsigned long long) statvfsbuf.f_blocks);
            printf("Free Blocks: %llu\n",
                   (unsigned long long) statvfsbuf.f_bfree);
            printf("Available Blocks: %llu\n",
                   (unsigned long long) statvfsbuf.f_bavail);
            printf("Files : %llu\n",
                   (unsigned long long) statvfsbuf.f_files);
            printf("Free Files: %llu\n",
                   (unsigned long long) statvfsbuf.f_ffree);
            printf("Available Files: %llu\n",
                   (unsigned long long) statvfsbuf.f_favail);
            printf("File System ID: %lu\n",
                   (unsigned long) statvfsbuf.f_fsid);
            printf("\n");

            printf("Flags: 0x%lx\n", statvfsbuf.f_flag);
            printf("Extended Features: ");

            if (statvfsbuf.f_flag & SMBC_VFS_FEATURE_NO_UNIXCIFS)
            {
                printf("NO_UNIXCIFS ");
            }
            else
            {
                printf("unixcifs ");
            }

            if (statvfsbuf.f_flag & SMBC_VFS_FEATURE_CASE_INSENSITIVE)
            {
                printf("CASE_INSENSITIVE ");
            }
            else
            {
                printf("case_sensitive ");
            }

            if (statvfsbuf.f_flag & SMBC_VFS_FEATURE_DFS)
            {
                printf("DFS ");
            }
            else
            {
                printf("no_dfs ");
            }

            printf("\n");
        }
    }

    return 0; 
}
