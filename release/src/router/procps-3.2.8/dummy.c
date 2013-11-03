// This is to test the compiler.

#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Foul POS defines all sorts of stuff...
#include <term.h>
#undef tab

#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <values.h>

int main(int argc, char *argv[]){
  (void)argc;
  (void)argv;
  return 0;
}
