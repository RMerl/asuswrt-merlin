/*
 * Lookup a group by gid.
 */

#include <stdio.h>
#include <grp.h>
#include <sys/types.h>

int main(int argc, char **argv)
{
    struct group *gr;
    gid_t gid;

    /* Check args */

    if (argc != 2) {
        printf("ERROR: no arg specified\n");
        exit(1);
    }

    if ((gid = atoi(argv[1])) == 0) {
        printf("ERROR: invalid gid specified\n");
        exit(1);
    }

    /* Do getgrgid() */

    if ((gr = getgrgid(gid)) == NULL) {
        printf("FAIL: gid %d does not exist\n", gid);
        exit(1);
    }
    
    /* Print group info */

    printf("PASS: gid %d exists\n", gid);
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
