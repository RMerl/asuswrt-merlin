#!/bin/sh
# ASUS check if spectrum is running

if [ -z "$(pidof spectrum)" ];
then
	#echo "spectrum is not running"
	/bin/spectrum
fi
