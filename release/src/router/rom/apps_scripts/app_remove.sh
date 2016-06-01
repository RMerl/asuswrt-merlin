#!/bin/sh
# $1: package name.


APPS_INSTALL_FOLDER=`nvram get apps_install_folder`
APPS_PATH=/opt
APPS_DEV=`nvram get apps_dev`
APPS_MOUNTED_PATH=`nvram get apps_mounted_path`
APPS_INSTALL_PATH=$APPS_MOUNTED_PATH/$APPS_INSTALL_FOLDER


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


nvram set apps_state_remove=0 # INITIALIZING
nvram set apps_state_error=0
if [ -z "$1" ]; then
	echo "Usage: app_remove.sh <Package name>"
	nvram set apps_state_error=1
	exit 1
fi

if [ -z "$APPS_MOUNTED_PATH" ]; then
	echo "Had not mounted yet!"
	nvram set apps_state_error=2
	exit 1
fi

_check_package $1
if [ "$?" == "0" ]; then
	echo "The \"$1\" is not installed yet!"
	nvram set apps_state_remove=2 # FINISHED
	exit 0
fi

need_asuslighttpd=0
need_smartsync=0
if [ "$1" == "downloadmaster" ]; then
	DM_version1=`/usr/sbin/app_get_field.sh downloadmaster Version 1 |awk '{FS=".";print $1}'`
	DM_version4=`/usr/sbin/app_get_field.sh downloadmaster Version 1 |awk '{FS=".";print $4}'`

	if [ "$DM_version1" -gt "3" ]; then
		need_asuslighttpd=1
	elif [ "$DM_version1" -eq "3" ] && [ "$DM_version4" -gt "59" ]; then
		need_asuslighttpd=1
	fi
elif [ "$1" == "mediaserver" ]; then
	MS_version1=`/usr/sbin/app_get_field.sh mediaserver Version 1 |awk '{FS=".";print $1}'`
	MS_version4=`/usr/sbin/app_get_field.sh mediaserver Version 1 |awk '{FS=".";print $4}'`

	if [ "$MS_version1" -gt "1" ]; then
		need_asuslighttpd=1
	elif [ "$MS_version1" -eq "1" ] && [ "$MS_version4" -gt "15" ]; then
		need_asuslighttpd=1
	fi
elif [ "$1" == "aicloud" ]; then
	AC_version1=`/usr/sbin/app_get_field.sh aicloud Version 1 |awk '{FS=".";print $1}'`
	AC_version4=`/usr/sbin/app_get_field.sh aicloud Version 1 |awk '{FS=".";print $4}'`

	if [ "$AC_version1" -gt "1" ]; then
		need_smartsync=1
	elif [ "$AC_version1" -eq "1" ] && [ "$AC_version4" -gt "4" ]; then
		need_smartsync=1
	fi
fi

if [ "$need_asuslighttpd" == "1" ]; then
	dep_on_asuslighttpd=0
	if [ "$1" == "downloadmaster" ] ; then
		dep_on_asuslighttpd=`ipkg list_installed|awk '{print $1}'|grep -c mediaserver`
	elif [ "$1" == "mediaserver" ] ; then
		dep_on_asuslighttpd=`ipkg list_installed|awk '{print $1}'|grep -c downloadmaster`
	fi

	if [ "$dep_on_asuslighttpd" != "0" ] ; then
		echo "asuslighttpd is need by another installed package"
		need_asuslighttpd=0
	fi
fi

nvram set apps_state_remove=1 # REMOVING
echo "Removing the package: $1..."
/usr/sbin/app_set_enabled.sh $1 no
ipkg remove $1
if [ "$?" != "0" ]; then
	nvram set apps_state_error=9
	exit 1
fi

if [ "$need_asuslighttpd" == "1" ]; then
	_check_package asuslighttpd
	if [ "$?" != "0" ]; then
		echo "Removing the dependent package: asuslighttpd..."
		/usr/sbin/app_set_enabled.sh asuslighttpd no
		ipkg remove asuslighttpd
		if [ "$?" != "0" ]; then
			nvram set apps_state_error=9
			exit 1
		fi
	fi
elif [ "$need_smartsync" == "1" ]; then
	_check_package smartsync
	if [ "$?" != "0" ]; then
		echo "Removing the dependent package: smartsync..."
		/usr/sbin/app_set_enabled.sh smartsync no
		ipkg remove smartsync
		if [ "$?" != "0" ]; then
			nvram set apps_state_error=9
			exit 1
		fi
	fi

	deps=`/usr/sbin/app_get_field.sh smartsync Depends 2 |sed 's/,/ /g'`

	for dep in $deps; do
		_check_package $dep
		if [ "$?" != "0" ]; then
			echo "Removing the dependent package of smartsync: $dep..."
			/usr/sbin/app_set_enabled.sh $dep no
			ipkg remove $dep
			if [ "$?" != "0" ]; then
				nvram set apps_state_error=9
				exit 1
			fi
		fi
	done
fi

APPS_MOUNTED_TYPE=`mount |grep "/dev/$APPS_DEV on " |awk '{print $5}'`
if [ "$APPS_MOUNTED_TYPE" != "vfat" ] && [ "$APPS_MOUNTED_TYPE" != "tfat" ]; then
	nvram set apps_state_remove=2 # FINISHED
	exit 0
fi

objs=`ls -a $APPS_INSTALL_PATH`
for obj in $objs; do
	if [ "$obj" == "." ] || [ "$obj" == ".." ]; then
		continue
	fi

	if [ "$obj" != "bin" ] && [ "$obj" != "lib" ] && [ ! -e "$APPS_PATH/$obj" ]; then
		rm -rf $APPS_INSTALL_PATH/$obj
	fi
done

objs=`ls -a $APPS_INSTALL_PATH/bin`
for obj in $objs; do
	if [ "$obj" == "." ] || [ "$obj" == ".." ]; then
		continue
	fi

	if [ ! -e "$APPS_PATH/bin/$obj" ]; then
		rm -rf $APPS_INSTALL_PATH/bin/$obj
	fi
done

objs=`ls -a $APPS_INSTALL_PATH/lib`
for obj in $objs; do
	if [ "$obj" == "." ] || [ "$obj" == ".." ]; then
		continue
	fi

	if [ ! -e "$APPS_PATH/lib/$obj" ]; then
		rm -rf $APPS_INSTALL_PATH/lib/$obj
	fi
done

/usr/sbin/app_base_link.sh
if [ "$?" != "0" ]; then
	# apps_state_error was already set by app_base_link.sh.
	exit 1
fi

nvram set apps_state_remove=2 # FINISHED
