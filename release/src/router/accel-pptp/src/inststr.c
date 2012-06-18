/* inststr.c ... stolen from bdupdate.c, which stole it from perl 4.
 *               Theft by C. Scott Ananian <cananian@alumni.princeton.edu>
 *               Modified similarly to perl 5.8.3's alignment-checking code
8                by Paul Howarth <paul@city-fan.org>
 *
 * $Id: inststr.c,v 1.3 2004/06/08 23:40:14 quozl Exp $
 */

#include <string.h>

#define PTRSIZE sizeof(void *)
typedef unsigned long UL;

void
inststr(int argc, char **argv, char **environ, char *src)
{
    if (strlen(src) <= strlen(argv[0]))
    {
        char *ptr;

        for (ptr = argv[0]; *ptr; *(ptr++) = '\0');

        strcpy(argv[0], src);
    } else
    {
        /* Stolen from the source to perl 4.036 (assigning to $0) */
        /* Modified to allow for aligned array members, assuming  */
        /* no system has something bizarre like the argv[]        */
        /* interleaved with some other data. Also allow for argv  */
        /* array having higher numbered elements lower in memory  */
        /* than lower numbered elements.                          */
        char *ptr, *ptr2;
        int count;
        UL mask = ~(UL)(PTRSIZE == 4 ? 3 : PTRSIZE == 8 ? 7 : PTRSIZE == 16 ? 15 : 0);
        int aligned = (mask < ~(UL)0) && (((UL)(argv[0]) & mask) == (UL)(argv[0]));
        ptr = argv[0] + strlen(argv[0]);
        if (argv[argc - 1] >= argv[1]) {
            /* argv pointers in ascending memory order */
            for (count = 1; count < argc; count++) {
                if (argv[count] == ptr + 1
                    ||
                    (aligned && argv[count] > ptr && argv[count] <= (char *)((UL)(ptr + PTRSIZE) & mask))
                   ) {
                    ptr = argv[count] + strlen(argv[count]);
                }
            }
        } else
        {
            /* sometimes the argv pointers go down in memory rather than up */
            for (count = argc - 1; count > 0; count--) {
                if (argv[count] == ptr + 1
                    ||
                    (aligned && argv[count] > ptr && argv[count] <= (char *)((UL)(ptr + PTRSIZE) & mask))
                   ) {
                    ptr = argv[count] + strlen(argv[count]);
                }
            }
        }
        for (count = 0; environ[count]; count++) {
            if (environ[count] == ptr + 1
                ||
                (aligned && environ[count] > ptr && environ[count] <= (char *)((UL)(ptr + PTRSIZE) & mask))
               ) {
                   ptr = environ[count] + strlen(environ[count]);
            }
        }
        count = 0;
        for (ptr2 = argv[0]; ptr2 <= ptr; ptr2++) {
            *ptr2 = '\0';
            count++;
        }
        strncpy(argv[0], src, count);
    }
}
