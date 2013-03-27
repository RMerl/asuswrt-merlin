#!/bin/sh


is_arm_machine=`uname -m |grep arm`

APP_UCLIBC_VERSION=0.9.28

APPS_DEV=`nvram get apps_dev`
APPS_MOUNTED_PATH=`nvram get apps_mounted_path`
APPS_INSTALL_FOLDER=`nvram get apps_install_folder`
APPS_INSTALL_PATH=$APPS_MOUNTED_PATH/$APPS_INSTALL_FOLDER

if [ -z "$APPS_MOUNTED_PATH" ]; then
	nvram set apps_state_error=2
	exit 1
fi

APP_BIN=$APPS_INSTALL_PATH/bin
APP_LIB=$APPS_INSTALL_PATH/lib

APP_LINK_DIR=/tmp/opt
APP_LINK_BIN=$APP_LINK_DIR/bin
APP_LINK_LIB=$APP_LINK_DIR/lib


APPS_MOUNTED_TYPE=`mount |grep "/dev/$APPS_DEV on " |awk '{print $5}'`
if [ "$APPS_MOUNTED_TYPE" != "vfat" ]; then
	chmod -R 777 $APPS_INSTALL_PATH
	user_account=`nvram get http_username`
	if [ -z "$user_account" ]; then
		user_account="admin"
	fi
	chown -R "$user_account":root $APPS_INSTALL_PATH
	rm -rf $APP_LINK_DIR
	ln -sf $APPS_INSTALL_PATH $APP_LINK_DIR
	exit 0
fi

# Others files or directories
objs=`ls -a $APPS_INSTALL_PATH |grep -v ^\.__*`
if [ -L "$APP_LINK_DIR" ] || [ ! -d "$APP_LINK_DIR" ]; then
	rm -rf $APP_LINK_DIR
	mkdir -p $APP_LINK_DIR
fi

for obj in $objs; do
	if [ "$obj" == "." ] || [ "$obj" == ".." ]; then
		continue
	fi

	if [ "$obj" != "bin" ] && [ "$obj" != "lib" ]; then
		if [ -d "$APP_LINK_DIR/$obj" ]; then
			rm -rf $APP_LINK_DIR/$obj
		fi
		ln -sf $APPS_INSTALL_PATH/$obj $APP_LINK_DIR/$obj
	fi
done


# BIN
objs=`ls -a $APP_BIN`
if [ -L "$APP_LINK_BIN" ] || [ ! -d "$APP_LINK_BIN" ]; then
	rm -rf $APP_LINK_BIN
	mkdir -p $APP_LINK_BIN
fi

# ipkg-opt
ln -sf $APP_BIN/ipkg $APP_LINK_BIN/ipkg-opt

for obj in $objs; do
	if [ "$obj" == "." ] || [ "$obj" == ".." ]; then
		continue
	fi

	if [ -d "$APP_LINK_DIR/$obj" ]; then
		rm -rf $APP_LINK_DIR/$obj
	fi
	ln -sf $APP_BIN/$obj $APP_LINK_BIN/$obj 
done


# LIB
objs=`ls -a $APP_LIB`
if [ -L "$APP_LINK_LIB" ] || [ ! -d "$APP_LINK_LIB" ]; then
	rm -rf $APP_LINK_LIB
	mkdir -p $APP_LINK_LIB
fi

# first find the other objs and then do uclibc.
for obj in $objs; do
	if [ "$obj" == "." ] || [ "$obj" == ".." ]; then
		continue
	fi

	if [ -d "$APP_LINK_DIR/$obj" ]; then
		rm -rf $APP_LINK_DIR/$obj
	fi
	ln -sf $APP_LIB/$obj $APP_LINK_LIB/$obj 
done

# ipkg-opt
ln -sf $APP_LIB/libipkg.so.0.0.0 $APP_LINK_LIB/libipkg.so.0
ln -sf $APP_LIB/libipkg.so.0.0.0 $APP_LINK_LIB/libipkg.so

# uclibc-opt
ln -sf $APP_LIB/ld-uClibc-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/ld-uClibc.so.0
ln -sf $APP_LIB/ld-uClibc-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/ld-uClibc.so
ln -sf $APP_LIB/libuClibc-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libc.so.0
ln -sf $APP_LIB/libuClibc-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libc.so
ln -sf $APP_LIB/libcrypt-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libcrypt.so.0
ln -sf $APP_LIB/libcrypt-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libcrypt.so
ln -sf $APP_LIB/libgcc_s.so.1 $APP_LINK_LIB/libgcc_s.so
if [ -n "$is_arm_machine" ]; then
	ln -sf $APP_LIB/libstdc++.so.6.0.2 $APP_LINK_LIB/libstdc++.so.6
	ln -sf $APP_LIB/libstdc++.so.6.0.2 $APP_LINK_LIB/libstdc++.so
else
	ln -sf $APP_LIB/libstdc++.so.6.0.8 $APP_LINK_LIB/libstdc++.so.6
	ln -sf $APP_LIB/libstdc++.so.6.0.8 $APP_LINK_LIB/libstdc++.so
	ln -sf $APP_LIB/libdl-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libdl.so.0
	ln -sf $APP_LIB/libdl-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libdl.so
	ln -sf $APP_LIB/libintl-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libintl.so.0
	ln -sf $APP_LIB/libintl-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libintl.so
	ln -sf $APP_LIB/libmudflap.so.0.0.0 $APP_LINK_LIB/libmudflap.so.0
	ln -sf $APP_LIB/libmudflap.so.0.0.0 $APP_LINK_LIB/libmudflap.so
	ln -sf $APP_LIB/libmudflapth.so.0.0.0 $APP_LINK_LIB/libmudflapth.so.0
	ln -sf $APP_LIB/libmudflapth.so.0.0.0 $APP_LINK_LIB/libmudflapth.so
	ln -sf $APP_LIB/libnsl-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libnsl.so.0
	ln -sf $APP_LIB/libnsl-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libnsl.so
fi
ln -sf $APP_LIB/libm-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libm.so.0
ln -sf $APP_LIB/libm-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libm.so
ln -sf $APP_LIB/libpthread-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libpthread.so.0
ln -sf $APP_LIB/libpthread-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libpthread.so
ln -sf $APP_LIB/libresolv-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libresolv.so.0
ln -sf $APP_LIB/libresolv-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libresolv.so
ln -sf $APP_LIB/librt-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/librt.so.0
ln -sf $APP_LIB/librt-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/librt.so
ln -sf $APP_LIB/libthread_db-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libthread_db.so.1
ln -sf $APP_LIB/libthread_db-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libthread_db.so
ln -sf $APP_LIB/libutil-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libutil.so.0
ln -sf $APP_LIB/libutil-${APP_UCLIBC_VERSION}.so $APP_LINK_LIB/libutil.so
