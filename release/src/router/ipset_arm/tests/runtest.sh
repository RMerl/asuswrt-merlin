#!/bin/bash

# set -x

ipset=${IPSET_BIN:-../src/ipset}

tests="init"
tests="$tests ipmap bitmap:ip"
tests="$tests macipmap portmap"
tests="$tests iphash hash:ip hash:ip6"
tests="$tests ipporthash hash:ip,port hash:ip6,port"
tests="$tests ipmarkhash hash:ip,mark hash:ip6,mark"
tests="$tests ipportiphash hash:ip,port,ip hash:ip6,port,ip6"
tests="$tests nethash hash:net hash:net6 hash:net,port hash:net6,port"
tests="$tests hash:ip,port,net hash:ip6,port,net6 hash:net,net hash:net6,net6"
tests="$tests hash:net,port,net hash:net6,port,net6"
tests="$tests hash:net,iface.t hash:mac.t"
tests="$tests comment setlist restore"
# tests="$tests iptree iptreemap"

# For correct sorting:
LC_ALL=C
export LC_ALL

add_tests() {
	# inet|inet6 network
	if [ $1 = "inet" ]; then
		cmd=iptables-save
		add="match_target match_flags"
	else
		cmd=ip6tables-save
		add=match_target6
	fi
	#line="`dmesg | tail -1 | cut -d " " -f 2-`"
	#if [ ! -e /var/log/kern.log -o -z "`grep -F \"$line\" /var/log/kern.log`" ]; then
	#	echo "The destination for kernel log is not /var/log/kern.log, skipping $1 match and target tests"
	#	return
	#fi
	c=${cmd%%-save}
	if [ "`$c -m set -h 2>&1| grep 'cannot open shared object'`" ]; then
		echo "$c does not support set match, skipping $1 match and target tests"
		return
	fi
	if [ `$cmd -t filter | wc -l` -eq 7 -a \
	     `$cmd -t filter | grep ACCEPT | wc -l` -eq 3 ]; then
	     	if [ -z "`which sendip`" ]; then
	     		echo "sendip utility is missig: skipping $1 match and target tests"
	     	elif [ -n "`netstat --protocol $1 -n | grep $2`" ]; then
	     		echo "Our test network $2 in use: skipping $1 match and target tests"
	     	else
	     		tests="$tests $add"
	     	fi
	     	:
	else
		echo "You have got iptables rules: skipping $1 match and target tests"
	fi
}

if [ "$1" ]; then
	tests="init $@"
else
	add_tests inet 10.255.255
	add_tests inet6 1002:1002:1002:1002::
fi

# Make sure the scripts are executable
chmod a+x check_* *.sh

for types in $tests; do
    $ipset -X test >/dev/null 2>&1
    if [ -f $types ]; then
    	filename=$types
    else
    	filename=$types.t
    fi
    while read ret cmd; do
	case $ret in
	    \#)
	    	if [ "$cmd" = "eof" ]; then
	    		break
	    	fi
	    	what=$cmd
		continue
		;;
	    skip)
	    	eval $cmd
	    	if [ $? -ne 0 ]; then
	    		echo "Skipping tests, '$cmd' failed"
	    		break
	    	fi
	    	continue
	    	;;
	    *)
		;;
	esac
	echo -ne "$types: $what: "
	cmd=`echo $cmd | sed "s|ipset|$ipset 2>.foo.err|"`
	eval $cmd
	r=$?
	# echo $ret $r
	if [ "$ret" = "$r" ]; then
		echo "passed"
	else
		echo "FAILED"
		echo "Failed test: $cmd"
		cat .foo.err
		exit 1
	fi
	# sleep 1
    done < $filename
done
# Remove test sets created by setlist.t
$ipset -X >/dev/null 2>&1
for x in $tests; do
	case $x in
	init)
		;;
	*)
		for x in `lsmod | grep ip_set_ | awk '{print $1}'`; do
			rmmod $x >/dev/null 2>&1
		done
		;;
	esac
done
rmmod ip_set >/dev/null 2>&1
rm -f .foo*
echo "All tests are passed"

