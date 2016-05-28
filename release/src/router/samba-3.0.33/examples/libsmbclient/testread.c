#include <sys/types.h>
#include <stdio.h> 
#include <unistd.h>
#include <string.h> 
#include <time.h> 
#include <errno.h>
#include <libsmbclient.h> 
#include "get_auth_data_fn.h"


int main(int argc, char * argv[]) 
{ 
    int             fd;
    int             ret;
    int             debug = 0;
    int             mode = 0666;
    int             savedErrno;
    char            buffer[2048]; 
    char *          pSmbPath = NULL;
    time_t          t0;
    time_t          t1;
    struct stat     st; 
    
    if (argc == 1)
    {
        pSmbPath = "smb://RANDOM/Public/bigfile";
    }
    else if (argc == 2)
    {
        pSmbPath = argv[1];
    }
    else
    {
        printf("usage: "
               "%s [ smb://path/to/file ]\n",
               argv[0]);
        return 1;
    }

    smbc_init(get_auth_data_fn, debug); 
    
    printf("Open file %s\n", pSmbPath);
    
    t0 = time(NULL);

    if ((fd = smbc_open(pSmbPath, O_RDONLY, 0)) < 0)
    {
        perror("smbc_open");
        return 1;
    }

    printf("Beginning read loop.\n");

    do
    {
        ret = smbc_read(fd, buffer, sizeof(buffer));
        savedErrno = errno;
        if (ret > 0) fwrite(buffer, 1, ret, stdout);
    } while (ret > 0);

    smbc_close(fd);

    if (ret < 0)
    {
        errno = savedErrno;
        perror("read");
        return 1;
    }

    t1 = time(NULL);

    printf("Elapsed time: %d seconds\n", t1 - t0);

    return 0; 
}
