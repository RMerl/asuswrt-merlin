#!/bin/sh

# set -x
set -e

ipset=${IPSET_BIN:-../src/ipset}

case "$1" in
    -4)
    	ip=10.0.
    	sep=.
    	net=32
    	ip2=192.168.162.33
    	;;
    -6)
    	ip=10::
    	sep=:
    	net=128
    	ip2=192:168::162:33
    	;;
esac

case "$2" in
    ipportnet)
    	$ipset n test hash:ip,port,net $1 hashsize 64
    	for x in `seq 0 16`; do
    	    for y in `seq 0 255`; do
    	    	$ipset a test $ip$x$sep$y,1023,$ip2/$net nomatch
    	    done
    	done
    	for x in `seq 0 16`; do
    	    for y in `seq 0 255`; do
    	    	$ipset t test $ip$x$sep$y,1023,$ip2/$net nomatch 2>/dev/null
    	    done
    	done
    	;;
    netportnet)
    	$ipset n test hash:net,port,net $1 hashsize 64
    	for x in `seq 0 16`; do
    	    for y in `seq 0 255`; do
    	    	$ipset a test $ip$x$sep$y,1023,$ip2/$net nomatch
    	    done
    	done
    	for x in `seq 0 16`; do
    	    for y in `seq 0 255`; do
    	    	$ipset t test $ip$x$sep$y,1023,$ip2/$net nomatch 2>/dev/null
    	    done
    	done
    	;;
    net)
    	$ipset n test hash:net $1 hashsize 64
    	for x in `seq 0 16`; do
    	    for y in `seq 0 255`; do
    	    	$ipset a test $ip$x$sep$y/$net nomatch
    	    done
    	done
    	for x in `seq 0 16`; do
    	    for y in `seq 0 255`; do
    	    	$ipset t test $ip$x$sep$y/$net nomatch 2>/dev/null
    	    done
    	done
    	;;
    netnet)
	$ipset n test hash:net,net $1 hashsize 64
	for x in `seq 0 16`; do
	    for y in `seq 0 255`; do
		$ipset a test $ip$x$sep$y/$net,$ip$y$sep$x/$net nomatch
	    done
	done
	for x in `seq 0 16`; do
	    for y in `seq 0 255`; do
		$ipset t test $ip$x$sep$y/$net,$ip$y$sep$x/$net nomatch \
		2>/dev/null
	    done
	done
	;;
    netport)
    	$ipset n test hash:net,port $1 hashsize 64
    	for x in `seq 0 16`; do
    	    for y in `seq 0 255`; do
    	    	$ipset a test $ip$x$sep$y/$net,1023 nomatch
    	    done
    	done
    	for x in `seq 0 16`; do
    	    for y in `seq 0 255`; do
    	    	$ipset t test $ip$x$sep$y/$net,1023 nomatch 2>/dev/null
    	    done
    	done
    	;;
    netiface)
    	$ipset n test hash:net,iface $1 hashsize 64
    	for x in `seq 0 16`; do
    	    for y in `seq 0 255`; do
    	    	$ipset a test $ip$x$sep$y/$net,eth0 nomatch
    	    done
    	done
    	for x in `seq 0 16`; do
    	    for y in `seq 0 255`; do
    	    	$ipset t test $ip$x$sep$y/$net,eth0 nomatch 2>/dev/null
    	    done
    	done
    	;;
esac
$ipset x
exit 0
