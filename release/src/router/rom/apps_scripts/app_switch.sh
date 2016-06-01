#!/bin/sh
# ASUS app switch script
# $1: package name, $2: device name.


autorun_file=.asusrouter
nonautorun_file=$autorun_file.disabled
APPS_INSTALL_FOLDER=`nvram get apps_install_folder`
SWAP_ENABLE=`nvram get apps_swap_enable`
SWAP_FILE=`nvram get apps_swap_file`
ORIG_APPS_MOUNTED_PATH=`nvram get apps_mounted_path`
ORIG_APPS_INSTALL_PATH=$ORIG_APPS_MOUNTED_PATH/$APPS_INSTALL_FOLDER
apps_local_space=`nvram get apps_local_space`
APPS_PATH=/opt
PATH=$APPS_PATH/usr/bin:$APPS_PATH/bin:$APPS_PATH/usr/sbin:$APPS_PATH/sbin:/usr/bin:/bin:/usr/sbin:/sbin
unset LD_LIBRARY_PATH
unset LD_PRELOAD


# $1: installed path.
_build_dir(){
	if [ -z "$1" ]; then
		return
	fi

	if [ -L "$1" ] || [ ! -d "$1" ]; then
		rm -rf $1
		mkdir -m 0777 $1
	fi
}


nvram set apps_state_switch=0 # INITIALIZING
nvram set apps_state_error=0
if [ -z "$1" ]; then
	echo "Usage: app_switch.sh <Package name> <device name>"
	nvram set apps_state_error=1
	exit 1
fi

if [ -z "$2" ] || [ ! -b "/dev/$2" ];then
	echo "Usage: app_switch.sh <Package name> <device name>"
	nvram set apps_state_error=1
	exit 1
fi

APPS_MOUNTED_PATH=`mount |grep "/dev/$2 on " |awk '{print $3}'`
if [ -z "$APPS_MOUNTED_PATH" ]; then
	echo "$2 had not mounted yet!"
	nvram set apps_state_error=2
	exit 1
fi

APPS_INSTALL_PATH=$APPS_MOUNTED_PATH/$APPS_INSTALL_FOLDER
_build_dir $APPS_INSTALL_PATH

nvram set apps_dev=$2
nvram set apps_mounted_path=$APPS_MOUNTED_PATH


nvram set apps_state_switch=1 # STOPPING apps
if [ -n "$ORIG_APPS_MOUNTED_PATH" ] && [ -d "$ORIG_APPS_INSTALL_PATH" ]; then
	/usr/sbin/app_stop.sh

	if [ -f "$ORIG_APPS_INSTALL_PATH/$autorun_file" ]; then
		mv $ORIG_APPS_INSTALL_PATH/$autorun_file $ORIG_APPS_INSTALL_PATH/$nonautorun_file
	else
		cp -f $apps_local_space/$autorun_file $ORIG_APPS_INSTALL_PATH/$nonautorun_file
	fi
	if [ "$?" != "0" ]; then
		nvram set apps_state_error=10
		exit 1
	fi
fi


nvram set apps_state_switch=2 # STOPPING swap
if [ "$SWAP_ENABLE" != "1" ]; then
	echo "Skip to swap off!"
elif [ -f "$ORIG_APPS_INSTALL_PATH/$SWAP_FILE" ]; then
	swapoff $ORIG_APPS_INSTALL_PATH/$SWAP_FILE
fi


nvram set apps_state_switch=3 # CHECKING the chosed pool
mount_ready=`/usr/sbin/app_check_pool.sh $2`
if [ "$mount_ready" == "Non-mounted" ]; then
	echo "Had not mounted yet!"
	nvram set apps_state_error=2
	exit 1
fi

if [ -d "$APPS_INSTALL_PATH" ]; then
	/usr/sbin/app_base_link.sh
	if [ "$?" != "0" ]; then
		# apps_state_error was already set by app_base_link.sh.
		exit 1
	fi
fi

if [ -f "$APPS_INSTALL_PATH/$nonautorun_file" ]; then
	rm -rf $APPS_PATH/$nonautorun_file
	rm -rf $APPS_INSTALL_PATH/$nonautorun_file
fi


nvram set apps_state_switch=4 # EXECUTING
/usr/sbin/app_install.sh $1
if [ "$?" != "0" ]; then
	# apps_state_error was already set by app_install.sh.
	exit 1
fi


nvram set apps_state_switch=5
