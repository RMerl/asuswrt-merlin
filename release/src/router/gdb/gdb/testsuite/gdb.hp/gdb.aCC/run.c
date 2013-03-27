/*
 *	This simple classical example of recursion is useful for
 *	testing stack backtraces and such.
 */

#ifdef vxworks

#  include <stdio.h>

/* VxWorks does not supply atoi.  */
static int
atoi (char *z)
    /*  char *z;*/
{
  int i = 0;

  while (*z >= '0' && *z <= '9')
    i = i * 10 + (*z++ - '0');
  return i;
}

/* I don't know of any way to pass an array to VxWorks.  This function
   can be called directly from gdb.  */

void vxmain (char *arg)
/*char *arg;*/
{
  char *argv[2];

  argv[0] = "";
  argv[1] = arg;
  main (2, argv, (char **) 0);
}

#else /* ! vxworks */
#  include <stdio.h>
#  include <stdlib.h>
#endif /* ! vxworks */

int main (int argc, char *argv[], char **envp)
/*int argc;
char *argv[], **envp;*/
{
    int factorial (int);
#ifdef usestubs
    set_debug_traps();
    breakpoint();
#endif
#ifdef FAKEARGV
    printf ("%d\n", factorial (1));
#else    
    if (argc != 2) {
	printf ("usage:  factorial <number>\n");
	return 1;
    } else {
	printf ("%d\n", factorial (atoi (argv[1])));
    }
#endif
    return 0;
}

int factorial (int value)
/*int value;*/
{
    int  local_var;

    if (value > 1) {
	value *= factorial (value - 1);
    }
    local_var = value;
    return (value);
}
