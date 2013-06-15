/*
 * Lookup a user by uid.
 */

#include <stdio.h>
#include <pwd.h>
#include <sys/types.h>

int main(int argc, char **argv)
{
    struct passwd *pw;
    uid_t uid;

    /* Check args */

    if (argc != 2) {
        printf("ERROR: no arg specified\n");
        exit(1);
    }

    if ((uid = atoi(argv[1])) == 0) {
        printf("ERROR: invalid uid specified\n");
        exit(1);
    }

    /* Do getpwuid() */

    if ((pw = getpwuid(uid)) == NULL) {
        printf("FAIL: uid %d does not exist\n", uid);
        exit(1);
    }
    
    printf("PASS: uid %d exists\n", uid);
    printf("pw_name = %s\n", pw->pw_name);
    printf("pw_passwd = %s\n", pw->pw_passwd);
    printf("pw_uid = %d\n", pw->pw_uid);
    printf("pw_gid = %d\n", pw->pw_gid);
    printf("pw_gecos = %s\n", pw->pw_gecos);
    printf("pw_dir = %s\n", pw->pw_dir);
    printf("pw_shell = %s\n", pw->pw_shell);

    exit(0);
}
