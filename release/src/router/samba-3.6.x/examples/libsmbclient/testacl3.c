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
    char            value[2048]; 
    char            path[2048];
    char *          the_acl;
    char *          p;
    time_t          t0;
    time_t          t1;
    struct stat     st; 
    SMBCCTX *       context;
    
    smbc_init(get_auth_data_fn, debug); 
    
    context = smbc_set_context(NULL);
    smbc_setOptionFullTimeNames(context, 1);
    
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
    
        the_acl = strdup("system.nt_sec_desc.*+");
        ret = smbc_getxattr(path, the_acl, value, sizeof(value));
        if (ret < 0)
        {
            printf("Could not get attributes for [%s] %d: %s\n",
                   path, errno, strerror(errno));
            return 1;
        }
    
        printf("Attributes for [%s] are:\n%s\n", path, value);
    }

    return 0; 
}
