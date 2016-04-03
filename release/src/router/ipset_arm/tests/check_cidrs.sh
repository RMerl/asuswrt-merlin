#!/bin/sh

set -e

ipset=${IPSET_BIN:-../src/ipset}

$ipset f
$ipset x
$ipset n test hash:net
for x in `seq 1 32`; do
    $ipset a test 10.0.0.0/$x
    n=`$ipset l test | wc -l`
    n=$((n - 8))
    test $n -eq $x || exit 1
done
for x in `seq 32 -1 1`; do
    $ipset d test 10.0.0.0/$x
    n=`$ipset l test | wc -l`
    # We deleted one element
    n=$((n - 8 + 1))
    test $n -eq $x || exit 1
done
$ipset x test
