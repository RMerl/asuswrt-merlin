#!/bin/sh
# $1: package name.


APPS_PATH=/opt
CONF_FILE=$APPS_PATH/etc/ipkg.conf
ASUS_SERVER=`nvram get apps_ipkg_server`
wget_timeout=`nvram get apps_wget_timeout`
#wget_options="-nv -t 2 -T $wget_timeout --dns-timeout=120"
wget_options="-q -t 2 -T $wget_timeout"
download_dir=/tmp
download_file=

# $1: package name.
# return value. 1: have package. 0: no package.
_check_package(){
	package_ready=`ipkg list_installed | grep "$1 "`

	if [ -z "$package_ready" ]; then
		return 0
	else
		return 1
	fi
}

# $1: package name, $2: ipkg server name, $3: force(1/0).
_get_pkg_file_name(){
	pkg_file_full=`app_get_field.sh $1 Filename 2`
	old_pkg_file=`echo "$pkg_file_full" |awk '{FS=".ipk";print $1}'`
	pkg_file=`echo "$old_pkg_file" |sed 's/\./-/g'`

	if [ "$3" == "1" ] || [ "$2" != "$ASUS_SERVER" ]; then
		echo "$pkg_file_full"
	else
		echo "$pkg_file.tgz"
	fi
}

# $1: package name, $2: installed path.
_download_package(){
	pkg_server=
	pkg_file=

	# Geting the app's file name...
	server_names=`grep -n '^src.*' $CONF_FILE |sort -r |awk '{print $3}'`
	for s in $server_names; do
		pkg_file=`_get_pkg_file_name $1 $s 0`
		echo "wget --spider $wget_options $s/$pkg_file"
		wget --spider $wget_options $s/$pkg_file
		if [ "$?" == "0" ]; then
			pkg_server=$s
			break
		fi
	done
	if [ -z "$pkg_server" ]; then
		nvram set apps_state_error=6
		return 1
	fi

	# Downloading the app's file name...
	if [ "$pkg_server" == "$ASUS_SERVER" ]; then
		download_file=`_get_pkg_file_name $1 $pkg_server 1`
	else
		download_file=$pkg_file
	fi

	echo "wget -c $wget_options $pkg_server/$pkg_file -O $2$download_dir/$download_file"
	wget -c $wget_options $pkg_server/$pkg_file -O $2$download_dir/$download_file
	if [ "$?" != "0" ]; then
		rm -f $2$download_dir/$download_file
		sync

		nvram set apps_state_error=6
		return 1
	fi

	return 0
}


nvram set apps_state_upgrade=0 # INITIALIZING
nvram set apps_state_error=0

if [ -z "$1" ]; then
	echo "Usage: app_upgrade.sh <Package name>"
	nvram set apps_state_error=1
	exit 1
fi

version=`app_get_field.sh $1 Version 1`
new_version=`app_get_field.sh $1 Version 2`

if [ "$version" == "$new_version" ]; then
	echo "The package: $1 is the newest one."
#	nvram set apps_state_upgrade=3 # FINISHED
#	exit 0
fi

APPS_DEV=`nvram get apps_dev`
APPS_MOUNTED_PATH=`nvram get apps_mounted_path`
APPS_INSTALL_FOLDER=`nvram get apps_install_folder`
if [ -z "$APPS_DEV" ] || [ -z "$APPS_MOUNTED_PATH" ]; then
	nvram set apps_state_error=2
	exit 1
fi
APPS_INSTALL_PATH=$APPS_MOUNTED_PATH/$APPS_INSTALL_FOLDER

_download_package $1 $APPS_INSTALL_PATH
if [ "$?" != "0" ]; then
	# apps_state_error was already set by _download_package().
	exit 1
fi


nvram set apps_state_upgrade=1 # REMOVING
_check_package $1
if [ "$?" != "0" ]; then
	app_remove.sh $1
	if [ "$?" != "0" ]; then
		# apps_state_error was already set by app_remove.sh.
		exit 1
	fi
fi


nvram set apps_state_upgrade=2 # INSTALLING
#if [ "$1" != "asuslighttpd" ]; then
#	_check_package "asuslighttpd"
#	if [ "$?" == "0" ]; then
#		app_install.sh "asuslighttpd"
#		if [ "$?" != "0" ]; then
#			# apps_state_error was already set by app_install.sh.
#			exit 1
#		fi
#	fi
#fi

ipkg install $APPS_INSTALL_PATH$download_dir/$download_file
if [ "$?" != "0" ]; then
	echo "Fail to install the package: $1!"
	nvram set apps_state_error=7
	exit 1
else
	rm -f $APPS_INSTALL_PATH$download_dir/$download_file
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

app_set_enabled.sh $1 "yes"


nvram set apps_state_upgrade=3 # FINISHED
