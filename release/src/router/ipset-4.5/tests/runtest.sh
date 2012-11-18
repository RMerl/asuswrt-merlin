#!/bin/bash

tests="init"
tests="$tests ipmap macipmap portmap"
tests="$tests iphash nethash ipporthash"
tests="$tests ipportiphash ipportnethash"
tests="$tests iptree iptreemap"
tests="$tests setlist"

if [ "$1" ]; then
	tests="init $@"
fi

for types in $tests; do
    ipset -X test >/dev/null 2>&1
    while read ret cmd; do
	case $ret in
	    \#)
	    	if [ "$cmd" = "eof" ]; then
	    		break
	    	fi
	    	what=$cmd
		continue
		;;
	    *)
		;;
	esac
	echo -ne "$types: $what: "
	eval $cmd >/dev/null 2>&1
	r=$?
	# echo $ret $r
	if [ "$ret" = "$r" ]; then
		echo "passed"
	else
		echo "FAILED"
		echo "Failed test: $cmd"
		exit 1
	fi
	# sleep 1
    done < $types.t
done
# Remove test sets created by setlist.t
ipset -X
for x in $tests; do
	case $x in
	init)
		;;
	*)
		rmmod ip_set_$x >/dev/null 2>&1
		;;
	esac
done
rmmod ip_set >/dev/null 2>&1
echo "All tests are passed"

