#!/bin/sh
# $1: filesystem type, $2: device path.

if [ -z "$1" ] || [ -z "$2" ]; then
	echo "Usage: app_fsck.sh [filesystem type] [device's path]"
	exit 0
fi

if [ "$1" == "ntfs" ] || [ "$1" == "ufsd" ]; then
	chkntfs -a $2 > /dev/null
else
	fsck.$1 -n $2 > /dev/null
fi

nvram set apps_pool_error=2
if [ $? -ne 0 ]; then
	nvram set apps_pool_error=1
else
	nvram set apps_pool_error=0
fi
