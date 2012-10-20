#!/bin/sh
# $1: device name.


autorun_file=.asusrouter
nonautorun_file=$autorun_file.disabled
APPS_INSTALL_FOLDER=`nvram get apps_install_folder`
APPS_DEV=`nvram get apps_dev`
ASUS_SERVER=`nvram get apps_ipkg_server`
wget_timeout=`nvram get apps_wget_timeout`
#wget_options="-nv -t 2 -T $wget_timeout --dns-timeout=120"
wget_options="-q -t 2 -T $wget_timeout"
apps_from_internet=`nvram get rc_support |grep appnet`
apps_local_space=`nvram get apps_local_space`

link_internet=`nvram get link_internet`

if [ -z "$APPS_DEV" ]; then
	echo "Wrong"
	APPS_DEV=$1
fi

if [ -z "$APPS_DEV" ] || [ ! -b "/dev/$APPS_DEV" ];then
	echo "Usage: app_base_packages.sh <device name>"
	nvram set apps_state_error=1
	exit 1
fi

APPS_MOUNTED_PATH=`mount |grep "/dev/$APPS_DEV on " |awk '{print $3}'`
if [ -z "$APPS_MOUNTED_PATH" ]; then
	echo "$1 had not mounted yet!"
	nvram set apps_state_error=2
	exit 1
fi

APPS_INSTALL_PATH=$APPS_MOUNTED_PATH/$APPS_INSTALL_FOLDER
if [ -L "$APPS_INSTALL_PATH" ] || [ ! -d "$APPS_INSTALL_PATH" ]; then
	echo "Building the base directory!"
	rm -rf $APPS_INSTALL_PATH
	mkdir -p -m 0777 $APPS_INSTALL_PATH
fi

if [ ! -f "$APPS_INSTALL_PATH/$nonautorun_file" ]; then
	cp -f $apps_local_space/$autorun_file $APPS_INSTALL_PATH
	if [ "$?" != "0" ]; then
		nvram set apps_state_error=10
		exit 1
	fi
else
	rm -f $APPS_INSTALL_PATH/$autorun_file
fi

if [ ! -f "$APPS_INSTALL_PATH/bin/ipkg" ] || [ ! -f "$APPS_INSTALL_PATH/lib/libuClibc-0.9.28.so" ]; then
	echo "Installing the base apps!"
	CURRENT_PWD=`pwd`
	cd $APPS_INSTALL_PATH

	if [ -z "$apps_from_internet" ]; then
		tar xzf $apps_local_space/asus_base_apps.tgz # uclibc-opt & ipkg-opt.
		cp -f $apps_local_space/optware.asus $APPS_INSTALL_PATH/lib/ipkg/lists/
		cp -f $apps_local_space/optware.oleg $APPS_INSTALL_PATH/lib/ipkg/lists/
		if [ "$?" != "0" ]; then
			cd $CURRENT_PWD
			nvram set apps_state_error=10
			exit 1
		fi
	else
		if [ "$link_internet" != "1" ]; then
			cd $CURRENT_PWD
			echo "Couldn't connect Internet to install the base apps!"
			nvram set apps_state_error=5
			exit 1
		fi

		rm -f asus_base_apps.tgz
		sync

		# TODO: just for test.
		dl_path=http://dlcdnet.asus.com/pub/ASUS/LiveUpdate/Release/Wireless

		echo "wget -c $wget_options $dl_path/asus_base_apps.tgz"
		wget -c $wget_options $dl_path/asus_base_apps.tgz
		if [ "$?" != "0" ]; then
			rm -f asus_base_apps.tgz
			sync

			cd $CURRENT_PWD
			echo "Failed to get asus_base_apps.tgz from Internet!"
			nvram set apps_state_error=6
			exit 1
		fi

		tar xzf asus_base_apps.tgz # uclibc-opt & ipkg-opt.
		if [ "$?" != "0" ]; then
			cd $CURRENT_PWD
			nvram set apps_state_error=10
			exit 1
		fi

		rm -f asus_base_apps.tgz
	fi

	cd $CURRENT_PWD
fi

APPS_MOUNTED_TYPE=`mount |grep "/dev/$APPS_DEV on " |awk '{print $5}'`
if [ "$APPS_MOUNTED_TYPE" == "vfat" ]; then
	app_move_to_pool.sh $APPS_DEV
	if [ "$?" != "0" ]; then
		# apps_state_error was already set by app_move_to_pool.sh.
		exit 1
	fi
fi

app_base_link.sh
if [ "$?" != "0" ]; then
	# apps_state_error was already set by app_base_link.sh.
	exit 1
fi

echo "Success to build the base environment!"
