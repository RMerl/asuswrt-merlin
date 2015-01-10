#!/bin/sh
# $1: device name.


APPS_INSTALL_FOLDER=`nvram get apps_install_folder`
APPS_DEV=`nvram get apps_dev`
APPS_MOUNTED_PATH=`nvram get apps_mounted_path`
APPS_PATH=/opt


if [ -z "$APPS_DEV" ]; then
	echo "Wrong"
	APPS_DEV=$1
fi

if [ -z "$APPS_DEV" ] || [ ! -b "/dev/$APPS_DEV" ];then
	echo "Usage: app_move_to_pool.sh <device name>"
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
if [ ! -d "$APPS_INSTALL_PATH" ]; then
	echo "No the base directory!"
	nvram set apps_state_error=4
	exit 1
fi

APPS_MOUNTED_TYPE=`mount |grep "/dev/$APPS_DEV on " |awk '{print $5}'`
if [ -z "$APPS_MOUNTED_TYPE" ]; then
	echo "$1 had not mounted yet!"
	nvram set apps_state_error=2
	exit 1
fi

objs=`ls -a $APPS_PATH/`
for obj in $objs; do
	if [ "$obj" == "." ] || [ "$obj" == ".." ]; then
		continue
	fi

	target=`echo $obj |awk '{FS="-"; printf $1}'`
	if [ "$target" == "ipkg" ]; then
		continue
	fi

	if [ "$obj" != "bin" ] && [ "$obj" != "lib" ] && [ ! -e "$APPS_INSTALL_PATH/$obj" ]; then
		if [ "$APPS_MOUNTED_TYPE" != "vfat" ] && [ "$APPS_MOUNTED_TYPE" != "tfat" ]; then
			cp -rf $APPS_PATH/$obj $APPS_INSTALL_PATH
			if [ "$?" != "0" ]; then
				nvram set apps_state_error=10
				exit 1
			fi
		elif [ ! -L "$APPS_PATH/$obj" ]; then
			cp -rf $APPS_PATH/$obj $APPS_INSTALL_PATH
			if [ "$?" != "0" ]; then
				nvram set apps_state_error=10
				exit 1
			fi
		fi
	fi
done

objs=`ls -a $APPS_PATH/bin/`
for obj in $objs; do
	if [ "$obj" == "." ] || [ "$obj" == ".." ]; then
		continue
	fi

	if [ ! -e "$APPS_INSTALL_PATH/bin/$obj" ]; then
		if [ "$APPS_MOUNTED_TYPE" != "vfat" ] && [ "$APPS_MOUNTED_TYPE" != "tfat" ]; then
			cp -rf $APPS_PATH/bin/$obj $APPS_INSTALL_PATH/bin
			if [ "$?" != "0" ]; then
				nvram set apps_state_error=10
				exit 1
			fi
		elif [ ! -L "$APPS_PATH/bin/$obj" ]; then
			cp -rf $APPS_PATH/bin/$obj $APPS_INSTALL_PATH/bin
			if [ "$?" != "0" ]; then
				nvram set apps_state_error=10
				exit 1
			fi
		fi
	fi
done

objs=`ls -a $APPS_PATH/lib/`
for obj in $objs; do
	if [ "$obj" == "." ] || [ "$obj" == ".." ]; then
		continue
	fi

	if [ ! -e "$APPS_INSTALL_PATH/lib/$obj" ]; then
		if [ "$APPS_MOUNTED_TYPE" != "vfat" ] && [ "$APPS_MOUNTED_TYPE" != "tfat" ]; then
			cp -rf $APPS_PATH/lib/$obj $APPS_INSTALL_PATH/lib
			if [ "$?" != "0" ]; then
				nvram set apps_state_error=10
				exit 1
			fi
		elif [ ! -L "$APPS_PATH/lib/$obj" ]; then
			cp -rf $APPS_PATH/lib/$obj $APPS_INSTALL_PATH/lib
			if [ "$?" != "0" ]; then
				nvram set apps_state_error=10
				exit 1
			fi
		fi
	fi
done
