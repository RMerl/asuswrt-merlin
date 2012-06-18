#
# -- START --
# init.linux.sh,v 1.1 2001/08/21 20:33:15 root Exp
#
# lpd           This shell script takes care of starting and stopping
#               lpd (printer daemon).
#  Taken from the RedHat Linux 6.2 distribution for the lpd startup
#  modified to make things a little more robust
#
# chkconfig: 2345 60 60
# description: lpd is the print daemon required for lpr to work properly. \
#   It is basically a server that arbitrates print jobs to printer(s).
# processname: lpd
# config: /etc/printcap

# Source function library.
if [ -f /etc/rc.d/init.d/functions ] ; then
. /etc/rc.d/init.d/functions
fi

# Source networking configuration.
if [ -f /etc/sysconfig/network ] ; then
. /etc/sysconfig/network
fi

# Check that networking is up.
[ "${NETWORKING}" = "no" ] && exit 0
[ -f "${LPD_PATH}" ] || exit 0
[ -f /etc/printcap ] || exit 0

RETVAL=0


# ignore INT signal
trap '' 2

# See how we were called.
case "$1" in
  start)
        # Start daemons.
        echo -n "Starting lpd: "
        if [ -f /etc/redhat-release ] ; then
            daemon ${LPD_PATH}
            RETVAL=$?
            [ $RETVAL -eq 0 ] && touch /var/lock/subsys/lprng
        else
            ${LPD_PATH}
            RETVAL=$?
        fi
        echo
        ;;
  stop)
        # Stop daemons.
        echo -n "Shutting down lprng: "
        if [ -f /etc/redhat-release ] ; then
            killproc lpd
            RETVAL=$?
            [ $RETVAL -eq 0 ] && rm -f /var/lock/subsys/lprng
        else
            kill -INT `ps ${PSHOWALL} | awk '/lpd/{ print $1;}'` >/dev/null 2>&1
            RETVAL=0
		fi
        echo
        ;;
  status)
    if [ -f /etc/redhat-release ] ; then
        status lpd
    else
        lpc lpd
    fi
    RETVAL=$?
    ;;
  restart|reload)
    $0 stop
    $0 start
    RETVAL=$?
    ;;
  *)
        echo "Usage: $0 {start|stop|restart|reload|status}"
        exit 1
esac

exit $RETVAL
