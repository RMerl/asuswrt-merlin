/*
 * Lookup a user by name
 */
 
#include <stdio.h>
#include <pwd.h>
#include <sys/types.h>

int main(int argc, char **argv)
{
    struct passwd *pw;
    
    /* Check args */

    if (argc != 2) {
        printf("ERROR: no arg specified\n");
        exit(1);
    }

    /* Do getpwnam() */

    if ((pw = getpwnam(argv[1])) == NULL) {
        printf("FAIL: user %s does not exist\n", argv[1]);
        exit(1);
    }

    printf("PASS: user %s exists\n", argv[1]);
    printf("pw_name = %s\n", pw->pw_name);
    printf("pw_passwd = %s\n", pw->pw_passwd);
    printf("pw_uid = %d\n", pw->pw_uid);
    printf("pw_gid = %d\n", pw->pw_gid);
    printf("pw_gecos = %s\n", pw->pw_gecos);
    printf("pw_dir = %s\n", pw->pw_dir);
    printf("pw_shell = %s\n", pw->pw_shell);
    
    exit(0);
}
