#!/bin/sh
# 
# dproxy           This shell script takes care of starting and stopping
#                  dproxy (DNS caching proxy).
# 
# chkconfig: 345 55 45
# 
# description: dproxy is a caching Domain Name Server (DNS)  proxy
# that is used to resolve host names to IP addresses.
# probe: true
# 

DAEMON=dproxy
DPROXY=BIN_DIR/$DAEMON
DPROXY_CONF=CONFIG_DIR/$DAEMON.conf

ifdef(`REDHAT',`
# Source function library.
. /etc/rc.d/init.d/functions

# Source networking configuration.
. /etc/sysconfig/network

# Check that networking is up.
[ ${NETWORKING} = "no" ] && exit 0

DSTART="daemon $DPROXY -c $DPROXY_CONF"
DSTOP="killproc $DPROXY"

LOCKFILE="/var/lock/subsys/dproxy"
LOCK="touch $LOCKFILE"
UNLOCK="rm -f $LOCKFILE"
',`')
ifdef(`DEBIAN',`
DSTART="start-stop-daemon -S -x $DPROXY -n $DAEMON -- -c $DPROXY_CONF"
DSTOP="start-stop-daemon -K -x $DPROXY -n $DAEMON -- -c $DPROXY_CONF"
LOCK=""
UNLOCK=""
')
ifdef(`SUSE',`
DSTART="start-stop-daemon -S -x $DPROXY -n $DAEMON -- -c $DPROXY_CONF"
DSTOP="start-stop-daemon -K -x $DPROXY -n $DAEMON -- -c $DPROXY_CONF"
LOCK=""
UNLOCK=""
')


[ -x "$DPROXY" ] || exit 0

# for the future.... I like to aim high ;-)
[ -f "$DPROXY_CONF" ] || exit 0

# See how we were called.
case "$1" in
  start)
        # Start daemons.
        echo -n "Starting dproxy: "
        eval $DSTART 
        echo
	eval $LOCK
        ;;
  stop)
        # Stop daemons.
        echo -n "Shutting down dproxy: "
        eval $DSTOP 
	eval $UNLOCK
        echo
        ;;
ifdef(`REDHAT',`
  status)
        status $DPROXY
        ;;
',`')
  reload|restart)
        $0 stop
        $0 start
        ;;
  *)
        echo "Usage: $0 {start|stop|status|restart}"
        exit 1
esac

exit 0
