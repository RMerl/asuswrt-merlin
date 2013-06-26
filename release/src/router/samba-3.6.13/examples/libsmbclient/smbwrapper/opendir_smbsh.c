#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <libsmbclient.h>

int
main(int argc, char * argv[])
{
    char *          p;
    char            buf[1024];
    DIR *           dir;
    struct dirent * dirent;

    setbuf(stdout, NULL);

    for (fputs("path: ", stdout), p = fgets(buf, sizeof(buf), stdin);
         p != NULL && *p != '\n' && *p != '\0';
         fputs("path: ", stdout), p = fgets(buf, sizeof(buf), stdin))
    {
        if ((p = strchr(buf, '\n')) != NULL)
        {
            *p = '\0';
        }
        
        printf("Opening (%s)...\n", buf);
        
        if ((dir = opendir(buf)) == NULL)
        {
            printf("Could not open directory [%s]: \n",
                   buf, strerror(errno));
            continue;
        }

        while ((dirent = readdir(dir)) != NULL)
        {
            printf("%-30s", dirent->d_name);
            printf("%-30s", dirent->d_name + strlen(dirent->d_name) + 1);
            printf("\n");
        }

        closedir(dir);
    }

    exit(0);
}
