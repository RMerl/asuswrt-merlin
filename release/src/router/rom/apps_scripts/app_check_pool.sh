#!/bin/sh
# $1: device name.


APPS_INSTALL_FOLDER=`nvram get apps_install_folder`
APPS_PATH=/opt
TEST_TARGET=/bin/ipkg
LINK_FILE=$APPS_PATH$TEST_TARGET


if [ -z "$1" ] || [ ! -b "/dev/$1" ]; then
	echo "Usage: apps_check_pool.sh <device name>"
	exit 1
fi

APPS_MOUNTED_PATH=`mount |grep "/dev/$1 on " |awk '{print $3}'`
if [ -z "$APPS_MOUNTED_PATH" ]; then
	echo "Non-mounted"
	exit 0
fi

APPS_INSTALL_PATH=$APPS_MOUNTED_PATH/$APPS_INSTALL_FOLDER
read_path1=`readlink $LINK_FILE`
read_path2=`readlink -f $LINK_FILE`

if [ ! -L "$LINK_FILE" ] || [ "$read_path1" != "$read_path2" ] || [ "$read_path1" != "$APPS_INSTALL_PATH$TEST_TARGET" ]; then
	echo "Non-linked"
else
	echo "linked"
fi
