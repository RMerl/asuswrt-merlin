/*
 * Use set/get/endpwent calls from two processes to iterate over the
 * password database.  This checks the multithreaded stuff works.
 */

#include <stdio.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <wait.h>

void dump_pwent(char *id)
{
    struct passwd *pw;
    char fname[255];
    FILE *fptr;

    /* Open results file */

    sprintf(fname, "/tmp/getpwent_r-%s.out-%d", id, getpid());

    if ((fptr = fopen(fname, "w")) < 0) {
        fprintf(stderr, "ERROR: could not open file %s: %s\n", fname,
                sys_errlist[errno]);
        return;
    }

    /* Dump passwd database */

    setpwent();
        
    while((pw = getpwent()) != NULL) {
       fprintf(fptr,"%s:%s:%s:%d:%d\n", pw->pw_name, pw->pw_passwd,
               pw->pw_gecos, pw->pw_uid, pw->pw_gid);
    }
        
    endpwent();

    /* Close results file */

    fclose(fptr);
}

#define NUM_FORKS 2

int main(int argc, char **argv)
{
    pid_t pids[NUM_FORKS];
    int i, status;

    /* Check args */

    if (argc != 2) {
        printf("ERROR: must specify output file identifier\n");
        return 1;
    }

    for(i = 0; i < NUM_FORKS; i++) {

        /* Fork off lots */

        if ((pids[i] = fork()) == -1) {
            perror("fork");
            return 1;
        }

        /* Child does tests */

        if (pids[i] == 0) {
            dump_pwent(argv[1]);
            return 0;
        }
    }

    /* Wait for everyone to finish */

    for (i = 0; i < NUM_FORKS; i++) {
        waitpid(pids[i], &status, 0);
    }

    printf("PASS: getpwent_r.c\n");
    return 0;
}
