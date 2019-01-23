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
    ip)
    	$ipset n test hash:ip $1 hashsize 64 comment
    	for x in `seq 0 16`; do
    	    for y in `seq 0 255`; do
    	    	$ipset a test $ip$x$sep$y comment "text $ip$x$sep$y"
    	    done
    	done
    	;;
    ipport)
    	$ipset n test hash:ip,port $1 hashsize 64 comment
    	for x in `seq 0 16`; do
    	    for y in `seq 0 255`; do
    	    	$ipset a test $ip$x$sep$y,1023 "text $ip$x$sep$y,1023"
    	    done
    	done
    	;;
    ipportip)
    	$ipset n test hash:ip,port,ip $1 hashsize 64 comment
    	for x in `seq 0 16`; do
    	    for y in `seq 0 255`; do
    	    	$ipset a test $ip$x$sep$y,1023,$ip2 comment "text $ip$x$sep$y,1023,$ip2"
    	    done
    	done
    	;;
    ipportnet)
    	$ipset n test hash:ip,port,net $1 hashsize 64 comment
    	for x in `seq 0 16`; do
    	    for y in `seq 0 255`; do
    	    	$ipset a test $ip$x$sep$y,1023,$ip2/$net comment "text $ip$x$sep$y,1023,$ip2/$net"
    	    done
    	done
    	;;
    net)
    	$ipset n test hash:net $1 hashsize 64 comment
    	for x in `seq 0 16`; do
    	    for y in `seq 0 255`; do
    	    	$ipset a test $ip$x$sep$y/$net comment "text $ip$x$sep$y/$net"
    	    done
    	done
    	;;
    netnet)
	$ipset n test hash:net,net $1 hashsize 64 comment
	for x in `seq 0 16`; do
	    for y in `seq 0 255`; do
		$ipset a test $ip$x$sep$y/$net,$ip$y$sep$x/$net comment "text $ip$x$sep$y/$net,$ip$y$sep$x/$net"
	    done
	done
	;;
    netport)
    	$ipset n test hash:net,port $1 hashsize 64 comment
    	for x in `seq 0 16`; do
    	    for y in `seq 0 255`; do
    	    	$ipset a test $ip$x$sep$y/$net,1023 comment "text $ip$x$sep$y/$net,1023"
    	    done
    	done
    	;;
    netiface)
    	$ipset n test hash:net,iface $1 hashsize 64 comment
    	for x in `seq 0 16`; do
    	    for y in `seq 0 255`; do
    	    	$ipset a test $ip$x$sep$y/$net,eth0 comment "text $ip$x$sep$y/$net,eth0"
    	    done
    	done
    	;;
esac
$ipset l test | grep ^$ip | while read x y z; do
    z=`echo $z | sed 's/\/32//g'`
    if [ "$z" != "\"text $x\"" ]; then
    	echo $x $y $z
    	exit 1
    fi
done
$ipset x
exit 0
