#!/bin/sh

export APN="internet.eplus.de"
export USR="eplus"
export PAS="gprs"
export PIN="0000"
#export APN="web.vodafone.de"
#export USR=""
#export PAS=""



DEVICE=/dev/ttyHS0

TMPFIL=/tmp/connect.$$

up() {
	echo connecting

	stty 19200 -tostop
	
	rm -f $TMPFIL
	(
		/usr/sbin/chat -E -s -V -f dial.cht <$DEVICE > $DEVICE
	) 2>&1 |tee $TMPFIL
	echo " "n inet
	echo connected
	echo " "
	PIP="`grep '^_OWANDATA' $TMPFIL | cut -d, -f2`"
	NS1="`grep '^_OWANDATA' $TMPFIL | cut -d, -f4`"
	NS2="`grep '^_OWANDATA' $TMPFIL | cut -d, -f5`"

	#ifconfig hso0 $PIP netmask 255.255.255.255 up

	ORT="`route | grep default | awk '{printf $8}'`"	# find the old default route and replace it
	#route delete default dev $ORT
	echo "add route"
	#route add default dev hso0
	#mv -f /etc/resolv.conf /tmp/resolv.conf.tmp

	echo "set nameserver"
	(							# update the DNS
		echo "nameserver	$NS1"
		echo "nameserver	$NS2"
	) > /etc/resolv.conf

	rm -f $TMPFIL
	
}

down() {
	echo disconnecting

	/usr/sbin/chat -V -f stop.cht <$DEVICE >$DEVICE
	ifconfig hso0 down
	echo "reset nameserver"
	mv -f /tmp/resolv.conf.tmp /etc/resolv.conf
}

init() {
	echo init
	
	/usr/sbin/chat -E -s -V -f init.cht <$DEVICE >$DEVICE
}

usage() {
	echo Usage: $0 \(up\|down\|restart\|init\)
}

case "$1" in
	up)
		up
		;;
	down)
		down
		;;
	restart)
		down
		up
		;;
	init)
		init
		;;
	*)
		usage
		;;
esac

