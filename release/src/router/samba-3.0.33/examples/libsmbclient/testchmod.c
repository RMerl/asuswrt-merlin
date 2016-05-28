#include <stdio.h> 
#include <unistd.h>
#include <string.h> 
#include <time.h> 
#include <libsmbclient.h> 
#include "get_auth_data_fn.h"


int main(int argc, char * argv[]) 
{ 
    int             ret;
    int             debug = 0;
    int             mode = 0666;
    char            buffer[16384]; 
    char *          pSmbPath = NULL;
    struct stat     st; 
    
    if (argc == 1)
    {
        pSmbPath = "smb://RANDOM/Public/small";
    }
    else if (argc == 2)
    {
        pSmbPath = argv[1];
    }
    else if (argc == 3)
    {
        pSmbPath = argv[1];
        mode = (int) strtol(argv[2], NULL, 8);
    }
    else
    {
        printf("usage: "
               "%s [ smb://path/to/file [ octal_mode ] ]\n",
               argv[0]);
        return 1;
    }

    smbc_init(get_auth_data_fn, debug); 
    
    if (smbc_stat(pSmbPath, &st) < 0)
    {
        perror("smbc_stat");
        return 1;
    }
    
    printf("\nBefore chmod: mode = %04o\n", st.st_mode);
    
    if (smbc_chmod(pSmbPath, mode) < 0)
    {
        perror("smbc_chmod");
        return 1;
    }

    if (smbc_stat(pSmbPath, &st) < 0)
    {
        perror("smbc_stat");
        return 1;
    }
    
    printf("After chmod: mode = %04o\n", st.st_mode);
    
    return 0; 
}
