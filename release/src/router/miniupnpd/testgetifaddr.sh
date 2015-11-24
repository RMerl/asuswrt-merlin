#!/bin/sh
# $Id: testgetifaddr.sh,v 1.2 2015/09/22 14:48:09 nanard Exp $

OS=`uname -s`
case $OS in
	OpenBSD | Darwin)
	NS="`which netstat`" || exit 1
	IFCONFIG="`which ifconfig`" || exit 1
	EXTIF="`$NS -r -f inet | grep 'default' | awk '{ print $NF  }' `" || exit 1
	EXTIP="`$IFCONFIG $EXTIF | awk '/inet / { print $2 }' `"
	;;
	*)
	IP="`which ip`" || exit 1
	EXTIF="`LC_ALL=C $IP -4 route | grep 'default' | sed -e 's/.*dev[[:space:]]*//' -e 's/[[:space:]].*//'`" || exit 1
	EXTIP="`LC_ALL=C $IP -4 addr show $EXTIF | awk '/inet/ { print $2 }' | cut -d "/" -f 1`"
	;;
esac

#echo "Interface : $EXTIF   IP address : $EXTIP"
RES=`./testgetifaddr $EXTIF | head -n1 | sed 's/Interface .* has IP address \(.*\)\./\1/'` || exit 1


if [ "$RES" = "$EXTIP" ] ; then
	echo "testgetifaddr test OK"
	exit 0
else
	echo "testgetifaddr test FAILED : $EXTIP != $RES"
	exit 1
fi
