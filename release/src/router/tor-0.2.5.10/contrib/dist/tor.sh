#!/bin/sh
#
# tor    The Onion Router
#
# Startup/shutdown script for tor. This is a wrapper around torctl;
# torctl does the actual work in a relatively system-independent, or at least
# distribution-independent, way, and this script deals with fitting the
# whole thing into the conventions of the particular system at hand.
# This particular script is written for Red Hat/Fedora Linux, and may
# also work on Mandrake, but not SuSE.
#
# These next couple of lines "declare" tor for the "chkconfig" program,
# originally from SGI, used on Red Hat/Fedora and probably elsewhere.
#
# chkconfig: 2345 90 10
# description: Onion Router - A low-latency anonymous proxy
#

PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
DAEMON=/usr/sbin/tor
NAME=tor
DESC="tor daemon"
TORPIDDIR=/var/run/tor
TORPID=$TORPIDDIR/tor.pid
WAITFORDAEMON=60
ARGS=""

# Library functions
if [ -f /etc/rc.d/init.d/functions ]; then
   . /etc/rc.d/init.d/functions
elif [ -f /etc/init.d/functions ]; then
   . /etc/init.d/functions
fi

TORCTL=/usr/local/bin/torctl

# torctl will use these environment variables
TORUSER=_tor
export TORUSER

if [ -x /bin/su ] ; then
    SUPROG=/bin/su
elif [ -x /sbin/su ] ; then
    SUPROG=/sbin/su
elif [ -x /usr/bin/su ] ; then
    SUPROG=/usr/bin/su
elif [ -x /usr/sbin/su ] ; then
    SUPROG=/usr/sbin/su
else
    SUPROG=/bin/su
fi

# Raise ulimit based on number of file descriptors available (thanks, Debian)

if [ -r /proc/sys/fs/file-max ]; then
	system_max=`cat /proc/sys/fs/file-max`
	if [ "$system_max" -gt "80000" ] ; then
		MAX_FILEDESCRIPTORS=32768
	elif [ "$system_max" -gt "40000" ] ; then
		MAX_FILEDESCRIPTORS=16384
	elif [ "$system_max" -gt "10000" ] ; then
		MAX_FILEDESCRIPTORS=8192
	else
		MAX_FILEDESCRIPTORS=1024
		cat << EOF

Warning: Your system has very few filedescriptors available in total.

Maybe you should try raising that by adding 'fs.file-max=100000' to your
/etc/sysctl.conf file.  Feel free to pick any number that you deem appropriate.
Then run 'sysctl -p'.  See /proc/sys/fs/file-max for the current value, and
file-nr in the same directory for how many of those are used at the moment.

EOF
	fi
else
	MAX_FILEDESCRIPTORS=8192
fi

NICE=""

case "$1" in

    start)
	if [ -n "$MAX_FILEDESCRIPTORS" ]; then
		echo -n "Raising maximum number of filedescriptors (ulimit -n) to $MAX_FILEDESCRIPTORS"
		if ulimit -n "$MAX_FILEDESCRIPTORS" ; then
			echo "."
		else
			echo ": FAILED."
		fi
	fi

    action $"Starting tor:" $TORCTL start
    RETVAL=$?
    ;;

    stop)
    action $"Stopping tor:" $TORCTL stop
    RETVAL=$?
    ;;

    restart)
    action $"Restarting tor:" $TORCTL restart
    RETVAL=$?
    ;;

    reload)
    action $"Reloading tor:" $TORCTL reload
    RETVAL=$?
    ;;

    status)
    $TORCTL status
    RETVAL=$?
    ;;

    *)
    echo "Usage: $0 (start|stop|restart|reload|status)"
    RETVAL=1
esac

exit $RETVAL
