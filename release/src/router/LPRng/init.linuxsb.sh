#! /bin/sh
#
# This is a gLSB 1.1 complient start script for LPRng. See
#   http://www.linuxbase.org/spec/refspecs/LSB_1.1.0/gLSB/sysinit.html
#
### BEGIN INIT INFO
# Provides: lpd
# Required-Start: $network $remote_fs syslog
# Required-Stop: $network
# Default-Start: 2 3 5
# Default-Stop: 0 1 4 6
# Short-Description: Start lpd to allow printing
# Description: lpd is the print daemon required for lpr to work properly.
#   It is basically a server that arbitrates print jobs to printer(s).
### END INIT INFO

# Source gLSB script
. /lib/lsb/init-functions

# LPD_PATH=/usr/sbin/lpd
test -x $LPD_PATH || exit 5

# Return values acc. to LSB for all commands but status:
# 0 - success
# 1 - misc error
# 2 - invalid or excess args
# 3 - unimplemented feature (e.g. reload)
# 4 - insufficient privilege
# 5 - program not installed
# 6 - program not configured
#
# Note that starting an already running service, stopping
# or restarting a not-running service as well as the restart
# with force-reload (in case signalling is not supported) are
# considered a success.

_status () {
  rc_status=$?
  name="$1"
  case "$rc_status" in
    0) log_success_msg "$name"  ;; # service running
    1) log_warning_msg "$name: no pid file"  ;; # service dead (no pid file)
    2) log_warning_msg "$name: no lock file" ;; # service dead (no lock if any)
    3) log_warning_msg "$name: not running"  ;; # service not running
  esac
  return rc_status
}


case "$1" in
    start)
        ## Start daemon with startproc(8). If this fails
        ## the echo return value is set appropriate.

        ## first run checkpc
        #
        checkpc -f

        # startproc should return 0, even if service is
        # already running to match LSB spec.
        startproc $LPD_PATH
        _status "Starting lpd"
        ;;
    stop)
        killproc `basename "$LPD_PATH"`
        _status "Stopping lpd"
    restart)
        $0 stop
        $1 start
    force-reload)
        $0 stop
        $0 start
    reload)
        killproc -HUP `basename "$LPD_PATH"`
        _status "Reload lpd"
    status)
        ## Check status with checkproc(8), if process is running
        ## checkproc will return with exit status 0.

        # Status has a slightly different for the status command:
        # 0 - service running
        # 1 - service dead, but /var/run/  pid  file exists
        # 2 - service dead, but /var/lock/ lock file exists
        # 3 - service not running

        # If checkproc would return LSB compliant ret values,
        # things could be a little bit easier here. This will
        # probably soon be the case ...
        pidofproc `basename "$LPD_PATH"`
        rc=$?
        if test $rc = 0
        then
          echo "lpd status: running"
        else
          echo "lpd status: No process"
          if test -e /var/run/lpd.printer
          then exit 1
          else exit 3
          fi
        fi
        ;;
        echo "Usage: $0 {start|stop|status|restart|force-reload|reload}"
        exit 1
        ;;
esac
