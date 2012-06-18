#!/bin/sh
# Usage: event.sh <event> <if-name>

case "$1" in
if-create)
	;;
if-up)
	if [ -x /sbin/dhclient3 ]; then
		/sbin/dhclient3 -nw -pf /var/run/dhclient."$2".pid -lf /var/lib/dhcp3/dhclient."$2".leases "$2" >/dev/null 2>&1
	elif [ -x /sbin/dhclient ]; then
		/sbin/dhclient -pf /var/run/dhclient."$2".pid -lf /var/lib/dhcp/dhclient."$2".leases "$2"
	elif [ -x /sbin/pump ]; then
		/sbin/pump -i "$2"
	elif [ -x /sbin/udhcpc ]; then
		/sbin/udhcpc -n -p /var/run/udhcpc."$2".pid -i "$2"
	elif [ -x /sbin/dhcpcd ]; then
		/sbin/dhcpcd "$2"
	fi
	;;
if-down)
	if [ -x /sbin/dhclient3 ]; then
		/sbin/dhclient3 -r -pf /var/run/dhclient."$2".pid -lf /var/lib/dhcp3/dhclient."$2".leases "$2" >/dev/null 2>&1
	elif [ -x /sbin/dhclient ]; then
		kill -TERM $(cat /var/run/dhclient."$2".pid)
	elif [ -x /sbin/pump ]; then
		/sbin/pump -i "$2" -r
	elif [ -x /sbin/udhcpc ]; then
		kill -TERM $(cat /var/run/udhcpc."$2".pid)
	elif [ -x /sbin/dhcpcd ]; then
		/sbin/dhcpcd -k "$2"
	fi
	;;
if-release)
	;;
*)
	echo "Usage: $0 { if-create | if-up | if-down | if-release }" >&2
	exit 3
	;;
esac

