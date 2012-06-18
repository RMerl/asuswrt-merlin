#!/bin/sh
# ASUS app stop script


nvram set apps_state_cancel=0 # INITIALIZING
nvram set apps_state_error=0
APPS_INSTALL_FOLDER=`nvram get apps_install_folder`
SWAP_ENABLE=`nvram get apps_swap_enable`
SWAP_FILE=`nvram get apps_swap_file`
APPS_MOUNTED_PATH=`nvram get apps_mounted_path`

nvram set apps_state_cancel=1 # STOPPING
# stop sh.
killall -SIGTERM sh
killall -SIGKILL sh

nvram set apps_state_cancel=2 # FINISHED
