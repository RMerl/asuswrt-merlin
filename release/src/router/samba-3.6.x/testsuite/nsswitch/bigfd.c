/*
 * Test maximum number of file descriptors winbind daemon can handle
 */

#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>

int main(int argc, char **argv)
{
    struct passwd *pw;
    int i;

    while(1) {
        
        /* Start getpwent until we get an NT user.  This way we know we
           have at least opened a connection to the winbind daemon */

        setpwent();

        while((pw = getpwent()) != NULL) {
            if (strchr(pw->pw_name, '/') != NULL) {
                break;
            }
        }

        if (pw != NULL) {
            i++;
            printf("got pwent handle %d\n", i);
        } else {
            printf("winbind daemon not running?\n");
            exit(1);
        }

        sleep(1);
    }
}
