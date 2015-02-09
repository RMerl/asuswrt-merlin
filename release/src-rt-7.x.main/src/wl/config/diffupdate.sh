#!/bin/sh
# usage: this_program file1 file2

if test -r $2 ; then
  if cmp $1 $2 > /dev/null ; then
#    echo $2 is unchanged
    rm -f $1
  else
    mv -f $1 $2
  fi
else
  mv -f $1 $2
fi
