#!/bin/bash

set -e
set -x

d=$(dirname $0)

cd $d/..
PREFIX=$HOME/testprefix

if [ $# -gt 0 ]; then
    tests="$*"
else
    tests="lib/replace lib/talloc lib/tevent lib/tdb source4/lib/ldb"
fi

echo "testing in dirs $tests"

for d in $tests; do
    echo "`date`: testing $d"
    pushd $d
    rm -rf bin
    type waf
    waf dist
    ./configure -C --enable-developer --prefix=$PREFIX
    time make
    make install
    make distcheck
    case $d in
	"source4/lib/ldb")
	    ldd bin/ldbadd
	    ;;
	"lib/replace")
	    ldd bin/replace_testsuite
	    ;;
	"lib/talloc")
	    ldd bin/talloc_testsuite
	    ;;
	"lib/tdb")
	    ldd bin/tdbtool
	    ;;
    esac
    popd
done

echo "testing python portability"
pushd lib/talloc
versions="python2.4 python2.5 python2.6 python3.0 python3.1"
for p in $versions; do
    ret=$(which $p || echo "failed")
    if [ $ret = "failed" ]; then
        echo "$p not found, skipping"
        continue
    fi
    echo "Testing $p"
    $p ../../buildtools/bin/waf configure -C --enable-developer --prefix=$PREFIX
    $p ../../buildtools/bin/waf build install
done
popd

echo "testing cross compiling"
pushd lib/talloc
ret=$(which arm-linux-gnueabi-gcc || echo "failed")
if [ $ret != "failed" ]; then
    CC=arm-linux-gnueabi-gcc ./configure -C --prefix=$PREFIX  --cross-compile --cross-execute='runarm'
    make && make install
else
    echo "Cross-compiler not installed, skipping test"
fi
popd
