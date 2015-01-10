#!/bin/sh
# ASUS app stop script


nvram set apps_state_cancel=0 # INITIALIZING
nvram set apps_state_error=0

nvram set apps_state_cancel=1 # STOPPING
# stop sh.
killall -SIGTERM sh
killall -SIGKILL sh

nvram set apps_state_cancel=2 # FINISHED
