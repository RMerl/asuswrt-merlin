#!/bin/sh
# $1: mounted path.


if [ -z "$1" ]; then
	echo "Usage: app_check_folder.sh <mounted path>"
	exit 1
fi

is_arm_machine=`uname -m |grep arm`
pid=`nvram get productid`
APPS_INSTALL_FOLDER=`nvram get apps_install_folder`
APPS_INSTALL_PATH=$1/$APPS_INSTALL_FOLDER
check_dir="$1/asusware"
check_file="$1/asusware/lib/ipkg/info/ipkg-opt.control"
platform=`echo "$APPS_INSTALL_FOLDER" |awk '{FS="."; print $2}'`

if [ -n "$is_arm_machine" ] || [ "$pid" == "DSL-N66U" ]; then
	if [ -d "$APPS_INSTALL_PATH" ]; then
		echo "Already have the correct installed APP folder."
		exit 0
	fi
fi

if [ ! -d "$check_dir" ]; then
	echo "No conflict APP folder."
	exit 0
elif [ ! -f "$check_file" ]; then
	echo "No conflict ipkg-opt.control."
	exit 0
fi

field_gone=`grep "Architecture: " $check_file`
if [ -z "$field_gone" ]; then
	echo "No Architecture field in the $check_file."
	exit 1
fi

field_value=`grep "Architecture: " $check_file |awk '{print $2}'`
if [ "$field_value" == "mipsel" ]; then
	echo "exist the correct MIPS APP folder."
	exit 0
fi

echo "field_value=$field_value, platform=$platform."
mv $check_dir $APPS_INSTALL_PATH

