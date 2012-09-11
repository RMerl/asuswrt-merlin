#!/bin/sh
# $1: filesystem type, $2: device path.

if [ -z "$1" ] || [ -z "$2" ]; then
	echo "Usage: app_fsck.sh [filesystem type] [device's path]"
	exit 0
fi

autocheck_option=
autofix_option=
autofix=`nvram get apps_state_autofix`

if [ "$autofix" == "1" ]; then
	if [ "$1" == "ntfs" ] || [ "$1" == "ufsd" ]; then
		autocheck_option="-a"
		autofix_option="-f"
	else
		autocheck_option=
		autofix_option=p
	fi
else
	if [ "$1" == "ntfs" ] || [ "$1" == "ufsd" ]; then
		autocheck_option="-a"
		autofix_option=
	else
		autocheck_option=n
		autofix_option=
	fi
fi

nvram set apps_pool_error=2
if [ "$1" == "ntfs" ] || [ "$1" == "ufsd" ]; then
	chkntfs $autocheck_option $autofix_option $2 > /dev/null
else
	fsck.$1 -$autocheck_option$autofix_option $2 > /dev/null
fi

if [ "$?" != "0" ]; then
	nvram set apps_pool_error=1
else
	nvram set apps_pool_error=0
fi
