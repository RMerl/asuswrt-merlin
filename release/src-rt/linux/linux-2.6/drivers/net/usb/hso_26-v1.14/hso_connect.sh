#!/bin/sh

#################################################################################################
#	Script: hso_connect.sh
#	Purpose:Bring the interface up and down, send the needed AT commands to connect
#################################################################################################

# checking for root
USERID=`id -u`
if [ "$USERID" != "0" ]
then
	echo "Need root permissions to run this script"
	exit
fi

# always start with checking if a connection data file has been supplied
CONNECTIONFILE="conninfo.ini"
if [ -z "$2" ]
then
	if [ ! -f $CONNECTIONFILE ]
	then
		echo "# this file contains the connection information for your subscription" >> $CONNECTIONFILE
		echo "APN=internet.proximus.be" >> $CONNECTIONFILE
		echo "# USER=" >> $CONNECTIONFILE
		echo "# PASS=" >> $CONNECTIONFILE
		echo "# PIN=" >> $CONNECTIONFILE
	fi
else
	if [ -f $2 ]
	then
		CONNECTIONFILE=$2
	else
		echo "Supplied file $2 does not exist"
		exit 1
	fi
fi
TTYS=`find /sys/class/tty -name "ttyHS*"`
APP_PORT=""
for i in $TTYS; do
    if [ `grep Application $i/hsotype` ]
    then
	APP_PORT=$i
	break
    fi
done
DEVICE=/dev/`echo $APP_PORT | cut -d/ -f5`
echo Using $DEVICE application port.
NETDEV=hso0
TMPFIL=/tmp/connect.$$
OUTPUTFILE=/tmp/output.$$
SCRIPTFILE=/tmp/scriptfile.$$

RunScript()
{
	rm -f $OUTPUTFILE
	( /usr/sbin/chat -E -s -V -f $SCRIPTFILE <$DEVICE >$DEVICE ) 2> $OUTPUTFILE
	ISERROR="`grep '^ERROR' $OUTPUTFILE`"
	if [ -n "$ISERROR" ]
	then
		echo "Failed to initialize connection"
		cat $OUTPUTFILE
		echo " "
		rm -f $OUTPUTFILE
		exit
	fi
	ISERROR="`grep '^+CME' $OUTPUTFILE`"
	if [ -n "$ISERROR" ]
	then
		echo "Failed to initialize connection"
		cat $OUTPUTFILE
		echo " "
		rm -f $OUTPUTFILE
		exit
	fi
	ISCONNECTED="`grep '^_OWANCALL: 1,1,0' $OUTPUTFILE`"
	if [ -n "$ISCONNECTED" ]
	then
		echo "Alreadu connected"
		cat $OUTPUTFILE
		echo " "
		rm -f $OUTPUTFILE
		exit
	fi
	rm -f $SCRIPTFILE
}


