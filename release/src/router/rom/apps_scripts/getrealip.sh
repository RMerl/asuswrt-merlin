#!/bin/sh
# use STUN to find the external IP.

servers="default stun.l.google.com:19302 stun.iptel.org"
prefixes="wan0_ wan1_"

which ministun >/dev/null || exit 1

for prefix in $prefixes; do
	primary=$(nvram get ${prefix}primary)
	[ "$primary" = "1" ] && break
done
[ "$primary" = "1" ] || exit 1

for server in $servers; do
	[ "$server" = "default" ] && server=
	result=$(ministun -t 1000 -c 1 $server 2>/dev/null)
	[ $? -eq 0 ] && break
	result=
done
[ -z "$result" ] && state=1 || state=2
nvram set ${prefix}realip_state=$state
nvram set ${prefix}realip_ip=$result

[ -z "$result" ] && echo "Failed." || echo "External IP is $result."
