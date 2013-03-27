#!/bin/sh

# A shell script to run the test suite on the DJGPP version of GDB.

ORIGDIR=`pwd`
GDB=${ORIGDIR}/../gdb.exe
SUBDIRS=`find $ORIGDIR -type d ! -ipath $ORIGDIR`

for d in $SUBDIRS
do
  cd $d
  echo "Running tests in $d..."
  for f in *.out
  do
    test -f $f || break
    base=`basename $f .out`
    if test "${base}" = "dbx" ; then
	options=-dbx
    else
	options=
    fi
    $GDB ${options} < ${base}.in 2>&1 \
      | sed -e '/GNU gdb /s/ [.0-9][.0-9]*//' \
            -e '/^Copyright/s/[12][0-9][0-9][0-9]/XYZZY/g' \
            -e '/Starting program: /s|[A-z]:/.*/||' \
            -e '/main (/s/=0x[0-9a-f][0-9a-f]*/=XYZ/g' \
      > ${base}.tst
    if diff --binary -u ${base}.out ${base}.tst ; then
      rm -f ${base}.tst
    fi
  done
done

