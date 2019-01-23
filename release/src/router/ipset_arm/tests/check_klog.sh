#!/bin/bash

# set -x
set -e

# arguments: ipaddr proto port setname ...

expand_ipv6() {
	# incomplete, but for our addresses it's OK
	addr=
	n=0
	while read a; do
		n=$((n+1))
		if [ -z "$a" ]; then
			addr="$addr::"
		else
			case `echo $a | wc -c` in
			4) a="0$a";;
			3) a="00$a";;
			2) a="000$a";;
			esac
			addr="$addr$a:"
		fi
	done < <(echo $1 | tr : '\n')
	addr=`echo $addr | sed -e 's/:$//'`
	null=
	while [ $n -le 8 ]; do
		null="$null:0000"
		n=$((n+1))
	done
	addr=`echo $addr | sed -e "s/::/$null/"`
	echo $addr
}

ipaddr=`expand_ipv6 $1`; shift
proto=`echo $1 | tr a-z A-Z`; shift
port=$1; shift

for setname in $@; do
	match=`dmesg| tail -n 2 | grep -e "in set $setname: .* SRC=$ipaddr .* PROTO=$proto SPT=$port .*"`
	if [ -z "$match" ]; then
		echo "no match!"
		exit 1
	fi
done
exit 0
