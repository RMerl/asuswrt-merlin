#include <libsmbclient.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "get_auth_data_fn.h"

/*
 * This test is intended to ensure that the timestamps returned by
 * libsmbclient are the same as timestamps returned by the local system.  To
 * test this, we assume a working Samba environment, and access the same
 * file via SMB and locally (or NFS).
 *
 */


static int gettime(const char * pUrl,
                   const char * pLocalPath);


int main(int argc, char* argv[])
{
        if(argc != 3)
        {
                printf("usage: %s <file_url> <file_localpath>\n", argv[0]);
                return 1;
        }

        gettime(argv[1], argv[2]);
        return 0;
}


static int gettime(const char * pUrl,
                   const char * pLocalPath)
{
        //char *pSmbPath = 0;
        struct stat st;
        char mtime[32];
        char ctime[32];
        char atime[32];
        
        smbc_init(get_auth_data_fn, 0);
        
        if (smbc_stat(pUrl, &st) < 0)
        {
                perror("smbc_stat");
                return 1;
        }
        
        printf("SAMBA\n mtime:%lu/%s ctime:%lu/%s atime:%lu/%s\n",
               st.st_mtime, ctime_r(&st.st_mtime, mtime),
               st.st_ctime, ctime_r(&st.st_ctime, ctime),
               st.st_atime, ctime_r(&st.st_atime, atime)); 
        
        
        // check the stat on this file
        if (stat(pLocalPath, &st) < 0)
        {
                perror("stat");
                return 1;
        }
        
        printf("LOCAL\n mtime:%lu/%s ctime:%lu/%s atime:%lu/%s\n",
               st.st_mtime, ctime_r(&st.st_mtime, mtime),
               st.st_ctime, ctime_r(&st.st_ctime, ctime),
               st.st_atime, ctime_r(&st.st_atime, atime));
        
        
        return 0;
}

