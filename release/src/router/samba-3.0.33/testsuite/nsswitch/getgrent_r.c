/*
 * Use set/get/endgrent calls from two processes to iterate over the
 * password database.  This checks the multithreaded stuff works.
 */

#include <stdio.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <wait.h>

void dump_grent(char *id)
{
    struct group *gr;
    char fname[255];
    FILE *fptr;

    /* Open results file */

    sprintf(fname, "/tmp/getgrent_r-%s.out-%d", id, getpid());

    if ((fptr = fopen(fname, "w")) < 0) {
        fprintf(stderr, "ERROR: could not open file %s: %s\n", fname,
                sys_errlist[errno]);
        return;
    }

    /* Dump group database */

    setgrent();
        
    while((gr = getgrent()) != NULL) {
        fprintf(fptr,"%s:%s:%d:%d\n", gr->gr_name, gr->gr_passwd,
                gr->gr_gid);
    }
        
    endgrent();

    /* Close results file */

    fclose(fptr);
}

int main(int argc, char **argv)
{
    pid_t pid;

    /* Check args */

    if (argc != 2) {
        printf("ERROR: must specify output file identifier\n");
        return 1;
    }

    /* Fork child process */

    if ((pid = fork()) == -1) {
        printf("ERROR: unable to fork\n");
        return 1;
    }

    /* Handle test case */

    if (pid > 0) {
        int status;

        /* Parent */

        dump_grent(argv[1]);
        wait(&status);

    } else {

        /* Child */

        dump_grent(argv[1]);
        return 0;
    }

    printf("PASS: run getgrent_r.c\n");
    return 0;
}
