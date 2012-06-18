#!/bin/sh
# $1: device name.


autorun_file=.asusrouter
nonautorun_file=$autorun_file.disabled
APPS_INSTALL_FOLDER=`nvram get apps_install_folder`
APPS_DEV=`nvram get apps_dev`
apps_from_internet=`nvram get rc_support |grep appnet`
apps_local_space=`nvram get apps_local_space`

if [ -n "$apps_from_internet" ]; then
	exit 0
fi

if [ -z "$APPS_DEV" ]; then
	echo "Wrong"
	APPS_DEV=$1
fi

if [ -z "$APPS_DEV" ] || [ ! -b "/dev/$APPS_DEV" ];then
	echo "Usage: app_base_library.sh <device name>"
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

list_installed=`ipkg list_installed`

if [ -z "`echo "$list_installed" |grep "openssl - "`" ]; then
	ipkg install $apps_local_space/openssl_*_mipsel.ipk
	if [ "$?" != "0" ]; then
		echo "Failed to install openssl!"
		nvram set apps_state_error=4
		exit 1
	fi
fi

if [ -z "`echo "$list_installed" |grep "zlib - "`" ]; then
	ipkg install $apps_local_space/zlib_*_mipsel.ipk
	if [ "$?" != "0" ]; then
		echo "Failed to install zlib!"
		nvram set apps_state_error=4
		exit 1
	fi
fi

if [ -z "`echo "$list_installed" |grep "libcurl - "`" ]; then
	ipkg install $apps_local_space/libcurl_*_mipsel.ipk
	if [ "$?" != "0" ]; then
		echo "Failed to install libcurl!"
		nvram set apps_state_error=4
		exit 1
	fi
fi

if [ -z "`echo "$list_installed" |grep "libevent - "`" ]; then
	ipkg install $apps_local_space/libevent_*_mipsel.ipk
	if [ "$?" != "0" ]; then
		echo "Failed to install libevent!"
		nvram set apps_state_error=4
		exit 1
	fi
fi

if [ -z "`echo "$list_installed" |grep "ncurses - "`" ]; then
	ipkg install $apps_local_space/ncurses_*_mipsel.ipk
	if [ "$?" != "0" ]; then
		echo "Failed to install ncurses!"
		nvram set apps_state_error=4
		exit 1
	fi
fi

if [ -z "`echo "$list_installed" |grep "libxml2 - "`" ]; then
	ipkg install $apps_local_space/libxml2_*_mipsel.ipk
	if [ "$?" != "0" ]; then
		echo "Failed to install libxml2!"
		nvram set apps_state_error=4
		exit 1
	fi
fi

if [ -z "`echo "$list_installed" |grep "libuclibc++ - "`" ]; then
	ipkg install $apps_local_space/libuclibc++_*_mipsel.ipk
	if [ "$?" != "0" ]; then
		echo "Failed to install libuclibc++!"
		nvram set apps_state_error=4
		exit 1
	fi
fi

if [ -z "`echo "$list_installed" |grep "libsigc++ - "`" ]; then
	ipkg install $apps_local_space/libsigc++_*_mipsel.ipk
	if [ "$?" != "0" ]; then
		echo "Failed to install libsigc++!"
		nvram set apps_state_error=4
		exit 1
	fi
fi

if [ -z "`echo "$list_installed" |grep "libpar2 - "`" ]; then
	ipkg install $apps_local_space/libpar2_*_mipsel.ipk
	if [ "$?" != "0" ]; then
		echo "Failed to install libpar2!"
		nvram set apps_state_error=4
		exit 1
	fi
fi

if [ -z "`echo "$list_installed" |grep "pcre - "`" ]; then
	ipkg install $apps_local_space/pcre_*_mipsel.ipk
	if [ "$?" != "0" ]; then
		echo "Failed to install pcre!"
		nvram set apps_state_error=4
		exit 1
	fi
fi

if [ -z "`echo "$list_installed" |grep "spawn-fcgi - "`" ]; then
	ipkg install $apps_local_space/spawn-fcgi_*_mipsel.ipk
	if [ "$?" != "0" ]; then
		echo "Failed to install spawn-fcgi!"
		nvram set apps_state_error=4
		exit 1
	fi
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
