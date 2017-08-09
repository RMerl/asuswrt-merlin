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
    int             i;
    int             fd;
    int             ret;
    int             debug = 0;
    int             mode = 0666;
    int             savedErrno;
    char            buffer[2048]; 
    char            path[2048];
    char *          p;
    time_t          t0;
    time_t          t1;
    struct stat     st; 
    
    smbc_init(get_auth_data_fn, debug); 
    
    printf("CAUTION: This program will overwrite a file.  "
           "Press ENTER to continue.");
    fgets(buffer, sizeof(buffer), stdin);
           

    for (;;)
    {
        fprintf(stdout, "\nPath: ");
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
    
        if ((fd = smbc_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0)) < 0)
        {
            perror("smbc_open");
            continue;
        }

        strcpy(buffer, "Hello world\n");

        ret = smbc_write(fd, buffer, strlen(buffer));
        savedErrno = errno;
        smbc_close(fd);

        if (ret < 0)
        {
            errno = savedErrno;
            perror("write");
        }
    }

    return 0; 
}
