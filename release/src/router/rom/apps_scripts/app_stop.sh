#!/bin/sh
# ASUS app stop script


nvram set apps_state_stop=0 # INITIALIZING
nvram set apps_state_error=0
APPS_INSTALL_FOLDER=`nvram get apps_install_folder`
SWAP_ENABLE=`nvram get apps_swap_enable`
SWAP_FILE=`nvram get apps_swap_file`
APPS_MOUNTED_PATH=`nvram get apps_mounted_path`

nvram set apps_state_stop=1 # STOPPING
# stop all APPs by order.
/usr/sbin/app_init_run.sh allpkg stop


nvram set apps_state_stop=2 # REMOVING_SWAP
if [ "$SWAP_ENABLE" == "1" ] && [ -f "$APPS_MOUNTED_PATH/$APPS_INSTALL_FOLDER/$SWAP_FILE" ]; then
	swapoff $APPS_MOUNTED_PATH/$APPS_INSTALL_FOLDER/$SWAP_FILE
	rm -rf $APPS_MOUNTED_PATH/$APPS_INSTALL_FOLDER/$SWAP_FILE
fi


nvram set apps_state_stop=3 # FINISHED
