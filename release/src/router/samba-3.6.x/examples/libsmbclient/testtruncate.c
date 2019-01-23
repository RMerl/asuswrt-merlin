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
    int             savedErrno;
    char            buffer[128];
    char *          pSmbPath = NULL;
    char *          pLocalPath = NULL;
    struct stat     st; 
    
    if (argc != 2)
    {
        printf("usage: "
               "%s smb://path/to/file\n",
               argv[0]);
        return 1;
    }

    smbc_init(get_auth_data_fn, debug); 
    
    if ((fd = smbc_open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0)) < 0)
    {
        perror("smbc_open");
        return 1;
    }

    strcpy(buffer, "Hello world.\nThis is a test.\n");

    ret = smbc_write(fd, buffer, strlen(buffer));
    savedErrno = errno;
    smbc_close(fd);

    if (ret < 0)
    {
        errno = savedErrno;
        perror("write");
    }

    if (smbc_stat(argv[1], &st) < 0)
    {
        perror("smbc_stat");
        return 1;
    }
    
    printf("Original size: %lu\n", (unsigned long) st.st_size);
    
    if ((fd = smbc_open(argv[1], O_WRONLY, 0)) < 0)
    {
        perror("smbc_open");
        return 1;
    }

    ret = smbc_ftruncate(fd, 13);
    savedErrno = errno;
    smbc_close(fd);
    if (ret < 0)
    {
        errno = savedErrno;
        perror("smbc_ftruncate");
        return 1;
    }
    
    if (smbc_stat(argv[1], &st) < 0)
    {
        perror("smbc_stat");
        return 1;
    }
    
    printf("New size: %lu\n", (unsigned long) st.st_size);
    
    return 0; 
}
