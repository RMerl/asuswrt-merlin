#!/bin/sh
# ASUS app install script
# $1: package name, $2: device name.


APPS_PATH=/opt
CONF_FILE=$APPS_PATH/etc/ipkg.conf
ASUS_SERVER=`nvram get apps_ipkg_server`
wget_timeout=`nvram get apps_wget_timeout`
#wget_options="-nv -t 2 -T $wget_timeout --dns-timeout=120"
wget_options="-q -t 2 -T $wget_timeout"
apps_from_internet=`nvram get rc_support |grep appnet`
apps_local_space=`nvram get apps_local_space`

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

# $1: package name, $2: mounted path.
_install_package(){
	if [ "$1" == "uclibc-opt" ] || [ "$1" == "ipkg-opt" ]; then
		return 0
	fi

	_check_package $1
	if [ "$?" == "0" ]; then
		echo "Installing the package: $1..."

		pkg_server=
		pkg_file=
		installed_ipk_path=

		if [ "$1" == "downloadmaster" ] && [ -z "$apps_from_internet" ]; then
			app_base_library.sh $APPS_DEV
			if [ "$?" != "0" ]; then
				# apps_state_error was already set by app_base_library.sh.
				return 1
			fi

			installed_ipk_path=`ls $apps_local_space/downloadmaster*`
		elif [ "$1" == "asuslighttpd" ] && [ -z "$apps_from_internet" ]; then
			app_base_library.sh $APPS_DEV
			if [ "$?" != "0" ]; then
				# apps_state_error was already set by app_base_library.sh.
				return 1
			fi

			installed_ipk_path=`ls $apps_local_space/asuslighttpd*`
		else
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
			ipk_file_name=
			if [ "$pkg_server" == "$ASUS_SERVER" ]; then
				ipk_file_name=`_get_pkg_file_name $1 $pkg_server 1`
			else
				ipk_file_name=$pkg_file
			fi

			echo "wget -c $wget_options $pkg_server/$pkg_file -O $2/$ipk_file_name"
			wget -c $wget_options $pkg_server/$pkg_file -O $2/$ipk_file_name
			if [ "$?" != "0" ]; then
				rm -f $2/$ipk_file_name
				sync

				nvram set apps_state_error=6
				return 1
			fi

			installed_ipk_path=$2"/"$ipk_file_name
		fi

		# Installing the apps...
		ipkg install $installed_ipk_path
		if [ "$?" != "0" ]; then
			nvram set apps_state_error=7
			return 1
		fi

		if [ "$1" == "downloadmaster" ] && [ -z "$apps_from_internet" ]; then
			# do nothing
			return 0
		elif [ "$1" == "asuslighttpd" ] && [ -z "$apps_from_internet" ]; then
			# do nothing
			return 0
		else
			rm -f $installed_ipk_path
		fi
	fi

	return 0
}


nvram set apps_state_install=0 # INITIALIZING
nvram set apps_state_error=0

autorun_file=.asusrouter
nonautorun_file=$autorun_file.disabled
APPS_INSTALL_FOLDER=`nvram get apps_install_folder`
SWAP_ENABLE=`nvram get apps_swap_enable`
SWAP_THRESHOLD=`nvram get apps_swap_threshold`
SWAP_FILE=`nvram get apps_swap_file`
SWAP_SIZE=`nvram get apps_swap_size`
PATH=$APPS_PATH/usr/bin:$APPS_PATH/bin:$APPS_PATH/usr/sbin:$APPS_PATH/sbin:/usr/bin:/bin:/usr/sbin:/sbin
unset LD_LIBRARY_PATH
unset LD_PRELOAD

if [ -z "$1" ]; then
	echo "Usage: app_install.sh <Package name> [device name]"
	nvram set apps_state_error=1
	exit 1
fi

APPS_DEV=`nvram get apps_dev`
APPS_MOUNTED_PATH=`nvram get apps_mounted_path`
if [ -z "$APPS_DEV" ]; then
	APPS_DEV=$2

	if [ -z "$APPS_DEV" ] || [ ! -b "/dev/$APPS_DEV" ];then
		echo "Usage: app_install.sh <Package name> [device name]"
		nvram set apps_state_error=1
		exit 1
	fi

	echo "Initializing the APP environment..."

	APPS_MOUNTED_PATH=`mount |grep "/dev/$2 on " |awk '{print $3}'`
	if [ -z "$APPS_MOUNTED_PATH" ]; then
		echo "$2 had not mounted yet!"
		nvram set apps_state_error=1
		exit 1
	fi

	nvram set apps_dev=$APPS_DEV
	nvram set apps_mounted_path=$APPS_MOUNTED_PATH
