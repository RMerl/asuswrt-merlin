#!/bin/sh
# $1: package name, $2: field name,
# $3: 0: find all files(default), 1: find the control file, 2: find the list file.


APPS_PATH=/opt
CONF_FILE=$APPS_PATH/etc/ipkg.conf
LIST_DIR=$APPS_PATH/lib/ipkg/lists
LIST_FILES=
TEMP_CONF_FILE=/tmp/package.txt
got_field=0
field_gone=
field_value=


if [ -z "$1" ] || [ -z "$2" ]; then
	echo "Usage: app_get_field.sh <Package name> <Field name> [0|1|2]"
	exit 1
fi

if [ -n "$3" ] && [ "$3" != "0" ]  && [ "$3" != "1" ] && [ "$3" != "2" ]; then
	echo "Usage: app_get_field.sh <Package name> <Field name> [0|1|2]"
	exit 1
fi

if [ "$3" != "2" ]; then
	pkg_control_file=$APPS_PATH/lib/ipkg/info/$1.control
	if [ -f "$pkg_control_file" ]; then
		field_gone=`grep "$2: " $pkg_control_file`
		if [ -n "$field_gone" ]; then
			field_value=`grep "$2: " $pkg_control_file |awk '{print $2}'`
			echo "$field_value"
			exit 0
		fi
	fi

	if [ "$3" == "1" ]; then
		exit 0
	fi
fi

if [ ! -f "$CONF_FILE" ]; then
	exit 1
fi

LIST_FILES=`grep -n '^src.*' $CONF_FILE |sort -r |awk '{print "'$LIST_DIR'/"$2}'`
if [ -z "$LIST_FILES" ]; then
	exit 1
fi

for f in $LIST_FILES; do
	if [ ! -f "$f" ]; then
		continue
	fi

	field_gone=`grep "Package: "$1"$" $f`
	if [ -z "$field_gone" ]; then
		continue
	fi
	grep -A 15 "Package: "$1"$" $f > $TEMP_CONF_FILE

	field_gone=`grep "$2: " $TEMP_CONF_FILE`
	if [ -z "$field_gone" ]; then
		continue
	fi

	field_value=`grep "$2: " $TEMP_CONF_FILE |sed '2,$d' |awk '{FS="'$2': "; print $2}' |sed 's/, /,/g'`
	rm -f $TEMP_CONF_FILE
	echo "$field_value"
	exit 0
done
