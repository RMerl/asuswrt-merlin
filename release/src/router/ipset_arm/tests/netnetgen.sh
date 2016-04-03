#!/bin/sh

while [ -n "$1" ]; do
    case "$1" in
    	comment)
    	    comment=" comment"
    	    ;;
    	timeout)
    	    timeout=" timeout 5"
    	    ;;
    	*)
    	    ;;
    esac
    shift
done

echo "n test hash:net,net hashsize 32 maxelem 87040$comment$timeout"
for x in `seq 0 255`; do
    for y in `seq 0 3 253`; do
	z=$((y+2))
	if [ -n "$comment" ]; then
	   c="$comment \"text 10.0.0.0-10.0.2.255,10.$x.$y.0-10.$x.$z.255\""
	fi
	echo "a test 10.0.0.0-10.0.2.255,10.$x.$y.0-10.$x.$z.255$c"
    done
done
