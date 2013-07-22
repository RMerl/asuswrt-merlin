/*
 *  procs.c -- functions to parse the linux /proc filesystem.
 *  (c) 1994 salvatore valente <svalente@mit.edu>
 *
 *   this program is free software.  you can redistribute it and
 *   modify it under the terms of the gnu general public license.
 *   there is no warranty.
 *
 *   faith
 *   1.2
 *   1995/02/23 01:20:40
 *
 */

#define _POSIX_SOURCE 1

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include "kill.h"

extern char *mybasename (char *);
static char *parse_parens (char *buf);

int *
get_pids (char *process_name, int get_all) {
    DIR *dir;
    struct dirent *ent;
    int status;
    char *dname, fname[100], *cp, buf[256];
    struct stat st;
    uid_t uid;
    FILE *fp;
    int pid, *pids, num_pids, pids_size;

    dir = opendir ("/proc");
    if (! dir) {
	perror ("opendir /proc");
	return NULL;
    }
    uid = getuid ();
    pids = NULL;
    num_pids = pids_size = 0;

    while ((ent = readdir (dir)) != NULL) {
	dname = ent->d_name;
	if (! isdigit (*dname)) continue;
	pid = atoi (dname);
	sprintf (fname, "/proc/%d/cmdline", pid);
	/* get the process owner */
	status = stat (fname, &st);
	if (status != 0) continue;
	if (! get_all && uid != st.st_uid) continue;
	/* get the command line */
	fp = fopen (fname, "r");
	if (! fp) continue;
	cp = fgets (buf, sizeof (buf), fp);
	fclose (fp);
	/* an empty command line means the process is swapped out */
	if (! cp || ! *cp) {
	    /* get the process name from the statfile */
	    sprintf (fname, "/proc/%d/stat", pid);
	    fp = fopen (fname, "r");
	    if (! fp) continue;
	    cp = fgets (buf, sizeof (buf), fp);
	    if (cp == NULL) continue;
	    fclose (fp);
	    cp = parse_parens (buf);
	    if (cp == NULL) continue;
	}
	/* ok, we got the process name. */
	if (strcmp (process_name, mybasename (cp))) continue;
	while (pids_size < num_pids + 2) {
	    pids_size += 5;
	    pids = (int *) realloc (pids, sizeof (int) * pids_size);
	}
	pids[num_pids++] = pid;
	pids[num_pids] = -1;
    }
    closedir (dir);
    return (pids);
}

/*
 *  parse_parens () -- return an index just past the first open paren in
 *	buf, and terminate the string at the matching close paren.
 */
static char *parse_parens (char *buf)
{
    char *cp, *ip;
    int depth;

    cp = strchr (buf, '(');
    if (cp == NULL) return NULL;
    cp++;
    depth = 1;
    for (ip = cp; *ip; ip++) {
	if (*ip == '(')
	    depth++;
	if (*ip == ')') {
	    depth--;
	    if (depth == 0) {
		*ip = 0;
		break;
	    }
	}
    }
    return cp;
}

char *mybasename (char *path)
{
    char *cp;

    cp = strrchr (path, '/');
    return (cp ? cp + 1 : path);
}

