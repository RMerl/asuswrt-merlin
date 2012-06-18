#!/bin/sh

#################################################################################################
#	Script: Ports.sh 
#	Purpose: Shows application or all ports for the hso driver
#	This script checks the sys folder (for newer driver versions) and the proc folder (for older driver versions)
#	Parameter all Shows all ports 
#	Parameter app Shows application port 
#	Author: J. Bellavia, D. Barow 
#################################################################################################

# checking for root
USERID=`id -u`
if [ "$USERID" != "0" ]
then
	echo "Need root permissions to run this script"
	exit
fi

#################################################################################################
#	METHOD:	application
#	PURPOSE:shows application interface
#################################################################################################
application()
{
	###########################################################
	#searching for application port in the sys filesystem
	###########################################################
	echo searching the application port in sys filesystem
	TTYS=`find /sys/class/tty -name "ttyHS*"`
	APP_PORT=""
	for i in $TTYS; do
	    if [ `grep Application $i/hsotype` ]
	    then	
		APP_PORT=$i
		found=1
		break
	    #else
	    #	echo The application port is not $i.
	    fi
	done
	####################
	#Found the app port?
	####################
	if [ -z "$APP_PORT" ]
	then
		echo Did not find the the app port in sys
	else
		DEVICE=/dev/`echo $APP_PORT | cut -d/ -f5`
		echo The application port is $DEVICE.
	fi	

	###########################################################
	#searching for application port in the proc filesystem
	###########################################################
	if [ -z "$found" ]
	then
		echo searching in proc filesystem 
		TTYS=`find /proc/hso/devices -name "ttyHS*"`
		APP_PORT=""
		for i in $TTYS; do
		    if [ `grep Application $i/hsotype` ]
		    then
			APP_PORT=$i
			found=1
			break
		    #else
		    #	echo The application port is not $i.
		    fi
		done
		####################
		#Found the app port?
		####################		
		if [ -z "$APP_PORT" ]
		then
			echo Did not find the the app port in proc
		else
			DEVICE=/dev/`echo $APP_PORT | cut -d/ -f5`
			echo The application port is $DEVICE.
		fi
	fi
	###############################################################################
	#it can be that hso driver is not installed or that the device is not inserted 
	###############################################################################
	if [ -z "$found" ]
	then
		echo Please check whether your device is installed and connected to the PC
	fi

}

#################################################################################################
#	METHOD:	all
#	PURPOSE:Shows portname for all ports
#################################################################################################
all()
{
	###########################################################
	#searching for ports in the sys filesystem first
	##########################################################
	echo searching in sys filesystem
	TTYS=`find /sys/class/tty -name "ttyHS*"`
	PORT=""
	for i in $TTYS; do
		CatType=`cat $i/hsotype`
		#echo Catype is $CatType
		PORT=$i
		DEVICE=/dev/`echo $PORT | cut -d/ -f5`
		if [ -n "$CatType" ]
		then
			echo The Device $DEVICE is the $CatType port.
			found=1		
		#else			
		fi
	done
	#####################################################################
	#searching for ports in the proc filesystem when device is not found
	#####################################################################
	if [ -z "$found" ]
	then
		echo searching in proc filesystem
		TTYS=`find /proc/hso/devices/ -name "ttyHS*"`
		PORT=""
		for i in $TTYS; do
			CatType=`cat $i/hsotype`
			#echo Catype is $CatType
			PORT=$i
			DEVICE=/dev/`echo $PORT | cut -d/ -f5`
			if [ -n "$CatType" ]
			then
				echo The Device $DEVICE is the $CatType port.
				found=1		
			#else		
			fi
		done
	fi	
	###############################################################################
	#it can be that hso driver is not installed or that the device is not inserted 
	###############################################################################

	if [ -z "$found" ]
	then
		echo Please check whether your device is installed and connected to the PC
	fi
}
#################################################################################################
#	METHOD:	usage
#	PURPOSE:
#################################################################################################
usage()
{
	echo Usage: $0 \(app\|all\)
}

#################################################################################################
# Choose your action
#################################################################################################
case "$1" in
	app)
		application
		;;
	all)
		all
		;;
	*)
		usage
		;;
esac

