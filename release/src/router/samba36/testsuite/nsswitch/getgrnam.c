/*
 * Lookup a group by name
 */
 
#include <stdio.h>
#include <grp.h>
#include <sys/types.h>

int main(int argc, char **argv)
{
    struct group *gr;
    
    /* Check args */

    if (argc != 2) {
        printf("ERROR: no arg specified\n");
        exit(1);
    }

    /* Do getgrnam() */

    if ((gr = getgrnam(argv[1])) == NULL) {
        printf("FAIL: group %s does not exist\n", argv[1]);
        exit(1);
    }

    /* Print group info */

    printf("PASS: group %s exists\n", argv[1]);
    printf("gr_name = %s\n", gr->gr_name);
    printf("gr_passwd = %s\n", gr->gr_passwd);
    printf("gr_gid = %d\n", gr->gr_gid);
    
    /* Group membership */

    if (gr->gr_mem != NULL) {
        int i = 0;

        printf("gr_mem = ");
        while(gr->gr_mem[i] != NULL) {
            printf("%s", gr->gr_mem[i]);
            i++;
            if (gr->gr_mem != NULL) {
                printf(",");
            }
        }
        printf("\n");
    }

    exit(0);
}
