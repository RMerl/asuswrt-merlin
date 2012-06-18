#!/bin/sh
#
# lpd           This shell script takes care of starting and stopping \
#               lpd (printer daemon).
#
# chkconfig: 2345 60 60
# description: lpd is the print daemon required for lpr to work properly. \
#       It is basically a server that arbitrates print jobs to printer(s).
# processname: ${LPD_PATH}
# config: /etc/printcap

# Source function library.
. /etc/rc.d/init.d/functions

# Source networking configuration and check that networking is up.
if [ -f /etc/sysconfig/network ] ; then
	. /etc/sysconfig/network
	[ "${NETWORKING}" = "no" ] && exit 0
fi

[ -x "${LPD_PATH}" ] || exit 0

prog=lpd

RETVAL=0

start () {
    echo -n $"Starting $prog: "
    # Is this a printconf system?
    if [[ -x /usr/sbin/printconf-backend ]]; then
	    # run printconf-backend to rebuild printcap and spools
	    if ! /usr/sbin/printconf-backend ; then
		# If the backend fails, we dont start no printers defined
		echo -n $"No Printers Defined"
		#echo_success
		#echo
	        #return 0
	    fi
    fi
    if ! [ -e /etc/printcap ] ; then
	echo_failure
	echo
	return 1
    fi
    # run checkpc to fix whatever lpd would complain about
    ${SBINDIR}/checkpc -f
    # start daemon
    daemon ${LPD_PATH}
    RETVAL=$?
    echo
    [ $RETVAL = 0 ] && touch /var/lock/subsys/lpd
    return $RETVAL
}

stop () {
    # stop daemon
    echo -n $"Stopping $prog: "
    killproc ${LPD_PATH}
    RETVAL=$?
    echo
    [ $RETVAL = 0 ] && rm -f /var/lock/subsys/lpd
    return $RETVAL
}

restart () {
    stop
    start
    RETVAL=$?
    return $RETVAL
}

# See how we were called.
case "$1" in
    start)
	start
	;;
    stop)
	stop
	;;
    status)
	status ${LPD_PATH}
	RETVAL=$?
	;;
    restart)
	restart
	;;
    condrestart)
	# only restart if it is already running
	[ -f /var/lock/subsys/lpd ] && restart || :
	;;
    reload)
	echo -n $"Reloading $prog: "
	killproc ${LPD_PATH} -HUP
	RETVAL=$?
	echo
	;;
    *)
        echo $"Usage: $0 {start|stop|restart|condrestart|reload|status}"
        RETVAL=1
esac

exit $RETVAL
