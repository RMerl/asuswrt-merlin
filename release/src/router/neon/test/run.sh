#!/bin/sh

rm -f debug.log child.log

ulimit -c unlimited
ulimit -t 120

unset LANG
unset LC_MESSAGES

# Enable glibc heap consistency checks, and memory randomization.
MALLOC_CHECK_=2
MALLOC_PERTURB_=`expr $RANDOM % 255 2>/dev/null`
export MALLOC_CHECK_ MALLOC_PERTURB_

export TEST_QUIET=${TEST_QUIET:-1}

RETVAL=0

for f in $*; do
    if ${HARNESS} ./$f ${SRCDIR}; then
	:
    else
	RETVAL=$?
    fi
done

exit $RETVAL