#################################################################################################
#	METHOD:	Connect
#	PURPOSE:Connect to the specified APN with the specified user and pass and get the ip
#		set the IP to the interface
#################################################################################################
Connect()
{
	echo "Initializing..."

	#============================================================
	# get the APN, USER, PASS and PIN out of the connection file
	#============================================================
	APN=`grep '^APN=' $CONNECTIONFILE | cut -d= -f2`
	USER=`grep '^USER=' $CONNECTIONFILE | cut -d= -f2`
	PASS=`grep '^PASS=' $CONNECTIONFILE | cut -d= -f2`
	PIN=`grep '^PIN=' $CONNECTIONFILE | cut -d= -f2`
	if [ -z "$APN" ]
	then
		echo "Please provide an APN (eg web.pro.be)"
		exit
	fi

	#============================================================
	# send the PIN, APN, USER and PASS
	#============================================================
	rm -f $SCRIPTFILE
	echo "ABORT ERROR" > $SCRIPTFILE
	echo "TIMEOUT 10" >> $SCRIPTFILE
	echo "\"\" ATZ" >> $SCRIPTFILE
	if [ -n "$PIN" ]
	then
		echo "OK \"AT+CPIN=\\\"$PIN\\\"^m\"" >> $SCRIPTFILE
	fi
	#echo "OK \"AT+COPS=0^m\"" >> $SCRIPTFILE
	#echo "OK \"\d\d\d\d\d\d\dAT+COPS=?^m\"" >> $SCRIPTFILE
	echo "OK \"AT_OWANCALL?\"" >> $SCRIPTFILE
	echo "OK \"\"" >> $SCRIPTFILE

	RunScript

	echo "ABORT ERROR" > $SCRIPTFILE
	echo "TIMEOUT 10" >> $SCRIPTFILE
	echo "\"\" ATZ" >> $SCRIPTFILE
	echo "OK \"AT+CGDCONT=1,\\\"IP\\\",\\\"$APN\\\"^m\"" >> $SCRIPTFILE 
	if [ -n "$USER" -a -n "$PASS" ]
	then
		echo "OK \"AT\$QCPDPP=1,1,\\\"$PASS\\\",\\\"$USER\\\"^m\"" >> $SCRIPTFILE
	fi
	echo "OK \"\"" >> $SCRIPTFILE

	#============================================================
	# run the script
	#============================================================
	echo "Trying $APN ..."
	RunScript

	#============================================================
	# now actually connect
	#============================================================
	echo "Connecting..."
	stty 19200 -tostop

	# make the call script
	echo "ABORT ERROR" > $SCRIPTFILE
	echo "TIMEOUT 10" >> $SCRIPTFILE
	echo "\"\" ATZ" >> $SCRIPTFILE
	echo "OK \"AT_OWANCALL=1,1,0^m\"" >> $SCRIPTFILE
	echo "OK \"\d\d\d\d\dAT_OWANDATA=1^m\"" >> $SCRIPTFILE
	echo "OK \"\"" >> $SCRIPTFILE
	
	PIP=""
	COUNTER=""
	while [ -z "$PIP" -a "$COUNTER" != "-----" ]
	do
		echo "trying$COUNTER"
		sleep 2
		rm -f $OUTPUTFILE
		( /usr/sbin/chat -E -s -V -f $SCRIPTFILE <$DEVICE > $DEVICE ) 2> $OUTPUTFILE
		ISERROR=`grep '^ERROR' $OUTPUTFILE`
		if [ -z "$ISERROR" ]
		then
			PIP="`grep '^_OWANDATA' $OUTPUTFILE | cut -d, -f2`"
			NS1="`grep '^_OWANDATA' $OUTPUTFILE | cut -d, -f4`"
			NS2="`grep '^_OWANDATA' $OUTPUTFILE | cut -d, -f5`"
		fi

		COUNTER="${COUNTER}-"
	done

	echo Connected

	#============================================================
	# always check the IP address
	#============================================================
	if [ -z "$PIP" ]
	then
		echo "We did not get an IP address from the provider, bailing ..."
		cat $OUTPUTFILE
		rm -f $OUTPUTFILE
		exit
	fi
	rm -f $OUTPUTFILE

	#============================================================
	# setting network settings
	#============================================================
	echo "Setting IP address to $PIP"
	ifconfig $NETDEV $PIP netmask 255.255.255.255 up
	echo "Adding route"
	route add default dev $NETDEV
	mv -f /etc/resolv.conf /tmp/resolv.conf.hso
	echo "Setting nameserver"
	echo "nameserver	$NS1" > $OUTPUTFILE
	echo "nameserver	$NS2" >> $OUTPUTFILE
	mv $OUTPUTFILE /etc/resolv.conf

	echo "Done."
}

#################################################################################################
#	METHOD:	Disconnect
#	PURPOSE:disconnect from the providers network
#################################################################################################
Disconnect()
{
	echo "Bringing interface down..."
	ifconfig $NETDEV down

	echo "Disconnecting..."

	# make the disconnect script
	rm -f $SCRIPTFILE
	echo "TIMEOUT 10" >> $SCRIPTFILE
	echo "ABORT ERROR" >> $SCRIPTFILE
	echo "\"\" ATZ" >> $SCRIPTFILE
	echo "OK \"AT_OWANCALL=1,0,0^m\"" >> $SCRIPTFILE
	echo "OK \"\"" >> $SCRIPTFILE

	#============================================================
	# run the script
	#============================================================
	/usr/sbin/chat -V -f $SCRIPTFILE <$DEVICE >$DEVICE 2> /dev/null
	if [ -f /tmp/resolv.conf.hso ]
	then
		echo "Reset nameserver..."
		mv -f /tmp/resolv.conf.hso /etc/resolv.conf
	fi
	echo "Done."
}

#################################################################################################
#	METHOD:	usage
#	PURPOSE:
#################################################################################################
usage()
{
	echo Usage: $0 \(up\|down\|restart\)
}

#################################################################################################
# Choose your action
#################################################################################################
case "$1" in
	up)
		Connect
		;;
	down)
		Disconnect
		;;
	restart)
		Disconnect
		Connect
		;;
	*)
		usage
		;;
esac

