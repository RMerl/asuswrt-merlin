#!/bin/sh
#
# This script can be used to run the installed neon test suite
# against an installed copy of the neon library.
# 

# enable glibc malloc safety checks
MALLOC_CHECK_=2
export MALLOC_CHECK_

cd data

if test -x ../bin/ssl; then
    rm -rf ca

    echo "INIT: generating SSL ceritifcates..."
    if sh ./makekeys.sh 2> makekeys.out; then :; else
        echo FAIL: could not generate SSL certificates
        exit 2
    fi
fi

for t in ../bin/*; do
    if ${t}; then :; else
        echo FAIL: ${t}
        exit 1
    fi
done

echo "PASS: all tests passed"

exit 0
