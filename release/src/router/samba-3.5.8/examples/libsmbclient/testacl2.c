#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <popt.h>
#include "libsmbclient.h"
#include "get_auth_data_fn.h"

enum acl_mode
{
    SMB_ACL_GET,
    SMB_ACL_SET,
    SMB_ACL_DELETE,
    SMB_ACL_MODIFY,
    SMB_ACL_ADD,
    SMB_ACL_CHOWN,
    SMB_ACL_CHGRP
};


int main(int argc, const char *argv[])
{
    int i;
    int opt;
    int flags;
    int debug = 0;
    int numeric = 0;
    int full_time_names = 0;
    enum acl_mode mode = SMB_ACL_GET;
    static char *the_acl = NULL;
    int ret;
    char *p;
    char *debugstr;
    char value[1024];

    if (smbc_init(get_auth_data_fn, debug) != 0)
    {
        printf("Could not initialize smbc_ library\n");
        return 1;
    }

    SMBCCTX *context = smbc_set_context(NULL);
    smbc_setOptionFullTimeNames(context, 1);
    
    the_acl = strdup("system.nt_sec_desc.*");
    ret = smbc_getxattr(argv[1], the_acl, value, sizeof(value));
    if (ret < 0)
    {
        printf("Could not get attributes for [%s] %d: %s\n",
               argv[1], errno, strerror(errno));
        return 1;
    }
    
    printf("Attributes for [%s] are:\n%s\n", argv[1], value);

    flags = 0;
    debugstr = "set attributes (1st time)";
        
    ret = smbc_setxattr(argv[1], the_acl, value, strlen(value), flags);
    if (ret < 0)
    {
        printf("Could not %s for [%s] %d: %s\n",
               debugstr, argv[1], errno, strerror(errno));
        return 1;
    }
    
    flags = 0;
    debugstr = "set attributes (2nd time)";
        
    ret = smbc_setxattr(argv[1], the_acl, value, strlen(value), flags);
    if (ret < 0)
    {
        printf("Could not %s for [%s] %d: %s\n",
               debugstr, argv[1], errno, strerror(errno));
        return 1;
    }
    
    return 0;
}
