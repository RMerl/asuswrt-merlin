#!/bin/sh
# $1: Package name/allpkg, $2: action.


APPS_PATH=/opt
APPS_RUN_DIR=$APPS_PATH/etc/init.d

if [ -z "$1" ] || [ -z "$2" ]; then
	echo "Usage: app_init_run.sh <Package name|allpkg> <action>"
	exit 1
fi

if [ ! -d "$APPS_RUN_DIR" ]; then
	echo "The APP's init dir was not existed!"
	exit 1
fi

for f in $APPS_RUN_DIR/S*; do
	tmp_apps_name=`get_apps_name $f`
	if [ "$1" != "allpkg" ] && [ "$1" != "$tmp_apps_name" ]; then
		continue
	fi

	if [ "$2" == "start" ]; then
		app_enable=`app_get_field.sh $tmp_apps_name "Enabled" 1`
		if [ "$app_enable" != "yes" ]; then
			if [ "$1" != "allpkg" ] && [ "$1" == "$tmp_apps_name" ]; then
				echo "No permission to start with the app: $1!"
				exit 1
			fi

			continue
		fi
	fi

	sh $f $2

	if [ "$1" != "allpkg" ] && [ "$1" == "$tmp_apps_name" ]; then
		break
	fi
done

