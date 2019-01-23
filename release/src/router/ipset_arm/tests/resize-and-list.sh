#!/bin/sh

./resize.sh &
ipset=${IPSET_BIN:-../src/ipset}
n=0
while [ $n -ne 8161 ]; do
    # echo $n
    n=`$ipset -S resize-test 2>/dev/null| wc -l`
done
$ipset x
