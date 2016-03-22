/* @(#) txtf_read unit test */

#include <stdio.h>
#include <sys/types.h>

#include "util.h"

int main (int argc, char* const argv[])
{
    char txtbuf[2048];
    ssize_t n = -1;

    if (argc < 2) {
        (void) fprintf (stderr, "Usage: %s filepath\n", argv[0]);
        return 1;
    }

    n = txtf_read (argv[1], txtbuf, sizeof(txtbuf), stderr);
    (void) fprintf (stderr, "got %ld from txtf_read\n", n);
    if (n <= 0) return 1;

    (void) printf ("Read %ld bytes from %s:\n", n, argv[0]);
    (void) printf ("<data>\n");
    (void) printf ("%s\n", txtbuf);
    (void) printf ("</data>\n");

    return 0;
}


/* __EOF__ */

