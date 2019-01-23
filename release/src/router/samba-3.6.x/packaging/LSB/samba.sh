#!/bin/sh
#
# "$Id: samba.sh,v 1.2 2001/07/03 01:01:12 jra Exp $"
#
# SAMBA startup (init) script for LSB-compliant systems.
#
# Provides: smbd nmbd
# Required-Start: 3 5
# Required-Stop: 0 2 1 6
# Default-Start: 3 5
# Default-Stop: 0 2 1 6
# Description: Starts and stops the SAMBA smbd and nmbd daemons \
#              used to provide SMB network services.
# 

# Source LSB function library.
. /lib/lsb/init-functions

# Check that smb.conf exists.
if test ! -f /etc/samba/smb.conf; then
	log_failure_msg "The smb.conf file does not exist."
	exit 6
fi

# Make sure that smbd and nmbd exist...
if test ! -f /usr/sbin/nmbd -o ! -f /usr/sbin/smbd; then
	log_failure_msg "The nmbd and/or smbd daemons are not installed."
	exit 5
fi

# See how we were called.
case "$1" in
	start)
		start_daemon nmbd -D 
		start_daemon smbd -D 	
		log_success_msg "Started SMB services."
		;;

	stop)
		killproc smbd
		killproc nmbd
		log_success_msg "Shutdown SMB services."
		;;

	reload)
		# smbd and nmbd automatically re-read the smb.conf file...
		log_success_msg "Reload not necessary with SAMBA."
		;;

	status)
		if test -z "`pidofproc smbd`"; then
			log_success_msg "smbd is not running."
		else
			log_success_msg "smbd is running."
		fi
		if test -z "`pidofproc nmbd`"; then
			log_success_msg "nmbd is not running."
		else
			log_success_msg "nmbd is running."
		fi
		;;


	restart | force-reload)
		$0 stop
		$0 start
		;;

	*)
		echo "Usage: smb {start|stop|reload|force-reload|restart|status}"
		exit 1
		;;
esac

# Return "success"
exit 0

#
# End of "$Id: samba.sh,v 1.2 2001/07/03 01:01:12 jra Exp $".
#
