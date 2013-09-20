#!/bin/sh
# This is an extension of ./configure.
# I am checking to see if I should use GNU strftime
# This is essential of the %z or %Z options used.

CC=$1

check_strftime()
{
retval=""

cat << EOF > /tmp/strftime_try.c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

int
main (void)
{
  time_t tim;
  char buf[100] = {0};
  struct tm *timeval;

  tim = time(NULL);
  timeval = localtime(&tim);
  strftime (buf, 99, "%z", timeval);
  if (!isdigit(buf[1]))
    return (EXIT_FAILURE);

  return (EXIT_SUCCESS);
}

EOF

$CC -o /tmp/strftime /tmp/strftime_try.c $LIBS 2>&1 > /dev/null
if [ $? != 0 ]; then
  echo "ERROR: Could not compile strftime()."
  echo "ERROR: Please make sure you have the latest version of Glibc."
  exit;
fi

#Execute program to see if %z worked
/tmp/strftime
if [ $? -eq 0 ]; then
  retval=0
else
  retval=1
fi

rm -f /tmp/strftime*
return $retval;
}

check_strftime
exit $?
