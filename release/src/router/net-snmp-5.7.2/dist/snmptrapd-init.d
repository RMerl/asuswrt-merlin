#!/bin/sh
#
# snmptrapd	This shell script takes care of starting and stopping
#	the net-snmp SNMPTRAP daemon
#
# chkconfig: - 25 75
# description: snmptrapd is net-snmp SNMPTRAP daemon.

# Source function library.
. /etc/rc.d/init.d/functions

# Source networking configuration.
. /etc/sysconfig/network

# Check that networking is up.
[ "${NETWORKING}" = "no" ] && exit 0

RETVAL=0
name="snmptrapd"
prog="/usr/local/sbin/snmptrapd"

[ -x $prog ] || exit 0

start() {
        # Start daemons.
        echo -n $"Starting $name: "
        daemon $prog
	RETVAL=$?
        echo
        [ $RETVAL -eq 0 ] && touch /var/lock/subsys/$name
	return $RETVAL
}

stop() {
        # Stop daemons.
        echo -n $"Shutting down $name: "
	killproc $prog
	RETVAL=$?
        echo
        [ $RETVAL -eq 0 ] && rm -f /var/lock/subsys/$name
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
	status $name
	RETVAL=$?
	;;
  restart|reload)
	stop
	start
	RETVAL=$?
	;;
  condrestart)
	if [ -f /var/lock/subsys/$name ]; then
	    stop
	    start
	    RETVAL=$?
	fi
	;;
  *)
        echo $"Usage: $0 {start|stop|restart|condrestart|status}"
        exit 1
esac

exit $RETVAL