fi


nvram set apps_state_install=1 # CHECKING_PARTITION
APPS_INSTALL_PATH=$APPS_MOUNTED_PATH/$APPS_INSTALL_FOLDER
app_base_packages.sh $APPS_DEV
if [ "$?" != "0" ]; then
	# apps_state_error was already set by app_base_packages.sh.
	exit 1
fi

nvram set apps_state_install=2 # CHECKING_SWAP
if [ "$SWAP_ENABLE" != "1" ]; then
	echo "Disable to swap!"
else
	mem_size=`free |sed '1,3d' |awk '{print $4}'`
	if [ "$SWAP_THRESHOLD" == "" ] || [ $mem_size -lt $SWAP_THRESHOLD ]; then
		pool_size=`df /dev/$APPS_DEV |sed '1d' |awk '{print $4}'`
		if [ $pool_size -gt $SWAP_SIZE ]; then
			if [ -e "$APPS_INSTALL_PATH/$SWAP_FILE" ]; then
				swapoff $APPS_INSTALL_PATH/$SWAP_FILE
				rm -rf $APPS_INSTALL_PATH/$SWAP_FILE
			fi

			swap_count=`expr $SWAP_SIZE / 1000 - 1`
			echo "dd if=/dev/zero of=$APPS_INSTALL_PATH/$SWAP_FILE bs=1M count=$swap_count"
			dd if=/dev/zero of=$APPS_INSTALL_PATH/$SWAP_FILE bs=1M count=$swap_count
			echo "mkswap $APPS_INSTALL_PATH/$SWAP_FILE"
			mkswap $APPS_INSTALL_PATH/$SWAP_FILE
			echo "swapon $APPS_INSTALL_PATH/$SWAP_FILE"
			swapon $APPS_INSTALL_PATH/$SWAP_FILE
		else
			echo "No enough partition size!"
			nvram set apps_state_error=3
			exit 1
		fi
	fi
fi

nvram set apps_state_install=3 # INSTALLING
link_internet=`nvram get link_internet`
if [ "$link_internet" != "1" ]; then
	cp -f $apps_local_space/optware.asus $APPS_INSTALL_PATH/lib/ipkg/lists/
	cp -f $apps_local_space/optware.oleg $APPS_INSTALL_PATH/lib/ipkg/lists/
elif [ "$1" == "downloadmaster" ] && [ -z "$apps_from_internet" ]; then
	cp -f $apps_local_space/optware.asus $APPS_INSTALL_PATH/lib/ipkg/lists/
	cp -f $apps_local_space/optware.oleg $APPS_INSTALL_PATH/lib/ipkg/lists/
else
	app_update.sh
fi

#if [ "$1" == "downloadmaster" ] || [ "$1" == "mediaserver" ]; then
#	DM_version1=`app_get_field.sh downloadmaster Version 2 |awk '{FS=".";print $1}'`
#	DM_version2=`app_get_field.sh downloadmaster Version 2 |awk '{FS=".";print $4}'`
#	MS_version=`app_get_field.sh mediaserver Version 2 |awk '{FS=".";print $4}'`
#
#	if [ "$1" == "downloadmaster" ] && [ "$DM_version1" -gt "2" ] && [ "$DM_version2" -gt "59" ]; then
#		_install_package asuslighttpd $APPS_INSTALL_PATH
#	elif [ "$1" == "mediaserver" ] && [ "$MS_version" -gt "15" ]; then
#		_install_package asuslighttpd $APPS_INSTALL_PATH
#	fi
#fi

_install_package $1 $APPS_INSTALL_PATH
if [ "$?" != "0" ]; then
	echo "Fail to install the package: $1!"
	# apps_state_error was already set by _install_package().
	exit 1
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

link_internet=`nvram get link_internet`
if [ "$link_internet" == "1" ]; then
	app_update.sh&
fi

test_of_var_files "$APPS_MOUNTED_PATH"
rc rc_service restart_nasapps

nvram set apps_state_install=4 # FINISHED
