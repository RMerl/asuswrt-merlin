#include <stdio.h> 
#include <unistd.h>
#include <string.h> 
#include <time.h> 
#include <libsmbclient.h> 
#include "get_auth_data_fn.h"


int main(int argc, char * argv[]) 
{ 
    int             debug = 0;
    char            buffer[16384]; 
    char            mtime[32];
    char            ctime[32];
    char            atime[32];
    char *          pSmbPath = NULL;
    char *          pLocalPath = NULL;
    struct stat     st; 
    
    if (argc == 1)
    {
        pSmbPath = "smb://RANDOM/Public/small";
        pLocalPath = "/random/home/samba/small";
    }
    else if (argc == 2)
    {
        pSmbPath = argv[1];
        pLocalPath = NULL;
    }
    else if (argc == 3)
    {
        pSmbPath = argv[1];
        pLocalPath = argv[2];
    }
    else
    {
        printf("usage: "
               "%s [ smb://path/to/file [ /nfs/or/local/path/to/file ] ]\n",
               argv[0]);
        return 1;
    }

    smbc_init(get_auth_data_fn, debug); 
    
    if (smbc_stat(pSmbPath, &st) < 0)
    {
        perror("smbc_stat");
        return 1;
    }
    
    printf("\nSAMBA\n mtime:%lu/%s ctime:%lu/%s atime:%lu/%s\n",
           st.st_mtime, ctime_r(&st.st_mtime, mtime),
           st.st_ctime, ctime_r(&st.st_ctime, ctime),
           st.st_atime, ctime_r(&st.st_atime, atime)); 
    
    if (pLocalPath != NULL)
    {
        if (stat(pLocalPath, &st) < 0)
        {
            perror("stat");
            return 1;
        }
        
        printf("LOCAL\n mtime:%lu/%s ctime:%lu/%s atime:%lu/%s\n",
               st.st_mtime, ctime_r(&st.st_mtime, mtime),
               st.st_ctime, ctime_r(&st.st_ctime, ctime),
               st.st_atime, ctime_r(&st.st_atime, atime)); 
    }

    return 0; 
}
